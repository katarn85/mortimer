
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* FIXME 0.11: suppress warnings for deprecated API such as GStaticRecMutex
 * with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <string.h>
//#include <gst/glib-compat-private.h>
#include "gstssdemux.h"

enum
{
  PROP_0,
  PROP_COOKIES,
  PROP_ALLOW_AUDIO_ONLY,
  PROP_CACHE_TIME,
  PROP_LOW_PERCENTAGE,
  PROP_HIGH_PERCENTAGE,
  PROP_BITRATE_SWITCH_TOLERANCE,
  PROP_LAST
};

static GstStaticPadTemplate ssdemux_videosrc_template =
GST_STATIC_PAD_TEMPLATE ("video",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate ssdemux_audiosrc_template =
GST_STATIC_PAD_TEMPLATE ("audio",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate ssdemux_subsrc_template =
GST_STATIC_PAD_TEMPLATE ("subtitle",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate ssdemux_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-ss")); // Need to decide source mimetype

GST_DEBUG_CATEGORY_STATIC (gst_ss_demux_debug);
#define GST_CAT_DEFAULT gst_ss_demux_debug

#undef SIMULATE_AUDIO_ONLY /* enable to simulate audio only case forcibly */

static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT (gst_ss_demux_debug, "ssdemux", 0, "ssdemux element");
}

GST_BOILERPLATE_FULL (GstSSDemux, gst_ss_demux, GstElement,
    GST_TYPE_ELEMENT, _do_init);

#define DEFAULT_CACHE_TIME 6*GST_SECOND
#define DEFAULT_BITRATE_SWITCH_TOLERANCE 0.4
#define DEFAULT_LOW_PERCENTAGE 1
#define DEFAULT_HIGH_PERCENTAGE 99

struct _GstSSDemuxStream
{
  /* Streaming task */
  void *parent;
  GstPad *pad;
  gchar *name;
  SS_STREAM_TYPE type;
  GstTask *stream_task;
  GStaticRecMutex stream_lock;
  GstElement *pipe;
  GstElement *urisrc;
  GstElement *parser;
  GstElement *sink;
  GstBus *bus;
  GMutex *lock;
  GCond *cond;
  GMutex *queue_lock;
  GCond *queue_full;
  GCond *queue_empty;
  guint frag_cnt;
  GQueue *queue;
  gchar *uri;
  guint64 start_ts;
  gboolean sent_ns;
  GstCaps *caps;
  guint64 switch_ts;
  guint64 avg_dur;
  gboolean is_buffering;
  gint64 percent;
  gboolean rcvd_percent;
  guint64 push_block_time;
  gint64 cached_duration;

  /* for fragment download rate calculation */
  guint64 download_start_ts;
  guint64 download_stop_ts;
  guint64 download_size;

};

static void gst_ss_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ss_demux_get_property (GObject * object, guint prop_id,
	GValue * value, GParamSpec * pspec);
static gboolean gst_ss_demux_sink_event (GstPad * pad, GstEvent * event);
static GstStateChangeReturn gst_ss_demux_change_state (GstElement * element, GstStateChange transition);
static void gst_ss_demux_dispose (GObject * obj);
static GstFlowReturn gst_ss_demux_chain (GstPad * pad, GstBuffer * buf);
static void gst_ss_demux_stream_loop (GstSSDemux * demux);
static gboolean gst_ss_demux_download_bus_cb(GstBus *bus, GstMessage *msg, gpointer data);
static void gst_ss_demux_stream_init (GstSSDemux *demux, GstSSDemuxStream *stream, SS_STREAM_TYPE stream_type);
static void gst_ss_demux_stream_free (GstSSDemux * demux, GstSSDemuxStream * stream);
static void gst_ssm_demux_on_new_buffer (GstElement * appsink, void* data);
static gboolean gst_ss_demux_download_fragment (GstSSDemux *demux, GstSSDemuxStream *stream, const gchar * uri, guint64 start_ts);
static gboolean gst_ss_demux_create_download_pipe (GstSSDemux * demux, GstSSDemuxStream *stream, const gchar * uri, guint64 start_ts);
static void gst_ss_demux_stop (GstSSDemux * demux, GstSSDemuxStream *stream);
static gboolean gst_ss_demux_create_dummy_pipe (GstSSDemux * demux, GstSSDemuxStream *stream);
static gboolean gst_ss_demux_create_dummy_sender(GstSSDemux *demux, GstSSDemuxStream *stream);
static void gst_ss_demux_push_loop (GstSSDemuxStream *stream);
static void gst_ss_demux_update_buffering (GstSSDemuxStream *stream, guint64 percent);

static void
gst_ss_demux_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&ssdemux_videosrc_template));
  gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&ssdemux_audiosrc_template));
  gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&ssdemux_subsrc_template));
  gst_element_class_add_pad_template (element_class, gst_static_pad_template_get (&ssdemux_sink_template));

  gst_element_class_set_details_simple (element_class,
      "SS Demuxer",
      "Demuxer/URIList",
      "Smooth Streaming demuxer",
      "Naveen Cherukuri<naveen.ch@samsung.com>");
}

