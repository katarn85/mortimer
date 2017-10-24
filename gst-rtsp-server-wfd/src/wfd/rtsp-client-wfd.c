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
#include <netdb.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <glib/gprintf.h>
#include <dlfcn.h>

#include "rtsp-client-wfd.h"
#include "mmf/wfdconfigmessage.h"

#ifndef ENABLE_QC_SPECIFIC
#include <exynos_drm.h>
#include <xf86drm.h>
#include <sys/ioctl.h>
#endif

#include <X11/X.h>
#include <X11/Xlibint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <dri2.h>
#include <utilX.h>

#define DEFAULT_UIBC_PORT 19005
#define DEFAULT_UIBC_HIDC_CAP_COUNT 4
#define DEFAULT_WFD_MTU_SIZE 1400
#define DEFAULT_RTSP_TIMEOUT 60

enum
{
  SIGNAL_CLOSED,
  SIGNAL_ERROR,
  SIGNAL_TEARDOWN,
  SIGNAL_LAST
};

GST_DEBUG_CATEGORY_STATIC (rtsp_client_wfd_debug);
#define GST_CAT_DEFAULT rtsp_client_wfd_debug

static GstRTSPResult handle_M1_message (GstRTSPClient * client);
static GstRTSPResult handle_M2_message (GstRTSPClient * client);
static GstRTSPResult handle_M3_message (GstRTSPClient * client);
static GstRTSPResult handle_M4_message (GstRTSPClient * client);
static GstRTSPResult handle_M5_message (GstRTSPClient * client, WFDTrigger trigger_type);

static GstRTSPResult handle_M12_message (GstRTSPClient * client);
static gboolean gst_rtsp_client_parse_methods (GstRTSPClient * client, GstRTSPMessage * response);
static gboolean gst_rtsp_client_sending_m16_message (GstRTSPClient * client);
static gboolean keep_alive_condition(gpointer userdata);

static const char fake_edid_info[] = {
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x4c, 0x2d, 0x05, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x30, 0x12, 0x01, 0x03, 0x80, 0x10, 0x09, 0x78,
    0x0a, 0xee, 0x91, 0xa3, 0x54, 0x4c, 0x99, 0x26, 0x0f, 0x50, 0x54, 0xbd,
    0xee, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x66, 0x21, 0x50, 0xb0, 0x51, 0x00,
    0x1b, 0x30, 0x40, 0x70, 0x36, 0x00, 0xa0, 0x5a, 0x00, 0x00, 0x00, 0x1e,
    0x01, 0x1d, 0x00, 0x72, 0x51, 0xd0, 0x1e, 0x20, 0x6e, 0x28, 0x55, 0x00,
    0xa0, 0x5a, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x18,
    0x4b, 0x1a, 0x44, 0x17, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x00, 0x00, 0x00, 0xfc, 0x00, 0x53, 0x41, 0x4d, 0x53, 0x55, 0x4e, 0x47,
    0x0a, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xbc, 0x02, 0x03, 0x1e, 0xf1,
    0x46, 0x84, 0x05, 0x03, 0x10, 0x20, 0x22, 0x23, 0x09, 0x07, 0x07, 0x83,
    0x01, 0x00, 0x00, 0xe2, 0x00, 0x0f, 0x67, 0x03, 0x0c, 0x00, 0x10, 0x00,
    0xb8, 0x2d, 0x01, 0x1d, 0x80, 0x18, 0x71, 0x1c, 0x16, 0x20, 0x58, 0x2c,
    0x25, 0x00, 0xa0, 0x5a, 0x00, 0x00, 0x00, 0x9e, 0x8c, 0x0a, 0xd0, 0x8a,
    0x20, 0xe0, 0x2d, 0x10, 0x10, 0x3e, 0x96, 0x00, 0xa0, 0x5a, 0x00, 0x00,
    0x00, 0x18, 0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
    0x45, 0x00, 0xa0, 0x5a, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x06
};

#ifndef ENABLE_QC_SPECIFIC
#define DRM_EXYNOS_VIDI_ON  1
#define DRM_EXYNOS_VIDI_OFF  0
#define VENC_SKIP_INBUF_VALUE 5

static void
set_edid_info(gchar *edid_info, gboolean plug);
#endif

//#define WFD_PAD_PROBE
#ifdef WFD_PAD_PROBE
gulong ahs_appsrc_cb_probe_id = 0;
FILE *f;

static gboolean
gst_dump_data (GstPad * pad, GstMiniObject * obj, gpointer u_data);
#endif

#ifdef ENABLE_WFD_EXTENDED_FEATURES
void
__gst_extended_feature_open(GstRTSPClient * client)
{
  void *handle = NULL;
  handle = dlopen(GST_EXTENDED_FEATURE_PATH, RTLD_LAZY);
  if (handle == NULL) {
    GST_ERROR("Can't open extended feature");

    client->ext_handle = NULL;
    client->extended_feature_support = FALSE;
    return;
  }

  client->ext_handle = handle;
  client->extended_feature_support = TRUE;
  GST_INFO("Open extended feature");
  return;
}

void *
__gst_extended_func(GstRTSPClient * client, const char *func)
{
  return dlsym(client->ext_handle, func);
}

void
__gst_extended_feature_close(GstRTSPClient * client)
{
  dlclose(client->ext_handle);
  client->ext_handle = NULL;
  client->extended_feature_support = FALSE;
  GST_INFO("Closed Extended feature");
}
#endif

static gboolean __client_emit_teardown_signal(void *data)
{
  GstRTSPClient *client = (GstRTSPClient *)data;
  GstRTSPSession *session;
  GstRTSPSessionMedia *media;

  if (client == NULL) return FALSE;

  if (client->sessionid) {
    session = gst_rtsp_session_pool_find (client->session_pool, client->sessionid);
    GST_INFO_OBJECT (client, "session = %p & sessionid = %s", session, session->sessionid);
  } else {
    GST_ERROR("Failed to get session");
    return FALSE;
  }

  media = session->medias->data;

  if (!media) {
    GST_ERROR("Failed to get media");
    return FALSE;
  }

  gst_rtsp_session_media_set_state (media, GST_STATE_PAUSED);
  gst_rtsp_session_media_set_state (media, GST_STATE_NULL);

  client_emit_signal(client, SIGNAL_TEARDOWN);

  return FALSE;
}

void gst_rtsp_client_wfd_init(GstRTSPClient * client)
{
  client->keep_alive_lock = g_mutex_new();
  client->keep_alive_flag = TRUE;
  client->state_wait = g_cond_new ();
  client->client_state = CLIENT_STATE_UNKNOWN;
  client->videosrc_type = WFD_INI_VSRC_XVIMAGESRC;
  client->session_mode = 0;
  client->infile = NULL;
  client->MTUsize = DEFAULT_WFD_MTU_SIZE;
  client->rtcp_pad_handle_id = 0;
  client->audio_device = NULL;
  client->audio_latency_time = 0;
  client->audio_buffer_time = 0;
  client->audio_do_timestamp = 0;
  client->mpeg_ts_pid = NULL;
  client->consecutive_low_bitrate_count = 0;
  client->prev_noti_time = 0;
  client->prev_fraction_lost = 0;
  client->prev_max_packet_lost = 0;
  client->prev_max_seq_num = 0;
  client->first_rtcp = FALSE;
  client->hdcp_enabled = 0;
  client->display_rotate = 0;
  client->cVideo_reso_supported = WFD_CEA_UNKNOWN;
  client->uibc = (UIBC *)malloc (sizeof (UIBC));
  memset(client->uibc, 0x00, sizeof(UIBC));
  client->timer_count = 0;

 #ifdef ENABLE_WFD_EXTENDED_FEATURES
  client->vconf_web_video_state = 0;
 #endif

  GST_DEBUG_CATEGORY_INIT (rtsp_client_wfd_debug, "rtspclient_wfd", 0, "GstRTSPClient");
}

void gst_rtsp_client_wfd_finalize(GstRTSPClient * client)
{
#ifndef ENABLE_QC_SPECIFIC
  set_edid_info(NULL, FALSE);
#endif
  if(client->rtcp_udpsrc_pad && (client->rtcp_pad_handle_id > 0)) {
    gst_pad_remove_data_probe(client->rtcp_udpsrc_pad, client->rtcp_pad_handle_id);
  }
  if(client->keep_alive_lock)
    g_mutex_free (client->keep_alive_lock);
  client->keep_alive_lock = NULL;

  if(client->uibc) {
    if(client->uibc->thread) {
      if(client->uibc->fd > 0) {
        shutdown (client->uibc->fd, SHUT_RDWR);
        client->uibc->fd = 0;
      }
      if(client->uibc->mainfd > 0) {
        shutdown (client->uibc->mainfd, SHUT_RDWR);
        client->uibc->mainfd = 0;
      }
      g_thread_join (client->uibc->thread);
      client->uibc->thread = NULL;
    }
    if(client->uibc->hidc_cap_list) {
      g_free(client->uibc->hidc_cap_list);
      client->uibc->hidc_cap_list = NULL;
    }
    if(client->uibc->neg_hidc_cap_list) {
      g_free (client->uibc->neg_hidc_cap_list);
      client->uibc->neg_hidc_cap_list = NULL;
    }
    g_free(client->uibc);
    client->uibc = NULL;
  }
}

/**
* gst_rtsp_client_negotiate:
* @client: client object
*
* This will handle capability negotiation part of WFD session
*
* Returns: a #GstRTSPResult. return GST_RTSP_OK on success
*/
GstRTSPResult
#ifdef ENABLE_WFD_EXTENDED_FEATURES
gst_rtsp_client_negotiate (GstRTSPClient * client, gchar **current_version)
#else
gst_rtsp_client_negotiate (GstRTSPClient * client)
#endif
{
  GstRTSPResult res = GST_RTSP_OK;

  /* handle M1 OPTIONS message */
  res = handle_M1_message (client);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to handle M1 message...");
    goto rtsp_method_failed;
  }

  /* handle M2 OPTIONS message */
  res = handle_M2_message (client);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to handle M2 message...");
    goto rtsp_method_failed;
  }
#ifdef ENABLE_WFD_EXTENDED_FEATURES
  if(client->enable_spec_features && client->user_agent) *current_version = g_strdup(client->user_agent);
#endif
  /* handle M3 GET_PARAMETER request message */
  res = handle_M3_message (client);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to handle M3 message...");
    goto rtsp_method_failed;
  }

  /* handle M4 SET_PARAMETER request message */
  res = handle_M4_message (client);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to handle M4 message...");
    goto rtsp_method_failed;
  }

  return GST_RTSP_OK;

rtsp_method_failed:
  GST_ERROR_OBJECT (client, "Failed to handle rtsp method : %d", res);
  return res;
}

/**
* gst_rtsp_client_start:
* @client: client object
*
* This will handle M5 message & asks the client to start session
*
* Returns: a #gboolean. return TRUE on success / FALSE on failure
*/
gboolean
gst_rtsp_client_start (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;

  res = handle_M5_message (client, WFD_TRIGGER_SETUP);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to handle M5 message...");
    return FALSE;
  }

  return TRUE;
}

void gst_rtsp_client_set_uibc_callback (GstRTSPClient *client, wfd_uibc_send_event_cb uibc_event_cb, wfd_uibc_control_cb uibc_control_cb, gpointer user_param)
{
  client->uibc->event_cb = uibc_event_cb;
  client->uibc->control_cb = uibc_control_cb;
  client->uibc->user_param = user_param;
}

void
gst_rtsp_client_set_params (GstRTSPClient *client, int videosrc_type, gint session_mode,
                                  gint mtu_size, gchar *infile,
                                  gchar *audio_device, gint audio_latency_time, gint audio_buffer_time, gint audio_do_timestamp,
                                  guint64 video_reso_supported, gint video_native_resolution, gint hdcp_enabled, guint8 uibc_gen_capability,
#ifdef ENABLE_WFD_EXTENDED_FEATURES
                                  gint display_rotate, guint *decide_udp_bitrate, guint *decide_tcp_bitrate)
#else
                                  gint display_rotate, guint *decide_udp_bitrate)
#endif
{
  int idx = 0;
  client->videosrc_type = videosrc_type;
  client->session_mode = session_mode;
  client->MTUsize = mtu_size;
  client->infile = g_strdup (infile);
  client->audio_device = g_strdup(audio_device);
  client->audio_latency_time = audio_latency_time;
  client->audio_buffer_time = audio_buffer_time;
  client->audio_do_timestamp = audio_do_timestamp;
  client->cVideo_reso_supported = video_reso_supported;
  for (idx=0; idx<21; ++idx) {
    client->decide_udp_bitrate[idx] = decide_udp_bitrate[idx];
#ifdef ENABLE_WFD_EXTENDED_FEATURES
    client->decide_tcp_bitrate[idx] = decide_tcp_bitrate[idx];
#endif
  }
  client->cSrcNative = video_native_resolution;
#ifdef USE_HDCP
  client->hdcp_enabled = hdcp_enabled;
#else
  client->hdcp_enabled = 0;
#endif
  client->display_rotate = display_rotate;
  if(uibc_gen_capability) {
    client->uibc->gen_capability = uibc_gen_capability;
    client->uibc->setting = TRUE;
    client->uibc->port = DEFAULT_UIBC_PORT;
  }

  g_print ("\n\n\nvideo src type = %d & session_mode = %d & filename = %s\n",
    videosrc_type, session_mode, client->infile);
}

#ifndef ENABLE_QC_SPECIFIC
static void
set_edid_info(gchar *edid_info, gboolean plug)
{
  Display *dpy = NULL;
  int drm_fd;
  int screen;
  drm_magic_t magic;
  int eventBase, errorBase;
  int dri2Major, dri2Minor;
  char *driverName, *deviceName;
  struct drm_exynos_vidi_connection vidi;
  GST_INFO("set_edid_info");

  dpy = XOpenDisplay(NULL);
  if (!dpy) {
    GST_ERROR(" Error: fail to open display");
    return;
  }
  screen = DefaultScreen(dpy);

  /*DRI2*/
  if (!DRI2QueryExtension (dpy, &eventBase, &errorBase)) {
    GST_ERROR(" Error : DRI2QueryExtension");
  }

  if (!DRI2QueryVersion (dpy, &dri2Major, &dri2Minor)) {
    GST_ERROR("!!Error : DRI2QueryVersion !!");
    return;
  }

  if (!DRI2Connect (dpy, RootWindow(dpy, screen), &driverName, &deviceName)) {
    GST_ERROR( "!!Error : DRI2Connect !!\n");
    return;
  }

  GST_INFO("Open drm device : %s\n", deviceName);

  /* get the drm_fd though opening the deviceName */
  drm_fd = open (deviceName, O_RDWR);
  if (drm_fd < 0) {
    GST_ERROR("!!Error : cannot open drm device (%s)", deviceName);
    Xfree(driverName);
    Xfree(deviceName);
    return;
  }
  /* get the drm magic */
  drmGetMagic(drm_fd, &magic);
  GST_INFO(">>> drm magic=%d", magic);

  if (!DRI2Authenticate(dpy, RootWindow(dpy, screen), magic)) {
    GST_ERROR("!!Error : DRI2Authenticate !!");
    close(drm_fd);
    Xfree(driverName);
    Xfree(deviceName);
    return;
  }
  if (plug == TRUE){
    vidi.connection = DRM_EXYNOS_VIDI_ON;
    vidi.extensions = DRM_EXYNOS_VIDI_ON;
    vidi.edid = (uint64_t *)fake_edid_info;
    GST_ERROR(">>>>>>>>>>>>>>>>>>>>set\n");
  } else {
    vidi.connection = DRM_EXYNOS_VIDI_OFF;
    vidi.extensions = DRM_EXYNOS_VIDI_OFF;
    vidi.edid = NULL;
    GST_ERROR(">>>>>>>>>>>>>>>>>>>>unset\n");
  }

  ioctl (drm_fd, DRM_IOCTL_EXYNOS_VIDI_CONNECTION, &vidi);

  close(drm_fd);
  Xfree(driverName);
  Xfree(deviceName);

  return;
}
#endif

