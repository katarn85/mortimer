/*
 * wfdrtspsrc
 *
 * Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

/**
* SECTION:element-wfdrtspsrc
*
* Makes a connection to an RTSP server and read the data.
* Device recognition is through wifi direct.
* wfdrtspsrc strictly follows Wifi display specification.
*
* RTSP supports transport over TCP or UDP in unicast or multicast mode. By
* default wfdrtspsrc will negotiate a connection in the following order:
* UDP unicast/UDP multicast/TCP. The order cannot be changed but the allowed
* protocols can be controlled with the #GstWFDRTSPSrc:protocols property.
*
* wfdrtspsrc currently understands WFD capability negotiation messages
*
* wfdrtspsrc will internally instantiate an RTP session manager element
* that will handle the RTCP messages to and from the server, jitter removal,
* packet reordering along with providing a clock for the pipeline.
* This feature is implemented using the gstrtpbin element.
*
* wfdrtspsrc acts like a live source and will therefore only generate data in the
* PLAYING state.
*
* <refsect2>
* <title>Example launch line</title>
* |[
* gst-launch wfdrtspsrc location=rtsp://some.server/url ! fakesink
* ]| Establish a connection to an RTSP server and send the raw RTP packets to a
* fakesink.
* </refsect2>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <stdio.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <dlfcn.h>



#include "gstwfdrtspsrc.h"
#include "wfdrtspmacro.h"

#ifdef G_OS_WIN32
#include <winsock2.h>
#endif

GST_DEBUG_CATEGORY_STATIC (wfdrtspsrc_debug);
#define GST_CAT_DEFAULT (wfdrtspsrc_debug)

static GstStaticPadTemplate gst_wfdrtspsrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "payload = (int) 33"));

/* signals and args */
enum
{
  SIGNAL_UPDATE_MEDIA_INFO,
  SIGNAL_AV_FORMAT_CHANGE,
  LAST_SIGNAL
};

enum _GstWfdRtspSrcBufferMode
{
  BUFFER_MODE_NONE,
  BUFFER_MODE_SLAVE,
  BUFFER_MODE_BUFFER,
  BUFFER_MODE_AUTO
};

#define _SET_FRAMERATE_TO_CAPS 1


#define GST_TYPE_WFD_RTSP_NAT_METHOD (gst_wfd_rtsp_nat_method_get_type())
static GType
gst_wfd_rtsp_nat_method_get_type (void)
{
  static GType wfd_rtsp_nat_method_type = 0;
  static const GEnumValue wfd_rtsp_nat_method[] = {
    {GST_WFD_RTSP_NAT_NONE, "None", "none"},
    {GST_WFD_RTSP_NAT_DUMMY, "Send Dummy packets", "dummy"},
    {0, NULL, NULL},
  };

  if (!wfd_rtsp_nat_method_type) {
    wfd_rtsp_nat_method_type =
        g_enum_register_static ("GstWFDRTSPNatMethod", wfd_rtsp_nat_method);
  }
  return wfd_rtsp_nat_method_type;
}

/* properties default values */
#define DEFAULT_LOCATION         NULL
#define DEFAULT_DEBUG            FALSE
#define DEFAULT_RETRY            20
#define DEFAULT_TCP_TIMEOUT      500000000
#define DEFAULT_NAT_METHOD       GST_WFD_RTSP_NAT_DUMMY
#define DEFAULT_PROXY            NULL
#define DEFAULT_RTP_BLOCKSIZE    0
#define DEFAULT_USER_ID          NULL
#define DEFAULT_USER_PW          NULL
#define DEFAULT_PORT_RANGE       NULL
#define DEFAULT_USER_AGENT       "SEC-WDH/VND-DAREF"
#define DEFAULT_DO_RTCP          TRUE
#define DEFAULT_LATENCY_MS       2000
#define DEFAULT_UDP_BUFFER_SIZE  0x80000
#define DEFAULT_UDP_TIMEOUT          10000000
#ifdef ENABLE_WFD_SINK_UIBC
#define DEFAULT_UIBC_GEN_CAPABILITY 15
#define DEFAULT_UIBC_HIDC_CAP_COUNT 4
#endif
#ifdef ENABLE_WFD_EXTENDED_FEATURES
#define DEFAULT_DEVICE_NAME      "Sink"
#define DEFAULT_BUF_ANIMATION    FALSE
#define DEFAULT_DO_REQUEST       FALSE
#endif


enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_DEBUG,
  PROP_RETRY,
  PROP_TCP_TIMEOUT,
  PROP_NAT_METHOD,
  PROP_PROXY,
  PROP_RTP_BLOCKSIZE,
  PROP_USER_ID,
  PROP_USER_PW,
  PROP_USER_AGENT,
  PROP_AUDIO_PARAM,
  PROP_VIDEO_PARAM,
  PROP_HDCP_PARAM,
  PROP_DO_RTCP,
  PROP_LATENCY,
  PROP_UDP_BUFFER_SIZE,
  PROP_UDP_TIMEOUT,
#ifdef ENABLE_WFD_SINK_UIBC
  PROP_USER_INPUT_EVENT,
  PROP_UIBC_STATE_CB,
#endif
#ifdef ENABLE_WFD_EXTENDED_FEATURES
  PROP_DEVICE_NAME,
  PROP_BUF_ANIMATION,
  PROP_DO_REQUEST,
  PROP_DEMUX_HANDLE,
  PROP_AUDIO_QUEUE_HANDLE,
#endif
  PROP_ENABLE_PAD_PROBE,
  PROP_LAST
};

/* commands we send to out loop to notify it of events */
#define CMD_OPEN	0
#define CMD_PLAY	1
#define CMD_PAUSE	2
#define CMD_CLOSE	3
#define CMD_WAIT	4
#define CMD_LOOP	5
#define CMD_SET_PARAM	6

#define GST_ELEMENT_PROGRESS(el, type, code, text)      \
G_STMT_START {                                          \
  gchar *__txt = _gst_element_error_printf text;        \
  gst_element_post_message (GST_ELEMENT_CAST (el),      \
      gst_message_new_progress (GST_OBJECT_CAST (el),   \
          GST_PROGRESS_TYPE_ ##type, code, __txt));     \
  g_free (__txt);                                       \
} G_STMT_END

/* object */
static void gst_wfdrtspsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_wfdrtspsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_wfdrtspsrc_dispose (GObject * object);
static void gst_wfdrtspsrc_finalize (GObject * object);

/* element */
static GstStateChangeReturn gst_wfdrtspsrc_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_wfdrtspsrc_send_event (GstElement * element, GstEvent * event);

/* bin */
static void gst_wfdrtspsrc_handle_message (GstBin * bin, GstMessage * message);

static void gst_wfdrtspsrc_loop_send_cmd (GstWFDRTSPSrc * src, gint cmd);

/* fundemental functions */
static GstRTSPResult gst_wfdrtspsrc_open (GstWFDRTSPSrc * src);
static gboolean gst_wfdrtspsrc_setup (GstWFDRTSPSrc * src);
static GstRTSPResult gst_wfdrtspsrc_play (GstWFDRTSPSrc * src);
static GstRTSPResult gst_wfdrtspsrc_pause (GstWFDRTSPSrc * src);
static GstRTSPResult gst_wfdrtspsrc_close (GstWFDRTSPSrc * src, gboolean only_close);

static gboolean gst_wfdrtspsrc_set_proxy (GstWFDRTSPSrc * rtsp, const gchar * proxy);
static void gst_wfdrtspsrc_set_tcp_timeout (GstWFDRTSPSrc * src, guint64 timeout);


static gboolean gst_wfdrtspsrc_setup_auth (GstWFDRTSPSrc * src,
    GstRTSPMessage * response);

static GstRTSPResult gst_wfdrtspsrc_send_cb (GstRTSPExtension * ext,
    GstRTSPMessage * request, GstRTSPMessage * response, GstWFDRTSPSrc * src);


static void gst_wfdrtspsrc_uri_handler_init (gpointer g_iface,
    gpointer iface_data);
static gboolean gst_wfdrtspsrc_uri_set_uri (GstURIHandler * handler,
    const gchar * uri);

static gboolean gst_wfdrtspsrc_loop (GstWFDRTSPSrc * src);
static gboolean gst_wfdrtspsrc_stream_push_event (WFDRTSPManager * manager,
    GstEvent * event, gboolean source);
static gboolean gst_wfdrtspsrc_push_event (GstWFDRTSPSrc * src, GstEvent * event,
    gboolean source);


static GstRTSPResult
gst_wfdrtspsrc_send (GstWFDRTSPSrc * src, GstRTSPConnection * conn,
    GstRTSPMessage * request, GstRTSPMessage * response,
    GstRTSPStatusCode * code);

static gboolean gst_wfdrtspsrc_parse_methods (GstWFDRTSPSrc * src, GstRTSPMessage * response);

static GstRTSPResult gst_wfdrtspsrc_send_keep_alive (GstWFDRTSPSrc * src);

static void gst_wfdrtspsrc_connection_flush (GstWFDRTSPSrc * src, gboolean flush);

static GstRTSPResult gst_wfdrtspsrc_get_video_parameter(GstWFDRTSPSrc * src, WFDMessage *msg);
static GstRTSPResult gst_wfdrtspsrc_get_audio_parameter(GstWFDRTSPSrc * src, WFDMessage *msg);

#ifdef ENABLE_WFD_SINK_UIBC
static void uibc_new_tcp (GstWFDRTSPSrc * src);
void __wfd_uibc_init(GstWFDRTSPSrc * src)
{
  src->uibc_handle = dlopen(WFD_UIBC_FEATURES_PATH, RTLD_LAZY);
  if (src->uibc_handle == NULL)
  {
    GST_ERROR("failed to init wfd uibc init");
    src->extended_uibc_support = FALSE;
  }
  else
  {
    GST_ERROR("Succeeded to init wfd uibc init");
    src->extended_uibc_support = TRUE;
  }
  return;
}

void *
__wfd_uibc_func(GstWFDRTSPSrc *src, const char *func)
{
  return dlsym(src->uibc_handle, func);
}

void
__wfd_uibc_feature_close(GstWFDRTSPSrc *src)
{
  GST_DEBUG("close uibc features");

  dlclose(src->uibc_handle);
  src->uibc_handle = NULL;
  src->extended_uibc_support = FALSE;
}

#endif


#ifdef ENABLE_WFD_MESSAGE

void __wfd_config_message_init(GstWFDRTSPSrc * src)
{
  src->message_handle = dlopen(WFD_MESSAGE_FEATURES_PATH, RTLD_LAZY);
  if (src->message_handle == NULL)
  {
    GST_ERROR("failed to init __wfd_config_message_init");
    src->extended_wfd_message_support = FALSE;
  }
  else
  {
    src->extended_wfd_message_support = TRUE;
  }
  return;
}

void *
__wfd_config_message_func(GstWFDRTSPSrc *src, const char *func)
{
  return dlsym(src->message_handle, func);
}

void
__wfd_config_message_close(GstWFDRTSPSrc *src)
{
  GST_DEBUG("close wfd cofig message");

  dlclose(src->message_handle);
  src->message_handle = NULL;
  src->extended_wfd_message_support = FALSE;
}
#endif


static gboolean
gst_wfdrtspsrc_handle_src_event (GstPad * pad, GstEvent * event);
static gboolean
gst_wfdrtspsrc_handle_src_query(GstPad * pad, GstQuery * query);


static guint gst_wfdrtspsrc_signals[LAST_SIGNAL] = { 0 };

#ifdef ENABLE_WFD_EXTENDED_FEATURES
#define WFD_EXTENDED_FEATURES_PATH "/usr/lib/libwfdrtspmanagerext.so"
#define WFD_EXTENDED_HANDLE_REQUEST "wfd_rtsp_manager_hadle_request"
#define WFD_EXTENDED_CONFIGURE_TRANSPORT "wfd_rtsp_manager_configure_transport"
#define WFD_EXTENDED_MANAGER_NEW "wfd_rtsp_manager_new"
#define WFD_EXTENDED_MESSAGE_DUMP "wfd_rtsp_manager_message_dump"
#define WFD_EXTENDED_FLUSH "wfd_rtsp_manager_flush"
#define WFD_EXTENDED_ENABLE_PAD_PROBE "wfd_rtsp_manager_enable_pad_probe"


void __wfd_extended_feature_init(void **handle)
{
  *handle = dlopen(WFD_EXTENDED_FEATURES_PATH, RTLD_LAZY);
  if (*handle == NULL)
    GST_ERROR("failed to init extended features");
  return;
}


void __wfd_extended_feature_open(void *  handle, WFDRTSPManager *manager)
{
  if (handle == NULL) {
    GST_ERROR("failed to open extended features");

    manager->extended_handle = NULL;
    manager->extended_feature_support = FALSE;
    return;
  }

  GST_DEBUG("extended feature is enabled");

  manager->extended_handle = handle;
  manager->extended_feature_support = TRUE;

  return;
}

void *
__wfd_extended_func(WFDRTSPManager *manager, const char *func)
{
  return dlsym(manager->extended_handle, func);
}

void
__wfd_extended_feature_close(WFDRTSPManager *manager)
{
  GST_DEBUG("close extended features");

  dlclose(manager->extended_handle);
  manager->extended_handle = NULL;
  manager->extended_feature_support = FALSE;
}

GstRTSPResult func_dump(GstWFDRTSPSrc *src, GstRTSPMessage *msg)
{
  GstRTSPResult res;
  GstRTSPResult (*func)(GstRTSPMessage *)
                = __wfd_extended_func(src->manager, WFD_EXTENDED_MESSAGE_DUMP);
  res =  func(msg);
  return res;
}
#endif

static void
_do_init (GType wfdrtspsrc_type)
{
  static const GInterfaceInfo urihandler_info = {
    gst_wfdrtspsrc_uri_handler_init,
    NULL,
    NULL
  };

  GST_DEBUG_CATEGORY_INIT (wfdrtspsrc_debug, "wfdrtspsrc", 0, "WFD RTSP src");

  g_type_add_interface_static (wfdrtspsrc_type, GST_TYPE_URI_HANDLER,
      &urihandler_info);
}

GST_BOILERPLATE_FULL (GstWFDRTSPSrc, gst_wfdrtspsrc, GstBin, GST_TYPE_BIN, _do_init);

static void
gst_wfdrtspsrc_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_wfdrtspsrc_src_template));

  gst_element_class_set_details_simple (element_class, "Wi-Fi Display source element",
      "Source/Network",
      "Receive data over the Wi-Fi Direct network via RTSP",
      "YeJin Cho <cho.yejin@samsung.com>");
}