static void
gst_ss_demux_class_init (GstSSDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_ss_demux_set_property;
  gobject_class->get_property = gst_ss_demux_get_property;
  gobject_class->dispose = gst_ss_demux_dispose;

  /* to share cookies with other sessions */
  g_object_class_install_property (gobject_class, PROP_COOKIES,
      g_param_spec_boxed ("cookies", "Cookies", "HTTP request cookies",
          G_TYPE_STRV, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* will be considered only in LIVE case */
  g_object_class_install_property (gobject_class, PROP_ALLOW_AUDIO_ONLY,
      g_param_spec_boolean ("allow-audio-only", "Allow audio only when downloadrate is less in live case",
          "Allow audio only stream download in live case when download rate is less",
          TRUE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CACHE_TIME,
      g_param_spec_uint64 ("max-cache-time", "caching time",
          "amount of data that can be cached in seconds", 0, G_MAXUINT64,
          DEFAULT_CACHE_TIME,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_LOW_PERCENTAGE,
      g_param_spec_int ("low-percent", "Low Percent",
          "Low threshold to start buffering",
          1, 100, DEFAULT_LOW_PERCENTAGE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_HIGH_PERCENTAGE,
      g_param_spec_int ("high-percent", "High percent",
          "High threshold to complete buffering",
          2, 100, DEFAULT_HIGH_PERCENTAGE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BITRATE_SWITCH_TOLERANCE,
      g_param_spec_float ("bitrate-switch-tolerance",
          "Bitrate switch tolerance",
          "Tolerance with respect of the fragment duration to switch to "
          "a different bitrate if the client is too slow/fast.",
          0, 1, DEFAULT_BITRATE_SWITCH_TOLERANCE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_ss_demux_change_state);
}

static void
gst_ss_demux_init (GstSSDemux * demux, GstSSDemuxClass * klass)
{
  /* sink pad */
  demux->sinkpad = gst_pad_new_from_static_template (&ssdemux_sink_template, "sink");
  gst_pad_set_chain_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ss_demux_chain));
  gst_pad_set_event_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_ss_demux_sink_event));
  gst_element_add_pad (GST_ELEMENT (demux), demux->sinkpad);

  demux->max_cache_time = DEFAULT_CACHE_TIME;
  demux->cookies = NULL;
  demux->ss_mode = SS_MODE_NO_SWITCH;
  demux->switch_eos = FALSE;
  demux->allow_audio_only = FALSE;
  demux->percent = 100;
  demux->low_percent = DEFAULT_LOW_PERCENTAGE;
  demux->high_percent = DEFAULT_HIGH_PERCENTAGE;
  demux->eos = FALSE;
}

static void
gst_ss_demux_dispose (GObject * obj)
{
  GstSSDemux *demux = GST_SS_DEMUX (obj);
  int n =0;

  for (n = 0; n < SS_STREAM_NUM; n++) {
    if (demux->streams[n]) {
      gst_pad_stop_task ((demux->streams[n])->pad);
      g_print ("\n\n\nstopped the TASK\n\n\n");
      gst_ss_demux_stream_free (demux, demux->streams[n]);
      demux->streams[n] = NULL;
    }
  }

  if (demux->parser) {
    gst_ssm_parse_free (demux->parser);
    demux->parser = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static void
gst_ss_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSSDemux *demux = GST_SS_DEMUX (object);

  switch (prop_id) {
     case PROP_COOKIES:
      g_strfreev (demux->cookies);
      demux->cookies = g_strdupv (g_value_get_boxed (value));
      break;
    case PROP_ALLOW_AUDIO_ONLY:
      demux->allow_audio_only = g_value_get_boolean (value);
      break;
    case PROP_CACHE_TIME:
      demux->max_cache_time = g_value_get_uint64 (value);
      break;
    case PROP_LOW_PERCENTAGE:
      demux->low_percent = g_value_get_int (value);
      break;
    case PROP_HIGH_PERCENTAGE:
      demux->high_percent = g_value_get_int (value);
      break;
    case PROP_BITRATE_SWITCH_TOLERANCE:
      demux->bitrate_switch_tol = g_value_get_float (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ss_demux_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstSSDemux *demux = GST_SS_DEMUX (object);

  switch (prop_id) {
    case PROP_COOKIES:
      g_value_set_boxed (value, g_strdupv (demux->cookies));
      break;
    case PROP_ALLOW_AUDIO_ONLY:
      g_value_set_boolean (value, demux->allow_audio_only);
      break;
    case PROP_CACHE_TIME:
      g_value_set_uint64 (value, demux->max_cache_time);
      break;
    case PROP_LOW_PERCENTAGE:
      g_value_set_int (value, demux->low_percent);
      break;
    case PROP_HIGH_PERCENTAGE:
      g_value_set_int (value, demux->high_percent);
      break;
    case PROP_BITRATE_SWITCH_TOLERANCE:
      g_value_set_float (value, demux->bitrate_switch_tol);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_ss_demux_sink_event (GstPad * pad, GstEvent * event)
{
  GstSSDemux *demux = GST_SS_DEMUX (gst_pad_get_parent (pad));
  GstQuery *query = NULL;
  gboolean ret;
  gchar *uri;

  switch (event->type) {
    case GST_EVENT_EOS: {
      int i = 0;
      if (demux->manifest == NULL) {
        GST_ERROR_OBJECT (demux, "Received EOS without a manifest.");
        goto error;
      }

      GST_DEBUG_OBJECT (demux, "Got EOS on the sink pad: mainifest file fetched");

      query = gst_query_new_uri ();
      ret = gst_pad_peer_query (demux->sinkpad, query);
      if (ret) {
        gst_query_parse_uri (query, &uri);
        demux->parser = gst_ssm_parse_new (uri);
        g_free (uri);
      } else {
        GST_ERROR_OBJECT (demux, "failed to query URI from upstream");
        goto error;
      }
      gst_query_unref (query);
      query = NULL;

      GST_LOG_OBJECT (demux, "data = %p & size = %d", GST_BUFFER_DATA(demux->manifest), GST_BUFFER_SIZE(demux->manifest));
      if (!gst_ssm_parse_manifest (demux->parser, (char *)GST_BUFFER_DATA(demux->manifest), GST_BUFFER_SIZE(demux->manifest))) {
        /* In most cases, this will happen if we set a wrong url in the
         * source element and we have received the 404 HTML response instead of
         * the playlist */
        GST_ELEMENT_ERROR (demux, STREAM, DECODE, ("Invalid playlist."), (NULL));
        goto error;
      }

      {
        unsigned char *protection_data = NULL;
        unsigned int protection_len = 0;

        /* get protection-header from manifest parser */
        ret = gst_ssm_parse_get_protection_header (demux->parser, &protection_data, &protection_len);
        if (!ret) {
          GST_ERROR_OBJECT (demux, "failed to get protection header...");
          GST_ELEMENT_ERROR (demux, RESOURCE, NO_SPACE_LEFT, ("fragment allocation failed..."), (NULL));
          goto error;
        }

        if (protection_data && protection_len) {
          g_print ("Got the protection header...\n");
          demux->protection_header = gst_buffer_new ();
          GST_BUFFER_DATA (demux->protection_header) = GST_BUFFER_MALLOCDATA (demux->protection_header) = protection_data;
          GST_BUFFER_SIZE (demux->protection_header) = protection_len;
        }
      }

      for( i = 0; i < SS_STREAM_NUM; i++) {
        if (gst_ssm_parse_check_stream (demux->parser, i)) {
          GstSSDemuxStream *stream = g_new0 (GstSSDemuxStream, 1);

          // Add pad emission of the stream
          gst_ss_demux_stream_init (demux, stream, i);

          if (!gst_pad_is_linked (stream->pad)) {
            GST_WARNING_OBJECT (demux, "%s - stream pad is not linked...clean up", ssm_parse_get_stream_name(i));
            gst_ss_demux_stream_free (demux, stream);
            continue;
          }

          /* create stream task */
          g_static_rec_mutex_init (&stream->stream_lock);
          stream->stream_task = gst_task_create ((GstTaskFunction) gst_ss_demux_stream_loop, demux);
          if (NULL == stream->stream_task) {
            GST_ERROR_OBJECT (demux, "failed to create stream task...");
            GST_ELEMENT_ERROR (demux, RESOURCE, FAILED, ("failed to create stream task"), (NULL));
            goto error;
          }
          gst_task_set_lock (stream->stream_task, &stream->stream_lock);

          /* create stream push loop */
          if (!gst_pad_start_task (stream->pad, (GstTaskFunction) gst_ss_demux_push_loop, stream)) {
            GST_ERROR_OBJECT (demux, "failed to create push loop...");
            GST_ELEMENT_ERROR (demux, RESOURCE, FAILED, ("failed to create push loop"), (NULL));
            goto error;
          }

          demux->streams[i] = stream;
          g_print ("Starting stream - %d task loop...\n", i);
          gst_task_start (stream->stream_task);
        }
      }

      gst_event_unref (event);
      gst_object_unref (demux);
      return TRUE;
    }
    case GST_EVENT_NEWSEGMENT:
      /* Swallow newsegments, we'll push our own */
      gst_event_unref (event);
      gst_object_unref (demux);
      return TRUE;
    default:
      break;
  }

  return gst_pad_event_default (pad, event);

error:
  // TODO: add closing

  //gst_ss_demux_stop (demux);
  gst_event_unref (event);
  gst_object_unref (demux);

  if (query)
    gst_query_unref (query);

  g_print ("Returning from sink event...\n");
  return FALSE;

}

static gboolean
gst_ss_demux_handle_src_query (GstPad * pad, GstQuery * query)
{
  gboolean res = FALSE;
  GstSSDemux *ssdemux = GST_SS_DEMUX (gst_pad_get_parent (pad));

  GST_LOG_OBJECT (pad, "%s query", GST_QUERY_TYPE_NAME (query));

  // TODO: need to add other query types as well

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_DURATION:{
      GstFormat fmt;

      gst_query_parse_duration (query, &fmt, NULL);
      if (fmt == GST_FORMAT_TIME) {
        gint64 duration = -1;

        duration = gst_util_uint64_scale (GST_SSM_PARSE_GET_DURATION(ssdemux->parser), GST_SECOND,
		GST_SSM_PARSE_GET_TIMESCALE(ssdemux->parser));
        if (duration > 0) {
          gst_query_set_duration (query, GST_FORMAT_TIME, duration);
          res = TRUE;
        }
      }
      break;
    }
    default:
      res = gst_pad_query_default (pad, query);
      break;
  }

  gst_object_unref (ssdemux);

  return res;
}


static gboolean
gst_ss_demux_handle_src_event (GstPad * pad, GstEvent * event)
{
  GstSSDemux *demux = GST_SS_DEMUX (gst_pad_get_parent (pad));

  switch (event->type) {
    case GST_EVENT_SEEK:
    {
      gdouble rate;
      GstFormat format;
      GstSeekFlags flags;
      GstSeekType start_type, stop_type;
      gint64 start, stop;
      gint i = 0;
      GstSSDemuxStream *stream = NULL;

      GST_INFO_OBJECT (demux, "Received GST_EVENT_SEEK");

      // TODO: should be able to seek in DVR window
      if (GST_SSM_PARSE_IS_LIVE_PRESENTATION (demux->parser)) {
        GST_WARNING_OBJECT (demux, "Received seek event for live stream");
        return FALSE;
      }

      gst_event_parse_seek (event, &rate, &format, &flags, &start_type, &start,
          &stop_type, &stop);

      if (format != GST_FORMAT_TIME) {
        GST_WARNING_OBJECT (demux, "Only time format is supported in seek");
        return FALSE;
      }

      GST_DEBUG_OBJECT (demux, "seek event, rate: %f start: %" GST_TIME_FORMAT
          " stop: %" GST_TIME_FORMAT, rate, GST_TIME_ARGS (start),
          GST_TIME_ARGS (stop));


      for( i = 0; i < SS_STREAM_NUM; i++) {
        if (stream = demux->streams[i]) {
          g_cond_signal (stream->cond);
          gst_task_stop (stream->stream_task);
        }
      }

      if (flags & GST_SEEK_FLAG_FLUSH) {
        GST_INFO_OBJECT (demux, "sending flush start");

        for( i = 0; i < SS_STREAM_NUM; i++) {
          if (stream = demux->streams[i]) {
            gst_pad_push_event (stream->pad, gst_event_new_flush_start ());
          }
        }
      }

      gst_ssm_parse_seek_manifest (demux->parser, start);

      if (flags & GST_SEEK_FLAG_FLUSH) {
        GST_INFO_OBJECT (demux, "sending flush stop");
        for( i = 0; i < SS_STREAM_NUM; i++) {
          if (stream = demux->streams[i]) {
            gst_pad_push_event (stream->pad, gst_event_new_flush_stop ());
            GST_LOG_OBJECT (stream->pad, "Starting pad TASK again...\n");
            stream->sent_ns = FALSE;
            stream->frag_cnt = 0; /*resetting to start buffering on SEEK */
            gst_task_start (stream->stream_task);
          }
        }
      }

      return TRUE;
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, event);
}

static GstStateChangeReturn
gst_ss_demux_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    default:
      break;
  }
  return ret;
}


static GstFlowReturn
gst_ss_demux_chain (GstPad * pad, GstBuffer * buf)
{
  GstSSDemux *demux = GST_SS_DEMUX (gst_pad_get_parent (pad));

  if (demux->manifest == NULL)
    demux->manifest = buf;
  else
    demux->manifest = gst_buffer_join (demux->manifest, buf);
  gst_object_unref (demux);

  return GST_FLOW_OK;
}


static gboolean
gst_ss_demux_get_next_fragment (GstSSDemux * demux, SS_STREAM_TYPE stream_type)
{
  GstSSDemuxStream *stream = demux->streams[stream_type];
  gchar *next_fragment_uri = NULL;
  guint64 start_ts = 0;

  if (!gst_ssm_parse_get_next_fragment_url (demux->parser, stream_type, &next_fragment_uri, &start_ts )) {
    GST_INFO_OBJECT (demux, "This Manifest does not contain more fragments");
    goto end_of_list;
  }

  GST_ERROR_OBJECT (demux, "Fetching next fragment %s", next_fragment_uri);

  stream->uri = g_strdup(next_fragment_uri);
  stream->start_ts = start_ts;

  if (!gst_ss_demux_download_fragment (demux, stream, next_fragment_uri, start_ts)) {
    GST_ERROR_OBJECT (demux, "failed to download fragment...");
    goto error;
  }

  return TRUE;

error:
  {
    GST_ELEMENT_ERROR (demux, RESOURCE, FAILED, ("failed to download fragment"), (NULL));
    gst_ss_demux_stop (demux, stream);
    return FALSE;
  }
end_of_list:
  {
    GST_INFO_OBJECT (demux, "Reached end of playlist, sending EOS");
    demux->eos = TRUE;
    gst_ss_demux_stop (demux, stream);
    return TRUE;
  }
}

static void
gst_ss_demux_push_loop (GstSSDemuxStream *stream)
{
  GstBuffer *outbuf = NULL;
  GstSSDemux *demux = stream->parent;
  GstFlowReturn fret = GST_FLOW_OK;

  // TODO: need to take care of EOS handling....

  g_mutex_lock (stream->queue_lock);

  if (g_queue_is_empty (stream->queue)) {
    GST_DEBUG_OBJECT (stream->pad,"queue is empty wait till, some buffers are available...");
    if (demux->eos) {
      GST_INFO_OBJECT (stream->pad, "stream EOS, pause the task");
      gst_pad_push_event (stream->pad, gst_event_new_eos ());
      gst_pad_pause_task (stream->pad);
      g_print ("Paused the task");
      return;
    }
    g_cond_wait (stream->queue_empty, stream->queue_lock);
  }

  outbuf = g_queue_pop_head (stream->queue);

  if (GST_BUFFER_DURATION_IS_VALID (outbuf)) {
    stream->cached_duration -= GST_BUFFER_DURATION(outbuf);
  } else {
    g_print ("\nDuration field is not valid.. check this issue !!!!!!!!\n");
    GST_ELEMENT_ERROR (demux, STREAM, DECODE, ("Invalid duration of a frame"), (NULL));
    g_mutex_unlock (stream->queue_lock);
    return;
  }

  g_cond_signal (stream->queue_full);
  //g_print ("[%s] Signalled full condition...\n", ssm_parse_get_stream_name(stream->type));
  g_mutex_unlock (stream->queue_lock);

  if (!stream->sent_ns) {
    guint64 duration = GST_CLOCK_TIME_NONE;
    guint64 start = GST_CLOCK_TIME_NONE;
    GstEvent *event = NULL;

    duration = gst_util_uint64_scale (GST_SSM_PARSE_GET_DURATION(demux->parser), GST_SECOND,
                                      GST_SSM_PARSE_GET_TIMESCALE(demux->parser));

    start = gst_util_uint64_scale (GST_SSM_PARSE_NS_START(demux->parser), GST_SECOND,
                                   GST_SSM_PARSE_GET_TIMESCALE(demux->parser));

    event = gst_event_new_new_segment (FALSE, 1.0, GST_FORMAT_TIME, start, duration, start);

    GST_DEBUG_OBJECT(demux," new_segment start = %"GST_TIME_FORMAT, GST_TIME_ARGS(start));

    if (!gst_pad_push_event (stream->pad, event)) {
      GST_ERROR_OBJECT (demux, "failed to push newsegment event");
      return; // No need to close task for this, because sometimes pad can unlined
    }
    stream->sent_ns = TRUE;
  }

  if (stream->type == SS_STREAM_VIDEO && demux->ss_mode == SS_MODE_AONLY) {
    GST_BUFFER_TIMESTAMP (outbuf) = stream->switch_ts;
    GST_BUFFER_DURATION (outbuf) = ((float)1/25) * GST_SECOND;
    stream->switch_ts = GST_BUFFER_TIMESTAMP (outbuf) + GST_BUFFER_DURATION (outbuf);
    g_print ("Dummy buffers ts : %"GST_TIME_FORMAT" and dur : %"GST_TIME_FORMAT"\n",
             GST_TIME_ARGS(GST_BUFFER_TIMESTAMP (outbuf)), GST_TIME_ARGS(GST_BUFFER_DURATION (outbuf)));
    gchar *caps_string = gst_caps_to_string(GST_BUFFER_CAPS(outbuf));
    g_print ("caps : %s\n", caps_string);
    g_free(caps_string);
    caps_string = NULL;
  }

  /* push data to downstream*/
  fret = gst_pad_push (stream->pad, outbuf);
  if (fret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (demux, "failed to push data, reason : %s", gst_flow_get_name (fret));
    goto error;
  }

  //g_print ("[%s] pushed buffer\n", ssm_parse_get_stream_name(stream->type));
error:
  // TODO: need to close task & post error to bus
  return;
}

static void
gst_ss_demux_stream_loop (GstSSDemux * demux)
{
  GThread *self = NULL;
  int stream_type = 0;
  GstSSDemuxStream *stream = NULL;

  self = g_thread_self ();

  for (stream_type = 0; stream_type < SS_STREAM_NUM; stream_type++) {
    if (demux->streams[stream_type] && demux->streams[stream_type]->stream_task->abidata.ABI.thread == self) {
      stream = demux->streams[stream_type];
      break;
    }
  }

  if (stream) {
    /* download next fragment of stream_type */
    if (!gst_ss_demux_get_next_fragment (demux, stream_type)) {
      GST_ERROR_OBJECT (demux, "failed to get next fragment...");
      goto error;
    }
  }

  return;

error:
  {
    gst_task_pause (stream->stream_task);
    GST_ELEMENT_ERROR (demux, RESOURCE, NOT_FOUND,
          ("could not download fragments"), (NULL));
      gst_ss_demux_stop (demux, stream);
    return;
  }
}

static gboolean
gst_ss_demux_download_fragment (GstSSDemux *demux, GstSSDemuxStream *stream, const gchar * uri, guint64 start_ts)
{
  GstStateChangeReturn ret;
  GTimeVal time = {0, };

  g_print ("Going to download fragment : %s\n", uri);
  if (!gst_ss_demux_create_download_pipe (demux, stream, uri, start_ts)) {
    GST_ERROR_OBJECT (demux, "failed to create download pipeline");
    return FALSE;
  }

  /* download rate calculation : note down start time*/
  g_get_current_time (&time);
  stream->download_start_ts = (time.tv_sec * 1000000)+ time.tv_usec;

  ret = gst_element_set_state (stream->pipe, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    GST_ERROR_OBJECT (demux, "set_state failed...");
    return FALSE;
  }

  if (stream->pipe && demux->ss_mode == SS_MODE_AONLY &&
    stream->type == SS_STREAM_VIDEO) {

    GST_DEBUG_OBJECT (demux, "Waiting to fetch the URI");
    g_mutex_lock (stream->lock);
    g_cond_wait (stream->cond, stream->lock);
    GST_INFO_OBJECT (stream->pad, "Recived signal to shutdown...");
    g_mutex_unlock (stream->lock);

    /* put live pipeline to PAUSED state to unlink urisrc & piffdemux */
    gst_element_set_state (stream->pipe, GST_STATE_NULL);
    gst_element_get_state (stream->pipe, NULL, NULL, GST_CLOCK_TIME_NONE);

    stream->pipe = NULL;

    stream->switch_ts = stream->start_ts;

    /* create dummy frame sender */
    if (!gst_ss_demux_create_dummy_sender (demux, stream)) {
      GST_ERROR_OBJECT (demux, "failed to create dummy sender pipeline...");
      GST_ELEMENT_ERROR (demux, RESOURCE, FAILED, ("Unable to create dummy pipe."), (NULL));
      return FALSE;
    }
  }

  /* wait until:
   *   - the download succeed (EOS)
   *   - the download failed (Error message on the fetcher bus)
   *   - the download was canceled
   */
  GST_DEBUG_OBJECT (demux, "Waiting to fetch the URI");
  g_mutex_lock (stream->lock);
  g_cond_wait (stream->cond, stream->lock);
  GST_INFO_OBJECT (stream->pad, "Recived signal to shutdown...");
  g_mutex_unlock (stream->lock);

  gst_element_set_state (stream->pipe, GST_STATE_NULL);
  gst_element_get_state (stream->pipe, NULL, NULL, GST_CLOCK_TIME_NONE);
  stream->pipe = NULL;

  return TRUE;
}

static gboolean
gst_ss_demux_create_dummy_sender(GstSSDemux *demux, GstSSDemuxStream *stream)
{
  GstStateChangeReturn ret;

  if (!gst_ss_demux_create_dummy_pipe (demux, stream)) {
    GST_ERROR_OBJECT (demux, "failed to create download pipeline");
    return FALSE;
  }

  ret = gst_element_set_state (stream->pipe, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    GST_ERROR_OBJECT (demux, "set_state failed...");
    return FALSE;
  }

#if 0
  GST_DEBUG_OBJECT (demux, "Waiting to download next video URI");
  g_mutex_lock (stream->lock);
  g_cond_wait (stream->cond, stream->lock);
  if (stream->pipe) {
    gst_element_set_state (stream->pipe, GST_STATE_NULL);
    gst_element_get_state (stream->pipe, NULL, NULL, GST_CLOCK_TIME_NONE);
    stream->pipe = NULL;
  }
  g_mutex_unlock (stream->lock);
#endif

  return TRUE;
}

static void
gst_ss_demux_append_live_params(GstElement *piffparser, piff_live_param_t *param, gpointer data)
{
  GstSSDemuxStream *stream = (GstSSDemuxStream *)data;
  GstSSDemux *demux = stream->parent;
  int i =0;
  guint64 timestamp = 0;
  guint64 duration = 0;

  GST_LOG_OBJECT (demux, "received signal structs count = %d\n", param->count);

  for (i = 0 ; i< param->count; i++) {
    if (param->long_info) {
      piff_fragment_longtime_info *info = &(param->long_info[i]);
      timestamp = info->ts;
      duration = info->duration;
    } else if (param->info) {
      piff_fragment_time_info *info = &(param->info[i]);
      timestamp = info->ts;
      duration = info->duration;
    }

    GST_LOG_OBJECT (demux, "Received ts = %llu and dur = %llu\n", timestamp, duration);

    if (!gst_ssm_parse_append_next_fragment (demux->parser, stream->type, timestamp, duration)) {
      GST_ERROR_OBJECT (demux, "failed to append new fragment");
      GST_ELEMENT_ERROR (demux, RESOURCE, NO_SPACE_LEFT, ("fragment allocation failed..."), (NULL));
      return;
    }
  }

  if (param->long_info) {
    free (param->long_info);
    param->long_info = NULL;
  }

  if (param->info) {
    free (param->info);
    param->info = NULL;
  }

  free (param);

  if ((stream->type == SS_STREAM_VIDEO) && (demux->ss_mode == SS_MODE_AONLY)) {
    g_print ("\n\n\t\tSignalling download pipe shutdonw....\n\n");

    g_object_get (stream->parser, "frame-dur", &stream->avg_dur, NULL);
    g_print ("frame duration = %"GST_TIME_FORMAT"\n\n\n", GST_TIME_ARGS(stream->avg_dur));
    g_cond_signal (stream->cond);
  }

}

static gboolean
gst_ss_demux_create_download_pipe (GstSSDemux * demux, GstSSDemuxStream *stream, const gchar * uri, guint64 start_ts)
{
  gchar *name = NULL;
  gchar *caps_string = NULL;

  if (!gst_uri_is_valid (uri))
    return FALSE;

  name = g_strdup_printf("%s-%s", stream->name, "downloader");

  stream->pipe = gst_pipeline_new (name);
  if (!stream->pipe) {
    GST_ERROR_OBJECT (demux, "failed to create pipeline");
    g_free(name);
    name = NULL;
    return FALSE;
  }

  name = g_strdup_printf("%s-%s", stream->name, "httpsrc");
  GST_DEBUG ("Creating source element for the URI:%s", uri);
  stream->urisrc = gst_element_make_from_uri (GST_URI_SRC, uri, name);
  if (!stream->urisrc) {
    GST_ERROR_OBJECT (demux, "failed to create urisrc");
    g_free(name);
    return FALSE;
  }

  if (GST_SSM_PARSE_IS_LIVE_PRESENTATION(demux->parser))
    g_object_set (G_OBJECT (stream->urisrc), "is-live", TRUE, NULL);
  else
    g_object_set (G_OBJECT (stream->urisrc), "is-live", FALSE, NULL);

  name = g_strdup_printf("%s-%s", stream->name, "parser");
  stream->parser = gst_element_factory_make ("piffdemux", name);
  if (!stream->parser) {
    GST_ERROR_OBJECT (demux, "failed to create piffdemux element");
    g_free(name);
    name = NULL;
    return FALSE;
  }

  if (stream->caps)
    gst_caps_unref (stream->caps);

  stream->caps = ssm_parse_get_stream_caps (demux->parser, stream->type);
  caps_string = gst_caps_to_string(stream->caps);
  GST_INFO_OBJECT (stream->pad, "prepare caps = %s", caps_string);
  g_free(caps_string);
  caps_string = NULL;

  g_object_set (G_OBJECT (stream->parser), "caps", stream->caps, NULL);
  g_object_set (G_OBJECT (stream->parser), "start-ts", start_ts, NULL);
  g_object_set (G_OBJECT (stream->parser), "duration", GST_SSM_PARSE_GET_DURATION(demux->parser), NULL);
  g_object_set (G_OBJECT (stream->parser), "is-live", GST_SSM_PARSE_IS_LIVE_PRESENTATION(demux->parser), NULL);
  g_object_set (G_OBJECT (stream->parser), "lookahead-count", GST_SSM_PARSE_LOOKAHEAD_COUNT(demux->parser), NULL);
  if (demux->protection_header)
    g_object_set (G_OBJECT (stream->parser), "protection-header", demux->protection_header, NULL);
  g_signal_connect (stream->parser, "live-param",  G_CALLBACK (gst_ss_demux_append_live_params), stream);

  name = g_strdup_printf("%s-%s", stream->name, "sink");
  stream->sink = gst_element_factory_make ("appsink", name);
  if (!stream->sink) {
    GST_ERROR_OBJECT (demux, "failed to create appsink element");
    g_free(name);
    name = NULL;
    return FALSE;
  }
  g_object_set (G_OBJECT (stream->sink), "emit-signals", TRUE, "sync", FALSE, NULL);
  g_signal_connect (stream->sink, "new-buffer",  G_CALLBACK (gst_ssm_demux_on_new_buffer), stream);

  gst_bin_add_many (GST_BIN (stream->pipe), stream->urisrc, stream->parser, stream->sink, NULL);
  if (!gst_element_link_many (stream->urisrc, stream->parser, stream->sink, NULL)) {
    GST_ERROR ("failed to link elements...");
    return FALSE;
  }

  stream->bus = gst_pipeline_get_bus (GST_PIPELINE (stream->pipe));
  gst_bus_add_watch (stream->bus, (GstBusFunc)gst_ss_demux_download_bus_cb, stream);
  gst_object_unref (stream->bus);

  g_free(name);
  name = NULL;

  return TRUE;
}

#if 0
static gboolean
gst_ss_demux_create_dummy_pipe (GstSSDemux * demux, GstSSDemuxStream *stream)
{
  gchar *name = NULL;
  GstCaps *caps = NULL;
  GstElement *capsfilter = NULL;
  GstElement *enc = NULL;
  guint64 avg_dur = -1;
  guint frame_rate = 0;

  name = g_strdup_printf("%s-%s", stream->name, "dummy");

  stream->pipe = gst_pipeline_new (name);
  if (!stream->pipe) {
    GST_ERROR_OBJECT (demux, "failed to create pipeline");
    return FALSE;
  }
  g_free(name);

  /* create dummy sender source */
  name = g_strdup_printf("%s-%s", stream->name, "dummysrc");
  stream->urisrc = gst_element_factory_make ("imagereader", name);
  if (!stream->urisrc) {
    GST_ERROR_OBJECT (demux,"failed to create filesrc element");
    return FALSE;
  }
  g_free(name);
  g_object_set (G_OBJECT (stream->urisrc), "location", "/opt/home/root/aonly_VGA_1frame_I420.yuv", NULL);
  g_object_set (G_OBJECT (stream->urisrc), "framerate", 25, NULL);
  g_object_set (G_OBJECT (stream->urisrc), "num-buffers", 60, NULL);

  /* caps filter */
  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  if (!capsfilter) {
    GST_ERROR_OBJECT (demux, "failed to create capsfilter element");
    return FALSE;
  }
  caps = gst_caps_new_simple ("video/x-raw-yuv",
                  "width", G_TYPE_INT, 640,
                  "height", G_TYPE_INT, 480,
                  "framerate",GST_TYPE_FRACTION, 25,1,
                  "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('I', '4', '2', '0'),
                  NULL);
  g_object_set (G_OBJECT (capsfilter), "caps", caps,  NULL);

  /* create h264parse element */
  enc = gst_element_factory_make ("savsenc_h264", "H264 encoder");
  if (!enc) {
    GST_ERROR_OBJECT (demux, "failed to create h264 parse element");
    return FALSE;
  }
  name = g_strdup_printf("%s-%s", stream->name, "sink");
  stream->sink = gst_element_factory_make ("appsink", name);
  if (!stream->sink) {
    GST_ERROR_OBJECT (demux, "failed to create appsink element");
    return FALSE;
  }
  g_free(name);
  g_object_set (G_OBJECT (stream->sink), "emit-signals", TRUE, "sync", FALSE, NULL);
  g_signal_connect (stream->sink, "new-buffer",  G_CALLBACK (gst_ssm_demux_on_new_buffer), stream);

  /* add to pipeline & link all elements */
  gst_bin_add_many (GST_BIN (stream->pipe), stream->urisrc, capsfilter, enc, stream->sink, NULL);

  if (!gst_element_link_many (stream->urisrc, capsfilter, enc, stream->sink, NULL)) {
    GST_ERROR_OBJECT (demux,"failed to link dummy pipe elements...");
    return FALSE;
  }

  stream->bus = gst_pipeline_get_bus (GST_PIPELINE (stream->pipe));
  gst_bus_add_watch (stream->bus, (GstBusFunc)gst_ss_demux_download_bus_cb, stream);
  gst_object_unref (stream->bus);

  return TRUE;
}
#else
static gboolean
gst_ss_demux_create_dummy_pipe (GstSSDemux * demux, GstSSDemuxStream *stream)
{
  gchar *name = NULL;
  GstBus *bus = NULL;
  GstCaps *caps = NULL;

  name = g_strdup_printf("%s-%s", stream->name, "dummy");

  stream->pipe = gst_pipeline_new (name);
  if (!stream->pipe) {
    GST_ERROR_OBJECT (demux, "failed to create pipeline");
    return FALSE;
  }
  g_free(name);

  /* create dummy sender source */
  name = g_strdup_printf("%s-%s", stream->name, "dummysrc");
  stream->urisrc = gst_element_factory_make ("filesrc", name);
  if (!stream->urisrc) {
    GST_ERROR_OBJECT (demux,"failed to create filesrc element");
    return FALSE;
  }
  g_free(name);
  g_object_set (G_OBJECT (stream->urisrc), "location", "/opt/home/root/sound_2sec.264", NULL);

  /* create appsink element */
  name = g_strdup_printf("%s-%s", stream->name, "parser");
  stream->parser= gst_element_factory_make ("legacyh264parse", name);
  if (!stream->parser) {
    GST_ERROR_OBJECT (demux, "failed to create h264 parse element");
    return FALSE;
  }
  g_object_set (G_OBJECT (stream->parser), "output-format", 1, NULL);

  /* create appsink element */
  name = g_strdup_printf("%s-%s", stream->name, "sink");
  stream->sink = gst_element_factory_make ("appsink", name);
  if (!stream->sink) {
    GST_ERROR_OBJECT (demux, "failed to create appsink element");
    return FALSE;
  }
  g_object_set (G_OBJECT (stream->sink), "emit-signals", TRUE, "sync", FALSE, NULL);

  caps = gst_caps_new_simple ("video/x-h264",
                  "width", G_TYPE_INT, 640,
                  "height", G_TYPE_INT, 480,
                  "stream-format", G_TYPE_STRING, "byte-stream",
                  NULL);
  g_object_set (G_OBJECT (stream->sink), "caps", caps, NULL);

  g_signal_connect (stream->sink, "new-buffer",  G_CALLBACK (gst_ssm_demux_on_new_buffer), stream);
  g_free(name);

  /* add to pipeline & link all elements */
  gst_bin_add_many (GST_BIN (stream->pipe), stream->urisrc, stream->parser, stream->sink, NULL);
  if (!gst_element_link_many (stream->urisrc, stream->parser, stream->sink, NULL)) {
    GST_ERROR_OBJECT (demux,"failed to link elements...");
    return FALSE;
  }

  bus = gst_pipeline_get_bus (GST_PIPELINE (stream->pipe));
  gst_bus_add_watch (bus, (GstBusFunc)gst_ss_demux_download_bus_cb, stream);
  gst_object_unref (bus);

  return TRUE;
}


#endif
static gboolean
gst_ss_demux_download_bus_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
  GstSSDemuxStream *stream = (GstSSDemuxStream *)data;
  GstSSDemux *demux = stream->parent;

  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS: {
      GTimeVal time = {0, };
      gint idx = 0;
      guint64 total_push_time = 0;
      guint64 download_rate = 0;

      GST_INFO_OBJECT (stream->pad, "received EOS on download pipe..");
      // increase the fragment count on EOS
      stream->frag_cnt++;

      /* download rate calculation : note down start time*/
      g_get_current_time (&time);
      stream->download_stop_ts = (time.tv_sec * 1000000)+ time.tv_usec;

      download_rate = ((stream->download_size * 8 * 1000000) / (stream->download_stop_ts - stream->download_start_ts - stream->push_block_time));
      g_print("*********** '%s' download rate = %"G_GUINT64_FORMAT" bpssss **************\n", stream->name, download_rate);
      stream->download_size = 0;
      stream->download_stop_ts = stream->download_start_ts = 0;
      stream->push_block_time = 0;

      if ((stream->type == SS_STREAM_VIDEO) && (demux->ss_mode != SS_MODE_AONLY)) {
        if (!stream->is_buffering) {
          /* for switching, we are considering video download rate only */
          demux->ss_mode = gst_ssm_parse_switch_qualitylevel (demux->parser, download_rate);
        }
      } else if (stream->type == SS_STREAM_AUDIO && (demux->ss_mode == SS_MODE_AONLY)) {
        /* when video is not present using audio download rate to calculate switching */
         demux->ss_mode = gst_ssm_parse_switch_qualitylevel (demux->parser, download_rate);
         if (demux->ss_mode != SS_MODE_AONLY) {
           g_print ("\n\nMoving to AV mode by audio considering audio download rate\n\n\n\n");
         }
      }

      g_cond_signal (stream->cond);

#ifdef SIMULATE_AUDIO_ONLY
      /* when fragment count is multiple of 4, switch to audio only case */
      if ((stream->frag_cnt % 4 == 0) && (stream->type == SS_STREAM_VIDEO) &&
	  	GST_SSM_PARSE_IS_LIVE_PRESENTATION(demux->parser)) {
        g_print ("\n\t ######## Forcibly switching to audio only for testing ##########\n");
        demux->ss_mode = SS_MODE_AONLY;
      }
  #endif
      GST_DEBUG_OBJECT (stream->pad, "Signalling eos condition...");

      GST_DEBUG_OBJECT (demux, "number of fragments downloaded = %d", stream->frag_cnt);
      break;
    }
    case GST_MESSAGE_ERROR: {
      GError *error = NULL;
      gchar* debug = NULL;

      g_print ("Error from %s\n", gst_element_get_name (GST_MESSAGE_SRC(msg)));

      gst_message_parse_error( msg, &error, &debug);
      if (error)
        GST_ERROR_OBJECT (demux, "GST_MESSAGE_ERROR: error= %s\n", error->message);

      GST_ERROR_OBJECT (demux, "GST_MESSAGE_ERROR: debug = %s\n", debug);

      /* handling error, when client requests url, which is yet to be prepared by server */
      if (GST_IS_URI_HANDLER(GST_MESSAGE_SRC(msg))) {
        GstStateChangeReturn ret;

        /* wait for 1sec & request the url again */
        // TODO: need to make wait time as generic or Adding loop count to request again & again
        if (error)
          GST_INFO_OBJECT (demux, "ERROR : code = %d, msg = %s, NEED to request again", error->code, error->message);

        usleep (1000000); // 1 sec

        /* put the current pipeline to NULL state */
        gst_element_set_state (stream->pipe, GST_STATE_NULL);
        gst_element_get_state (stream->pipe, NULL, NULL, GST_CLOCK_TIME_NONE);
        stream->pipe = stream->urisrc = stream->parser = stream->sink = NULL;

        g_print ("Going to download fragment AGAIN : %s\n", stream->uri);
        if (!gst_ss_demux_create_download_pipe (demux, stream, stream->uri, stream->start_ts)) {
          GST_ERROR_OBJECT (demux, "failed to create download pipeline");
          if (!gst_element_post_message (GST_ELEMENT(demux), msg)) {
            GST_ERROR_OBJECT (demux, "failed to post error");
            g_free(debug);
            debug = NULL;

            return FALSE;
          }
        }

        ret = gst_element_set_state (stream->pipe, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
          if (!gst_element_post_message (GST_ELEMENT(demux), msg)) {
            GST_ERROR_OBJECT (demux, "failed to post error");
            return FALSE;
          }
        }

        } else {
          if (error)
          g_print ("GST_MESSAGE_ERROR: error= %s\n", error->message);

          g_print ("GST_MESSAGE_ERROR: debug = %s\n", debug);
          if (!gst_element_post_message (GST_ELEMENT(demux), msg)) {
            GST_ERROR_OBJECT (demux, "failed to post error");
            gst_ss_demux_stop (demux, stream);
            g_free(debug);
            debug = NULL;
            g_error_free(error);
            return FALSE;
        }
        gst_ss_demux_stop (demux, stream);
      }

      g_free( debug);
      debug = NULL;
      g_error_free( error);
      break;
    }
    case GST_MESSAGE_BUFFERING: {
      int n =0;
      int total_cache_perc = 0;
      int active_stream_cnt = 0;
      GstSSDemuxStream *cur_stream = NULL;
      int avg_percent = 0;

      /* update buffer percent */
      gst_message_parse_buffering (msg, &stream->rcvd_percent);
      gchar *name = gst_element_get_name (GST_MESSAGE_SRC (msg));
      GST_LOG_OBJECT (stream->pad, "Internal bus : Buffering from %s = %d\n", name, stream->rcvd_percent);
      g_free(name);
      name = NULL;
      // TODO: need to check for better logic
      for (n = 0; n < SS_STREAM_NUM; n++) {
        cur_stream = demux->streams[n];
        if (cur_stream) {
          active_stream_cnt++;
          total_cache_perc += cur_stream->rcvd_percent;
        }
      }

      avg_percent = total_cache_perc / active_stream_cnt;

      GST_LOG_OBJECT (demux, "avg buffering completed = %d", avg_percent);

      if (avg_percent > 100)
        avg_percent = 100;

      // TODO: need to add mutex for protecting percent
      if (avg_percent != demux->percent) {
        demux->percent = avg_percent;
        GST_LOG_OBJECT (demux, "#########Posting %d buffering msg to main bus ###########", demux->percent);

        gst_element_post_message (GST_ELEMENT (demux), gst_message_new_buffering (GST_OBJECT (demux), avg_percent));
      }
    }
    break;
    case GST_MESSAGE_WARNING: {
      char* debug = NULL;
      GError* error = NULL;
      gst_message_parse_warning(msg, &error, &debug);
      GST_WARNING_OBJECT(demux, "warning : %s\n", error->message);
      GST_WARNING_OBJECT(demux, "debug : %s\n", debug);
      g_error_free( error );
      g_free( debug);
      break;
    }
    default : {
      GST_LOG_OBJECT(demux, "unhandled message : %s\n", gst_message_type_get_name (GST_MESSAGE_TYPE (msg)));
      break;
    }
  }

  return TRUE;
}

static void
gst_ss_demux_update_buffering (GstSSDemuxStream *stream, guint64 percent)
{
  gboolean do_post = FALSE;
  GstSSDemux *demux = stream->parent;

  if (stream->is_buffering) {
    do_post = TRUE;
    if (percent >= demux->high_percent)
      stream->is_buffering = FALSE;
  } else {
    if (percent < demux->low_percent) {
      stream->is_buffering = TRUE;
      do_post = TRUE;
    }
  }

  if (do_post) {
    GstMessage *message;
    GstBufferingMode mode;
    gint64 buffering_left = -1;

    percent = percent * 100 / demux->high_percent;

    if (percent > 100)
      percent = 100;

    if (percent != stream->percent) {
      stream->percent = percent;

      GST_DEBUG_OBJECT (stream->pad, "buffering %d percent", (gint) percent);
      g_print ("'%s' buffering %d percent done\n", stream->name, (gint) percent);

      /* posting buffering to internal bus, which will take average & post to main bus */
      message = gst_message_new_buffering (GST_OBJECT_CAST (stream->sink), (gint) percent);
      gst_element_post_message (GST_ELEMENT_CAST (stream->sink), message);
    }
  }

}

static void
gst_ssm_demux_on_new_buffer (GstElement * appsink, void* data)
{
  GstSSDemuxStream *stream = (GstSSDemuxStream *)data;
  GstSSDemux *demux = stream->parent;
  GstBuffer *inbuf = NULL;
  GstFlowReturn fret = GST_FLOW_OK;
  GstBuffer *headbuf = NULL;
  gint64 diff = 0;
  gint64 percent = 0;
  GTimeVal start = {0, };
  GTimeVal stop = {0, };
  guint64 push_start_time = 0;
  guint64 push_end_time =0;

  inbuf = gst_app_sink_pull_buffer ((GstAppSink *)appsink);
  if (!inbuf) {
    GST_WARNING_OBJECT (demux, "Input buffer not available.,..\n");
    return;
  }

  g_mutex_lock (stream->queue_lock);

  stream->download_size += GST_BUFFER_SIZE(inbuf);

  /* download rate calculation : note push_start_ts */
  g_get_current_time (&start);
  push_start_time = (start.tv_sec * 1000000)+ start.tv_usec;

  GST_LOG_OBJECT (stream->pad, "Inbuf : size = %d, ts = %"GST_TIME_FORMAT", dur = %"GST_TIME_FORMAT,
      GST_BUFFER_SIZE(inbuf), GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(inbuf)), GST_TIME_ARGS(GST_BUFFER_DURATION(inbuf)));

  g_queue_push_tail (stream->queue, inbuf);

  if (GST_BUFFER_DURATION_IS_VALID (inbuf)) {
    stream->cached_duration += GST_BUFFER_DURATION(inbuf);
  } else {
    g_print ("\nDuration field is not valid.. check this issue !!!!!!!!\n");
    GST_ELEMENT_ERROR (demux, STREAM, DECODE, ("Invalid duration of a frame"), (NULL));
    g_mutex_unlock (stream->queue_lock);
    return;
  }

  if (stream->cached_duration >= 0) {
    percent = (stream->cached_duration * 100) / demux->max_cache_time;
    //g_print ("[%s] percent done = %d[%"G_GINT64_FORMAT"]\n", ssm_parse_get_stream_name(stream->type), percent, percent);

    // TODO: need to decide, whther to call before wait or after ??
    gst_ss_demux_update_buffering (stream, percent);

    if (percent > 100) {
      /* update buffering & wait if space is not available */
      GST_DEBUG_OBJECT (stream->pad, "Reached more than 100 percent, queue full & wait till free");
      g_cond_wait(stream->queue_full, stream->queue_lock);
      GST_DEBUG_OBJECT (stream->pad,"Received signal to add more data...");
    }
  } else {
    g_print ("cached duration can not be negative\n\n\n");
    GST_ELEMENT_ERROR (demux, STREAM, DECODE, ("Invalid cached duration"), (NULL));
    g_mutex_unlock (stream->queue_lock);
    return;
  }

  /* download rate calculation : note push_stop_ts */
  g_get_current_time (&stop);
  push_end_time = (stop.tv_sec * 1000000)+ stop.tv_usec;

  stream->push_block_time += push_end_time - push_start_time;

  g_cond_signal (stream->queue_empty);

  g_mutex_unlock (stream->queue_lock);
  return;
}

static void
gst_ss_demux_stop (GstSSDemux * demux, GstSSDemuxStream *stream)
{
  if (GST_TASK_STATE (stream->stream_task) != GST_TASK_STOPPED)
    gst_task_stop (stream->stream_task);
}

static void
gst_ss_demux_stream_init (GstSSDemux *demux, GstSSDemuxStream *stream, SS_STREAM_TYPE stream_type)
{
  stream->cond = g_cond_new ();
  stream->lock = g_mutex_new ();
  stream->queue = g_queue_new ();
  stream->queue_full = g_cond_new ();
  stream->queue_empty = g_cond_new ();
  stream->queue_lock = g_mutex_new ();
  stream->parent = demux;
  stream->pipe = NULL;
  stream->urisrc = NULL;
  stream->parser = NULL;
  stream->sink = NULL;
  stream->frag_cnt = 0;
  stream->type = stream_type ;
  stream->uri = NULL;
  stream->start_ts = -1;
  stream->sent_ns = FALSE;
  stream->switch_ts = GST_CLOCK_TIME_NONE;
  stream->avg_dur = GST_CLOCK_TIME_NONE;
  stream->percent = 100;
  stream->rcvd_percent = 0;
  stream->push_block_time = 0;
  stream->cached_duration = 0;
  stream->download_start_ts = 0;
  stream->download_stop_ts = 0;
  stream->download_size = 0;

  if (stream->type == SS_STREAM_VIDEO) {
    stream->pad = gst_pad_new_from_static_template (&ssdemux_videosrc_template, "video");
    stream->name = g_strdup("video");
  } else if (stream->type == SS_STREAM_AUDIO) {
    stream->pad = gst_pad_new_from_static_template (&ssdemux_audiosrc_template, "audio");
    stream->name = g_strdup("audio");
  } else if (stream->type == SS_STREAM_TEXT) {
    stream->pad = gst_pad_new_from_static_template (&ssdemux_subsrc_template, "subtitle");
    stream->name = g_strdup("text");
  }

  GST_PAD_ELEMENT_PRIVATE (stream->pad) = stream;

  gst_pad_use_fixed_caps (stream->pad);
  gst_pad_set_event_function (stream->pad, gst_ss_demux_handle_src_event);
  gst_pad_set_query_function (stream->pad, gst_ss_demux_handle_src_query);

  stream->caps = ssm_parse_get_stream_caps (demux->parser, stream->type);
  gchar *caps_name = gst_caps_to_string(stream->caps);
  g_print ("prepare video caps = %s", caps_name);
  g_free(caps_name);

  GST_DEBUG_OBJECT (demux, "setting caps %" GST_PTR_FORMAT, stream->caps);
  gst_pad_set_caps (stream->pad, stream->caps);

  GST_DEBUG_OBJECT (demux, "adding pad %s %p to demux %p", GST_OBJECT_NAME (stream->pad), stream->pad, demux);
  gst_pad_set_active (stream->pad, TRUE);
  gst_element_add_pad (GST_ELEMENT_CAST (demux), stream->pad);
}

static void
gst_ss_demux_stream_free (GstSSDemux * demux, GstSSDemuxStream * stream)
{
  if (stream->queue) {
    while (!g_queue_is_empty(stream->queue)) {
      gst_buffer_unref (g_queue_pop_head (stream->queue));
    }
    g_queue_free (stream->queue);
    stream->queue = NULL;
  }

  if (stream->pad) {
    gst_element_remove_pad (GST_ELEMENT_CAST (demux), stream->pad);
    stream->pad = NULL;
  }
  if (stream->cond) {
    g_cond_free (stream->cond);
    stream->cond = NULL;
  }
  if (stream->lock) {
    g_mutex_free (stream->lock);
    stream->lock = NULL;
  }
  if (stream->queue_lock) {
    g_mutex_free (stream->queue_lock);
    stream->queue_lock = NULL;
  }
  if (stream->queue_full) {
    g_cond_free (stream->queue_full);
    stream->queue_full = NULL;
  }
  if (stream->queue_empty) {
    g_cond_free (stream->queue_empty);
    stream->queue_empty= NULL;
  }
  g_free (stream);
}
static gboolean
ssdemux_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "ssdemux", GST_RANK_PRIMARY,
          GST_TYPE_SS_DEMUX) || FALSE)
    return FALSE;
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "ssdemux",
    "Smooth streaming demux plugin",
    ssdemux_init, VERSION, "LGPL", PACKAGE_NAME, "http://www.samsung.com/")

