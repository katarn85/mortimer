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


#include "rtsp-client-extended.h"
#include "mmf/wfdconfigmessage-ext.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <gst/base/gstbitreader.h>
#include <app.h>
#include <vconf.h>
#ifdef USE_HDCP
#include <hdcp2.h>
#endif
#include "rtsp-resender.h"

#define VENC_SKIP_INBUF_VALUE 5
#define UNSTABLE_NW_POPUP_INTERVAL  15

enum
{
  UNLOADED,
  LOADED1,
  LOADED2,
  CONGESTED
};

typedef enum{
  APP_NETWORK_WARN,
  APP_HDMI_WARN
}network_type_e;

enum
{
  NOT_EOS,
  EOS
};

#define BITRATE_INTERVAL 1024000
#define TH_STABLE_BUFFER_PERCENTAGE 80
#define TH_STABLE_BUFFERLEFT 300000
#define TH_UNLOADED_RTT (100*1000)
#define TH_CONGESTED_RTT (200*1000)
#define TH_ALLOWED_RTT_CHANGED (200*1000)
#define TH_ALLOWED_TCP_LAST_SENT 99
#define TH_VALID_CWND_INCREASING 2
#define TH_VALID_CWND_DECREASING 5
#define MAXIMUM_WINDOW_SIZE 30
#define NETWORK_UNLOADED 0
#define NETWORK_LOADED 1
#define NETWORK_CONGESTED 3
#define NETWORK_STATUS_BUFFER 10
#define ALLSHARE_CAST_PKGNAME "com.samsung.allshare-cast-popup"
#define DEFAULT_UIBC_PORT 19005

GST_DEBUG_CATEGORY_STATIC (rtsp_extended_client_debug);
#define GST_CAT_DEFAULT rtsp_extended_client_debug


void gst_rtsp_client_extended_switch_to_B_mode (void *data, GstRTSPClient * client);
static void gst_rtsp_client_extended_switch_to_A_mode (void *data, GstRTSPClient * client);
static void gst_rtsp_client_extended_handle_video_with_ui (void *data, GstRTSPClient * client);
static void gst_rtsp_client_extended_handle_selection_notify (void *data, GstRTSPClient * client);
#ifdef USE_VIDEO_PLAYER_STATE_CHANGE_CB
static void gst_rtsp_client_extended_video_player_state_change_cb (keynode_t *key, void *data);
static void gst_rtsp_client_extended_web_video_player_state_change_cb (keynode_t *key, void *data);
#endif

gboolean udp_src_cb_handler(GstPad * pad, GstMiniObject * obj, gpointer u_data);
#ifdef SUPPORT_LAUNCH_SCREEN_MIRROR_APP
static int launch_screen_mirror_app(char *app_id, network_type_e type);
#endif
static GstRTSPResult new_tcp (GstRTSPClient * client);

void gst_rtsp_client_extended_init (GstRTSPClient * client)
{
  GST_DEBUG_CATEGORY_INIT (rtsp_extended_client_debug, "rtspclient_ext", 0, "GstRTSPClient");

  client->eos = NOT_EOS;
  client->T1_mode = WFD_T1_A_MODE;
  client->B_mode_cond = g_cond_new ();
  client->B_mode_cond_lock = g_mutex_new ();
  client->B_mode_eos = FALSE;
  client->tcp_var = (TCP_Info *)g_malloc (sizeof(TCP_Info));
  client->B_mode_params = (WFDBModeParams *)g_malloc (sizeof(WFDBModeParams));
  client->sent_T2_E_msg = FALSE;
  client->T3_message_supported = FALSE;

  /* Register rtpresender element */
  gst_element_register (NULL, "rtpresender", GST_RANK_NONE, GST_TYPE_RTP_RESENDER);
}

void gst_rtsp_client_extended_finalize (GstRTSPClient * client)
{
  if(client->data_conn) {
    gst_rtsp_connection_close (client->data_conn);
    gst_rtsp_connection_free(client->data_conn);

    if (client->datawatch)
      g_source_destroy ((GSource *) client->datawatch);
    client->datawatch = NULL;
  }

  if(client->tcp_var)
    g_free(client->tcp_var);
  if(client->B_mode_params)
    g_free(client->B_mode_params);
}

GstRTSPResult
gst_rtsp_client_extended_message_response(GstRTSPClient *client, GstRTSPMessage *response, GstRTSPMessage *request, WFDMessageType message_type)
{
    switch(message_type) {
      case WFD_MESSAGE_T3:
        gst_rtsp_message_init_response (response, GST_RTSP_STS_OK, gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);
        return GST_RTSP_OK;
    default:
      return GST_RTSP_EINVAL;
    }
}

WFDMessageType
gst_rtsp_client_extended_src_config_message_type (WFDMessage *msg, GstRTSPMethod message_type)
{

  if (message_type == GST_RTSP_GET_PARAMETER) {
    return WFD_MESSAGE_UNKNOWN;
  } else if (message_type == GST_RTSP_SET_PARAMETER) {
    if (msg->audio_report) return WFD_MESSAGE_T3;
  } else {
    return WFD_MESSAGE_UNKNOWN;
  }

  return WFD_MESSAGE_UNKNOWN;
}

gboolean
gst_rtsp_client_extended_handle_set_param_request (GstRTSPClient * client, WFDMessage *msg, WFDMessageType message_type)
{
  guint64 pts;
  guint bufsize;

  switch (message_type) {
    case WFD_MESSAGE_T3:
    {
      GST_DEBUG("T3 server set param request for EOS handling");
      wfdconfig_get_audio_report(msg, &bufsize, &pts);
      GST_DEBUG("Aud bufsize is %d",bufsize);
      GST_DEBUG("Aud pts is %ld",pts);
      client->T3_message_supported = TRUE;
      if(pts == client->prev_aud_pts) {
        GST_LOG("setting the tcp eos");
        client->B_mode_eos = TRUE;
      }
      client->prev_aud_pts = pts;
    }
    break;
    default:
    {
      GST_ERROR_OBJECT (client, "Not handling message num - %d", message_type);
      return FALSE;
    }
  }

  return TRUE;
}

void gst_rtsp_client_extended_set_B_mode_params (GstRTSPClient *client) {

  client->B_mode_params->init_bitrate = client->decide_tcp_bitrate[0];
  client->B_mode_params->min_bitrate = client->decide_tcp_bitrate[1];
  client->B_mode_params->max_bitrate = client->decide_tcp_bitrate[2];

  if ((client->cMaxWidth * client->cMaxHeight) >= (1920 * 1080)) {
    client->B_mode_params->init_bitrate = client->decide_tcp_bitrate[3];
    client->B_mode_params->min_bitrate = client->decide_tcp_bitrate[4];
    client->B_mode_params->max_bitrate = client->decide_tcp_bitrate[5];
  } else if ((client->cMaxWidth * client->cMaxHeight) >= (1280 * 720)) {
    client->B_mode_params->init_bitrate = client->decide_tcp_bitrate[6];
    client->B_mode_params->min_bitrate = client->decide_tcp_bitrate[7];
    client->B_mode_params->max_bitrate = client->decide_tcp_bitrate[8];
  } else if ((client->cMaxWidth * client->cMaxHeight) >= (960 * 540)) {
    client->B_mode_params->init_bitrate = client->decide_tcp_bitrate[9];
    client->B_mode_params->min_bitrate = client->decide_tcp_bitrate[10];
    client->B_mode_params->max_bitrate = client->decide_tcp_bitrate[11];
  } else if ((client->cMaxWidth * client->cMaxHeight) >= (854 * 480)) {
    client->B_mode_params->init_bitrate = client->decide_tcp_bitrate[12];
    client->B_mode_params->min_bitrate = client->decide_tcp_bitrate[13];
    client->B_mode_params->max_bitrate = client->decide_tcp_bitrate[14];
  } else if((client->cMaxWidth * client->cMaxHeight) >= (640 * 360)) {
    client->B_mode_params->init_bitrate = client->decide_tcp_bitrate[15];
    client->B_mode_params->min_bitrate = client->decide_tcp_bitrate[16];
    client->B_mode_params->max_bitrate = client->decide_tcp_bitrate[17];
  }
  GST_DEBUG ("TCP Bitrate range is %u %u and init bitrate is %u", client->B_mode_params->min_bitrate,
      client->B_mode_params->max_bitrate, client->B_mode_params->init_bitrate);
}