static void
gst_wfdrtspsrc_class_init (GstWFDRTSPSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBinClass *gstbin_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbin_class = (GstBinClass *) klass;

  gobject_class->set_property = gst_wfdrtspsrc_set_property;
  gobject_class->get_property = gst_wfdrtspsrc_get_property;
  gobject_class->dispose = gst_wfdrtspsrc_dispose;
  gobject_class->finalize = gst_wfdrtspsrc_finalize;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "RTSP Location",
          "Location of the RTSP url to read",
          DEFAULT_LOCATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DEBUG,
      g_param_spec_boolean ("debug", "Debug",
          "Dump request and response messages to stdout",
          DEFAULT_DEBUG, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RETRY,
      g_param_spec_uint ("retry", "Retry",
          "Max number of retries when allocating RTP ports.",
          0, G_MAXUINT16, DEFAULT_RETRY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TCP_TIMEOUT,
      g_param_spec_uint64 ("tcp-timeout", "TCP Timeout",
          "Fail after timeout microseconds on TCP connections (0 = disabled)",
          0, G_MAXUINT64, DEFAULT_TCP_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NAT_METHOD,
      g_param_spec_enum ("nat-method", "NAT Method",
          "Method to use for traversing firewalls and NAT",
          GST_TYPE_WFD_RTSP_NAT_METHOD, DEFAULT_NAT_METHOD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PROXY,
      g_param_spec_string ("proxy", "Proxy",
          "Proxy settings for HTTP tunneling. Format: [http://][user:passwd@]host[:port]",
          DEFAULT_PROXY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RTP_BLOCKSIZE,
      g_param_spec_uint ("rtp-blocksize", "RTP Blocksize",
          "RTP package size to suggest to server (0 = disabled)",
          0, 65536, DEFAULT_RTP_BLOCKSIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_USER_ID,
      g_param_spec_string ("user-id", "user-id",
          "RTSP location URI user id for authentication", DEFAULT_USER_ID,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_USER_PW,
      g_param_spec_string ("user-pw", "user-pw",
          "RTSP location URI user password for authentication", DEFAULT_USER_PW,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

   g_object_class_install_property (gobject_class, PROP_USER_AGENT,
      g_param_spec_string ("user-agent", "User Agent",
          "User agent specified string", DEFAULT_USER_AGENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_AUDIO_PARAM,
      g_param_spec_boxed ("audio-param", "audio parameters",
          "A GstStructure specifies the mapping for audio parameters",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_VIDEO_PARAM,
      g_param_spec_boxed ("video-param", "video parameters",
          "A GstStructure specifies the mapping for video parameters",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_HDCP_PARAM,
      g_param_spec_boxed ("hdcp-param", "HDCP parameters",
          "A GstStructure specifies the mapping for HDCP parameters",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_DO_RTCP,
      g_param_spec_boolean ("do-rtcp", "Do RTCP",
          "Send RTCP packets, disable for old incompatible server.",
          DEFAULT_DO_RTCP, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LATENCY,
      g_param_spec_uint ("latency", "Buffer latency in ms",
          "Amount of ms to buffer", 0, G_MAXUINT, DEFAULT_LATENCY_MS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_UDP_BUFFER_SIZE,
      g_param_spec_int ("udp-buffer-size", "UDP Buffer Size",
          "Size of the kernel UDP receive buffer in bytes, 0=default",
          0, G_MAXINT, DEFAULT_UDP_BUFFER_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_UDP_TIMEOUT,
      g_param_spec_uint64 ("timeout", "Timeout",
          "Fail after timeout microseconds on UDP connections (0 = disabled)",
          0, G_MAXUINT64, DEFAULT_UDP_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

#ifdef ENABLE_WFD_SINK_UIBC
  g_object_class_install_property (gobject_class, PROP_USER_INPUT_EVENT,
      g_param_spec_pointer ("user-input-event", "User Input Event",
          "UIBC related User Input Events", G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_UIBC_STATE_CB,
        g_param_spec_pointer ("uibc-state-cb", "UIBC State Callback",
            "Notifying UIBC state to application", G_PARAM_WRITABLE));
#endif

#ifdef ENABLE_WFD_EXTENDED_FEATURES
  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device Name",
          "Device Name of the WFD sink",
          DEFAULT_LOCATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUF_ANIMATION,
      g_param_spec_boolean ("buffering-animation", "Buffering Animation",
          "State of buffering animation",
          DEFAULT_BUF_ANIMATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DO_REQUEST,
      g_param_spec_boolean ("do-request", "Enable RTP Retransmission Request",
          "Send RTCP FB packets and handel retransmitted RTP packets.",
          DEFAULT_DO_REQUEST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DEMUX_HANDLE,
        g_param_spec_pointer ("demux-handle", "Demux Handle",
            "To get demux pointer from application", G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_AUDIO_QUEUE_HANDLE,
          g_param_spec_pointer ("audio-queue-handle", "Audio Queue Handle",
              "To get audio queue pointer from application", G_PARAM_READWRITE));
#endif

  g_object_class_install_property (gobject_class, PROP_ENABLE_PAD_PROBE,
          g_param_spec_boolean ("enable-pad-probe", "Enable Pad Probe",
              "Enable pad probe for debugging",
              FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_wfdrtspsrc_signals[SIGNAL_UPDATE_MEDIA_INFO] =
      g_signal_new ("update-media-info", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstWFDRTSPSrcClass, update_media_info),
      NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, GST_TYPE_STRUCTURE);

  gst_wfdrtspsrc_signals[SIGNAL_AV_FORMAT_CHANGE] =
      g_signal_new ("change-av-format", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstWFDRTSPSrcClass, change_av_format),
      NULL, NULL, g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

  gstelement_class->send_event = gst_wfdrtspsrc_send_event;
  gstelement_class->change_state = gst_wfdrtspsrc_change_state;

  gstbin_class->handle_message = gst_wfdrtspsrc_handle_message;

  gst_wfd_rtsp_ext_list_init ();
}


static void
gst_wfdrtspsrc_init (GstWFDRTSPSrc * src, GstWFDRTSPSrcClass * g_class)
{
  GstPadTemplate *template = NULL;
  GstPad * outpad = NULL;
#ifdef ENABLE_WFD_EXTENDED_FEATURES
  void * handle;
#endif

#ifdef G_OS_WIN32
  WSADATA wsa_data;

  if (WSAStartup (MAKEWORD (2, 2), &wsa_data) != 0) {
    GST_ERROR_OBJECT (src, "WSAStartup failed: 0x%08x", WSAGetLastError ());
  }
#endif

  src->conninfo.location = g_strdup (DEFAULT_LOCATION);
  src->conninfo.url_str = NULL;
  src->debug = DEFAULT_DEBUG;
  src->retry = DEFAULT_RETRY;
  gst_wfdrtspsrc_set_tcp_timeout (src, DEFAULT_TCP_TIMEOUT);
  src->nat_method = DEFAULT_NAT_METHOD;
  gst_wfdrtspsrc_set_proxy (src, DEFAULT_PROXY);
  src->rtp_blocksize = DEFAULT_RTP_BLOCKSIZE;
  src->user_id = g_strdup (DEFAULT_USER_ID);
  src->user_pw = g_strdup (DEFAULT_USER_PW);
  src->user_agent = g_strdup (DEFAULT_USER_AGENT);
  src->ssr_timeout.tv_sec = 0;
  src->ssr_timeout.tv_usec = 0;
  src->is_paused = TRUE;
  src->do_stop = FALSE;
  src->audio_param = NULL;
  src->video_param = NULL;
  src->hdcp_param = NULL;
  src->wfd_param = WFD_PARAM_NONE;
#ifdef ENABLE_WFD_SINK_UIBC
  src->uibc = (UIBC *)malloc(sizeof(UIBC));
  memset (src->uibc, 0x00, sizeof(UIBC));
  __wfd_uibc_init(src);
#endif
#ifdef ENABLE_WFD_MESSAGE
  __wfd_config_message_init(src);
#endif

  /* get a list of all extensions */
  src->extensions = gst_wfd_rtsp_ext_list_get ();

  /* connect to send signal */
  gst_wfd_rtsp_ext_list_connect (src->extensions, "send",
      (GCallback) gst_wfdrtspsrc_send_cb, src);

  /* init lock */
  src->task_rec_lock = g_new (GStaticRecMutex, 1);
  g_static_rec_mutex_init (src->task_rec_lock);

#ifdef ENABLE_WFD_EXTENDED_FEATURES
  __wfd_extended_feature_init(&handle);
  /* create manager */
  if (handle != NULL) {
    WFDRTSPManager * (*func)(GstWFDRTSPSrc *src) = dlsym(handle, WFD_EXTENDED_MANAGER_NEW);
    src->manager =  func(src);
   }
   __wfd_extended_feature_open(handle,src->manager);
#else
  src->manager = wfd_rtsp_manager_new (GST_ELEMENT_CAST(src));
#endif

  /* construct wfdrtspsrc */
  src->manager->session = gst_element_factory_make ("gstrtpsession", "wfdrtspsrc_session");
  if (G_UNLIKELY (src->manager->session == NULL)) {
    GST_ERROR_OBJECT (src, "could not create gstrtpsession element");
    return;
  }

  GST_BIN_CLASS (parent_class)->add_element (GST_BIN_CAST (src),
      src->manager->session);

  src->manager->requester = gst_element_factory_make ("wfdrtprequester", "wfdrtspsrc_requester");
  if (G_UNLIKELY (src->manager->requester == NULL)) {
    GST_ERROR_OBJECT (src, "could not create wfdrtprequester element");
    return;
  }

  GST_BIN_CLASS (parent_class)->add_element (GST_BIN_CAST (src),
      src->manager->requester);

  src->manager->wfdrtpbuffer = gst_element_factory_make ("wfdrtpbuffer", "wfdrtspsrc_wfdrtpbuffer");
  if (G_UNLIKELY (src->manager->wfdrtpbuffer == NULL)) {
    GST_ERROR_OBJECT (src, "could not create wfdrtpbuffer element");
    return;
  }

  GST_BIN_CLASS (parent_class)->add_element (GST_BIN_CAST (src),
      src->manager->wfdrtpbuffer);

  src->manager->channelpad[0] = gst_element_get_request_pad (src->manager->session, "recv_rtp_sink");
  if (G_UNLIKELY (src->manager->channelpad[0]  == NULL)) {
    GST_ERROR_OBJECT (src, "could not create rtp channel pad");
    return;
  }

  src->manager->channelpad[1] = gst_element_get_request_pad (src->manager->session, "recv_rtcp_sink");
  if (G_UNLIKELY (src->manager->channelpad[1]  == NULL)) {
    GST_ERROR_OBJECT (src, "could not create rtcp channel pad");
    return;
  }

  if (!gst_element_link_many(src->manager->session, src->manager->requester, src->manager->wfdrtpbuffer, NULL)) {
    GST_ERROR_OBJECT (src, "failed to link elements for wfdrtspsrc");
    return;
  }

  /* create ghost pad for using src pad */
  outpad = gst_element_get_static_pad (src->manager->wfdrtpbuffer, "src");
  template = gst_static_pad_template_get (&gst_wfdrtspsrc_src_template);
  src->manager->srcpad = gst_ghost_pad_new_from_template ("src", outpad, template);
  gst_object_unref (template);

  gst_pad_set_event_function (src->manager->srcpad,
      GST_DEBUG_FUNCPTR (gst_wfdrtspsrc_handle_src_event));
  gst_pad_set_query_function (src->manager->srcpad,
      GST_DEBUG_FUNCPTR (gst_wfdrtspsrc_handle_src_query));

  gst_element_add_pad (GST_ELEMENT_CAST (src), src->manager->srcpad);

  src->state = GST_RTSP_STATE_INVALID;

  GST_OBJECT_FLAG_SET (src, GST_ELEMENT_IS_SOURCE);
}

static void
gst_wfdrtspsrc_dispose (GObject * object)
{
  GstWFDRTSPSrc *src = GST_WFDRTSPSRC (object);
  WFDRTSPManager *manager = src->manager;

  if (manager->session != NULL) {
    GST_BIN_CLASS (parent_class)->remove_element (GST_BIN_CAST (src),
        manager->session);
    manager->session = NULL;
  }

    if (manager->requester != NULL) {
     GST_BIN_CLASS (parent_class)->remove_element (GST_BIN_CAST (src),
        manager->requester);
    manager->requester = NULL;
  }

  if (manager->wfdrtpbuffer != NULL) {
     GST_BIN_CLASS (parent_class)->remove_element (GST_BIN_CAST (src),
        manager->wfdrtpbuffer);
    manager->wfdrtpbuffer = NULL;
  }

  if (manager->srcpad) {
    gst_pad_set_active (manager->srcpad, FALSE);
    gst_element_remove_pad (GST_ELEMENT_CAST (src), manager->srcpad);
    manager->srcpad = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
gst_wfdrtspsrc_finalize (GObject * object)
{
  GstWFDRTSPSrc *src;

  src = GST_WFDRTSPSRC (object);

  gst_wfd_rtsp_ext_list_free (src->extensions);
  gst_rtsp_url_free (src->conninfo.url);
  if (src->conninfo.location)
    g_free (src->conninfo.location);
  src->conninfo.location = NULL;
  if (src->conninfo.url_str)
    g_free (src->conninfo.url_str);
  src->conninfo.url_str = NULL;
  if (src->user_id)
    g_free (src->user_id);
  src->user_id = NULL;
  if (src->user_pw)
    g_free (src->user_pw);
  src->user_pw = NULL;
  if (src->user_agent)
    g_free (src->user_agent);
  src->user_agent = NULL;

  if(src->audio_param) {
    gst_structure_free(src->audio_param);
    src->audio_param = NULL;
  }
  if(src->video_param) {
    gst_structure_free(src->video_param);
    src->video_param = NULL;
  }
  if(src->hdcp_param) {
    gst_structure_free(src->hdcp_param);
    src->hdcp_param = NULL;
  }

  /* free locks */
  g_static_rec_mutex_free (src->task_rec_lock);
  g_free (src->task_rec_lock);

#ifdef ENABLE_WFD_SINK_UIBC
__wfd_uibc_feature_close(src);
#endif
#ifdef ENABLE_WFD_MESSAGE
__wfd_config_message_close(src);
#endif

#ifdef ENABLE_WFD_EXTENDED_FEATURES
  if (src->manager->extended_feature_support) {
    __wfd_extended_feature_close(src->manager);
  }
#endif

  g_object_unref (G_OBJECT(src->manager));

#ifdef G_OS_WIN32
  WSACleanup ();
#endif

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


/* a proxy string of the format [user:passwd@]host[:port] */
static gboolean
gst_wfdrtspsrc_set_proxy (GstWFDRTSPSrc * rtsp, const gchar * proxy)
{
  gchar *at = NULL, *col=NULL;
  const char * p = NULL;

  g_free (rtsp->proxy_user);
  rtsp->proxy_user = NULL;
  g_free (rtsp->proxy_passwd);
  rtsp->proxy_passwd = NULL;
  g_free (rtsp->proxy_host);
  rtsp->proxy_host = NULL;
  rtsp->proxy_port = 0;

  p = proxy;
  if (p == NULL)
    return TRUE;

  /* we allow http:// in front but ignore it */
  if (g_str_has_prefix (p, "http://"))
    p += 7;

  at = strchr (p, '@');
  if (at) {
    /* look for user:passwd */
    col = strchr (proxy, ':');
    if (col == NULL || col > at)
      return FALSE;

    rtsp->proxy_user = g_strndup (p, col - p);
    col++;
    rtsp->proxy_passwd = g_strndup (col, at - col);

    /* move to host */
    p = at + 1;
  }
  col = strchr (p, ':');

  if (col) {
    /* everything before the colon is the hostname */
    rtsp->proxy_host = g_strndup (p, col - p);
    p = col + 1;
    rtsp->proxy_port = strtoul (p, (char **) &p, 10);
  } else {
    rtsp->proxy_host = g_strdup (p);
    rtsp->proxy_port = 8080;
  }
  return TRUE;
}

static void
gst_wfdrtspsrc_set_tcp_timeout (GstWFDRTSPSrc * src, guint64 timeout)
{
  src->tcp_timeout.tv_sec = timeout / G_USEC_PER_SEC;
  src->tcp_timeout.tv_usec = timeout % G_USEC_PER_SEC;

  if (timeout != 0)
    src->ptcp_timeout = &src->tcp_timeout;
  else
    src->ptcp_timeout = NULL;
}

static void
gst_wfdrtspsrc_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstWFDRTSPSrc *src = GST_WFDRTSPSRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
      gst_wfdrtspsrc_uri_set_uri (GST_URI_HANDLER (src),
          g_value_get_string (value));
      break;
    case PROP_DEBUG:
      src->debug = g_value_get_boolean (value);
      break;
    case PROP_RETRY:
      src->retry = g_value_get_uint (value);
      break;
    case PROP_TCP_TIMEOUT:
      gst_wfdrtspsrc_set_tcp_timeout (src, g_value_get_uint64 (value));
      break;
    case PROP_NAT_METHOD:
      src->nat_method = g_value_get_enum (value);
      break;
    case PROP_PROXY:
      gst_wfdrtspsrc_set_proxy (src, g_value_get_string (value));
      break;
    case PROP_RTP_BLOCKSIZE:
      src->rtp_blocksize = g_value_get_uint (value);
      break;
    case PROP_USER_ID:
      if (src->user_id)
        g_free (src->user_id);
      src->user_id = g_value_dup_string (value);
      break;
    case PROP_USER_PW:
      if (src->user_pw)
        g_free (src->user_pw);
      src->user_pw = g_value_dup_string (value);
      break;
    case PROP_USER_AGENT:
      if (src->user_agent)
        g_free(src->user_agent);
      src->user_agent = g_value_dup_string (value);
      break;
    case PROP_AUDIO_PARAM:
    {
      const GstStructure *s = gst_value_get_structure (value);
      if (src->audio_param) {
        gst_structure_free (src->audio_param);
      }
      if (s)
        src->audio_param = gst_structure_copy (s);
      else
        src->audio_param = NULL;
      break;
    }
    case PROP_VIDEO_PARAM:
    {
      const GstStructure *s = gst_value_get_structure (value);
      if (src->video_param) {
        gst_structure_free (src->video_param);
      }
      if (s)
        src->video_param = gst_structure_copy (s);
      else
        src->video_param = NULL;
      break;
    }
    case PROP_HDCP_PARAM:
    {
      const GstStructure *s = gst_value_get_structure (value);
      if (src->hdcp_param) {
        gst_structure_free (src->hdcp_param);
	   src->hdcp_param = NULL;
      }
      if (s)
        src->hdcp_param = gst_structure_copy (s);
      else
        src->hdcp_param = NULL;
      break;
    }
    case PROP_DO_RTCP:
      g_object_set_property (G_OBJECT (src->manager), "do-rtcp", value);
      break;
    case PROP_LATENCY:
      g_object_set_property (G_OBJECT (src->manager), "latency", value);
      break;
    case PROP_UDP_BUFFER_SIZE:
      g_object_set_property (G_OBJECT (src->manager), "udp-buffer-size", value);
      break;
    case PROP_UDP_TIMEOUT:
      g_object_set_property (G_OBJECT (src->manager), "timeout", value);
      break;
#ifdef ENABLE_WFD_SINK_UIBC
    case PROP_USER_INPUT_EVENT:
    {
      void *user_event = NULL;
      char *wbuf = NULL;
      gint result= -1;
      user_event = g_value_get_pointer(value);
      if (user_event) {
        WFDUIBCMessage * msg = (WFDUIBCMessage *)user_event;

        WFD_UIBC_MESSAGE_DUMP(msg);
        WFD_UIBC_MESSAGE_AS_TEXT(msg, wbuf);
        if(wbuf) {
          if(src->uibc->sockfd > 0)
          {
            result = write(src->uibc->sockfd, wbuf, msg->length);
            if(result == -1)
              GST_ERROR_OBJECT (src, "Failed to send uibc message.");
/*            else
              GST_DEBUG_OBJECT (src, "Sent a uibc message [socket writing result:%d, message size : %d, msg length : %d]" ,
                result, sizeof(wbuf), msg->length);*/
          }
          free(wbuf);
        }
      }
      break;
    }
    case PROP_UIBC_STATE_CB:
    {
      void ** params = NULL;
      params = (void **)g_value_get_pointer(value);
      src->uibc->wfd_sink_handle = (void *)params[1];
      src->uibc->state_cb = (wfd_uibc_state_cb)params[0];
      src->uibc->is_enabled = TRUE;
      break;
    }
#endif
#ifdef ENABLE_WFD_EXTENDED_FEATURES
    case PROP_DEVICE_NAME:
      g_object_set_property (G_OBJECT (src->manager), "device-name", value);
      break;
    case PROP_BUF_ANIMATION:
      g_object_set_property (G_OBJECT (src->manager), "buffering-animation", value);
      break;
    case PROP_DO_REQUEST:
      g_object_set_property (G_OBJECT (src->manager), "do-request", value);
      break;
    case PROP_DEMUX_HANDLE:
      g_object_set_property (G_OBJECT (src->manager), "demux-handle", value);
      break;
    case PROP_AUDIO_QUEUE_HANDLE:
      g_object_set_property (G_OBJECT (src->manager), "audio-queue-handle", value);
      break;
#endif
    case PROP_ENABLE_PAD_PROBE:
    {
      g_object_set_property (G_OBJECT (src->manager), "enable-pad-probe", value);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wfdrtspsrc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstWFDRTSPSrc *src = GST_WFDRTSPSRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, src->conninfo.location);
      break;
    case PROP_DEBUG:
      g_value_set_boolean (value, src->debug);
      break;
    case PROP_RETRY:
      g_value_set_uint (value, src->retry);
      break;
    case PROP_TCP_TIMEOUT:
    {
      guint64 timeout;

      timeout = src->tcp_timeout.tv_sec * (guint64)G_USEC_PER_SEC +
          src->tcp_timeout.tv_usec;
      g_value_set_uint64 (value, timeout);
      break;
    }
    case PROP_NAT_METHOD:
      g_value_set_enum (value, src->nat_method);
      break;
    case PROP_PROXY:
    {
      gchar *str;

      if (src->proxy_host) {
        str =
            g_strdup_printf ("%s:%d", src->proxy_host, src->proxy_port);
      } else {
        str = NULL;
      }
      g_value_take_string (value, str);
      break;
    }
    case PROP_RTP_BLOCKSIZE:
      g_value_set_uint (value, src->rtp_blocksize);
      break;
    case PROP_USER_ID:
      g_value_set_string (value, src->user_id);
      break;
    case PROP_USER_PW:
      g_value_set_string (value, src->user_pw);
      break;
    case PROP_USER_AGENT:
      g_value_set_string (value, src->user_agent);
      break;
    case PROP_AUDIO_PARAM:
      gst_value_set_structure (value, src->audio_param);
      break;
    case PROP_VIDEO_PARAM:
      gst_value_set_structure (value, src->video_param);
      break;
    case PROP_HDCP_PARAM:
      gst_value_set_structure (value, src->hdcp_param);
      break;
    case PROP_DO_RTCP:
      g_object_get_property (G_OBJECT (src->manager), "do-rtcp", value);
      break;
    case PROP_LATENCY:
      g_object_get_property (G_OBJECT (src->manager), "latency", value);
      break;
    case PROP_UDP_BUFFER_SIZE:
      g_object_get_property (G_OBJECT (src->manager), "udp-buffer-size", value);
      break;
    case PROP_UDP_TIMEOUT:
      g_object_get_property (G_OBJECT (src->manager), "timeout", value);
      break;
#ifdef ENABLE_WFD_EXTENDED_FEATURES
    case PROP_DEVICE_NAME:
      g_object_get_property (G_OBJECT (src->manager), "device-name", value);
      break;
    case PROP_BUF_ANIMATION:
      g_object_get_property (G_OBJECT (src->manager), "buffering-animation", value);
      break;
    case PROP_DO_REQUEST:
      g_object_get_property (G_OBJECT (src->manager), "do-request", value);
      break;
    case PROP_DEMUX_HANDLE:
      g_object_get_property (G_OBJECT (src->manager), "demux-handle", value);
     break;
    case PROP_AUDIO_QUEUE_HANDLE:
      g_object_get_property (G_OBJECT (src->manager), "audio-queue-handle", value);
      break;
#endif
    case PROP_ENABLE_PAD_PROBE:
      g_object_get_property (G_OBJECT (src->manager), "enable-pad-probe", value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wfdrtspsrc_cleanup (GstWFDRTSPSrc * src)
{
  GST_DEBUG_OBJECT (src, "cleanup");

#ifdef ENABLE_WFD_SINK_UIBC
  if (src->uibc && (src->uibc->sockfd > 0)) {
    src->uibc->state_cb( 0, NULL, 0, src->video_width, src->video_height, src->uibc->wfd_sink_handle);
    shutdown (src->uibc->sockfd, SHUT_RDWR);
    src->uibc->sockfd = -1;
  }

  if(src->uibc)
    g_free (src->uibc);
  src->uibc = NULL;
#endif

#ifdef ENABLE_WFD_EXTENDED_FEATURES
  // Added closing rtcp fb socket code and log for the safety.
  if(src->manager && src->manager->rtcp_fb_socketfd != -1) {
    close (src->manager->rtcp_fb_socketfd);
    src->manager->rtcp_fb_socketfd = -1;
    GST_ERROR_OBJECT (src, "no-error:rtcp feedback socket close(port19121)");
  }
#endif

}


static GstRTSPResult
gst_wfdrtspsrc_connection_send (GstWFDRTSPSrc * src, GstRTSPConnection * conn,
    GstRTSPMessage * message, GTimeVal * timeout)
{
  GstRTSPResult ret;

  if (conn)
    ret = gst_rtsp_connection_send (conn, message, timeout);
  else
    ret = GST_RTSP_ERROR;

  return ret;
}

static GstRTSPResult
gst_wfdrtspsrc_connection_receive (GstWFDRTSPSrc * src, GstRTSPConnection * conn,
    GstRTSPMessage * message, GTimeVal * timeout)
{
  GstRTSPResult ret;

  if (conn)
    ret = gst_rtsp_connection_receive (conn, message, timeout);
  else
    ret = GST_RTSP_ERROR;

  return ret;
}

static GstRTSPResult
gst_wfdrtspsrc_prepare_set_param (GstWFDRTSPSrc * src)
{
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPResult res = GST_RTSP_OK;
  WFDResult wfd_res = WFD_OK;
  WFDMessage *msg_no= NULL;
  GstWFDParam param = WFD_PARAM_NONE;
  guint msglen = 0;
  GString *msglength = NULL;
  gchar *msg = NULL;

  if (src->wfd_param == WFD_PARAM_NONE) {
    GST_ERROR_OBJECT (src, "no wfd messag to be sent...");
    return GST_RTSP_EINVAL;
  }

  param = src->wfd_param;
  src->wfd_param = WFD_PARAM_NONE;

  res = gst_rtsp_message_init_request (&request, GST_RTSP_SET_PARAMETER,
    src->conninfo.url_str);
  if (res < 0)
    goto error;

  /* Create set_parameter body to be sent in the request */
  WFDCONFIG_MESSAGE_NEW (&msg_no, wfd_res);

  if (wfd_res != WFD_OK) {
    GST_ERROR_OBJECT (src, "Failed to create wfd message...");
    res = GST_RTSP_ERROR;
    goto error;
  }

  WFDCONFIG_MESSAGE_INIT(msg_no, wfd_res);
  if (wfd_res != WFD_OK) {
    GST_ERROR_OBJECT (src, "Failed to init wfd message...");
    res = GST_RTSP_ERROR;
    goto error;
  }

  switch(param) {

    case WFD_ROUTE: {
      WFDCONFIG_SET_AUDIO_SINK_TYPE(msg_no, WFD_SECONDARY_SINK, wfd_res);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (src, "Failed to set audio sink type...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      break;
    }

    case WFD_CONNECTOR_TYPE: {
      WFDCONFIG_SET_CONNECTOR_TYPE(msg_no, WFD_CONNECTOR_VGA, wfd_res);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (src, "Failed to set connector type...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      break;
    }

    case WFD_STANDBY: {
      WFDCONFIG_SET_STANDBY(msg_no, TRUE, wfd_res);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (src, "Failed to set standby...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      break;
    }

    case WFD_IDR_REQUEST: {
      WFDCONFIG_SET_IDR_REQUESTER(msg_no, wfd_res);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (src, "Failed to set IDR request...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      break;
    }

    case WFD_UIBC_CAPABILITY: {
      WFDCONFIG_SET_UIBC_CAPABILITY(msg_no, WFD_UIBC_INPUT_CAT_UNKNOWN, 0, NULL, 0, 0, wfd_res);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (src, "Failed to set UIBC capability...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      break;
    }

    case WFD_UIBC_SETTING: {
      WFDCONFIG_SET_UIBC_STATUS(msg_no, TRUE, wfd_res);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (src, "Failed to set UIBC setting...");
        res = GST_RTSP_ERROR;
        goto error;
      }
      break;
    }

    default:
      GST_ERROR_OBJECT (src, "Unhandled WFD message type...");
      goto error;
      break;
  }

  WFDCONFIG_MESSAGE_DUMP(msg_no, wfd_res);
  if (wfd_res != WFD_OK) {
    GST_ERROR_OBJECT (src, "Failed to dump wfd message...");
    res = GST_RTSP_ERROR;
    goto error;
  }

  WFDCONFIG_MESSAGE_AS_TEXT(msg_no, msg);
  if (msg == NULL) {
    GST_ERROR_OBJECT (src, "Failed to get wfd message as text...");
    res = GST_RTSP_ERROR;
    goto error;
  }

  msglen = strlen(msg);
  msglength = g_string_new ("");
  g_string_append_printf (msglength,"%d",msglen);

  res = gst_rtsp_message_add_header (&request, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);

  GST_DEBUG ("WFD message body: %s", msg);
  /* add content-length type */
  res = gst_rtsp_message_add_header (&request, GST_RTSP_HDR_CONTENT_LENGTH, g_string_free (msglength, FALSE));
  if (res != GST_RTSP_OK) {
    GST_ERROR_OBJECT (src, "Failed to add header to rtsp request...");
    goto error;
  }

  /* adding wfdconfig data to request */
  res = gst_rtsp_message_set_body (&request,(guint8 *)msg, msglen);
  if (res != GST_RTSP_OK) {
    GST_ERROR_OBJECT (src, "Failed to set body to rtsp request...");
    goto error;
  }

  WFDCONFIG_MESSAGE_FREE(msg_no);

  /* send request message  */
  GST_DEBUG_OBJECT (src, "send reuest...");
  if (res = gst_wfdrtspsrc_send (src, src->conninfo.connection, &request, &response,
      NULL) < 0)
    goto error;

  if (src->debug) {
#ifdef ENABLE_WFD_EXTENDED_FEATURES
    if (src->manager->extended_feature_support)
      func_dump(src, &response);
#else
    wfd_rtsp_manager_message_dump(&response);
#endif
  }

  return res;

/* ERRORS */
error:
 {
    if(msg_no)
      WFDCONFIG_MESSAGE_FREE(msg_no);
    gst_rtsp_message_unset (&request);
    gst_rtsp_message_unset (&response);
    return res;
 }
}

static gboolean
gst_wfdrtspsrc_handle_src_event (GstPad * pad, GstEvent * event)
{
  GstWFDRTSPSrc *src;
  gboolean res = TRUE;
  gboolean forward = FALSE;
  const GstStructure *s;
  /*static gdouble elapsed = 0.0;
  static int count = 0;*/

  src = GST_WFDRTSPSRC_CAST (gst_pad_get_parent (pad));
  if(src == NULL)  {
    GST_ERROR_OBJECT (src, "src is NULL.");
    return FALSE;
  }

  //GST_DEBUG_OBJECT (src, "pad %s:%s received event %s",
  //    GST_DEBUG_PAD_NAME (pad), GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_QOS:
    case GST_EVENT_NAVIGATION:
    case GST_EVENT_LATENCY:
      break;
    case GST_EVENT_CUSTOM_UPSTREAM:
      s = gst_event_get_structure (event);
      if (gst_structure_has_name (s, "GstWFDIDRRequest")) {
        /* Send IDR request */
	 src->wfd_param = WFD_IDR_REQUEST;
	 gst_wfdrtspsrc_loop_send_cmd(src, CMD_SET_PARAM);
      }
      break;
    default:
      forward = TRUE;
      break;
  }
  if (forward) {
    GstPad *target;

    if ((target = gst_ghost_pad_get_target (GST_GHOST_PAD_CAST (pad)))) {
      res = gst_pad_send_event (target, event);
      gst_object_unref (target);
    } else {
      gst_event_unref (event);
    }
  } else {
    gst_event_unref (event);
  }
  gst_object_unref (src);

  return res;
}

/* this query is executed on the ghost source pad exposed on manager. */
static gboolean
gst_wfdrtspsrc_handle_src_query (GstPad * pad, GstQuery * query)
{
  GstWFDRTSPSrc *src = NULL;
  gboolean res = FALSE;

  src = GST_WFDRTSPSRC_CAST (gst_pad_get_parent (pad));
  if(src == NULL)
  {
    GST_ERROR_OBJECT (src, "src is NULL.");
    return res;
  }

  //GST_DEBUG_OBJECT (src, "pad %s:%s received query %s",
  //    GST_DEBUG_PAD_NAME (pad), GST_QUERY_TYPE_NAME (query));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
    {
      if (src->conninfo.location == NULL) {
        res = FALSE;
      } else {
        gst_query_set_uri( query, src->conninfo.location);
        res = TRUE;
      }
      break;
    }
    default:
    {
      GstPad *target = gst_ghost_pad_get_target (GST_GHOST_PAD_CAST (pad));

      /* forward the query to the proxy target pad */
      if (target) {
        res = gst_pad_query (target, query);
        gst_object_unref (target);
      }
      break;
    }
  }
  gst_object_unref (src);

  return res;
}


/* send a couple of dummy random packets on the receiver RTP port to the server,
 * this should make a firewall think we initiated the data transfer and
 * hopefully allow packets to go from the sender port to our RTP receiver port */
static gboolean
gst_wfdrtspsrc_send_dummy_packets (GstWFDRTSPSrc * src)
{
  WFDRTSPManager *manager = src->manager;

  if (src->nat_method != GST_WFD_RTSP_NAT_DUMMY)
    return TRUE;

  if (manager) {
    if (manager->fakesrc && manager->udpsink[0]) {
      GST_DEBUG_OBJECT (src, "sending dummy packet to manager %p", manager);
      gst_element_set_state (manager->udpsink[0], GST_STATE_NULL);
      gst_element_set_state (manager->fakesrc, GST_STATE_NULL);
      gst_element_set_state (manager->udpsink[0], GST_STATE_PLAYING);
      gst_element_set_state (manager->fakesrc, GST_STATE_PLAYING);
    }
  }

  return TRUE;
}


static void
gst_wfdrtspsrc_configure_caps (GstWFDRTSPSrc * src)
{
  WFDRTSPManager *manager = src->manager;;
  GstCaps *caps;
  guint64 start, stop;
  gdouble play_speed, play_scale;
  GstStructure *structure;

  GST_DEBUG_OBJECT (src, "configuring manager caps");

  start = 0;
  stop = GST_CLOCK_TIME_NONE;
  play_speed = 1.000;
  play_scale = 1.000;

  if ((caps = manager->caps)) {
    caps = gst_caps_make_writable (caps);
    structure = gst_caps_get_structure (caps, 0);
    /* update caps */
    if (manager->timebase != GST_CLOCK_TIME_NONE)
      gst_structure_set (structure, "clock-base", G_TYPE_UINT,
          (guint) manager->timebase, NULL);
    if (manager->seqbase != GST_CLOCK_TIME_NONE)
      gst_structure_set (structure, "seqnum-base", G_TYPE_UINT,
          (guint) manager->seqbase, NULL);
    gst_structure_set (structure, "npt-start", G_TYPE_UINT64, start, NULL);

    if (stop != GST_CLOCK_TIME_NONE)
      gst_structure_set (structure, "npt-stop", G_TYPE_UINT64, stop, NULL);
    gst_structure_set (structure, "play-speed", G_TYPE_DOUBLE, play_speed, NULL);
    gst_structure_set (structure, "play-scale", G_TYPE_DOUBLE, play_scale, NULL);
    gst_structure_set (structure, "height", G_TYPE_INT, src->video_height, NULL);
    gst_structure_set (structure, "width", G_TYPE_INT, src->video_width, NULL);
    gst_structure_set (structure, "video-framerate", G_TYPE_INT, src->video_framerate, NULL);

    GST_DEBUG_OBJECT(src, "Frame rate : %d", src->video_framerate);

    manager->caps = caps;

#ifdef _SET_FRAMERATE_TO_CAPS
    /* Set caps to udpsrc */
    GstCaps *udp_caps = NULL;
    GstStructure *s = NULL;
    g_object_get(manager->udpsrc[0], "caps", &udp_caps, NULL);
    udp_caps = gst_caps_make_writable (udp_caps);
    s = gst_caps_get_structure (udp_caps, 0);
    gst_structure_set (s, "npt-start", G_TYPE_UINT64, start, NULL);
    gst_structure_set (s, "npt-stop", G_TYPE_UINT64, stop, NULL);
    gst_structure_set (s, "play-speed", G_TYPE_DOUBLE, play_speed, NULL);
    gst_structure_set (s, "play-scale", G_TYPE_DOUBLE, play_scale, NULL);
    gst_structure_set (s, "width", G_TYPE_INT, src->video_width, NULL);
    gst_structure_set (s, "height", G_TYPE_INT, src->video_height, NULL);
    gst_structure_set (s, "video-framerate", G_TYPE_INT, src->video_framerate, NULL);
    g_object_set(manager->udpsrc[0], "caps", udp_caps, NULL);

#ifdef ENABLE_WFD_EXTENDED_FEATURES
    if (manager->udpsrc[2]) {
      GstCaps *udp_caps2 = NULL;
      GstStructure *s2 = NULL;
      g_object_get(manager->udpsrc[2], "caps", &udp_caps2, NULL);
      udp_caps2 = gst_caps_make_writable (udp_caps2);
      s2 = gst_caps_get_structure (udp_caps2, 0);
      gst_structure_set (s2, "npt-start", G_TYPE_UINT64, start, NULL);
      gst_structure_set (s2, "npt-stop", G_TYPE_UINT64, stop, NULL);
      gst_structure_set (s2, "play-speed", G_TYPE_DOUBLE, play_speed, NULL);
      gst_structure_set (s2, "play-scale", G_TYPE_DOUBLE, play_scale, NULL);
      gst_structure_set (s2, "width", G_TYPE_INT, src->video_width, NULL);
      gst_structure_set (s2, "height", G_TYPE_INT, src->video_height, NULL);
      gst_structure_set (s2, "video-framerate", G_TYPE_INT, src->video_framerate, NULL);
      g_object_set(manager->udpsrc[2], "caps", udp_caps2, NULL);
    }
#endif
#endif
  }
  GST_DEBUG_OBJECT (src, "manager %p, caps %" GST_PTR_FORMAT, manager, caps);

  if (manager->srcpad) {
    GST_DEBUG_OBJECT (src, "set caps for srcpad");
    gst_pad_set_caps(manager->srcpad, manager->caps);
  }
}

static gboolean
gst_wfdrtspsrc_stream_push_event (WFDRTSPManager * manager, GstEvent * event, gboolean source)
{
  gboolean res = TRUE;

  if (manager->srcpad == NULL)
    goto done;

  if (source && manager->udpsrc[0]) {
    gst_event_ref (event);
    res = gst_element_send_event (manager->udpsrc[0], event);
  } else if (manager->channelpad[0]) {
    gst_event_ref (event);
    if (GST_PAD_IS_SRC (manager->channelpad[0]))
      res = gst_pad_push_event (manager->channelpad[0], event);
    else
      res = gst_pad_send_event (manager->channelpad[0], event);
  }

  if (source && manager->udpsrc[1]) {
    gst_event_ref (event);
    res &= gst_element_send_event (manager->udpsrc[1], event);
  } else if (manager->channelpad[1]) {
    gst_event_ref (event);
    if (GST_PAD_IS_SRC (manager->channelpad[1]))
      res &= gst_pad_push_event (manager->channelpad[1], event);
    else
      res &= gst_pad_send_event (manager->channelpad[1], event);
  }

  if (source && manager->udpsrc[2]) {
    gst_event_ref (event);
    res &= gst_element_send_event (manager->udpsrc[2], event);
  } else if (manager->channelpad[2]) {
    gst_event_ref (event);
    if (GST_PAD_IS_SRC (manager->channelpad[2]))
      res &= gst_pad_push_event (manager->channelpad[2], event);
    else
      res &= gst_pad_send_event (manager->channelpad[2], event);
  }

done:
  gst_event_unref (event);

  return res;
}

static gboolean
gst_wfdrtspsrc_push_event (GstWFDRTSPSrc * src, GstEvent * event, gboolean source)
{
  gboolean res = TRUE;

  if (src->manager) {
    gst_event_ref (event);
    res = gst_wfdrtspsrc_stream_push_event (src->manager, event, source);
    gst_event_unref (event);
  }

  return res;
}

static GstRTSPResult
gst_wfdrtspsrc_conninfo_connect (GstWFDRTSPSrc * src, GstWFDRTSPConnInfo * info)
{
  GstRTSPResult res= GST_RTSP_OK;
  gint fd = -1;
  gboolean do_keep_alive = TRUE;
  gint keep_alive_idle = 10000; //10sec
  gint keep_alive_cnt = 10; //1sec
  gint keep_alive_interval = 1000; //1sec


  if (info->connection == NULL) {
    if (info->url == NULL) {
      GST_DEBUG_OBJECT (src, "parsing uri (%s)...", info->location);
      if ((res = gst_rtsp_url_parse (info->location, &info->url)) < 0)
        goto parse_error;
    }

    /* create connection */
    GST_DEBUG_OBJECT (src, "creating connection (%s)...", info->location);
    if ((res = gst_rtsp_connection_create (info->url, &info->connection)) < 0)
      goto could_not_create;

    if (info->url_str)
      g_free (info->url_str);
    info->url_str = gst_rtsp_url_get_request_uri (info->url);

    GST_DEBUG_OBJECT (src, "sanitized uri %s", info->url_str);

    if (info->url->transports & GST_RTSP_LOWER_TRANS_HTTP)
      gst_rtsp_connection_set_tunneled (info->connection, TRUE);

    if (src->proxy_host) {
      GST_DEBUG_OBJECT (src, "setting proxy %s:%d", src->proxy_host,
          src->proxy_port);
      gst_rtsp_connection_set_proxy (info->connection, src->proxy_host,
          src->proxy_port);
    }
  }

  if (!info->connected) {
    /* connect */
    GST_DEBUG_OBJECT (src, "connecting (%s)...", info->location);
    int retry = 0;
connect_retry:
    if (src->do_stop) {
      GST_ERROR_OBJECT (src, "stop connecting....");
      return GST_RTSP_EINTR;
    }

    if ((res =
            gst_rtsp_connection_connect (info->connection,
                src->ptcp_timeout)) < 0) {
      if (retry < 50) {
        GST_ERROR_OBJECT(src, "Connection failed... Try again...");
        usleep(100000);
        retry++;
        goto connect_retry;
      } else {
        goto could_not_connect;
      }
    } else {
      GST_INFO_OBJECT(src, "Connection success");
    }

    info->connected = TRUE;
  }

  fd = gst_rtsp_connection_get_readfd(info->connection);
  if (fd>0) {
    GST_DEBUG_OBJECT(src, "set keep alive on rtsp scoket for checking zombie session");
    if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &do_keep_alive, sizeof(do_keep_alive))<0)
      GST_WARNING_OBJECT(src, "failed setsockopt():so_keepalive");
    if(setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &keep_alive_idle, sizeof(keep_alive_idle))<0)
      GST_WARNING_OBJECT(src, "failed setsockopt():so_keepidle");
    if(setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &keep_alive_cnt, sizeof(keep_alive_cnt))<0)
      GST_WARNING_OBJECT(src, "failed setsockopt():so_keepcnt");
    if(setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &keep_alive_interval, sizeof(keep_alive_interval))<0)
       GST_WARNING_OBJECT(src, "failed setsockopt():so_keepintvl");
  }

  return GST_RTSP_OK;

  /* ERRORS */
parse_error:
  {
    GST_ERROR_OBJECT (src, "No valid RTSP URL was provided");
    return res;
  }
could_not_create:
  {
    gchar *str = gst_rtsp_strresult (res);
    GST_ERROR_OBJECT (src, "Could not create connection. (%s)", str);
    g_free (str);
    return res;
  }
could_not_connect:
  {
    gchar *str = gst_rtsp_strresult (res);
    GST_ERROR_OBJECT (src, "Could not connect to server. (%s)", str);
    g_free (str);
    return res;
  }
}

static GstRTSPResult
gst_wfdrtspsrc_conninfo_close (GstWFDRTSPSrc * src, GstWFDRTSPConnInfo * info,
    gboolean free)
{
  g_return_val_if_fail (info, GST_RTSP_EINVAL);

  if (info->connected) {
    GST_DEBUG_OBJECT (src, "closing connection...");
    gst_rtsp_connection_close (info->connection);
    info->connected = FALSE;
  }
  if (free && info->connection) {
    /* free connection */
    GST_DEBUG_OBJECT (src, "freeing connection...");
    gst_rtsp_connection_free (info->connection);
    info->connection = NULL;
  }
  return GST_RTSP_OK;
}

static GstRTSPResult
gst_wfdrtspsrc_conninfo_reconnect (GstWFDRTSPSrc * src, GstWFDRTSPConnInfo * info)
{
  GstRTSPResult res;

  GST_DEBUG_OBJECT (src, "reconnecting connection...");
  gst_wfdrtspsrc_conninfo_close (src, info, FALSE);
  res = gst_wfdrtspsrc_conninfo_connect (src, info);

  return res;
}

static void
gst_wfdrtspsrc_connection_flush (GstWFDRTSPSrc * src, gboolean flush)
{
  GST_DEBUG_OBJECT (src, "set flushing %d", flush);
  if (src->conninfo.connection) {
    GST_DEBUG_OBJECT (src, "connection flush %d", flush);
    gst_rtsp_connection_flush (src->conninfo.connection, flush);
  }

  if (src->manager) {
    WFDRTSPManager *manager = src->manager;
    GST_DEBUG_OBJECT (src, "manager %p flush %d", manager, flush);
    if (manager->conninfo.connection)
      gst_rtsp_connection_flush (manager->conninfo.connection, flush);
  }
  GST_DEBUG_OBJECT (src, "flushing %d is done", flush);
}


static GstRTSPResult
gst_wfdrtspsrc_handle_request (GstWFDRTSPSrc * src, GstRTSPConnection * conn,
    GstRTSPMessage * request)
{
  GstRTSPMessage response = { 0 };
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMethod method;
  const gchar *uristr;
  GstRTSPVersion version;
  GstRTSPMethod options;
  gchar *str;
  guint8 *data;
  guint size;

  GST_DEBUG_OBJECT (src, "got server request message");

  res = gst_wfd_rtsp_ext_list_receive_request (src->extensions, request);

  gst_rtsp_message_parse_request (request, &method, &uristr, &version);

  if (version != GST_RTSP_VERSION_1_0) {
    /* we can only handle 1.0 requests */
    res = GST_RTSP_ENOTIMPL;
  }

  if (src->debug) {
#ifdef ENABLE_WFD_EXTENDED_FEATURES
  if (src->manager->extended_feature_support)
      res =  func_dump(src, request);
#else
    wfd_rtsp_manager_message_dump(request);
#endif
  }

  switch(method) {
    case GST_RTSP_OPTIONS:
    {
      gchar *tmp = NULL;
      char *options_str = NULL;

      options = GST_RTSP_GET_PARAMETER |GST_RTSP_SET_PARAMETER;

      str = gst_rtsp_options_as_text (options);

      tmp = g_strdup (", org.wfa.wfd1.0");

      GST_LOG ("tmp = %s\n\n", tmp);

      options_str = (char *) malloc (strlen(tmp) + strlen (str) + 1);
      if (!options_str) {
        GST_ERROR ("Failed to allocate memory...");
        res = GST_RTSP_ENOMEM;
        goto send_error;
      } else {
        strncpy (options_str, str, strlen (str));
        strncpy (options_str+strlen(str), tmp, strlen (tmp));
        options_str [strlen(tmp) + strlen (str)] = '\0';

        GST_LOG ("\n\noptions_str = %s\n\n", options_str);
      }
      GST_DEBUG_OBJECT (src, "Creating OPTIONS response");

      res = gst_rtsp_message_init_response (&response, GST_RTSP_STS_OK,
          gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);
      if(res < 0) {
        if(options_str)
          g_free (options_str);
        goto send_error;
      }
      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_PUBLIC, options_str);
      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);
      if(options_str)
        g_free (options_str);
      break;
    }

    case GST_RTSP_GET_PARAMETER:
    {
      gchar *msg = NULL;
      guint msglen = 0;
      GString *msglength = NULL;
      WFDMessage *msg3rep = NULL;
      WFDResult wfd_res = WFD_OK;

      GST_DEBUG_OBJECT (src, "Got GET_PARAMETER request");

      res = gst_rtsp_message_init_response (&response, GST_RTSP_STS_OK, gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);
      if(res < 0)
        goto send_error;

      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_CONTENT_TYPE, "text/parameters");
      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);

      gst_rtsp_message_get_body (request, &data, &size);

      /* Check if message is keep-alive if body is empty*/
      if(size==0) {
        res = gst_rtsp_message_init_response (&response, GST_RTSP_STS_OK, "OK",
              request);

		gst_rtsp_message_add_header (&response, GST_RTSP_HDR_CONTENT_TYPE, "text/parameters");
      	gst_rtsp_message_add_header (&response, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);
      	gst_rtsp_message_add_header (&response, GST_RTSP_HDR_CONTENT_LENGTH, "0");
      	
        gst_rtsp_connection_reset_timeout (src->conninfo.connection);
        if (res < 0)
          goto send_error;
        break;
      }

      /* Creating WFD parameters supported to send as response for
        * GET_PARAMETER request from server*/

      WFDCONFIG_MESSAGE_NEW(&msg3rep, wfd_res);
      if(msg3rep == NULL || wfd_res != WFD_OK) {
        res = GST_RTSP_ERROR;
        goto send_error;
      }

      WFDCONFIG_MESSAGE_INIT(msg3rep, wfd_res);
      WFDCONFIG_MESSAGE_PARSE_BUFFER(data, size, msg3rep, wfd_res);
      if(wfd_res != WFD_OK) {
        res = GST_RTSP_ERROR;
        goto send_error;
      }
      WFDCONFIG_MESSAGE_DUMP(msg3rep, wfd_res);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (src, "Failed to dump wfd message...");
      }

      /* Check if the request message has "wfd_audio_codecs" in it */
      if(msg3rep->audio_codecs) {
        guint audio_codec = 0;
        guint audio_sampling_frequency = 0;
        guint audio_channels = 0;
        guint audio_latency = 0;
        //reading audio parameters which are set from sink ini file
        if(src->audio_param != NULL) {
          if (gst_structure_has_field (src->audio_param, "audio_codec"))
            gst_structure_get_uint (src->audio_param, "audio_codec", &audio_codec);
          if (gst_structure_has_field (src->audio_param, "audio_latency"))
            gst_structure_get_uint (src->audio_param, "audio_latency", &audio_latency);
          if (gst_structure_has_field (src->audio_param, "audio_channels"))
            gst_structure_get_uint (src->audio_param, "audio_channels", &audio_channels);
          if (gst_structure_has_field (src->audio_param, "audio_sampling_frequency"))
            gst_structure_get_uint (src->audio_param, "audio_sampling_frequency", &audio_sampling_frequency);
        }

        WFDCONFIG_SET_SUPPORTED_AUDIO_FORMAT(msg3rep, audio_codec,
            audio_sampling_frequency, audio_channels, 16, audio_latency, wfd_res);
      }

      /* Check if the request message has "wfd_video_formats" in it */
      if(msg3rep->video_formats) {
        guint video_codec = 0;
        guint video_native_resolution = 0;
        guint video_cea_support = 0;
        guint video_vesa_support = 0;
        guint video_hh_support = 0;
        guint video_profile = 0;
        guint video_level = 0;
        guint video_latency = 0;
        gint video_vertical_resolution = 0;
        gint video_horizontal_resolution = 0;
        gint video_minimum_slicing = 0;
        gint video_slice_enc_param = 0;
        gint video_framerate_control_support = 0;

        //reading video parameters which are set from sink ini file
        if (src->video_param != NULL) {
          if (gst_structure_has_field (src->video_param, "video_codec"))
            gst_structure_get_uint (src->video_param, "video_codec", &video_codec);
          if (gst_structure_has_field (src->video_param, "video_native_resolution"))
            gst_structure_get_uint (src->video_param, "video_native_resolution", &video_native_resolution);
          if (gst_structure_has_field (src->video_param, "video_cea_support"))
            gst_structure_get_uint (src->video_param, "video_cea_support", &video_cea_support);
          if (gst_structure_has_field (src->video_param, "video_vesa_support"))
            gst_structure_get_uint (src->video_param, "video_vesa_support", &video_vesa_support);
          if (gst_structure_has_field (src->video_param, "video_hh_support"))
            gst_structure_get_uint (src->video_param, "video_hh_support", &video_hh_support);
          if (gst_structure_has_field (src->video_param, "video_profile"))
            gst_structure_get_uint (src->video_param, "video_profile", &video_profile);
          if (gst_structure_has_field (src->video_param, "video_level"))
            gst_structure_get_uint (src->video_param, "video_level", &video_level);
          if (gst_structure_has_field (src->video_param, "video_latency"))
            gst_structure_get_uint (src->video_param, "video_latency", &video_latency);
          if (gst_structure_has_field (src->video_param, "video_vertical_resolution"))
            gst_structure_get_int (src->video_param, "video_vertical_resolution", &video_vertical_resolution);
          if (gst_structure_has_field (src->video_param, "video_horizontal_resolution"))
            gst_structure_get_int (src->video_param, "video_horizontal_resolution", &video_horizontal_resolution);
          if (gst_structure_has_field (src->video_param, "video_minimum_slicing"))
            gst_structure_get_int (src->video_param, "video_minimum_slicing", &video_minimum_slicing);
          if (gst_structure_has_field (src->video_param, "video_slice_enc_param"))
            gst_structure_get_int (src->video_param, "video_slice_enc_param", &video_slice_enc_param);
          if (gst_structure_has_field (src->video_param, "video_framerate_control_support"))
            gst_structure_get_int (src->video_param, "video_framerate_control_support", &video_framerate_control_support);
        }


        WFDCONFIG_SET_SUPPORTED_VIDEO_FORMAT(msg3rep,
            video_codec,
            WFD_VIDEO_CEA_RESOLUTION,
            video_native_resolution,
            video_cea_support,
            video_vesa_support,
            video_hh_support,
            video_profile,
            video_level,
            video_latency,
            video_vertical_resolution,
            video_horizontal_resolution,
            video_minimum_slicing,
            video_slice_enc_param,
            video_framerate_control_support,
            wfd_res);
      }

      /* Check if the request has "wfd_client_rtp_ports" in it */
      if(msg3rep->client_rtp_ports) {
        /* Hardcoded as of now. This is to comply with dongle port settings.
        This should be derived from gst_wfdrtspsrc_alloc_udp_ports */
        src->primary_rtpport = 19000;
        src->secondary_rtpport = 0;
        WFDCONFIG_SET_PREFERD_RTP_PORT(msg3rep,
                     WFD_RTSP_TRANS_RTP,
                     WFD_RTSP_PROFILE_AVP,
                     WFD_RTSP_LOWER_TRANS_UDP,
                     src->primary_rtpport,
                     src->secondary_rtpport,
                     wfd_res);
      }

      if(wfd_res != WFD_OK) {
        res = GST_RTSP_ERROR;
        goto send_error;
      }

      /* Check if the request has "wfd_3d_video_formats " in it */
      if(msg3rep->video_3d_formats) {
        /* TODO: Set preferred 3d_video_formats */
        wfd_res = WFD_OK;
      }

      /* When set preferred 3d_video_formats is done, following block need to be uncommented */
      /*if(wfd_res != WFD_OK) {
      res = GST_RTSP_ERROR;
      goto send_error;
      }*/

      /* Check if the request has "wfd_content_protection " in it */
      if(msg3rep->content_protection) {
        gint hdcp_version = 0;
        gint hdcp_port_no = 0;
        //reading hdcp parameters which are set from sink ini file
        if (src->hdcp_param != NULL) {
          if (gst_structure_has_field (src->hdcp_param, "hdcp_version")) {
            gst_structure_get_int (src->hdcp_param, "hdcp_version", &hdcp_version);
            GST_DEBUG_OBJECT(src, "hdcp version : %d", hdcp_version);
          }

          if (gst_structure_has_field (src->hdcp_param, "hdcp_port_no")) {
            gst_structure_get_int (src->hdcp_param, "hdcp_port_no", &hdcp_port_no);
            GST_DEBUG_OBJECT(src, "hdcp_port_no : %d", hdcp_port_no);
          }
        } else {
            GST_DEBUG_OBJECT(src, "No HDCP");
        }
        WFDCONFIG_SET_CONTPROTECTION_TYPE(msg3rep, hdcp_version, (guint32)hdcp_port_no, wfd_res);
      }

      if(wfd_res != WFD_OK) {
        res = GST_RTSP_ERROR;
        goto send_error;
      }
#ifdef ENABLE_WFD_SINK_UIBC
      if(src->uibc->is_enabled && msg3rep->uibc_capability) {
        src->uibc->sockfd = -1;
        src->uibc->gen_capability = DEFAULT_UIBC_GEN_CAPABILITY;
#if 0
        //Support HIDC for keyboard and mouse.
        src->uibc->hidc_cap_list = (WFDHIDCTypePathPair *)malloc (sizeof(WFDHIDCTypePathPair) * 4);
        src->uibc->hidc_cap_list[0].inp_type = WFD_UIBC_INPUT_TYPE_KEYBOARD;
        src->uibc->hidc_cap_list[0].inp_path = WFD_UIBC_INPUT_PATH_BT;
        src->uibc->hidc_cap_list[1].inp_type = WFD_UIBC_INPUT_TYPE_KEYBOARD;
        src->uibc->hidc_cap_list[1].inp_path = WFD_UIBC_INPUT_PATH_USB;
        src->uibc->hidc_cap_list[2].inp_type = WFD_UIBC_INPUT_TYPE_MOUSE;
        src->uibc->hidc_cap_list[2].inp_path = WFD_UIBC_INPUT_PATH_BT;
        src->uibc->hidc_cap_list[3].inp_type = WFD_UIBC_INPUT_TYPE_MOUSE;
        src->uibc->hidc_cap_list[3].inp_path = WFD_UIBC_INPUT_PATH_USB;
        WFDCONFIG_SET_UIBC_CAPABILITY(msg3rep, WFD_UIBC_INPUT_CAT_GENERIC, src->uibc->gen_capability,
          src->uibc->hidc_cap_list, DEFAULT_UIBC_HIDC_CAP_COUNT, 0, wfd_res);
#endif
        WFDCONFIG_SET_UIBC_CAPABILITY(msg3rep, WFD_UIBC_INPUT_CAT_GENERIC, src->uibc->gen_capability,
          NULL, 0, 0, wfd_res);
        if(wfd_res  != WFD_OK) {
          GST_ERROR("failed to set_uibc_capability.");
          res = GST_RTSP_ERROR;
          goto send_error;
        }
      }
#endif
      /* Check if the request has "wfd_display_edid" in it */
      if(msg3rep->display_edid) {
        /* TODO: Set preferred display_edid */
        wfd_res = WFD_OK;
      }

      /* when set preferred display_edid following block need to be uncommented. */
      /*if(wfd_res != WFD_OK) {
      res = GST_RTSP_ERROR;
      goto send_error;
      }*/

      /* Check if the request has "wfd_coupled_sink" in it */
      if(msg3rep->coupled_sink) {
        //wfd_res = wfdconfig_set_coupled_sink(msg3rep, WFD_SINK_NOT_COUPLED, NULL);
        /* To test with dummy coupled sink address */
        WFDCONFIG_SET_COUPLED_SINK(msg3rep,WFD_SINK_COUPLED,(gchar *)"1.0.0.1:435", wfd_res);
      }

      if(wfd_res != WFD_OK) {
        res = GST_RTSP_ERROR;
        goto send_error;
      }
      WFDCONFIG_MESSAGE_AS_TEXT(msg3rep, msg);

      msglen = strlen(msg);
      msglength = g_string_new ("");
      g_string_append_printf (msglength,"%d",msglen);

      GST_DEBUG_OBJECT (src, "GET_PARAMETER response message body: %s\n\n", msg);

      gst_rtsp_message_set_body (&response, (guint8*) msg, msglen);

      WFDCONFIG_MESSAGE_FREE(msg3rep);

      break;
      
    }

    case GST_RTSP_SET_PARAMETER:
    {
      WFDMessage *msg = NULL;
      WFDResult wfd_res = WFD_OK;

      GST_DEBUG_OBJECT (src, "Got SET_PARAMETER request");

      res = gst_rtsp_message_init_response (&response, GST_RTSP_STS_OK,
            gst_rtsp_status_as_text (GST_RTSP_STS_OK), request);
      if (res < 0)
        goto send_error;

      gst_rtsp_message_add_header (&response, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);

      gst_rtsp_message_get_body (request, &data, &size);
      WFDCONFIG_MESSAGE_NEW(&msg, wfd_res);
      if(msg == NULL || wfd_res != WFD_OK) {
        res = GST_RTSP_ERROR;
        goto send_error;
      }

      WFDCONFIG_MESSAGE_INIT(msg, wfd_res);
      if(wfd_res != WFD_OK) {
        res = GST_RTSP_ERROR;
        goto send_error;
      }
      WFDCONFIG_MESSAGE_PARSE_BUFFER(data, size, msg, wfd_res);
      if(wfd_res != WFD_OK) {
        res = GST_RTSP_ERROR;
        goto send_error;
      }

      WFDCONFIG_MESSAGE_DUMP(msg, wfd_res);
      if (wfd_res != WFD_OK) {
        GST_ERROR_OBJECT (src, "Failed to dump wfd message...");
      }
      if (msg->audio_codecs || msg->video_formats || msg->video_3d_formats) {
        GstStructure *stream_info = NULL;
        stream_info = gst_structure_new ("WFDStreamInfo", NULL);

        /* check audio codec */
        if(msg->audio_codecs) {
          gst_wfdrtspsrc_get_audio_parameter(src, msg);

          gst_structure_set (stream_info,
              "audio_format", G_TYPE_STRING, src->audio_format,
              "audio_channels", G_TYPE_INT, src->audio_channels,
              "audio_rate", G_TYPE_INT, src->audio_frequency,
              "audio_bitwidth", G_TYPE_INT, src->audio_bitwidth/16,
              NULL);
        }

        /* check video formats  */
        if(msg->video_formats) {
          gst_wfdrtspsrc_get_video_parameter(src, msg);

          gst_structure_set (stream_info,
              "video_format", G_TYPE_STRING, "H264",
              "video_width", G_TYPE_INT, src->video_width,
              "video_height", G_TYPE_INT, src->video_height,
              "video_framerate", G_TYPE_INT, src->video_framerate,
              NULL);
        }

        /* check video formats  */
        if(msg->video_3d_formats) {
          guint count = 0;
          count = msg->video_3d_formats->count;
          GST_DEBUG_OBJECT (src, "got video 3d formats with %d count.", count);
        }

        g_signal_emit (src, gst_wfdrtspsrc_signals[SIGNAL_UPDATE_MEDIA_INFO], 0, stream_info);
      }

      /* check av format change timing  */
      if (msg->av_format_change_timing) {
        if (src->state != GST_RTSP_STATE_PLAYING) {
          GST_WARNING_OBJECT(src, "could not handle wfd_av_format_change_timing before session is established");
          break;
        } else {
          guint64 pts=0LL, dts=0LL;
          gboolean need_to_flush = FALSE;

          pts = msg->av_format_change_timing->PTS;
          dts = msg->av_format_change_timing->DTS;

          GST_DEBUG_OBJECT(src, "change format with PTS[%lld] and DTS[%lld]", pts, dts);

          g_signal_emit (src, gst_wfdrtspsrc_signals[SIGNAL_AV_FORMAT_CHANGE], 0, (gpointer)&need_to_flush);

          if (need_to_flush) {
#ifdef ENABLE_WFD_EXTENDED_FEATURES
            if (src->manager->extended_feature_support) {
              void (*func)(WFDRTSPManager *, gboolean)
                       = __wfd_extended_func(src->manager, WFD_EXTENDED_FLUSH);
              func(src->manager, TRUE);
              func(src->manager, FALSE);
            }
#else
              wfd_rtsp_manager_flush(src->manager, TRUE);
              wfd_rtsp_manager_flush(src->manager, FALSE);
#endif
          }
        }
      }

      if(msg->presentation_url) {
        gchar *url0, *url1;

        WFDCONFIG_GET_PRESENTATION_URL(msg, &url0, &url1, wfd_res);
        if(wfd_res == WFD_OK) {
          g_free (src->conninfo.location);
          src->conninfo.location = g_strdup (url0);
          /* url1 is ignored as of now */
        } else {
          res = GST_RTSP_ERROR;
          goto send_error;
        }
      }

      if(msg->standby) {
        gboolean standby_enable = FALSE;
        WFDCONFIG_GET_STANDBY(msg, &standby_enable, wfd_res);
        if(wfd_res == WFD_OK)
          GST_DEBUG("M12 server set param request STANDBY %s", standby_enable?"ENABLE":"DISABLE");
      }

#ifdef ENABLE_WFD_SINK_UIBC
      if(msg->uibc_capability) {
        guint32 input_category;
        guint32 inp_type;

        WFDCONFIG_GET_UIBC_CAPABILITY(msg, &input_category, &inp_type, &src->uibc->hidc_cap_list_src, &src->uibc->hidc_cap_count_src, &src->uibc->port, wfd_res);
        GST_DEBUG ("uibc_port read is %u", src->uibc->port);
        if(wfd_res == WFD_OK) {
          GST_DEBUG("M14 server set param UIBC input category: %s %s", input_category&WFD_UIBC_INPUT_CAT_GENERIC?"GENERIC":"", input_category&WFD_UIBC_INPUT_CAT_HIDC?"HIDC":"");
          GST_DEBUG("UIBC input type: %s %s %s %s %s %s %s %s", inp_type&WFD_UIBC_INPUT_TYPE_KEYBOARD?"KEYBOARD":"", inp_type&WFD_UIBC_INPUT_TYPE_MOUSE?"MOUSE":"",
           inp_type&WFD_UIBC_INPUT_TYPE_SINGLETOUCH?"SINGLETOUCH":"", inp_type&WFD_UIBC_INPUT_TYPE_MULTITOUCH?"MULTITOUCH":"",
           inp_type&WFD_UIBC_INPUT_TYPE_JOYSTICK?"JOYSTICK":"", inp_type&WFD_UIBC_INPUT_TYPE_CAMERA?"CAMERA":"",
           inp_type&WFD_UIBC_INPUT_TYPE_GESTURE?"GESTURE":"", inp_type&WFD_UIBC_INPUT_TYPE_REMOTECONTROL?"REMOTECONTROL":"");
        }
      }

      if(msg->uibc_setting) {
        gboolean uibc_enable = FALSE;
        WFDCONFIG_GET_UIBC_STATUS(msg, &uibc_enable, wfd_res);
        if(wfd_res == WFD_OK) {
          if(uibc_enable == TRUE) {
            GST_DEBUG ("uibc_enable is TRUE");
            uibc_new_tcp (src);
          } else {
            src->uibc->state_cb (0, NULL, 0, src->video_width, src->video_height, src->uibc->wfd_sink_handle);
            shutdown (src->uibc->sockfd, SHUT_RDWR);
            src->uibc->sockfd = -1;
          }
        }
      }
#endif

#ifdef ENABLE_WFD_EXTENDED_FEATURES
    gboolean check_result = FALSE;
    gboolean need_idr = FALSE;

    if (src->manager->extended_feature_support) {
      GstRTSPResult (*func)(WFDRTSPManager *, guint8 *, GstRTSPMessage *, gboolean *, gboolean *)
               = __wfd_extended_func(src->manager, WFD_EXTENDED_HANDLE_REQUEST);
      res =  func(src->manager, data, request, &check_result, &need_idr);
    }

    if (check_result) {
      if (res != GST_RTSP_OK)
        goto send_error;
      else if (need_idr) {
	 src->wfd_param = WFD_IDR_REQUEST;
        gst_wfdrtspsrc_prepare_set_param (src);
      }

      goto exit;
    }
#endif
      break;
    }

    default:
    {
      res = gst_rtsp_message_init_response (&response, GST_RTSP_STS_OK, "OK",
            request);
      if (res < 0)
        goto send_error;
      break;
    }
  }

  if (src->debug) {
#ifdef ENABLE_WFD_EXTENDED_FEATURES
    if (src->manager->extended_feature_support)
      res = func_dump(src, &response);
#else
      wfd_rtsp_manager_message_dump(&response);
#endif
  }

  res = gst_wfdrtspsrc_connection_send (src, conn, &response, NULL);
  if (res < 0)
    goto send_error;

  /* Handling wfd_trigger_method: PAUSE */
  if(method == GST_RTSP_SET_PARAMETER && g_strrstr((gchar*)data,"PAUSE")) {
    GST_DEBUG_OBJECT (src, "trigger PAUSE \n");
    gst_wfdrtspsrc_loop_send_cmd (src, CMD_PAUSE);
  }

  /* Handling wfd_trigger_method: RESUME */
  if(method == GST_RTSP_SET_PARAMETER && g_strrstr((gchar*)data,"PLAY")) {
    GST_DEBUG_OBJECT (src, "trigger RESUME \n");
    gst_wfdrtspsrc_loop_send_cmd (src, CMD_PLAY);
  }

  /* Handling wfd_trigger_method: TEARDOWN */
  if(method == GST_RTSP_SET_PARAMETER && g_strrstr((gchar*)data,"TEARDOWN")) {
    GstBus *bus;

    GST_DEBUG_OBJECT (src, "trigger TEARDOWN \n");

    gst_wfdrtspsrc_loop_send_cmd (src, CMD_CLOSE);

    bus = gst_element_get_bus(GST_ELEMENT_CAST(src));
    if(!gst_bus_post(bus, gst_message_new_application(GST_OBJECT_CAST(src), gst_structure_empty_new("TEARDOWN")))){
      GST_ERROR_OBJECT(src, "Failed to send TEARDOWN message\n");
    }
    gst_object_unref(bus);

    goto exit;
  }

  /* Handling wfd_trigger_method: setup */
  if(method == GST_RTSP_SET_PARAMETER && g_strrstr((gchar*)data,"SETUP")) {
    src->state = GST_RTSP_STATE_INIT;

    /* setup streams */
    if (!gst_wfdrtspsrc_setup (src))
      goto setup_failed;

    src->state = GST_RTSP_STATE_READY;
  }

exit:
  gst_rtsp_message_unset (request);
  gst_rtsp_message_unset (&response);

  return GST_RTSP_OK;

  /* ERRORS */
setup_failed:
  {
    gst_rtsp_message_unset (request);
    gst_rtsp_message_unset (&response);
    return GST_RTSP_ERROR;
  }
send_error:
  {
    gst_rtsp_message_unset (request);
    gst_rtsp_message_unset (&response);
    return res;
  }
}

/* send server keep-alive */
static GstRTSPResult
gst_wfdrtspsrc_send_keep_alive (GstWFDRTSPSrc * src)
{
  GstRTSPMessage request = { 0 };
  GstRTSPResult res;
  GstRTSPMethod method;
  gchar *control;

  GST_INFO_OBJECT (src, "creating server keep-alive");

  /* find a method to use for keep-alive */
  if (src->methods & GST_RTSP_GET_PARAMETER)
    method = GST_RTSP_GET_PARAMETER;
  else
    method = GST_RTSP_OPTIONS;

  control = src->conninfo.url_str;

  if (control == NULL)
    goto no_control;

  res = gst_rtsp_message_init_request (&request, method, control);
  if (res < 0)
    goto send_error;
#ifdef ENABLE_WFD_EXTENDED_FEATURES
  if (src->manager->extended_feature_support) {
    if (src->debug && request.body != NULL) {
      res =  func_dump(src, &request);
    }
  }
#else
  if (src->debug && request.body != NULL) {
    wfd_rtsp_manager_message_dump(&request);
  }
#endif

  res =
      gst_wfdrtspsrc_connection_send (src, src->conninfo.connection, &request,
      NULL);
  if (res < 0)
    goto send_error;

  gst_rtsp_connection_reset_timeout (src->conninfo.connection);
  gst_rtsp_message_unset (&request);

  return GST_RTSP_OK;

  /* ERRORS */
no_control:
  {
    GST_WARNING_OBJECT (src, "no control url to send keepalive");
    return GST_RTSP_OK;
  }
send_error:
  {
    gchar *str = gst_rtsp_strresult (res);

    gst_rtsp_message_unset (&request);
    GST_ELEMENT_WARNING (src, RESOURCE, WRITE, (NULL),
        ("Could not send keep-alive. (%s)", str));
    g_free (str);
    return res;
  }
}

static void
gst_wfdrtspsrc_loop_start_cmd (GstWFDRTSPSrc * src, gint cmd)
{
  GST_DEBUG_OBJECT (src, "start cmd %d", cmd);

  switch (cmd) {
    case CMD_OPEN:
      GST_ELEMENT_PROGRESS (src, START, "open", ("Opening Stream"));
      break;
    case CMD_PLAY:
      GST_ELEMENT_PROGRESS (src, START, "play", ("Sending PLAY request"));
      break;
    case CMD_PAUSE:
      GST_ELEMENT_PROGRESS (src, START, "pause", ("Sending PAUSE request"));
      break;
    case CMD_CLOSE:
      GST_ELEMENT_PROGRESS (src, START, "close", ("Closing Stream"));
      break;
    default:
      break;
  }
}

static void
gst_wfdrtspsrc_loop_complete_cmd (GstWFDRTSPSrc * src, gint cmd)
{
  GST_DEBUG_OBJECT (src, "complete cmd %d", cmd);

  switch (cmd) {
    case CMD_OPEN:
      GST_ELEMENT_PROGRESS (src, COMPLETE, "open", ("Opened Stream"));
      break;
    case CMD_PLAY:
      GST_ELEMENT_PROGRESS (src, COMPLETE, "play", ("Sent PLAY request"));
      break;
    case CMD_PAUSE:
      GST_ELEMENT_PROGRESS (src, COMPLETE, "pause", ("Sent PAUSE request"));
      break;
    case CMD_CLOSE:
      GST_ELEMENT_PROGRESS (src, COMPLETE, "close", ("Closed Stream"));
      break;
    default:
      break;
  }
}

static void
gst_wfdrtspsrc_loop_cancel_cmd (GstWFDRTSPSrc * src, gint cmd)
{
  GST_DEBUG_OBJECT (src, "cancel cmd %d", cmd);

  switch (cmd) {
    case CMD_OPEN:
      GST_ELEMENT_PROGRESS (src, CANCELED, "open", ("Open canceled"));
      break;
    case CMD_PLAY:
      GST_ELEMENT_PROGRESS (src, CANCELED, "play", ("PLAY canceled"));
      break;
    case CMD_PAUSE:
      GST_ELEMENT_PROGRESS (src, CANCELED, "pause", ("PAUSE canceled"));
      break;
    case CMD_CLOSE:
      GST_ELEMENT_PROGRESS (src, CANCELED, "close", ("Close canceled"));
      break;
    default:
      break;
  }
}

static void
gst_wfdrtspsrc_loop_error_cmd (GstWFDRTSPSrc * src, gint cmd)
{
  GST_DEBUG_OBJECT (src, "error cmd %d", cmd);

  switch (cmd) {
    case CMD_OPEN:
      GST_ELEMENT_PROGRESS (src, ERROR, "open", ("Open failed"));
      break;
    case CMD_PLAY:
      GST_ELEMENT_PROGRESS (src, ERROR, "play", ("PLAY failed"));
      break;
    case CMD_PAUSE:
      GST_ELEMENT_PROGRESS (src, ERROR, "pause", ("PAUSE failed"));
      break;
    case CMD_CLOSE:
      GST_ELEMENT_PROGRESS (src, ERROR, "close", ("Close failed"));
      break;
    default:
      break;
  }
}

static void
gst_wfdrtspsrc_loop_end_cmd (GstWFDRTSPSrc * src, gint cmd, GstRTSPResult ret)
{
  GST_DEBUG_OBJECT (src, "end cmd %d", cmd);

  if (ret == GST_RTSP_OK)
    gst_wfdrtspsrc_loop_complete_cmd (src, cmd);
  else if (ret == GST_RTSP_EINTR)
    gst_wfdrtspsrc_loop_cancel_cmd (src, cmd);
  else
    gst_wfdrtspsrc_loop_error_cmd (src, cmd);
}

static void
gst_wfdrtspsrc_loop_send_cmd (GstWFDRTSPSrc * src, gint cmd)
{
  gint old;

  /* start new request */
  gst_wfdrtspsrc_loop_start_cmd (src, cmd);

  GST_OBJECT_LOCK (src);

  GST_DEBUG_OBJECT (src, "sending cmd %d", cmd);

  old = src->loop_cmd;
  if (old != CMD_WAIT) {
    src->loop_cmd = CMD_WAIT;
    GST_OBJECT_UNLOCK (src);
    /* cancel previous request */
    gst_wfdrtspsrc_loop_cancel_cmd (src, old);
    GST_OBJECT_LOCK (src);
  }
  src->loop_cmd = cmd;

  /* interrupt if allowed */
  if (src->waiting) {
    GST_DEBUG_OBJECT (src, "start connection flush");
    gst_wfdrtspsrc_connection_flush (src, TRUE);
  }

  if (src->task)
    gst_task_start (src->task);

  GST_OBJECT_UNLOCK (src);
}

static GstFlowReturn
gst_wfdrtspsrc_loop_udp (GstWFDRTSPSrc * src)
{
  GstRTSPResult res;
  GstRTSPMessage message = { 0 };
  gint retry = 0;

  while (TRUE) {
    GTimeVal tv_timeout;

    /* get the next timeout interval */
    gst_rtsp_connection_next_timeout (src->conninfo.connection, &tv_timeout);

    GST_DEBUG_OBJECT (src, "doing receive with timeout %d seconds",
        (gint) tv_timeout.tv_sec);

    gst_rtsp_message_unset (&message);

    /* we should continue reading the TCP socket because the server might
     * send us requests. When the session timeout expires, we need to send a
     * keep-alive request to keep the session open. */
    res = gst_wfdrtspsrc_connection_receive (src, src->conninfo.connection,
        &message, &tv_timeout);

    switch (res) {
      case GST_RTSP_OK:
        GST_DEBUG_OBJECT (src, "we received a server message");
        break;
      case GST_RTSP_EINTR:
        /* we got interrupted, see what we have to do */
        goto interrupt;
      case GST_RTSP_ETIMEOUT:
        /* send keep-alive, ignore the result, a warning will be posted. */
        GST_DEBUG_OBJECT (src, "timeout, sending keep-alive");
        if ((res = gst_wfdrtspsrc_send_keep_alive (src)) == GST_RTSP_EINTR)
          goto interrupt;
	break;
      case GST_RTSP_EEOF:
        /* server closed the connection. not very fatal for UDP, reconnect and
         * see what happens. */
        GST_ELEMENT_WARNING (src, RESOURCE, READ, (NULL),
            ("The server closed the connection."));
        goto connect_error;
      default:
        goto receive_error;
    }

    switch (message.type) {
      case GST_RTSP_MESSAGE_REQUEST:
        /* server sends us a request message, handle it */
        res =
            gst_wfdrtspsrc_handle_request (src, src->conninfo.connection,
            &message);
        if (res == GST_RTSP_EEOF)
          goto server_eof;
        else if (res < 0)
          goto handle_request_failed;
        break;
      case GST_RTSP_MESSAGE_RESPONSE:
        /* we ignore response and data messages */
        GST_DEBUG_OBJECT (src, "ignoring response message");

      if (src->debug) {
#ifdef ENABLE_WFD_EXTENDED_FEATURES
        if (src->manager->extended_feature_support)
          res =  func_dump(src, &message);
#else
          wfd_rtsp_manager_message_dump(&message);
#endif
        }

        if (message.type_data.response.code == GST_RTSP_STS_UNAUTHORIZED) {
          GST_DEBUG_OBJECT (src, "but is Unauthorized response ...");
          if (gst_wfdrtspsrc_setup_auth (src, &message) && !(retry++)) {
            GST_DEBUG_OBJECT (src, "so retrying keep-alive");
            if ((res = gst_wfdrtspsrc_send_keep_alive (src)) == GST_RTSP_EINTR)
              goto interrupt;
          }
        } else {
          retry = 0;
        }
        break;
      case GST_RTSP_MESSAGE_DATA:
        /* we ignore response and data messages */
        GST_DEBUG_OBJECT (src, "ignoring data message");
        break;
      default:
        GST_WARNING_OBJECT (src, "ignoring unknown message type %d",
            message.type);
        break;
    }
  }

  /* we get here when the connection got interrupted */
interrupt:
  {
    gst_rtsp_message_unset (&message);
    GST_DEBUG_OBJECT (src, "got interrupted");
    return GST_FLOW_WRONG_STATE;
  }
connect_error:
  {
    gchar *str = gst_rtsp_strresult (res);
    GstFlowReturn ret;

    src->conninfo.connected = FALSE;
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ_WRITE, (NULL),
        ("Could not connect to server. (%s)", str));
    g_free (str);
    ret = GST_FLOW_ERROR;

    return ret;
  }
receive_error:
  {
    gchar *str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Could not receive message. (%s)", str));
    g_free (str);
    return GST_FLOW_ERROR;
  }
handle_request_failed:
  {
    gchar *str = gst_rtsp_strresult (res);
    GstFlowReturn ret;

    gst_rtsp_message_unset (&message);
    if (res != GST_RTSP_EINTR) {
      GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
          ("Could not handle server message. (%s)", str));
      g_free (str);
      ret = GST_FLOW_ERROR;
    } else {
      ret = GST_FLOW_WRONG_STATE;
    }
    return ret;
  }
server_eof:
  {
    GST_DEBUG_OBJECT (src, "we got an eof from the server");
    GST_ELEMENT_WARNING (src, RESOURCE, READ, (NULL),
        ("The server closed the connection."));
    src->conninfo.connected = FALSE;
    gst_rtsp_message_unset (&message);
    return GST_FLOW_UNEXPECTED;
  }
}

static gboolean
gst_wfdrtspsrc_loop (GstWFDRTSPSrc * src)
{
  GstFlowReturn ret;

  if (!src->conninfo.connection || !src->conninfo.connected)
    goto no_connection;

  ret = gst_wfdrtspsrc_loop_udp (src);

  if (ret != GST_FLOW_OK)
    goto pause;

  return TRUE;

  /* ERRORS */
no_connection:
  {
    GST_ERROR_OBJECT (src, "we are not connected");
    ret = GST_FLOW_WRONG_STATE;
    goto pause;
  }
pause:
  {
    const gchar *reason = gst_flow_get_name (ret);

    GST_DEBUG_OBJECT (src, "pausing task, reason %s", reason);
    if (ret == GST_FLOW_UNEXPECTED) {
      /* perform EOS logic */
      gst_wfdrtspsrc_push_event (src, gst_event_new_eos (), FALSE);
    } else if (ret == GST_FLOW_NOT_LINKED || ret < GST_FLOW_UNEXPECTED) {
      /* for fatal errors we post an error message, post the error before the
       * EOS so the app knows about the error first. */
      GST_ELEMENT_ERROR (src, STREAM, FAILED,
          ("Internal data flow error."),
          ("streaming task paused, reason %s (%d)", reason, ret));
      gst_wfdrtspsrc_push_event (src, gst_event_new_eos (), FALSE);
    }

    return FALSE;
  }
}

static const gchar *
gst_wfdrtspsrc_auth_method_to_string (GstRTSPAuthMethod method)
{
  gint index = 0;

  while (method != 0) {
    index++;
    method >>= 1;
  }
  switch (index) {
    case 0:
      return "None";
    case 1:
      return "Basic";
    case 2:
      return "Digest";
    default:
      break;
  }

  return "Unknown";
}

static const gchar *
gst_wfdrtspsrc_skip_lws (const gchar * s)
{
  while (g_ascii_isspace (*s))
    s++;
  return s;
}

static const gchar *
gst_wfdrtspsrc_unskip_lws (const gchar * s, const gchar * start)
{
  while (s > start && g_ascii_isspace (*(s - 1)))
    s--;
  return s;
}

static const gchar *
gst_wfdrtspsrc_skip_commas (const gchar * s)
{
  /* The grammar allows for multiple commas */
  while (g_ascii_isspace (*s) || *s == ',')
    s++;
  return s;
}

static const gchar *
gst_wfdrtspsrc_skip_item (const gchar * s)
{
  gboolean quoted = FALSE;
  const gchar *start = s;

  /* A list item ends at the last non-whitespace character
   * before a comma which is not inside a quoted-string. Or at
   * the end of the string.
   */
  while (*s) {
    if (*s == '"')
      quoted = !quoted;
    else if (quoted) {
      if (*s == '\\' && *(s + 1))
        s++;
    } else {
      if (*s == ',')
        break;
    }
    s++;
  }

  return gst_wfdrtspsrc_unskip_lws (s, start);
}

static void
gst_rtsp_decode_quoted_string (gchar * quoted_string)
{
  gchar *src, *dst;

  src = quoted_string + 1;
  dst = quoted_string;
  while (*src && *src != '"') {
    if (*src == '\\' && *(src + 1))
      src++;
    *dst++ = *src++;
  }
  *dst = '\0';
}

/* Extract the authentication tokens that the server provided for each method
 * into an array of structures and give those to the connection object.
 */
static void
gst_wfdrtspsrc_parse_digest_challenge (GstRTSPConnection * conn,
    const gchar * header, gboolean * stale)
{
  GSList *list = NULL, *iter;
  const gchar *end;
  gchar *item, *eq, *name_end, *value;

  g_return_if_fail (stale != NULL);

  gst_rtsp_connection_clear_auth_params (conn);
  *stale = FALSE;

  /* Parse a header whose content is described by RFC2616 as
   * "#something", where "something" does not itself contain commas,
   * except as part of quoted-strings, into a list of allocated strings.
   */
  header = gst_wfdrtspsrc_skip_commas (header);
  while (*header) {
    end = gst_wfdrtspsrc_skip_item (header);
    list = g_slist_prepend (list, g_strndup (header, end - header));
    header = gst_wfdrtspsrc_skip_commas (end);
  }
  if (!list)
    return;

  list = g_slist_reverse (list);
  for (iter = list; iter; iter = iter->next) {
    item = iter->data;

    if (item == NULL) continue;

    eq = strchr (item, '=');
    if (eq) {
      name_end = (gchar *) gst_wfdrtspsrc_unskip_lws (eq, item);
      if (name_end == item) {
        /* That's no good... */
        g_free (item);
        item = NULL;
        continue;
      }

      *name_end = '\0';

      value = (gchar *) gst_wfdrtspsrc_skip_lws (eq + 1);
      if (*value == '"')
        gst_rtsp_decode_quoted_string (value);
    } else
        value = NULL;

    if(value && item)
      if ((strcmp (item, "stale") == 0) && (strcmp (value, "TRUE") == 0))
        *stale = TRUE;
    gst_rtsp_connection_set_auth_param (conn, item, value);
    g_free (item);
  }

  g_slist_free (list);
}

/* Parse a WWW-Authenticate Response header and determine the
 * available authentication methods
 *
 * This code should also cope with the fact that each WWW-Authenticate
 * header can contain multiple challenge methods + tokens
 *
 * At the moment, for Basic auth, we just do a minimal check and don't
 * even parse out the realm */
static void
gst_wfdrtspsrc_parse_auth_hdr (gchar * hdr, GstRTSPAuthMethod * methods,
    GstRTSPConnection * conn, gboolean * stale)
{
  gchar *start;

  g_return_if_fail (hdr != NULL);
  g_return_if_fail (methods != NULL);
  g_return_if_fail (stale != NULL);

  /* Skip whitespace at the start of the string */
  for (start = hdr; start[0] != '\0' && g_ascii_isspace (start[0]); start++);

  if (g_ascii_strncasecmp (start, "basic", 5) == 0)
    *methods |= GST_RTSP_AUTH_BASIC;
  else if (g_ascii_strncasecmp (start, "digest ", 7) == 0) {
    *methods |= GST_RTSP_AUTH_DIGEST;
    gst_wfdrtspsrc_parse_digest_challenge (conn, &start[7], stale);
  }
}

/**
 * gst_wfdrtspsrc_setup_auth:
 * @src: the rtsp source
 *
 * Configure a username and password and auth method on the
 * connection object based on a response we received from the
 * peer.
 *
 * Currently, this requires that a username and password were supplied
 * in the uri. In the future, they may be requested on demand by sending
 * a message up the bus.
 *
 * Returns: TRUE if authentication information could be set up correctly.
 */
static gboolean
gst_wfdrtspsrc_setup_auth (GstWFDRTSPSrc * src, GstRTSPMessage * response)
{
  gchar *user = NULL;
  gchar *pass = NULL;
  GstRTSPAuthMethod avail_methods = GST_RTSP_AUTH_NONE;
  GstRTSPAuthMethod method;
  GstRTSPResult auth_result;
  GstRTSPUrl *url;
  GstRTSPConnection *conn;
  gchar *hdr;
  gboolean stale = FALSE;

  conn = src->conninfo.connection;

  /* Identify the available auth methods and see if any are supported */
  if (gst_rtsp_message_get_header (response, GST_RTSP_HDR_WWW_AUTHENTICATE,
          &hdr, 0) == GST_RTSP_OK) {
    gst_wfdrtspsrc_parse_auth_hdr (hdr, &avail_methods, conn, &stale);
  }

  if (avail_methods == GST_RTSP_AUTH_NONE)
    goto no_auth_available;

  /* For digest auth, if the response indicates that the session
   * data are stale, we just update them in the connection object and
   * return TRUE to retry the request */
  if (stale)
    src->tried_url_auth = FALSE;

  url = gst_rtsp_connection_get_url (conn);

  /* Do we have username and password available? */
  if (url != NULL && !src->tried_url_auth && url->user != NULL
      && url->passwd != NULL) {
    user = url->user;
    pass = url->passwd;
    src->tried_url_auth = TRUE;
    GST_DEBUG_OBJECT (src,
        "Attempting authentication using credentials from the URL");
  } else {
    user = src->user_id;
    pass = src->user_pw;
    GST_DEBUG_OBJECT (src,
        "Attempting authentication using credentials from the properties");
  }

  /* FIXME: If the url didn't contain username and password or we tried them
   * already, request a username and passwd from the application via some kind
   * of credentials request message */

  /* If we don't have a username and passwd at this point, bail out. */
  if (user == NULL || pass == NULL)
    goto no_user_pass;

  /* Try to configure for each available authentication method, strongest to
   * weakest */
  for (method = GST_RTSP_AUTH_MAX; method != GST_RTSP_AUTH_NONE; method >>= 1) {
    /* Check if this method is available on the server */
    if ((method & avail_methods) == 0)
      continue;

    /* Pass the credentials to the connection to try on the next request */
    auth_result = gst_rtsp_connection_set_auth (conn, method, user, pass);
    /* INVAL indicates an invalid username/passwd were supplied, so we'll just
     * ignore it and end up retrying later */
    if (auth_result == GST_RTSP_OK || auth_result == GST_RTSP_EINVAL) {
      GST_DEBUG_OBJECT (src, "Attempting %s authentication",
          gst_wfdrtspsrc_auth_method_to_string (method));
      break;
    }
  }

  if (method == GST_RTSP_AUTH_NONE)
    goto no_auth_available;

  return TRUE;

no_auth_available:
  {
    /* Output an error indicating that we couldn't connect because there were
     * no supported authentication protocols */
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("No supported authentication protocol was found"));
    return FALSE;
  }
no_user_pass:
  {
    /* We don't fire an error message, we just return FALSE and let the
     * normal NOT_AUTHORIZED error be propagated */
    return FALSE;
  }
}

static GstRTSPResult
gst_wfdrtspsrc_try_send (GstWFDRTSPSrc * src, GstRTSPConnection * conn,
    GstRTSPMessage * request, GstRTSPMessage * response,
    GstRTSPStatusCode * code)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPStatusCode thecode = GST_RTSP_STS_OK;

  gst_wfd_rtsp_ext_list_before_send (src->extensions, request);

  GST_DEBUG_OBJECT (src, "sending message");
#ifdef ENABLE_WFD_EXTENDED_FEATURES
  if (src->manager->extended_feature_support) {
    if (src->debug) {
      res =  func_dump(src, request);
    }
  }
#else
  if (src->debug) {
    wfd_rtsp_manager_message_dump(request);
  }
#endif

  res = gst_wfdrtspsrc_connection_send (src, conn, request, src->ptcp_timeout);
  if (res < 0)
    goto send_error;

  gst_rtsp_connection_reset_timeout (conn);

next:
  res = gst_wfdrtspsrc_connection_receive (src, conn, response, src->ptcp_timeout);
  if (res < 0)
    goto receive_error;

#ifdef ENABLE_WFD_EXTENDED_FEATURES
  if (src->manager->extended_feature_support) {
    if (src->debug) {
      res =  func_dump(src, response);
    }
  }
#else
  if (src->debug) {
    wfd_rtsp_manager_message_dump(response);
  }
#endif

  switch (response->type) {
    case GST_RTSP_MESSAGE_REQUEST:
      res = gst_wfdrtspsrc_handle_request (src, conn, response);
      if (res == GST_RTSP_EEOF)
        goto server_eof;
      else if (res < 0)
        goto handle_request_failed;
      goto next;
    case GST_RTSP_MESSAGE_RESPONSE:
      /* ok, a response is good */
      GST_DEBUG_OBJECT (src, "received response message");
      break;
    default:
    case GST_RTSP_MESSAGE_DATA:
      /* get next response */
      GST_DEBUG_OBJECT (src, "ignoring data response message");
      goto next;
  }

  thecode = response->type_data.response.code;

  GST_DEBUG_OBJECT (src, "got response message %d", thecode);

  /* if the caller wanted the result code, we store it. */
  if (code)
    *code = thecode;

  /* If the request didn't succeed, bail out before doing any more */
  if (thecode != GST_RTSP_STS_OK)
    return GST_RTSP_OK;

  gst_wfd_rtsp_ext_list_after_send (src->extensions, request, response);

  return GST_RTSP_OK;

  /* ERRORS */
send_error:
  {
    gchar *str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message. (%s)", str));
    g_free (str);
    return res;
  }
receive_error:
  {
    switch (res) {
      case GST_RTSP_EEOF:
        GST_ERROR_OBJECT (src, "server closed connection, doing reconnect");
#if 0
        if (try == 0) {
          try++;
          /* if reconnect succeeds, try again */
          if ((res = gst_wfdrtspsrc_conninfo_reconnect (src, &src->conninfo)) == 0)
            goto again;
        }
#endif
        /* only try once after reconnect, then fallthrough and error out */
      default:
      {
        gchar *str = gst_rtsp_strresult (res);

        GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
            ("Could not receive message. (%s)", str));
        g_free (str);
        break;
      }
    }
    return res;
  }
handle_request_failed:
  {
    /* ERROR was posted */
    gst_rtsp_message_unset (response);
    return res;
  }
server_eof:
  {
    GST_DEBUG_OBJECT (src, "we got an eof from the server");
    GST_ELEMENT_WARNING (src, RESOURCE, READ, (NULL),
        ("The server closed the connection."));
    gst_rtsp_message_unset (response);
    return res;
  }
}

/**
 * gst_wfdrtspsrc_send:
 * @src: the rtsp source
 * @conn: the connection to send on
 * @request: must point to a valid request
 * @response: must point to an empty #GstRTSPMessage
 * @code: an optional code result
 *
 * send @request and retrieve the response in @response. optionally @code can be
 * non-NULL in which case it will contain the status code of the response.
 *
 * If This function returns #GST_RTSP_OK, @response will contain a valid response
 * message that should be cleaned with gst_rtsp_message_unset() after usage.
 *
 * If @code is NULL, this function will return #GST_RTSP_ERROR (with an invalid
 * @response message) if the response code was not 200 (OK).
 *
 * If the attempt results in an authentication failure, then this will attempt
 * to retrieve authentication credentials via gst_wfdrtspsrc_setup_auth and retry
 * the request.
 *
 * Returns: #GST_RTSP_OK if the processing was successful.
 */

static GstRTSPResult
gst_wfdrtspsrc_send (GstWFDRTSPSrc * src, GstRTSPConnection * conn,
    GstRTSPMessage * request, GstRTSPMessage * response,
    GstRTSPStatusCode * code)
{
  GstRTSPStatusCode int_code = GST_RTSP_STS_OK;
  GstRTSPResult res = GST_RTSP_ERROR;
  gint count;
  gboolean retry;
  GstRTSPMethod method = GST_RTSP_INVALID;

  count = 0;
  do {
    retry = FALSE;

    /* make sure we don't loop forever */
    if (count++ > 8)
      break;

    /* save method so we can disable it when the server complains */
    method = request->type_data.request.method;

    if ((res =
            gst_wfdrtspsrc_try_send (src, conn, request, response, &int_code)) < 0)
      goto error;

    switch (int_code) {
      case GST_RTSP_STS_UNAUTHORIZED:
        if (gst_wfdrtspsrc_setup_auth (src, response)) {
          /* Try the request/response again after configuring the auth info
           * and loop again */
          retry = TRUE;
        }
        break;
      default:
        break;
    }
  } while (retry == TRUE);

  /* If the user requested the code, let them handle errors, otherwise
   * post an error below */
  if (code != NULL)
    *code = int_code;
  else if (int_code != GST_RTSP_STS_OK)
    goto error_response;

  return res;

  /* ERRORS */
error:
  {
    GST_DEBUG_OBJECT (src, "got error %d", res);
    return res;
  }
error_response:
  {
    res = GST_RTSP_ERROR;

    switch (response->type_data.response.code) {
      case GST_RTSP_STS_NOT_FOUND:
        GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL), ("%s",
                response->type_data.response.reason));
        break;
      case GST_RTSP_STS_MOVED_PERMANENTLY:
      case GST_RTSP_STS_MOVE_TEMPORARILY:
      {
        gchar *new_location;
        GstRTSPLowerTrans transports;

        GST_DEBUG_OBJECT (src, "got redirection");
        /* if we don't have a Location Header, we must error */
        if (gst_rtsp_message_get_header (response, GST_RTSP_HDR_LOCATION,
                &new_location, 0) < 0)
          break;

        /* When we receive a redirect result, we go back to the INIT state after
         * parsing the new URI. The caller should do the needed steps to issue
         * a new setup when it detects this state change. */
        GST_DEBUG_OBJECT (src, "redirection to %s", new_location);

        /* save current transports */
        if (src->conninfo.url)
          transports = src->conninfo.url->transports;
        else
          transports = GST_RTSP_LOWER_TRANS_UNKNOWN;

        gst_wfdrtspsrc_uri_set_uri (GST_URI_HANDLER (src), new_location);

        /* set old transports */
        if (src->conninfo.url && transports != GST_RTSP_LOWER_TRANS_UNKNOWN)
          src->conninfo.url->transports = transports;

        src->need_redirect = TRUE;
        src->state = GST_RTSP_STATE_INIT;
        res = GST_RTSP_OK;
        break;
      }
      case GST_RTSP_STS_NOT_ACCEPTABLE:
      case GST_RTSP_STS_NOT_IMPLEMENTED:
      case GST_RTSP_STS_METHOD_NOT_ALLOWED:
        GST_ERROR_OBJECT (src, "got NOT IMPLEMENTED, disable method %s",
            gst_rtsp_method_as_text (method));
        src->methods &= ~method;
        res = GST_RTSP_OK;
        break;
      default:
        GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
            ("Got error response: %d (%s).", response->type_data.response.code,
                response->type_data.response.reason));
        break;
    }
    /* if we return ERROR we should unset the response ourselves */
    if (res == GST_RTSP_ERROR)
      gst_rtsp_message_unset (response);

    return res;
  }
}

static GstRTSPResult
gst_wfdrtspsrc_send_cb (GstRTSPExtension * ext, GstRTSPMessage * request,
    GstRTSPMessage * response, GstWFDRTSPSrc * src)
{
  return gst_wfdrtspsrc_send (src, src->conninfo.connection, request, response,
      NULL);
}


/* parse the response and collect all the supported methods. We need this
 * information so that we don't try to send an unsupported request to the
 * server.
 */
static gboolean
gst_wfdrtspsrc_parse_methods (GstWFDRTSPSrc * src, GstRTSPMessage * response)
{
  GstRTSPHeaderField field;
  gchar *respoptions;
  gchar **options;
  gint indx = 0;
  gint i;

  /* reset supported methods */
  src->methods = 0;

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
     * OPTIONS, DESCRIBE, ANNOUNCE, PLAY, SETUP, ...
     */
    options = g_strsplit (respoptions, ",", 0);

    for (i = 0; options[i]; i++) {
      gchar *stripped;
      gint method;

      stripped = g_strstrip (options[i]);
      method = gst_rtsp_find_method (stripped);

      /* keep bitfield of supported methods */
      if (method != GST_RTSP_INVALID)
        src->methods |= method;
    }
    g_strfreev (options);

    indx++;
  }

  if (src->methods == 0) {
    /* neither Allow nor Public are required, assume the server supports
     * at least DESCRIBE, SETUP, we always assume it supports PLAY as
     * well. */
    GST_DEBUG_OBJECT (src, "could not get OPTIONS");
    src->methods = GST_RTSP_DESCRIBE | GST_RTSP_SETUP;
  }
  /* always assume PLAY, FIXME, extensions should be able to override
   * this */
  src->methods |= GST_RTSP_PLAY;

  if (!(src->methods & GST_RTSP_SETUP))
    goto no_setup;

  return TRUE;

no_setup:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("Server does not support SETUP."));
    return FALSE;
  }
}

static GstRTSPResult
gst_wfdrtspsrc_create_transports_string (GstWFDRTSPSrc * src, gchar ** transports)
{
  GstRTSPResult res = GST_RTSP_OK;
  GString *result;

  *transports = NULL;

  res =
      gst_wfd_rtsp_ext_list_get_transports (src->extensions, GST_RTSP_LOWER_TRANS_UDP, transports);

  if (res < 0)
    return GST_RTSP_ERROR;

  GST_DEBUG_OBJECT (src, "got transports %s", GST_STR_NULL (*transports));

  /* extension listed transports, use those */
  if (*transports != NULL)
    return GST_RTSP_OK;

  /* the default RTSP transports */
  result = g_string_new ("");
  GST_DEBUG_OBJECT (src, "adding UDP unicast");

  g_string_append (result, "RTP/AVP");
  g_string_append (result, "/UDP");
  g_string_append (result, ";unicast;client_port=");
  g_string_append_printf (result, "%d", src->primary_rtpport);
  g_string_append (result, "-");
  g_string_append_printf (result, "%d", src->primary_rtpport+1);

  *transports = g_string_free (result, FALSE);

  GST_DEBUG_OBJECT (src, "prepared transports %s", GST_STR_NULL (*transports));

  return GST_RTSP_OK;
}

/* Perform the SETUP request for all the streams.
 *
 * We ask the server for a specific transport, which initially includes all the
 * ones we can support (UDP/TCP/MULTICAST). For the UDP transport we allocate
 * two local UDP ports that we send to the server.
 *
 * Once the server replied with a transport, we configure the other streams
 * with the same transport.
 *
 * This function will also configure the manager for the selected transport,
 * which basically means creating the pipeline.
 */
static gboolean
gst_wfdrtspsrc_setup (GstWFDRTSPSrc * src)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  WFDRTSPManager *manager = NULL;
  GstRTSPLowerTrans protocols = GST_RTSP_LOWER_TRANS_UNKNOWN;
  GstRTSPStatusCode code = GST_RTSP_STS_OK;
  GstRTSPUrl *url;
  gchar *hval;
  GstRTSPConnection *conn;
  gchar *transports = NULL;

  if (src->conninfo.connection) {
    url = gst_rtsp_connection_get_url (src->conninfo.connection);
    /* we initially allow all configured lower transports. based on the URL
     * transports and the replies from the server we narrow them down. */
    protocols = url->transports & GST_RTSP_LOWER_TRANS_UDP;
  } else {
    url = NULL;
    protocols = GST_RTSP_LOWER_TRANS_UDP;
  }

  if (protocols == 0)
    goto no_protocols;

  manager = src->manager;

  manager->conninfo.location = src->conninfo.location;
  GST_DEBUG_OBJECT (src, " setup: %s",
    GST_STR_NULL (manager->conninfo.location));

  manager->control_url = g_strdup (src->conninfo.url_str);
  GST_DEBUG_OBJECT (src, " setup: %s",
    GST_STR_NULL (manager->control_url));

  manager->control_connection = src->conninfo.connection;
  GST_DEBUG_OBJECT (src, " setup: %p", manager->control_connection);

  /* see if we need to configure this manager */
  if (!gst_wfd_rtsp_ext_list_configure_stream (src->extensions, manager->caps)) {
    GST_DEBUG_OBJECT (src, "disabled by extension");
    goto no_setup;
  }

  /* skip setup if we have no URL for it */
  if (manager->conninfo.location == NULL) {
    GST_DEBUG_OBJECT (src, "no URL for set.");
    goto no_setup;
  }

  if (src->conninfo.connection == NULL) {
    GST_DEBUG_OBJECT (src, "try to connect manager %p", manager);
    if (!gst_wfdrtspsrc_conninfo_connect (src, &manager->conninfo)) {
      GST_DEBUG_OBJECT (src, "failed to connect", manager);
      goto no_setup;
    }
    conn = manager->conninfo.connection;
  } else {
    conn = src->conninfo.connection;
  }
  GST_DEBUG_OBJECT (src, "doing setup of manager %p with %s", manager,
      manager->conninfo.location);

  /* wfd use only UDP protocol for setup */
  if (!(protocols & GST_RTSP_LOWER_TRANS_UDP))
    goto no_protocols;

  GST_DEBUG_OBJECT (src, "protocols = 0x%x", protocols);
  /* create a string with first transport in line */
  res = gst_wfdrtspsrc_create_transports_string (src, &transports);
  if (res < 0 || transports == NULL)
    goto setup_transport_failed;

  if (strlen (transports) == 0) {
    g_free (transports);
    GST_DEBUG_OBJECT (src, "no transports found");
    goto setup_transport_failed;
  }

  /* create SETUP request */
  res =
      gst_rtsp_message_init_request (&request, GST_RTSP_SETUP,
      manager->conninfo.location);
  if (res < 0) {
    g_free (transports);
    goto create_request_failed;
  }

  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_USER_AGENT, (const gchar *)src->user_agent);
  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_TRANSPORT, transports);
  g_free (transports);

  /* if the user wants a non default RTP packet size we add the blocksize
   * parameter */
  if (src->rtp_blocksize > 0) {
    hval = g_strdup_printf ("%d", src->rtp_blocksize);
    gst_rtsp_message_add_header (&request, GST_RTSP_HDR_BLOCKSIZE, hval);
    g_free (hval);
  }

  /* handle the code ourselves */
  if ((res = gst_wfdrtspsrc_send (src, conn, &request, &response, &code) < 0))
    goto send_error;

  switch (code) {
    case GST_RTSP_STS_OK:
      break;
    case GST_RTSP_STS_UNSUPPORTED_TRANSPORT:
      gst_rtsp_message_unset (&request);
      gst_rtsp_message_unset (&response);
      goto no_protocols;
      break;
    default:
      goto response_error;
  }

  /* parse response transport */
  {
    gchar *resptrans = NULL;
    GstRTSPTransport transport = { 0 };

    gst_rtsp_message_get_header (&response, GST_RTSP_HDR_TRANSPORT,
        &resptrans, 0);
    if (!resptrans)
      goto no_transport;

    /* parse transport, go to next manager on parse error */
    if (gst_rtsp_transport_parse (resptrans, &transport) != GST_RTSP_OK) {
      GST_ERROR_OBJECT (src, "failed to parse transport %s", resptrans);
      goto setup_failed;
    }

    switch (transport.lower_transport) {
      case GST_RTSP_LOWER_TRANS_UDP:
        protocols = GST_RTSP_LOWER_TRANS_UDP;
        break;
      case GST_RTSP_LOWER_TRANS_TCP:
      case GST_RTSP_LOWER_TRANS_UDP_MCAST:
        GST_DEBUG_OBJECT (src, "transport %d is not supported",
            transport.lower_transport);
        goto setup_failed;
	  break;
      default:
        GST_DEBUG_OBJECT (src, "manager %p unknown transport %d", manager,
            transport.lower_transport);
        goto setup_failed;
        break;
    }
#ifdef ENABLE_WFD_EXTENDED_FEATURES
    /* now configure the manager with the selected transport */
    if (src->manager->extended_feature_support) {
      int (*func)(WFDRTSPManager *, GstRTSPTransport *, gint , gint)
               = __wfd_extended_func(src->manager, WFD_EXTENDED_CONFIGURE_TRANSPORT);
      if ( !func(manager, &transport,src->primary_rtpport, src->primary_rtpport+1)) {
        GST_DEBUG_OBJECT (src, "could not configure transport");
        goto setup_failed;
      }
    }
#else
    if (!wfd_rtsp_manager_configure_transport (manager, &transport,
		src->primary_rtpport, src->primary_rtpport+1)) {
      GST_DEBUG_OBJECT (src, "could not configure transport");
      goto setup_failed;
    }
#endif

#ifdef ENABLE_WFD_EXTENDED_FEATURES
    if(src->manager->enable_pad_probe) {
      if (src->manager->extended_feature_support) {
        void (*func)(WFDRTSPManager *)= __wfd_extended_func(src->manager, WFD_EXTENDED_ENABLE_PAD_PROBE);
        func(src->manager);
      }
    }
#else
    if(src->manager->enable_pad_probe) {
      wfd_rtsp_manager_enable_pad_probe(src->manager);
    }
#endif


    /* clean up our transport struct */
    gst_rtsp_transport_init (&transport);
    /* clean up used RTSP messages */
    gst_rtsp_message_unset (&request);
    gst_rtsp_message_unset (&response);
  }

  gst_wfd_rtsp_ext_list_stream_select (src->extensions, url);

  return TRUE;

  /* ERRORS */
no_protocols:
  {
    /* no transport possible, post an error and stop */
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Could not connect to server, no protocols left"));
    return FALSE;
  }
no_setup:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("Could not setup"));
    return FALSE;
  }
create_request_failed:
  {
    gchar *str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request. (%s)", str));
    g_free (str);
    goto cleanup_error;
  }
setup_transport_failed:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Could not setup transport."));
    goto cleanup_error;
  }
response_error:
  {
    const gchar *str = gst_rtsp_status_as_text (code);

    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Error (%d): %s", code, GST_STR_NULL (str)));
    goto cleanup_error;
  }
send_error:
  {
    gchar *str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message. (%s)", str));
    g_free (str);
    goto cleanup_error;
  }
no_transport:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Server did not select transport."));
    goto cleanup_error;
  }
