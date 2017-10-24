/* GStreamer
 * Copyright (C) 2005 Wim Taymans <wim@fluendo.com>
 * Copyright (C) 2006 Tim-Philipp Muller <tim centricular net>
 *
 * gstalsasink.c:
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
 */

/**
 * SECTION:element-alsasink
 * @see_also: alsasrc, alsamixer
 *
 * This element renders raw audio samples using the ALSA api.
 *
 * <refsect2>
 * <title>Example pipelines</title>
 * |[
 * gst-launch -v filesrc location=sine.ogg ! oggdemux ! vorbisdec ! audioconvert ! audioresample ! alsasink
 * ]| Play an Ogg/Vorbis file.
 * </refsect2>
 *
 * Last reviewed on 2006-03-01 (0.10.4)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include <alsa/asoundlib.h>

#include "gstalsa.h"
#include "gstalsasink.h"
#include "gstalsadeviceprobe.h"

#include <gst/gst-i18n-plugin.h>
#include "gst/glib-compat-private.h"
#include <audio-session-manager.h>

#include "secaudio_control.h"

#include <avoc/avoc_avsink.h>
#include <sys/time.h>

#define DEFAULT_MUTE		  TRUE
#define DEFAULT_HD_AUDIO_MODE FALSE
#define DEFAULT_DEVICE		"default"
#define DEFAULT_DEVICE_NAME	""
#define DEFAULT_CARD_NAME	""
#define SPDIF_PERIOD_SIZE 1536
#define SPDIF_BUFFER_SIZE 15360
#define MIN_LATENCY 5000
#define DEFAULT_MAINOUT_DELAY 227  //MM case default speaker delay is 227ms
#define GAME_START_THRESHOLD 10 
//#define DUMP_PCM
static int dump_count = 0;

#define ENABLE_MASK(x, y) ( (x) = ((x) | (y)) )
#define DISABLE_MASK(x, y) ( (x) = ((x) & (~(y))) )
#define FIND_MASK(x, y) ( (((x) & (y)) == (y)) ? true : false)


enum MUTE_STATE
{
  UNMUTE_STATE = 0x00,	// Do not use directly
  MUTE_DEFAULT = 0x01,  // Mute By previous resource user,  and it'll be unmute after first putimage.
  MUTE_EXTERNAL = 0x02,
};

enum
{
  PROP_0,
  PROP_MUTE,
  PROP_DEVICE,
  PROP_DEVICE_NAME,
  PROP_CARD_NAME,
  PROP_DEVICE_ID,
  PROP_UPDATE_RENDER_DELAY,
  PROP_GAME_RENDER,
  PROP_HD_AUDIO_MODE,
  PROP_MLS_FOCUSED_ZONE_ID,
  PROP_DELAYED_UNMUTE,
  PROP_LAST
};

static void gst_alsasink_init_interfaces (GType type);

GST_BOILERPLATE_FULL (GstAlsaSink, gst_alsasink, GstAudioSink,
    GST_TYPE_AUDIO_SINK, gst_alsasink_init_interfaces);

static void gst_alsasink_finalise (GObject * object);
static void gst_alsasink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_alsasink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstCaps *gst_alsasink_getcaps (GstBaseSink * bsink);

static gboolean gst_alsasink_open (GstAudioSink * asink);
static gboolean gst_alsasink_prepare (GstAudioSink * asink,
    GstRingBufferSpec * spec);
static gboolean gst_alsasink_unprepare (GstAudioSink * asink);
static gboolean gst_alsasink_close (GstAudioSink * asink);
static guint gst_alsasink_write (GstAudioSink * asink, gpointer data,
    guint length);
static guint gst_alsasink_delay (GstAudioSink * asink);
static void gst_alsasink_reset (GstAudioSink * asink);
static gboolean gst_alsasink_query (GstAudioSink * asink, GstQuery * query);

static void gst_alsasink_set_amixer_pp_onoff (GstAudioSink * asink, gboolean pp_switch);
static gboolean gst_alsasink_send_query_audiosplitter (GstAudioSink * asink);
static GstStructure* gst_alsasink_send_query_decorder (GstAudioSink * asink);
static void gst_alsasink_avoc_set_hd_audio(GstAudioSink * asink, gboolean mute, gboolean hd);
static void gst_alsasink_avoc_set_audio_info(GstAudioSink * asink, GstStructure * adec_sinkcaps, GstRingBufferSpec * spec);


static gint output_ref;         /* 0    */
static snd_output_t *output;    /* NULL */
static GStaticMutex output_mutex = G_STATIC_MUTEX_INIT;
static const gint DefaultDeviceId = -1;

static unsigned long long get_time()
{
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	return ((ts.tv_sec * GST_SECOND + ts.tv_nsec * GST_NSECOND)/1000);
}

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
# define ALSA_SINK_FACTORY_ENDIANNESS	"LITTLE_ENDIAN, BIG_ENDIAN"
#else
# define ALSA_SINK_FACTORY_ENDIANNESS	"BIG_ENDIAN, LITTLE_ENDIAN"
#endif

static GstStaticPadTemplate alsasink_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "endianness = (int) { " ALSA_SINK_FACTORY_ENDIANNESS " }, "
        "signed = (boolean) { TRUE, FALSE }, "
        "width = (int) 32, "
        "depth = (int) 32, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]; "
        "audio/x-raw-int, "
        "endianness = (int) { " ALSA_SINK_FACTORY_ENDIANNESS " }, "
        "signed = (boolean) { TRUE, FALSE }, "
        "width = (int) 24, "
        "depth = (int) 24, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]; "
        "audio/x-raw-int, "
        "endianness = (int) { " ALSA_SINK_FACTORY_ENDIANNESS " }, "
        "signed = (boolean) { TRUE, FALSE }, "
        "width = (int) 32, "
        "depth = (int) 24, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]; "
        "audio/x-raw-int, "
        "endianness = (int) { " ALSA_SINK_FACTORY_ENDIANNESS " }, "
        "signed = (boolean) { TRUE, FALSE }, "
        "width = (int) 16, "
        "depth = (int) 16, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]; "
        "audio/x-raw-int, "
        "signed = (boolean) { TRUE, FALSE }, "
        "width = (int) 8, "
        "depth = (int) 8, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ];"
        "audio/x-iec958")
    );


static void gst_alsasink_set_mainout_mmsrc(GstAudioSink* sink)
{
  int result = -1;
  GstAlsaSink *alsa;
  alsa = GST_ALSA_SINK (sink);

  if(g_str_equal(alsa->device, "hw:0,0") == FALSE) // Only the main alsasink can control the source selection.
  {
    GST_INFO_OBJECT(alsa, "The sub alsasink is cannot select source[ %s ]", alsa->device);
    return;
  }
 
  guint64 T1=0;
  guint64 T2=0;
  T1= get_time();
  avoc_mls_current_focused_zone(&alsa->focused_zone);
  T2 = get_time();
  if(alsa->focused_zone == -1)
  {
  result = sec_ctl_set_enumName (0,"main out source select", "MM" );
  }
  GST_ERROR_OBJECT(alsa, "focused zone[%d] main out source select MM, result=%d [ %lld ms ]", alsa->focused_zone,result, (T2-T1)/1000);
}

