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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstaudiospectrum.h"

GST_DEBUG_CATEGORY_STATIC (audio_spectrum_debug);
#define GST_CAT_DEFAULT audio_spectrum_debug

typedef void (*spectrum_func_cb) (void* userdata, gint *bands, gint size);

enum
{
	PROP_0,
  PROP_SPECTRUM_CB,
	PROP_SPECTRUM_CB_USERDATA,
};


static GstStaticPadTemplate gst_audio_spectrum_src_template =
GST_STATIC_PAD_TEMPLATE (GST_BASE_TRANSFORM_SRC_NAME,
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int"));

static GstStaticPadTemplate gst_audio_spectrum_sink_template =
GST_STATIC_PAD_TEMPLATE (GST_BASE_TRANSFORM_SINK_NAME,
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int"));

static GstFlowReturn gst_audio_spectrum_transform_ip (GstBaseTransform * object, GstBuffer * in);

static gboolean gst_audio_spectrum_set_caps (GstBaseTransform * base, GstCaps * incaps, GstCaps * outcaps);

static void gst_audio_spectrum_finalize (GObject * object);

static void gst_audio_spectrum_set_property (GObject * object, guint prop_id,
	const GValue * value, GParamSpec * pspec);

static void gst_audio_spectrum_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static audioframe_s get_audioframe(GstBuffer* buffer);

GST_BOILERPLATE (GstAudioSpectrum, gst_audio_spectrum, GstBaseTransform,
    GST_TYPE_BASE_TRANSFORM);

static void
gst_audio_spectrum_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (element_class, "Audio Spectrum Analysis Element",
      "Filter/Audio",
      "Audio Spectrum Analysis Element",
      "< >");

  gst_element_class_add_static_pad_template (element_class,
      &gst_audio_spectrum_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_audio_spectrum_src_template);
}

static void
gst_audio_spectrum_class_init (GstAudioSpectrumClass * g_class)
{
  GObjectClass *gobject_class = (GObjectClass *) g_class;
  GstBaseTransformClass *trans_class = (GstBaseTransformClass *) g_class;

  gobject_class->set_property = gst_audio_spectrum_set_property;
  gobject_class->get_property = gst_audio_spectrum_get_property;

  gobject_class->finalize = gst_audio_spectrum_finalize;

  trans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_audio_spectrum_transform_ip);
	trans_class->set_caps = GST_DEBUG_FUNCPTR(gst_audio_spectrum_set_caps);

	g_object_class_install_property (gobject_class, PROP_SPECTRUM_CB,
  	g_param_spec_pointer ("spectrum-cb", "spectrum callback function to recv analysis results",
          "pointer of calback function to get analysis results - "
          "typedef void (*spectrum_func_cb) (void* userdata, gint *bands, gint size);", G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_SPECTRUM_CB_USERDATA,
		g_param_spec_pointer ("spectrum-cb-userdata", "userdata for spectrum callback function",
				"pointer of userdata for spectrum callback function - "
				"typedef void (*spectrum_func_cb) (void* userdata, gint *bands, gint size);", G_PARAM_READWRITE));
}

static void
gst_audio_spectrum_init (GstAudioSpectrum * spectrum, GstAudioSpectrumClass * g_class)
{
	audiospectrum_create(&spectrum->analyser);
	spectrum->spectrum_cb = NULL;
	spectrum->spectrum_cb_userdata = NULL;
}

static void
gst_audio_spectrum_finalize (GObject * object)
{
  GstAudioSpectrum *spectrum = GST_AUDIO_SPECTRUM (object);

	audiospectrum_destroy(spectrum->analyser);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstFlowReturn
gst_audio_spectrum_transform_ip (GstBaseTransform * object, GstBuffer * buffer)
{
	GstAudioSpectrum *spectrum = GST_AUDIO_SPECTRUM (object);

	if(spectrum->spectrum_cb)
	{
		audiospectum_band_s analysis;
		audioframe_s frame = get_audioframe(buffer);

		GST_INFO_OBJECT(spectrum, "frame %p, %d, %d, %d, %d, %d", 
			frame.data, frame.size, frame.num_ch, frame.sample_freq, frame.sample_size, frame.interleaved);

		if(audiospectrum_analyse(spectrum->analyser, &frame, &analysis) == AUDIOSPECTRUM_ERROR_NONE)
		{
			((spectrum_func_cb)spectrum->spectrum_cb)(spectrum->spectrum_cb_userdata, analysis.bands, 31);
		}
	}

  return GST_FLOW_OK;
}

gboolean gst_audio_spectrum_set_caps (GstBaseTransform * base, GstCaps * incaps, GstCaps * outcaps)
{
	return TRUE;
}

static void
gst_audio_spectrum_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
	GstAudioSpectrum *spectrum = GST_AUDIO_SPECTRUM (object);	

  switch (prop_id) {
		case PROP_SPECTRUM_CB:
			{
				spectrum->spectrum_cb = (void*)g_value_get_pointer(value);
				GST_INFO_OBJECT (spectrum, "set spectrum returning callback function : %p", spectrum->spectrum_cb);
			}
			break;
		case PROP_SPECTRUM_CB_USERDATA:
			{
				spectrum->spectrum_cb_userdata = (void*)g_value_get_pointer(value);
				GST_INFO_OBJECT (spectrum, "set userdata for spectrum returning callback function : %p", spectrum->spectrum_cb_userdata);
			}
			break;		
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_audio_spectrum_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
	GstAudioSpectrum *spectrum = GST_AUDIO_SPECTRUM (object);

  switch (prop_id) {
		case PROP_SPECTRUM_CB:
			{
				g_value_set_pointer(value, spectrum->spectrum_cb);
			}
			break;
		case PROP_SPECTRUM_CB_USERDATA:
			{
				g_value_set_pointer(value, spectrum->spectrum_cb_userdata);
			}
			break;		
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static audioframe_s get_audioframe(GstBuffer* buffer)
{
	gint channels, samplerate, samplesize;
			gboolean interleaved = TRUE;

	GstStructure* structure = gst_caps_get_structure(GST_BUFFER_CAPS(buffer), 0);

	gst_structure_get_int(structure, "channels", &channels);
	gst_structure_get_int(structure, "rate", &samplerate);
	gst_structure_get_int(structure, "width", &samplesize); 

	audioframe_s frame =
	{
		GST_BUFFER_DATA(buffer),
		GST_BUFFER_SIZE(buffer),
		channels,
		samplerate,
		((samplesize == 16) ? 2 : 4),
		TRUE /* interleaved */
	};

	return frame;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
	GST_DEBUG_CATEGORY_INIT (audio_spectrum_debug, "audiospectrum", 0, "audio spectrum analysis element");

	return gst_element_register (plugin, "audiospectrum", 0, GST_TYPE_AUDIO_SPECTRUM);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		"audiospectrum",
		"Audio Spectrum Analysis Element",
		plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)


