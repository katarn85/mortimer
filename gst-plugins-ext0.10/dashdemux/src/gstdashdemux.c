/*
 * DASH demux plugin for GStreamer
 *
 * gstdashdemux.c
 *
 * Copyright (C) 2012 Orange
 *
 * Authors:
 *   David Corvoysier <david.corvoysier@orange.com>
 *   Hamid Zakari <hamid.zakari@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/**
 * SECTION:element-dashdemux
 *
 * DASH demuxer element.
 * <title>Example launch line</title>
 * |[
 * gst-launch playbin2 uri="http://www-itec.uni-klu.ac.at/ftp/datasets/mmsys12/RedBullPlayStreets/redbull_4s/RedBullPlayStreets_4s_isoffmain_DIS_23009_1_v_2_1c2_2011_08_30.mpd"
 * ]|
 */

/* Implementation notes:
 *
 * The following section describes how dashdemux works internally.
 *
 * Introduction:
 *
 * dashdemux is a "fake" demux, as unlike traditional demux elements, it
 * doesn't split data streams contained in an enveloppe to expose them
 * to downstream decoding elements.
 *
 * Instead, it parses an XML file called a manifest to identify a set of
 * individual stream fragments it needs to fetch and expose to the actual
 * demux elements that will handle them (this behavior is sometimes
 * referred as the "demux after a demux" scenario).
 *
 * For a given section of content, several representations corresponding
 * to different bitrates may be available: dashdemux will select the most
 * appropriate representation based on local conditions (typically the
 * available bandwidth and the amount of buffering available, capped by
 * a maximum allowed bitrate).
 *
 * The representation selection algorithm can be configured using
 * specific properties: max bitrate, min/max buffering, bandwidth ratio.
 *
 *
 * General Design:
 *
 * dashdemux has a single sink pad that accepts the data corresponding
 * to the manifest, typically fetched from an HTTP or file source.
 *
 * dashdemux exposes the streams it recreates based on the fragments it
 * fetches through dedicated src pads corresponding to the caps of the
 * fragments container (ISOBMFF/MP4 or MPEG2TS).
 *
 * During playback, new representations will typically be exposed as a
 * new set of pads (see 'Switching between representations' below).
 *
 * Fragments downloading is performed using a dedicated task that fills
 * an internal queue. Another task is in charge of popping fragments
 * from the queue and pushing them downstream.
 *
 * Switching between representations:
 *
 * Decodebin supports scenarios allowing to seamlessly switch from one
 * stream to another inside the same "decoding chain".
 *
 * To achieve that, it combines the elements it autoplugged in chains
 *  and groups, allowing only one decoding group to be active at a given
 * time for a given chain.
 *
 * A chain can signal decodebin that it is complete by sending a
 * no-more-pads event, but even after that new pads can be added to
 * create new subgroups, providing that a new no-more-pads event is sent.
 *
 * We take advantage of that to dynamically create a new decoding group
 * in order to select a different representation during playback.
 *
 * Typically, assuming that each fragment contains both audio and video,
 * the following tree would be created:
 *
 * chain "DASH Demux"
 * |_ group "Representation set 1"
 * |   |_ chain "Qt Demux 0"
 * |       |_ group "Stream 0"
 * |           |_ chain "H264"
 * |           |_ chain "AAC"
 * |_ group "Representation set 2"
 *     |_ chain "Qt Demux 1"
 *         |_ group "Stream 1"
 *             |_ chain "H264"
 *             |_ chain "AAC"
 *
 * Or, if audio and video are contained in separate fragments:
 *
 * chain "DASH Demux"
 * |_ group "Representation set 1"
 * |   |_ chain "Qt Demux 0"
 * |   |   |_ group "Stream 0"
 * |   |       |_ chain "H264"
 * |   |_ chain "Qt Demux 1"
 * |       |_ group "Stream 1"
 * |           |_ chain "AAC"
 * |_ group "Representation set 2"
 *     |_ chain "Qt Demux 3"
 *     |   |_ group "Stream 2"
 *     |       |_ chain "H264"
 *     |_ chain "Qt Demux 4"
 *         |_ group "Stream 3"
 *             |_ chain "AAC"
 *
 * In both cases, when switching from Set 1 to Set 2 an EOS is sent on
 * each end pad corresponding to Rep 0, triggering the "drain" state to
 * propagate upstream.
 * Once both EOS have been processed, the "Set 1" group is completely
 * drained, and decodebin2 will switch to the "Set 2" group.
 *
 * Note: nothing can be pushed to the new decoding group before the
 * old one has been drained, which means that in order to be able to
 * adapt quickly to bandwidth changes, we will not be able to rely
 * on downstream buffering, and will instead manage an internal queue.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* FIXME 0.11: suppress warnings for deprecated API such as GStaticRecMutex
 * with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <string.h>
#include <inttypes.h>
#include <gst/base/gsttypefindhelper.h>
#include "gstdashdemux.h"

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src%d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/dash+xml"));

GST_DEBUG_CATEGORY_STATIC (gst_dash_demux_debug);
#define GST_CAT_DEFAULT gst_dash_demux_debug

enum
{
  PROP_0,

  PROP_MAX_BUFFERING_TIME,
  PROP_BANDWIDTH_USAGE,
  PROP_MAX_BITRATE,
  PROP_LAST
};

/* Default values for properties */
#define DEFAULT_MAX_BUFFERING_TIME       30     /* in seconds */
#define DEFAULT_BANDWIDTH_USAGE         0.8     /* 0 to 1     */
#define DEFAULT_MAX_BITRATE        24000000     /* in bit/s  */

#define DEFAULT_FAILED_COUNT 3
#define DOWNLOAD_RATE_HISTORY_MAX 3
#define DOWNLOAD_RATE_TIME_MAX 3 * GST_SECOND

/* Custom internal event to signal end of period */
#define GST_EVENT_DASH_EOP GST_EVENT_MAKE_TYPE(81, GST_EVENT_TYPE_DOWNSTREAM | GST_EVENT_TYPE_SERIALIZED)
static GstEvent *
gst_event_new_dash_eop (void)
{
  return gst_event_new_custom (GST_EVENT_DASH_EOP, NULL);
}


/* GObject */
static void gst_dash_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dash_demux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_dash_demux_dispose (GObject * obj);

/* GstElement */
static GstStateChangeReturn
gst_dash_demux_change_state (GstElement * element, GstStateChange transition);

/* GstDashDemux */
static GstFlowReturn gst_dash_demux_pad (GstPad * pad, GstBuffer * buf);
static gboolean gst_dash_demux_sink_event (GstPad * pad, GstEvent * event);
static gboolean gst_dash_demux_src_event (GstPad * pad, GstEvent * event);
static gboolean gst_dash_demux_src_query (GstPad * pad, GstQuery * query);
static void gst_dash_demux_stream_loop (GstDashDemux * demux);
static void gst_dash_demux_download_loop (GstDashDemux * demux);
static void gst_dash_demux_stop (GstDashDemux * demux);
static void gst_dash_demux_resume_stream_task (GstDashDemux * demux);
static void gst_dash_demux_resume_download_task (GstDashDemux * demux);
static gboolean gst_dash_demux_setup_all_streams (GstDashDemux * demux);
static gboolean gst_dash_demux_select_representations (GstDashDemux * demux);
static GstCaps *gst_dash_demux_get_input_caps (GstDashDemux * demux, GstActiveStream * stream);
static gboolean gst_dash_demux_get_next_fragment (GstDashDemux * demux, GstActiveStream **fragment_stream, GstClockTime *selected_ts);

static void gst_dash_demux_clear_streams(GstDashDemux * demux);
static void gst_dash_demux_reset (GstDashDemux * demux, gboolean dispose);
static GstClockTime gst_dash_demux_get_buffering_time (GstDashDemux * demux);

static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT (gst_dash_demux_debug, "dashdemux", 0,
      "dashdemux element");
}

GST_BOILERPLATE_FULL (GstDashDemux, gst_dash_demux, GstElement,
    GST_TYPE_ELEMENT, _do_init);

static void
gst_dash_demux_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (element_class, &srctemplate);

  gst_element_class_add_static_pad_template (element_class, &sinktemplate);

  gst_element_class_set_details_simple (element_class,
      "DASH Demuxer",
      "Codec/Demuxer",
      "Dynamic Adaptive Streaming over HTTP demuxer",
      "David Corvoysier <david.corvoysier@orange.com>\n\
                Hamid Zakari <hamid.zakari@gmail.com>\n\
                Gianluca Gennari <gennarone@gmail.com>");
}

