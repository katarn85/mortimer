/*
 * camerasrc
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jeongmo Yang <jm80.yang@samsung.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "gstcamerasrccontrol.h"
#include "camerasrc-common.h"

#define gst_camerasrc_debug(fmt, args...)       GST_INFO(fmt, ##args)

#define CAMERA_CONTROL_AF_STOP_TOTALTIME        2000000
#define CAMERA_CONTROL_AF_STOP_INTERVAL         20000

GST_BOILERPLATE (GstCamerasrcControlChannel,
    gst_camerasrc_control_channel,
    GstCameraControlChannel, GST_TYPE_CAMERA_CONTROL_CHANNEL);


static void
gst_camerasrc_control_channel_base_init( gpointer g_class )
{
	gst_camerasrc_debug( "" );
}

static void
gst_camerasrc_control_channel_class_init( GstCamerasrcControlChannelClass* klass )
{
	gst_camerasrc_debug( "" );
}

static void
gst_camerasrc_control_channel_init( GstCamerasrcControlChannel* control_channel, GstCamerasrcControlChannelClass* klass )
{
	gst_camerasrc_debug( "" );
	
	control_channel->id = (guint32) - 1;
}

static G_GNUC_UNUSED gboolean
gst_camerasrc_control_contains_channel( GstCameraSrc* camerasrc, GstCamerasrcControlChannel* camerasrc_control_channel )
{
	gst_camerasrc_debug( "" );
	
	const GList* item;

	for( item = camerasrc->camera_controls ; item != NULL ; item = item->next )
	{
		if( item->data == camerasrc_control_channel )
		{
			return TRUE;
		}
	}

	return FALSE;
}

const GList*
gst_camerasrc_control_list_channels( GstCameraSrc* camerasrc )
{
	gst_camerasrc_debug( "" );
	
	return camerasrc->camera_controls;
}

gboolean
gst_camerasrc_control_set_value( GstCameraSrc* camerasrc, GstCameraControlChannel *control_channel, gint value )
{
	gst_camerasrc_debug( "" );
	
	int error = CAMERASRC_ERROR;
	
	GstCamerasrcControlChannel *camerasrc_control_channel = GST_CAMERASRC_CONTROL_CHANNEL( control_channel );

	g_return_val_if_fail( camerasrc, FALSE );
	g_return_val_if_fail( gst_camerasrc_control_contains_channel( camerasrc, camerasrc_control_channel ), FALSE );

	error = camerasrc_set_control( camerasrc->v4l2_handle, camerasrc_control_channel->id, value );

	if( error != CAMERASRC_SUCCESS )
	{
		gst_camerasrc_debug( "Failed to set value. Ctrl-id [%d], value [%d], err code [%d]", camerasrc_control_channel->id, value, error );
		return FALSE;
	}

	return TRUE;
}

gboolean
gst_camerasrc_control_get_value( GstCameraSrc* camerasrc, GstCameraControlChannel *control_channel, gint *value )
{
	gst_camerasrc_debug( "" );
	
	int error = CAMERASRC_ERROR;
	
	GstCamerasrcControlChannel *camerasrc_control_channel = GST_CAMERASRC_CONTROL_CHANNEL( control_channel );

	g_return_val_if_fail( camerasrc, FALSE );
	g_return_val_if_fail( gst_camerasrc_control_contains_channel( camerasrc, camerasrc_control_channel ), FALSE );

	error = camerasrc_get_control( camerasrc->v4l2_handle, camerasrc_control_channel->id, value );

	if( error != CAMERASRC_SUCCESS )
	{
		gst_camerasrc_debug( "Failed to get control value. Ctrl-id [%d], err code[%x]", camerasrc_control_channel->id, error );
		return FALSE;
	}

	return TRUE;
}

gboolean
gst_camerasrc_control_set_capture_mode( GstCameraSrc* camerasrc, gint type, gint value )
{
	/* TODO : single/multishot select(capture mode), output mode, frame count, JPEG quality */

	gst_camerasrc_debug( "" );

	int error = CAMERASRC_ERROR;

	g_return_val_if_fail( camerasrc, FALSE );

	switch( type )
	{
		case GST_CAMERA_CONTROL_CAPTURE_MODE:
			break;
		case GST_CAMERA_CONTROL_OUTPUT_MODE:
			break;
		case GST_CAMERA_CONTROL_FRAME_COUNT:
			break;
		case GST_CAMERA_CONTROL_JPEG_QUALITY:
			break;
		default:
			gst_camerasrc_debug( "Not supported type." );
			return FALSE;
	}

	if( error != CAMERASRC_SUCCESS )
	{
		gst_camerasrc_debug( "Failed to set capture mode. Type[%d],value[%d],err code[%x]", type, value, error );
		return FALSE;
	}

	return TRUE;
}

