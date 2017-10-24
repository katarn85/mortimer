

#ifndef __SS_MANIFEST_PARSE_H__
#define __SS_MANIFEST_PARSE_H__

#include <glib.h>
#include <gst/gst.h>
#include <libxml2/libxml/tree.h>

G_BEGIN_DECLS
typedef struct _GstSSMParse GstSSMParse;

#define GST_SSMPARESE(m) ((GstSSMParse*)m)
#define XML_MAKE_FOURCC(a,b,c,d)        ((guint32)((a)|(b)<<8|(c)<<16|(d)<<24))
#define BITRATE_SWITCH_UPPER_THRESHOLD 0.4
#define BITRATE_SWITCH_LOWER_THRESHOLD 0.1


typedef enum
{
  SS_STREAM_UNKNOWN = -1,
  SS_STREAM_VIDEO = 0,
  SS_STREAM_AUDIO,
  SS_STREAM_TEXT,
  SS_STREAM_NUM,
}SS_STREAM_TYPE;

typedef enum
{
  SS_MODE_NO_SWITCH, /* no switch is need */
  SS_MODE_AONLY, /* switch to audio only */
  SS_MODE_AV,
}SS_BW_MODE;

typedef struct
{
  guint64 StreamTimeScale;
  guint64 stream_duration;
  SS_STREAM_TYPE type;
  guint nChunks;
  guint nQualityLevels;
  guint MaxWidth;
  guint MaxHeight;
  guint DisplayWidth;
  guint DisplayHeight;
  GList *quality_lists;
  GList *fragment_lists;
  gchar *StreamType;
  gchar *StreamUrl;
  gchar *StreamSubType;
  gchar *StreamName;
  gboolean fraglist_eos;
  GMutex *frag_lock;
  GCond *frag_cond;
}GstSSMStreamNode;

typedef struct
{
  guint index;
  guint bitrate;
  guint max_width;
  guint max_height;
  guint samplingrate;
  guint channels;
  guint bps; /* bits per sample */
  guint packet_size;
  guint audio_tag;
  guint NALULengthofLength;
  gchar *fourcc;
  gchar *codec_data;
}GstSSMQualityNode;

typedef struct
{
  guint num;
  guint64 dur;
  guint64 time;
  guint media_type;
}GstSSMFragmentNode;

typedef struct
{
  gchar *SystemID;
  gchar *Content;
  guint ContentSize;
}GstSSMProtectionNode;

typedef struct
{
  guint MajorVersion;
  guint MinorVersion;
  guint64 TimeScale;
  guint64 Duration;
  guint LookAheadCount;
  guint64 DVRWindowLength;
  gboolean PresentationIsLive;
  GList *streams[SS_STREAM_NUM];
  GstSSMProtectionNode *ProtectNode;
}GstSSMRootNode;

struct _GstSSMParse
{
  gchar *uri; /* manifest URI */
  gchar *presentation_uri;
  GstSSMRootNode *RootNode;
  GMutex *lock;
  guint64 ns_start;
};

#define gst_ssm_parse_check_stream(parser, stream_type) (parser->RootNode->streams[stream_type])
#define GST_SSM_PARSE_GET_DURATION(parser) (parser->RootNode->Duration)
#define GST_SSM_PARSE_GET_TIMESCALE(parser) (parser->RootNode->TimeScale)
#define GST_SSM_PARSE_IS_LIVE_PRESENTATION(parser) (parser->RootNode->PresentationIsLive)
#define GST_SSM_PARSE_LOOKAHEAD_COUNT(parser) (parser->RootNode->LookAheadCount)
#define GST_SSM_PARSE_NS_START(parser) (parser->ns_start)

const gchar *ssm_parse_get_stream_name(SS_STREAM_TYPE type);
GstSSMParse *gst_ssm_parse_new (const gchar * uri);
void gst_ssm_parse_free (GstSSMParse *parser);
gboolean gst_ssm_parse_manifest (GstSSMParse *parser, char *data, unsigned int size);
gboolean gst_ssm_parse_get_next_fragment_url (GstSSMParse *parser, SS_STREAM_TYPE stream_type, gchar **uri, guint64 *start_ts);
gboolean gst_ssm_parse_append_next_fragment (GstSSMParse *parser, SS_STREAM_TYPE stream_type, guint64 timestamp, guint64 duration);
GstCaps *ssm_parse_get_stream_caps (GstSSMParse *parser, SS_STREAM_TYPE stream_type);
SS_BW_MODE gst_ssm_parse_switch_qualitylevel (GstSSMParse *parser, guint drate);
gboolean gst_ssm_parse_seek_manifest (GstSSMParse *parser, guint64 seek_time);
gboolean gst_ssm_parse_get_protection_header (GstSSMParse *parser, unsigned char **protection_header, unsigned int *protection_header_len);
G_END_DECLS
#endif /* __SS_MANIFEST_PARSE_H__ */