static void
gst_dash_demux_dispose (GObject * obj)
{
  GstDashDemux *demux = GST_DASH_DEMUX (obj);

  if (demux->stream_task) {
    if (GST_TASK_STATE (demux->stream_task) != GST_TASK_STOPPED) {
      GST_DEBUG_OBJECT (demux, "Leaving streaming task");
      gst_task_stop (demux->stream_task);
      gst_task_join (demux->stream_task);
    }
    gst_object_unref (demux->stream_task);
    g_static_rec_mutex_free (&demux->stream_lock);
    g_mutex_free(demux->stream_timed_lock);
    demux->stream_task = NULL;
  }

  if (demux->download_task) {
    if (GST_TASK_STATE (demux->download_task) != GST_TASK_STOPPED) {
      GST_DEBUG_OBJECT (demux, "Leaving download task");
      gst_task_stop (demux->download_task);
      gst_task_join (demux->download_task);
    }
    gst_object_unref (demux->download_task);
    g_static_rec_mutex_free (&demux->download_lock);
    demux->download_task = NULL;
  }

  g_cond_clear (&demux->download_cond);
  g_mutex_clear (&demux->download_mutex);

  if (demux->downloader != NULL) {
    g_object_unref (demux->downloader);
    demux->downloader = NULL;
  }

  gst_dash_demux_reset (demux, TRUE);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
gst_dash_demux_class_init (GstDashDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_dash_demux_set_property;
  gobject_class->get_property = gst_dash_demux_get_property;
  gobject_class->dispose = gst_dash_demux_dispose;

  g_object_class_install_property (gobject_class, PROP_MAX_BUFFERING_TIME,
      g_param_spec_uint ("max-buffering-time", "Maximum buffering time",
          "Maximum number of seconds of buffer accumulated during playback",
          2, G_MAXUINT, DEFAULT_MAX_BUFFERING_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BANDWIDTH_USAGE,
      g_param_spec_float ("bandwidth-usage",
          "Bandwidth usage [0..1]",
          "Percentage of the available bandwidth to use when selecting representations",
          0, 1, DEFAULT_BANDWIDTH_USAGE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_BITRATE,
      g_param_spec_uint ("max-bitrate", "Max bitrate",
          "Max of bitrate supported by target decoder",
          1000, G_MAXUINT, DEFAULT_MAX_BITRATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_dash_demux_change_state);
}

static gboolean
_check_queue_full (GstDataQueue * q, guint visible, guint bytes, guint64 time,
    GstDashDemux *demux)
{
  return time > demux->max_buffering_time;
}

static void
_data_queue_item_destroy (GstDataQueueItem * item)
{
  gst_mini_object_unref (item->object);
  g_free (item);
}

static void
gst_dash_demux_stream_push_event (GstDashDemuxStream * stream,
                                  GstEvent *event)
{
  GstDataQueueItem *item = g_new0 (GstDataQueueItem, 1);

  item->object = GST_MINI_OBJECT_CAST (event);
  item->destroy = (GDestroyNotify) _data_queue_item_destroy;

  gst_data_queue_push (stream->queue, item);
}

static void
gst_dash_demux_stream_push_data (GstDashDemuxStream * stream,
    GstBuffer * buffer)
{
  GstDataQueueItem *item = g_new0 (GstDataQueueItem, 1);

  item->object = GST_MINI_OBJECT_CAST (buffer);
  item->duration = GST_BUFFER_DURATION (buffer);
  item->visible = TRUE;
  item->size = GST_BUFFER_SIZE (buffer);

  item->destroy = (GDestroyNotify) _data_queue_item_destroy;

  gst_data_queue_push (stream->queue, item);
}

static void
gst_dash_demux_init (GstDashDemux * demux, GstDashDemuxClass * klass)
{
  /* sink pad */
  demux->sinkpad = gst_pad_new_from_static_template (&sinktemplate, "sink");
  gst_pad_set_chain_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dash_demux_pad));
  gst_pad_set_event_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dash_demux_sink_event));
  gst_element_add_pad (GST_ELEMENT (demux), demux->sinkpad);

  /* Downloader */
  demux->downloader = gst_uri_downloader_new ();

  /* Properties */
  demux->max_buffering_time = DEFAULT_MAX_BUFFERING_TIME * GST_SECOND;
  demux->bandwidth_usage = DEFAULT_BANDWIDTH_USAGE;
  demux->max_bitrate = DEFAULT_MAX_BITRATE;

  demux->max_video_width = 0;
  demux->max_video_height = 0;

  /* Updates task */
  g_static_rec_mutex_init (&demux->download_lock);
  demux->download_task =
      gst_task_create ((GstTaskFunction) gst_dash_demux_download_loop, demux);
  gst_task_set_lock (demux->download_task, &demux->download_lock);
  g_cond_init (&demux->download_cond);
  g_mutex_init (&demux->download_mutex);

  /* Streaming task */
  g_static_rec_mutex_init (&demux->stream_lock);
  demux->stream_task =
      gst_task_create ((GstTaskFunction) gst_dash_demux_stream_loop, demux);
  gst_task_set_lock (demux->stream_task, &demux->stream_lock);
  demux->stream_timed_lock = g_mutex_new ();
}

