/*
 * wfdrtspsrcmanager
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

#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>

#include "wfdrtspmanager.h"

#ifdef G_OS_WIN32
#include <winsock2.h>
#endif

GST_DEBUG_CATEGORY_STATIC (wfd_rtsp_manager_debug);
#define GST_CAT_DEFAULT wfd_rtsp_manager_debug

/* signals and args */
enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_DO_RTCP,
  PROP_LATENCY,
  PROP_UDP_BUFFER_SIZE,
  PROP_UDP_TIMEOUT,
  PROP_ENABLE_PAD_PROBE,
  PROP_LAST
};

#define DEFAULT_DO_RTCP          TRUE
#define DEFAULT_LATENCY_MS       2000
#define DEFAULT_UDP_BUFFER_SIZE  0x80000
#define DEFAULT_UDP_TIMEOUT          10000000


G_DEFINE_TYPE (WFDRTSPManager, wfd_rtsp_manager, G_TYPE_OBJECT);

/* GObject vmethods */
static void wfd_rtsp_manager_finalize (GObject * object);
static void wfd_rtsp_manager_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void wfd_rtsp_manager_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean
wfd_rtsp_manager_pad_probe_cb(GstPad * pad, GstMiniObject * object, gpointer u_data);

/*rtsp dump code start*/
typedef struct _RTSPKeyValue
{
  GstRTSPHeaderField field;
  gchar *value;
} RTSPKeyValue;


static void
wfd_rtsp_manager_key_value_foreach (GArray * array, GFunc func, gpointer user_data)
{
  guint i;

  g_return_if_fail (array != NULL);

  for (i = 0; i < array->len; i++) {
    (*func) (&g_array_index (array, RTSPKeyValue, i), user_data);
  }
}

static void
wfd_rtsp_manager_dump_key_value (gpointer data, gpointer user_data G_GNUC_UNUSED)
{
  RTSPKeyValue *key_value = (RTSPKeyValue *) data;

  GST_ERROR (" key: '%s', value: '%s'",
      gst_rtsp_header_as_text (key_value->field), key_value->value);
}

GstRTSPResult
wfd_rtsp_manager_message_dump (GstRTSPMessage * msg)
{
  guint8 *data;
  guint size;

  g_return_val_if_fail (msg != NULL, GST_RTSP_EINVAL);

  GST_ERROR("------------------------------------------------------");
  switch (msg->type) {
    case GST_RTSP_MESSAGE_REQUEST:
      GST_ERROR ("RTSP request message %p", msg);
      GST_ERROR (" request line:");
      GST_ERROR ("   method: '%s'",
          gst_rtsp_method_as_text (msg->type_data.request.method));
      GST_ERROR ("   uri:    '%s'", msg->type_data.request.uri);
      GST_ERROR ("   version: '%s'",
          gst_rtsp_version_as_text (msg->type_data.request.version));
      GST_ERROR (" headers:");
      wfd_rtsp_manager_key_value_foreach (msg->hdr_fields, wfd_rtsp_manager_dump_key_value, NULL);
      GST_ERROR (" body:");
      gst_rtsp_message_get_body (msg, &data, &size);
      //gst_util_dump_mem (data, size);
      if (size > 0) GST_ERROR ("%s(%d)", data, size);
      break;
    case GST_RTSP_MESSAGE_RESPONSE:
      GST_ERROR ("RTSP response message %p", msg);
      GST_ERROR (" status line:");
      GST_ERROR ("   code:   '%d'", msg->type_data.response.code);
      GST_ERROR ("   reason: '%s'", msg->type_data.response.reason);
      GST_ERROR ("   version: '%s'",
          gst_rtsp_version_as_text (msg->type_data.response.version));
      GST_ERROR (" headers:");
      wfd_rtsp_manager_key_value_foreach (msg->hdr_fields, wfd_rtsp_manager_dump_key_value, NULL);
      gst_rtsp_message_get_body (msg, &data, &size);
      GST_ERROR (" body: length %d", size);
      //gst_util_dump_mem (data, size);
      if (size > 0) GST_ERROR ("%s(%d)", data, size);
      break;
    case GST_RTSP_MESSAGE_HTTP_REQUEST:
      GST_ERROR ("HTTP request message %p", msg);
      GST_ERROR (" request line:");
      GST_ERROR ("   method:  '%s'",
          gst_rtsp_method_as_text (msg->type_data.request.method));
      GST_ERROR ("   uri:     '%s'", msg->type_data.request.uri);
      GST_ERROR ("   version: '%s'",
          gst_rtsp_version_as_text (msg->type_data.request.version));
      GST_ERROR (" headers:");
      wfd_rtsp_manager_key_value_foreach (msg->hdr_fields, wfd_rtsp_manager_dump_key_value, NULL);
      GST_ERROR (" body:");
      gst_rtsp_message_get_body (msg, &data, &size);
      //gst_util_dump_mem (data, size);
      if (size > 0) GST_ERROR ("%s(%d)", data, size);
      break;
    case GST_RTSP_MESSAGE_HTTP_RESPONSE:
      GST_ERROR ("HTTP response message %p", msg);
      GST_ERROR (" status line:");
      GST_ERROR ("   code:    '%d'", msg->type_data.response.code);
      GST_ERROR ("   reason:  '%s'", msg->type_data.response.reason);
      GST_ERROR ("   version: '%s'",
          gst_rtsp_version_as_text (msg->type_data.response.version));
      GST_ERROR (" headers:");
      wfd_rtsp_manager_key_value_foreach (msg->hdr_fields, wfd_rtsp_manager_dump_key_value, NULL);
      gst_rtsp_message_get_body (msg, &data, &size);
      GST_ERROR (" body: length %d", size);
      //gst_util_dump_mem (data, size);
      if (size > 0) GST_ERROR ("%s(%d)", data, size);
      break;
    case GST_RTSP_MESSAGE_DATA:
      GST_ERROR ("RTSP data message %p", msg);
      GST_ERROR (" channel: '%d'", msg->type_data.data.channel);
      GST_ERROR (" size:    '%d'", msg->body_size);
      gst_rtsp_message_get_body (msg, &data, &size);
      //gst_util_dump_mem (data, size);
      if (size > 0) GST_ERROR ("%s(%d)", data, size);
      break;
    default:
      GST_ERROR ("unsupported message type %d", msg->type);
      return GST_RTSP_EINVAL;
  }

  GST_ERROR("------------------------------------------------------");
  return GST_RTSP_OK;
}
/*rtsp dump code end*/