GstRTSPResult gst_rtsp_client_extended_prepare_set_param_request (GstRTSPClient *client, GstRTSPMessage *request, WFDMessageType message_type, WFDTrigger trigger_type)
{
  GstRTSPResult res = GST_RTSP_OK;
  WFDResult wfd_res = WFD_OK;

  switch (message_type) {
    case WFD_MESSAGE_T1: {
      WFDMessage *msgb1;
      gchar *msg;
      guint msglen = 0;
      GString *msglength;

      /* create T1 message to be sent in the request */
      wfd_res = wfdconfig_message_new(&msgb1);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to create wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      wfd_res = wfdconfig_message_init(msgb1);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to init wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      if (client->T1_mode == WFD_T1_B_MODE) {
        wfd_res = wfdconfig_set_buffer_length(msgb1,512);
        wfd_res = wfdconfig_set_prefered_RTP_ports(msgb1, WFD_RTSP_TRANS_RTP, WFD_RTSP_PROFILE_AVP,
                  WFD_RTSP_LOWER_TRANS_TCP, client->crtp_port0, client->crtp_port1);
        wfd_res = wfdconfig_set_buffer_volume(msgb1, 15);
      }
      else if (client->T1_mode == WFD_T1_A_MODE) {
        wfd_res = wfdconfig_set_buffer_length(msgb1,0);
        wfd_res = wfdconfig_set_prefered_RTP_ports(msgb1, WFD_RTSP_TRANS_RTP, WFD_RTSP_PROFILE_AVP,
                  WFD_RTSP_LOWER_TRANS_UDP, client->crtp_port0, client->crtp_port1);
      }

      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set preffered RTP ports...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      wfd_res = wfdconfig_message_dump(msgb1);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to dump wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      msg = wfdconfig_message_as_text(msgb1);
      if (msg == NULL) {
        GST_ERROR_OBJECT (client, "Failed to get wfd message as text...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      msglen = strlen(msg);
      msglength = g_string_new ("");
      g_string_append_printf (msglength,"%d",msglen);

      GST_DEBUG ("T1 message body: %s", msg);

      /* add content-length type */
      res = gst_rtsp_message_add_header (request, GST_RTSP_HDR_CONTENT_LENGTH, g_string_free (msglength, FALSE));
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to add header to rtsp request...");
        goto error;
      }

      /* adding wfdconfig data to request */
      res = gst_rtsp_message_set_body (request, (guint8*)msg, msglen);
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to set body to rtsp request...");
        goto error;
      }

      wfdconfig_message_free(msgb1);
      break;
    }
    case WFD_MESSAGE_T2_A: {
      WFDMessage *msgb3;
      gchar *msg;
      guint msglen = 0;
      GString *msglength;
      /* create T2 A message to be sent in the request */
      wfd_res = wfdconfig_message_new(&msgb3);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to create wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      wfd_res = wfdconfig_message_init(msgb3);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to init wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      wfd_res = wfdconfig_set_playback_play(msgb3);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set playback play...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      wfd_res = wfdconfig_message_dump(msgb3);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to dump wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      msg = wfdconfig_message_as_text(msgb3);
      if (msg == NULL) {
        GST_ERROR_OBJECT (client, "Failed to get wfd message as text...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      msglen = strlen(msg);
      msglength = g_string_new ("");
      g_string_append_printf (msglength,"%d",msglen);

      GST_DEBUG ("T2 A message body: %s", msg);

      /* add content-length type */
      res = gst_rtsp_message_add_header (request, GST_RTSP_HDR_CONTENT_LENGTH, g_string_free (msglength, FALSE));
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to add header to rtsp request...");
        goto error;
      }
      /* adding wfdconfig data to request */
      res = gst_rtsp_message_set_body (request, (guint8*)msg, msglen);
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to set body to rtsp request...");
        goto error;
      }

      wfdconfig_message_free(msgb3);
      break;
    }
    case WFD_MESSAGE_T2_B: {
      WFDMessage *msgb3;
      gchar *msg;
      guint msglen = 0;
      GString *msglength;
      /* create T2 B message to be sent in the request */
      wfd_res = wfdconfig_message_new(&msgb3);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to create wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      wfd_res = wfdconfig_message_init(msgb3);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to init wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      wfd_res = wfdconfig_set_playback_pause(msgb3);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set playback pause...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      wfd_res = wfdconfig_message_dump(msgb3);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to dump wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      msg = wfdconfig_message_as_text(msgb3);
      if (msg == NULL) {
        GST_ERROR_OBJECT (client, "Failed to get wfd message as text...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      msglen = strlen(msg);
      msglength = g_string_new ("");
      g_string_append_printf (msglength,"%d",msglen);

      GST_DEBUG ("T2 B message body: %s", msg);

      /* add content-length type */
      res = gst_rtsp_message_add_header (request, GST_RTSP_HDR_CONTENT_LENGTH, g_string_free (msglength, FALSE));
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to add header to rtsp request...");
        goto error;
      }
      /* adding wfdconfig data to request */
      res = gst_rtsp_message_set_body (request, (guint8*)msg, msglen);
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to set body to rtsp request...");
        goto error;
      }

      wfdconfig_message_free(msgb3);
      break;
    }
    case WFD_MESSAGE_T2_E: {
      WFDMessage *msgt2;
      gchar *msg;
      guint msglen = 0;
      GString *msglength;
      /* create T2 message to be sent in the request */
      wfd_res = wfdconfig_message_new(&msgt2);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to create wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      wfd_res = wfdconfig_message_init(msgt2);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to init wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      wfd_res = wfdconfig_set_buffer_volume(msgt2, client->volume);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set buffer volume...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      wfd_res = wfdconfig_message_dump(msgt2);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to dump wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      msg = wfdconfig_message_as_text(msgt2);
      if (msg == NULL) {
        GST_ERROR_OBJECT (client, "Failed to get wfd message as text...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      msglen = strlen(msg);
      msglength = g_string_new ("");
      g_string_append_printf (msglength,"%d",msglen);

      GST_DEBUG ("T2 message body: %s", msg);

      /* add content-length type */
      res = gst_rtsp_message_add_header (request, GST_RTSP_HDR_CONTENT_LENGTH, g_string_free (msglength, FALSE));
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to add header to rtsp request...");
        goto error;
      }
      /* adding wfdconfig data to request */
      res = gst_rtsp_message_set_body (request, (guint8*)msg, msglen);
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to set body to rtsp request...");
        goto error;
      }

      wfdconfig_message_free(msgt2);
      break;
    }
    case WFD_MESSAGE_U1: {
      WFDMessage *msgu1;
      gchar *msg;
      guint msglen = 0;
      GString *msglength;
      /* create U1 message to be sent in the request */
      wfd_res = wfdconfig_message_new(&msgu1);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to create wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      wfd_res = wfdconfig_message_init(msgu1);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to init wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      wfd_res = wfdconfig_set_dongle_upgrade(msgu1, client->upgrade_version);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set dongle version ...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      wfd_res = wfdconfig_message_dump(msgu1);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to dump wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      msg = wfdconfig_message_as_text(msgu1);
      if (msg == NULL) {
        GST_ERROR_OBJECT (client, "Failed to get wfd message as text...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      msglen = strlen(msg);
      msglength = g_string_new ("");
      g_string_append_printf (msglength,"%d",msglen);

      GST_DEBUG ("U1 message body: %s", msg);

      /* add content-length type */
      res = gst_rtsp_message_add_header (request, GST_RTSP_HDR_CONTENT_LENGTH, g_string_free (msglength, FALSE));
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to add header to rtsp request...");
        goto error;
      }
      /* adding wfdconfig data to request */
      res = gst_rtsp_message_set_body (request, (guint8*)msg, msglen);
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to set body to rtsp request...");
        goto error;
      }

      wfdconfig_message_free(msgu1);
      break;
    }
    case WFD_MESSAGE_U2: {
      WFDMessage *msgt4;
      gchar *msg;
      guint msglen = 0;
      GString *msglength;
      /* create U2 message to be sent in the request */
      wfd_res = wfdconfig_message_new(&msgt4);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to create wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      wfd_res = wfdconfig_message_init(msgt4);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to init wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      wfd_res = wfdconfig_set_rename_dongle(msgt4, client->rename_dongle);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set dongle name...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      wfd_res = wfdconfig_message_dump(msgt4);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to dump wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      msg = wfdconfig_message_as_text(msgt4);
      if (msg == NULL) {
        GST_ERROR_OBJECT (client, "Failed to get wfd message as text...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      msglen = strlen(msg);
      msglength = g_string_new ("");
      g_string_append_printf (msglength,"%d",msglen);

      GST_DEBUG ("U2 message body: %s", msg);

      /* add content-length type */
      res = gst_rtsp_message_add_header (request, GST_RTSP_HDR_CONTENT_LENGTH, g_string_free (msglength, FALSE));
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to add header to rtsp request...");
        goto error;
      }
      /* adding wfdconfig data to request */
      res = gst_rtsp_message_set_body (request, (guint8*)msg, msglen);
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to set body to rtsp request...");
        goto error;
      }

      wfdconfig_message_free(msgt4);
      break;
    }

    default:
      GST_ERROR_OBJECT (client, "Unhandled WFD message type...");
      return GST_RTSP_EINVAL;
  }
  return res;

error:
  return res;
}

void gst_rtsp_client_extended_set_videosrc_prop (GstRTSPClient * client, GstElement *videosrc)
{
  g_signal_connect (videosrc, "ui-only", G_CALLBACK(gst_rtsp_client_extended_switch_to_A_mode), client);
  g_signal_connect (videosrc, "video-only", G_CALLBACK(gst_rtsp_client_extended_switch_to_B_mode), client);
  g_signal_connect (videosrc, "video-with-ui", G_CALLBACK(gst_rtsp_client_extended_handle_video_with_ui), client);
  g_signal_connect (videosrc, "selection-notify", G_CALLBACK(gst_rtsp_client_extended_handle_selection_notify), client);
  GST_ERROR_OBJECT (client,"set vconf_notify_key_changed video player STATE changed");

#ifdef USE_VIDEO_PLAYER_STATE_CHANGE_CB  
  if (0 != vconf_notify_key_changed(VCONFKEY_XV_STATE, gst_rtsp_client_extended_video_player_state_change_cb, client)) {
    GST_ERROR_OBJECT (client,"vconf_notify_key_changed() failed");
  }
  if (0 != vconf_notify_key_changed(VCONFKEY_WFD_WEB_VIDEO_PLAYER_STATE, gst_rtsp_client_extended_web_video_player_state_change_cb, client)) {
    GST_ERROR_OBJECT (client,"vconf_notify_key_changed() failed");
  }
#endif  
}

//static
void gst_rtsp_client_extended_switch_to_B_mode (void *data, GstRTSPClient * client)
{
  if (!client->enable_spec_features) {
    return;
  }

  if (!client->enable_T1_switching) {
    return;
  }

  if (client->T1_mode ==WFD_T1_B_MODE) {
    return;
  }

  if (client->client_state != CLIENT_STATE_CONNECTED) {
    GST_INFO("WFD doesn't still reach playing");
    client->B_mode_requested = TRUE;
    return;
  }

  GST_INFO("gst_rtsp_client_extended_switch_to_B_mode");
  client->first_rtcp = FALSE;
  if (!gst_rtsp_client_extended_switch_to_TCP (client)) {
    GST_ERROR ("Failed to start client...");
  } else {
    if(client->rtcp_udpsrc_pad && (client->rtcp_pad_handle_id > 0))
      gst_pad_remove_data_probe(client->rtcp_udpsrc_pad, client->rtcp_pad_handle_id);
  }
}

static void vconf_switch_to_udp(GstRTSPClient *client)
{
  GST_DEBUG("vconf called");
  if (!gst_rtsp_client_extended_switch_to_UDP (client)) {
    GST_ERROR ("Failed to start client...");
  } else {
    client->rtcp_pad_handle_id = gst_pad_add_data_probe(client->rtcp_udpsrc_pad,  G_CALLBACK(udp_src_cb_handler), client);
  }
}

gboolean timer_switch_to_udp(gpointer userdata)
{
  GstRTSPClient *client = (GstRTSPClient *)userdata;
  if (client == NULL) return FALSE;

  if ((client->B_mode_eos && client->T3_message_supported && client->timer_count < 10)
  	|| client->T3_message_supported == FALSE || client->timer_count > 10) {
    GST_LOG("calling timer true");

    if (!gst_rtsp_client_extended_switch_to_UDP (client)) {
      GST_ERROR ("Failed to switch to TCP...");
    } else {
      client->rtcp_pad_handle_id = gst_pad_add_data_probe (client->rtcp_udpsrc_pad,  G_CALLBACK(udp_src_cb_handler), client);
    }

    client->B_mode_eos = FALSE;
    return FALSE;
  } else {
    client->timer_count++;
    GST_LOG("calling timer false");
    return TRUE;
  }
}


void gst_switch_to_udp_thread_start(GstRTSPClient *client)
{
  GST_INFO ("waiting for complete playback in TCP mode");
  g_cond_wait (client->B_mode_cond, client->B_mode_cond_lock);
  g_timeout_add (500, timer_switch_to_udp, client);
}

static void
gst_rtsp_client_extended_switch_to_A_mode (void *data, GstRTSPClient * client)
{
  if (!client->enable_spec_features) {
    return;
  }

  if (client->T1_mode == WFD_T1_A_MODE) {
    return;
  }

  /* When video doesn't reach to EOS, switch to udp directly */
  if(client->eos != EOS) {
    vconf_switch_to_udp(client);
  }

  client->first_rtcp = FALSE;
}

