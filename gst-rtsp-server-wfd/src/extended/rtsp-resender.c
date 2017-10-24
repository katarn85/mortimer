/*
 * Copyright (c) 2000-2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Contact: Yejin Cho < cho.yejin@samsung.com >
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <unistd.h>
#include <gst/gst.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtcpbuffer.h>

#include "rtsp-resender.h"

GST_DEBUG_CATEGORY_STATIC (rtp_resender_debug);
#define GST_CAT_DEFAULT rtp_resender_debug

static const GstElementDetails gst_rtp_resender_details =
GST_ELEMENT_DETAILS ("RTP packet resender",
    "Generic",
    "RTP packet resender",
    "YeJin Cho <cho.yejin@samsung.com>");

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_MAX_RESEND_NUM,
  PROP_MAX_SLOT_NUM,
  PROP_RTP_FRACTION_LOST,
  PROP_PACKETS_RESEND
  };

#define DEFAULT_MAX_RESEND_NUM 3
#define DEFAULT_MAX_SLOT_NUM 4096


#define GST_RESENDER_MUTEX_LOCK(r) G_STMT_START {                          \
  g_mutex_lock (r->qlock);                                              \
} G_STMT_END

#define GST_RESENDER_MUTEX_UNLOCK(r) G_STMT_START {                        \
  g_mutex_unlock (r->qlock);                                            \
} G_STMT_END

/* the capabilities of the inputs and outputs. */
static GstStaticPadTemplate rtp_sink_template = GST_STATIC_PAD_TEMPLATE ("rtp_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

static GstStaticPadTemplate rtcp_sink_template = GST_STATIC_PAD_TEMPLATE ("rtcp_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rctp")
    );

static GstStaticPadTemplate send_src_template = GST_STATIC_PAD_TEMPLATE ("send_src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

static GstStaticPadTemplate resend_src_template = GST_STATIC_PAD_TEMPLATE ("resend_src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT (rtp_resender_debug, "rtpresender", 0,
      "rtp resender element");
}

GST_BOILERPLATE_FULL (GstRTPResender, gst_rtp_resender, GstElement, GST_TYPE_ELEMENT,
    _do_init);


static void gst_rtp_resender_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rtp_resender_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_rtp_resender_rtp_sink_event (GstPad * pad, GstEvent * event);
static GstFlowReturn gst_rtp_resender_chain_recv_rtp (GstPad * pad, GstBuffer * buffer);
static gboolean gst_rtp_resender_insert_rtp(GstRTPResender * resender, GstBuffer * buf);
static GstBuffer * gst_rtp_resender_extract_rtp(GstRTPResender * resender, guint16 seqnum, guint16 pid);


static GstFlowReturn gst_rtp_resender_chain_recv_rtcp (GstPad * pad, GstBuffer * buf);
static gboolean gst_rtp_resender_parse_rtcp(GstRTPResender * resender, GstBuffer * buf, guint16 * pid, guint16 * blp);

static void alloc_slots (GstRTPResender *resender);
static void free_slots (GstRTPResender *resender);
static void set_resend_num(GstRTPResender *resender);
static void update_fraction_lost (GstRTPResender *resender, guint fraction_lost);

/* GObject vmethod implementations */
static void
gst_rtp_resender_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (gstelement_class, &gst_rtp_resender_details);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&rtp_sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&rtcp_sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&send_src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&resend_src_template));
}

static void
gst_rtp_resender_dispose (GObject * object)
{
  GstRTPResender *resender = GST_RTP_RESENDER (object);

  g_timer_destroy (resender->timer);
  g_mutex_free (resender->qlock);
  free_slots(resender);
  
  G_OBJECT_CLASS (parent_class)->dispose (object);
}