gboolean
gst_camerasrc_control_get_capture_mode( GstCameraSrc* camerasrc, gint type, gint *value )
{
	/* TODO : single/multishot select(capture mode), output mode, frame count, JPEG quality */

	gst_camerasrc_debug( "" );

	int error = CAMERASRC_ERROR;

	g_return_val_if_fail( camerasrc, FALSE );

	switch( type )
	{
		case GST_CAMERA_CONTROL_CAPTURE_MODE:
			break;
		case GST_CAMERA_CONTROL_OUTPUT_MODE:
			break;
		case GST_CAMERA_CONTROL_FRAME_COUNT:
			break;
		case GST_CAMERA_CONTROL_JPEG_QUALITY:
			break;
		default:
			gst_camerasrc_debug( "Not supported type." );
			return FALSE;
	}

	if( error != CAMERASRC_SUCCESS )
	{
		gst_camerasrc_debug( "Failed to set capture mode. Type[%d],value[%d],err code[%x]", type, value, error );
		return FALSE;
	}

	return TRUE;
}

gboolean gst_camerasrc_control_get_basic_dev_info ( GstCameraSrc* camerasrc, gint dev_id, GstCameraControlCapsInfoType* info )
{
	gst_camerasrc_debug( "" );

	int error = CAMERASRC_ERROR;

	g_return_val_if_fail( camerasrc, FALSE );

	/**
	 * Just implementation issue, but at this time, we assume
	 * GstCameraControlCapsInfoType is exactly same with camerasrc_caps_info_t
	 * For performance.
	 * Here is plugin code. we can do like this?
	 */
#if 1
	error = camerasrc_read_basic_dev_info(dev_id, (camerasrc_caps_info_t*)info);
	if(error != CAMERASRC_SUCCESS)
	{
		return FALSE;
	}
#else
	int i, j, k;
	camerasrc_caps_info_t caps_info;

	error = camerasrc_read_basic_dev_info(dev_id, &caps_info);
	if(error != CAMERASRC_SUCCESS)
	{
		return FALSE;
	}

	if(caps_info.num_fmt_desc != 0)
	{
		info->num_fmt_desc = caps_info.num_fmt_desc;
		for (i=0; i<caps_info.num_fmt_desc; i++) {
		if(caps_info.fmt_desc[i].num_resolution != 0)
		{
			info->fmt_desc[i].fcc = caps_info.fmt_desc[i].fcc;
			info->fmt_desc[i].num_resolution = caps_info.fmt_desc[i].num_resolution;
			for (j=0; j<caps_info.fmt_desc[i].num_resolution; j++)
			{
			if(caps_info.fmt_desc[i].resolutions[j].num_avail_tpf != 0)
			{
				info->fmt_desc[i].resolutions[j].w = caps_info.fmt_desc[i].resolutions[j].w;
				info->fmt_desc[i].resolutions[j].h = caps_info.fmt_desc[i].resolutions[j].h;
				info->fmt_desc[i].resolutions[j].num_avail_tpf = caps_info.fmt_desc[i].resolutions[j].num_avail_tpf;
				for(k=0; k< caps_info.fmt_desc[i].resolutions[j].num_avail_tpf; k++)
				{
				info->fmt_desc[i].resolutions[j].tpf[k].num = caps_info.fmt_desc[i].resolutions[j].tpf[k].num;
				info->fmt_desc[i].resolutions[j].tpf[k].den = caps_info.fmt_desc[i].resolutions[j].tpf[k].den;
				}
			}
			else
			{
				/* No available timeperframe */
				return FALSE;
			}
			}
		}
		else
		{
			/* No available resolution set */
			return FALSE;
		}
		}
	}
	else
	{
		/* No available image format(fourcc) */
		return FALSE;
	}
#endif
	return TRUE;
}