static
void gst_rtsp_client_extended_handle_video_with_ui (void *data, GstRTSPClient * client)
{
  if(!client) goto FAILURE;

#ifdef USE_VIDEO_PLAYER_STATE_CHANGE_CB
  if (client->vconf_web_video_state == VCONFKEY_WFD_WEB_VIDEO_PLAYER_STOP) {
    gst_rtsp_client_extended_switch_to_A_mode (client, client);
  } else {
    gst_rtsp_client_extended_switch_to_B_mode (client, client);
  }
#endif

  return;

FAILURE:
  GST_DEBUG ("Video Player STATE change failed to handle");
  return;
}

static
void gst_rtsp_client_extended_handle_selection_notify (void *data, GstRTSPClient * client)
{
  GST_INFO("handle_selection_notify");

#ifdef SUPPORT_LAUNCH_SCREEN_MIRROR_APP  
  int ret = launch_screen_mirror_app (ALLSHARE_CAST_PKGNAME, APP_HDMI_WARN);
  if(ret < 0)
    GST_ERROR (" pop up is not working");
#else
  GST_ERROR ("not supported function !");
#endif
}

static void
map_transports (GstRTSPClient *client , GstRTSPTransport *ct)
{
  switch(client->ctrans) {
    case WFD_RTSP_TRANS_RTP:
      ct->trans = GST_RTSP_TRANS_RTP;
      break;
    case WFD_RTSP_TRANS_RDT:
      ct->trans = GST_RTSP_TRANS_RDT;
      break;
    default:
      ct->trans = GST_RTSP_TRANS_UNKNOWN;
      break;
  }
  switch(client->cprofile) {
    case WFD_RTSP_PROFILE_AVP:
      ct->profile = GST_RTSP_PROFILE_AVP;
      break;
    case WFD_RTSP_PROFILE_SAVP:
      ct->profile = GST_RTSP_PROFILE_SAVP;
      break;
    default:
      ct->profile = GST_RTSP_PROFILE_UNKNOWN;
      break;
  }
  switch(client->clowertrans) {
    case WFD_RTSP_LOWER_TRANS_UDP:
      ct->lower_transport = GST_RTSP_LOWER_TRANS_UDP;
      break;
    case WFD_RTSP_LOWER_TRANS_UDP_MCAST:
      ct->lower_transport = GST_RTSP_LOWER_TRANS_UDP_MCAST;
      break;
    case WFD_RTSP_LOWER_TRANS_TCP:
      ct->lower_transport = GST_RTSP_LOWER_TRANS_TCP;
      break;
    case WFD_RTSP_LOWER_TRANS_HTTP:
      ct->lower_transport = GST_RTSP_LOWER_TRANS_HTTP;
      break;
    default:
      ct->lower_transport = GST_RTSP_LOWER_TRANS_UNKNOWN;
      break;
  }
}

#define MM_BITRATE_UPDATE_PERIOD    4000 //ms
#define MM_LINKSPEED_CHECK_INTERVAL 500 //ms
#define MM_BITRATE_UPDATE_AVG_WINDOW  (MM_BITRATE_UPDATE_PERIOD / MM_LINKSPEED_CHECK_INTERVAL)

static WFDBModeInfoBuffer g_tcp_infos[MM_BITRATE_UPDATE_AVG_WINDOW];

#if MM_BITRATE_UPDATE_AVG_WINDOW == 1
static const guint32 tbl_weight[1] = {100};
#elif MM_BITRATE_UPDATE_AVG_WINDOW == 2
static const guint32 tbl_weight[2] = {30, 70};
#elif MM_BITRATE_UPDATE_AVG_WINDOW == 3
static const guint32 tbl_weight[3] = {16, 34, 50};
#elif MM_BITRATE_UPDATE_AVG_WINDOW == 4
static const guint32 tbl_weight[4] = {10, 20, 30, 40};
#elif MM_BITRATE_UPDATE_AVG_WINDOW == 5
static const guint32 tbl_weight[5] = {2, 3, 5, 8, 12};
#elif MM_BITRATE_UPDATE_AVG_WINDOW == 6
static const guint32 tbl_weight[6] = {2, 3, 5, 8, 12, 15};
#elif MM_BITRATE_UPDATE_AVG_WINDOW == 7
static const guint32 tbl_weight[7] = {2, 3, 5, 8, 12, 15, 20};
#elif MM_BITRATE_UPDATE_AVG_WINDOW == 8
static const guint32 tbl_weight[8] = {2, 3, 5, 8, 12, 15, 20, 35};
#elif MM_BITRATE_UPDATE_AVG_WINDOW == 9
static const guint32 tbl_weight[9] = {2, 3, 5, 8, 12, 15, 20, 35, 50};
#elif MM_BITRATE_UPDATE_AVG_WINDOW == 10
static const guint32 tbl_weight[10] = {2, 3, 5, 8, 12, 15, 20, 35, 50, 60};
#else
static const guint32 tbl_weight[0]; // make comfile error for unsupported cases
#endif

static void calculate_RTT_weight_avg(GstRTSPClient * client)
{
    int i = 0;
    TCP_Info *info = client->tcp_var;

    info->tcpi_rtt = 0;
    for (i = 0; i < MM_BITRATE_UPDATE_AVG_WINDOW; i++) {
        info->tcpi_rtt += (g_tcp_infos[i].tcp_info.tcpi_rtt * tbl_weight[i]) / 100;
    }

    info->tcpi_rttvar = 0;
    for (i = 0; i < MM_BITRATE_UPDATE_AVG_WINDOW; i++) {
        info->tcpi_rttvar += (g_tcp_infos[i].tcp_info.tcpi_rttvar * tbl_weight[i]) / 100;
    }

    info->tcpi_last_data_sent = 0;
    for (i = 0; i < MM_BITRATE_UPDATE_AVG_WINDOW; i++) {
        info->tcpi_last_data_sent += (g_tcp_infos[i].tcp_info.tcpi_last_data_sent * tbl_weight[i]) / 100;
    }

    info->tcpi_snd_cwnd = 0;
    for (i = 0; i < MM_BITRATE_UPDATE_AVG_WINDOW; i++) {
        info->tcpi_snd_cwnd += (g_tcp_infos[i].tcp_info.tcpi_snd_cwnd * tbl_weight[i]) / 100;
    }

    client->socket_buffer_size = 0;
    for (i = 0; i < MM_BITRATE_UPDATE_AVG_WINDOW; i++) {
        client->socket_buffer_size += (g_tcp_infos[i].socket_buffer_size * tbl_weight[i]) / 100;
    }

    client->socket_buffer_size_left = 0;
    for (i = 0; i < MM_BITRATE_UPDATE_AVG_WINDOW; i++) {
        client->socket_buffer_size_left += (g_tcp_infos[i].socket_buffer_size_left * tbl_weight[i]) / 100;
    }

    return;
}