static GstRTSPResult
prepare_src_response (GstRTSPClient *client, GstRTSPMessage *response, GstRTSPMessage *request, GstRTSPMethod method, WFDMessageType message_type)
{
  if(method == GST_RTSP_GET_PARAMETER) {
    //gst_rtsp_message_init_response (response, GST_RTSP_STS_OK, gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);
    return GST_RTSP_EINVAL;
  } else if(method == GST_RTSP_SET_PARAMETER) {
    switch(message_type) {
    case WFD_MESSAGE_10:
    case WFD_MESSAGE_11:
    case WFD_MESSAGE_12:
    case WFD_MESSAGE_13:
    case WFD_MESSAGE_14:
    case WFD_MESSAGE_15:
      gst_rtsp_message_init_response (response, GST_RTSP_STS_OK, gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);
      return GST_RTSP_OK;
    default:
    {
#ifdef ENABLE_WFD_EXTENDED_FEATURES
      if (client->extended_feature_support) {
        GstRTSPResult (*func)(GstRTSPClient *, GstRTSPMessage *, GstRTSPMessage *, WFDMessageType)
             = __gst_extended_func(client, EXT_MSG_RESP);
        return func(client, response, request, message_type);
      }
#else
      return GST_RTSP_EINVAL;
#endif
    }
    }
  }
  return GST_RTSP_EINVAL;
}

WFDMessageType get_rtsp_source_config_message_type(GstRTSPClient * client, WFDMessage *msg, GstRTSPMethod message_type)
{
  if(message_type == GST_RTSP_GET_PARAMETER) {
    return WFD_MESSAGE_UNKNOWN;
  } else if(message_type == GST_RTSP_SET_PARAMETER) {
    if (msg->route) return WFD_MESSAGE_10;
    else if (msg->connector_type) return WFD_MESSAGE_11;
    else if (msg->standby) return WFD_MESSAGE_12;
    else if (msg->idr_request) return WFD_MESSAGE_13;
    else if (msg->uibc_capability) return WFD_MESSAGE_14;
    else if (msg->uibc_setting) return WFD_MESSAGE_15;
    else {
#ifdef ENABLE_WFD_EXTENDED_FEATURES
      if (client->extended_feature_support) {
        WFDMessageType (*func)(WFDMessage *, GstRTSPMethod)
             = __gst_extended_func(client, EXT_SRC_CONFIG_MSG);
        return func(msg, message_type);
      }
#else
      return WFD_MESSAGE_UNKNOWN;
#endif
    }
  }
  return WFD_MESSAGE_UNKNOWN;
}

gboolean
handle_wfd_get_param_request (GstRTSPClient * client, GstRTSPClientState * state)
{
  GstRTSPResult res;
  guint8 *data;
  guint size;
  WFDMessageType message_type;

  res = gst_rtsp_message_get_body (state->request, &data, &size);
  if (res != GST_RTSP_OK)
    return FALSE;

  WFDMessage *msg;
  wfdconfig_message_new(&msg);
  wfdconfig_message_init(msg);
  wfdconfig_message_parse_buffer(data,size,msg);
  wfdconfig_message_dump(msg);
  //GST_DEBUG("\nM3 response server side message body: %s\n\n\n", wfdconfig_message_as_text(msg));
  message_type = get_rtsp_source_config_message_type (client, msg, GST_RTSP_GET_PARAMETER);

  res = prepare_src_response(client, state->response, state->request, GST_RTSP_GET_PARAMETER, message_type);
  if (res != GST_RTSP_OK)
    return FALSE;

  return TRUE;
}

gboolean
handle_wfd_set_param_request (GstRTSPClient * client, GstRTSPClientState * state)
{
  GstRTSPResult res;
  guint8 *data;
  guint size;
  WFDMessageType message_type;
  WFDResult confret;

  res = gst_rtsp_message_get_body (state->request, &data, &size);
  if (res != GST_RTSP_OK)
    return FALSE;

    WFDMessage *msg;
    wfdconfig_message_new(&msg);
    wfdconfig_message_init(msg);
    wfdconfig_message_parse_buffer(data,size,msg);
    //GST_DEBUG("\nM3 response server side message body: %s\n\n\n", wfdconfig_message_as_text(msg));
    wfdconfig_message_dump(msg);

    message_type = get_rtsp_source_config_message_type (client, msg, GST_RTSP_SET_PARAMETER);
    switch (message_type) {
      case WFD_MESSAGE_10:
      {
        WFDSinkType sinktype;
        confret = wfdconfig_get_audio_sink_type(msg, &sinktype);
        if(confret == WFD_OK)
          GST_DEBUG("M10 server set param request route to  %s  sink", sinktype?"SECONDARY":"PRIMARY");
      }
      break;
      case WFD_MESSAGE_11:
      {
        WFDConnector connector;
        confret = wfdconfig_get_connector_type(msg, &connector);
        if(confret == WFD_OK)
          GST_DEBUG("M11 server set param request connector type to %d", connector);
      }
      break;
      case WFD_MESSAGE_12:
      {
        gboolean standby_enable;
        confret = wfdconfig_get_standby(msg, &standby_enable);
        if(confret == WFD_OK)
          GST_DEBUG("M12 server set param request STANDBY %s", standby_enable?"ENABLE":"DISABLE");
      }
      break;
      case WFD_MESSAGE_13:
      {
        GST_DEBUG("M13 server set param request for IDR frame");
        g_object_set (client->srcbin->venc, "force-i-frame", 1, NULL);
//    gIDRReqcount++;
      }
      break;
      case WFD_MESSAGE_14:
      {
        guint32 input_category;
        guint32 inp_type;
        WFDHIDCTypePathPair *inp_pair = NULL;
        guint32 inp_type_path_count = 0;
        guint32 i=0, j=0;
        guint32 tcp_port;
        gboolean *hidc_cap_list_presence = NULL;
        confret = wfdconfig_get_uibc_capability(msg, &input_category, &inp_type, &inp_pair, &inp_type_path_count, &tcp_port);

        if(confret == WFD_OK) {
          GST_DEBUG("M14 server set param UIBC input category: %s %s", input_category&WFD_UIBC_INPUT_CAT_GENERIC?"GENERIC":"", input_category&WFD_UIBC_INPUT_CAT_HIDC?"HIDC":"");
          GST_DEBUG("UIBC input type: %s %s %s %s %s %s %s %s", inp_type&WFD_UIBC_INPUT_TYPE_KEYBOARD?"KEYBOARD":"", inp_type&WFD_UIBC_INPUT_TYPE_MOUSE?"MOUSE":"",
             inp_type&WFD_UIBC_INPUT_TYPE_SINGLETOUCH?"SINGLETOUCH":"", inp_type&WFD_UIBC_INPUT_TYPE_MULTITOUCH?"MULTITOUCH":"",
             inp_type&WFD_UIBC_INPUT_TYPE_JOYSTICK?"JOYSTICK":"", inp_type&WFD_UIBC_INPUT_TYPE_CAMERA?"CAMERA":"",
             inp_type&WFD_UIBC_INPUT_TYPE_GESTURE?"GESTURE":"", inp_type&WFD_UIBC_INPUT_TYPE_REMOTECONTROL?"REMOTECONTROL":"");
          for(;i<inp_type_path_count;i++) {
            switch (inp_pair[i].inp_type) {
              case WFD_UIBC_INPUT_TYPE_KEYBOARD:
                GST_DEBUG("UIBC HIDC %d type KEYBOARD", i);
                break;
              case WFD_UIBC_INPUT_TYPE_MOUSE:
                GST_DEBUG("UIBC HIDC %d type MOUSE", i);
                break;
              case WFD_UIBC_INPUT_TYPE_SINGLETOUCH:
                GST_DEBUG("UIBC HIDC %d type SINGLETOUCH", i);
                break;
              case WFD_UIBC_INPUT_TYPE_MULTITOUCH:
                GST_DEBUG("UIBC HIDC %d type MULTITOUCH", i);
                break;
              case WFD_UIBC_INPUT_TYPE_JOYSTICK:
                GST_DEBUG("UIBC HIDC %d type JOYSTICK", i);
                break;
              case WFD_UIBC_INPUT_TYPE_CAMERA:
                GST_DEBUG("UIBC HIDC %d type CAMERA", i);
                break;
              case WFD_UIBC_INPUT_TYPE_GESTURE:
                GST_DEBUG("UIBC HIDC %d type GESTURE", i);
                break;
              case WFD_UIBC_INPUT_TYPE_REMOTECONTROL:
                GST_DEBUG("UIBC HIDC %d type REMOTECONTROL", i);
                break;
              default :
                GST_ERROR ("UIBC HIDC %d invalid type", i);
                break;
            }
            switch (inp_pair[i].inp_path) {
              case WFD_UIBC_INPUT_PATH_INFRARED:
                GST_DEBUG("UIBC HIDC %d path INFRARED", i);
                break;
              case WFD_UIBC_INPUT_PATH_USB:
                GST_DEBUG("UIBC HIDC %d path USB", i);
                break;
              case WFD_UIBC_INPUT_PATH_BT:
                GST_DEBUG("UIBC HIDC %d path BT", i);
                break;
              case WFD_UIBC_INPUT_PATH_ZIGBEE:
                GST_DEBUG("UIBC HIDC %d path ZIGBEE", i);
                break;
              case WFD_UIBC_INPUT_PATH_WIFI:
                GST_DEBUG("UIBC HIDC %d path WIFI", i);
                break;
              case WFD_UIBC_INPUT_PATH_NOSP:
                GST_DEBUG("UIBC HIDC %d path NOSP", i);
                break;
              default:
                GST_ERROR ("UIBC HIDC %d invalid path", i);
                break;
            }
          }
          client->uibc->neg_gen_capability = client->uibc->gen_capability & inp_type;
          if(client->uibc->neg_gen_capability) client->uibc->neg_input_cat |= WFD_UIBC_INPUT_CAT_GENERIC;
          if(inp_type_path_count) {
            hidc_cap_list_presence = (gboolean *)malloc (sizeof(gboolean)*inp_type_path_count);
            memset(hidc_cap_list_presence, 0x00,sizeof(gboolean)*inp_type_path_count);
            client->uibc->neg_hidc_cap_count = 0;
            for(i=0;i<inp_type_path_count;i++) {
              for(j=0;j<DEFAULT_UIBC_HIDC_CAP_COUNT;j++) {
                if((inp_pair[i].inp_type == client->uibc->hidc_cap_list[j].inp_type) &&
                    (inp_pair[i].inp_path == client->uibc->hidc_cap_list[j].inp_path)) {
                  hidc_cap_list_presence[i] = TRUE;
                  ++client->uibc->neg_hidc_cap_count;
                  break;
                }
              }
            }
            if(client->uibc->neg_hidc_cap_count) client->uibc->neg_input_cat |= WFD_UIBC_INPUT_CAT_HIDC;
            client->uibc->neg_hidc_cap_list = (WFDHIDCTypePathPair *)malloc (sizeof(WFDHIDCTypePathPair)*client->uibc->neg_hidc_cap_count);
            j = 0;
            for(i=0; i<inp_type_path_count;++i) {
              if(hidc_cap_list_presence[i] == TRUE) {
                client->uibc->neg_hidc_cap_list[j].inp_type = inp_pair[i].inp_type;
                client->uibc->neg_hidc_cap_list[j].inp_path = inp_pair[i].inp_path;
                ++j;
              }
            }
          }
        }
      }
      break;
      case WFD_MESSAGE_15:
      {
        gboolean uibc_enable;
        confret = wfdconfig_get_uibc_status(msg, &uibc_enable);
        if(confret == WFD_OK)
          GST_DEBUG("M15 server set param request for UIBC %s", uibc_enable?"ENABLE":"DISABLE");
        if(uibc_enable == FALSE && client->uibc->thread) {
          if(client->uibc->fd > 0) {
            shutdown (client->uibc->fd, SHUT_RDWR);
            client->uibc->fd = 0;
          }
          if(client->uibc->mainfd > 0) {
            shutdown (client->uibc->mainfd, SHUT_RDWR);
            client->uibc->mainfd = 0;
          }
          g_thread_join (client->uibc->thread);
          client->uibc->thread = NULL;
        }
        if((uibc_enable == TRUE) && (client->uibc->thread == NULL)) {
          uibc_enable_request (client);
        }
      }
      break;
      default:
      {
#ifdef ENABLE_WFD_EXTENDED_FEATURES
        if (client->extended_feature_support) {
          gboolean (*func)(GstRTSPClient *, WFDMessage *, WFDMessageType)
               = __gst_extended_func(client, EXT_HANDLE_SET_PARAM_REQ);

          if (func(client, msg, message_type)) {
            break;
          } else {
            GST_ERROR_OBJECT (client, "Not handling message num - %d", message_type);
            goto bad_request;
          }
        }
#else
        GST_ERROR_OBJECT (client, "Not handling message num - %d", message_type);
        goto bad_request;
#endif
      }
      break;
    }

    res = prepare_src_response(client, state->response, state->request, GST_RTSP_SET_PARAMETER, message_type);
    if (res != GST_RTSP_OK)
      return FALSE;

  return TRUE;

bad_request:
  return FALSE;
}

void
send_request (GstRTSPClient * client, GstRTSPSession * session, GstRTSPMessage * request)
{
  /* remove any previous header */
  gst_rtsp_message_remove_header (request, GST_RTSP_HDR_SESSION, -1);

  /* add the new session header for new session ids */
  if (session) {
    gchar *str;

    GST_DEBUG ("session is not NULL - %p", session);

    if (session->timeout != DEFAULT_RTSP_TIMEOUT)
      str =
          g_strdup_printf ("%s; timeout=%d", session->sessionid,
          session->timeout);
    else
      str = g_strdup (session->sessionid);

    gst_rtsp_message_take_header (request, GST_RTSP_HDR_SESSION, str);
  }

  if (gst_debug_category_get_threshold (rtsp_client_wfd_debug) >= GST_LEVEL_LOG) {
    gst_rtsp_message_dump (request);
  }

  gst_rtsp_watch_send_message (client->watch, request, NULL);
  gst_rtsp_message_unset (request);
}

/**
* handle_M1_message:
* @client: client object
*
* Handles M1 WFD message.
* This API will send OPTIONS request to WFDSink & waits for the relavent response.
* After getting the response, this will check whether all mandatory messages are supported by the WFDSink or NOT.
*
* Returns: a #GstRTSPResult.
*/

static GstRTSPResult
handle_M1_message (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  gboolean bret = FALSE;

  res = prepare_request (client, &request, GST_RTSP_OPTIONS, "*", WFD_MESSAGE_1, WFD_TRIGGER_UNKNOWN);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to prepare M1 request....\n");
    return res;
  }

  GST_DEBUG_OBJECT (client, "Sending OPTIONS request message (M1)...");

  // TODO: need to add session i.e. 2nd variable
  g_printf("\n --Send M1 request-- \n");
  send_request (client, NULL, &request);

  /* Wait for OPTIONS response (M1 response) */
  g_printf("\n-- Read M1 response-- \n");
  res = gst_rtsp_connection_receive (client->connection, &response, NULL);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to receive M1 response....\n");
    return res;
  }

  if (gst_debug_category_get_threshold (rtsp_client_wfd_debug) >= GST_LEVEL_LOG) {
    gst_rtsp_message_dump (&response);
  }

  bret = gst_rtsp_client_parse_methods (client, &response);
  if (FALSE == bret) {
    return GST_RTSP_ERROR;
  }

  return res;

}

/**
* handle_M2_message:
* @client: client object
*
* Handles M2 WFD message.
* This API will waits for OPTIONS request from WFDSink & responds to the same.
*
* Returns: a #GstRTSPResult.
*/
static GstRTSPResult
handle_M2_message (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPMethod method;
  const gchar *uristr;
  GstRTSPVersion version;
  GstRTSPClientState state = { NULL };
#ifdef ENABLE_WFD_EXTENDED_FEATURES
  gchar *user_agent = NULL;
#endif
  /* Wait for OPTIONS request from client (M2 request) */
  g_printf("\n --Read M2 request-- \n");
  res = gst_rtsp_connection_receive (client->connection, &request, NULL);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to receiv M2 request....\n");
    return res;
  }
  /*dump M2 request message*/
  if (gst_debug_category_get_threshold (rtsp_client_wfd_debug) >= GST_LEVEL_LOG) {
    gst_rtsp_message_dump (&request);
  }

  /* Parse the request received */
  res = gst_rtsp_message_parse_request (&request, &method, &uristr, &version);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to parse request....\n");
    return res;
  }