setup_failed:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS, (NULL),
        ("Could not setup."));
    goto cleanup_error;
  }
cleanup_error:
  {
    gst_rtsp_message_unset (&request);
    gst_rtsp_message_unset (&response);
    return FALSE;
  }
}


/* Parse all the wifi related messages.
 *
 * Uses wfdconfig utility to parse the wfd parameters sent from the server
 */

static GstRTSPResult
gst_wfdrtspsrc_retrieve_wifi_parameters (GstWFDRTSPSrc * src)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPMessage message = { 0 };
  GstRTSPMethod method = { 0 };
  gchar *str = NULL;

  /* can't continue without a valid url */
  if (G_UNLIKELY (src->conninfo.url == NULL))
    goto no_url;
  src->tried_url_auth = FALSE;

  if ((res = gst_wfdrtspsrc_conninfo_connect (src, &src->conninfo)) < 0)
    goto connect_failed;

  res = gst_wfdrtspsrc_connection_receive (src, src->conninfo.connection, &message,
    src->ptcp_timeout);
  if(res != GST_RTSP_OK)
    goto connect_failed;

  if(message.type == GST_RTSP_MESSAGE_REQUEST) {
    method = message.type_data.request.method;
    if(method == GST_RTSP_OPTIONS)
      gst_wfdrtspsrc_handle_request (src, src->conninfo.connection, &message);
    else
      goto methods_error;
  }

  /* create OPTIONS */
  GST_DEBUG_OBJECT (src, "create options...");
  res = gst_rtsp_message_init_request (&request, GST_RTSP_OPTIONS,"*");
  if (res < 0)
    goto create_request_failed;

  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_REQUIRE, "org.wfa.wfd1.0");
  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);

  /* send OPTIONS */
  GST_DEBUG_OBJECT (src, "send options...");
  if (gst_wfdrtspsrc_send (src, src->conninfo.connection, &request, &response,
          NULL) < 0)
    goto send_error;

  /* parse OPTIONS */
  if (!gst_wfdrtspsrc_parse_methods (src, &response))
    goto methods_error;

 /* Receive request message from source */
