/* GStreamer
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
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
 *
 * * Modifications by Samsung Electronics Co., Ltd.
 */

#ifndef __GST_SPDIFSINK_H__
#define __GST_SPDIFSINK_H__

#define INHERIT_GSTBASESINK

#include <gst/gst.h>
#ifdef INHERIT_GSTBASESINK
#include <gst/base/gstbasesink.h>
#else
#include <gst/audio/gstaudiosink.h>
#endif

#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "spdif_compress_params.h"
#include "spdif_compress_offload.h"
#include "spdif_tinycompress.h"


G_BEGIN_DECLS

#define GST_TYPE_SPDIFSINK \
  (gst_spdifsink_get_type())
#define GST_SPDIFSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_SPDIFSINK, GstSpdifSink))
#define GST_SPDIFSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_SPDIFSINK, GstSpdifSinkClass))
#define GST_IS_SPDIFSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_SPDIFSINK))
#define GST_IS_SPDIFSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_SPDIFSINK))

typedef struct _GstSpdifSink GstSpdifSink;
typedef struct _GstSpdifSinkClass GstSpdifSinkClass;

typedef struct compr_config compr_config_;
typedef struct snd_codec snd_codec_;
typedef struct compress compress_;


typedef enum
{
	AUDIO_LPCM,
	AUDIO_NULL,
	AUDIO_PAUSE,	
	AUDIO_AC3,
	AUDIO_MPEG,
	AUDIO_MPEG1_L1,
	AUDIO_MPEG1_L2,
	AUDIO_MPEG1_L3,
	AUDIO_MPEG2,
	AUDIO_MPEG2_L1,
	AUDIO_MPEG2_L2,
	AUDIO_MPEG2_L3,	
	AUDIO_MPEG2_AAC,
	AUDIO_DTS,
	AUDIO_DTS_TYPE1,
	AUDIO_DTS_TYPE2,
	AUDIO_DTS_TYPE3,
	AUDIO_DTS_TYPE4,
	AUDIO_ATRAC,
	AUDIO_ATRAC1,
	AUDIO_ATRAC2,
	AUDIO_ATRAC3,
	AUDIO_WMA,
	AUDIO_OGG,
	AUDIO_AAC,
	AUDIO_ADPCM,
	AUDIO_WAVE,
	AUDIO_DVDPCM,
	AUDIO_MULAW_PCM,
	AUDIO_ALAW_PCM, 
	AUDIO_EAC3, 
	AUDIO_QCELP,
	AUDIO_AMRNB,
	AUDIO_AMRWB,
	AUDIO_RA_G2COOK,
	AUDIO_RA_LOSSLESS,
	AUDIO_G729_DEC,
	AUDIO_G729_ENC,
	AUDIO_HEAAC,
	AUDIO_HEAAC_LTP,
	
	//GENOA
	AUDIO_DTS_HD_MA,
	AUDIO_DTS_HD_HRA,	
	AUDIO_DTS_HD_LBR,
	AUDIO_TRUE_HD,
	AUDIO_DDP_DCV,

	AUDIO_DRA,

	AUDIO_UNKNOWN,

	AUDIO_CODEC_MAX	
} Audio_Codec_e;

/**
 * GstSpdifSink:
 *
 * The #GstSpdifSink data structure.
 */
struct _GstSpdifSink {
#ifdef INHERIT_GSTBASESINK
  GstBaseSink sink;
#else
  GstAudioSink sink;
#endif
  int device_fd;
  unsigned int spdif_codec_type;
  int srp_dev_spdif;
  int compress_spdif_start;
  compr_config_ spdif_config;
  snd_codec_ spdif_codec;
  compress_ *sdp_spdif_compress;
  guchar start_fast_rendering; // 0:OFF(default),  1:REQUESTED, 2:APPLIED
  guint64 max_fast_rendering_size;
};

struct _GstSpdifSinkClass {
#ifdef INHERIT_GSTBASESINK
  GstBaseSinkClass parent_class;
#else
  GstAudioSinkClass parent_class;
#endif
};

GType gst_spdifsink_get_type(void);

G_END_DECLS

#endif /* __GST_SPDIFSINK_H__ */
