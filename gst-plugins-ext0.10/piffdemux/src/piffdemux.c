

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "piffdemux.h"
#include <glib/gprintf.h>
#include <gst/tag/tag.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <piffatomparser.h>
#include <piffdemux_fourcc.h>
#include <piffpalette.h>
#include <piffdemux_types.h>
#include <piffdemux_dump.h>

#define PIFF_DEFAULT_TRACKID   -1
#define PIFF_DEFAULT_FOURCC   0
#define PIFF_DEFAULT_TIMESCALE 10000000
#define PIFF_DEFAULT_DURATION -1
#define PIFF_DEFAULT_START_TS 0
#define PIFF_DEFAULT_START_TS 0

#define PIFF_DEFAULT_WIDTH 16
#define PIFF_DEFAULT_HEIGHT 16
#define PIFF_DEFAULT_BPS 16

#undef DEC_OUT_FRAME_DUMP

#ifdef DEC_OUT_FRAME_DUMP
#include <stdio.h>
FILE *piffdump = NULL;
#endif

#define PIFFDEMUX_RB16(x)	((((const unsigned char*)(x))[0] << 8) | ((const unsigned char*)(x))[1])
/* max. size considered 'sane' for non-mdat atoms */
#define PIFFDEMUX_MAX_ATOM_SIZE (25*1024*1024)

/* if the sample index is larger than this, something is likely wrong */
#define PIFFDEMUX_MAX_SAMPLE_INDEX_SIZE (50*1024*1024)

GST_DEBUG_CATEGORY (piffdemux_debug);

typedef struct _PiffDemuxSegment PiffDemuxSegment;
typedef struct _PiffDemuxSample PiffDemuxSample;
typedef struct _PiffDemuxSubSampleEncryption PiffDemuxSubSampleEncryption;
typedef struct _PiffDemuxSubSampleEntryInfo PiffDemuxSubSampleEntryInfo;

enum
{
  PROR_PIFF_0,
  PROP_PIFF_MEDIA_CAPS,
  PROP_PIFF_MEDIA_TIMESCALE,
  PROP_PIFF_MEDIA_DURATION,
  PROP_PIFF_MEDIA_START_TIMESTAMP,
  PROP_PIFF_IS_LIVE,
  PROP_PIFF_LOOKAHEAD_COUNT,
  PROP_PIFF_AVG_FRAME_DUR,
#ifdef DRM_ENABLE
  PROP_PROTECTION_HEADER_BUFFER,
#endif
};

enum
{
  SIGNAL_LIVE_PARAM,
  LAST_SIGNAL
};

static guint gst_piffdemux_signals[LAST_SIGNAL] = { 0 };

struct _PiffDemuxSubSampleEntryInfo
{
  guint16 LenofClearData;
  guint32 LenofEncryptData;
};

struct _PiffDemuxSubSampleEncryption
{
  guint16 n_entries;
  PiffDemuxSubSampleEntryInfo *sub_entry;
};

struct _PiffDemuxSample
{
  guint32 size;
  gint32 pts_offset;            /* Add this value to timestamp to get the pts */
  guint64 offset;
  guint64 timestamp;            /* DTS In mov time */
  guint32 duration;             /* In mov time */
  gboolean keyframe;            /* TRUE when this packet is a keyframe */
  guint8 *iv;				/* initialization vector for decryption*/
  PiffDemuxSubSampleEncryption *sub_encry;
};

/* timestamp is the DTS */
#define PIFFSAMPLE_DTS(stream,sample) gst_util_uint64_scale ((sample)->timestamp,\
    GST_SECOND, (stream)->timescale)
/* timestamp + offset is the PTS */
#define PIFFSAMPLE_PTS(stream,sample) gst_util_uint64_scale ((sample)->timestamp + \
    (sample)->pts_offset, GST_SECOND, (stream)->timescale)
/* timestamp + duration - dts is the duration */
#define PIFFSAMPLE_DUR_DTS(stream,sample,dts) (gst_util_uint64_scale ((sample)->timestamp + \
    (sample)->duration, GST_SECOND, (stream)->timescale) - (dts));
/* timestamp + offset + duration - pts is the duration */
#define PIFFSAMPLE_DUR_PTS(stream,sample,pts) (gst_util_uint64_scale ((sample)->timestamp + \
    (sample)->pts_offset + (sample)->duration, GST_SECOND, (stream)->timescale) - (pts));

#define PIFFSAMPLE_KEYFRAME(stream,sample) ((sample)->keyframe);

typedef char uuid_t[16];

static const uuid_t tfxd_uuid = { 0x6d, 0x1d, 0x9b, 0x05,
                                               0x42, 0xd5, 0x44, 0xe6,
                                               0x80, 0xe2, 0x14, 0x1d,
                                               0xaf, 0xf7, 0x57, 0xb2 };

static const uuid_t tfrf_uuid = { 0xd4, 0x80, 0x7e, 0xf2,
                                              0xca, 0x39, 0x46, 0x95,
                                              0x8e, 0x54, 0x26, 0xcb,
                                              0x9e, 0x46, 0xa7, 0x9f };

static const uuid_t encrypt_uuid = {  0xa2, 0x39, 0x4f, 0x52,
                                                        0x5a, 0x9b, 0x4f, 0x14,
                                                        0xa2, 0x44, 0x6c, 0x42,
                                                        0x7c, 0x64, 0x8d, 0xf4 };

#define SE_OVERRIDE_TE_FLAGS 0x000001
#define SE_USE_SUBSAMPLE_ENCRYPTION 0x000002

typedef enum
{
  UUID_UNKNOWN = -1,
  UUID_TFXD,
  UUID_TFRF,
  UUID_SAMPLE_ENCRYPT,
}uuid_type_t;

struct _PiffDemuxSegment
{
  /* global time and duration, all gst time */
  guint64 time;
  guint64 stop_time;
  guint64 duration;
  /* media time of trak, all gst time */
  guint64 media_start;
  guint64 media_stop;
  gdouble rate;
};


struct _PiffDemuxStream
{
  /* stream type */
  guint32 subtype;
  GstCaps *caps;
  guint32 fourcc;

  /* duration/scale */
  guint64 duration;             /* in timescale */
  guint32 timescale;

  /* our samples */
  guint32 n_samples;
  PiffDemuxSample *samples;
  guint32 min_duration;         /* duration in timescale of first sample, used for figuring out
                                   the framerate, in timescale units */

  /* if we use chunks or samples */
  gboolean sampled;
  guint padding;

  /* when a discontinuity is pending */
  gboolean discont;

  /* list of buffers to push first */
  GSList *buffers;

  /* buffer needs some custom processing, e.g. subtitles */
  gboolean need_process;

    /* current position */
  guint32 segment_index;
  guint32 sample_index;
  guint64 time_position;        /* in gst time */

  /* the Gst segment we are processing out, used for clipping */
  GstSegment segment;

  /* last GstFlowReturn */
  GstFlowReturn last_ret;


  /* quicktime segments */
  guint32 n_segments;
  PiffDemuxSegment *segments;
  guint32 from_sample;
  guint32 to_sample;

  gboolean sent_eos;
  GstTagList *pending_tags;
  gboolean send_global_tags;

  GstEvent *pending_event;

  gboolean sent_nsevent;

  guint64 start_ts;

  guint64 avg_dur; /* average frame duration */
};


enum PiffDemuxState
{
  PIFFDEMUX_STATE_INITIAL,        /* Initial state (haven't got the header yet) */
  PIFFDEMUX_STATE_HEADER,         /* Parsing the header */
  PIFFDEMUX_STATE_MOVIE,          /* Parsing/Playing the media data */
  PIFFDEMUX_STATE_BUFFER_MDAT     /* Buffering the mdat atom */
};


static GNode *piffdemux_tree_get_child_by_type (GNode * node, guint32 fourcc);
static GNode *piffdemux_tree_get_child_by_type_full (GNode * node,
    guint32 fourcc, GstByteReader * parser);
static GNode *piffdemux_tree_get_sibling_by_type (GNode * node, guint32 fourcc);
static GNode *piffdemux_tree_get_sibling_by_type_full (GNode * node,
    guint32 fourcc, GstByteReader * parser);

static GstStaticPadTemplate gst_piffdemux_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-piff")
    );

static GstStaticPadTemplate gst_piffdemux_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);


GST_BOILERPLATE (GstPiffDemux, gst_piffdemux, GstPiffDemux, GST_TYPE_ELEMENT);

static void gst_piffdemux_dispose (GObject * object);

static GstStateChangeReturn gst_piffdemux_change_state (GstElement * element,
    GstStateChange transition);
static void
gst_piffdemux_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void
gst_piffdemux_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static GstFlowReturn gst_piffdemux_chain (GstPad * sinkpad, GstBuffer * inbuf);
static gboolean gst_piffdemux_handle_sink_event (GstPad * pad, GstEvent * event);
static gboolean piffdemux_parse_node (GstPiffDemux * piffdemux, GNode * node, const guint8 * buffer, guint length);
static gboolean piffdemux_parse_sample_encryption(GstPiffDemux * piffdemux, GstByteReader *sample_encrypt, PiffDemuxStream * stream);
static gboolean piffdemux_parse_mfhd (GstPiffDemux * piffdemux, GstByteReader * mfhd);
static gboolean gst_piffdemux_handle_src_event (GstPad * pad, GstEvent * event);
static const GstQueryType *gst_piffdemux_get_src_query_types (GstPad * pad);
static gboolean gst_piffdemux_handle_src_query (GstPad * pad, GstQuery * query);

#ifdef DRM_ENABLE
static void piffdemux_get_playready_licence (GstPiffDemux *demux);
void test_drm_trusted_operation_cb(drm_trusted_user_operation_info_s *operation_info, void *output_data);
#endif

static gboolean
ConvertH264_MetaDCI_to_3GPPDCI(unsigned char *dci_meta_buf, unsigned int dci_meta_size, unsigned char **dci_3gpp_buf, unsigned int *dci_3gpp_size);
void
__gst_piffdemux_marshal_BOOLEAN__OBJECT (GClosure *closure,
                                   GValue       *return_value G_GNUC_UNUSED,
                                   guint         n_param_values,
                                   const GValue *param_values,
                                   gpointer      invocation_hint G_GNUC_UNUSED,
                                   gpointer      marshal_data);

static void
gst_piffdemux_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_piffdemux_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_piffdemux_src_template));
  gst_element_class_set_details_simple (element_class, "PIFF demuxer",
      "Codec/Parser",
      "Parser for PIFF file format",
      "naveen ch <naveen.ch@samsung.com>");

  GST_DEBUG_CATEGORY_INIT (piffdemux_debug, "piffdemux", 0, "piffdemux plugin");
}