static void gst_alsasink_set_mainout_mute(GstAudioSink* sink, gboolean bMuteOn, guint mask)
{
	GstAlsaSink *alsa = GST_ALSA_SINK (sink);
	gboolean accepted = FALSE;
	guint prev_mute_flag = alsa->mute_flag;

	if (bMuteOn)
		ENABLE_MASK(alsa->mute_flag, mask);
	else
		DISABLE_MASK(alsa->mute_flag, mask);

	if (bMuteOn == FALSE && prev_mute_flag != UNMUTE_STATE &&  alsa->mute_flag == UNMUTE_STATE)
	{
		// MUTE -> UNMUTE
		accepted = TRUE;
		GST_ERROR_OBJECT (alsa, "NO-ERROR! JUST.. AUDIO UNMUTE, mask[ %04x ], flag[ %04x -> %04x ]", mask, prev_mute_flag, alsa->mute_flag);
	}
	else if (bMuteOn == TRUE && prev_mute_flag == UNMUTE_STATE &&  alsa->mute_flag != UNMUTE_STATE)
	{
		// UNMUTE -> MUTE
		accepted = TRUE;
		GST_ERROR_OBJECT (alsa, "NO-ERROR! JUST.. AUDIO MUTE, mask[ %04x ], flag[ %04x -> %04x ]", mask, prev_mute_flag, alsa->mute_flag);
	}
	else
	{
		GST_INFO_OBJECT(alsa, "bMuteOn[ %d ], mask[ %04x ], flag[ %04x -> %04x ]", bMuteOn, mask, prev_mute_flag, alsa->mute_flag );
		return;
	}

  int result = -1;
  guint64 T1=0;
  guint64 T2=0;
 
  if(g_str_equal(alsa->device, "hw:0,0") == FALSE) // Only the main alsasink can control the source selection.
  {
    GST_INFO_OBJECT(alsa, "The sub alsasink is cannot control mute.[ %s ]", alsa->device);
    alsa->mute = bMuteOn;
    return;
  }
 
  if(alsa->focused_zone == -1)
  {
    result = sec_ctl_set_boolean (0,"main out mute", bMuteOn?1:0);
  }
  else if(alsa->focused_zone == 3 || alsa->focused_zone == -2) //:TODO: AVOC
  {
    T1 = get_time();
    result = avoc_mls_audio_mute(3/*zone 3*/, bMuteOn?AVOC_SETTING_ON:AVOC_SETTING_OFF);
    T2 = get_time();
  }
  else
    GST_ERROR_OBJECT(alsa, "no %s, because the focused zone is [ %d ]", bMuteOn?"MUTE":"UNMUTE", alsa->focused_zone);

  alsa->mute = bMuteOn;
  GST_ERROR_OBJECT(alsa, "focused zone[%d] %s, result=%d [ %lld ms ]", alsa->focused_zone, bMuteOn?"MUTE":"UNMUTE", result, (T2-T1)/1000);
}

static void
gst_alsasink_finalise (GObject * object)
{
  GstAlsaSink *sink = GST_ALSA_SINK (object);

  g_free (sink->device);
  g_mutex_free (sink->alsa_lock);

  g_static_mutex_lock (&output_mutex);
  --output_ref;
  if (output_ref == 0) {
    snd_output_close (output);
    output = NULL;
  }
  g_static_mutex_unlock (&output_mutex);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_alsasink_init_interfaces (GType type)
{
  gst_alsa_type_add_device_property_probe_interface (type);
}

static void
gst_alsasink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (element_class,
      "Audio sink (ALSA)", "Sink/Audio",
      "Output to a sound card via ALSA", "Wim Taymans <wim@fluendo.com>");

  gst_element_class_add_static_pad_template (element_class,
      &alsasink_sink_factory);
}

