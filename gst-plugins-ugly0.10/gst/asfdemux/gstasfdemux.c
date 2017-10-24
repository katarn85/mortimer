/* GStreamer ASF/WMV/WMA demuxer
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2006-2009 Tim-Philipp Müller <tim centricular net>
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

/* TODO:
 *
 * - _loop():
 *   stop if at end of segment if != end of file, ie. demux->segment.stop
 *
 * - fix packet parsing:
 *   there's something wrong with timestamps for packets with keyframes,
 *   and durations too.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* FIXME 0.11: suppress warnings for deprecated API such as GStaticRecMutex
 * with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <gst/gstutils.h>
#include <gst/base/gstbytereader.h>
#include <gst/riff/riff-media.h>
#include <gst/tag/tag.h>
#include <gst/gst-i18n-plugin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gstasfdemux.h"
#include "asfheaders.h"
#include "asfpacket.h"

static GstStaticPadTemplate gst_asf_demux_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-ms-asf")
    );

static GstStaticPadTemplate audio_src_template =
GST_STATIC_PAD_TEMPLATE ("audio_%02d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate video_src_template =
GST_STATIC_PAD_TEMPLATE ("video_%02d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

/* size of an ASF object header, ie. GUID (16 bytes) + object size (8 bytes) */
#define ASF_OBJECT_HEADER_SIZE  (16+8)

/* FIXME: get rid of this */
/* abuse this GstFlowReturn enum for internal usage */
#define ASF_FLOW_NEED_MORE_DATA  99

#define gst_asf_get_flow_name(flow)    \
  (flow == ASF_FLOW_NEED_MORE_DATA) ?  \
  "need-more-data" : gst_flow_get_name (flow)

GST_DEBUG_CATEGORY (asfdemux_dbg);

static GstStateChangeReturn gst_asf_demux_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_asf_demux_element_send_event (GstElement * element,
    GstEvent * event);
static gboolean gst_asf_demux_send_event_unlocked (GstASFDemux * demux,
    GstEvent * event);
static gboolean gst_asf_demux_handle_src_query (GstPad * pad, GstQuery * query);
static const GstQueryType *gst_asf_demux_get_src_query_types (GstPad * pad);
static GstFlowReturn gst_asf_demux_chain (GstPad * pad, GstBuffer * buf);
static gboolean gst_asf_demux_sink_event (GstPad * pad, GstEvent * event);
static GstFlowReturn gst_asf_demux_process_object (GstASFDemux * demux,
    guint8 ** p_data, guint64 * p_size);
static gboolean gst_asf_demux_activate (GstPad * sinkpad);
static gboolean gst_asf_demux_activate_push (GstPad * sinkpad, gboolean active);
static gboolean gst_asf_demux_activate_pull (GstPad * sinkpad, gboolean active);
static void gst_asf_demux_loop (GstASFDemux * demux);
static void
gst_asf_demux_process_queued_extended_stream_objects (GstASFDemux * demux);
static gboolean gst_asf_demux_pull_headers (GstASFDemux * demux);
static void gst_asf_demux_pull_indices (GstASFDemux * demux);
static void gst_asf_demux_reset_stream_state_after_discont (GstASFDemux * asf);
static gboolean
gst_asf_demux_parse_data_object_start (GstASFDemux * demux, guint8 * data);
static void gst_asf_demux_descramble_buffer (GstASFDemux * demux,
    AsfStream * stream, GstBuffer ** p_buffer);
static void gst_asf_demux_activate_stream (GstASFDemux * demux,
    AsfStream * stream);
static GstStructure *gst_asf_demux_get_metadata_for_stream (GstASFDemux * d,
    guint stream_num);
static GstFlowReturn gst_asf_demux_push_complete_payloads (GstASFDemux * demux,
    gboolean force);
#ifdef ASFDEMUX_MODIFICATION

static gboolean gst_asf_demux_forward_trickplay (GstASFDemux * demux, AsfStream *stream, AsfPayload *payload);
static gboolean gst_asf_demux_backward_trickplay(GstASFDemux * demux, AsfStream *stream, AsfPayload *payload);
static gboolean gst_asf_demux_parse_payload_custom (GstASFDemux * demux, AsfPacket * packet, gint lentype, const guint8 ** p_data, 
    guint * p_size, AsfPayload *next_kpayload, gboolean *is_video);
static gboolean gst_asf_demux_find_next_keypacket (GstASFDemux * demux);
static gboolean gst_asf_demux_find_prev_keypacket (GstASFDemux * demux);
static guint32
gst_asf_demux_find_num_samples_bw_kpayloads (GstASFDemux * demux, AsfPayload *start_kpayload, AsfPayload *end_kpayload);
static gboolean 
gst_asf_demux_find_keypayload_from_packet (GstASFDemux * demux, AsfPayload **next_kpayload);
static gboolean gst_asf_demux_create_simple_index_table (GstASFDemux *demux);
static gboolean gst_asf_demux_index_table_parse_packet (GstASFDemux * demux, GstBuffer * buf, GQueue *sidx_queue, gint *packet_num);
static gboolean gst_asf_demux_index_table_parse_payload (GstASFDemux * demux, AsfPacket * packet,
    gint lentype, const guint8 ** p_data, guint * p_size, AsfKeyPacketInfo *keyinfo);
static gboolean gst_asf_demux_push_sidx_queue(GstASFDemux * demux, GQueue *sidx_queue, gint *packet_num, AsfKeyPacketInfo *keyinfo);
#endif
#ifdef CODEC_ENTRY
static GstFlowReturn
gst_asf_demux_process_codec_list (GstASFDemux * demux, guint8 * data, guint64 size);
#endif

GST_BOILERPLATE (GstASFDemux, gst_asf_demux, GstElement, GST_TYPE_ELEMENT);

static void
gst_asf_demux_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (element_class,
      &audio_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &video_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_asf_demux_sink_template);

  gst_element_class_set_details_simple (element_class, "ASF Demuxer",
      "Codec/Demuxer",
      "Demultiplexes ASF Streams", "Owen Fraser-Green <owen@discobabe.net>");
}

static void
gst_asf_demux_class_init (GstASFDemuxClass * klass)
{
  GstElementClass *gstelement_class;

  gstelement_class = (GstElementClass *) klass;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_asf_demux_change_state);
  gstelement_class->send_event =
      GST_DEBUG_FUNCPTR (gst_asf_demux_element_send_event);
}

static void
gst_asf_demux_free_stream (GstASFDemux * demux, AsfStream * stream)
{
  gst_caps_replace (&stream->caps, NULL);
  if (stream->pending_tags) {
    gst_tag_list_free (stream->pending_tags);
    stream->pending_tags = NULL;
  }
  if (stream->pad) {
    if (stream->active)
      gst_element_remove_pad (GST_ELEMENT_CAST (demux), stream->pad);
    else
      gst_object_unref (stream->pad);
    stream->pad = NULL;
  }

  while (stream->payloads && stream->payloads->len > 0) {
    AsfPayload *payload;
    guint last;

    last = stream->payloads->len - 1;
    payload = &g_array_index (stream->payloads, AsfPayload, last);
    gst_buffer_replace (&payload->buf, NULL);
    g_array_remove_index (stream->payloads, last);
  }
  if (stream->payloads) {
    g_array_free (stream->payloads, TRUE);
    stream->payloads = NULL;
  }
  if (stream->ext_props.valid) {
    g_free (stream->ext_props.payload_extensions);
    stream->ext_props.payload_extensions = NULL;
  }
}

static void
gst_asf_demux_reset (GstASFDemux * demux, gboolean chain_reset)
{
  GST_LOG_OBJECT (demux, "resetting");

  gst_segment_init (&demux->segment, GST_FORMAT_UNDEFINED);
  demux->segment_running = FALSE;
  if (demux->adapter && !chain_reset) {
    gst_adapter_clear (demux->adapter);
    g_object_unref (demux->adapter);
    demux->adapter = NULL;
  }
  if (demux->taglist) {
    gst_tag_list_free (demux->taglist);
    demux->taglist = NULL;
  }
  if (demux->metadata) {
    gst_caps_unref (demux->metadata);
    demux->metadata = NULL;
  }
  if (demux->global_metadata) {
    gst_structure_free (demux->global_metadata);
    demux->global_metadata = NULL;
  }

  demux->state = GST_ASF_DEMUX_STATE_HEADER;
  g_free (demux->objpath);
  demux->objpath = NULL;
  g_strfreev (demux->languages);
  demux->languages = NULL;
  demux->num_languages = 0;
  g_slist_foreach (demux->ext_stream_props, (GFunc) gst_mini_object_unref,
      NULL);
  g_slist_free (demux->ext_stream_props);
  demux->ext_stream_props = NULL;

  while (demux->old_num_streams > 0) {
    gst_asf_demux_free_stream (demux,
        &demux->old_stream[demux->old_num_streams - 1]);
    --demux->old_num_streams;
  }
  memset (demux->old_stream, 0, sizeof (demux->old_stream));
  demux->old_num_streams = 0;

  /* when resetting for a new chained asf, we don't want to remove the pads
   * before adding the new ones */
  if (chain_reset) {
    memcpy (demux->old_stream, demux->stream, sizeof (demux->stream));
    demux->old_num_streams = demux->num_streams;
    demux->num_streams = 0;
  }

  while (demux->num_streams > 0) {
    gst_asf_demux_free_stream (demux, &demux->stream[demux->num_streams - 1]);
    --demux->num_streams;
  }
  memset (demux->stream, 0, sizeof (demux->stream));
  if (!chain_reset) {
    /* do not remove those for not adding pads with same name */
    demux->num_audio_streams = 0;
    demux->num_video_streams = 0;
  }
  demux->num_streams = 0;
  demux->activated_streams = FALSE;
  demux->first_ts = GST_CLOCK_TIME_NONE;
  demux->segment_ts = GST_CLOCK_TIME_NONE;
  demux->in_gap = 0;
  if (!chain_reset)
    gst_segment_init (&demux->in_segment, GST_FORMAT_UNDEFINED);
  demux->state = GST_ASF_DEMUX_STATE_HEADER;
  demux->seekable = FALSE;
  demux->broadcast = FALSE;
  demux->sidx_interval = 0;
  demux->sidx_num_entries = 0;
  g_free (demux->sidx_entries);
  demux->sidx_entries = NULL;

  demux->speed_packets = 1;

  if (chain_reset) {
    GST_LOG_OBJECT (demux, "Restarting");
    gst_segment_init (&demux->segment, GST_FORMAT_TIME);
    demux->need_newsegment = TRUE;
    demux->segment_running = FALSE;
    demux->accurate = FALSE;
    demux->metadata = gst_caps_new_empty ();
    demux->global_metadata = gst_structure_empty_new ("metadata");
    demux->data_size = 0;
    demux->data_offset = 0;
    demux->index_offset = 0;
  } else {
    demux->base_offset = 0;
  }
#ifdef CODEC_ENTRY
  if (demux->codec_entry) {
    int i=0;
    for (i<0; i<demux->codec_entry_count; i++) {
      GST_LOG_OBJECT (demux, "Trying to free codec entry [%d]", i);
      if (demux->codec_entry[i].name) {
        g_free (demux->codec_entry[i].name);
        demux->codec_entry[i].name = NULL;
      }
      if (demux->codec_entry[i].description) {
        g_free (demux->codec_entry[i].description);
        demux->codec_entry[i].description = NULL;
      }
      if (demux->codec_entry[i].information) {
        g_free (demux->codec_entry[i].information);
        demux->codec_entry[i].information = NULL;
      }
    }
    g_free (demux->codec_entry);
    demux->codec_entry = NULL;
    demux->codec_entry_count = 0;
  }
#endif
}

static void
gst_asf_demux_init (GstASFDemux * demux, GstASFDemuxClass * klass)
{
  demux->sinkpad =
      gst_pad_new_from_static_template (&gst_asf_demux_sink_template, "sink");
  gst_pad_set_chain_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_asf_demux_chain));
  gst_pad_set_event_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_asf_demux_sink_event));
  gst_pad_set_activate_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_asf_demux_activate));
  gst_pad_set_activatepull_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_asf_demux_activate_pull));
  gst_pad_set_activatepush_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_asf_demux_activate_push));
  gst_element_add_pad (GST_ELEMENT (demux), demux->sinkpad);

#ifdef ASFDEMUX_MODIFICATION
  demux->num_audio_streams = 0;
  demux->num_video_streams = 0;
  demux->num_streams = 0;
  demux->vidStrmId = -1;
  demux->audStrmId = -1;
  demux->firstAudBufSent = FALSE;
  demux->firstVidBufSent = FALSE;
  demux->trickplay_info.first_video_push = FALSE;
  demux->trickplay_info.resume_nrl_play = FALSE;
  demux->trickplay_info.first_payload = FALSE;
  demux->taglist = NULL;
  demux->first_ts = GST_CLOCK_TIME_NONE;
  demux->state = GST_ASF_DEMUX_STATE_HEADER;
  demux->current_ts = 0;
  demux->stop_ts = -1;
#endif
#ifdef LAST_STOP_SEGMENT
  demux->packet_to_stop = 0;
#endif
  /* set initial state */
  gst_asf_demux_reset (demux, FALSE);
}

static gboolean
gst_asf_demux_activate (GstPad * sinkpad)
{
  if (gst_pad_check_pull_range (sinkpad)) {
    return gst_pad_activate_pull (sinkpad, TRUE);
  } else {
    return gst_pad_activate_push (sinkpad, TRUE);
  }
}

static gboolean
gst_asf_demux_activate_push (GstPad * sinkpad, gboolean active)
{
  GstASFDemux *demux;

  demux = GST_ASF_DEMUX (GST_OBJECT_PARENT (sinkpad));

  demux->state = GST_ASF_DEMUX_STATE_HEADER;
  demux->streaming = TRUE;

  return TRUE;
}

static gboolean
gst_asf_demux_activate_pull (GstPad * pad, gboolean active)
{
  GstASFDemux *demux;

  demux = GST_ASF_DEMUX (GST_OBJECT_PARENT (pad));

  if (active) {
    demux->state = GST_ASF_DEMUX_STATE_HEADER;
    demux->streaming = FALSE;

    return gst_pad_start_task (pad, (GstTaskFunction) gst_asf_demux_loop,
        demux);
  } else {
    return gst_pad_stop_task (pad);
  }
}


static gboolean
gst_asf_demux_sink_event (GstPad * pad, GstEvent * event)
{
  GstASFDemux *demux;
  gboolean ret = TRUE;

  demux = GST_ASF_DEMUX (gst_pad_get_parent (pad));

  GST_LOG_OBJECT (demux, "handling %s event", GST_EVENT_TYPE_NAME (event));
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:{
      GstFormat newsegment_format;
      gint64 newsegment_start, stop, time;
      gdouble rate, arate;
      gboolean update;

      gst_event_parse_new_segment_full (event, &update, &rate, &arate,
          &newsegment_format, &newsegment_start, &stop, &time);

      if (newsegment_format == GST_FORMAT_BYTES) {
        if (demux->packet_size && newsegment_start > demux->data_offset)
          demux->packet = (newsegment_start - demux->data_offset) /
              demux->packet_size;
        else
          demux->packet = 0;
      } else if (newsegment_format == GST_FORMAT_TIME) {
        /* do not know packet position, not really a problem */
        demux->packet = -1;
      } else {
        GST_WARNING_OBJECT (demux, "unsupported newsegment format, ignoring");
        gst_event_unref (event);
        break;
      }

      /* record upstream segment for interpolation */
      if (newsegment_format != demux->in_segment.format)
        gst_segment_init (&demux->in_segment, GST_FORMAT_UNDEFINED);
      gst_segment_set_newsegment_full (&demux->in_segment, update, rate, arate,
          newsegment_format, newsegment_start, stop, time);

      /* in either case, clear some state and generate newsegment later on */
      GST_OBJECT_LOCK (demux);
      demux->segment_ts = GST_CLOCK_TIME_NONE;
      demux->in_gap = GST_CLOCK_TIME_NONE;
      demux->need_newsegment = TRUE;
      gst_asf_demux_reset_stream_state_after_discont (demux);
      GST_OBJECT_UNLOCK (demux);

      gst_event_unref (event);
      break;
    }
    case GST_EVENT_EOS:{
      GstFlowReturn flow;

      if (demux->state == GST_ASF_DEMUX_STATE_HEADER) {
        GST_ELEMENT_ERROR (demux, STREAM, DEMUX,
            (_("This stream contains no data.")),
            ("got eos and didn't receive a complete header object"));
        break;
      }
      flow = gst_asf_demux_push_complete_payloads (demux, TRUE);
      if (flow < GST_FLOW_UNEXPECTED || flow == GST_FLOW_NOT_LINKED) {
        GST_ELEMENT_ERROR (demux, STREAM, FAILED,
            (_("Internal data stream error.")),
            ("streaming stopped, reason %s", gst_flow_get_name (flow)));
        break;
      }

      GST_OBJECT_LOCK (demux);
      gst_adapter_clear (demux->adapter);
      GST_OBJECT_UNLOCK (demux);
      gst_asf_demux_send_event_unlocked (demux, event);
      break;
    }

    case GST_EVENT_FLUSH_STOP:
      GST_OBJECT_LOCK (demux);
      gst_asf_demux_reset_stream_state_after_discont (demux);
      GST_OBJECT_UNLOCK (demux);
      gst_asf_demux_send_event_unlocked (demux, event);
      /* upon activation, latency is no longer introduced, e.g. after seek */
      if (demux->activated_streams)
        demux->latency = 0;
      break;

    default:
      ret = gst_pad_event_default (pad, event);
      break;
  }

  gst_object_unref (demux);
  return ret;
}

static gboolean
gst_asf_demux_seek_index_lookup (GstASFDemux * demux, guint * packet,
    GstClockTime seek_time, GstClockTime * p_idx_time, guint * speed)
{
  GstClockTime idx_time;
  guint idx;

  if (G_UNLIKELY (demux->sidx_num_entries == 0 || demux->sidx_interval == 0))
    return FALSE;

  idx = (guint) ((seek_time + demux->preroll) / demux->sidx_interval);

#ifdef ASFDEMUX_MODIFICATION
  demux->seek_idx = idx;  
  GST_DEBUG_OBJECT (demux, "seek_time = %"GST_TIME_FORMAT", seek index  = %d",
    GST_TIME_ARGS(seek_time), demux->seek_idx);
#endif

  /* FIXME: seek beyond end of file should result in immediate EOS from
   * streaming thread instead of a failed seek */
  if (G_UNLIKELY (idx >= demux->sidx_num_entries)) {
    GST_WARNING ("Seek beyond Simple idx entires...");
    return FALSE;
  }

  *packet = demux->sidx_entries[idx].packet;
  if (speed)
    *speed = demux->sidx_entries[idx].count;

  /* so we get closer to the actual time of the packet ... actually, let's not
   * do this, since we throw away superfluous payloads before the seek position
   * anyway; this way, our key unit seek 'snap resolution' is a bit better
   * (ie. same as index resolution) */
  /*
     while (idx > 0 && demux->sidx_entries[idx-1] == demux->sidx_entries[idx])
     --idx;
   */

  idx_time = demux->sidx_interval * idx;
  if (G_LIKELY (idx_time >= demux->preroll))
    idx_time -= demux->preroll;

  GST_DEBUG_OBJECT (demux, "%" GST_TIME_FORMAT " => packet %u at %"
      GST_TIME_FORMAT, GST_TIME_ARGS (seek_time), *packet,
      GST_TIME_ARGS (idx_time));

  if (G_LIKELY (p_idx_time))
    *p_idx_time = idx_time;

  return TRUE;
}

static void
gst_asf_demux_reset_stream_state_after_discont (GstASFDemux * demux)
{
  guint n;

  gst_adapter_clear (demux->adapter);

  GST_DEBUG_OBJECT (demux, "reset stream state");

  for (n = 0; n < demux->num_streams; n++) {
    demux->stream[n].discont = TRUE;
    demux->stream[n].last_flow = GST_FLOW_OK;

    while (demux->stream[n].payloads->len > 0) {
      AsfPayload *payload;
      guint last;

      last = demux->stream[n].payloads->len - 1;
      payload = &g_array_index (demux->stream[n].payloads, AsfPayload, last);
      gst_buffer_replace (&payload->buf, NULL);
      g_array_remove_index (demux->stream[n].payloads, last);
    }
  }
}

static void
gst_asf_demux_mark_discont (GstASFDemux * demux)
{
  guint n;

  GST_DEBUG_OBJECT (demux, "Mark stream discont");

  for (n = 0; n < demux->num_streams; n++)
    demux->stream[n].discont = TRUE;
}

/* do a seek in push based mode */
static gboolean
gst_asf_demux_handle_seek_push (GstASFDemux * demux, GstEvent * event)
{
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType cur_type, stop_type;
  gint64 cur, stop;
  guint packet;
  gboolean res;

  gst_event_parse_seek (event, &rate, &format, &flags, &cur_type, &cur,
      &stop_type, &stop);

  stop_type = GST_SEEK_TYPE_NONE;
  stop = -1;

  GST_DEBUG_OBJECT (demux, "seeking to %" GST_TIME_FORMAT, GST_TIME_ARGS (cur));

  /* determine packet, by index or by estimation */
  if (!gst_asf_demux_seek_index_lookup (demux, &packet, cur, NULL, NULL)) {
    packet = (guint) gst_util_uint64_scale (demux->num_packets,
        cur, demux->play_time);
  }

  if (packet > demux->num_packets) {
    GST_DEBUG_OBJECT (demux, "could not determine packet to seek to, "
        "seek aborted.");
    return FALSE;
  }

  GST_DEBUG_OBJECT (demux, "seeking to packet %d", packet);

  cur = demux->data_offset + (packet * demux->packet_size);

  GST_DEBUG_OBJECT (demux, "Pushing BYTE seek rate %g, "
      "start %" G_GINT64_FORMAT ", stop %" G_GINT64_FORMAT, rate, cur, stop);
  /* BYTE seek event */
  event = gst_event_new_seek (rate, GST_FORMAT_BYTES, flags, cur_type, cur,
      stop_type, stop);
  res = gst_pad_push_event (demux->sinkpad, event);

  return res;
}