#ifdef ENABLE_WFD_EXTENDED_FEATURES
  res = gst_rtsp_message_get_header (&request, GST_RTSP_HDR_USER_AGENT, &user_agent, 0);
  if (res == GST_RTSP_OK)
  {
    client->user_agent = g_strdup(user_agent);
    if (client->user_agent) GST_LOG_OBJECT(client, "User Agent : %s", client->user_agent);
    if (g_strrstr(user_agent, "SEC-WDH") && !g_strrstr(user_agent, "DAREF")) {
      client->enable_spec_features = TRUE; /* Dongle supports Samsung proprietory features */
    } else if (g_strrstr(user_agent, "SEC-WDH") && g_strrstr(user_agent, "DAREF")) {
      /* Handling the case that user agent is "SEC-WDH/VND-DAREF", which is sent from both refrigerator and sidesync */
      if (client->cVideo_reso_supported == 0xab) {
        client->cVideo_reso_supported = WFD_CEA_720x480P60;
        GST_LOG_OBJECT(client, "Video Resolution for refrigerator : 0x%08llx", client->cVideo_reso_supported);
      } else {
        GST_LOG_OBJECT(client, "Video Resolution for sidesync : 0x%08llx", client->cVideo_reso_supported);
      }
    }
  }
#endif
  state.request = &request;
  state.response = &response;

  if (version != GST_RTSP_VERSION_1_0) {
    /* we can only handle 1.0 requests */
    send_generic_response (client, GST_RTSP_STS_RTSP_VERSION_NOT_SUPPORTED,
        &state);
    return res;
  }

  if (method != GST_RTSP_OPTIONS) {
    GST_ERROR_OBJECT (client, "Received WRONG request, when server is Waiting for OPTIONS request...");
    return GST_RTSP_ERROR;
  }
#if 0
  /* we always try to parse the url first */
  if (gst_rtsp_url_parse (uristr, &uri) != GST_RTSP_OK) {
    send_generic_response (client, GST_RTSP_STS_BAD_REQUEST, &state);
    return GST_RTSP_ERROR;
  }

  /* not using the uri at this moment */
  gst_rtsp_url_free (uri);
#endif

  GST_DEBUG_OBJECT (client, "Received OPTIONS Request (M2 request)...");

  /* prepare the response for WFDsink request */
  res = prepare_response (client, &request, &response, GST_RTSP_OPTIONS);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to prepare M2 request....\n");
    return res;
  }
  g_printf("\n --Send M2 response-- \n");
  send_response (client, NULL, &response);

  GST_DEBUG_OBJECT (client, "Sent OPTIONS Response (M2 response)...");

  return res;

}

/**
* handle_M3_message:
* @client: client object
*
* Handles M3 WFD message.
* This API will send M3 message (GET_PARAMETER) to WFDSink to query supported formats by the WFDSink.
* After getting supported formats info, this API will set those values on WFDConfigMessage obj
*
* Returns: a #GstRTSPResult.
*/
static GstRTSPResult
handle_M3_message (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPUrl *url = NULL;
  gchar *url_str = NULL;

  client->caCodec = WFD_AUDIO_UNKNOWN;
  client->cFreq = WFD_FREQ_UNKNOWN;
  client->cChanels = WFD_CHANNEL_UNKNOWN;
  client->cBitwidth = 0;
  client->caLatency = 0;
  client->cvCodec = WFD_VIDEO_H264;
  client->cNative = WFD_VIDEO_CEA_RESOLUTION;
  client->cNativeResolution = 0;
  client->cCEAResolution = WFD_CEA_UNKNOWN;
  client->cVESAResolution = WFD_VESA_UNKNOWN;
  client->cHHResolution = WFD_HH_UNKNOWN;
  client->cProfile = WFD_H264_UNKNOWN_PROFILE;
  client->cLevel = WFD_H264_LEVEL_UNKNOWN;
  client->cMaxHeight = 0;
  client->cMaxWidth = 0;
  client->cFramerate = 0;
  client->cInterleaved = 0;
  client->cmin_slice_size = 0;
  client->cslice_enc_params = 0;
  client->cframe_rate_control = 0;
  client->cvLatency = 0;
  client->ctrans = WFD_RTSP_TRANS_UNKNOWN;
  client->cprofile = WFD_RTSP_PROFILE_UNKNOWN;
  client->clowertrans = WFD_RTSP_LOWER_TRANS_UNKNOWN;
  client->crtp_port0 = 0;
  client->crtp_port1 = 0;
  client->hdcp_version = WFD_HDCP_NONE;
  client->hdcp_tcpport = 0;
  client->hdcp_support = FALSE;
#ifdef STANDBY_RESUME_CAPABILITY
  client->standby_resume_capability_support = FALSE;
#endif

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

  res = prepare_request (client, &request, GST_RTSP_GET_PARAMETER, url_str, WFD_MESSAGE_3, WFD_TRIGGER_UNKNOWN);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to prepare M3 request....\n");
    return res;
  }

  GST_DEBUG_OBJECT (client, "Sending GET_PARAMETER request message (M3)...");

  // TODO: need to add session i.e. 2nd variable
  g_printf("\n --Send M3 request-- \n");
  send_request (client, NULL, &request);

  /* Wait for the response */
  res = gst_rtsp_connection_receive (client->connection, &response, NULL);
  if (GST_RTSP_OK != res) {
    GST_ERROR ("Failed to received response....\n");
    return FALSE;
  }
  g_printf("\n --Read M3 response-- \n");
  if (gst_debug_category_get_threshold (rtsp_client_wfd_debug) >= GST_LEVEL_LOG) {
    gst_rtsp_message_dump (&response);
  }

  /* parsing the GET_PARAMTER response */
  {
    gchar *data = NULL;
    guint size=0;
    WFDMessage *msg3res;
    WFDResult wfd_res = WFD_OK;

    res = gst_rtsp_message_get_body (&response, (guint8**)&data, &size);
    if (res != GST_RTSP_OK) {
      GST_ERROR_OBJECT (client, "Failed to get body of response...");
      goto error;
    }

    /* create M3 response message */
    wfd_res = wfdconfig_message_new(&msg3res);
    if (wfd_res != WFD_OK) {
      GST_ERROR_OBJECT (client, "Failed to prepare wfd message...");
      goto error;
    }

    wfd_res = wfdconfig_message_init(msg3res);
    if (wfd_res != WFD_OK) {
      GST_ERROR_OBJECT (client, "Failed to init wfd message...");
      goto error;
    }

    wfd_res = wfdconfig_message_parse_buffer((guint8*)data,size,msg3res);
    if (wfd_res != WFD_OK) {
      GST_ERROR_OBJECT (client, "Failed to init wfd message...");
      goto error;
    }

    GST_DEBUG_OBJECT(client, "M3 response server side message body: %s", wfdconfig_message_as_text(msg3res));

    /* Get the audio formats supported by WFDSink */
    if (msg3res->audio_codecs) {
      wfd_res = wfdconfig_get_supported_audio_format(msg3res, &client->caCodec, &client->cFreq, &client->cChanels, &client->cBitwidth, &client->caLatency);
      if (wfd_res != WFD_OK) {
        GST_WARNING_OBJECT (client, "Failed to get wfd support audio formats...");
        goto error;
      }
    }


    /* Get the Video formats supported by WFDSink */
    wfd_res = wfdconfig_get_supported_video_format(msg3res, &client->cvCodec, &client->cNative, &client->cNativeResolution,
                      (guint64*)&client->cCEAResolution, (guint64*)&client->cVESAResolution, (guint64*)&client->cHHResolution,
                      &client->cProfile, &client->cLevel, &client->cvLatency, &client->cMaxHeight,
                      &client->cMaxWidth, &client->cmin_slice_size, &client->cslice_enc_params, &client->cframe_rate_control);
    if (wfd_res != WFD_OK) {
      GST_WARNING_OBJECT (client, "Failed to get wfd supported video formats...");
      goto error;
    }

    if (msg3res->client_rtp_ports) {
      /* Get the RTP ports preferred by WFDSink */
      wfd_res = wfdconfig_get_prefered_RTP_ports(msg3res, &client->ctrans, &client->cprofile, &client->clowertrans, &client->crtp_port0, &client->crtp_port1);
      if (wfd_res != WFD_OK) {
        GST_WARNING_OBJECT (client, "Failed to get wfd prefered RTP ports...");
        goto error;
      }
    }

    if (msg3res->display_edid) {
      guint32 edid_block_count = 0;
      gchar *edid_payload = NULL;
      client->edid_supported = FALSE;
      /* Get the display edid preferred by WFDSink */
      GST_WARNING_OBJECT (client, "Going to wfdconfig_get_display_EDID");
      wfd_res = wfdconfig_get_display_EDID(msg3res, &client->edid_supported, &edid_block_count, &edid_payload);
      if (wfd_res != WFD_OK) {
        GST_WARNING_OBJECT (client, "Failed to get wfd display edid...");
        goto error;
      }
      GST_WARNING_OBJECT (client, " edid supported: %d edid_block_count: %d", client->edid_supported, edid_block_count);
      GST_WARNING_OBJECT (client, "set_edid_info");
      if(client->edid_supported) {
        client->edid_hres = 0;
        client->edid_vres = 0;
        client->edid_hres = (guint32)(((edid_payload[54+4]>>4)<<8) | edid_payload[54+2]);
        client->edid_vres = (guint32)(((edid_payload[54+7]>>4)<<8) | edid_payload[54+5]);
        GST_WARNING_OBJECT (client, " edid supported Hres: %d Wres: %d", client->edid_hres, client->edid_vres);
        if((client->edid_hres<640)||(client->edid_vres<480) || (client->edid_hres>1920) || (client->edid_vres>1080)) {
          client->edid_hres = 0;
          client->edid_vres = 0;
          client->edid_supported = FALSE;
          GST_WARNING_OBJECT (client, " edid invalid resolutions");
        }
#ifndef ENABLE_QC_SPECIFIC
        set_edid_info(edid_payload, TRUE);
#endif
      }
    }

#ifdef USE_HDCP
   if (msg3res->content_protection) {
     /*Get the hdcp version and tcp port by WFDSink*/
     wfd_res = wfdconfig_get_contentprotection_type(msg3res, &client->hdcp_version, &client->hdcp_tcpport);
     GST_DEBUG("hdcp version =%d, tcp port = %d", client->hdcp_version, client->hdcp_tcpport);
     if (client->hdcp_version > 0 && client->hdcp_tcpport > 0)
       client->hdcp_support = TRUE;

      if (wfd_res != WFD_OK) {
        GST_WARNING_OBJECT (client, "Failed to get wfd content protection...");
        goto error;
      }
   }
#endif

   if (msg3res->uibc_capability) {
     guint32 input_category;
     guint32 inp_type;
     WFDHIDCTypePathPair *inp_pair = NULL;
     guint32 inp_type_path_count = 0;
     guint32 i=0, j=0;
     guint32 tcp_port;
     gboolean *hidc_cap_list_presence = NULL;
     wfd_res = wfdconfig_get_uibc_capability(msg3res, &input_category, &inp_type, &inp_pair, &inp_type_path_count, &tcp_port);
     if(wfd_res == WFD_OK) {
       GST_DEBUG("M3 sink supported params UIBC input category: %s %s", input_category&WFD_UIBC_INPUT_CAT_GENERIC?"GENERIC":"", input_category&WFD_UIBC_INPUT_CAT_HIDC?"HIDC":"");
       GST_DEBUG("UIBC input type: %s %s %s %s %s %s %s %s", inp_type&WFD_UIBC_INPUT_TYPE_KEYBOARD?"KEYBOARD":"", inp_type&WFD_UIBC_INPUT_TYPE_MOUSE?"MOUSE":"",
          inp_type&WFD_UIBC_INPUT_TYPE_SINGLETOUCH?"SINGLETOUCH":"", inp_type&WFD_UIBC_INPUT_TYPE_MULTITOUCH?"MULTITOUCH":"",
          inp_type&WFD_UIBC_INPUT_TYPE_JOYSTICK?"JOYSTICK":"", inp_type&WFD_UIBC_INPUT_TYPE_CAMERA?"CAMERA":"",
          inp_type&WFD_UIBC_INPUT_TYPE_GESTURE?"GESTURE":"", inp_type&WFD_UIBC_INPUT_TYPE_REMOTECONTROL?"REMOTECONTROL":"");
       for(;i<inp_type_path_count;i++) {
         switch (inp_pair[i].inp_type) {
           case WFD_UIBC_INPUT_TYPE_KEYBOARD:
             GST_DEBUG("UIBC HIDC %d type KEYBOARD", i);
             break;
           case WFD_UIBC_INPUT_TYPE_MOUSE:
             GST_DEBUG("UIBC HIDC %d type MOUSE", i);
             break;
           case WFD_UIBC_INPUT_TYPE_SINGLETOUCH:
             GST_DEBUG("UIBC HIDC %d type SINGLETOUCH", i);
             break;
           case WFD_UIBC_INPUT_TYPE_MULTITOUCH:
             GST_DEBUG("UIBC HIDC %d type MULTITOUCH", i);
             break;
           case WFD_UIBC_INPUT_TYPE_JOYSTICK:
             GST_DEBUG("UIBC HIDC %d type JOYSTICK", i);
             break;
           case WFD_UIBC_INPUT_TYPE_CAMERA:
             GST_DEBUG("UIBC HIDC %d type CAMERA", i);
             break;
           case WFD_UIBC_INPUT_TYPE_GESTURE:
             GST_DEBUG("UIBC HIDC %d type GESTURE", i);
             break;
           case WFD_UIBC_INPUT_TYPE_REMOTECONTROL:
             GST_DEBUG("UIBC HIDC %d type REMOTECONTROL", i);
             break;
           default :
             GST_ERROR ("UIBC HIDC %d invalid type", i);
             break;
         }
         switch (inp_pair[i].inp_path) {
           case WFD_UIBC_INPUT_PATH_INFRARED:
             GST_DEBUG("UIBC HIDC %d path INFRARED", i);
             break;
           case WFD_UIBC_INPUT_PATH_USB:
             GST_DEBUG("UIBC HIDC %d path USB", i);
             break;
           case WFD_UIBC_INPUT_PATH_BT:
             GST_DEBUG("UIBC HIDC %d path BT", i);
             break;
           case WFD_UIBC_INPUT_PATH_ZIGBEE:
             GST_DEBUG("UIBC HIDC %d path ZIGBEE", i);
             break;
           case WFD_UIBC_INPUT_PATH_WIFI:
             GST_DEBUG("UIBC HIDC %d path WIFI", i);
             break;
           case WFD_UIBC_INPUT_PATH_NOSP:
             GST_DEBUG("UIBC HIDC %d path NOSP", i);
             break;
           default:
             GST_ERROR ("UIBC HIDC %d invalid path", i);
             break;
         }
       }
       client->uibc->neg_gen_capability = client->uibc->gen_capability & inp_type;
       if(client->uibc->neg_gen_capability) client->uibc->neg_input_cat |= WFD_UIBC_INPUT_CAT_GENERIC;
       if(inp_type_path_count) {
         hidc_cap_list_presence = (gboolean *)malloc (sizeof(gboolean)*inp_type_path_count);
         memset(hidc_cap_list_presence, 0x00,sizeof(gboolean)*inp_type_path_count);
         client->uibc->neg_hidc_cap_count = 0;
         for(i=0;i<inp_type_path_count;i++) {
           for(j=0;j<DEFAULT_UIBC_HIDC_CAP_COUNT;j++) {
             if((inp_pair[i].inp_type == client->uibc->hidc_cap_list[j].inp_type) &&
                 (inp_pair[i].inp_path == client->uibc->hidc_cap_list[j].inp_path)) {
               hidc_cap_list_presence[i] = TRUE;
               ++client->uibc->neg_hidc_cap_count;
               break;
             }
           }
         }
         GST_DEBUG ("Count of negotiated hid cap list is %d", client->uibc->neg_hidc_cap_count);
         if(client->uibc->neg_hidc_cap_count) {
           client->uibc->neg_input_cat |= WFD_UIBC_INPUT_CAT_HIDC;
           client->uibc->neg_hidc_cap_list = (WFDHIDCTypePathPair *)malloc (sizeof(WFDHIDCTypePathPair)*client->uibc->neg_hidc_cap_count);
           j = 0;
           for(i=0; i<inp_type_path_count;++i) {
             if(hidc_cap_list_presence[i] == TRUE) {
               client->uibc->neg_hidc_cap_list[j].inp_type = inp_pair[i].inp_type;
               client->uibc->neg_hidc_cap_list[j].inp_path = inp_pair[i].inp_path;
               ++j;
             }
           }
         }
       }
     }
   }
#ifdef STANDBY_RESUME_CAPABILITY
   if (msg3res->standby_resume_capability) {
     /*Get the standby_resume_capability value by WFDSink*/
     wfd_res = wfdconfig_get_standby_resume_capability(msg3res, &client->standby_resume_capability_support );
     if (wfd_res != WFD_OK) {
       GST_WARNING_OBJECT (client, "Failed to get wfd standby resume capability...");
       goto error;
     }
   }
#endif
    wfdconfig_message_dump(msg3res);
  }

  return res;