/* initialize the plugin's class */
static void
gst_rtp_resender_class_init (GstRTPResenderClass * klass)
{
  GObjectClass *gobject_class;
  //GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  //gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_rtp_resender_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_rtp_resender_get_property);
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_rtp_resender_dispose);

  g_object_class_install_property (gobject_class, PROP_MAX_RESEND_NUM,
    g_param_spec_uint ("max-resend-num", "Maximum number of resend",
      "Maximum number of resending RTP packet", 0, G_MAXUINT,
      DEFAULT_MAX_RESEND_NUM, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MAX_SLOT_NUM,
    g_param_spec_uint ("slot-num", "The number of slots",
      "The number of slots", 0, G_MAXUINT,
      DEFAULT_MAX_SLOT_NUM, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_RTP_FRACTION_LOST,
    g_param_spec_uint ("rtp-fraction-lost", "The RTP fraction lost value from RTCP RR",
      "The RTP fraction lost value from RTCP RR", 0, G_MAXUINT,
      0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_PACKETS_RESEND,
    g_param_spec_uint ("rtp-packets-resend", "Total number of packets resent in this session",
      "Total number of packets resent in this session", 0, G_MAXUINT,
      0, G_PARAM_READWRITE));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_rtp_resender_init (GstRTPResender * resender, GstRTPResenderClass * g_class)
{
  /* sink pad for handling rtp packets */
  resender->rtp_sinkpad = gst_pad_new_from_static_template (&rtp_sink_template, "rtp_sink");
  gst_pad_set_event_function (resender->rtp_sinkpad,
                              GST_DEBUG_FUNCPTR(gst_rtp_resender_rtp_sink_event));
  gst_pad_set_chain_function (resender->rtp_sinkpad,
                              GST_DEBUG_FUNCPTR(gst_rtp_resender_chain_recv_rtp));
  gst_element_add_pad (GST_ELEMENT (resender), resender->rtp_sinkpad);

  /* sink pad for handling rtcp packets */
  resender->rtcp_sinkpad = gst_pad_new_from_static_template (&rtcp_sink_template, "rtcp_sink");
  gst_pad_set_chain_function (resender->rtcp_sinkpad,
                              GST_DEBUG_FUNCPTR(gst_rtp_resender_chain_recv_rtcp));
  gst_element_add_pad (GST_ELEMENT (resender), resender->rtcp_sinkpad);

  /* sink pad for handling rtcp packets */
  resender->send_srcpad = gst_pad_new_from_static_template (&send_src_template, "send_src");
  gst_element_add_pad (GST_ELEMENT (resender), resender->send_srcpad);

  resender->resend_srcpad = gst_pad_new_from_static_template (&resend_src_template, "resend_src");
  gst_element_add_pad (GST_ELEMENT (resender), resender->resend_srcpad);

  resender->timer = g_timer_new ();

  /* properties */
  resender->max_resend_num = DEFAULT_MAX_RESEND_NUM;
  resender->max_slot_num = DEFAULT_MAX_SLOT_NUM;

  resender->qlock = g_mutex_new ();
  resender->is_set_caps = FALSE;

  resender->prev_rtcp_pid = 0;
  resender->prev_rtcp_blp = 0;
  resender->rtcp_repeat_time = 0;
  resender->resend_seqnum = 0;
  resender->packets_resend = 0;

  alloc_slots(resender);
}



static gboolean
gst_rtp_resender_rtp_sink_event (GstPad * pad, GstEvent * event)
{
  GstRTPResender *resender;
  gboolean ret;

  resender = (GstRTPResender *) GST_OBJECT_PARENT (pad);

  switch (GST_EVENT_TYPE (event)) {
    default:
      ret = gst_pad_push_event (resender->send_srcpad, event);
      break;
  }
  return ret;
}

static GstFlowReturn
gst_rtp_resender_chain_recv_rtp (GstPad * pad, GstBuffer * buffer)
{
  GstRTPResender *resender;
  GstCaps *caps = NULL;
  gchar *type = NULL;

  resender = (GstRTPResender *) GST_OBJECT_PARENT (pad);

  if (!resender->is_set_caps) {
    caps = gst_buffer_get_caps(buffer);
    gst_pad_set_caps(resender->resend_srcpad, caps);

    type = gst_caps_to_string(caps);

    GST_DEBUG_OBJECT(resender, "set caps if resend_srcpad to %s", type);

    g_free(type);
    gst_caps_unref(caps);

    resender->is_set_caps = TRUE;
  }
  
  if (!gst_rtp_resender_insert_rtp(resender, buffer))
    GST_WARNING_OBJECT (resender, "fail to insert rtp packet");

  /* just push out the incoming buffer without touching it */
  return gst_pad_push (resender->send_srcpad, buffer);
}

static gboolean
gst_rtp_resender_insert_rtp(GstRTPResender *resender, GstBuffer * buffer)
{
  guint16 seqnum;
  guint16 slotnum;
  guint8 *buf_data;
  guint buf_size;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), FALSE);

  buf_data = GST_BUFFER_DATA(buffer);
  buf_size = GST_BUFFER_SIZE(buffer);

  /* Check if the data pointed to by buf is a valid RTP packet */
  if (G_UNLIKELY (!gst_rtp_buffer_validate_data(buf_data, buf_size)))
    goto invalid_buffer;

  /* get rtp packet #seqnum */
  seqnum = gst_rtp_buffer_get_seq (buffer);

  /* calculate #slotnum for rtp packet to be inserted */ 
  switch (resender->max_slot_num) {
    case 1024:
      slotnum = seqnum & 0x03ff;
      break;
    case 2048:
      slotnum = seqnum & 0x07ff;
      break;
    case 4096:	
      slotnum = seqnum & 0x0fff;
      break;
    case 8192:	
      slotnum = seqnum & 0x1fff;
      break;
    case 16384:
      slotnum = seqnum & 0x3fff;
      break;
    case 32768:
      slotnum = seqnum & 0x7fff;
      break;
    case 65536:
      slotnum = seqnum & 0xffff;
      break;
    default:
      slotnum = seqnum & 0x03ff;
      break;
  }

  /* insert rtp packet into #slotnum */
  GST_RESENDER_MUTEX_LOCK(resender);
  
#if 0
  if (resender->slots[slotnum]) {
    //GST_LOG_OBJECT (resender, "free slot #%d for new rtp packet", slotnum);
    g_byte_array_free (resender->slots[slotnum], TRUE);
    resender->slots[slotnum] = NULL;
  }
    GST_LOG_OBJECT (resender, "buffer size : %d, data size %d", sizeof(GstBuffer), buf_size);

  //GST_LOG_OBJECT (resender, "insert rtp packet #%d(%d bytes) into slot #%d", seqnum, buf_size, slotnum);

  resender->slots[slotnum] = g_byte_array_sized_new(buf_size);
  if (resender->slots[slotnum] == NULL) {
    GST_WARNING_OBJECT (resender, "fail to allocate slot #%d for rtp packet #%d(%d bytes)", slotnum, seqnum, buf_size);
    GST_RESENDER_MUTEX_UNLOCK(resender);
    return FALSE;
  }

  resender->slots[slotnum] = g_byte_array_append(resender->slots[slotnum], buf_data, buf_size);
  if (resender->slots[slotnum] == NULL) {
    GST_WARNING_OBJECT (resender, "fail to append rtp packet #%d(%d bytes) to slot #%d", seqnum, buf_size, slotnum);
    GST_RESENDER_MUTEX_UNLOCK(resender);
    return FALSE;
  }
#else
  if (resender->slots[slotnum]) {
    //GST_LOG_OBJECT (resender, "free slot #%d for new rtp packet", slotnum);
    gst_buffer_unref (resender->slots[slotnum]);
    resender->slots[slotnum] = NULL;
  }

  resender->slots[slotnum] = buffer;
  gst_buffer_ref(buffer);
#endif

  GST_RESENDER_MUTEX_UNLOCK(resender);

  return TRUE;

invalid_buffer:
  GST_ERROR_OBJECT(resender, "invalid buffer");
  return FALSE;
}

static GstBuffer *
gst_rtp_resender_extract_rtp(GstRTPResender * resender, guint16 seqnum, guint16 pid)
{
  GstBuffer *out_buffer;
  guint16 slotnum;
  guint buf_size;
  guint8 *buf_data;
  
  
  /* calculate #slotnum for rtp packet to be extracted */
  switch (resender->max_slot_num) {
    case 1024:
      slotnum = seqnum & 0x03ff;
      break;
    case 2048:
      slotnum = seqnum & 0x07ff;
      break;
    case 4096:	
      slotnum = seqnum & 0x0fff;
      break;
    case 8192:	
      slotnum = seqnum & 0x1fff;
      break;
    case 16384:
      slotnum = seqnum & 0x3fff;
      break;
    case 32768:
      slotnum = seqnum & 0x7fff;
      break;
    case 65536:
      slotnum = seqnum & 0xffff;
      break;
    default:
      slotnum = seqnum & 0x03ff;
      break;
  }

  /* extract rtp packet from #slotnum */
  GST_RESENDER_MUTEX_LOCK(resender);
#if 0
  GST_LOG_OBJECT (resender, "extract rtp packet #%d(%d bytes) from slot #%d", seqnum, resender->slots[slotnum]->len, slotnum);

  /* alloc 2 more bytes for pid between rtp header and rtp payload */
  buffer = gst_buffer_new_and_alloc(resender->slots[slotnum]->len + 2);
  if (!buffer) {
    GST_WARNING_OBJECT(resender, "fail to alloc for buffer");
    GST_RESENDER_MUTEX_UNLOCK(resender);
    return NULL;
  }

  /* add rtp header */
  memcpy(GST_BUFFER_DATA (buffer), resender->slots[slotnum]->data, 12);
  /* add pid */
  GST_WRITE_UINT16_BE(GST_BUFFER_DATA (buffer)+12, pid);
  /* add rtp payload */  
  memcpy(GST_BUFFER_DATA (buffer)+14, resender->slots[slotnum]->data+12, resender->slots[slotnum]->len-12);
#else
  buf_size = GST_BUFFER_SIZE(resender->slots[slotnum]);
  buf_data = GST_BUFFER_DATA (resender->slots[slotnum]);
  resender->packets_resend++;
  GST_ERROR ("RTP Packets resent is %d", resender->packets_resend);
  GST_LOG_OBJECT (resender, "extract rtp packet #%d(%d bytes) from slot #%d", seqnum, buf_size, slotnum);

  /* alloc 2 more bytes for pid between rtp header and rtp payload */
  out_buffer = gst_buffer_new_and_alloc(buf_size + 2);
  if (!out_buffer) {
    GST_WARNING_OBJECT(resender, "fail to alloc for buffer");
    GST_RESENDER_MUTEX_UNLOCK(resender);
    return NULL;
  }

  /* add rtp header */
  memcpy(GST_BUFFER_DATA (out_buffer), buf_data, 2);
  /* add seqnum of resender packet */
  ++resender->resend_seqnum;
  GST_WRITE_UINT16_BE(GST_BUFFER_DATA (out_buffer)+2, resender->resend_seqnum);

  memcpy(GST_BUFFER_DATA (out_buffer)+4, buf_data+4, 8);
  GST_WRITE_UINT16_BE(GST_BUFFER_DATA (out_buffer)+12, seqnum);
  /* add rtp payload */
  memcpy(GST_BUFFER_DATA (out_buffer)+14, buf_data+12, buf_size-12);
#endif

  GST_RESENDER_MUTEX_UNLOCK(resender);

  /* Check if the data pointed to by buf is a valid RTP packet */
  if (G_UNLIKELY (!gst_rtp_buffer_validate (out_buffer)))
    goto invalid_buffer;

  /*seqnum is written by us so there is no need to check it.*/
  /* check rtp seqnum of the extracted rtp packet */
  /*if (seqnum != gst_rtp_buffer_get_seq (out_buffer)) {
    GST_DEBUG_OBJECT (resender, "requested rtp packet(#%d) could not be extracted (%d)", seqnum, gst_rtp_buffer_get_seq (out_buffer));	
    return NULL;
  }*/

  return out_buffer;

invalid_buffer:
  GST_ERROR_OBJECT (resender, "invalid buffer");
  return NULL;
}

static GstFlowReturn
gst_rtp_resender_chain_recv_rtcp (GstPad * pad, GstBuffer * buffer)
{
  GstRTPResender *resender;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer * outbuf = NULL;
  guint header_len;
  guint8 version;
  guint data_len;
  gboolean padding;
  guint8 pad_bytes;
  guint8 *data;
  guint len;
  guint16 seqnum, mask;
  guint16 pid = 0, blp = 0;
  guint resendnum = 1, i, j;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), GST_FLOW_ERROR);
  g_return_val_if_fail (GST_BUFFER_DATA(buffer) != NULL, GST_FLOW_ERROR);

  resender = (GstRTPResender *) GST_OBJECT_PARENT (pad);

  GST_DEBUG_OBJECT(resender, "received RTCP packet");

  data = GST_BUFFER_DATA (buffer);
  len = GST_BUFFER_SIZE (buffer);

  /* we need 4 bytes for the type and length */
  if (G_UNLIKELY (len < 4))
    goto wrong_length;

  /* no padding when mask succeeds */
  padding = FALSE;

  /* store len */
  data_len = len;

  while (TRUE) {
    /* get packet length */
    header_len = (((data[2] << 8) | data[3]) + 1) << 2;
    if (data_len < header_len)
      goto wrong_length;

    /* move to next compount packet */
    data += header_len;
    data_len -= header_len;

    /* we are at the end now */
    if (data_len < 4)
      break;

    /* check version of new packet */
    version = data[0] & 0xc0;
    if (version != (GST_RTCP_VERSION << 6))
      goto wrong_version;

    /* padding only allowed on last packet */
    if ((padding = data[0] & 0x20))
      break;
  }
  if (data_len > 0) {
    /* some leftover bytes, check padding */
    if (!padding)
      goto wrong_length;

    /* get padding */
    pad_bytes = data[data_len - 1];
    if (data_len != pad_bytes)
      goto wrong_padding;
  }

  /* parse rtcp packet to get requestd rtp sequence number(#seqnum) */
  if (!gst_rtp_resender_parse_rtcp(resender, buffer, &pid, &blp)) {
    GST_WARNING_OBJECT (resender, "fail to parse rtcp packet");
    return GST_FLOW_OK;
  }

  /* check this rtcp packet is same as before one */
  if ((resender->prev_rtcp_pid == pid) && (resender->prev_rtcp_blp == blp) && resender->rtcp_repeat_time < 3) {
    resender->rtcp_repeat_time++;
    GST_DEBUG_OBJECT (resender, "this RTCP packet is same as previous one(%d times), ignore it", resender->rtcp_repeat_time);
    return GST_FLOW_OK;
  }
  resender->prev_rtcp_pid = pid;
  resender->prev_rtcp_blp = blp;
  resender->rtcp_repeat_time = 0;

  /* extract and send rtp packet using pid */
  seqnum = pid;

  /* extract */
  outbuf = gst_rtp_resender_extract_rtp (resender, seqnum, pid);
  if (!outbuf) {
    GST_WARNING_OBJECT (resender, "fail to extract rtp packet");
    return GST_FLOW_OK;
  }

  /* set resned_num using fraction lost */
  set_resend_num(resender);

  /* push out the buffer as many times as #resendnum */
  for (i=0; i<resendnum; i++) {
    ret = gst_pad_push (resender->resend_srcpad, outbuf);
    if (ret< GST_FLOW_OK) {
      GST_WARNING_OBJECT (resender, "fail to push requested rtp packet");
      break;
    }
  }

  /* extract and send rtp packets using blp */
  for (i=0; i<16; i++) {
    mask = blp & (0x0001 << i);
    if (!mask)
      continue;

    seqnum = pid + i + 1;

    /* extract rtp packet to be resent */
    outbuf = gst_rtp_resender_extract_rtp (resender, seqnum, pid);
    if (!outbuf) {
      GST_WARNING_OBJECT (resender, "fail to extract rtp packet");
      return GST_FLOW_OK;
    }

    /* push out the buffer as many times as #resendnum */
    for (j=0; j<resendnum; j++) {
      ret = gst_pad_push (resender->resend_srcpad, outbuf);
      if (ret< GST_FLOW_OK) {
	  GST_WARNING_OBJECT (resender, "fail to push requested rtp packet");
	  break;
      	}
    }
  }

  return ret;