receive_request_message:
  res = gst_wfdrtspsrc_connection_receive (src, src->conninfo.connection, &message,
    src->ptcp_timeout);
  if(res != GST_RTSP_OK)
    goto connect_failed;

  if(message.type == GST_RTSP_MESSAGE_REQUEST) {
    method = message.type_data.request.method;
    if(method == GST_RTSP_GET_PARAMETER ||method == GST_RTSP_SET_PARAMETER)
      gst_wfdrtspsrc_handle_request (src, src->conninfo.connection, &message);
    else
      goto methods_error;

    if (src->state != GST_RTSP_STATE_READY)
      goto receive_request_message;
  }

  /* clean up any messages */
  gst_rtsp_message_unset (&request);
  gst_rtsp_message_unset (&response);

  return res;

  /* ERRORS */
no_url:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL),
        ("No valid RTSP URL was provided"));
    goto cleanup_error;
  }
connect_failed:
  {
    str = NULL;
    str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ_WRITE, (NULL),
        ("Failed to connect. (%s)", str));
    g_free (str);
    goto cleanup_error;
  }
create_request_failed:
  {
    str = NULL;
    str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request. (%s)", str));
    g_free (str);
    goto cleanup_error;
  }

send_error:
  {
    /* Don't post a message - the rtsp_send method will have
     * taken care of it because we passed NULL for the response code */
    goto cleanup_error;
  }