static void
gst_dash_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDashDemux *demux = GST_DASH_DEMUX (object);

  switch (prop_id) {
    case PROP_MAX_BUFFERING_TIME:
      demux->max_buffering_time = g_value_get_uint (value) * GST_SECOND;
      break;
    case PROP_BANDWIDTH_USAGE:
      demux->bandwidth_usage = g_value_get_float (value);
      break;
    case PROP_MAX_BITRATE:
      demux->max_bitrate = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dash_demux_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstDashDemux *demux = GST_DASH_DEMUX (object);

  switch (prop_id) {
    case PROP_MAX_BUFFERING_TIME:
      g_value_set_uint (value, demux->max_buffering_time / GST_SECOND);
      break;
    case PROP_BANDWIDTH_USAGE:
      g_value_set_float (value, demux->bandwidth_usage);
      break;
    case PROP_MAX_BITRATE:
      g_value_set_uint (value, demux->max_bitrate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_dash_demux_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstDashDemux *demux = GST_DASH_DEMUX (element);

  GST_DEBUG_OBJECT (demux, "changing state %s - %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_dash_demux_reset (demux, FALSE);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      demux->cancelled = TRUE;
      gst_dash_demux_stop (demux);
      gst_task_join (demux->stream_task);
      gst_task_join (demux->download_task);
      break;
    default:
      break;
  }
  return ret;
}

void
gst_dash_demux_flush_stream_queues (GstDashDemux * demux)
{
  GSList *it;
  GstDashDemuxStream *stream;
  for(it = demux->streams; it; it=it->next)
  {
    stream = it->data;
    gst_data_queue_set_flushing(stream->queue, TRUE);
    gst_data_queue_flush(stream->queue);
  }
}

static gboolean
gst_dash_demux_src_event (GstPad * pad, GstEvent * event)
{
  GstDashDemux *demux;

  demux = GST_DASH_DEMUX (gst_pad_get_element_private (pad));

  switch (event->type) {
    case GST_EVENT_SEEK:
    {
      gdouble rate;
      GstFormat format;
      GstSeekFlags flags;
      GstSeekType start_type, stop_type;
      gint64 start, stop;
      GList *list;
      GstClockTime current_pos = GST_CLOCK_TIME_NONE;
      GstClockTime target_pos;
      guint current_period;
      GstActiveStream *stream;
      GstStreamPeriod *period = NULL;
      guint nb_active_stream;
      guint stream_idx = 0;
      guint *seek_idx = NULL;   /*Seek positions on each stream*/
      gboolean end_of_mpd = FALSE;

      if (gst_mpd_client_is_live (demux->client)) {
        GST_WARNING_OBJECT (demux, "Received seek event for live stream");
        return FALSE;
      }

      gst_event_parse_seek (event, &rate, &format, &flags, &start_type, &start,
          &stop_type, &stop);

      if (format != GST_FORMAT_TIME)
        return FALSE;

      GST_DEBUG_OBJECT (demux,
          "seek event, rate: %f type: %d start: %" GST_TIME_FORMAT " stop: %"
          GST_TIME_FORMAT, rate, start_type, GST_TIME_ARGS (start),
          GST_TIME_ARGS (stop));

      //GST_MPD_CLIENT_LOCK (demux->client);

      /* select the requested Period in the Media Presentation */
      target_pos = (GstClockTime) start;
      current_period = 0;
      for (list = g_list_first (demux->client->periods); list;
          list = g_list_next (list)) {
        period = list->data;
        current_pos = period->start;
        current_period = period->number;
        if (current_pos <= target_pos
            && target_pos < current_pos + period->duration) {
          break;
        }
      }
      if(target_pos == current_pos + period->duration) {
        /*Seeking to the end of MPD*/
        end_of_mpd = TRUE;
        goto seeking;
      }
      if (list == NULL) {
        GST_WARNING_OBJECT (demux, "Could not find seeked Period");
        return FALSE;
      }
      if (current_period != gst_mpd_client_get_period_index (demux->client)) {
        GST_DEBUG_OBJECT (demux, "Seeking to Period %d", current_period);
        /* setup video, audio and subtitle streams, starting from the new Period */
        if (!gst_mpd_client_set_period_index (demux->client, current_period) ||
            !gst_dash_demux_setup_all_streams (demux))
          return FALSE;
      }

      /*select the requested segments for all streams*/
      nb_active_stream = gst_mpdparser_get_nb_active_stream (demux->client);
      seek_idx = g_malloc0(sizeof(gint)*nb_active_stream);
      gint video_idx = gst_mpd_client_get_video_active_stream_id(demux->client);
      if(video_idx >= 0) {
        /*Seeking on video stream firstly.*/
        GstClockTime segment_start;
        segment_start = gst_mpd_client_stream_find_segment(demux->client, video_idx,
                                                         target_pos, &seek_idx[video_idx]);
        if(!GST_CLOCK_TIME_IS_VALID(segment_start))
          goto no_segment;
        target_pos = segment_start;
      }
      /*Seeking on non video streams*/
      for (stream_idx = 0; stream_idx < nb_active_stream; stream_idx++) {
        if (video_idx != stream_idx) {
          GstClockTime stream_start = gst_mpd_client_stream_find_segment(demux->client,
                                          stream_idx, target_pos, &seek_idx[stream_idx]);
          if(!GST_CLOCK_TIME_IS_VALID (stream_start)) {
            goto no_segment;
          }
        }
      }

seeking:
      /* We can actually perform the seek */
      nb_active_stream = gst_mpdparser_get_nb_active_stream (demux->client);

      if (flags & GST_SEEK_FLAG_FLUSH) {
        GST_DEBUG_OBJECT (demux, "sending flush start");
        stream_idx = 0;
        while (stream_idx < nb_active_stream) {
          GstDashDemuxStream *dash_stream = g_slist_nth_data (demux->streams, stream_idx);
	  dash_stream->need_header = TRUE;
          gst_pad_push_event (dash_stream->srcpad,
              gst_event_new_flush_start ());
          stream_idx++;
        }
      }

      /* Stop the demux */
      demux->cancelled = TRUE;
      gst_dash_demux_stop (demux);
      GST_DEBUG_OBJECT (demux, "joining tasks");
      gst_task_join (demux->stream_task);
      gst_task_join (demux->download_task);
      GST_DEBUG_OBJECT (demux, "tasks was joined");

      /* Wait for streaming to finish */
      g_static_rec_mutex_lock (&demux->stream_lock);

      //GST_MPD_CLIENT_LOCK (demux->client);
      demux->end_of_period = end_of_mpd;
      //GST_MPD_CLIENT_UNLOCK (demux->client);


      for (stream_idx = 0; stream_idx < nb_active_stream; stream_idx++) {
        GstDashDemuxStream *dash_stream = g_slist_nth_data (demux->streams, stream_idx);
        GstCaps *caps = gst_pad_get_negotiated_caps (dash_stream->srcpad);
        if(caps) {
          gst_caps_replace (&dash_stream->input_caps, NULL);
          gst_caps_unref (caps);
        }
        if(!end_of_mpd) {
          GST_DEBUG_OBJECT (demux, "Seeking to sequence %d on stream %d", seek_idx[stream_idx], stream_idx);
          stream = gst_mpdparser_get_active_stream_by_index (demux->client, stream_idx);
          gst_mpd_client_set_segment_index(stream, seek_idx[stream_idx]);
        }
        gst_data_queue_set_flushing(dash_stream->queue, FALSE);
        dash_stream->start_time = target_pos;
        dash_stream->download_end_of_period = end_of_mpd;
        dash_stream->stream_end_of_period = end_of_mpd;
        dash_stream->stream_eos = end_of_mpd;
        dash_stream->need_segment = TRUE;
      }
      if(!end_of_mpd)
        g_free(seek_idx);

      if (flags & GST_SEEK_FLAG_FLUSH) {
        GST_DEBUG_OBJECT (demux, "Sending flush stop on all pad");

        for (stream_idx = 0; stream_idx < nb_active_stream; stream_idx++) {
          GstDashDemuxStream *dash_stream = g_slist_nth_data (demux->streams, stream_idx);
          gst_pad_push_event (dash_stream->srcpad,
              gst_event_new_flush_stop ());
        }
      }

      /* Restart the demux */
      demux->cancelled = FALSE;
      gst_dash_demux_resume_download_task (demux);
      gst_dash_demux_resume_stream_task (demux);
      g_static_rec_mutex_unlock (&demux->stream_lock);

      return TRUE;
no_segment:
      {
        GST_WARNING_OBJECT (demux, "Could not find seeked fragment on stream %d", stream_idx);
        g_free(seek_idx);
        return FALSE;
      }
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, event);
}

static gboolean
gst_dash_demux_setup_mpdparser_streams (GstDashDemux * demux, GstMpdClient *client)
{
  GList *listLang = NULL;
  guint i, nb_audio;
  gchar *lang;

  GST_MPD_CLIENT_LOCK (client);
  /* clean old active stream list, if any */
  gst_active_streams_free (client);

  if (!gst_mpd_client_setup_streaming (client, GST_STREAM_VIDEO, "")) {
    GST_INFO_OBJECT (demux, "No video adaptation set found");
  } else {
    gst_mpd_client_get_max_video_dimensions(client, &demux->max_video_width,
                                            &demux->max_video_height);
  }

  nb_audio =
      gst_mpdparser_get_list_and_nb_of_audio_language (client,
      &listLang);
  if (nb_audio == 0)
    nb_audio = 1;
  GST_INFO_OBJECT (demux, "Number of language is=%d", nb_audio);

  for (i = 0; i < nb_audio; i++) {
    lang = (gchar *) g_list_nth_data (listLang, i);
    if (gst_mpdparser_get_nb_adaptationSet (client) > 1)
      if (!gst_mpd_client_setup_streaming (client, GST_STREAM_AUDIO,
              lang))
        GST_INFO_OBJECT (demux, "No audio adaptation set found");

    if (gst_mpdparser_get_nb_adaptationSet (client) > nb_audio)
      if (!gst_mpd_client_setup_streaming (client,
              GST_STREAM_APPLICATION, lang))
        GST_INFO_OBJECT (demux, "No application adaptation set found");
  }
  GST_MPD_CLIENT_UNLOCK (client);
  return TRUE;
}

static gboolean
gst_dash_demux_setup_all_streams (GstDashDemux * demux)
{
  guint i;
  if( !gst_dash_demux_setup_mpdparser_streams(demux, demux->client))
    return FALSE;

  GST_DEBUG_OBJECT (demux, "Creating dashdemux streams");
  gst_dash_demux_clear_streams(demux);
  for ( i =0; i < gst_mpdparser_get_nb_active_stream (demux->client); i++) {
    GstDashDemuxStream *dash_stream;
    GstCaps *caps;
    GstActiveStream *active_stream;
    dash_stream = g_new0(GstDashDemuxStream, 1);
    demux->streams = g_slist_append(demux->streams, dash_stream);
    dash_stream->idx = i;
    dash_stream->queue = gst_data_queue_new ((GstDataQueueCheckFullFunction) _check_queue_full, demux);
    dash_stream->need_header = TRUE;
    dash_stream->need_segment = TRUE;
    dash_stream->start_time = GST_CLOCK_TIME_NONE;
    gst_download_rate_init (&dash_stream->dnl_rate);
    gst_download_rate_set_max_length (&dash_stream->dnl_rate,
				      DOWNLOAD_RATE_HISTORY_MAX);
    gst_download_rate_set_aver_period (&dash_stream->dnl_rate,
				       DOWNLOAD_RATE_TIME_MAX);
    /*Create stream pad*/
    active_stream = gst_mpdparser_get_active_stream_by_index(demux->client, i);
    caps = gst_dash_demux_get_input_caps(demux, active_stream);
    dash_stream->srcpad = gst_pad_new_from_static_template (&srctemplate, NULL);
    gst_pad_set_event_function (dash_stream->srcpad,
        GST_DEBUG_FUNCPTR (gst_dash_demux_src_event));
    gst_pad_set_query_function (dash_stream->srcpad,
        GST_DEBUG_FUNCPTR (gst_dash_demux_src_query));
    gst_pad_set_element_private (dash_stream->srcpad, demux);
    gst_pad_set_active (dash_stream->srcpad, TRUE);
    gst_pad_set_caps (dash_stream->srcpad, caps);
    gst_caps_unref(caps);
    gst_element_add_pad (GST_ELEMENT (demux), gst_object_ref (dash_stream->srcpad));
  }
  /* Send 'no-more-pads' to have decodebin create the new group */
  gst_element_no_more_pads (GST_ELEMENT (demux));

  return TRUE;
}

static gboolean
gst_dash_demux_sink_event (GstPad * pad, GstEvent * event)
{
  GstDashDemux *demux = GST_DASH_DEMUX (gst_pad_get_parent (pad));

  switch (event->type) {
    case GST_EVENT_EOS:{
      gchar *manifest;
      GstQuery *query;
      gboolean res;

      if (demux->manifest == NULL) {
        GST_WARNING_OBJECT (demux, "Received EOS without a manifest.");
        break;
      }

      GST_DEBUG_OBJECT (demux, "Got EOS on the sink pad: manifest fetched");

      if (demux->client)
        gst_mpd_client_free (demux->client);
      demux->client = gst_mpd_client_new ();

      query = gst_query_new_uri ();
      res = gst_pad_peer_query (pad, query);
      if (res) {
        gst_query_parse_uri (query, &demux->client->mpd_uri);
        GST_DEBUG_OBJECT (demux, "Fetched MPD file at URI: %s",
            demux->client->mpd_uri);
      } else {
        GST_WARNING_OBJECT (demux, "MPD URI query failed.");
      }
      gst_query_unref (query);

      manifest = (gchar *) GST_BUFFER_DATA (demux->manifest);
      if (manifest == NULL) {
        GST_WARNING_OBJECT (demux, "Error validating the manifest.");
      } else if (!gst_mpd_parse (demux->client, manifest,
              GST_BUFFER_SIZE (demux->manifest))) {
        /* In most cases, this will happen if we set a wrong url in the
         * source element and we have received the 404 HTML response instead of
         * the manifest */
        GST_ELEMENT_ERROR (demux, STREAM, DECODE, ("Invalid manifest."),
            (NULL));
        return FALSE;
      }

      gst_buffer_unref (demux->manifest);
      demux->manifest = NULL;

      if (!gst_mpd_client_setup_media_presentation (demux->client)) {
        GST_ELEMENT_ERROR (demux, STREAM, DECODE,
            ("Incompatible manifest file."), (NULL));
        return FALSE;
      }

      /* setup video, audio and subtitle streams, starting from first Period */
      if (!gst_mpd_client_set_period_index (demux->client, 0) ||
          !gst_dash_demux_setup_all_streams (demux))
        return FALSE;

      /* start playing from the first segment */
      gst_mpd_client_set_segment_index_for_all_streams (demux->client, 0);

      /* Send duration message */
      if (!gst_mpd_client_is_live (demux->client)) {
        GstClockTime duration =
            gst_mpd_client_get_media_presentation_duration (demux->client);

        if (duration != GST_CLOCK_TIME_NONE) {
          GST_DEBUG_OBJECT (demux,
              "Sending duration message : %" GST_TIME_FORMAT,
              GST_TIME_ARGS (duration));
          gst_element_post_message (GST_ELEMENT (demux),
              gst_message_new_duration (GST_OBJECT (demux), GST_FORMAT_TIME,
                  duration));
        } else {
          GST_DEBUG_OBJECT (demux,
              "mediaPresentationDuration unknown, can not send the duration message");
        }
      }
      gst_dash_demux_resume_download_task (demux);
      gst_dash_demux_resume_stream_task (demux);
      gst_event_unref (event);
      return TRUE;
    }
    case GST_EVENT_NEWSEGMENT:
      /* Swallow newsegments, we'll push our own */
      gst_event_unref (event);
      return TRUE;
    default:
      break;
  }

  return gst_pad_event_default (pad, event);
}

static gboolean
gst_dash_demux_src_query (GstPad * pad, GstQuery * query)
{
  GstDashDemux *dashdemux;
  gboolean ret = FALSE;

  if (query == NULL)
    return FALSE;

  dashdemux = GST_DASH_DEMUX (gst_pad_get_element_private (pad));

  switch (query->type) {
    case GST_QUERY_DURATION:{
      GstClockTime duration = -1;
      GstFormat fmt;

      gst_query_parse_duration (query, &fmt, NULL);
      if (fmt == GST_FORMAT_TIME) {
        duration =
            gst_mpd_client_get_media_presentation_duration (dashdemux->client);
        if (GST_CLOCK_TIME_IS_VALID (duration) && duration > 0) {
          gst_query_set_duration (query, GST_FORMAT_TIME, duration);
          ret = TRUE;
        }
      }
      GST_DEBUG_OBJECT (dashdemux,
          "GST_QUERY_DURATION returns %s with duration %" GST_TIME_FORMAT,
          ret ? "TRUE" : "FALSE", GST_TIME_ARGS (duration));
      break;
    }
    case GST_QUERY_SEEKING:{
      GstFormat fmt;
      gint64 stop = -1;

      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);
      GST_DEBUG_OBJECT (dashdemux, "Received GST_QUERY_SEEKING with format %d",
          fmt);
      if (fmt == GST_FORMAT_TIME) {
        GstClockTime duration;

        duration =
            gst_mpd_client_get_media_presentation_duration (dashdemux->client);
        if (GST_CLOCK_TIME_IS_VALID (duration) && duration > 0)
          stop = duration;

        gst_query_set_seeking (query, fmt,
            !gst_mpd_client_is_live (dashdemux->client), 0, stop);
        ret = TRUE;
        GST_DEBUG_OBJECT (dashdemux, "GST_QUERY_SEEKING returning with stop : %"
            GST_TIME_FORMAT, GST_TIME_ARGS (stop));
      }
      break;
    }
    case GST_QUERY_URI: {
      /* forwarding uri */
      GST_DEBUG("URI query recevied in DASH demux.....");
        gboolean res;
        res = gst_pad_query_default (pad,query);
        if(res)
          GST_DEBUG("forwarding URI is done successfully!!...");
      ret = TRUE;
      break;
    }
    default:{
      // By default, do not forward queries upstream
      break;
    }
  }

  return ret;
}

static GstFlowReturn
gst_dash_demux_pad (GstPad * pad, GstBuffer * buf)
{
  GstDashDemux *demux = GST_DASH_DEMUX (gst_pad_get_parent (pad));

  if (demux->manifest == NULL)
    demux->manifest = buf;
  else
    demux->manifest = gst_buffer_join (demux->manifest, buf);

  gst_object_unref (demux);

  return GST_FLOW_OK;
}

static void
gst_dash_demux_stop (GstDashDemux * demux)
{
  gst_uri_downloader_cancel (demux->downloader);
  gst_dash_demux_flush_stream_queues (demux);

  if (GST_TASK_STATE (demux->download_task) != GST_TASK_STOPPED) {
    GST_TASK_SIGNAL (demux->download_task);
    gst_task_stop (demux->download_task);
    g_mutex_lock (&demux->download_mutex);
    g_cond_signal (&demux->download_cond);
    g_mutex_unlock (&demux->download_mutex);
  }
  if (GST_TASK_STATE (demux->stream_task) != GST_TASK_STOPPED) {
    GST_TASK_SIGNAL (demux->stream_task);
    gst_task_stop (demux->stream_task);
  }
}

/* gst_dash_demux_stream_loop:
 *
 * Loop for the "stream' task that pushes fragments to the src pads.
 *
 * Startup:
 * The task is started as soon as we have received the manifest and
 * waits for the first fragment to be downloaded and pushed in the
 * queue. Once this fragment has been pushed, the task pauses itself
 * until actual playback begins.
 *
 * During playback:
 * The task pushes fragments downstream at regular intervals based on
 * the fragment duration. If it detects a queue underrun, it sends
 * a buffering event to tell the main application to pause.
 *
 * Teardown:
 * The task is stopped when we have reached the end of the manifest
 * and emptied our queue.
 *
 */
static void
gst_dash_demux_stream_loop (GstDashDemux * demux)
{
  GstFlowReturn ret;
  GstActiveStream *active_stream;
  GstDashDemuxStream *selected_stream = NULL;
  GstClockTime min_ts = GST_CLOCK_TIME_NONE;
  guint i = 0;
  gboolean eos = TRUE;
  gboolean eop = TRUE;

  for (i = 0; i < g_slist_length (demux->streams); i++) {
    GstDashDemuxStream *dash_stream = g_slist_nth_data (demux->streams, i);
    GstBuffer *buffer;
    GstDataQueueItem *item;

    if (dash_stream->stream_eos) {
      GST_DEBUG_OBJECT (demux, "Stream %d is eos, skipping", dash_stream->idx);
      continue;
    }

    if (dash_stream->stream_end_of_period) {
      GST_DEBUG_OBJECT (demux, "Stream %d is eop, skipping", dash_stream->idx);
      eos = FALSE;
      continue;
    }
    eos = FALSE;
    eop = FALSE;

    if (!gst_data_queue_peek (dash_stream->queue, &item))
      goto flushing;

    if(GST_IS_BUFFER(item->object)) {
      buffer = GST_BUFFER(item->object);
      if(GST_BUFFER_TIMESTAMP(buffer) < min_ts ||
         !GST_CLOCK_TIME_IS_VALID(min_ts)) {
        min_ts = GST_BUFFER_TIMESTAMP(buffer);
        selected_stream = dash_stream;
      } else if (!GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (item->object))) {
        selected_stream = dash_stream;
        break;
      }
    } else {
      selected_stream = dash_stream;
      break;
    }
  }

  if(selected_stream) {
    GstBuffer *buffer;
    GstDataQueueItem *item;

    if (!gst_data_queue_pop (selected_stream->queue, &item))
      goto end;
    if ( GST_IS_BUFFER (item->object)) {
      buffer = GST_BUFFER(item->object);
      active_stream = gst_mpdparser_get_active_stream_by_index (demux->client, selected_stream->idx);

      if (selected_stream->need_segment) {
        if(!GST_CLOCK_TIME_IS_VALID (selected_stream->start_time)) {
          if(GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (buffer))){
            selected_stream->start_time = GST_BUFFER_TIMESTAMP (buffer);
          } else {
            selected_stream->start_time = 0;
          }
        }
        /* And send a newsegment */
        GST_DEBUG_OBJECT (demux, "Sending new-segment stream #%d. segment start:%"
            GST_TIME_FORMAT, selected_stream->idx, GST_TIME_ARGS (selected_stream->start_time));
        gst_pad_push_event (selected_stream->srcpad,
            gst_event_new_new_segment (FALSE, 1.0, GST_FORMAT_TIME,
                selected_stream->start_time, GST_CLOCK_TIME_NONE, selected_stream->start_time));
        selected_stream->need_segment = FALSE;
      }

      GST_DEBUG_OBJECT (demux, "Pushing fragment #%llu (stream %d) ts=%"GST_TIME_FORMAT, GST_BUFFER_OFFSET (buffer),
                        selected_stream->idx, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));
      ret = gst_pad_push (selected_stream->srcpad, gst_buffer_ref(buffer) );
      item->destroy (item);
      if ((ret != GST_FLOW_OK) && (active_stream->mimeType == GST_STREAM_VIDEO))
        goto error_pushing;
    } else {
      GstEvent *event = GST_EVENT (item->object);
      if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
        selected_stream->stream_eos = TRUE;
        selected_stream->stream_end_of_period = TRUE;
      } else if (GST_EVENT_TYPE (event) == GST_EVENT_DASH_EOP) {
        selected_stream->stream_end_of_period = TRUE;
      }

      if (GST_EVENT_TYPE (item->object) != GST_EVENT_DASH_EOP) {
        gst_pad_push_event (selected_stream->srcpad,
            gst_event_ref (GST_EVENT_CAST (item->object)));
      }

      item->destroy (item);
    }
  } else {
    if (eos) {
      goto end_of_manifest;
    } else if (eop) {
      /*TODO Switch to next period*/
    }
  }

