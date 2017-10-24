/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim.taymans@gmail.com>
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

#ifndef __WFD_RTP_BUFFER_H__
#define __WFD_RTP_BUFFER_H__

#include <gst/gst.h>
#include <gst/rtp/gstrtcpbuffer.h>

typedef struct _WfdRTPBuffer WfdRTPBuffer;
typedef struct _WfdRTPBufferClass WfdRTPBufferClass;

#define WFD_TYPE_RTP_BUFFER             (wfd_rtp_buffer_get_type())
#define WFD_RTP_BUFFER(src)             (G_TYPE_CHECK_INSTANCE_CAST((src),WFD_TYPE_RTP_BUFFER,WfdRTPBuffer))
#define WFD_RTP_BUFFER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),WFD_TYPE_RTP_BUFFER,WfdRTPBufferClass))
#define WFD_IS_RTP_BUFFER(src)          (G_TYPE_CHECK_INSTANCE_TYPE((src),WFD_TYPE_RTP_BUFFER))
#define WFD_IS_RTP_BUFFER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),WFD_TYPE_RTP_BUFFER))
#define WFD_RTP_BUFFER_CAST(src)        ((WfdRTPBuffer *)(src))

/**
 * WfdRTPBufferMode:
 *
 * WFD_RTP_BUFFER_MODE_NONE: don't do any skew correction, outgoing
 *    timestamps are calculated directly from the RTP timestamps. This mode is
 *    good for recording but not for real-time applications.
 * WFD_RTP_BUFFER_MODE_SLAVE: calculate the skew between sender and receiver
 *    and produce smoothed adjusted outgoing timestamps. This mode is good for
 *    low latency communications.
 * WFD_RTP_BUFFER_MODE_BUFFER: buffer packets between low/high watermarks.
 *    This mode is good for streaming communication.
 * WFD_RTP_BUFFER_MODE_LAST: last buffer mode.
 *
 * The different buffer modes for a wfdrtpbuffer.
 */
typedef enum {
  WFD_RTP_BUFFER_MODE_NONE    = 0,
  WFD_RTP_BUFFER_MODE_SLAVE   = 1,
  WFD_RTP_BUFFER_MODE_BUFFER  = 2,
  WFD_RTP_BUFFER_MODE_LAST
} WfdRTPBufferMode;

#define WFD_TYPE_RTP_BUFFER_MODE (wfd_rtp_buffer_mode_get_type())
GType wfd_rtp_buffer_mode_get_type (void);

#define WFD_RTP_BUFFER_MAX_WINDOW 512
/**
 * WfdRTPBuffer:
 *
 * A WfdRTPBuffer in the #RTPSession
 */
struct _WfdRTPBuffer {
  GObject        object;

  GQueue        *packets;

  WfdRTPBufferMode mode;

  GstClockTime   delay;

  /* for buffering */
  gboolean          buffering;
  guint64           low_level;
  guint64           high_level;

  /* for calculating skew */
  GstClockTime   base_time;
  GstClockTime   base_rtptime;
  guint32        clock_rate;
  GstClockTime   base_extrtp;
  GstClockTime   prev_out_time;
  guint64        ext_rtptime;
  guint64        last_rtptime;
  gint64         window[WFD_RTP_BUFFER_MAX_WINDOW];
  guint          window_pos;
  guint          window_size;
  gboolean       window_filling;
  gint64         window_min;
  gint64         skew;
  gint64         prev_send_diff;
};

struct _WfdRTPBufferClass {
  GObjectClass   parent_class;
};

GType wfd_rtp_buffer_get_type (void);

/* managing lifetime */
WfdRTPBuffer*      wfd_rtp_buffer_new              (void);

WfdRTPBufferMode   wfd_rtp_buffer_get_mode         (WfdRTPBuffer *jbuf);
void                  wfd_rtp_buffer_set_mode         (WfdRTPBuffer *jbuf, WfdRTPBufferMode mode);

GstClockTime          wfd_rtp_buffer_get_delay        (WfdRTPBuffer *jbuf);
void                  wfd_rtp_buffer_set_delay        (WfdRTPBuffer *jbuf, GstClockTime delay);

void                  wfd_rtp_buffer_reset_skew       (WfdRTPBuffer *jbuf);

gboolean              wfd_rtp_buffer_insert           (WfdRTPBuffer *jbuf, GstBuffer *buf,
                                                          GstClockTime time,
                                                          guint32 clock_rate,
                                                          gboolean *tail, gint *percent);
GstBuffer *           wfd_rtp_buffer_peek             (WfdRTPBuffer *jbuf);
GstBuffer *           wfd_rtp_buffer_pop              (WfdRTPBuffer *jbuf, gint *percent);

void                  wfd_rtp_buffer_flush            (WfdRTPBuffer *jbuf);

gboolean              wfd_rtp_buffer_is_buffering     (WfdRTPBuffer * jbuf);
void                  wfd_rtp_buffer_set_buffering    (WfdRTPBuffer * jbuf, gboolean buffering);
gint                  wfd_rtp_buffer_get_percent      (WfdRTPBuffer * jbuf);

guint                 wfd_rtp_buffer_num_packets      (WfdRTPBuffer *jbuf);
guint32               wfd_rtp_buffer_get_ts_diff      (WfdRTPBuffer *jbuf);

void                  wfd_rtp_buffer_get_sync         (WfdRTPBuffer *jbuf, guint64 *rtptime,
                                                          guint64 *timestamp, guint32 *clock_rate,
                                                          guint64 *last_rtptime);

#endif /* __WFD_RTP_BUFFER_H__ */
