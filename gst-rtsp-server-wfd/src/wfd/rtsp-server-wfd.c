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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include "rtsp-server-wfd.h"
#include "rtsp-client-wfd.h"
#include "rtsp-session-pool.h"
#include "rtsp-media-mapping.h"
#include "rtsp-media-factory-uri.h"
#include "rtsp-auth.h"

gint
#ifdef ENABLE_WFD_EXTENDED_FEATURES
gst_rtsp_server_negotiate_client (GstRTSPServer * server, void *_client, gchar **current_version)
#else
gst_rtsp_server_negotiate_client (GstRTSPServer * server, void *_client)
#endif
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);
#ifdef ENABLE_WFD_EXTENDED_FEATURES
  res = gst_rtsp_client_negotiate (client, current_version);
#else
  res = gst_rtsp_client_negotiate (client);
#endif
  if(res != GST_RTSP_OK)
  {
    GST_ERROR ("Failed to negotiate with client...");
  }

  return res;
}

gboolean
gst_rtsp_server_start_client (GstRTSPServer * server, void *_client)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);

  if (!gst_rtsp_client_start (client)) {
    GST_ERROR ("Failed to start client...");
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_rtsp_server_pause_client (GstRTSPServer * server, void *_client)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);

  if (!gst_rtsp_client_pause (client)) {
    GST_ERROR ("Failed to start client...");
    return FALSE;
  }
  return TRUE;
}

gboolean
gst_rtsp_server_resume_client (GstRTSPServer * server, void *_client)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);

  if (!gst_rtsp_client_resume (client)) {
    GST_ERROR ("Failed to start client...");
    return FALSE;
  }
  return TRUE;
}

gboolean
gst_rtsp_server_standby_client (GstRTSPServer * server, void *_client)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);
  if (!gst_rtsp_client_standby(client)) {
    GST_ERROR ("Failed to standby client...");
    return FALSE;
  }
  return TRUE;
}


gboolean
gst_rtsp_server_stop_client (GstRTSPServer * server, void *_client)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);

  if (!gst_rtsp_client_stop (client)) {
    GST_ERROR ("Failed to start client...");
    return FALSE;
  }
  return TRUE;
}

void
gst_rtsp_server_set_uibc_callback (GstRTSPServer * server, void *_client, void *uibc_event_cb, void *uibc_control_cb, gpointer user_param)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  gst_rtsp_client_set_uibc_callback (client, uibc_event_cb, uibc_control_cb, user_param);
}

void
gst_rtsp_server_set_client_params (GstRTSPServer * server, void *_client,
                                    int videosrc_type, gint session_mode, gint mtu_size,
                                    gchar *infile, gchar *audio_device, gint audio_latency_time, gint audio_buffer_time, gint audio_do_timestamp,
                                    guint64 video_reso_supported, gint video_native_resolution, gint hdcp_enabled, guint8 uibc_gen_capability,
#ifdef ENABLE_WFD_EXTENDED_FEATURES
                                    gint display_rotate, guint *decide_udp_bitrate, guint *decide_tcp_bitrate)
#else
                                    gint display_rotate, guint *decide_udp_bitrate)
#endif
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);

  gst_rtsp_client_set_params(client, videosrc_type, session_mode, mtu_size, infile,
                              audio_device, audio_latency_time, audio_buffer_time, audio_do_timestamp,
                              video_reso_supported, video_native_resolution, hdcp_enabled, uibc_gen_capability,
#ifdef ENABLE_WFD_EXTENDED_FEATURES
                              display_rotate, decide_udp_bitrate, decide_tcp_bitrate);
#else
                              display_rotate, decide_udp_bitrate);
#endif
}

/* Apis for rtsp-client */