end:
  return;

flushing:
  {
    GST_INFO_OBJECT (demux, "Queue is flushing. Stopped streaming task");
    gst_task_stop (demux->stream_task);
    return;
  }

end_of_manifest:
  {
    GST_INFO_OBJECT (demux, "Reached end of manifest, sending EOS");
    guint i = 0;
    for (i = 0; i < gst_mpdparser_get_nb_active_stream (demux->client); i++) {
      GstDashDemuxStream *dash_stream = g_slist_nth_data (demux->streams, i);
      gst_pad_push_event (dash_stream->srcpad, gst_event_new_eos ());
    }
    GST_INFO_OBJECT (demux, "Stopped streaming task");
    gst_task_stop (demux->stream_task);
    return;
  }

error_pushing:
  {
    /* FIXME: handle error */
    GST_ERROR_OBJECT (demux,
        "Error pushing buffer: %s... terminating the demux",
        gst_flow_get_name (ret));
    gst_dash_demux_stop (demux);
    return;
  }
}

static void
gst_dash_demux_clear_streams(GstDashDemux * demux) {
  guint i = 0;
  gst_dash_demux_flush_stream_queues (demux);
  for (i = 0; i < g_slist_length(demux->streams); i++) {
    GstDashDemuxStream *dash_stream = g_slist_nth_data (demux->streams, i);
    gst_download_rate_deinit (&dash_stream->dnl_rate);
    if (dash_stream->input_caps) {
      gst_caps_unref (dash_stream->input_caps);
      dash_stream->input_caps = NULL;
    }
    if (dash_stream->srcpad) {
      gst_object_unref (dash_stream->srcpad);
      dash_stream->srcpad = NULL;
    }
    /*TODO consider unref stream->output_caps*/
    g_object_unref (dash_stream->queue);
  }
  if(demux->streams) {
    g_slist_free(demux->streams);
    demux->streams = NULL;
  }
}