static gboolean
gst_asf_demux_handle_seek_event (GstASFDemux * demux, GstEvent * event)
{
  GstClockTime idx_time;
  GstSegment segment;
  GstSeekFlags flags;
  GstSeekType cur_type, stop_type;
  GstFormat format;
  gboolean only_need_update;
  gboolean keyunit_sync;
  gboolean flush;
  gdouble rate;
  gint64 cur, stop;
  gint64 seek_time;
  guint packet, speed_count = 1;
#ifdef LAST_STOP_SEGMENT
  gint64 seek_end_time = 0;
  guint packet_to_stop = 0;
#endif

  if (G_UNLIKELY (demux->seekable == FALSE || demux->packet_size == 0 ||
          demux->num_packets == 0 || demux->play_time == 0)) {
    GST_LOG_OBJECT (demux, "stream is not seekable");
    return FALSE;
  }

  if (G_UNLIKELY (!demux->activated_streams)) {
    GST_LOG_OBJECT (demux, "streams not yet activated, ignoring seek");
    return FALSE;
  }

  gst_event_parse_seek (event, &rate, &format, &flags, &cur_type, &cur,
      &stop_type, &stop);

#ifdef ASFDEMUX_MODIFICATION
  GST_INFO_OBJECT (demux, "Going to seek to %"GST_TIME_FORMAT" with rate=%f",
    GST_TIME_ARGS(cur), rate);
  GST_INFO("stop is %"GST_TIME_FORMAT, GST_TIME_ARGS(stop));
  demux->stop_ts = stop;
#endif

  if (G_UNLIKELY (format != GST_FORMAT_TIME)) {
    GST_LOG_OBJECT (demux, "seeking is only supported in TIME format");
    return FALSE;
  }

#ifndef ASFDEMUX_MODIFICATION
  if (G_UNLIKELY (rate <= 0.0)) {
    GST_LOG_OBJECT (demux, "backward playback is not supported yet");
    return FALSE;
  }
#endif

  flush = ((flags & GST_SEEK_FLAG_FLUSH) == GST_SEEK_FLAG_FLUSH);
  demux->accurate =
      ((flags & GST_SEEK_FLAG_ACCURATE) == GST_SEEK_FLAG_ACCURATE);
  keyunit_sync = ((flags & GST_SEEK_FLAG_KEY_UNIT) == GST_SEEK_FLAG_KEY_UNIT);

  if (G_UNLIKELY (demux->streaming)) {
    /* support it safely needs more segment handling, e.g. closing etc */
    if (!flush) {
      GST_LOG_OBJECT (demux, "streaming; non-flushing seek not supported");
      return FALSE;
    }
    /* we can (re)construct the start later on, but not the end */
    if (stop_type != GST_SEEK_TYPE_NONE) {
      GST_LOG_OBJECT (demux, "streaming; end type must be NONE");
      return FALSE;
    }
    gst_event_ref (event);
    /* upstream might handle TIME seek, e.g. mms or rtsp,
     * or not, e.g. http, then we give it a hand */
    if (!gst_pad_push_event (demux->sinkpad, event))
      return gst_asf_demux_handle_seek_push (demux, event);
    else
      return TRUE;
  }

  /* unlock the streaming thread */
  if (G_LIKELY (flush)) {
    gst_pad_push_event (demux->sinkpad, gst_event_new_flush_start ());
    gst_asf_demux_send_event_unlocked (demux, gst_event_new_flush_start ());
  } else {
    gst_pad_pause_task (demux->sinkpad);
  }

  /* grab the stream lock so that streaming cannot continue, for
   * non flushing seeks when the element is in PAUSED this could block
   * forever */
  GST_PAD_STREAM_LOCK (demux->sinkpad);

  /* we now can stop flushing, since we have the stream lock now */
  gst_pad_push_event (demux->sinkpad, gst_event_new_flush_stop ());

  if (G_LIKELY (flush))
    gst_asf_demux_send_event_unlocked (demux, gst_event_new_flush_stop ());

  /* operating on copy of segment until we know the seek worked */
  segment = demux->segment;
#ifdef ASFDEMUX_MODIFICATION
  segment.stop = stop;
#endif

  if (G_UNLIKELY (demux->segment_running && !flush)) {
    GstEvent *newseg;

    /* create the segment event to close the current segment */
#ifdef ASFDEMUX_MODIFICATION
    newseg = gst_event_new_new_segment (TRUE, segment.rate,
        GST_FORMAT_TIME, segment.start, stop, segment.time);
#else
    newseg = gst_event_new_new_segment (TRUE, segment.rate,
        GST_FORMAT_TIME, segment.start, segment.last_stop, segment.time);
#endif

    gst_asf_demux_send_event_unlocked (demux, newseg);
  }

  gst_segment_set_seek (&segment, rate, format, flags, cur_type,
      cur, stop_type, stop, &only_need_update);

#ifdef ASFDEMUX_MODIFICATION
  gst_segment_set_last_stop (&segment, GST_FORMAT_TIME, cur);
  demux->trickplay_info.next_keyframe_ts = cur;
  demux->trickplay_info.first_video_push = FALSE;
  demux->trickplay_info.first_payload = FALSE;
#endif

  GST_DEBUG_OBJECT (demux, "seeking to time %" GST_TIME_FORMAT ", segment: "
      "%" GST_SEGMENT_FORMAT, GST_TIME_ARGS (segment.start), &segment);

#ifdef ASFDEMUX_MODIFICATION
  /*Modification: Trickplay support added*/
  if (segment.rate > 0.0) {
    if (segment.rate == 1.0) {
      seek_time = segment.start;
      #ifdef LAST_STOP_SEGMENT
      seek_end_time = segment.stop;
      #endif
      segment.stop = GST_CLOCK_TIME_NONE;
    } else {
      seek_time = segment.start = segment.last_stop;
      segment.stop=GST_CLOCK_TIME_NONE;
    }
  } else {
    // TODO:  last_stop to cur time
    seek_time = segment.last_stop;
    segment.start = 0;
    segment.stop = segment.last_stop; // TODO: I think stop would be last_stop ??
  }
#else
  seek_time = segment.start;
#endif

  /* FIXME: should check the KEY_UNIT flag; need to adjust last_stop to
   * real start of data and segment_start to indexed time for key unit seek*/
  if (G_UNLIKELY (!gst_asf_demux_seek_index_lookup (demux, &packet, seek_time,
              &idx_time, &speed_count))) {
    /* First try to query our source to see if it can convert for us. This is
       the case when our source is an mms stream, notice that in this case
       gstmms will do a time based seek to get the byte offset, this is not a
       problem as the seek to this offset needs to happen anway. */
    gint64 offset;
    GstFormat dest_format = GST_FORMAT_BYTES;

    if (gst_pad_query_peer_convert (demux->sinkpad, GST_FORMAT_TIME, seek_time,
            &dest_format, &offset) && dest_format == GST_FORMAT_BYTES) {
      packet = (offset - demux->data_offset) / demux->packet_size;
      GST_LOG_OBJECT (demux, "convert %" GST_TIME_FORMAT
          " to bytes query result: %" G_GINT64_FORMAT ", data_ofset: %"
          G_GINT64_FORMAT ", packet_size: %u," " resulting packet: %u\n",
          GST_TIME_ARGS (seek_time), offset, demux->data_offset,
          demux->packet_size, packet);
    } else {
      /* FIXME: For streams containing video, seek to an earlier position in
       * the hope of hitting a keyframe and let the sinks throw away the stuff
       * before the segment start. For audio-only this is unnecessary as every
       * frame is 'key'. */
      if (flush && (demux->accurate || keyunit_sync)
          && demux->num_video_streams > 0) {
        seek_time -= 5 * GST_SECOND;
        if (seek_time < 0)
          seek_time = 0;
      }

      packet = (guint) gst_util_uint64_scale (demux->num_packets,
          seek_time, demux->play_time);

      if (packet > demux->num_packets)
        packet = demux->num_packets;

#ifdef LAST_STOP_SEGMENT
      if (segment.stop != segment.duration) {
        //seek_end_time += 1 * GST_SECOND; /*FIXME : demuxer can't find exact stop packet currently. */
        packet_to_stop = (guint)gst_util_uint64_scale (demux->num_packets, seek_end_time, demux->play_time); /* here, play_time is total duration */

        GST_INFO_OBJECT(demux, " packet to start = %u, total packets = %u, packet to stop = %u\n",
        (guint)packet, (guint)demux->num_packets, (guint)packet_to_stop);

        if (packet_to_stop > demux->num_packets)
          packet_to_stop = demux->num_packets;
      }
#endif
    }
  } else {
    if (G_LIKELY (keyunit_sync)) {
      GST_DEBUG_OBJECT (demux, "key unit seek, adjust seek_time = %"
          GST_TIME_FORMAT " to index_time = %" GST_TIME_FORMAT,
          GST_TIME_ARGS (seek_time), GST_TIME_ARGS (idx_time));
#ifdef ASFDEMUX_MODIFICATION
      if (segment.rate < 0.0) {
        segment.last_stop = idx_time;
        segment.time = idx_time;
      } else {
 #endif
        segment.start = idx_time;
        segment.last_stop = idx_time;
        segment.time = idx_time;
#ifdef ASFDEMUX_MODIFICATION
      }
 #endif
    }
  }

  GST_DEBUG_OBJECT (demux, "seeking to packet %u (%d)", packet, speed_count);

  GST_OBJECT_LOCK (demux);
  demux->segment = segment;
  demux->packet = packet;
#ifdef LAST_STOP_SEGMENT
  if (packet_to_stop)
    demux->packet_to_stop = packet_to_stop;
#endif
  demux->need_newsegment = TRUE;
  demux->speed_packets = speed_count;
  gst_asf_demux_reset_stream_state_after_discont (demux);
#ifdef ASFDEMUX_MODIFICATION
  if (demux->segment.rate != 1.0) {
    demux->trickplay_info.kpacket_dur_diff = 0;
    demux->trickplay_info.prev_kpacket = 0;
    if (demux->segment.rate  < 0.0)
      demux->trickplay_info.prev_kpacket = -1;
    demux->trickplay_info.next_kpacket = 0;
    demux->trickplay_info.show_samples = 0;
    demux->trickplay_info.start_ts = GST_CLOCK_TIME_NONE;
    demux->trickplay_info.is_eos = FALSE;
    demux->trickplay_info.cur_kpacket = 0;
  }
  demux->firstAudBufSent = FALSE;
  demux->firstVidBufSent = FALSE;
#endif
  GST_OBJECT_UNLOCK (demux);

  /* restart our task since it might have been stopped when we did the flush */
  gst_pad_start_task (demux->sinkpad, (GstTaskFunction) gst_asf_demux_loop,
      demux);

  /* streaming can continue now */
  GST_PAD_STREAM_UNLOCK (demux->sinkpad);

  return TRUE;
}