static void configure_encoder_bitrate_over_tcp (GstRTSPClient * client)
{
  int fd = 0;
  guint32 socket_used_size = 0;
  guint32 socket_buffer_size = 0;
  guint i = 0;
  guint socket_buffer_size_len = sizeof(socket_buffer_size);
  int tcp_info_length = sizeof(TCP_Info);
  gint ret = 0;

  static int sample_cnt = 0;

  fd = gst_rtsp_connection_get_readfd(client->data_conn);
  if(fd < 0) {
    GST_ERROR_OBJECT (client, "failed in getting fd %s", strerror(errno));
    return;
  }
  ret = getsockopt( fd, 6, TCP_INFO, (void *)client->tcp_var, (socklen_t *)&tcp_info_length );
  if(ret != 0) {
    GST_ERROR_OBJECT (client, "failed in getting tcp info %s", strerror(errno));
    return;
  }
  ret = getsockopt( fd, SOL_SOCKET, SO_SNDBUF, &socket_buffer_size, &socket_buffer_size_len);
  if(ret != 0) {
    GST_ERROR_OBJECT (client, "failed in getting socket buffer size %s", strerror(errno));
    return;
  }
  ret = ioctl(fd, TIOCOUTQ, &socket_used_size);
  if(ret != 0) {
    GST_ERROR_OBJECT (client, "failed in ioctl %s", strerror(errno));
    return;
  }

  TCP_Info *tcp_ptr = NULL;
  guint32 socket_buffer_size_left;

  if (sample_cnt >= MM_BITRATE_UPDATE_AVG_WINDOW) {
    sample_cnt = 0;
    calculate_RTT_weight_avg(client);
  } else {
    tcp_ptr = &(g_tcp_infos[sample_cnt].tcp_info);
    memset(tcp_ptr, 0x00, sizeof(TCP_Info));
    memcpy(tcp_ptr, client->tcp_var, sizeof(TCP_Info));
    socket_buffer_size_left = (socket_buffer_size * 3)/4 - socket_used_size;
    g_tcp_infos[sample_cnt].socket_buffer_size = socket_buffer_size;
    g_tcp_infos[sample_cnt].socket_buffer_size_left = socket_buffer_size_left;

    sample_cnt++;
    return;
  }

  gboolean buffer_dec = FALSE;
  guint32 rtt = client->tcp_var->tcpi_rtt;
  guint32 rtt_var = client->tcp_var->tcpi_rttvar;
  guint32 snd_cwnd = client->tcp_var->tcpi_snd_cwnd;
  guint32 last_data_sent = client->tcp_var->tcpi_last_data_sent;
  guint32 step_size = 0;
  guint32 current_network_status = 0;
  guint32 venc_bitrate = 0;
  guint32 mPrevEncoderBitrate = 0;
  WFDBModeParams *B_mode_params = (client->B_mode_params);
  socket_buffer_size = client->socket_buffer_size;
  socket_buffer_size_left = client->socket_buffer_size_left;

  g_print("---------- TCP bitrate [%d] ------------\n", sample_cnt);
  GST_WARNING("Socket buffer size : %d\n", socket_buffer_size);
  GST_WARNING("Socket used size : %d\n", socket_used_size);
  GST_WARNING("rtt : %d\n", rtt);
  GST_WARNING("rtt_var : %d\n", rtt_var);
  GST_WARNING("snd_cwnd : %d\n", snd_cwnd);
  GST_WARNING("last_data_sent : %d\n", last_data_sent);
  GST_WARNING("socket_buffer_size_left : %d\n", socket_buffer_size_left);

  g_object_get (client->srcbin->venc, "bitrate", &venc_bitrate, NULL);
  if(B_mode_params->curr_rtt == 0) {
    B_mode_params->curr_rtt = rtt;
    B_mode_params->rtt_moving_avg = rtt;
    B_mode_params->prev_rtt_when_mode_changed = rtt;
    B_mode_params->prev_mode_changed_time =0;
    B_mode_params->qos_count = 0;
    B_mode_params->prev_cwnd = snd_cwnd;
    B_mode_params->prev_buffer_left = socket_buffer_size_left;
    B_mode_params->max_buffer_size = socket_buffer_size_left;
    B_mode_params->prev_setting_bps = client->B_mode_params->init_bitrate;

    for(i=0;i<10;++i)
      B_mode_params->prev_network_status[i] = 0;
  }
  B_mode_params->rtt_moving_avg = rtt;
  mPrevEncoderBitrate = B_mode_params->prev_setting_bps;
  if(B_mode_params->max_buffer_size < socket_buffer_size_left)
    B_mode_params->max_buffer_size = socket_buffer_size_left;

/* Check whether the socket buffer size decreased drastically.
* 1. Less than 80% of max buffer size and
* 2. Less than the threshold from max buffer size.
*/

  if (socket_buffer_size_left*100 < B_mode_params->max_buffer_size * TH_STABLE_BUFFER_PERCENTAGE &&
           socket_buffer_size_left + TH_STABLE_BUFFERLEFT < B_mode_params->max_buffer_size) {
    buffer_dec = TRUE;
    GST_WARNING("Step 1 check : Buffer decreased\n\n");
  }

//Stage 2 - Network status estimation
  GST_WARNING("rtt_moving_avg : %d\n", B_mode_params->rtt_moving_avg);
  GST_WARNING("max buffer size : %d\n", B_mode_params->max_buffer_size);

  if (B_mode_params->rtt_moving_avg < TH_UNLOADED_RTT && last_data_sent <= TH_ALLOWED_TCP_LAST_SENT &&
           snd_cwnd > B_mode_params->prev_cwnd - TH_VALID_CWND_INCREASING && buffer_dec == FALSE) {
    current_network_status = NETWORK_UNLOADED;
  } else if (last_data_sent > TH_ALLOWED_TCP_LAST_SENT) {
    current_network_status = NETWORK_CONGESTED;
    g_print("Step 2-1 check! : Network CONGESTED last data sent time is over\n\n");
    GST_WARNING("Step 2-1 check! : Network CONGESTED last data sent time is over\n\n");
  } else if (snd_cwnd < B_mode_params->prev_cwnd - TH_VALID_CWND_DECREASING) {
    current_network_status = NETWORK_CONGESTED;
    g_print("Step 2-2 check! : Network CONGESTED window size decrease\n\n");
    GST_WARNING("Step 2-2-2 check! : Network CONGESTED window size decrease");
  } else if (buffer_dec == TRUE) {
    current_network_status = NETWORK_CONGESTED;
    g_print("Step 2-3 check! : Network CONGESTED buffer decrease\n\n");
    GST_WARNING("Step 2-3 check! : Network CONGESTED buffer decrease");
  } else if ((B_mode_params->rtt_moving_avg > B_mode_params->prev_rtt_when_mode_changed
                       && B_mode_params->rtt_moving_avg - B_mode_params->prev_rtt_when_mode_changed > TH_ALLOWED_RTT_CHANGED)) {
    current_network_status = NETWORK_CONGESTED;
    g_print("Step 2-4 check! : Network CONGESTED rtt moving avr\n\n");
    GST_WARNING("Step 2-4 check! : Network CONGESTED rtt moving avr");
  } else {
    current_network_status = NETWORK_LOADED;
  }
  B_mode_params->prev_network_status[NETWORK_STATUS_BUFFER - 1] = current_network_status;

//Stage 3 - Decide bitrate of the encoder
  guint32 consecutive_loaded_or_congested = 0;
  guint32 consecutive_unloaded = 0;
  guint32 recent_consecutive_unloaded = 0;
  guint32 bps_estimated_from_TCPwindow;
  for(i = NETWORK_STATUS_BUFFER - 5 ; i < NETWORK_STATUS_BUFFER; i++)
    if (B_mode_params->prev_network_status[i] >= NETWORK_LOADED)
      consecutive_loaded_or_congested++;

  for(i = 0 ; i < NETWORK_STATUS_BUFFER; i++)
    if (B_mode_params->prev_network_status[i] == NETWORK_UNLOADED)
      consecutive_unloaded++;

  for(i = NETWORK_STATUS_BUFFER - 5 ; i < NETWORK_STATUS_BUFFER; i++)
    if (B_mode_params->prev_network_status[i] == NETWORK_UNLOADED)
      recent_consecutive_unloaded++;

  bps_estimated_from_TCPwindow = (client->B_mode_params->max_bitrate - client->B_mode_params->min_bitrate) * snd_cwnd /MAXIMUM_WINDOW_SIZE + client->B_mode_params->min_bitrate;

//Decreasing status
  if (current_network_status >= NETWORK_LOADED
          && ((current_network_status == NETWORK_CONGESTED
                && B_mode_params->qos_count - B_mode_params->prev_mode_changed_time > 7)         //400ms interval for congested network status
          || (consecutive_loaded_or_congested > 3
                && B_mode_params->qos_count - B_mode_params->prev_mode_changed_time > 19))) {    //1000ms interval for loaded network status
    if (current_network_status == NETWORK_LOADED) {
      step_size = venc_bitrate/10;         // 10 % step size
    } else if (current_network_status == NETWORK_CONGESTED) {
      if (last_data_sent >= TH_ALLOWED_TCP_LAST_SENT || buffer_dec == TRUE)
        step_size = venc_bitrate/3;        //33 % step size
      else
        step_size = venc_bitrate/5;
    }

    if (venc_bitrate - step_size < client->B_mode_params->min_bitrate) {
      venc_bitrate = client->B_mode_params->min_bitrate;
    } else {
      venc_bitrate = venc_bitrate - step_size;
    }

//Control using TCP window size
    if (bps_estimated_from_TCPwindow < venc_bitrate && buffer_dec == TRUE) {
      venc_bitrate = bps_estimated_from_TCPwindow;
    }
    if (B_mode_params->prev_setting_bps != venc_bitrate) {
      B_mode_params->prev_rtt_when_mode_changed = B_mode_params->rtt_moving_avg;
      B_mode_params->prev_mode_changed_time = B_mode_params->qos_count;
    }

  g_print("\nconsecutive_unloaded : %d\n", consecutive_unloaded);
  g_print("recent_consecutive_unloaded : %d\n", recent_consecutive_unloaded);
  g_print("qos_count : %d\n", B_mode_params->qos_count);
  g_print("B_mode_params->prev_mode_changed_time : %d\n", B_mode_params->prev_mode_changed_time);

//Increasing status
  } else if (consecutive_unloaded > NETWORK_STATUS_BUFFER- 4
              && recent_consecutive_unloaded > 3
              && current_network_status == NETWORK_UNLOADED
              && ((B_mode_params->qos_count - B_mode_params->prev_mode_changed_time > 19)      //1000ms interval for unloaded network status
              || (B_mode_params->qos_count - B_mode_params->prev_mode_changed_time > 9
                     && snd_cwnd >= MAXIMUM_WINDOW_SIZE))) {                //500ms interval for maximum window size
//Control using TCP window size
    if (snd_cwnd >= MAXIMUM_WINDOW_SIZE) {              //TCP Window Size is maximum. It means TCP throughput is maximum.
      step_size = venc_bitrate/5;     //20 % step size
    } else {
      step_size = venc_bitrate/10;    //10 % step size
    }

    if (venc_bitrate + step_size > client->B_mode_params->max_bitrate)
      venc_bitrate = client->B_mode_params->max_bitrate;
    else
      venc_bitrate = venc_bitrate + step_size;

    if (B_mode_params->prev_setting_bps != venc_bitrate) {
      B_mode_params->prev_rtt_when_mode_changed = B_mode_params->rtt_moving_avg;
      B_mode_params->prev_mode_changed_time = B_mode_params->qos_count;
    }
  }

  g_print("prev_rtt_when_mode_changed : %d\n", B_mode_params->prev_rtt_when_mode_changed);

  if(venc_bitrate < client->B_mode_params->min_bitrate)
    venc_bitrate = client->B_mode_params->min_bitrate;
  else if(venc_bitrate > client->B_mode_params->max_bitrate)
    venc_bitrate = client->B_mode_params->max_bitrate;

//Update parameters
  for(i = 0; i < NETWORK_STATUS_BUFFER-1; i++) {
    B_mode_params->prev_network_status[i] = B_mode_params->prev_network_status[i+1];
  }

  B_mode_params->prev_cwnd = snd_cwnd;
  B_mode_params->qos_count++;
  GST_WARNING ("[TCP] Current Bitrate value [%d] CnS[%d]", mPrevEncoderBitrate, current_network_status);
  g_print ("\n[TCP] Current Bitrate value [%d] CnS[%d]\n", mPrevEncoderBitrate, current_network_status);

  if(mPrevEncoderBitrate != venc_bitrate) {
    GST_ERROR ("[TCP] New Bitrate value [%d]", venc_bitrate);
    g_print ("[TCP] New Bitrate value [%d]\n", venc_bitrate);
    g_object_set (client->srcbin->venc, "bitrate", venc_bitrate, NULL);
    client->consecutive_low_bitrate_count = 0;
  } else if(mPrevEncoderBitrate == client->B_mode_params->min_bitrate){
    client->consecutive_low_bitrate_count++;
    if(client->consecutive_low_bitrate_count >= UNSTABLE_NW_POPUP_INTERVAL) {
      GstClockTime current_time = gst_clock_get_time (gst_element_get_clock(GST_ELEMENT (client->srcbin->venc)));
      current_time = current_time/1000000000;
      if((current_time - client->prev_noti_time) > UNSTABLE_NW_POPUP_INTERVAL) {
#ifdef SUPPORT_LAUNCH_SCREEN_MIRROR_APP		  
        int ret = launch_screen_mirror_app(ALLSHARE_CAST_PKGNAME, APP_NETWORK_WARN);
        if(ret < 0)
          GST_ERROR (" pop up is not working");
#else
		GST_ERROR (" launch_screen_mirror_app function is not supported !");
#endif

        client->consecutive_low_bitrate_count = 0;
        client->prev_noti_time = current_time;
      }
    }
  }

  B_mode_params->prev_setting_bps = venc_bitrate;
  g_print("--------- TCP bitrate done ----------------\n\n\n");
}

static gboolean
do_send_RTP_data_over_TCP (GstBuffer * buffer, guint8 channel, GstRTSPClient * client)
{
  GstRTSPMessage message = { 0 };
  guint8 *data;
  guint size;

  gst_rtsp_message_init_data (&message, channel);

  data = GST_BUFFER_DATA (buffer);
  size = GST_BUFFER_SIZE (buffer);

  gst_rtsp_message_take_body (&message, data, size);
  client->tcp_frag_num += 1;
  if(client->tcp_frag_num == 100) {
    configure_encoder_bitrate_over_tcp(client);
    client->tcp_frag_num = 0;
  }
  gst_rtsp_watch_send_message (client->datawatch, &message, NULL);

  gst_rtsp_message_steal_body (&message, &data, &size);
  gst_rtsp_message_unset (&message);

  return TRUE;
}

static gboolean
do_send_RTCP_data_over_TCP (GstBuffer * buffer, guint8 channel, GstRTSPClient * client)
{
  GstRTSPMessage message = { 0 };
  guint8 *data;
  guint size;

  gst_rtsp_message_init_data (&message, channel);

  data = GST_BUFFER_DATA (buffer);
  size = GST_BUFFER_SIZE (buffer);

  gst_rtsp_message_take_body (&message, data, size);

  //if (size == 80) g_print("\n\nFound 80 bytes!! in RTCP\n\n");
  //gst_rtsp_watch_send_message (client->datawatch, &message, NULL);

  gst_rtsp_message_steal_body (&message, &data, &size);
  gst_rtsp_message_unset (&message);

  return TRUE;
}

