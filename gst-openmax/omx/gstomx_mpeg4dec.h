/*
 * Copyright (C) 2007-2009 Nokia Corporation.
 *
 * Author: Felipe Contreras <felipe.contreras@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef GSTOMX_MPEG4DEC_H
#define GSTOMX_MPEG4DEC_H

#include <gst/gst.h>
#include <stdint.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include "divxdrm/divx_drm.h"
G_BEGIN_DECLS
#define GST_OMX_MPEG4DEC(obj) (GstOmxMpeg4Dec *) (obj)
#define GST_OMX_MPEG4DEC_TYPE (gst_omx_mpeg4dec_get_type ())
typedef struct GstOmxMpeg4Dec GstOmxMpeg4Dec;
typedef struct GstOmxMpeg4DecClass GstOmxMpeg4DecClass;


#include "gstomx_base_videodec.h"


typedef enum drmErrorCodes
{
  DRM_SUCCESS = 0,
  DRM_NOT_AUTHORIZED,
  DRM_NOT_REGISTERED,
  DRM_RENTAL_EXPIRED,
  DRM_GENERAL_ERROR,
  DRM_NEVER_REGISTERED,
} drmErrorCodes_t;

typedef struct{
	int open_divx_drm_manager;
	void *handle_dl;
    int (*divx_drm_get_manager)(divx_drm_manager_h* manager);
    int (*divx_drm_close)(divx_drm_manager_h manager);
    int (*divx_drm_decoder_commit)(divx_drm_manager_h manager);
    int (*divx_drm_decoder_decrypt_video)(divx_drm_manager_h manager,unsigned char *frame,unsigned int frame_size);
} divx_drm_symbol;


struct GstOmxMpeg4Dec
{
  GstOmxBaseVideoDec omx_base;
  divx_drm_symbol func;
  divx_drm_manager_h divx_manager;
};

struct GstOmxMpeg4DecClass
{
  GstOmxBaseVideoDecClass parent_class;
};

GType gst_omx_mpeg4dec_get_type (void);

/********************  Mpeg4Dec 1 **********************/
#define GST_OMX_MPEG4DEC1(obj) (GstOmxMpeg4Dec1 *) (obj)
#define GST_OMX_MPEG4DEC1_TYPE (gst_omx_mpeg4dec1_get_type ())
typedef struct GstOmxMpeg4Dec1 GstOmxMpeg4Dec1;
typedef struct GstOmxMpeg4Dec1Class GstOmxMpeg4Dec1Class;

struct GstOmxMpeg4Dec1
{
  GstOmxBaseVideoDec omx_base;
  divx_drm_symbol func;
  divx_drm_manager_h divx_manager;
};

struct GstOmxMpeg4Dec1Class
{
  GstOmxBaseVideoDecClass parent_class;
};

GType gst_omx_mpeg4dec1_get_type (void);

G_END_DECLS
#endif /* GSTOMX_MPEG4DEC_H */
