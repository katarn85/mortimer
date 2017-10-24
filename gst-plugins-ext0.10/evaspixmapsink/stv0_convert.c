/*
 * stv0_convert.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */
#include <string.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <gst/video/video.h>

/* headers for drm */
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <X11/Xmd.h>
#include <dri2/dri2.h>
#include <libdrm/drm.h>
#include <xf86drm.h>

#include <tbm_bufmgr.h>

#include "xv_pixmap_utils.h"
#include "stv0_convert.h"

struct _STV0ConversionCtx
{
	/* One time initialization fields. */
	Display *dpy;
	tbm_bufmgr bufmgr;

	/* */
	tbm_bo boPixmap;
	tbm_bo_handle bo_hnd_Pixmap;
	int bo_Pixmap_width;
	int bo_Pixmap_height;

	/* */
	tbm_bo boY, boCbCr;
	tbm_bo_handle bo_hnd_Y, bo_hnd_CbCr;

	unsigned char *pScaledY;
	unsigned char *pScaledCbCr;
};


static void 
free_intermediate_buffers(STV0ConversionCtx ctx) 
{
	if (ctx->bo_hnd_Y.ptr) {
		tbm_bo_unmap(ctx->boY);
		ctx->bo_hnd_Y.ptr = NULL;
	}
	if (ctx->boY) {
		tbm_bo_unref(ctx->boY);
		ctx->boY = NULL;
	}
	if (ctx->bo_hnd_CbCr.ptr) {
		tbm_bo_unmap(ctx->boCbCr);
		ctx->bo_hnd_CbCr.ptr = NULL;
	}
	if (ctx->boCbCr) {
		tbm_bo_unref(ctx->boCbCr);
		ctx->boCbCr = NULL;
	}
	if (ctx->boPixmap) {
		tbm_bo_unref(ctx->boPixmap);
		ctx->boPixmap = NULL;
	}
	if (ctx->pScaledCbCr) {
		free(ctx->pScaledCbCr);
		ctx->pScaledCbCr = NULL;
	}
	if (ctx->pScaledY) {
		free(ctx->pScaledY);
		ctx->pScaledY = NULL;
	}
}


/* To get the buffer object of a pixmap */
static tbm_bo get_pixmap_bo(Display *dpy, Pixmap pixmap, tbm_bufmgr bufmgr, int *dri2_width, int *dri2_height)
{
	guint attachments[1];
	gint dri2_count, dri2_out_count;
	DRI2Buffer *dri2_buffers = NULL;
	tbm_bo bo = NULL;

	DRI2CreateDrawable (dpy, pixmap);

	attachments[0] = DRI2BufferFrontLeft;
	dri2_count = 1;
	dri2_buffers = DRI2GetBuffers (dpy, pixmap, dri2_width, dri2_height, attachments, dri2_count, &dri2_out_count);
	if (!dri2_buffers || dri2_buffers[0].name <= 0){
		GST_ERROR("error: DRI2GetBuffers failed!");
		goto fail_get;
	}

	/* real buffer of pixmap */
	bo = tbm_bo_import (bufmgr, dri2_buffers[0].name);
	if (!bo){
		GST_ERROR("error: tbm_bo_import failed!");
		goto fail_get;
	}
	
	GST_DEBUG("pixmap: w(%d) h(%d) pitch(%d) cpp(%d) flink_id(%d)\n",
			*dri2_width, *dri2_height,
			dri2_buffers[0].pitch,
			dri2_buffers[0].cpp,
			dri2_buffers[0].name);
	
	free (dri2_buffers);
	return bo;
	
fail_get:
	if (dri2_buffers)
		free (dri2_buffers);
	return NULL;
}

