/* GStreamer
 * Copyright (c) 2012, 2013 Samsung Electronics Co., Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <glib.h>               /* for G_OS_WIN32 */
#include <gst/gstinfo.h>        /* For GST_STR_NULL */
#include <gst/gst.h>
#include "wfdconfigmessage.h"
#include <mm_debug.h>

/* FIXME, is currently allocated on the stack */
#define MAX_LINE_LEN    1024 * 16

#define FREE_STRING(field)              if(field != NULL) g_free (field); (field) = NULL
#define REPLACE_STRING(field, val)      FREE_STRING(field); (field) = g_strdup (val)
#define EDID_BLOCK_SIZE 128
#define LIBHDCP "/usr/lib/libhdcp library"
enum
{
  WFD_SESSION,
  WFD_MEDIA,
};

typedef struct
{
  guint state;
  WFDMessage *msg;
} WFDContext;

/**
* wfdconfig_message_new:
* @msg: pointer to new #WFDMessage
*
* Allocate a new WFDMessage and store the result in @msg.
*
* Returns: a #WFDResult.
*/
WFDResult
wfdconfig_message_new (WFDMessage ** msg)
{
  WFDMessage *newmsg;

  g_return_val_if_fail (msg != NULL, WFD_EINVAL);

  newmsg = g_new0 (WFDMessage, 1);

  *msg = newmsg;

  return wfdconfig_message_init (newmsg);
}

/**
* wfdconfig_message_init:
* @msg: a #WFDMessage
*
* Initialize @msg so that its contents are as if it was freshly allocated
* with wfdconfig_message_new(). This function is mostly used to initialize a message
* allocated on the stack. wfdconfig_message_uninit() undoes this operation.
*
* When this function is invoked on newly allocated data (with malloc or on the
* stack), its contents should be set to 0 before calling this function.
*
* Returns: a #WFDResult.
*/
WFDResult
wfdconfig_message_init (WFDMessage * msg)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);

  return WFD_OK;
}

/**
* wfdconfig_message_uninit:
* @msg: a #WFDMessage
*
* Free all resources allocated in @msg. @msg should not be used anymore after
* this function. This function should be used when @msg was allocated on the
* stack and initialized with wfdconfig_message_init().
*
* Returns: a #WFDResult.
*/
WFDResult
wfdconfig_message_uninit (WFDMessage * msg)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);

  if(msg->audio_codecs) {
    guint i=0;
    if(msg->audio_codecs->list) {
      for(; i<msg->audio_codecs->count; i++) {
        FREE_STRING(msg->audio_codecs->list[i].audio_format);
        msg->audio_codecs->list[i].modes=0;
        msg->audio_codecs->list[i].latency=0;
      }
      FREE_STRING(msg->audio_codecs->list);
    }
    FREE_STRING(msg->audio_codecs);
  }

  if(msg->video_formats) {
    FREE_STRING(msg->video_formats->list);
    FREE_STRING(msg->video_formats);
  }

  if(msg->video_3d_formats) {
    FREE_STRING(msg->video_3d_formats);
  }

  if(msg->content_protection) {
    FREE_STRING(msg->content_protection);
  }

  if(msg->display_edid) {
    if(msg->display_edid->edid_payload)
      FREE_STRING(msg->display_edid->edid_payload);
    FREE_STRING(msg->display_edid);
  }

  if(msg->coupled_sink) {
    FREE_STRING(msg->coupled_sink);
  }

  if(msg->trigger_method) {
    FREE_STRING(msg->trigger_method->wfd_trigger_method);
    FREE_STRING(msg->trigger_method);
  }

  if(msg->presentation_url) {
    FREE_STRING(msg->trigger_method);
  }

  if(msg->client_rtp_ports) {
    FREE_STRING(msg->client_rtp_ports->profile);
    FREE_STRING(msg->client_rtp_ports->mode);
    FREE_STRING(msg->client_rtp_ports);
  }

  if(msg->route) {
    FREE_STRING(msg->route);
  }

  if(msg->I2C) {
    FREE_STRING(msg->I2C);
  }

  if(msg->av_format_change_timing) {
    FREE_STRING(msg->av_format_change_timing);
  }

  if(msg->preferred_display_mode) {
    FREE_STRING(msg->preferred_display_mode);
  }

  if(msg->uibc_capability) {
    FREE_STRING(msg->uibc_capability);
  }

  if(msg->uibc_setting) {
    FREE_STRING(msg->uibc_setting);
  }

  if(msg->standby_resume_capability) {
    FREE_STRING(msg->standby_resume_capability);
  }

  if(msg->standby) {
    FREE_STRING(msg->standby);
  }

  if(msg->connector_type) {
    FREE_STRING(msg->connector_type);
  }

  if(msg->idr_request) {
    FREE_STRING(msg->idr_request);
  }

  return WFD_OK;
}

/**
* wfdconfig_message_free:
* @msg: a #WFDMessage
*
* Free all resources allocated by @msg. @msg should not be used anymore after
* this function. This function should be used when @msg was dynamically
* allocated with wfdconfig_message_new().
*
* Returns: a #WFDResult.
*/
WFDResult
wfdconfig_message_free (WFDMessage * msg)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);

  wfdconfig_message_uninit (msg);
  g_free (msg);

  return WFD_OK;
}