static void
gst_dash_demux_reset (GstDashDemux * demux, gboolean dispose)
{
  gint stream_idx;
  demux->end_of_period = FALSE;
  demux->cancelled = FALSE;

  gst_dash_demux_clear_streams(demux);

  if (demux->manifest) {
    gst_buffer_unref (demux->manifest);
    demux->manifest = NULL;
  }
  if (demux->client) {
    gst_mpd_client_free (demux->client);
    demux->client = NULL;
  }
  if (!dispose) {
    demux->client = gst_mpd_client_new ();
  }

  demux->last_manifest_update = GST_CLOCK_TIME_NONE;
  for (stream_idx = 0; stream_idx < g_slist_length (demux->streams); stream_idx++) {
    GstDashDemuxStream *dash_stream = g_slist_nth_data (demux->streams, stream_idx);
    dash_stream->need_segment = TRUE;
  }
}

static GstClockTime
gst_dash_demux_get_buffering_time (GstDashDemux * demux)
{
  GstClockTime buffer_time = 0;
  GSList *it;
  GstDashDemuxStream *stream;
  GstDataQueueSize queue_size;

  for(it=demux->streams; it; it=it->next) {
    stream = it->data;
    gst_data_queue_get_level(stream->queue, &queue_size);

    if (queue_size.time > 0) {
      buffer_time = queue_size.time;
      break;
    }
  }

  return buffer_time;
}

