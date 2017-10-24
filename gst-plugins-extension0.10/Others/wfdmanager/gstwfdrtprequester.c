/*
 * wfdrtprequester
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
#include <config.h>
#endif

#include <gst/gst.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtcpbuffer.h>
#include <string.h>

#include "gstwfdrtprequester.h"

GST_DEBUG_CATEGORY_STATIC (wfdrtprequester_debug);
#define GST_CAT_DEFAULT (wfdrtprequester_debug)

/* signals and args */
enum
{
  SIGNAL_REQUEST_IDR,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_DO_REQUEST,
  PROP_SSRC,
  PROP_PT,
  PROP_TIMEOUT
};

#define DEFAULT_DO_REQUEST TRUE
#define DEFAULT_SSRC 0x0000
#define DEFAULT_PT 33 //MP2t-ES payload type
#define DEFAULT_TIMEOUT_MS      200

 /* sink pads */
static GstStaticPadTemplate rtp_sink_template =
GST_STATIC_PAD_TEMPLATE ("rtp_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

static GstStaticPadTemplate retransmitted_rtp_sink_template =
GST_STATIC_PAD_TEMPLATE ("retransmitted_rtp_sink",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("application/x-rtp")
    );

/* src pads */
static GstStaticPadTemplate rtp_src_template =
GST_STATIC_PAD_TEMPLATE ("rtp_src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

static GstStaticPadTemplate rtcp_src_template =
GST_STATIC_PAD_TEMPLATE ("rtcp_src",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("application/x-rtcp")
    );


#define GST_WFD_RTP_REQUESTER_GET_PRIVATE(obj)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_WFD_RTP_REQUESTER, GstWFDRTPRequesterPrivate))

#define GST_WFD_RTP_REQUESTER_LOCK(requester)   g_mutex_lock ((requester)->priv->lock)
#define GST_WFD_RTP_REQUESTER_UNLOCK(requester) g_mutex_unlock ((requester)->priv->lock)

struct _GstWFDRTPRequesterPrivate
{
  GMutex *lock;

  gboolean flushing;

  /* the next expected seqnum we receive */
  guint64 next_in_seqnum;

  /* for checking requeting timeout */
  GList *request_ranges;
  GTimer *timer;
  guint timeout_checker_id;
};

typedef struct _GstWFDRTPRequestRange GstWFDRTPRequestRange;
struct _GstWFDRTPRequestRange
{
  guint16 start_seqnum;
  guint16 end_seqnum;

  guint request_packet_num;
  guint retransmitted_packet_num;

  gdouble request_time;
};


/*
 * The maximum number of missing packets we tollerate. These are packets with a
 * sequence number bigger than the last seen packet.
 */
#define REQUESTER_MAX_DROPOUT      17
/*
 * The maximum number of misordered packets we tollerate. These are packets with
 * a sequence number smaller than the last seen packet.
 */
#define REQUESTER_MAX_MISORDER     16

#define REQUESTER_MTU_SIZE        1400 /* bytes */


//#define gst_wfd_rtp_requester_parent_class parent_class

/* GObject vmethods */
static void gst_wfd_rtp_requester_finalize (GObject * object);
static void gst_wfd_rtp_requester_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_wfd_rtp_requester_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_wfd_rtp_requester_sink_event (GstPad * pad, GstEvent * event);
static gboolean gst_wfd_rtp_requester_sink_setcaps (GstPad * pad, GstCaps * caps);
static gboolean gst_wfd_rtp_requester_sink_event_retransmitted_rtp (GstPad * pad, GstEvent * event);
static gboolean gst_wfd_rtp_requester_src_event_rtcp (GstPad * pad, GstEvent * event);

/* GstElement vmethods */
static GstFlowReturn gst_wfd_rtp_requester_chain (GstPad * pad, GstBuffer * buf);
static GstPad *gst_wfd_rtp_requester_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name);
static void gst_wfd_rtp_requester_release_pad (GstElement * element, GstPad * pad);

static GstFlowReturn gst_wfd_rtp_requester_chain_retransmitted_rtp (GstPad * pad, GstBuffer * buf);

static void gst_wfd_rtp_requester_flush_start (GstWFDRTPRequester * requester);
static void gst_wfd_rtp_requester_flush_stop (GstWFDRTPRequester * requester);


static guint gst_wfd_rtp_requester_signals[LAST_SIGNAL] = { 0 };

/* GObject vmethod implementations */
static void
_do_init (GType wfdrtprequester_type)
{
  GST_DEBUG_CATEGORY_INIT (wfdrtprequester_debug, "wfdrtprequester", 0, "WFD RTP Requester");
}

GST_BOILERPLATE_FULL (GstWFDRTPRequester, gst_wfd_rtp_requester, GstElement, GST_TYPE_ELEMENT, _do_init);

static void
gst_wfd_rtp_requester_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  /* sink pads */
  gst_element_class_add_static_pad_template (element_class,
     &rtp_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &retransmitted_rtp_sink_template);
  /* src pads */
  gst_element_class_add_static_pad_template (element_class,
      &rtp_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &rtcp_src_template);


  gst_element_class_set_details_simple (element_class,
    "Wi-Fi Display RTP Request Retransmission Element",
    "Filter/Network/RTP",
    "Receive RTP packet and request RTP retransmission",
    "YeJin Cho <cho.yejin@samsung.com>");
}