/**
* wfdconfig_message_as_text:
* @msg: a #WFDMessage
*
* Convert the contents of @msg to a text string.
*
* Returns: A dynamically allocated string representing the WFD description.
*/
gchar *
wfdconfig_message_as_text (const WFDMessage * msg)
{
  /* change all vars so they match rfc? */
  GString *lines;
  g_return_val_if_fail (msg != NULL, NULL);

  lines = g_string_new ("");

  /* list of audio codecs */
  if(msg->audio_codecs) {
    guint i=0;
    g_string_append_printf (lines,"wfd_audio_codecs");
    if(msg->audio_codecs->list) {
      g_string_append_printf (lines,":");
      for(; i<msg->audio_codecs->count; i++) {
        g_string_append_printf (lines," %s",msg->audio_codecs->list[i].audio_format);
        g_string_append_printf (lines," %08x",msg->audio_codecs->list[i].modes);
        g_string_append_printf (lines," %02x",msg->audio_codecs->list[i].latency);
        if((i+1)<msg->audio_codecs->count)
          g_string_append_printf (lines,",");
      }
    }
    g_string_append_printf (lines,"\r\n");
  }

  /* list of video codecs */
  if(msg->video_formats) {
    g_string_append_printf (lines,"wfd_video_formats");
    if(msg->video_formats->list) {
      g_string_append_printf (lines,":");
      g_string_append_printf (lines," %02x", msg->video_formats->list->native);
      g_string_append_printf (lines," %02x", msg->video_formats->list->preferred_display_mode_supported);
      g_string_append_printf (lines," %02x", msg->video_formats->list->H264_codec.profile);
      g_string_append_printf (lines," %02x", msg->video_formats->list->H264_codec.level);
      g_string_append_printf (lines," %08x", msg->video_formats->list->H264_codec.misc_params.CEA_Support);
      g_string_append_printf (lines," %08x", msg->video_formats->list->H264_codec.misc_params.VESA_Support);
      g_string_append_printf (lines," %08x", msg->video_formats->list->H264_codec.misc_params.HH_Support);
      g_string_append_printf (lines," %02x", msg->video_formats->list->H264_codec.misc_params.latency);
      g_string_append_printf (lines," %04x", msg->video_formats->list->H264_codec.misc_params.min_slice_size);
      g_string_append_printf (lines," %04x", msg->video_formats->list->H264_codec.misc_params.slice_enc_params);
      g_string_append_printf (lines," %02x", msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support);

      if(msg->video_formats->list->H264_codec.max_hres)
        g_string_append_printf (lines," %04x", msg->video_formats->list->H264_codec.max_hres);
      else
        g_string_append_printf (lines," none");

      if(msg->video_formats->list->H264_codec.max_vres)
        g_string_append_printf (lines," %04x", msg->video_formats->list->H264_codec.max_vres);
      else
        g_string_append_printf (lines," none");
    }
    g_string_append_printf (lines,"\r\n");
  }

  /* list of video 3D codecs */
  if(msg->video_3d_formats) {
    g_string_append_printf (lines,"wfd_3d_video_formats");
    g_string_append_printf (lines,":");
    if(msg->video_3d_formats->list) {
      g_string_append_printf (lines," %02x", msg->video_3d_formats->list->native);
      g_string_append_printf (lines," %02x", msg->video_3d_formats->list->preferred_display_mode_supported);
      g_string_append_printf (lines," %02x", msg->video_3d_formats->list->H264_codec.profile);
      g_string_append_printf (lines," %02x", msg->video_3d_formats->list->H264_codec.level);
      g_string_append_printf (lines," %16x", msg->video_3d_formats->list->H264_codec.misc_params.video_3d_capability);
      g_string_append_printf (lines," %02x", msg->video_3d_formats->list->H264_codec.misc_params.latency);
      g_string_append_printf (lines," %04x", msg->video_3d_formats->list->H264_codec.misc_params.min_slice_size);
      g_string_append_printf (lines," %04x", msg->video_3d_formats->list->H264_codec.misc_params.slice_enc_params);
      g_string_append_printf (lines," %02x", msg->video_3d_formats->list->H264_codec.misc_params.frame_rate_control_support);
      if(msg->video_3d_formats->list->H264_codec.max_hres)
        g_string_append_printf (lines," %04x", msg->video_formats->list->H264_codec.max_hres);
      else
        g_string_append_printf (lines," none");
      if(msg->video_3d_formats->list->H264_codec.max_vres)
        g_string_append_printf (lines," %04x", msg->video_formats->list->H264_codec.max_vres);
      else
        g_string_append_printf (lines," none");
    } else {
      g_string_append_printf (lines," none");
    }
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->content_protection) {
    g_string_append_printf (lines,"wfd_content_protection");
    if(msg->content_protection->hdcp2_spec) {
      g_string_append_printf (lines,":");
      if( msg->content_protection->hdcp2_spec->hdcpversion) {
        g_string_append_printf (lines," %s", msg->content_protection->hdcp2_spec->hdcpversion);
        g_string_append_printf (lines," %s", msg->content_protection->hdcp2_spec->TCPPort);
      } else {
        g_string_append_printf (lines," none");
      }
    }
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->display_edid) {
    g_string_append_printf (lines,"wfd_display_edid");
    g_string_append_printf (lines,":");
    if(msg->display_edid->edid_supported) {
      g_string_append_printf (lines," %d", msg->display_edid->edid_supported);
      if(msg->display_edid->edid_block_count)
        g_string_append_printf (lines," %d", msg->display_edid->edid_block_count);
      else
        g_string_append_printf (lines," none");
    } else {
      g_string_append_printf (lines," none");
    }
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->coupled_sink) {
    g_string_append_printf (lines,"wfd_coupled_sink");
    g_string_append_printf (lines,":");
    if(msg->coupled_sink->coupled_sink_cap) {
      g_string_append_printf (lines," %02x", msg->coupled_sink->coupled_sink_cap->status);
      if(msg->coupled_sink->coupled_sink_cap->sink_address)
        g_string_append_printf (lines," %s", msg->coupled_sink->coupled_sink_cap->sink_address);
      else
        g_string_append_printf (lines," none");
    } else {
      g_string_append_printf (lines," none");
    }
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->trigger_method) {
    g_string_append_printf (lines,"wfd_trigger_method");
    g_string_append_printf (lines,":");
    g_string_append_printf (lines," %s", msg->trigger_method->wfd_trigger_method);
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->presentation_url) {
    g_string_append_printf (lines,"wfd_presentation_URL");
    g_string_append_printf (lines,":");
    if(msg->presentation_url->wfd_url0)
      g_string_append_printf (lines," %s", msg->presentation_url->wfd_url0);
    else
      g_string_append_printf (lines," none");
    if(msg->presentation_url->wfd_url1)
      g_string_append_printf (lines," %s", msg->presentation_url->wfd_url1);
    else
      g_string_append_printf (lines," none");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->client_rtp_ports) {
    g_string_append_printf (lines,"wfd_client_rtp_ports");
    if(msg->client_rtp_ports->profile) {
      g_string_append_printf (lines,":");
      g_string_append_printf (lines," %s", msg->client_rtp_ports->profile);
      g_string_append_printf (lines," %d", msg->client_rtp_ports->rtp_port0);
      g_string_append_printf (lines," %d", msg->client_rtp_ports->rtp_port1);
      g_string_append_printf (lines," %s", msg->client_rtp_ports->mode);
    }
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->route) {
    g_string_append_printf (lines,"wfd_route");
    g_string_append_printf (lines,":");
    g_string_append_printf (lines," %s", msg->route->destination);
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->I2C) {
    g_string_append_printf (lines,"wfd_I2C");
    g_string_append_printf (lines,":");
    if(msg->I2C->I2CPresent)
      g_string_append_printf (lines," %x", msg->I2C->I2C_port);
    else
      g_string_append_printf (lines," none");
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->av_format_change_timing) {
    g_string_append_printf (lines,"wfd_av_format_change_timing");
    g_string_append_printf (lines,":");
    g_string_append_printf (lines," %10llu", msg->av_format_change_timing->PTS);
    g_string_append_printf (lines," %10llu", msg->av_format_change_timing->DTS);
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->preferred_display_mode) {
    g_string_append_printf (lines,"wfd_preferred_display_mode");
    g_string_append_printf (lines,":");
    if(msg->preferred_display_mode->displaymodesupported) {
      g_string_append_printf (lines," %06llu", msg->preferred_display_mode->p_clock);
      g_string_append_printf (lines," %04x", msg->preferred_display_mode->H);
      g_string_append_printf (lines," %04x", msg->preferred_display_mode->HB);
      g_string_append_printf (lines," %04x", msg->preferred_display_mode->HSPOL_HSOFF);
      g_string_append_printf (lines," %04x", msg->preferred_display_mode->HSW);
      g_string_append_printf (lines," %04x", msg->preferred_display_mode->V);
      g_string_append_printf (lines," %04x", msg->preferred_display_mode->VB);
      g_string_append_printf (lines," %04x", msg->preferred_display_mode->VSPOL_VSOFF);
      g_string_append_printf (lines," %04x", msg->preferred_display_mode->VSW);
      g_string_append_printf (lines," %02x", msg->preferred_display_mode->VBS3D);
      g_string_append_printf (lines," %02x", msg->preferred_display_mode->V2d_s3d_modes);
      g_string_append_printf (lines," %02x", msg->preferred_display_mode->P_depth);
    } else g_string_append_printf (lines," none");
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->uibc_capability) {
    g_string_append_printf (lines,"wfd_uibc_capability");
    g_string_append_printf (lines,":");
    if(msg->uibc_capability->uibcsupported) {
      g_string_append_printf (lines," input_category_list=");
      if(msg->uibc_capability->input_category_list.input_cat) {
        guint32 tempcap = 0;
        if(msg->uibc_capability->input_category_list.input_cat & WFD_UIBC_INPUT_CAT_GENERIC) {
          tempcap |= WFD_UIBC_INPUT_CAT_GENERIC;
          g_string_append_printf (lines,"GENERIC");
          if(msg->uibc_capability->input_category_list.input_cat != tempcap) g_string_append_printf (lines,", ");
        }
        if(msg->uibc_capability->input_category_list.input_cat & WFD_UIBC_INPUT_CAT_HIDC) {
          tempcap |= WFD_UIBC_INPUT_CAT_HIDC;
          g_string_append_printf (lines,"HIDC");
          if(msg->uibc_capability->input_category_list.input_cat != tempcap) g_string_append_printf (lines,", ");
        }
      }
      else g_string_append_printf (lines,"none");
      g_string_append_printf (lines,";");
      g_string_append_printf (lines," generic_cap_list=");
      if(msg->uibc_capability->generic_cap_list.inp_type) {
        guint32 tempcap = 0;
        if(msg->uibc_capability->generic_cap_list.inp_type & WFD_UIBC_INPUT_TYPE_KEYBOARD) {
          tempcap |= WFD_UIBC_INPUT_TYPE_KEYBOARD;
          g_string_append_printf (lines,"Keyboard");
          if(msg->uibc_capability->generic_cap_list.inp_type != tempcap) g_string_append_printf (lines,", ");
        }
        if(msg->uibc_capability->generic_cap_list.inp_type & WFD_UIBC_INPUT_TYPE_MOUSE) {
          tempcap |= WFD_UIBC_INPUT_TYPE_MOUSE;
          g_string_append_printf (lines,"Mouse");
          if(msg->uibc_capability->generic_cap_list.inp_type != tempcap) g_string_append_printf (lines,", ");
        }
        if(msg->uibc_capability->generic_cap_list.inp_type & WFD_UIBC_INPUT_TYPE_SINGLETOUCH) {
          tempcap |= WFD_UIBC_INPUT_TYPE_SINGLETOUCH;
          g_string_append_printf (lines,"SingleTouch");
          if(msg->uibc_capability->generic_cap_list.inp_type != tempcap) g_string_append_printf (lines,", ");
        }
        if(msg->uibc_capability->generic_cap_list.inp_type & WFD_UIBC_INPUT_TYPE_MULTITOUCH) {
          tempcap |= WFD_UIBC_INPUT_TYPE_MULTITOUCH;
          g_string_append_printf (lines,"MultiTouch");
          if(msg->uibc_capability->generic_cap_list.inp_type != tempcap) g_string_append_printf (lines,", ");
        }
        if(msg->uibc_capability->generic_cap_list.inp_type & WFD_UIBC_INPUT_TYPE_JOYSTICK) {
          tempcap |= WFD_UIBC_INPUT_TYPE_JOYSTICK;
          g_string_append_printf (lines,"Joystick");
          if(msg->uibc_capability->generic_cap_list.inp_type != tempcap) g_string_append_printf (lines,", ");
        }
        if(msg->uibc_capability->generic_cap_list.inp_type & WFD_UIBC_INPUT_TYPE_CAMERA) {
          tempcap |= WFD_UIBC_INPUT_TYPE_CAMERA;
          g_string_append_printf (lines,"Camera");
          if(msg->uibc_capability->generic_cap_list.inp_type != tempcap) g_string_append_printf (lines,", ");
        }
        if(msg->uibc_capability->generic_cap_list.inp_type & WFD_UIBC_INPUT_TYPE_GESTURE) {
          tempcap |= WFD_UIBC_INPUT_TYPE_GESTURE;
          g_string_append_printf (lines,"Gesture");
          if(msg->uibc_capability->generic_cap_list.inp_type != tempcap) g_string_append_printf (lines,", ");
        }
        if(msg->uibc_capability->generic_cap_list.inp_type & WFD_UIBC_INPUT_TYPE_REMOTECONTROL) {
          tempcap |= WFD_UIBC_INPUT_TYPE_REMOTECONTROL;
          g_string_append_printf (lines,"RemoteControl");
          if(msg->uibc_capability->generic_cap_list.inp_type != tempcap) g_string_append_printf (lines,", ");
        }
      }
      else g_string_append_printf (lines,"none");
      g_string_append_printf (lines,";");
      g_string_append_printf (lines," hidc_cap_list=");
      if(msg->uibc_capability->hidc_cap_list.cap_count) {
        detailed_cap *temp_cap = msg->uibc_capability->hidc_cap_list.next;
        while(temp_cap)
        {
          if(temp_cap->p.inp_type == WFD_UIBC_INPUT_TYPE_KEYBOARD) g_string_append_printf (lines,"Keyboard");
          else if(temp_cap->p.inp_type == WFD_UIBC_INPUT_TYPE_MOUSE) g_string_append_printf (lines,"Mouse");
          else if(temp_cap->p.inp_type == WFD_UIBC_INPUT_TYPE_SINGLETOUCH) g_string_append_printf (lines,"SingleTouch");
          else if(temp_cap->p.inp_type == WFD_UIBC_INPUT_TYPE_MULTITOUCH) g_string_append_printf (lines,"MultiTouch");
          else if(temp_cap->p.inp_type == WFD_UIBC_INPUT_TYPE_JOYSTICK) g_string_append_printf (lines,"Joystick");
          else if(temp_cap->p.inp_type == WFD_UIBC_INPUT_TYPE_CAMERA) g_string_append_printf (lines,"Camera");
          else if(temp_cap->p.inp_type == WFD_UIBC_INPUT_TYPE_GESTURE) g_string_append_printf (lines,"Gesture");
          else if(temp_cap->p.inp_type == WFD_UIBC_INPUT_TYPE_REMOTECONTROL) g_string_append_printf (lines,"RemoteControl");
          g_string_append_printf (lines,"/");
          if(temp_cap->p.inp_path == WFD_UIBC_INPUT_PATH_INFRARED) g_string_append_printf (lines,"Infrared");
          else if(temp_cap->p.inp_path == WFD_UIBC_INPUT_PATH_USB) g_string_append_printf (lines,"USB");
          else if(temp_cap->p.inp_path == WFD_UIBC_INPUT_PATH_BT) g_string_append_printf (lines,"BT");
          else if(temp_cap->p.inp_path == WFD_UIBC_INPUT_PATH_ZIGBEE) g_string_append_printf (lines,"Zigbee");
          else if(temp_cap->p.inp_path == WFD_UIBC_INPUT_PATH_WIFI) g_string_append_printf (lines,"Wi-Fi");
          else if(temp_cap->p.inp_path == WFD_UIBC_INPUT_PATH_NOSP) g_string_append_printf (lines,"No-SP");
          temp_cap = temp_cap->next;
          if(temp_cap) g_string_append_printf (lines,", ");
        }
      } else g_string_append_printf (lines,"none");
      g_string_append_printf (lines,";");
      if(msg->uibc_capability->tcp_port) g_string_append_printf (lines," port=%x", msg->uibc_capability->tcp_port);
      else  g_string_append_printf (lines,"port=none");
    } else g_string_append_printf (lines," none");
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->uibc_setting) {
    g_string_append_printf (lines,"wfd_uibc_setting");
    g_string_append_printf (lines,":");
    if(msg->uibc_setting->uibc_setting)
      g_string_append_printf (lines," enable");
    else
      g_string_append_printf (lines," disable");
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->standby_resume_capability) {
    g_string_append_printf (lines,"wfd_standby_resume_capability");
    g_string_append_printf (lines,":");
    if(msg->standby_resume_capability->standby_resume_cap)
      g_string_append_printf (lines," supported");
    else
      g_string_append_printf (lines," none");
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->standby) {
    g_string_append_printf (lines,"wfd_standby");
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->connector_type) {
    g_string_append_printf (lines,"wfd_connector_type");
    g_string_append_printf (lines,":");
    if(msg->connector_type->connector_type)
      g_string_append_printf (lines," %02x", msg->connector_type->connector_type);
    else
      g_string_append_printf (lines," none");
    g_string_append_printf (lines,"\r\n");
  }

  if(msg->idr_request) {
    g_string_append_printf (lines,"wfd_idr_request");
    g_string_append_printf (lines,"\r\n");
  }

  //g_string_append_printf (lines,"\0");
  /*if(g_str_has_suffix (lines,"\r\n\0"))
  {
  guint32 length = g_strlen(lines);
  lines[length-2] = '\0';
  }*/
  return g_string_free (lines, FALSE);
}