static void
gst_alsasink_class_init (GstAlsaSinkClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSinkClass *gstbasesink_class;
  GstAudioSinkClass *gstaudiosink_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstbasesink_class = (GstBaseSinkClass *) klass;
  gstaudiosink_class = (GstAudioSinkClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_alsasink_finalise;
  gobject_class->get_property = gst_alsasink_get_property;
  gobject_class->set_property = gst_alsasink_set_property;

  gstelement_class->query = gst_alsasink_query;

  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_alsasink_getcaps);

  gstaudiosink_class->open = GST_DEBUG_FUNCPTR (gst_alsasink_open);
  gstaudiosink_class->prepare = GST_DEBUG_FUNCPTR (gst_alsasink_prepare);
  gstaudiosink_class->unprepare = GST_DEBUG_FUNCPTR (gst_alsasink_unprepare);
  gstaudiosink_class->close = GST_DEBUG_FUNCPTR (gst_alsasink_close);
  gstaudiosink_class->write = GST_DEBUG_FUNCPTR (gst_alsasink_write);
  gstaudiosink_class->delay = GST_DEBUG_FUNCPTR (gst_alsasink_delay);
  gstaudiosink_class->reset = GST_DEBUG_FUNCPTR (gst_alsasink_reset);

  g_object_class_install_property (gobject_class, PROP_MUTE,
      g_param_spec_boolean ("mute", "Mute",
          "Mute state of alsa device",
          DEFAULT_MUTE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "ALSA device, as defined in an asound configuration file",
          DEFAULT_DEVICE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device name",
          "Human-readable name of the sound device", 
          DEFAULT_DEVICE_NAME, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CARD_NAME,
      g_param_spec_string ("card-name", "Card name",
          "Human-readable name of the sound card", 
          DEFAULT_CARD_NAME, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DEVICE_ID,
      g_param_spec_int ("device-id", "Device ID",
            "device id for the actual hw resource",
            DefaultDeviceId, INT_MAX, DefaultDeviceId,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_UPDATE_RENDER_DELAY,
      g_param_spec_boolean ("update-render-delay", "update render delay",
          "Update render-delay by start_threshold",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_GAME_RENDER,
      g_param_spec_boolean ("game-mode", "Game Mode",
          "reduce the delay at rendering for game mode",
          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));		  

  g_object_class_install_property (gobject_class, PROP_HD_AUDIO_MODE,
	  g_param_spec_boolean ("hd-audio-mode", "HD-audio-mode",
		  "HD audio mode flag for set hw",
		  DEFAULT_HD_AUDIO_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MLS_FOCUSED_ZONE_ID,
      g_param_spec_int ("mls-focused-zone-id", "MLS Focused Zone ID",
            "MLS Focused Zone ID, The element onwer must set this value whenever the focused zone is changed.",
            -G_MAXINT, G_MAXINT, -1,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));	

  g_object_class_install_property (gobject_class, PROP_DELAYED_UNMUTE,
      g_param_spec_uint("delayed-unmute", "set delay for unmute",
            "set unmute time (msecond), max:1sec",
            0, 1000, 0,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));	
}

static void
gst_alsasink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAlsaSink *sink;

  sink = GST_ALSA_SINK (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_free (sink->device);
      sink->device = g_value_dup_string (value);
      /* setting NULL restores the default device */
      if (sink->device == NULL) {
        sink->device = g_strdup (DEFAULT_DEVICE);
      }
      break;
    case PROP_MUTE:
      {
        gboolean mute = g_value_get_boolean(value);
        GST_ERROR_OBJECT(sink, "mute[%d]", mute);
        gst_alsasink_set_mainout_mute(sink, mute, MUTE_EXTERNAL);
      }
      break;
    case PROP_DEVICE_ID:
      g_value_set_int(value, sink->device_id);
      break;
    case PROP_UPDATE_RENDER_DELAY:
      sink->need_update_render_delay = g_value_get_boolean(value);
      GST_INFO_OBJECT(sink, "update_render_delay[ %s ]", sink->need_update_render_delay?"ON":"OFF");
      break;  
    case PROP_GAME_RENDER:
      GST_ERROR_OBJECT(sink, "SETTING GAME RENDER to TRUE");
      sink->game_render = true;
      break;
    case PROP_HD_AUDIO_MODE:
	  sink->hd_audio_mode = g_value_get_boolean (value);
	  GST_ERROR_OBJECT(sink, "PROP_HD_AUDIO_MODE : %s", (sink->hd_audio_mode)?"on":"off");
	  break; 
    case PROP_MLS_FOCUSED_ZONE_ID:
    {
      gint org_focused_zone = sink->focused_zone;
      sink->focused_zone = g_value_get_int(value);
      GST_ERROR_OBJECT(sink, "New MLS focused_zone ID, [ old:%d -> new:%d ]", org_focused_zone, sink->focused_zone);
      break;
    }
    case PROP_DELAYED_UNMUTE:
    {
      guint org_delayed_unmute = sink->delayed_unmute;
      sink->delayed_unmute = g_value_get_uint(value);
      GST_ERROR_OBJECT(sink, "New delayed_unmute, [ old:%d -> new:%d ]", org_delayed_unmute, sink->delayed_unmute);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_alsasink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAlsaSink *sink;

  sink = GST_ALSA_SINK (object);

  switch (prop_id) {
    case PROP_MUTE:
      g_value_set_boolean (value, sink->mute);
      break;
    case PROP_DEVICE:
      g_value_set_string (value, sink->device);
      break;
    case PROP_DEVICE_NAME:
      g_value_take_string (value,
          gst_alsa_find_device_name (GST_OBJECT_CAST (sink),
              sink->device, sink->handle, SND_PCM_STREAM_PLAYBACK));
      break;
    case PROP_CARD_NAME:
      g_value_take_string (value,
          gst_alsa_find_card_name (GST_OBJECT_CAST (sink),
              sink->device, SND_PCM_STREAM_PLAYBACK));
      break;
    case PROP_DEVICE_ID:
      sink->device_id = g_value_get_int(value);
      GST_LOG_OBJECT(sink, "device-id[%d]", sink->device_id);
      break;
    case PROP_UPDATE_RENDER_DELAY:
      g_value_set_boolean (value, sink->need_update_render_delay);
      break;
    case PROP_HD_AUDIO_MODE:
	  g_value_set_boolean (value, sink->hd_audio_mode);
      GST_LOG_OBJECT(sink, "hd_audio_mode[%d]", sink->hd_audio_mode);
	  break;	  
    case PROP_MLS_FOCUSED_ZONE_ID:
      g_value_set_int(value, sink->focused_zone);
      GST_LOG_OBJECT(sink, "focused_zone[%d]", sink->focused_zone);
      break;
    case PROP_DELAYED_UNMUTE:
      g_value_set_uint(value, sink->delayed_unmute);
      GST_LOG_OBJECT(sink, "delayed_unmute[%d]", sink->delayed_unmute);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

GST_DEBUG_CATEGORY_EXTERN(GST_CAT_AREN);

static void
gst_alsasink_init (GstAlsaSink * alsasink, GstAlsaSinkClass * g_class)
{
  GstBaseSink *basesink = NULL;
  GST_DEBUG_OBJECT (alsasink, "initializing alsasink");

  alsasink->device = g_strdup (DEFAULT_DEVICE);
  alsasink->handle = NULL;
  alsasink->cached_caps = NULL;
  alsasink->alsa_lock = g_mutex_new ();
  alsasink->device_id = DefaultDeviceId;
  alsasink->start_threshold = 0;
  alsasink->sum_of_written_frames = 0;
  alsasink->need_update_render_delay = FALSE;
  alsasink->delayset = false;
  alsasink->prev_delay = DEFAULT_MAINOUT_DELAY;
  alsasink->amixer_pp_onoff = TRUE;
  alsasink->hd_audio_mode = FALSE;
  alsasink->hd_audio_avoc_set = FALSE;
  alsasink->game_render = false;
  alsasink->focused_zone = -1;
  alsasink->reset_called = FALSE;
  alsasink->render_start_time = 0;
  alsasink->delayed_unmute = 0;
  alsasink->mute_flag = MUTE_DEFAULT;
  basesink = GST_BASE_SINK(alsasink);
  basesink->debugCategory = GST_CAT_AREN;
  g_static_mutex_lock (&output_mutex);
  if (output_ref == 0) {
    snd_output_stdio_attach (&output, stdout, 0);
    ++output_ref;
  }
  g_static_mutex_unlock (&output_mutex);
}

#define CHECK(call, error) \
G_STMT_START {                  \
if ((err = call) < 0)           \
  goto error;                   \
} G_STMT_END;

static GstCaps *
gst_alsasink_getcaps (GstBaseSink * bsink)
{
  GstElementClass *element_class;
  GstPadTemplate *pad_template;
  GstAlsaSink *sink = GST_ALSA_SINK (bsink);
  GstCaps *caps;

  if (sink->handle == NULL) {
    GST_DEBUG_OBJECT (sink, "device not open, using template caps");
    return NULL;                /* base class will get template caps for us */
  }

  if (sink->cached_caps) {
    GST_LOG_OBJECT (sink, "Returning cached caps");
    return gst_caps_ref (sink->cached_caps);
  }

  element_class = GST_ELEMENT_GET_CLASS (sink);
  pad_template = gst_element_class_get_pad_template (element_class, "sink");
  g_return_val_if_fail (pad_template != NULL, NULL);

  caps = gst_alsa_probe_supported_formats (GST_OBJECT (sink), sink->handle,
      gst_pad_template_get_caps (pad_template));

  if (caps) {
    sink->cached_caps = gst_caps_ref (caps);
  }

  GST_INFO_OBJECT (sink, "returning caps %" GST_PTR_FORMAT, caps);

  return caps;
}

static int
set_hwparams (GstAlsaSink * alsa)
{
  guint rrate;
  gint err;
  snd_pcm_hw_params_t *params;
  guint period_time, buffer_time;

  snd_pcm_hw_params_malloc (&params);

  GST_DEBUG_OBJECT (alsa, "Negotiating to %d channels @ %d Hz (format = %s) "
      "SPDIF (%d)", alsa->channels, alsa->rate,
      snd_pcm_format_name (alsa->format), alsa->iec958);

  /* start with requested values, if we cannot configure alsa for those values,
   * we set these values to -1, which will leave the default alsa values */
  buffer_time = alsa->buffer_time;
  period_time = alsa->period_time;

retry:
  /* choose all parameters */
  CHECK (snd_pcm_hw_params_any (alsa->handle, params), no_config);
  /* set the interleaved read/write format */
  CHECK (snd_pcm_hw_params_set_access (alsa->handle, params, alsa->access),
      wrong_access);
  /* set the sample format */
  if (alsa->iec958) {
    /* Try to use big endian first else fallback to le and swap bytes */
    if (snd_pcm_hw_params_set_format (alsa->handle, params, alsa->format) < 0) {
      alsa->format = SND_PCM_FORMAT_S16_LE;
      alsa->need_swap = TRUE;
      GST_DEBUG_OBJECT (alsa, "falling back to little endian with swapping");
    } else {
      alsa->need_swap = FALSE;
    }
  }
  CHECK (snd_pcm_hw_params_set_format (alsa->handle, params, alsa->format),
      no_sample_format);
  /* set the count of channels */
  CHECK (snd_pcm_hw_params_set_channels (alsa->handle, params, alsa->channels),
      no_channels);
  /* set the stream rate */
  rrate = alsa->rate;
  CHECK (snd_pcm_hw_params_set_rate_near (alsa->handle, params, &rrate, NULL),
      no_rate);

#ifndef GST_DISABLE_GST_DEBUG
  /* get and dump some limits */
  {
    guint min, max;

    snd_pcm_hw_params_get_buffer_time_min (params, &min, NULL);
    snd_pcm_hw_params_get_buffer_time_max (params, &max, NULL);

    GST_DEBUG_OBJECT (alsa, "buffer time %u, min %u, max %u",
        alsa->buffer_time, min, max);

    snd_pcm_hw_params_get_period_time_min (params, &min, NULL);
    snd_pcm_hw_params_get_period_time_max (params, &max, NULL);

    GST_DEBUG_OBJECT (alsa, "period time %u, min %u, max %u",
        alsa->period_time, min, max);

    snd_pcm_hw_params_get_periods_min (params, &min, NULL);
    snd_pcm_hw_params_get_periods_max (params, &max, NULL);

    GST_DEBUG_OBJECT (alsa, "periods min %u, max %u", min, max);
  }
#endif

  /* now try to configure the buffer time and period time, if one
   * of those fail, we fall back to the defaults and emit a warning. */
  if (buffer_time != -1 && !alsa->iec958) {
    /* set the buffer time */
    if ((err = snd_pcm_hw_params_set_buffer_time_near (alsa->handle, params,
                &buffer_time, NULL)) < 0) {
      GST_ELEMENT_WARNING (alsa, RESOURCE, SETTINGS, (NULL),
          ("Unable to set buffer time %i for playback: %s",
              buffer_time, snd_strerror (err)));
      /* disable buffer_time the next round */
      buffer_time = -1;
      goto retry;
    }
    GST_DEBUG_OBJECT (alsa, "buffer time %u", buffer_time);
  }
  if (period_time != -1 && !alsa->iec958) {
    /* set the period time */
    if ((err = snd_pcm_hw_params_set_period_time_near (alsa->handle, params,
                &period_time, NULL)) < 0) {
      GST_ELEMENT_WARNING (alsa, RESOURCE, SETTINGS, (NULL),
          ("Unable to set period time %i for playback: %s",
              period_time, snd_strerror (err)));
      /* disable period_time the next round */
      period_time = -1;
      goto retry;
    }
    GST_DEBUG_OBJECT (alsa, "period time %u", period_time);
  }

  /* Set buffer size and period size manually for SPDIF */
  if (G_UNLIKELY (alsa->iec958)) {
    snd_pcm_uframes_t buffer_size = SPDIF_BUFFER_SIZE;
    snd_pcm_uframes_t period_size = SPDIF_PERIOD_SIZE;

    CHECK (snd_pcm_hw_params_set_buffer_size_near (alsa->handle, params,
            &buffer_size), buffer_size);
    CHECK (snd_pcm_hw_params_set_period_size_near (alsa->handle, params,
            &period_size, NULL), period_size);
  }

  /* write the parameters to device */
  CHECK (snd_pcm_hw_params (alsa->handle, params), set_hw_params);

  /* now get the configured values */
  CHECK (snd_pcm_hw_params_get_buffer_size (params, &alsa->buffer_size),
      buffer_size);
  CHECK (snd_pcm_hw_params_get_period_size (params, &alsa->period_size, NULL),
      period_size);

  GST_DEBUG_OBJECT (alsa, "buffer size %lu, period size %lu", alsa->buffer_size,
      alsa->period_size);

  snd_pcm_hw_params_free (params);
  return 0;

  /* ERRORS */
no_config:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Broken configuration for playback: no configurations available: %s",
            snd_strerror (err)));
    snd_pcm_hw_params_free (params);
    return err;
  }
wrong_access:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Access type not available for playback: %s", snd_strerror (err)));
    snd_pcm_hw_params_free (params);
    return err;
  }
no_sample_format:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Sample format not available for playback: %s", snd_strerror (err)));
    snd_pcm_hw_params_free (params);
    return err;
  }
no_channels:
  {
    gchar *msg = NULL;

    if ((alsa->channels) == 1)
      msg = g_strdup (_("Could not open device for playback in mono mode."));
    if ((alsa->channels) == 2)
      msg = g_strdup (_("Could not open device for playback in stereo mode."));
    if ((alsa->channels) > 2)
      msg =
          g_strdup_printf (_
          ("Could not open device for playback in %d-channel mode."),
          alsa->channels);
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, ("%s", msg),
        ("%s", snd_strerror (err)));
    g_free (msg);
    snd_pcm_hw_params_free (params);
    return err;
  }
no_rate:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Rate %iHz not available for playback: %s",
            alsa->rate, snd_strerror (err)));
    return err;
  }