methods_error:
  {
    GST_ERROR_OBJECT (src, "method errors");
    /* error was posted */
    goto cleanup_error;
  }

cleanup_error:
  {
    if (src->conninfo.connection) {
      GST_ERROR_OBJECT (src, "free connection");
      gst_wfdrtspsrc_conninfo_close (src, &src->conninfo, TRUE);
    }
    gst_rtsp_message_unset (&request);
    gst_rtsp_message_unset (&response);
    return res;
  }
}



static GstRTSPResult
gst_wfdrtspsrc_open (GstWFDRTSPSrc * src)
{
  GstRTSPResult res;

  src->methods =
      GST_RTSP_SETUP | GST_RTSP_PLAY | GST_RTSP_PAUSE | GST_RTSP_TEARDOWN;

  if ((res = gst_wfdrtspsrc_retrieve_wifi_parameters (src)) < 0)
    goto open_failed;

done:
  gst_wfdrtspsrc_loop_end_cmd (src, CMD_OPEN, res);

  return res;

  /* ERRORS */
open_failed:
  {
    GST_ERROR_OBJECT (src, "failed to open");
    goto done;
  }
}


static GstRTSPResult
gst_wfdrtspsrc_close (GstWFDRTSPSrc * src, gboolean only_close)
{
  WFDRTSPManager *manager = NULL;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPResult res = GST_RTSP_OK;
  gchar *control;
  gchar *setup_url;
  GstWFDRTSPConnInfo *info;

  GST_DEBUG_OBJECT (src, "TEARDOWN...");

  if (src->state < GST_RTSP_STATE_READY) {
    GST_DEBUG_OBJECT (src, "not ready, doing cleanup");
    goto close;
  }

  if (only_close)
    goto close;

  /* construct a control url */
  control = src->conninfo.url_str;

  if (!(src->methods & (GST_RTSP_PLAY | GST_RTSP_TEARDOWN)))
    goto not_supported;

  manager = src->manager;

  /* try aggregate control first but do non-aggregate control otherwise */
  if (control)
    setup_url = control;
  else if ((setup_url = manager->conninfo.location) == NULL)
    goto  close;

  if (src->conninfo.connection) {
    info = &src->conninfo;
  } else if (manager->conninfo.connection) {
    info = &manager->conninfo;
  } else {
    goto close;
  }
  if (!info->connected)
    goto close;

  /* do TEARDOWN */
  res =
      gst_rtsp_message_init_request (&request, GST_RTSP_TEARDOWN, setup_url);
  if (res < 0)
    goto create_request_failed;

  if ((res =
          gst_wfdrtspsrc_send (src, info->connection, &request, &response,
              NULL)) < 0)
    goto send_error;

  /* FIXME, parse result? */
  gst_rtsp_message_unset (&request);
  gst_rtsp_message_unset (&response);

close:
  /* close connections */
  GST_DEBUG_OBJECT (src, "closing connection...");
  gst_wfdrtspsrc_conninfo_close (src, &src->conninfo, TRUE);
  if (manager) {
    GST_DEBUG_OBJECT (src, "closing manager %p connection...", manager);
    gst_wfdrtspsrc_conninfo_close (src, &manager->conninfo, TRUE);
  }

  /* cleanup */
  gst_wfdrtspsrc_cleanup (src);

  src->state = GST_RTSP_STATE_INVALID;

  gst_wfdrtspsrc_loop_end_cmd (src, CMD_CLOSE, res);

  return res;

  /* ERRORS */
create_request_failed:
  {
    gchar *str = gst_rtsp_strresult (res);

    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request. (%s)", str));
    g_free (str);
    goto close;
  }
send_error:
  {
    gchar *str = gst_rtsp_strresult (res);

    gst_rtsp_message_unset (&request);
    if (res != GST_RTSP_EINTR) {
      GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
          ("Could not send message. (%s)", str));
    } else {
      GST_ERROR_OBJECT (src, "TEARDOWN interrupted");
    }
    g_free (str);
    goto close;
  }
