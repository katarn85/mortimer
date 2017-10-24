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

#ifndef __GST_RTSP_WFD_H__
#define __GST_RTSP_WFD_H__

#include <gst/gst.h>
#include "rtsp-server.h"

#ifdef ENABLE_WFD_EXTENDED_FEATURES
gint gst_rtsp_server_negotiate_client (GstRTSPServer * server, void *_client, gchar **current_version);
#else
gint gst_rtsp_server_negotiate_client (GstRTSPServer * server, void *_client);
#endif
gboolean gst_rtsp_server_start_client (GstRTSPServer * server, void *_client);
gboolean gst_rtsp_server_pause_client (GstRTSPServer * server, void *_client);
gboolean gst_rtsp_server_resume_client (GstRTSPServer * server, void *_client);
gboolean gst_rtsp_server_standby_client (GstRTSPServer * server, void *_client);
gboolean gst_rtsp_server_stop_client (GstRTSPServer * server, void *_client);
void gst_rtsp_server_set_uibc_callback (GstRTSPServer * server, void *_client, void *uibc_event_cb, void *uibc_control_cb, gpointer user_param);
void gst_rtsp_server_set_client_params (GstRTSPServer * server, void *_client,
                                          int videosrc_type, gint session_mode, gint mtu_size,
                                          gchar *infile, gchar *audio_device, gint audio_latency_time, gint audio_buffer_time, gint audio_do_timestamp,
                                          guint64 video_reso_supported, gint video_native_resolution, gint hdcp_enabled, guint8 uibc_gen_capability,
#ifdef ENABLE_WFD_EXTENDED_FEATURES
                                          gint display_rotate, guint *decide_udp_bitrate, guint *decide_tcp_bitrate);
#else
                                          gint display_rotate, guint *decide_udp_bitrate);
#endif

G_END_DECLS

#endif /* __GST_RTSP_WFD_H__ */