buffer_size:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to get buffer size for playback: %s", snd_strerror (err)));
    snd_pcm_hw_params_free (params);
    return err;
  }
period_size:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to get period size for playback: %s", snd_strerror (err)));
    snd_pcm_hw_params_free (params);
    return err;
  }
set_hw_params:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to set hw params for playback: %s", snd_strerror (err)));
    snd_pcm_hw_params_free (params);
    return err;
  }
}

static int
set_swparams (GstAlsaSink * alsa)
{
  int err;
  snd_pcm_sw_params_t *params;

  snd_pcm_sw_params_malloc (&params);

  /* get the current swparams */
  CHECK (snd_pcm_sw_params_current (alsa->handle, params), no_config);
  /* start the transfer when the buffer is almost full: */
  /* (buffer_size / avail_min) * avail_min */
  if (alsa->game_render)
  {
    //reduce the start threshold..
    CHECK (snd_pcm_sw_params_set_start_threshold (alsa->handle, params, GAME_START_THRESHOLD),
      start_threshold);
     alsa->start_threshold = GAME_START_THRESHOLD; 
  }
  else
  {
    CHECK (snd_pcm_sw_params_set_start_threshold (alsa->handle, params,
          (alsa->buffer_size / alsa->period_size) * alsa->period_size),
      start_threshold);

    alsa->start_threshold = (alsa->buffer_size / alsa->period_size) * alsa->period_size;
	// start_threshold = 8192 frames (1frame =  2byte * 2sample(L/R) )
	// buffer_size = 8192 frames
	// period_size = 512 frames
  }

  /* allow the transfer when at least period_size samples can be processed */
  CHECK (snd_pcm_sw_params_set_avail_min (alsa->handle, params,
          alsa->period_size), set_avail);

#if GST_CHECK_ALSA_VERSION(1,0,16)
  /* snd_pcm_sw_params_set_xfer_align() is deprecated, alignment is always 1 */
#else
  /* align all transfers to 1 sample */
  CHECK (snd_pcm_sw_params_set_xfer_align (alsa->handle, params, 1), set_align);
#endif

  /* write the parameters to the playback device */
  CHECK (snd_pcm_sw_params (alsa->handle, params), set_sw_params);

  snd_pcm_sw_params_free (params);
  return 0;

  /* ERRORS */
no_config:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to determine current swparams for playback: %s",
            snd_strerror (err)));
    snd_pcm_sw_params_free (params);
    return err;
  }
start_threshold:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to set start threshold mode for playback: %s",
            snd_strerror (err)));
    snd_pcm_sw_params_free (params);
    return err;
  }
set_avail:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to set avail min for playback: %s", snd_strerror (err)));
    snd_pcm_sw_params_free (params);
    return err;
  }
#if !GST_CHECK_ALSA_VERSION(1,0,16)
set_align:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to set transfer align for playback: %s", snd_strerror (err)));
    snd_pcm_sw_params_free (params);
    return err;
  }
#endif
set_sw_params:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Unable to set sw params for playback: %s", snd_strerror (err)));
    snd_pcm_sw_params_free (params);
    return err;
  }
}

static gboolean
alsasink_parse_spec (GstAlsaSink * alsa, GstRingBufferSpec * spec)
{
  /* Initialize our boolean */
  alsa->iec958 = FALSE;

  switch (spec->type) {
    case GST_BUFTYPE_LINEAR:
      GST_DEBUG_OBJECT (alsa,
          "Linear format : depth=%d, width=%d, sign=%d, bigend=%d", spec->depth,
          spec->width, spec->sign, spec->bigend);

      alsa->format = snd_pcm_build_linear_format (spec->depth, spec->width,
          spec->sign ? 0 : 1, spec->bigend ? 1 : 0);
      break;
    case GST_BUFTYPE_FLOAT:
      switch (spec->format) {
        case GST_FLOAT32_LE:
          alsa->format = SND_PCM_FORMAT_FLOAT_LE;
          break;
        case GST_FLOAT32_BE:
          alsa->format = SND_PCM_FORMAT_FLOAT_BE;
          break;
        case GST_FLOAT64_LE:
          alsa->format = SND_PCM_FORMAT_FLOAT64_LE;
          break;
        case GST_FLOAT64_BE:
          alsa->format = SND_PCM_FORMAT_FLOAT64_BE;
          break;
        default:
          goto error;
      }
      break;
    case GST_BUFTYPE_A_LAW:
      alsa->format = SND_PCM_FORMAT_A_LAW;
      break;
    case GST_BUFTYPE_MU_LAW:
      alsa->format = SND_PCM_FORMAT_MU_LAW;
      break;
    case GST_BUFTYPE_IEC958:
      alsa->format = SND_PCM_FORMAT_S16_BE;
      alsa->iec958 = TRUE;
      break;
    default:
      goto error;

  }
  alsa->rate = spec->rate;
  alsa->channels = spec->channels;
  alsa->buffer_time = spec->buffer_time;
  alsa->period_time = spec->latency_time;
  alsa->access = SND_PCM_ACCESS_RW_INTERLEAVED;

  return TRUE;

  /* ERRORS */
error:
  {
    return FALSE;
  }
}

