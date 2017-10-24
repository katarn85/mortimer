/*
 * audioeq
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Aditi Narula <aditi.n@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstaudioeq.h"

GST_DEBUG_CATEGORY_STATIC (gst_audioeq_debug);
#define GST_CAT_DEFAULT gst_audioeq_debug

#define AUDIOEQ_ENABLE_DUMP
#define AUDIOEQ_REDUCE_MEMCPY

/* Filter signals and args */
enum
{
	/* FILL ME */
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_FILTER_ACTION,
	PROP_CUSTOM_EQ,
	PROP_CUSTOM_EQ_NUM,
	PROP_CUSTOM_EQ_FREQ,
	PROP_CUSTOM_EQ_WIDTH,
};

enum FilterActionType
{
	FILTER_NONE,
	FILTER_PRESET,
	FILTER_ADVANCED_SETTING
};

enum SampleRate
{
	SAMPLERATE_48000Hz,
	SAMPLERATE_44100Hz,
	SAMPLERATE_32000Hz,
	SAMPLERATE_24000Hz,
	SAMPLERATE_22050Hz,
	SAMPLERATE_16000Hz,
	SAMPLERATE_12000Hz,
	SAMPLERATE_11025Hz,
	SAMPLERATE_8000Hz,

	SAMPLERATE_NUM
};

#define DEFAULT_SAMPLE_SIZE			2
#define DEFAULT_VOLUME					15
#define DEFAULT_GAIN					1
#define DEFAULT_SAMPLE_RATE			SAMPLERATE_44100Hz
#define DEAFULT_CHANNELS				2
#define DEFAULT_FILTER_ACTION			FILTER_NONE
#define DEFAULT_CUSTOM_EQ_NUM 		7

static GstStaticPadTemplate sinktemplate =
	GST_STATIC_PAD_TEMPLATE(
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (
			"audio/x-raw-int, "
			"endianness = (int) " G_STRINGIFY (G_BYTE_ORDER) ", "
			"signed = (boolean) true, "
			"width = (int) 16, "
			"depth = (int) 16, "
			"channels = (int) [1,2]"
			)
	);

static GstStaticPadTemplate srctemplate =
	GST_STATIC_PAD_TEMPLATE(
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (
			"audio/x-raw-int, "
			"endianness = (int) " G_STRINGIFY (G_BYTE_ORDER) ", "
			"signed = (boolean) true, "
			"width = (int) 16, "
			"depth = (int) 16, "
			"channels = (int) [1,2]"
			)
	);

static void gst_iir_equalizer_child_proxy_interface_init (gpointer g_iface,
    gpointer iface_data);

static void gst_iir_equalizer_finalize (GObject * object);

static gboolean gst_iir_equalizer_setup (GstAudioFilter * filter,
    GstRingBufferSpec * fmt);

static void
_do_init (GType object_type)
{
  const GInterfaceInfo child_proxy_interface_info = {
    (GInterfaceInitFunc) gst_iir_equalizer_child_proxy_interface_init,
    NULL,                       /* interface_finalize */
    NULL                        /* interface_data */
  };

  g_type_add_interface_static (object_type, GST_TYPE_CHILD_PROXY,
      &child_proxy_interface_info);
}

GST_BOILERPLATE_FULL(Gstaudioeq, gst_audioeq, GstBaseTransform, GST_TYPE_BASE_TRANSFORM,_do_init);

static void gst_audioeq_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_audioeq_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
#ifdef AUDIOEQ_REDUCE_MEMCPY
static GstFlowReturn gst_audioeq_transform_ip (GstBaseTransform * base, GstBuffer * buf);
#else
static GstFlowReturn gst_audioeq_transform (GstBaseTransform * base, GstBuffer * inbuf, GstBuffer * outbuf);
#endif
static gboolean gst_audioeq_set_caps (GstBaseTransform * base, GstCaps * incaps, GstCaps * outcaps);

static GstStateChangeReturn
gst_audioeq_change_state (GstElement * element, GstStateChange transition)
{
  GST_DEBUG ("gst_audioeq_change_state");
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	Gstaudioeq *audioeq = GST_AUDIOEQ (element);

	switch (transition) {
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		audioeq->need_update_filter = TRUE;
		break;
	default:
		break;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

	if (ret == GST_STATE_CHANGE_FAILURE)
		return ret;

	switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		break;
	default:
		break;
	}

	return ret;
}

static void
gst_audioeq_base_init (gpointer gclass)
{

  GST_DEBUG ("gst_audioeq_base_init");
	static GstElementDetails element_details = {
		"Audio Equalizer",
		"Filter/Effect/Audio",
		"Set equalisation effect on audio/raw streams",
		"Samsung Electronics <www.samsung.com>"
	};

	GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

	gst_element_class_add_pad_template(element_class,
		gst_static_pad_template_get (&srctemplate));
	gst_element_class_add_pad_template(element_class,
		gst_static_pad_template_get (&sinktemplate));

	gst_element_class_set_details(element_class, &element_details);
}