static gboolean
gst_asf_demux_handle_src_event (GstPad * pad, GstEvent * event)
{
  GstASFDemux *demux;
  gboolean ret;

  demux = GST_ASF_DEMUX (gst_pad_get_parent (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      GST_LOG_OBJECT (pad, "seek event");
      ret = gst_asf_demux_handle_seek_event (demux, event);
      gst_event_unref (event);
      break;
    case GST_EVENT_QOS:
    case GST_EVENT_NAVIGATION:
      /* just drop these two silently */
      gst_event_unref (event);
      ret = FALSE;
      break;
    default:
      GST_LOG_OBJECT (pad, "%s event", GST_EVENT_TYPE_NAME (event));
      ret = gst_pad_event_default (pad, event);
      break;
  }

  gst_object_unref (demux);
  return ret;
}

static inline guint32
gst_asf_demux_identify_guid (const ASFGuidHash * guids, ASFGuid * guid)
{
  guint32 ret;

  ret = gst_asf_identify_guid (guids, guid);

  GST_LOG ("%s  0x%08x-0x%08x-0x%08x-0x%08x",
      gst_asf_get_guid_nick (guids, ret),
      guid->v1, guid->v2, guid->v3, guid->v4);

  return ret;
}

typedef struct
{
  AsfObjectID id;
  guint64 size;
} AsfObject;


/* expect is true when the user is expeting an object,
 * when false, it will give no warnings if the object
 * is not identified
 */
static gboolean
asf_demux_peek_object (GstASFDemux * demux, const guint8 * data,
    guint data_len, AsfObject * object, gboolean expect)
{
  ASFGuid guid;

  if (data_len < ASF_OBJECT_HEADER_SIZE)
    return FALSE;

  guid.v1 = GST_READ_UINT32_LE (data + 0);
  guid.v2 = GST_READ_UINT32_LE (data + 4);
  guid.v3 = GST_READ_UINT32_LE (data + 8);
  guid.v4 = GST_READ_UINT32_LE (data + 12);

  object->size = GST_READ_UINT64_LE (data + 16);

  /* FIXME: make asf_demux_identify_object_guid() */
  object->id = gst_asf_demux_identify_guid (asf_object_guids, &guid);
  if (object->id == ASF_OBJ_UNDEFINED && expect) {
    GST_WARNING_OBJECT (demux, "Unknown object %08x-%08x-%08x-%08x",
        guid.v1, guid.v2, guid.v3, guid.v4);
  }

  return TRUE;
}

static void
gst_asf_demux_release_old_pads (GstASFDemux * demux)
{
  GST_DEBUG_OBJECT (demux, "Releasing old pads");

  while (demux->old_num_streams > 0) {
    gst_pad_push_event (demux->old_stream[demux->old_num_streams - 1].pad,
        gst_event_new_eos ());
    gst_asf_demux_free_stream (demux,
        &demux->old_stream[demux->old_num_streams - 1]);
    --demux->old_num_streams;
  }
  memset (demux->old_stream, 0, sizeof (demux->old_stream));
  demux->old_num_streams = 0;
}

static GstFlowReturn
gst_asf_demux_chain_headers (GstASFDemux * demux)
{
  GstFlowReturn flow;
  AsfObject obj;
  guint8 *header_data, *data = NULL;
  const guint8 *cdata = NULL;
  guint64 header_size;

  cdata = (guint8 *) gst_adapter_peek (demux->adapter, ASF_OBJECT_HEADER_SIZE);
  if (cdata == NULL)
    goto need_more_data;

  asf_demux_peek_object (demux, cdata, ASF_OBJECT_HEADER_SIZE, &obj, TRUE);
  if (obj.id != ASF_OBJ_HEADER)
    goto wrong_type;

  GST_LOG_OBJECT (demux, "header size = %u", (guint) obj.size);

  /* + 50 for non-packet data at beginning of ASF_OBJ_DATA */
  if (gst_adapter_available (demux->adapter) < obj.size + 50)
    goto need_more_data;

  data = gst_adapter_take (demux->adapter, obj.size + 50);

  header_data = data;
  header_size = obj.size;
  flow = gst_asf_demux_process_object (demux, &header_data, &header_size);
  if (flow != GST_FLOW_OK)
    goto parse_failed;

  /* calculate where the packet data starts */
  demux->data_offset = obj.size + 50;

  /* now parse the beginning of the ASF_OBJ_DATA object */
  if (!gst_asf_demux_parse_data_object_start (demux, data + obj.size))
    goto wrong_type;

  if (demux->num_streams == 0)
    goto no_streams;

  g_free (data);
  return GST_FLOW_OK;

/* NON-FATAL */
need_more_data:
  {
    GST_LOG_OBJECT (demux, "not enough data in adapter yet");
    return GST_FLOW_OK;
  }

/* ERRORS */
wrong_type:
  {
    GST_ELEMENT_ERROR (demux, STREAM, WRONG_TYPE, (NULL),
        ("This doesn't seem to be an ASF file"));
    g_free (data);
    return GST_FLOW_ERROR;
  }
no_streams:
parse_failed:
  {
    GST_ELEMENT_ERROR (demux, STREAM, DEMUX, (NULL),
        ("header parsing failed, or no streams found, flow = %s",
            gst_flow_get_name (flow)));
    g_free (data);
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_asf_demux_aggregate_flow_return (GstASFDemux * demux, AsfStream * stream,
    GstFlowReturn flow)
{
  int i;

  GST_DEBUG_OBJECT (demux, "Aggregating");

  /* Store the value */
  stream->last_flow = flow;

  /* any other error that is not not-linked can be returned right away */
  if (flow != GST_FLOW_NOT_LINKED)
    goto done;

  for (i = 0; i < demux->num_streams; i++) {
    if (demux->stream[i].active) {
      flow = demux->stream[i].last_flow;
      GST_DEBUG_OBJECT (demux, "Aggregating: flow %i return %s", i,
          gst_flow_get_name (flow));
      if (flow != GST_FLOW_NOT_LINKED)
        goto done;
    }
  }

  /* If we got here, then all our active streams are not linked */
done:
  return flow;
}

static gboolean
gst_asf_demux_pull_data (GstASFDemux * demux, guint64 offset, guint size,
    GstBuffer ** p_buf, GstFlowReturn * p_flow)
{
  GstFlowReturn flow;

  GST_LOG_OBJECT (demux, "pulling buffer at %" G_GUINT64_FORMAT "+%u",
      offset, size);

  flow = gst_pad_pull_range (demux->sinkpad, offset, size, p_buf);

  if (G_LIKELY (p_flow))
    *p_flow = flow;

  if (G_UNLIKELY (flow != GST_FLOW_OK)) {
    GST_DEBUG_OBJECT (demux, "flow %s pulling buffer at %" G_GUINT64_FORMAT
        "+%u", gst_flow_get_name (flow), offset, size);
    *p_buf = NULL;
    return FALSE;
  }

  g_assert (*p_buf != NULL);

  if (G_UNLIKELY (GST_BUFFER_SIZE (*p_buf) < size)) {
    GST_DEBUG_OBJECT (demux, "short read pulling buffer at %" G_GUINT64_FORMAT
        "+%u (got only %u bytes)", offset, size, GST_BUFFER_SIZE (*p_buf));
    gst_buffer_unref (*p_buf);
    if (G_LIKELY (p_flow))
      *p_flow = GST_FLOW_UNEXPECTED;
    *p_buf = NULL;
    return FALSE;
  }

  return TRUE;
}

static void
gst_asf_demux_pull_indices (GstASFDemux * demux)
{
  GstBuffer *buf = NULL;
  guint64 offset;
  guint num_read = 0;

  offset = demux->index_offset;

  if (G_UNLIKELY (offset == 0)) {
    GST_DEBUG_OBJECT (demux, "can't read indices, don't know index offset");
    return;
  }

  while (gst_asf_demux_pull_data (demux, offset, 16 + 8, &buf, NULL)) {
    GstFlowReturn flow;
    AsfObject obj;

    asf_demux_peek_object (demux, GST_BUFFER_DATA (buf), 16 + 8, &obj, TRUE);
    gst_buffer_replace (&buf, NULL);

    /* check for sanity */
    if (G_UNLIKELY (obj.size > (5 * 1024 * 1024))) {
      GST_DEBUG_OBJECT (demux, "implausible index object size, bailing out");
      break;
    }

    if (G_UNLIKELY (!gst_asf_demux_pull_data (demux, offset, obj.size, &buf,
                NULL)))
      break;

    GST_LOG_OBJECT (demux, "index object at offset 0x%" G_GINT64_MODIFIER "X"
        ", size %u", offset, (guint) obj.size);

    offset += obj.size;         /* increase before _process_object changes it */

    flow = gst_asf_demux_process_object (demux, &buf->data, &obj.size);
    gst_buffer_replace (&buf, NULL);

    if (G_UNLIKELY (flow != GST_FLOW_OK))
      break;

    ++num_read;
  }
  GST_DEBUG_OBJECT (demux, "read %u index objects", num_read);
}

static gboolean
gst_asf_demux_parse_data_object_start (GstASFDemux * demux, guint8 * data)
{
  AsfObject obj;

  asf_demux_peek_object (demux, data, 50, &obj, TRUE);
  if (obj.id != ASF_OBJ_DATA) {
    GST_WARNING_OBJECT (demux, "headers not followed by a DATA object");
    return FALSE;
  }

  demux->state = GST_ASF_DEMUX_STATE_DATA;

  if (!demux->broadcast && obj.size > 50) {
    demux->data_size = obj.size - 50;
    /* CHECKME: for at least one file this is off by +158 bytes?! */
    demux->index_offset = demux->data_offset + demux->data_size;
  } else {
    demux->data_size = 0;
    demux->index_offset = 0;
  }

  demux->packet = 0;

  if (!demux->broadcast) {
    /* skip object header (24 bytes) and file GUID (16 bytes) */
    demux->num_packets = GST_READ_UINT64_LE (data + (16 + 8) + 16);
  } else {
    demux->num_packets = 0;
  }

  if (demux->num_packets == 0)
    demux->seekable = FALSE;

  /* fallback in the unlikely case that headers are inconsistent, can't hurt */
  if (demux->data_size == 0 && demux->num_packets > 0) {
    demux->data_size = demux->num_packets * demux->packet_size;
    demux->index_offset = demux->data_offset + demux->data_size;
  }

  /* process pending stream objects and create pads for those */
  gst_asf_demux_process_queued_extended_stream_objects (demux);

  GST_INFO_OBJECT (demux, "Stream has %" G_GUINT64_FORMAT " packets, "
      "data_offset=%" G_GINT64_FORMAT ", data_size=%" G_GINT64_FORMAT
      ", index_offset=%" G_GUINT64_FORMAT, demux->num_packets,
      demux->data_offset, demux->data_size, demux->index_offset);

  return TRUE;
}

static gboolean
gst_asf_demux_pull_headers (GstASFDemux * demux)
{
  GstFlowReturn flow;
  AsfObject obj;
  GstBuffer *buf = NULL;
  guint64 size;

  GST_LOG_OBJECT (demux, "reading headers");

  /* pull HEADER object header, so we know its size */
  if (!gst_asf_demux_pull_data (demux, demux->base_offset, 16 + 8, &buf, NULL))
    goto read_failed;

  asf_demux_peek_object (demux, GST_BUFFER_DATA (buf), 16 + 8, &obj, TRUE);
  gst_buffer_replace (&buf, NULL);

  if (obj.id != ASF_OBJ_HEADER)
    goto wrong_type;

  GST_LOG_OBJECT (demux, "header size = %u", (guint) obj.size);

  /* pull HEADER object */
  if (!gst_asf_demux_pull_data (demux, demux->base_offset, obj.size, &buf,
          NULL))
    goto read_failed;

  size = obj.size;              /* don't want obj.size changed */
  flow = gst_asf_demux_process_object (demux, &buf->data, &size);
  gst_buffer_replace (&buf, NULL);

  if (flow != GST_FLOW_OK) {
    GST_WARNING_OBJECT (demux, "process_object: %s", gst_flow_get_name (flow));
    goto parse_failed;
  }

  /* calculate where the packet data starts */
  demux->data_offset = demux->base_offset + obj.size + 50;

  /* now pull beginning of DATA object before packet data */
  if (!gst_asf_demux_pull_data (demux, demux->base_offset + obj.size, 50, &buf,
          NULL))
    goto read_failed;

  if (!gst_asf_demux_parse_data_object_start (demux, GST_BUFFER_DATA (buf)))
    goto wrong_type;

  if (demux->num_streams == 0)
    goto no_streams;

  gst_buffer_replace (&buf, NULL);
  return TRUE;

/* ERRORS */
wrong_type:
  {
    gst_buffer_replace (&buf, NULL);
    GST_ELEMENT_ERROR (demux, STREAM, WRONG_TYPE, (NULL),
        ("This doesn't seem to be an ASF file"));
    return FALSE;
  }
no_streams:
read_failed:
parse_failed:
  {
    GST_ELEMENT_ERROR (demux, STREAM, DEMUX, (NULL), (NULL));
    return FALSE;
  }
}

static gboolean
all_streams_prerolled (GstASFDemux * demux)
{
  GstClockTime preroll_time;
  guint i, num_no_data = 0;

  /* Allow at least 500ms of preroll_time  */
  preroll_time = MAX (demux->preroll, 500 * GST_MSECOND);

  /* returns TRUE as long as there isn't a stream which (a) has data queued
   * and (b) the timestamp of last piece of data queued is < demux->preroll
   * AND there is at least one other stream with data queued */
  for (i = 0; i < demux->num_streams; ++i) {
    AsfPayload *last_payload;
    AsfStream *stream;
    guint last_idx;

    stream = &demux->stream[i];
    if (G_UNLIKELY (stream->payloads->len == 0)) {
      ++num_no_data;
      GST_LOG_OBJECT (stream->pad, "no data queued");
      continue;
    }

    last_idx = stream->payloads->len - 1;
    last_payload = &g_array_index (stream->payloads, AsfPayload, last_idx);

    GST_LOG_OBJECT (stream->pad, "checking if %" GST_TIME_FORMAT " > %"
        GST_TIME_FORMAT, GST_TIME_ARGS (last_payload->ts),
        GST_TIME_ARGS (preroll_time));

#ifndef ASFDEMUX_MODIFICATION
    if (G_UNLIKELY (last_payload->ts <= preroll_time)) {
      GST_LOG_OBJECT (stream->pad, "not beyond preroll point yet");
      return FALSE;
    }
#endif
  }

#ifdef ASFDEMUX_MODIFICATION
  /* Previously demuxer is Buffering until the Preroll Time Before Play Statr,
   * Now Only waiting for the at lest one payalod for all streams
   */
  if (num_no_data)
    return FALSE;
  else
    return TRUE;
#else
  if (G_UNLIKELY (num_no_data == demux->num_streams))
    return FALSE;

  return TRUE;
#endif
}

#if 0
static gboolean
gst_asf_demux_have_mutually_exclusive_active_stream (GstASFDemux * demux,
    AsfStream * stream)
{
  GSList *l;

  for (l = demux->mut_ex_streams; l != NULL; l = l->next) {
    guint8 *mes;

    /* check for each mutual exclusion group whether it affects this stream */
    for (mes = (guint8 *) l->data; mes != NULL && *mes != 0xff; ++mes) {
      if (*mes == stream->id) {
        /* we are in this group; let's check if we've already activated streams
         * that are in the same group (and hence mutually exclusive to this
         * one) */
        for (mes = (guint8 *) l->data; mes != NULL && *mes != 0xff; ++mes) {
          guint i;

          for (i = 0; i < demux->num_streams; ++i) {
            if (demux->stream[i].id == *mes && demux->stream[i].active) {
              GST_LOG_OBJECT (demux, "stream with ID %d is mutually exclusive "
                  "to already active stream with ID %d", stream->id,
                  demux->stream[i].id);
              return TRUE;
            }
          }
        }
        /* we can only be in this group once, let's break out and move on to
         * the next mutual exclusion group */
        break;
      }
    }
  }

  return FALSE;
}
#endif

static gboolean
gst_asf_demux_check_activate_streams (GstASFDemux * demux, gboolean force)
{
  guint i;

  if (demux->activated_streams)
    return TRUE;

  if (!all_streams_prerolled (demux) && !force) {
    GST_DEBUG_OBJECT (demux, "not all streams with data beyond preroll yet");
    return FALSE;
  }

  for (i = 0; i < demux->num_streams; ++i) {
    AsfStream *stream = &demux->stream[i];

    if (stream->payloads->len > 0) {
      /* we don't check mutual exclusion stuff here; either we have data for
       * a stream, then we active it, or we don't, then we'll ignore it */
      GST_LOG_OBJECT (stream->pad, "is prerolled - activate!");
      gst_asf_demux_activate_stream (demux, stream);
    } else {
      GST_LOG_OBJECT (stream->pad, "no data, ignoring stream");
#ifdef ASFDEMUX_MODIFICATION
      if(demux->audStrmId == i) {
        demux->audStrmId = -1;
        GST_LOG_OBJECT(demux, "Forced the audio stream id to -1\n");
      }
      if(demux->vidStrmId == i) {
        demux->vidStrmId = -1;
        GST_LOG_OBJECT(demux, "Forced the video stream id to -1\n");
      }
#endif
    }
  }

  gst_asf_demux_release_old_pads (demux);

  demux->activated_streams = TRUE;
  GST_LOG_OBJECT (demux, "signalling no more pads");
  gst_element_no_more_pads (GST_ELEMENT (demux));
  return TRUE;
}

#ifndef ASFDEMUX_MODIFICATION
/* returns the stream that has a complete payload with the lowest timestamp
 * queued, or NULL (we push things by timestamp because during the internal
 * prerolling we might accumulate more data then the external queues can take,
 * so we'd lock up if we pushed all accumulated data for stream N in one go) */
static AsfStream *
gst_asf_demux_find_stream_with_complete_payload (GstASFDemux * demux)
{
  AsfPayload *best_payload = NULL;
  AsfStream *best_stream = NULL;
  guint i;

  for (i = 0; i < demux->num_streams; ++i) {
    AsfStream *stream;

    stream = &demux->stream[i];

    /* Don't push any data until we have at least one payload that falls within
     * the current segment. This way we can remove out-of-segment payloads that
     * don't need to be decoded after a seek, sending only data from the
     * keyframe directly before our segment start */
    if (stream->payloads->len > 0) {
      AsfPayload *payload;
      guint last_idx;

      last_idx = stream->payloads->len - 1;
      payload = &g_array_index (stream->payloads, AsfPayload, last_idx);
      if (G_UNLIKELY (GST_CLOCK_TIME_IS_VALID (payload->ts) &&
              (payload->ts < demux->segment.start))) {
        if (G_UNLIKELY ((!demux->accurate) && payload->keyframe)) {
          GST_DEBUG_OBJECT (stream->pad,
              "Found keyframe, updating segment start to %" GST_TIME_FORMAT,
              GST_TIME_ARGS (payload->ts));
          demux->segment.start = payload->ts;
          demux->segment.time = payload->ts;
        } else {
          GST_DEBUG_OBJECT (stream->pad, "Last queued payload has timestamp %"
              GST_TIME_FORMAT " which is before our segment start %"
              GST_TIME_FORMAT ", not pushing yet", GST_TIME_ARGS (payload->ts),
              GST_TIME_ARGS (demux->segment.start));
          continue;
        }
      }

      /* Now see if there's a complete payload queued for this stream */

      payload = &g_array_index (stream->payloads, AsfPayload, 0);
      if (!gst_asf_payload_is_complete (payload))
        continue;

      /* ... and whether its timestamp is lower than the current best */
      if (best_stream == NULL || best_payload->ts > payload->ts) {
        best_stream = stream;
        best_payload = payload;
      }
    }
  }

  return best_stream;
}
#else
/* Writes TRUE If any complete Complete payloads in the Stream */
static gboolean
gst_asf_demux_check_payload (GstASFDemux * demux,AsfStream *stream,gint64 *payTS)
{
  AsfPayload *payload;

  /* Don't push any data until we have at least one payload that falls within
  * the current segment. This way we can remove out-of-segment payloads that
  * don't need to be decoded after a seek, sending only data from the
  * keyframe directly before our segment start */

  if (stream->payloads->len > 0) {
    payload = &g_array_index (stream->payloads, AsfPayload, stream->payloads->len - 1);
	
    if (GST_CLOCK_TIME_IS_VALID (payload->ts) &&
        payload->ts < demux->segment.start) {
      GST_DEBUG_OBJECT (stream->pad, "Last queued payload has timestamp %"
        GST_TIME_FORMAT " which is before our segment start %"
        GST_TIME_FORMAT ", not pushing yet", GST_TIME_ARGS (payload->ts),
        GST_TIME_ARGS (demux->segment.start));
      return FALSE;
    }
	
    /* Now see if there's a complete payload queued for this stream */
    payload = &g_array_index (stream->payloads, AsfPayload, 0);
    if (!gst_asf_payload_is_complete (payload)) {
      return FALSE;
    }
	
    *payTS = payload->ts;
    GST_DEBUG_OBJECT (stream->pad, "Last queued payload has timestamp %"
      GST_TIME_FORMAT"", GST_TIME_ARGS (payload->ts));
    return TRUE;
  }
  return FALSE;
}

static AsfStream *
gst_asf_demux_find_stream_with_complete_payload (GstASFDemux * demux)
{
  AsfStream *streama = NULL,*streamv=NULL;
  guint strmCnt=0;
  gint64 audTS=-1,vidTS=-1;

  /* If the Audio Stream Id  is Valied Check for the Complete Paylods in the Audio stream*/
  
  if(demux->audStrmId >= 0) {
    streama = &demux->stream[demux->audStrmId];
    if(gst_asf_demux_check_payload(demux,streama,&audTS)) {
      GST_DEBUG_OBJECT (demux, " Pending Audio stream Payload with Time Stamp : %"GST_TIME_FORMAT , GST_TIME_ARGS(audTS));
      strmCnt++;
    } else {
      GST_DEBUG_OBJECT (demux, " No Pending Payloads in the Audio stream ");
    }
  }

  /* If the Video Stream Id  is Valied Check for the Complete Paylods in the Video stream*/
  if(demux->vidStrmId >= 0) {
    streamv = &demux->stream[demux->vidStrmId];
    if(gst_asf_demux_check_payload(demux,streamv,&vidTS)) {
      GST_DEBUG_OBJECT (demux, " Pending Video stream Payload with Time Stamp : %"GST_TIME_FORMAT, GST_TIME_ARGS(vidTS));
      strmCnt++;
    } else {
      GST_DEBUG_OBJECT (demux, " No Pending Payloads in the Video stream ");
    }
  }

  if(strmCnt == 1) {
    GST_DEBUG_OBJECT (demux, "Only One Stream has pending payload  Audio TS : %"GST_TIME_FORMAT" /video TS : %"GST_TIME_FORMAT,
      GST_TIME_ARGS(audTS),GST_TIME_ARGS(vidTS));

    /* Check the validity of the audio stream & its Time Stamp*/
    if(audTS >= 0 && streama != NULL && demux->vidStrmId == -1) {
        return streama;
    }
	
    /* Check the validity of the video stream & its Time Stamp*/
    if(vidTS >= 0 && streamv != NULL && demux->audStrmId  == -1) {
      return streamv;
    }
  } else if(strmCnt > 1) {
    GST_DEBUG_OBJECT (demux, "Multiple Streams has pending payload	Audio TS : %"GST_TIME_FORMAT" /video TS : %"GST_TIME_FORMAT,
    GST_TIME_ARGS(audTS),GST_TIME_ARGS(vidTS));

    if(!demux->firstVidBufSent) {
      GST_INFO_OBJECT (demux, " Sending Initial Video Buffers with TS : %"GST_TIME_FORMAT,GST_TIME_ARGS(vidTS));
      demux->firstVidBufSent = TRUE;
      return streamv;
    }

    if(!demux->firstAudBufSent) {
      GST_INFO_OBJECT (demux, " Sending Initial Audio Buffers with TS : %"GST_TIME_FORMAT,GST_TIME_ARGS(audTS));
      demux->firstAudBufSent = TRUE;
      return streama;
    }
	
    /* In case of video + audio, Audio always lead video by 10 ms*/
    if(audTS <= vidTS + 10000000){
      return streama;
    } else {
      return streamv;
    }
  }

  GST_DEBUG_OBJECT (demux, " Not Sending any Payload to decoders");
  
  return NULL;
}
#endif


static GstFlowReturn
gst_asf_demux_push_complete_payloads (GstASFDemux * demux, gboolean force)
{
  AsfStream *stream;
  GstFlowReturn ret = GST_FLOW_OK;

  if (G_UNLIKELY (!demux->activated_streams)) {
    if (!gst_asf_demux_check_activate_streams (demux, force))
      return GST_FLOW_OK;
    /* streams are now activated */
  }

  /* wait until we had a chance to "lock on" some payload's timestamp */
  if (G_UNLIKELY (demux->need_newsegment
          && !GST_CLOCK_TIME_IS_VALID (demux->segment_ts)))
    return GST_FLOW_OK;

  while ((stream = gst_asf_demux_find_stream_with_complete_payload (demux))) {
    AsfPayload *payload;

    payload = &g_array_index (stream->payloads, AsfPayload, 0);

#ifdef ASFDEMUX_MODIFICATION
    if(stream->is_video) {
      demux->current_ts = payload->ts;
    }
    if ((demux->segment.rate > 1.0) && (stream->is_video) && (payload->ts > demux->segment.start)) {
      if (!gst_asf_demux_forward_trickplay (demux, stream, payload)) {
        gst_buffer_replace (&payload->buf, NULL);
        if (stream->payloads->len > 0) {
          g_array_remove_index (stream->payloads, 0);
        }
        GST_WARNING_OBJECT (demux, "returning from foward trick play with %s", gst_flow_get_name (stream->last_flow));
        break;
      }
    } else if(demux->segment.rate<0.0 && stream->is_video) {
      if (!gst_asf_demux_backward_trickplay(demux, stream, payload)) {
        gst_buffer_replace (&payload->buf, NULL);

        if (stream->payloads->len > 0) {
          g_array_remove_index (stream->payloads, 0);
        }
        GST_WARNING_OBJECT (demux, "returning from backward trick play with %s", gst_flow_get_name (stream->last_flow));

        if (stream->last_flow == GST_FLOW_ERROR)
          break;
        else
          continue;
      }
    }
#endif

    /* do we need to send a newsegment event */
    if ((G_UNLIKELY (demux->need_newsegment))) {

      /* safe default if insufficient upstream info */
      if (!GST_CLOCK_TIME_IS_VALID (demux->in_gap))
        demux->in_gap = 0;

      if (demux->segment.stop == GST_CLOCK_TIME_NONE &&
          demux->segment.duration > 0) {
        /* slight HACK; prevent clipping of last bit */
        demux->segment.stop = demux->segment.duration + demux->in_gap;
      }

      /* FIXME : only if ACCURATE ! */
      if (G_LIKELY (!demux->accurate
              && (GST_CLOCK_TIME_IS_VALID (payload->ts)))) {
        GST_DEBUG ("Adjusting newsegment start to %" GST_TIME_FORMAT,
            GST_TIME_ARGS (payload->ts));
        demux->segment.start = payload->ts;
        demux->segment.time = payload->ts;
      }

      GST_DEBUG_OBJECT (demux, "sending new-segment event %" GST_SEGMENT_FORMAT,
          &demux->segment);
#ifdef ASFDEMUX_MODIFICATION
      if (demux->segment.rate > 0.0) {
        /* note: we fix up all timestamps to start from 0, so this should be ok */
	    gst_asf_demux_send_event_unlocked (demux,
		          gst_event_new_new_segment (FALSE, demux->segment.rate,
				  GST_FORMAT_TIME, demux->segment.start, demux->segment.stop,
				  demux->segment.start));
      } else if (demux->segment.rate < 0.0) {
        demux->segment.stop = demux->segment.last_stop;
        demux->segment.start = 0.0;
        demux->segment.time = 0.0;
		
        if(demux->segment.stop < payload->ts) {
          GST_WARNING ("Adjusting newsegment stop to %" GST_TIME_FORMAT, GST_TIME_ARGS (payload->ts));
          demux->segment.stop=payload->ts+GST_SECOND;
        }
        /* note: we fix up all timestamps to start from 0, so this should be ok */
	    gst_asf_demux_send_event_unlocked (demux,
		          gst_event_new_new_segment (FALSE, demux->segment.rate,
				  GST_FORMAT_TIME, demux->segment.start, demux->segment.stop,
				  demux->segment.start));
      }
#endif
      /* now post any global tags we may have found */
      if (demux->taglist == NULL)
        demux->taglist = gst_tag_list_new ();

      gst_tag_list_add (demux->taglist, GST_TAG_MERGE_REPLACE,
          GST_TAG_CONTAINER_FORMAT, "ASF", NULL);

      GST_DEBUG_OBJECT (demux, "global tags: %" GST_PTR_FORMAT, demux->taglist);
      gst_element_found_tags (GST_ELEMENT (demux), demux->taglist);
      demux->taglist = NULL;

      demux->need_newsegment = FALSE;
      demux->segment_running = TRUE;
    }

    /* Do we have tags pending for this stream? */
    if (G_UNLIKELY (stream->pending_tags)) {
      GST_LOG_OBJECT (stream->pad, "%" GST_PTR_FORMAT, stream->pending_tags);
      gst_element_found_tags_for_pad (GST_ELEMENT (demux), stream->pad,
          stream->pending_tags);
      stream->pending_tags = NULL;
    }

    /* We have the whole packet now so we should push the packet to
     * the src pad now. First though we should check if we need to do
     * descrambling */
    if (G_UNLIKELY (demux->span > 1)) {
      gst_asf_demux_descramble_buffer (demux, stream, &payload->buf);
    }

    if ((payload->buf)->mini_object.refcount == 0) {
        GST_ERROR_OBJECT (stream->pad, "before metadata buffer : payload =  %p, writable = %d & cnt = %d",
                                       payload, gst_buffer_is_metadata_writable(payload->buf), (payload->buf)->mini_object.refcount);
        GST_ERROR_OBJECT (demux, "PUSH : media object ts     : %" GST_TIME_FORMAT, GST_TIME_ARGS (payload->ts));
    }
    payload->buf = gst_buffer_make_metadata_writable (payload->buf);

    if (G_LIKELY (!payload->keyframe)) {
      GST_BUFFER_FLAG_SET (payload->buf, GST_BUFFER_FLAG_DELTA_UNIT);
    }

    if (G_UNLIKELY (stream->discont)) {
      GST_DEBUG_OBJECT (stream->pad, "marking DISCONT on stream");
      GST_BUFFER_FLAG_SET (payload->buf, GST_BUFFER_FLAG_DISCONT);
      stream->discont = FALSE;
    }

    if (G_UNLIKELY (stream->is_video && payload->par_x && payload->par_y &&
            (payload->par_x != stream->par_x) &&
            (payload->par_y != stream->par_y))) {
      GST_DEBUG ("Updating PAR (%d/%d => %d/%d)",
          stream->par_x, stream->par_y, payload->par_x, payload->par_y);
      stream->par_x = payload->par_x;
      stream->par_y = payload->par_y;
      gst_caps_set_simple (stream->caps, "pixel-aspect-ratio",
          GST_TYPE_FRACTION, stream->par_x, stream->par_y, NULL);
      gst_pad_set_caps (stream->pad, stream->caps);
    }

    if (G_UNLIKELY (stream->interlaced != payload->interlaced)) {
      GST_DEBUG ("Updating interlaced status (%d => %d)", stream->interlaced,
          payload->interlaced);
      stream->interlaced = payload->interlaced;
      gst_caps_set_simple (stream->caps, "interlaced", G_TYPE_BOOLEAN,
          stream->interlaced, NULL);
    }

    gst_buffer_set_caps (payload->buf, stream->caps);

    /* (sort of) interpolate timestamps using upstream "frame of reference",
     * typically useful for live src, but might (unavoidably) mess with
     * position reporting if a live src is playing not so live content
     * (e.g. rtspsrc taking some time to fall back to tcp) */
    GST_BUFFER_TIMESTAMP (payload->buf) = payload->ts + demux->in_gap;
    if (payload->duration == GST_CLOCK_TIME_NONE
        && stream->ext_props.avg_time_per_frame != 0)
      GST_BUFFER_DURATION (payload->buf) =
          stream->ext_props.avg_time_per_frame * 100;
    else
      GST_BUFFER_DURATION (payload->buf) = payload->duration;

    /* FIXME: we should really set durations on buffers if we can */

    GST_INFO_OBJECT (stream->pad, "pushing buffer [%p], ts=%" GST_TIME_FORMAT
        ", dur=%" GST_TIME_FORMAT " size=%u", payload,
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (payload->buf)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (payload->buf)),
        GST_BUFFER_SIZE (payload->buf));

#ifdef ASFDEMUX_MODIFICATION
    /* Set last_stop with the keyframe timestamp we pushed of that stream */  
    gst_segment_set_last_stop (&demux->segment, GST_FORMAT_TIME, GST_BUFFER_TIMESTAMP (payload->buf));
#endif
    ret = gst_pad_push (stream->pad, payload->buf);
    ret = gst_asf_demux_aggregate_flow_return (demux, stream, ret);
    payload->buf = NULL;
    g_array_remove_index (stream->payloads, 0);

    /* Break out as soon as we have an issue */
    if (G_UNLIKELY (ret != GST_FLOW_OK))
      break;
  }

  return ret;
}

static gboolean
gst_asf_demux_check_buffer_is_header (GstASFDemux * demux, GstBuffer * buf)
{
  AsfObject obj;
  g_assert (buf != NULL);

  GST_LOG_OBJECT (demux, "Checking if buffer is a header");

  /* we return false on buffer too small */
  if (GST_BUFFER_SIZE (buf) < ASF_OBJECT_HEADER_SIZE)
    return FALSE;

  /* check if it is a header */
  asf_demux_peek_object (demux, GST_BUFFER_DATA (buf),
      ASF_OBJECT_HEADER_SIZE, &obj, TRUE);
  if (obj.id == ASF_OBJ_HEADER) {
    return TRUE;
  }
  return FALSE;
}

static gboolean
gst_asf_demux_check_chained_asf (GstASFDemux * demux)
{
  guint64 off = demux->data_offset + (demux->packet * demux->packet_size);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buf = NULL;
  gboolean header = FALSE;

  /* TODO maybe we should skip index objects after the data and look
   * further for a new header */
  if (gst_asf_demux_pull_data (demux, off, ASF_OBJECT_HEADER_SIZE, &buf, &ret)) {
    g_assert (buf != NULL);
    /* check if it is a header */
    if (gst_asf_demux_check_buffer_is_header (demux, buf)) {
      GST_DEBUG_OBJECT (demux, "new base offset: %" G_GUINT64_FORMAT, off);
      demux->base_offset = off;
      header = TRUE;
    }

    gst_buffer_unref (buf);
  }

  return header;
}

static void
gst_asf_demux_loop (GstASFDemux * demux)
{
  GstFlowReturn flow = GST_FLOW_OK;
  GstBuffer *buf = NULL;
  guint64 off;
  gboolean sent_eos = FALSE;

  if (G_UNLIKELY (demux->state == GST_ASF_DEMUX_STATE_HEADER)) {
#ifdef ASFDEMUX_MODIFICATION
    gint i;
    gboolean bret = FALSE;
#endif
    if (!gst_asf_demux_pull_headers (demux)) {
      flow = GST_FLOW_ERROR;
      goto pause;
    }

    gst_asf_demux_pull_indices (demux);

#ifdef ASFDEMUX_MODIFICATION
    for(i=0;i<demux->num_streams;++i) {
      if(demux->stream[i].is_video) {
        guint idx_entries = 0;
        guint entries_offset = 2;

        GST_DEBUG ("sidx interval = %"GST_TIME_FORMAT, GST_TIME_ARGS (demux->sidx_interval));
        if (demux->sidx_interval)
          idx_entries = gst_util_uint64_scale (demux->play_time, 1, demux->sidx_interval);
        else
          idx_entries = gst_util_uint64_scale (demux->play_time, 1, GST_SECOND);

        if ((idx_entries - entries_offset) > demux->sidx_num_entries) {
          GST_INFO_OBJECT (demux, "Index table is not complete.. need to rebuild");
          demux->sidx_num_entries = 0;
        }

        if (G_UNLIKELY (demux->sidx_num_entries == 0 || demux->sidx_interval == 0)) {
          GstClockTime ts = gst_util_get_timestamp ();

          /* File does not index table for seeking and trickplay, build it */
          GST_INFO_OBJECT (demux, "Video Stream does not have index table..Creating it");

          bret = gst_asf_demux_create_simple_index_table (demux);
          if (FALSE == bret) {
            GST_ERROR_OBJECT (demux, "Failed to create index table...");
            goto parse_error;
          }
          ts = gst_util_get_timestamp () - ts;
          GST_INFO_OBJECT (demux, "Time taken to prepare index table is %" GST_TIME_FORMAT, GST_TIME_ARGS (ts));
        }
        demux->vidStrmId = i;
        GST_INFO_OBJECT (demux, "ASF VIDEO STREAM ID %d", demux->vidStrmId);
      } else {
        demux->audStrmId = i;
        GST_INFO_OBJECT (demux, "ASF AUDIO STREAM ID %d", demux->audStrmId);
      }
    }
#endif
  }

  g_assert (demux->state == GST_ASF_DEMUX_STATE_DATA);

  if (G_UNLIKELY (demux->num_packets != 0
          && demux->packet >= demux->num_packets))
    goto eos;

  GST_LOG_OBJECT (demux, "packet %u/%u", (guint) demux->packet + 1,
      (guint) demux->num_packets);

  off = demux->data_offset + (demux->packet * demux->packet_size);

  if (G_UNLIKELY (!gst_asf_demux_pull_data (demux, off,
              demux->packet_size * demux->speed_packets, &buf, &flow))) {
    GST_DEBUG_OBJECT (demux, "got flow %s", gst_flow_get_name (flow));
    if (flow == GST_FLOW_UNEXPECTED)
      goto eos;
    else if (flow == GST_FLOW_WRONG_STATE) {
      GST_DEBUG_OBJECT (demux, "Not fatal");
      goto pause;
    } else
      goto read_failed;
  }

  if (G_LIKELY (demux->speed_packets == 1)) {
    /* FIXME: maybe we should just skip broken packets and error out only
     * after a few broken packets in a row? */
    GST_OBJECT_LOCK (demux);
    if (G_UNLIKELY (!gst_asf_demux_parse_packet (demux, buf))) {
      /* when we don't know when the data object ends, we should check
       * for a chained asf */
      GST_OBJECT_UNLOCK (demux);
      if (demux->num_packets == 0) {
        if (gst_asf_demux_check_buffer_is_header (demux, buf)) {
          GST_INFO_OBJECT (demux, "Chained asf found");
          demux->base_offset = off;
          gst_asf_demux_reset (demux, TRUE);
          gst_buffer_unref (buf);
          return;
        }
      }
      goto parse_error;
    }
    GST_OBJECT_UNLOCK (demux);
    flow = gst_asf_demux_push_complete_payloads (demux, FALSE);

    ++demux->packet;

  } else {
    guint n;
    for (n = 0; n < demux->speed_packets; n++) {
      GstBuffer *sub;
    GST_OBJECT_LOCK (demux);

      sub =
          gst_buffer_create_sub (buf, n * demux->packet_size,
          demux->packet_size);
      /* FIXME: maybe we should just skip broken packets and error out only
       * after a few broken packets in a row? */
      if (G_UNLIKELY (!gst_asf_demux_parse_packet (demux, sub))) {
      GST_OBJECT_UNLOCK (demux);

        /* when we don't know when the data object ends, we should check
         * for a chained asf */
        if (demux->num_packets == 0) {
          if (gst_asf_demux_check_buffer_is_header (demux, sub)) {
            GST_INFO_OBJECT (demux, "Chained asf found");
            demux->base_offset = off + n * demux->packet_size;
            gst_asf_demux_reset (demux, TRUE);
            gst_buffer_unref (sub);
            gst_buffer_unref (buf);
            return;
          }
        }
        goto parse_error;
      }
      GST_OBJECT_UNLOCK (demux);

      gst_buffer_unref (sub);

      flow = gst_asf_demux_push_complete_payloads (demux, FALSE);

      ++demux->packet;

    }

    /* reset speed pull */
    demux->speed_packets = 1;
  }

  gst_buffer_unref (buf);

#ifdef LAST_STOP_SEGMENT
  GST_LOG_OBJECT(demux, "checking eos... stop packet  = %d, cur packet = %d",
    (guint)demux->packet_to_stop, (guint)demux->packet);

  if (G_UNLIKELY ( demux->packet_to_stop > 0
    && demux->num_packets > 0
	&& demux->packet_to_stop <= demux->packet)) {
	demux->packet_to_stop = 0;
	GST_INFO_OBJECT (demux, "reached EOS");
	demux->packet_to_stop = 0;
	goto eos;
  }

#ifdef ASFDEMUX_MODIFICATION
  if(G_LIKELY (demux->trickplay_info.resume_nrl_play)) {
    GST_LOG_OBJECT (demux, "reached EOS by backward Trickplay");
	demux->trickplay_info.resume_nrl_play = FALSE;
	goto eos;
  }
#endif  
#endif

#ifdef ASFDEMUX_MODIFICATION
  if (G_UNLIKELY (demux->num_packets > 0
          && demux->packet >= demux->num_packets) || demux->current_ts >= demux->stop_ts) {
#else
  if (G_UNLIKELY (demux->num_packets > 0
          && demux->packet >= demux->num_packets)) {
#endif
    GST_LOG_OBJECT (demux, "reached EOS");
    goto eos;
  }

  if (G_UNLIKELY (flow != GST_FLOW_OK)) {
    GST_DEBUG_OBJECT (demux, "pushing complete payloads failed");
    goto pause;
  }

  /* check if we're at the end of the configured segment */
  /* FIXME: check if segment end reached etc. */

  return;

eos:
  {
    /* if we haven't activated our streams yet, this might be because we have
     * less data queued than required for preroll; force stream activation and
     * send any pending payloads before sending EOS */
    if (!demux->activated_streams)
      gst_asf_demux_push_complete_payloads (demux, TRUE);

    /* we want to push an eos or post a segment-done in any case */
    if (demux->segment.flags & GST_SEEK_FLAG_SEGMENT) {
      gint64 stop;

      /* for segment playback we need to post when (in stream time)
       * we stopped, this is either stop (when set) or the duration. */
      if ((stop = demux->segment.stop) == -1)
        stop = demux->segment.duration;

      GST_INFO_OBJECT (demux, "Posting segment-done, at end of segment");
      gst_element_post_message (GST_ELEMENT_CAST (demux),
          gst_message_new_segment_done (GST_OBJECT (demux), GST_FORMAT_TIME,
              stop));
    } else if (flow != GST_FLOW_UNEXPECTED) {
      /* check if we have a chained asf, in case, we don't eos yet */
      if (gst_asf_demux_check_chained_asf (demux)) {
        GST_INFO_OBJECT (demux, "Chained ASF starting");
        gst_asf_demux_reset (demux, TRUE);
        return;
      }
    }
    /* normal playback, send EOS to all linked pads */
    GST_INFO_OBJECT (demux, "Sending EOS, at end of stream");
    gst_asf_demux_send_event_unlocked (demux, gst_event_new_eos ());
    sent_eos = TRUE;
    /* ... and fall through to pause */
  }
pause:
  {
    GST_DEBUG_OBJECT (demux, "pausing task, flow return: %s",
        gst_flow_get_name (flow));
    demux->segment_running = FALSE;
    gst_pad_pause_task (demux->sinkpad);

    /* For the error cases (not EOS) */
    if (!sent_eos) {
      if (flow == GST_FLOW_UNEXPECTED)
        gst_asf_demux_send_event_unlocked (demux, gst_event_new_eos ());
      else if (flow < GST_FLOW_UNEXPECTED || flow == GST_FLOW_NOT_LINKED) {
        /* Post an error. Hopefully something else already has, but if not... */
        GST_ELEMENT_ERROR (demux, STREAM, FAILED,
            (_("Internal data stream error.")),
            ("streaming stopped, reason %s", gst_flow_get_name (flow)));
      }
    }
    return;
  }

/* ERRORS */
read_failed:
  {
    GST_DEBUG_OBJECT (demux, "Read failed, doh");
    gst_asf_demux_send_event_unlocked (demux, gst_event_new_eos ());
    flow = GST_FLOW_UNEXPECTED;
    goto pause;
  }
parse_error:
  {
    gst_buffer_unref (buf);
    GST_ELEMENT_ERROR (demux, STREAM, DEMUX, (NULL),
        ("Error parsing ASF packet %u", (guint) demux->packet));
    gst_asf_demux_send_event_unlocked (demux, gst_event_new_eos ());
    flow = GST_FLOW_ERROR;
    goto pause;
  }
}

#define GST_ASF_DEMUX_CHECK_HEADER_YES       0
#define GST_ASF_DEMUX_CHECK_HEADER_NO        1
#define GST_ASF_DEMUX_CHECK_HEADER_NEED_DATA 2

static gint
gst_asf_demux_check_header (GstASFDemux * demux)
{
  AsfObject obj;
  guint8 *cdata = (guint8 *) gst_adapter_peek (demux->adapter,
      ASF_OBJECT_HEADER_SIZE);
  if (cdata == NULL)            /* need more data */
    return GST_ASF_DEMUX_CHECK_HEADER_NEED_DATA;

  asf_demux_peek_object (demux, cdata, ASF_OBJECT_HEADER_SIZE, &obj, FALSE);
  if (obj.id != ASF_OBJ_HEADER) {
    return GST_ASF_DEMUX_CHECK_HEADER_NO;
  } else {
    return GST_ASF_DEMUX_CHECK_HEADER_YES;
  }
}

static GstFlowReturn
gst_asf_demux_chain (GstPad * pad, GstBuffer * buf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstASFDemux *demux;

  demux = GST_ASF_DEMUX (GST_PAD_PARENT (pad));

  GST_LOG_OBJECT (demux, "buffer: size=%u, offset=%" G_GINT64_FORMAT ", time=%"
      GST_TIME_FORMAT, GST_BUFFER_SIZE (buf), GST_BUFFER_OFFSET (buf),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)));

  if (G_UNLIKELY (GST_BUFFER_IS_DISCONT (buf))) {
    GST_DEBUG_OBJECT (demux, "received DISCONT");
    gst_asf_demux_mark_discont (demux);
  }

  if (G_UNLIKELY ((!GST_CLOCK_TIME_IS_VALID (demux->in_gap) &&
              GST_BUFFER_TIMESTAMP_IS_VALID (buf)))) {
    demux->in_gap = GST_BUFFER_TIMESTAMP (buf) - demux->in_segment.start;
    GST_DEBUG_OBJECT (demux, "upstream segment start %" GST_TIME_FORMAT
        ", interpolation gap: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (demux->in_segment.start), GST_TIME_ARGS (demux->in_gap));
  }

  gst_adapter_push (demux->adapter, buf);

  switch (demux->state) {
    case GST_ASF_DEMUX_STATE_INDEX:{
      gint result = gst_asf_demux_check_header (demux);
      if (result == GST_ASF_DEMUX_CHECK_HEADER_NEED_DATA)       /* need more data */
        break;

      if (result == GST_ASF_DEMUX_CHECK_HEADER_NO) {
        /* we don't care about this, probably an index */
        /* TODO maybe would be smarter to skip all the indices
         * until we got a new header or EOS to decide */
        GST_LOG_OBJECT (demux, "Received index object, its EOS");
        goto eos;
      } else {
        GST_INFO_OBJECT (demux, "Chained asf starting");
        /* cleanup and get ready for a chained asf */
        gst_asf_demux_reset (demux, TRUE);
        /* fall through */
      }
    }
    case GST_ASF_DEMUX_STATE_HEADER:{
      ret = gst_asf_demux_chain_headers (demux);
      if (demux->state != GST_ASF_DEMUX_STATE_DATA)
        break;
      /* otherwise fall through */
    }
    case GST_ASF_DEMUX_STATE_DATA:
    {
      guint64 data_size;

      data_size = demux->packet_size;

      while (gst_adapter_available (demux->adapter) >= data_size) {
        GstBuffer *buf;

        /* we don't know the length of the stream
         * check for a chained asf everytime */
        if (demux->num_packets == 0) {
          gint result = gst_asf_demux_check_header (demux);

          if (result == GST_ASF_DEMUX_CHECK_HEADER_YES) {
            GST_INFO_OBJECT (demux, "Chained asf starting");
            /* cleanup and get ready for a chained asf */
            gst_asf_demux_reset (demux, TRUE);
            break;
          }
        } else if (G_UNLIKELY (demux->num_packets != 0 && demux->packet >= 0
                && demux->packet >= demux->num_packets)) {
          /* do not overshoot data section when streaming */
          break;
        }

        buf = gst_adapter_take_buffer (demux->adapter, data_size);

        /* FIXME: maybe we should just skip broken packets and error out only
         * after a few broken packets in a row? */
        if (G_UNLIKELY (!gst_asf_demux_parse_packet (demux, buf))) {
          GST_WARNING_OBJECT (demux, "Parse error");
        }

        gst_buffer_unref (buf);

        ret = gst_asf_demux_push_complete_payloads (demux, FALSE);

        if (demux->packet >= 0)
          ++demux->packet;
      }
      if (G_UNLIKELY (demux->num_packets != 0 && demux->packet >= 0
              && demux->packet >= demux->num_packets)) {
        demux->state = GST_ASF_DEMUX_STATE_INDEX;
      }
      break;
    }
    default:
      g_assert_not_reached ();
  }

done:
  if (ret != GST_FLOW_OK)
    GST_DEBUG_OBJECT (demux, "flow: %s", gst_flow_get_name (ret));

  return ret;

eos:
  {
    GST_DEBUG_OBJECT (demux, "Handled last packet, setting EOS");
    ret = GST_FLOW_UNEXPECTED;
    goto done;
  }
}

static inline gboolean
gst_asf_demux_skip_bytes (guint num_bytes, guint8 ** p_data, guint64 * p_size)
{
  if (*p_size < num_bytes)
    return FALSE;

  *p_data += num_bytes;
  *p_size -= num_bytes;
  return TRUE;
}

static inline guint8
gst_asf_demux_get_uint8 (guint8 ** p_data, guint64 * p_size)
{
  guint8 ret;

  g_assert (*p_size >= 1);
  ret = GST_READ_UINT8 (*p_data);
  *p_data += sizeof (guint8);
  *p_size -= sizeof (guint8);
  return ret;
}

static inline guint16
gst_asf_demux_get_uint16 (guint8 ** p_data, guint64 * p_size)
{
  guint16 ret;

  g_assert (*p_size >= 2);
  ret = GST_READ_UINT16_LE (*p_data);
  *p_data += sizeof (guint16);
  *p_size -= sizeof (guint16);
  return ret;
}

static inline guint32
gst_asf_demux_get_uint32 (guint8 ** p_data, guint64 * p_size)
{
  guint32 ret;

  g_assert (*p_size >= 4);
  ret = GST_READ_UINT32_LE (*p_data);
  *p_data += sizeof (guint32);
  *p_size -= sizeof (guint32);
  return ret;
}

static inline guint64
gst_asf_demux_get_uint64 (guint8 ** p_data, guint64 * p_size)
{
  guint64 ret;

  g_assert (*p_size >= 8);
  ret = GST_READ_UINT64_LE (*p_data);
  *p_data += sizeof (guint64);
  *p_size -= sizeof (guint64);
  return ret;
}

static inline guint32
gst_asf_demux_get_var_length (guint8 type, guint8 ** p_data, guint64 * p_size)
{
  switch (type) {
    case 0:
      return 0;

    case 1:
      g_assert (*p_size >= 1);
      return gst_asf_demux_get_uint8 (p_data, p_size);

    case 2:
      g_assert (*p_size >= 2);
      return gst_asf_demux_get_uint16 (p_data, p_size);

    case 3:
      g_assert (*p_size >= 4);
      return gst_asf_demux_get_uint32 (p_data, p_size);

    default:
      g_assert_not_reached ();
      break;
  }
  return 0;
}

static gboolean
gst_asf_demux_get_buffer (GstBuffer ** p_buf, guint num_bytes_to_read,
    guint8 ** p_data, guint64 * p_size)
{
  *p_buf = NULL;

  if (*p_size < num_bytes_to_read)
    return FALSE;

  *p_buf = gst_buffer_new_and_alloc (num_bytes_to_read);
  memcpy (GST_BUFFER_DATA (*p_buf), *p_data, num_bytes_to_read);
  *p_data += num_bytes_to_read;
  *p_size -= num_bytes_to_read;
  return TRUE;
}

static gboolean
gst_asf_demux_get_bytes (guint8 ** p_buf, guint num_bytes_to_read,
    guint8 ** p_data, guint64 * p_size)
{
  *p_buf = NULL;

  if (*p_size < num_bytes_to_read)
    return FALSE;

  *p_buf = g_memdup (*p_data, num_bytes_to_read);
  *p_data += num_bytes_to_read;
  *p_size -= num_bytes_to_read;
  return TRUE;
}

static gboolean
gst_asf_demux_get_string (gchar ** p_str, guint16 * p_strlen,
    guint8 ** p_data, guint64 * p_size)
{
  guint16 s_length;
  guint8 *s;

  *p_str = NULL;

  if (*p_size < 2)
    return FALSE;

  s_length = gst_asf_demux_get_uint16 (p_data, p_size);

  if (p_strlen)
    *p_strlen = s_length;

  if (s_length == 0) {
    GST_WARNING ("zero-length string");
    *p_str = g_strdup ("");
    return TRUE;
  }

  if (!gst_asf_demux_get_bytes (&s, s_length, p_data, p_size))
    return FALSE;

  g_assert (s != NULL);

  /* just because They don't exist doesn't
   * mean They are not out to get you ... */
  if (s[s_length - 1] != '\0') {
    s = g_realloc (s, s_length + 1);
    s[s_length] = '\0';
  }

  *p_str = (gchar *) s;
  return TRUE;
}


static void
gst_asf_demux_get_guid (ASFGuid * guid, guint8 ** p_data, guint64 * p_size)
{
  g_assert (*p_size >= 4 * sizeof (guint32));

  guid->v1 = gst_asf_demux_get_uint32 (p_data, p_size);
  guid->v2 = gst_asf_demux_get_uint32 (p_data, p_size);
  guid->v3 = gst_asf_demux_get_uint32 (p_data, p_size);
  guid->v4 = gst_asf_demux_get_uint32 (p_data, p_size);
}

static gboolean
gst_asf_demux_get_stream_audio (asf_stream_audio * audio, guint8 ** p_data,
    guint64 * p_size)
{
  if (*p_size < (2 + 2 + 4 + 4 + 2 + 2 + 2))
    return FALSE;

  /* WAVEFORMATEX Structure */
  audio->codec_tag = gst_asf_demux_get_uint16 (p_data, p_size);
  audio->channels = gst_asf_demux_get_uint16 (p_data, p_size);
  audio->sample_rate = gst_asf_demux_get_uint32 (p_data, p_size);
  audio->byte_rate = gst_asf_demux_get_uint32 (p_data, p_size);
  audio->block_align = gst_asf_demux_get_uint16 (p_data, p_size);
  audio->word_size = gst_asf_demux_get_uint16 (p_data, p_size);
  /* Codec specific data size */
  audio->size = gst_asf_demux_get_uint16 (p_data, p_size);
  return TRUE;
}

static gboolean
gst_asf_demux_get_stream_video (asf_stream_video * video, guint8 ** p_data,
    guint64 * p_size)
{
  if (*p_size < (4 + 4 + 1 + 2))
    return FALSE;

  video->width = gst_asf_demux_get_uint32 (p_data, p_size);
  video->height = gst_asf_demux_get_uint32 (p_data, p_size);
  video->unknown = gst_asf_demux_get_uint8 (p_data, p_size);
  video->size = gst_asf_demux_get_uint16 (p_data, p_size);
  return TRUE;
}

static gboolean
gst_asf_demux_get_stream_video_format (asf_stream_video_format * fmt,
    guint8 ** p_data, guint64 * p_size)
{
  if (*p_size < (4 + 4 + 4 + 2 + 2 + 4 + 4 + 4 + 4 + 4 + 4))
    return FALSE;

  fmt->size = gst_asf_demux_get_uint32 (p_data, p_size);
  fmt->width = gst_asf_demux_get_uint32 (p_data, p_size);
  fmt->height = gst_asf_demux_get_uint32 (p_data, p_size);
  fmt->planes = gst_asf_demux_get_uint16 (p_data, p_size);
  fmt->depth = gst_asf_demux_get_uint16 (p_data, p_size);
  fmt->tag = gst_asf_demux_get_uint32 (p_data, p_size);
  fmt->image_size = gst_asf_demux_get_uint32 (p_data, p_size);
  fmt->xpels_meter = gst_asf_demux_get_uint32 (p_data, p_size);
  fmt->ypels_meter = gst_asf_demux_get_uint32 (p_data, p_size);
  fmt->num_colors = gst_asf_demux_get_uint32 (p_data, p_size);
  fmt->imp_colors = gst_asf_demux_get_uint32 (p_data, p_size);
  return TRUE;
}

AsfStream *
gst_asf_demux_get_stream (GstASFDemux * demux, guint16 id)
{
  guint i;

  for (i = 0; i < demux->num_streams; i++) {
    if (demux->stream[i].id == id)
      return &demux->stream[i];
  }

  GST_WARNING ("Segment found for undefined stream: (%d)", id);
  return NULL;
}

static void
gst_asf_demux_setup_pad (GstASFDemux * demux, GstPad * src_pad,
    GstCaps * caps, guint16 id, gboolean is_video, GstTagList * tags)
{
  AsfStream *stream;

  gst_pad_use_fixed_caps (src_pad);
  gst_pad_set_caps (src_pad, caps);

  gst_pad_set_event_function (src_pad,
      GST_DEBUG_FUNCPTR (gst_asf_demux_handle_src_event));
  gst_pad_set_query_type_function (src_pad,
      GST_DEBUG_FUNCPTR (gst_asf_demux_get_src_query_types));
  gst_pad_set_query_function (src_pad,
      GST_DEBUG_FUNCPTR (gst_asf_demux_handle_src_query));

  stream = &demux->stream[demux->num_streams];
  stream->caps = caps;
  stream->pad = src_pad;
  stream->id = id;
  stream->fps_known = !is_video;        /* bit hacky for audio */
  stream->is_video = is_video;
  stream->pending_tags = tags;
  stream->discont = TRUE;
  if (is_video) {
    GstStructure *st;
    gint par_x, par_y;
    st = gst_caps_get_structure (caps, 0);
    if (gst_structure_get_fraction (st, "pixel-aspect-ratio", &par_x, &par_y) &&
        par_x > 0 && par_y > 0) {
      GST_DEBUG ("PAR %d/%d", par_x, par_y);
      stream->par_x = par_x;
      stream->par_y = par_y;
    }
  }

  stream->payloads = g_array_new (FALSE, FALSE, sizeof (AsfPayload));

  GST_INFO ("Created pad %s for stream %u with caps %" GST_PTR_FORMAT,
      GST_PAD_NAME (src_pad), demux->num_streams, caps);

  ++demux->num_streams;

  stream->active = FALSE;
}

static void
gst_asf_demux_add_audio_stream (GstASFDemux * demux,
    asf_stream_audio * audio, guint16 id, guint8 ** p_data, guint64 * p_size)
{
  GstTagList *tags = NULL;
  GstBuffer *extradata = NULL;
  GstPad *src_pad;
  GstCaps *caps;
  guint16 size_left = 0;
  gchar *codec_name = NULL;
  gchar *name = NULL;

  size_left = audio->size;

  /* Create the audio pad */
  name = g_strdup_printf ("audio_%02d", demux->num_audio_streams);

  src_pad = gst_pad_new_from_static_template (&audio_src_template, name);
  g_free (name);

  /* Swallow up any left over data and set up the 
   * standard properties from the header info */
  if (size_left) {
    GST_INFO_OBJECT (demux, "Audio header contains %d bytes of "
        "codec specific data", size_left);

    g_assert (size_left <= *p_size);
    gst_asf_demux_get_buffer (&extradata, size_left, p_data, p_size);
  }

  /* asf_stream_audio is the same as gst_riff_strf_auds, but with an
   * additional two bytes indicating extradata. */
    caps = gst_riff_create_audio_caps (audio->codec_tag, NULL,
        (gst_riff_strf_auds *) audio, extradata, NULL, &codec_name);

  if (caps == NULL) {
    caps = gst_caps_new_simple ("audio/x-asf-unknown", "codec_id",
        G_TYPE_INT, (gint) audio->codec_tag, NULL);
  }

  /* Informing about that audio format we just added */
  if (codec_name) {
    tags = gst_tag_list_new ();
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_AUDIO_CODEC,
        codec_name, NULL);
    g_free (codec_name);
  }

  if (extradata)
    gst_buffer_unref (extradata);

  GST_INFO ("Adding audio stream #%u, id %u codec %u (0x%04x), tags=%"
      GST_PTR_FORMAT, demux->num_audio_streams, id, audio->codec_tag,
      audio->codec_tag, tags);

  ++demux->num_audio_streams;

  gst_asf_demux_setup_pad (demux, src_pad, caps, id, FALSE, tags);
}

static void
gst_asf_demux_add_video_stream (GstASFDemux * demux,
    asf_stream_video_format * video, guint16 id,
    guint8 ** p_data, guint64 * p_size)
{
  GstTagList *tags = NULL;
  GstBuffer *extradata = NULL;
  GstPad *src_pad;
  GstCaps *caps;
  gchar *name = NULL;
  gchar *codec_name = NULL;
  gint size_left = video->size - 40;

  /* Create the video pad */
  name = g_strdup_printf ("video_%02d", demux->num_video_streams);
  src_pad = gst_pad_new_from_static_template (&video_src_template, name);
  g_free (name);

  /* Now try some gstreamer formatted MIME types (from gst_avi_demux_strf_vids) */
  if (size_left) {
    GST_LOG ("Video header has %d bytes of codec specific data", size_left);
    //g_assert (size_left <= *p_size); //commented to avoid SIGABORT
    gst_asf_demux_get_buffer (&extradata, size_left, p_data, p_size);
  }

  GST_DEBUG ("video codec %" GST_FOURCC_FORMAT, GST_FOURCC_ARGS (video->tag));

    /* yes, asf_stream_video_format and gst_riff_strf_vids are the same */
    caps = gst_riff_create_video_caps (video->tag, NULL,
                  (gst_riff_strf_vids *) video, extradata, NULL, &codec_name);

  if (caps == NULL) {
    caps = gst_caps_new_simple ("video/x-asf-unknown", "fourcc",
        GST_TYPE_FOURCC, video->tag, NULL);
  } else {
    GstStructure *s;
    gint ax, ay;

    s = gst_asf_demux_get_metadata_for_stream (demux, id);
    if (gst_structure_get_int (s, "AspectRatioX", &ax) &&
        gst_structure_get_int (s, "AspectRatioY", &ay) && (ax > 0 && ay > 0)) {
      gst_caps_set_simple (caps, "pixel-aspect-ratio", GST_TYPE_FRACTION,
          ax, ay, NULL);

    } else {
      guint ax, ay;
      /* retry with the global metadata */
      GST_DEBUG ("Retrying with global metadata %" GST_PTR_FORMAT,
          demux->global_metadata);
      s = demux->global_metadata;
      if (gst_structure_get_uint (s, "AspectRatioX", &ax) &&
          gst_structure_get_uint (s, "AspectRatioY", &ay)) {
        GST_DEBUG ("ax:%d, ay:%d", ax, ay);
        if (ax > 0 && ay > 0)
          gst_caps_set_simple (caps, "pixel-aspect-ratio", GST_TYPE_FRACTION,
              ax, ay, NULL);
      }
    }
    s = gst_caps_get_structure (caps, 0);
    gst_structure_remove_field (s, "framerate");
  }

  /* add fourcc format to caps, some proprietary decoders seem to need it */
  gst_caps_set_simple (caps, "format", GST_TYPE_FOURCC, video->tag, NULL);

  if (codec_name) {
    tags = gst_tag_list_new ();
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_VIDEO_CODEC,
        codec_name, NULL);
    g_free (codec_name);
  }

  if (extradata)
    gst_buffer_unref (extradata);

  GST_INFO ("Adding video stream #%u, id %u, codec %"
      GST_FOURCC_FORMAT " (0x%08x)", demux->num_video_streams, id,
      GST_FOURCC_ARGS (video->tag), video->tag);

  ++demux->num_video_streams;

  gst_asf_demux_setup_pad (demux, src_pad, caps, id, TRUE, tags);
}