static void
wfd_rtsp_manager_class_init (WFDRTSPManagerClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = wfd_rtsp_manager_finalize;
  gobject_class->set_property = wfd_rtsp_manager_set_property;
  gobject_class->get_property = wfd_rtsp_manager_get_property;

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
      g_param_spec_uint64 ("timeout", "UDP Timeout",
          "Fail after timeout microseconds on UDP connections (0 = disabled)",
          0, G_MAXUINT64, DEFAULT_UDP_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ENABLE_PAD_PROBE,
          g_param_spec_boolean ("enable-pad-probe", "Enable Pad Probe",
          "Enable pad probe for debugging",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (wfd_rtsp_manager_debug, "wfdrtspmanager", 0, "WFD RTSP Manager");
}

static void
wfd_rtsp_manager_init (WFDRTSPManager * manager)
{
  GstStructure *s = NULL;

  /* initialize variables */
  manager->do_rtcp = DEFAULT_DO_RTCP;
  manager->latency = DEFAULT_LATENCY_MS;
  manager->udp_buffer_size = DEFAULT_UDP_BUFFER_SIZE;
  manager->udp_timeout = DEFAULT_UDP_TIMEOUT;
  manager->enable_pad_probe = FALSE;

  manager->eos = FALSE;
  manager->discont = TRUE;
  manager->seqbase = GST_CLOCK_TIME_NONE;
  manager->timebase = GST_CLOCK_TIME_NONE;
  manager->is_ipv6 = FALSE;
  manager->caps = gst_caps_new_simple ("application/x-rtp",
                  "media", G_TYPE_STRING, "video", "payload", G_TYPE_INT, 33, NULL);

  s = gst_caps_get_structure (manager->caps, 0);
  gst_structure_set (s, "clock-rate", G_TYPE_INT, 90000, NULL);
  gst_structure_set (s, "encoding-params", G_TYPE_STRING, "MP2T-ES", NULL);

  manager->state_rec_lock = g_new (GStaticRecMutex, 1);
  g_static_rec_mutex_init (manager->state_rec_lock);

  manager->protocol = GST_RTSP_LOWER_TRANS_UDP;
}


static void
wfd_rtsp_manager_finalize (GObject * object)
{
  WFDRTSPManager *manager;
  gint i;

  manager = WFD_RTSP_MANAGER_CAST (object);

  if (manager->caps)
    gst_caps_unref (manager->caps);
  manager->caps = NULL;

  if (manager->conninfo.location)
    g_free (manager->conninfo.location);
  manager->conninfo.location = NULL;

  for (i=0; i <2; i++) {
    if (manager->udpsrc[i]) {
      gst_element_set_state (manager->udpsrc[i], GST_STATE_NULL);
      gst_bin_remove (GST_BIN_CAST (manager->wfdrtspsrc), manager->udpsrc[i]);
      gst_object_unref (manager->udpsrc[i]);
      manager->udpsrc[i] = NULL;
    }
    if (manager->channelpad[i]) {
      gst_object_unref (manager->channelpad[i]);
      manager->channelpad[i] = NULL;
    }
    if (manager->udpsink[i]) {
      gst_element_set_state (manager->udpsink[i], GST_STATE_NULL);
      gst_bin_remove (GST_BIN_CAST (manager->wfdrtspsrc), manager->udpsink[i]);
      gst_object_unref (manager->udpsink[i]);
      manager->udpsink[i] = NULL;
    }
  }
  if (manager->requester) {
    gst_element_set_state (manager->requester, GST_STATE_NULL);
    gst_bin_remove (GST_BIN_CAST (manager->wfdrtspsrc), manager->requester);
    gst_object_unref (manager->requester);
    manager->requester = NULL;
  }
  if (manager->session) {
    gst_element_set_state (manager->session, GST_STATE_NULL);
    gst_bin_remove (GST_BIN_CAST (manager->wfdrtspsrc), manager->session);
    gst_object_unref (manager->session);
    manager->session = NULL;
  }
  if (manager->wfdrtpbuffer) {
    gst_element_set_state (manager->wfdrtpbuffer, GST_STATE_NULL);
    gst_bin_remove (GST_BIN_CAST (manager->wfdrtspsrc), manager->wfdrtpbuffer);
    gst_object_unref (manager->wfdrtpbuffer);
    manager->wfdrtpbuffer = NULL;
  }
  if (manager->fakesrc) {
    gst_element_set_state (manager->fakesrc, GST_STATE_NULL);
    gst_bin_remove (GST_BIN_CAST (manager->wfdrtspsrc), manager->fakesrc);
    gst_object_unref (manager->fakesrc);
    manager->fakesrc = NULL;
  }
  if (manager->srcpad) {
    gst_pad_set_active (manager->srcpad, FALSE);
    gst_element_remove_pad (GST_ELEMENT_CAST (manager->wfdrtspsrc), manager->srcpad);
    manager->srcpad = NULL;
  }
  if (manager->rtcppad) {
    gst_object_unref (manager->rtcppad);
    manager->rtcppad = NULL;
  }
  g_static_rec_mutex_free (manager->state_rec_lock);
  g_free (manager->state_rec_lock);

  G_OBJECT_CLASS (wfd_rtsp_manager_parent_class)->finalize (object);
}


WFDRTSPManager *
wfd_rtsp_manager_new (GstElement *wfdrtspsrc)
{
  WFDRTSPManager *manager;

  g_return_val_if_fail (wfdrtspsrc, NULL);
  g_return_val_if_fail (GST_IS_ELEMENT(wfdrtspsrc), NULL);

  manager = g_object_new (WFD_TYPE_RTSP_MANAGER, NULL);
  if (G_UNLIKELY(!manager)) {
    GST_ERROR("failed to create Wi-Fi Display manager.");
    return NULL;
  }

  manager->wfdrtspsrc = wfdrtspsrc;

  return manager;
}

static void
wfd_rtsp_manager_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  WFDRTSPManager *manager;

  manager = WFD_RTSP_MANAGER_CAST (object);

  switch (prop_id) {
    case PROP_DO_RTCP:
      manager->do_rtcp = g_value_get_boolean (value);
      break;
    case PROP_LATENCY:
      manager->latency = g_value_get_uint (value);
      break;
    case PROP_UDP_BUFFER_SIZE:
      manager->udp_buffer_size = g_value_get_int (value);
      break;
    case PROP_UDP_TIMEOUT:
      manager->udp_timeout = g_value_get_uint64 (value);
      break;
    case PROP_ENABLE_PAD_PROBE:
      manager->enable_pad_probe = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void wfd_rtsp_manager_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  WFDRTSPManager *manager;

  manager = WFD_RTSP_MANAGER_CAST (object);

  switch (prop_id) {
    case PROP_DO_RTCP:
      g_value_set_boolean (value, manager->do_rtcp);
      break;
    case PROP_LATENCY:
      g_value_set_uint (value, manager->latency);
      break;
    case PROP_UDP_BUFFER_SIZE:
      g_value_set_int (value, manager->udp_buffer_size);
      break;
    case PROP_UDP_TIMEOUT:
      g_value_set_uint64 (value, manager->udp_timeout);
      break;
    case PROP_ENABLE_PAD_PROBE:
      g_value_set_boolean (value, manager->enable_pad_probe);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
pad_unblocked (GstPad * pad, gboolean blocked, WFDRTSPManager *manager)
{
  GST_DEBUG_OBJECT (manager, "pad %s:%s unblocked", GST_DEBUG_PAD_NAME (pad));
}


static gboolean
wfd_rtsp_manager_activate (WFDRTSPManager *manager)
{
  GST_DEBUG_OBJECT (manager, "activating streams");

  if (manager->udpsrc[0]) {
    /* remove timeout, we are streaming now and timeouts will be handled by
     * the session manager and jitter buffer */
    g_object_set (G_OBJECT (manager->udpsrc[0]), "timeout", (guint64) 0, NULL);
  }

  if (manager->srcpad) {
    GST_DEBUG_OBJECT (manager, "setting pad caps for manager %p", manager);
    gst_pad_set_caps (manager->srcpad, manager->caps);

    GST_DEBUG_OBJECT (manager, "activating manager pad %p", manager);
    gst_pad_set_active (manager->srcpad, TRUE);
  }

  /* unblock all pads */
  if (manager->blockedpad) {
    GST_DEBUG_OBJECT (manager, "unblocking manager pad %p", manager);
    gst_pad_set_blocked_async (manager->blockedpad, FALSE,
        (GstPadBlockCallback) pad_unblocked, manager);
    manager->blockedpad = NULL;
  }

  return TRUE;
}


static void
pad_blocked (GstPad * pad, gboolean blocked, WFDRTSPManager *manager)
{
  GST_DEBUG_OBJECT (manager, "pad %s:%s blocked, activating streams",
      GST_DEBUG_PAD_NAME (pad));

  /* activate the streams */
//  GST_OBJECT_LOCK (manager);
  if (!manager->need_activate)
    goto was_ok;

  manager->need_activate = FALSE;
//  GST_OBJECT_UNLOCK (manager);

  wfd_rtsp_manager_activate (manager);

  return;

was_ok:
  {
    GST_OBJECT_UNLOCK (manager);
    return;
  }
}


static gboolean
wfd_rtsp_manager_configure_udp (WFDRTSPManager *manager, gint rtpport, gint rtcpport)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstElement *udpsrc0 = NULL, *udpsrc1 = NULL;
  gint tmp_rtp = 0, tmp_rtcp = 0;
  const gchar *host;
  GstCaps *caps;
  GstPad *outpad = NULL;

  if (manager->is_ipv6)
    host = "udp://[::0]";
  else
    host = "udp://0.0.0.0";

  udpsrc0 = gst_element_make_from_uri (GST_URI_SRC, host, NULL);
  if (udpsrc0 == NULL)
    goto no_udp_protocol;

  g_object_set (G_OBJECT (udpsrc0), "port", rtpport, "reuse", FALSE, NULL);

  if (manager->udp_buffer_size != 0) {
    g_object_set (G_OBJECT (udpsrc0), "buffer-size", manager->udp_buffer_size,
        NULL);
    g_object_set (G_OBJECT (udpsrc0), "blocksize", manager->udp_buffer_size,
        NULL);
  }

  caps = gst_caps_new_simple ("application/x-rtp",
                    "media", G_TYPE_STRING, "video", "payload", G_TYPE_INT, 33,
                    "clock-rate", G_TYPE_INT, 90000, "encoding-params", G_TYPE_STRING, "MP2T-ES", NULL);

  g_object_set (udpsrc0, "caps", caps, NULL);
  gst_caps_unref (caps);

  ret = gst_element_set_state (udpsrc0, GST_STATE_PAUSED);
  if (ret == GST_STATE_CHANGE_FAILURE)  {
    GST_DEBUG_OBJECT (manager, "fail to change state udpsrc for RTP");
    goto no_udp_protocol;
  }

  g_object_get (G_OBJECT (udpsrc0), "port", &tmp_rtp, NULL);
  GST_DEBUG_OBJECT (manager, "got RTP port %d", tmp_rtp);

  /* check if port is even */
  if ((tmp_rtp & 0x01) != 0) {
    GST_DEBUG_OBJECT (rtpport, "RTP port not even");
    goto no_ports;
  }

  /* allocate port+1 for RTCP now */
  udpsrc1 = gst_element_make_from_uri (GST_URI_SRC, host, NULL);
  if (udpsrc1 == NULL)
    goto no_udp_rtcp_protocol;

  g_object_set (G_OBJECT (udpsrc1), "port", rtcpport, "reuse", FALSE, NULL);

  GST_DEBUG_OBJECT (manager, "starting RTCP on port %d", rtcpport);
  ret = gst_element_set_state (udpsrc1, GST_STATE_PAUSED);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    GST_DEBUG_OBJECT (manager, "fail to change state udpsrc for RTCP");
    goto no_udp_rtcp_protocol;
  }

  /* all fine, do port check */
  g_object_get (G_OBJECT (udpsrc0), "port", &tmp_rtp, NULL);
  g_object_get (G_OBJECT (udpsrc1), "port", &tmp_rtcp, NULL);

  /* this should not happen... */
  if (rtpport != tmp_rtp || rtcpport != tmp_rtcp)
    goto port_error;

  /* we keep these elements, we configure all in configure_transport when the
   * server told us to really use the UDP ports. */
  manager->udpsrc[0] = gst_object_ref (udpsrc0);
  manager->udpsrc[1] = gst_object_ref (udpsrc1);

  /* they are ours now */
  gst_object_sink (udpsrc0);
  gst_object_sink (udpsrc1);

  if (manager->udpsrc[0]) {
    gst_bin_add (GST_BIN_CAST (manager->wfdrtspsrc), manager->udpsrc[0]);

    GST_DEBUG_OBJECT (manager, "setting up UDP source");

    /* configure a timeout on the UDP port. When the timeout message is
     * posted, we assume UDP transport is not possible. We reconnect using TCP
     * if we can. */
    g_object_set (G_OBJECT (manager->udpsrc[0]), "timeout", manager->udp_timeout,
        NULL);

    GST_DEBUG_OBJECT (manager, "got outpad from udpsrc");
    /* get output pad of the UDP source. */
    outpad = gst_element_get_static_pad (manager->udpsrc[0], "src");

    /* save it so we can unblock */
    manager->blockedpad = outpad;

    /* configure pad block on the pad. As soon as there is dataflow on the
     * UDP source, we know that UDP is not blocked by a firewall and we can
     * configure all the streams to let the application autoplug decoders. */
    gst_pad_set_blocked_async (manager->blockedpad, TRUE,
        (GstPadBlockCallback) pad_blocked, manager);

    /* RTP port */
    if (manager->channelpad[0]) {
      GST_DEBUG_OBJECT (manager, "connecting to session");
      /* configure for UDP delivery, we need to connect the UDP pads to
       * the session plugin. */
      gst_pad_link (outpad, manager->channelpad[0]);
      gst_object_unref (outpad);
      outpad = NULL;
      /* we connected to pad-added signal to get pads from the manager */
    } else {
      GST_DEBUG_OBJECT (manager, "using UDP src pad as output");
    }
  }

  /* RTCP port */
  if (manager->udpsrc[1]) {
    gst_bin_add (GST_BIN_CAST (manager->wfdrtspsrc), manager->udpsrc[1]);

    if (manager->channelpad[1]) {
      GstPad *pad;

      GST_DEBUG_OBJECT (manager, "connecting UDP source 1 to manager");

      pad = gst_element_get_static_pad (manager->udpsrc[1], "src");
      gst_pad_link (pad, manager->channelpad[1]);
      gst_object_unref (pad);
    } else {
      /* leave unlinked */
    }
  }

  return TRUE;

  /* ERRORS */
no_udp_protocol:
  {
    GST_DEBUG_OBJECT (manager, "could not get UDP source");
    goto cleanup;
  }
no_ports:
  {
    GST_DEBUG_OBJECT (manager, "could not allocate UDP port pair");
    goto cleanup;
  }
no_udp_rtcp_protocol:
  {
    GST_DEBUG_OBJECT (manager, "could not get UDP source for RTCP");
    goto cleanup;
  }
port_error:
  {
    GST_DEBUG_OBJECT (manager, "ports don't match rtp: %d<->%d, rtcp: %d<->%d",
        tmp_rtp, rtpport, tmp_rtcp, rtcpport);
    goto cleanup;
  }
cleanup:
  {
    if (udpsrc0) {
      gst_element_set_state (udpsrc0, GST_STATE_NULL);
      gst_object_unref (udpsrc0);
    }
    if (udpsrc1) {
      gst_element_set_state (udpsrc1, GST_STATE_NULL);
      gst_object_unref (udpsrc1);
    }
    return FALSE;
  }
}

static void
gst_wfdrtspsrc_get_transport_info (WFDRTSPManager * manager,
    GstRTSPTransport * transport, const gchar ** destination, gint * min, gint * max)
{
  g_return_if_fail (transport);
  g_return_if_fail (transport->lower_transport == GST_RTSP_LOWER_TRANS_UDP);

  if (destination) {
    /* first take the source, then the endpoint to figure out where to send
     * the RTCP. */
    if (!(*destination = transport->source)) {
      if (manager->control_connection )
        *destination = gst_rtsp_connection_get_ip (manager->control_connection );
      else if (manager->conninfo.connection)
        *destination =
            gst_rtsp_connection_get_ip (manager->conninfo.connection);
    }
  }
  if (min && max) {
    /* for unicast we only expect the ports here */
    *min = transport->server_port.min;
    *max = transport->server_port.max;
  }
}


/* configure the UDP sink back to the server for status reports */
static gboolean
wfd_rtsp_manager_configure_udp_sinks ( WFDRTSPManager * manager, GstRTSPTransport * transport)
{
  GstPad *pad = NULL;
  gint rtp_port = -1, rtcp_port = -1, sockfd = -1;
  gboolean do_rtp = FALSE, do_rtcp = FALSE;
  const gchar *destination =  NULL;
  gchar *uri = NULL;

  /* get transport info */
  gst_wfdrtspsrc_get_transport_info (manager, transport, &destination,
      &rtp_port, &rtcp_port);

  /* see what we need to do */
  do_rtp = (rtp_port != -1);
  /* it's possible that the server does not want us to send RTCP in which case
   * the port is -1 */
  do_rtcp = (rtcp_port != -1 && manager->session != NULL && manager->do_rtcp);

  /* we need a destination when we have RTP or RTCP ports */
  if (destination == NULL && (do_rtp || do_rtcp))
    goto no_destination;

  /* try to construct the fakesrc to the RTP port of the server to open up any
   * NAT firewalls */
  if (do_rtp) {
    GST_DEBUG_OBJECT (manager, "configure RTP UDP sink for %s:%d", destination,
        rtp_port);

    uri = g_strdup_printf ("udp://%s:%d", destination, rtp_port);
    manager->udpsink[0] = gst_element_make_from_uri (GST_URI_SINK, uri, NULL);
    g_free (uri);
    if (manager->udpsink[0] == NULL)
      goto no_sink_element;

    /* don't join multicast group, we will have the source socket do that */
    /* no sync or async state changes needed */
    g_object_set (G_OBJECT (manager->udpsink[0]), "auto-multicast", FALSE,
        "loop", FALSE, "sync", FALSE, "async", FALSE, NULL);

    if (manager->udpsrc[0]) {
      /* configure socket, we give it the same UDP socket as the udpsrc for RTP
       * so that NAT firewalls will open a hole for us */
      g_object_get (G_OBJECT (manager->udpsrc[0]), "sock", &sockfd, NULL);
      GST_DEBUG_OBJECT (manager, "RTP UDP src has sock %d", sockfd);
      /* configure socket and make sure udpsink does not close it when shutting
       * down, it belongs to udpsrc after all. */
      g_object_set (G_OBJECT (manager->udpsink[0]), "sockfd", sockfd,
          "closefd", FALSE, NULL);
    }

    /* the source for the dummy packets to open up NAT */
    manager->fakesrc = gst_element_factory_make ("fakesrc", NULL);
    if (manager->fakesrc == NULL)
      goto no_fakesrc_element;

    /* random data in 5 buffers, a size of 200 bytes should be fine */
    g_object_set (G_OBJECT (manager->fakesrc), "filltype", 3, "num-buffers", 5,
        "sizetype", 2, "sizemax", 200, "silent", TRUE, NULL);

    /* we don't want to consider this a sink */
    GST_OBJECT_FLAG_UNSET (manager->udpsink[0], GST_ELEMENT_IS_SINK);

    /* keep everything locked */
    gst_element_set_locked_state (manager->udpsink[0], TRUE);
    gst_element_set_locked_state (manager->fakesrc, TRUE);

    gst_object_ref (manager->udpsink[0]);
    gst_bin_add (GST_BIN_CAST (manager->wfdrtspsrc), manager->udpsink[0]);
    gst_object_ref (manager->fakesrc);
    gst_bin_add (GST_BIN_CAST (manager->wfdrtspsrc), manager->fakesrc);

    gst_element_link (manager->fakesrc, manager->udpsink[0]);
  }
  if (do_rtcp) {
    GST_DEBUG_OBJECT (manager, "configure RTCP UDP sink for %s:%d", destination,
        rtcp_port);

    uri = g_strdup_printf ("udp://%s:%d", destination, rtcp_port);
    manager->udpsink[1] = gst_element_make_from_uri (GST_URI_SINK, uri, NULL);
    g_free (uri);
    if (manager->udpsink[1] == NULL)
      goto no_sink_element;

    /* don't join multicast group, we will have the source socket do that */
    /* no sync or async state changes needed */
    g_object_set (G_OBJECT (manager->udpsink[1]), "auto-multicast", FALSE,
        "loop", FALSE, "sync", FALSE, "async", FALSE, NULL);

    if (manager->udpsrc[1]) {
      /* configure socket, we give it the same UDP socket as the udpsrc for RTCP
       * because some servers check the port number of where it sends RTCP to identify
       * the RTCP packets it receives */
      g_object_get (G_OBJECT (manager->udpsrc[1]), "sock", &sockfd, NULL);
      GST_DEBUG_OBJECT (manager, "RTCP UDP src has sock %d", sockfd);
      /* configure socket and make sure udpsink does not close it when shutting
       * down, it belongs to udpsrc after all. */
      g_object_set (G_OBJECT (manager->udpsink[1]), "sockfd", sockfd,
          "closefd", FALSE, NULL);
    }

    /* we don't want to consider this a sink */
    GST_OBJECT_FLAG_UNSET (manager->udpsink[1], GST_ELEMENT_IS_SINK);

    /* we keep this playing always */
    gst_element_set_locked_state (manager->udpsink[1], TRUE);
    gst_element_set_state (manager->udpsink[1], GST_STATE_PLAYING);

    gst_object_ref (manager->udpsink[1]);
    gst_bin_add (GST_BIN_CAST (manager->wfdrtspsrc), manager->udpsink[1]);

    manager->rtcppad = gst_element_get_static_pad (manager->udpsink[1], "sink");

    /* get session RTCP pad */
    pad = gst_element_get_request_pad (manager->session, "send_rtcp_src");

    /* and link */
    if (pad) {
      gst_pad_link (pad, manager->rtcppad);
      gst_object_unref (pad);
    }
  }

  return TRUE;

  /* ERRORS */
no_destination:
  {
    GST_DEBUG_OBJECT (manager, "no destination address specified");
    return FALSE;
  }
no_sink_element:
  {
    GST_DEBUG_OBJECT (manager, "no UDP sink element found");
    return FALSE;
  }
no_fakesrc_element:
  {
    GST_DEBUG_OBJECT (manager, "no fakesrc element found");
    return FALSE;
  }
}

static void
on_bye_ssrc (GObject * session, GObject * source, WFDRTSPManager * manager)
{
  GST_DEBUG_OBJECT (manager, "source in session received BYE");

  //gst_wfdrtspsrc_do_stream_eos (src, manager);
}

static void
on_new_ssrc (GObject * session, GObject * source, WFDRTSPManager * manager)
{
  GST_DEBUG_OBJECT (manager, "source in session received NEW");
}

static void
on_timeout (GObject * session, GObject * source, WFDRTSPManager * manager)
{
  GST_DEBUG_OBJECT (manager, "source in session timed out");

  //gst_wfdrtspsrc_do_stream_eos (src, manager);
}

static void
on_ssrc_active (GObject * session, GObject * source, WFDRTSPManager * manager)
{
  GST_DEBUG_OBJECT (manager, "source in session  is active");
}


static GstCaps *
request_pt_map_for_wfdrtpbuffer (GstElement * wfdrtpbuffer, guint pt, WFDRTSPManager * manager)
{
  GstCaps *caps;

  if (!manager)
    goto unknown_stream;

  GST_DEBUG_OBJECT (manager, "getting pt map for pt %d", pt);

  WFD_RTSP_MANAGER_STATE_LOCK (manager);

  caps = manager->caps;
  if (caps)
    gst_caps_ref (caps);
  WFD_RTSP_MANAGER_STATE_UNLOCK (manager);

  return caps;

unknown_stream:
  {
    GST_ERROR ( " manager is NULL");
    return NULL;
  }
}

static GstCaps *
request_pt_map_for_session (GstElement * session, guint session_id, guint pt, WFDRTSPManager * manager)
{
  GstCaps *caps;

  GST_DEBUG_OBJECT (manager, "getting pt map for pt %d in session %d", pt, session_id);

  WFD_RTSP_MANAGER_STATE_LOCK (manager);
  caps = manager->caps;
  if (caps)
    gst_caps_ref (caps);
  WFD_RTSP_MANAGER_STATE_UNLOCK (manager);

  return caps;
}


static void
wfd_rtsp_manager_do_stream_eos (WFDRTSPManager * manager)
{
  GST_DEBUG_OBJECT (manager, "setting for session  to EOS");

  if (manager->eos)
    goto was_eos;

  manager->eos = TRUE;
  //gst_wfdrtspsrc_stream_push_event (src, manager, gst_event_new_eos (), TRUE);
  return;

  /* ERRORS */
was_eos:
  {
    GST_DEBUG_OBJECT (manager, "already EOS");
    return;
  }
}

static gboolean
wfd_rtsp_manager_set_manager (WFDRTSPManager * manager)
{
  g_return_val_if_fail (manager, FALSE);

  if (manager->session) {
    g_signal_connect (manager->session, "on-bye-ssrc", (GCallback) on_bye_ssrc,
        manager);
    g_signal_connect (manager->session, "on-bye-timeout", (GCallback) on_timeout,
        manager);
    g_signal_connect (manager->session, "on-timeout", (GCallback) on_timeout,
        manager);
    g_signal_connect (manager->session, "on-ssrc-active",
        (GCallback) on_ssrc_active, manager);
    g_signal_connect (manager->session, "on-new-ssrc", (GCallback) on_new_ssrc,
        manager);
   g_signal_connect (manager->session, "request-pt-map",
        (GCallback) request_pt_map_for_session, manager);

    g_object_set (G_OBJECT(manager->session), "rtcp-min-interval", (guint64)1000000000, NULL);
  }

  if (manager->wfdrtpbuffer != NULL) {
    /* configure latency and packet lost */
    g_object_set (manager->wfdrtpbuffer, "latency", manager->latency, NULL);

    g_signal_connect (manager->wfdrtpbuffer, "request-pt-map",
	 (GCallback) request_pt_map_for_wfdrtpbuffer, manager);
  }

  return TRUE;
}


gboolean
wfd_rtsp_manager_configure_transport (WFDRTSPManager * manager,
    GstRTSPTransport * transport, gint rtpport, gint rtcpport)
{
  GstStructure *s;
  const gchar *mime;

  GST_DEBUG_OBJECT (manager, "configuring transport");

  s = gst_caps_get_structure (manager->caps, 0);

  /* get the proper mime type for this manager now */
  if (gst_rtsp_transport_get_mime (transport->trans, &mime) < 0)
    goto unknown_transport;
  if (!mime)
    goto unknown_transport;

  /* configure the final mime type */
  GST_DEBUG_OBJECT (manager, "setting mime to %s", mime);
  gst_structure_set_name (s, mime);

  if (!wfd_rtsp_manager_set_manager (manager))
    goto no_manager;

  switch (transport->lower_transport) {
    case GST_RTSP_LOWER_TRANS_TCP:
    case GST_RTSP_LOWER_TRANS_UDP_MCAST:
     goto transport_failed;
    case GST_RTSP_LOWER_TRANS_UDP:
      if (!wfd_rtsp_manager_configure_udp (manager, rtpport, rtcpport))
        goto transport_failed;
      if (!wfd_rtsp_manager_configure_udp_sinks (manager, transport))
        goto transport_failed;
      break;
    default:
      goto unknown_transport;
  }

  manager->need_activate = TRUE;
  manager->protocol = transport->lower_transport;

  return TRUE;

  /* ERRORS */
transport_failed:
  {
    GST_DEBUG_OBJECT (manager, "failed to configure transport");
    return FALSE;
  }
unknown_transport:
  {
    GST_DEBUG_OBJECT (manager, "unknown transport");
    return FALSE;
  }
no_manager:
  {
    GST_DEBUG_OBJECT (manager, "cannot get a session manager");
    return FALSE;
  }
}

static gboolean
wfd_rtsp_manager_push_event (WFDRTSPManager * manager, GstEvent * event, gboolean source)
{
  gboolean res = TRUE;

  /* only wfdrtspsrcs that have a connection to the outside world */
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

done:
  gst_event_unref (event);

  return res;
}

/*static*/ void
wfd_rtsp_manager_flush (WFDRTSPManager * manager, gboolean flush)
{
  GstEvent *event = NULL;
  GstClock *clock = NULL;
  GstClockTime base_time = GST_CLOCK_TIME_NONE;
  gint i;

  if (flush) {
    event = gst_event_new_flush_start ();
    GST_DEBUG_OBJECT(manager, "start flush");
   } else {
    event = gst_event_new_flush_stop ();
    GST_DEBUG_OBJECT(manager, "stop flush");
    clock = gst_element_get_clock (GST_ELEMENT_CAST (manager->wfdrtspsrc));
    if (clock) {
      base_time = gst_clock_get_time (clock);
      gst_object_unref (clock);
    }
  }

  if(flush) {
    GST_DEBUG_OBJECT (manager, "need to pause udpsrc");

    for (i=0; i<2; i++) {
      if (manager->udpsrc[i]) {
        gst_element_set_locked_state (manager->udpsrc[i], TRUE);
        gst_element_set_state (manager->udpsrc[i], GST_STATE_PAUSED);
      }
    }
  }

  wfd_rtsp_manager_push_event (manager, event, FALSE);

  if (manager->session)
    gst_element_set_base_time (GST_ELEMENT_CAST (manager->session), base_time);
  if (manager->requester)
    gst_element_set_base_time (GST_ELEMENT_CAST (manager->requester), base_time);
  if (manager->wfdrtpbuffer)
    gst_element_set_base_time (GST_ELEMENT_CAST (manager->wfdrtpbuffer), base_time);

  /* make running time start start at 0 again */
  for (i = 0; i < 2; i++) {
    if (manager->udpsrc[i]) {
      if (base_time != GST_CLOCK_TIME_NONE)
        gst_element_set_base_time (manager->udpsrc[i], base_time);
    }
  }

  /* for tcp interleaved case */
  if (base_time != GST_CLOCK_TIME_NONE)
    gst_element_set_base_time (GST_ELEMENT_CAST (manager->wfdrtspsrc), base_time);

  if(!flush) {
    GST_DEBUG_OBJECT (manager, "need to run udpsrc");

    for (i=0; i<2; i++) {
      if (manager->udpsrc[i]) {
        gst_element_set_locked_state (manager->udpsrc[i], FALSE);
        gst_element_set_state (manager->udpsrc[i], GST_STATE_PLAYING);
      }
    }
  }
}

static gboolean
wfd_rtsp_manager_pad_probe_cb(GstPad * pad, GstMiniObject * object, gpointer u_data)
{
  GstElement* parent = NULL;

  parent = (GstElement*)gst_object_get_parent(GST_OBJECT(pad));

  if (GST_IS_EVENT (object)) {
    GstEvent *event = GST_EVENT_CAST(object);

    /* show name and event type */
    GST_DEBUG_OBJECT(WFD_RTSP_MANAGER (u_data), "EVENT PROBE : %s:%s :  %s\n",
      GST_STR_NULL(GST_ELEMENT_NAME(parent)), GST_STR_NULL(GST_PAD_NAME(pad)),
      GST_EVENT_TYPE_NAME(event));

    if (GST_EVENT_TYPE (event) == GST_EVENT_NEWSEGMENT) {
      gint64 start, stop, time;

      gst_event_parse_new_segment_full (event, NULL, NULL, NULL, NULL, &start, &stop, &time);

      GST_DEBUG_OBJECT (WFD_RTSP_MANAGER (u_data), "NEWSEGMENT : %" G_GINT64_FORMAT " -- %" G_GINT64_FORMAT ", time %" G_GINT64_FORMAT " \n",
      start, stop, time);
    }
  }
  else if  (GST_IS_BUFFER(object)) {
    GstBuffer *buffer = GST_BUFFER_CAST(object);

    /* show name and timestamp */
    GST_DEBUG_OBJECT(WFD_RTSP_MANAGER (u_data), "BUFFER PROBE : %s:%s :  %u:%02u:%02u.%09u  (%d bytes)\n",
        GST_STR_NULL(GST_ELEMENT_NAME(parent)), GST_STR_NULL(GST_PAD_NAME(pad)),
        GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buffer)), GST_BUFFER_SIZE(buffer));
  }

  if ( parent ) {
    gst_object_unref(parent);
  }

  return TRUE;
}