gchar* wfdconfig_parameter_names_as_text (const WFDMessage *msg)
{
  /* change all vars so they match rfc? */
  GString *lines;
  g_return_val_if_fail (msg != NULL, NULL);

  lines = g_string_new ("");

  /* list of audio codecs */
  if(msg->audio_codecs) {
    g_string_append_printf (lines,"wfd_audio_codecs");
    g_string_append_printf (lines,"\r\n");
  }
  /* list of video codecs */
  if(msg->video_formats) {
    g_string_append_printf (lines,"wfd_video_formats");
    g_string_append_printf (lines,"\r\n");
  }
  /* list of video 3D codecs */
  if(msg->video_3d_formats) {
    g_string_append_printf (lines,"wfd_3d_video_formats");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->content_protection) {
    g_string_append_printf (lines,"wfd_content_protection");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->display_edid) {
    g_string_append_printf (lines,"wfd_display_edid");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->coupled_sink) {
    g_string_append_printf (lines,"wfd_coupled_sink");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->trigger_method) {
    g_string_append_printf (lines,"wfd_trigger_method");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->presentation_url) {
    g_string_append_printf (lines,"wfd_presentation_URL");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->client_rtp_ports) {
    g_string_append_printf (lines,"wfd_client_rtp_ports");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->route) {
    g_string_append_printf (lines,"wfd_route");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->I2C) {
    g_string_append_printf (lines,"wfd_I2C");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->av_format_change_timing) {
    g_string_append_printf (lines,"wfd_av_format_change_timing");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->preferred_display_mode) {
    g_string_append_printf (lines,"wfd_preferred_display_mode");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->uibc_capability) {
    g_string_append_printf (lines,"wfd_uibc_capability");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->uibc_setting) {
    g_string_append_printf (lines,"wfd_uibc_setting");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->standby_resume_capability) {
    g_string_append_printf (lines,"wfd_standby_resume_capability");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->standby) {
    g_string_append_printf (lines,"wfd_standby");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->connector_type) {
    g_string_append_printf (lines,"wfd_connector_type");
    g_string_append_printf (lines,"\r\n");
  }
  if(msg->idr_request) {
    g_string_append_printf (lines,"wfd_idr_request");
    g_string_append_printf (lines,"\r\n");
  }
  return g_string_free (lines, FALSE);
}

static int
hex_to_int (gchar c)
{
return c >= '0' && c <= '9' ? c - '0'
: c >= 'A' && c <= 'F' ? c - 'A' + 10
: c >= 'a' && c <= 'f' ? c - 'a' + 10 : 0;
}

static void
read_string (gchar * dest, guint size, gchar ** src)
{
  guint idx;

  idx = 0;
  /* skip spaces */
  while (g_ascii_isspace (**src))
    (*src)++;

  while (!g_ascii_isspace (**src) && **src != '\0') {
    if (idx < size - 1)
      dest[idx++] = **src;
    (*src)++;
  }

  if (size > 0)
    dest[idx] = '\0';
}

static void
read_string_del (gchar * dest, guint size, gchar del, gchar ** src)
{
  guint idx;

  idx = 0;
  /* skip spaces */
  while (g_ascii_isspace (**src))
    (*src)++;

  while (**src != del && **src != '\0') {
    if (idx < size - 1)
      dest[idx++] = **src;
    (*src)++;
  }
  if (size > 0)
    dest[idx] = '\0';
}

static void
read_string_space_ended (gchar * dest, guint size, gchar * src)
{
  guint idx = 0;

  while (!g_ascii_isspace (*src) && *src != '\0') {
    if (idx < size - 1)
      dest[idx++] = *src;
    src++;
  }

  if (size > 0)
    dest[idx] = '\0';
}

static void
read_string_char_ended (gchar * dest, guint size, gchar del, gchar * src)
{
  guint idx = 0;

  while (*src != del && *src != '\0') {
    if (idx < size - 1)
      dest[idx++] = *src;
    src++;
  }

  if (size > 0)
    dest[idx] = '\0';
}

static void
read_string_type_and_value (gchar * type, gchar * value, guint tsize, guint vsize, gchar del, gchar * src)
{
  guint idx;

  idx = 0;
  while (*src != del && *src != '\0') {
    if (idx < tsize - 1)
      type[idx++] = *src;
    src++;
  }

  if (tsize > 0)
    type[idx] = '\0';

  src++;
  idx = 0;
  while (*src != '\0') {
    if (idx < vsize - 1)
      value[idx++] = *src;
    src++;
  }
  if (vsize > 0)
    value[idx] = '\0';
}