gboolean gst_camerasrc_control_get_misc_dev_info( GstCameraSrc* camerasrc, gint dev_id, GstCameraControlCtrlListInfoType * info)
{
	gst_camerasrc_debug( "" );

	int error = CAMERASRC_ERROR;

	g_return_val_if_fail( camerasrc, FALSE );

	/**
	 * Just implementation issue, but at this time, we assume
	 * GstCameraControlCtrlListInfoType is exactly same with camerasrc_ctrl_list_info_t
	 * For performance.
	 * Here is plugin code. we can do like this?
	 */
#if 1
	error = camerasrc_read_misc_dev_info(dev_id, (camerasrc_ctrl_list_info_t*)info);
	if(error != CAMERASRC_SUCCESS)
	{
		return FALSE;
	}
#else
	int i, j;
	camerasrc_ctrl_list_info_t ctrl_info;

	error = camerasrc_read_misc_dev_info(dev_id, &ctrl_info);
	if(error != CAMERASRC_SUCCESS)
	{
		return FALSE;
	}

	if(ctrl_info.num_ctrl_list_info != 0)
	{
		info->num_ctrl_list_info = ctrl_info.num_ctrl_list_info;
		for(i=0; i< ctrl_info.num_ctrl_list_info; i++)
		{
			info->ctrl_info[i].camerasrc_ctrl_id = ctrl_info.ctrl_info[i].camerasrc_ctrl_id;
			info->ctrl_info[i].v4l2_ctrl_id = ctrl_info.ctrl_info[i].v4l2_ctrl_id;
			info->ctrl_info[i].ctrl_type = ctrl_info.ctrl_info[i].ctrl_type;
			info->ctrl_info[i].max = ctrl_info.ctrl_info[i].max;
			info->ctrl_info[i].min = ctrl_info.ctrl_info[i].min;
			info->ctrl_info[i].step = ctrl_info.ctrl_info[i].step;
			info->ctrl_info[i].default_val = ctrl_info.ctrl_info[i].default_val;
			info->ctrl_info[i].num_ctrl_menu = ctrl_info.ctrl_info[i].num_ctrl_menu;
			memcpy(info->ctrl_info[i].ctrl_name,ctrl_info.ctrl_info[i].ctrl_name,MAX_SZ_CTRL_NAME_STRING);
			if(ctrl_info.ctrl_info[i].ctrl_type == CTRL_TYPE_ARRAY && ctrl_info.ctrl_info[i].num_ctrl_menu != 0)
			{
				for(j=0; j<ctrl_info.ctrl_info[i].num_ctrl_menu; j++)
				{
				info->ctrl_info[i].ctrl_menu[j].menu_index = ctrl_info.ctrl_info[i].ctrl_menu[j].menu_index;
				memcpy(info->ctrl_info[i].ctrl_menu[j].menu_name, ctrl_info.ctrl_info[i].ctrl_menu[j].menu_name, MAX_SZ_CTRL_NAME_STRING);
				}
			}
			else
			{
				/* Not a menu type or not available menus */
				return FALSE;
			}
		}
	}
	else
	{
		/* Not avaliable controls */
		return FALSE;
	}
#endif
	return TRUE;
}

void gst_camerasrc_control_set_capture_command( GstCameraSrc* camerasrc, GstCameraControlCaptureCommand cmd )
{
	gst_camerasrc_debug( "" );

	if( camerasrc == NULL )
	{
		gst_camerasrc_debug( "camerasrc is NULL" );
		return;
	}

	gst_camerasrc_set_capture_command( camerasrc, cmd );

	return;
}
