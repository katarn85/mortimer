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

#ifndef __GST_RTP_RESENDER_H__
#define __GST_RTP_RESENDER_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_RTP_RESENDER \
  (gst_rtp_resender_get_type())
#define GST_RTP_RESENDER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RTP_RESENDER,GstRTPResender))
#define GST_RTP_RESENDER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RTP_RESENDER,GstRTPResenderClass))
#define GST_IS_RTP_RESENDER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RTP_RESENDER))
#define GST_IS_RTP_RESENDER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RTP_RESENDER))

/* Note : rtp packet max packet = maximum packetsize + 2 bytes */
#define RESENDER_RTP_PACKET_MAX_SIZE 1502
#define RESENDER_RTP_PACKET_MAX_NUM 1024
#define RESENDER_MAX_RESEND_NUM 3


typedef struct _GstRTPResender      GstRTPResender;
typedef struct _GstRTPResenderClass GstRTPResenderClass;

struct _GstRTPResender
{
  GstElement element;

  GstPad *rtp_sinkpad, *rtcp_sinkpad, *send_srcpad, *resend_srcpad;

  GMutex *qlock;        /* lock for slots */
  //GByteArray **slots;
  GstBuffer **slots;

  gboolean is_set_caps;
  
  /* for measuring average rtp fraction lost*/
  GTimer *timer;
  gboolean timer_started;
  guint fraction_lost;
  gdouble last_elapsed;
  gdouble fraction_lost_rate;

  /* properties */
  guint max_resend_num;
  guint max_slot_num;
  guint rtp_fraction_lost;

  guint resend_num;

  /* Note : Dongle specific feature. the dongle always send same RTCP packet 3 times */
  guint prev_rtcp_pid;
  guint prev_rtcp_blp;
  guint rtcp_repeat_time;
  guint16 resend_seqnum;
  gint packets_resend;
};

struct _GstRTPResenderClass 
{
  GstElementClass parent_class;
};

GType gst_rtp_resender_get_type (void);

G_END_DECLS

#endif /* __GST_RTP_RESENDER_H__ */
