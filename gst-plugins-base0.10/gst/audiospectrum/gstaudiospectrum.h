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

#ifndef __GST_AUDIO_SPECTRUM_H__
#define __GST_AUDIO_SPECTRUM_H__

#include <gst/base/gstbasetransform.h>

#include "utils/spectrum.h"

G_BEGIN_DECLS

#define GST_TYPE_AUDIO_SPECTRUM (gst_audio_spectrum_get_type())
#define GST_AUDIO_SPECTRUM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AUDIO_SPECTRUM,GstAudioSpectrum))
#define GST_AUDIO_SPECTRUM_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AUDIO_SPECTRUM,GstAudioSpectrumClass))
#define GST_IS_AUDIO_SPECTRUM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AUDIO_SPECTRUM))
#define GST_IS_AUDIO_SPECTRUM_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AUDIO_SPECTRUM))

GType gst_audio_spectrum_get_type (void);

typedef struct _GstAudioSpectrum GstAudioSpectrum;
typedef struct _GstAudioSpectrumClass GstAudioSpectrumClass;

struct _GstAudioSpectrum
{
  GstBaseTransform parent;
	audiospectrum_h analyser;
	void* spectrum_cb;
	void* spectrum_cb_userdata;
};

struct _GstAudioSpectrumClass
{
  GstBaseTransformClass parent_class;
};

G_END_DECLS

#endif /* __GST_AUDIO_SPECTRUM_H__ */

