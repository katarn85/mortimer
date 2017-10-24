/*
 * Copyright (c) 2000-2014 Samsung Electronics Co., Ltd All Rights Reserved 
 *
 * Contact: Hyunjun Ko < zzoon.ko@samsung.com >
 *
 * PROPRIETARY/CONFIDENTIAL
 *
 * This software is the confidential and proprietary information of
 * SAMSUNG ELECTRONICS ("Confidential Information").
 * You shall not disclose such Confidential Information and shall
 * use it only in accordance with the terms of the license agreement
 * you entered into with SAMSUNG ELECTRONICS.
 * SAMSUNG make no representations or warranties about the suitability
 * of the software, either express or implied, including but not
 * limited to the implied warranties of merchantability, fitness for
 * a particular purpose, or non-infringement.
 * SAMSUNG shall not be liable for any damages suffered by licensee as
 * a result of using, modifying or distributing this software or its derivatives.
 */



#include "rtsp-server-extended.h"
#include "rtsp-client-extended.h"

GST_DEBUG_CATEGORY_STATIC (rtsp_extended_server_debug);
#define GST_CAT_DEFAULT rtsp_extended_server_debug

gboolean gst_rtsp_server_switch_to_TCP (GstRTSPServer * server, void *_client)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);

  if (!gst_rtsp_client_extended_switch_to_TCP (client)) {
    GST_ERROR ("Failed to start client...");
    return FALSE;
  }
  return TRUE;
}

gboolean gst_rtsp_server_switch_to_UDP(GstRTSPServer * server, void *_client)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);

  if (!gst_rtsp_client_extended_switch_to_UDP (client)) {
    GST_ERROR ("Failed to start client...");
    return FALSE;
  }
  return TRUE;
}

gboolean gst_rtsp_server_playback_play (GstRTSPServer * server, void *_client)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);

  if (!gst_rtsp_client_extended_playback_play (client)) {
    GST_ERROR ("Failed to rename client...");
    return FALSE;
  }
  return TRUE;
}

gboolean gst_rtsp_server_playback_pause (GstRTSPServer * server, void *_client)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);

  if (!gst_rtsp_client_extended_playback_pause (client)) {
    GST_ERROR ("Failed to rename client...");
    return FALSE;
  }
  return TRUE;
}

gboolean gst_rtsp_server_set_volume (GstRTSPServer * server, void *_client, gchar *volume)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);

  if (!gst_rtsp_client_extended_playback_set_volume (client, volume)) {
    GST_ERROR ("Failed to set client volume...");
    return FALSE;
  }
  return TRUE;
}

gboolean gst_rtsp_server_upgrade_dongle (GstRTSPServer * server, void *_client, gchar* upgrage_version, gchar **upgrade_url)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);

  if (!gst_rtsp_client_extended_upgrade_dongle (client, upgrage_version, upgrade_url)) {
    GST_ERROR ("Failed to upgrade dongle...");
    return FALSE;
  }

  return TRUE;
}

gboolean gst_rtsp_server_rename_dongle (GstRTSPServer * server, void *_client, gchar *rename)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);

  if (!gst_rtsp_client_extended_rename_dongle (client, rename)) {
    GST_ERROR ("Failed to rename client...");
    return FALSE;
  }
  return TRUE;
}


GstRTSPResult
gst_rtsp_server_set_tcp_switching (GstRTSPServer * server, void *_client, gboolean enable_t1_switching)
{
  GstRTSPClient *client = (GstRTSPClient *)_client;
  GST_DEBUG_OBJECT (server, "Client = %p", client);

  if(client == NULL) {
    GST_ERROR ("Invalid Parameter");
    return GST_RTSP_ERROR;
  }

  gst_rtsp_client_extended_enable_T1_switching (client, enable_t1_switching);

  return GST_RTSP_OK;
}