static gboolean
gst_alsasink_open (GstAudioSink * asink)
{
  GstAlsaSink *alsa;
  gint err;

  alsa = GST_ALSA_SINK (asink);

  alsa->mute = DEFAULT_MUTE;
  alsa->first_frame_processed = FALSE;

  if (alsa->device_id == DefaultDeviceId) {
    GST_WARNING_OBJECT(asink, "The actual device id has not been assigned");
  } else {
    GST_LOG_OBJECT(asink, "device-id[%d]", alsa->device_id);
  }

  gst_alsasink_set_mainout_mmsrc(asink);

  /* open in non-blocking mode, we'll use snd_pcm_wait() for space to become
   * available. */
  CHECK (snd_pcm_open (&alsa->handle, alsa->device, SND_PCM_STREAM_PLAYBACK,
          SND_PCM_NONBLOCK), open_error);
  GST_LOG_OBJECT (alsa, "Opened device %s", alsa->device);

  return TRUE;

  /* ERRORS */
open_error:
  {
    if (err == -EBUSY) {
      GST_ELEMENT_ERROR (alsa, RESOURCE, BUSY,
          (_("Could not open audio device for playback. "
                  "Device is being used by another application.")),
          ("Device '%s' is busy", alsa->device));
    } else {
      GST_ELEMENT_ERROR (alsa, RESOURCE, OPEN_WRITE,
          (_("Could not open audio device for playback.")),
          ("Playback open error on device '%s': %s", alsa->device,
              snd_strerror (err)));
    }
    return FALSE;
  }
}

static gboolean
gst_alsasink_prepare (GstAudioSink * asink, GstRingBufferSpec * spec)
{
  GstAlsaSink *alsa = GST_ALSA_SINK (asink);
  gint err;

  if (spec->format == GST_IEC958) {
    snd_pcm_close (alsa->handle);
    alsa->handle = gst_alsa_open_iec958_pcm (GST_OBJECT (alsa));
    if (G_UNLIKELY (!alsa->handle)) {
      goto no_iec958;
    }
  }

  if (g_str_equal(alsa->device, "hw:0,0") == TRUE) 
  {
  	if (alsa->hd_audio_mode == TRUE)
  	{
	  gst_alsasink_avoc_set_hd_audio (asink, 1, 1); //amp-mute, for setting hd
    }

	GstStructure* adec_sinkcaps = gst_alsasink_send_query_decorder (asink);
    gst_alsasink_avoc_set_audio_info (asink, adec_sinkcaps, spec);

    if (gst_alsasink_send_query_audiosplitter (asink) == FALSE)
	{
	  gst_alsasink_set_amixer_pp_onoff (asink, 0); //amixer - pp data off
    }
  }
  
  if (!alsasink_parse_spec (alsa, spec))
    goto spec_parse;

  CHECK (set_hwparams (alsa), hw_params_failed);
  CHECK (set_swparams (alsa), sw_params_failed);

  alsa->bytes_per_sample = spec->bytes_per_sample;
  spec->segsize = alsa->period_size * spec->bytes_per_sample;
  spec->segtotal = alsa->buffer_size / alsa->period_size;

  {
    snd_output_t *out_buf = NULL;
    char *msg = NULL;

    snd_output_buffer_open (&out_buf);
    snd_pcm_dump_hw_setup (alsa->handle, out_buf);
    snd_output_buffer_string (out_buf, &msg);
    GST_DEBUG_OBJECT (alsa, "Hardware setup: \n%s", msg);
    snd_output_close (out_buf);
    snd_output_buffer_open (&out_buf);
    snd_pcm_dump_sw_setup (alsa->handle, out_buf);
    snd_output_buffer_string (out_buf, &msg);
    GST_DEBUG_OBJECT (alsa, "Software setup: \n%s", msg);
    snd_output_close (out_buf);
  }
#ifdef DUMP_PCM
  if (alsa->debug_fd == NULL)
  {
  	gchar str[1024];
  	snprintf(str, 1023, "/tmp/_%s_.pcm", GST_ELEMENT_NAME(asink));
  	alsa->debug_fd = (void*)fopen(str, "w+");
	if (alsa->debug_fd == NULL)
		GST_ERROR_OBJECT(alsa, "can't open [ %s ]", str);
  }
#endif
  if (alsa->game_render && alsa->delayset == false)
  {
   	long speakerDelay = 0;
	int result;
  	result = sec_ctl_get_integer (0,"main out delay", &speakerDelay);
	if (speakerDelay > (long)1)
	{
		alsa->prev_delay  = speakerDelay;
		speakerDelay = (long)1;
		GST_ERROR_OBJECT(alsa, "main out delay exists is %d and setting it to %ld", alsa->prev_delay, speakerDelay);
		result = sec_ctl_set_integer (0,"main out delay", speakerDelay);
		alsa->delayset = true;
	}
  }
  
  if (alsa->hd_audio_avoc_set == TRUE)
  {
  	gst_alsasink_avoc_set_hd_audio (asink, 0, 1); //amp-unmute, hd
  }

  return TRUE;

  /* ERRORS */
no_iec958:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, OPEN_WRITE, (NULL),
        ("Could not open IEC958 (SPDIF) device for playback"));
    return FALSE;
  }
spec_parse:
  {
    if (alsa->hd_audio_avoc_set == TRUE)
    {
      gst_alsasink_avoc_set_hd_audio (asink, 0, 0); //amp-unmute, hd -off 
    }
	
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Error parsing spec"));
    return FALSE;
  }
hw_params_failed:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Setting of hwparams failed: %s", snd_strerror (err)));
    return FALSE;
  }
sw_params_failed:
  {
    GST_ELEMENT_ERROR (alsa, RESOURCE, SETTINGS, (NULL),
        ("Setting of swparams failed: %s", snd_strerror (err)));
    return FALSE;
  }
}

static gboolean
gst_alsasink_unprepare (GstAudioSink * asink)
{
  GstAlsaSink *alsa;

  alsa = GST_ALSA_SINK (asink);

  snd_pcm_drop (alsa->handle);
  snd_pcm_hw_free (alsa->handle);

#ifdef DUMP_PCM
  if (alsa->debug_fd != NULL)
  {
  	fclose((FILE*)alsa->debug_fd);
	alsa->debug_fd = NULL;
  }
#endif

  return TRUE;
}

static gboolean
gst_alsasink_close (GstAudioSink * asink)
{
  GstAlsaSink *alsa = GST_ALSA_SINK (asink);

  gst_alsasink_set_mainout_mute(asink, TRUE, MUTE_DEFAULT);

  gst_alsasink_set_amixer_pp_onoff (asink, 1);

  if (alsa->hd_audio_avoc_set == TRUE)
  {
    /*mute amp for hd off*/
	gst_alsasink_avoc_set_hd_audio (asink, 1, 0); 
  }

  if (alsa->handle) {
    snd_pcm_close (alsa->handle);
    alsa->handle = NULL;
  }

  if (alsa->hd_audio_avoc_set == TRUE)
  {
	/*set amp hd_audio-off should be placed next to 'snd_pcm_close()' */
	gst_alsasink_avoc_set_hd_audio (asink, 0, 0); 
  }
  
  gst_caps_replace (&alsa->cached_caps, NULL);
  if (alsa->delayset)
  {
    GST_ERROR_OBJECT(alsa, "main out delay setting back to %d", alsa->prev_delay);
    sec_ctl_set_integer (0,"main out delay", alsa->prev_delay);
    alsa->delayset = false;
    alsa->prev_delay = DEFAULT_MAINOUT_DELAY;
  }

  return TRUE;
}

/*
 *   Underrun and suspend recovery
 */