static void
gst_asf_demux_activate_stream (GstASFDemux * demux, AsfStream * stream)
{
  if (!stream->active) {
    GST_INFO_OBJECT (demux, "Activating stream %2u, pad %s, caps %"
        GST_PTR_FORMAT, stream->id, GST_PAD_NAME (stream->pad), stream->caps);
    gst_pad_set_active (stream->pad, TRUE);
    gst_element_add_pad (GST_ELEMENT_CAST (demux), stream->pad);
    stream->active = TRUE;
  }
}

static AsfStream *
gst_asf_demux_parse_stream_object (GstASFDemux * demux, guint8 * data,
    guint64 size)
{
  AsfCorrectionType correction_type;
  AsfStreamType stream_type;
  GstClockTime time_offset;
  gboolean is_encrypted;
  guint16 stream_id;
  guint16 flags;
  ASFGuid guid;
  guint stream_specific_size;
  guint type_specific_size;
  guint unknown;

  /* Get the rest of the header's header */
  if (size < (16 + 16 + 8 + 4 + 4 + 2 + 4))
    goto not_enough_data;

  gst_asf_demux_get_guid (&guid, &data, &size);
  stream_type = gst_asf_demux_identify_guid (asf_stream_guids, &guid);

  gst_asf_demux_get_guid (&guid, &data, &size);
  correction_type = gst_asf_demux_identify_guid (asf_correction_guids, &guid);

  time_offset = gst_asf_demux_get_uint64 (&data, &size) * 100;

  type_specific_size = gst_asf_demux_get_uint32 (&data, &size);
  stream_specific_size = gst_asf_demux_get_uint32 (&data, &size);

  flags = gst_asf_demux_get_uint16 (&data, &size);
  stream_id = flags & 0x7f;
  is_encrypted = ! !((flags & 0x8000) << 15);
  unknown = gst_asf_demux_get_uint32 (&data, &size);

  GST_DEBUG_OBJECT (demux, "Found stream %u, time_offset=%" GST_TIME_FORMAT,
      stream_id, GST_TIME_ARGS (time_offset));

  switch (stream_type) {
    case ASF_STREAM_AUDIO:{
      asf_stream_audio audio_object;
#ifdef ASFDEMUX_MODIFICATION
      if(demux->num_audio_streams)
        goto not_supported;
#endif

      if (!gst_asf_demux_get_stream_audio (&audio_object, &data, &size))
        goto not_enough_data;

      GST_INFO ("Object is an audio stream with %u bytes of additional data",
          audio_object.size);

      gst_asf_demux_add_audio_stream (demux, &audio_object, stream_id,
          &data, &size);
#ifdef ASFDEMUX_MODIFICATION
      demux->audStrmId = stream_id-1;
      GST_INFO_OBJECT (demux, "Asf Audio Stream Id Is %u",demux->audStrmId);
#endif

      switch (correction_type) {
        case ASF_CORRECTION_ON:{
          guint span, packet_size, chunk_size, data_size, silence_data;

          GST_INFO ("Using error correction");

          if (size < (1 + 2 + 2 + 2 + 1))
            goto not_enough_data;

          span = gst_asf_demux_get_uint8 (&data, &size);
          packet_size = gst_asf_demux_get_uint16 (&data, &size);
          chunk_size = gst_asf_demux_get_uint16 (&data, &size);
          data_size = gst_asf_demux_get_uint16 (&data, &size);
          silence_data = gst_asf_demux_get_uint8 (&data, &size);

          /* FIXME: shouldn't this be per-stream? */
          demux->span = span;

          GST_DEBUG_OBJECT (demux, "Descrambling ps:%u cs:%u ds:%u s:%u sd:%u",
              packet_size, chunk_size, data_size, span, silence_data);

          if (demux->span > 1) {
            if (chunk_size == 0 || ((packet_size / chunk_size) <= 1)) {
              /* Disable descrambling */
              demux->span = 0;
            } else {
              /* FIXME: this else branch was added for
               * weird_al_yankovic - the saga begins.asf */
              demux->ds_packet_size = packet_size;
              demux->ds_chunk_size = chunk_size;
            }
          } else {
            /* Descambling is enabled */
            demux->ds_packet_size = packet_size;
            demux->ds_chunk_size = chunk_size;
          }
#if 0
          /* Now skip the rest of the silence data */
          if (data_size > 1)
            gst_bytestream_flush (demux->bs, data_size - 1);
#else
          /* FIXME: CHECKME. And why -1? */
          if (data_size > 1) {
            if (!gst_asf_demux_skip_bytes (data_size - 1, &data, &size)) {
              goto not_enough_data;
            }
          }
#endif
          break;
        }
        case ASF_CORRECTION_OFF:{
          GST_INFO ("Error correction off");
          if (!gst_asf_demux_skip_bytes (stream_specific_size, &data, &size))
            goto not_enough_data;
          break;
        }
        default:
          GST_ELEMENT_ERROR (demux, STREAM, DEMUX, (NULL),
              ("Audio stream using unknown error correction"));
          return NULL;
      }

      break;
    }

    case ASF_STREAM_VIDEO:{
      asf_stream_video_format video_format_object;
      asf_stream_video video_object;
      guint16 vsize;
#ifdef ASFDEMUX_MODIFICATION
      if(demux->num_video_streams)
        goto not_supported;
#endif

      if (!gst_asf_demux_get_stream_video (&video_object, &data, &size))
        goto not_enough_data;

      vsize = video_object.size - 40;   /* Byte order gets offset by single byte */

      GST_INFO ("object is a video stream with %u bytes of "
          "additional data", vsize);

      if (!gst_asf_demux_get_stream_video_format (&video_format_object,
              &data, &size)) {
        goto not_enough_data;
      }

      gst_asf_demux_add_video_stream (demux, &video_format_object, stream_id,
          &data, &size);
#ifdef ASFDEMUX_MODIFICATION
      demux->vidStrmId = stream_id-1;
      GST_INFO_OBJECT (demux, "Asf Video Stream Id Is %u",demux->vidStrmId);
#endif

      break;
    }

    default:
      GST_WARNING_OBJECT (demux, "Unknown stream type for stream %u",
          stream_id);
      break;
  }

  return gst_asf_demux_get_stream (demux, stream_id);

not_enough_data:
  {
    GST_WARNING_OBJECT (demux, "Unexpected end of data parsing stream object");
    /* we'll error out later if we found no streams */
    return NULL;
  }
#ifdef ASFDEMUX_MODIFICATION
not_supported:
  {
    GST_WARNING_OBJECT (demux, "Only One stream is supported per stream type");
    /* we'll error out later if we found no streams */
    return NULL;
  }
#endif
}

static const gchar *
gst_asf_demux_get_gst_tag_from_tag_name (const gchar * name_utf8)
{
  const struct
  {
    const gchar *asf_name;
    const gchar *gst_name;
  } tags[] = {
    {
    "WM/Genre", GST_TAG_GENRE}, {
    "WM/AlbumTitle", GST_TAG_ALBUM}, {
    "WM/AlbumArtist", GST_TAG_ARTIST}, {
    "WM/Picture", GST_TAG_IMAGE}, {
    "WM/Track", GST_TAG_TRACK_NUMBER}, {
    "WM/TrackNumber", GST_TAG_TRACK_NUMBER}, {
    "WM/Year", GST_TAG_DATE}
    /* { "WM/Composer", GST_TAG_COMPOSER } */
  };
  gsize out;
  guint i;

  if (name_utf8 == NULL) {
    GST_WARNING ("Failed to convert name to UTF8, skipping");
    return NULL;
  }

  out = strlen (name_utf8);

  for (i = 0; i < G_N_ELEMENTS (tags); ++i) {
    if (strncmp (tags[i].asf_name, name_utf8, out) == 0) {
      GST_LOG ("map tagname '%s' -> '%s'", name_utf8, tags[i].gst_name);
      return tags[i].gst_name;
    }
  }

  return NULL;
}

/* gst_asf_demux_add_global_tags() takes ownership of taglist! */
static void
gst_asf_demux_add_global_tags (GstASFDemux * demux, GstTagList * taglist)
{
  GstTagList *t;

  GST_DEBUG_OBJECT (demux, "adding global tags: %" GST_PTR_FORMAT, taglist);

  if (taglist == NULL)
    return;

  if (gst_tag_list_is_empty (taglist)) {
    gst_tag_list_free (taglist);
    return;
  }

  t = gst_tag_list_merge (demux->taglist, taglist, GST_TAG_MERGE_APPEND);
  if (demux->taglist)
    gst_tag_list_free (demux->taglist);
  gst_tag_list_free (taglist);
  demux->taglist = t;
  GST_LOG_OBJECT (demux, "global tags now: %" GST_PTR_FORMAT, demux->taglist);
}

#define ASF_DEMUX_DATA_TYPE_UTF16LE_STRING  0
#define ASF_DEMUX_DATA_TYPE_BYTE_ARRAY      1
#define ASF_DEMUX_DATA_TYPE_DWORD           3

static void
asf_demux_parse_picture_tag (GstTagList * tags, const guint8 * tag_data,
    guint tag_data_len)
{
  GstByteReader r;
  const guint8 *img_data = NULL;
  guint32 img_data_len = 0;
  guint8 pic_type = 0;

  gst_byte_reader_init (&r, tag_data, tag_data_len);

  /* skip mime type string (we don't trust it and do our own typefinding),
   * and also skip the description string, since we don't use it */
  if (!gst_byte_reader_get_uint8 (&r, &pic_type) ||
      !gst_byte_reader_get_uint32_le (&r, &img_data_len) ||
      !gst_byte_reader_skip_string_utf16 (&r) ||
      !gst_byte_reader_skip_string_utf16 (&r) ||
      !gst_byte_reader_get_data (&r, img_data_len, &img_data)) {
    goto not_enough_data;
  }


  if (!gst_tag_list_add_id3_image (tags, img_data, img_data_len, pic_type))
    GST_DEBUG ("failed to add image extracted from WM/Picture tag to taglist");

  return;

not_enough_data:
  {
    GST_DEBUG ("Failed to read WM/Picture tag: not enough data");
    GST_MEMDUMP ("WM/Picture data", tag_data, tag_data_len);
    return;
  }
}