/**
* do_send_RTP_over_TCP_data_list:
* @blist: buffer list object
* @channel: chanel object
* @client: client object
*
* Callback function
* This is a callback function registered for TCP data transport when TCP streaming is enabled
*
* Returns: TRUE or FALSE
*/
static gboolean
do_send_RTP_over_TCP_data_list (GstBufferList * blist, guint8 channel,
    GstRTSPClient * client)
{
  GstBufferListIterator *it;

  it = gst_buffer_list_iterate (blist);
  while (gst_buffer_list_iterator_next_group (it)) {
    GstBuffer *group = gst_buffer_list_iterator_merge_group (it);

    if (group == NULL)
      continue;

    do_send_RTP_data_over_TCP (group, channel, client);
  }
  gst_buffer_list_iterator_free (it);

  return TRUE;
}

static gboolean
do_send_RTCP_over_TCP_data_list (GstBufferList * blist, guint8 channel,
    GstRTSPClient * client)
{
  GstBufferListIterator *it;

  it = gst_buffer_list_iterate (blist);
  while (gst_buffer_list_iterator_next_group (it)) {
    GstBuffer *group = gst_buffer_list_iterator_merge_group (it);

    if (group == NULL)
      continue;

    do_send_RTCP_data_over_TCP (group, channel, client);
  }
  gst_buffer_list_iterator_free (it);

  return TRUE;
}

#ifdef USE_VIDEO_PLAYER_STATE_CHANGE_CB
static void
gst_rtsp_client_extended_video_player_state_change_cb (keynode_t *key, void *data)
{
  GstRTSPClient * client = (GstRTSPClient *)data;
  guint temp_state = 0;
  if(!client) goto FAILURE;
  GST_DEBUG("Video Player state changed");
  temp_state = vconf_keynode_get_int(key);

  client->vconf_state = temp_state;
  client->eos = temp_state >> 2;

  GST_INFO("player state is %d", temp_state);
  GST_INFO("EOS is %d", client->eos);
  return;

FAILURE:
  GST_DEBUG ("Video Player STATE change failed to handle");
  return;
}

static void
gst_rtsp_client_extended_web_video_player_state_change_cb (keynode_t *key, void *data)
{
  GstRTSPClient * client = (GstRTSPClient *)data;
  guint web_video_player_state = 0;
  if(!client) goto FAILURE;
  web_video_player_state = vconf_keynode_get_int(key);
  client->vconf_web_video_state = web_video_player_state;

  if (web_video_player_state == VCONFKEY_WFD_WEB_VIDEO_PLAYER_STOP) {
    GST_WARNING_OBJECT(client, "vconf web video is not playing. change to udp");
    gst_rtsp_client_extended_switch_to_A_mode (client, client);
  }
  else {
    GST_WARNING_OBJECT(client, "vconf web video is playing. change to tcp");
    gst_rtsp_client_extended_switch_to_B_mode (client, client);
  }

  return;

FAILURE:
  GST_DEBUG ("vconf Web Video Player STATE change failed to handle");
  return;
}
#endif


GstRTSPResult
send_T1_request (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPUrl *url = NULL;
  gchar *url_str = NULL;

  url = gst_rtsp_connection_get_url (client->connection);
  if (url == NULL) {
    GST_ERROR_OBJECT (client, "Failed to get connection URL");
    return GST_RTSP_ERROR;
  }

  url_str = gst_rtsp_url_get_request_uri (url);
  if (url_str == NULL) {
    GST_ERROR_OBJECT (client, "Failed to get connection URL");
    return GST_RTSP_ERROR;
  }

  /* prepare the request for T1 message */
  res = prepare_request (client, &request, GST_RTSP_SET_PARAMETER, url_str, WFD_MESSAGE_T1, WFD_TRIGGER_UNKNOWN);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to prepare T1 request....\n");
    return res;
  }

  GST_DEBUG_OBJECT (client, "Sending SET_PARAMETER request message (M5)...");

  // TODO: need to add session i.e. 2nd variable
  send_request (client, NULL, &request);

  GstRTSPSession *session;
  GstRTSPSessionMedia *media;

  if (!(session = gst_rtsp_session_pool_find (client->session_pool, client->sessionID)))
  {
    GST_ERROR_OBJECT (client, "Failed to handle T1 message...");
    return FALSE;
  }
  client_watch_session (client, session);

  media = session->medias->data;
  media->media->adding = TRUE;
  media->media->target_state = GST_STATE_PLAYING;
  gst_rtsp_media_set_state (media->media, GST_STATE_PAUSED, media->streams);

  return res;
}

GstRTSPResult
handle_T1_response (GstRTSPClient * client, GstRTSPClientState * state, GstRTSPMessage * response)
{
  GstRTSPResult res = GST_RTSP_OK;

  /* Parsing the T1 response */
  {
    gchar *data = NULL;
    guint size=0;
    WFDMessage *msgb1res;
    WFDResult wfd_res = WFD_OK;
    guint buf_len = -1;
    GstRTSPTransport *ct, *st;
    GstRTSPSession *session;
    GstRTSPSessionMedia *media;
    GstRTSPSessionStream *stream;
    guint streamid = 0; /* Since there is only 1 stream */
    GstRTSPUrl *url = NULL;

    res = gst_rtsp_message_get_body (response, (guint8**)&data, &size);
    if (res != GST_RTSP_OK) {
      GST_ERROR_OBJECT (client, "Failed to get body of response...");
      goto error;
    }

    /* create M3 response message */
    wfd_res = wfdconfig_message_new(&msgb1res);
    if (wfd_res != WFD_OK) {
      GST_ERROR_OBJECT (client, "Failed to prepare wfd message...");
      goto error;
    }

    wfd_res = wfdconfig_message_init(msgb1res);
    if (wfd_res != WFD_OK) {
      GST_ERROR_OBJECT (client, "Failed to init wfd message...");
      goto error;
    }

    wfd_res = wfdconfig_message_parse_buffer((guint8*)data,size,msgb1res);
    if (wfd_res != WFD_OK) {
      GST_ERROR_OBJECT (client, "Failed to init wfd message...");
      goto error;
    }

    wfdconfig_message_dump(msgb1res);

    if ((client->T1_mode == WFD_T1_B_MODE) && (!msgb1res->client_rtp_ports)) {
      GST_ERROR_OBJECT (client, "This is NOT response for T1...");
      goto error;
    }

    /* Get the buffer length supported by WFDSink */

    wfd_res = wfdconfig_get_buffer_length(msgb1res, &buf_len);
   /* TODO : Decide what to do with the buffer length */

    if (wfd_res != WFD_OK) {
      GST_WARNING_OBJECT (client, "Failed to get wfd supported video formats...");
      goto error;
    }

    if ((client->T1_mode == WFD_T1_A_MODE) && (buf_len != 0)) {
      GST_ERROR_OBJECT (client, "This is NOT response for T1...(buf length : %d)", buf_len);
      goto error;
    }

    media = state->session->medias->data;
    session = state->session;

    /* get stream for media */
    if (!(stream = gst_rtsp_session_media_get_stream (media, streamid))) {
      GST_WARNING_OBJECT (client, "Failed to get stream...");
      g_object_unref (media);
      g_object_unref (session);
      goto error;
    }

    /* Force I-frame to send I-frame first - this is for homesync */
    g_object_set (client->srcbin->venc, "force-i-frame", 1, NULL);
    gst_rtsp_session_stream_set_callbacks (stream, NULL, NULL, NULL, NULL, NULL, NULL);

    gst_rtsp_transport_new (&ct);
    gst_rtsp_transport_init (ct);
    if (client->T1_mode == WFD_T1_B_MODE) {
      if (msgb1res->client_rtp_ports) {
        /* Get the RTP ports preferred by WFDSink */
        wfd_res = wfdconfig_get_prefered_RTP_ports(msgb1res, &client->ctrans, &client->cprofile, &client->clowertrans, &client->crtp_port0, &client->crtp_port1);
        if (wfd_res != WFD_OK) {
          GST_WARNING_OBJECT (client, "Failed to get wfd prefered RTP ports...");
          goto error;
        }
      } else {
        client->ctrans = WFD_RTSP_TRANS_RTP;
        client->cprofile = WFD_RTSP_PROFILE_AVP;
        client->clowertrans = WFD_RTSP_LOWER_TRANS_TCP;
      }
    } else if (client->T1_mode == WFD_T1_A_MODE) {
      client->ctrans = WFD_RTSP_TRANS_RTP;
      client->cprofile = WFD_RTSP_PROFILE_AVP;
      client->clowertrans = WFD_RTSP_LOWER_TRANS_UDP;
      client->sent_T2_E_msg = FALSE;
    }

    /* Parse proper transport protocol */
    map_transports(client,ct);
    if (ct->trans != GST_RTSP_TRANS_RTP || ct->profile != GST_RTSP_PROFILE_AVP) {
      GST_WARNING_OBJECT (client, "Trans or profile is wrong");
      goto no_transport;
    }
    if (ct->lower_transport == GST_RTSP_LOWER_TRANS_HTTP ||
       ct->lower_transport == GST_RTSP_LOWER_TRANS_UNKNOWN) {
      GST_WARNING_OBJECT (client, "Lowertrans is wrong");
      goto no_transport;
    }
    g_free (ct->destination);

    if (client->T1_mode == WFD_T1_A_MODE) {
      g_print ("\nSwitched to UDP !!!\n");
      /* Free any previous TCP connection */
      if(client->data_conn)
      {
        gst_rtsp_connection_close (client->data_conn);
        gst_rtsp_connection_free(client->data_conn);
        if (client->datawatch)
          g_source_destroy ((GSource *) client->datawatch);
      }
      url = gst_rtsp_connection_get_url (client->connection);
      gst_rtsp_url_set_port (url, client->crtp_port0);
      ct->destination = g_strdup (client->uristr);//g_strdup (url->host);
      ct->client_port.min = client->crtp_port0;
      if(client->crtp_port1 == 0)
        ct->client_port.max = client->crtp_port0 + 1;
      else ct->client_port.max = client->crtp_port1;
    } else if (client->T1_mode == WFD_T1_B_MODE) {
      res = new_tcp(client);
      if(res != GST_RTSP_OK)
      goto error;

      url = gst_rtsp_connection_get_url (client->data_conn);
      ct->destination = g_strdup (client->uristr);//g_strdup (url->host);
      ct->client_port.min = client->crtp_port0;
      if(client->crtp_port1 == 0)
        ct->client_port.max = client->crtp_port0 + 1;
      else ct->client_port.max = client->crtp_port1;
    }
    if (url) stream->media_stream->server_port.min= url->port;
    stream->media_stream->server_port.max= -1;

    st = gst_rtsp_session_stream_set_transport (stream, ct);
    if(st == NULL) {
      GST_ERROR_OBJECT (client, "Error in setting transport");
      goto no_transport;
    }

    GST_DEBUG ("client %p: linking stream %p", client, stream);
    if (client->T1_mode == WFD_T1_B_MODE) {
      g_print ("\nSwitched to TCP !!!\n");
      gst_rtsp_session_stream_set_callbacks (stream, (GstRTSPSendFunc) do_send_RTP_data_over_TCP,
          (GstRTSPSendFunc) do_send_RTCP_data_over_TCP, (GstRTSPSendListFunc) do_send_RTP_over_TCP_data_list,
          (GstRTSPSendListFunc) do_send_RTCP_over_TCP_data_list, client, NULL);

      g_object_set (client->srcbin->venc, "bitrate", client->B_mode_params->init_bitrate, NULL);
    }
    else if(client->T1_mode == WFD_T1_A_MODE) {
      g_print ("\nSwitched to UDP !!!\n");
      /* configure keepalive for this transport */
      gst_rtsp_session_stream_set_callbacks (stream, NULL, NULL, NULL, NULL, client, NULL);
      gst_rtsp_session_stream_set_keepalive (stream, (GstRTSPKeepAliveFunc) do_keepalive, session, NULL);

      g_object_set (client->srcbin->venc, "bitrate", client->init_udp_bitrate, NULL);
    }
    /* make sure our session can't expire */
    gst_rtsp_session_prevent_expire (session);

    /* start playing after sending the request */
    gst_rtsp_session_media_set_state (media, GST_STATE_PLAYING);
    media->state = GST_RTSP_STATE_PLAYING;
  }
  return res;

  error:
  return GST_RTSP_ERROR;

  no_transport:
  {
    send_generic_response (client, GST_RTSP_STS_UNSUPPORTED_TRANSPORT, state);
    return GST_RTSP_ERROR;
  }
}