/* initialize the plugin's class */
static void
gst_wfd_rtp_requester_class_init (GstWFDRTPRequesterClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  g_type_class_add_private (klass, sizeof (GstWFDRTPRequesterPrivate));

  gobject_class->finalize = gst_wfd_rtp_requester_finalize;
  gobject_class->set_property = gst_wfd_rtp_requester_set_property;
  gobject_class->get_property = gst_wfd_rtp_requester_get_property;

  g_object_class_install_property (gobject_class, PROP_DO_REQUEST,
      g_param_spec_boolean ("do-request", "Do request",
          "Request RTP retransmission",
          DEFAULT_DO_REQUEST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SSRC,
      g_param_spec_uint ("ssrc", "SSRC",
          "The SSRC of the RTP packets", 0, G_MAXUINT32,
          DEFAULT_SSRC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PT,
      g_param_spec_uint ("pt", "payload type",
          "The payload type of the RTP packets", 0, 0x80, DEFAULT_PT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWFDRTPRequester::timeout:
   *
   * The maximum timeout of the requester. Packets should be retransmitted
   * at most this time. Unless, the request-timeout signal will be emit.
   */
  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_uint ("timeout", "Retransmssion timeout in ms",
          "Timeout in ms for retransmission", 0, G_MAXUINT, DEFAULT_TIMEOUT_MS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWFDRTPRequester::request-timeout:
   * @req: a #GstWFDRTPRequester
   *
   * Notify of timeout which is requesting rtp retransmission.
   */
  gst_wfd_rtp_requester_signals[SIGNAL_REQUEST_IDR] =
      g_signal_new ("request-idr", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (GstWFDRTPRequesterClass, request_idr), NULL, NULL,
      gst_marshal_VOID__VOID, G_TYPE_NONE, 0, G_TYPE_NONE);

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_wfd_rtp_requester_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_wfd_rtp_requester_release_pad);

}

static void
gst_wfd_rtp_requester_init (GstWFDRTPRequester * requester, GstWFDRTPRequesterClass * g_class)
{
  requester->priv = GST_WFD_RTP_REQUESTER_GET_PRIVATE(requester);
  requester->priv->lock = g_mutex_new ();
  requester->priv->flushing = FALSE;
  requester->priv->next_in_seqnum = GST_CLOCK_TIME_NONE;
  requester->priv->timer = NULL;
  requester->priv->timeout_checker_id = 0;
  requester->priv->request_ranges = NULL;

  requester->rtp_sink = gst_pad_new_from_static_template (&rtp_sink_template, "rtp_sink");
  gst_pad_set_event_function (requester->rtp_sink,
                              GST_DEBUG_FUNCPTR(gst_wfd_rtp_requester_sink_event));
  gst_pad_set_chain_function (requester->rtp_sink,
                              GST_DEBUG_FUNCPTR(gst_wfd_rtp_requester_chain));
 gst_pad_set_setcaps_function (requester->rtp_sink,
      GST_DEBUG_FUNCPTR (gst_wfd_rtp_requester_sink_setcaps));
  gst_element_add_pad (GST_ELEMENT (requester), requester->rtp_sink);

  requester->rtp_src = gst_pad_new_from_static_template (&rtp_src_template, "rtp_src");
  gst_element_add_pad (GST_ELEMENT (requester), requester->rtp_src);

  requester->do_request = DEFAULT_DO_REQUEST;
  requester->ssrc = DEFAULT_SSRC;
  requester->pt = DEFAULT_PT;
  requester->timeout_ms = DEFAULT_TIMEOUT_MS;
  requester->timeout_ns = requester->timeout_ms * GST_MSECOND;

  return;
}

static void
gst_wfd_rtp_requester_finalize (GObject * object)
{
  GstWFDRTPRequester *requester = GST_WFD_RTP_REQUESTER (object);
  GList *walk;

  requester = GST_WFD_RTP_REQUESTER (object);

  if (requester->priv->lock) {
    g_mutex_free(requester->priv->lock);
    requester->priv->lock = NULL;
  }

  for (walk = requester->priv->request_ranges; walk; walk = g_list_next (walk)) {
    GstWFDRTPRequestRange *range = (GstWFDRTPRequestRange *) walk->data;
    g_free (range);
  }
  g_list_free (requester->priv->request_ranges);
  requester->priv->request_ranges = NULL;

  if (requester->priv->timeout_checker_id)
    g_source_remove (requester->priv->timeout_checker_id);
  requester->priv->timeout_checker_id = 0;

  if (requester->priv->timer)
    g_timer_destroy (requester->priv->timer);
  requester->priv->timer = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_wfd_rtp_requester_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWFDRTPRequester *requester = GST_WFD_RTP_REQUESTER (object);

  switch (prop_id) {
    case PROP_DO_REQUEST:
      requester->do_request = g_value_get_boolean (value);
      break;
    case PROP_SSRC:
      requester->ssrc = g_value_get_uint (value);
      break;
    case PROP_PT:
      requester->pt = g_value_get_uint (value);
      break;
    case PROP_TIMEOUT:
      requester->timeout_ms = g_value_get_uint (value);
      requester->timeout_ns = requester->timeout_ms * GST_MSECOND;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_wfd_rtp_requester_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWFDRTPRequester *requester = GST_WFD_RTP_REQUESTER (object);

  switch (prop_id) {
    case PROP_DO_REQUEST:
      g_value_set_boolean (value, requester->do_request);
      break;
    case PROP_SSRC:
        g_value_set_uint (value, requester->ssrc);
      break;
    case PROP_PT:
      g_value_set_uint (value, requester->pt);
      break;
    case PROP_TIMEOUT:
      g_value_set_uint (value, requester->timeout_ms);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_wfd_rtp_requester_sink_setcaps (GstPad * pad, GstCaps * caps)
{
  GstWFDRTPRequester *requester;
  gboolean res = TRUE;

  requester = GST_WFD_RTP_REQUESTER (gst_pad_get_parent (pad));

  /* set same caps on srcpad on success */
  gst_pad_set_caps (requester->rtp_src, caps);

  gst_object_unref (requester);

  return res;
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_wfd_rtp_requester_sink_event (GstPad * pad, GstEvent * event)
{
  gboolean ret = FALSE;
  GstWFDRTPRequester *requester = NULL;

  requester = GST_WFD_RTP_REQUESTER_CAST (gst_pad_get_parent (pad));
  if(requester == NULL)
  {
    GST_ERROR_OBJECT(requester, "requester is NULL.");
    return ret;
  }

  GST_DEBUG_OBJECT (requester, "requester got event %s", GST_EVENT_TYPE_NAME(event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      gst_wfd_rtp_requester_flush_start (requester);
      ret = gst_pad_push_event (requester->rtp_src, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      ret = gst_pad_push_event (requester->rtp_src, event);
      gst_wfd_rtp_requester_flush_stop (requester);
      break;
    default:
      ret = gst_pad_event_default (pad, event);
      break;
  }

  gst_object_unref (requester);

  return ret;
}

static void
gst_wfd_rtp_requester_flush_start (GstWFDRTPRequester * requester)
{
  GstWFDRTPRequesterPrivate *priv;

  priv = GST_WFD_RTP_REQUESTER_GET_PRIVATE(requester);

  GST_WFD_RTP_REQUESTER_LOCK(requester);

  /* mark ourselves as flushing */
  priv->flushing = TRUE;
  GST_DEBUG_OBJECT (requester, "flush start..");

  GST_WFD_RTP_REQUESTER_UNLOCK(requester);
}

static void
gst_wfd_rtp_requester_flush_stop (GstWFDRTPRequester * requester)
{
  GstWFDRTPRequesterPrivate *priv;
  GList *walk;

  priv = GST_WFD_RTP_REQUESTER_GET_PRIVATE(requester);

  GST_WFD_RTP_REQUESTER_LOCK(requester);

  GST_DEBUG_OBJECT (requester, "flush stop...");
  /* Mark as non flushing */
  priv->flushing = FALSE;

  priv->next_in_seqnum = GST_CLOCK_TIME_NONE;

  for (walk = priv->request_ranges; walk; walk = g_list_next (walk)) {
    GstWFDRTPRequestRange *range = (GstWFDRTPRequestRange *) walk->data;
    g_free (range);
  }
  g_list_free (priv->request_ranges);
  priv->request_ranges = NULL;

  requester->ssrc = 0;
  requester->pt = 0;

  GST_WFD_RTP_REQUESTER_UNLOCK(requester);
}

static gboolean
gst_wfd_rtp_requester_sink_event_retransmitted_rtp (GstPad * pad, GstEvent * event)
{
  gboolean ret;
  GstWFDRTPRequester * requester;

  requester = GST_WFD_RTP_REQUESTER_CAST (gst_pad_get_parent (pad));

  GST_DEBUG_OBJECT (requester, "requester got event %s", GST_EVENT_TYPE_NAME(event));

  switch (GST_EVENT_TYPE (event)) {
    default:
      ret = gst_pad_event_default (pad, event);
      break;
  }

  gst_object_unref (requester);

  return ret;
}

static gboolean
gst_wfd_rtp_requester_src_event_rtcp (GstPad * pad, GstEvent * event)
{
  GstWFDRTPRequester * requester;
  gboolean ret;

  requester = GST_WFD_RTP_REQUESTER_CAST (gst_pad_get_parent (pad));

  GST_DEBUG_OBJECT (requester, "requester got event %s", GST_EVENT_TYPE_NAME(event));

  switch (GST_EVENT_TYPE (event)) {
    default:
      ret = gst_pad_event_default (pad, event);
      break;
  }

  gst_object_unref (requester);
  return ret;
}

static gboolean
check_timeout (gpointer *data)
{
  GstWFDRTPRequester *requester = (GstWFDRTPRequester*)data;
  GstWFDRTPRequesterPrivate *priv;
  gboolean is_timeout = FALSE;
  gboolean ret = TRUE;
  GList *walk;
  gdouble elapsed;

  priv = GST_WFD_RTP_REQUESTER_GET_PRIVATE(requester);
  GST_WFD_RTP_REQUESTER_LOCK(requester);

  if (priv->flushing) {
    if (!g_source_remove (priv->timeout_checker_id))
      GST_DEBUG_OBJECT (requester, "fail to remove timer_id");
    priv->timeout_checker_id = 0;
    ret = FALSE;
    goto stop;
  }

  if (!priv->request_ranges) {
    GST_DEBUG_OBJECT (requester, "stop checking timeout due to no request");
    if (!g_source_remove (priv->timeout_checker_id))
      GST_DEBUG_OBJECT (requester, "fail to remove timer_id");
    priv->timeout_checker_id = 0;
    ret = FALSE;
    goto stop;
  }

  for (walk = priv->request_ranges; walk;) {
    GList *next = g_list_next (walk);
    GstWFDRTPRequestRange *range = (GstWFDRTPRequestRange *) walk->data;

    if (range) {
      elapsed = g_timer_elapsed (priv->timer, NULL);
      if (elapsed - range->request_time > (requester->timeout_ms / 1000)) {
        GST_DEBUG_OBJECT (requester, "remove range for #%ld~#%ld",
            range->start_seqnum, range->end_seqnum);
        priv->request_ranges = g_list_delete_link  (priv->request_ranges, walk);
        g_free (range);
        is_timeout = TRUE;
      }
    }
    walk = next;
  }
  GST_WFD_RTP_REQUESTER_UNLOCK(requester);
  
  if (is_timeout) {
    GST_DEBUG_OBJECT (requester, "emit request IDR signal");
    g_signal_emit (requester, gst_wfd_rtp_requester_signals[SIGNAL_REQUEST_IDR], 0);
  }
  return ret;

stop:
  GST_WFD_RTP_REQUESTER_UNLOCK(requester);
  return ret;
}

static GstWFDRTPRequestRange *
create_range (GstWFDRTPRequester *requester, guint16 start_seqnum, guint16 end_seqnum)
{
  GstWFDRTPRequestRange *range = NULL;
  GstWFDRTPRequesterPrivate *priv;

  priv = GST_WFD_RTP_REQUESTER_GET_PRIVATE(requester);

  range = g_new0 (GstWFDRTPRequestRange, 1);
  if (!range) {
    GST_DEBUG_OBJECT (requester, "fail to create range for #%ld~#%ld", start_seqnum, end_seqnum);
    return  NULL;
  }

  if (!priv->timer)
    priv->timer = g_timer_new ();

  if (!priv->timeout_checker_id) {
    priv->timeout_checker_id = g_timeout_add (requester->timeout_ms,
						(GSourceFunc)check_timeout, (gpointer)requester);
    GST_DEBUG_OBJECT (requester, "create timer(%d) for checking request timeout", priv->timeout_checker_id);
  }

  range->start_seqnum = start_seqnum;
  range->end_seqnum = end_seqnum;
  range->request_packet_num = end_seqnum - start_seqnum + 0x0001;
  range->retransmitted_packet_num = 0;
  range->request_time = g_timer_elapsed (priv->timer, NULL);

  GST_DEBUG_OBJECT (requester, "create range for #%ld~#%ld at %.4f sec",
    range->start_seqnum, range->end_seqnum, range->request_time);

  return range;
}

static gint
find_range_by_seqnum (GstWFDRTPRequestRange * range, guint16 * seqnum)
{
  if (range->start_seqnum <= *seqnum && range->end_seqnum >= *seqnum)
    return 0;

  return -1;
}

/*
 * Warning! 
 * The function can't be called, if a calling thread already have 
 * requester->priv->lock (by GST_WFD_RTP_REQUESTER_LOCK) taken. 
 * This limitation is a result of GMutex's implementation.
 */
static void
remove_range (GstWFDRTPRequester *requester, guint16 seqnum)
{
  GstWFDRTPRequestRange *range;
  GList *node;

  GST_WFD_RTP_REQUESTER_LOCK (requester);
  
  node = g_list_find_custom (requester->priv->request_ranges, &seqnum, find_range_by_seqnum);
  if (!node) {
    GST_WFD_RTP_REQUESTER_UNLOCK (requester);
    GST_DEBUG_OBJECT (requester, "fail to find range for #%d rtp pakcet", seqnum);
    return;
  }

  range = (GstWFDRTPRequestRange *) node->data;
  range->retransmitted_packet_num++;

  GST_DEBUG_OBJECT (requester, "found range for #%d [%d~%d, remain %d packets]",
        seqnum, range->start_seqnum, range->end_seqnum,
        range->request_packet_num - range->retransmitted_packet_num);

  if (range->retransmitted_packet_num == range->request_packet_num) {
    GST_DEBUG_OBJECT (requester, "all requested rtp packets are arrived, remove range");
    requester->priv->request_ranges = g_list_delete_link(requester->priv->request_ranges, node);
    g_free(range);
  }
  GST_WFD_RTP_REQUESTER_UNLOCK (requester);

  return;
}

void
wfd_rtp_requester_perform_request (GstWFDRTPRequester *requester, guint16 expected_seqnum, guint16 received_seqnum)
{
  GstWFDRTPRequesterPrivate *priv;
  GstFlowReturn ret =GST_FLOW_OK;
  GstRTCPPacket rtcp_fb_packet;
  GstBuffer *rtcp_fb_buf;
  GstWFDRTPRequestRange * range;
  guint8 *fci_data;
  guint16 pid, blp;
  guint rtcp_packet_cnt = 0;
  gint gap;
  gint i;

  priv = GST_WFD_RTP_REQUESTER_GET_PRIVATE(requester);

  /* can't continue without gap */
  gap = received_seqnum - expected_seqnum;
  if (G_UNLIKELY (gap<1))
    goto no_gap;

  pid = expected_seqnum;

again:
  /* create buffer for rtcp fb packets */
  rtcp_fb_buf = gst_rtcp_buffer_new(REQUESTER_MTU_SIZE);
  if (!G_UNLIKELY(rtcp_fb_buf))
    goto fail_buf;

  /* create rtcp fb packet */
  do {
    if (!gst_rtcp_buffer_add_packet (rtcp_fb_buf, GST_RTCP_TYPE_RTPFB,
        &rtcp_fb_packet)) {
      if (rtcp_packet_cnt>0)
        goto send_packets;
      else
        goto fail_packet;
    }

    /* set rtcp fb header : wfd use RTCPFB with Generick NACK */
    gst_rtcp_packet_fb_set_type (&rtcp_fb_packet, GST_RTCP_RTPFB_TYPE_NACK);
    gst_rtcp_packet_fb_set_sender_ssrc (&rtcp_fb_packet, 0);
    gst_rtcp_packet_fb_set_media_ssrc (&rtcp_fb_packet, 0);

    /* set rtcp fb fci(feedback control information) : 32bits */
    if (!gst_rtcp_packet_fb_set_fci_length (&rtcp_fb_packet, 1)) {
      GST_DEBUG_OBJECT (requester, "fail to set FCI length to RTCP FB packet");
      gst_rtcp_packet_remove (&rtcp_fb_packet);
      return;
    }

    fci_data = gst_rtcp_packet_fb_get_fci ((&rtcp_fb_packet));

    /* set pid */
    GST_WRITE_UINT16_BE (fci_data, pid);
    pid += 0x0011;
    gap--;

    /* set blp */
    blp = 0x0000;
    for (i=0; i<16 && gap>0; i++) {
      blp +=  (0x0001 << i);
      gap--;
    }
    GST_WRITE_UINT16_BE (fci_data + 2, blp);

    rtcp_packet_cnt++;

    GST_DEBUG_OBJECT (requester, "%d RTCP FB packet : pid : %x, blp : %x", rtcp_packet_cnt, pid-0x0011, blp);
  }while (gap>0);

send_packets:
   /* end rtcp fb buffer */
  gst_rtcp_buffer_end (rtcp_fb_buf);

  /* Note : Should be crate range before requesting */
  if (requester->timeout_ms > 0) {
    range = create_range (requester, expected_seqnum, received_seqnum - 0x0001);
    if (range)
      priv->request_ranges = g_list_append (priv->request_ranges, range);
  }

  ret = gst_pad_push(requester->rtcp_src, rtcp_fb_buf);
  if (ret != GST_FLOW_OK)
    GST_WARNING_OBJECT(requester, "fail to pad push RTCP FB buffer");

  if (gap>0)
    goto again;

  return;

  /* ERRORS */
no_gap:
  {
    GST_DEBUG_OBJECT (requester, "there is no gap, don't need to make the RTSP FB packet");
    return;
  }
fail_buf:
  {
    GST_DEBUG_OBJECT (requester, "fail to make RTSP FB buffer");
    return;
  }
fail_packet:
  {
    GST_DEBUG_OBJECT (requester, "fail to make RTSP FB packet");
    return;
  }
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_wfd_rtp_requester_chain (GstPad * pad, GstBuffer * buf)
{
  GstWFDRTPRequester *requester;
  GstWFDRTPRequesterPrivate *priv;
  guint16 seqnum;
  guint8 pt;
  guint32 ssrc;
  GstFlowReturn ret = GST_FLOW_OK;

  requester = GST_WFD_RTP_REQUESTER (gst_pad_get_parent (pad));
  priv = GST_WFD_RTP_REQUESTER_GET_PRIVATE(requester);

  GST_WFD_RTP_REQUESTER_LOCK (requester);

  if (priv->flushing)
    goto drop_buffer;

  if (G_UNLIKELY (!gst_rtp_buffer_validate (buf)))
    goto invalid_buffer;

  /* check ssrc */
  ssrc = gst_rtp_buffer_get_ssrc (buf);
  if (G_LIKELY(!requester->ssrc)) {
    GST_DEBUG_OBJECT(requester, "set ssrc as %x using first rtp packet", ssrc);
    requester->ssrc = ssrc;
  } else if (G_LIKELY (requester->ssrc != ssrc)) {
    //goto invalid_ssrc;
    GST_ERROR_OBJECT(requester, "ssrc is changed from %x to %x", requester->ssrc, ssrc);
    requester->ssrc = ssrc;
  }

  /* check pt : pt should always 33 for MP2T-ES  */
  pt = gst_rtp_buffer_get_payload_type (buf);
  if (G_LIKELY (requester->pt != pt)) {
    if (G_LIKELY(!requester->pt)) {
      requester->pt = pt;
    } else
      goto invalid_pt;
  }

  seqnum = gst_rtp_buffer_get_seq (buf);

  /* now check against our expected seqnum */
  if (G_LIKELY (priv->next_in_seqnum != GST_CLOCK_TIME_NONE)) {
    gint gap;

    gap = gst_rtp_buffer_compare_seqnum (priv->next_in_seqnum, seqnum);
    if (G_UNLIKELY (gap != 0)) {
      GST_ERROR_OBJECT (requester, "expected #%d, got #%d, gap of %d", (guint16)priv->next_in_seqnum, seqnum, gap);

      if (requester->do_request) {
        if (G_UNLIKELY (gap < 0)) {
          if (G_UNLIKELY (gap < -150)) {
            GST_WARNING_OBJECT (requester, "#%d is too late, just unref the buffer for prevent of resetting wfdrtpbuffer", seqnum);
            gst_buffer_unref (buf);
            goto finished;
          } else {
            GST_DEBUG_OBJECT (requester, "#%d is late, but try to push", seqnum);
            goto skip;
          }
        } else if (G_UNLIKELY (gap > REQUESTER_MAX_DROPOUT)) {
          GST_DEBUG_OBJECT (requester, "too many dropped packets %d, need to request IDR",
              gap);
          g_signal_emit (requester, gst_wfd_rtp_requester_signals[SIGNAL_REQUEST_IDR], 0);
        } else {
          GST_DEBUG_OBJECT (requester, "tolerable gap");
          wfd_rtp_requester_perform_request(requester, priv->next_in_seqnum, seqnum);
        }
      }
    }
  } else {
    GST_ERROR_OBJECT (requester," No-error : got the first buffer, need to set buffer timestamp 0");
    GST_BUFFER_TIMESTAMP (buf) = 0;
  }

  priv->next_in_seqnum = (seqnum + 1) & 0xffff;

skip:
  /* just push out the incoming buffer without touching it */
  ret = gst_pad_push (requester->rtp_src, buf);
  if (ret != GST_FLOW_OK)
    GST_ERROR_OBJECT (requester, "failed to pad push..reason %s", gst_flow_get_name(ret));

finished:
  GST_WFD_RTP_REQUESTER_UNLOCK(requester);

  gst_object_unref (requester);

  return ret;

  /* ERRORS */
drop_buffer:
  {
    GST_ERROR_OBJECT (requester,
        "requeseter is flushing, drop incomming buffers..");
    gst_buffer_unref (buf);
    ret = GST_FLOW_OK;
    goto finished;
  }
invalid_buffer:
  {
    /* this is not fatal but should be filtered earlier */
    GST_ELEMENT_WARNING (requester, STREAM, DECODE, (NULL),
        ("Received invalid RTP payload, dropping"));
    gst_buffer_unref (buf);
    ret = GST_FLOW_OK;
    goto finished;
  }
invalid_ssrc:
  {
    /* this is not fatal but should be filtered earlier */
    GST_ELEMENT_WARNING (requester, STREAM, DECODE, (NULL),
        ("ssrc of this rtp packet is differtent from before.  dropping"));
    gst_buffer_unref (buf);
    ret = GST_FLOW_OK;
    goto finished;
  }
invalid_pt:
  {
    /* this is not fatal but should be filtered earlier */
    GST_ELEMENT_WARNING (requester, STREAM, DECODE, (NULL),
       ("pt of this rtp packet is differtent from before. dropping"));
    gst_buffer_unref (buf);
    ret = GST_FLOW_OK;
    goto finished;
  }
}

GstBuffer*
wfd_requester_handle_retransmitted_rtp (GstWFDRTPRequester *requester, GstBuffer *ret_rtp_buf)
{
  GstBuffer *rtp_buf;
  guint16 seqnum;
  guint8 *buf_data;
  guint buf_size;
  guint8 *payload;
  guint payload_len, header_len;

  if (G_UNLIKELY (!gst_rtp_buffer_validate (ret_rtp_buf))) {
    /* this is not fatal but should be filtered earlier */
    GST_ELEMENT_WARNING (requester, STREAM, DECODE, (NULL),
        ("Received invalid RTP payload, dropping"));
    gst_buffer_unref (ret_rtp_buf);
    return NULL;
  }

  GST_ERROR_OBJECT (requester, "No-error: received retransmitted rtp packet #%d",
      gst_rtp_buffer_get_seq (ret_rtp_buf));

  buf_data = GST_BUFFER_DATA(ret_rtp_buf);
  buf_size = GST_BUFFER_SIZE(ret_rtp_buf);
  payload = gst_rtp_buffer_get_payload(ret_rtp_buf);
  payload_len = gst_rtp_buffer_get_payload_len(ret_rtp_buf);
  header_len = gst_rtp_buffer_get_header_len(ret_rtp_buf);

  rtp_buf = gst_buffer_new_and_alloc(buf_size - 2);
  if (!rtp_buf) {
    GST_WARNING_OBJECT(requester, "failed to alloc for rtp buffer");
    return NULL;
  }

  /* copy rtp header */
  memcpy(GST_BUFFER_DATA (rtp_buf), buf_data, header_len);
  /* copy rtp payload */
  memcpy(GST_BUFFER_DATA (rtp_buf)+header_len, payload+2, payload_len-2);

  /* set seqnum to original */
  seqnum = GST_READ_UINT16_BE (payload);
  gst_rtp_buffer_set_seq(rtp_buf, seqnum);

  GST_ERROR_OBJECT (requester, "No-error: restored rtp packet #%d", seqnum);

  if (requester->timeout_ms>0)
    remove_range (requester, seqnum);

  return rtp_buf;
}

static GstFlowReturn
gst_wfd_rtp_requester_chain_retransmitted_rtp (GstPad * pad, GstBuffer * buf)
{
  GstWFDRTPRequester *requester;
  GstWFDRTPRequesterPrivate *priv;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *outbuf;
  gint gap = 0;
  gint seqnum = 0;

  requester = GST_WFD_RTP_REQUESTER (gst_pad_get_parent (pad));
  priv = GST_WFD_RTP_REQUESTER_GET_PRIVATE(requester);

  if (!requester->do_request)
    goto skip_buffer;

  outbuf = wfd_requester_handle_retransmitted_rtp(requester, buf);
  if (!outbuf) {
    GST_ERROR_OBJECT (requester, "No-error: failed to handle retransmitted rtp packet...");
    return GST_FLOW_OK;
  }

  /* check sequence of retransmitted rtp packet. */
  if(G_UNLIKELY (priv->next_in_seqnum == GST_CLOCK_TIME_NONE)) {
    goto skip_buffer;
  }
  seqnum = gst_rtp_buffer_get_seq (outbuf);
  gap = gst_rtp_buffer_compare_seqnum (priv->next_in_seqnum, seqnum);
  if (G_UNLIKELY (gap > 0)) {
    GST_ERROR_OBJECT (requester, "#%d is invalid sequence number, gap of %d", seqnum, gap);
    goto skip_buffer;
  }

  ret = gst_wfd_rtp_requester_chain(requester->rtp_sink, outbuf);
  if (ret != GST_FLOW_OK)
    GST_ERROR_OBJECT (requester, "No-error: failed to push retransmitted rtp packet...");

finished:
  gst_object_unref (requester);

  /* just return OK */
  return GST_FLOW_OK;

  /* ERRORS */
skip_buffer:
  {
    GST_DEBUG_OBJECT (requester,
        "requeseter is set to not handle retransmission, dropping");
    gst_buffer_unref (buf);
    goto finished;
  }
}

static GstPad *
gst_wfd_rtp_requester_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name)
{
  GstWFDRTPRequester *requester;
  GstElementClass *klass;

  requester = GST_WFD_RTP_REQUESTER (element);
  klass = GST_ELEMENT_GET_CLASS (element);

  GST_WFD_RTP_REQUESTER_LOCK (requester);

  if (templ != gst_element_class_get_pad_template (klass, name))
    goto wrong_template;

  if (requester->retransmitted_rtp_sink != NULL)
    goto exists;

  GST_LOG_OBJECT (requester, "Creating new pad for retreansmitted RTP packets");

  requester->retransmitted_rtp_sink =
      gst_pad_new_from_static_template (&retransmitted_rtp_sink_template,
      "retransmitted_rtp_sink");
  gst_pad_set_chain_function (requester->retransmitted_rtp_sink,
      gst_wfd_rtp_requester_chain_retransmitted_rtp);
  gst_pad_set_event_function (requester->retransmitted_rtp_sink,
      (GstPadEventFunction) gst_wfd_rtp_requester_sink_event_retransmitted_rtp);
  gst_pad_use_fixed_caps (requester->retransmitted_rtp_sink);
  gst_pad_set_active (requester->retransmitted_rtp_sink, TRUE);
  gst_element_add_pad (GST_ELEMENT_CAST (requester),
      requester->retransmitted_rtp_sink);

  GST_DEBUG_OBJECT (requester, "creating RTCP src pad for RTCP FB packets");
  requester->rtcp_src =
      gst_pad_new_from_static_template (&rtcp_src_template,
      "rtcp_src");
  gst_pad_set_event_function (requester->rtcp_src,
      (GstPadEventFunction) gst_wfd_rtp_requester_src_event_rtcp);
  gst_pad_use_fixed_caps (requester->rtcp_src);
  gst_pad_set_active (requester->rtcp_src, TRUE);
  gst_element_add_pad (GST_ELEMENT_CAST (requester), requester->rtcp_src);

  GST_WFD_RTP_REQUESTER_UNLOCK (requester);

  return requester->retransmitted_rtp_sink;

/* ERRORS */
wrong_template:
  {
    GST_WFD_RTP_REQUESTER_UNLOCK (requester);
    g_warning ("wfdrtprequester: this is not our template");
    return NULL;
  }
exists:
  {
    GST_WFD_RTP_REQUESTER_UNLOCK (requester);
    g_warning ("wfdrtprequester: pad already requested");
    return NULL;
  }
}

static void
gst_wfd_rtp_requester_release_pad (GstElement * element, GstPad * pad)
{
  GstWFDRTPRequester *requester;

  g_return_if_fail (GST_IS_WFD_RTP_REQUESTER (element));
  g_return_if_fail (GST_IS_PAD (pad));

  requester = GST_WFD_RTP_REQUESTER (element);

  GST_DEBUG_OBJECT (element, "releasing pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  GST_WFD_RTP_REQUESTER_LOCK (requester);

  if (requester->retransmitted_rtp_sink == pad) {
    /* deactivate from source to sink */
    gst_pad_set_active (requester->rtcp_src, FALSE);
    gst_pad_set_active (requester->retransmitted_rtp_sink, FALSE);

    /* remove pads */
    GST_DEBUG_OBJECT (requester, "removing retransmitted RTP sink pad");
    gst_element_remove_pad (GST_ELEMENT_CAST (requester),
        requester->retransmitted_rtp_sink);
    requester->retransmitted_rtp_sink = NULL;

    GST_DEBUG_OBJECT (requester, "removing RTCP src pad");
    gst_element_remove_pad (GST_ELEMENT_CAST (requester),
        requester->rtcp_src);
    requester->rtcp_src = NULL;
  } else
    goto wrong_pad;

  GST_WFD_RTP_REQUESTER_UNLOCK (requester);

  return;

  /* ERRORS */
wrong_pad:
  {
    GST_WFD_RTP_REQUESTER_UNLOCK (requester);
    g_warning ("wfdrtprequester: asked to release an unknown pad");
    return;
  }
}