static void
gst_piffdemux_class_init (GstPiffDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->dispose = gst_piffdemux_dispose;
  gobject_class->set_property = gst_piffdemux_set_property;
  gobject_class->get_property = gst_piffdemux_get_property;

  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_piffdemux_change_state);

  g_object_class_install_property (gobject_class, PROP_PIFF_MEDIA_CAPS,
                                   g_param_spec_boxed ("caps", "Caps",
                                   "The allowed caps for the src pad", GST_TYPE_CAPS,
                                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* timescale of media to be set by application */
  g_object_class_install_property (gobject_class, PROP_PIFF_MEDIA_TIMESCALE,
                                   g_param_spec_uint64 ("timescale", "media timescale",
                                   "media timescale in PIFF Manifest", 0, G_MAXUINT64,
                                   PIFF_DEFAULT_TIMESCALE,
                                   G_PARAM_READWRITE));
#ifdef DRM_ENABLE
  g_object_class_install_property (gobject_class, PROP_PROTECTION_HEADER_BUFFER,
                                   gst_param_spec_mini_object ("protection-header", "protection header buffer",
                                   "protection header used for playready", GST_TYPE_BUFFER,
                                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif
  g_object_class_install_property (gobject_class, PROP_PIFF_MEDIA_DURATION,
                                   g_param_spec_int64 ("duration", "Duration of media",
                                   "Total duration of the content", -1, G_MAXINT64,
                                   PIFF_DEFAULT_DURATION,
                                   G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PIFF_MEDIA_START_TIMESTAMP,
                                   g_param_spec_uint64 ("start-ts", "expected start timestamp",
                                   "expected start timestamp to avoid reset", 0, G_MAXUINT64,
                                   PIFF_DEFAULT_START_TS,
                                   G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PIFF_IS_LIVE,
                                   g_param_spec_boolean ("is-live", "Is presentation is Live or VOD",
                                   "If Presentation is Live (true) else VOD (false)",
                                   FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PIFF_LOOKAHEAD_COUNT,
                                   g_param_spec_uint ("lookahead-count", "Lookahead count value",
			           "Look ahead count used in case of Live presentation", 0, G_MAXUINT,
			           0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_PIFF_AVG_FRAME_DUR,
                                   g_param_spec_uint64 ("frame-dur", "Average frame duration",
                                   "Average frame duration", 0, G_MAXUINT64,
                                   G_MAXUINT64,
                                   G_PARAM_READABLE));

  gst_piffdemux_signals[SIGNAL_LIVE_PARAM] = g_signal_new ("live-param",
                                                           G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                                                           G_STRUCT_OFFSET (GstPiffDemuxClass, live_param), NULL, NULL,
                                                           g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);

}


static void
gst_piffdemux_init (GstPiffDemux * piffdemux, GstPiffDemuxClass * klass)
{
  /* sink pad */
  piffdemux->sinkpad = gst_pad_new_from_static_template (&gst_piffdemux_sink_template, "sink");
  gst_pad_set_chain_function (piffdemux->sinkpad, gst_piffdemux_chain);
  gst_pad_set_event_function (piffdemux->sinkpad, gst_piffdemux_handle_sink_event);
  gst_element_add_pad (GST_ELEMENT_CAST (piffdemux), piffdemux->sinkpad);

  /* source pad */
  piffdemux->srcpad = gst_pad_new_from_static_template (&gst_piffdemux_src_template, "src");
  gst_pad_set_event_function (piffdemux->srcpad, gst_piffdemux_handle_src_event);
  gst_pad_use_fixed_caps (piffdemux->srcpad);
  gst_pad_set_query_type_function (piffdemux->srcpad, gst_piffdemux_get_src_query_types);
  gst_pad_set_query_function (piffdemux->srcpad, gst_piffdemux_handle_src_query);
  gst_element_add_pad (GST_ELEMENT_CAST (piffdemux), piffdemux->srcpad);

  piffdemux->stream = g_new0 (PiffDemuxStream, 1);
  piffdemux->stream->fourcc = PIFF_DEFAULT_FOURCC;
  piffdemux->stream->timescale = PIFF_DEFAULT_TIMESCALE;
  piffdemux->stream->duration = PIFF_DEFAULT_DURATION;
  piffdemux->stream->caps = NULL;
  piffdemux->stream->discont = TRUE;
  piffdemux->stream->need_process = FALSE;
  piffdemux->stream->segment_index = -1;
  piffdemux->stream->time_position = 0;
  piffdemux->stream->sample_index = -1;
  piffdemux->stream->last_ret = GST_FLOW_OK;
  piffdemux->stream->sent_nsevent = FALSE;
  piffdemux->stream->start_ts = PIFF_DEFAULT_START_TS;
  piffdemux->stream->avg_dur = -1;

  piffdemux->state = PIFFDEMUX_STATE_INITIAL;
  piffdemux->neededbytes = 16;
  piffdemux->todrop = 0;
  piffdemux->adapter = gst_adapter_new ();
  piffdemux->offset = 0;
  piffdemux->first_mdat = -1;
  piffdemux->mdatoffset = GST_CLOCK_TIME_NONE;
  piffdemux->mdatbuffer = NULL;
  piffdemux->moof_rcvd = FALSE;
  piffdemux->is_live = FALSE;
  piffdemux->lookahead_cnt = 0;
#ifdef DRM_ENABLE
  piffdemux->pr_handle = NULL;
#endif
  piffdemux->decrypt_init = FALSE;
  piffdemux->encrypt_content = FALSE;

#ifdef DEC_OUT_FRAME_DUMP
    piffdump = fopen ("/opt/media/piff_out_dump.dmp", "w+");
    if (piffdump == NULL)
    {
        g_print ("\nNot able to create frame dump file\n");
    }
#endif

  gst_segment_init (&piffdemux->segment, GST_FORMAT_TIME);
}

static void
gst_piffdemux_dispose (GObject * object)
{
  GstPiffDemux *piffdemux = GST_PIFFDEMUX (object);

  if (piffdemux->adapter) {
    g_object_unref (G_OBJECT (piffdemux->adapter));
    piffdemux->adapter = NULL;
  }

#ifdef DEC_OUT_FRAME_DUMP
    {
        fclose (piffdump);
        piffdump = NULL;
    }
#endif
  G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
gst_piffdemux_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  GstPiffDemux *piffdemux = GST_PIFFDEMUX (object);

  switch (prop_id) {
    case PROP_PIFF_MEDIA_CAPS: {
      if (piffdemux->stream->caps)
        gst_caps_unref(piffdemux->stream->caps);
      piffdemux->stream->caps = gst_caps_copy (gst_value_get_caps (value));
      gchar *caps_string = gst_caps_to_string(piffdemux->stream->caps);
      GST_DEBUG_OBJECT (piffdemux, "stream caps = %s", caps_string);
      g_free(caps_string);
      caps_string = NULL;
      if (!gst_pad_set_caps(piffdemux->srcpad, piffdemux->stream->caps)) {
        GST_ERROR_OBJECT (piffdemux, "not able to set caps...");
      }
      break;
    }
    case PROP_PIFF_MEDIA_TIMESCALE:
      piffdemux->stream->timescale = g_value_get_uint64(value);
      break;
    case PROP_PIFF_MEDIA_DURATION:
      piffdemux->stream->duration = g_value_get_int64(value);
      break;
    case PROP_PIFF_MEDIA_START_TIMESTAMP:
      piffdemux->stream->start_ts = g_value_get_uint64(value);
      GST_INFO_OBJECT (piffdemux, "start_ts = %"GST_TIME_FORMAT, GST_TIME_ARGS(piffdemux->stream->start_ts));
      break;
    case PROP_PIFF_IS_LIVE:
      piffdemux->is_live = g_value_get_boolean(value);
      break;
    case PROP_PIFF_LOOKAHEAD_COUNT:
      piffdemux->lookahead_cnt = g_value_get_uint(value);
      GST_DEBUG_OBJECT (piffdemux, "Look ahead count = %d", piffdemux->lookahead_cnt);
      break;
#ifdef DRM_ENABLE
    case PROP_PROTECTION_HEADER_BUFFER:
      piffdemux->protection_header = gst_value_get_buffer(value);
      piffdemux_get_playready_licence (piffdemux);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
gst_piffdemux_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GstPiffDemux *piffdemux = GST_PIFFDEMUX (object);

  switch (prop_id) {
     case PROP_PIFF_MEDIA_CAPS:
      gst_value_set_caps (value, piffdemux->stream->caps);
      break;
    case PROP_PIFF_MEDIA_TIMESCALE:
      g_value_set_uint64 (value, piffdemux->stream->timescale);
      break;
    case PROP_PIFF_MEDIA_DURATION:
      g_value_set_int64 (value, piffdemux->stream->duration);
      break;
    case PROP_PIFF_MEDIA_START_TIMESTAMP:
      g_value_set_uint64 (value, piffdemux->stream->start_ts);
      break;
    case PROP_PIFF_IS_LIVE:
      g_value_set_boolean(value, piffdemux->is_live);
      break;
    case PROP_PIFF_LOOKAHEAD_COUNT:
      g_value_set_uint (value, piffdemux->lookahead_cnt);
      break;
    case PROP_PIFF_AVG_FRAME_DUR:
      g_value_set_uint64 (value, piffdemux->stream->avg_dur);
      break;
#ifdef DRM_ENABLE
    case PROP_PROTECTION_HEADER_BUFFER:
      gst_value_take_buffer (value, piffdemux->protection_header);
      break;
#endif
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
gst_piffdemux_post_no_playable_stream_error (GstPiffDemux * piffdemux)
{
  if (piffdemux->posted_redirect) {
    GST_ELEMENT_ERROR (piffdemux, STREAM, DEMUX,
        ("This file contains no playable streams."),
        ("no known streams found, a redirect message has been posted"));
  } else {
    GST_ELEMENT_ERROR (piffdemux, STREAM, DEMUX,
        ("This file contains no playable streams."),
        ("no known streams found"));
  }

}

static gboolean
gst_piffdemux_src_convert (GstPad * pad, GstFormat src_format, gint64 src_value,
    GstFormat dest_format, gint64 * dest_value)
{
  gboolean res = TRUE;
  PiffDemuxStream *stream = gst_pad_get_element_private (pad);
  GstPiffDemux *piffdemux = GST_PIFFDEMUX (gst_pad_get_parent (pad));

  if (stream->subtype != FOURCC_vide) {
    res = FALSE;
    goto done;
  }

  switch (src_format) {
    case GST_FORMAT_TIME:
      switch (dest_format) {
        case GST_FORMAT_BYTES:{

          break;
        }
        default:
          res = FALSE;
          break;
      }
      break;
    case GST_FORMAT_BYTES:
      switch (dest_format) {
        case GST_FORMAT_TIME:{

          break;
        }
        default:
          res = FALSE;
          break;
      }
      break;
    default:
      res = FALSE;
  }

done:
  gst_object_unref (piffdemux);

  return res;
}

static const GstQueryType *
gst_piffdemux_get_src_query_types (GstPad * pad)
{
  static const GstQueryType src_types[] = {
    GST_QUERY_POSITION,
    GST_QUERY_DURATION,
    GST_QUERY_CONVERT,
    GST_QUERY_FORMATS,
    GST_QUERY_SEEKING,
    0
  };

  return src_types;
}

static gboolean
gst_piffdemux_get_duration (GstPiffDemux * piffdemux, gint64 * duration)
{
  gboolean res = TRUE;

  *duration = GST_CLOCK_TIME_NONE;

  if (piffdemux->stream->duration != 0) {
    if (piffdemux->stream->duration != G_MAXINT64 && piffdemux->stream->timescale != 0) {
      *duration = gst_util_uint64_scale (piffdemux->stream->duration,
          GST_SECOND, piffdemux->stream->timescale);
    }
  }
  return res;
}

static gboolean
gst_piffdemux_handle_src_query (GstPad * pad, GstQuery * query)
{
  gboolean res = FALSE;
  GstPiffDemux *piffdemux = GST_PIFFDEMUX (gst_pad_get_parent (pad));

  GST_LOG_OBJECT (pad, "%s query", GST_QUERY_TYPE_NAME (query));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
      GST_ERROR ("Querying POSITION from piffdemux....");
      if (GST_CLOCK_TIME_IS_VALID (piffdemux->segment.last_stop)) {
        gst_query_set_position (query, GST_FORMAT_TIME,
            piffdemux->segment.last_stop);
        res = TRUE;
      }
      break;
    case GST_QUERY_DURATION:{
      GstFormat fmt;
      GST_ERROR ("Querying DURATION from piffdemux....");

      gst_query_parse_duration (query, &fmt, NULL);
      if (fmt == GST_FORMAT_TIME) {
        gint64 duration = -1;

        gst_piffdemux_get_duration (piffdemux, &duration);
        if (duration > 0) {
          gst_query_set_duration (query, GST_FORMAT_TIME, duration);
          res = TRUE;
        }
      }
      break;
    }
    case GST_QUERY_CONVERT:{
      GstFormat src_fmt, dest_fmt;
      gint64 src_value, dest_value = 0;

      gst_query_parse_convert (query, &src_fmt, &src_value, &dest_fmt, NULL);

      res = gst_piffdemux_src_convert (pad,
          src_fmt, src_value, dest_fmt, &dest_value);
      if (res) {
        gst_query_set_convert (query, src_fmt, src_value, dest_fmt, dest_value);
        res = TRUE;
      }
      break;
    }
    case GST_QUERY_FORMATS:
      gst_query_set_formats (query, 2, GST_FORMAT_TIME, GST_FORMAT_BYTES);
      res = TRUE;
      break;
    case GST_QUERY_SEEKING:{

      break;
    }
    default:
      res = gst_pad_query_default (pad, query);
      break;
  }

  gst_object_unref (piffdemux);

  return res;
}


static void
gst_piffdemux_push_tags (GstPiffDemux * piffdemux, PiffDemuxStream * stream)
{
  if (G_UNLIKELY (stream->pending_tags)) {
    GST_DEBUG_OBJECT (piffdemux, "Sending tags %" GST_PTR_FORMAT,
        stream->pending_tags);
    gst_pad_push_event (piffdemux->srcpad,
        gst_event_new_tag (stream->pending_tags));
    stream->pending_tags = NULL;
  }

  if (G_UNLIKELY (stream->send_global_tags && piffdemux->tag_list)) {
    GST_DEBUG_OBJECT (piffdemux, "Sending global tags %" GST_PTR_FORMAT,
        piffdemux->tag_list);
    gst_pad_push_event (piffdemux->srcpad,
        gst_event_new_tag (gst_tag_list_copy (piffdemux->tag_list)));
    stream->send_global_tags = FALSE;
  }
}


static void
gst_piffdemux_push_event (GstPiffDemux * piffdemux, GstEvent * event)
{
  GstEventType etype = GST_EVENT_TYPE (event);

  GST_DEBUG_OBJECT (piffdemux, "pushing %s event on source pad",
      GST_EVENT_TYPE_NAME (event));

  if (piffdemux->stream->sent_eos) {
    GST_INFO_OBJECT (piffdemux, "already sent eos");
    return;
  }

  if (!gst_pad_push_event (piffdemux->srcpad, event)) {
    GST_ERROR_OBJECT (piffdemux, "error in sending event to srcpad...");
  }

  if (etype == GST_EVENT_EOS)
    piffdemux->stream->sent_eos = TRUE;
}


/* find the segment for @time_position for @stream
 *
 * Returns -1 if the segment cannot be found.
 */
static guint32
gst_piffdemux_find_segment (GstPiffDemux * piffdemux, PiffDemuxStream * stream,
    guint64 time_position)
{
  gint i;
  guint32 seg_idx;

  GST_LOG_OBJECT (piffdemux, "finding segment for %" GST_TIME_FORMAT,
      GST_TIME_ARGS (time_position));

  /* find segment corresponding to time_position if we are looking
   * for a segment. */
  seg_idx = -1;
  for (i = 0; i < stream->n_segments; i++) {
    PiffDemuxSegment *segment = &stream->segments[i];

    GST_LOG_OBJECT (piffdemux,
        "looking at segment %" GST_TIME_FORMAT "-%" GST_TIME_FORMAT,
        GST_TIME_ARGS (segment->time), GST_TIME_ARGS (segment->stop_time));

    /* For the last segment we include stop_time in the last segment */
    if (i < stream->n_segments - 1) {
      if (segment->time <= time_position && time_position < segment->stop_time) {
        GST_LOG_OBJECT (piffdemux, "segment %d matches", i);
        seg_idx = i;
        break;
      }
    } else {
      if (segment->time <= time_position && time_position <= segment->stop_time) {
        GST_LOG_OBJECT (piffdemux, "segment %d matches", i);
        seg_idx = i;
        break;
      }
    }
  }
  return seg_idx;
}


static gboolean
gst_piffdemux_handle_src_event (GstPad * pad, GstEvent * event)
{
  gboolean res = TRUE;
  GstPiffDemux *piffdemux = GST_PIFFDEMUX (gst_pad_get_parent (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_QOS:
    case GST_EVENT_NAVIGATION:
      res = FALSE;
      gst_event_unref (event);
      break;
    case GST_EVENT_SEEK:
    default:
      res = gst_pad_event_default (pad, event);
      break;
  }

  gst_object_unref (piffdemux);

  return res;
}


static void
gst_piffdemux_move_stream (GstPiffDemux * piffdemux, PiffDemuxStream * str,
    guint32 index)
{
  /* no change needed */
  if (index == str->sample_index)
    return;

  GST_DEBUG_OBJECT (piffdemux, "moving to sample %u of %u", index,
      str->n_samples);

  /* position changed, we have a discont */
  str->sample_index = index;
  /* Each time we move in the stream we store the position where we are
   * starting from */
  str->from_sample = index;
  str->discont = TRUE;
}

// TODO: need to check more on this below function
/* stream/index return sample that is min/max w.r.t. byte position,
 * time is min/max w.r.t. time of samples,
 * the latter need not be time of the former sample */
static void
gst_piffdemux_find_sample (GstPiffDemux * piffdemux, gint64 byte_pos, gboolean fw,
    gboolean set, PiffDemuxStream ** _stream, gint * _index, gint64 * _time)
{
  gint i, index;
  gint64 time, min_time;
  PiffDemuxStream *stream;
  PiffDemuxStream *str = piffdemux->stream;
  gint inc;
  gboolean set_sample;

  min_time = -1;
  stream = NULL;
  index = -1;

  set_sample = !set;
  if (fw) {
    i = 0;
    inc = 1;
  } else {
    i = str->n_samples - 1;
    inc = -1;
  }

  for (; (i >= 0) && (i < str->n_samples); i += inc) {
    if (str->samples[i].size &&
    ((fw && (str->samples[i].offset >= byte_pos)) ||
    (!fw &&
    (str->samples[i].offset + str->samples[i].size <=
    byte_pos)))) {
      /* move stream to first available sample */
      if (set) {
        gst_piffdemux_move_stream (piffdemux, str, i);
        set_sample = TRUE;
      }
      /* determine min/max time */
      time = str->samples[i].timestamp + str->samples[i].pts_offset;
      time = gst_util_uint64_scale (time, GST_SECOND, str->timescale);
      /*if (min_time == -1 || (!fw && time > min_time) ||
      (fw && time < min_time)) : Dead code*/ {
        min_time = time;
      }
      index = i;
      break;
    }
  }
  /* no sample for this stream, mark eos */
  if (!set_sample)
    gst_piffdemux_move_stream (piffdemux, str, str->n_samples);

  if (_time)
    *_time = min_time;
  if (_stream)
    *_stream = str;
  if (_index)
    *_index = index;
}


static gboolean
gst_piffdemux_handle_sink_event (GstPad * sinkpad, GstEvent * event)
{
  GstPiffDemux *demux = GST_PIFFDEMUX (GST_PAD_PARENT (sinkpad));
  gboolean res;

  GST_LOG_OBJECT (demux, "handling %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:
    {
      GstFormat format;
      gdouble rate, arate;
      gint64 start, stop, time, offset = 0;
      PiffDemuxStream *stream;
      gint idx;
      gboolean update;
      GstSegment segment;

      /* some debug output */
      gst_segment_init (&segment, GST_FORMAT_UNDEFINED);
      gst_event_parse_new_segment_full (event, &update, &rate, &arate, &format,
          &start, &stop, &time);
      gst_segment_set_newsegment_full (&segment, update, rate, arate, format,
          start, stop, time);
      GST_ERROR_OBJECT (demux,
          "received format %d newsegment %" GST_SEGMENT_FORMAT, format,
          &segment);

      /* chain will send initial newsegment after pads have been added */
      if (demux->state != PIFFDEMUX_STATE_MOVIE ) {
        GST_DEBUG_OBJECT (demux, "still starting, eating event");
        goto exit;
      }

      /* we only expect a BYTE segment, e.g. following a seek */
      if (format == GST_FORMAT_BYTES) {
        if (start > 0) {
          gint64 requested_seek_time;
          guint64 seek_offset;

          offset = start;

          GST_OBJECT_LOCK (demux);
          requested_seek_time = demux->requested_seek_time;
          seek_offset = demux->seek_offset;
          demux->requested_seek_time = -1;
          demux->seek_offset = -1;
          GST_OBJECT_UNLOCK (demux);

          if (offset == seek_offset) {
            start = requested_seek_time;
          } else {
            gst_piffdemux_find_sample (demux, start, TRUE, FALSE, NULL, NULL,
                &start);
            start = MAX (start, 0);
          }
        }
        if (stop > 0) {
          gst_piffdemux_find_sample (demux, stop, FALSE, FALSE, NULL, NULL,
              &stop);
          /* keyframe seeking should already arrange for start >= stop,
           * but make sure in other rare cases */
          stop = MAX (stop, start);
        }
      }
#if 0
      else if (format == GST_FORMAT_TIME) {
          // Supporting TIME_FORMAT for new_segment
          //gst_piffdemux_push_event (demux,event);
        PiffDemuxStream *stream = NULL;
        int i = -1;

          demux->neededbytes = 16;
          demux->state = PIFFDEMUX_STATE_INITIAL;
          demux->offset = 0;

        /* Figure out which stream this is packet belongs to */
        for (i = 0; i < demux->n_streams; i++) {
          stream = demux->streams[i];
	   stream->last_ts = start;
	   stream->discont = TRUE;
	   stream->sample_index = stream->n_samples;
        }

           /* accept upstream's notion of segment and distribute along */
          gst_segment_set_newsegment_full (&demux->segment, update, rate, arate,
              GST_FORMAT_TIME, start, stop, start);
          GST_ERROR_OBJECT (demux, "Pushing newseg update %d, rate %g, "
              "applied rate %g, format %d, start %" GST_TIME_FORMAT ", "
              "stop %" GST_TIME_FORMAT, update, rate, arate, GST_FORMAT_TIME,
              GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

          gst_piffdemux_push_event (demux,
              gst_event_new_new_segment_full (update, rate, arate, GST_FORMAT_TIME, start, stop, start));

           /* clear leftover in current segment, if any */
          gst_adapter_clear (demux->adapter);

          goto exit;
      }
#endif
      else {
        GST_DEBUG_OBJECT (demux, "unsupported segment format, ignoring");
        goto exit;
      }

      /* accept upstream's notion of segment and distribute along */
      gst_segment_set_newsegment_full (&demux->segment, update, rate, arate,
          GST_FORMAT_TIME, start, stop, start);
      GST_ERROR_OBJECT (demux, "Pushing newseg update %d, rate %g, "
          "applied rate %g, format %d, start %" GST_TIME_FORMAT ", "
          "stop %" GST_TIME_FORMAT, update, rate, arate, GST_FORMAT_TIME,
          GST_TIME_ARGS (start), GST_TIME_ARGS (stop));

      gst_piffdemux_push_event (demux,
          gst_event_new_new_segment_full (update, rate, arate, GST_FORMAT_TIME,
              start, stop, start));

      /* clear leftover in current segment, if any */
      gst_adapter_clear (demux->adapter);
      /* set up streaming thread */
      gst_piffdemux_find_sample (demux, offset, TRUE, TRUE, &stream, &idx, NULL);
      demux->offset = offset;
      if (stream) {
        demux->todrop = stream->samples[idx].offset - offset;
        demux->neededbytes = demux->todrop + stream->samples[idx].size;
      } else {
        /* set up for EOS */
        demux->neededbytes = -1;
        demux->todrop = 0;
      }
    exit:
      gst_event_unref (event);
      res = TRUE;
      goto drop;
      break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      /* clean up, force EOS if no more info follows */
      gst_adapter_clear (demux->adapter);
      demux->offset = 0;
      demux->neededbytes = -1;
      /* reset flow return, e.g. following seek */
      demux->stream->last_ret = GST_FLOW_OK;
      demux->stream->sent_eos = FALSE;
      break;
    }
    case GST_EVENT_EOS:
      break;
    default:
      break;
  }

  res = gst_pad_event_default (demux->sinkpad, event);

drop:
  return res;
}


static void
gst_piffdemux_stream_free (GstPiffDemux * piffdemux, PiffDemuxStream * stream)
{
  int i = 0;

  g_return_if_fail (stream != NULL);

  while (stream->buffers) {
    gst_buffer_unref (GST_BUFFER_CAST (stream->buffers->data));
    stream->buffers = g_slist_delete_link (stream->buffers, stream->buffers);
  }

  for (i = 0; i < stream->n_samples; i++) {
    if (stream->samples[i].iv) {
      free (stream->samples[i].iv);
      stream->samples[i].iv = NULL;
    }
    if (stream->samples[i].sub_encry) {
      if (stream->samples[i].sub_encry->sub_entry) {
        g_free (stream->samples[i].sub_encry->sub_entry);
        stream->samples[i].sub_encry->sub_entry = NULL;
      }

      free (stream->samples[i].sub_encry);
      stream->samples[i].sub_encry = NULL;
    }
  }

  if (stream->samples) {
    g_free (stream->samples);
    stream->samples = NULL;
  }
  if (stream->caps) {
    gst_caps_unref (stream->caps);
    stream->caps = NULL;
  }
  if (stream->segments) {
    g_free (stream->segments);
    stream->segments = NULL;
  }
  if (stream->pending_tags) {
    gst_tag_list_free (stream->pending_tags);
    stream->pending_tags = NULL;
  }
  g_free (stream);
}


static GstStateChangeReturn
gst_piffdemux_change_state (GstElement * element, GstStateChange transition)
{
  GstPiffDemux *piffdemux = GST_PIFFDEMUX (element);
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      piffdemux->state = PIFFDEMUX_STATE_INITIAL;
      piffdemux->neededbytes = 16;
      piffdemux->todrop = 0;
      piffdemux->posted_redirect = FALSE;
      piffdemux->offset = 0;
      piffdemux->first_mdat = -1;
      piffdemux->mdatoffset = GST_CLOCK_TIME_NONE;
      if (piffdemux->mdatbuffer)
        gst_buffer_unref (piffdemux->mdatbuffer);
      piffdemux->mdatbuffer = NULL;
      if (piffdemux->tag_list)
        gst_tag_list_free (piffdemux->tag_list);
      piffdemux->tag_list = NULL;
      gst_adapter_clear (piffdemux->adapter);
      gst_piffdemux_stream_free (piffdemux, piffdemux->stream);
      gst_segment_init (&piffdemux->segment, GST_FORMAT_TIME);
      break;
    }
    default:
      break;
  }

  return result;
}

static void
piffdemux_post_global_tags (GstPiffDemux * piffdemux)
{
  if (piffdemux->tag_list) {
    /* all header tags ready and parsed, push them */
    GST_INFO_OBJECT (piffdemux, "posting global tags: %" GST_PTR_FORMAT,
        piffdemux->tag_list);
    /* post now, send event on pads later */
    gst_element_post_message (GST_ELEMENT (piffdemux),
        gst_message_new_tag (GST_OBJECT (piffdemux),
            gst_tag_list_copy (piffdemux->tag_list)));
  }
}


/* caller verifies at least 8 bytes in buf */
static void
extract_initial_length_and_fourcc (const guint8 * data, guint size,
    guint64 * plength, guint32 * pfourcc)
{
  guint64 length;
  guint32 fourcc;

  length = PIFF_UINT32 (data);
  GST_DEBUG ("length 0x%08" G_GINT64_MODIFIER "x", length);
  fourcc = PIFF_FOURCC (data + 4);
  GST_DEBUG ("atom type %" GST_FOURCC_FORMAT, GST_FOURCC_ARGS (fourcc));

  if (length == 0) {
    length = G_MAXUINT32;
  } else if (length == 1 && size >= 16) {
    /* this means we have an extended size, which is the 64 bit value of
     * the next 8 bytes */
    length = PIFF_UINT64 (data + 8);
    GST_DEBUG ("length 0x%08" G_GINT64_MODIFIER "x", length);
  }

  if (plength)
    *plength = length;
  if (pfourcc)
    *pfourcc = fourcc;
}

static gboolean
piffdemux_update_sample_offset (GstPiffDemux * piffdemu, PiffDemuxStream * stream, gint64 uuid_offset)
{
  PiffDemuxSample *sample;
  gint i;

  sample = stream->samples ;
  for (i = 0; i < stream->n_samples; i++)
  {
    sample->offset = sample->offset + uuid_offset;
    sample++;
  }
  return TRUE;
}

static uuid_type_t
piffdemux_get_uuid_type(GstPiffDemux * piffdemux, GstByteReader *uuid_data, gint64 *uuid_offset)
{
  uuid_type_t uuid_type = UUID_UNKNOWN;
  guint32 box_len = 0;
  guint64 box_long_len = 0;
  gchar uuid[16] = {0,};
  int i = 0;

  if (!gst_byte_reader_get_uint32_be (uuid_data, &box_len))
    goto invalid_uuid;

  /* Skipping fourcc */
  if (!gst_byte_reader_skip (uuid_data, 4))
    goto invalid_uuid;

  if (box_len == 1)
  {
    GST_WARNING ("TfxdBoxLongLength field is present...");
    if (!gst_byte_reader_get_uint64_be (uuid_data, &box_long_len))
      goto invalid_uuid;
    GST_DEBUG ("tfxd long length = %llu", box_long_len);

    *uuid_offset = box_long_len;
  }
  else
  {
    GST_DEBUG ("Box Len = %d", box_len);
    *uuid_offset = box_len;
  }

  //g_print ("\n\n\n 0x");
  for (i = 0; i < sizeof (uuid); i++)
  {
    if (!gst_byte_reader_get_uint8 (uuid_data, &(uuid[i])))
      goto invalid_uuid;
    //g_print ("%02x", uuid[i]);
  }
  //g_print ("\n\n\n");

  if (!memcmp(uuid, tfxd_uuid, sizeof (uuid_t)))
  {
    GST_INFO ("Found TFXD box");
    return UUID_TFXD;
  }
  else if (!memcmp(uuid, tfrf_uuid, sizeof (uuid_t)))
  {
    GST_INFO ("Found TFRF box");
    return UUID_TFRF;
  }
  else if (!memcmp(uuid, encrypt_uuid, sizeof (uuid_t)))
  {
    GST_INFO ("Found sample encryption box");
    return UUID_SAMPLE_ENCRYPT;
  }
  else
  {
    GST_WARNING ("Not an valid UUID box..");
    goto invalid_uuid;
  }
  return uuid_type;

invalid_uuid:
  GST_ERROR ("Error in parsing UUID atom...");
  return UUID_UNKNOWN;
}

static gboolean
piffdemux_parse_sample_encryption(GstPiffDemux * piffdemux, GstByteReader *sample_encrypt, PiffDemuxStream * stream)
{
  guint32 flags = 0;
  guint32 sample_count = 0;
  guint32 i = 0;
  guint32 algo_id;
  guint8 iv_size = 0;

  if (!gst_byte_reader_skip (sample_encrypt, 1) ||
      !gst_byte_reader_get_uint24_be (sample_encrypt, &flags))
    goto invalid_encryption;

  if (flags & SE_OVERRIDE_TE_FLAGS) {
    /* get algorithm id */
    if (!gst_byte_reader_get_uint32_be (sample_encrypt, &algo_id))
      goto invalid_encryption;

    /* get IV size */
    if (!gst_byte_reader_get_uint8 (sample_encrypt, &iv_size))
      goto invalid_encryption;

    // TODO: need to add reading of KID
  } else {
    GST_INFO_OBJECT (piffdemux, "Override flags are not present... taking default IV_Size = 8");
    iv_size = 8;
  }

  /* Get sample count*/
  if (!gst_byte_reader_get_uint32_be (sample_encrypt, &sample_count))
    goto invalid_encryption;

  GST_INFO_OBJECT (piffdemux, "Sample count = %d", sample_count);

  if (sample_count != stream->n_samples) {
    GST_ERROR_OBJECT (piffdemux, "Not all samples has IV vectors... Don't know how to handle. sample_cnt = %d and stream->n_samples = %d",
      sample_count, stream->n_samples);
    goto invalid_encryption;
  }

  for (i = 0; i < stream->n_samples; i++) {
    guint8 iv_idx = iv_size;

    /* resetting entire IV array */
    stream->samples[i].iv = (guint8 *)malloc (iv_size);
    if (NULL == stream->samples[i].iv) {
      GST_ERROR ("Failed to allocate memory...\n");
      goto invalid_encryption;
    }

    memset (stream->samples[i].iv, 0x00, iv_size);

    iv_idx = 0;
    while (iv_idx < iv_size) {
      /* get IV byte */
      if (!gst_byte_reader_get_uint8 (sample_encrypt, &(stream->samples[i].iv[iv_idx])))
        goto invalid_encryption;

      iv_idx++;
    }

#ifdef DEBUG_IV
  {
    guint8 tmp_idx = 0;
    g_print ("sample[%d] : 0x ", i);

    while (tmp_idx < iv_size ) {
      g_print ("%02x ", stream->samples[i].iv[tmp_idx]);
      tmp_idx++;
    }
    g_print ("\n");
  }
#endif

    if (flags & SE_USE_SUBSAMPLE_ENCRYPTION) {
      guint16 n_entries;
      guint16 n_idx;

      /* NumberofEntries in SubSampleEncryption */
      if (!gst_byte_reader_get_uint16_be (sample_encrypt, &n_entries))
        goto invalid_encryption;

      stream->samples[i].sub_encry = (PiffDemuxSubSampleEncryption *)malloc (sizeof (PiffDemuxSubSampleEncryption));
      if (NULL == stream->samples[i].sub_encry) {
        GST_ERROR ("Failed to allocate memory...\n");
        goto invalid_encryption;
      }

      stream->samples[i].sub_encry->sub_entry = g_try_new0 (PiffDemuxSubSampleEntryInfo, n_entries);
      if (NULL == stream->samples[i].sub_encry->sub_entry) {
        GST_ERROR_OBJECT (piffdemux, "Failed to allocate memory...");
        goto invalid_encryption;
      }

      stream->samples[i].sub_encry->n_entries = n_entries;

      GST_DEBUG_OBJECT (piffdemux,"No. of subsample entries = %d", stream->samples[i].sub_encry->n_entries);

      for (n_idx = 0; n_idx < n_entries; n_idx++) {
        if (!gst_byte_reader_get_uint16_be (sample_encrypt, &(stream->samples[i].sub_encry->sub_entry[n_idx].LenofClearData)))
          goto invalid_encryption;

        GST_DEBUG_OBJECT (piffdemux,"entry[%d] and lengthofClearData = %d", n_idx, stream->samples[i].sub_encry->sub_entry[n_idx].LenofClearData);

        if (!gst_byte_reader_get_uint32_be (sample_encrypt, &(stream->samples[i].sub_encry->sub_entry[n_idx].LenofEncryptData)))
          goto invalid_encryption;

        GST_DEBUG_OBJECT (piffdemux,"entry[%d] and lengthofEncryptData = %d", n_idx, stream->samples[i].sub_encry->sub_entry[n_idx].LenofEncryptData);
      }
    }
  }

  return TRUE;

invalid_encryption:
  {
    GST_WARNING_OBJECT (piffdemux, "invalid sample encryption header");
    return FALSE;
  }
}


static gboolean
piffdemux_parse_trun (GstPiffDemux * piffdemux, GstByteReader * trun,
    PiffDemuxStream * stream, guint32 d_sample_duration, guint32 d_sample_size,
    guint32 d_sample_flags, gint64 moof_offset, gint64 moof_length,
    gint64 * base_offset, gint64 * running_offset)
{
  guint64 timestamp;
  gint32 data_offset = 0;
  guint32 flags = 0, first_flags = 0, samples_count = 0;
  gint i;
  guint8 *data;
  guint entry_size, dur_offset, size_offset, flags_offset = 0, ct_offset = 0;
  PiffDemuxSample *sample;
  gboolean ismv = FALSE;
  guint64 total_duration = 0;

  GST_LOG_OBJECT (piffdemux, "parsing trun stream ; "
      "default dur %d, size %d, flags 0x%x, base offset %" G_GINT64_FORMAT,
       d_sample_duration, d_sample_size, d_sample_flags,
      *base_offset);

  //Resetting the samples
  stream->n_samples = 0;

  if (!gst_byte_reader_skip (trun, 1) ||
      !gst_byte_reader_get_uint24_be (trun, &flags))
    goto fail;

  if (!gst_byte_reader_get_uint32_be (trun, &samples_count))
    goto fail;

  if (flags & TR_DATA_OFFSET) {
    /* note this is really signed */
    if (!gst_byte_reader_get_int32_be (trun, &data_offset))
      goto fail;
    GST_LOG_OBJECT (piffdemux, "trun data offset %d", data_offset);
    /* default base offset = first byte of moof */
    if (*base_offset == -1) {
      GST_LOG_OBJECT (piffdemux, "base_offset at moof and moof_offset = %"G_GINT64_FORMAT, moof_offset);
      *base_offset = moof_offset;
    }
    *running_offset = *base_offset + data_offset;
  } else {
    /* if no offset at all, that would mean data starts at moof start,
     * which is a bit wrong and is ismv crappy way, so compensate
     * assuming data is in mdat following moof */
    if (*base_offset == -1) {
      *base_offset = moof_offset + moof_length + 8;
      GST_LOG_OBJECT (piffdemux, "base_offset assumed in mdat after moof");
      ismv = TRUE;
    }
    if (*running_offset == -1)
      *running_offset = *base_offset;
  }

  GST_LOG_OBJECT (piffdemux, "running offset now %" G_GINT64_FORMAT,
      *running_offset);
  GST_LOG_OBJECT (piffdemux, "trun offset %d, flags 0x%x, entries %d",
      data_offset, flags, samples_count);

  if (flags & TR_FIRST_SAMPLE_FLAGS) {
    if (G_UNLIKELY (flags & TR_SAMPLE_FLAGS)) {
      GST_DEBUG_OBJECT (piffdemux,
          "invalid flags; SAMPLE and FIRST_SAMPLE present, discarding latter");
      flags ^= TR_FIRST_SAMPLE_FLAGS;
    } else {
      if (!gst_byte_reader_get_uint32_be (trun, &first_flags))
        goto fail;
      GST_LOG_OBJECT (piffdemux, "first flags: 0x%x", first_flags);
    }
  }

  /* FIXME ? spec says other bits should also be checked to determine
   * entry size (and prefix size for that matter) */
  entry_size = 0;
  dur_offset = size_offset = 0;
  if (flags & TR_SAMPLE_DURATION) {
    GST_LOG_OBJECT (piffdemux, "entry duration present");
    dur_offset = entry_size;
    entry_size += 4;
  }
  if (flags & TR_SAMPLE_SIZE) {
    GST_LOG_OBJECT (piffdemux, "entry size present");
    size_offset = entry_size;
    entry_size += 4;
  }
  if (flags & TR_SAMPLE_FLAGS) {
    GST_LOG_OBJECT (piffdemux, "entry flags present");
    flags_offset = entry_size;
    entry_size += 4;
  }
  if (flags & TR_COMPOSITION_TIME_OFFSETS) {
    GST_LOG_OBJECT (piffdemux, "entry ct offset present");
    ct_offset = entry_size;
    entry_size += 4;
  }

  if (!piff_atom_parser_has_chunks (trun, samples_count, entry_size))
    goto fail;
  data = (guint8 *) gst_byte_reader_peek_data_unchecked (trun);

  if (stream->n_samples >=
      PIFFDEMUX_MAX_SAMPLE_INDEX_SIZE / sizeof (PiffDemuxSample))
    goto index_too_big;

  GST_DEBUG_OBJECT (piffdemux, "allocating n_samples %u * %u (%.2f MB)",
      stream->n_samples, (guint) sizeof (PiffDemuxSample),
      stream->n_samples * sizeof (PiffDemuxSample) / (1024.0 * 1024.0));

  /* create a new array of samples if it's the first sample parsed */
  if (stream->n_samples == 0)
    stream->samples = g_try_new0 (PiffDemuxSample, samples_count);
  /* or try to reallocate it with space enough to insert the new samples */
  else
    stream->samples = g_try_renew (PiffDemuxSample, stream->samples,
        stream->n_samples + samples_count);
  if (stream->samples == NULL)
    goto out_of_memory;

  if (G_UNLIKELY (stream->n_samples == 0)) {
    /* the timestamp of the first sample is also provided by the tfra entry
     * but we shouldn't rely on it as it is at the end of files */
    timestamp = 0;
  } else {
    /* subsequent fragments extend stream */
    timestamp =
        stream->samples[stream->n_samples - 1].timestamp +
        stream->samples[stream->n_samples - 1].duration;
  }
  sample = stream->samples + stream->n_samples;
  for (i = 0; i < samples_count; i++) {
    guint32 dur, size, sflags, ct;

    /* first read sample data */
    if (flags & TR_SAMPLE_DURATION) {
      dur = PIFF_UINT32 (data + dur_offset);
    } else {
      dur = d_sample_duration;
    }
    if (flags & TR_SAMPLE_SIZE) {
      size = PIFF_UINT32 (data + size_offset);
    } else {
      size = d_sample_size;
    }

    GST_DEBUG_OBJECT(piffdemux,"Size of sample %d is %d", i, size);

    if (flags & TR_FIRST_SAMPLE_FLAGS) {
      if (i == 0) {
        sflags = first_flags;
      } else {
        sflags = d_sample_flags;
      }
    } else if (flags & TR_SAMPLE_FLAGS) {
      sflags = PIFF_UINT32 (data + flags_offset);
    } else {
      sflags = d_sample_flags;
    }
    if (flags & TR_COMPOSITION_TIME_OFFSETS) {
      ct = PIFF_UINT32 (data + ct_offset);
    } else {
      ct = 0;
    }
    data += entry_size;

    /* fill the sample information */
    sample->offset = *running_offset;
    sample->pts_offset = ct;
    sample->size = size;
    sample->timestamp = timestamp;
    sample->duration = dur;
    /* sample-is-difference-sample */
    /* ismv seems to use 0x40 for keyframe, 0xc0 for non-keyframe,
     * now idea how it relates to bitfield other than massive LE/BE confusion */
    sample->keyframe = ismv ? ((sflags & 0xff) == 0x40) : !(sflags & 0x10000);
    sample->iv = NULL;
    sample->sub_encry = NULL;

    stream->samples[i] = *sample;

    *running_offset += size;
    timestamp += dur;
    sample++;

    /* calculate total duration of the present fragment */
    total_duration += gst_util_uint64_scale (dur, GST_SECOND, stream->timescale);
  }

  stream->sample_index = 0;

  stream->n_samples += samples_count;

  /* calculate avg fps based on avg frame duration */
  stream->avg_dur = total_duration/samples_count;
  g_print ("total dur = %"GST_TIME_FORMAT", avg_dur = %"GST_TIME_FORMAT"count = %d\n",
  	GST_TIME_ARGS(total_duration), GST_TIME_ARGS(stream->avg_dur), samples_count);

  return TRUE;

fail:
  {
    GST_WARNING_OBJECT (piffdemux, "failed to parse trun");
    return FALSE;
  }
out_of_memory:
  {
    GST_WARNING_OBJECT (piffdemux, "failed to allocate %d samples",
        stream->n_samples);
    return FALSE;
  }
index_too_big:
  {
    GST_WARNING_OBJECT (piffdemux, "not allocating index of %d samples, would "
        "be larger than %uMB (broken file?)", stream->n_samples,
        PIFFDEMUX_MAX_SAMPLE_INDEX_SIZE >> 20);
    return FALSE;
  }
}

static gboolean
piffdemux_parse_mfhd (GstPiffDemux * piffdemux, GstByteReader * mfhd)
{
  guint32 seq_num = 0;

  if (!gst_byte_reader_skip (mfhd, 4))
    goto invalid_mfhd;

  if (!gst_byte_reader_get_uint32_be (mfhd, &seq_num))
    goto invalid_mfhd;

  GST_DEBUG_OBJECT (piffdemux, "sequence number present in mfhd = %d", seq_num);

  return TRUE;

invalid_mfhd:
  {
    GST_WARNING_OBJECT (piffdemux, "invalid movie fragment header");
    return FALSE;
  }
}


static gboolean
piffdemux_parse_tfhd (GstPiffDemux * piffdemux, GstByteReader * tfhd,
    guint32 * default_sample_duration,
    guint32 * default_sample_size, guint32 * default_sample_flags,
    gint64 * base_offset)
{
  guint32 flags = 0;
  guint32 track_id = 0;

  if (!gst_byte_reader_skip (tfhd, 1) ||
      !gst_byte_reader_get_uint24_be (tfhd, &flags))
    goto invalid_track;

  if (!gst_byte_reader_get_uint32_be (tfhd, &track_id))
    goto invalid_track;

  GST_DEBUG_OBJECT (piffdemux, "trackID = %d", track_id);

  if (flags & TF_BASE_DATA_OFFSET) {
    if (!gst_byte_reader_get_uint64_be (tfhd, (guint64 *) base_offset))
      goto invalid_track;
    GST_DEBUG ("BaseData Offset = %"G_GUINT64_FORMAT, base_offset);
  }

  /* FIXME: Handle TF_SAMPLE_DESCRIPTION_INDEX properly */
  if (flags & TF_SAMPLE_DESCRIPTION_INDEX)
    if (!gst_byte_reader_skip (tfhd, 4))
      goto invalid_track;

  if (flags & TF_DEFAULT_SAMPLE_DURATION)
    if (!gst_byte_reader_get_uint32_be (tfhd, default_sample_duration))
      goto invalid_track;

  if (flags & TF_DEFAULT_SAMPLE_SIZE)
    if (!gst_byte_reader_get_uint32_be (tfhd, default_sample_size))
      goto invalid_track;

  if (flags & TF_DEFAULT_SAMPLE_FLAGS)
    if (!gst_byte_reader_get_uint32_be (tfhd, default_sample_flags))
      goto invalid_track;

  return TRUE;

invalid_track:
  {
    GST_WARNING_OBJECT (piffdemux, "invalid track fragment header");
    return FALSE;
  }
}

static gboolean
piffdemux_parse_tfxd (GstPiffDemux * piffdemux, PiffDemuxStream *stream,GstByteReader * tfxd)
{
  guint8 version = 0;

  // TODO: In my opinion, tfxd will be mainly useful when lookahead count = 0. In this case, based on this duration, next fragment timstamp can be calculted.. Need to test this using our server

  if (!gst_byte_reader_get_uint8 (tfxd, &version))
    goto invalid_tfxd;

  if (!gst_byte_reader_skip (tfxd, 3))
    goto invalid_tfxd;

  if (!piffdemux->lookahead_cnt) {
    piffdemux->param = (piff_live_param_t *)malloc (sizeof (piff_live_param_t));
    if (NULL == piffdemux->param) {
      GST_ERROR_OBJECT (piffdemux, "Memory not available...\n");
      return FALSE;
    }
    piffdemux->param->count = 1;
    piffdemux->param->long_info = NULL;
    piffdemux->param->info = NULL;
    piffdemux->param->is_eos = FALSE;

    // TODO: presentation will be ended based on timeout in souphttpsrc in lookaheadcnt = 0 case
  }

  if (version == 1) {
    guint64 duration = 0;
    guint64 timestamp = 0;

    GST_LOG_OBJECT (piffdemux, "Time and Duration are in 64-bit format...");
    if (!gst_byte_reader_get_uint64_be (tfxd, &timestamp))
      goto invalid_tfxd;
    if (!gst_byte_reader_get_uint64_be (tfxd, &duration))
      goto invalid_tfxd;

    GST_DEBUG_OBJECT (piffdemux, "tfxd : absolute timestamp = %"G_GUINT64_FORMAT" and duration of fragment = %"G_GUINT64_FORMAT,
        timestamp, duration);

    if (!piffdemux->lookahead_cnt) {
      piffdemux->param->long_info = (piff_fragment_longtime_info *)malloc (piffdemux->param->count * sizeof (piff_fragment_longtime_info));
      if (NULL == piffdemux->param->long_info) {
        GST_ERROR_OBJECT (piffdemux, "Memory not available...\n");
        return FALSE;
      }

      /* Calculate next fragment's timestamp using current fragment's timestamp + duration */
      piffdemux->param->long_info->duration = GST_CLOCK_TIME_NONE;
      piffdemux->param->long_info->ts = timestamp +duration;
    }
  } else if (version == 0) {
    guint32 duration = 0;
    guint32 timestamp = 0;
    GST_LOG_OBJECT (piffdemux, "Time and Duration are in 32-bit format...");

    if (!gst_byte_reader_get_uint32_be (tfxd, &timestamp))
      goto invalid_tfxd;

    if (!gst_byte_reader_get_uint32_be (tfxd, &duration))
      goto invalid_tfxd;

    GST_DEBUG_OBJECT (piffdemux, "tfxd : absolute timestamp = %"G_GUINT32_FORMAT" and duration of fragment = %"G_GUINT32_FORMAT,
        timestamp, duration);

    if (!piffdemux->lookahead_cnt) {
      piffdemux->param->info = (piff_fragment_time_info *)malloc (piffdemux->param->count * sizeof (piff_fragment_time_info));
      if (NULL == piffdemux->param->info) {
        GST_ERROR_OBJECT (piffdemux, "Memory not available...\n");
        return FALSE;
      }
      /* Calculate next fragment's timestamp using current fragment's timestamp + duration */
      piffdemux->param->info->duration = GST_CLOCK_TIME_NONE;
      piffdemux->param->info->ts = timestamp +duration;
    }
  } else {
    GST_ERROR_OBJECT (piffdemux, "Invalid Version in tfxd...");
    return FALSE;
  }

  if (!piffdemux->lookahead_cnt) {
    GST_DEBUG_OBJECT (piffdemux, "Emitting live-param signal...");
    g_signal_emit (piffdemux, gst_piffdemux_signals[SIGNAL_LIVE_PARAM], 0, piffdemux->param);
  }

  return TRUE;

invalid_tfxd:
  GST_ERROR ("Invalid TFXD atom...");
  return FALSE;
}


static gboolean
piffdemux_parse_tfrf (GstPiffDemux * piffdemux, PiffDemuxStream *stream,GstByteReader * tfrf)
{
  guint8 version = 0;
  guint8 frag_cnt = 0;
  guint8 i = 0;

  /* Getting version info */
  if (!gst_byte_reader_get_uint8 (tfrf, &version))
    goto invalid_tfrf;

  /* skipping reserved flags */
  if (!gst_byte_reader_skip (tfrf, 3))
    goto invalid_tfrf;

  if (!gst_byte_reader_get_uint8 (tfrf, &frag_cnt))
    goto invalid_tfrf;

  GST_INFO_OBJECT (piffdemux, "Subsequent fragments info count = %d", frag_cnt);

  piffdemux->param = (piff_live_param_t *)malloc(sizeof (piff_live_param_t));
  if (NULL == piffdemux->param) {
    GST_ERROR_OBJECT (piffdemux, "Memory not available...\n");
    return FALSE;
  }

  piffdemux->param->count = frag_cnt;
  piffdemux->param->long_info = NULL;
  piffdemux->param->info = NULL;
  piffdemux->param->is_eos = FALSE;

  // TODO: Duration and timestamp values need to be posted to msl using g_signal_emit

  if (version == 1) {
    guint64 duration = 0;
    guint64 timestamp = 0;
    GST_LOG_OBJECT (piffdemux, "Time and Duration are in 64-bit format...");

    piffdemux->param->long_info = (piff_fragment_longtime_info *)malloc (piffdemux->param->count * sizeof (piff_fragment_longtime_info));
    if (NULL == piffdemux->param->long_info) {
      GST_ERROR_OBJECT (piffdemux, "Memory not available...\n");
      return FALSE;
    }

    for (i = 0; i < frag_cnt; i++) {
      if (!gst_byte_reader_get_uint64_be (tfrf, &timestamp))
        goto invalid_tfrf;
      if (!gst_byte_reader_get_uint64_be (tfrf, &duration))
        goto invalid_tfrf;
      GST_DEBUG_OBJECT (piffdemux, "tfrf long: absolute timestamp = %"G_GUINT64_FORMAT" and duration of fragment = %"G_GUINT64_FORMAT"\n",
          timestamp, duration);
      (piffdemux->param->long_info[i]).ts = timestamp;
      (piffdemux->param->long_info[i]).duration = duration;
    }
  } else if (version == 0) {
    guint32 duration = 0;
    guint32 timestamp = 0;
    GST_LOG_OBJECT (piffdemux, "Time and Duration are in 32-bit format...");

    piffdemux->param->info = (piff_fragment_time_info *)malloc (piffdemux->param->count * sizeof (piff_fragment_time_info));
    if (NULL == piffdemux->param->info) {
      GST_ERROR ("Memory not available...\n");
      return FALSE;
    }

    for (i = 0; i < frag_cnt; i++) {
      if (!gst_byte_reader_get_uint32_be (tfrf, &timestamp))
        goto invalid_tfrf;
      if (!gst_byte_reader_get_uint32_be (tfrf, &duration))
        goto invalid_tfrf;

      GST_DEBUG_OBJECT (piffdemux, "tfrf int: absolute timestamp = %"G_GUINT32_FORMAT" and duration of fragment = %"G_GUINT32_FORMAT,
          timestamp, duration);
      (piffdemux->param->info[i]).ts = timestamp;
      (piffdemux->param->info[i]).duration = duration;
    }
  } else {
    GST_ERROR_OBJECT (piffdemux, "Invalid Version in tfrf...");
    return FALSE;
  }

  g_print ("Signalling from TFRF box..\n");
  g_signal_emit (piffdemux, gst_piffdemux_signals[SIGNAL_LIVE_PARAM], 0, piffdemux->param);

  return TRUE;

invalid_tfrf:
  GST_ERROR_OBJECT (piffdemux, "Invalid TFRF atom...");
  return FALSE;
}


static gboolean
piffdemux_parse_moof (GstPiffDemux * piffdemux, const guint8 * buffer, guint length,
    guint64 moof_offset, PiffDemuxStream * stream)
{
  GNode *moof_node, *mfhd_node, *traf_node, *tfhd_node, *trun_node, *uuid_node;
  GstByteReader mfhd_data, trun_data, tfhd_data, uuid_data;
  guint32 ds_size = 0, ds_duration = 0, ds_flags = 0;
  gint64 base_offset, running_offset;
  gint64 uuid_offset = 0;
  gboolean found_tfxd = FALSE;
  gboolean found_tfrf = FALSE;

  /* NOTE @stream ignored */

  moof_node = g_node_new ((guint8 *) buffer);
  piffdemux_parse_node (piffdemux, moof_node, buffer, length);
  //piffdemux_node_dump (piffdemux, moof_node);

  /* unknown base_offset to start with */
  base_offset = running_offset = -1;

  mfhd_node = piffdemux_tree_get_child_by_type_full (moof_node, FOURCC_mfhd, &mfhd_data);
  if (!mfhd_node)
    goto missing_mfhd;

  if (!piffdemux_parse_mfhd (piffdemux, &mfhd_data))
    goto missing_mfhd;

  traf_node = piffdemux_tree_get_child_by_type (moof_node, FOURCC_traf);
  while (traf_node) {
    /* Fragment Header node */
    tfhd_node =
        piffdemux_tree_get_child_by_type_full (traf_node, FOURCC_tfhd,
        &tfhd_data);
    if (!tfhd_node)
      goto missing_tfhd;
    if (!piffdemux_parse_tfhd (piffdemux, &tfhd_data, &ds_duration,
            &ds_size, &ds_flags, &base_offset))
      goto missing_tfhd;

    if (G_UNLIKELY (base_offset < -1))
      goto lost_offset;

    /* Track Run node */
    trun_node =
        piffdemux_tree_get_child_by_type_full (traf_node, FOURCC_trun,
        &trun_data);
    while (trun_node) {
      piffdemux_parse_trun (piffdemux, &trun_data, stream,
          ds_duration, ds_size, ds_flags, moof_offset, length, &base_offset,
          &running_offset);
      /* iterate all siblings */
      trun_node = piffdemux_tree_get_sibling_by_type_full (trun_node, FOURCC_trun,
          &trun_data);
    }

    uuid_node =  piffdemux_tree_get_child_by_type (traf_node, FOURCC_uuid);
    while (uuid_node) {
      uuid_type_t uuid_type;
      guint8 *lbuffer = (guint8 *) uuid_node->data;

      gst_byte_reader_init (&uuid_data, lbuffer, PIFF_UINT32 (lbuffer));

      uuid_type = piffdemux_get_uuid_type (piffdemux, &uuid_data, &uuid_offset);

      if ((UUID_TFXD == uuid_type) && piffdemux->is_live) {
        // TODO: Dont know, why we should not consider tfxd offset...if we use tfxd offset also, not working.. PIFF doc does not say anything :(
        found_tfxd = TRUE;
        if (!piffdemux_parse_tfxd (piffdemux, stream, &uuid_data))
          goto fail;
      } else if ((UUID_TFRF == uuid_type) && piffdemux->is_live && piffdemux->lookahead_cnt) {
        found_tfrf = TRUE;
        if (!piffdemux_parse_tfrf (piffdemux, stream, &uuid_data))
          goto fail;
	 piffdemux_update_sample_offset (piffdemux, stream, uuid_offset);
        running_offset += uuid_offset;
      } else if (UUID_SAMPLE_ENCRYPT == uuid_type) {
        if (!piffdemux_parse_sample_encryption (piffdemux, &uuid_data, stream))
          goto fail;
      } else {
        GST_WARNING_OBJECT (piffdemux, "Ignoring Wrong UUID...");
      }

      /* iterate all siblings */
      uuid_node = piffdemux_tree_get_sibling_by_type (uuid_node, FOURCC_uuid);
    }

    if (piffdemux->is_live) {
      if (!found_tfxd) {
        GST_ERROR_OBJECT (piffdemux, "TFXD box is not present for live stream");
        goto fail;
      }

      if (!found_tfrf && piffdemux->lookahead_cnt) {
        /* when lookahead count is non-zero in manifest & if tfrf box is not present., means EOS */
        GST_INFO_OBJECT (piffdemux, "Reached Endof Live presentation..");

        piffdemux->param = (piff_live_param_t *)malloc (sizeof (piff_live_param_t));
        if (NULL == piffdemux->param) {
          GST_ERROR_OBJECT (piffdemux, "Memory not available...\n");
          goto fail;
        }
        piffdemux->param->count = 0;
        piffdemux->param->long_info = NULL;
        piffdemux->param->info = NULL;
        piffdemux->param->is_eos = TRUE; /* marking EOS */
        g_signal_emit (piffdemux, gst_piffdemux_signals[SIGNAL_LIVE_PARAM], 0, piffdemux->param);
      }
    }

    /* if no new base_offset provided for next traf,
     * base is end of current traf */
    base_offset = running_offset;
    running_offset = -1;

    /* iterate all siblings */
    traf_node = piffdemux_tree_get_sibling_by_type (traf_node, FOURCC_traf);
  }
  g_node_destroy (moof_node);
  return TRUE;

missing_mfhd:
  {
    GST_DEBUG_OBJECT (piffdemux, "missing mfhd box");
    goto fail;
  }

missing_tfhd:
  {
    GST_DEBUG_OBJECT (piffdemux, "missing tfhd box");
    goto fail;
  }
lost_offset:
  {
    GST_DEBUG_OBJECT (piffdemux, "lost offset");
    goto fail;
  }
fail:
  {
    g_node_destroy (moof_node);

    GST_ELEMENT_ERROR (piffdemux, STREAM, DEMUX,
        ("This file is corrupt and cannot be played."), (NULL));

    return FALSE;
  }
}


/* activate the given segment number @seg_idx of @stream at time @offset.
 * @offset is an absolute global position over all the segments.
 *
 * This will push out a NEWSEGMENT event with the right values and
 * position the stream index to the first decodable sample before
 * @offset.
 */
static gboolean
gst_piffdemux_activate_segment (GstPiffDemux * piffdemux, PiffDemuxStream * stream,
    guint32 seg_idx, guint64 offset)
{
  GstEvent *event;
  PiffDemuxSegment *segment;
  guint64 seg_time;
  guint64 start, stop, time;
  gdouble rate;

  GST_LOG_OBJECT (piffdemux, "activate segment %d, offset %" G_GUINT64_FORMAT,
      seg_idx, offset);

  /* update the current segment */
  stream->segment_index = seg_idx;

  /* get the segment */
  segment = &stream->segments[seg_idx];

  if (G_UNLIKELY (offset < segment->time)) {
    GST_WARNING_OBJECT (piffdemux, "offset < segment->time %" G_GUINT64_FORMAT,
        segment->time);
    return FALSE;
  }

  /* segment lies beyond total indicated duration */
  if (G_UNLIKELY (piffdemux->segment.duration != -1 &&
          segment->time > piffdemux->segment.duration)) {
    GST_WARNING_OBJECT (piffdemux, "file duration %" G_GINT64_FORMAT
        " < segment->time %" G_GUINT64_FORMAT, piffdemux->segment.duration,
        segment->time);
    return FALSE;
  }

  /* get time in this segment */
  seg_time = offset - segment->time;

  GST_LOG_OBJECT (piffdemux, "seg_time %" GST_TIME_FORMAT,
      GST_TIME_ARGS (seg_time));

  if (G_UNLIKELY (seg_time > segment->duration)) {
    GST_LOG_OBJECT (piffdemux, "seg_time > segment->duration %" GST_TIME_FORMAT,
        GST_TIME_ARGS (segment->duration));
    return FALSE;
  }

  /* piffdemux->segment.stop is in outside-time-realm, whereas
   * segment->media_stop is in track-time-realm.
   *
   * In order to compare the two, we need to bring segment.stop
   * into the track-time-realm */

  stop = piffdemux->segment.stop;
  if (stop == -1)
    stop = piffdemux->segment.duration;
  if (stop == -1)
    stop = segment->media_stop;
  else
    stop =
        MIN (segment->media_stop, stop - segment->time + segment->media_start);

  if (piffdemux->segment.rate >= 0) {
    start = MIN (segment->media_start + seg_time, stop);
    time = offset;
  } else {
    if (segment->media_start >= piffdemux->segment.start) {
      start = segment->media_start;
      time = segment->time;
    } else {
      start = piffdemux->segment.start;
      time = segment->time + (piffdemux->segment.start - segment->media_start);
    }

    start = MAX (segment->media_start, piffdemux->segment.start);
    stop = MIN (segment->media_start + seg_time, stop);
  }

  GST_DEBUG_OBJECT (piffdemux, "newsegment %d from %" GST_TIME_FORMAT
      " to %" GST_TIME_FORMAT ", time %" GST_TIME_FORMAT, seg_idx,
      GST_TIME_ARGS (start), GST_TIME_ARGS (stop), GST_TIME_ARGS (time));

  /* combine global rate with that of the segment */
  rate = segment->rate * piffdemux->segment.rate;

  /* update the segment values used for clipping */
  gst_segment_init (&stream->segment, GST_FORMAT_TIME);
  gst_segment_set_newsegment (&stream->segment, FALSE, rate, GST_FORMAT_TIME,
      start, stop, time);

  /* now prepare and send the segment */
  event = gst_event_new_new_segment (FALSE, rate, GST_FORMAT_TIME,
      start, stop, time);
  gst_pad_push_event (piffdemux->srcpad, event);
  /* assume we can send more data now */
  stream->last_ret = GST_FLOW_OK;
  /* clear to send tags on this pad now */
  gst_piffdemux_push_tags (piffdemux, stream);

  return TRUE;
}


/* prepare to get the current sample of @stream, getting essential values.
 *
 * This function will also prepare and send the segment when needed.
 *
 * Return FALSE if the stream is EOS.
 */
static gboolean
gst_piffdemux_prepare_current_sample (GstPiffDemux * piffdemux,
    PiffDemuxStream * stream, guint64 * offset, guint * size, guint64 * timestamp,
    guint64 * duration, gboolean * keyframe)
{
  PiffDemuxSample *sample;
  guint64 time_position;
  guint32 seg_idx;

  g_return_val_if_fail (stream != NULL, FALSE);

  time_position = stream->time_position;
  if (G_UNLIKELY (time_position == -1))
    goto eos;

  seg_idx = stream->segment_index;
  if (G_UNLIKELY (seg_idx == -1)) {
    /* find segment corresponding to time_position if we are looking
     * for a segment. */
    seg_idx = gst_piffdemux_find_segment (piffdemux, stream, time_position);

    /* nothing found, we're really eos */
    if (seg_idx == -1)
      goto eos;
  }

  /* different segment, activate it, sample_index will be set. */
  if (G_UNLIKELY (stream->segment_index != seg_idx))
    gst_piffdemux_activate_segment (piffdemux, stream, seg_idx, time_position);

  GST_LOG_OBJECT (piffdemux, "segment active, index = %u of %u",
      stream->sample_index, stream->n_samples);

  if (G_UNLIKELY (stream->sample_index >= stream->n_samples))
    goto eos;


  /* now get the info for the sample we're at */
  sample = &stream->samples[stream->sample_index];

  *timestamp = PIFFSAMPLE_PTS (stream, sample);
  *offset = sample->offset;
  *size = sample->size;
  *duration = PIFFSAMPLE_DUR_PTS (stream, sample, *timestamp);
  *keyframe = PIFFSAMPLE_KEYFRAME (stream, sample);

  return TRUE;

  /* special cases */
eos:
  {
    stream->time_position = -1;
    return FALSE;
  }
}

/* the input buffer metadata must be writable,
 * but time/duration etc not yet set and need not be preserved */
static GstBuffer *
gst_piffdemux_process_buffer (GstPiffDemux * piffdemux, PiffDemuxStream * stream,
    GstBuffer * buf)
{
  guint8 *data;
  guint size, nsize = 0;
  gchar *str;

  data = GST_BUFFER_DATA (buf);
  size = GST_BUFFER_SIZE (buf);

  if (G_UNLIKELY (stream->subtype != FOURCC_text)) {
    return buf;
  }

  if (G_LIKELY (size >= 2)) {
    nsize = GST_READ_UINT16_BE (data);
    nsize = MIN (nsize, size - 2);
  }

  GST_LOG_OBJECT (piffdemux, "3GPP timed text subtitle: %d/%d", nsize, size);

  /* takes care of UTF-8 validation or UTF-16 recognition,
   * no other encoding expected */
  str = gst_tag_freeform_string_to_utf8 ((gchar *) data + 2, nsize, NULL);
  if (str) {
    gst_buffer_unref (buf);
    buf = gst_buffer_new ();
    GST_BUFFER_DATA (buf) = GST_BUFFER_MALLOCDATA (buf) = (guint8 *) str;
    GST_BUFFER_SIZE (buf) = strlen (str);
  } else {
    /* may be 0-size subtitle, which is also sent to keep pipeline going */
    GST_BUFFER_DATA (buf) = data + 2;
    GST_BUFFER_SIZE (buf) = nsize;
  }

  /* FIXME ? convert optional subsequent style info to markup */

  return buf;
}

/* Sets a buffer's attributes properly and pushes it downstream.
 * Also checks for additional actions and custom processing that may
 * need to be done first.
 */
static gboolean
gst_piffdemux_decorate_and_push_buffer (GstPiffDemux * piffdemux,
    PiffDemuxStream * stream, GstBuffer * buf,
    guint64 timestamp, guint64 duration, gboolean keyframe, guint64 position,
    guint64 byte_position)
{
  GstFlowReturn ret = GST_FLOW_OK;

  if (!stream->caps) {
    GST_WARNING_OBJECT (piffdemux, "caps are empty...creat any caps");
    stream->caps = gst_caps_new_any();
    if (!stream->caps) {
      GST_ERROR_OBJECT (piffdemux, "failed to create caps...");
      ret = GST_FLOW_ERROR;
      goto exit;
    }
  }

  /* position reporting */
  if (piffdemux->segment.rate >= 0) {
   // TODO: Segment fault is coming here for Audio stream.. need to check

    gst_segment_set_last_stop (&piffdemux->segment, GST_FORMAT_TIME, position);
    //gst_piffdemux_sync_streams (piffdemux);
  }

  /* send out pending buffers */
  while (stream->buffers) {
    GstBuffer *buffer = (GstBuffer *) stream->buffers->data;

    if (G_UNLIKELY (stream->discont)) {
      GST_LOG_OBJECT (piffdemux, "marking discont buffer");
      GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
      stream->discont = FALSE;
    }
    gst_buffer_set_caps (buffer, stream->caps);

    gst_pad_push (piffdemux->srcpad, buffer);

    stream->buffers = g_slist_delete_link (stream->buffers, stream->buffers);
  }

  /* we're going to modify the metadata */
  buf = gst_buffer_make_metadata_writable (buf);

  /* for subtitle processing */
  if (G_UNLIKELY (stream->need_process))
    buf = gst_piffdemux_process_buffer (piffdemux, stream, buf);

  GST_BUFFER_TIMESTAMP (buf) = timestamp;
  GST_BUFFER_DURATION (buf) = duration;
  GST_BUFFER_OFFSET (buf) = -1;
  GST_BUFFER_OFFSET_END (buf) = -1;

  if (G_UNLIKELY (stream->padding)) {
    GST_BUFFER_DATA (buf) += stream->padding;
    GST_BUFFER_SIZE (buf) -= stream->padding;
  }

  if (G_UNLIKELY (buf == NULL))
    goto exit;

  if (G_UNLIKELY (stream->discont)) {
    GST_LOG_OBJECT (piffdemux, "marking discont buffer");
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
    stream->discont = FALSE;
  }

  if (!keyframe)
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);

 //g_print ("\n\npad caps : %s\n\n", gst_caps_to_string (gst_pad_get_caps (stream->pad)));

  //gst_buffer_set_caps (buf, stream->caps); // commenting to avoid caps by setting properties

  // TODO: need to see how resolution switch will work
  gst_buffer_set_caps (buf, stream->caps);

#ifdef DRM_ENABLE
  if (piffdemux->encrypt_content) {
    drm_trusted_payload_info_s read_input_data = {0, };
    drm_trusted_read_decrypt_resp_data_s read_output_data = {0, };
    gint offset = 0;
    PiffDemuxSample *sample = &stream->samples[stream->sample_index];

    if (sample->sub_encry) {
      offset = sample->sub_encry->sub_entry[0].LenofClearData;
    }

    read_input_data.media_offset = 0;
    read_input_data.payload_data =  GST_BUFFER_DATA(buf) + offset;
    read_input_data.payload_data_len = GST_BUFFER_SIZE(buf) - offset;
    read_input_data.payload_iv_len = 8;
    read_input_data.payload_iv = (unsigned char *) malloc (8);
    read_input_data.payload_data_output =  GST_BUFFER_DATA(buf) + offset;
    if (NULL == read_input_data.payload_iv) {
      GST_ERROR ("Failed to allocate memory...");
      ret = GST_FLOW_ERROR;
      goto exit;
    }
    memcpy (read_input_data.payload_iv, sample->iv, 8);

    ret = drm_trusted_read_decrypt_session(piffdemux->pr_handle , &read_input_data, &read_output_data);
    if (DRM_TRUSTED_RETURN_SUCCESS != ret) {
      GST_ERROR_OBJECT (piffdemux, "failed to decrypt buffer...");
      free (read_input_data.payload_iv);
      ret = GST_FLOW_ERROR;
      goto exit;
    }

    if (read_output_data.read_size != read_input_data.payload_data_len) {
      g_print ("Decrypter did not consume data fully...\n\n\n");
    }

    free (read_input_data.payload_iv);
    read_input_data.payload_iv = NULL;

  }
#endif

  GST_LOG_OBJECT (piffdemux,
      "Pushing buffer of size = %d with time %" GST_TIME_FORMAT ", duration %"
      GST_TIME_FORMAT, GST_BUFFER_SIZE(buf), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (buf)));

#ifdef DEC_OUT_FRAME_DUMP
    {
        int written = 0;
        written = fwrite (GST_BUFFER_DATA (buf), sizeof (unsigned char), GST_BUFFER_SIZE (buf), piffdump);
        g_print ("PIFFDEMUX: written = %d\n", written);
        fflush (piffdump);
    }
#endif

  ret = gst_pad_push (piffdemux->srcpad, buf);

exit:
  return ret;
}


/*
 * next_entry_size
 *
 * Returns the size of the first entry at the current offset.
 * If -1, there are none (which means EOS or empty file).
 */
static guint64
next_entry_size (GstPiffDemux * demux)
{
  PiffDemuxStream *stream = demux->stream;
  PiffDemuxSample *sample;

  GST_LOG_OBJECT (demux, "Finding entry at offset %" G_GUINT64_FORMAT,
      demux->offset);

  GST_DEBUG_OBJECT (demux, "demux->sample_index = %d", stream->sample_index);

  if (stream->sample_index == -1)
    stream->sample_index = 0;

  if (stream->sample_index >= stream->n_samples) {
    GST_LOG_OBJECT (demux, "stream %d samples exhausted n_samples = %d",
		stream->sample_index, stream->n_samples);
    return -1;
  }

  sample = &stream->samples[stream->sample_index];

  GST_LOG_OBJECT (demux,
      "Checking Stream %d (sample_index:%d / offset:%" G_GUINT64_FORMAT
      " / size:%" G_GUINT32_FORMAT ")", stream->sample_index, stream->sample_index,
      sample->offset, sample->size);

  GST_LOG_OBJECT (demux, "stream : demux->offset :%"G_GUINT64_FORMAT, demux->offset);

  stream = demux->stream;
  sample = &stream->samples[stream->sample_index];

  if (sample->offset >= demux->offset) {
    demux->todrop = sample->offset - demux->offset;
    return sample->size + demux->todrop;
  }

  GST_DEBUG_OBJECT (demux,
      "There wasn't any entry at offset %" G_GUINT64_FORMAT, demux->offset);
  return -1;
}

static void
gst_piffdemux_post_progress (GstPiffDemux * demux, gint num, gint denom)
{
  gint perc = (gint) ((gdouble) num * 100.0 / (gdouble) denom);

  gst_element_post_message (GST_ELEMENT_CAST (demux),
      gst_message_new_element (GST_OBJECT_CAST (demux),
          gst_structure_new ("progress", "percent", G_TYPE_INT, perc, NULL)));
}

static gboolean
piffdemux_seek_offset (GstPiffDemux * demux, guint64 offset)
{
  GstEvent *event;
  gboolean res = 0;

  GST_DEBUG_OBJECT (demux, "Seeking to %" G_GUINT64_FORMAT, offset);

  event =
      gst_event_new_seek (1.0, GST_FORMAT_BYTES,
      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE, GST_SEEK_TYPE_SET, offset,
      GST_SEEK_TYPE_NONE, -1);

  res = gst_pad_push_event (demux->sinkpad, event);

  return res;
}

static GstFlowReturn
gst_piffdemux_chain (GstPad * sinkpad, GstBuffer * inbuf)
{
  GstPiffDemux *demux;
  GstFlowReturn ret = GST_FLOW_OK;
  demux = GST_PIFFDEMUX (gst_pad_get_parent (sinkpad));
#ifdef DRM_ENABLE
  if (demux->encrypt_content && !demux->decrypt_init) {
    int ret = -1;
    drm_trusted_permission_type_e perm_type = DRM_TRUSTED_PERMISSION_TYPE_PLAY;
    drm_trusted_open_decrypt_info_s open_input_data = {0, };
    drm_trusted_open_decrypt_resp_data_s open_output_data = {0, };
    drm_trusted_set_consumption_state_info_s state_input_data = {0, };

    open_input_data.file_type = DRM_TRUSTED_TYPE_PIFF;
    open_input_data.permission = perm_type;
    open_input_data.operation_callback.callback = test_drm_trusted_operation_cb;
    open_input_data.lic_header.header = GST_BUFFER_DATA(demux->protection_header);
    open_input_data.lic_header.header_len = GST_BUFFER_SIZE (demux->protection_header);

    /* Open Decrypt Session*/
    ret = drm_trusted_open_decrypt_session(&open_input_data, &open_output_data, &(demux->pr_handle));
    if (DRM_TRUSTED_RETURN_SUCCESS != ret) {
      GST_ERROR_OBJECT (demux, "failed to open decrypt session");
      goto unknown_stream;
    }

    /* Before Read, Appropriate state MUST be SET */
    state_input_data.state = DRM_CONSUMPTION_STARTED;
    ret = drm_trusted_set_decrypt_state(demux->pr_handle, &state_input_data);
    if (DRM_TRUSTED_RETURN_SUCCESS != ret) {
      GST_ERROR_OBJECT (demux, "failed to set decrypt state...");
      goto unknown_stream;
    }

    demux->decrypt_init = TRUE;
  }
#endif
  gst_adapter_push (demux->adapter, inbuf);

  /* we never really mean to buffer that much */
  if (demux->neededbytes == -1)
    goto eos;

  GST_DEBUG_OBJECT (demux, "pushing in inbuf %p, neededbytes:%u, available:%u",
      inbuf, demux->neededbytes, gst_adapter_available (demux->adapter));

  while (((gst_adapter_available (demux->adapter)) >= demux->neededbytes) &&
      (ret == GST_FLOW_OK)) {

    GST_DEBUG_OBJECT (demux,
        "state:%d , demux->neededbytes:%d, demux->offset:%" G_GUINT64_FORMAT,
        demux->state, demux->neededbytes, demux->offset);

    switch (demux->state) {
      case PIFFDEMUX_STATE_INITIAL:{
        const guint8 *data;
        guint32 fourcc;
        guint64 size;

        data = gst_adapter_peek (demux->adapter, demux->neededbytes);

        /* get fourcc/length, set neededbytes */
        extract_initial_length_and_fourcc ((guint8 *) data, demux->neededbytes,
            &size, &fourcc);
        GST_DEBUG_OBJECT (demux, "Peeking found [%" GST_FOURCC_FORMAT "] "
            "size: %" G_GUINT64_FORMAT, GST_FOURCC_ARGS (fourcc), size);
        if (size == 0) {
          GST_ELEMENT_ERROR (demux, STREAM, DEMUX,
              ("This file is invalid and cannot be played."),
              ("initial atom '%" GST_FOURCC_FORMAT "' has empty length",
                  GST_FOURCC_ARGS (fourcc)));

          ret = GST_FLOW_ERROR;
          break;
        }

        if (fourcc == FOURCC_mdat) {
          if (demux->moof_rcvd) {
            /* we have the headers, start playback */
            demux->state = PIFFDEMUX_STATE_MOVIE;
            demux->neededbytes = next_entry_size (demux);
            demux->mdatleft = size;

            /* Only post, event on pads is done after newsegment */
            piffdemux_post_global_tags (demux);
          } else {
             GST_ERROR_OBJECT (demux, "mdata received before moof.. not handled");
             goto unknown_stream;
          }
        } else if (G_UNLIKELY (size > PIFFDEMUX_MAX_ATOM_SIZE)) {
          GST_ELEMENT_ERROR (demux, STREAM, DEMUX,
              ("This file is invalid and cannot be played."),
              ("atom %" GST_FOURCC_FORMAT " has bogus size %" G_GUINT64_FORMAT,
                  GST_FOURCC_ARGS (fourcc), size));
          ret = GST_FLOW_ERROR;
          break;
        } else {
          demux->neededbytes = size;
          demux->state = PIFFDEMUX_STATE_HEADER;
        }
        break;
      }
      case PIFFDEMUX_STATE_HEADER:{
        const guint8 *data;
        guint32 fourcc;

        GST_DEBUG_OBJECT (demux, "In header");

        data = gst_adapter_peek (demux->adapter, demux->neededbytes);

        /* parse the header */
        extract_initial_length_and_fourcc (data, demux->neededbytes, NULL,
            &fourcc);
        if (fourcc == FOURCC_moof) {
            GST_DEBUG_OBJECT (demux, "Parsing [moof]");
            if (!piffdemux_parse_moof (demux, data, demux->neededbytes,
                    demux->offset, demux->stream)) {
              ret = GST_FLOW_ERROR;
              goto done;
            }
            demux->moof_rcvd = TRUE;
        }  else {
          GST_WARNING_OBJECT (demux,
              "Unknown fourcc while parsing header : %" GST_FOURCC_FORMAT,
              GST_FOURCC_ARGS (fourcc));
          /* Let's jump that one and go back to initial state */
        }

        if (demux->mdatbuffer) {
          /* the mdat was before the header */
          GST_DEBUG_OBJECT (demux, "We have mdatbuffer:%p",
             demux->mdatbuffer);
          gst_adapter_clear (demux->adapter);
          demux->mdatbuffer = NULL;
          demux->offset = demux->mdatoffset;
          demux->neededbytes = next_entry_size (demux);
          demux->state = PIFFDEMUX_STATE_MOVIE;
          demux->mdatleft = gst_adapter_available (demux->adapter);

          /* Only post, event on pads is done after newsegment */
          piffdemux_post_global_tags (demux);
        } else {
          GST_DEBUG_OBJECT (demux, "Carrying on normally");
          gst_adapter_flush (demux->adapter, demux->neededbytes);
            demux->offset += demux->neededbytes;
          demux->neededbytes = 16;
          demux->state = PIFFDEMUX_STATE_INITIAL;
        }

        break;
      }
      case PIFFDEMUX_STATE_BUFFER_MDAT:{
        GstBuffer *buf;

        GST_DEBUG_OBJECT (demux, "Got our buffer at offset %" G_GUINT64_FORMAT,
            demux->offset);
        buf = gst_adapter_take_buffer (demux->adapter, demux->neededbytes);
        GST_DEBUG_OBJECT (demux, "mdatbuffer starts with %" GST_FOURCC_FORMAT,
            GST_FOURCC_ARGS (PIFF_FOURCC (GST_BUFFER_DATA (buf) + 4)));
        if (demux->mdatbuffer)
          demux->mdatbuffer = gst_buffer_join (demux->mdatbuffer, buf);
        else
          demux->mdatbuffer = buf;
        demux->offset += demux->neededbytes;
        demux->neededbytes = 16;
        demux->state = PIFFDEMUX_STATE_INITIAL;
        gst_piffdemux_post_progress (demux, 1, 1);

        break;
      }
      case PIFFDEMUX_STATE_MOVIE:{
        GstBuffer *outbuf;
        PiffDemuxStream *stream = demux->stream;
        PiffDemuxSample *sample;
        guint64 timestamp, duration, position;
        gboolean keyframe;

        GST_DEBUG_OBJECT (demux,
            "BEGIN // in MOVIE for offset %" G_GUINT64_FORMAT, demux->offset);

        if (demux->fragmented) {
          GST_DEBUG_OBJECT (demux, "mdat remaining %" G_GUINT64_FORMAT,
              demux->mdatleft);
          if (G_LIKELY (demux->todrop < demux->mdatleft)) {
            /* if needed data starts within this atom,
             * then it should not exceed this atom */
            if (G_UNLIKELY (demux->neededbytes > demux->mdatleft)) {

              GST_ELEMENT_ERROR (demux, STREAM, DEMUX,
                  ("This file is invalid and cannot be played."),
                  ("sample data crosses atom boundary"));

              ret = GST_FLOW_ERROR;
              break;
            }
            demux->mdatleft -= demux->neededbytes;
          } else {
            GST_DEBUG_OBJECT (demux, "data atom emptied; resuming atom scan");
            /* so we are dropping more than left in this atom */
            demux->todrop -= demux->mdatleft;
            demux->neededbytes -= demux->mdatleft;
            demux->mdatleft = 0;
            /* need to resume atom parsing so we do not miss any other pieces */
            demux->state = PIFFDEMUX_STATE_INITIAL;
            demux->neededbytes = 16;
            break;
          }
        }

        if (demux->todrop) {
          GST_LOG_OBJECT (demux, "Dropping %d bytes", demux->todrop);
          gst_adapter_flush (demux->adapter, demux->todrop);
          demux->neededbytes -= demux->todrop;
          demux->offset += demux->todrop;
        }

        if ( !stream->sent_nsevent) {
          //TODO: better to parse sink event function and send that new_segment
#if 1
          demux->pending_newsegment = gst_event_new_new_segment (FALSE, 1.0, GST_FORMAT_TIME,
	                                demux->stream->start_ts, -1, demux->stream->start_ts);
#else
          demux->pending_newsegment = gst_event_new_new_segment (FALSE, 1.0, GST_FORMAT_TIME,
                                        0, gst_util_uint64_scale (stream->duration, GST_SECOND, stream->timescale), 0);
#endif

          GST_INFO_OBJECT (demux, "New segment event : start = %"GST_TIME_FORMAT", stop = %" GST_TIME_FORMAT,
                            GST_TIME_ARGS (demux->stream->start_ts), GST_TIME_ARGS(gst_util_uint64_scale (stream->duration, GST_SECOND, stream->timescale)));

          if (!gst_pad_push_event (demux->srcpad, demux->pending_newsegment)) {
            GST_ERROR_OBJECT (demux, "failding to send new segment...");
            goto newsegment_error;
          }
          stream->sent_nsevent = TRUE;
	}

        /* Put data in a buffer, set timestamps, caps, ... */
        outbuf = gst_adapter_take_buffer (demux->adapter, demux->neededbytes);

        GST_DEBUG_OBJECT (demux, "Taken %d size buffer from adapter...", outbuf ? GST_BUFFER_SIZE (outbuf) : 0);

        GST_DEBUG_OBJECT (demux, "stream : %" GST_FOURCC_FORMAT, GST_FOURCC_ARGS (stream->fourcc));

        g_return_val_if_fail (outbuf != NULL, GST_FLOW_ERROR);

        sample = &stream->samples[stream->sample_index];

        GST_DEBUG_OBJECT (demux, "start_ts = %"GST_TIME_FORMAT" ts : %"GST_TIME_FORMAT" ts = %llu, pts_offset = %u, scale = %d\n",
			GST_TIME_ARGS(stream->start_ts),GST_TIME_ARGS(sample->timestamp), sample->timestamp,
			sample->pts_offset,stream->timescale);

        position = PIFFSAMPLE_DTS (stream, sample);
        timestamp = PIFFSAMPLE_PTS (stream, sample) + stream->start_ts; // Adding to avoid resetting of timestamp
        duration = PIFFSAMPLE_DUR_DTS (stream, sample, position);
        keyframe = PIFFSAMPLE_KEYFRAME (stream, sample);

        ret = gst_piffdemux_decorate_and_push_buffer (demux, stream, outbuf,
            timestamp, duration, keyframe, position, demux->offset);

        stream->sample_index++;

        /* update current offset and figure out size of next buffer */
        GST_LOG_OBJECT (demux, "increasing offset %" G_GUINT64_FORMAT " by %u",
            demux->offset, demux->neededbytes);
        demux->offset += demux->neededbytes;
        GST_LOG_OBJECT (demux, "offset is now %" G_GUINT64_FORMAT,
            demux->offset);

        if ((demux->neededbytes = next_entry_size (demux)) == -1) {
          GST_DEBUG_OBJECT (demux, "finished parsing mdat, need to search next moof atom");
          demux->neededbytes = 16;
          demux->state = PIFFDEMUX_STATE_INITIAL;
          GST_DEBUG ("\n\n Storing %s last_ts %"GST_TIME_FORMAT"\n\n", stream->subtype == FOURCC_vide ? "video" : "audio", GST_TIME_ARGS(timestamp));
          stream->start_ts = timestamp + duration;
          //goto eos;
        }
        break;
      }
      default:
        goto invalid_state;
    }
  }

  /* when buffering movie data, at least show user something is happening */
  if (ret == GST_FLOW_OK && demux->state == PIFFDEMUX_STATE_BUFFER_MDAT &&
      gst_adapter_available (demux->adapter) <= demux->neededbytes) {
    gst_piffdemux_post_progress (demux, gst_adapter_available (demux->adapter),
        demux->neededbytes);
  }
done:
  gst_object_unref (demux);

  return ret;

  /* ERRORS */
unknown_stream:
  {
    GST_ELEMENT_ERROR (demux, STREAM, FAILED, (NULL), ("unknown stream found"));
    ret = GST_FLOW_ERROR;
    goto done;
  }
eos:
  {
    GST_DEBUG_OBJECT (demux, "no next entry, EOS");
    ret = GST_FLOW_UNEXPECTED;
    goto done;
  }
invalid_state:
  {
    GST_ELEMENT_ERROR (demux, STREAM, FAILED,
        (NULL), ("piffdemuxer invalid state %d", demux->state));
    ret = GST_FLOW_ERROR;
    goto done;
  }
newsegment_error:
  {
    GST_ELEMENT_ERROR (demux, STREAM, FAILED,
        (NULL), ("could not send newsegment event"));
    ret = GST_FLOW_ERROR;
    goto done;
  }
}

static gboolean
piffdemux_parse_container (GstPiffDemux * piffdemux, GNode * node, const guint8 * buf,
    const guint8 * end)
{
  while (G_UNLIKELY (buf < end)) {
    GNode *child;
    guint32 len;

    if (G_UNLIKELY (buf + 4 > end)) {
      GST_LOG_OBJECT (piffdemux, "buffer overrun");
      break;
    }
    len = PIFF_UINT32 (buf);
    if (G_UNLIKELY (len == 0)) {
      GST_LOG_OBJECT (piffdemux, "empty container");
      break;
    }
    if (G_UNLIKELY (len < 8)) {
      GST_WARNING_OBJECT (piffdemux, "length too short (%d < 8)", len);
      break;
    }
    if (G_UNLIKELY (len > (end - buf))) {
      GST_WARNING_OBJECT (piffdemux, "length too long (%d > %d)", len,
          (gint) (end - buf));
      break;
    }

    child = g_node_new ((guint8 *) buf);
    g_node_append (node, child);
    GST_LOG_OBJECT (piffdemux, "adding new node of len %d", len);
    piffdemux_parse_node (piffdemux, child, buf, len);

    buf += len;
  }
  return TRUE;
}


static gboolean
piffdemux_parse_node (GstPiffDemux * piffdemux, GNode * node, const guint8 * buffer,
    guint length)
{
  guint32 fourcc = 0;
  guint32 node_length = 0;
  const PiffNodeType *type;
  const guint8 *end;

  GST_LOG_OBJECT (piffdemux, "piffdemux_parse buffer %p length %u", buffer, length);

  if (G_UNLIKELY (length < 8))
    goto not_enough_data;

  node_length = PIFF_UINT32 (buffer);
  fourcc = PIFF_FOURCC (buffer + 4);

  /* ignore empty nodes */
  if (G_UNLIKELY (fourcc == 0 || node_length == 8))
    return TRUE;

  type = piffdemux_type_get (fourcc);

  end = buffer + length;

  GST_LOG_OBJECT (piffdemux,
      "parsing '%" GST_FOURCC_FORMAT "', length=%u, name '%s'",
      GST_FOURCC_ARGS (fourcc), node_length, type->name);

  if (node_length > length)
    goto broken_atom_size;

  if (type->flags & PIFF_FLAG_CONTAINER) {
    piffdemux_parse_container (piffdemux, node, buffer + 8, end);
  }
  GST_LOG_OBJECT (piffdemux, "parsed '%" GST_FOURCC_FORMAT "'",
      GST_FOURCC_ARGS (fourcc));
  return TRUE;

/* ERRORS */
not_enough_data:
  {

    GST_ELEMENT_ERROR (piffdemux, STREAM, DEMUX,
        ("This file is corrupt and cannot be played."),
        ("Not enough data for an atom header, got only %u bytes", length));

    return FALSE;
  }
broken_atom_size:
  {
    GST_ELEMENT_ERROR (piffdemux, STREAM, DEMUX,
        ("This file is corrupt and cannot be played."),
        ("Atom '%" GST_FOURCC_FORMAT "' has size of %u bytes, but we have only "
            "%u bytes available.", GST_FOURCC_ARGS (fourcc), node_length,
            length));

    return FALSE;
  }
}


static GNode *
piffdemux_tree_get_child_by_type (GNode * node, guint32 fourcc)
{
  GNode *child;
  guint8 *buffer;
  guint32 child_fourcc;

  for (child = g_node_first_child (node); child;
      child = g_node_next_sibling (child)) {
    buffer = (guint8 *) child->data;

    child_fourcc = PIFF_FOURCC (buffer + 4);

    if (G_UNLIKELY (child_fourcc == fourcc)) {
      return child;
    }
  }
  return NULL;
}

static GNode *
piffdemux_tree_get_child_by_type_full (GNode * node, guint32 fourcc,
    GstByteReader * parser)
{
  GNode *child;
  guint8 *buffer;
  guint32 child_fourcc, child_len;

  for (child = g_node_first_child (node); child;
      child = g_node_next_sibling (child)) {
    buffer = (guint8 *) child->data;

    child_len = PIFF_UINT32 (buffer);
    child_fourcc = PIFF_FOURCC (buffer + 4);

    if (G_UNLIKELY (child_fourcc == fourcc)) {
      if (G_UNLIKELY (child_len < (4 + 4)))
        return NULL;
      /* FIXME: must verify if atom length < parent atom length */
      gst_byte_reader_init (parser, buffer + (4 + 4), child_len - (4 + 4));
      return child;
    }
  }
  return NULL;
}

static GNode *
piffdemux_tree_get_sibling_by_type_full (GNode * node, guint32 fourcc,
    GstByteReader * parser)
{
  GNode *child;
  guint8 *buffer;
  guint32 child_fourcc, child_len;

  for (child = g_node_next_sibling (node); child;
      child = g_node_next_sibling (child)) {
    buffer = (guint8 *) child->data;

    child_fourcc = PIFF_FOURCC (buffer + 4);

    if (child_fourcc == fourcc) {
      if (parser) {
        child_len = PIFF_UINT32 (buffer);
        if (G_UNLIKELY (child_len < (4 + 4)))
          return NULL;
        /* FIXME: must verify if atom length < parent atom length */
        gst_byte_reader_init (parser, buffer + (4 + 4), child_len - (4 + 4));
      }
      return child;
    }
  }
  return NULL;
}

static GNode *
piffdemux_tree_get_sibling_by_type (GNode * node, guint32 fourcc)
{
  return piffdemux_tree_get_sibling_by_type_full (node, fourcc, NULL);
}

#define _codec(name) \
  do { \
    if (codec_name) { \
      *codec_name = g_strdup (name); \
    } \
  } while (0)

void
gst_piffdemux_set_video_params (GstPiffDemux * piffdemux, guint fourcc,
					guint width, guint height,
					guint fps_n, guint fps_d, unsigned char *codec_data, unsigned int codec_data_len)
{
  GstCaps *caps = NULL;
  GstBuffer *dci = NULL;

  if (codec_data && codec_data_len) {
    dci = gst_buffer_new_and_alloc (codec_data_len);
    if (!dci) {
      GST_ERROR_OBJECT (piffdemux, "failed to create codec data buffer...");
    } else {
      memcpy (GST_BUFFER_DATA(dci), codec_data, codec_data_len);
    }
  }

  switch (fourcc) {

    case GST_MAKE_FOURCC ('a', 'v', 'c', '1'):
      caps = gst_caps_new_simple ("video/x-h264",
                                  "width", G_TYPE_INT, width,
                                  "height", G_TYPE_INT, height,
                                  "framerate", GST_TYPE_FRACTION, fps_n, fps_d,
                                  "stream-format", G_TYPE_STRING, "avc",
                                  "alignment", G_TYPE_STRING, "au",
                                  "codec_data", GST_TYPE_BUFFER, dci,
                                  NULL);
      break;

    case FOURCC_ovc1:
      caps = gst_caps_new_simple ("video/x-wmv",
	  	                  "width", G_TYPE_INT, width,
                                  "height", G_TYPE_INT, height,
                                  "framerate", GST_TYPE_FRACTION, fps_n, fps_d,
                                  "wmvversion", G_TYPE_INT, 3,
                                  "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('W', 'V', 'C', '1'),
                                  "codec_data", GST_TYPE_BUFFER, dci,
                                  NULL);
      break;

    default: {
      char *s;
      s = g_strdup_printf ("video/x-gst-fourcc-%" GST_FOURCC_FORMAT,
          GST_FOURCC_ARGS (fourcc));
      caps = gst_caps_new_simple (s,
                                  "width", G_TYPE_INT, width,
                                  "height", G_TYPE_INT, height,
                                  "framerate", GST_TYPE_FRACTION, fps_n, fps_d,
                                  "codec_data", GST_TYPE_BUFFER, dci,
                                  NULL);
      break;
    }
  }

  piffdemux->stream->caps = caps;
  gchar *caps_string = gst_caps_to_string(caps);
  GST_INFO_OBJECT (piffdemux, "prepared video caps : %s", caps_string);
  g_free(caps_string);
  caps_string = NULL;
}

void
gst_piffdemux_set_audio_params (GstPiffDemux * piffdemux, guint fourcc,
					guint sampling_rate, guint bps, guint channels, unsigned char *codec_data, unsigned int codec_data_len)
{
  GstCaps *caps = NULL;
  GstBuffer *dci = NULL;

  if (codec_data && codec_data_len) {
    dci = gst_buffer_new_and_alloc (codec_data_len);
    if (!dci) {
      GST_ERROR_OBJECT (piffdemux, "failed to create codec data buffer...");
    } else {
      memcpy (GST_BUFFER_DATA(dci), codec_data, codec_data_len);
    }
  }

  switch (fourcc) {

    case GST_MAKE_FOURCC ('m', 'p', '4', 'a'):
      caps = gst_caps_new_simple ("audio/mpeg",
                                  "mpegversion", G_TYPE_INT, 4,
                                  "framed", G_TYPE_BOOLEAN, TRUE,
                                  "stream-format", G_TYPE_STRING, "raw",
                                  "rate", G_TYPE_INT, (int) sampling_rate,
                                  "channels", G_TYPE_INT, channels,
                                  NULL);
      break;

    case FOURCC_owma:
      caps = gst_caps_new_simple ("audio/x-wma",
	  	                  "rate", G_TYPE_INT, (int) sampling_rate,
                                  "channels", G_TYPE_INT, channels,
	  	                  NULL);
      break;

    default: {
      char *s;
      s = g_strdup_printf ("audio/x-gst-fourcc-%" GST_FOURCC_FORMAT,
          GST_FOURCC_ARGS (fourcc));
      caps = gst_caps_new_simple (s,
	  	                  "rate", G_TYPE_INT, (int) sampling_rate,
                                  "channels", G_TYPE_INT, channels,
                                  NULL);
      break;
    }
  }

  piffdemux->stream->caps = caps;
  char *tmp_caps_name = gst_caps_to_string(caps);
  GST_INFO_OBJECT (piffdemux, "prepared audio caps : %s", tmp_caps_name);
  g_free(tmp_caps_name);

}

#define g_marshal_value_peek_object(v)   g_value_get_object (v)

void
__gst_piffdemux_marshal_BOOLEAN__OBJECT (GClosure *closure,
                                   GValue       *return_value G_GNUC_UNUSED,
                                   guint         n_param_values,
                                   const GValue *param_values,
                                   gpointer      invocation_hint G_GNUC_UNUSED,
                                   gpointer      marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__OBJECT) (gpointer  data1,
                                                    gpointer      arg_1,
                                                    gpointer     data2);
  register GMarshalFunc_BOOLEAN__OBJECT callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gboolean v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
  {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
  }
  else
  {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
  }
  callback = (GMarshalFunc_BOOLEAN__OBJECT) (marshal_data ? marshal_data : cc->callback);

  v_return = callback (data1,
                       g_marshal_value_peek_object (param_values + 1),
                       data2);

  g_value_set_boolean (return_value, v_return);
}

#define PIFFDEMUX_SPSPPS_LENGTH_SIZE     2

static gboolean
ConvertH264_MetaDCI_to_3GPPDCI(unsigned char *dci_meta_buf, unsigned int dci_meta_size, unsigned char **dci_3gpp_buf, unsigned int *dci_3gpp_size)
{
  unsigned short unit_size = 0;
  unsigned int total_size = 0;
  unsigned char unit_nb = 0;
  unsigned char sps_done = 0;
  const unsigned char *extradata = NULL;
  unsigned int h264_nal_length_size = 0;
  unsigned char *out = NULL;
  //g_print ("\n\nConvertH264_MetaDCI_to_3GPPDCI Entering.............\n");

  /* nothing to filter */
  if ((dci_meta_buf == NULL) || (dci_meta_size < 6))
  {
    GST_ERROR ("Insufficient codec data...\n");
    return FALSE;
  }

  /* Removing unnecessary info in meta data */
  extradata = (unsigned char *)dci_meta_buf + 4;

  /* retrieve Length of Length*/
  h264_nal_length_size = (*extradata++ & 0x03) + 1;

  GST_LOG ("Length Of Length is %d\n", h264_nal_length_size);
  if (h264_nal_length_size == 3)
  {
    GST_ERROR ("LengthOfLength is WRONG...\n");
    return FALSE;
  }

  /* retrieve sps and pps unit(s) */
  unit_nb = *extradata++ & 0x1f; /* number of sps unit(s) */
  GST_LOG ("No. of SPS units = %u\n", unit_nb);

  if (!unit_nb)
  {
    GST_ERROR ("SPS is not present....\n");
    return FALSE;
  }

  while (unit_nb--)
  {
    /* get SPS/PPS data Length*/
    unit_size = PIFFDEMUX_RB16(extradata);

    GST_LOG ("SPS size = %d", unit_size);

    /* Extra 4 bytes for adding size of the packet */
    total_size += unit_size + h264_nal_length_size;

    /* Check if SPS/PPS Data Length crossed buffer Length */
    if ((extradata + 2 + unit_size) > (dci_meta_buf + dci_meta_size))
    {
      GST_ERROR ("SPS Length is wrong in DCI...\n");
      if (out)
      {
        free (out);
      }
      return FALSE;
    }
    out = realloc(out, total_size);
    if (!out)
    {
      GST_ERROR ("realloc FAILED...\n");
      return FALSE;
    }
    /* Copy length of SPS header */
   // tmp = (unsigned int *)(out + total_size - unit_size - h264_nal_length_size);
   // *tmp = unit_size;
   (out + total_size - unit_size - h264_nal_length_size)[0] = 0;
   (out + total_size - unit_size - h264_nal_length_size)[1] = 0;
   (out + total_size - unit_size - h264_nal_length_size)[2] = 0;
   (out + total_size - unit_size - h264_nal_length_size)[3] = (unsigned char)unit_size;

   // memcpy(out + total_size - unit_size - h264_nal_length_size, &unit_size, h264_nal_length_size);
   //g_print ("out[0] = %02x, out[1] = %02x, out[2] = %02x = out[3] = %02x\n",
   //out[total_size - unit_size - h264_nal_length_size],  out[total_size - unit_size - h264_nal_length_size+1],
   //out[total_size - unit_size - h264_nal_length_size + 2],  out[total_size - unit_size - h264_nal_length_size + 3]);

    /* Copy SPS/PPS Length and data */
    memcpy(out + total_size - unit_size,  extradata + PIFFDEMUX_SPSPPS_LENGTH_SIZE, unit_size);

    extradata += (PIFFDEMUX_SPSPPS_LENGTH_SIZE + unit_size);

    if (!unit_nb && !sps_done++)
    {
      /* Completed reading SPS data, now read PPS data */
      unit_nb = *extradata++; /* number of pps unit(s) */
      GST_DEBUG ("No. of PPS units = %d\n", unit_nb);
    }
  }

  *dci_3gpp_buf = malloc (total_size);
  if (NULL == *dci_3gpp_buf)
  {
    GST_ERROR ("Memory Allocation FAILED...\n");
    free (out);
    return FALSE;
  }

  memcpy(*dci_3gpp_buf, out, total_size);
  *dci_3gpp_size = total_size;

  GST_DEBUG ("SPS_PPS size = %d\n", total_size);

  free(out);

  return TRUE;
 }

#ifdef DRM_ENABLE
static void
piffdemux_get_playready_licence (GstPiffDemux *demux)
{
  int ret = -1;
  drm_trusted_piff_get_license_info_s license_info;
  drm_trusted_request_type_e request_type = DRM_TRUSTED_REQ_TYPE_PIFF_GET_LICENSE;

  memset(&license_info, 0x00, sizeof(drm_trusted_piff_get_license_info_s));

  license_info.lic_header.header = (unsigned char*) GST_BUFFER_DATA (demux->protection_header);
  license_info.lic_header.header_len = GST_BUFFER_SIZE (demux->protection_header);

  ret = drm_trusted_handle_request(request_type, (void *) &license_info, NULL);
  if (DRM_TRUSTED_RETURN_SUCCESS != ret) {
    GST_ERROR_OBJECT (demux,"failed to get license...");
    GST_ELEMENT_ERROR (demux, RESOURCE, FAILED, ("failed to get license"), (NULL));
    return;
  }

  GST_INFO_OBJECT (demux, "Got license....\n");

  demux->encrypt_content = TRUE;

  return;
}

void
test_drm_trusted_operation_cb(drm_trusted_user_operation_info_s *operation_info, void *output_data)
{
	g_print ("Callback Hit:test_drm_trusted_operation_cb\n");
	g_print ("operation_status=%d\n",operation_info->operation_status);
	g_print ("operation_type=%d\n",operation_info->operation_type);
}
#endif