static gboolean
wfdconfig_parse_line (WFDMessage * msg, gchar * buffer)
{
  gchar type[8192];
  gchar value[8192];
  gchar temp[8192];
  gchar *p = buffer;
  gchar *v = value;

  #define WFD_SKIP_SPACE(q) if (*q && g_ascii_isspace (*q)) q++
  #define WFD_SKIP_COMMA(q) if (*q && g_ascii_ispunct (*q)) q++
  #define WFD_READ_STRING(field) read_string_space_ended (temp, sizeof (temp), v); v+=strlen(temp); REPLACE_STRING (field, temp)
  #define WFD_READ_CHAR_END_STRING(field, del) read_string_char_ended (temp, sizeof (temp), del, v); v+=strlen(temp); REPLACE_STRING (field, temp)
  #define WFD_READ_UINT32(field) read_string_space_ended (temp, sizeof (temp), v); v+=strlen(temp); field = strtoul (temp, NULL, 16)
  #define WFD_READ_UINT32_DIGIT(field) read_string_space_ended (temp, sizeof (temp), v); v+=strlen(temp); field = strtoul (temp, NULL, 10)

  //g_print("wfdconfig_parse_line input: %s\n", buffer);
  read_string_type_and_value (type, value, sizeof(type), sizeof(value), ':', p);
  //g_print("wfdconfig_parse_line type:%s value:%s\n", type, value);
  if(!g_strcmp0(type,"wfd_audio_codecs")) {
    msg->audio_codecs = g_new0 (WFDAudioCodeclist, 1);
    if(strlen(v)) {
      guint i=0;
      msg->audio_codecs->count= strlen(v)/16;
      msg->audio_codecs->list = g_new0 (WFDAudioCodec, msg->audio_codecs->count);
      for(;i<msg->audio_codecs->count;i++) {
        WFD_SKIP_SPACE(v);
        WFD_READ_STRING(msg->audio_codecs->list[i].audio_format);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->audio_codecs->list[i].modes);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->audio_codecs->list[i].latency);
        WFD_SKIP_COMMA(v);
      }
    }
  } else if(!g_strcmp0(type,"wfd_video_formats")) {
    msg->video_formats = g_new0 (WFDVideoCodeclist, 1);
    if(strlen(v)) {
      msg->video_formats->count = 1;
      msg->video_formats->list = g_new0 (WFDVideoCodec, 1);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_formats->list->native);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_formats->list->preferred_display_mode_supported);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_formats->list->H264_codec.profile);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_formats->list->H264_codec.level);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.CEA_Support);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.VESA_Support);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.HH_Support);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.latency);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.min_slice_size);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.slice_enc_params);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support);
      WFD_SKIP_SPACE(v);
      if(msg->video_formats->list->preferred_display_mode_supported == 1) {
        WFD_READ_UINT32(msg->video_formats->list->H264_codec.max_hres);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->video_formats->list->H264_codec.max_vres);
        WFD_SKIP_SPACE(v);
      }
    }
  }else if(!g_strcmp0(type,"wfd_3d_formats")) {
    msg->video_3d_formats = g_new0 (WFD3DFormats, 1);
    if(strlen(v)) {
      msg->video_3d_formats->count = 1;
      msg->video_3d_formats->list = g_new0 (WFD3dCapList, 1);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_3d_formats->list->native);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_3d_formats->list->preferred_display_mode_supported);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.profile);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.level);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.misc_params.video_3d_capability);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.misc_params.latency);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.misc_params.min_slice_size);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.misc_params.slice_enc_params);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32(msg->video_3d_formats->list->H264_codec.misc_params.frame_rate_control_support);
      WFD_SKIP_SPACE(v);
      if(msg->video_formats->list->preferred_display_mode_supported == 1) {
        WFD_READ_UINT32(msg->video_formats->list->H264_codec.max_hres);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->video_formats->list->H264_codec.max_vres);
        WFD_SKIP_SPACE(v);
      }
    }
  } else if(!g_strcmp0(type,"wfd_content_protection")) {
    msg->content_protection = g_new0 (WFDContentProtection, 1);
    if(strlen(v)) {
      WFD_SKIP_SPACE(v);
      msg->content_protection->hdcp2_spec = g_new0 (WFDHdcp2Spec, 1);
      if(strstr(v,"none")) {
	 msg->content_protection->hdcp2_spec->hdcpversion=g_strdup("none");
      } else {
        WFD_READ_STRING(msg->content_protection->hdcp2_spec->hdcpversion);
        WFD_SKIP_SPACE(v);
        WFD_READ_STRING(msg->content_protection->hdcp2_spec->TCPPort);
      }
    }
  } else if(!g_strcmp0(type,"wfd_display_edid")) {
    msg->display_edid = g_new0 (WFDDisplayEdid, 1);
    if(strlen(v)) {
      WFD_SKIP_SPACE(v);
      if(strstr(v,"none")) {
        msg->display_edid->edid_supported = 0;
      } else {
        msg->display_edid->edid_supported = 1;
        WFD_READ_UINT32(msg->display_edid->edid_block_count);
        WFD_SKIP_SPACE(v);
        if(msg->display_edid->edid_block_count) {
          guint32 payload_size = EDID_BLOCK_SIZE * msg->display_edid->edid_block_count;
          msg->display_edid->edid_payload = g_malloc(payload_size);
          memcpy(msg->display_edid->edid_payload, v, payload_size);
	    v += payload_size;
        } else v += strlen(v);
      }
    }
  } else if(!g_strcmp0(type,"wfd_coupled_sink")) {
    msg->coupled_sink = g_new0 (WFDCoupledSink, 1);
    if(strlen(v)) {
    msg->coupled_sink->coupled_sink_cap = g_new0 (WFDCoupled_sink_cap, 1);
    WFD_SKIP_SPACE(v);
    WFD_READ_UINT32(msg->coupled_sink->coupled_sink_cap->status);
    WFD_SKIP_SPACE(v);
    WFD_READ_STRING(msg->coupled_sink->coupled_sink_cap->sink_address);
    }
  } else if(!g_strcmp0(type,"wfd_trigger_method")) {
    msg->trigger_method = g_new0 (WFDTriggerMethod, 1);
    if(strlen(v)) {
      WFD_SKIP_SPACE(v);
      WFD_READ_STRING(msg->trigger_method->wfd_trigger_method);
    }
  } else if(!g_strcmp0(type,"wfd_presentation_URL")) {
    msg->presentation_url = g_new0 (WFDPresentationUrl, 1);
    if(strlen(v)) {
      WFD_SKIP_SPACE(v);
      WFD_READ_STRING(msg->presentation_url->wfd_url0);
      WFD_SKIP_SPACE(v);
      WFD_READ_STRING(msg->presentation_url->wfd_url1);
    }
  } else if(!g_strcmp0(type,"wfd_client_rtp_ports")) {
    msg->client_rtp_ports = g_new0 (WFDClientRtpPorts, 1);
    if(strlen(v)) {
      WFD_SKIP_SPACE(v);
      WFD_READ_STRING(msg->client_rtp_ports->profile);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32_DIGIT(msg->client_rtp_ports->rtp_port0);
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32_DIGIT(msg->client_rtp_ports->rtp_port1);
      WFD_SKIP_SPACE(v);
      WFD_READ_STRING(msg->client_rtp_ports->mode);
    }
  } else if(!g_strcmp0(type,"wfd_route")) {
    msg->route = g_new0 (WFDRoute, 1);
    if(strlen(v)) {
      WFD_SKIP_SPACE(v);
      WFD_READ_STRING(msg->route->destination);
    }
  } else if(!g_strcmp0(type,"wfd_I2C")) {
    msg->I2C = g_new0 (WFDI2C, 1);
    if(strlen(v)) {
      msg->I2C->I2CPresent = TRUE;
      WFD_SKIP_SPACE(v);
      WFD_READ_UINT32_DIGIT(msg->I2C->I2C_port);
      if(msg->I2C->I2C_port) msg->I2C->I2CPresent = TRUE;
    }
  } else if(!g_strcmp0(type,"wfd_av_format_change_timing")) {
    msg->av_format_change_timing = g_new0 (WFDAVFormatChangeTiming, 1);
    if(strlen(v)) {
    WFD_SKIP_SPACE(v);
    WFD_READ_UINT32(msg->av_format_change_timing->PTS);
    WFD_SKIP_SPACE(v);
    WFD_READ_UINT32(msg->av_format_change_timing->DTS);
    }
  } else if(!g_strcmp0(type,"wfd_preferred_display_mode")) {
    msg->preferred_display_mode = g_new0 (WFDPreferredDisplayMode, 1);
    if(strlen(v)) {
      WFD_SKIP_SPACE(v);
      if(!strstr(v,"none")) {
        msg->preferred_display_mode->displaymodesupported = FALSE;
      } else {
        WFD_READ_UINT32(msg->preferred_display_mode->p_clock);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->H);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->HB);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->HSPOL_HSOFF);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->HSW);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->V);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->VB);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->VSPOL_VSOFF);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->VSW);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->VBS3D);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->V2d_s3d_modes);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->P_depth);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.profile);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.level);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.CEA_Support);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.VESA_Support);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.HH_Support);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.latency);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.min_slice_size);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.slice_enc_params);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.misc_params.frame_rate_control_support);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.max_hres);
        WFD_SKIP_SPACE(v);
        WFD_READ_UINT32(msg->preferred_display_mode->H264_codec.max_vres);
        WFD_SKIP_SPACE(v);
      }
    }
  } else if(!g_strcmp0(type,"wfd_uibc_capability")) {
    msg->uibc_capability = g_new0 (WFDUibcCapability, 1);
    if(!strstr(v,"none")) {
      msg->uibc_capability->uibcsupported = FALSE;
    } else {
      gchar *tstring = NULL;
      msg->uibc_capability->uibcsupported = TRUE;
      WFD_READ_CHAR_END_STRING(tstring, '=');
      if(!g_strcmp0(tstring,"input_category_list")) {
        if(!strstr(v,"none")) {

        } else {
          gchar temp[8192];
          guint rem_len, read_len=0;
          WFD_READ_CHAR_END_STRING(tstring, ';');
          rem_len = strlen(tstring);
          do {
            read_string_char_ended (temp, 8192, ',', tstring+read_len);
            read_len += (strlen(temp)+1);
            if(!g_strcmp0(temp,"GENERIC")) msg->uibc_capability->input_category_list.input_cat |= WFD_UIBC_INPUT_CAT_GENERIC;
            else if(!g_strcmp0(temp,"HIDC")) msg->uibc_capability->input_category_list.input_cat |= WFD_UIBC_INPUT_CAT_HIDC;
            else msg->uibc_capability->input_category_list.input_cat |= WFD_UIBC_INPUT_CAT_UNKNOWN;
          } while(read_len < rem_len);
        }
      }
      WFD_READ_CHAR_END_STRING(tstring, '=');
      if(!g_strcmp0(tstring,"generic_cap_list=")) {
        if(!strstr(v,"none")) {
        } else {
          gchar temp[8192];
          guint rem_len, read_len=0;
          WFD_READ_CHAR_END_STRING(tstring, ';');
          rem_len = strlen(tstring);
          do {
            read_string_char_ended (temp, 8192, ',', tstring+read_len);
            read_len += (strlen(temp)+1);
            if(!g_strcmp0(temp,"Keyboard")) msg->uibc_capability->generic_cap_list.inp_type |= WFD_UIBC_INPUT_TYPE_KEYBOARD;
            else if(!g_strcmp0(temp,"Mouse")) msg->uibc_capability->generic_cap_list.inp_type |= WFD_UIBC_INPUT_TYPE_MOUSE;
            else if(!g_strcmp0(temp,"SingleTouch")) msg->uibc_capability->generic_cap_list.inp_type |= WFD_UIBC_INPUT_TYPE_SINGLETOUCH;
            else if(!g_strcmp0(temp,"MultiTouch")) msg->uibc_capability->generic_cap_list.inp_type |= WFD_UIBC_INPUT_TYPE_MULTITOUCH;
            else if(!g_strcmp0(temp,"Joystick")) msg->uibc_capability->generic_cap_list.inp_type |= WFD_UIBC_INPUT_TYPE_JOYSTICK;
            else if(!g_strcmp0(temp,"Camera")) msg->uibc_capability->generic_cap_list.inp_type |= WFD_UIBC_INPUT_TYPE_CAMERA;
            else if(!g_strcmp0(temp,"Gesture")) msg->uibc_capability->generic_cap_list.inp_type |= WFD_UIBC_INPUT_TYPE_GESTURE;
            else if(!g_strcmp0(temp,"RemoteControl")) msg->uibc_capability->generic_cap_list.inp_type |= WFD_UIBC_INPUT_TYPE_REMOTECONTROL;
            else msg->uibc_capability->generic_cap_list.inp_type |= WFD_UIBC_INPUT_TYPE_UNKNOWN;
          } while(read_len < rem_len);
        }
      }
      WFD_READ_CHAR_END_STRING(tstring, '=');
      if(!g_strcmp0(tstring,"hidc_cap_list=")) {
        if(!strstr(v,"none")) {
        } else {
          gchar temp[8192];
          gchar inp_type[8192];
          gchar inp_path[8192];
          guint rem_len, read_len=0;
          detailed_cap *temp_cap;
          WFD_READ_CHAR_END_STRING(tstring, ';');
          rem_len = strlen(tstring);
          msg->uibc_capability->hidc_cap_list.next = g_new0 (detailed_cap, 1);
          temp_cap = msg->uibc_capability->hidc_cap_list.next;
          do {
            msg->uibc_capability->hidc_cap_list.cap_count++;
            read_string_char_ended (temp, 8192, ',', tstring+read_len);
            read_len += (strlen(temp)+1);
            read_string_type_and_value (inp_type, inp_path, sizeof(inp_type), sizeof(inp_path), '/', temp);
            if(!strstr(inp_type,"Keyboard")) temp_cap->p.inp_type = WFD_UIBC_INPUT_TYPE_KEYBOARD;
            else if(!strstr(inp_type,"Mouse")) temp_cap->p.inp_type = WFD_UIBC_INPUT_TYPE_MOUSE;
            else if(!strstr(inp_type,"SingleTouch")) temp_cap->p.inp_type = WFD_UIBC_INPUT_TYPE_SINGLETOUCH;
            else if(!strstr(inp_type,"MultiTouch")) temp_cap->p.inp_type = WFD_UIBC_INPUT_TYPE_MULTITOUCH;
            else if(!strstr(inp_type,"Joystick")) temp_cap->p.inp_type = WFD_UIBC_INPUT_TYPE_JOYSTICK;
            else if(!strstr(inp_type,"Camera")) temp_cap->p.inp_type = WFD_UIBC_INPUT_TYPE_CAMERA;
            else if(!strstr(inp_type,"Gesture")) temp_cap->p.inp_type = WFD_UIBC_INPUT_TYPE_GESTURE;
            else if(!strstr(inp_type,"RemoteControl")) temp_cap->p.inp_type = WFD_UIBC_INPUT_TYPE_REMOTECONTROL;
            else temp_cap->p.inp_type = WFD_UIBC_INPUT_TYPE_UNKNOWN;

            if(!strstr(inp_path,"Infrared")) temp_cap->p.inp_path = WFD_UIBC_INPUT_PATH_INFRARED;
            else if(!strstr(inp_path,"USB")) temp_cap->p.inp_path = WFD_UIBC_INPUT_PATH_USB;
            else if(!strstr(inp_path,"BT")) temp_cap->p.inp_path = WFD_UIBC_INPUT_PATH_BT;
            else if(!strstr(inp_path,"Zigbee")) temp_cap->p.inp_path = WFD_UIBC_INPUT_PATH_ZIGBEE;
            else if(!strstr(inp_path,"Wi-Fi")) temp_cap->p.inp_path = WFD_UIBC_INPUT_PATH_WIFI;
            else if(!strstr(inp_path,"No-SP")) temp_cap->p.inp_path = WFD_UIBC_INPUT_PATH_NOSP;
            else temp_cap->p.inp_path = WFD_UIBC_INPUT_PATH_UNKNOWN;
            if(read_len < rem_len) {
              temp_cap->next = g_new0 (detailed_cap, 1);
              temp_cap = temp_cap->next;
            }
          } while(read_len < rem_len);
        }
      }
      WFD_READ_CHAR_END_STRING(tstring, '=');
      if(!g_strcmp0(tstring,"hidc_cap_list=")) {
        if(!strstr(v,"none")) {
        } else {
          WFD_READ_UINT32(msg->uibc_capability->tcp_port);
        }
      }
    }
  } else if(!g_strcmp0(type,"wfd_uibc_setting")) {
    msg->uibc_setting = g_new0 (WFDUibcSetting, 1);
    if(strlen(v)) {
      WFD_SKIP_SPACE(v);
      if(!g_strcmp0(v, "enable"))
        msg->uibc_setting->uibc_setting = TRUE;
      else
        msg->uibc_setting->uibc_setting = FALSE;
    }
  } else if(!g_strcmp0(type,"wfd_standby_resume_capability")) {
    msg->standby_resume_capability = g_new0 (WFDStandbyResumeCapability, 1);
    if(strlen(v)) {
      WFD_SKIP_SPACE(v);
      if(!g_strcmp0(v, "supported"))
        msg->standby_resume_capability->standby_resume_cap = TRUE;
      else
        msg->standby_resume_capability->standby_resume_cap = FALSE;
    }
  } else if(!g_strcmp0(type,"wfd_standby")) {
    msg->standby = g_new0 (WFDStandby, 1);
    msg->standby->wfd_standby = TRUE;
  } else if(!g_strcmp0(type,"wfd_connector_type")) {
    msg->connector_type = g_new0 (WFDConnectorType, 1);
    if(strlen(v)) {
    msg->connector_type->supported = TRUE;
    WFD_SKIP_SPACE(v);
    WFD_READ_UINT32(msg->connector_type->connector_type);
    }
  } else if(!g_strcmp0(type,"wfd_idr_request")) {
    msg->idr_request = g_new0 (WFDIdrRequest, 1);
    msg->idr_request->idr_request = TRUE;
  }

  return TRUE;
}