not_supported:
  {
    GST_ERROR_OBJECT (src,
        "TEARDOWN and PLAY not supported, can't do TEARDOWN");
    goto close;
  }
}

/* RTP-Info is of the format:
 *
 * url=<URL>;[seq=<seqbase>;rtptime=<timebase>] [, url=...]
 *
 * rtptime corresponds to the timestamp for the NPT time given in the header
 * seqbase corresponds to the next sequence number we received. This number
 * indicates the first seqnum after the seek and should be used to discard
 * packets that are from before the seek.
 */
static gboolean
gst_wfdrtspsrc_parse_rtpinfo (GstWFDRTSPSrc * src, gchar * rtpinfo)
{
  gchar **infos;
  gint i, j;

  GST_DEBUG_OBJECT (src, "parsing RTP-Info %s", rtpinfo);

  infos = g_strsplit (rtpinfo, ",", 0);
  for (i = 0; infos[i]; i++) {
    gchar **fields;
    WFDRTSPManager *manager;
    gint64 seqbase;
    gint64 timebase;

    GST_DEBUG_OBJECT (src, "parsing info %s", infos[i]);

    /* init values, types of seqbase and timebase are bigger than needed so we
     * can store -1 as uninitialized values */
    manager = NULL;
    seqbase = GST_CLOCK_TIME_NONE;
    timebase = GST_CLOCK_TIME_NONE;

    /* parse url, find manager for url.
     * parse seq and rtptime. The seq number should be configured in the rtp
     * depayloader or session manager to detect gaps. Same for the rtptime, it
     * should be used to create an initial time newsegment. */
    fields = g_strsplit (infos[i], ";", 0);
    for (j = 0; fields[j]; j++) {
      GST_DEBUG_OBJECT (src, "parsing field %s", fields[j]);
      /* remove leading whitespace */
      fields[j] = g_strchug (fields[j]);
      if (g_str_has_prefix (fields[j], "url=")) {
        /* get the url and the manager */
        manager = src->manager;
      } else if (g_str_has_prefix (fields[j], "seq=")) {
        seqbase = atoi (fields[j] + 4);
      } else if (g_str_has_prefix (fields[j], "rtptime=")) {
        timebase = g_ascii_strtoll (fields[j] + 8, NULL, 10);
      }
    }
    g_strfreev (fields);
    /* now we need to store the values for the caps of the manager */
    if (manager != NULL) {
      GST_DEBUG_OBJECT (src,
          "found manager %p, setting: seqbase %d, timebase %" G_GINT64_FORMAT,
          manager, seqbase, timebase);

      /* we have a manager, configure detected params */
      manager->seqbase = seqbase;
      manager->timebase = timebase;
    }
  }
  g_strfreev (infos);

  return TRUE;
}