error:
  return GST_RTSP_ERROR;

}

/**
* handle_M4_message:
* @client: client object
*
* Handles M4 WFD message.
* This API will send M4 message (SET_PARAMETER) to WFDSink to set the format to be used in session
*
* Returns: a #GstRTSPResult.
*/
static GstRTSPResult
handle_M4_message (GstRTSPClient * client)
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

  /* prepare the request for M4 message */
  res = prepare_request (client, &request, GST_RTSP_SET_PARAMETER, url_str, WFD_MESSAGE_4, WFD_TRIGGER_UNKNOWN);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to prepare M4 request....\n");
    return res;
  }

  GST_DEBUG_OBJECT (client, "Sending SET_PARAMETER request message (M4)...");

  // TODO: need to add session i.e. 2nd variable
  g_printf("\n --Send M4 request-- \n");
  send_request (client, NULL, &request);

  /* Wait for the M4 response from WFDSink */
  res = gst_rtsp_connection_receive (client->connection, &response, NULL);
  if (GST_RTSP_OK != res) {
    GST_ERROR ("Failed to received response....\n");
    return res;
  }
  g_printf("\n --Read M4 response-- \n");
  if (gst_debug_category_get_threshold (rtsp_client_wfd_debug) >= GST_LEVEL_LOG) {
    gst_rtsp_message_dump (&response);
  }

#if defined(USE_HDCP) && defined(ENABLE_WFD_EXTENDED_FEATURES)
  if (client->extended_feature_support) {
    GstRTSPResult (*func)(GstRTSPClient *) = __gst_extended_func(client, EXT_PREPARE_PROTECTION);
    res = func(client);
  }
#endif

  return res;

}

static gboolean
__timeout_trigger(gpointer userdata)
{
  GstRTSPClient *client;
  client = (GstRTSPClient *)userdata;
  if (!client) {
    return FALSE;
  }

  if(client->waiting_teardown) {
    GST_ERROR ("Failed to received request for TEARDOWN....Now just get started to be destroyed by itself");
    //client_emit_signal(client, SIGNAL_TEARDOWN);
    g_idle_add(__client_emit_teardown_signal, client);
  } else {
    GST_INFO("Got Teardown");
    return FALSE;
  }

  return FALSE;
}

/**
* handle_M5_message:
* @client: client object
*
* Handles M5 WFD message.
* This API will send M5 SETUP trigger message using SET_PARAMETER to WFDSink to intimate that client request SETUP now.
*
* Returns: a #GstRTSPResult.
*/
static GstRTSPResult
handle_M5_message (GstRTSPClient * client, WFDTrigger trigger_type)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPUrl *url = NULL;
  gchar *url_str = NULL;
  GstRTSPSession *session = NULL;

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

  /* prepare the request for M5 message */
  res = prepare_request (client, &request, GST_RTSP_SET_PARAMETER, url_str, WFD_MESSAGE_5, trigger_type);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to prepare M5 request....\n");
    return res;
  }

  GST_DEBUG_OBJECT (client, "Sending SET_PARAMETER request message (M5)...");

  // TODO: need to add session i.e. 2nd variable
  if (client->sessionid) {
    session = gst_rtsp_session_pool_find (client->session_pool, client->sessionid);
    GST_INFO_OBJECT (client, "session = %p & sessionid = %s", session, session->sessionid);
  }
  g_printf("\n --Send M5 request-- \n");
  send_request (client, session, &request);

  if (trigger_type == WFD_TRIGGER_TEARDOWN) {
    GTimeVal timeout = { 0, 200000 };
    /* Wait for the M5 response from WFDSink */
    res = gst_rtsp_connection_receive (client->connection, &response, &timeout);
    if (GST_RTSP_OK != res) {
      if (GST_RTSP_ETIMEOUT == res) {
        GST_ERROR ("Failed to received response for trigger TEARDOWN....Now just get started to be destroyed by itself : %d\n", res);
        g_idle_add(__client_emit_teardown_signal, client);
        return GST_RTSP_OK;
      } else {
        GST_ERROR ("Failed to received response for trigger TEARDOWN...%d\n", res);
        return res;
      }
    }
  } else {
    /* Wait for the M5 response from WFDSink */
    res = gst_rtsp_connection_receive (client->connection, &response, NULL);
    if (GST_RTSP_OK != res) {
      GST_ERROR ("Failed to received response....\n");
      return FALSE;
    }
  }
  g_printf("\n --Read M5 response-- \n");
  if (gst_debug_category_get_threshold (rtsp_client_wfd_debug) >= GST_LEVEL_LOG) {
    gst_rtsp_message_dump (&response);
  }

  if (trigger_type == WFD_TRIGGER_TEARDOWN) {
    GST_INFO("Received response for trigger TEARDOWN");
    client->waiting_teardown = TRUE;
    client->teardown_timerid = g_timeout_add(500, __timeout_trigger, client);
  }
  return res;
}


/**
* handle_M12_message:
* @client: client object
*
* Handles M12 WFD message.
* This API will send M5 SETUP trigger message using SET_PARAMETER to WFDSink to intimate that client request SETUP now.
*
* Returns: a #GstRTSPResult.
*/
static GstRTSPResult
handle_M12_message (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPUrl *url = NULL;
  gchar *url_str = NULL;
  GstRTSPSession *session = NULL;
  GstRTSPSessionMedia *media;

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

  /* prepare the request for M12 message */
  res = prepare_request (client, &request, GST_RTSP_SET_PARAMETER, url_str, WFD_MESSAGE_12, WFD_TRIGGER_UNKNOWN);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to prepare M12 request....\n");
    return res;
  }

  GST_DEBUG_OBJECT (client, "Sending SET_PARAMETER request message (M12)...");

  // TODO: need to add session i.e. 2nd variable
  if (client->sessionid) {
    session = gst_rtsp_session_pool_find (client->session_pool, client->sessionid);
    GST_INFO_OBJECT (client, "session = %p & sessionid = %s", session, session->sessionid);
  }
  send_request (client, session, &request);

  /* Wait for the M12 response from WFDSink */
  res = gst_rtsp_connection_receive (client->connection, &response, NULL);
  if (GST_RTSP_OK != res) {
    GST_ERROR ("Failed to received response....\n");
    return FALSE;
  }

  if (gst_debug_category_get_threshold (rtsp_client_wfd_debug) >= GST_LEVEL_LOG) {
    gst_rtsp_message_dump (&response);
  }
  if (!(session = gst_rtsp_session_pool_find (client->session_pool, client->sessionID)))
  {
    GST_ERROR_OBJECT (client, "Failed to handle B1 message...");
    return FALSE;
  }
  GST_DEBUG ("sessid = %s", client->sessionID);

  media = session->medias->data;

  /* unlink the all TCP callbacks */
  unlink_session_streams (client, session, media);

  /* then pause sending */
  gst_rtsp_session_media_set_state (media, GST_STATE_PAUSED);

  /* the state is now READY */
  media->state = GST_RTSP_STATE_READY;

  return res;

}

