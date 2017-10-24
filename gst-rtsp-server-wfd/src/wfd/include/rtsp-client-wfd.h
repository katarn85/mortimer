/* GStreamer
 * Copyright (C) 2008 Wim Taymans <wim.taymans at gmail.com>

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
 *

* * Modifications by Samsung Electronics Co., Ltd.

* 1. Applied Miracast WFD Server function
*/

#include <gst/gst.h>
#include <gst/rtsp/gstrtspconnection.h>

#include "rtsp-client.h"

#ifndef __GST_RTSP_CLIENT_WFD_H__
#define __GST_RTSP_CLIENT_WFD_H__

G_BEGIN_DECLS

#ifdef ENABLE_WFD_EXTENDED_FEATURES
#define GST_EXTENDED_FEATURE_PATH "/usr/lib/libmmfwfd_rtsp_server_ext.so"
#define EXT_INIT "gst_rtsp_client_extended_init"
#define EXT_FINALIZE "gst_rtsp_client_extended_finalize"
#define EXT_SEND_T2_E_MSG "gst_rtsp_client_extended_send_T2_E_msg"
#define EXT_MSG_RESP "gst_rtsp_client_extended_message_response"
#define EXT_SRC_CONFIG_MSG "gst_rtsp_client_extended_src_config_message_type"
#define EXT_HANDLE_SET_PARAM_REQ "gst_rtsp_client_extended_handle_set_param_request"
#define EXT_SET_B_MODE_PARAMS "gst_rtsp_client_extended_set_B_mode_params"
#define EXT_PREPARE_SET_PARAM_REQ "gst_rtsp_client_extended_prepare_set_param_request"
#define EXT_SET_VIDEOSRC_PROP "gst_rtsp_client_extended_set_videosrc_prop"
#define EXT_HANDLE_RESP "gst_rtsp_client_extended_handle_response"
#define EXT_PAD_ADD_EVENT_PROBE "gst_rtsp_client_extended_pad_add_event_probe"
#define EXT_ADD_UDPSRC_PAD_PROBE "gst_rtsp_client_extended_add_udpsrc_pad_probe"
#define EXT_PREPARE_PROTECTION "gst_rtsp_client_extended_prepare_protection"
#define EXT_REG_RESEND_ELEMENT "gst_rtsp_client_extended_register_resend_element"
#define EXT_SWITCH_TO_B_MODE "gst_rtsp_client_extended_switch_to_B_mode"

void __gst_extended_feature_open(GstRTSPClient * client);
void *__gst_extended_func(GstRTSPClient * client, const char *func);
void __gst_extended_feature_close(GstRTSPClient * client);
#endif

void gst_rtsp_client_wfd_init(GstRTSPClient * client);
void gst_rtsp_client_wfd_finalize(GstRTSPClient * client);

#ifdef ENABLE_WFD_EXTENDED_FEATURES
GstRTSPResult gst_rtsp_client_negotiate (GstRTSPClient * client, gchar **current_version);
#else
GstRTSPResult gst_rtsp_client_negotiate (GstRTSPClient * client);
#endif
gboolean gst_rtsp_client_start (GstRTSPClient * client);
gboolean gst_rtsp_client_pause (GstRTSPClient * client);
gboolean gst_rtsp_client_resume (GstRTSPClient * client);
gboolean gst_rtsp_client_standby (GstRTSPClient * client);
gboolean gst_rtsp_client_stop (GstRTSPClient * client);
void gst_rtsp_client_set_uibc_callback (GstRTSPClient *client, wfd_uibc_send_event_cb uibc_event_cb, wfd_uibc_control_cb uibc_control_cb, gpointer user_param);
void gst_rtsp_client_set_params (GstRTSPClient *client, int videosrc_type, gint session_mode,
                                  gint mtu_size, gchar *infile,
                                  gchar *audio_device, gint audio_latency_time, gint audio_buffer_time, gint audio_do_timestamp,
                                  guint64 video_reso_supported, gint video_native_resolution, gint hdcp_enabled, guint8 uibc_gen_capability,
#ifdef ENABLE_WFD_EXTENDED_FEATURES
                                  gint display_rotate, guint *decide_udp_bitrate, guint *decide_tcp_bitrate);
#else
                                  gint display_rotate, guint *decide_udp_bitrate);
#endif

GstRTSPResult uibc_enable_request (GstRTSPClient * client);
void gst_rtsp_client_create_srcbin (GstRTSPClient * client);

gboolean handle_wfd_set_param_request (GstRTSPClient * client, GstRTSPClientState * state);
gboolean handle_wfd_get_param_request (GstRTSPClient * client, GstRTSPClientState * state);

void set_keep_alive_condition(GstRTSPClient * client);
void gst_wfd_print_rtsp_msg(GstRTSPMethod method);
GstRTSPResult gst_wfd_handle_response(GstRTSPClient * client, GstRTSPMessage * message);

G_END_DECLS

#endif /* __GST_RTSP_CLIENT_WFD_H__ */
