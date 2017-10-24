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


#include "rtsp-client.h"

#ifndef __GST_RTSP_CLIENT_EXTENDED_H__
#define __GST_RTSP_CLIENT_EXTENDED_H__

void gst_rtsp_client_extended_init (GstRTSPClient * client);
void gst_rtsp_client_extended_finalize (GstRTSPClient * client);

gboolean gst_rtsp_client_extended_switch_to_TCP (GstRTSPClient * client);
gboolean gst_rtsp_client_extended_switch_to_UDP (GstRTSPClient * client);
void gst_rtsp_client_extended_enable_T1_switching (GstRTSPClient *client, gboolean enable_T1_switching);
gboolean gst_rtsp_client_extended_playback_play (GstRTSPClient * client);
gboolean gst_rtsp_client_extended_playback_pause (GstRTSPClient * client);
gboolean gst_rtsp_client_extended_playback_set_volume (GstRTSPClient *client, gchar *volume);
gboolean gst_rtsp_client_extended_upgrade_dongle (GstRTSPClient * client, gchar *upgrade_version, gchar **upgrade_url);
gboolean gst_rtsp_client_extended_rename_dongle (GstRTSPClient * client, gchar *rename);

GstRTSPResult gst_rtsp_client_extended_message_response (GstRTSPClient *client, GstRTSPMessage *response, GstRTSPMessage *request, WFDMessageType message_type);
WFDMessageType gst_rtsp_client_extended_src_config_message_type(WFDMessage *msg, GstRTSPMethod message_type);
gboolean gst_rtsp_client_extended_handle_set_param_request (GstRTSPClient * client, WFDMessage *msg, WFDMessageType message_type);
void gst_rtsp_client_extended_set_B_mode_params (GstRTSPClient *client);
GstRTSPResult gst_rtsp_client_extended_prepare_set_param_request (GstRTSPClient *client, GstRTSPMessage *request, WFDMessageType message_type, WFDTrigger trigger_type);
void gst_rtsp_client_extended_set_videosrc_prop (GstRTSPClient * client, GstElement *videosrc);
void gst_rtsp_client_extended_pad_add_event_probe (GstRTSPClient * client, GstPad *pad);

void gst_rtsp_client_extended_add_udpsrc_pad_probe (GstRTSPClient *client, GstRTSPMediaStream *stream);
GstRTSPResult gst_rtsp_client_extended_handle_response (GstRTSPClient * client, GstRTSPMessage * message);
void gst_rtsp_client_extended_send_T2_E_msg (GstRTSPClient *client);

#ifdef USE_HDCP
GstRTSPResult gst_rtsp_client_extended_prepare_protection (GstRTSPClient *client);
#endif

#endif /* __GST_RTSP_CLIENT_EXTENDED_H__ */