static gint
xrun_recovery (GstAlsaSink * alsa, snd_pcm_t * handle, gint err)
{
  GST_DEBUG_OBJECT (alsa, "xrun recovery %d", err);

  if (err == -EPIPE) {          /* under-run */
    err = snd_pcm_prepare (handle);
    if (err < 0)
      GST_WARNING_OBJECT (alsa,
          "Can't recovery from underrun, prepare failed: %s",
          snd_strerror (err));
    return 0;
  } else if (err == -ESTRPIPE) {
    GST_INFO_OBJECT(alsa, "err = -86 ESTRPIPE, sleep 200ms,  no try to snd_pcm_resume");
    g_usleep (200000);
#if 0
    while ((err = snd_pcm_resume (handle)) == -EAGAIN)
    {
      if (alsa->reset_called)
        break;
      g_usleep (100);           /* wait until the suspend flag is released */
    }
#endif

    if (err < 0) {
      err = snd_pcm_prepare (handle);
      if (err < 0)
        GST_WARNING_OBJECT (alsa,
            "Can't recovery from suspend, prepare failed: %s",
            snd_strerror (err));
    }
    return 0;
  }
  return err;
}

static guint
gst_alsasink_write (GstAudioSink * asink, gpointer data, guint length)
{
  GstAlsaSink *alsa;
  gint err;
  gint cptr;
  gint16 *ptr = data;
  guint64 writing_time = 0;

  alsa = GST_ALSA_SINK (asink);

  if (alsa->iec958 && alsa->need_swap) {
    guint i;

    GST_DEBUG_OBJECT (asink, "swapping bytes");
    for (i = 0; i < length / 2; i++) {
      ptr[i] = GUINT16_SWAP_LE_BE (ptr[i]);
    }
  }

  GST_LOG_OBJECT (asink, "received audio samples buffer of %u bytes,  bytes_per_sample[ %d ]", length, alsa->bytes_per_sample);

  cptr = length / alsa->bytes_per_sample;
  
#ifdef DUMP_PCM
  if (alsa->debug_fd != NULL)
  {
  	int size = 0;
	while(size < length) {
		int ret = fwrite((const void*)data, 1, length, (FILE*)alsa->debug_fd);
		//GST_ERROR_OBJECT(asink, "[ %s ] write[ %d / %d / %d], ", GST_ELEMENT_NAME(asink), ret , size,  length);
		if (ret == 0) {
			GST_ERROR_OBJECT(asink, "[ %s ]can't write pcm,  size[ %d ], length[ %d ]", GST_ELEMENT_NAME(asink), size, length);
			break;
		}
		size += ret;
	}
  }
#endif

  GST_ALSA_SINK_LOCK (asink);
  while (cptr > 0) {
    /* start by doing a blocking wait for free space. Set the timeout
     * to 4 times the period time */
    err = snd_pcm_wait (alsa->handle, (4 * alsa->period_time / 1000));
    if (err < 0) {
      GST_DEBUG_OBJECT (asink, "wait error, %d", err);
    } else {
#ifdef AV_SYNC_DEBUG
      guint8 *seg_start = (guint8 *)ptr;
	  if (*seg_start == 0x47)
	  	{
	  	 GstClockTime seg_timestamp = 0;
		 memcpy((guint8*)(&seg_timestamp),seg_start+1, 8);
		 //GST_DEBUG_OBJECT (asink, "written segment timestamp is: %" GST_TIME_FORMAT, seg_timestamp);
		  GST_DEBUG_OBJECT (asink, "written segment timestamp is: %" GST_TIME_FORMAT
      ", end: %" GST_TIME_FORMAT, GST_TIME_ARGS (seg_timestamp), GST_TIME_ARGS (seg_timestamp));
	  	}
#endif
	if (alsa->game_render )
	{

#define MAX_ALSADELAY  30000
#define MAX_SKIPDELAY   42000

		if (alsa->render_start_time  > 0)
		{
			guint64 current_time = get_time() - alsa->render_start_time; // in micro seconds
			guint64 total_time = 	((alsa->sum_of_written_frames  *1000)/ (alsa->rate /1000));

			GST_DEBUG_OBJECT (asink, "[GAME AUDIO][current_time %llu][total_time %llu][ Wait Time %d]", current_time, total_time, (int)((total_time - current_time) -MAX_ALSADELAY));

			if ((total_time > current_time) )
			{
				if (total_time - current_time  > MAX_ALSADELAY)
				{
					//Drop Audio Frame ( because alsa is already accumulated audio data. Avoid accumulation at ring buffer

					if ( (total_time - current_time)  > MAX_SKIPDELAY) 
					{
						GST_ERROR_OBJECT(asink, "[GAME AUDIO]SKIPPING ONE PERIOD");
						goto write_error;
					}
					usleep((total_time - current_time) - MAX_ALSADELAY);
				}
			}
			else
			{
				//Adjust the total samples
				alsa->sum_of_written_frames = (current_time *(alsa->rate /1000) /1000) ;
			}
		}

	}
      if (alsa->render_start_time == 0)
        writing_time = get_time()*1000; // This must be measured before Writei, to check the accurate alsa starting time.
      err = snd_pcm_writei (alsa->handle, ptr, cptr);
    }

    GST_DEBUG_OBJECT (asink, "written %d frames out of %d", err, cptr);
    if (err < 0) {
      GST_DEBUG_OBJECT (asink, "Write error: %s", snd_strerror (err));
      if (alsa->reset_called || err == -EBADFD) {
        goto write_error;
      } else if (err == -EAGAIN) {
        continue;
      }else if (err == -ENOTCONN) {
      	err = cptr;
        if (alsa->render_start_time == 0)
          writing_time = get_time()*1000; // This must be measured before Writei, to check the accurate alsa starting time.
        if (alsa->render_start_time > 0)
        {
	       	guint64 acum_time= (alsa->sum_of_written_frames + cptr)*1000000000/alsa->rate;
	       	guint64 curr_time = get_time()  * 1000; //nano
	       	guint64 real_duration  = curr_time - alsa->render_start_time;
	       	GST_DEBUG_OBJECT (asink, "acum_time[%lld] curr_time [%lld] render_start_time[%lld] real_duration[%lld] written_frames[%lld] cptr[%d]",acum_time, curr_time,alsa->render_start_time,real_duration,alsa->sum_of_written_frames,cptr);
	       	if(real_duration < acum_time)
	       	{
	       		if(acum_time - real_duration > GST_SECOND)
	       		{
	       			GST_ERROR_OBJECT (asink, "duration_error acum_time[%lld] curr_time [%lld] render_start_time[%lld] real_duration[%lld] written_frames[%lld] cptr[%d]",acum_time, curr_time,alsa->render_start_time,real_duration,alsa->sum_of_written_frames,cptr);
	       		}
	       		else
	       			g_usleep((acum_time - real_duration)/1000);
	       	}
      	}
      } else {
        if (xrun_recovery (alsa, alsa->handle, err) < 0) {
          goto write_error;
        }
        g_usleep (5000);
        continue;
      }
    }

	if (alsa->game_render && (alsa->render_start_time == 0) )
	{
		alsa->render_start_time = (guint64)(writing_time/ 1000);
		alsa->render_start_time += 1;
	}
    /* Notify the device starting */
    alsa->sum_of_written_frames += err;
    if (alsa->render_start_time == 0) {
      if (alsa->sum_of_written_frames >= alsa->start_threshold)
      {
        alsa->render_start_time = writing_time+1; // Add 1 nsec to make sure this can't be zero.
 				if(alsa->need_update_render_delay && GST_AUDIO_SINK(asink)->started_cb)
        GST_AUDIO_SINK(asink)->started_cb(GST_AUDIO_SINK(asink), alsa->render_start_time);
      }
    }

    // Audio Unmute
    if ((alsa->need_update_render_delay == FALSE || alsa->render_start_time > 0) && alsa->first_frame_processed == FALSE)
    {
      if (alsa->delayed_unmute == 0) // Unmute immediately.
      {
        gst_alsasink_set_mainout_mute (asink, FALSE, MUTE_DEFAULT);
        alsa->first_frame_processed = TRUE;
      }
      else // Unmute after given time.
      {
        guint64 written_data_in_nsec = gst_util_uint64_scale_int(alsa->sum_of_written_frames , GST_SECOND, alsa->rate);
        if (written_data_in_nsec/GST_MSECOND >= alsa->delayed_unmute)
        {
          GST_ERROR_OBJECT(asink, "delayed_unmute[ %d ]", alsa->delayed_unmute);
          gst_alsasink_set_mainout_mute (asink, FALSE, MUTE_DEFAULT);
          alsa->first_frame_processed = TRUE;
        }
      }
    }
	
    ptr += snd_pcm_frames_to_bytes (alsa->handle, err);
    cptr -= err;
  }
  GST_ALSA_SINK_UNLOCK (asink);

  return length - (cptr * alsa->bytes_per_sample);

write_error:
  {
    GST_ALSA_SINK_UNLOCK (asink);
    return length;              /* skip one period */
  }
}