/**
* wfdconfig_message_parse_buffer:
* @data: the start of the buffer
* @size: the size of the buffer
* @msg: the result #WFDMessage
*
* Parse the contents of @size bytes pointed to by @data and store the result in
* @msg.
*
* Returns: #WFD_OK on success.
*/
WFDResult
wfdconfig_message_parse_buffer (const guint8 * data, guint size, WFDMessage * msg)
{
  gchar *p;
  gchar buffer[MAX_LINE_LEN];
  guint idx = 0;

  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  g_return_val_if_fail (data != NULL, WFD_EINVAL);
  g_return_val_if_fail (size != 0, WFD_EINVAL);

  GST_LOG("wfdconfig_message_parse_buffer input: %s\n", data);

  p = (gchar *) data;
  while (TRUE) {

    if (*p == '\0')
    break;

    idx = 0;
    while (*p != '\n' && *p != '\r' && *p != '\0') {
      if (idx < sizeof (buffer) - 1)
        buffer[idx++] = *p;
      p++;
    }
    buffer[idx] = '\0';
    wfdconfig_parse_line (msg, buffer);

    if (*p == '\0')
      break;
    p+=2;
  }

  return WFD_OK;
}

/**
* wfdconfig_message_dump:
* @msg: a #WFDMessage
*
* Dump the parsed contents of @msg to stdout.
*
* Returns: a #WFDResult.
*/
WFDResult
wfdconfig_message_dump (const WFDMessage * msg)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  debug_log("===========WFD Message dump=========");

  if(msg->audio_codecs) {
    guint i=0;
    debug_log("Audio supported formats : \n");
    for(;i<msg->audio_codecs->count;i++) {
      debug_log("Codec: %s\n",msg->audio_codecs->list[i].audio_format);
      if(!strcmp(msg->audio_codecs->list[i].audio_format,"LPCM")) {
        if(msg->audio_codecs->list[i].modes & WFD_FREQ_44100)
          debug_log("	Freq: %d\n", 44100);
        if(msg->audio_codecs->list[i].modes & WFD_FREQ_48000)
          debug_log("	Freq: %d\n", 48000);
        debug_log("	Channels: %d\n", 2);
      }
      if(!strcmp(msg->audio_codecs->list[i].audio_format,"AAC")) {
        debug_log("	Freq: %d\n", 48000);
        if(msg->audio_codecs->list[i].modes & WFD_CHANNEL_2)
          debug_log("	Channels: %d\n", 2);
        if(msg->audio_codecs->list[i].modes & WFD_CHANNEL_4)
          debug_log("	Channels: %d\n", 4);
        if(msg->audio_codecs->list[i].modes & WFD_CHANNEL_6)
          debug_log("	Channels: %d\n", 6);
        if(msg->audio_codecs->list[i].modes & WFD_CHANNEL_8)
          debug_log("	Channels: %d\n", 8);
      }
      if(!strcmp(msg->audio_codecs->list[i].audio_format,"AC3")) {
        debug_log("	Freq: %d\n", 48000);
        if(msg->audio_codecs->list[i].modes & WFD_CHANNEL_2)
          debug_log("	Channels: %d\n", 2);
        if(msg->audio_codecs->list[i].modes & WFD_CHANNEL_4)
          debug_log("	Channels: %d\n", 4);
        if(msg->audio_codecs->list[i].modes & WFD_CHANNEL_6)
          debug_log("	Channels: %d\n", 6);
      }
      debug_log("	Bitwidth: %d\n", 16);
      debug_log("	Latency: %d\n", msg->audio_codecs->list[i].latency);
    }
  }


  if(msg->video_formats) {
    debug_log("Video supported formats : \n");
    if(msg->video_formats->list) {
      debug_log("Codec: H264\n");
      guint nativeindex = 0;
      if((msg->video_formats->list->native & 0x7) == WFD_VIDEO_CEA_RESOLUTION) {
        debug_log ("	Native type: CEA\n");
      } else if((msg->video_formats->list->native & 0x7) == WFD_VIDEO_VESA_RESOLUTION) {
        debug_log ("	Native type: VESA\n");
      } else if((msg->video_formats->list->native & 0x7) == WFD_VIDEO_HH_RESOLUTION) {
        debug_log ("	Native type: HH\n");
      }
      nativeindex = msg->video_formats->list->native >> 3;
      debug_log ("	Resolution: %d\n", (1 << nativeindex));

      if(msg->video_formats->list->H264_codec.profile & WFD_H264_BASE_PROFILE) {
        debug_log ("	Profile: BASE\n");
      } else if(msg->video_formats->list->H264_codec.profile & WFD_H264_HIGH_PROFILE) {
        debug_log ("	Profile: HIGH\n");
      } if(msg->video_formats->list->H264_codec.level & WFD_H264_LEVEL_3_1) {
        debug_log ("	Level: 3.1\n");
      } else if(msg->video_formats->list->H264_codec.level & WFD_H264_LEVEL_3_2) {
        debug_log ("	Level: 3.2\n");
      } else if(msg->video_formats->list->H264_codec.level & WFD_H264_LEVEL_4) {
        debug_log ("	Level: 4\n");
      } else if(msg->video_formats->list->H264_codec.level & WFD_H264_LEVEL_4_1) {
        debug_log ("	Level: 4.1\n");
      } else if(msg->video_formats->list->H264_codec.level & WFD_H264_LEVEL_4_2) {
        debug_log ("	Level: 4.2\n");
      }
      debug_log ("	Latency: %d\n", msg->video_formats->list->H264_codec.misc_params.latency);
      debug_log ("	min_slice_size: %x\n", msg->video_formats->list->H264_codec.misc_params.min_slice_size);
      debug_log ("	slice_enc_params: %x\n", msg->video_formats->list->H264_codec.misc_params.slice_enc_params);
      debug_log ("	frame_rate_control_support: %x\n", msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support);
      if(msg->video_formats->list->H264_codec.max_hres) {
        debug_log ("	Max Height: %04d\n", msg->video_formats->list->H264_codec.max_hres);
      }
      if(msg->video_formats->list->H264_codec.max_vres) {
        debug_log ("	Max Width: %04d\n", msg->video_formats->list->H264_codec.max_vres);
      }
    }
  }

  if(msg->video_3d_formats) {
    debug_log ("wfd_3d_formats");
    debug_log ("\r\n");
  }

  if(msg->content_protection) {
    debug_log ("wfd_content_protection");
    debug_log ("\r\n");
  }

  if(msg->display_edid) {
    debug_log ("wfd_display_edid");
    debug_log ("\r\n");
  }

  if(msg->coupled_sink) {
    debug_log ("wfd_coupled_sink");
    debug_log ("\r\n");
  }

  if(msg->trigger_method) {
    debug_log ("	Trigger type: %s\n", msg->trigger_method->wfd_trigger_method);
  }

  if(msg->presentation_url) {
    debug_log ("wfd_presentation_URL");
    debug_log ("\r\n");
  }

  if(msg->client_rtp_ports) {
    debug_log(" Client RTP Ports : \n");
    if(msg->client_rtp_ports->profile) {
      debug_log ("%s\n", msg->client_rtp_ports->profile);
      debug_log ("	%d\n", msg->client_rtp_ports->rtp_port0);
      debug_log ("	%d\n", msg->client_rtp_ports->rtp_port1);
      debug_log ("	%s\n", msg->client_rtp_ports->mode);
    }
  }

  if(msg->route) {
    debug_log ("wfd_route");
    debug_log ("\r\n");
  }

  if(msg->I2C) {
    debug_log ("wfd_I2C");
    debug_log ("\r\n");
  }

  if(msg->av_format_change_timing) {
    debug_log ("wfd_av_format_change_timing");
    debug_log ("\r\n");
  }

  if(msg->preferred_display_mode) {
    debug_log ("wfd_preferred_display_mode");
    debug_log ("\r\n");
  }

  if(msg->uibc_capability) {
    debug_log ("wfd_uibc_capability");
    debug_log ("\r\n");
  }

  if(msg->uibc_setting) {
    debug_log ("wfd_uibc_setting");
    debug_log ("\r\n");
  }

  if(msg->standby_resume_capability) {
    debug_log ("wfd_standby_resume_capability");
    debug_log ("\r\n");
  }

  if(msg->standby) {
    debug_log ("wfd_standby");
    debug_log ("\r\n");
  }

  if(msg->connector_type) {
    debug_log ("wfd_connector_type");
    debug_log ("\r\n");
  }

  if(msg->idr_request) {
    debug_log ("wfd_idr_request");
    debug_log ("\r\n");
  }
  debug_log("===============================================\n");
  return WFD_OK;
}

