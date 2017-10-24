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


#ifndef __GST_RTSP_SERVER_EXTENDED_H__
#define __GST_RTSP_SERVER_EXTENDED_H__

#include <gst/gst.h>
#include <gst/rtsp/gstrtspconnection.h>
#include "rtsp-server.h"

GstRTSPResult gst_rtsp_server_set_tcp_switching(GstRTSPServer * server, void *_client, gboolean enable_tcp_switching);
gboolean gst_rtsp_server_switch_to_TCP (GstRTSPServer * server, void *_client);
gboolean gst_rtsp_server_switch_to_UDP(GstRTSPServer * server, void *_client);
gboolean gst_rtsp_server_playback_play (GstRTSPServer * server, void * _client);
gboolean gst_rtsp_server_playback_pause (GstRTSPServer * server, void * _client);
gboolean gst_rtsp_server_set_volume (GstRTSPServer * server, void *_client, gchar *volume);
gboolean gst_rtsp_server_upgrade_dongle (GstRTSPServer * server, void *_client, gchar* upgrage_version, gchar **upgrade_url);
gboolean gst_rtsp_server_rename_dongle (GstRTSPServer * server, void * _client, gchar *rename);

G_END_DECLS

#endif /* __GST_RTSP_SERVER_EXTENDED_H__ */