gboolean
gst_rtsp_client_pause (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;

  g_printf("\n -- Send M5 request with tirgger PAUSE-- \n");
  res = handle_M5_message (client, WFD_TRIGGER_PAUSE);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to handle M5 message...");
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_rtsp_client_resume (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  g_printf("\n -- Send M5 request with tirgger PLAY-- \n");
  res = handle_M5_message (client, WFD_TRIGGER_PLAY);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to handle M5 message...");
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_rtsp_client_standby(GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  res = handle_M12_message (client);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to handle M12 message...");
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_rtsp_client_stop (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  g_printf("\n -- Send M5 request with tirgger TEARDOWN-- \n");
  res = handle_M5_message (client, WFD_TRIGGER_TEARDOWN);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to handle M5 message...");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_rtsp_client_parse_methods (GstRTSPClient * client, GstRTSPMessage * response)
{
  GstRTSPHeaderField field;
  gchar *respoptions;
  gchar **options;
  gint indx = 0;
  gint i;
  gboolean found_wfd_method = FALSE;

  /* reset supported methods */
  client->methods = 0;

  /* Try Allow Header first */
  field = GST_RTSP_HDR_ALLOW;
  while (TRUE) {
    respoptions = NULL;
    gst_rtsp_message_get_header (response, field, &respoptions, indx);
    if (indx == 0 && !respoptions) {
      /* if no Allow header was found then try the Public header... */
      field = GST_RTSP_HDR_PUBLIC;
      gst_rtsp_message_get_header (response, field, &respoptions, indx);
    }
    if (!respoptions)
      break;

    /* If we get here, the server gave a list of supported methods, parse
     * them here. The string is like:
     *
     * OPTIONS,  PLAY, SETUP, ...
     */
    options = g_strsplit (respoptions, ",", 0);

    for (i = 0; options[i]; i++) {
      gchar *stripped;
      gint method;

      stripped = g_strstrip (options[i]);
      method = gst_rtsp_find_method (stripped);

      if (!g_ascii_strcasecmp ("org.wfa.wfd1.0", stripped))
        found_wfd_method = TRUE;

      /* keep bitfield of supported methods */
      if (method != GST_RTSP_INVALID)
        client->methods |= method;
    }
    g_strfreev (options);

    indx++;
  }

  if (!found_wfd_method) {
    GST_ERROR_OBJECT (client, "WFD client is not supporting WFD mandatory message : org.wfa.wfd1.0...");
    goto no_required_methods;
  }

  /* Checking mandatory method */
  if (!(client->methods & GST_RTSP_SET_PARAMETER)) {
    GST_ERROR_OBJECT (client, "WFD client is not supporting WFD mandatory message : SET_PARAMETER...");
    goto no_required_methods;
  }

  /* Checking mandatory method */
  if (!(client->methods & GST_RTSP_GET_PARAMETER)) {
    GST_ERROR_OBJECT (client, "WFD client is not supporting WFD mandatory message : GET_PARAMETER...");
    goto no_required_methods;
  }

  if (!(client->methods & GST_RTSP_OPTIONS)) {
    GST_INFO_OBJECT (client, "assuming OPTIONS is supported by client...");
    client->methods |= GST_RTSP_OPTIONS;
  }

  return TRUE;

/* ERRORS */
no_required_methods:
  {
    GST_ELEMENT_ERROR (client, RESOURCE, OPEN_READ, (NULL),
        ("WFD Client does not support mandatory methods."));
  // TODO: temporary hack
    return FALSE;
  }
}

static gboolean
keep_alive_response_check (gpointer userdata)
{
  GstRTSPClient *client = (GstRTSPClient *)userdata;
  if (!client) {
    return FALSE;
  }
  if (client->keep_alive_flag) {
    return FALSE;
  }
  else {
    GST_INFO ("%p: source error notification", client);
    client_emit_signal(client, SIGNAL_ERROR);
    return FALSE;
  }
}

void set_keep_alive_condition(GstRTSPClient * client)
{
  g_timeout_add((DEFAULT_RTSP_TIMEOUT - 5)*1000, keep_alive_condition, client);
}

/*CHecking whether source has got response of any request.
 * If yes, keep alive message is sent otherwise error message
 * will be displayed.*/
static gboolean
keep_alive_condition(gpointer userdata)
{
  GstRTSPClient *client;
  gboolean res;
  client = (GstRTSPClient *)userdata;
  if (!client) {
    return FALSE;
  }
  if(client->keep_alive_lock)
    g_mutex_lock(client->keep_alive_lock);
  else
    return FALSE;
  if(!client->keep_alive_flag) {
    g_timeout_add(5000, keep_alive_response_check, client);
  }
  else {
    GST_DEBUG_OBJECT (client, "have received last keep alive message response");
  }
  GST_DEBUG("sending keep alive message");
  res = gst_rtsp_client_sending_m16_message(client);
  if(res) {
    client->keep_alive_flag = FALSE;
  }
  else {
    GST_ERROR_OBJECT (client, "Failed to send Keep Alive Message");
    g_mutex_unlock(client->keep_alive_lock);
    return FALSE;
  }
  g_mutex_unlock(client->keep_alive_lock);
  return TRUE;
}

/*Sending keep_alive (M16) message.
  Without calling prepare_request function.*/
static gboolean
gst_rtsp_client_sending_m16_message (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  gchar *url_str = NULL;
  GstRTSPSession *session = NULL;

  url_str = g_strdup("rtsp://localhost/wfd1.0");

  res = gst_rtsp_message_init_request (&request, GST_RTSP_GET_PARAMETER, url_str);
  if (res < 0) {
    GST_ERROR ("init request failed");
    return FALSE;
  }

  if (client->sessionid) {
    session = gst_rtsp_session_pool_find (client->session_pool, client->sessionid);
    GST_INFO_OBJECT (client, "session = %p & sessionid = %s", session, session->sessionid);
  }

  send_request (client, session, &request);

  return TRUE;
}

static guint64 wfd_get_prefered_resolution(guint64 srcResolution, guint64 sinkResolution, WFDVideoNativeResolution native, guint32 *cMaxWidth, guint32 *cMaxHeight, guint32 *cFramerate, guint32 *interleaved)
{
  int i=0;
  guint64 resolution=0;
  for(i=0; i<32; i++) {
    if(((sinkResolution<<i)&0x80000000) && ((srcResolution<<i)&0x80000000)) {
      resolution = (0x00000001<<(31-i));
      break;
    }
  }
  switch(native)
  {
    case WFD_VIDEO_CEA_RESOLUTION:
    {
      switch(resolution)
      {
        case WFD_CEA_640x480P60: *cMaxWidth = 640; *cMaxHeight = 480; *cFramerate = 60; *interleaved = 0; break;
        case WFD_CEA_720x480P60: *cMaxWidth = 720; *cMaxHeight = 480; *cFramerate = 60; *interleaved = 0; break;
        case WFD_CEA_720x480I60: *cMaxWidth = 720; *cMaxHeight = 480; *cFramerate = 60; *interleaved = 1; break;
        case WFD_CEA_720x576P50: *cMaxWidth = 720; *cMaxHeight = 576; *cFramerate = 50; *interleaved = 0; break;
        case WFD_CEA_720x576I50: *cMaxWidth = 720; *cMaxHeight = 576; *cFramerate = 50; *interleaved = 1; break;
        case WFD_CEA_1280x720P30: *cMaxWidth = 1280; *cMaxHeight = 720; *cFramerate = 30; *interleaved = 0; break;
        case WFD_CEA_1280x720P60: *cMaxWidth = 1280; *cMaxHeight = 720; *cFramerate = 60; *interleaved = 0; break;
        case WFD_CEA_1920x1080P30: *cMaxWidth = 1920; *cMaxHeight = 1080; *cFramerate = 30; *interleaved = 0; break;
        case WFD_CEA_1920x1080P60: *cMaxWidth = 1920; *cMaxHeight = 1080; *cFramerate = 60; *interleaved = 0; break;
        case WFD_CEA_1920x1080I60: *cMaxWidth = 1920; *cMaxHeight = 1080; *cFramerate = 60; *interleaved = 1; break;
        case WFD_CEA_1280x720P25: *cMaxWidth = 1280; *cMaxHeight = 720; *cFramerate = 25; *interleaved = 0; break;
        case WFD_CEA_1280x720P50: *cMaxWidth = 1280; *cMaxHeight = 720; *cFramerate = 50; *interleaved = 0; break;
        case WFD_CEA_1920x1080P25: *cMaxWidth = 1920; *cMaxHeight = 1080; *cFramerate = 25; *interleaved = 0; break;
        case WFD_CEA_1920x1080P50: *cMaxWidth = 1920; *cMaxHeight = 1080; *cFramerate = 50; *interleaved = 0; break;
        case WFD_CEA_1920x1080I50: *cMaxWidth = 1920; *cMaxHeight = 1080; *cFramerate = 50; *interleaved = 1; break;
        case WFD_CEA_1280x720P24: *cMaxWidth = 1280; *cMaxHeight = 720; *cFramerate = 24; *interleaved = 0; break;
        case WFD_CEA_1920x1080P24: *cMaxWidth = 1920; *cMaxHeight = 1080; *cFramerate = 24; *interleaved = 0; break;
        default: *cMaxWidth = 0; *cMaxHeight = 0; *cFramerate = 0; *interleaved = 0; break;
      }
    }
    break;
    case WFD_VIDEO_VESA_RESOLUTION:
    {
      switch(resolution)
      {
        case WFD_VESA_800x600P30: *cMaxWidth = 800; *cMaxHeight = 600; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_800x600P60: *cMaxWidth = 800; *cMaxHeight = 600; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1024x768P30: *cMaxWidth = 1024; *cMaxHeight = 768; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1024x768P60: *cMaxWidth = 1024; *cMaxHeight = 768; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1152x864P30: *cMaxWidth = 1152; *cMaxHeight = 864; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1152x864P60: *cMaxWidth = 1152; *cMaxHeight = 864; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1280x768P30: *cMaxWidth = 1280; *cMaxHeight = 768; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1280x768P60: *cMaxWidth = 1280; *cMaxHeight = 768; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1280x800P30: *cMaxWidth = 1280; *cMaxHeight = 800; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1280x800P60: *cMaxWidth = 1280; *cMaxHeight = 800; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1360x768P30: *cMaxWidth = 1360; *cMaxHeight = 768; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1360x768P60: *cMaxWidth = 1360; *cMaxHeight = 768; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1366x768P30: *cMaxWidth = 1366; *cMaxHeight = 768; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1366x768P60: *cMaxWidth = 1366; *cMaxHeight = 768; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1280x1024P30: *cMaxWidth = 1280; *cMaxHeight = 1024; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1280x1024P60: *cMaxWidth = 1280; *cMaxHeight = 1024; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1400x1050P30: *cMaxWidth = 1400; *cMaxHeight = 1050; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1400x1050P60: *cMaxWidth = 1400; *cMaxHeight = 1050; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1440x900P30: *cMaxWidth = 1440; *cMaxHeight = 900; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1440x900P60: *cMaxWidth = 1440; *cMaxHeight = 900; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1600x900P30: *cMaxWidth = 1600; *cMaxHeight = 900; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1600x900P60: *cMaxWidth = 1600; *cMaxHeight = 900; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1600x1200P30: *cMaxWidth = 1600; *cMaxHeight = 1200; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1600x1200P60: *cMaxWidth = 1600; *cMaxHeight = 1200; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1680x1024P30: *cMaxWidth = 1680; *cMaxHeight = 1024; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1680x1024P60: *cMaxWidth = 1680; *cMaxHeight = 1024; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1680x1050P30: *cMaxWidth = 1680; *cMaxHeight = 1050; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1680x1050P60: *cMaxWidth = 1680; *cMaxHeight = 1050; *cFramerate = 60; *interleaved = 0; break;
        case WFD_VESA_1920x1200P30: *cMaxWidth = 1920; *cMaxHeight = 1200; *cFramerate = 30; *interleaved = 0; break;
        case WFD_VESA_1920x1200P60: *cMaxWidth = 1920; *cMaxHeight = 1200; *cFramerate = 60; *interleaved = 0; break;
        default: *cMaxWidth = 0; *cMaxHeight = 0; *cFramerate = 0; *interleaved = 0; break;
      }
    }
    break;
    case WFD_VIDEO_HH_RESOLUTION:
    {
       *interleaved = 0;
      switch(resolution)
      {
        case WFD_HH_800x480P30: *cMaxWidth = 800; *cMaxHeight = 480; *cFramerate = 30; break;
        case WFD_HH_800x480P60: *cMaxWidth = 800; *cMaxHeight = 480; *cFramerate = 60; break;
        case WFD_HH_854x480P30: *cMaxWidth = 854; *cMaxHeight = 480; *cFramerate = 30; break;
        case WFD_HH_854x480P60: *cMaxWidth = 854; *cMaxHeight = 480; *cFramerate = 60; break;
        case WFD_HH_864x480P30: *cMaxWidth = 864; *cMaxHeight = 480; *cFramerate = 30; break;
        case WFD_HH_864x480P60: *cMaxWidth = 864; *cMaxHeight = 480; *cFramerate = 60; break;
        case WFD_HH_640x360P30: *cMaxWidth = 640; *cMaxHeight = 360; *cFramerate = 30; break;
        case WFD_HH_640x360P60: *cMaxWidth = 640; *cMaxHeight = 360; *cFramerate = 60; break;
        case WFD_HH_960x540P30: *cMaxWidth = 960; *cMaxHeight = 540; *cFramerate = 30; break;
        case WFD_HH_960x540P60: *cMaxWidth = 960; *cMaxHeight = 540; *cFramerate = 60; break;
        case WFD_HH_848x480P30: *cMaxWidth = 848; *cMaxHeight = 480; *cFramerate = 30; break;
        case WFD_HH_848x480P60: *cMaxWidth = 848; *cMaxHeight = 480; *cFramerate = 60; break;
        default: *cMaxWidth = 0; *cMaxHeight = 0; *cFramerate = 0; *interleaved = 0; break;
      }
    }
    break;
    *cMaxWidth = 0; *cMaxHeight = 0; *cFramerate = 0; *interleaved = 0; break;
  }
  return resolution;
}

/**
* prepare_request:
* @client: client object
* @request : requst message to be prepared
* @method : RTSP method of the request
* @url : url need to be in the request
* @message_type : WFD message type
* @trigger_type : trigger method to be used for M5 mainly
*
* Prepares request based on @method & @message_type
*
* Returns: a #GstRTSPResult.
*/
GstRTSPResult
prepare_request (GstRTSPClient *client, GstRTSPMessage *request,
  GstRTSPMethod method, gchar *url, WFDMessageType message_type, WFDTrigger trigger_type)
{
  GstRTSPResult res = GST_RTSP_OK;
  gchar *str = NULL;
  WFDResult wfd_res = WFD_OK;

  if (method == GST_RTSP_GET_PARAMETER || method == GST_RTSP_SET_PARAMETER) {
    g_free(url);
    url = g_strdup("rtsp://localhost/wfd1.0");
  }

  /* initialize the request */
  res = gst_rtsp_message_init_request (request, method, url);
  if (res < 0) {
    GST_ERROR ("init request failed");
    return res;
  }

  switch (method) {

    /* Prepare OPTIONS request to send */
    case GST_RTSP_OPTIONS: {
      /* add wfd specific require filed "org.wfa.wfd1.0" */
      str = g_strdup ("org.wfa.wfd1.0");
      res = gst_rtsp_message_add_header (request, GST_RTSP_HDR_REQUIRE, str);
      if (res < 0) {
        GST_ERROR ("Failed to add header");
        return res;
      }
      g_free (str);
      str = NULL;
      break;
    }

    /* Prepare GET_PARAMETER request */
    case GST_RTSP_GET_PARAMETER: {
      gchar *msg;
      guint msglen = 0;
      GString *msglength;
      WFDMessage *msg3;

      /* add content type */
      res = gst_rtsp_message_add_header (request, GST_RTSP_HDR_CONTENT_TYPE, "text/parameters");
      if (res < 0) {
        GST_ERROR ("Failed to add header");
        return res;
      }

      /* create M3 message to be sent in the request */
      wfd_res = wfdconfig_message_new(&msg3);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to create wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      wfd_res = wfdconfig_message_init(msg3);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to init wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      /* set the supported audio formats by the WFD server*/
      wfd_res = wfdconfig_set_supported_audio_format(msg3, WFD_AUDIO_UNKNOWN, WFD_FREQ_UNKNOWN,WFD_CHANNEL_UNKNOWN, 0, 0);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set supported audio formats on wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      /* set the supported Video formats by the WFD server*/
      wfd_res = wfdconfig_set_supported_video_format(msg3, WFD_VIDEO_UNKNOWN, WFD_VIDEO_CEA_RESOLUTION, WFD_CEA_UNKNOWN,
                        WFD_CEA_UNKNOWN, WFD_VESA_UNKNOWN,WFD_HH_UNKNOWN, WFD_H264_UNKNOWN_PROFILE,
                        WFD_H264_LEVEL_UNKNOWN,0,0, 0, 0, 0, 0);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set supported video formats on wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }

      GST_DEBUG ("wfdconfig_set_display_EDID...");
      wfd_res = wfdconfig_set_display_EDID(msg3, 0, 0, NULL);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set display EDID type on wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      if(client->hdcp_enabled) {
        GST_DEBUG ("wfdconfig_set_contentprotection_type...");
        wfd_res = wfdconfig_set_contentprotection_type(msg3, WFD_HDCP_NONE, 0);
        if (wfd_res != WFD_OK) {
          GST_ERROR_OBJECT (client, "Failed to set supported content protection type on wfd message...");
          res = GST_RTSP_ERROR;
          goto error;
        }
      }
      if(client->uibc->gen_capability) {
        GST_DEBUG ("wfdconfig_set_uibc_capability...");
        client->uibc->hidc_cap_list = (WFDHIDCTypePathPair *)malloc (sizeof(WFDHIDCTypePathPair) * 4);
        client->uibc->hidc_cap_list[0].inp_type = WFD_UIBC_INPUT_TYPE_KEYBOARD;
        client->uibc->hidc_cap_list[0].inp_path = WFD_UIBC_INPUT_PATH_BT;
        client->uibc->hidc_cap_list[1].inp_type = WFD_UIBC_INPUT_TYPE_KEYBOARD;
        client->uibc->hidc_cap_list[1].inp_path = WFD_UIBC_INPUT_PATH_USB;
        client->uibc->hidc_cap_list[2].inp_type = WFD_UIBC_INPUT_TYPE_MOUSE;
        client->uibc->hidc_cap_list[2].inp_path = WFD_UIBC_INPUT_PATH_BT;
        client->uibc->hidc_cap_list[3].inp_type = WFD_UIBC_INPUT_TYPE_MOUSE;
        client->uibc->hidc_cap_list[3].inp_path = WFD_UIBC_INPUT_PATH_USB;
        wfd_res = wfdconfig_set_uibc_capability(msg3, WFD_UIBC_INPUT_CAT_UNKNOWN, WFD_UIBC_INPUT_TYPE_UNKNOWN, NULL, 0, 0);
        if (wfd_res != WFD_OK) {
          GST_ERROR_OBJECT (client, "Failed to set UIBC on wfd message...");
          res = GST_RTSP_ERROR;
          goto error;
        }
      }
      /*GST_DEBUG ("wfdconfig_set_coupled_sink...");
      wfd_res = wfdconfig_set_coupled_sink(msg3, WFD_SINK_UNKNOWN, NULL);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set coupled sink type on wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      GST_DEBUG ("wfdconfig_set_I2C_port...");
      wfd_res = wfdconfig_set_I2C_port(msg3, FALSE, 0);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set coupled sink type on wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      GST_DEBUG ("wfdconfig_set_connector_type...");
      wfd_res = wfdconfig_set_connector_type(msg3, WFD_CONNECTOR_NO);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set coupled sink type on wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      } */
#ifdef STANDBY_RESUME_CAPABILITY
      GST_DEBUG ("wfdconfig_set_standby_resume_capability...");
      wfd_res = wfdconfig_set_standby_resume_capability(msg3, FALSE);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set coupled sink type on wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
#endif
      /* set the preffered RTP ports for the WFD server*/
      wfd_res = wfdconfig_set_prefered_RTP_ports(msg3, WFD_RTSP_TRANS_UNKNOWN, WFD_RTSP_PROFILE_UNKNOWN,
                        WFD_RTSP_LOWER_TRANS_UNKNOWN, 0, 0);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set supported video formats on wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      GST_DEBUG ("wfdconfig_parameter_names_as_text...");

/*
      wfd_res = wfdconfig_message_dump(msg3);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (client, "Failed to set supported video formats on wfd message...");
        res = GST_RTSP_ERROR;
        goto error;
      }*/

      msg = wfdconfig_parameter_names_as_text(msg3);
      if (msg == NULL) {
        GST_ERROR_OBJECT (client, "Failed to get wfd message as text...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      GST_DEBUG ("wfdconfig_message_as_text...");
      msglen = strlen(msg);
      msglength = g_string_new ("");
      g_string_append_printf (msglength,"%d",msglen);
      GST_DEBUG("M3 server side message body: %s", msg);

      /* add content-length type */
      res = gst_rtsp_message_add_header (request, GST_RTSP_HDR_CONTENT_LENGTH, g_string_free (msglength, FALSE));
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to add header to rtsp message...");
        goto error;
      }

      /* adding wfdconfig data to request */
      res = gst_rtsp_message_set_body (request, (guint8*)msg, msglen);
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to add header to rtsp message...");
        goto error;
      }

      wfdconfig_message_free(msg3);
      break;
    }

    /* Prepare SET_PARAMETER request */
    case GST_RTSP_SET_PARAMETER: {
      /* add content type */
      res = gst_rtsp_message_add_header (request, GST_RTSP_HDR_CONTENT_TYPE, "text/parameters");
      if (res != GST_RTSP_OK) {
        GST_ERROR_OBJECT (client, "Failed to add header to rtsp request...");
        goto error;
      }

      switch (message_type) {
        case WFD_MESSAGE_4: {
          gchar *msg;
          guint msglen = 0;
          GString *msglength;
          WFDMessage *msg4;

          GString *lines;

          /* create M4 message to be sent in the request */
          wfd_res = wfdconfig_message_new(&msg4);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to create wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          wfd_res = wfdconfig_message_init(msg4);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to init wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          lines = g_string_new ("");
          g_string_append_printf (lines,"rtsp://");
          g_string_append(lines, client->server_ip);
          g_string_append_printf (lines,"/wfd1.0/streamid=0");
          wfd_res = wfdconfig_set_presentation_url(msg4, g_string_free (lines, FALSE), NULL);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to set preffered video formats...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          /* set the preffered audio formats for the WFD server*/
          WFDAudioFormats taudiocodec = WFD_AUDIO_UNKNOWN;
          WFDAudioFreq taudiofreq = WFD_FREQ_UNKNOWN;
          WFDAudioChannels taudiochannels = WFD_CHANNEL_UNKNOWN;

          if(client->caCodec & WFD_AUDIO_AC3) taudiocodec = WFD_AUDIO_AAC;  // TODO Currently AC3 encoder is not present
          else if(client->caCodec & WFD_AUDIO_AAC) taudiocodec = WFD_AUDIO_AAC;
          else if(client->caCodec & WFD_AUDIO_LPCM) taudiocodec = WFD_AUDIO_LPCM;
          client->caCodec = taudiocodec;

          if(client->cFreq & WFD_FREQ_48000) taudiofreq = WFD_FREQ_48000;
          else if(client->cFreq & WFD_FREQ_44100) taudiofreq = WFD_FREQ_44100;
          client->cFreq = taudiofreq;

          if(client->cChanels & WFD_CHANNEL_8) taudiochannels = WFD_CHANNEL_2; // TODO Currently only 2 channels is present
          else if(client->cChanels & WFD_CHANNEL_6) taudiochannels = WFD_CHANNEL_2;
          else if(client->cChanels & WFD_CHANNEL_4) taudiochannels = WFD_CHANNEL_2;
          else if(client->cChanels & WFD_CHANNEL_2) taudiochannels = WFD_CHANNEL_2;
          client->cChanels = taudiochannels;

          wfd_res = wfdconfig_set_prefered_audio_format(msg4, taudiocodec, taudiofreq, taudiochannels, client->cBitwidth, client->caLatency);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to set preffered audio formats...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          WFDVideoCEAResolution tcCEAResolution = WFD_CEA_UNKNOWN;
          WFDVideoVESAResolution tcVESAResolution = WFD_VESA_UNKNOWN;
          WFDVideoHHResolution tcHHResolution = WFD_HH_UNKNOWN;
          WFDVideoH264Profile tcProfile;
          WFDVideoH264Level tcLevel;
          guint64 edid_supported_res = 0;
          /* set the preffered video formats for the WFD server*/
          client->cvCodec = WFD_VIDEO_H264;
          client->cProfile = tcProfile = WFD_H264_BASE_PROFILE; // TODO need to fetch from INI file
          client->cLevel = tcLevel = WFD_H264_LEVEL_3_1; // TODO need to fetch from INI file
          edid_supported_res = client->cVideo_reso_supported;
          if(client->edid_supported) {
            if(client->edid_hres < 1920) edid_supported_res = edid_supported_res & 0x8C7F;
            if(client->edid_hres < 1280) edid_supported_res = edid_supported_res & 0x1F;
            if(client->edid_hres < 720) edid_supported_res = edid_supported_res & 0x01;
          }

          if (client->cSrcNative == WFD_VIDEO_CEA_RESOLUTION) {
            tcCEAResolution = wfd_get_prefered_resolution(edid_supported_res, client->cCEAResolution, client->cSrcNative, &client->cMaxWidth, &client->cMaxHeight, &client->cFramerate, &client->cInterleaved);
            GST_DEBUG("wfd negotiated resolution: %08x, width: %d, height: %d, framerate: %d, interleaved: %d", tcCEAResolution, client->cMaxWidth, client->cMaxHeight, client->cFramerate, client->cInterleaved);
          } else if (client->cSrcNative == WFD_VIDEO_VESA_RESOLUTION) {
            tcVESAResolution = wfd_get_prefered_resolution(edid_supported_res, client->cVESAResolution, client->cSrcNative, &client->cMaxWidth, &client->cMaxHeight, &client->cFramerate, &client->cInterleaved);
            GST_DEBUG("wfd negotiated resolution: %08x, width: %d, height: %d, framerate: %d, interleaved: %d", tcVESAResolution, client->cMaxWidth, client->cMaxHeight, client->cFramerate, client->cInterleaved);
          } else if (client->cSrcNative == WFD_VIDEO_HH_RESOLUTION) {
            tcHHResolution = wfd_get_prefered_resolution(edid_supported_res, client->cHHResolution, client->cSrcNative, &client->cMaxWidth, &client->cMaxHeight, &client->cFramerate, &client->cInterleaved);
            GST_DEBUG("wfd negotiated resolution: %08x, width: %d, height: %d, framerate: %d, interleaved: %d", tcHHResolution, client->cMaxWidth, client->cMaxHeight, client->cFramerate, client->cInterleaved);
          }
          client->init_udp_bitrate = client->decide_udp_bitrate[0];
          client->min_udp_bitrate = client->decide_udp_bitrate[1];
          client->max_udp_bitrate = client->decide_udp_bitrate[2];
          if ((client->cMaxWidth * client->cMaxHeight) >= (1920 * 1080)) {
            client->init_udp_bitrate = client->decide_udp_bitrate[3];
            client->min_udp_bitrate = client->decide_udp_bitrate[4];
            client->max_udp_bitrate = client->decide_udp_bitrate[5];
          } else if ((client->cMaxWidth * client->cMaxHeight) >= (1280 * 720)) {
            client->init_udp_bitrate = client->decide_udp_bitrate[6];
            client->min_udp_bitrate = client->decide_udp_bitrate[7];
            client->max_udp_bitrate = client->decide_udp_bitrate[8];
          } else if ((client->cMaxWidth * client->cMaxHeight) >= (960 * 540)) {
            client->init_udp_bitrate = client->decide_udp_bitrate[9];
            client->min_udp_bitrate = client->decide_udp_bitrate[10];
            client->max_udp_bitrate = client->decide_udp_bitrate[11];
          } else if ((client->cMaxWidth * client->cMaxHeight) >= (854 * 480)) {
            client->init_udp_bitrate = client->decide_udp_bitrate[12];
            client->min_udp_bitrate = client->decide_udp_bitrate[13];
            client->max_udp_bitrate = client->decide_udp_bitrate[14];
          } else if((client->cMaxWidth * client->cMaxHeight) >= (640 * 360)) {
            client->init_udp_bitrate = client->decide_udp_bitrate[15];
            client->min_udp_bitrate = client->decide_udp_bitrate[16];
            client->max_udp_bitrate = client->decide_udp_bitrate[17];
          }
#ifdef ENABLE_WFD_EXTENDED_FEATURES
          if (client->extended_feature_support) {
            void (*func)(GstRTSPClient *) = __gst_extended_func(client, EXT_SET_B_MODE_PARAMS);
            func(client);
          }
#endif
          GST_DEBUG ("UDP Bitrate range is %u %u and init bitrate is %u", client->min_udp_bitrate,
              client->max_udp_bitrate, client->init_udp_bitrate);
          wfd_res = wfdconfig_set_prefered_video_format(msg4, client->cvCodec, client->cSrcNative, WFD_CEA_UNKNOWN,
              tcCEAResolution, tcVESAResolution, tcHHResolution, tcProfile, tcLevel, client->cvLatency,
              client->cMaxWidth, client->cMaxHeight, client->cmin_slice_size, client->cslice_enc_params, client->cframe_rate_control);

          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to set preffered video formats...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          GST_DEBUG("wfd config set presentation URL %s",url);

          /* set the preffered RTP ports for the WFD server*/
          GST_LOG("Port are %d, %d\n", client->crtp_port0, client->crtp_port1);
          wfd_res = wfdconfig_set_prefered_RTP_ports(msg4, WFD_RTSP_TRANS_RTP, WFD_RTSP_PROFILE_AVP,
              WFD_RTSP_LOWER_TRANS_UDP, client->crtp_port0, client->crtp_port1);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to set preffered RTP ports...");
            res = GST_RTSP_ERROR;
            goto error;
          }
#ifdef USE_HDCP
          /*set the preffered hdcp version and port for the WFD server*/
          if (client->hdcp_support) {
            GST_DEBUG("hdcp version =%d, tcp port = %d", client->hdcp_version, client->hdcp_tcpport);
            wfd_res = wfdconfig_set_contentprotection_type(msg4, client->hdcp_version, client->hdcp_tcpport);
            if (wfd_res != WFD_OK) {
              GST_ERROR_OBJECT (client, "Failed to set supported content protection type on wfd message...");
              res = GST_RTSP_ERROR;
              goto error;
            }
	   }
#endif
          if(client->uibc->gen_capability) {
            GST_DEBUG ("uibc input cat %d, generic Capability %d, uibc port %u", client->uibc->neg_input_cat,
                  client->uibc->neg_gen_capability, client->uibc->port);
            wfd_res = wfdconfig_set_uibc_capability(msg4, client->uibc->neg_input_cat, client->uibc->neg_gen_capability, client->uibc->neg_hidc_cap_list,
                        client->uibc->neg_hidc_cap_count, client->uibc->port);
          }
          if(wfd_res  != WFD_OK) {
            GST_ERROR ("failed to set_uibc_capability.");
            res = GST_RTSP_ERROR;
            goto error;
          }
#ifdef STANDBY_RESUME_CAPABILITY
          if(client->standby_resume_capability_support){
	        wfd_res = wfdconfig_set_standby_resume_capability(msg4, TRUE);
	        if (wfd_res != WFD_OK) {
              GST_ERROR_OBJECT (client, "Failed to set supported standby resume capability  type on wfd message...");
              res = GST_RTSP_ERROR;
              goto error;
            }
          }
#endif
          wfd_res = wfdconfig_message_dump(msg4);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to dump wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          msg = wfdconfig_message_as_text(msg4);
          if (msg == NULL) {
            GST_ERROR_OBJECT (client, "Failed to get wfd message as text...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          msglen = strlen(msg);
          msglength = g_string_new ("");
          g_string_append_printf (msglength,"%d",msglen);
          GST_DEBUG("M4 message body server side : %s", msg);

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

          wfdconfig_message_free(msg4);
          break;
        }
        case WFD_MESSAGE_5: {
          WFDMessage *msg5;
          gchar *msg;
          guint msglen = 0;
          GString *msglength;

          /* create M4 message to be sent in the request */
          wfd_res = wfdconfig_message_new(&msg5);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to create wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          wfd_res = wfdconfig_message_init(msg5);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to init wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          /* preparing SETUP trigger message. After client receving this request, client has to send SETUP request */
          wfd_res = wfdconfig_set_trigger_type(msg5, trigger_type);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to trigger type...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          wfd_res = wfdconfig_message_dump(msg5);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to dump wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          msg = wfdconfig_message_as_text(msg5);
          if (msg == NULL) {
            GST_ERROR_OBJECT (client, "Failed to get wfd message as text...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          msglen = strlen(msg);
          msglength = g_string_new ("");
          g_string_append_printf (msglength,"%d",msglen);

          GST_DEBUG ("M5 trigger message body: %s", msg);

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

          wfdconfig_message_free(msg5);
          break;
        }
        case WFD_MESSAGE_12: {
          WFDMessage *msg12;
          gchar *msg;
          guint msglen = 0;
          GString *msglength;

          /* create M4 message to be sent in the request */
          wfd_res = wfdconfig_message_new(&msg12);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to create wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          wfd_res = wfdconfig_message_init(msg12);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to init wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          /* preparing SETUP trigger message. After client receving this request, client has to send SETUP request */
          wfd_res = wfdconfig_set_standby(msg12, TRUE);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to trigger type...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          wfd_res = wfdconfig_message_dump(msg12);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to dump wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          msg = wfdconfig_message_as_text(msg12);
          if (msg == NULL) {
            GST_ERROR_OBJECT (client, "Failed to get wfd message as text...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          msglen = strlen(msg);
          msglength = g_string_new ("");
          g_string_append_printf (msglength,"%d",msglen);

          GST_DEBUG ("M12 standby message body: %s", msg);

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

          wfdconfig_message_free(msg12);
          break;
        }
        case WFD_MESSAGE_14: {
          WFDMessage *msg14;
          gchar *msg;
          guint msglen = 0;
          GString *msglength;

          /* create M4 message to be sent in the request */
          wfd_res = wfdconfig_message_new(&msg14);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to create wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          wfd_res = wfdconfig_message_init(msg14);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to init wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          /* preparing SETUP trigger message. After client receving this request, client has to send SETUP request */
          wfd_res = wfdconfig_set_uibc_capability(msg14, client->uibc->neg_input_cat, client->uibc->gen_capability, client->uibc->neg_hidc_cap_list,
                    client->uibc->neg_hidc_cap_count, client->uibc->port);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to set_uibc_capability...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          wfd_res = wfdconfig_message_dump(msg14);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to dump wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          msg = wfdconfig_message_as_text(msg14);
          if (msg == NULL) {
            GST_ERROR_OBJECT (client, "Failed to get wfd message as text...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          msglen = strlen(msg);
          msglength = g_string_new ("");
          g_string_append_printf (msglength,"%d",msglen);

          GST_DEBUG ("M14 UIBC message body: %s", msg);

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

          wfdconfig_message_free(msg14);
          break;
        }
        case WFD_MESSAGE_15: {
          WFDMessage *msg15;
          gchar *msg;
          guint msglen = 0;
          GString *msglength;

          /* create M4 message to be sent in the request */
          wfd_res = wfdconfig_message_new(&msg15);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to create wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          wfd_res = wfdconfig_message_init(msg15);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to init wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          /* preparing SETUP trigger message. After client receving this request, client has to send SETUP request */
          wfd_res = wfdconfig_set_uibc_status(msg15, client->uibc->setting);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to uibc_setting...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          wfd_res = wfdconfig_message_dump(msg15);
          if (wfd_res != WFD_OK) {
            GST_ERROR_OBJECT (client, "Failed to dump wfd message...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          msg = wfdconfig_message_as_text(msg15);
          if (msg == NULL) {
            GST_ERROR_OBJECT (client, "Failed to get wfd message as text...");
            res = GST_RTSP_ERROR;
            goto error;
          }

          msglen = strlen(msg);
          msglength = g_string_new ("");
          g_string_append_printf (msglength,"%d",msglen);

          GST_DEBUG ("M15 UIBC enable message body: %s", msg);

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

          wfdconfig_message_free(msg15);
          break;
        }
        default:
#ifdef ENABLE_WFD_EXTENDED_FEATURES
          if (client->extended_feature_support) {
            GstRTSPResult (*func)(GstRTSPClient *, GstRTSPMessage *, WFDMessageType, WFDTrigger)
                     = __gst_extended_func(client, EXT_PREPARE_SET_PARAM_REQ);
            return func(client, request, message_type, trigger_type);
          }
#else
          GST_ERROR_OBJECT (client, "Unhandled WFD message type...");
          return GST_RTSP_EINVAL;
#endif
          break;
      }
    }
    break;

    default:
      GST_ERROR_OBJECT (client, "Unhandled method...");
      return GST_RTSP_EINVAL;
      break;
  }

  return res;

error:
  return res;
}

/**
* prepare_response:
* @client: client object
* @request : requst message received
* @response : response to be prepare based on request
* @method : RTSP method
*
* prepare response to the request based on @method & @message_type
*
* Returns: a #GstRTSPResult.
*/
GstRTSPResult
prepare_response (GstRTSPClient *client, GstRTSPMessage *request, GstRTSPMessage *response, GstRTSPMethod method)
{
  GstRTSPResult res = GST_RTSP_OK;

  switch (method) {
    /* prepare OPTIONS response */
    case GST_RTSP_OPTIONS: {
      GstRTSPMethod options;
      gchar *tmp = NULL;
      gchar *str = NULL;
      gchar *user_agent = NULL;

      options = GST_RTSP_OPTIONS |
          GST_RTSP_PAUSE |
          GST_RTSP_PLAY |
          GST_RTSP_SETUP |
          GST_RTSP_GET_PARAMETER | GST_RTSP_SET_PARAMETER | GST_RTSP_TEARDOWN;

      str = gst_rtsp_options_as_text (options);

      /*append WFD specific method */
      tmp = g_strdup (", org.wfa.wfd1.0");
      g_strlcat (str, tmp, strlen (tmp) + strlen (str) + 1);

      gst_rtsp_message_init_response (response, GST_RTSP_STS_OK,
          gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);

      gst_rtsp_message_add_header (response, GST_RTSP_HDR_PUBLIC, str);
      g_free (str);
      str = NULL;
      res = gst_rtsp_message_get_header (request, GST_RTSP_HDR_USER_AGENT, &user_agent, 0);
      if (res == GST_RTSP_OK)
      {
        gst_rtsp_message_add_header (response, GST_RTSP_HDR_USER_AGENT, user_agent);
      }
      else res = GST_RTSP_OK;
      break;
    }
    default:
      GST_ERROR_OBJECT (client, "Unhandled method...");
      return GST_RTSP_EINVAL;
      break;
  }

  return res;
}

static void
pad_added (GstElement* ele, GstPad* pad, gpointer data)
{
  GstRTSPClient *client = (GstRTSPClient *)data;
  gchar* name = gst_pad_get_name (pad);

  if (name[0] == 'v') {
    GstPad* spad = NULL;

    g_print (" =========== >>>>>>>>>> Received VIDEO pad...\n");

    spad = gst_element_get_pad(GST_ELEMENT(client->srcbin->vparse), "sink");
    if (gst_pad_link(pad, spad)  != GST_PAD_LINK_OK) {
      GST_ERROR ("Failed to link video demuxer pad & video parser sink pad...\n");
      return;
    }
    gst_object_unref(spad);

  } else if (name[0] == 'a') {
    GstPad* spad = NULL;

#if 0
    GstElement *aparse = NULL;
    aparse = gst_element_factory_make ("aacparse", "aparser");
    if (!aparse) {
      GST_ERROR("failed to create audio parse element");
      return;
    }
    gst_bin_add (srcbin->srcbin, aparse);

    if (!gst_element_link (aparse, srcbin->aqueue)) {
      GST_ERROR ("failed to link aparse & aqueue...\n");
      return;
    }
#endif

    g_print (" =========== >>>>>>>>>> Received AUDIO pad...\n");
    spad = gst_element_get_pad(GST_ELEMENT(client->srcbin->aqueue), "sink");
    if (gst_pad_link(pad, spad)  != GST_PAD_LINK_OK) {
      GST_ERROR ("Failed to link audio demuxer pad & audio parse pad...\n");
      return;
    }
    gst_object_unref(spad);
  } else {
    GST_ERROR ("not handling.....\n\n\n");
  }

  g_free (name);
}

static gboolean
gst_rtsp_client_create_audio_capture_bin (GstRTSPClient * client, GstRTSPClientSrcBin *srcbin)
{
  GstElement *audiosrc = NULL;
  GstElement *acaps = NULL;
  GstElement *aenc = NULL;
#ifdef ENABLE_QC_SPECIFIC
  GstElement *audio_convert = NULL;
#endif
  int channels = 0;
  int is_enc_req = 1;
  int freq = 0;
  gchar *acodec = NULL;
#ifdef ENABLE_QC_SPECIFIC
  //usleep(100000);
  /* create audio src element */
  audiosrc = gst_element_factory_make ("alsasrc", "audiosrc");
#else
  /* create audio src element */
  audiosrc = gst_element_factory_make ("pulsesrc", "audiosrc");
#endif
  if (!audiosrc) {
    GST_ERROR_OBJECT (client, "failed to create audiosrc element");
    goto create_error;
  }
  GST_INFO_OBJECT (client, "audio device : %s", client->audio_device);
  GST_INFO_OBJECT (client, "audio latency time  : %d", client->audio_latency_time);
  GST_INFO_OBJECT (client, "audio_buffer_time  : %d", client->audio_buffer_time);
  GST_INFO_OBJECT (client, "audio_do_timestamp  : %d", client->audio_do_timestamp);

  g_object_set (audiosrc, "device", client->audio_device, NULL);
#ifndef ENABLE_QC_SPECIFIC
  g_object_set (audiosrc, "latency-time", (gint64)client->audio_latency_time, NULL);
#endif
  g_object_set (audiosrc, "buffer-time", (gint64)client->audio_buffer_time, NULL);
  g_object_set (audiosrc, "do-timestamp",(gboolean) client->audio_do_timestamp, NULL);
  g_object_set (audiosrc, "provide-clock",(gboolean) FALSE, NULL);
  g_object_set (audiosrc, "is-live",(gboolean) TRUE, NULL);

  if(client->caCodec == WFD_AUDIO_LPCM) {
    /* To meet miracast certification */
    gint64 block_size = 1920;
    g_object_set (audiosrc, "blocksize",(gint64)block_size, NULL);

#ifdef ENABLE_QC_SPECIFIC
    audio_convert = gst_element_factory_make("audioconvert", "audio_convert");
    if (NULL == audio_convert) {
        GST_ERROR_OBJECT (client, "failed to create audio convert element");
        goto create_error;
    }
#endif
  }

  if(client->audio_device) g_free(client->audio_device);

  /* create audio caps element */
  acaps  = gst_element_factory_make ("capsfilter", "audiocaps");
  if (NULL == acaps) {
    GST_ERROR_OBJECT (client, "failed to create audio capsilfter element");
    goto create_error;
  }

  if(client->cChanels == WFD_CHANNEL_2)
    channels = 2;
  else if(client->cChanels == WFD_CHANNEL_4)
    channels = 4;
  else if(client->cChanels == WFD_CHANNEL_6)
    channels = 6;
  else if(client->cChanels == WFD_CHANNEL_8)
    channels = 8;
  else
    channels = 2;

  if(client->cFreq == WFD_FREQ_44100)
    freq = 44100;
  else if(client->cFreq == WFD_FREQ_48000)
    freq = 48000;
  else
    freq = 44100;

  if(client->caCodec == WFD_AUDIO_LPCM) {
    g_object_set (G_OBJECT (acaps), "caps",
        gst_caps_new_simple ("audio/x-lpcm",
            "width", G_TYPE_INT, 16,
            /* In case of LPCM, uses big endian */
            "endianness", G_TYPE_INT, 4321,
            "signed", G_TYPE_BOOLEAN, TRUE,
            "depth", G_TYPE_INT, 16,
            "rate", G_TYPE_INT, freq,
            "channels", G_TYPE_INT, channels, NULL), NULL);
  } else if((client->caCodec == WFD_AUDIO_AAC) || (client->caCodec == WFD_AUDIO_AC3)) {
    g_object_set (G_OBJECT (acaps), "caps",
        gst_caps_new_simple ("audio/x-raw-int",
            "width", G_TYPE_INT, client->cBitwidth,
            "endianness", G_TYPE_INT, 1234,
            "signed", G_TYPE_BOOLEAN, TRUE,
            "depth", G_TYPE_INT, 16,
            "rate", G_TYPE_INT, freq,
            "channels", G_TYPE_INT, channels, NULL), NULL);
  }

  if(client->caCodec == WFD_AUDIO_AAC) {
    acodec = g_strdup("savsenc_aac");
    is_enc_req = 1;
  } else if(client->caCodec == WFD_AUDIO_AC3) {
    acodec = g_strdup("savsenc_ac3");
    is_enc_req = 1;
  } else if(client->caCodec == WFD_AUDIO_LPCM){
    GST_DEBUG_OBJECT (client, "No codec required, raw data will be sent");
    is_enc_req = 0;
  } else {
    GST_ERROR_OBJECT (client, "Yet to support other than H264 format");
    goto create_error;
  }
  if(is_enc_req) {
    aenc = gst_element_factory_make (acodec, "audioenc");
    if (NULL == aenc) {
      GST_ERROR_OBJECT (client, "failed to create audio encoder element");
      goto create_error;
    }
    g_object_set (aenc, "bitrate",(guint) 128000, NULL);
    g_object_set (aenc, "min-frame-size",(guint) 170, NULL);
    srcbin->aqueue  = gst_element_factory_make ("queue", "audio-queue");
    if (!srcbin->aqueue) {
      GST_ERROR_OBJECT (client, "failed to create audio queue element");
      goto create_error;
    }

    gst_bin_add_many (srcbin->srcbin, audiosrc, acaps, aenc, srcbin->aqueue, NULL);

    if (!gst_element_link_many (audiosrc, acaps, aenc, srcbin->aqueue, NULL)) {
      GST_ERROR_OBJECT (client, "Failed to link audio src elements...");
      goto create_error;
    }
  } else {
    srcbin->aqueue  = gst_element_factory_make ("queue", "audio-queue");
    if (!srcbin->aqueue) {
      GST_ERROR_OBJECT (client, "failed to create audio queue element");
      goto create_error;
    }
#ifdef ENABLE_QC_SPECIFIC
    gst_bin_add_many (srcbin->srcbin, audiosrc, audio_convert, acaps, srcbin->aqueue, NULL);

    if (!gst_element_link_many (audiosrc, audio_convert, acaps, srcbin->aqueue, NULL)) {
      GST_ERROR_OBJECT (client, "Failed to link audio src elements...");
      goto create_error;
    }
#else
    gst_bin_add_many (srcbin->srcbin, audiosrc, acaps, srcbin->aqueue, NULL);

    if (!gst_element_link_many (audiosrc, acaps, srcbin->aqueue, NULL)) {
      GST_ERROR_OBJECT (client, "Failed to link audio src elements...");
      goto create_error;
    }
#endif
  }

  return TRUE;

create_error:
  return FALSE;
}

static gboolean
gst_rtsp_client_create_camera_capture_bin (GstRTSPClient * client, GstRTSPClientSrcBin *srcbin)
{
  GstElement *videosrc = NULL;
  GstElement *vcaps = NULL;
  gchar *vcodec = NULL;

  videosrc = gst_element_factory_make ("camerasrc", "videosrc");
  if (NULL == videosrc) {
    GST_ERROR_OBJECT (client, "failed to create camerasrc element");
    goto create_error;
  }
  g_object_set (videosrc, "camera-id", 1, NULL);

  /* create video caps element */
  vcaps = gst_element_factory_make ("capsfilter", "videocaps");
  if (NULL == vcaps) {
    GST_ERROR_OBJECT (client, "failed to create video capsilfter element");
    goto create_error;
  }

  GST_INFO_OBJECT (client, "picked xvimagesrc as video source");
  g_object_set (G_OBJECT (vcaps), "caps",
      gst_caps_new_simple ("video/x-raw-yuv",
          "width", G_TYPE_INT, client->cMaxWidth,
          "height", G_TYPE_INT, client->cMaxHeight,
          "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('S','N','1','2'),
          "framerate", GST_TYPE_FRACTION, 30, 1, NULL), NULL);

  if(client->cvCodec == WFD_VIDEO_H264)
    vcodec = g_strdup("omx_h264enc");
  else {
    GST_ERROR_OBJECT (client, "Yet to support other than H264 format");
    goto create_error;
  }

  srcbin->venc  = gst_element_factory_make (vcodec, "videoenc");
  if (!srcbin->venc) {
    GST_ERROR_OBJECT (client, "failed to create video encoder element");
    goto create_error;
  }
  g_object_set (srcbin->venc, "bitrate", client->init_udp_bitrate, NULL);
  g_object_set (srcbin->venc, "byte-stream", 1, NULL);
  g_object_set (srcbin->venc, "append-dci", 1, NULL);


  srcbin->vqueue  = gst_element_factory_make ("queue", "video-queue");
  if (!srcbin->vqueue) {
    GST_ERROR_OBJECT (client, "failed to create video queue element");
    goto create_error;
  }

  // TODO: check whether queue is required to have default values

  gst_bin_add_many (srcbin->srcbin, videosrc, vcaps, srcbin->venc, srcbin->vqueue, NULL);
  if (!gst_element_link_many (videosrc, vcaps, srcbin->venc, srcbin->vqueue,NULL)) {
    GST_ERROR_OBJECT (client, "Failed to link video src elements...");
    goto create_error;
  }

  return TRUE;

create_error:
  return FALSE;
}

static gboolean
gst_rtsp_client_create_xvcapture_bin (GstRTSPClient * client, GstRTSPClientSrcBin *srcbin)
{
  GstElement *videosrc = NULL;
  GstElement *vcaps = NULL;
#ifdef SOFTWARE_ENC
  GstElement *colorspace = NULL, *colorcaps = NULL;
#endif
  gchar *vcodec = NULL;

  videosrc = gst_element_factory_make ("xvimagesrc", "videosrc");
  if (NULL == videosrc) {
    GST_ERROR_OBJECT (client, "failed to create xvimagesrc element");
    goto create_error;
  }

#ifdef ENABLE_WFD_EXTENDED_FEATURES
  if (client->extended_feature_support) {
    void (*func)(GstRTSPClient *, GstElement *) = __gst_extended_func(client, EXT_SET_VIDEOSRC_PROP);
    func(client, videosrc);
  }
#endif

#ifdef ENABLE_QC_SPECIFIC
  if (client->hdcp_support) {
    GST_INFO_OBJECT (client, " property set to Xvimagesrc : enable secure buffers");
    g_object_set (videosrc, "secure-mode", 1, NULL);
  }
#endif
	g_object_set (videosrc, "display-rotate", (guint64)client->display_rotate, NULL);

  /* create video caps element */
  vcaps = gst_element_factory_make ("capsfilter", "videocaps");
  if (NULL == vcaps) {
    GST_ERROR_OBJECT (client, "failed to create video capsilfter element");
    goto create_error;
  }

  GST_INFO_OBJECT (client, "picked xvimagesrc as video source");
  g_object_set (G_OBJECT (vcaps), "caps",
      gst_caps_new_simple ("video/x-raw-yuv",
          "width", G_TYPE_INT, client->cMaxWidth,
          "height", G_TYPE_INT, client->cMaxHeight,
          "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('S','N','1','2'),
          "framerate", GST_TYPE_FRACTION, client->cFramerate, 1, NULL), NULL);
#ifdef SOFTWARE_ENC
  /* create video caps element */
  colorspace = gst_element_factory_make ("ffmpegcolorspace", "videocolorspace");
  if (NULL == colorspace) {
    GST_ERROR_OBJECT (client, "failed to create ffmpegcolorspace element");
    goto create_error;
  }
  colorcaps = gst_element_factory_make ("capsfilter", "videocolorcaps");
  if (NULL == colorcaps) {
    GST_ERROR_OBJECT (client, "failed to create video capsilfter element");
    goto create_error;
  }
  g_object_set (G_OBJECT (colorcaps), "caps",
      gst_caps_new_simple ("video/x-raw-yuv",
          "width", G_TYPE_INT, client->cMaxWidth,
          "height", G_TYPE_INT, client->cMaxHeight,
          "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('I','4','2','0'),
          "framerate", GST_TYPE_FRACTION, 30, 1, NULL), NULL);

  if(client->cvCodec == WFD_VIDEO_H264)
    vcodec = g_strdup("savsenc_h264");//("omx_h264enc");
#else
  if(client->cvCodec == WFD_VIDEO_H264) {
#if defined(ENABLE_QC_SPECIFIC) && (USE_HDCP)
  if (client->hdcp_support)
    vcodec = g_strdup("omx_h264enc_secure");
  else
    vcodec = g_strdup("omx_h264enc");
#else
  vcodec = g_strdup("omx_h264enc");
#endif
  }
#endif
  else {
    GST_ERROR_OBJECT (client, "Yet to support other than H264 format");
    goto create_error;
  }

  srcbin->venc  = gst_element_factory_make (vcodec, "videoenc");
  if (!srcbin->venc) {
    GST_ERROR_OBJECT (client, "failed to create video encoder element");
    goto create_error;
  }
  g_object_set (srcbin->venc, "bitrate", client->init_udp_bitrate, NULL);
  g_object_set (srcbin->venc, "byte-stream", 1, NULL);
  g_object_set (srcbin->venc, "append-dci", 1, NULL);
  g_object_set (srcbin->venc, "idr-period", 120, NULL);
  if (!client->hdcp_support) {
    g_object_set (srcbin->venc, "iframe-as-idr", 1, NULL);
    g_object_set (srcbin->venc, "min-qp", 8, NULL);
    g_object_set (srcbin->venc, "max-qp", 44, NULL);
    g_object_set (srcbin->venc, "control-rate", 2, NULL);
    g_object_set (srcbin->venc, "encoder-profile", 0x7F000001, NULL);
    g_object_set (srcbin->venc, "encoder-level", 0x200, NULL);
  }

#ifdef USE_HDCP
  if (client->hdcp_support) {
    GST_INFO_OBJECT (client, " property set to Encorder : physical output");
    g_object_set (srcbin->venc, "physical-output", 1, NULL);
  }
#endif

#ifdef ENABLE_WFD_EXTENDED_FEATURES
#ifndef ENABLE_QC_SPECIFIC
  g_object_set (srcbin->venc, "skip-inbuf", VENC_SKIP_INBUF_VALUE, NULL);
#endif
#endif
  srcbin->vqueue  = gst_element_factory_make ("queue", "video-queue");
  if (!srcbin->vqueue) {
    GST_ERROR_OBJECT (client, "failed to create video queue element");
    goto create_error;
  }

  // TODO: check whether queue is required to have default values
#ifdef SOFTWARE_ENC
  gst_bin_add_many (srcbin->srcbin, videosrc, vcaps, colorspace, colorcaps, srcbin->venc, srcbin->vqueue, NULL);
  if (!gst_element_link_many (videosrc, vcaps, colorspace, colorcaps, srcbin->venc, srcbin->vqueue,NULL))
#else
  gst_bin_add_many (srcbin->srcbin, videosrc, vcaps, srcbin->venc, srcbin->vqueue, NULL);
  if (!gst_element_link_many (videosrc, vcaps, srcbin->venc, srcbin->vqueue,NULL))
#endif
  {
    GST_ERROR_OBJECT (client, "Failed to link video src elements...");
    goto create_error;
  }

  return TRUE;

create_error:
  return FALSE;
}

static gboolean
gst_rtsp_client_create_filesrc_bin (GstRTSPClient * client, GstRTSPClientSrcBin *srcbin)
{
  GstElement *vdec = NULL;
  GstElement *videosrc = NULL;
  GstElement *srcdemux = NULL;

  videosrc = gst_element_factory_make ("filesrc", "videosrc");
  if (NULL == videosrc) {
    GST_ERROR_OBJECT (client, "failed to create filesrc element");
    goto create_error;
  }
  g_object_set (videosrc, "location", client->infile, NULL);

  GST_INFO_OBJECT (client, "picked filesrc as video source");

  // TODO: need to add support for remaing demuxers
  srcdemux = gst_element_factory_make ("qtdemux", "demuxer");
  if (!srcdemux) {
    GST_ERROR_OBJECT (client, "failed to create demuxer element");
    goto create_error;
  }
  g_signal_connect (srcdemux, "pad-added", G_CALLBACK(pad_added), client);

  srcbin->vparse = gst_element_factory_make ("legacyh264parse", "parser");
  if (!srcbin->vparse) {
    GST_ERROR_OBJECT(client, "failed to create video parse element");
    goto create_error;
  }
  g_object_set (srcbin->vparse, "config-interval", 1, NULL);
  g_object_set (srcbin->vparse, "output-format", 1, NULL);

  vdec = gst_element_factory_make ("omx_h264dec", "video-dec");
  if (!vdec) {
    GST_ERROR_OBJECT(client, "failed to create video decoder element");
    goto create_error;
  }

  srcbin->venc = gst_element_factory_make ("omx_h264enc", "video-enc");
  if (!srcbin->venc) {
    GST_ERROR_OBJECT(client, "failed to create video encoder element");
    goto create_error;
  }
  g_object_set (srcbin->venc, "bitrate", client->init_udp_bitrate, NULL);
  g_object_set (srcbin->venc, "byte-stream", 1, NULL);
  g_object_set (srcbin->venc, "append-dci", 1, NULL);

  srcbin->vqueue  = gst_element_factory_make ("queue", "video-queue");
  if (!srcbin->vqueue) {
    GST_ERROR_OBJECT (client, "failed to create video queue element");
    goto create_error;
  }

  srcbin->aqueue  = gst_element_factory_make ("queue", "audio-queue");
  if (!srcbin->aqueue) {
    GST_ERROR_OBJECT (client, "failed to create audio queue element");
    goto create_error;
  }

  gst_bin_add_many (srcbin->srcbin, videosrc, srcdemux, srcbin->vparse, vdec,
    srcbin->venc, srcbin->vqueue, srcbin->aqueue, NULL);

  gst_element_link (videosrc, srcdemux);

  if (!gst_element_link_many (srcbin->vparse, vdec, srcbin->venc, srcbin->vqueue, NULL)) {
    GST_ERROR_OBJECT(client,"failed to link vparse &vqueue...\n");
    goto create_error;
  }

  return TRUE;

create_error:
  return FALSE;

}
/**
* gst_rtsp_client_create_srcbin:
* @client: client object
*
* Creates the server source bin
*
* Returns: void
*/
void gst_rtsp_client_create_srcbin (GstRTSPClient * client)
{
  GstRTSPClientSrcBin *srcbin = NULL;
  GstElement *mux = NULL;
  GstElement *mux_queue = NULL;
  GstElement *payload = NULL;
  GstPad *srcpad = NULL;

  srcbin = g_slice_new0 (GstRTSPClientSrcBin);

  /* create source bin */
  srcbin->srcbin = GST_BIN(gst_bin_new ("srcbin"));
  if (!srcbin->srcbin) {
    GST_ERROR_OBJECT (client, "failed to create source bin...");
    goto create_error;
  }
  if (client->session_mode != WFD_INI_AUDIO_ONLY)
  {
  /* create video src element */
  switch (client->videosrc_type) {
    /* using xvimagesrc */
    case WFD_INI_VSRC_XVIMAGESRC: {
      if (!gst_rtsp_client_create_xvcapture_bin (client, srcbin)) {
        GST_ERROR_OBJECT (client, "failed to create xvcapture bin...");
        goto create_error;
      }
    }
    break;
    case WFD_INI_VSRC_FILESRC: {
      /* using filesrc */
      if (!gst_rtsp_client_create_filesrc_bin (client, srcbin)) {
        GST_ERROR_OBJECT (client, "failed to create xvcapture bin...");
        goto create_error;
      }
    }
    break;
    case WFD_INI_VSRC_CAMERASRC: {
      if (!gst_rtsp_client_create_camera_capture_bin (client, srcbin)) {
        GST_ERROR_OBJECT (client, "failed to create xvcapture bin...");
        goto create_error;
      }
    }
    break;
    case WFD_INI_VSRC_VIDEOTESTSRC:
    default:
      GST_ERROR_OBJECT (client, "unknow mode selected...");
      goto create_error;
  }
  }
  mux = gst_element_factory_make ("mpegtsmux", "tsmux");
  if (!mux) {
    GST_ERROR_OBJECT (client, "failed to create muxer element");
    goto create_error;
  }

  client->mpeg_ts_pid = gst_structure_new ("prog_map", "prog_pmt_pid", G_TYPE_UINT, 0x0100,
            "pcr_pid", G_TYPE_INT, 0x1000, "video_es_pid", G_TYPE_INT, 0x1011,
            "audio_es_pid", G_TYPE_INT, 0x1100, NULL);
  g_object_set(mux, "prog-map",client->mpeg_ts_pid, NULL);
  gst_structure_free(client->mpeg_ts_pid);

#ifdef USE_HDCP
  if (client->hdcp_support) {
    g_object_set(mux, "HDCP-handle", client->hdcp_handle, NULL);
    g_object_set(mux, "HDCP-IP", client->wfdsink_ip, NULL);
    g_object_set(mux, "HDCP-port", client->hdcp_tcpport, NULL);
    g_object_set(mux, "HDCP-version", client->hdcp_version, NULL);
    GST_DEBUG_OBJECT (client, "HDCP version is %d", client->hdcp_version);
  }
#endif

  mux_queue  = gst_element_factory_make ("queue", "muxer-queue");
  if (!mux_queue) {
    GST_ERROR_OBJECT (client, "failed to create muxer queue element");
    goto create_error;
  }

  g_object_set (mux_queue, "max-size-buffers", 20000, NULL);

  payload = gst_element_factory_make ("rtpmp2tpay", "pay0");
  if (!payload) {
    GST_ERROR_OBJECT (client, "failed to create payload element");
    goto create_error;
  }

  g_object_set (payload, "pt", 33, NULL);
  g_object_set (payload, "mtu", client->MTUsize, NULL);
  g_object_set (payload, "rtp-flush", (gboolean)TRUE, NULL);

  gst_bin_add_many (srcbin->srcbin, mux, mux_queue, payload, NULL);

  if (!gst_element_link_many (mux, mux_queue, payload, NULL)) {
    GST_ERROR_OBJECT (client, "Failed to link muxer & payload...");
    goto create_error;
  }
  if (client->session_mode != WFD_INI_AUDIO_ONLY)
  {
  /* request sink pad from muxer */
  srcbin->mux_vsinkpad = gst_element_get_request_pad (mux, "sink_%d");
  if (!srcbin->mux_vsinkpad) {
    GST_ERROR_OBJECT (client, "Failed to get sink pad from muxer...");
    goto create_error;
  }

  /* request srcpad from video queue */
  srcpad = gst_element_get_static_pad (srcbin->vqueue, "src");
  if (!srcpad) {
    GST_ERROR_OBJECT (client, "Failed to get srcpad from video queue...");
    goto create_error;
  }

  if (gst_pad_link (srcpad, srcbin->mux_vsinkpad) != GST_PAD_LINK_OK) {
    GST_ERROR_OBJECT (client, "Failed to link video queue src pad & muxer video sink pad...");
    goto create_error;
  }

  gst_object_unref (srcpad);
  srcpad = NULL;
  }
  if (client->session_mode != WFD_INI_VIDEO_ONLY) {

    if (client->videosrc_type != WFD_INI_VSRC_FILESRC) {
      /* create audio source elements & add to pipeline */
      if (!gst_rtsp_client_create_audio_capture_bin (client, srcbin))
        goto create_error;
    }

    /* request sink pad from muxer */
    srcbin->mux_asinkpad = gst_element_get_request_pad (mux, "sink_%d");
    if (!srcbin->mux_asinkpad) {
      GST_ERROR_OBJECT (client, "Failed to get sinkpad from muxer...");
      goto create_error;
    }

    /* request srcpad from audio queue */
    srcpad = gst_element_get_static_pad (srcbin->aqueue, "src");
    if (!srcpad) {
      GST_ERROR_OBJECT (client, "Failed to get srcpad from audio queue...");
      goto create_error;
    }

    /* link audio queue's srcpad & muxer sink pad */
    if (gst_pad_link (srcpad, srcbin->mux_asinkpad) != GST_PAD_LINK_OK) {
      GST_ERROR_OBJECT (client, "Failed to link audio queue src pad & muxer audio sink pad...");
      goto create_error;
    }

#ifdef WFD_PAD_PROBE
    GstPad *pad_probe = NULL;
    pad_probe = gst_element_get_static_pad (mux, "src");
    if(NULL == pad_probe)
    {
      GST_INFO("pad for probe not created");
    } else
    GST_INFO("pad for probe SUCCESSFUL");
    gst_pad_add_data_probe (pad_probe, G_CALLBACK (gst_dump_data), client);
#endif

    GstPad *event_probe = NULL;
    event_probe = gst_element_get_static_pad (payload, "src");
    if(NULL == event_probe)
    {
      GST_INFO("pad for probe not created");
    } else
      GST_INFO("pad for probe SUCCESSFUL");
#ifdef ENABLE_WFD_EXTENDED_FEATURES
    if (client->extended_feature_support) {
      void (*func)(GstRTSPClient *, GstPad *) = __gst_extended_func(client, EXT_PAD_ADD_EVENT_PROBE);
      func(client, event_probe);
    }
#endif

    gst_object_unref (srcpad);
    srcpad = NULL;
  }

  client->srcbin = srcbin;

  GST_DEBUG_OBJECT (client, "successfully created source bin...");

  return;

create_error:
  g_slice_free (GstRTSPClientSrcBin, srcbin);

  // TODO: Need to clean everything
  return;
}



#ifdef WFD_PAD_PROBE
static gboolean gst_dump_data (GstPad * pad, GstMiniObject * obj, gpointer u_data)
{
  gint8 *data;
  gint size;

  if (GST_IS_BUFFER (obj)) {
    GstBuffer *buffer = GST_BUFFER_CAST (obj);
    GST_LOG ("got buffer %p with size %d", buffer, GST_BUFFER_SIZE (buffer));
    data = (gint8 *)GST_BUFFER_DATA(buffer);
    size = GST_BUFFER_SIZE(buffer);
    f = fopen("/root/probe.ts", "a");
    fwrite(data, size, 1, f);
    fclose(f);
  }
  return TRUE;
}
#endif

void uibc_new_tcp(GstRTSPClient *client)
{
  int  newsockfd, sockfd=0, nSockOpt=1, namelen, read_len=0, prev_buf_len = 0, offset = 0;
  guint16 msg_len = 0;
  guint8 read_buf[101] = {0};
  guint8 prev_buf[200] = {0};
  struct sockaddr_in serv_addr;
  struct sockaddr_in _client;
  WFDUIBCMessage *msg = NULL;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    GST_ERROR_OBJECT (client, "ERROR opening socket for UIBC\n");
  }

  memset(&serv_addr, '0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(client->uibc->port);

  if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, (void *) &nSockOpt, sizeof (nSockOpt)) < 0) {
    GST_ERROR ("SO_REUSEADDR setsockopt failed");
  }

  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    GST_ERROR_OBJECT (client, "ERROR on binding");
  } else

  if (listen(sockfd,1) <0) {
    GST_ERROR_OBJECT (client, "ERROR on socket listen \n");
  }

  client->uibc->mainfd = sockfd;

  GST_DEBUG_OBJECT(client, "uibc  waiting for accept \n");
  namelen = sizeof(_client);
  newsockfd = accept(sockfd, (struct sockaddr *)&_client, (socklen_t * __restrict__)(&namelen));
  if (newsockfd < 0) {
    GST_ERROR_OBJECT(client, "ERROR on accept\n");
    close(sockfd);
    return;
  } else {
    GST_DEBUG_OBJECT (client, "uibc connected \n");
    client->uibc->fd = newsockfd;
    client->uibc->control_cb(TRUE, (void *)client->uibc->neg_hidc_cap_list, client->uibc->neg_hidc_cap_count, client->cMaxWidth, client->cMaxHeight, client->uibc->user_param);
  }

  do {
    read_len = read(newsockfd, read_buf, 100);
    if (read_len == 0 || read_len == -1) break;
    GST_DEBUG ("UIBC msg of %d length has been received and prev buffer len is %d", read_len, prev_buf_len);
    memcpy (prev_buf + prev_buf_len, read_buf, read_len);
    prev_buf_len += read_len;
    offset = 0;
    msg_len = 0;
    //GST_DEBUG ("offset and msg_len are %d %d", offset, msg_len);
EXTRACT_MSG:
    if((prev_buf_len - offset) > 4) {
      msg_len = (prev_buf[2 + offset] << 8 | prev_buf[3 + offset]);
      //GST_DEBUG ("msg_len of first msg is %d", msg_len);
      if(msg_len && (msg_len <= (prev_buf_len - offset))) {
        msg = (WFDUIBCMessage *) malloc (sizeof (WFDUIBCMessage));
        memset(msg, 0x00, sizeof(WFDUIBCMessage));
        int res = wfd_uibc_message_parse_buffer ((const guint8 *)(prev_buf + offset), msg_len, msg);
        GST_DEBUG ("return from parse buffer is %d", res);
        //wfd_uibc_message_dump (msg);
        client->uibc->event_cb((void *)msg, client->uibc->user_param);
        offset += msg_len;
        //GST_DEBUG ("offset is %d ", offset);
        if(offset < prev_buf_len) goto EXTRACT_MSG;
      }
    }
    prev_buf_len -= offset;
    memcpy (prev_buf , prev_buf + offset, prev_buf_len);
    //GST_DEBUG ("prev_buf_len and offset is %d %d", prev_buf_len, offset);
  }while(read_len > 0);

  client->uibc->control_cb(FALSE, NULL, 0, 0, 0, client->uibc->user_param);
  close(sockfd);
  return;
}

GstRTSPResult uibc_enable_request (GstRTSPClient * client)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPUrl *url = NULL;
  gchar *url_str = NULL;
  GError *error = NULL;

  client->uibc->thread = g_thread_create ((GThreadFunc) uibc_new_tcp, client, TRUE, &error);
  if(client->uibc->thread) GST_DEBUG_OBJECT (client, "uibc thread has been created");

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

  res = prepare_request (client, &request, GST_RTSP_SET_PARAMETER, url_str, WFD_MESSAGE_15, WFD_TRIGGER_UNKNOWN);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to prepare M15 request....\n");
    return res;
  }

  GST_DEBUG_OBJECT (client, "Sending GST_RTSP_SET_PARAMETER request message (M15)...");

  g_print(" --Send M15 UIBC Request-- ");
  send_request (client, NULL, &request);

  /* Wait for GST_RTSP_SET_PARAMETER response (M15 response) */
  res = gst_rtsp_connection_receive (client->connection, &response, NULL);
  if (GST_RTSP_OK != res) {
    GST_ERROR_OBJECT (client, "Failed to receive M15 response....\n");
    return res;
  }

  if (gst_debug_category_get_threshold (rtsp_client_wfd_debug) >= GST_LEVEL_LOG) {
    gst_rtsp_message_dump (&response);
  }

  return res;
}

void gst_wfd_print_rtsp_msg(GstRTSPMethod method)
{
  switch (method) {
    case GST_RTSP_SETUP:
      g_printf("\n --Read M6 SETUP request-- \n");
      break;
    case GST_RTSP_PLAY:
      g_printf("\n --Read M7 PLAY request-- \n");
      break;
    case GST_RTSP_PAUSE:
      g_printf("\n --Read M9 PAUSE request-- \n");
      break;
    case GST_RTSP_TEARDOWN:
      g_printf("\n --Read M8 PAUSE request-- \n");
      break;
    case GST_RTSP_SET_PARAMETER:
      break;
    case GST_RTSP_GET_PARAMETER:
      break;
    default:
      break;
  }

  return;
}

GstRTSPResult
gst_wfd_handle_response(GstRTSPClient * client, GstRTSPMessage * message)
{
  GstRTSPStatusCode code;
  const gchar *uristr;
  GstRTSPVersion version;

  gst_rtsp_message_parse_response (message, &code, &uristr, &version);
  GST_INFO_OBJECT (client, "revd message method = %d", code);

  GstRTSPResult res = GST_RTSP_OK;
  gchar *data = NULL;
  guint size=0;

  res = gst_rtsp_message_get_body (message, (guint8**)&data, &size);

  if (res != GST_RTSP_OK) {
    GST_ERROR_OBJECT (client, "Failed to get body of response...");
    return res;
  }

  if (size == 0) {
    g_mutex_lock(client->keep_alive_lock);
    if (client->keep_alive_flag == FALSE) {
      GST_DEBUG_OBJECT(client, "This is response for keep alive msg");
      client->keep_alive_flag = TRUE;
    }
    g_mutex_unlock(client->keep_alive_lock);
  } else if (size > 0) {
#ifdef ENABLE_WFD_EXTENDED_FEATURES
  if (client->extended_feature_support) {
    GstRTSPResult (*func)(GstRTSPClient *, GstRTSPMessage *) = __gst_extended_func(client, EXT_HANDLE_RESP);
    res = func(client, message);
  }
#endif
  }

  return res;
}