static GstRTSPResult
handle_T2_A_message (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPUrl *url = NULL;
  gchar *url_str = NULL;

  url = gst_rtsp_connection_get_url (client->connection);
  if (url == NULL) {
    GST_ERROR_OBJECT (client, "Failed to get connection URL");
    return GST_RTSP_ERROR;
  }

  url_str = gst_rtsp_url_get_request_uri (url);
  if (url_str == NULL) {
    GST_ERROR_OBJECT (client, "Failed to get connection URL");
    return GST_RTSP_ERROR;
  }

  res = prepare_request (client, &request, GST_RTSP_SET_PARAMETER, url_str, WFD_MESSAGE_T2_A, WFD_TRIGGER_PLAY);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to prepare T2 A request....\n");
    return res;
  }

  GST_DEBUG_OBJECT (client, "Sending GST_RTSP_SET_PARAMETER request message (T2 A)...");

  // TODO: need to add session i.e. 2nd variable
  send_request (client, NULL, &request);

  /* Wait for GST_RTSP_SET_PARAMETER response (T2 A response) */
  res = gst_rtsp_connection_receive (client->connection, &response, NULL);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to receive T2 A response....\n");
    return res;
  }

  if (gst_debug_category_get_threshold (rtsp_extended_client_debug) >= GST_LEVEL_LOG) {
    gst_rtsp_message_dump (&response);
  }

  return res;
}

static GstRTSPResult
handle_T2_B_message (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPUrl *url = NULL;
  gchar *url_str = NULL;

  url = gst_rtsp_connection_get_url (client->connection);
  if (url == NULL) {
    GST_ERROR_OBJECT (client, "Failed to get connection URL");
    return GST_RTSP_ERROR;
  }

  url_str = gst_rtsp_url_get_request_uri (url);
  if (url_str == NULL) {
    GST_ERROR_OBJECT (client, "Failed to get connection URL");
    return GST_RTSP_ERROR;
  }

  res = prepare_request (client, &request, GST_RTSP_SET_PARAMETER, url_str, WFD_MESSAGE_T2_B, WFD_TRIGGER_PAUSE);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to prepare T2 B request....\n");
    return res;
  }

  GST_DEBUG_OBJECT (client, "Sending GST_RTSP_SET_PARAMETER request message (T2 B)...");

  // TODO: need to add session i.e. 2nd variable
  send_request (client, NULL, &request);

  /* Wait for GST_RTSP_SET_PARAMETER response (T2 B response) */
  res = gst_rtsp_connection_receive (client->connection, &response, NULL);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to receive T2 B response....\n");
    return res;
  }

  if (gst_debug_category_get_threshold (rtsp_extended_client_debug) >= GST_LEVEL_LOG) {
    gst_rtsp_message_dump (&response);
  }

  return res;
}

GstRTSPResult handle_T2_E_message (GstRTSPClient * client) {

  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPUrl *url = NULL;
  gchar *url_str = NULL;

  url = gst_rtsp_connection_get_url (client->connection);
  if (url == NULL) {
    GST_ERROR_OBJECT (client, "Failed to get connection URL");
    return GST_RTSP_ERROR;
  }

  url_str = gst_rtsp_url_get_request_uri (url);
  if (url_str == NULL) {
    GST_ERROR_OBJECT (client, "Failed to get connection URL");
    return GST_RTSP_ERROR;
  }

  res = prepare_request (client, &request, GST_RTSP_SET_PARAMETER, url_str, WFD_MESSAGE_T2_E, WFD_TRIGGER_UNKNOWN);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to prepare T2 E request....\n");
    return res;
  }

  GST_DEBUG_OBJECT (client, "Sending GST_RTSP_SET_PARAMETER request message (T2)...");

  // TODO: need to add session i.e. 2nd variable
  send_request (client, NULL, &request);

  return res;
}

static GstRTSPResult
handle_U1_message (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPUrl *url = NULL;
  gchar *url_str = NULL;
  gchar *data = NULL;
  guint size=0;
  WFDMessage *msgu1res;
  WFDResult wfd_res = WFD_OK;

  url = gst_rtsp_connection_get_url (client->connection);
  if (url == NULL) {
    GST_ERROR_OBJECT (client, "Failed to get connection URL");
    return GST_RTSP_ERROR;
  }

  url_str = gst_rtsp_url_get_request_uri (url);
  if (url_str == NULL) {
    GST_ERROR_OBJECT (client, "Failed to get connection URL");
    return GST_RTSP_ERROR;
  }

  res = prepare_request (client, &request, GST_RTSP_SET_PARAMETER, url_str, WFD_MESSAGE_U1, WFD_TRIGGER_UNKNOWN);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to prepare U1 request....\n");
    return res;
  }

  GST_DEBUG_OBJECT (client, "Sending GST_RTSP_SET_PARAMETER request message (U1)...");

  // TODO: need to add session i.e. 2nd variable
  send_request (client, NULL, &request);

  /* Wait for GST_RTSP_SET_PARAMETER response (U1 response) */
  res = gst_rtsp_connection_receive (client->connection, &response, NULL);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to receive U1 response....\n");
    return res;
  }

  if (gst_debug_category_get_threshold (rtsp_extended_client_debug) >= GST_LEVEL_LOG) {
    gst_rtsp_message_dump (&response);
  }

  /* Parsing the U1 response */

   res = gst_rtsp_message_get_body (&response, (guint8 **)&data, &size);
   if (res != GST_RTSP_OK) {
     GST_ERROR_OBJECT (client, "Failed to get body of response...");
     return res;
   }

   /* create U1 response message */
   wfd_res = wfdconfig_message_new(&msgu1res);
   if (wfd_res != WFD_OK) {
     GST_ERROR_OBJECT (client, "Failed to prepare wfd message...");
     return res;
   }

   wfd_res = wfdconfig_message_init(msgu1res);
   if (wfd_res != WFD_OK) {
     GST_ERROR_OBJECT (client, "Failed to init wfd message...");
     return res;
   }

   wfd_res = wfdconfig_message_parse_buffer((guint8 *)data,size,msgu1res);
   if (wfd_res != WFD_OK) {
     GST_ERROR_OBJECT (client, "Failed to init wfd message...");
     return res;
   }

   wfdconfig_message_dump(msgu1res);
   wfd_res = wfdconfig_get_dongle_upgrade(msgu1res, &client->upgrade_URL);
   if (wfd_res != WFD_OK) {
     GST_WARNING_OBJECT (client, "Failed to get wfd dongle upgradation URL...");
     return res;
   }

  return res;
}

static GstRTSPResult
handle_U2_message (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPUrl *url = NULL;
  gchar *url_str = NULL;

  url = gst_rtsp_connection_get_url (client->connection);
  if (url == NULL) {
    GST_ERROR_OBJECT (client, "Failed to get connection URL");
    return GST_RTSP_ERROR;
  }

  url_str = gst_rtsp_url_get_request_uri (url);
  if (url_str == NULL) {
    GST_ERROR_OBJECT (client, "Failed to get connection URL");
    return GST_RTSP_ERROR;
  }

  res = prepare_request (client, &request, GST_RTSP_SET_PARAMETER, url_str, WFD_MESSAGE_U2, WFD_TRIGGER_UNKNOWN);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to prepare U2 request....\n");
    return res;
  }

  GST_DEBUG_OBJECT (client, "Sending GST_RTSP_SET_PARAMETER request message (U2)...");

  // TODO: need to add session i.e. 2nd variable
  send_request (client, NULL, &request);

  /* Wait for GST_RTSP_SET_PARAMETER response (U2 response) */
  res = gst_rtsp_connection_receive (client->connection, &response, NULL);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to receive U2 response....\n");
    return res;
  }

  if (gst_debug_category_get_threshold (rtsp_extended_client_debug) >= GST_LEVEL_LOG) {
    gst_rtsp_message_dump (&response);
  }
  return res;
}

gboolean
gst_rtsp_client_extended_switch_to_TCP (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  client->T1_mode = WFD_T1_B_MODE;

  res = send_T1_request(client);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to send T1 message...");
    return FALSE;
  }

  GST_DEBUG_OBJECT (client, "switching to TCP is done successfully");
  return TRUE;
}

gboolean
gst_rtsp_client_extended_switch_to_UDP (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;

  client->T1_mode = WFD_T1_A_MODE;

  res = send_T1_request(client);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to send T1 message...");
    return FALSE;
  }

  GST_DEBUG_OBJECT (client, "switching to UDP is done successfully");
  return TRUE;
}


gboolean gst_rtsp_client_extended_playback_play (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;

  if(client->enable_spec_features) {
    res = handle_T2_A_message (client);
    if (GST_RTSP_OK != res) {
      GST_ERROR_OBJECT (client, "Failed to handle T2 A message...");
      return FALSE;
    }
  } else {
    GST_ERROR_OBJECT (client, "Failed to handle T2 A : sink control playback - PLAY...");
    return FALSE;
  }

  return TRUE;
}

gboolean gst_rtsp_client_extended_playback_pause (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;

  if(client->enable_spec_features) {
    res = handle_T2_B_message (client);
    if (GST_RTSP_OK != res) {
      GST_ERROR_OBJECT (client, "Failed to handle T2 B message...");
      return FALSE;
    }
  } else {
    GST_ERROR_OBJECT (client, "Failed to handle T2 B : sink control playback - PAUSE...");
    return FALSE;
  }

  return TRUE;
}