wrong_length:
  {
    GST_DEBUG_OBJECT (resender,"len check failed");
    g_object_unref(buffer);
    return GST_FLOW_OK;
  }
wrong_version:
  {
    GST_DEBUG_OBJECT (resender,"wrong version (%d < 2)", version >> 6);
    g_object_unref(buffer);
    return GST_FLOW_OK;
  }
wrong_padding:
  {
    GST_DEBUG_OBJECT (resender,"padding check failed");
    g_object_unref(buffer);
    return GST_FLOW_OK;
  }
}

static gboolean
gst_rtp_resender_parse_rtcp(GstRTPResender * resender, GstBuffer * buffer, guint16 *pid, guint16 *blp)
{
  GstRTCPPacket packet;
  gboolean more;
  GstRTCPType type;
  //GstRTCPFBType fbtype;
  //guint32 sender_ssrc;
  //guint32 media_ssrc;
  guint8 *fci_data;
  //guint fci_length;

  g_return_val_if_fail (buffer != NULL, FALSE);
  
  /* start processing the compound packet */
  more = gst_rtcp_buffer_get_first_packet (buffer, &packet);
  while (more) {
    /* check feedback type rtcp packet or not */
    type = gst_rtcp_packet_get_type (&packet);
    switch (type) {
      case GST_RTCP_TYPE_SR:
      case GST_RTCP_TYPE_RR:
      case GST_RTCP_TYPE_SDES:
      case GST_RTCP_TYPE_BYE:
      case GST_RTCP_TYPE_APP:
      case GST_RTCP_TYPE_PSFB:
       GST_WARNING_OBJECT(resender, "got RTCP packet type %d", type);
       break;
      case GST_RTCP_TYPE_RTPFB:
  	  /* parse feedback type rtcp packet, get the requested rtp seqnum for resending */
        //fbtype = gst_rtcp_packet_fb_get_type (&packet);
        //sender_ssrc = gst_rtcp_packet_fb_get_sender_ssrc (&packet);
        //media_ssrc = gst_rtcp_packet_fb_get_media_ssrc (&packet);
        fci_data= gst_rtcp_packet_fb_get_fci (&packet);
        //fci_length = 4 * gst_rtcp_packet_fb_get_fci_length (&packet);
	 *pid = GST_READ_UINT16_BE (fci_data);
	 *blp = GST_READ_UINT16_BE (fci_data + 2);

        GST_DEBUG_OBJECT (resender, "pid is %d, blp is %d", *pid, *blp);
        break;
      default:
        GST_WARNING_OBJECT(resender, "got unknown RTCP packet");
        break;
    }
    more = gst_rtcp_packet_move_to_next (&packet);
  }

  return TRUE;
}