static Bool
set_intermediate_buffers(STV0ConversionCtx ctx, unsigned int width, unsigned int height, int colorformat)
{
	ctx->boY = tbm_bo_alloc(ctx->bufmgr, width * height, TBM_BO_SCANOUT);
	if (!ctx->boY)
		goto tbm_prepare_failed;
	ctx->bo_hnd_Y = tbm_bo_map(ctx->boY, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
	if (!ctx->bo_hnd_Y.ptr)
		goto tbm_prepare_failed;
	
	ctx->pScaledY = (unsigned char*) malloc(width * height);
	if (!ctx->pScaledY)
		goto tbm_prepare_failed;

	if(colorformat != V4L2_DRM_COLORFORMAT_YUV422)	//for YUV 420 format
	{
		/*if the value of width is odd number.*/
		width = (width & 0x1) ? ((width >> 1) + 1) : (width >> 1);
	}
	ctx->boCbCr = tbm_bo_alloc(ctx->bufmgr, width*height /*YUV420 : width*height/2*/, TBM_BO_SCANOUT);
	if (!ctx->boCbCr)
		goto tbm_prepare_failed;
	ctx->bo_hnd_CbCr = tbm_bo_map(ctx->boCbCr, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
	if (!ctx->bo_hnd_CbCr.ptr)
		goto tbm_prepare_failed;
	
	ctx->pScaledCbCr = (unsigned char*) malloc(width*height);
	if (!ctx->pScaledCbCr)
		goto tbm_prepare_failed;

	return TRUE;
	
tbm_prepare_failed:
	GST_ERROR("prepare TBM scaling buffer failed!\n");
	free_intermediate_buffers(ctx);
	
	return FALSE;
}


static Bool
do_hw_tbm_scaling(STV0ConversionCtx ctx, struct v4l2_drm *in, unsigned int width, unsigned int height,int colorformat)
{
	tbm_ga_scale_wrap scale_wrap;
	int ret;
	struct v4l2_private_frame_info *frminfo;

	frminfo = &(in->u.dec_info.pFrame[0]);
	
	if ( frminfo->y_linesize == 0 || frminfo->u_linesize == 0 || frminfo->y_phyaddr == 0 || frminfo->u_phyaddr == 0) {
		GST_ERROR("error: the line size of hw frame is invalid: y_linesize(%d), u_linesize(%d) y_phyaddr[ %x ], u_phyaddr[ %x ]!",
				    frminfo->y_linesize, frminfo->u_linesize, frminfo->y_phyaddr, frminfo->u_phyaddr);
		return FALSE;
	}

	memset(&scale_wrap, 0, sizeof(tbm_ga_scale_wrap));
	scale_wrap.bufmgr = ctx->bufmgr;

	/* scale Y */
	scale_wrap.src_bo = NULL;
	scale_wrap.dst_bo = ctx->boY;
	scale_wrap.src_paddr = (void *) frminfo->y_phyaddr;
	scale_wrap.dst_paddr = NULL;
	scale_wrap.scale.color_mode = TBM_GA_FORMAT_8BPP;
	scale_wrap.scale.rop_mode = TBM_GA_ROP_COPY;
	scale_wrap.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
	scale_wrap.scale.src_hbyte_size = frminfo->y_linesize * TBM_BPP8; /* Y line size */
	scale_wrap.scale.src_rect.x = 0;
	scale_wrap.scale.src_rect.y = 0; /* Y: Top of buffer (w*h) */
	scale_wrap.scale.src_rect.w = frminfo->width;
	scale_wrap.scale.src_rect.h = frminfo->height;
	scale_wrap.scale.dst_hbyte_size = width * TBM_BPP8;
	scale_wrap.scale.dst_rect.x = 0;
	scale_wrap.scale.dst_rect.y = 0;
	scale_wrap.scale.dst_rect.w = width; /* the width and height of the target pixmap.*/
	scale_wrap.scale.dst_rect.h = height;
	scale_wrap.scale.rop_ca_value = 0;
	scale_wrap.scale.src_key = 0;
	scale_wrap.scale.rop_on_off = 0;
	ret = tbm_bo_ga_scale(&scale_wrap);
	if (!ret) {
		GST_ERROR("scaling Y failed! ret(%d)", ret);
		return FALSE;
	}
	
	/* scale CbCr */
	scale_wrap.src_bo = NULL;
	scale_wrap.dst_bo = ctx->boCbCr;
	scale_wrap.src_paddr = (void *) frminfo->u_phyaddr;
	scale_wrap.dst_paddr = NULL;
	scale_wrap.scale.color_mode = TBM_GA_FORMAT_16BPP; /* Because of CbCr Interleaved */
	scale_wrap.scale.rop_mode = TBM_GA_ROP_COPY;
	scale_wrap.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
	scale_wrap.scale.src_hbyte_size = frminfo->u_linesize * TBM_BPP8; /* for YUV420 interleaved case*/
	scale_wrap.scale.src_rect.x = 0;
	scale_wrap.scale.src_rect.y = 0;
	scale_wrap.scale.src_rect.w = frminfo->width/2;
	if(colorformat == V4L2_DRM_COLORFORMAT_YUV422)	//for YUV 422 format
		scale_wrap.scale.src_rect.h = frminfo->height;
	else
		scale_wrap.scale.src_rect.h = frminfo->height/2;
	scale_wrap.scale.dst_hbyte_size = width * TBM_BPP8;
	scale_wrap.scale.dst_rect.x = 0;
	scale_wrap.scale.dst_rect.y = 0;
	scale_wrap.scale.dst_rect.w = width/2; /* the width and height of the target pixmap.*/
	if(colorformat == V4L2_DRM_COLORFORMAT_YUV422)	//for YUV 422 format
		scale_wrap.scale.dst_rect.h = height;
	else
		scale_wrap.scale.dst_rect.h = height/2;
	scale_wrap.scale.rop_ca_value = 0;
	scale_wrap.scale.src_key = 0;
	scale_wrap.scale.rop_on_off = 0;
	ret = tbm_bo_ga_scale(&scale_wrap);
	if (!ret) {
		GST_ERROR("scaling CbCr failed! ret(%d)", ret);
		return FALSE;
	}
	return TRUE;
}

/* This function converts the colorspace of decoded frames to what pixmap supports. */
static Bool
do_colorspace_conversion (STV0ConversionCtx ctx, unsigned int width, unsigned int height,int colorformat)
{
	ctx->bo_hnd_Pixmap = tbm_bo_map (ctx->boPixmap, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
	if (!ctx->bo_hnd_Pixmap.ptr) {
		GST_ERROR("error: tbm_bo_map Pixmap bo failed! ");
		return FALSE;
	}

	unsigned char *boPixmap_addr = (unsigned char *)ctx->bo_hnd_Pixmap.ptr;

	memcpy(ctx->pScaledY, ((unsigned char *)ctx->bo_hnd_Y.ptr), width * height);

	if(colorformat == V4L2_DRM_COLORFORMAT_YUV422)	// for YUV 422 format
	{
		memcpy(ctx->pScaledCbCr, ((unsigned char *)ctx->bo_hnd_CbCr.ptr), (width  * height));
		convert_yuv422_interleaved_to_argb(ctx->pScaledY, ctx->pScaledCbCr, width, width, width, height, boPixmap_addr);
	}
	else
	{
		memcpy(ctx->pScaledCbCr, ((unsigned char *)ctx->bo_hnd_CbCr.ptr), (((width & 0x1) ? ((width >> 1) + 1) : (width >> 1)) * height));
		convert_yuv420_interleaved_to_argb(ctx->pScaledY, ctx->pScaledCbCr, width, width, width, height, boPixmap_addr);
	}

	tbm_bo_unmap (ctx->boPixmap);
	
	return TRUE;
}


STV0ConversionCtx stv0_create_context(int drmfd)
{
	STV0ConversionCtx ctx = NULL;
	xv_colorconversion_init();
	
	ctx = (STV0ConversionCtx) calloc(1, sizeof(struct _STV0ConversionCtx));
	if (ctx == NULL)
		return NULL;

	ctx->dpy = XOpenDisplay(0);
	if (ctx->dpy == NULL)
		goto fail;

	ctx->bufmgr = tbm_bufmgr_init(drmfd);
	if (ctx->bufmgr == NULL)
	{
		GST_ERROR("tbm_bufmgr_init failed");
		goto fail;
	}	
	
	return ctx;

fail:
	if (ctx->dpy != NULL)
		XCloseDisplay(ctx->dpy);
	free(ctx);
	return NULL;
}

void stv0_destroy_context(STV0ConversionCtx ctx)
{
	if (!ctx->boPixmap)
		tbm_bo_unref(ctx->boPixmap);
	free_intermediate_buffers(ctx);	

	tbm_bufmgr_deinit(ctx->bufmgr);
	XCloseDisplay(ctx->dpy);
	free(ctx);
}

static Bool update_pixmap_related_data(STV0ConversionCtx ctx, Pixmap pix, int colorformat)
{
	if (!ctx->boPixmap) {
		tbm_bo_unref(ctx->boPixmap);
		ctx->boPixmap = NULL;
	}

	/* Free is immune for free before set. */
	free_intermediate_buffers(ctx);
	
	/* the width and height are taken from dri2 of the target pixmap.*/
	ctx->boPixmap = get_pixmap_bo(ctx->dpy, pix, ctx->bufmgr, &ctx->bo_Pixmap_width, &ctx->bo_Pixmap_height);
	if (!ctx->boPixmap) {
		return FALSE;
	}

	if ( ! set_intermediate_buffers(ctx, ctx->bo_Pixmap_width, ctx->bo_Pixmap_height, colorformat) ) {
		return FALSE;
	}
	return TRUE;
}

/* STV0 - TV HW Decoded frame (v4l2_drm structure) */
Bool stv0_scale_convert(STV0ConversionCtx ctx, struct v4l2_drm *in, Pixmap pix)
{
	/* TODO: Pixmap related data shouldn't be updated during every stv0_scale_convert,
	 * but only when needed. Due to pixmaps pool in a sink, simple changes detection of
	 * pixmap id is not enough. */
	struct v4l2_private_frame_info *frminfo;
	int colorformat;
	frminfo = &(in->u.dec_info.pFrame[0]);
	colorformat = frminfo->colorformat;

	GST_INFO("colorformat : %d", colorformat);

	if ( ! update_pixmap_related_data(ctx, pix, colorformat) ) {
		GST_ERROR("error: stv0_pixmap_update failed!");
		return FALSE;
	}

	if ( ! do_hw_tbm_scaling(ctx, in, ctx->bo_Pixmap_width, ctx->bo_Pixmap_height,colorformat) ) {
		GST_ERROR("error: do_hw_tbm_scaling failed!");
		return FALSE;
	}

	if ( ! do_colorspace_conversion(ctx, ctx->bo_Pixmap_width, ctx->bo_Pixmap_height,colorformat) ) {
		GST_ERROR("error: do_colorspace_conversion!");
		return FALSE;
	}

	return TRUE;
}