/* Extended Content Description Object */
static GstFlowReturn
gst_asf_demux_process_ext_content_desc (GstASFDemux * demux, guint8 * data,
    guint64 size)
{
  /* Other known (and unused) 'text/unicode' metadata available :
   *
   *   WM/Lyrics =
   *   WM/MediaPrimaryClassID = {D1607DBC-E323-4BE2-86A1-48A42A28441E}
   *   WMFSDKVersion = 9.00.00.2980
   *   WMFSDKNeeded = 0.0.0.0000
   *   WM/UniqueFileIdentifier = AMGa_id=R    15334;AMGp_id=P     5149;AMGt_id=T  2324984
   *   WM/Publisher = 4AD
   *   WM/Provider = AMG
   *   WM/ProviderRating = 8
   *   WM/ProviderStyle = Rock (similar to WM/Genre)
   *   WM/GenreID (similar to WM/Genre)
   *   WM/TrackNumber (same as WM/Track but as a string)
   *
   * Other known (and unused) 'non-text' metadata available :
   *
   *   WM/EncodingTime
   *   WM/MCDI
   *   IsVBR
   *
   * We might want to read WM/TrackNumber and use atoi() if we don't have
   * WM/Track
   */

  GstTagList *taglist;
  guint16 blockcount, i;

  GST_INFO_OBJECT (demux, "object is an extended content description");

  taglist = gst_tag_list_new ();

  /* Content Descriptor Count */
  if (size < 2)
    goto not_enough_data;

  blockcount = gst_asf_demux_get_uint16 (&data, &size);

  for (i = 1; i <= blockcount; ++i) {
    const gchar *gst_tag_name;
    guint16 datatype;
    guint16 value_len;
    guint16 name_len;
    GValue tag_value = { 0, };
    gsize in, out;
    gchar *name;
    gchar *name_utf8 = NULL;
    gchar *value;

    /* Descriptor */
    if (!gst_asf_demux_get_string (&name, &name_len, &data, &size))
      goto not_enough_data;

    if (size < 2) {
      g_free (name);
      goto not_enough_data;
    }
    /* Descriptor Value Data Type */
    datatype = gst_asf_demux_get_uint16 (&data, &size);

    /* Descriptor Value (not really a string, but same thing reading-wise) */
    if (!gst_asf_demux_get_string (&value, &value_len, &data, &size)) {
      g_free (name);
      goto not_enough_data;
    }

    name_utf8 =
        g_convert (name, name_len, "UTF-8", "UTF-16LE", &in, &out, NULL);

    if (name_utf8 != NULL) {
      GST_DEBUG ("Found tag/metadata %s", name_utf8);

      gst_tag_name = gst_asf_demux_get_gst_tag_from_tag_name (name_utf8);
      GST_DEBUG ("gst_tag_name %s", GST_STR_NULL (gst_tag_name));

      switch (datatype) {
        case ASF_DEMUX_DATA_TYPE_UTF16LE_STRING:{
          gchar *value_utf8;

          value_utf8 = g_convert (value, value_len, "UTF-8", "UTF-16LE",
              &in, &out, NULL);

          /* get rid of tags with empty value */
          if (value_utf8 != NULL && *value_utf8 != '\0') {
            GST_DEBUG ("string value %s", value_utf8);

            value_utf8[out] = '\0';

            if (gst_tag_name != NULL) {
              if (strcmp (gst_tag_name, GST_TAG_DATE) == 0) {
                guint year = atoi (value_utf8);

                if (year > 0) {
                  GDate *date = g_date_new_dmy (1, 1, year);

                  g_value_init (&tag_value, GST_TYPE_DATE);
                  gst_value_set_date (&tag_value, date);
                  g_date_free (date);
                }
              } else if (strcmp (gst_tag_name, GST_TAG_GENRE) == 0) {
                guint id3v1_genre_id;
                const gchar *genre_str;
#ifndef ASFDEMUX_MODIFICATION
                if (sscanf (value_utf8, "(%u)", &id3v1_genre_id) == 1 &&
                    ((genre_str = gst_tag_id3_genre_get (id3v1_genre_id)))) {
                  GST_DEBUG ("Genre: %s -> %s", value_utf8, genre_str);
                  g_free (value_utf8);
                  value_utf8 = g_strdup (genre_str);
                }
#else
                GST_DEBUG_OBJECT (demux, "adding genre value = %s", value_utf8);
                g_value_init (&tag_value, G_TYPE_STRING);
                g_value_set_string (&tag_value, value_utf8);
#endif

              } else {
                GType tag_type;

                /* convert tag from string to other type if required */
                tag_type = gst_tag_get_type (gst_tag_name);
                g_value_init (&tag_value, tag_type);
                if (!gst_value_deserialize (&tag_value, value_utf8)) {
                  GValue from_val = { 0, };

                  g_value_init (&from_val, G_TYPE_STRING);
                  g_value_set_string (&from_val, value_utf8);
                  if (!g_value_transform (&from_val, &tag_value)) {
                    GST_WARNING_OBJECT (demux,
                        "Could not transform string tag to " "%s tag type %s",
                        gst_tag_name, g_type_name (tag_type));
                    g_value_unset (&tag_value);
                  }
                  g_value_unset (&from_val);
                }
              }
            } else {
              /* metadata ! */
              GST_DEBUG ("Setting metadata");
              g_value_init (&tag_value, G_TYPE_STRING);
              g_value_set_string (&tag_value, value_utf8);
            }
          } else if (value_utf8 == NULL) {
            GST_WARNING ("Failed to convert string value to UTF8, skipping");
          } else {
            GST_DEBUG ("Skipping empty string value for %s",
                GST_STR_NULL (gst_tag_name));
          }
          g_free (value_utf8);
          break;
        }
        case ASF_DEMUX_DATA_TYPE_BYTE_ARRAY:{
          if (gst_tag_name) {
            if (!g_str_equal (gst_tag_name, GST_TAG_IMAGE)) {
              GST_FIXME ("Unhandled byte array tag %s",
                  GST_STR_NULL (gst_tag_name));
              break;
            } else {
              asf_demux_parse_picture_tag (taglist, (guint8 *) value,
                  value_len);
            }
          }
          break;
        }
        case ASF_DEMUX_DATA_TYPE_DWORD:{
          guint uint_val = GST_READ_UINT32_LE (value);

          /* this is the track number */
          g_value_init (&tag_value, G_TYPE_UINT);

          /* WM/Track counts from 0 */
          if (!strcmp (name_utf8, "WM/Track"))
            ++uint_val;

          g_value_set_uint (&tag_value, uint_val);
          break;
        }
        default:{
          GST_DEBUG ("Skipping tag %s of type %d", gst_tag_name, datatype);
          break;
        }
      }

      if (G_IS_VALUE (&tag_value)) {
        if (gst_tag_name) {
          GstTagMergeMode merge_mode = GST_TAG_MERGE_APPEND;

          /* WM/TrackNumber is more reliable than WM/Track, since the latter
           * is supposed to have a 0 base but is often wrongly written to start
           * from 1 as well, so prefer WM/TrackNumber when we have it: either
           * replace the value added earlier from WM/Track or put it first in
           * the list, so that it will get picked up by _get_uint() */
          if (strcmp (name_utf8, "WM/TrackNumber") == 0)
            merge_mode = GST_TAG_MERGE_REPLACE;

          gst_tag_list_add_values (taglist, merge_mode, gst_tag_name,
              &tag_value, NULL);
        } else {
          GST_DEBUG ("Setting global metadata %s", name_utf8);
          gst_structure_set_value (demux->global_metadata, name_utf8,
              &tag_value);
        }

        g_value_unset (&tag_value);
      }
    }

    g_free (name);
    g_free (value);
    g_free (name_utf8);
  }

  gst_asf_demux_add_global_tags (demux, taglist);

  return GST_FLOW_OK;

  /* Errors */
not_enough_data:
  {
    GST_WARNING ("Unexpected end of data parsing ext content desc object");
    gst_tag_list_free (taglist);
    return GST_FLOW_OK;         /* not really fatal */
  }
}

static GstStructure *
gst_asf_demux_get_metadata_for_stream (GstASFDemux * demux, guint stream_num)
{
  gchar sname[32];
  guint i;

  g_snprintf (sname, sizeof (sname), "stream-%u", stream_num);

  for (i = 0; i < gst_caps_get_size (demux->metadata); ++i) {
    GstStructure *s;

    s = gst_caps_get_structure (demux->metadata, i);
    if (gst_structure_has_name (s, sname))
      return s;
  }

  gst_caps_append_structure (demux->metadata, gst_structure_empty_new (sname));

  /* try lookup again; demux->metadata took ownership of the structure, so we
   * can't really make any assumptions about what happened to it, so we can't
   * just return it directly after appending it */
  return gst_asf_demux_get_metadata_for_stream (demux, stream_num);
}

static GstFlowReturn
gst_asf_demux_process_metadata (GstASFDemux * demux, guint8 * data,
    guint64 size)
{
  guint16 blockcount, i;

  GST_INFO_OBJECT (demux, "object is a metadata object");

  /* Content Descriptor Count */
  if (size < 2)
    goto not_enough_data;

  blockcount = gst_asf_demux_get_uint16 (&data, &size);

  for (i = 0; i < blockcount; ++i) {
    GstStructure *s;
    guint16 lang_idx, stream_num, name_len, data_type;
    guint32 data_len, ival;
    gchar *name_utf8;

    if (size < (2 + 2 + 2 + 2 + 4))
      goto not_enough_data;

    lang_idx = gst_asf_demux_get_uint16 (&data, &size);
    stream_num = gst_asf_demux_get_uint16 (&data, &size);
    name_len = gst_asf_demux_get_uint16 (&data, &size);
    data_type = gst_asf_demux_get_uint16 (&data, &size);
    data_len = gst_asf_demux_get_uint32 (&data, &size);

    if (size < name_len + data_len)
      goto not_enough_data;

    /* convert name to UTF-8 */
    name_utf8 = g_convert ((gchar *) data, name_len, "UTF-8", "UTF-16LE",
        NULL, NULL, NULL);
    gst_asf_demux_skip_bytes (name_len, &data, &size);

    if (name_utf8 == NULL) {
      GST_WARNING ("Failed to convert value name to UTF8, skipping");
      gst_asf_demux_skip_bytes (data_len, &data, &size);
      continue;
    }

    if (data_type != ASF_DEMUX_DATA_TYPE_DWORD) {
      gst_asf_demux_skip_bytes (data_len, &data, &size);
      g_free (name_utf8);
      continue;
    }

    /* read DWORD */
    if (size < 4) {
      g_free (name_utf8);
      goto not_enough_data;
    }

    ival = gst_asf_demux_get_uint32 (&data, &size);

    /* skip anything else there may be, just in case */
    gst_asf_demux_skip_bytes (data_len - 4, &data, &size);

    s = gst_asf_demux_get_metadata_for_stream (demux, stream_num);
    gst_structure_set (s, name_utf8, G_TYPE_INT, ival, NULL);
    g_free (name_utf8);
  }

  GST_INFO_OBJECT (demux, "metadata = %" GST_PTR_FORMAT, demux->metadata);
  return GST_FLOW_OK;

  /* Errors */
not_enough_data:
  {
    GST_WARNING ("Unexpected end of data parsing metadata object");
    return GST_FLOW_OK;         /* not really fatal */
  }
}