static void
gst_rtp_resender_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRTPResender *resender = GST_RTP_RESENDER (object);

  switch (prop_id) {
    case PROP_MAX_RESEND_NUM:
      resender->max_resend_num = g_value_get_uint (value);
      break;
    case PROP_MAX_SLOT_NUM:
      resender->max_slot_num = g_value_get_uint (value);
      alloc_slots(resender);
      break;
    case PROP_RTP_FRACTION_LOST:
      resender->rtp_fraction_lost = g_value_get_uint (value);
      update_fraction_lost(resender, resender->rtp_fraction_lost);
      break;
    case PROP_PACKETS_RESEND:
      resender->packets_resend = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_resender_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRTPResender *resender = GST_RTP_RESENDER (object);

  switch (prop_id) {
    case PROP_MAX_RESEND_NUM:
      g_value_set_uint (value, resender->max_resend_num);
      break;
    case PROP_MAX_SLOT_NUM:
      g_value_set_uint (value, resender->max_slot_num);
      break;
    case PROP_RTP_FRACTION_LOST:
      g_value_set_uint (value, resender->rtp_fraction_lost);
      break;
    case PROP_PACKETS_RESEND:
      g_value_set_uint (value, resender->packets_resend);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
alloc_slots (GstRTPResender *resender)
{
  g_return_if_fail(resender->max_slot_num > 0);

#if 0
  resender->slots = (GByteArray*)g_malloc0(sizeof(GByteArray) * resender->max_slot_num);
  if (!resender->slots)
    GST_WARNING_OBJECT(resender, "fail to allocation memory for slots");
#else
  resender->slots = (GstBuffer **)g_malloc0(sizeof(GstBuffer *) * resender->max_slot_num);
  if (resender->slots == NULL)
    GST_WARNING_OBJECT(resender, "fail to allocation memory for slots");
#endif
}

static void
free_slots (GstRTPResender *resender)
{
  gint i;
  
  i=0;
  while (resender->slots[i]) {
#if 0
    g_byte_array_free(resender->slots[i], TRUE);
#else
    gst_buffer_unref(resender->slots[i]);
#endif
    i++;	
  }
}

static void
set_resend_num(GstRTPResender *resender)
{
  guint resend_num = 1;

  /* update fraction lost rate */
  update_fraction_lost (resender, 0);

  /* Fix me : need to calculate resend num using averaged fraction lost */
  if (resender->fraction_lost_rate == 0) {
    resender->resend_num = 1;
  } else if (resender->fraction_lost_rate > 1){
    resender->resend_num = 2;
  } else {
    resender->resend_num = 3;
  }

  if (resend_num > resender->max_resend_num)
    resend_num = resender->max_resend_num;

  resender->resend_num = resend_num;

  GST_DEBUG_OBJECT(resender, "resend requested RTP packet %d times", resender->resend_num);
}

#define TIME_INTERVAL    30
static void
update_fraction_lost (GstRTPResender *resender, guint fraction_lost)
{
  gdouble elapsed, period = 0;

  if (!resender->timer_started) {
    resender->timer_started = TRUE;
    g_timer_start (resender->timer);
    return;
  }

  elapsed = g_timer_elapsed (resender->timer, NULL);

  /* recalc after each interval. */
  if (elapsed - resender->last_elapsed < TIME_INTERVAL) {
    period = elapsed - resender->last_elapsed;
    resender->fraction_lost += fraction_lost;
    resender->fraction_lost_rate = resender->fraction_lost / period;
  } else {
    resender->fraction_lost = 0;
    resender->fraction_lost_rate = 0;
  }

  GST_DEBUG_OBJECT (resender,
      "fraction lost summation %d, period %f, average rate %f",
      resender->fraction_lost, period, resender->fraction_lost_rate);

  /* update last elapsed time */
  resender->last_elapsed = elapsed;
}