static void
gst_audioeq_class_init (GstaudioeqClass * klass)
{
       GST_DEBUG ("gst_audioeq_class_init");
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	GstBaseTransformClass *basetransform_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gstelement_class = GST_ELEMENT_CLASS (klass);
	basetransform_class = GST_BASE_TRANSFORM_CLASS(klass);

	gobject_class->set_property = GST_DEBUG_FUNCPTR(gst_audioeq_set_property);
	gobject_class->get_property = GST_DEBUG_FUNCPTR(gst_audioeq_get_property);

	gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_audioeq_change_state);

	g_object_class_install_property(gobject_class, PROP_FILTER_ACTION,
		g_param_spec_uint("filter-action", "filter action", "(0)none (1)preset (2)advanced setting",
		0, 2, DEFAULT_FILTER_ACTION, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_CUSTOM_EQ,
		g_param_spec_pointer("custom-eq", "custom eq",
		"pointer for 9 bands of EQ array", G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_CUSTOM_EQ_NUM,
		g_param_spec_uint("custom-eq-num", "custom eq num", "number of custom EQ bands",
		0, 9, DEFAULT_CUSTOM_EQ_NUM, G_PARAM_READABLE));

	g_object_class_install_property(gobject_class, PROP_CUSTOM_EQ_FREQ,
		g_param_spec_pointer("custom-eq-freq", "custom eq freq", "pointer for EQ bands central frequency(Hz) array",
		G_PARAM_READABLE));

	g_object_class_install_property(gobject_class, PROP_CUSTOM_EQ_WIDTH,
		g_param_spec_pointer("custom-eq-width", "custom eq width", "pointer for EQ bands width(Hz) array",
		G_PARAM_READABLE));

	gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_iir_equalizer_finalize);

/* It is possible to reduce memcpy by setting output same as input of AudioEq_InOutConfig */
#ifdef AUDIOEQ_REDUCE_MEMCPY
	basetransform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_audioeq_transform_ip);
#endif
	basetransform_class->set_caps = GST_DEBUG_FUNCPTR(gst_audioeq_set_caps);
}

static void
gst_audioeq_init (Gstaudioeq * audioeq, GstaudioeqClass * gclass)
{
  GST_DEBUG ("gst_audioeq_init");
	audioeq->samplerate = DEFAULT_SAMPLE_RATE;
	audioeq->channels = DEAFULT_CHANNELS;

	audioeq->filter_action = DEFAULT_FILTER_ACTION;
	memset(audioeq->custom_eq, 0x00, sizeof(gint) * CUSTOM_EQ_BAND_MAX);
	audioeq->need_update_filter = TRUE;
	audioeq->equ.bands_lock = g_mutex_new ();
	audioeq->equ.need_new_coefficients = TRUE;
	gst_iir_equalizer_compute_frequencies (audioeq, DEFAULT_CUSTOM_EQ_NUM);
}
/* equalizer implementation */