static GstFlowReturn
gst_asf_demux_process_header (GstASFDemux * demux, guint8 * data, guint64 size)
{
  GstFlowReturn ret = GST_FLOW_OK;
  guint32 i, num_objects;
  guint8 unknown;

  /* Get the rest of the header's header */
  if (size < (4 + 1 + 1))
    goto not_enough_data;

  num_objects = gst_asf_demux_get_uint32 (&data, &size);
  unknown = gst_asf_demux_get_uint8 (&data, &size);
  unknown = gst_asf_demux_get_uint8 (&data, &size);

  GST_INFO_OBJECT (demux, "object is a header with %u parts", num_objects);

  /* Loop through the header's objects, processing those */
  for (i = 0; i < num_objects; ++i) {
    GST_INFO_OBJECT (demux, "reading header part %u", i);
    ret = gst_asf_demux_process_object (demux, &data, &size);
    if (ret != GST_FLOW_OK) {
      GST_WARNING ("process_object returned %s", gst_asf_get_flow_name (ret));
      break;
    }
  }

  return ret;

not_enough_data:
  {
    GST_ELEMENT_ERROR (demux, STREAM, DEMUX, (NULL),
        ("short read parsing HEADER object"));
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_asf_demux_process_file (GstASFDemux * demux, guint8 * data, guint64 size)
{
  guint64 file_size, creation_time, packets_count;
  guint64 play_time, send_time, preroll;
  guint32 flags, min_pktsize, max_pktsize, min_bitrate;

  if (size < (16 + 8 + 8 + 8 + 8 + 8 + 8 + 4 + 4 + 4 + 4))
    goto not_enough_data;

  gst_asf_demux_skip_bytes (16, &data, &size);  /* skip GUID */
  file_size = gst_asf_demux_get_uint64 (&data, &size);
  creation_time = gst_asf_demux_get_uint64 (&data, &size);
  packets_count = gst_asf_demux_get_uint64 (&data, &size);
  play_time = gst_asf_demux_get_uint64 (&data, &size);
  send_time = gst_asf_demux_get_uint64 (&data, &size);
  preroll = gst_asf_demux_get_uint64 (&data, &size);
  flags = gst_asf_demux_get_uint32 (&data, &size);
  min_pktsize = gst_asf_demux_get_uint32 (&data, &size);
  max_pktsize = gst_asf_demux_get_uint32 (&data, &size);
  min_bitrate = gst_asf_demux_get_uint32 (&data, &size);

  demux->broadcast = ! !(flags & 0x01);
  demux->seekable = ! !(flags & 0x02);

  GST_DEBUG_OBJECT (demux, "min_pktsize = %u", min_pktsize);
  GST_DEBUG_OBJECT (demux, "flags::broadcast = %d", demux->broadcast);
  GST_DEBUG_OBJECT (demux, "flags::seekable  = %d", demux->seekable);
#ifdef ASFDEMUX_MODIFICATION
  GST_DEBUG_OBJECT (demux, "preroll  = %d", preroll);
  demux->trickplay_info.tp_preroll = preroll * GST_MSECOND;
#endif

  if (demux->broadcast) {
    /* these fields are invalid if the broadcast flag is set */
    play_time = 0;
    file_size = 0;
  }

  if (min_pktsize != max_pktsize)
    goto non_fixed_packet_size;

  demux->packet_size = max_pktsize;

  /* FIXME: do we need send_time as well? what is it? */
  if ((play_time * 100) >= (preroll * GST_MSECOND))
    demux->play_time = (play_time * 100) - (preroll * GST_MSECOND);
  else
    demux->play_time = 0;

  /* Making preroll to zero  to reduce the memory requirement*/	
#ifndef ASFDEMUX_MODIFICATION
  preroll = 0;  //Intentionally avoiding this..otherwise trickplay is not working properly
#endif

  demux->preroll = preroll * GST_MSECOND;

  /* initial latency */
  demux->latency = demux->preroll;

  if (demux->play_time == 0)
    demux->seekable = FALSE;
#ifdef ASFDEMUX_MODIFICATION
  if (demux->play_time > 0) {
    gst_segment_set_duration (&demux->segment, GST_FORMAT_TIME, demux->play_time);
  }
#endif

  GST_DEBUG_OBJECT (demux, "play_time %" GST_TIME_FORMAT,
      GST_TIME_ARGS (demux->play_time));
  GST_DEBUG_OBJECT (demux, "preroll   %" GST_TIME_FORMAT,
      GST_TIME_ARGS (demux->preroll));

  if (demux->play_time > 0) {
    gst_segment_set_duration (&demux->segment, GST_FORMAT_TIME,
        demux->play_time);
  }

  GST_INFO ("object is a file with %" G_GUINT64_FORMAT " data packets",
      packets_count);
  GST_INFO ("preroll = %" G_GUINT64_FORMAT, demux->preroll);

  return GST_FLOW_OK;

/* ERRORS */
non_fixed_packet_size:
  {
    GST_ELEMENT_ERROR (demux, STREAM, DEMUX, (NULL),
        ("packet size must be fixed"));
    return GST_FLOW_ERROR;
  }
not_enough_data:
  {
    GST_ELEMENT_ERROR (demux, STREAM, DEMUX, (NULL),
        ("short read parsing FILE object"));
    return GST_FLOW_ERROR;
  }
}

/* Content Description Object */
static GstFlowReturn
gst_asf_demux_process_comment (GstASFDemux * demux, guint8 * data, guint64 size)
{
  struct
  {
    const gchar *gst_tag;
    guint16 val_length;
    gchar *val_utf8;
  } tags[5] = {
    {
    GST_TAG_TITLE, 0, NULL}, {
    GST_TAG_ARTIST, 0, NULL}, {
    GST_TAG_COPYRIGHT, 0, NULL}, {
    GST_TAG_DESCRIPTION, 0, NULL}, {
    GST_TAG_COMMENT, 0, NULL}
  };
  GstTagList *taglist;
  GValue value = { 0 };
  gsize in, out;
  gint i = -1;

  GST_INFO_OBJECT (demux, "object is a comment");

  if (size < (2 + 2 + 2 + 2 + 2))
    goto not_enough_data;

  tags[0].val_length = gst_asf_demux_get_uint16 (&data, &size);
  tags[1].val_length = gst_asf_demux_get_uint16 (&data, &size);
  tags[2].val_length = gst_asf_demux_get_uint16 (&data, &size);
  tags[3].val_length = gst_asf_demux_get_uint16 (&data, &size);
  tags[4].val_length = gst_asf_demux_get_uint16 (&data, &size);

  GST_DEBUG_OBJECT (demux, "Comment lengths: title=%d author=%d copyright=%d "
      "description=%d rating=%d", tags[0].val_length, tags[1].val_length,
      tags[2].val_length, tags[3].val_length, tags[4].val_length);

  for (i = 0; i < G_N_ELEMENTS (tags); ++i) {
    if (size < tags[i].val_length)
      goto not_enough_data;

    /* might be just '/0', '/0'... */
    if (tags[i].val_length > 2 && tags[i].val_length % 2 == 0) {
      /* convert to UTF-8 */
      tags[i].val_utf8 = g_convert ((gchar *) data, tags[i].val_length,
          "UTF-8", "UTF-16LE", &in, &out, NULL);
    }
    gst_asf_demux_skip_bytes (tags[i].val_length, &data, &size);
  }

  /* parse metadata into taglist */
  taglist = gst_tag_list_new ();
  g_value_init (&value, G_TYPE_STRING);
  for (i = 0; i < G_N_ELEMENTS (tags); ++i) {
    if (tags[i].val_utf8 && strlen (tags[i].val_utf8) > 0 && tags[i].gst_tag) {
      g_value_set_string (&value, tags[i].val_utf8);
      gst_tag_list_add_values (taglist, GST_TAG_MERGE_APPEND,
          tags[i].gst_tag, &value, NULL);
    }
  }
  g_value_unset (&value);

  gst_asf_demux_add_global_tags (demux, taglist);

  for (i = 0; i < G_N_ELEMENTS (tags); ++i)
    g_free (tags[i].val_utf8);

  return GST_FLOW_OK;

not_enough_data:
  {
    GST_WARNING_OBJECT (demux, "unexpectedly short of data while processing "
        "comment tag section %d, skipping comment object", i);
    for (i = 0; i < G_N_ELEMENTS (tags); i++)
      g_free (tags[i].val_utf8);
    return GST_FLOW_OK;         /* not really fatal */
  }
}

static GstFlowReturn
gst_asf_demux_process_bitrate_props_object (GstASFDemux * demux, guint8 * data,
    guint64 size)
{
  guint16 num_streams, i;
  AsfStream *stream;

  if (size < 2)
    goto not_enough_data;

  num_streams = gst_asf_demux_get_uint16 (&data, &size);

  GST_INFO ("object is a bitrate properties object with %u streams",
      num_streams);

  if (size < (num_streams * (2 + 4)))
    goto not_enough_data;

  for (i = 0; i < num_streams; ++i) {
    guint32 bitrate;
    guint16 stream_id;

    stream_id = gst_asf_demux_get_uint16 (&data, &size);
    bitrate = gst_asf_demux_get_uint32 (&data, &size);

    if (stream_id < GST_ASF_DEMUX_NUM_STREAM_IDS) {
      GST_DEBUG_OBJECT (demux, "bitrate of stream %u = %u", stream_id, bitrate);
      stream = gst_asf_demux_get_stream (demux, stream_id);
      if (stream) {
        if (stream->pending_tags == NULL)
          stream->pending_tags = gst_tag_list_new ();
        gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_REPLACE,
            GST_TAG_BITRATE, bitrate, NULL);
      } else {
        GST_WARNING_OBJECT (demux, "Stream id %u wasn't found", stream_id);
      }
    } else {
      GST_WARNING ("stream id %u is too large", stream_id);
    }
  }

  return GST_FLOW_OK;

not_enough_data:
  {
    GST_WARNING_OBJECT (demux, "short read parsing bitrate props object!");
    return GST_FLOW_OK;         /* not really fatal */
  }
}

static GstFlowReturn
gst_asf_demux_process_header_ext (GstASFDemux * demux, guint8 * data,
    guint64 size)
{
  GstFlowReturn ret = GST_FLOW_OK;
  guint64 hdr_size;

  /* Get the rest of the header's header */
  if (size < (16 + 2 + 4))
    goto not_enough_data;

  /* skip GUID and two other bytes */
  gst_asf_demux_skip_bytes (16 + 2, &data, &size);
  hdr_size = gst_asf_demux_get_uint32 (&data, &size);

  GST_INFO ("extended header object with a size of %u bytes", (guint) size);

  /* FIXME: does data_size include the rest of the header that we have read? */
  if (hdr_size > size)
    goto not_enough_data;

  while (hdr_size > 0) {
    ret = gst_asf_demux_process_object (demux, &data, &hdr_size);
    if (ret != GST_FLOW_OK)
      break;
  }

  return ret;

not_enough_data:
  {
    GST_ELEMENT_ERROR (demux, STREAM, DEMUX, (NULL),
        ("short read parsing extended header object"));
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_asf_demux_process_language_list (GstASFDemux * demux, guint8 * data,
    guint64 size)
{
  guint i;

  if (size < 2)
    goto not_enough_data;

  if (demux->languages) {
    GST_WARNING ("More than one LANGUAGE_LIST object in stream");
    g_strfreev (demux->languages);
    demux->languages = NULL;
    demux->num_languages = 0;
  }

  demux->num_languages = gst_asf_demux_get_uint16 (&data, &size);
  GST_LOG ("%u languages:", demux->num_languages);

  demux->languages = g_new0 (gchar *, demux->num_languages + 1);
  for (i = 0; i < demux->num_languages; ++i) {
    guint8 len, *lang_data = NULL;

    if (size < 1)
      goto not_enough_data;
    len = gst_asf_demux_get_uint8 (&data, &size);
    if (gst_asf_demux_get_bytes (&lang_data, len, &data, &size)) {
      gchar *utf8;

      utf8 = g_convert ((gchar *) lang_data, len, "UTF-8", "UTF-16LE", NULL,
          NULL, NULL);

      /* truncate "en-us" etc. to just "en" */
      if (utf8 && strlen (utf8) >= 5 && (utf8[2] == '-' || utf8[2] == '_')) {
        utf8[2] = '\0';
      }
      GST_DEBUG ("[%u] %s", i, GST_STR_NULL (utf8));
      demux->languages[i] = utf8;
      g_free (lang_data);
    } else {
      goto not_enough_data;
    }
  }

  return GST_FLOW_OK;

not_enough_data:
  {
    GST_WARNING_OBJECT (demux, "short read parsing language list object!");
    g_free (demux->languages);
    demux->languages = NULL;
    return GST_FLOW_OK;         /* not fatal */
  }
}

static GstFlowReturn
gst_asf_demux_process_simple_index (GstASFDemux * demux, guint8 * data,
    guint64 size)
{
  GstClockTime interval;
  guint32 count, i;

  if (size < (16 + 8 + 4 + 4))
    goto not_enough_data;

  /* skip file id */
  gst_asf_demux_skip_bytes (16, &data, &size);
  interval = gst_asf_demux_get_uint64 (&data, &size) * (GstClockTime) 100;
  gst_asf_demux_skip_bytes (4, &data, &size);
  count = gst_asf_demux_get_uint32 (&data, &size);
  if (count > 0) {
    demux->sidx_interval = interval;
    demux->sidx_num_entries = count;
    g_free (demux->sidx_entries);
    demux->sidx_entries = g_new0 (AsfSimpleIndexEntry, count);

    for (i = 0; i < count; ++i) {
      if (G_UNLIKELY (size <= 6))
        break;
      demux->sidx_entries[i].packet = gst_asf_demux_get_uint32 (&data, &size);
      demux->sidx_entries[i].count = gst_asf_demux_get_uint16 (&data, &size);
      GST_LOG_OBJECT (demux, "%" GST_TIME_FORMAT " = packet %4u  count : %2d",
          GST_TIME_ARGS (i * interval), demux->sidx_entries[i].packet,
          demux->sidx_entries[i].count);
    }
  } else {
    GST_DEBUG_OBJECT (demux, "simple index object with 0 entries");
  }

  return GST_FLOW_OK;

not_enough_data:
  {
    GST_WARNING_OBJECT (demux, "short read parsing simple index object!");
    return GST_FLOW_OK;         /* not fatal */
  }
}

static GstFlowReturn
gst_asf_demux_process_advanced_mutual_exclusion (GstASFDemux * demux,
    guint8 * data, guint64 size)
{
  ASFGuid guid;
  guint16 num, i;
  guint8 *mes;

  if (size < 16 + 2 + (2 * 2))
    goto not_enough_data;

  gst_asf_demux_get_guid (&guid, &data, &size);
  num = gst_asf_demux_get_uint16 (&data, &size);

  if (num < 2) {
    GST_WARNING_OBJECT (demux, "nonsensical mutually exclusive streams count");
    return GST_FLOW_OK;
  }

  if (size < (num * sizeof (guint16)))
    goto not_enough_data;

  /* read mutually exclusive stream numbers */
  mes = g_new (guint8, num + 1);
  for (i = 0; i < num; ++i) {
    mes[i] = gst_asf_demux_get_uint16 (&data, &size) & 0x7f;
    GST_LOG_OBJECT (demux, "mutually exclusive: stream #%d", mes[i]);
  }

  /* add terminator so we can easily get the count or know when to stop */
  mes[i] = (guint8) - 1;

  demux->mut_ex_streams = g_slist_append (demux->mut_ex_streams, mes);

  return GST_FLOW_OK;

  /* Errors */
not_enough_data:
  {
    GST_WARNING_OBJECT (demux, "short read parsing advanced mutual exclusion");
    return GST_FLOW_OK;         /* not absolutely fatal */
  }
}

static GstFlowReturn
gst_asf_demux_process_ext_stream_props (GstASFDemux * demux, guint8 * data,
    guint64 size)
{
  AsfStreamExtProps esp;
  AsfStream *stream = NULL;
  AsfObject stream_obj;
  guint16 stream_name_count;
  guint16 num_payload_ext;
  guint64 len;
  guint8 *stream_obj_data = NULL;
  guint8 *data_start;
  guint obj_size;
  guint i, stream_num;

  data_start = data;
  obj_size = (guint) size;

  if (size < 64)
    goto not_enough_data;

  esp.valid = TRUE;
  esp.start_time = gst_asf_demux_get_uint64 (&data, &size) * GST_MSECOND;
  esp.end_time = gst_asf_demux_get_uint64 (&data, &size) * GST_MSECOND;
  esp.data_bitrate = gst_asf_demux_get_uint32 (&data, &size);
  esp.buffer_size = gst_asf_demux_get_uint32 (&data, &size);
  esp.intial_buf_fullness = gst_asf_demux_get_uint32 (&data, &size);
  esp.data_bitrate2 = gst_asf_demux_get_uint32 (&data, &size);
  esp.buffer_size2 = gst_asf_demux_get_uint32 (&data, &size);
  esp.intial_buf_fullness2 = gst_asf_demux_get_uint32 (&data, &size);
  esp.max_obj_size = gst_asf_demux_get_uint32 (&data, &size);
  esp.flags = gst_asf_demux_get_uint32 (&data, &size);
  stream_num = gst_asf_demux_get_uint16 (&data, &size);
  esp.lang_idx = gst_asf_demux_get_uint16 (&data, &size);
  esp.avg_time_per_frame = gst_asf_demux_get_uint64 (&data, &size);
  stream_name_count = gst_asf_demux_get_uint16 (&data, &size);
  num_payload_ext = gst_asf_demux_get_uint16 (&data, &size);

  GST_INFO ("start_time             = %" GST_TIME_FORMAT,
      GST_TIME_ARGS (esp.start_time));
  GST_INFO ("end_time               = %" GST_TIME_FORMAT,
      GST_TIME_ARGS (esp.end_time));
  GST_INFO ("flags                  = %08x", esp.flags);
  GST_INFO ("average time per frame = %" GST_TIME_FORMAT,
      GST_TIME_ARGS (esp.avg_time_per_frame * 100));
  GST_INFO ("stream number          = %u", stream_num);
  GST_INFO ("stream language ID idx = %u (%s)", esp.lang_idx,
      (esp.lang_idx < demux->num_languages) ?
      GST_STR_NULL (demux->languages[esp.lang_idx]) : "??");
  GST_INFO ("stream name count      = %u", stream_name_count);

  /* read stream names */
  for (i = 0; i < stream_name_count; ++i) {
    guint16 stream_lang_idx;
    gchar *stream_name = NULL;

    if (size < 2)
      goto not_enough_data;
    stream_lang_idx = gst_asf_demux_get_uint16 (&data, &size);
    if (!gst_asf_demux_get_string (&stream_name, NULL, &data, &size))
      goto not_enough_data;
    GST_INFO ("stream name %d: %s", i, GST_STR_NULL (stream_name));
    g_free (stream_name);       /* TODO: store names in struct */
  }

  /* read payload extension systems stuff */
  GST_LOG ("payload extension systems count = %u", num_payload_ext);

  if (num_payload_ext > 0)
    esp.payload_extensions = g_new0 (AsfPayloadExtension, num_payload_ext + 1);
  else
    esp.payload_extensions = NULL;

  for (i = 0; i < num_payload_ext; ++i) {
    AsfPayloadExtension ext;
    ASFGuid ext_guid;
    guint32 sys_info_len;

    if (size < 16 + 2 + 4)
      goto not_enough_data;

    gst_asf_demux_get_guid (&ext_guid, &data, &size);
    ext.id = gst_asf_demux_identify_guid (asf_payload_ext_guids, &ext_guid);
    ext.len = gst_asf_demux_get_uint16 (&data, &size);

    sys_info_len = gst_asf_demux_get_uint32 (&data, &size);
    GST_LOG ("payload systems info len = %u", sys_info_len);
    if (!gst_asf_demux_skip_bytes (sys_info_len, &data, &size))
      goto not_enough_data;

    esp.payload_extensions[i] = ext;
  }

  GST_LOG ("bytes read: %u/%u", (guint) (data - data_start), obj_size);

  /* there might be an optional STREAM_INFO object here now; if not, we
   * should have parsed the corresponding stream info object already (since
   * we are parsing the extended stream properties objects delayed) */
  if (size == 0) {
    stream = gst_asf_demux_get_stream (demux, stream_num);
    goto done;
  }

  /* get size of the stream object */
  if (!asf_demux_peek_object (demux, data, size, &stream_obj, TRUE))
    goto not_enough_data;

  if (stream_obj.id != ASF_OBJ_STREAM)
    goto expected_stream_object;

  if (stream_obj.size < ASF_OBJECT_HEADER_SIZE ||
      stream_obj.size > (10 * 1024 * 1024))
    goto not_enough_data;

  gst_asf_demux_skip_bytes (ASF_OBJECT_HEADER_SIZE, &data, &size);

  /* process this stream object later after all the other 'normal' ones
   * have been processed (since the others are more important/non-hidden) */
  len = stream_obj.size - ASF_OBJECT_HEADER_SIZE;
  if (!gst_asf_demux_get_bytes (&stream_obj_data, len, &data, &size))
    goto not_enough_data;

  /* parse stream object */
  stream = gst_asf_demux_parse_stream_object (demux, stream_obj_data, len);
  g_free (stream_obj_data);

done:

  if (stream) {
    stream->ext_props = esp;

    /* try to set the framerate */
    if (stream->is_video && stream->caps) {
      GValue framerate = { 0 };
      GstStructure *s;
      gint num, denom;

      g_value_init (&framerate, GST_TYPE_FRACTION);

      num = GST_SECOND / 100;
      denom = esp.avg_time_per_frame;
      if (denom == 0) {
        /* avoid division by 0, assume 25/1 framerate */
        denom = GST_SECOND / 2500;
      }

#ifdef ASFDEMUX_MODIFICATION
      demux->trickplay_info.avg_dur = denom * 100;
#endif

      gst_value_set_fraction (&framerate, num, denom);

      stream->caps = gst_caps_make_writable (stream->caps);
      s = gst_caps_get_structure (stream->caps, 0);
      gst_structure_set_value (s, "framerate", &framerate);
      g_value_unset (&framerate);
      GST_DEBUG_OBJECT (demux, "setting framerate of %d/%d = %f",
          num, denom, ((gdouble) num) / denom);
    }

    /* add language info now if we have it */
    if (stream->ext_props.lang_idx < demux->num_languages) {
      if (stream->pending_tags == NULL)
        stream->pending_tags = gst_tag_list_new ();
      GST_LOG_OBJECT (demux, "stream %u has language '%s'", stream->id,
          demux->languages[stream->ext_props.lang_idx]);
      gst_tag_list_add (stream->pending_tags, GST_TAG_MERGE_APPEND,
          GST_TAG_LANGUAGE_CODE, demux->languages[stream->ext_props.lang_idx],
          NULL);
    }
  } else {
    GST_WARNING_OBJECT (demux, "Ext. stream properties for unknown stream");
  }

  return GST_FLOW_OK;

  /* Errors */
not_enough_data:
  {
    GST_WARNING_OBJECT (demux, "short read parsing ext stream props object!");
    return GST_FLOW_OK;         /* not absolutely fatal */
  }
expected_stream_object:
  {
    GST_WARNING_OBJECT (demux, "error parsing extended stream properties "
        "object: expected embedded stream object, but got %s object instead!",
        gst_asf_get_guid_nick (asf_object_guids, stream_obj.id));
    return GST_FLOW_OK;         /* not absolutely fatal */
  }
}

static const gchar *
gst_asf_demux_push_obj (GstASFDemux * demux, guint32 obj_id)
{
  const gchar *nick;

  nick = gst_asf_get_guid_nick (asf_object_guids, obj_id);
  if (g_str_has_prefix (nick, "ASF_OBJ_"))
    nick += strlen ("ASF_OBJ_");

  if (demux->objpath == NULL) {
    demux->objpath = g_strdup (nick);
  } else {
    gchar *newpath;

    newpath = g_strdup_printf ("%s/%s", demux->objpath, nick);
    g_free (demux->objpath);
    demux->objpath = newpath;
  }

  return (const gchar *) demux->objpath;
}

static void
gst_asf_demux_pop_obj (GstASFDemux * demux)
{
  gchar *s;

  if ((s = g_strrstr (demux->objpath, "/"))) {
    *s = '\0';
  } else {
    g_free (demux->objpath);
    demux->objpath = NULL;
  }
}

static void
gst_asf_demux_process_queued_extended_stream_objects (GstASFDemux * demux)
{
  GSList *l;
  guint i;
#ifdef ASFDEMUX_MODIFICATION
  AsfStream *stream = NULL;
  GST_DEBUG("no of stream is %d", demux->num_streams);
#endif

  /* Parse the queued extended stream property objects and add the info
   * to the existing streams or add the new embedded streams, but without
   * activating them yet */
  GST_LOG_OBJECT (demux, "%u queued extended stream properties objects",
      g_slist_length (demux->ext_stream_props));

#ifdef ASFDEMUX_MODIFICATION
  if(g_slist_length (demux->ext_stream_props) == 0) {
    GST_DEBUG("ext_stream_props is missing");
    for(int i = 0; i < demux->num_streams; i++) {
      stream = gst_asf_demux_get_stream (demux, i+1);
      if (stream && stream->is_video && stream->caps) {
        GST_DEBUG("setting of framerate");
        GValue framerate = { 0 };
        GstStructure *s;
        gint num, denom;

        g_value_init (&framerate, GST_TYPE_FRACTION);

        num = GST_SECOND / 100;
        denom = GST_SECOND / 3000;

        gst_value_set_fraction (&framerate, num, denom);

        stream->caps = gst_caps_make_writable (stream->caps);
        s = gst_caps_get_structure (stream->caps, 0);
        gst_structure_set_value (s, "framerate", &framerate);
        g_value_unset (&framerate);
        GST_DEBUG_OBJECT (demux, "setting framerate of %d/%d = %f",
            num, denom, ((gdouble) num) / denom);
      }
    }
  }
#endif
  for (l = demux->ext_stream_props, i = 0; l != NULL; l = l->next, ++i) {
    GstBuffer *buf = GST_BUFFER (l->data);

    GST_LOG_OBJECT (demux, "parsing ext. stream properties object #%u", i);
    gst_asf_demux_process_ext_stream_props (demux, GST_BUFFER_DATA (buf),
        GST_BUFFER_SIZE (buf));
    gst_buffer_unref (buf);
  }
  g_slist_free (demux->ext_stream_props);
  demux->ext_stream_props = NULL;
}

#if 0
static void
gst_asf_demux_activate_ext_props_streams (GstASFDemux * demux)
{
  guint i, j;

  for (i = 0; i < demux->num_streams; ++i) {
    AsfStream *stream;
    gboolean is_hidden;
    GSList *x;

    stream = &demux->stream[i];

    GST_LOG_OBJECT (demux, "checking  stream %2u", stream->id);

    if (stream->active) {
      GST_LOG_OBJECT (demux, "stream %2u is already activated", stream->id);
      continue;
    }

    is_hidden = FALSE;
    for (x = demux->mut_ex_streams; x != NULL; x = x->next) {
      guint8 *mes;

      /* check for each mutual exclusion whether it affects this stream */
      for (mes = (guint8 *) x->data; mes != NULL && *mes != 0xff; ++mes) {
        if (*mes == stream->id) {
          /* if yes, check if we've already added streams that are mutually
           * exclusive with the stream we're about to add */
          for (mes = (guint8 *) x->data; mes != NULL && *mes != 0xff; ++mes) {
            for (j = 0; j < demux->num_streams; ++j) {
              /* if the broadcast flag is set, assume the hidden streams aren't
               * actually streamed and hide them (or playbin won't work right),
               * otherwise assume their data is available */
              if (demux->stream[j].id == *mes && demux->broadcast) {
                is_hidden = TRUE;
                GST_LOG_OBJECT (demux, "broadcast stream ID %d to be added is "
                    "mutually exclusive with already existing stream ID %d, "
                    "hiding stream", stream->id, demux->stream[j].id);
                goto next;
              }
            }
          }
          break;
        }
      }
    }

  next:

    /* FIXME: we should do stream activation based on preroll data in
     * streaming mode too */
    if (demux->streaming && !is_hidden)
      gst_asf_demux_activate_stream (demux, stream);
  }
}
#endif

static GstFlowReturn
gst_asf_demux_process_object (GstASFDemux * demux, guint8 ** p_data,
    guint64 * p_size)
{
  GstFlowReturn ret = GST_FLOW_OK;
  AsfObject obj;
  guint64 obj_data_size;

  if (*p_size < ASF_OBJECT_HEADER_SIZE)
    return ASF_FLOW_NEED_MORE_DATA;

  asf_demux_peek_object (demux, *p_data, ASF_OBJECT_HEADER_SIZE, &obj, TRUE);
  gst_asf_demux_skip_bytes (ASF_OBJECT_HEADER_SIZE, p_data, p_size);

  obj_data_size = obj.size - ASF_OBJECT_HEADER_SIZE;

  if (*p_size < obj_data_size)
    return ASF_FLOW_NEED_MORE_DATA;

  gst_asf_demux_push_obj (demux, obj.id);

  GST_INFO ("%s: size %" G_GUINT64_FORMAT, demux->objpath, obj.size);

  switch (obj.id) {
    case ASF_OBJ_STREAM:
      gst_asf_demux_parse_stream_object (demux, *p_data, obj_data_size);
      ret = GST_FLOW_OK;
      break;
    case ASF_OBJ_FILE:
      ret = gst_asf_demux_process_file (demux, *p_data, obj_data_size);
      break;
    case ASF_OBJ_HEADER:
      ret = gst_asf_demux_process_header (demux, *p_data, obj_data_size);
      break;
    case ASF_OBJ_COMMENT:
      ret = gst_asf_demux_process_comment (demux, *p_data, obj_data_size);
      break;
    case ASF_OBJ_HEAD1:
      ret = gst_asf_demux_process_header_ext (demux, *p_data, obj_data_size);
      break;
    case ASF_OBJ_BITRATE_PROPS:
      ret =
          gst_asf_demux_process_bitrate_props_object (demux, *p_data,
          obj_data_size);
      break;
    case ASF_OBJ_EXT_CONTENT_DESC:
      ret =
          gst_asf_demux_process_ext_content_desc (demux, *p_data,
          obj_data_size);
      break;
    case ASF_OBJ_METADATA_OBJECT:
      ret = gst_asf_demux_process_metadata (demux, *p_data, obj_data_size);
      break;
    case ASF_OBJ_EXTENDED_STREAM_PROPS:{
      GstBuffer *buf;

      /* process these later, we might not have parsed the corresponding
       * stream object yet */
      GST_LOG ("%s: queued for later parsing", demux->objpath);
      buf = gst_buffer_new_and_alloc (obj_data_size);
      memcpy (GST_BUFFER_DATA (buf), *p_data, obj_data_size);
      demux->ext_stream_props = g_slist_append (demux->ext_stream_props, buf);
      ret = GST_FLOW_OK;
      break;
    }
    case ASF_OBJ_LANGUAGE_LIST:
      ret = gst_asf_demux_process_language_list (demux, *p_data, obj_data_size);
      break;
    case ASF_OBJ_ADVANCED_MUTUAL_EXCLUSION:
      ret = gst_asf_demux_process_advanced_mutual_exclusion (demux, *p_data,
          obj_data_size);
      break;
    case ASF_OBJ_SIMPLE_INDEX:
      ret = gst_asf_demux_process_simple_index (demux, *p_data, obj_data_size);
      break;
    case ASF_OBJ_CONTENT_ENCRYPTION:
    case ASF_OBJ_EXT_CONTENT_ENCRYPTION:
    case ASF_OBJ_DIGITAL_SIGNATURE_OBJECT:
      goto error_encrypted;
    case ASF_OBJ_CODEC_COMMENT:
#ifdef CODEC_ENTRY
      ret = gst_asf_demux_process_codec_list (demux, *p_data, obj_data_size);
      break;
#endif
    case ASF_OBJ_CONCEAL_NONE:
    case ASF_OBJ_HEAD2:
    case ASF_OBJ_UNDEFINED:
    case ASF_OBJ_INDEX:
    case ASF_OBJ_PADDING:
    case ASF_OBJ_BITRATE_MUTEX:
    case ASF_OBJ_COMPATIBILITY:
    case ASF_OBJ_INDEX_PLACEHOLDER:
    case ASF_OBJ_INDEX_PARAMETERS:
    case ASF_OBJ_STREAM_PRIORITIZATION:
    case ASF_OBJ_SCRIPT_COMMAND:
    default:
      /* Unknown/unhandled object, skip it and hope for the best */
      GST_INFO ("%s: skipping object", demux->objpath);
      ret = GST_FLOW_OK;
      break;
  }

  /* this can't fail, we checked the number of bytes available before */
  gst_asf_demux_skip_bytes (obj_data_size, p_data, p_size);

  GST_LOG ("%s: ret = %s", demux->objpath, gst_asf_get_flow_name (ret));

  gst_asf_demux_pop_obj (demux);

  return ret;

/* ERRORS */
error_encrypted:
  {
    GST_ELEMENT_ERROR (demux, STREAM, DECRYPT, (NULL), (NULL));
    return GST_FLOW_ERROR;
  }
}

static void
gst_asf_demux_descramble_buffer (GstASFDemux * demux, AsfStream * stream,
    GstBuffer ** p_buffer)
{
  GstBuffer *descrambled_buffer;
  GstBuffer *scrambled_buffer;
  GstBuffer *sub_buffer;
  guint offset;
  guint off;
  guint row;
  guint col;
  guint idx;

  /* descrambled_buffer is initialised in the first iteration */
  descrambled_buffer = NULL;
  scrambled_buffer = *p_buffer;

  if (GST_BUFFER_SIZE (scrambled_buffer) < demux->ds_packet_size * demux->span)
    return;

  for (offset = 0; offset < GST_BUFFER_SIZE (scrambled_buffer);
      offset += demux->ds_chunk_size) {
    off = offset / demux->ds_chunk_size;
    row = off / demux->span;
    col = off % demux->span;
    idx = row + col * demux->ds_packet_size / demux->ds_chunk_size;
    GST_DEBUG ("idx=%u, row=%u, col=%u, off=%u, ds_chunk_size=%u", idx, row,
        col, off, demux->ds_chunk_size);
    GST_DEBUG ("scrambled buffer size=%u, span=%u, packet_size=%u",
        GST_BUFFER_SIZE (scrambled_buffer), demux->span, demux->ds_packet_size);
    GST_DEBUG ("GST_BUFFER_SIZE (scrambled_buffer) = %u",
        GST_BUFFER_SIZE (scrambled_buffer));
    sub_buffer =
        gst_buffer_create_sub (scrambled_buffer, idx * demux->ds_chunk_size,
        demux->ds_chunk_size);
    if (!offset) {
      descrambled_buffer = sub_buffer;
    } else {
      descrambled_buffer = gst_buffer_join (descrambled_buffer, sub_buffer);
    }
  }

  gst_buffer_copy_metadata (descrambled_buffer, scrambled_buffer,
      GST_BUFFER_COPY_TIMESTAMPS);

  /* FIXME/CHECK: do we need to transfer buffer flags here too? */

  gst_buffer_unref (scrambled_buffer);
  *p_buffer = descrambled_buffer;
}

static gboolean
gst_asf_demux_element_send_event (GstElement * element, GstEvent * event)
{
  GstASFDemux *demux = GST_ASF_DEMUX (element);
  gint i;

  GST_DEBUG ("handling element event of type %s", GST_EVENT_TYPE_NAME (event));

  for (i = 0; i < demux->num_streams; ++i) {
    gst_event_ref (event);
    if (gst_asf_demux_handle_src_event (demux->stream[i].pad, event)) {
      gst_event_unref (event);
      return TRUE;
    }
  }

  gst_event_unref (event);
  return FALSE;
}

/* takes ownership of the passed event */
static gboolean
gst_asf_demux_send_event_unlocked (GstASFDemux * demux, GstEvent * event)
{
  gboolean ret = TRUE;
  gint i;

  GST_DEBUG_OBJECT (demux, "sending %s event to all source pads",
      GST_EVENT_TYPE_NAME (event));

  for (i = 0; i < demux->num_streams; ++i) {
    gst_event_ref (event);
    ret &= gst_pad_push_event (demux->stream[i].pad, event);
  }
  gst_event_unref (event);
  return ret;
}

static const GstQueryType *
gst_asf_demux_get_src_query_types (GstPad * pad)
{
  static const GstQueryType types[] = {
    GST_QUERY_POSITION,
    GST_QUERY_DURATION,
    GST_QUERY_SEEKING,
    0
  };

  return types;
}

static gboolean
gst_asf_demux_handle_src_query (GstPad * pad, GstQuery * query)
{
  GstASFDemux *demux;
  gboolean res = FALSE;

  demux = GST_ASF_DEMUX (gst_pad_get_parent (pad));

  GST_DEBUG ("handling %s query",
      gst_query_type_get_name (GST_QUERY_TYPE (query)));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:
    {
      GstFormat format;

      gst_query_parse_duration (query, &format, NULL);

      if (format != GST_FORMAT_TIME) {
        GST_LOG ("only support duration queries in TIME format");
        break;
      }

      GST_OBJECT_LOCK (demux);

      if (demux->segment.duration != GST_CLOCK_TIME_NONE) {
        GST_LOG ("returning duration: %" GST_TIME_FORMAT,
            GST_TIME_ARGS (demux->segment.duration));

        gst_query_set_duration (query, GST_FORMAT_TIME,
            demux->segment.duration);

        res = TRUE;
      } else {
        GST_LOG ("duration not known yet");
      }

      GST_OBJECT_UNLOCK (demux);
      break;
    }

    case GST_QUERY_POSITION:{
      GstFormat format;

      gst_query_parse_position (query, &format, NULL);

      if (format != GST_FORMAT_TIME) {
        GST_LOG ("only support position queries in TIME format");
        break;
      }

      GST_OBJECT_LOCK (demux);

      if (demux->segment.last_stop != GST_CLOCK_TIME_NONE) {
        GST_LOG ("returning position: %" GST_TIME_FORMAT,
            GST_TIME_ARGS (demux->segment.last_stop));

        gst_query_set_position (query, GST_FORMAT_TIME,
            demux->segment.last_stop);

        res = TRUE;
      } else {
        GST_LOG ("position not known yet");
      }

      GST_OBJECT_UNLOCK (demux);
      break;
    }

    case GST_QUERY_SEEKING:{
      GstFormat format;

      gst_query_parse_seeking (query, &format, NULL, NULL, NULL);
      if (format == GST_FORMAT_TIME) {
        gint64 duration;

        GST_OBJECT_LOCK (demux);
        duration = demux->segment.duration;
        GST_OBJECT_UNLOCK (demux);

        if (!demux->streaming || !demux->seekable) {
          gst_query_set_seeking (query, GST_FORMAT_TIME, demux->seekable, 0,
              duration);
          res = TRUE;
        } else {
          GstFormat fmt;
          gboolean seekable;

          /* try downstream first in TIME */
          res = gst_pad_query_default (pad, query);

          gst_query_parse_seeking (query, &fmt, &seekable, NULL, NULL);
          GST_LOG_OBJECT (demux, "upstream %s seekable %d",
              GST_STR_NULL (gst_format_get_name (fmt)), seekable);
          /* if no luck, maybe in BYTES */
          if (!seekable || fmt != GST_FORMAT_TIME) {
            GstQuery *q;

            q = gst_query_new_seeking (GST_FORMAT_BYTES);
            if ((res = gst_pad_peer_query (demux->sinkpad, q))) {
              gst_query_parse_seeking (q, &fmt, &seekable, NULL, NULL);
              GST_LOG_OBJECT (demux, "upstream %s seekable %d",
                  GST_STR_NULL (gst_format_get_name (fmt)), seekable);
              if (fmt != GST_FORMAT_BYTES)
                seekable = FALSE;
            }
            gst_query_unref (q);
            gst_query_set_seeking (query, GST_FORMAT_TIME, seekable, 0,
                duration);
            res = TRUE;
          }
        }
      } else
        GST_LOG_OBJECT (demux, "only support seeking in TIME format");
      break;
    }

    case GST_QUERY_LATENCY:
    {
      gboolean live;
      GstClockTime min, max;

      /* preroll delay does not matter in non-live pipeline,
       * but we might end up in a live (rtsp) one ... */

      /* first forward */
      res = gst_pad_query_default (pad, query);
      if (!res)
        break;

      gst_query_parse_latency (query, &live, &min, &max);

      GST_DEBUG_OBJECT (demux, "Peer latency: live %d, min %"
          GST_TIME_FORMAT " max %" GST_TIME_FORMAT, live,
          GST_TIME_ARGS (min), GST_TIME_ARGS (max));

      GST_OBJECT_LOCK (demux);
      if (min != -1)
        min += demux->latency;
      if (max != -1)
        max += demux->latency;
      GST_OBJECT_UNLOCK (demux);

      gst_query_set_latency (query, live, min, max);
      break;
    }
    default:
      res = gst_pad_query_default (pad, query);
      break;
  }

  gst_object_unref (demux);
  return res;
}

static GstStateChangeReturn
gst_asf_demux_change_state (GstElement * element, GstStateChange transition)
{
  GstASFDemux *demux = GST_ASF_DEMUX (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:{
      gst_segment_init (&demux->segment, GST_FORMAT_TIME);
      demux->need_newsegment = TRUE;
      demux->segment_running = FALSE;
      demux->accurate = FALSE;
      demux->adapter = gst_adapter_new ();
      demux->metadata = gst_caps_new_empty ();
      demux->global_metadata = gst_structure_empty_new ("metadata");
      demux->data_size = 0;
      demux->data_offset = 0;
      demux->index_offset = 0;
      demux->base_offset = 0;
      break;
    }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_asf_demux_reset (demux, FALSE);
      break;
    default:
      break;
  }

  return ret;
}


#ifdef ASFDEMUX_MODIFICATION
static gboolean
gst_asf_demux_forward_trickplay (GstASFDemux * demux, AsfStream *stream, AsfPayload *payload)
{
  gboolean ret = FALSE;

  if ((payload->keyframe || (!payload->keyframe && (payload->ts > demux->segment.start)))  &&
    !demux->trickplay_info.prev_kpacket && !demux->trickplay_info.show_samples) {
    AsfPayload *next_kpayload = NULL;
    guint32 nsamples = 0; /* Number of samples between two consecutive keyframes */

    demux->trickplay_info.prev_kpacket = demux->packet;
    demux->trickplay_info.prev_keyframe = payload->mo_number;
    demux->trickplay_info.current_frame = payload->mo_number;

    GST_DEBUG_OBJECT (demux, "Received keyframe : ts = %"GST_TIME_FORMAT", dur = %"GST_TIME_FORMAT,
	  GST_TIME_ARGS(payload->ts), GST_TIME_ARGS(payload->duration));

    while (nsamples == 0) {
      ret = gst_asf_demux_find_next_keypacket (demux);
      if (ret == FALSE) {
        GST_ERROR_OBJECT(demux, "Error in finding frames between keyframes...");
        stream->last_flow = GST_FLOW_ERROR;
        goto exit;
      }

      ret = gst_asf_demux_find_keypayload_from_packet (demux, &next_kpayload);
      if (ret == FALSE || next_kpayload == NULL) {
        GST_ERROR_OBJECT(demux, "Error in finding frames between keyframes...");
        stream->last_flow = GST_FLOW_ERROR;
        goto exit;
      }
  
      nsamples = gst_asf_demux_find_num_samples_bw_kpayloads (demux, payload, next_kpayload);
      if (next_kpayload) {
        g_free (next_kpayload);
        next_kpayload = NULL;
      }
    }

    demux->trickplay_info.kpacket_dur_diff = demux->trickplay_info.avg_dur;
    demux->trickplay_info.show_samples = nsamples / (guint32) demux->segment.rate;
    demux->trickplay_info.start_ts = payload->ts;

    GST_DEBUG_OBJECT (demux, "samples to show between key frames = %d", demux->trickplay_info.show_samples);

    /* Alter the next video sample timestamp */
    if (stream->payloads->len > 1) {
      AsfPayload *next_payload = NULL;

      /* Now see if there's a complete payload queued for this stream */
      next_payload = &g_array_index (stream->payloads, AsfPayload, 1);
      if (gst_asf_payload_is_complete (next_payload)) {
        GST_DEBUG_OBJECT (demux, "Modifying ts from %"GST_TIME_FORMAT, GST_TIME_ARGS(next_payload->ts));
        ++demux->trickplay_info.current_frame;
        next_payload->ts = demux->trickplay_info.start_ts +
           (GstClockTime)(demux->trickplay_info.kpacket_dur_diff * (demux->segment.rate * (demux->trickplay_info.current_frame - demux->trickplay_info.prev_keyframe)));
        GST_DEBUG_OBJECT (demux, "to %"GST_TIME_FORMAT, GST_TIME_ARGS(next_payload->ts));
      }
    }
  } else if ((demux->trickplay_info.prev_kpacket == 0) && (demux->trickplay_info.show_samples == 0)) {
    GST_WARNING_OBJECT (demux, "expecting keyframe payload...Received '%s' with mo_num = %u",
	    payload->keyframe ? "key" : "non-key", payload->mo_number);
    stream->last_flow = GST_FLOW_OK;
    goto exit;
  } else {
    GstClockTime ts = GST_CLOCK_TIME_NONE;

    ++demux->trickplay_info.current_frame;
    ts = demux->trickplay_info.start_ts + 
      (GstClockTime)(demux->trickplay_info.kpacket_dur_diff * (demux->segment.rate * (demux->trickplay_info.current_frame - demux->trickplay_info.prev_keyframe)));

    if (ts != payload->ts) {
      payload->ts = ts;

      GST_LOG_OBJECT (demux, "Modifying payload->ts now...");
       /* Alter the next video sample timestamp */
      if (stream->payloads->len > 1) {
        AsfPayload *next_payload = NULL;  

        /* Now see if there's a complete payload queued for this stream */
        next_payload = &g_array_index (stream->payloads, AsfPayload, 1);
        if (gst_asf_payload_is_complete (next_payload)) {
          GST_DEBUG_OBJECT (demux, "Modifying ts from %"GST_TIME_FORMAT, GST_TIME_ARGS(next_payload->ts));
          ++demux->trickplay_info.current_frame;
          next_payload->ts = demux->trickplay_info.start_ts + 
            (GstClockTime)(demux->trickplay_info.kpacket_dur_diff * (demux->segment.rate * (demux->trickplay_info.current_frame - demux->trickplay_info.prev_keyframe)));
          GST_DEBUG_OBJECT (demux, "to %"GST_TIME_FORMAT, GST_TIME_ARGS(next_payload->ts));
        }
      }
    } else {
      /* Alter the next video sample timestamp */
      if (stream->payloads->len > 1) {
        AsfPayload *next_payload = NULL;

        /* Now see if there's a complete payload queued for this stream */
        next_payload = &g_array_index (stream->payloads, AsfPayload, 1);
        if (gst_asf_payload_is_complete (next_payload)) {
          next_payload->ts = ts;
        }
      } 
    }

    if (demux->trickplay_info.show_samples == 0) {
      /* we are done with present key frame check.....jump to next key frame */
      GST_INFO_OBJECT (demux, "Displayed all frames in present keyframe chunk...jumping to next keyframe : %u", demux->trickplay_info.next_kpacket);
      GST_OBJECT_LOCK (demux);
      gst_asf_demux_reset_stream_state_after_discont (demux);
      demux->packet = demux->trickplay_info.next_kpacket;
      demux->packet--;
      demux->trickplay_info.prev_kpacket = 0;
      demux->firstVidBufSent = FALSE;
      demux->firstAudBufSent = FALSE;
      GST_OBJECT_UNLOCK (demux);
      stream->last_flow = GST_FLOW_OK;
      goto exit;
    }
    GST_DEBUG_OBJECT (demux, "samples to show between keyframes = %u", demux->trickplay_info.show_samples);
    demux->trickplay_info.show_samples--;
  }

  stream->last_flow = GST_FLOW_OK;
  return TRUE;

exit:
  return FALSE;
}

static gboolean
gst_asf_demux_backward_trickplay(GstASFDemux * demux, AsfStream *stream, AsfPayload *payload)
{
  gboolean ret = FALSE;

  if (payload->keyframe || (payload->ts >= demux->segment.stop) || demux->firstVidBufSent == FALSE) {
    if(payload->ts >= demux->trickplay_info.next_keyframe_ts) {
	  GST_DEBUG_OBJECT (demux, "Received keyframe...");
	  GST_INFO("next_keyframe_ts is %"GST_TIME_FORMAT" payload time-stamp is %"GST_TIME_FORMAT,
	    GST_TIME_ARGS(demux->trickplay_info.next_keyframe_ts), GST_TIME_ARGS(payload->ts));

      if (demux->trickplay_info.is_eos == TRUE) {
        /* Reached EOS in trickplay, start normal playback from beginning */
		/* To avoid the case where packet has multiple key payloads */
        GST_INFO_OBJECT (demux, "Received EOS.. Sending new_segment with rate = 1.000");
        demux->trickplay_info.is_eos = FALSE;
        demux->trickplay_info.first_video_push = FALSE;
        demux->trickplay_info.resume_nrl_play = TRUE;
        demux->trickplay_info.first_payload = FALSE;

        GST_OBJECT_LOCK (demux);
        demux->packet = -1;
        demux->trickplay_info.prev_kpacket = -1;
        gst_buffer_replace (&payload->buf, NULL);

        if (stream->payloads->len > 0) {
          g_array_remove_index (stream->payloads, 0);
        }
        gst_asf_demux_reset_stream_state_after_discont (demux);
        demux->firstVidBufSent = FALSE;
        demux->firstAudBufSent = FALSE;
        GST_OBJECT_UNLOCK (demux);

        stream->last_flow = GST_FLOW_OK;
        goto exit;
      }

      if (demux->trickplay_info.prev_kpacket != -1) {
        GST_INFO_OBJECT (demux, "End of current segment.. Jump to previous key frame = %d..", demux->trickplay_info.prev_kpacket);

        GST_OBJECT_LOCK (demux);
        demux->trickplay_info.next_kpacket = demux->trickplay_info.cur_kpacket;
        demux->packet = demux->trickplay_info.prev_kpacket;
		demux->trickplay_info.next_keyframe_ts = demux->trickplay_info.cur_keyframe_ts;
		GST_INFO("Setting of next_keyframe_ts in end of segment %"GST_TIME_FORMAT,GST_TIME_ARGS(demux->trickplay_info.next_keyframe_ts));
		demux->trickplay_info.first_video_push = FALSE;
		demux->trickplay_info.first_payload = FALSE;
        demux->packet--;
        demux->trickplay_info.prev_kpacket = -1;
        gst_asf_demux_reset_stream_state_after_discont (demux);
        demux->firstVidBufSent = FALSE;
        demux->firstAudBufSent = FALSE;
        GST_OBJECT_UNLOCK (demux);

        stream->last_flow = GST_FLOW_OK;
        goto exit;
      }

      /* Finiding the previous key frame */
      GST_DEBUG ("Present key packet number = %"G_GINT64_FORMAT, demux->packet);
      demux->trickplay_info.cur_kpacket = demux->packet;
      demux->trickplay_info.cur_keyframe_ts = payload->ts;  //to capture the current key-frame time stamp.
	  GST_INFO("Time stamp of cur_keyframe_ts is %"GST_TIME_FORMAT" payload time stamps is %"GST_TIME_FORMAT,
	    GST_TIME_ARGS(demux->trickplay_info.cur_keyframe_ts),GST_TIME_ARGS(payload->ts));

      ret = gst_asf_demux_find_prev_keypacket (demux);
      if (ret == FALSE) {
        GST_ERROR_OBJECT(demux, "Error in finding previous keyframe...");
        stream->last_flow = GST_FLOW_ERROR;
        goto exit;
      }
	}

	GST_DEBUG ("Present key packet number = %"G_GINT64_FORMAT, demux->packet);
    demux->trickplay_info.cur_kpacket = demux->packet;
    demux->trickplay_info.cur_keyframe_ts = payload->ts;  //to capture the current key-frame time stamp.
	GST_INFO("Time stamp of cur_keyframe_ts is %"GST_TIME_FORMAT" payload time stamps is %"GST_TIME_FORMAT,
	  GST_TIME_ARGS(demux->trickplay_info.cur_keyframe_ts),GST_TIME_ARGS(payload->ts));

	if((payload->keyframe) && (stream->is_video) &&
	    (demux->trickplay_info.first_video_push) && (payload->ts < demux->trickplay_info.next_keyframe_ts)) {
	  GST_DEBUG_OBJECT (demux, "Already sent key frame, forcibly unreffing current video-keyframe...");
	  stream->last_flow = GST_FLOW_OK;
	  goto exit;
	}

  } else {
    if (-1 == demux->trickplay_info.prev_kpacket) {
      GST_DEBUG ("Expecting Key frame.. Unreffing current NON-Keyframe with mo_num = %d", payload->mo_number);
      demux->firstVidBufSent = FALSE;
      stream->last_flow = GST_FLOW_OK;
      goto exit;
    } else {
      GST_DEBUG_OBJECT (demux, "Already sent key frame, unreffing current non-keyframe...");
      stream->last_flow = GST_FLOW_OK;
	  goto exit;
    }
  }

  if(!demux->trickplay_info.first_video_push && stream->is_video && payload->keyframe) {
    demux->trickplay_info.first_video_push = TRUE;
  }
  stream->last_flow = GST_FLOW_OK;
  return TRUE;

exit:
  return FALSE;
}


static gboolean 
gst_asf_demux_find_next_keypacket (GstASFDemux * demux)
{
  guint idx = 0;

  if (G_UNLIKELY (demux->sidx_num_entries == 0 || demux->sidx_interval == 0)) {
    /* need to post error saying that.. trick play will not work*/
    GST_ERROR_OBJECT (demux, "No proper index table....");
    return FALSE;
  }

  /* past key frame idx */
  idx = demux->seek_idx;
  idx++;
  
  /* FIXME: seek beyond end of file should result in immediate EOS from
   * streaming thread instead of a failed seek */
  if (G_UNLIKELY (idx >= demux->sidx_num_entries)) {
    GST_INFO_OBJECT (demux, "No indexes left..... EOS will come");
    goto eos;
  }

  /* find next key frame idx */
  while (demux->sidx_entries[idx].packet == demux->sidx_entries[demux->seek_idx].packet) {
    GST_DEBUG_OBJECT (demux, "sidx_entries[%d].packet = %d....", idx, demux->sidx_entries[idx].packet);
    idx++;
    if (G_UNLIKELY (idx >= demux->sidx_num_entries)) {
      GST_INFO_OBJECT (demux, "No indexes left..... EOS will come");
      goto eos;
    }
  }

  demux->seek_idx = idx;
  demux->trickplay_info.next_kpacket = demux->sidx_entries[idx].packet;
  GST_INFO_OBJECT (demux, "next key packet = %d\n", demux->trickplay_info.next_kpacket);

  return TRUE;

eos:
  {
    /* No key frames left... taking end frame as keyframe*/
    GST_WARNING_OBJECT (demux, "No key frames left.. taking last frame as key frame");
    demux->trickplay_info.next_kpacket = demux->num_packets - 1;
    demux->seek_idx = demux->sidx_num_entries;
    demux->trickplay_info.is_eos = TRUE;
    return TRUE;
  }
}

static gboolean 
gst_asf_demux_find_prev_keypacket (GstASFDemux * demux)
{
  guint idx = 0;

  if (G_UNLIKELY (demux->sidx_num_entries == 0 || demux->sidx_interval == 0)) {
    /* need to post error saying that.. trick play will not work*/
    GST_ERROR_OBJECT (demux, "No proper index table....");
    return FALSE;
  }

  /* past key frame idx */
  idx = demux->seek_idx;

  /* find next key frame idx */
  while (demux->sidx_entries[idx].packet == demux->sidx_entries[demux->seek_idx].packet) {
    GST_DEBUG_OBJECT (demux, "sidx_entries[%d].packet = %d....", idx, demux->sidx_entries[idx].packet);
    idx--;
    if (G_UNLIKELY (idx == 0)) {
      GST_WARNING_OBJECT (demux, "No indexes left..... reached start");
      goto eos;
    }
  }

  demux->seek_idx = idx;
  demux->trickplay_info.prev_kpacket = demux->sidx_entries[idx].packet;
  demux->trickplay_info.first_video_push = FALSE;
  GST_INFO_OBJECT (demux, "prev key packet = %d to curent key keypacket", demux->trickplay_info.prev_kpacket);

  return TRUE;

eos:
  {
    /* No key frames left... taking first frame as keyframe*/
    GST_WARNING_OBJECT (demux, "No key frames left.. taking first frame as key frame");
    demux->trickplay_info.prev_kpacket = 0;
    demux->trickplay_info.first_video_push = FALSE;
    demux->seek_idx = 0;
    demux->trickplay_info.is_eos = TRUE;
    return TRUE;
  }
}


static guint32
gst_asf_demux_find_num_samples_bw_kpayloads (GstASFDemux * demux, AsfPayload *start_kpayload, AsfPayload *end_kpayload)
{
  gint32 approx_frames_bw_kframes = 0;
  gint32 diff_bw_keyframes = 0;
  guint32 i = 0;

  if (!demux->trickplay_info.avg_dur) {
    demux->trickplay_info.avg_dur = GST_SECOND/25;
    GST_INFO_OBJECT (demux, "Making Avg Duration as %"GST_TIME_FORMAT, GST_TIME_ARGS(demux->trickplay_info.avg_dur));
  }

  GST_LOG_OBJECT (demux, "start->ts = %"GST_TIME_FORMAT" and end->ts = %"GST_TIME_FORMAT,
    GST_TIME_ARGS(start_kpayload->ts), GST_TIME_ARGS(end_kpayload->ts));

  GST_DEBUG_OBJECT (demux, "duration bw key frames using payload time = %"GST_TIME_FORMAT"and avg duration = %"GST_TIME_FORMAT, 
    GST_TIME_ARGS(end_kpayload->ts  - start_kpayload->ts), GST_TIME_ARGS(demux->trickplay_info.avg_dur));

  approx_frames_bw_kframes = (gint32)((end_kpayload->ts  - start_kpayload->ts) /demux->trickplay_info.avg_dur);

  GST_INFO_OBJECT (demux, "approximate num of frames bw key frames = %d", approx_frames_bw_kframes);

  diff_bw_keyframes = end_kpayload->mo_number - start_kpayload->mo_number;

  while (approx_frames_bw_kframes > diff_bw_keyframes) {
    ++i;
    diff_bw_keyframes = (end_kpayload->mo_number + ( i * demux->trickplay_info.mo_len_type) - start_kpayload->mo_number);
  }

  // this is trade-off condition
  if (diff_bw_keyframes > (approx_frames_bw_kframes + (demux->trickplay_info.mo_len_type>>1))) {
    GST_INFO_OBJECT (demux, "Special condition in finding difference between keyframes...");
    --i;
  }
  
  diff_bw_keyframes = (end_kpayload->mo_number + ( i * demux->trickplay_info.mo_len_type) - start_kpayload->mo_number);
  GST_INFO_OBJECT (demux, "Difference between key frames from Obj Num = %d", diff_bw_keyframes);

  /*at present taking approximate num of frames based on duration and timestamps */
  return diff_bw_keyframes;

}

static gboolean 
gst_asf_demux_find_keypayload_from_packet (GstASFDemux * demux, AsfPayload **keypayload)
{
  guint packet_num = 0;
  guint64 off = 0;
  GstBuffer *buf = NULL;
  GstFlowReturn flow = GST_FLOW_OK;
  const guint8 *data = NULL;
  guint size = 0;
  AsfPacket packet = {0, };
  guint8 ec_flags, flags1;
  gboolean has_multiple_payloads;
  gboolean ret = FALSE;
  gboolean is_video = FALSE;
  AsfPayload *cur_payload = NULL;

pull_again:
  if (*keypayload) {
    g_free (*keypayload);
    *keypayload = NULL;
  }

  if (demux->trickplay_info.is_eos == FALSE)
    packet_num = demux->sidx_entries[demux->seek_idx].packet;
  else
    packet_num = demux->trickplay_info.next_kpacket;

  off = demux->data_offset + (packet_num * demux->packet_size);

  GST_DEBUG_OBJECT (demux, "for finding next keypayload :: pulling buffer at %" G_GUINT64_FORMAT "+%u", off, demux->packet_size * demux->speed_packets);

  if (G_UNLIKELY (!gst_asf_demux_pull_data (demux, off, demux->packet_size * demux->speed_packets, &buf, &flow)) || buf == NULL) {
    GST_ERROR_OBJECT (demux, "pull data flow return : %s", gst_flow_get_name (flow));
    return FALSE;
  }

  data = GST_BUFFER_DATA (buf);
  size = GST_BUFFER_SIZE (buf);

  packet.buf = buf;
  
  ec_flags = GST_READ_UINT8 (data);

  /* skip optional error correction stuff */
  if ((ec_flags & 0x80) != 0) {
    guint ec_len_type, ec_len;

    ec_len_type = (ec_flags & 0x60) >> 5;
    if (ec_len_type == 0) {
      ec_len = ec_flags & 0x0f;
    } else {
      GST_WARNING_OBJECT (demux, "unexpected error correction length type %u", ec_len_type);
      ec_len = 2;
    }
	
    GST_DEBUG_OBJECT (demux, "packet has error correction (%u bytes)", ec_len);

    /* still need at least two payload flag bytes, send time, and duration */
    if (size <= (1 + ec_len) + 2 + 4 + 2) {
      GST_ERROR_OBJECT (demux, "EC :: short packet.....");
      goto short_packet;
    }
    data += 1 + ec_len;
    size -= 1 + ec_len;
  }

 /* parse payload info */
  flags1 = GST_READ_UINT8 (data);
  packet.prop_flags = GST_READ_UINT8 (data + 1);

  switch (packet.prop_flags && 0x30) {
    case 0:
      GST_ERROR_OBJECT (demux, "The Media Object Number Length field does not exist...");
      goto error;
    case 1:
      GST_INFO_OBJECT (demux, "The Media Object Number Length field is coded using a BYTE");
      demux->trickplay_info.mo_len_type = 256;
      break;
    case 2:
      GST_INFO_OBJECT (demux, "The Media Object Number Length field is coded using a WORD");
      demux->trickplay_info.mo_len_type = 65536;
      break;
    case 3:
      GST_INFO_OBJECT (demux, "The Media Object Number Length field is coded using a DWORD");
      demux->trickplay_info.mo_len_type =  4294967296;
      break;
    default:
      GST_ERROR_OBJECT (demux, "Wrong length type...");
      goto error;
  }
  data += 2;
  size -= 2;
  
  has_multiple_payloads = (flags1 & 0x01) != 0;

  packet.length = asf_packet_read_varlen_int (flags1, 5, &data, &size);
  packet.sequence = asf_packet_read_varlen_int (flags1, 1, &data, &size);
  packet.padding = asf_packet_read_varlen_int (flags1, 3, &data, &size);

  if (G_UNLIKELY (size < 6)) {
    GST_ERROR_OBJECT (demux, "short packet.....");
    goto short_packet;
  }

  packet.send_time = GST_READ_UINT32_LE (data) * GST_MSECOND;
  packet.duration = GST_READ_UINT16_LE (data + 4) * GST_MSECOND;

  GST_DEBUG_OBJECT (demux, "multiple payloads: %u", has_multiple_payloads);
  GST_DEBUG_OBJECT (demux, "packet length    : %u", packet.length);
  GST_DEBUG_OBJECT (demux, "sequence         : %u", packet.sequence);
  GST_DEBUG_OBJECT (demux, "padding          : %u", packet.padding);
  GST_DEBUG_OBJECT (demux, "send time        : %" GST_TIME_FORMAT,
      GST_TIME_ARGS (packet.send_time));
  GST_DEBUG_OBJECT (demux, "duration         : %" GST_TIME_FORMAT,
      GST_TIME_ARGS (packet.duration));

  data += 4 + 2;
  size -= 4 + 2;

  if (G_UNLIKELY (packet.padding == (guint) -1 || size < packet.padding)) {
    GST_ERROR_OBJECT (demux, "padding :: short packet.....");
    goto short_packet;
  }
  
  size -= packet.padding;

  if (has_multiple_payloads) {
    guint num, lentype;
    gint i = 0;
	
    if (G_UNLIKELY (size < 1))
      goto short_packet;

    num = (GST_READ_UINT8 (data) & 0x3F) >> 0;
    lentype = (GST_READ_UINT8 (data) & 0xC0) >> 6;

    ++data;
    --size;

    GST_DEBUG_OBJECT (demux, "num payloads     : %u", num);
   
    for (i = 0; i < num; ++i) {
      GST_DEBUG_OBJECT (demux, "Parsing payload %u/%u", i + 1, num);

      cur_payload = (AsfPayload *) g_malloc0 (sizeof (AsfPayload));
      if (NULL == cur_payload) {
        GST_ERROR_OBJECT (demux, "Failed to allocate memory...");
        goto error;
      }

      ret = gst_asf_demux_parse_payload_custom (demux, &packet, lentype, &data, &size, cur_payload, &is_video);
      if (G_UNLIKELY (!ret)) {
        GST_ERROR_OBJECT (demux, "Failed to parse payload %u/%u", i + 1, num);
        goto error;
      }

      if (cur_payload->keyframe) {
        GST_DEBUG_OBJECT (demux, "Found key frame pay load...");
        if (*keypayload) {
          g_free (*keypayload);
        }
        *keypayload = cur_payload;
        cur_payload = NULL;
      } else if (demux->trickplay_info.is_eos && is_video) {
        GST_DEBUG_OBJECT (demux, "on eos, received video payload...");
        if (*keypayload) {
          g_free (*keypayload);
        }
        *keypayload = cur_payload;
        cur_payload = NULL;
      } else {
        g_free (cur_payload);
        cur_payload = NULL;
      }
    }
  } else {
    cur_payload = (AsfPayload *) g_malloc0 (sizeof (AsfPayload));
    if (NULL == cur_payload) {
      GST_ERROR_OBJECT (demux, "Failed to allocate memory...");
      return FALSE;
    }

    GST_LOG_OBJECT (demux, "Parsing single payload");
    ret = gst_asf_demux_parse_payload_custom (demux, &packet, -1, &data, &size, cur_payload, &is_video);
    if (G_UNLIKELY (!ret)) {
      GST_ERROR_OBJECT (demux, "Failed to parse single payload");
      goto error;
    }

    if (cur_payload->keyframe) {
      GST_DEBUG_OBJECT (demux, "Found key frame pay load...");
      if (*keypayload) {
        g_free (*keypayload);
      }
      *keypayload = cur_payload;
      cur_payload = NULL;
    } else if (demux->trickplay_info.is_eos && is_video) {
      GST_DEBUG_OBJECT (demux, "on eos, received video payload...");
      if (*keypayload) {
        g_free (*keypayload);
      }
      *keypayload = cur_payload;
      cur_payload = NULL;
    } else {
      g_free (cur_payload);
      cur_payload = NULL;
    }
  }

  if (*keypayload == NULL) {
    if (demux->trickplay_info.is_eos) {
      GST_INFO_OBJECT (demux, "Key frame not found, pull again...");
      if (packet.buf) {
        gst_buffer_unref (packet.buf);
        packet.buf = NULL; 
      }
      demux->trickplay_info.next_kpacket--;
      goto pull_again;
    } else {
      GST_ERROR_OBJECT (demux, "Key frame not found... not a proper packet");
      goto error;
    }
  }

  if (packet.buf) {
    gst_buffer_unref (packet.buf);
    packet.buf = NULL; 
  }

  return TRUE;
  
short_packet:
  {
    GST_WARNING_OBJECT (demux, "Short packet!");
    if (packet.buf) {
	  gst_buffer_unref (packet.buf);
	  packet.buf = NULL;
    }
	*keypayload = NULL;
    return FALSE;
  }
error:
  {
    if (packet.buf) {
      gst_buffer_unref (packet.buf);
      packet.buf = NULL;
    }
	*keypayload = NULL;
    return FALSE;
  }
}

static gboolean
gst_asf_demux_parse_payload_custom (GstASFDemux * demux, AsfPacket * packet, gint lentype, const guint8 ** p_data, 
      guint *p_size, AsfPayload *payload, gboolean *is_video)
{
  AsfStream *stream;
  gboolean is_compressed;
  guint payload_len;
  guint stream_num;

  if (G_UNLIKELY (*p_size < 1)) {
    GST_WARNING_OBJECT (demux, "Short packet!");
    return FALSE;
  }
 
  stream_num = GST_READ_UINT8 (*p_data) & 0x7f;
  payload->keyframe = ((GST_READ_UINT8 (*p_data) & 0x80) != 0);
  
  *p_data += 1;
  *p_size -= 1;

  payload->ts = GST_CLOCK_TIME_NONE;
  payload->duration = GST_CLOCK_TIME_NONE;
  payload->par_x = 0;
  payload->par_y = 0;
  payload->interlaced = FALSE;
  payload->tff = FALSE;
  payload->rff = FALSE;

  payload->mo_number =
      asf_packet_read_varlen_int (packet->prop_flags, 4, p_data, p_size);
  payload->mo_offset =
      asf_packet_read_varlen_int (packet->prop_flags, 2, p_data, p_size);
  payload->rep_data_len =
      asf_packet_read_varlen_int (packet->prop_flags, 0, p_data, p_size);

  is_compressed = (payload->rep_data_len == 1);

  GST_DEBUG_OBJECT (demux, "payload for stream %u", stream_num);
  GST_DEBUG_OBJECT (demux, "(stream -%d) payload media obj num %u", stream_num, payload->mo_number);
  GST_DEBUG_OBJECT (demux, "keyframe   : %s", (payload->keyframe) ? "yes" : "no");
  GST_DEBUG_OBJECT (demux, "compressed : %s", (is_compressed) ? "yes" : "no");

  if (G_UNLIKELY (*p_size < payload->rep_data_len)) {
    GST_ERROR_OBJECT (demux, "Short packet! rep_data_len=%u, size=%u",
        payload->rep_data_len, *p_size);
    return FALSE;
  }

  memcpy (payload->rep_data, *p_data,
      MIN (sizeof (payload->rep_data), payload->rep_data_len));
  
  *p_data += payload->rep_data_len;
  *p_size -= payload->rep_data_len;

  if (G_UNLIKELY (*p_size == 0)) {
    GST_WARNING_OBJECT (demux, "payload without data!?");
    return FALSE;
  }

  /* we use -1 as lentype for a single payload that's the size of the packet */
  if (G_UNLIKELY ((lentype >= 0 && lentype <= 3))) {
    payload_len = asf_packet_read_varlen_int (lentype, 0, p_data, p_size);
    if (*p_size < payload_len) {
      GST_WARNING_OBJECT (demux, "Short packet! payload_len=%u, size=%u",
          payload_len, *p_size);
      return FALSE;
    }
  } else {
    payload_len = *p_size;
  }

  GST_DEBUG_OBJECT (demux, "payload length: %u", payload_len);

  stream = gst_asf_demux_get_stream (demux, stream_num);
  if (G_UNLIKELY (stream == NULL)) {
    GST_ERROR_OBJECT (demux, "Payload for unknown stream %u, skipping",
        stream_num);
    *p_data += payload_len;
    *p_size -= payload_len;
    return TRUE;
  }

  if (stream->is_video)
    *is_video = TRUE;

  if (G_UNLIKELY (!is_compressed)) {
    GST_DEBUG_OBJECT (demux, "replicated data length: %u", payload->rep_data_len);

    if (payload->rep_data_len >= 8) {
      payload->mo_size = GST_READ_UINT32_LE (payload->rep_data);
      payload->ts = GST_READ_UINT32_LE (payload->rep_data + 4) * GST_MSECOND;
      payload->ts -= demux->trickplay_info.tp_preroll;
      asf_payload_parse_replicated_data_extensions (stream, payload);

      GST_DEBUG_OBJECT (demux, "media object size   : %u", payload->mo_size);
      GST_DEBUG_OBJECT (demux, "media object ts     : %" GST_TIME_FORMAT,
          GST_TIME_ARGS (payload->ts));
      GST_DEBUG_OBJECT (demux, "media object dur    : %" GST_TIME_FORMAT,
          GST_TIME_ARGS (payload->duration));
	  *p_data += payload_len;
      *p_size -= payload_len;
    } else if (payload->rep_data_len != 0) {
      GST_ERROR_OBJECT (demux, "invalid replicated data length, very bad");
      *p_data += payload_len;
      *p_size -= payload_len;
      return FALSE;
    }

    GST_DEBUG_OBJECT (demux, "media object offset : %u", payload->mo_offset);
    GST_DEBUG_OBJECT (demux, "payload length: %u", payload_len);
  } else {
    const guint8 *payload_data;

    GST_LOG_OBJECT (demux, "Compressed payload, length=%u", payload_len);

    payload_data = *p_data;

    *p_data += payload_len;
    *p_size -= payload_len;
  }
  
  return TRUE;
}

static gboolean
gst_asf_demux_index_table_parse_packet (GstASFDemux * demux, GstBuffer * buf, GQueue *sidx_queue, gint *packet_num)
{
  AsfPacket packet = { 0, };
  const guint8 *data;
  AsfKeyPacketInfo *keyinfo = NULL;
  gboolean has_multiple_payloads;
  gboolean ret = TRUE;
  gboolean sret = TRUE;
  guint8 ec_flags, flags1;
  guint size;

  if(buf == NULL) {
    GST_INFO("Invalid Buffer");
	return FALSE;
  }
  data = GST_BUFFER_DATA (buf);
  size = GST_BUFFER_SIZE (buf);
  GST_LOG_OBJECT (demux, "Buffer size: %u", size);

  /* need at least two payload flag bytes, send time, and duration */
  if (G_UNLIKELY (size < 2 + 4 + 2))
    goto short_packet;

  packet.buf = buf;

  ec_flags = GST_READ_UINT8 (data);

  /* skip optional error correction stuff */
  if ((ec_flags & 0x80) != 0) {
    guint ec_len_type, ec_len;

    ec_len_type = (ec_flags & 0x60) >> 5;
    if (ec_len_type == 0) {
      ec_len = ec_flags & 0x0f;
    } else {
      GST_WARNING_OBJECT (demux, "unexpected error correction length type %u",
          ec_len_type);
      ec_len = 2;
    }
    GST_LOG_OBJECT (demux, "packet has error correction (%u bytes)", ec_len);

    /* still need at least two payload flag bytes, send time, and duration */
    if (size <= (1 + ec_len) + 2 + 4 + 2)
      goto short_packet;

    data += 1 + ec_len;
    size -= 1 + ec_len;
  }

  /* parse payload info */
  flags1 = GST_READ_UINT8 (data);
  packet.prop_flags = GST_READ_UINT8 (data + 1);

  data += 2;
  size -= 2;

  has_multiple_payloads = (flags1 & 0x01) != 0;

  packet.length = asf_packet_read_varlen_int (flags1, 5, &data, &size);

  packet.sequence = asf_packet_read_varlen_int (flags1, 1, &data, &size);

  packet.padding = asf_packet_read_varlen_int (flags1, 3, &data, &size);

  if (G_UNLIKELY (size < 6))
    goto short_packet;

  packet.send_time = GST_READ_UINT32_LE (data) * GST_MSECOND;
  packet.duration = GST_READ_UINT16_LE (data + 4) * GST_MSECOND;

  data += 4 + 2;
  size -= 4 + 2;

  GST_LOG_OBJECT (demux, "flags            : 0x%x", flags1);
  GST_LOG_OBJECT (demux, "multiple payloads: %u", has_multiple_payloads);
  GST_LOG_OBJECT (demux, "packet length    : %u", packet.length);
  GST_LOG_OBJECT (demux, "sequence         : %u", packet.sequence);
  GST_LOG_OBJECT (demux, "padding          : %u", packet.padding);
  GST_LOG_OBJECT (demux, "send time        : %" GST_TIME_FORMAT,
      GST_TIME_ARGS (packet.send_time));
  GST_LOG_OBJECT (demux, "duration         : %" GST_TIME_FORMAT,
      GST_TIME_ARGS (packet.duration));

  if (G_UNLIKELY (packet.padding == (guint) - 1 || size < packet.padding))
    goto short_packet;

  size -= packet.padding;

  /* adjust available size for parsing if there's less actual packet data for
   * parsing than there is data in bytes (for sample see bug 431318) */
  if (G_UNLIKELY (packet.length != 0 && packet.padding == 0
          && packet.length < demux->packet_size)) {
    GST_LOG_OBJECT (demux, "shortened packet with implicit padding, "
        "adjusting available data size");
    if (size < demux->packet_size - packet.length) {
      /* the buffer is smaller than the implicit padding */
      goto short_packet;
    } else {
      /* subtract the implicit padding */
      size -= (demux->packet_size - packet.length);
    }
  }

  if (has_multiple_payloads) {
    guint i, num, lentype;

    if (G_UNLIKELY (size < 1))
      goto short_packet;

    num = (GST_READ_UINT8 (data) & 0x3F) >> 0;
    lentype = (GST_READ_UINT8 (data) & 0xC0) >> 6;

    ++data;
    --size;

    GST_LOG_OBJECT (demux, "num payloads     : %u", num);

    for (i = 0; i < num; ++i) {
      GST_LOG_OBJECT (demux, "Parsing payload %u/%u, size left: %u", i + 1, num, size);
      keyinfo = (AsfKeyPacketInfo *) malloc (sizeof (AsfKeyPacketInfo));
      if (NULL == keyinfo) {
        GST_ERROR_OBJECT (demux, "Failed to allocate memory...");
        goto error;
      }
      keyinfo->speed_count = 1;
      keyinfo->key_time = GST_CLOCK_TIME_NONE;
      keyinfo->key_packet = -1;
      ret = gst_asf_demux_index_table_parse_payload (demux, &packet, lentype, &data, &size, keyinfo);
      if (G_UNLIKELY (!ret)) {
        GST_WARNING_OBJECT (demux, "Failed to parse payload %u/%u", i + 1, num);
        free (keyinfo);
	      keyinfo = NULL;
        break;
      }
      if (keyinfo->key_time != GST_CLOCK_TIME_NONE) {
        GST_DEBUG_OBJECT (demux, "Got key payload...");
		sret = gst_asf_demux_push_sidx_queue(demux, sidx_queue, packet_num, keyinfo);
		if (G_UNLIKELY (!sret)) {
          GST_WARNING("Failed to push payload into sidx_queue");
          goto error;
		}
      } else {
        free (keyinfo);
		keyinfo = NULL;
	  }
    }
  } else {
    GST_LOG_OBJECT (demux, "Parsing single payload");

	keyinfo = (AsfKeyPacketInfo *) malloc (sizeof (AsfKeyPacketInfo));
    if (NULL == keyinfo) {
      GST_ERROR_OBJECT (demux, "Failed to allocate memory...");
      goto error;
    }
    keyinfo->speed_count = 1;
    keyinfo->key_time = GST_CLOCK_TIME_NONE;
    keyinfo->key_packet = -1;

    ret = gst_asf_demux_index_table_parse_payload (demux, &packet, -1, &data, &size, keyinfo);
	if (G_UNLIKELY (!ret)) {
      GST_WARNING_OBJECT (demux, "Failed to parse payload ");
      goto error;
    }

	if (keyinfo->key_time != GST_CLOCK_TIME_NONE) {
      GST_DEBUG_OBJECT (demux, "Got key payload...");
      sret = gst_asf_demux_push_sidx_queue(demux, sidx_queue, packet_num, keyinfo);
	  if (G_UNLIKELY (!sret)) {
	    GST_WARNING("Failed to push payload into sidx_queue");
		goto error;
	  }
    } else {
      free (keyinfo);
	  keyinfo = NULL;
	}
  }

  return ret;

/* ERRORS */
error:
  {
    if (buf)
      gst_buffer_replace(&buf, NULL);
    if (keyinfo)
      free (keyinfo);
    if (sidx_queue)
      g_queue_free (sidx_queue);
    return FALSE;
  }


short_packet:
  {
    GST_WARNING_OBJECT (demux, "Short packet!");
    return FALSE;
  }
}


static gboolean
gst_asf_demux_push_sidx_queue(GstASFDemux * demux, GQueue *sidx_queue, gint *packet_num, AsfKeyPacketInfo *keyinfo)
{
  AsfKeyPacketInfo *last_keyinfo = NULL;
  gboolean ret = TRUE;

  keyinfo->key_packet = *packet_num;
  if (!g_queue_is_empty (sidx_queue)) {
    GST_DEBUG ("length of queue = %d\n", g_queue_get_length (sidx_queue));

    last_keyinfo = g_queue_peek_tail (sidx_queue);
    if (NULL == last_keyinfo) {
      GST_ERROR_OBJECT (demux, "Failed to peek last element...");
      return FALSE;
    }
    GST_DEBUG_OBJECT (demux, "last keyinfo in queue with ts = %"GST_TIME_FORMAT, GST_TIME_ARGS (last_keyinfo->key_time));
  }

  GST_INFO_OBJECT (demux, "Recieved keypacket num = %d and ts = %"GST_TIME_FORMAT,
  keyinfo->key_packet, GST_TIME_ARGS (keyinfo->key_time));

  if (last_keyinfo) {
    /* Checking if multiple keypackets belong to same key frame */
    if (last_keyinfo->key_time < keyinfo->key_time) {
      /* Received new keyframe packet */
      GST_DEBUG_OBJECT (demux, "Pushing keyinfo to sidx queue...");
      g_queue_push_tail (sidx_queue, keyinfo);
    } else if (last_keyinfo->key_time == keyinfo->key_time) {
      /* calculating keypackets count those are belong to same keyframe */
      last_keyinfo->speed_count++;
      GST_DEBUG_OBJECT (demux, "Now Recieved Speed packet cnt = %d", last_keyinfo->speed_count);
	  free (keyinfo);
    } else {
      GST_WARNING_OBJECT (demux, "Not expected key info...");
	  free (keyinfo);
    }
    last_keyinfo = NULL;
  } else {
    GST_DEBUG_OBJECT (demux, "sidx queue is empty.. recieved first keyinfo");
	GST_INFO_OBJECT (demux, "Pushing keyinfo into sidx_queue = %d and ts = %"GST_TIME_FORMAT,
    keyinfo->key_packet, GST_TIME_ARGS (keyinfo->key_time));
    g_queue_push_tail (sidx_queue, keyinfo);
  }
  return ret;
}


static gboolean
gst_asf_demux_index_table_parse_payload (GstASFDemux * demux, AsfPacket * packet,
    gint lentype, const guint8 ** p_data, guint * p_size, AsfKeyPacketInfo *keyinfo)
{
  AsfPayload payload = { 0, };
  AsfStream *stream;
  gboolean is_compressed;
  guint payload_len;
  guint stream_num;

  if (G_UNLIKELY (*p_size < 1)) {
    GST_WARNING_OBJECT (demux, "Short packet!");
    return FALSE;
  }

  stream_num = GST_READ_UINT8 (*p_data) & 0x7f;
  payload.keyframe = ((GST_READ_UINT8 (*p_data) & 0x80) != 0);

  *p_data += 1;
  *p_size -= 1;

  payload.ts = GST_CLOCK_TIME_NONE;
  payload.duration = GST_CLOCK_TIME_NONE;
  payload.par_x = 0;
  payload.par_y = 0;
  payload.interlaced = FALSE;
  payload.tff = FALSE;
  payload.rff = FALSE;

  payload.mo_number =
      asf_packet_read_varlen_int (packet->prop_flags, 4, p_data, p_size);
  payload.mo_offset =
      asf_packet_read_varlen_int (packet->prop_flags, 2, p_data, p_size);
  payload.rep_data_len =
      asf_packet_read_varlen_int (packet->prop_flags, 0, p_data, p_size);

  is_compressed = (payload.rep_data_len == 1);

  GST_LOG_OBJECT (demux, "payload for stream %u", stream_num);
  GST_LOG_OBJECT (demux, "keyframe   : %s", (payload.keyframe) ? "yes" : "no");
  GST_LOG_OBJECT (demux, "compressed : %s", (is_compressed) ? "yes" : "no");

  if (G_UNLIKELY (*p_size < payload.rep_data_len)) {
    GST_WARNING_OBJECT (demux, "Short packet! rep_data_len=%u, size=%u",
        payload.rep_data_len, *p_size);
    return FALSE;
  }

  memcpy (payload.rep_data, *p_data,
      MIN (sizeof (payload.rep_data), payload.rep_data_len));

  *p_data += payload.rep_data_len;
  *p_size -= payload.rep_data_len;

  if (G_UNLIKELY (*p_size == 0)) {
    GST_WARNING_OBJECT (demux, "payload without data!?");
    return FALSE;
  }

  /* we use -1 as lentype for a single payload that's the size of the packet */
  if (G_UNLIKELY ((lentype >= 0 && lentype <= 3))) {
    payload_len = asf_packet_read_varlen_int (lentype, 0, p_data, p_size);
    if (*p_size < payload_len) {
      GST_WARNING_OBJECT (demux, "Short packet! payload_len=%u, size=%u",
          payload_len, *p_size);
      return FALSE;
    }
  } else {
    payload_len = *p_size;
  }

  GST_LOG_OBJECT (demux, "payload length: %u", payload_len);

  stream = gst_asf_demux_get_stream (demux, stream_num);

  if (G_UNLIKELY (stream == NULL)) {
    GST_WARNING_OBJECT (demux, "Payload for unknown stream %u, skipping",
        stream_num);
    if (*p_size < payload_len) {
      *p_data += *p_size;
      *p_size = 0;
    } else {
      *p_data += payload_len;
      *p_size -= payload_len;
    }
    return TRUE;
  }

  if (G_UNLIKELY (!is_compressed)) {
    GST_LOG_OBJECT (demux, "replicated data length: %u", payload.rep_data_len);

    if (payload.rep_data_len >= 8) {
      payload.mo_size = GST_READ_UINT32_LE (payload.rep_data);
      payload.ts = GST_READ_UINT32_LE (payload.rep_data + 4) * GST_MSECOND;
      if (G_UNLIKELY (payload.ts < demux->preroll))
        payload.ts = 0;
      else
        payload.ts -= demux->preroll;
      asf_payload_parse_replicated_data_extensions (stream, &payload);

      GST_LOG_OBJECT (demux, "media object size   : %u", payload.mo_size);
      GST_LOG_OBJECT (demux, "media object ts     : %" GST_TIME_FORMAT,
          GST_TIME_ARGS (payload.ts));
      GST_LOG_OBJECT (demux, "media object dur    : %" GST_TIME_FORMAT,
          GST_TIME_ARGS (payload.duration));
      *p_data += payload_len;
      *p_size -= payload_len;
    } else if (payload.rep_data_len != 0) {
      GST_WARNING_OBJECT (demux, "invalid replicated data length, very bad");
      *p_data += payload_len;
      *p_size -= payload_len;
      return FALSE;
    }

    if (payload.keyframe) {
      GST_DEBUG_OBJECT (demux, "key payload ts = %"GST_TIME_FORMAT, GST_TIME_ARGS(payload.ts));
      keyinfo->key_time = payload.ts;
    } else {
      keyinfo->key_time = GST_CLOCK_TIME_NONE;
    }

    GST_LOG_OBJECT (demux, "media object offset : %u", payload.mo_offset);
    GST_LOG_OBJECT (demux, "payload length: %u", payload_len);;

  } else {
    const guint8 *payload_data;
    GstClockTime ts;

    GST_LOG_OBJECT (demux, "Compressed payload, length=%u", payload_len);

    payload_data = *p_data;

    *p_data += payload_len;
    *p_size -= payload_len;

    ts = payload.mo_offset * GST_MSECOND;
    if (G_UNLIKELY (ts < demux->preroll))
      ts = 0;
    else
      ts -= demux->preroll;

    if (payload.keyframe) {
      GST_DEBUG_OBJECT (demux, "key payload ts = %"GST_TIME_FORMAT, GST_TIME_ARGS(payload.ts));
      keyinfo->key_time = ts;
    } else {
      keyinfo->key_time = GST_CLOCK_TIME_NONE;
    }
  }

  return TRUE;
}


static gboolean
gst_asf_demux_create_simple_index_table (GstASFDemux *demux)
{
  guint64 offset = 0;
  GstBuffer *packet_buf = NULL;
  GstBuffer *buf = NULL;
  gboolean bret = FALSE;
  AsfKeyPacketInfo *keyinfo = NULL;
  gint i = 0;
  gint packet_num = 0;
  GQueue *sidx_queue = NULL;

  /* pull data object for creating simple index table */
  if (!gst_asf_demux_pull_data (demux, demux->data_offset, demux->data_size, &buf, NULL))
    goto error;
  GST_LOG_OBJECT (demux, "Received buffer of size = %d", GST_BUFFER_SIZE (buf));

  sidx_queue = g_queue_new ();
  if (NULL == sidx_queue) {
    GST_ERROR_OBJECT (demux, "Failed to create GQueue");
    goto error;
  }

  /* Parsing of all packets to know the key frames available */
  while (offset < GST_BUFFER_SIZE (buf)) {
    /* Create sub buffer of size packet_size */
    packet_buf = gst_buffer_create_sub (buf, offset, demux->packet_size);
    if (NULL == packet_buf) {
      GST_ERROR_OBJECT (demux, "Failed to create sub buffer");
      goto error;
    }

    bret = gst_asf_demux_index_table_parse_packet (demux, packet_buf, sidx_queue, &packet_num);
    if (FALSE == bret) {
      GST_ERROR_OBJECT (demux, "Failed to parse packet");
      goto error;
    }

    offset += demux->packet_size;
    packet_num++;
  }

  gst_buffer_replace(&buf, NULL);

  GST_INFO_OBJECT (demux, "Total keyframe entries = %d", g_queue_get_length (sidx_queue));
  GST_LOG_OBJECT (demux, "Parsing of File is finished...Going to build sidx_entries");

  if (!demux->sidx_interval)
    demux->sidx_interval = GST_SECOND;

  demux->sidx_num_entries = gst_util_uint64_scale (demux->play_time, 1, demux->sidx_interval);
  GST_INFO_OBJECT (demux, "Number of simple index entries = %d", demux->sidx_num_entries);

  demux->sidx_entries = g_new0 (AsfSimpleIndexEntry, demux->sidx_num_entries);

  /* Creating index table (sidx table) */
  for (i = 0; i < demux->sidx_num_entries; i++) {
    AsfKeyPacketInfo *next_keyinfo = NULL;

    if (!g_queue_is_empty (sidx_queue)) {

      if (NULL == keyinfo) {
        keyinfo = g_queue_pop_head (sidx_queue);
        if (NULL == keyinfo) {
          GST_ERROR_OBJECT (demux, "Failed to get obj from queue");
          goto error;
        }
      }

      if ((i * demux->sidx_interval) > (demux->preroll + keyinfo->key_time)) {
        while (!g_queue_is_empty (sidx_queue)) {
          next_keyinfo = g_queue_pop_head (sidx_queue);
          if (NULL == next_keyinfo) {
            GST_ERROR_OBJECT (demux, "Failed to peek object");
            goto error;
          }
          GST_DEBUG_OBJECT (demux, "sidx ts = %"GST_TIME_FORMAT", keyinfo->ts = %"GST_TIME_FORMAT ", next_keyinfo->ts = %"GST_TIME_FORMAT,
		      GST_TIME_ARGS (i * demux->sidx_interval), GST_TIME_ARGS (keyinfo->key_time), 	GST_TIME_ARGS (next_keyinfo->key_time));

          if ((i * demux->sidx_interval) > (demux->preroll + next_keyinfo->key_time)) {
            free (keyinfo);
            keyinfo = next_keyinfo;
            GST_LOG ("Doing continue to packet %d", keyinfo->key_packet);
            continue;
          } else {
            GST_LOG ("Pushing again popped one");
            demux->sidx_entries[i].packet = keyinfo->key_packet;
            demux->sidx_entries[i].count = keyinfo->speed_count;
            g_queue_push_head (sidx_queue, next_keyinfo);
            break;
          }
        }

        if (g_queue_is_empty (sidx_queue)) {
          GST_INFO_OBJECT (demux, "queue is empty...");
          demux->sidx_entries[i].packet = keyinfo->key_packet;
          demux->sidx_entries[i].count = keyinfo->speed_count;
        }
      } else {
        demux->sidx_entries[i].packet = keyinfo->key_packet;
        demux->sidx_entries[i].count = keyinfo->speed_count;
      }
    } else if (keyinfo) {
      demux->sidx_entries[i].packet = keyinfo->key_packet;
      demux->sidx_entries[i].count = keyinfo->speed_count;
    } else {
      GST_ERROR_OBJECT (demux, "Not expected...");
      goto error;
    }
    GST_DEBUG_OBJECT (demux, "%" GST_TIME_FORMAT " = packet %4u  count : %2d",
          GST_TIME_ARGS (i * demux->sidx_interval), demux->sidx_entries[i].packet,
          demux->sidx_entries[i].count);
  }

  g_queue_free (sidx_queue);
  return TRUE;

error:
  {
    if (buf)
      gst_buffer_replace(&buf, NULL);
    if (sidx_queue)
      g_queue_free (sidx_queue);
    return FALSE;
  }
}

#endif

#ifdef CODEC_ENTRY
static GstFlowReturn
gst_asf_demux_process_codec_list (GstASFDemux * demux, guint8 * data,
    guint64 size)
{
  guint i=0,j=0;

#if 0 // FixMe
  if (size < 2)
    goto not_enough_data;

  if (demux->languages) {
    GST_WARNING ("More than one LANGUAGE_LIST object in stream");
    g_strfreev (demux->languages);
    demux->languages = NULL;
    demux->num_languages = 0;
  }
#endif

  /* skip reserved 16 bytes */
  gst_asf_demux_skip_bytes (16, &data, &size);

  /* Get codec entry count */
  demux->codec_entry_count = gst_asf_demux_get_uint32 (&data, &size);
  GST_LOG ("========================= codec_entry_count = %d", demux->codec_entry_count);

  /* Alloc codec entry based on count */
  demux->codec_entry = malloc  (demux->codec_entry_count * sizeof (CODEC_ENTRY));
  memset (demux->codec_entry, 0, demux->codec_entry_count * sizeof (CODEC_ENTRY));

  /* Read codec entry */
  for (i=0; i<demux->codec_entry_count; ++i) {
    guint8 *string_data = NULL;

    GST_LOG ("--------------------------------------------- codec_entry = %d", i);

	/* TYPE */
    demux->codec_entry[i].type = gst_asf_demux_get_uint16 (&data, &size);
	GST_LOG(":::::::::::::: type[%d]", demux->codec_entry[i].type);

	/* NAME */
	demux->codec_entry[i].name_len = gst_asf_demux_get_uint16 (&data, &size);
	if ( gst_asf_demux_get_bytes (&string_data, demux->codec_entry[i].name_len*2, &data, &size)) {
	  gchar *utf8 = g_convert ((gchar *) string_data, demux->codec_entry[i].name_len*2, "UTF-8", "UTF-16LE", NULL,  NULL, NULL);
	  demux->codec_entry[i].name = utf8;
	  g_free (string_data);
	}
	GST_LOG(":::::::::::::: name [%d][%s]", demux->codec_entry[i].name_len, demux->codec_entry[i].name);

	/* DESCRIPTION */
	demux->codec_entry[i].description_len = gst_asf_demux_get_uint16 (&data, &size);
	if ( gst_asf_demux_get_bytes (&string_data, demux->codec_entry[i].description_len*2, &data, &size)) {
	  gchar *utf8 = g_convert ((gchar *) string_data, demux->codec_entry[i].description_len*2, "UTF-8", "UTF-16LE", NULL, NULL, NULL);
	  demux->codec_entry[i].description = utf8;
	  g_free (string_data);
	}
	GST_LOG(":::::::::::::: description [%d][%s]", demux->codec_entry[i].description_len, demux->codec_entry[i].description);

	/* INFORMATION */
	demux->codec_entry[i].information_len = gst_asf_demux_get_uint16 (&data, &size);
	gst_asf_demux_get_bytes (&demux->codec_entry[i].information, demux->codec_entry[i].information_len, &data, &size);

	GST_LOG(":::::::::::::: information len [%d]", demux->codec_entry[i].information_len);
	for (j=0; j<demux->codec_entry[i].information_len; j++) {
	  GST_LOG(":::::::::::::: [0x%x]", demux->codec_entry[i].information[j]);
	}
  }
  return GST_FLOW_OK;

#if 0  // FixMe
not_enough_data:
  {
    GST_WARNING_OBJECT (demux, "short read parsing language list object!");
    //g_free (demux->languages);
    //demux->languages = NULL;
    return GST_FLOW_OK;         /* not fatal */
  }
#endif
}
#endif