gboolean gst_rtsp_client_extended_playback_set_volume (GstRTSPClient *client, gchar *volume)
{
  GstRTSPResult res = GST_RTSP_OK;
  client->volume = g_ascii_strtod(volume, NULL);
  g_print("spec_features %d\n", client->enable_spec_features);
  if (client->enable_spec_features) {
    res = handle_T2_E_message (client);
    if (GST_RTSP_OK != res) {
      GST_ERROR_OBJECT (client, "Failed to handle T2 E message...");
      return FALSE;
    }
  } else {
    GST_ERROR_OBJECT (client, "Failed to handle T2 E : sink control playback - set volume...");
    return FALSE;
  }
  return TRUE;
}

gboolean gst_rtsp_client_extended_upgrade_dongle (GstRTSPClient * client, gchar *upgrade_version, gchar **upgrade_url)
{
  GstRTSPResult res = GST_RTSP_OK;
  client->upgrade_version = g_strdup(upgrade_version);

  res = handle_U1_message (client);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to handle U1 message...");
    return FALSE;
  }
  *upgrade_url = g_strdup(client->upgrade_URL);
  return TRUE;
}

gboolean gst_rtsp_client_extended_rename_dongle (GstRTSPClient * client, gchar *rename)
{
  GstRTSPResult res = GST_RTSP_OK;
  client->rename_dongle = g_strdup(rename);
  if(client->enable_spec_features) {
    res = handle_U2_message (client);
    if (GST_RTSP_OK != res) {
      GST_ERROR_OBJECT (client, "Failed to handle U2 message...");
      return FALSE;
    }
  } else {
    GST_ERROR_OBJECT (client, "Failed to handle U2 dongle rename message...");
    return FALSE;
  }
  return TRUE;
}

GstRTSPResult
gst_rtsp_client_extended_handle_response (GstRTSPClient * client, GstRTSPMessage * message)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPSession *session;
  GstRTSPClientState state = { NULL };
  guint8 *data;
  guint size;

  res = gst_rtsp_message_get_body (message, &data, &size);
  if (res != GST_RTSP_OK)
    return res;

  if (g_strrstr((char *)data, "wfd_vnd_sec_max_buffer_length")) {
    GST_DEBUG_OBJECT(client, "This is response for T1 msg");

    if (!(session = gst_rtsp_session_pool_find (client->session_pool, client->sessionID)))
    {
      GST_ERROR_OBJECT (client, "Failed to handle T1 response...");
      return GST_RTSP_OK;
    }
    state.session = session;

    res = handle_T1_response(client, &state, message);
    if (GST_RTSP_OK != res) {
      GST_ERROR_OBJECT (client, "Failed to handle T1 response...");

      /* change state to PLAYING again */
      GstRTSPSessionMedia *media;
      media = session->medias->data;

      gst_rtsp_session_media_set_state (media, GST_STATE_PLAYING);
      media->state = GST_RTSP_STATE_PLAYING;

      if (client->T1_mode == WFD_T1_A_MODE)
        client->T1_mode = WFD_T1_B_MODE;
      else
        client->T1_mode = WFD_T1_A_MODE;

      return res;
    }
  } else if (g_strrstr((char *)data, "set_volume")) {
    GST_DEBUG_OBJECT(client, "This is response for T2 msg");
  }

  return res;
}

static GstRTSPResult
closed_tcp (GstRTSPWatch * watch, gpointer user_data)
{
  GstRTSPClient *client = GST_RTSP_CLIENT (user_data);

  GST_INFO ("client %p: connection closed", client);

  return GST_RTSP_OK;
}

static GstRTSPResult
error_full_tcp (GstRTSPWatch * watch, GstRTSPResult result,
    GstRTSPMessage * message, guint id, gpointer user_data)
{
  GstRTSPClient *client = GST_RTSP_CLIENT (user_data);
  gchar *str;

  str = gst_rtsp_strresult (result);
  GST_INFO
      ("client %p: received an error %s when handling message %p with id %d",
      client, str, message, id);
  g_free (str);

  return GST_RTSP_OK;
}

#if 0
static GstRTSPWatchFuncs watch_funcs_tcp = {
  message_received,
  message_sent,
  closed_tcp,
  error,
  tunnel_start,
  tunnel_complete,
  error_full_tcp,
  tunnel_lost
};
#else
static GstRTSPWatchFuncs watch_funcs_tcp = {
  message_received,
  message_sent,
  closed_tcp,
  error,
  NULL,
  NULL,
  error_full_tcp,
  NULL
};
#endif

static void
client_watch_notify_tcp (GstRTSPClient * client)
{
  GST_INFO ("client %p: watch destroyed", client);
  client->datawatch = NULL;
  client->data_conn = NULL;
}

/**
* new_tcp:
* @client: client object
*
* Creates new TCP connection
* This API will create new TCP socket for TCP streaming
*
* Returns: #GstRTSPResult.
*/

static GstRTSPResult
new_tcp (GstRTSPClient * client)
{
  int fd;
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPConnection *conn = NULL;
  GstRTSPUrl *url;
  GTimeVal tcp_timeout;
  guint64 timeout = 20000000;
  struct sockaddr_in server_addr;
  socklen_t sin_len;
  GSource *source;
  GMainContext *context;
  int retry = 0;

  tcp_timeout.tv_sec = timeout / G_USEC_PER_SEC;
  tcp_timeout.tv_usec = timeout % G_USEC_PER_SEC;

  /* Get the client connection details */
  url = gst_rtsp_connection_get_url(client->connection);
  if(!url)
    return GST_RTSP_ERROR;

  gst_rtsp_url_set_port (url, client->crtp_port0);
  url->host = g_strdup(client->uristr);

  GST_INFO ("create new connection %p ip %s:%d %s", client, url->host, url->port, client->uristr);

  if ((res = gst_rtsp_connection_create (url, &conn)) < 0)
    return GST_RTSP_ERROR;

tcp_retry:
  if ((res = gst_rtsp_connection_connect (conn, &tcp_timeout)) < 0) {
    g_print( "Error connecting socket : %s\n", strerror( errno ) );
    if (retry < 50) {
      GST_ERROR("Connection failed... Try again...");
      usleep(100000);
      retry++;
      goto tcp_retry;
    }

    return GST_RTSP_ERROR;
  }

  GST_DEBUG_OBJECT (client, "Able to connect to new port");

  sin_len = sizeof(struct sockaddr);
  fd = gst_rtsp_connection_get_readfd (conn);
  if (fd == -1) {
    return GST_RTSP_EINVAL;
  }

  if (getsockname (fd, (struct sockaddr *)&server_addr, &sin_len) < 0) {
    GST_ERROR_OBJECT (client, "Getsockname fail");
    close(fd);
    return GST_RTSP_ERROR;
  }

  GST_DEBUG_OBJECT (client, "New port created : %d", ntohs(server_addr.sin_port));

  int bsize = 1024000;
  int rn = sizeof(int);
  /* Set send buffer size to 5242880 */
  if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bsize, (socklen_t)rn) < 0) {
    GST_ERROR_OBJECT(client, "setsockopt failed");
  } else {
    getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bsize, (socklen_t *)&rn);
    GST_WARNING_OBJECT(client, "New Send buf size : %d\n", bsize);
  }

  int state = 1;
  if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &state, sizeof(state)) < 0) {
    GST_ERROR_OBJECT(client, "setsockopt failed");
  } else {
    GST_WARNING_OBJECT(client, "TCP NO DELAY");
  }

  url->transports = GST_RTSP_LOWER_TRANS_TCP;
  url->host = g_strndup (client->server_ip, INET6_ADDRSTRLEN);
  url->port = ntohs(server_addr.sin_port);

  GST_DEBUG_OBJECT (client, "url->host copied : %s", url->host);
  GST_DEBUG_OBJECT (client, "url->port copied : %d", url->port);
  client->data_conn = conn;

  /* create watch for the connection and attach */
  client->datawatch = gst_rtsp_watch_new (client->data_conn, &watch_funcs_tcp, client, (GDestroyNotify) client_watch_notify_tcp);
  client->tcp_frag_num = 0;
  client->B_mode_params->curr_rtt = 0;
  GST_DEBUG_OBJECT (client, "data watch : %x", client->datawatch);
   /* find the context to add the watch */
   if ((source = g_main_current_source ()))
     context = g_source_get_context (source);
   else
     context = NULL;

   GST_DEBUG (" source = %p", source);
   GST_INFO ("attaching to context %p", context);
   client->datawatchid = gst_rtsp_watch_attach (client->datawatch, context);
   gst_rtsp_watch_unref (client->datawatch);
     return res;
}

static gboolean bitrate_config (GstRTSPClient * client, int bitrate)
{
  int prev_bitrate = 0;
  g_object_get (client->srcbin->venc, "bitrate", &prev_bitrate, NULL);

  if(prev_bitrate != bitrate) {
    g_object_set (client->srcbin->venc, "bitrate", bitrate, NULL);
    GST_ERROR ("[UDP] New Bitrate value [%d]", bitrate);
  }

  if(prev_bitrate == client->min_udp_bitrate && prev_bitrate == bitrate)
    client->consecutive_low_bitrate_count++;
  else client->consecutive_low_bitrate_count = 0;

  if(client->consecutive_low_bitrate_count >= UNSTABLE_NW_POPUP_INTERVAL){
    GstClockTime current_time = gst_clock_get_time (gst_element_get_clock(GST_ELEMENT (client->srcbin->venc)));
    current_time = current_time/1000000000;
    if((current_time - client->prev_noti_time) > UNSTABLE_NW_POPUP_INTERVAL) {
#ifdef SUPPORT_LAUNCH_SCREEN_MIRROR_APP 		
      int ret = launch_screen_mirror_app(ALLSHARE_CAST_PKGNAME, APP_NETWORK_WARN);
      if(ret < 0)
        GST_ERROR (" pop up is not working");
#else
	  GST_ERROR (" launch_screen_mirror_app function is not supported !");
#endif

      client->consecutive_low_bitrate_count = 0;
      client->prev_noti_time = current_time;
    }
  }

  return TRUE;
}