static void
gst_iir_equalizer_finalize (GObject * object)
{
  GST_DEBUG ("gst_iir_equalizer_finalize");
  Gstaudioeq *audioeq = GST_AUDIOEQ(object);
  GstIirEqualizer *equ = &audioeq->equ;
  gint i;

  for (i = 0; i < equ->freq_band_count; i++) {
    if (equ->bands[i])
      gst_object_unparent (GST_OBJECT (equ->bands[i]));
    equ->bands[i] = NULL;
  }
  equ->freq_band_count = 0;

  g_free (equ->bands);
  g_free (equ->history);

  g_mutex_free (equ->bands_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

#define BANDS_LOCK(equ) g_mutex_lock(equ->bands_lock)
#define BANDS_UNLOCK(equ) g_mutex_unlock(equ->bands_lock)

/* child object */

enum
{
  PROP_GAIN = 1,
  PROP_FREQ,
  PROP_BANDWIDTH,
  PROP_TYPE
};

typedef enum
{
  BAND_TYPE_PEAK = 0,
  BAND_TYPE_LOW_SHELF,
  BAND_TYPE_HIGH_SHELF
} GstIirEqualizerBandType;

#define GST_TYPE_IIR_EQUALIZER_BAND_TYPE (gst_iir_equalizer_band_type_get_type ())
static GType
gst_iir_equalizer_band_type_get_type (void)
{
  GST_DEBUG ("gst_iir_equalizer_band_type_get_type");
  static GType gtype = 0;

  if (gtype == 0) {
    static const GEnumValue values[] = {
      {BAND_TYPE_PEAK, "Peak filter (default for inner bands)", "peak"},
      {BAND_TYPE_LOW_SHELF, "Low shelf filter (default for first band)",
          "low-shelf"},
      {BAND_TYPE_HIGH_SHELF, "High shelf filter (default for last band)",
          "high-shelf"},
      {0, NULL, NULL}
    };

    gtype = g_enum_register_static ("GstIirEqualizerBandType", values);
  }
  return gtype;
}


typedef struct _GstIirEqualizerBandClass GstIirEqualizerBandClass;

#define GST_TYPE_IIR_EQUALIZER_BAND \
  (gst_iir_equalizer_band_get_type())
#define GST_IIR_EQUALIZER_BAND(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_IIR_EQUALIZER_BAND,GstIirEqualizerBand))
#define GST_IIR_EQUALIZER_BAND_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_IIR_EQUALIZER_BAND,GstIirEqualizerBandClass))
#define GST_IS_IIR_EQUALIZER_BAND(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_IIR_EQUALIZER_BAND))
#define GST_IS_IIR_EQUALIZER_BAND_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_IIR_EQUALIZER_BAND))

struct _GstIirEqualizerBand
{
  GstObject object;

  /*< private > */
  /* center frequency and gain */
  gdouble freq;
  gdouble gain;
  gdouble width;
  GstIirEqualizerBandType type;

  /* second order iir filter */
  gdouble b1, b2;               /* IIR coefficients for outputs */
  gdouble a0, a1, a2;           /* IIR coefficients for inputs */
};

struct _GstIirEqualizerBandClass
{
  GstObjectClass parent_class;
};

static GType gst_iir_equalizer_band_get_type (void);

static void
gst_iir_equalizer_band_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GST_DEBUG ("gst_iir_equalizer_band_set_property");
  GstIirEqualizerBand *band = GST_IIR_EQUALIZER_BAND (object);
  Gstaudioeq *audioeq = GST_AUDIOEQ (gst_object_get_parent (GST_OBJECT (band)));
  GstIirEqualizer *equ = &audioeq->equ;

  switch (prop_id) {
    case PROP_GAIN:{
      gdouble gain;

      gain = g_value_get_double (value);
      GST_DEBUG_OBJECT (band, "gain = %lf -> %lf", band->gain, gain);
      if (gain != band->gain) {
        BANDS_LOCK (equ);
        equ->need_new_coefficients = TRUE;
        band->gain = gain;
        BANDS_UNLOCK (equ);
        GST_DEBUG_OBJECT (band, "changed gain = %lf ", band->gain);
      }
      break;
    }
    case PROP_FREQ:{
      gdouble freq;

      freq = g_value_get_double (value);
      GST_DEBUG_OBJECT (band, "freq = %lf -> %lf", band->freq, freq);
      if (freq != band->freq) {
        BANDS_LOCK (equ);
        equ->need_new_coefficients = TRUE;
        band->freq = freq;
        BANDS_UNLOCK (equ);
        GST_DEBUG_OBJECT (band, "changed freq = %lf ", band->freq);
      }
      break;
    }
    case PROP_BANDWIDTH:{
      gdouble width;

      width = g_value_get_double (value);
      GST_DEBUG_OBJECT (band, "width = %lf -> %lf", band->width, width);
      if (width != band->width) {
        BANDS_LOCK (equ);
        equ->need_new_coefficients = TRUE;
        band->width = width;
        BANDS_UNLOCK (equ);
        GST_DEBUG_OBJECT (band, "changed width = %lf ", band->width);
      }
      break;
    }
    case PROP_TYPE:{
      GstIirEqualizerBandType type;

      type = g_value_get_enum (value);
      GST_DEBUG_OBJECT (band, "type = %d -> %d", band->type, type);
      if (type != band->type) {
        BANDS_LOCK (equ);
        equ->need_new_coefficients = TRUE;
        band->type = type;
        BANDS_UNLOCK (equ);
        GST_DEBUG_OBJECT (band, "changed type = %d ", band->type);
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  gst_object_unref (audioeq);
}

static void
gst_iir_equalizer_band_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GST_DEBUG ("gst_iir_equalizer_band_get_property");
  GstIirEqualizerBand *band = GST_IIR_EQUALIZER_BAND (object);

  switch (prop_id) {
    case PROP_GAIN:
      g_value_set_double (value, band->gain);
      break;
    case PROP_FREQ:
      g_value_set_double (value, band->freq);
      break;
    case PROP_BANDWIDTH:
      g_value_set_double (value, band->width);
      break;
    case PROP_TYPE:
      g_value_set_enum (value, band->type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_iir_equalizer_band_class_init (GstIirEqualizerBandClass * klass)
{
  GST_DEBUG ("gst_iir_equalizer_band_class_init");
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_iir_equalizer_band_set_property;
  gobject_class->get_property = gst_iir_equalizer_band_get_property;

  g_object_class_install_property (gobject_class, PROP_GAIN,
      g_param_spec_double ("gain", "gain",
          "gain for the frequency band ranging from -12.0 dB to +12.0 dB",
          -12.0, 12.0, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

  g_object_class_install_property (gobject_class, PROP_FREQ,
      g_param_spec_double ("freq", "freq",
          "center frequency of the band",
          0.0, 100000.0, 0.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

  g_object_class_install_property (gobject_class, PROP_BANDWIDTH,
      g_param_spec_double ("bandwidth", "bandwidth",
          "difference between bandedges in Hz",
          0.0, 100000.0, 1.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));

  g_object_class_install_property (gobject_class, PROP_TYPE,
      g_param_spec_enum ("type", "Type",
          "Filter type", GST_TYPE_IIR_EQUALIZER_BAND_TYPE,
          BAND_TYPE_PEAK,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_CONTROLLABLE));
}

static void
gst_iir_equalizer_band_init (GstIirEqualizerBand * band,
    GstIirEqualizerBandClass * klass)
{
  GST_DEBUG ("gst_iir_equalizer_band_init");
  band->freq = 0.0;
  band->gain = 0.0;
  band->width = 1.0;
  band->type = BAND_TYPE_PEAK;
}

static GType
gst_iir_equalizer_band_get_type (void)
{
  GST_DEBUG ("gst_iir_equalizer_band_get_type");
  static GType type = 0;

  if (G_UNLIKELY (!type)) {
    const GTypeInfo type_info = {
      sizeof (GstIirEqualizerBandClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_iir_equalizer_band_class_init,
      NULL,
      NULL,
      sizeof (GstIirEqualizerBand),
      0,
      (GInstanceInitFunc) gst_iir_equalizer_band_init,
    };
    type =
        g_type_register_static (GST_TYPE_OBJECT, "GstIirEqualizerBand",
        &type_info, 0);
  }
  return (type);
}


/* child proxy iface */
static GstObject *
gst_iir_equalizer_child_proxy_get_child_by_index (GstChildProxy * child_proxy,
    guint index)
{
  GST_DEBUG ("gst_iir_equalizer_child_proxy_get_child_by_index");
  Gstaudioeq *audioeq = GST_AUDIOEQ(child_proxy);
  GstIirEqualizer *equ = &audioeq->equ;
  GstObject *ret;

  BANDS_LOCK (equ);
  if (G_UNLIKELY (index >= equ->freq_band_count)) {
    BANDS_UNLOCK (equ);
    g_return_val_if_fail (index < equ->freq_band_count, NULL);
  }

  ret = gst_object_ref (equ->bands[index]);
  BANDS_UNLOCK (equ);

  GST_LOG_OBJECT (equ, "return child[%d] %" GST_PTR_FORMAT, index, ret);
  return ret;
}

static guint
gst_iir_equalizer_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  GST_DEBUG ("gst_iir_equalizer_child_proxy_get_children_count");
  Gstaudioeq *audioeq = GST_AUDIOEQ(child_proxy);
  GstIirEqualizer *equ = &audioeq->equ;

  GST_LOG ("we have %d children", equ->freq_band_count);
  return equ->freq_band_count;
}

static void
gst_iir_equalizer_child_proxy_interface_init (gpointer g_iface,
    gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  GST_DEBUG ("initializing iface");

  iface->get_child_by_index = gst_iir_equalizer_child_proxy_get_child_by_index;
  iface->get_children_count = gst_iir_equalizer_child_proxy_get_children_count;
}
static void
gst_iir_equalizer_class_init (GstIirEqualizerClass * klass)
{
  GST_DEBUG ("gst_iir_equalizer_class_init");
}

static void
gst_iir_equalizer_init (GstIirEqualizer * eq, GstIirEqualizerClass * g_class)
{
  GST_DEBUG ("gst_iir_equalizer_init");
  eq->bands_lock = g_mutex_new ();
  eq->need_new_coefficients = TRUE;
}

GType
gst_iir_equalizer_get_type (void)
{
  GST_DEBUG ("gst_iir_equalizer_get_type");
  static GType type = 0;

  if (G_UNLIKELY (!type)) {
    const GTypeInfo type_info = {
      sizeof (GstIirEqualizerClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_iir_equalizer_class_init,
      NULL,
      NULL,
      sizeof (GstIirEqualizer),
      0,
      (GInstanceInitFunc) gst_iir_equalizer_init,
    };
    type =
        g_type_register_static (GST_TYPE_OBJECT, "GstIirEqualizer",
        &type_info, 0);
  }
  return (type);
}
/* Filter taken from
 *
 * The Equivalence of Various Methods of Computing
 * Biquad Coefficients for Audio Parametric Equalizers
 *
 * by Robert Bristow-Johnson
 *
 * http://www.aes.org/e-lib/browse.cfm?elib=6326
 * http://www.musicdsp.org/files/EQ-Coefficients.pdf
 * http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt
 *
 * The bandwidth method that we use here is the preferred
 * one from this article transformed from octaves to frequency
 * in Hz.
 */
static inline gdouble
arg_to_scale (gdouble arg)
{
  GST_DEBUG ("arg_to_scale");
  return (pow (10.0, arg / 40.0));
}

static gdouble
calculate_omega (gdouble freq, gint rate)
{
  GST_DEBUG ("calculate_omega");
  gdouble omega;

  if (freq / rate >= 0.5)
    omega = G_PI;
  else if (freq <= 0.0)
    omega = 0.0;
  else
    omega = 2.0 * G_PI * (freq / rate);

  return omega;
}

static gdouble
calculate_bw (GstIirEqualizerBand * band, gint rate)
{
  GST_DEBUG ("calculate_bw");
  gdouble bw = 0.0;

  if (band->width / rate >= 0.5) {
    /* If bandwidth == 0.5 the calculation below fails as tan(G_PI/2)
     * is undefined. So set the bandwidth to a slightly smaller value.
     */
    bw = G_PI - 0.00000001;
  } else if (band->width <= 0.0) {
    /* If bandwidth == 0 this band won't change anything so set
     * the coefficients accordingly. The coefficient calculation
     * below would create coefficients that for some reason amplify
     * the band.
     */
    band->a0 = 1.0;
    band->a1 = 0.0;
    band->a2 = 0.0;
    band->b1 = 0.0;
    band->b2 = 0.0;
  } else {
    bw = 2.0 * G_PI * (band->width / rate);
  }
  return bw;
}

static void
setup_peak_filter (Gstaudioeq* audioeq, GstIirEqualizerBand * band)
{
  GST_DEBUG ("setup_peak_filter");
  //g_return_if_fail (GST_AUDIO_FILTER (equ)->format.rate);

  {
    gdouble gain, omega, bw;
    gdouble alpha, alpha1, alpha2, b0;

    gain = arg_to_scale (band->gain);
    omega = calculate_omega (band->freq, audioeq->samplerate);
    bw = calculate_bw (band, audioeq->samplerate);
    if (bw == 0.0)
      goto out;

    alpha = tan (bw / 2.0);

    alpha1 = alpha * gain;
    alpha2 = alpha / gain;

    b0 = (1.0 + alpha2);

    band->a0 = (1.0 + alpha1) / b0;
    band->a1 = (-2.0 * cos (omega)) / b0;
    band->a2 = (1.0 - alpha1) / b0;
    band->b1 = (2.0 * cos (omega)) / b0;
    band->b2 = -(1.0 - alpha2) / b0;

  out:
    GST_INFO
        ("gain = %5.1f, width= %7.2f, freq = %7.2f, a0 = %7.5g, a1 = %7.5g, a2=%7.5g b1 = %7.5g, b2 = %7.5g",
        band->gain, band->width, band->freq, band->a0, band->a1, band->a2,
        band->b1, band->b2);
  }
}

static void
setup_low_shelf_filter (Gstaudioeq* audioeq, GstIirEqualizerBand * band)
{
  GST_DEBUG ("setup_low_shelf_filter");
  //g_return_if_fail (GST_AUDIO_FILTER (equ)->format.rate);

  {
    gdouble gain, omega, bw;
    gdouble alpha, delta, b0;
    gdouble egp, egm;

    gain = arg_to_scale (band->gain);
    omega = calculate_omega (band->freq, audioeq->samplerate);
    bw = calculate_bw (band, audioeq->samplerate);
    if (bw == 0.0)
      goto out;

    egm = gain - 1.0;
    egp = gain + 1.0;
    alpha = tan (bw / 2.0);

    delta = 2.0 * sqrt (gain) * alpha;
    b0 = egp + egm * cos (omega) + delta;

    band->a0 = ((egp - egm * cos (omega) + delta) * gain) / b0;
    band->a1 = ((egm - egp * cos (omega)) * 2.0 * gain) / b0;
    band->a2 = ((egp - egm * cos (omega) - delta) * gain) / b0;
    band->b1 = ((egm + egp * cos (omega)) * 2.0) / b0;
    band->b2 = -((egp + egm * cos (omega) - delta)) / b0;


  out:
    GST_INFO
        ("gain = %5.1f, width= %7.2f, freq = %7.2f, a0 = %7.5g, a1 = %7.5g, a2=%7.5g b1 = %7.5g, b2 = %7.5g",
        band->gain, band->width, band->freq, band->a0, band->a1, band->a2,
        band->b1, band->b2);
  }
}

static void
setup_high_shelf_filter (Gstaudioeq* audioeq, GstIirEqualizerBand * band)
{
  GST_DEBUG ("setup_high_shelf_filter");
  {
    gdouble gain, omega, bw;
    gdouble alpha, delta, b0;
    gdouble egp, egm;

    gain = arg_to_scale (band->gain);
    omega = calculate_omega (band->freq, audioeq->samplerate);
    bw = calculate_bw (band, audioeq->samplerate);
    if (bw == 0.0)
      goto out;

    egm = gain - 1.0;
    egp = gain + 1.0;
    alpha = tan (bw / 2.0);

    delta = 2.0 * sqrt (gain) * alpha;
    b0 = egp - egm * cos (omega) + delta;

    band->a0 = ((egp + egm * cos (omega) + delta) * gain) / b0;
    band->a1 = ((egm + egp * cos (omega)) * -2.0 * gain) / b0;
    band->a2 = ((egp + egm * cos (omega) - delta) * gain) / b0;
    band->b1 = ((egm - egp * cos (omega)) * -2.0) / b0;
    band->b2 = -((egp - egm * cos (omega) - delta)) / b0;


  out:
    GST_INFO
        ("gain = %5.1f, width= %7.2f, freq = %7.2f, a0 = %7.5g, a1 = %7.5g, a2=%7.5g b1 = %7.5g, b2 = %7.5g",
        band->gain, band->width, band->freq, band->a0, band->a1, band->a2,
        band->b1, band->b2);
  }
}

/* Must be called with bands_lock and transform lock! */
static void
set_passthrough (Gstaudioeq* audioeq)
{
  GST_DEBUG ("set_passthrough");
  GstIirEqualizer* equ=&audioeq->equ;
  gint i;
  gboolean passthrough = TRUE;

  for (i = 0; i < equ->freq_band_count; i++) {
    passthrough = passthrough && (equ->bands[i]->gain == 0.0);
  }

  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (audioeq), passthrough);
  GST_DEBUG ("Passthrough mode: %d\n", passthrough);
}

/* Must be called with bands_lock and transform lock! */
static void
update_coefficients (Gstaudioeq* audioeq)
{
  GST_DEBUG ("update_coefficients");
  GstIirEqualizer* equ=&audioeq->equ;
  gint i, n = equ->freq_band_count;

  for (i = 0; i < n; i++) {
    if (equ->bands[i]->type == BAND_TYPE_PEAK)
      setup_peak_filter (audioeq, equ->bands[i]);
    else if (equ->bands[i]->type == BAND_TYPE_LOW_SHELF)
      setup_low_shelf_filter (audioeq, equ->bands[i]);
    else
      setup_high_shelf_filter (audioeq, equ->bands[i]);
  }

  equ->need_new_coefficients = FALSE;
}

/* Must be called with transform lock! */
static void
alloc_history (GstIirEqualizer * equ)
{
  GST_DEBUG ("alloc_history");
  /* free + alloc = no memcpy */
  g_free (equ->history);
  equ->history =
      g_malloc0 (equ->history_size *  equ->audiofilter.format.channels *
      equ->freq_band_count);
}
void
gst_audioeq_band_set_property(Gstaudioeq * audioeq)
{
  GST_DEBUG ("gst_audioeq_band_set_property");
  GstIirEqualizer *equ = &audioeq->equ;
  gshort i ;
  for ( i = 0; i < DEFAULT_CUSTOM_EQ_NUM; i++){
     GST_DEBUG ("gain = %lf -> %d", equ->bands[i]->gain, audioeq->custom_eq[i] );
     if (audioeq->custom_eq[i] != equ->bands[i]->gain) {
        equ->bands[i]->gain = audioeq->custom_eq[i];
        GST_DEBUG("changed gain = %lf ", equ->bands[i]->gain);
        g_object_notify (G_OBJECT (equ->bands[i]), "gain");
     }
  }
}

void
gst_iir_equalizer_compute_frequencies (Gstaudioeq * audioeq, guint new_count)
{
  GST_DEBUG ("gst_iir_equalizer_compute_frequencies");

  GstIirEqualizer *equ = &audioeq->equ;
  guint old_count, i;
  gdouble freq0, freq1, step;
  gchar name[20];
  GST_DEBUG ("gst_iir_equalizer_compute_frequencies before calling equalizer object");
  if (equ->freq_band_count == new_count)
    return;

  GST_DEBUG ("gst_iir_equalizer_compute_frequencies equalizer object");

  BANDS_LOCK (equ);
  GST_DEBUG ("gst_iir_equalizer_compute_frequencies 1");
  if (G_UNLIKELY (equ->freq_band_count == new_count)) {
    BANDS_UNLOCK (equ);
    return;
  }
  GST_DEBUG ("gst_iir_equalizer_compute_frequencies 2");
  old_count = equ->freq_band_count;
  equ->freq_band_count = new_count;
  GST_DEBUG ("bands %u -> %u", old_count, new_count);

  if (old_count < new_count) {
    /* add new bands */
    equ->bands = g_realloc (equ->bands, sizeof (GstObject *) * new_count);
    for (i = old_count; i < new_count; i++) {
      equ->bands[i] = g_object_new (GST_TYPE_IIR_EQUALIZER_BAND, NULL);
      /* otherwise they get names like 'iirequalizerband5' */
      sprintf (name, "band%u", i);
      gst_object_set_name (GST_OBJECT (equ->bands[i]), name);
      GST_DEBUG ("adding band[%d]=%p", i, equ->bands[i]);

      gst_object_set_parent (GST_OBJECT (equ->bands[i]), GST_OBJECT (audioeq));
      gst_child_proxy_child_added (GST_OBJECT (audioeq),
          GST_OBJECT (equ->bands[i]));
    }
  } else {
    /* free unused bands */
    for (i = new_count; i < old_count; i++) {
      GST_DEBUG ("removing band[%d]=%p", i, equ->bands[i]);
      gst_child_proxy_child_removed (GST_OBJECT (audioeq),
          GST_OBJECT (equ->bands[i]));
      gst_object_unparent (GST_OBJECT (equ->bands[i]));
      equ->bands[i] = NULL;
    }
  }

  alloc_history (equ);

  /* set center frequencies and name band objects
   * FIXME: arg! we can't change the name of parented objects :(
   *   application should read band->freq to get the name
   */

  step = pow (HIGHEST_FREQ / LOWEST_FREQ, 1.0 / new_count);
  freq0 = LOWEST_FREQ;
  for (i = 0; i < new_count; i++) {
    freq1 = freq0 * step;

    if (i == 0)
      equ->bands[i]->type = BAND_TYPE_LOW_SHELF;
    else if (i == new_count - 1)
      equ->bands[i]->type = BAND_TYPE_HIGH_SHELF;
    else
      equ->bands[i]->type = BAND_TYPE_PEAK;

    equ->bands[i]->freq = freq0 + ((freq1 - freq0) / 2.0);
    equ->bands[i]->width = freq1 - freq0;
    GST_DEBUG ("band[%2d] = '%lf'", i, equ->bands[i]->freq);

    g_object_notify (G_OBJECT (equ->bands[i]), "bandwidth");
    g_object_notify (G_OBJECT (equ->bands[i]), "freq");
    g_object_notify (G_OBJECT (equ->bands[i]), "type");

    freq0 = freq1;
  }

  equ->need_new_coefficients = TRUE;

  BANDS_UNLOCK (equ);
}
/* start of code that is type specific */

#define CREATE_OPTIMIZED_FUNCTIONS_INT(TYPE,BIG_TYPE,MIN_VAL,MAX_VAL)   \
typedef struct {                                                        \
  BIG_TYPE x1, x2;          /* history of input values for a filter */  \
  BIG_TYPE y1, y2;          /* history of output values for a filter */ \
} SecondOrderHistory ## TYPE;                                           \
                                                                        \
static inline BIG_TYPE                                                  \
one_step_ ## TYPE (GstIirEqualizerBand *filter,                         \
    SecondOrderHistory ## TYPE *history, BIG_TYPE input)                \
{                                                                       \
  /* calculate output */                                                \
  BIG_TYPE output = filter->a0 * input +                                \
      filter->a1 * history->x1 + filter->a2 * history->x2 +             \
      filter->b1 * history->y1 + filter->b2 * history->y2;              \
  /* update history */                                                  \
  history->y2 = history->y1;                                            \
  history->y1 = output;                                                 \
  history->x2 = history->x1;                                            \
  history->x1 = input;                                                  \
                                                                        \
  return output;                                                        \
}                                                                       \
                                                                        \
static const guint                                                      \
history_size_ ## TYPE = sizeof (SecondOrderHistory ## TYPE);            \
                                                                        \
static void                                                             \
gst_iir_equ_process_ ## TYPE (GstIirEqualizer *equ, guint8 *data,       \
guint size, guint channels)                                             \
{                                                                       \
  guint frames = size / channels / sizeof (TYPE);                       \
  guint i, c, f, nf = equ->freq_band_count;                             \
  BIG_TYPE cur;                                                         \
  GstIirEqualizerBand **filters = equ->bands;                           \
                                                                        \
  for (i = 0; i < frames; i++) {                                        \
    SecondOrderHistory ## TYPE *history = equ->history;                 \
    for (c = 0; c < channels; c++) {                                    \
      cur = *((TYPE *) data);                                           \
      for (f = 0; f < nf; f++) {                                        \
        cur = one_step_ ## TYPE (filters[f], history, cur);             \
        history++;                                                      \
      }                                                                 \
      cur = CLAMP (cur, MIN_VAL, MAX_VAL);                              \
      *((TYPE *) data) = (TYPE) floor (cur);                            \
      data += sizeof (TYPE);                                            \
    }                                                                   \
  }                                                                     \
}

#define CREATE_OPTIMIZED_FUNCTIONS(TYPE)                                \
typedef struct {                                                        \
  TYPE x1, x2;          /* history of input values for a filter */  \
  TYPE y1, y2;          /* history of output values for a filter */ \
} SecondOrderHistory ## TYPE;                                           \
                                                                        \
static inline TYPE                                                      \
one_step_ ## TYPE (GstIirEqualizerBand *filter,                         \
    SecondOrderHistory ## TYPE *history, TYPE input)                    \
{                                                                       \
  /* calculate output */                                                \
  TYPE output = filter->a0 * input + filter->a1 * history->x1 +         \
      filter->a2 * history->x2 + filter->b1 * history->y1 +             \
      filter->b2 * history->y2;                                         \
  /* update history */                                                  \
  history->y2 = history->y1;                                            \
  history->y1 = output;                                                 \
  history->x2 = history->x1;                                            \
  history->x1 = input;                                                  \
                                                                        \
  return output;                                                        \
}                                                                       \
                                                                        \
static const guint                                                      \
history_size_ ## TYPE = sizeof (SecondOrderHistory ## TYPE);            \
                                                                        \
static void                                                             \
gst_iir_equ_process_ ## TYPE (GstIirEqualizer *equ, guint8 *data,       \
guint size, guint channels)                                             \
{                                                                       \
  guint frames = size / channels / sizeof (TYPE);                       \
  guint i, c, f, nf = equ->freq_band_count;                             \
  TYPE cur;                                                             \
  GstIirEqualizerBand **filters = equ->bands;                           \
                                                                        \
  for (i = 0; i < frames; i++) {                                        \
    SecondOrderHistory ## TYPE *history = equ->history;                 \
    for (c = 0; c < channels; c++) {                                    \
      cur = *((TYPE *) data);                                           \
      for (f = 0; f < nf; f++) {                                        \
        cur = one_step_ ## TYPE (filters[f], history, cur);             \
        history++;                                                      \
      }                                                                 \
      *((TYPE *) data) = (TYPE) cur;                                    \
      data += sizeof (TYPE);                                            \
    }                                                                   \
  }                                                                     \
}

CREATE_OPTIMIZED_FUNCTIONS_INT (gint16, gfloat, -32768.0, 32767.0);
CREATE_OPTIMIZED_FUNCTIONS (gfloat);
CREATE_OPTIMIZED_FUNCTIONS (gdouble);

#ifdef AUDIOEQ_REDUCE_MEMCPY
static GstFlowReturn
gst_audioeq_transform_ip (GstBaseTransform * base, GstBuffer * buf)
{
  GST_DEBUG ("gst_audioeq_transform_ip");
  Gstaudioeq *audioeq = GST_AUDIOEQ(base);
  GstIirEqualizer *equ = &audioeq->equ;

  equ->history_size = history_size_gint16;
  equ->process = gst_iir_equ_process_gint16;
   g_free (equ->history);
  equ->history = g_malloc0 (equ->history_size *  audioeq->channels * equ->freq_band_count);
  GstClockTime timestamp;

  if (G_UNLIKELY (audioeq->channels < 1 || equ->process == NULL)) {
    GST_DEBUG ("gst_audioeq_transform_ip return GST_FLOW_NOT_NEGOTIATED;");
    if (G_UNLIKELY (equ->process == NULL))
    GST_DEBUG ("gst_audioeq_transform_ip equ->process ");
    if (G_UNLIKELY (audioeq->channels < 1))
    GST_DEBUG ("gst_audioeq_transform_ip audioeq->channels");
    return GST_FLOW_NOT_NEGOTIATED;
  }
      GST_DEBUG ("gst_audioeq_transform_ip  BANDS_LOCK (equ);");
  BANDS_LOCK (equ);
  if (equ->need_new_coefficients) {
    GST_DEBUG ("gst_audioeq_transform_ip  update_coefficients");
    update_coefficients (audioeq);
    set_passthrough (audioeq);
  }
  BANDS_UNLOCK (equ);

  if (gst_base_transform_is_passthrough (base)) {
    GST_DEBUG ("gst_audioeq_transform_ip gst_base_transform_is_passthrough return GST_FLOW_OK;");
    return GST_FLOW_OK;
  }
  timestamp = GST_BUFFER_TIMESTAMP (buf);
  timestamp =
      gst_segment_to_stream_time (&base->segment, GST_FORMAT_TIME, timestamp);

  if (GST_CLOCK_TIME_IS_VALID (timestamp))
    gst_object_sync_values (G_OBJECT (audioeq), timestamp);
  GST_DEBUG ("  equ->process ++");
  equ->process (equ, GST_BUFFER_DATA (buf), GST_BUFFER_SIZE (buf),
      audioeq->channels);
  GST_DEBUG ("  equ->process --");
  GST_DEBUG ("gst_audioeq_transform_ip return GST_FLOW_OK;");
  return GST_FLOW_OK;
}
#endif

static gboolean
gst_audioeq_set_caps (GstBaseTransform * base, GstCaps * incaps,
	GstCaps * outcaps)
{
  GST_DEBUG ("gst_audioeq_set_caps");
	Gstaudioeq *audioeq = GST_AUDIOEQ(base);
	GstStructure *ins;
	GstPad *pad;
	gint samplerate;
	gint channels;
	gshort old_samplerate;
	gshort old_channels;

	pad = gst_element_get_static_pad(GST_ELEMENT(audioeq), "src");

	/* forward-negotiate */
	if(!gst_pad_set_caps(pad, incaps)) {
		gst_object_unref(pad);
		return FALSE;
	}

	/* negotiation succeeded, so now configure ourselves */
	ins = gst_caps_get_structure(incaps, 0);

	/* get samplerate from caps & convert */
	old_samplerate = audioeq->samplerate;
	old_channels = audioeq->channels;
	gst_structure_get_int(ins, "rate", &samplerate);
	switch (samplerate) {
	case 48000:
		audioeq->samplerate = SAMPLERATE_48000Hz;
		break;
	case 44100:
		audioeq->samplerate = SAMPLERATE_44100Hz;
		break;
	case 32000:
		audioeq->samplerate = SAMPLERATE_32000Hz;
		break;
	case 24000:
		audioeq->samplerate = SAMPLERATE_24000Hz;
		break;
	case 22050:
		audioeq->samplerate = SAMPLERATE_22050Hz;
		break;
	case 16000:
		audioeq->samplerate = SAMPLERATE_16000Hz;
		break;
	case 12000:
		audioeq->samplerate = SAMPLERATE_12000Hz;
		break;
	case 11025:
		audioeq->samplerate = SAMPLERATE_11025Hz;
		break;
	case 8000:
		audioeq->samplerate = SAMPLERATE_8000Hz;
		break;
	default:
		if (samplerate < 8000) {
			audioeq->samplerate = SAMPLERATE_8000Hz;
		} else if (samplerate > 48000) {
			audioeq->samplerate = SAMPLERATE_48000Hz;
		}
		break;
	}
	/* get number of channels from caps */
	gst_structure_get_int(ins, "channels", &channels);
	audioeq->channels = (gshort)channels;

	if ((old_samplerate != audioeq->samplerate)
		|| (old_channels != audioeq->channels)) {
		audioeq->need_update_filter = TRUE;
	}

	gst_object_unref (pad);

	return TRUE;
}

static void
gst_audioeq_set_property (GObject * object, guint prop_id,
	const GValue * value, GParamSpec * pspec)
{
	GST_DEBUG ("gst_audioeq_set_property");
	Gstaudioeq *audioeq = GST_AUDIOEQ (object);
	GstIirEqualizer *equ = &audioeq->equ;
	gshort *pointer;

	switch (prop_id) {

	case PROP_FILTER_ACTION:
		audioeq->filter_action = g_value_get_uint(value);
		BANDS_LOCK(equ);
		equ->need_new_coefficients = TRUE;
		BANDS_UNLOCK(equ);
		break;

	case PROP_CUSTOM_EQ:
		pointer = g_value_get_pointer(value);
		if (pointer) {
			memcpy(audioeq->custom_eq, pointer, sizeof(gint) * CUSTOM_EQ_BAND_MAX);
			if (audioeq->filter_action == FILTER_ADVANCED_SETTING) {
				BANDS_LOCK(equ);
				equ->need_new_coefficients = TRUE;
				gst_audioeq_band_set_property(audioeq);
				BANDS_UNLOCK(equ);
			}
		}
		break;

	default:
		break;
	}
	GST_DEBUG ("gst_audioeq_set_property need_update_filter %d", audioeq->need_update_filter);
}

static void
gst_audioeq_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
GST_DEBUG ("gst_audioeq_get_property");

	Gstaudioeq *audioeq = GST_AUDIOEQ (object);
	GstIirEqualizer *equ = &audioeq->equ;
	gshort i;
	gdouble widtharr[DEFAULT_CUSTOM_EQ_NUM],freqarr[DEFAULT_CUSTOM_EQ_NUM];

	switch (prop_id) {
	case PROP_FILTER_ACTION:
		g_value_set_uint(value, audioeq->filter_action);
		break;

	case PROP_CUSTOM_EQ:
		g_value_set_pointer(value, audioeq->custom_eq);
		break;

	case PROP_CUSTOM_EQ_NUM:
		g_value_set_uint(value, DEFAULT_CUSTOM_EQ_NUM);
		break;

	case PROP_CUSTOM_EQ_FREQ:
		for(i=0;i<DEFAULT_CUSTOM_EQ_NUM;i++) {
			 freqarr[i] =	equ->bands[i]->freq;
		}
		g_value_set_pointer(value, &freqarr);
		break;

	case PROP_CUSTOM_EQ_WIDTH:
		for(i=0;i<DEFAULT_CUSTOM_EQ_NUM;i++) {
			 widtharr[i] =	equ->bands[i]->width;
		}
		g_value_set_pointer(value, &widtharr);
		break;

	default:
		break;
	}
}

static gboolean
gst_iir_equalizer_setup (GstAudioFilter * audio, GstRingBufferSpec * fmt)
{
GST_DEBUG ("gst_iir_equalizer_setup");
  GstIirEqualizer *equ = GST_IIR_EQUALIZER (audio);

  switch (fmt->type) {
    case GST_BUFTYPE_LINEAR:
      switch (fmt->width) {
        case 16:
          equ->history_size = history_size_gint16;
          equ->process = gst_iir_equ_process_gint16;
          break;
        default:
          return FALSE;
      }
      break;
    case GST_BUFTYPE_FLOAT:
      switch (fmt->width) {
        case 32:
          equ->history_size = history_size_gfloat;
          equ->process = gst_iir_equ_process_gfloat;
          break;
        case 64:
          equ->history_size = history_size_gdouble;
          equ->process = gst_iir_equ_process_gdouble;
          break;
        default:
          return FALSE;
      }
      break;
    default:
      return FALSE;
  }

  alloc_history (equ);
  return TRUE;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
       GST_DEBUG ("audioeq plugin_init ");
	GST_DEBUG_CATEGORY_INIT(gst_audioeq_debug, "audioeq", 0, "Audio Equalizer Plugin");
	return gst_element_register(plugin, "audioeq", GST_RANK_NONE, GST_TYPE_AUDIOEQ);
}


GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	"audioeq",
	"Audio Equalizer Plugin",
	plugin_init,
	VERSION,
	"LGPL",
	"gst-plugins-ext",
	"https://www.tizen.org/")