static gboolean
gst_dash_demux_update_manifest(GstDashDemux *demux) {
  GstFragment *download;
  GstBuffer *buffer;
  GstClockTime duration, now = gst_util_get_timestamp();
  gint64 update_period = demux->client->mpd_node->minimumUpdatePeriod;

  if (update_period == -1) {
    GST_DEBUG_OBJECT (demux, "minimumUpdatePeriod unspecified, will not update MPD");
    return TRUE;
  }

  /* init reference time for manifest file updates */
  if (!GST_CLOCK_TIME_IS_VALID (demux->last_manifest_update))
    demux->last_manifest_update = now;

  /* update the manifest file */
  if (now >= demux->last_manifest_update + update_period * GST_MSECOND) {
    GST_DEBUG_OBJECT (demux, "Updating manifest file from URL %s",
        demux->client->mpd_uri);
    download =
        gst_uri_downloader_fetch_uri (demux->downloader,
        demux->client->mpd_uri);
    if (download == NULL) {
      GST_WARNING_OBJECT (demux,
          "Failed to update the manifest file from URL %s",
          demux->client->mpd_uri);
    } else {
      GstMpdClient *new_client = NULL;
      guint period_idx;
      const gchar *period_id;
      GSList *iter;

      buffer = gst_fragment_get_buffer(download);
      g_object_unref (download);
      /* parse the manifest file */
      if (buffer == NULL) {
        GST_WARNING_OBJECT (demux, "Error validating the manifest.");
        return TRUE;
      }

      new_client = gst_mpd_client_new ();
      new_client->mpd_uri = g_strdup (demux->client->mpd_uri);
      if (!gst_mpd_parse (new_client,
              (gchar *) GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer))) {
        /* In most cases, this will happen if we set a wrong url in the
         * source element and we have received the 404 HTML response instead of
         * the manifest */
        GST_WARNING_OBJECT (demux, "Error parsing the manifest.");
        gst_buffer_unref (buffer);
        return TRUE;
      }

      gst_buffer_unref (buffer);
      GST_DEBUG_OBJECT (demux, "Updating manifest");

      period_id = gst_mpd_client_get_period_id (demux->client);
      period_idx = gst_mpd_client_get_period_index (demux->client);

      /* setup video, audio and subtitle streams, starting from current Period */
      if (!gst_mpd_client_setup_media_presentation (new_client)) {
        /* TODO */
      }

      if (period_idx) {
        /*If more than one period exists.*/
        if (!gst_mpd_client_set_period_id (new_client, period_id)) {
          GST_DEBUG_OBJECT (demux,
              "Error setting up the updated manifest file");
          return FALSE;
        }
      } else {
        if (!gst_mpd_client_set_period_index (new_client, period_idx)) {
          GST_DEBUG_OBJECT (demux,
              "Error setting up the updated manifest file");
          return FALSE;
        }
      }

      if (!gst_dash_demux_setup_mpdparser_streams (demux, new_client)) {
            GST_ERROR_OBJECT (demux, "Failed to setup streams on manifest "
                "update");
            return FALSE;
      }

      /* update the streams to play from the next segment */
      for (iter = demux->streams; iter; iter = g_slist_next (iter)) {
        GstDashDemuxStream *demux_stream = iter->data;
        GstActiveStream *new_stream;
        GstClockTime ts;

        new_stream = gst_mpdparser_get_active_stream_by_index (new_client,
            demux_stream->idx);

        if (!new_stream) {
          GST_DEBUG_OBJECT (demux,
              "Stream of index %d is missing from manifest update",
              demux_stream->idx);
          return FALSE;
        }

        if (gst_mpd_client_get_next_fragment_timestamp (demux->client,
                demux_stream->idx, &ts)) {
          gst_mpd_client_stream_seek (new_client, demux_stream->idx, ts);

        } else
            if (gst_mpd_client_get_last_fragment_timestamp (demux->client,
                demux_stream->idx, &ts)) {
          /* try to set to the old timestamp + 1 */
          gst_mpd_client_stream_seek (new_client, demux_stream->idx, ts+1);
        }
      }

      /*Remember download failed count*/
      new_client->download_failed_count = demux->client->download_failed_count;

      gst_mpd_client_free (demux->client);
      demux->client = new_client;

      /* Send an updated duration message */
      duration =
          gst_mpd_client_get_media_presentation_duration (demux->client);

      if (duration != GST_CLOCK_TIME_NONE) {
        GST_DEBUG_OBJECT (demux,
            "Sending duration message : %" GST_TIME_FORMAT,
            GST_TIME_ARGS (duration));
        gst_element_post_message (GST_ELEMENT (demux),
            gst_message_new_duration(GST_OBJECT (demux), GST_FORMAT_TIME, duration));
      } else {
        GST_DEBUG_OBJECT (demux,
            "mediaPresentationDuration unknown, can not send the duration message");
      }
      demux->last_manifest_update = gst_util_get_timestamp ();
      GST_DEBUG_OBJECT (demux, "Manifest file successfully updated");
    }
  }
  return TRUE;
}

static void
gst_dash_demux_download_wait (GstDashDemux * demux, GstClockTime time_diff)
{
  gint64 end_time = g_get_monotonic_time () + time_diff / GST_USECOND;

  GST_DEBUG_OBJECT (demux, "Download waiting for %" GST_TIME_FORMAT,
      GST_TIME_ARGS (time_diff));
  g_cond_wait_until (&demux->download_cond, &demux->download_mutex, end_time);
  GST_DEBUG_OBJECT (demux, "Download finished waiting");
}

static void
gst_dash_demux_check_live(GstDashDemux* demux, GstActiveStream *fragment_stream,
     GstClockTime fragment_ts)
{
  gint64 time_diff;
  gint pos;

  pos =
      gst_mpd_client_check_time_position (demux->client, fragment_stream,
      fragment_ts, &time_diff);
  GST_DEBUG_OBJECT (demux,
      "Checked position for fragment ts %" GST_TIME_FORMAT
      ", res: %d, diff: %" G_GINT64_FORMAT, GST_TIME_ARGS (fragment_ts),
      pos, time_diff);

  time_diff *= GST_USECOND;
  if (pos < 0) {
    /* we're behind, try moving to the 'present' */
    GDateTime *now = g_date_time_new_now_utc ();

    GST_DEBUG_OBJECT (demux,
        "Falling behind live stream, moving forward");
      gst_mpd_client_seek_to_time(demux->client, now);
    g_date_time_unref (now);

  } else if (pos > 0) {
    /* we're ahead, wait a little */

    GST_DEBUG_OBJECT (demux, "Waiting for next segment to be created");
    gst_dash_demux_download_wait (demux, time_diff);
  } else {
    demux->client->download_failed_count++;
  }
}

/* gst_dash_demux_download_loop:
 *
 * Loop for the "download' task that fetches fragments based on the
 * selected representations.
 *
 * Startup:
 *
 * The task is started from the stream loop.
 *
 * During playback:
 *
 * It sequentially fetches fragments corresponding to the current
 * representations and pushes them into a queue.
 *
 * It tries to maintain the number of queued items within a predefined
 * range: if the queue is full, it will pause, checking every 100 ms if
 * it needs to restart downloading fragments.
 *
 * When a new set of fragments has been downloaded, it evaluates the
 * download time to check if we can or should switch to a different
 * set of representations.
 *
 * Teardown:
 *
 * The task will exit when it encounters an error or when the end of the
 * manifest has been reached.
 *
 */