static guint
gst_alsasink_delay (GstAudioSink * asink)
{
  GstAlsaSink *alsa;
  snd_pcm_sframes_t delay;
  int res;

  alsa = GST_ALSA_SINK (asink);

  res = snd_pcm_delay (alsa->handle, &delay);
  if (G_UNLIKELY (res < 0)) {
    /* on errors, report 0 delay */
    GST_DEBUG_OBJECT (alsa, "snd_pcm_delay returned %d", res);
    delay = 0;
  }
  if (G_UNLIKELY (delay < 0)) {
    /* make sure we never return a negative delay */
    GST_WARNING_OBJECT (alsa, "snd_pcm_delay returned negative delay");
    delay = 0;
  }
#if 0
  if (G_LIKELY(res >= 0 && delay >= 0 && alsa->render_start_time>0))
  {
		gint64 elapsed_time = get_time()*1000 - alsa->render_start_time;
		gint64 amount_of_written_data = gst_util_uint64_scale_int(alsa->sum_of_written_frames, GST_SECOND, alsa->rate);
		gint64 amount_of_remain_data = gst_util_uint64_scale_int(delay, GST_SECOND, alsa->rate);
		if (G_LIKELY(amount_of_written_data >= elapsed_time))
		{
			if (ABS(amount_of_written_data-elapsed_time-amount_of_remain_data) > 30*GST_MSECOND)
			{
				GST_WARNING_OBJECT(alsa, "too big diff, delay[ %lld ms / %lld ms ], elapsed time[ %lld ms ],  amount of data[ %lld ms ]",
						amount_of_remain_data/GST_MSECOND, (amount_of_written_data-elapsed_time)/GST_MSECOND, elapsed_time/GST_MSECOND, amount_of_written_data/GST_MSECOND);
			}
		}
		else
		{
			if (amount_of_remain_data != 0)
				GST_WARNING_OBJECT(alsa, "Underflow case, but how it has delay[ %lld ms ], elapsed time[ %lld ms ],  amount of data[ %lld ms ]",
						amount_of_remain_data/GST_MSECOND, elapsed_time/GST_MSECOND, amount_of_written_data/GST_MSECOND);
		}
  }
#endif
  return delay;
}

static void
gst_alsasink_reset (GstAudioSink * asink)
{
  GstAlsaSink *alsa;
  gint err;

  alsa = GST_ALSA_SINK (asink);
  alsa->reset_called = TRUE;
  gst_alsasink_set_mainout_mute(asink, TRUE, MUTE_DEFAULT);
  GST_ALSA_SINK_LOCK (asink);
  GST_DEBUG_OBJECT (alsa, "lock, and drop");
  CHECK (snd_pcm_drop (alsa->handle), drop_error);
  GST_DEBUG_OBJECT (alsa, "prepare");
  CHECK (snd_pcm_prepare (alsa->handle), prepare_error);
  GST_DEBUG_OBJECT (alsa, "reset done");
  alsa->sum_of_written_frames = 0;
  alsa->render_start_time = 0;
  alsa->first_frame_processed = FALSE;
  alsa->reset_called = FALSE;
  GST_ALSA_SINK_UNLOCK (asink);
  GST_DEBUG_OBJECT (alsa, "unlock");
  return;

  /* ERRORS */
drop_error:
  {
    GST_ERROR_OBJECT (alsa, "alsa-reset: pcm drop error: %s",
        snd_strerror (err));
    GST_ALSA_SINK_UNLOCK (asink);
    return;
  }
prepare_error:
  {
    GST_ERROR_OBJECT (alsa, "alsa-reset: pcm prepare error: %s",
        snd_strerror (err));
    GST_ALSA_SINK_UNLOCK (asink);
    return;
  }
}

static gboolean
gst_alsasink_query (GstAudioSink * asink, GstQuery * query)
{
  gboolean ret = FALSE;
  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_RESOURCE:
      GST_INFO_OBJECT(asink, "RESOURCE QUERY - RESOURCE CATEGORY[ ASM_RESOURCE_AUDIO_MAIN_OUT ]");
      gst_query_add_resource(query, ASM_RESOURCE_AUDIO_MAIN_OUT);
      ret = TRUE;
      break;
    default:
      ret = GST_ELEMENT_CLASS (parent_class)->query (GST_ELEMENT(asink), query);
      break;
  }

  return ret;
}

static void
gst_alsasink_set_amixer_pp_onoff (GstAudioSink * asink, gboolean pp_onoff)
{
	GstAlsaSink *alsa = GST_ALSA_SINK (asink);
	gint result = -1;

	if (alsa->amixer_pp_onoff == pp_onoff)
	{
		GST_LOG_OBJECT (alsa, "set amixer postproccesed data : %d - NO need", pp_onoff);
		return;
	}

	GST_INFO_OBJECT(alsa, "set amixer postproccesed data - try : %d", pp_onoff);

	if (pp_onoff == FALSE)
	{ 
		result = sec_ctl_set_enumName (0,"mm pp onoff", "PP Disable" );
		GST_DEBUG_OBJECT(alsa, "mm pp onoff PP Disable, result = %d", result);	
	}
	else
	{
		result = sec_ctl_set_enumName (0,"mm pp onoff", "PP Enable" );
		GST_DEBUG_OBJECT(alsa, "mm pp onoff PP Enable, result = %d", result);
	}

	if (result != 0)
	{
		GST_ERROR_OBJECT(alsa, "call amixer - failed");
	}
	else
	{
		alsa->amixer_pp_onoff = pp_onoff;
		GST_DEBUG_OBJECT(alsa, "set amixer postproccesed data - done");
	}
}

static gboolean
gst_alsasink_send_query_audiosplitter (GstAudioSink * asink)
{
	GstAlsaSink *alsa = GST_ALSA_SINK (asink);	
	GstPad *sinkpad = gst_element_get_static_pad (alsa, "sink");

	GstStructure *structure = gst_structure_new ("check-audiosplitter-exist", NULL);

	GstQuery *query = gst_query_new_application (GST_QUERY_CUSTOM, structure);
	if (query == NULL)
	{
		GST_ERROR_OBJECT (alsa, "query-make : failed");
		goto exit;
	}
	
	GST_DEBUG_OBJECT (alsa, "query-send : %d", GST_QUERY_CUSTOM);

	gboolean ret = gst_pad_peer_query(sinkpad, query);

	GST_DEBUG_OBJECT (alsa, "query-back : %d", ret);

exit:
	if (query)
	{
		gst_query_unref (query);
	}
	return ret;
}