WFDResult wfdconfig_set_supported_audio_format(WFDMessage *msg, WFDAudioFormats aCodec, guint aFreq, guint aChanels,
                                                                                              guint aBitwidth, guint32 aLatency)
{
  guint temp = aCodec;
  guint i = 0;
  guint pcm = 0, aac = 0, ac3 = 0;

  g_return_val_if_fail (msg != NULL, WFD_EINVAL);

  if(!msg->audio_codecs)
    msg->audio_codecs = g_new0 (WFDAudioCodeclist, 1);

  if(aCodec != WFD_AUDIO_UNKNOWN) {
    while(temp) {
      msg->audio_codecs->count++;
      temp >>= 1;
    }
    msg->audio_codecs->list = g_new0 (WFDAudioCodec, msg->audio_codecs->count);
    for(; i<msg->audio_codecs->count; i++) {
      if((aCodec & WFD_AUDIO_LPCM) && (!pcm)) {
        msg->audio_codecs->list[i].audio_format = g_strdup("LPCM");
        msg->audio_codecs->list[i].modes = aFreq;
        msg->audio_codecs->list[i].latency = aLatency;
        pcm = 1;
      } else if((aCodec & WFD_AUDIO_AAC) && (!aac)) {
        msg->audio_codecs->list[i].audio_format = g_strdup("AAC");
        msg->audio_codecs->list[i].modes = aChanels;
        msg->audio_codecs->list[i].latency = aLatency;
        aac = 1;
      } else if((aCodec & WFD_AUDIO_AC3) && (!ac3)) {
        msg->audio_codecs->list[i].audio_format = g_strdup("AC3");
        msg->audio_codecs->list[i].modes = aChanels;
        msg->audio_codecs->list[i].latency = aLatency;
        ac3 = 1;
      }
    }
  }
  return WFD_OK;
}

WFDResult wfdconfig_set_prefered_audio_format(WFDMessage *msg, WFDAudioFormats aCodec, WFDAudioFreq aFreq, WFDAudioChannels aChanels,
                                                                                          guint aBitwidth, guint32 aLatency)
{

  g_return_val_if_fail (msg != NULL, WFD_EINVAL);

  if(!msg->audio_codecs)
    msg->audio_codecs = g_new0 (WFDAudioCodeclist, 1);

  msg->audio_codecs->list = g_new0 (WFDAudioCodec, 1);
  msg->audio_codecs->count = 1;
  if(aCodec == WFD_AUDIO_LPCM) {
    msg->audio_codecs->list->audio_format = g_strdup("LPCM");
    msg->audio_codecs->list->modes = aFreq;
    msg->audio_codecs->list->latency = aLatency;
  } else if(aCodec == WFD_AUDIO_AAC) {
    msg->audio_codecs->list->audio_format = g_strdup("AAC");
    msg->audio_codecs->list->modes = aChanels;
    msg->audio_codecs->list->latency = aLatency;
  } else if(aCodec == WFD_AUDIO_AC3) {
    msg->audio_codecs->list->audio_format = g_strdup("AC3");
    msg->audio_codecs->list->modes = aChanels;
    msg->audio_codecs->list->latency = aLatency;
  }
  return WFD_OK;
}

WFDResult wfdconfig_get_supported_audio_format(WFDMessage *msg, guint *aCodec, guint *aFreq, guint *aChanels,
                                                                                             guint *aBitwidth, guint32 *aLatency)
{
  guint i = 0;
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  g_return_val_if_fail (msg->audio_codecs != NULL, WFD_EINVAL);

  for(; i<msg->audio_codecs->count; i++) {
    if(!g_strcmp0(msg->audio_codecs->list[i].audio_format,"LPCM")) {
      *aCodec |= WFD_AUDIO_LPCM;
      *aFreq |= msg->audio_codecs->list[i].modes;
      *aChanels |= WFD_CHANNEL_2;
      *aBitwidth = 16;
      *aLatency = msg->audio_codecs->list[i].latency;
    } else if(!g_strcmp0(msg->audio_codecs->list[i].audio_format,"AAC")) {
      *aCodec |= WFD_AUDIO_AAC;
      *aFreq |= WFD_FREQ_48000;
      *aChanels |= msg->audio_codecs->list[i].modes;
      *aBitwidth = 16;
      *aLatency = msg->audio_codecs->list[i].latency;
    } else if(!g_strcmp0(msg->audio_codecs->list[i].audio_format,"AC3")) {
      *aCodec |= WFD_AUDIO_AC3;
      *aFreq |= WFD_FREQ_48000;
      *aChanels |= msg->audio_codecs->list[i].modes;
      *aBitwidth = 16;
      *aLatency = msg->audio_codecs->list[i].latency;
    }
  }
  return WFD_OK;
}

WFDResult wfdconfig_get_prefered_audio_format(WFDMessage *msg, WFDAudioFormats *aCodec, WFDAudioFreq *aFreq, WFDAudioChannels *aChanels,
                                                                                          guint *aBitwidth, guint32 *aLatency)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);

  if(!g_strcmp0(msg->audio_codecs->list->audio_format,"LPCM")) {
    *aCodec = WFD_AUDIO_LPCM;
    *aFreq = msg->audio_codecs->list->modes;
    *aChanels = WFD_CHANNEL_2;
    *aBitwidth = 16;
    *aLatency = msg->audio_codecs->list->latency;
  } else if(!g_strcmp0(msg->audio_codecs->list->audio_format,"AAC")) {
    *aCodec = WFD_AUDIO_AAC;
    *aFreq = WFD_FREQ_48000;
    *aChanels = msg->audio_codecs->list->modes;
    *aBitwidth = 16;
    *aLatency = msg->audio_codecs->list->latency;
  } else if(!g_strcmp0(msg->audio_codecs->list->audio_format,"AC3")) {
    *aCodec = WFD_AUDIO_AC3;
    *aFreq = WFD_FREQ_48000;
    *aChanels = msg->audio_codecs->list->modes;
    *aBitwidth = 16;
    *aLatency = msg->audio_codecs->list->latency;
  }
  return WFD_OK;
}

WFDResult wfdconfig_set_supported_video_format(WFDMessage *msg, WFDVideoCodecs vCodec,
                      WFDVideoNativeResolution vNative, guint64 vNativeResolution,
                      guint64 vCEAResolution, guint64 vVESAResolution, guint64 vHHResolution,
                      guint vProfile, guint vLevel, guint32 vLatency, guint32 vMaxHeight,
                      guint32 vMaxWidth, guint32 min_slice_size, guint32 slice_enc_params, guint frame_rate_control)
{
  guint nativeindex = 0;
  guint64 temp = vNativeResolution;

  g_return_val_if_fail (msg != NULL, WFD_EINVAL);

  if(!msg->video_formats)
  msg->video_formats = g_new0 (WFDVideoCodeclist, 1);

  if(vCodec != WFD_VIDEO_UNKNOWN) {
    msg->video_formats->list = g_new0 (WFDVideoCodec, 1);
    while(temp) {
      nativeindex++;
      temp >>= 1;
    }

    msg->video_formats->list->native = nativeindex-1;
    msg->video_formats->list->native <<= 3;

    if(vNative == WFD_VIDEO_VESA_RESOLUTION)
      msg->video_formats->list->native |= 1;
    else if(vNative == WFD_VIDEO_HH_RESOLUTION)
      msg->video_formats->list->native |= 2;

    msg->video_formats->list->preferred_display_mode_supported = 1;
    msg->video_formats->list->H264_codec.profile = vProfile;
    msg->video_formats->list->H264_codec.level = vLevel;
    msg->video_formats->list->H264_codec.max_hres = vMaxHeight;
    msg->video_formats->list->H264_codec.max_vres = vMaxWidth;
    msg->video_formats->list->H264_codec.misc_params.CEA_Support = vCEAResolution;
    msg->video_formats->list->H264_codec.misc_params.VESA_Support = vVESAResolution;
    msg->video_formats->list->H264_codec.misc_params.HH_Support = vHHResolution;
    msg->video_formats->list->H264_codec.misc_params.latency = vLatency;
    msg->video_formats->list->H264_codec.misc_params.min_slice_size = min_slice_size;
    msg->video_formats->list->H264_codec.misc_params.slice_enc_params = slice_enc_params;
    msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support = frame_rate_control;
  }
  return WFD_OK;
}

WFDResult wfdconfig_set_prefered_video_format(WFDMessage *msg, WFDVideoCodecs vCodec,
                  WFDVideoNativeResolution vNative, guint64 vNativeResolution,
                  WFDVideoCEAResolution vCEAResolution, WFDVideoVESAResolution vVESAResolution,
                  WFDVideoHHResolution vHHResolution,	WFDVideoH264Profile vProfile,
                  WFDVideoH264Level vLevel, guint32 vLatency, guint32 vMaxHeight,
                  guint32 vMaxWidth, guint32 min_slice_size, guint32 slice_enc_params, guint frame_rate_control)
{
  guint nativeindex = 0;
  guint64 temp = vNativeResolution;

  g_return_val_if_fail (msg != NULL, WFD_EINVAL);

  if(!msg->video_formats)
    msg->video_formats = g_new0 (WFDVideoCodeclist, 1);
  msg->video_formats->list = g_new0 (WFDVideoCodec, 1);

  while(temp) {
    nativeindex++;
    temp >>= 1;
  }

  if(nativeindex) msg->video_formats->list->native = nativeindex-1;
  msg->video_formats->list->native <<= 3;

  if(vNative == WFD_VIDEO_VESA_RESOLUTION)
    msg->video_formats->list->native |= 1;
  else if(vNative == WFD_VIDEO_HH_RESOLUTION)
    msg->video_formats->list->native |= 2;

  msg->video_formats->list->preferred_display_mode_supported = 0;
  msg->video_formats->list->H264_codec.profile = vProfile;
  msg->video_formats->list->H264_codec.level = vLevel;
  msg->video_formats->list->H264_codec.max_hres = vMaxHeight;
  msg->video_formats->list->H264_codec.max_vres = vMaxWidth;
  msg->video_formats->list->H264_codec.misc_params.CEA_Support = vCEAResolution;
  msg->video_formats->list->H264_codec.misc_params.VESA_Support = vVESAResolution;
  msg->video_formats->list->H264_codec.misc_params.HH_Support = vHHResolution;
  msg->video_formats->list->H264_codec.misc_params.latency = vLatency;
  msg->video_formats->list->H264_codec.misc_params.min_slice_size = min_slice_size;
  msg->video_formats->list->H264_codec.misc_params.slice_enc_params = slice_enc_params;
  msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support = frame_rate_control;
  return WFD_OK;
}