gboolean udp_src_cb_handler(GstPad * pad, GstMiniObject * obj, gpointer u_data)
{
  GstRTSPClient * client = (GstRTSPClient *)u_data;
  gint8 *data;
  gint size = 0;
  gint bitrate = 0;
  static int rr_freq = 0;
  guint8 fraction_lost = 0;
  guint32 max_packet_lost = 0;
  guint16 max_seq_num = 0;
  guint latest_packets_resend = 0;
  float thretholdValue = 0;
  static int fraction_lost_MA;

  if (obj == NULL || pad == NULL || client == NULL ) {
    GST_WARNING("udp_src_cb_handler is NULL");
    return FALSE;
  }

  g_object_get (client->srcbin->venc, "bitrate", &bitrate, NULL);
  GST_WARNING ("[UDP] Current Bitrate value [%d]", bitrate);

  if (GST_IS_BUFFER (obj)) {
    rr_freq++;
    GstBuffer *buffer = GST_BUFFER_CAST (obj);
    //GST_WARNING ("got buffer %p with size %d", buffer, GST_BUFFER_SIZE (buffer));
    data = (gint8 *)GST_BUFFER_DATA(buffer);
    size = GST_BUFFER_SIZE(buffer);

    GstBitReader br = GST_BIT_READER_INIT ((const guint8 *)data, size);
    gst_bit_reader_set_pos(&br, (8*12));
    gst_bit_reader_get_bits_uint8(&br, &fraction_lost, 8);
    gst_bit_reader_get_bits_uint32(&br, &max_packet_lost, 24);
    gst_bit_reader_skip(&br, 16);
    gst_bit_reader_get_bits_uint16(&br, &max_seq_num, 16);
    GST_ERROR("fraction lost from from bitreader is %d %d %d", fraction_lost, max_seq_num, max_packet_lost);

    /* Cross checking with resender packet resend count in case of faulty RTCP reports
     * as it happens with TV.*/
    g_object_get (client->media->resender, "rtp-packets-resend", &latest_packets_resend, NULL);
    GST_DEBUG ("Number of packets resent in this session is %d", latest_packets_resend);
    if(client->packets_resend == latest_packets_resend)
      fraction_lost = 0;
    client->packets_resend = latest_packets_resend;

    if(client->prev_max_seq_num == max_seq_num)
      goto config;

    if(client->first_rtcp == FALSE) {
      GST_WARNING("Ignoring first receiver report");
      client->prev_fraction_lost = 0;
      /*In some dongle the first rtcp report generated after switching from TCP to UDP, we see there are some losses shown
       * so to avoid such scenarios, we make fraction loss of first RTCP as 0*/
      client->prev_max_packet_lost = max_packet_lost;
      client->prev_max_seq_num = max_seq_num;
      fraction_lost_MA = 0;
      client->first_rtcp = TRUE;
      return TRUE;
    }

    if(client->prev_fraction_lost == 0)
      thretholdValue = 1.0;
    else
      thretholdValue = 0.8;

    int temp_fraction_lost = 0;
    int statistics_fraction_lost = 0;

    if (fraction_lost > 0) {
      temp_fraction_lost = fraction_lost * 100 / 256;
      GST_WARNING("fraction lost from sink RR [%d]", temp_fraction_lost);
    } else {
      if((max_seq_num > client->prev_max_seq_num) && (max_packet_lost > client->prev_max_packet_lost))
        temp_fraction_lost = (((max_packet_lost - client->prev_max_packet_lost) * 100)/(max_seq_num - client->prev_max_seq_num));
      GST_WARNING("fraction lost calculated in videotask [%d]", temp_fraction_lost);
    }
    statistics_fraction_lost = (int)(temp_fraction_lost * thretholdValue + client->prev_fraction_lost * (1 - thretholdValue));
    fraction_lost_MA = (fraction_lost_MA * 7 + statistics_fraction_lost * 5)/8;

    if(fraction_lost_MA > 100) {
      fraction_lost_MA = 100;
    }
    GST_WARNING("statistics_fraction_lost = %d, fraction_lost_MA = %d",statistics_fraction_lost, fraction_lost_MA);


    if (temp_fraction_lost > 0) {
      guint32 temp_change_bandwith_amount = 0;

      if (statistics_fraction_lost >= 5) {
        temp_change_bandwith_amount = client->max_udp_bitrate - client->min_udp_bitrate;
      } else if (statistics_fraction_lost >= 3) {
    	temp_change_bandwith_amount = (client->max_udp_bitrate - client->min_udp_bitrate)/2;
      } else {
    	temp_change_bandwith_amount = (client->max_udp_bitrate - client->min_udp_bitrate)/4;
      }

      GST_WARNING("LOSS case, statistics_fraction_lost = %d percent, temp_change_bandwith_amount = %d bit",statistics_fraction_lost, temp_change_bandwith_amount);
      if (bitrate <= client->min_udp_bitrate) {
        bitrate = client->min_udp_bitrate;
        client->prev_fraction_lost = statistics_fraction_lost;
        client->prev_max_seq_num = max_seq_num;
        client->prev_max_packet_lost = max_packet_lost;
        goto config;
      }

      bitrate = bitrate - temp_change_bandwith_amount;

      if(bitrate < client->min_udp_bitrate)
        bitrate = client->min_udp_bitrate;

    } else if (0 == temp_fraction_lost && fraction_lost_MA < 1) {
      if (bitrate >= client->max_udp_bitrate) {
        GST_WARNING("bitrate can not be increased");
        bitrate = client->max_udp_bitrate;
        client->prev_fraction_lost = statistics_fraction_lost;
        client->prev_max_seq_num = max_seq_num;
        client->prev_max_packet_lost = max_packet_lost;
        goto config;
      }

      if (0 == client->prev_fraction_lost) {
        bitrate += 512*1024;
    	//no loss in previous RR. Increase by 500 Kbps
      } else {
        bitrate += 1024*1024;
    	//previous RR showed loss. Increase by 100Kbps
      }
      if(bitrate > client->max_udp_bitrate)
        bitrate = client->max_udp_bitrate;
    }

    client->prev_fraction_lost = statistics_fraction_lost;
    client->prev_max_seq_num = max_seq_num;
    client->prev_max_packet_lost = max_packet_lost;

    GST_WARNING("final_bitrate_is %d", bitrate);
  }

config:
  bitrate_config(client, bitrate);

  return TRUE;
}

#ifdef SUPPORT_LAUNCH_SCREEN_MIRROR_APP
static int launch_screen_mirror_app(char *app_id, network_type_e type)
{
  int ret = SERVICE_ERROR_NONE;
  service_h service = NULL;

  if (app_id == NULL) {
    GST_ERROR("app id is NULL");
    return -1;
  }

  ret = service_create(&service);
  if (ret != SERVICE_ERROR_NONE) {
   GST_DEBUG("service_create() return error : %d", ret);
   return ret;
  }

  if (service == NULL) {
    GST_ERROR("service is NULL");
    return -1;
  }

  service_set_operation(service, SERVICE_OPERATION_DEFAULT);
  service_set_app_id(service, app_id);

  if (type == APP_NETWORK_WARN) {
    service_add_extra_data(service, "-t", "warning_by_unstable_network");
  } else if (type == APP_HDMI_WARN) {
    service_add_extra_data(service, "-t", "off_without_popup");
  }

  GST_DEBUG("Launch popup" );
  ret = service_send_launch_request(service, NULL, NULL);
  if (ret != SERVICE_ERROR_NONE) {
    GST_DEBUG("service_send_launch_request() is failed : %d", ret);
  }

  service_destroy(service);
  return ret;
}
#endif
void gst_rtsp_client_extended_add_udpsrc_pad_probe (GstRTSPClient *client, GstRTSPMediaStream *stream)
{
  int port;
  GstPad *pad;
  g_object_get (G_OBJECT (stream->udpsrc[1]), "port", &port, NULL);
  GST_DEBUG("port %d", port);
  pad = gst_element_get_static_pad (stream->udpsrc[1], "src");
  client->rtcp_udpsrc_pad = pad;
  client->rtcp_pad_handle_id = gst_pad_add_data_probe(pad,  G_CALLBACK(udp_src_cb_handler), client);
}

void gst_rtsp_client_extended_send_T2_E_msg (GstRTSPClient *client)
{
  GstRTSPResult res = GST_RTSP_OK;

  if (client->T1_mode == WFD_T1_B_MODE && client->sent_T2_E_msg == FALSE) {
    client->volume = 15.0;
    res = handle_T2_E_message (client);
    if (GST_RTSP_OK != res) {
      GST_ERROR_OBJECT (client, "Failed to handle T2 E message...");
    }
    client->sent_T2_E_msg = TRUE;
  }
}

void
gst_rtsp_client_extended_enable_T1_switching (GstRTSPClient *client, gboolean enable_T1_switching)
{
  client->enable_T1_switching = enable_T1_switching;
  return;
}

static gboolean dump_event (GstPad * pad, GstMiniObject * obj, gpointer u_data)
{
  GstEvent *event = GST_EVENT (obj);
  GstRTSPClient *client = (GstRTSPClient*)u_data;

  if (!client->enable_spec_features) {
    return TRUE;
  }

  if (client->T1_mode == WFD_T1_A_MODE) {
    return TRUE;
  }

  if (GST_EVENT_TYPE (event) == GST_EVENT_CUSTOM_DOWNSTREAM) {
    GST_WARNING("Downstream event received");
    client->B_mode_eos = FALSE;
    client->timer_count = 0;
    gst_element_set_state(client->media->pipeline, GST_STATE_PAUSED);

    g_timeout_add(500, timer_switch_to_udp, client);
    //g_cond_signal(client->B_mode_cond);
  }
  return TRUE;
}

void
gst_rtsp_client_extended_pad_add_event_probe(GstRTSPClient * client, GstPad *pad)
{
  gst_pad_add_event_probe (pad, G_CALLBACK (dump_event), client);
}


GstRTSPResult gst_rtsp_client_extended_prepare_protection(GstRTSPClient * client)
{
#ifdef USE_HDCP
  if (client->hdcp_support) {
        int ret = 0;
        int retry = 0;
        int hdcp_version;
        HDCP2_Ctx *hdcp = NULL;
        hdcp = (HDCP2_Ctx*)malloc(sizeof(HDCP2_Ctx));
        GST_DEBUG("HDCP_Init Called");
        if (client->hdcp_version == 1) {
          hdcp_version = HDCP2_VERSION_2_0;
          GST_WARNING("HDCP version 2.0");
        } else if (client->hdcp_version == 2) {
          hdcp_version = HDCP2_VERSION_2_1;
          GST_WARNING("HDCP version 2.1");
        } else {
          hdcp_version = HDCP2_VERSION_2_1;
          GST_WARNING("Unknown HDCP version.. set to 2.1");
        }

        if ((ret = HDCP2_Init(hdcp, HDCP2_TRANSMITTER, hdcp_version)) < 0) {
          GST_ERROR("HDCP_Init failed [%d]", ret);
          HDCP2_Close(hdcp);
		if(ret == -131) {
			GST_ERROR("HDCP Key not installed");
			return GST_RTSP_ENOHDCPKEY;
		} else
			return GST_RTSP_ERROR;
        } else {
          GST_DEBUG("HDCP2_Connect Called");
hdcp_retry:
          ret = HDCP2_Connect(hdcp, client->wfdsink_ip, client->hdcp_tcpport);
          if (ret == 0) {
            GST_DEBUG("Successfully Done AKE Transmitter\n");
          } else {
            if (retry == 0) {
              usleep(1000000);
              retry++;
              GST_WARNING("Connect failed...Try again...");
              goto hdcp_retry;
            }
            GST_ERROR("HDCP2_Connect failed");
            HDCP2_Close(hdcp);
            return GST_RTSP_ERROR;
          }
        }
        client->hdcp_handle = (void *)hdcp;
  }
#endif
  return GST_RTSP_OK;
}

void on_rtp_fraction_lost_cb (void *sess, guint fraction_lost, guint bandwidth, gpointer udata)
{
  GstRTSPClient * client = (GstRTSPClient *)udata;
  if(!client) return;
  GST_ERROR ("on rtp fraction lost fraction: %d bandwidth: %d",fraction_lost, bandwidth);

  if(fraction_lost){
    g_object_set (client->media->resender, "rtp-fraction-lost", fraction_lost, NULL);
  }
  return;
}

