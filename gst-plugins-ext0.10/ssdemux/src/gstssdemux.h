
#ifndef __GST_SS_DEMUX_H__
#define __GST_SS_DEMUX_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include "ssmanifestparse.h"
#include "piffcommon.h"

G_BEGIN_DECLS
#define GST_TYPE_SS_DEMUX \
  (gst_ss_demux_get_type())
#define GST_SS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_SS_DEMUX, GstSSDemux))
#define GST_SS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SS_DEMUX,GstSSDemuxClass))
#define GST_IS_SS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SS_DEMUX))
#define GST_IS_SS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SS_DEMUX))

typedef struct _GstSSDemux GstSSDemux;
typedef struct _GstSSDemuxClass GstSSDemuxClass;
typedef struct _GstSSDemuxStream GstSSDemuxStream;


/**
 * GstSSDemux:
 *
 * Opaque #GstSSDemux data structure.
 */
struct _GstSSDemux
{
  GstElement parent;

  GstPad *sinkpad;

  /* Properties */
  gchar **cookies;             /* HTTP request cookies. */
  gboolean allow_audio_only;  /*In LIVE case, allow audio only download when downloadrate is less */
  guint64 max_cache_time;
  gfloat bitrate_switch_tol;    /* tolerance with respect to the fragment duration to switch the bitarate*/
  GstBuffer *manifest;
  GstSSMParse *parser; /* manifest parser */
  gint percent;
  gint low_percent;
  gint high_percent;
  gboolean eos;
  GstSSDemuxStream *streams[SS_STREAM_NUM];
  SS_BW_MODE ss_mode;
  gboolean switch_eos;
  GstBuffer *protection_header;
};

struct _GstSSDemuxClass
{
  GstElementClass parent_class;
};

GType gst_ss_demux_get_type (void);

G_END_DECLS
#endif /* __GST_SS_DEMUX_H__ */