WFDResult wfdconfig_get_supported_video_format(WFDMessage *msg, WFDVideoCodecs *vCodec,
                  WFDVideoNativeResolution *vNative, guint64 *vNativeResolution,
                  guint64 *vCEAResolution, guint64 *vVESAResolution, guint64 *vHHResolution,
                  guint *vProfile, guint *vLevel, guint32 *vLatency, guint32 *vMaxHeight,
                  guint32 *vMaxWidth, guint32 *min_slice_size, guint32 *slice_enc_params, guint *frame_rate_control)
{
  guint nativeindex = 0;

  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  *vCodec = WFD_VIDEO_H264;
  *vNative = msg->video_formats->list->native & 0x7;
  nativeindex = msg->video_formats->list->native >> 3;
  *vNativeResolution = 1 << nativeindex;
  *vProfile = msg->video_formats->list->H264_codec.profile;
  *vLevel = msg->video_formats->list->H264_codec.level;
  *vMaxHeight = msg->video_formats->list->H264_codec.max_hres;
  *vMaxWidth = msg->video_formats->list->H264_codec.max_vres;
  *vCEAResolution = msg->video_formats->list->H264_codec.misc_params.CEA_Support;
  *vVESAResolution = msg->video_formats->list->H264_codec.misc_params.VESA_Support;
  *vHHResolution = msg->video_formats->list->H264_codec.misc_params.HH_Support;
  *vLatency = msg->video_formats->list->H264_codec.misc_params.latency;
  *min_slice_size = msg->video_formats->list->H264_codec.misc_params.min_slice_size;
  *slice_enc_params = msg->video_formats->list->H264_codec.misc_params.slice_enc_params;
  *frame_rate_control = msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support;
  return WFD_OK;
}

WFDResult wfdconfig_get_prefered_video_format(WFDMessage *msg, WFDVideoCodecs *vCodec,
                 WFDVideoNativeResolution *vNative, guint64 *vNativeResolution,
                 WFDVideoCEAResolution *vCEAResolution, WFDVideoVESAResolution *vVESAResolution,
                 WFDVideoHHResolution *vHHResolution,	WFDVideoH264Profile *vProfile,
                 WFDVideoH264Level *vLevel, guint32 *vLatency, guint32 *vMaxHeight,
                 guint32 *vMaxWidth, guint32 *min_slice_size, guint32 *slice_enc_params, guint *frame_rate_control)
{
  guint nativeindex = 0;
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);

  *vCodec = WFD_VIDEO_H264;
  *vNative = msg->video_formats->list->native & 0x7;
  nativeindex = msg->video_formats->list->native >> 3;
  *vNativeResolution = 1 << nativeindex;
  *vProfile = msg->video_formats->list->H264_codec.profile;
  *vLevel = msg->video_formats->list->H264_codec.level;
  *vMaxHeight = msg->video_formats->list->H264_codec.max_hres;
  *vMaxWidth = msg->video_formats->list->H264_codec.max_vres;
  *vCEAResolution = msg->video_formats->list->H264_codec.misc_params.CEA_Support;
  *vVESAResolution = msg->video_formats->list->H264_codec.misc_params.VESA_Support;
  *vHHResolution = msg->video_formats->list->H264_codec.misc_params.HH_Support;
  *vLatency = msg->video_formats->list->H264_codec.misc_params.latency;
  *min_slice_size = msg->video_formats->list->H264_codec.misc_params.min_slice_size;
  *slice_enc_params = msg->video_formats->list->H264_codec.misc_params.slice_enc_params;
  *frame_rate_control = msg->video_formats->list->H264_codec.misc_params.frame_rate_control_support;
  return WFD_OK;
}

WFDResult wfdconfig_set_contentprotection_type(WFDMessage *msg, WFDHDCPProtection hdcpversion, guint32 TCPPort)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);

 /*check LIBHDCP*/
  FILE *fp = NULL;
  fp = fopen(LIBHDCP, "r");
  if (fp == NULL) {
	return WFD_OK;
  }
  fclose(fp);

  if(!msg->content_protection) msg->content_protection = g_new0 (WFDContentProtection, 1);
  if(hdcpversion == WFD_HDCP_NONE) return WFD_OK;
  msg->content_protection->hdcp2_spec = g_new0 (WFDHdcp2Spec, 1);
  if(hdcpversion == WFD_HDCP_2_0) msg->content_protection->hdcp2_spec->hdcpversion = g_strdup("HDCP2.0");
  else if(hdcpversion == WFD_HDCP_2_1) msg->content_protection->hdcp2_spec->hdcpversion = g_strdup("HDCP2.1");
  char str[11]={0,};
  sprintf(str, "port=%d", TCPPort);
  msg->content_protection->hdcp2_spec->TCPPort = g_strdup(str);
  return WFD_OK;
}

WFDResult wfdconfig_get_contentprotection_type(WFDMessage *msg, WFDHDCPProtection *hdcpversion, guint32 *TCPPort)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(msg->content_protection && msg->content_protection->hdcp2_spec) {
  char *result = NULL;
  gint value= 0;
    if(!g_strcmp0(msg->content_protection->hdcp2_spec->hdcpversion,"none")) {
	*hdcpversion = WFD_HDCP_NONE;
	*TCPPort = 0;
	return WFD_OK;
    }
    if(!g_strcmp0(msg->content_protection->hdcp2_spec->hdcpversion,"HDCP2.0")) *hdcpversion = WFD_HDCP_2_0;
    else if(!g_strcmp0(msg->content_protection->hdcp2_spec->hdcpversion,"HDCP2.1")) *hdcpversion = WFD_HDCP_2_1;
    result = strtok(msg->content_protection->hdcp2_spec->TCPPort, "=");
    while (result !=NULL) {
	result = strtok(NULL, "=");
	*TCPPort = atoi (result);
	break;
    }
  } else *hdcpversion = WFD_HDCP_NONE;
  return WFD_OK;
}

WFDResult wfdconfig_set_display_EDID(WFDMessage *msg, gboolean edid_supported, guint32 edid_blockcount, gchar *edid_playload)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(!msg->display_edid) msg->display_edid = g_new0 (WFDDisplayEdid, 1);
  msg->display_edid->edid_supported = edid_supported;
  if(!edid_supported) return WFD_OK;
  msg->display_edid->edid_block_count = edid_blockcount;
  if(edid_blockcount) {
    msg->display_edid->edid_payload = g_malloc(128 * edid_blockcount);
    if(!msg->display_edid->edid_payload)
      memcpy(msg->display_edid->edid_payload, edid_playload, 128 * edid_blockcount);
  } else msg->display_edid->edid_payload = g_strdup("none");
  return WFD_OK;
}

WFDResult wfdconfig_get_display_EDID(WFDMessage *msg, gboolean *edid_supported, guint32 *edid_blockcount, gchar **edid_playload)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(msg->display_edid ) {
    if(msg->display_edid->edid_supported) {
      *edid_blockcount = msg->display_edid->edid_block_count;
      if(msg->display_edid->edid_block_count) {
        char * temp;
        temp = g_malloc(EDID_BLOCK_SIZE * msg->display_edid->edid_block_count);
        if(temp) {
	   memset(temp, 0, EDID_BLOCK_SIZE * msg->display_edid->edid_block_count);
          memcpy(temp, msg->display_edid->edid_payload, EDID_BLOCK_SIZE * msg->display_edid->edid_block_count);
	   *edid_playload = temp;
          *edid_supported = TRUE;
        }
      } else *edid_playload = g_strdup("none");
    }
  } else *edid_supported = FALSE;
  return WFD_OK;
}

WFDResult wfdconfig_set_coupled_sink(WFDMessage *msg, WFDCoupledSinkStatus status, gchar *sink_address)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(!msg->coupled_sink) msg->coupled_sink = g_new0 (WFDCoupledSink, 1);
  if(status == WFD_SINK_UNKNOWN) return WFD_OK;
  msg->coupled_sink->coupled_sink_cap = g_new0 (WFDCoupled_sink_cap, 1);
  msg->coupled_sink->coupled_sink_cap->status = status;
  msg->coupled_sink->coupled_sink_cap->sink_address = g_strdup(sink_address);
  return WFD_OK;
}

WFDResult wfdconfig_get_coupled_sink(WFDMessage *msg, WFDCoupledSinkStatus *status, gchar **sink_address)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(msg->coupled_sink && msg->coupled_sink->coupled_sink_cap) {
    *status = msg->coupled_sink->coupled_sink_cap->status;
    *sink_address = g_strdup(msg->coupled_sink->coupled_sink_cap->sink_address);
  } else *status = WFD_SINK_UNKNOWN;
  return WFD_OK;
}

WFDResult wfdconfig_set_trigger_type(WFDMessage *msg, WFDTrigger trigger)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);

  if(!msg->trigger_method)
    msg->trigger_method = g_new0 (WFDTriggerMethod, 1);
  if(trigger == WFD_TRIGGER_SETUP)
    msg->trigger_method->wfd_trigger_method = g_strdup("SETUP");
  else if(trigger == WFD_TRIGGER_PAUSE)
    msg->trigger_method->wfd_trigger_method = g_strdup("PAUSE");
  else if(trigger == WFD_TRIGGER_TEARDOWN)
    msg->trigger_method->wfd_trigger_method = g_strdup("TEARDOWN");
  else if(trigger == WFD_TRIGGER_PLAY)
    msg->trigger_method->wfd_trigger_method = g_strdup("PLAY");
  else
    return WFD_EINVAL;
  return WFD_OK;
}

WFDResult wfdconfig_get_trigger_type(WFDMessage *msg, WFDTrigger *trigger)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(!g_strcmp0(msg->trigger_method->wfd_trigger_method, "SETUP"))
    *trigger = WFD_TRIGGER_SETUP;
  else if(!g_strcmp0(msg->trigger_method->wfd_trigger_method, "PAUSE"))
    *trigger = WFD_TRIGGER_PAUSE;
  else if(!g_strcmp0(msg->trigger_method->wfd_trigger_method, "TEARDOWN"))
    *trigger = WFD_TRIGGER_TEARDOWN;
  else if(!g_strcmp0(msg->trigger_method->wfd_trigger_method, "PLAY"))
    *trigger = WFD_TRIGGER_PLAY;
  else {
    *trigger = WFD_TRIGGER_UNKNOWN;
    return WFD_EINVAL;
  }
  return WFD_OK;
}

WFDResult wfdconfig_set_presentation_url(WFDMessage *msg, gchar *wfd_url0, gchar *wfd_url1)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(!msg->presentation_url) msg->presentation_url = g_new0 (WFDPresentationUrl, 1);
  if(wfd_url0) msg->presentation_url->wfd_url0 = g_strdup(wfd_url0);
  if(wfd_url1) msg->presentation_url->wfd_url1 = g_strdup(wfd_url1);
  return WFD_OK;
}