void
gst_dash_demux_download_loop (GstDashDemux * demux)
{
  GstActiveStream *fragment_stream = NULL;
  GstClockTime fragment_ts;
  if ( gst_mpd_client_is_live (demux->client) && demux->client->mpd_uri != NULL ) {
    if (!gst_dash_demux_update_manifest(demux))
      goto end_of_manifest;
  }

  /* try to switch to another set of representations if needed */
  gst_dash_demux_select_representations (demux);

  /* fetch the next fragment */
  while (!gst_dash_demux_get_next_fragment (demux, &fragment_stream, &fragment_ts)) {
    if (demux->end_of_period) {
      GST_INFO_OBJECT (demux, "Reached the end of the Period");
      /* setup video, audio and subtitle streams, starting from the next Period */
      if (!gst_mpd_client_set_period_index (demux->client,
              gst_mpd_client_get_period_index (demux->client) + 1)
          || !gst_dash_demux_setup_all_streams (demux)) {
        GST_INFO_OBJECT (demux, "Reached the end of the manifest file");
        gst_task_start (demux->stream_task);
        goto end_of_manifest;
      }
      /* start playing from the first segment of the new period */
      gst_mpd_client_set_segment_index_for_all_streams (demux->client, 0);
      demux->end_of_period = FALSE;
    } else if (!demux->cancelled) {
      gst_uri_downloader_reset (demux->downloader);
      if(gst_mpd_client_is_live (demux->client)) {
        gst_dash_demux_check_live (demux, fragment_stream, fragment_ts);
      } else {
        demux->client->download_failed_count++;
      }

      if (demux->client->download_failed_count < DEFAULT_FAILED_COUNT) {
        GST_WARNING_OBJECT (demux, "Could not fetch the next fragment");
        goto quit;
      } else if (gst_mpd_client_set_next_baseURL_for_stream (demux->client)) {
	GST_INFO_OBJECT (demux, "Current baseURL is %s. Trying to select another",
			 gst_mpdparser_get_baseURL (demux->client));
	demux->client->download_failed_count = 0;
      } else {
        goto error_downloading;
      }
    } else {
      goto quit;
    }
  }
  GST_INFO_OBJECT (demux, "Internal buffering : %" PRIu64 " s",
      gst_dash_demux_get_buffering_time (demux) / GST_SECOND);
  demux->client->download_failed_count = 0;


quit:
  {
    return;
  }

end_of_manifest:
  {
    GST_INFO_OBJECT (demux, "Stopped download task");
    gst_task_stop (demux->download_task);
    return;
  }

error_downloading:
  {
    GST_ELEMENT_ERROR (demux, RESOURCE, NOT_FOUND,
        ("Could not fetch the next fragment"), (NULL));
    gst_dash_demux_stop (demux);
    return;
  }
}

static void
gst_dash_demux_resume_stream_task (GstDashDemux * demux)
{
  gst_task_start (demux->stream_task);
}

static void
gst_dash_demux_resume_download_task (GstDashDemux * demux)
{
  gst_uri_downloader_reset(demux->downloader);
  gst_task_start (demux->download_task);
}

/* gst_dash_demux_select_representations:
 *
 * Select the most appropriate media representations based on a target
 * bitrate.
 *
 * FIXME: all representations are selected against the same bitrate, but
 * they will share the same bandwidth. This only works today because the
 * audio representations bitrate usage is negligible as compared to the
 * video representation one.
 *
 * Returns TRUE if a new set of representations has been selected
 */
static gboolean
gst_dash_demux_select_representations (GstDashDemux * demux)
{
  GstDashDemuxStream *stream = NULL;
  GstActiveStream *active_stream = NULL;
  GList *rep_list = NULL;
  GSList *iter;
  gint new_index;
  gboolean ret = FALSE;

  GST_MPD_CLIENT_LOCK (demux->client);
  for (iter = demux->streams; iter; iter = g_slist_next (iter)) {
    guint64 bitrate;

    stream = iter->data;
    active_stream =
      gst_mpdparser_get_active_stream_by_index (demux->client, stream->idx);
    if (!active_stream)
      return FALSE;

    /* retrieve representation list */
    if (active_stream->cur_adapt_set)
      rep_list = active_stream->cur_adapt_set->Representations;
    if (!rep_list)
      return FALSE;

    bitrate = gst_download_rate_get_current_rate (&stream->dnl_rate) *
      demux->bandwidth_usage;

    GST_DEBUG_OBJECT (demux, "Trying to change bitrate to %" G_GUINT64_FORMAT, bitrate);

    /* get representation index with current max_bandwidth */
    new_index =
        gst_mpdparser_get_rep_idx_with_max_bandwidth (rep_list, bitrate);

    /* if no representation has the required bandwidth, take the lowest one */
    if (new_index == -1)
      new_index = 0;

    if (new_index != active_stream->representation_idx) {
      GstRepresentationNode *rep = g_list_nth_data (rep_list, new_index);
      GST_INFO_OBJECT (demux, "Changing representation idx: %d %d %u",
		       stream->idx, new_index, rep->bandwidth);
      if (gst_mpd_client_setup_representation (demux->client, active_stream,
					       rep)) {
	ret = TRUE;
	stream->need_header = TRUE;
        GST_INFO_OBJECT (demux, "Switching bitrate to %d",
            active_stream->cur_representation->bandwidth);
      } else {
        GST_WARNING_OBJECT (demux,
            "Can not switch representation, aborting...");
      }
    }
  }
  GST_MPD_CLIENT_UNLOCK (demux->client);
  return ret;
}

static GstFragment *
gst_dash_demux_get_next_header (GstDashDemux * demux, guint stream_idx)
{
  const gchar *initializationURL;
  gchar *next_header_uri;
  GstFragment *fragment;

  if (!gst_mpd_client_get_next_header (demux->client, &initializationURL,
          stream_idx))
    return NULL;

  if (strncmp (initializationURL, "http://", 7) != 0) {
    next_header_uri =
        g_strconcat (gst_mpdparser_get_baseURL (demux->client),
        initializationURL, NULL);
  } else {
    next_header_uri = g_strdup (initializationURL);
  }

  GST_INFO_OBJECT (demux, "Fetching header %s", next_header_uri);

  fragment = gst_uri_downloader_fetch_uri (demux->downloader, next_header_uri);
  g_free (next_header_uri);
  g_free (initializationURL);

  return fragment;
}

static GstCaps *
gst_dash_demux_get_video_input_caps (GstDashDemux * demux,
    GstActiveStream * stream)
{
  guint width = 0, height = 0, bandwidth = 0;
  const gchar *mimeType = NULL;
  GstCaps *caps = NULL;

  if (stream == NULL)
    return NULL;
#ifdef DASHDEMUX_MODIFICATION
  /* caps need to inlcude resolution and bandwidth */
    width = gst_mpd_client_get_video_stream_width (stream);
    height = gst_mpd_client_get_video_stream_height (stream);
    bandwidth = gst_mpd_client_get_video_stream_bandwidth (stream);
#else
  /* if bitstreamSwitching is true we dont need to swich pads on resolution change */
  if (!gst_mpd_client_get_bitstream_switching_flag (stream)) {
    width = gst_mpd_client_get_video_stream_width (stream);
    height = gst_mpd_client_get_video_stream_height (stream);
  }
#endif
  mimeType = gst_mpd_client_get_stream_mimeType (stream);
  if (mimeType == NULL)
    return NULL;

  caps = gst_caps_new_simple (mimeType, NULL);
  if (width > 0 && height > 0) {
    gst_caps_set_simple (caps, "width", G_TYPE_INT, width, "height",
        G_TYPE_INT, height, NULL);
  }

  if (bandwidth > 0)
    gst_caps_set_simple (caps, "bandwidth", G_TYPE_INT, bandwidth, NULL);

  gst_caps_set_simple (caps, "max-width", G_TYPE_INT, demux->max_video_width, "max-height",
      G_TYPE_INT, demux->max_video_height, NULL);

/*  add ContentProtection to caps  */
  if ( stream->cur_adapt_set->RepresentationBase->ContentProtection != NULL){
    GList *list;
    GstDescriptorType *ContentProtectionDesc;

    list = g_list_first (stream->cur_adapt_set->RepresentationBase->ContentProtection);
    ContentProtectionDesc = (GstDescriptorType *) list->data;
    gchar *schemeIdUri = ContentProtectionDesc->schemeIdUri;
    gchar *msprPro = ContentProtectionDesc->msprPro;

  if ( (schemeIdUri != NULL) && (msprPro != NULL) ){
    gst_caps_set_simple (caps, "contentprotection_scheme_iduri", G_TYPE_STRING, schemeIdUri, "mspr:pro", G_TYPE_STRING, msprPro, NULL);
  }
 }
  return caps;
}

static GstCaps *
gst_dash_demux_get_audio_input_caps (GstDashDemux * demux,
    GstActiveStream * stream)
{
  guint rate = 0, channels = 0;
  const gchar *mimeType;
  GstCaps *caps = NULL;

  if (stream == NULL)
    return NULL;

  /* if bitstreamSwitching is true we dont need to swich pads on rate/channels change */
  if (!gst_mpd_client_get_bitstream_switching_flag (stream)) {
    channels = gst_mpd_client_get_audio_stream_num_channels (stream);
    rate = gst_mpd_client_get_audio_stream_rate (stream);
  }
  mimeType = gst_mpd_client_get_stream_mimeType (stream);
  if (mimeType == NULL)
    return NULL;

  caps = gst_caps_new_simple (mimeType, NULL);
  if (rate > 0) {
    gst_caps_set_simple (caps, "rate", G_TYPE_INT, rate, NULL);
  }
  if (channels > 0) {
    gst_caps_set_simple (caps, "channels", G_TYPE_INT, channels, NULL);
  }

/*  add ContentProtection to caps  */
  if ( stream->cur_adapt_set->RepresentationBase->ContentProtection != NULL){
    GList *list;
    GstDescriptorType *ContentProtectionDesc;

    list = g_list_first (stream->cur_adapt_set->RepresentationBase->ContentProtection);
    ContentProtectionDesc = (GstDescriptorType *) list->data;
    gchar *schemeIdUri = ContentProtectionDesc->schemeIdUri;
    gchar *msprPro = ContentProtectionDesc->msprPro;

  if ( (schemeIdUri != NULL) && (msprPro != NULL) ){
    gst_caps_set_simple (caps, "contentprotection_scheme_iduri", G_TYPE_STRING, schemeIdUri, "mspr:pro", G_TYPE_STRING, msprPro, NULL);
  }
 }
  return caps;
}