static GstStructure*
gst_alsasink_send_query_decorder(GstAudioSink * asink)
{
	GstAlsaSink *alsa = GST_ALSA_SINK (asink);	
	GstPad *sinkpad = gst_element_get_static_pad (alsa, "sink");
	GstStructure *send_structure = gst_structure_new ("alsa-ask-sinkcaps", 
									"pAudioCodec", G_TYPE_STRING, "", "samplerate", G_TYPE_INT, -1,
									"mpegversion", G_TYPE_INT, -1, "mpeglayer", G_TYPE_INT, -1, NULL);

	GstQuery *query = gst_query_new_application (GST_QUERY_CUSTOM, send_structure);
	if (query == NULL)
	{
		GST_ERROR_OBJECT (alsa, "query-make : failed");
		goto exit;
	}
	
	GST_DEBUG_OBJECT (alsa, "query-send : to ask decoder");	
	if (gst_pad_peer_query (sinkpad, query) == FALSE)
	{
		GST_ERROR_OBJECT (alsa, "query-back : no response");
		goto exit;
	}

	GstStructure *adec_sinkcaps = gst_query_get_structure (query);
	if (adec_sinkcaps == NULL)
	{
		GST_ERROR_OBJECT (alsa, "query-failed to get structure");
		goto exit;
	}
	
	return adec_sinkcaps;

exit:
	if (query)
	{
		gst_query_unref (query);
	}
	
	return NULL;
}

static void
gst_alsasink_avoc_set_hd_audio(GstAudioSink * asink, gboolean mute, gboolean hd_onoff)
{
	GstAlsaSink *alsa = GST_ALSA_SINK (asink);	

	gint avoc_ret = avoc_change_hd_audio_mode(mute, hd_onoff);
	if (avoc_ret != AVOC_EXIT_SUCCESS)
	{
		GST_ERROR_OBJECT(alsa, "failed to set h/w hd_audio_mode. avoc returned err : %d", avoc_ret);
		return;
	}
	GST_DEBUG_OBJECT (alsa, "set h/w for hd audio: mute(%s),hd(%s)", mute?"on":"off", hd_onoff?"on":"off");

	if ((mute == FALSE) && (hd_onoff == FALSE)) //when alsa close.
	{
		alsa->hd_audio_avoc_set = FALSE;
	}
	else
	{
		alsa->hd_audio_avoc_set = TRUE;
	}
	return;
}

static void
gst_alsasink_avoc_set_audio_info(GstAudioSink * asink, GstStructure * adec_sinkcaps, GstRingBufferSpec * spec)
{
	if (adec_sinkcaps == NULL && spec == NULL)
	{
		GST_ERROR_OBJECT(asink, "no data, adec_sinkcaps[ %p ] spec[ %p ]", adec_sinkcaps, spec);
		return;
	}

	GstAlsaSink *alsa = GST_ALSA_SINK (asink);	

    /* inform audio information to AVOC */
    avoc_audio_information_s info;
    memset(&info, 0, sizeof(avoc_audio_information_s));
    info.audio_dec = AVOC_AUDIO_DEC_0;
    info.source = AVOC_AUDIO_SOURCE_MM;

	gchar* pAudioCodec = NULL;
    info.encode_type = AVOC_AUDIO_ENCODE_TYPE_MPEG1_L3;
    int mpegversion = 0;
	int mpeglayer = 0;
	if (adec_sinkcaps)
	{
		gst_structure_get_int(adec_sinkcaps, "mpegversion", &mpegversion);
		gst_structure_get_int(adec_sinkcaps, "mpeglayer", &mpeglayer);
		if (pAudioCodec = gst_structure_get_string(adec_sinkcaps, "pAudioCodec"))
		{
			if (g_strrstr(pAudioCodec, "audio/mpeg") && (mpeglayer==1) && (mpegversion==1)) info.encode_type = AVOC_AUDIO_ENCODE_TYPE_MPEG1;
			else if (g_strrstr(pAudioCodec, "audio/x-ac3")) info.encode_type = AVOC_AUDIO_ENCODE_TYPE_AC3;
			else if (g_strrstr(pAudioCodec, "audio/x-aac") || (g_strrstr(pAudioCodec, "audio/mpeg") && (mpegversion==2 || mpegversion==4))) info.encode_type = AVOC_AUDIO_ENCODE_TYPE_AAC;
			else if (g_strrstr(pAudioCodec, "audio/x-dts")) info.encode_type = AVOC_AUDIO_ENCODE_TYPE_DTS;
			else if (g_strrstr(pAudioCodec, "audio/x-eac3")) info.encode_type = AVOC_AUDIO_ENCODE_TYPE_EAC3;
			else if (g_strrstr(pAudioCodec, "audio/x-adpcm")) info.encode_type = AVOC_AUDIO_ENCODE_TYPE_ADPCM;
			else if (g_strrstr(pAudioCodec, "audio/x-wma")) info.encode_type = AVOC_AUDIO_ENCODE_TYPE_WMA;
			else if (g_strrstr(pAudioCodec, "audio/x-vorbis")) info.encode_type = AVOC_AUDIO_ENCODE_TYPE_VORBIS;
			else if (g_strrstr(pAudioCodec, "audio/x-pn-realaudio")) info.encode_type = AVOC_AUDIO_ENCODE_TYPE_RA_G2COOK;
			else if (g_strrstr(pAudioCodec, "audio/x-alaw") || g_strrstr(pAudioCodec, "audio/x-mulaw")) info.encode_type = AVOC_AUDIO_ENCODE_TYPE_G711;
		}
		else
		{
			GST_ERROR_OBJECT(alsa, "failed to inform codec name to AVOC : %s, set default MP3", pAudioCodec);
			info.encode_type = AVOC_AUDIO_ENCODE_TYPE_MPEG1_L3;
		}
	}
	else
	{
		GST_LOG_OBJECT(asink, "Maybe no decoder, this is wav");
		info.encode_type = AVOC_AUDIO_ENCODE_TYPE_PCM;
	}
	
	int samplerate = 0;
	if (adec_sinkcaps)
		gst_structure_get_int(adec_sinkcaps, "samplerate", &samplerate);
	else
	{
		GST_LOG_OBJECT(asink, "wav, spec->rate[ %d ]", spec->rate);
		samplerate = spec->rate;
	}

	switch (samplerate) 
	{
		case 8000:  info.fs_rate = AVOC_AUDIO_FS_8KHZ; break;
		case 11025:  info.fs_rate = AVOC_AUDIO_FS_11KHZ; break;
		case 12000:  info.fs_rate = AVOC_AUDIO_FS_12KHZ; break;
		case 16000:  info.fs_rate = AVOC_AUDIO_FS_16KHZ; break;
		case 22050:  info.fs_rate = AVOC_AUDIO_FS_22KHZ; break;
		case 24000:  info.fs_rate = AVOC_AUDIO_FS_24KHZ; break;
		case 32000:  info.fs_rate = AVOC_AUDIO_FS_32KHZ; break;
		case 44100:  info.fs_rate = AVOC_AUDIO_FS_44KHZ; break;
		case 48000:  info.fs_rate = AVOC_AUDIO_FS_48KHZ; break;
		case 88200:  info.fs_rate = AVOC_AUDIO_FS_88KHZ; break;
		case 96000:  info.fs_rate = AVOC_AUDIO_FS_96KHZ; break;
		case 176400:  info.fs_rate = AVOC_AUDIO_FS_176KHZ; break;
		case 192000:  info.fs_rate = AVOC_AUDIO_FS_192KHZ; break;
		default: 
			GST_ERROR_OBJECT(alsa, "failed to select samplerate for AVOC : %d, set default 48KHz", samplerate);
			info.fs_rate = AVOC_AUDIO_FS_48KHZ;
			break;
	}

    GST_ERROR_OBJECT(alsa, "codec[ %s : %d ], fs_rate[ %d : %d ]", pAudioCodec?pAudioCodec:"", info.encode_type, samplerate, info.fs_rate);
	
    guint64 T1 = get_time();
    gint ret = avoc_set_audio_information(&info);
    guint64 T2 = get_time();
	
    if (T2-T1 > MIN_LATENCY)
		GST_ERROR_OBJECT(alsa, "[ %lld ms ] ret[ %d ]avoc_set_audio_information makes a latency for audio", (T2-T1)/1000, ret);

    if (ret < 0)
		GST_ERROR_OBJECT(alsa, "Fail to avoc_set_audio_information, ret[ %d ]", ret);

	return;
}


