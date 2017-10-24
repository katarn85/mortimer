
#ifndef __GST_PIFFDEMUX_TYPES_H__
#define __GST_PIFFDEMUX_TYPES_H__

#include <gst/gst.h>
#include <gst/base/gstbytereader.h>

#include "piffdemux.h"

G_BEGIN_DECLS

typedef gboolean (*PiffDumpFunc) (GstPiffDemux * piffdemux, GstByteReader * data, int depth);

typedef struct _PiffNodeType PiffNodeType;

#define PIFF_UINT32(a)  (GST_READ_UINT32_BE(a))
#define PIFF_UINT24(a)  (GST_READ_UINT32_BE(a) >> 8)
#define PIFF_UINT16(a)  (GST_READ_UINT16_BE(a))
#define PIFF_UINT8(a)   (GST_READ_UINT8(a))
#define PIFF_FP32(a)    ((GST_READ_UINT32_BE(a))/65536.0)
#define PIFF_SFP32(a)   (((gint)(GST_READ_UINT32_BE(a)))/65536.0)
#define PIFF_FP16(a)    ((GST_READ_UINT16_BE(a))/256.0)
#define PIFF_FOURCC(a)  (GST_READ_UINT32_LE(a))
#define PIFF_UINT64(a)  ((((guint64)PIFF_UINT32(a))<<32)|PIFF_UINT32(((guint8 *)a)+4))

typedef enum {
  PIFF_FLAG_NONE      = (0),
  PIFF_FLAG_CONTAINER = (1 << 0)
} PiffFlags;

struct _PiffNodeType {
  guint32      fourcc;
  const gchar *name;
  PiffFlags      flags;
  PiffDumpFunc   dump;
};

enum TfFlags
{
  TF_BASE_DATA_OFFSET         = 0x000001,   /* base-data-offset-present */
  TF_SAMPLE_DESCRIPTION_INDEX = 0x000002,   /* sample-description-index-present */
  TF_DEFAULT_SAMPLE_DURATION  = 0x000008,   /* default-sample-duration-present */
  TF_DEFAULT_SAMPLE_SIZE      = 0x000010,   /* default-sample-size-present */
  TF_DEFAULT_SAMPLE_FLAGS     = 0x000020,   /* default-sample-flags-present */
  TF_DURATION_IS_EMPTY        = 0x100000    /* duration-is-empty */
};

enum TrFlags
{
  TR_DATA_OFFSET              = 0x000001,   /* data-offset-present */
  TR_FIRST_SAMPLE_FLAGS       = 0x000004,   /* first-sample-flags-present */
  TR_SAMPLE_DURATION          = 0x000100,   /* sample-duration-present */
  TR_SAMPLE_SIZE              = 0x000200,   /* sample-size-present */
  TR_SAMPLE_FLAGS             = 0x000400,   /* sample-flags-present */
  TR_COMPOSITION_TIME_OFFSETS = 0x000800    /* sample-composition-time-offsets-presents */
};

enum SEFlags
{
  SE_OVERRIDE_TE_FLAGS              = 0x000001,   /* override existing track encryption parameters */
  SE_USE_SUBSAMPLE_ENCRYPTION       = 0x000002,   /* Use SubSample Encryption */
};
const PiffNodeType *piffdemux_type_get (guint32 fourcc);

G_END_DECLS

#endif /* __GST_PIFFDEMUX_TYPES_H__ */