static GstRTSPResult
gst_wfdrtspsrc_play (GstWFDRTSPSrc * src)
{
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  GstRTSPResult res = GST_RTSP_OK;
  gchar *hval;
  gint hval_idx;
  gchar *control;
  WFDRTSPManager *manager;
  gchar *setup_url;
  GstRTSPConnection *conn;

  src->is_paused = FALSE;


  GST_DEBUG_OBJECT (src, "PLAY...");

  if (!(src->methods & GST_RTSP_PLAY))
    goto not_supported;

  if (src->state == GST_RTSP_STATE_PLAYING)
    goto was_playing;

  if (!src->conninfo.connection || !src->conninfo.connected)
    goto done;

  /* send some dummy packets before we activate the receive in the
   * udp sources */
  gst_wfdrtspsrc_send_dummy_packets (src);

  gst_element_set_state (GST_ELEMENT_CAST (src), GST_STATE_PLAYING);

  /* construct a control url */
  control = src->conninfo.url_str;

  manager = src->manager;
  /* try aggregate control first but do non-aggregate control otherwise */
  if (control)
    setup_url = control;
  else if ((setup_url = manager->conninfo.location) == NULL)
    goto skip;

  if (src->conninfo.connection) {
    conn = src->conninfo.connection;
  } else if (manager->conninfo.connection) {
    conn = manager->conninfo.connection;
  } else {
    goto skip;
  }

  /* do play */
  res = gst_rtsp_message_init_request (&request, GST_RTSP_PLAY, setup_url);
  if (res < 0)
    goto create_request_failed;

  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_USER_AGENT, (const gchar*)src->user_agent);

  if ((res = gst_wfdrtspsrc_send (src, conn, &request, &response, NULL)) < 0)
    goto send_error;

  gst_rtsp_message_unset (&request);

  /* parse the RTP-Info header field (if ANY) to get the base seqnum and timestamp
   * for the RTP packets. If this is not present, we assume all starts from 0...
   * This is info for the RTP session manager that we pass to it in caps. */
  hval_idx = 0;
  while (gst_rtsp_message_get_header (&response, GST_RTSP_HDR_RTP_INFO,
          &hval, hval_idx++) == GST_RTSP_OK)
    gst_wfdrtspsrc_parse_rtpinfo (src, hval);

  gst_rtsp_message_unset (&response);

skip:
  /* configure the caps of the streams after we parsed all headers. */
  gst_wfdrtspsrc_configure_caps (src);

  src->state = GST_RTSP_STATE_PLAYING;

  /* mark discont */
  GST_DEBUG_OBJECT (src, "mark DISCONT");
  manager->discont = TRUE;

done:
  gst_wfdrtspsrc_loop_end_cmd (src, CMD_PLAY, res);

  return res;

  /* ERRORS */
not_supported:
  {
    GST_ERROR_OBJECT (src, "PLAY is not supported");
    goto done;
  }
was_playing:
  {
    GST_DEBUG_OBJECT (src, "we were already PLAYING");
    goto done;
  }
create_request_failed:
  {
    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request."));
    return FALSE;
  }
send_error:
  {
    gst_rtsp_message_unset (&request);
    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message."));
    return FALSE;
  }
}

static GstRTSPResult
gst_wfdrtspsrc_pause (GstWFDRTSPSrc * src)
{
  GstRTSPResult res = GST_RTSP_OK;
  GstRTSPMessage request = { 0 };
  GstRTSPMessage response = { 0 };
  WFDRTSPManager *manager;
  GstRTSPConnection *conn;
  gchar *setup_url;
  gchar *control;

  GST_DEBUG_OBJECT (src, "PAUSE...");

  if (!(src->methods & GST_RTSP_PAUSE))
    goto not_supported;

  if (src->state == GST_RTSP_STATE_READY)
    goto was_paused;

  if (!src->conninfo.connection || !src->conninfo.connected)
    goto no_connection;

  /* construct a control url */
  control = src->conninfo.url_str;

  manager = src->manager;

  /* try aggregate control first but do non-aggregate control otherwise */
  if (control)
    setup_url = control;
  else if ((setup_url = manager->conninfo.location) == NULL)
    goto no_connection;

  if (src->conninfo.connection) {
    conn = src->conninfo.connection;
  } else if (manager->conninfo.connection) {
    conn = manager->conninfo.connection;
  } else {
    goto no_connection;
  }

  if (gst_rtsp_message_init_request (&request, GST_RTSP_PAUSE, setup_url) < 0)
    goto create_request_failed;

  gst_rtsp_message_add_header (&request, GST_RTSP_HDR_USER_AGENT, (const gchar *)src->user_agent);

  if ((res = gst_wfdrtspsrc_send (src, conn, &request, &response, NULL)) < 0)
    goto send_error;

  gst_rtsp_message_unset (&request);
  gst_rtsp_message_unset (&response);

  src->is_paused = TRUE;

no_connection:
  src->state = GST_RTSP_STATE_READY;

done:
  gst_wfdrtspsrc_loop_end_cmd (src, CMD_PAUSE, res);

  return res;

  /* ERRORS */
not_supported:
  {
    GST_ERROR_OBJECT (src, "PAUSE is not supported");
    goto done;
  }
was_paused:
  {
    GST_DEBUG_OBJECT (src, "we were already PAUSED");
    goto done;
  }
create_request_failed:
  {
    GST_ELEMENT_ERROR (src, LIBRARY, INIT, (NULL),
        ("Could not create request."));
    goto done;
  }
send_error:
  {
    gst_rtsp_message_unset (&request);
    GST_ELEMENT_ERROR (src, RESOURCE, WRITE, (NULL),
        ("Could not send message."));
    goto done;
  }
}

static void
gst_wfdrtspsrc_handle_message (GstBin * bin, GstMessage * message)
{
  GstWFDRTSPSrc *src;

  src = GST_WFDRTSPSRC (bin);

  GST_DEBUG_OBJECT (src, "got %s message from %s",
      GST_MESSAGE_TYPE_NAME (message), GST_MESSAGE_SRC_NAME (message));

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_EOS:
      gst_message_unref (message);
      break;
    case GST_MESSAGE_ELEMENT:
    {
      const GstStructure *s = gst_message_get_structure (message);
      if (gst_structure_has_name (s, "GstUDPSrcTimeout"))
        GST_DEBUG_OBJECT (bin, "timeout on UDP port");

      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
      break;
    }
    case GST_MESSAGE_ERROR:
    {
      WFDRTSPManager *manager;
      GstObject *message_src;
      gchar* debug = NULL;
      GError* error = NULL;

      message_src = GST_MESSAGE_SRC (message);

      gst_message_parse_error (message, &error, &debug);
      GST_ERROR_OBJECT (src, "error : %s", error->message);
      GST_ERROR_OBJECT (src, "debug : %s", debug);

      if (debug)
        g_free (debug);
      debug = NULL;
      g_error_free (error);

      manager = src->manager;
      if (!manager)
        goto forward;

      /* we ignore the RTCP udpsrc */
      if (manager->udpsrc[1] == GST_ELEMENT_CAST (message_src))
        goto done;

#ifdef ENABLE_WFD_EXTENDED_FEATURES
      /* we ignore the Retransmitted RTP udpsrc */
      if (manager->udpsrc[2] == GST_ELEMENT_CAST (message_src))
        goto done;
#endif

    forward:
      /* fatal but not our message, forward */
      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
      break;
    done:
      gst_message_unref (message);
      break;
    }
    default:
    {
      GST_BIN_CLASS (parent_class)->handle_message (bin, message);
      break;
    }
  }
}

/* the thread where everything happens */
static void
gst_wfdrtspsrc_thread (GstWFDRTSPSrc * src)
{
  gint cmd;
  GstRTSPResult ret;
  gboolean running = FALSE;

  GST_OBJECT_LOCK (src);
  cmd = src->loop_cmd;
  src->loop_cmd = CMD_WAIT;
  GST_DEBUG_OBJECT (src, "got command %d", cmd);

  /* we got the message command, so ensure communication is possible again */
  gst_wfdrtspsrc_connection_flush (src, FALSE);

  /* we allow these to be interrupted */
  if (cmd == CMD_LOOP || cmd == CMD_PAUSE || cmd == CMD_SET_PARAM )
    src->waiting = TRUE;
  GST_OBJECT_UNLOCK (src);

  switch (cmd) {
    case CMD_OPEN:
      ret = gst_wfdrtspsrc_open (src);
      break;
    case CMD_PLAY:
      ret = gst_wfdrtspsrc_play (src);
      if (ret == GST_RTSP_OK)
        running = TRUE;
      break;
    case CMD_PAUSE:
      ret = gst_wfdrtspsrc_pause (src);
      if (ret == GST_RTSP_OK)
        running = TRUE;
      break;
    case CMD_CLOSE:
      ret = gst_wfdrtspsrc_close (src, FALSE);
      break;
    case CMD_LOOP:
      running = gst_wfdrtspsrc_loop (src);
      break;
     case CMD_SET_PARAM:
      ret = gst_wfdrtspsrc_prepare_set_param (src);
      if (ret == GST_RTSP_OK)
        running = TRUE;
      break;
   default:
      break;
  }

  GST_OBJECT_LOCK (src);
  /* and go back to sleep */
  if (src->loop_cmd == CMD_WAIT) {
    if (running)
      src->loop_cmd = CMD_LOOP;
    else if (src->task)
      gst_task_pause (src->task);
  }

  /* reset waiting */
  src->waiting = FALSE;
  GST_OBJECT_UNLOCK (src);
}