WFDResult wfdconfig_get_presentation_url(WFDMessage *msg, gchar **wfd_url0, gchar **wfd_url1)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(msg->presentation_url) {
    *wfd_url0 = g_strdup(msg->presentation_url->wfd_url0);
    *wfd_url1 = g_strdup(msg->presentation_url->wfd_url1);
  }
  return WFD_OK;
}

WFDResult wfdconfig_set_prefered_RTP_ports(WFDMessage *msg, WFDRTSPTransMode trans, WFDRTSPProfile profile,
                 WFDRTSPLowerTrans lowertrans, guint32 rtp_port0, guint32 rtp_port1)
{
  GString *lines;
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);

  if(!msg->client_rtp_ports)
    msg->client_rtp_ports = g_new0 (WFDClientRtpPorts, 1);

  if(trans != WFD_RTSP_TRANS_UNKNOWN) {
    lines = g_string_new ("");
    if(trans == WFD_RTSP_TRANS_RTP)	g_string_append_printf (lines,"RTP");
    else if(trans == WFD_RTSP_TRANS_RDT) g_string_append_printf (lines,"RDT");

    if(profile == WFD_RTSP_PROFILE_AVP) g_string_append_printf (lines,"/AVP");
    else if(profile == WFD_RTSP_PROFILE_SAVP) g_string_append_printf (lines,"/SAVP");

    if(lowertrans == WFD_RTSP_LOWER_TRANS_UDP) g_string_append_printf (lines,"/UDP;unicast");
    else if(lowertrans == WFD_RTSP_LOWER_TRANS_UDP_MCAST) g_string_append_printf (lines,"/UDP;multicast");
    else if(lowertrans == WFD_RTSP_LOWER_TRANS_TCP) g_string_append_printf (lines,"/TCP;unicast");
    else if(lowertrans == WFD_RTSP_LOWER_TRANS_HTTP) g_string_append_printf (lines,"/HTTP");

    msg->client_rtp_ports->profile = g_string_free (lines, FALSE);
    msg->client_rtp_ports->rtp_port0 = rtp_port0;
    msg->client_rtp_ports->rtp_port1 = rtp_port1;
    msg->client_rtp_ports->mode = g_strdup("mode=play");
  }
  return WFD_OK;
}

WFDResult wfdconfig_get_prefered_RTP_ports(WFDMessage *msg, WFDRTSPTransMode *trans, WFDRTSPProfile *profile,
                WFDRTSPLowerTrans *lowertrans, guint32 *rtp_port0, guint32 *rtp_port1)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  g_return_val_if_fail (msg->client_rtp_ports != NULL, WFD_EINVAL);

  if(g_strrstr(msg->client_rtp_ports->profile, "RTP")) *trans = WFD_RTSP_TRANS_RTP;
  if(g_strrstr(msg->client_rtp_ports->profile, "RDT")) *trans = WFD_RTSP_TRANS_RDT;
  if(g_strrstr(msg->client_rtp_ports->profile, "AVP")) *profile = WFD_RTSP_PROFILE_AVP;
  if(g_strrstr(msg->client_rtp_ports->profile, "SAVP")) *profile = WFD_RTSP_PROFILE_SAVP;
  if(g_strrstr(msg->client_rtp_ports->profile, "UDP;unicast")) *lowertrans = WFD_RTSP_LOWER_TRANS_UDP;
  if(g_strrstr(msg->client_rtp_ports->profile, "UDP;multicast")) *lowertrans = WFD_RTSP_LOWER_TRANS_UDP_MCAST;
  if(g_strrstr(msg->client_rtp_ports->profile, "TCP;unicast")) *lowertrans = WFD_RTSP_LOWER_TRANS_TCP;
  if(g_strrstr(msg->client_rtp_ports->profile, "HTTP")) *lowertrans = WFD_RTSP_LOWER_TRANS_HTTP;

  *rtp_port0 = msg->client_rtp_ports->rtp_port0;
  *rtp_port1 = msg->client_rtp_ports->rtp_port1;

  return WFD_OK;
}

WFDResult wfdconfig_set_audio_sink_type(WFDMessage *msg, WFDSinkType sinktype)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(!msg->route) msg->route = g_new0 (WFDRoute, 1);
  if(sinktype == WFD_PRIMARY_SINK) msg->route->destination = g_strdup("primary");
  else if(sinktype == WFD_SECONDARY_SINK) msg->route->destination = g_strdup("secondary");
  return WFD_OK;
}

WFDResult wfdconfig_get_audio_sink_type(WFDMessage *msg, WFDSinkType *sinktype)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(msg->route) {
    if(!g_strcmp0(msg->route->destination,"primary")) *sinktype = WFD_PRIMARY_SINK;
    else if(!g_strcmp0(msg->route->destination,"secondary")) *sinktype = WFD_SECONDARY_SINK;
  }
  return WFD_OK;
}

WFDResult wfdconfig_set_I2C_port(WFDMessage *msg, gboolean i2csupport, guint32 i2cport)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(!msg->I2C) msg->I2C = g_new0 (WFDI2C, 1);
  msg->I2C->I2CPresent = i2csupport;
  msg->I2C->I2C_port = i2cport;
  return WFD_OK;
}

WFDResult wfdconfig_get_I2C_port(WFDMessage *msg, gboolean *i2csupport, guint32 *i2cport)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(msg->I2C && msg->I2C->I2CPresent) {
    *i2csupport = msg->I2C->I2CPresent;
    *i2cport = msg->I2C->I2C_port;
  } else *i2csupport = FALSE;
  return WFD_OK;
}

WFDResult wfdconfig_set_av_format_change_timing(WFDMessage *msg, guint64 PTS, guint64 DTS)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(!msg->av_format_change_timing) msg->av_format_change_timing = g_new0 (WFDAVFormatChangeTiming, 1);
  msg->av_format_change_timing->PTS = PTS;
  msg->av_format_change_timing->DTS = DTS;
  return WFD_OK;
}

WFDResult wfdconfig_get_av_format_change_timing(WFDMessage *msg, guint64 *PTS, guint64 *DTS)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(msg->av_format_change_timing) {
    *PTS = msg->av_format_change_timing->PTS;
    *DTS = msg->av_format_change_timing->DTS;
  }
  return WFD_OK;
}

WFDResult wfdconfig_set_uibc_capability(WFDMessage *msg, guint32 input_category, guint32 inp_type, WFDHIDCTypePathPair *inp_pair,
												guint32 inp_type_path_count, guint32 tcp_port)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(!msg->uibc_capability) msg->uibc_capability = g_new0 (WFDUibcCapability, 1);
  msg->uibc_capability->uibcsupported = TRUE;
  msg->uibc_capability->input_category_list.input_cat = input_category;
  msg->uibc_capability->generic_cap_list.inp_type = inp_type;
  msg->uibc_capability->hidc_cap_list.cap_count = inp_type_path_count;
  if(msg->uibc_capability->hidc_cap_list.cap_count) {
    detailed_cap *temp_cap;
    guint i=0;
    msg->uibc_capability->hidc_cap_list.next = g_new0 (detailed_cap, 1);
    temp_cap = msg->uibc_capability->hidc_cap_list.next;
    for(;i<inp_type_path_count;) {
      temp_cap->p.inp_type = inp_pair[i].inp_type;
      temp_cap->p.inp_path = inp_pair[i].inp_path;
      i++;
      if(i<inp_type_path_count) {
        temp_cap->next = g_new0 (detailed_cap, 1);
        temp_cap = temp_cap->next;
      }
    }
  }
  msg->uibc_capability->tcp_port = tcp_port;
  return WFD_OK;
}

WFDResult wfdconfig_get_uibc_capability(WFDMessage *msg, guint32 *input_category, guint32 *inp_type, WFDHIDCTypePathPair *inp_pair,
												guint32 *inp_type_path_count, guint32 *tcp_port)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(msg->uibc_capability && msg->uibc_capability->uibcsupported) {
    *input_category = msg->uibc_capability->input_category_list.input_cat;
    *inp_type = msg->uibc_capability->generic_cap_list.inp_type;
    if(msg->uibc_capability->hidc_cap_list.cap_count) {
      detailed_cap *temp_cap;
      guint i = 0;
      inp_pair = g_new0 (WFDHIDCTypePathPair, msg->uibc_capability->hidc_cap_list.cap_count);
      temp_cap = msg->uibc_capability->hidc_cap_list.next;
      while(temp_cap) {
        inp_pair[i].inp_type = temp_cap->p.inp_type;
        inp_pair[i].inp_path = temp_cap->p.inp_path;
        temp_cap = temp_cap->next;
        i++;
      }
    }
    *tcp_port = msg->uibc_capability->tcp_port;
  }
  return WFD_OK;
}

WFDResult wfdconfig_set_uibc_status(WFDMessage *msg, gboolean uibc_enable)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(!msg->uibc_setting) msg->uibc_setting = g_new0 (WFDUibcSetting, 1);
  msg->uibc_setting->uibc_setting = uibc_enable;
  return WFD_OK;
}

WFDResult wfdconfig_get_uibc_status(WFDMessage *msg, gboolean *uibc_enable)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(msg->uibc_setting) *uibc_enable = msg->uibc_setting->uibc_setting;
  return WFD_OK;
}
#ifdef STANDBY_RESUME_CAPABILITY
WFDResult wfdconfig_set_standby_resume_capability(WFDMessage *msg, gboolean supported)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(!msg->standby_resume_capability) msg->standby_resume_capability = g_new0 (WFDStandbyResumeCapability, 1);
  msg->standby_resume_capability->standby_resume_cap = supported;
  return WFD_OK;
}

WFDResult wfdconfig_get_standby_resume_capability(WFDMessage *msg, gboolean *supported)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(msg->standby_resume_capability) *supported = msg->standby_resume_capability->standby_resume_cap;
  return WFD_OK;
}
#endif
WFDResult wfdconfig_set_standby(WFDMessage *msg, gboolean standby_enable)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(!msg->standby) msg->standby = g_new0 (WFDStandby, 1);
  msg->standby->wfd_standby = standby_enable;
  return WFD_OK;
}

WFDResult wfdconfig_get_standby(WFDMessage *msg, gboolean *standby_enable)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(msg->standby) *standby_enable = msg->standby->wfd_standby;
  return WFD_OK;
}

WFDResult wfdconfig_set_connector_type(WFDMessage *msg, WFDConnector connector)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(!msg->connector_type) msg->connector_type = g_new0 (WFDConnectorType, 1);
  msg->connector_type->connector_type = connector;
  return WFD_OK;
}

WFDResult wfdconfig_get_connector_type(WFDMessage *msg, WFDConnector *connector)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(msg->connector_type) *connector = msg->connector_type->connector_type;
  return WFD_OK;
}

WFDResult wfdconfig_set_idr_request(WFDMessage *msg)
{
  g_return_val_if_fail (msg != NULL, WFD_EINVAL);
  if(!msg->idr_request) msg->idr_request = g_new0 (WFDIdrRequest, 1);
  msg->idr_request->idr_request = TRUE;
  return WFD_OK;
}