static GstCaps *
gst_dash_demux_get_application_input_caps (GstDashDemux * demux,
    GstActiveStream * stream)
{
  const gchar *mimeType;
  GstCaps *caps = NULL;

  if (stream == NULL)
    return NULL;

  mimeType = gst_mpd_client_get_stream_mimeType (stream);
  if (mimeType == NULL)
    return NULL;

  caps = gst_caps_new_simple (mimeType, NULL);

  return caps;
}

static GstCaps *
gst_dash_demux_get_input_caps (GstDashDemux * demux, GstActiveStream * stream)
{
  GstCaps *caps;
  switch (stream->mimeType) {
    case GST_STREAM_VIDEO:
      caps = gst_dash_demux_get_video_input_caps (demux, stream);
      break;
    case GST_STREAM_AUDIO:
      caps = gst_dash_demux_get_audio_input_caps (demux, stream);
      break;
    case GST_STREAM_APPLICATION:
      caps = gst_dash_demux_get_application_input_caps (demux, stream);
      break;
    default:
      return GST_CAPS_NONE;
  }
  /*Need to signal downstream elements about dash*/
  gst_caps_set_simple(caps, "variant", G_TYPE_STRING, "dash-fragmented", NULL);
  return caps;
}

/* gst_dash_demux_get_next_fragment_set:
 *
 * Get the next set of fragments for the current representations.
 *
 * This function uses the generic URI downloader API.
 *
 * Returns FALSE if an error occured while downloading fragments
 *
 */
static gboolean
gst_dash_demux_get_next_fragment (GstDashDemux * demux,GstActiveStream **fragment_stream,
     GstClockTime *selected_ts)
{
  GstActiveStream *stream;
  GstDashDemuxStream *dash_stream;
  GstDashDemuxStream *selected_stream = NULL;
  GstFragment *download, *header;
  gchar *next_fragment_uri;
  GstClockTime duration;
  GstClockTime timestamp;
  GstClockTime min_timestamp = GST_CLOCK_TIME_NONE;
  gboolean discont;
  GTimeVal now;
  GTimeVal start;
  GstClockTime diff;
  guint64 size_buffer = 0;
  GstBuffer *buffer;
  guint stream_idx;
  gboolean end_of_period = TRUE;

  /*Select stream with smallest progress*/
  for (stream_idx = 0; stream_idx < g_slist_length (demux->streams); stream_idx++) {
    dash_stream = g_slist_nth_data (demux->streams, stream_idx);

    if (dash_stream->download_end_of_period)
      continue;

    if (gst_mpd_client_get_next_fragment_timestamp (demux->client, stream_idx, &timestamp)) {
      if( timestamp < min_timestamp || !GST_CLOCK_TIME_IS_VALID(min_timestamp) ) {
        selected_stream = dash_stream;
        min_timestamp = timestamp;
      }
    } else {
      GstEvent *event = NULL;

      GST_INFO_OBJECT (demux,
          "This Period doesn't contain more fragments for stream %u",
          dash_stream->idx);

      /* check if this is live and we should wait for more data */
      if (gst_mpd_client_is_live (demux->client)
          && demux->client->mpd_node->minimumUpdatePeriod != -1) {
        end_of_period = FALSE;
        continue;
      }

      if (gst_mpd_client_has_next_period (demux->client)) {
        event = gst_event_new_dash_eop ();
      } else {
        GST_DEBUG_OBJECT (demux,
            "No more fragments or periods for this stream, setting EOS");
        event = gst_event_new_eos ();
      }
      dash_stream->download_end_of_period = TRUE;
      gst_dash_demux_stream_push_event (dash_stream, event);
    }
  }

  if (selected_ts)
    *selected_ts = min_timestamp;
  if (fragment_stream && selected_stream)
    *fragment_stream = gst_mpdparser_get_active_stream_by_index (demux->client, selected_stream->idx);
   /* Fetch next fragment from selected stream */
  if(selected_stream) {

    if (!gst_mpd_client_get_next_fragment (demux->client,
            selected_stream->idx, &discont, &next_fragment_uri, &duration, &timestamp)) {
      GST_WARNING_OBJECT (demux, "Failed to download fragment for stream %d", selected_stream->idx);
    } else {

      g_get_current_time (&start);
      GST_INFO_OBJECT (demux, "Fetching next fragment stream=%d ts=%"GST_TIME_FORMAT" url=%s",
                       selected_stream->idx, GST_TIME_ARGS(timestamp), next_fragment_uri);

      stream = gst_mpdparser_get_active_stream_by_index (demux->client, selected_stream->idx);

      end_of_period = FALSE;

      download = gst_uri_downloader_fetch_uri (demux->downloader,
          next_fragment_uri);
      g_free (next_fragment_uri);

      if (stream == NULL)
        return FALSE;

      if (download == NULL) {
        guint segment_idx = gst_mpd_client_get_segment_index ( stream );
        if(segment_idx > 0) {
          /*Move to previous segment if download failed*/
          gst_mpd_client_set_segment_index (stream, segment_idx - 1);
        }
        return FALSE;
      }

      download->start_time = timestamp;
      download->stop_time = timestamp + duration;

      download->index = gst_mpd_client_get_segment_index (stream) - 1;

      GstCaps *caps = gst_dash_demux_get_input_caps (demux, stream);
      buffer = gst_fragment_get_buffer (download);
      g_return_val_if_fail (buffer != NULL, FALSE);

      if (selected_stream->need_header) {
        /* Store the new input caps for that stream */
        gst_caps_replace (&dash_stream->input_caps, caps);
        GST_INFO_OBJECT (demux, "Input source caps: %" GST_PTR_FORMAT,
            dash_stream->input_caps);

        /* We need to fetch a new header */
        if ((header = gst_dash_demux_get_next_header (demux, selected_stream->idx)) == NULL) {
          GST_INFO_OBJECT (demux, "Unable to fetch header");
        } else {
          /* Replace fragment buffer with a new one including the header */
          GstBuffer *header_buffer = gst_fragment_get_buffer(header);
          buffer = gst_buffer_join(header_buffer, buffer);
          g_object_unref (header);
	  selected_stream->need_header = FALSE;
        }
      } else {
        gst_caps_unref (caps);
      }

      g_get_current_time (&now);
      g_object_unref (download);

      gst_buffer_set_caps(buffer, dash_stream->input_caps);
      GST_BUFFER_TIMESTAMP(buffer) = timestamp;
      GST_BUFFER_DURATION(buffer) = duration;
      GST_BUFFER_OFFSET(buffer) = gst_mpd_client_get_segment_index (stream) - 1;
      size_buffer = GST_BUFFER_SIZE (buffer);
      /* Push fragment into the queue */
      gst_dash_demux_stream_push_data (selected_stream, buffer);
      diff = (GST_TIMEVAL_TO_TIME (now) - GST_TIMEVAL_TO_TIME (start));
      gst_download_rate_add_rate (&selected_stream->dnl_rate, size_buffer, diff, duration);
      GST_DEBUG_OBJECT (demux,
                        "Stream: %d Download rate = %" G_GUINT64_FORMAT " Kbits/s (%" G_GUINT64_FORMAT
                        " Ko in %.2f s)\n", selected_stream->idx,
                        gst_download_rate_get_current_rate (&selected_stream->dnl_rate) / 1000,
                        size_buffer / 1024,
                        ((double) diff / GST_SECOND));
    }
  }

  demux->end_of_period = end_of_period;

  return !end_of_period;
}