static gboolean
gst_wfdrtspsrc_start (GstWFDRTSPSrc * src)
{
  GST_DEBUG_OBJECT (src, "starting");

  GST_OBJECT_LOCK (src);

  src->loop_cmd = CMD_WAIT;

  if (src->task == NULL) {
    src->task = gst_task_create ((GstTaskFunction) gst_wfdrtspsrc_thread, src);
    if (src->task == NULL)
      goto task_error;

    gst_task_set_lock (src->task, GST_WFD_RTSP_TASK_GET_LOCK (src));
  }
  GST_OBJECT_UNLOCK (src);

  return TRUE;

  /* ERRORS */
task_error:
  {
    GST_ERROR_OBJECT (src, "failed to create task");
    return FALSE;
  }
}

static gboolean
gst_wfdrtspsrc_stop (GstWFDRTSPSrc * src)
{
  GstTask *task;

  GST_DEBUG_OBJECT (src, "stopping");

  GST_OBJECT_LOCK (src);
  GST_DEBUG_OBJECT (src, "interrupt all command");
  src->waiting = TRUE;
  src->do_stop = TRUE;
  GST_OBJECT_UNLOCK (src);

  /* also cancels pending task */
  gst_wfdrtspsrc_loop_send_cmd (src, CMD_WAIT);

  GST_OBJECT_LOCK (src);
  if ((task = src->task)) {
    src->task = NULL;
    GST_OBJECT_UNLOCK (src);

    gst_task_stop (task);

    /* make sure it is not running */
    GST_WFD_RTSP_TASK_LOCK (src);
    GST_WFD_RTSP_TASK_UNLOCK (src);

    /* now wait for the task to finish */
    gst_task_join (task);

    /* and free the task */
    gst_object_unref (GST_OBJECT (task));
    GST_OBJECT_LOCK (src);
  }
  GST_OBJECT_UNLOCK (src);

  /* ensure synchronously all is closed and clean */
  gst_wfdrtspsrc_close (src, FALSE);

  return TRUE;
}


static GstStateChangeReturn
gst_wfdrtspsrc_change_state (GstElement * element, GstStateChange transition)
{
  GstWFDRTSPSrc *src;
  GstStateChangeReturn ret;

  src = GST_WFDRTSPSRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      GST_DEBUG_OBJECT (src, "NULL->READY");
      if (!gst_wfdrtspsrc_start (src))
        goto start_failed;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_DEBUG_OBJECT (src, "READY->PAUSED");
      gst_wfdrtspsrc_loop_send_cmd (src, CMD_OPEN);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      GST_DEBUG_OBJECT(src, "PAUSED->PLAYING");
      gst_wfdrtspsrc_loop_send_cmd (src, CMD_WAIT);
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      GST_DEBUG_OBJECT(src, "PLAYING->PAUSED");
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_DEBUG_OBJECT(src, "PAUSED->READY");
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      GST_DEBUG_OBJECT (src, "READY->NULL");
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto done;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      gst_wfdrtspsrc_loop_send_cmd (src, CMD_PLAY);
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      ret = GST_STATE_CHANGE_NO_PREROLL;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_wfdrtspsrc_stop (src);
      break;
    default:
      break;
  }

done:
  GST_DEBUG_OBJECT (src, "state change is done");
  return ret;

start_failed:
  {
    GST_ERROR_OBJECT (src, "start failed");
    return GST_STATE_CHANGE_FAILURE;
  }
}

static gboolean
gst_wfdrtspsrc_send_event (GstElement * element, GstEvent * event)
{
  gboolean res;
  GstWFDRTSPSrc *src;

  src = GST_WFDRTSPSRC (element);

  if (GST_EVENT_IS_DOWNSTREAM (event)) {
    res = gst_wfdrtspsrc_push_event (src, event, TRUE);
  } else {
    res = GST_ELEMENT_CLASS (parent_class)->send_event (element, event);
  }

  return res;
}


/*** GSTURIHANDLER INTERFACE *************************************************/
static GstURIType
gst_wfdrtspsrc_uri_get_type (void)
{
  return GST_URI_SRC;
}

static gchar **
gst_wfdrtspsrc_uri_get_protocols (void)
{
  static const gchar *protocols[] =
      { "rtsp", NULL };

  return (gchar **) protocols;
}

static const gchar *
gst_wfdrtspsrc_uri_get_uri (GstURIHandler * handler)
{
  GstWFDRTSPSrc *src = GST_WFDRTSPSRC (handler);

  /* should not dup */
  return src->conninfo.location;
}

static gboolean
gst_wfdrtspsrc_uri_set_uri (GstURIHandler * handler, const gchar * uri)
{
  GstWFDRTSPSrc *src;
  GstRTSPResult res;
  GstRTSPUrl *newurl = NULL;

  src = GST_WFDRTSPSRC (handler);

  /* same URI, we're fine */
  if (src->conninfo.location && uri && !strcmp (uri, src->conninfo.location))
    goto was_ok;

    /* try to parse */
    GST_DEBUG_OBJECT (src, "parsing URI");
    if ((res = gst_rtsp_url_parse (uri, &newurl)) < 0)
      goto parse_error;


  /* if worked, free previous and store new url object along with the original
   * location. */
  GST_DEBUG_OBJECT (src, "configuring URI");
  g_free (src->conninfo.location);
  src->conninfo.location = g_strdup (uri);
  gst_rtsp_url_free (src->conninfo.url);
  src->conninfo.url = newurl;
  g_free (src->conninfo.url_str);
  if (newurl)
    src->conninfo.url_str = gst_rtsp_url_get_request_uri (src->conninfo.url);
  else
    src->conninfo.url_str = NULL;

  GST_DEBUG_OBJECT (src, "set uri: %s", GST_STR_NULL (uri));
  GST_DEBUG_OBJECT (src, "request uri is: %s",
      GST_STR_NULL (src->conninfo.url_str));

  return TRUE;

  /* Special cases */
was_ok:
  {
    GST_DEBUG_OBJECT (src, "URI was ok: '%s'", GST_STR_NULL (uri));
    return TRUE;
  }
parse_error:
  {
    GST_ERROR_OBJECT (src, "Not a valid RTSP url '%s' (%d)",
        GST_STR_NULL (uri), res);
    return FALSE;
  }
}

static void
gst_wfdrtspsrc_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_wfdrtspsrc_uri_get_type;
  iface->get_protocols = gst_wfdrtspsrc_uri_get_protocols;
  iface->get_uri = gst_wfdrtspsrc_uri_get_uri;
  iface->set_uri = gst_wfdrtspsrc_uri_set_uri;
}


static GstRTSPResult _get_cea_resolution_and_set_to_src(GstWFDRTSPSrc *src, WFDVideoCEAResolution Resolution)
{
  WFDVideoCEAResolution CEARes = Resolution;
  switch(CEARes)
  {
    case WFD_CEA_UNKNOWN:
      break;
    case WFD_CEA_640x480P60:
      src->video_width=640;
      src->video_height=480;
      src->video_framerate=60;
      break;
    case WFD_CEA_720x480P60:
      src->video_width=720;
      src->video_height=480;
      src->video_framerate=60;
      break;
    case WFD_CEA_720x480I60:
      src->video_width=720;
      src->video_height=480;
      src->video_framerate=60;
      break;
    case WFD_CEA_720x576P50:
      src->video_width=720;
      src->video_height=576;
      src->video_framerate=50;
      break;
    case WFD_CEA_720x576I50:
       src->video_width=720;
      src->video_height=576;
      src->video_framerate=50;
      break;
    case WFD_CEA_1280x720P30:
      src->video_width=1280;
      src->video_height=720;
      src->video_framerate=30;
      break;
    case WFD_CEA_1280x720P60:
      src->video_width=1280;
      src->video_height=720;
      src->video_framerate=60;
      break;
    case WFD_CEA_1920x1080P30:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=30;
      break;
    case WFD_CEA_1920x1080P60:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=60;
      break;
    case WFD_CEA_1920x1080I60:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=60;
      break;
    case WFD_CEA_1280x720P25:
      src->video_width=1280;
      src->video_height=720;
      src->video_framerate=25;
      break;
    case WFD_CEA_1280x720P50:
      src->video_width=1280;
      src->video_height=720;
      src->video_framerate=50;
      break;
    case WFD_CEA_1920x1080P25:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=25;
      break;
    case WFD_CEA_1920x1080P50:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=50;
      break;
    case WFD_CEA_1920x1080I50:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=50;
      break;
    case WFD_CEA_1280x720P24:
      src->video_width=1280;
      src->video_height=720;
      src->video_framerate=24;
      break;
    case WFD_CEA_1920x1080P24:
      src->video_width=1920;
      src->video_height=1080;
      src->video_framerate=24;
      break;
    default:
      break;
  }
  return GST_RTSP_OK;
}

static GstRTSPResult _get_vesa_resolution_and_set_to_src(GstWFDRTSPSrc *src, WFDVideoVESAResolution Resolution)
{
  WFDVideoVESAResolution VESARes = Resolution;
  switch(VESARes)
  {
    case WFD_VESA_UNKNOWN:
      break;
    case WFD_VESA_800x600P30:
      src->video_width=800;
      src->video_height=600;
      src->video_framerate=30;
      break;
    case WFD_VESA_800x600P60:
      src->video_width=800;
      src->video_height=600;
      src->video_framerate=60;
      break;
    case WFD_VESA_1024x768P30:
      src->video_width=1024;
      src->video_height=768;
      src->video_framerate=30;
      break;
    case WFD_VESA_1024x768P60:
      src->video_width=1024;
      src->video_height=768;
      src->video_framerate=60;
      break;
    case WFD_VESA_1152x864P30:
      src->video_width=1152;
      src->video_height=864;
      src->video_framerate=30;
      break;
    case WFD_VESA_1152x864P60:
      src->video_width=1152;
      src->video_height=864;
      src->video_framerate=60;
      break;
    case WFD_VESA_1280x768P30:
      src->video_width=1280;
      src->video_height=768;
      src->video_framerate=30;
      break;
    case WFD_VESA_1280x768P60:
      src->video_width=1280;
      src->video_height=768;
      src->video_framerate=60;
      break;
    case WFD_VESA_1280x800P30:
      src->video_width=1280;
      src->video_height=800;
      src->video_framerate=30;
      break;
    case WFD_VESA_1280x800P60:
      src->video_width=1280;
      src->video_height=800;
      src->video_framerate=60;
      break;
    case WFD_VESA_1360x768P30:
      src->video_width=1360;
      src->video_height=768;
      src->video_framerate=30;
      break;
    case WFD_VESA_1360x768P60:
      src->video_width=1360;
      src->video_height=768;
      src->video_framerate=60;
      break;
    case WFD_VESA_1366x768P30:
      src->video_width=1366;
      src->video_height=768;
      src->video_framerate=30;
      break;
    case WFD_VESA_1366x768P60:
      src->video_width=1366;
      src->video_height=768;
      src->video_framerate=60;
      break;
    case WFD_VESA_1280x1024P30:
      src->video_width=1280;
      src->video_height=1024;
      src->video_framerate=30;
      break;
    case WFD_VESA_1280x1024P60:
      src->video_width=1280;
      src->video_height=1024;
      src->video_framerate=60;
      break;
    case WFD_VESA_1400x1050P30:
      src->video_width=1400;
      src->video_height=1050;
      src->video_framerate=30;
      break;
    case WFD_VESA_1400x1050P60:
      src->video_width=1400;
      src->video_height=1050;
      src->video_framerate=60;
      break;
    case WFD_VESA_1440x900P30:
      src->video_width=1440;
      src->video_height=900;
      src->video_framerate=30;
      break;
    case WFD_VESA_1440x900P60:
      src->video_width=1440;
      src->video_height=900;
      src->video_framerate=60;
      break;
    case WFD_VESA_1600x900P30:
      src->video_width=1600;
      src->video_height=900;
      src->video_framerate=30;
      break;
    case WFD_VESA_1600x900P60:
      src->video_width=1600;
      src->video_height=900;
      src->video_framerate=60;
      break;
    case WFD_VESA_1600x1200P30:
      src->video_width=1600;
      src->video_height=1200;
      src->video_framerate=30;
      break;
    case WFD_VESA_1600x1200P60:
      src->video_width=1600;
      src->video_height=1200;
      src->video_framerate=60;
      break;
    case WFD_VESA_1680x1024P30:
      src->video_width=1680;
      src->video_height=1024;
      src->video_framerate=30;
      break;
    case WFD_VESA_1680x1024P60:
      src->video_width=1680;
      src->video_height=1024;
      src->video_framerate=60;
      break;
    case WFD_VESA_1680x1050P30:
      src->video_width=1680;
      src->video_height=1050;
      src->video_framerate=30;
      break;
    case WFD_VESA_1680x1050P60:
      src->video_width=1680;
      src->video_height=1050;
      src->video_framerate=60;
      break;
    case WFD_VESA_1920x1200P30:
      src->video_width=1920;
      src->video_height=1200;
      src->video_framerate=30;
      break;
    case WFD_VESA_1920x1200P60:
      src->video_width=1920;
      src->video_height=1200;
      src->video_framerate=60;
      break;
    default:
      break;
  }
  return GST_RTSP_OK;
}

static GstRTSPResult _get_hh_resolution_and_set_to_src(GstWFDRTSPSrc *src, WFDVideoHHResolution Resolution)
{
  WFDVideoHHResolution HHRes = Resolution;
  switch(HHRes)
  {
    case WFD_HH_UNKNOWN:
      break;
    case WFD_HH_800x480P30:
      src->video_width=800;
      src->video_height=480;
      src->video_framerate=30;
      break;
    case WFD_HH_800x480P60:
      src->video_width=800;
      src->video_height=480;
      src->video_framerate=60;
      break;
    case WFD_HH_854x480P30:
      src->video_width=854;
      src->video_height=480;
      src->video_framerate=30;
      break;
    case WFD_HH_854x480P60:
      src->video_width=854;
      src->video_height=480;
      src->video_framerate=60;
      break;
    case WFD_HH_864x480P30:
      src->video_width=864;
      src->video_height=480;
      src->video_framerate=30;
      break;
    case WFD_HH_864x480P60:
      src->video_width=864;
      src->video_height=480;
      src->video_framerate=60;
      break;
    case WFD_HH_640x360P30:
      src->video_width=640;
      src->video_height=360;
      src->video_framerate=30;
      break;
    case WFD_HH_640x360P60:
      src->video_width=640;
      src->video_height=360;
      src->video_framerate=60;
      break;
    case WFD_HH_960x540P30:
      src->video_width=960;
      src->video_height=540;
      src->video_framerate=30;
      break;
    case WFD_HH_960x540P60:
      src->video_width=960;
      src->video_height=540;
      src->video_framerate=60;
      break;
    case WFD_HH_848x480P30:
      src->video_width=848;
      src->video_height=480;
      src->video_framerate=30;
      break;
    case WFD_HH_848x480P60:
      src->video_width=848;
      src->video_height=480;
      src->video_framerate=60;
      break;
    default:
      break;
  }
  return GST_RTSP_OK;
}

static GstRTSPResult
gst_wfdrtspsrc_get_audio_parameter(GstWFDRTSPSrc * src, WFDMessage * msg)
{
  WFDAudioFormats audio_format = WFD_AUDIO_UNKNOWN;
  WFDAudioChannels audio_channels = WFD_CHANNEL_UNKNOWN;
  WFDAudioFreq audio_frequency = WFD_FREQ_UNKNOWN;
  guint audio_bitwidth = 0;
  guint32 audio_latency = 0;

  WFDCONFIG_GET_PREFERED_AUDIO_FORMAT(msg, &audio_format, &audio_frequency, &audio_channels, &audio_bitwidth, &audio_latency);

  src->audio_format = g_strdup(msg->audio_codecs->list->audio_format);
  if(audio_frequency == WFD_FREQ_48000)
    audio_frequency = 48000;
  else if(audio_frequency == WFD_FREQ_44100)
    audio_frequency = 44100;

  if(audio_channels == WFD_CHANNEL_2)
    audio_channels = 2;
  else if(audio_channels == WFD_CHANNEL_4)
    audio_channels = 4;
  else if(audio_channels == WFD_CHANNEL_6)
    audio_channels = 6;
  else if(audio_channels == WFD_CHANNEL_8)
    audio_channels = 8;

  src->audio_channels = audio_channels;
  src->audio_frequency = audio_frequency;
  src->audio_bitwidth = audio_bitwidth;

#ifdef ENABLE_WFD_EXTENDED_FEATURES
  src->manager->audio_data_per_sec = audio_frequency * audio_channels * (audio_bitwidth/8);
#endif

  return GST_RTSP_OK;
}

static GstRTSPResult
gst_wfdrtspsrc_get_video_parameter(GstWFDRTSPSrc * src, WFDMessage * msg)
{
  WFDVideoCodecs cvCodec = WFD_VIDEO_UNKNOWN;
  WFDVideoNativeResolution cNative = WFD_VIDEO_CEA_RESOLUTION;
  guint64 cNativeResolution = 0;
  WFDVideoCEAResolution cCEAResolution = WFD_CEA_UNKNOWN;
  WFDVideoVESAResolution cVESAResolution = WFD_VESA_UNKNOWN;
  WFDVideoHHResolution cHHResolution = WFD_HH_UNKNOWN;
  WFDVideoH264Profile cProfile = WFD_H264_UNKNOWN_PROFILE;
  WFDVideoH264Level cLevel = WFD_H264_LEVEL_UNKNOWN;
  guint32 cMaxHeight = 0;
  guint32 cMaxWidth = 0;
  guint32 cmin_slice_size = 0;
  guint32 cslice_enc_params = 0;
  guint cframe_rate_control = 0;
  guint cvLatency = 0;
  WFDCONFIG_GET_PREFERED_VIDEO_FORMAT(msg, &cvCodec, &cNative, &cNativeResolution,
      &cCEAResolution, &cVESAResolution, &cHHResolution,
      &cProfile, &cLevel, &cvLatency, &cMaxHeight,
      &cMaxWidth, &cmin_slice_size, &cslice_enc_params, &cframe_rate_control);
#if 0
  switch(cNative)
  {
    case WFD_VIDEO_CEA_RESOLUTION:
      _get_cea_resolution_and_set_to_src(src, cCEAResolution);
      break;
    case WFD_VIDEO_VESA_RESOLUTION:
      _get_vesa_resolution_and_set_to_src(src, cVESAResolution);
      break;
    case WFD_VIDEO_HH_RESOLUTION:
      _get_hh_resolution_and_set_to_src(src, cHHResolution);
      break;
    default:
      break;
  }
#endif

  if(cCEAResolution != WFD_CEA_UNKNOWN) {
    _get_cea_resolution_and_set_to_src(src, cCEAResolution);
  }
  else if(cVESAResolution != WFD_VESA_UNKNOWN) {
    _get_vesa_resolution_and_set_to_src(src, cVESAResolution);
  }
   else if(cHHResolution != WFD_HH_UNKNOWN) {
    _get_hh_resolution_and_set_to_src(src, cHHResolution);
  }

  return GST_RTSP_OK;
}

#ifdef ENABLE_WFD_SINK_UIBC
static void
uibc_new_tcp (GstWFDRTSPSrc * src)
{
  struct sockaddr_in serv_addr;
  int retry = 0;
  int rc = 0;

  g_return_if_fail(src->uibc);

  if(src->uibc->state_cb != NULL) {
    GST_DEBUG_OBJECT (src, "Calling uibc_state_cb %x", (void *)src->uibc->state_cb);
    src->uibc->state_cb(1, (void *)src->uibc->hidc_cap_list_src, src->uibc->hidc_cap_count_src, src->video_width, src->video_height, src->uibc->wfd_sink_handle);
  } else {
    GST_ERROR_OBJECT (src, "UIBC state callback is not set!");
  }

  src->uibc->sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (src->uibc->sockfd < 0)
    goto fail_to_create;

  memset(&serv_addr, 0x00, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(src->uibc->port);
  serv_addr.sin_addr.s_addr = inet_addr(gst_rtsp_connection_get_ip (src->conninfo.connection));
  GST_DEBUG_OBJECT(src, "uibc get socket before connect");

try_again:
  rc = connect(src->uibc->sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
  if (rc < 0) {
    GST_DEBUG_OBJECT(src, "try to connect uibc channel (%d times..)", retry);
    if (retry <5) {
      retry++;
      usleep(10000);
      goto try_again;
    } else {
      goto fail_to_connect;
    }
  }

  GST_DEBUG_OBJECT(src, "UIBC is initiated and connected using port %u", src->uibc->port);

  return;

  /* ERRORS */
fail_to_connect:
  {
      GST_ERROR_OBJECT(src, "failed to connect uibc channel with reason %d", rc);
      src->uibc->state_cb(0, NULL, 0, src->video_width, src->video_height, src->uibc->wfd_sink_handle);
      shutdown (src->uibc->sockfd, SHUT_RDWR);
      src->uibc->sockfd = -1;
  }
fail_to_create:
  {
    GST_ERROR_OBJECT(src, "failed to open socket");
    src->uibc->state_cb(0, NULL, 0, src->video_width, src->video_height, src->uibc->wfd_sink_handle);
    return;
  }
}
#endif