void wfd_rtsp_manager_enable_pad_probe(WFDRTSPManager * manager)
{
  GstPad * pad = NULL;

  g_return_if_fail(manager);

  if(manager->udpsrc[0]) {
    pad = NULL;
    pad = gst_element_get_static_pad(manager->udpsrc[0], "src");
    if (pad) {
      if( gst_pad_add_data_probe(pad, G_CALLBACK(wfd_rtsp_manager_pad_probe_cb), (gpointer)manager))
        GST_DEBUG_OBJECT (manager, "added pad probe (pad : %s, element : %s)", gst_pad_get_name(pad), gst_element_get_name(manager->udpsrc[0]));
      gst_object_unref (pad);
      pad = NULL;
    }
  }

  if(manager->session) {
    pad = gst_element_get_static_pad(manager->session, "recv_rtp_src");
    if (pad) {
      if( gst_pad_add_data_probe(pad, G_CALLBACK(wfd_rtsp_manager_pad_probe_cb), (gpointer)manager))
        GST_DEBUG_OBJECT (manager, "added pad probe (pad : %s, element : %s)", gst_pad_get_name(pad), gst_element_get_name(manager->session));
      gst_object_unref (pad);
      pad = NULL;
    }
  }

  if(manager->requester) {
    pad = gst_element_get_static_pad(manager->requester, "rtp_sink");
    if (pad) {
      if( gst_pad_add_data_probe(pad, G_CALLBACK(wfd_rtsp_manager_pad_probe_cb), (gpointer)manager))
        GST_DEBUG_OBJECT (manager, "added pad probe (pad : %s, element : %s)", gst_pad_get_name(pad), gst_element_get_name(manager->requester));
      gst_object_unref (pad);
      pad = NULL;
    }

    pad = gst_element_get_static_pad(manager->requester, "rtp_src");
    if (pad) {
      if( gst_pad_add_data_probe(pad, G_CALLBACK(wfd_rtsp_manager_pad_probe_cb), (gpointer)manager))
        GST_DEBUG_OBJECT (manager, "added pad probe (pad : %s, element : %s)", gst_pad_get_name(pad), gst_element_get_name(manager->requester));
      gst_object_unref (pad);
      pad = NULL;
    }
  }

  if(manager->wfdrtpbuffer) {
    pad = gst_element_get_static_pad(manager->wfdrtpbuffer, "sink");
    if (pad) {
      if( gst_pad_add_data_probe(pad, G_CALLBACK(wfd_rtsp_manager_pad_probe_cb), (gpointer)manager))
        GST_DEBUG_OBJECT (manager, "added pad probe (pad : %s, element : %s)", gst_pad_get_name(pad), gst_element_get_name(manager->wfdrtpbuffer));
      gst_object_unref (pad);
      pad = NULL;
    }

    pad = gst_element_get_static_pad(manager->wfdrtpbuffer, "src");
    if (pad) {
      if( gst_pad_add_data_probe(pad, G_CALLBACK(wfd_rtsp_manager_pad_probe_cb), (gpointer)manager))
        GST_DEBUG_OBJECT (manager, "added pad probe (pad : %s, element : %s)", gst_pad_get_name(pad), gst_element_get_name(manager->wfdrtpbuffer));
      gst_object_unref (pad);
      pad = NULL;
    }
  }
}

