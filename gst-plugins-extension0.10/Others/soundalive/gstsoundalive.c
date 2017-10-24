 /*
 * soundalive
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Hyunseok Lee <hs7388.lee@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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




#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstsoundalive.h"
#include <vconf/vconf.h>

GST_DEBUG_CATEGORY_STATIC (gst_soundalive_debug);
#define GST_CAT_DEFAULT gst_soundalive_debug

#define SOUNDALIVE_ENABLE_DUMP
#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA
#define SOUNDALIVE_ENABLE_RESAMPLER
#endif

#define VCONF_KEY_VOLUME_PREFIX				"file/private/sound/volume"
#define VCONF_KEY_VOLUME_TYPE_MEDIA			VCONF_KEY_VOLUME_PREFIX"/media"

#define SOUNDALIVE_SB_AM_INI_CSC_PATH			"/opt/system/csc-default/usr/tuning/mmfw_audio_sb_am.ini"
#define SOUNDALIVE_SB_AM_INI_DEFAULT_PATH		"/usr/etc/mmfw_audio_sb_am.ini"
#define SOUNDALIVE_SB_AM_INI_TEMP_PATH			"/opt/system/mmfw_audio_sb_am.ini"

#define SOUDNALIVE_DUMP_INI_DEFAULT_PATH		"/usr/etc/mmfw_audio_pcm_dump.ini"
#define SOUDNALIVE_DUMP_INI_TEMP_PATH			"/opt/system/mmfw_audio_pcm_dump.ini"
#define SOUNDALIVE_DUMP_INPUT_PATH_PREFIX		"/tmp/dump_gst_filter_"
#define SOUNDALIVE_DUMP_OUTPUT_PATH_PREFIX		"/tmp/dump_sec_filter_"

#define SOUNDALIVE_SA_BUFFER_MAX_SIZE				8192
#define SOUNDALIVE_SB_AM_BUFFER_MAX_SIZE			4096
#define SOUNDALIVE_SB_AM_BUFFER_HALF				SOUNDALIVE_SB_AM_BUFFER_MAX_SIZE / 2
#define SOUNDALIVE_SB_AM_BUFFER_QUATER			SOUNDALIVE_SB_AM_BUFFER_MAX_SIZE / 4
#define SOUNDALIVE_SRC_BUFFER_MAX_SIZE			4096
#define SOUNDALIVE_SRC_EXTRA_FRAMES				128
#define SOUNDALIVE_SRC_MULTIPLIER_OUTPUT		6

#define SOUNDALIVE_SA_DEFAULT_CHANNEL				2

#define SA_PULGIN_NAME "soundalive"

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
	PROP_FILTER_OUTPUT_MODE,
	PROP_PRESET_MODE,
	PROP_CUSTOM_EQ,
	PROP_CUSTOM_EXT,
	PROP_CUSTOM_EQ_BAND_NUM,
	PROP_CUSTOM_EQ_BAND_FQ,
	PROP_CUSTOM_EQ_BAND_WIDTH
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
	SAMPLERATE_11025Hz,
	SAMPLERATE_8000Hz,

	SAMPLERATE_NUM
};

enum OutputMode
{
	OUTPUT_SPK,
	OUTPUT_EAR,
	OUTPUT_OTHERS,
	OUTPUT_NUM
};

enum PresetMode
{
	PRESET_NORMAL,
	PRESET_POP,
	PRESET_ROCK,
	PRESET_DANCE,
	PRESET_JAZZ,
	PRESET_CLASSIC,
	PRESET_VOCAL,
	PRESET_BASS_BOOST,
	PRESET_TREBLE_BOOST,
	PRESET_MTHEATER,
	PRESET_EXTERNALIZATION,
	PRESET_CAFE,
	PRESET_CONCERT_HALL,
	PRESET_VOICE,
	PRESET_MOVIE,
	PRESET_VIRT51,
	PRESET_HIPHOP,
	PRESET_RNB,
	PRESET_FLAT,
	PRESET_TUBE,

	PRESET_NUM
};

#define DEFAULT_SAMPLE_SIZE				2
#define DEFAULT_VOLUME					15
#define DEFAULT_GAIN					1
#define DEFAULT_SAMPLE_RATE				44100
#define DEAFULT_CHANNELS				2
#define DEFAULT_FILTER_ACTION			FILTER_NONE
#define DEFAULT_FILTER_OUTPUT_MODE		OUTPUT_SPK
#define DEFAULT_PRESET_MODE				PRESET_NORMAL

#define SOUNDALIVE_KEY_MAX_LEN 256

static GStaticMutex _gst_soundalive_mutex = G_STATIC_MUTEX_INIT;
static gboolean _gst_soundalive_valid = FALSE;

#define SOUNDALIVE_INI_GET_BOOLEAN(dict, key, value, default) \
do { \
	value = iniparser_getboolean(dict, key, default); \
	GST_DEBUG("get %s = %s", key, (value) ? "y" : "n"); \
} while(0)

#define SOUNDALIVE_INI_GET_INT(dict, key, value, default) \
do { \
	value = iniparser_getint(dict, key, default); \
	GST_DEBUG("get %s = %d", key, value); \
} while(0)

#define SOUNDALIVE_INI_GET_INT_LIST(dict, key, values, max_count, default) \
do { \
	int idx; \
	const char delimiter[] = ", "; \
	char *list_str, *token, *ptr = NULL; \
	for (idx = 0; idx < max_count; idx++) \
		values[idx] = default; \
	list_str = iniparser_getstr(dict, key); \
	GST_DEBUG("get %s = %s", key, list_str); \
	idx = 0; \
	if (list_str) { \
		token = strtok_r(list_str, delimiter, &ptr); \
		while (token && (idx < max_count)) { \
			values[idx++] = atoi(token); \
			token = strtok_r(NULL, delimiter, &ptr); \
		} \
	} \
} while(0)

static const char *g_band_str[] = {
	"am_band_01",
	"am_band_02",
	"am_band_03"
};
static const char *g_freq_str[] = {
	"48kHz",
	"44_1kHz",
	"32kHz",
	"24kHz",
	"22_05kHz",
	"16kHz",
	"8kHz"
};

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
			"rate = (int) 44100, "
			"channels = (int) [1,6]"
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
#ifdef SOUNDALIVE_ENABLE_RESAMPLER
			"rate = (int) 44100, "
#endif
			"channels = (int) [1,6]"
			)
	);


GST_BOILERPLATE(Gstsoundalive, gst_soundalive, GstBaseTransform, GST_TYPE_BASE_TRANSFORM);


static void gst_soundalive_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_soundalive_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
#ifdef SOUNDALIVE_ENABLE_RESAMPLER
static void gst_soundalive_update_passthrough_mode (Gstsoundalive * soundalive);
static GstCaps *gst_soundalive_transform_caps (GstBaseTransform * base, GstPadDirection direction, GstCaps * caps);
static gboolean gst_soundalive_transform_size (GstBaseTransform * trans, GstPadDirection direction, GstCaps * incaps, guint insize, GstCaps * outcaps, guint * outsize);
static GstFlowReturn gst_soundalive_transform (GstBaseTransform * base, GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_soundalive_sink_event (GstBaseTransform * base, GstEvent * event);
#else
static GstFlowReturn gst_soundalive_transform_ip (GstBaseTransform * base, GstBuffer * buf);
#endif
static gboolean gst_soundalive_set_caps (GstBaseTransform * base, GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_soundalive_update_sa_filter_output_mode (Gstsoundalive * soundalive);


#ifdef USE_PA_AUDIO_FILTER
static void
_update_filter_status(Gstsoundalive * soundalive)
{
	GstElement *parent = GST_ELEMENT (soundalive);
	gboolean done = FALSE;

	/* get properties from http src plugin */
	while (parent = GST_ELEMENT_PARENT (parent)) {
		GstIterator *it;
		GstIteratorResult ires;
		gpointer item;
		guint latency = -1;

		it = gst_bin_iterate_sinks (GST_BIN (parent));
		do {
			ires = gst_iterator_next (it, &item);
			switch (ires) {
			case GST_ITERATOR_OK:
				g_object_get (G_OBJECT (item), "latency", &latency, NULL);
				/* Check high latency playback */
				if (latency != (guint)-1) {
					soundalive->sink = G_OBJECT (item);
					soundalive->use_pa_audio_filter = (latency == 2) ? TRUE : FALSE;
					done = TRUE;
				}
				break;
			case GST_ITERATOR_RESYNC:
				gst_iterator_resync (it);
				break;
			default:
				break;
			}
		} while (ires == GST_ITERATOR_OK || ires == GST_ITERATOR_RESYNC);
		gst_iterator_free (it);
		if (done)
			break;
	}

#ifdef SOUNDALIVE_ENABLE_RESAMPLER
		gst_soundalive_update_passthrough_mode(soundalive);
#endif

	GST_INFO_OBJECT(soundalive, "use_pa_audio_filter:%d", soundalive->use_pa_audio_filter);
}

static void
_update_pa_audio_filter(Gstsoundalive * soundalive)
{
	g_object_set(soundalive->sink, "filter-action", soundalive->filter_action, NULL);
	g_object_set(soundalive->sink, "filter-output-mode", soundalive->filter_output_mode, NULL);

	if (soundalive->filter_action == FILTER_PRESET) {
		/* update preset mode */
		g_object_set(soundalive->sink, "preset-mode", soundalive->preset_mode, NULL);
	} else if (soundalive->filter_action == FILTER_ADVANCED_SETTING) {
		/* update custom eq */
		g_object_set(soundalive->sink, "custom-eq", soundalive->custom_eq, NULL);
		/* update custom ext */
		g_object_set(soundalive->sink, "custom-ext", soundalive->custom_ext, NULL);
	}
}
#endif

static void
_volume_changed_callback(keynode_t *node, void* user_data)
{
	Gstsoundalive *soundalive = NULL;
	int ret =  0;

	g_static_mutex_lock(&_gst_soundalive_mutex);

	if(!_gst_soundalive_valid) {
		g_static_mutex_unlock(&_gst_soundalive_mutex);
		return;
	}

	if (user_data) {
		soundalive = (Gstsoundalive*)user_data;
		/* get current media type volume */
		ret = mm_sound_volume_get_value(VOLUME_TYPE_MEDIA, &soundalive->current_volume);
		if (ret) {
			GST_ERROR_OBJECT(soundalive, "failed to mm_sound_volume_get_value(), ret(0x%x)", ret);
		} else {
			GST_WARNING_OBJECT(soundalive, "current_volume(%d)", soundalive->current_volume);
		}
	} else {
		GST_ERROR("user_data is null..");
	}
#ifdef USE_PA_AUDIO_FILTER
	if (soundalive->use_pa_audio_filter == TRUE)
		_update_pa_audio_filter(soundalive);
#endif

	g_static_mutex_unlock(&_gst_soundalive_mutex);
}

void
_active_device_changed_callback(mm_sound_device_in device_in, mm_sound_device_out device_out, void *user_data)
{
	Gstsoundalive *soundalive = NULL;

	g_static_mutex_lock(&_gst_soundalive_mutex);

	if(!_gst_soundalive_valid) {
		g_static_mutex_unlock(&_gst_soundalive_mutex);
		return;
	}

	if (user_data) {
		soundalive = (Gstsoundalive*)user_data;

#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA
		if(soundalive->sa_handle == NULL) {
			/* judge sa_handle in case of abnormal unregister cb time sequence */
			GST_WARNING("invalid time sequence calling");
			return;
		}
#endif
		/* get current active device_out type */
		GST_WARNING_OBJECT(soundalive, "changed active device_out(0x%x)", device_out);
		switch (device_out) {
		case MM_SOUND_DEVICE_OUT_SPEAKER:
			soundalive->filter_output_mode = OUTPUT_SPK;
			break;
		case MM_SOUND_DEVICE_OUT_HDMI:
		case MM_SOUND_DEVICE_OUT_MIRRORING:
			soundalive->filter_output_mode = OUTPUT_OTHERS; /* bypass SB/SA */
			break;
		default:
			/* FIXME : what if in case of BT/DOCK ... */
			soundalive->filter_output_mode = OUTPUT_EAR;
			break;
		}

		/* update output mode */
		if (!gst_soundalive_update_sa_filter_output_mode(soundalive)) {
			GST_ERROR_OBJECT(soundalive, "gst_soundalive_update_sa_filter_output_mode failed");
		}
#ifdef SOUNDALIVE_ENABLE_RESAMPLER
		gst_soundalive_update_passthrough_mode(soundalive);
#endif
#ifdef USE_PA_AUDIO_FILTER
		if (soundalive->use_pa_audio_filter == TRUE)
			_update_pa_audio_filter(soundalive);
#endif
	} else {
		GST_ERROR("user_data is null..");
	}

	g_static_mutex_unlock(&_gst_soundalive_mutex);
}

static void
gst_soundalive_init_sb_am_config (Gstsoundalive * soundalive)
{
	dictionary * dict = NULL;
	char key[SOUNDALIVE_KEY_MAX_LEN] = {0, };
	int i, j;

	dict = iniparser_load(SOUNDALIVE_SB_AM_INI_DEFAULT_PATH);
	if (!dict) {
		GST_INFO("%s load failed. Use temporary file", SOUNDALIVE_SB_AM_INI_DEFAULT_PATH);
		dict = iniparser_load(SOUNDALIVE_SB_AM_INI_TEMP_PATH);
		if (!dict) {
			GST_WARNING("%s load failed", SOUNDALIVE_SB_AM_INI_TEMP_PATH);
			return;
		}
	}
	/* SB */
	SOUNDALIVE_INI_GET_BOOLEAN(dict, "sb_normal:enable", soundalive->sb_onoff, 0);
	SOUNDALIVE_INI_GET_INT(dict, "sb_normal:high_pass_filter", soundalive->lowcut_freq, 0);
	SOUNDALIVE_INI_GET_INT(dict, "sb_normal:gain_level", soundalive->gain_level, 0);
	SOUNDALIVE_INI_GET_INT(dict, "sb_normal:clarity_level", soundalive->clarity_level, 0);

	/* AM */
	SOUNDALIVE_INI_GET_BOOLEAN(dict, "am:enable", soundalive->am_onoff, 0);
	for (i = 0; i < AM_BAND_NUM; i++) {
		sprintf(key, "%s:enable", g_band_str[i]);
		SOUNDALIVE_INI_GET_BOOLEAN(dict, key, soundalive->am_band_onoff[i], 0);
		for (j = 0; j < AM_FREQ_NUM; j++) {
			sprintf(key, "%s:%s", g_band_str[i], g_freq_str[j]);
			SOUNDALIVE_INI_GET_INT_LIST(dict, key, soundalive->am_band_coef[i][j], AM_COEF_NUM, 0);
		}
	}

	iniparser_freedict(dict);
}

#ifdef SOUNDALIVE_ENABLE_DUMP
static void
gst_soundalive_init_dump_config (Gstsoundalive * soundalive)
{
	dictionary * dict = NULL;

	dict = iniparser_load(SOUDNALIVE_DUMP_INI_DEFAULT_PATH);
	if (!dict) {
		GST_INFO("%s load failed. Use temporary file", SOUDNALIVE_DUMP_INI_DEFAULT_PATH);
		dict = iniparser_load(SOUDNALIVE_DUMP_INI_TEMP_PATH);
		if (!dict) {
			GST_WARNING("%s load failed", SOUDNALIVE_DUMP_INI_TEMP_PATH);
			return;
		}
	}

	/* input */
	SOUNDALIVE_INI_GET_BOOLEAN(dict, "pcm_dump:gst_filter", soundalive->need_dump_input, 0);
	soundalive->dump_input_fp = NULL;

	/* output */
	SOUNDALIVE_INI_GET_BOOLEAN(dict, "pcm_dump:sec_filter", soundalive->need_dump_output, 0);
	soundalive->dump_output_fp = NULL;

	iniparser_freedict(dict);
}

static void
gst_soundalive_open_dump_file (Gstsoundalive * soundalive)
{
	char *suffix, *dump_path;
	GDateTime *time = g_date_time_new_now_local();

	suffix = g_date_time_format(time, "%Y%m%d_%H%M%S.pcm");
	/* input */
	if (soundalive->need_dump_input) {
		dump_path = g_strjoin(NULL, SOUNDALIVE_DUMP_INPUT_PATH_PREFIX, suffix, NULL);
		soundalive->dump_input_fp = fopen(dump_path, "w+");
		g_free(dump_path);
	}

	/* output */
	if (soundalive->need_dump_output) {
		dump_path = g_strjoin(NULL, SOUNDALIVE_DUMP_OUTPUT_PATH_PREFIX, suffix, NULL);
		soundalive->dump_output_fp = fopen(dump_path, "w+");
		g_free(dump_path);
	}
	g_free(suffix);

	g_date_time_unref(time);
}

static void
gst_soundalive_close_dump_file (Gstsoundalive * soundalive)
{
	/* input */
	if (soundalive->dump_input_fp) {
		fclose(soundalive->dump_input_fp);
		soundalive->dump_input_fp = NULL;
	}

	/* output */
	if (soundalive->dump_output_fp) {
		fclose(soundalive->dump_output_fp);
		soundalive->dump_output_fp = NULL;
	}
}
#endif

static void
gst_soundalive_clear_buffer (Gstsoundalive * soundalive)
{
#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA
	if (!soundalive->sa_handle) {
		GST_ERROR("sa_handle is null");
		return;
	}
	SoundAlive_Navigation_Button_Pressed(soundalive->sa_handle);
#endif
}

static GstStateChangeReturn
gst_soundalive_change_state (GstElement * element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	Gstsoundalive *soundalive = GST_SOUNDALIVE (element);
	int retval = 0;

	switch (transition) {
	case GST_STATE_CHANGE_NULL_TO_READY:
	{
		mm_sound_device_in device_in = MM_SOUND_DEVICE_IN_NONE;
		mm_sound_device_out device_out = MM_SOUND_DEVICE_OUT_NONE;

		GST_WARNING_OBJECT(soundalive, "GST_STATE_CHANGE_NULL_TO_READY");

		/* get current media type volume */
		retval = mm_sound_volume_get_value(VOLUME_TYPE_MEDIA, &soundalive->current_volume);
		if (retval) {
			GST_ERROR_OBJECT(soundalive, "failed to mm_sound_volume_get_value(), ret(%x)", retval);
		} else {
			GST_WARNING_OBJECT(soundalive, "current_volume(%d)", soundalive->current_volume);
		}

		/* get current active device_out type */
		retval = mm_sound_get_active_device(&device_in, &device_out);
		if (retval) {
			GST_ERROR_OBJECT(soundalive, "failed to mm_sound_get_active_device(), ret(%x)", retval);
		} else {
			GST_WARNING_OBJECT(soundalive, "active device(0x%x)", device_out);
			switch (device_out) {
			case MM_SOUND_DEVICE_OUT_SPEAKER:
				soundalive->filter_output_mode = OUTPUT_SPK;
				break;
			case MM_SOUND_DEVICE_OUT_HDMI:
			case MM_SOUND_DEVICE_OUT_MIRRORING:
				soundalive->filter_output_mode = OUTPUT_OTHERS; /* bypass SB/SA */
				break;
			default:
				/* FIXME : what if in case of BT/DOCK ... */
				soundalive->filter_output_mode = OUTPUT_EAR;
				break;
			}
		}
	}
		break;
	case GST_STATE_CHANGE_READY_TO_PAUSED:
		GST_WARNING_OBJECT(soundalive, "GST_STATE_CHANGE_READY_TO_PAUSED");
		break;
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		GST_WARNING_OBJECT(soundalive, "GST_STATE_CHANGE_PAUSED_TO_PLAYING");
#ifdef USE_PA_AUDIO_FILTER
		_update_filter_status(soundalive);
		if (soundalive->use_pa_audio_filter == TRUE)
			_update_pa_audio_filter(soundalive);
#endif
#ifdef SOUNDALIVE_ENABLE_DUMP
		gst_soundalive_open_dump_file(soundalive);
#endif
		break;
	default:
		break;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

	if (ret == GST_STATE_CHANGE_FAILURE)
		return ret;

	switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		GST_WARNING_OBJECT(soundalive, "GST_STATE_CHANGE_PLAYING_TO_PAUSED");
#ifdef USE_PA_AUDIO_FILTER
		_update_filter_status(soundalive);
#endif
#ifdef SOUNDALIVE_ENABLE_DUMP
		gst_soundalive_close_dump_file(soundalive);
#endif
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		GST_WARNING_OBJECT(soundalive, "GST_STATE_CHANGE_PAUSED_TO_READY");
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		GST_WARNING_OBJECT(soundalive, "GST_STATE_CHANGE_READY_TO_NULL");
		break;
	default:
		break;
	}

	return ret;
}

static void
gst_soundalive_fini (GObject *object)
{
	Gstsoundalive *soundalive;
	int ret = 0;
	soundalive = GST_SOUNDALIVE (object);

	/* reset before unregister callback to ensure time sequence */

	g_static_mutex_lock(&_gst_soundalive_mutex);

	_gst_soundalive_valid = FALSE;

#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA
	SoundAlive_Reset(soundalive->sa_handle);
	soundalive->sa_handle = NULL;
	GST_WARNING("SoundAlive_Reset() finished");
#endif

	if (soundalive->sb_struct) {
		free(soundalive->sb_struct);
		soundalive->sb_struct = NULL;
	}
#ifdef SOUNDALIVE_ENABLE_RESAMPLER
	if (soundalive->src_state) {
		SRC_Reset(soundalive->src_state);
		soundalive->src_state = NULL;
	}
#endif
	if (vconf_ignore_key_changed(VCONF_KEY_VOLUME_TYPE_MEDIA, _volume_changed_callback)) {
		GST_ERROR_OBJECT(soundalive, "failed to vconf_ignore_key_changed() for volume");
	}
	ret = mm_sound_remove_active_device_changed_callback(SA_PULGIN_NAME);
	if (ret) {
		GST_ERROR_OBJECT(soundalive, "failed to mm_sound_remove_active_device_changed_callback(), ret(0x%x)", ret);
	}
	GST_WARNING("SoundAlive_Reset() finished");

	G_OBJECT_CLASS (parent_class)->finalize (object);

	g_static_mutex_unlock(&_gst_soundalive_mutex);
}

static void
gst_soundalive_base_init (gpointer gclass)
{
	static GstElementDetails element_details = {
		"Sound Alive Audio Filter",
		"Filter/Effect/Audio",
		"Set effect on audio/raw streams",
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
gst_soundalive_class_init (GstsoundaliveClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	GstBaseTransformClass *basetransform_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gstelement_class = GST_ELEMENT_CLASS (klass);
	basetransform_class = GST_BASE_TRANSFORM_CLASS(klass);

	gobject_class->set_property = GST_DEBUG_FUNCPTR(gst_soundalive_set_property);
	gobject_class->get_property = GST_DEBUG_FUNCPTR(gst_soundalive_get_property);

	gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_soundalive_change_state);

	g_object_class_install_property(gobject_class, PROP_FILTER_ACTION,
		g_param_spec_uint("filter-action", "filter action", "(0)none (1)preset (2)advanced setting",
		0, 2, DEFAULT_FILTER_ACTION, G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_FILTER_OUTPUT_MODE,
		g_param_spec_uint("filter-output-mode", "filter output mode", "(0)Speaker (1)Ear (2)Others",
		0, 2, DEFAULT_FILTER_OUTPUT_MODE, G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_PRESET_MODE,
		g_param_spec_uint("preset-mode", "preset mode", "(0)normal (1)pop (2)rock (3)dance (4)jazz (5)classic (6)vocal (7)bass boost (8)treble boost (9)mtheater (10)externalization (11)cafe (12)concert hall (13)voice (14)movie (15)virt 5.1 (16)hip-hop (17)R&B (18)flat (19)tube",
		0, 19, DEFAULT_PRESET_MODE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_CUSTOM_EQ,
		g_param_spec_pointer("custom-eq", "custom eq level array",
		"pointer for array of EQ bands level", G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class, PROP_CUSTOM_EXT,
		g_param_spec_pointer("custom-ext", "custom ext level array",
		"pointer for array of custom effect level", G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_CUSTOM_EQ_BAND_NUM,
		g_param_spec_uint("custom-eq-num", "number of custom EQ bands", " ",
		0, CUSTOM_EQ_BAND_MAX, CUSTOM_EQ_BAND_MAX, G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_CUSTOM_EQ_BAND_FQ,
			g_param_spec_pointer("custom-eq-freq", "central frequency array of custom EQ bands",
			"pointer for EQ bands frequency array", G_PARAM_READABLE));

	g_object_class_install_property (gobject_class, PROP_CUSTOM_EQ_BAND_WIDTH,
			g_param_spec_pointer("custom-eq-width", "width array of custom EQ bands",
			"pointer for EQ bands width array", G_PARAM_READABLE));

	gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_soundalive_fini);
#ifdef SOUNDALIVE_ENABLE_RESAMPLER
	basetransform_class->transform_caps = GST_DEBUG_FUNCPTR(gst_soundalive_transform_caps);
	basetransform_class->transform_size = GST_DEBUG_FUNCPTR(gst_soundalive_transform_size);
	basetransform_class->transform = GST_DEBUG_FUNCPTR(gst_soundalive_transform);
	basetransform_class->event = GST_DEBUG_FUNCPTR(gst_soundalive_sink_event);
#else
	basetransform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_soundalive_transform_ip);
#endif
	basetransform_class->set_caps = GST_DEBUG_FUNCPTR(gst_soundalive_set_caps);
}

static void
gst_soundalive_init (Gstsoundalive * soundalive, GstsoundaliveClass * gclass)
{
	int i = 0;
	int ret = 0;

#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA
	soundalive->sa_handle = NULL;
#endif
	soundalive->samplerate = DEFAULT_SAMPLE_RATE;
	soundalive->channels = DEAFULT_CHANNELS;

	soundalive->filter_action = DEFAULT_FILTER_ACTION;
	soundalive->filter_output_mode = DEFAULT_FILTER_OUTPUT_MODE;
	soundalive->preset_mode = DEFAULT_PRESET_MODE;
	memset(soundalive->custom_eq, 0x00, sizeof(gint) * CUSTOM_EQ_BAND_MAX);
	memset(soundalive->custom_ext, 0x00, sizeof(gint) * CUSTOM_EXT_PARAM_MAX);
	soundalive->need_update_filter = TRUE;
	soundalive->current_volume = DEFAULT_VOLUME;
#ifdef SOUNDALIVE_ENABLE_DUMP
	gst_soundalive_init_dump_config(soundalive);
#endif
	gst_soundalive_init_sb_am_config(soundalive);

#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA
	soundalive->sa_handle = SoundAlive_Init();
	if (!soundalive->sa_handle) {
		GST_ERROR("could not get sa_handle");
		return;
	}
	GST_INFO("soundAlive version : %s", SoundAlive_Get_Version(soundalive->sa_handle));
	soundalive->custom_eq_band_num = SoundAlive_Get_EQ_BandNum(soundalive->sa_handle);
	GST_INFO("number of eq bands(%d)", soundalive->custom_eq_band_num);
	for (i = 0 ; i < soundalive->custom_eq_band_num ; i++) {
		soundalive->custom_eq_band_freq[i] = SoundAlive_Get_EQ_BandFc(soundalive->sa_handle, i);
		soundalive->custom_eq_band_width[i] = SoundAlive_Get_EQ_BandWidth(soundalive->sa_handle, i);
		GST_DEBUG("band index[%02d] : central frequency(%6dHz), width(%5dHz)",
			i, soundalive->custom_eq_band_freq[i],soundalive->custom_eq_band_width[i]);
	}
#endif

	soundalive->sb_struct = (SamsungSBMemStruct*)malloc(sizeof(SamsungSBMemStruct));
	if(!soundalive->sb_struct) {
		GST_ERROR("failed to malloc for SamsungSBMemStruct");
		return;
	}
	memset(soundalive->sb_struct, 0x00, sizeof(SamsungSBMemStruct));
	SamsungSBInit(soundalive->sb_struct);

	if (vconf_notify_key_changed(VCONF_KEY_VOLUME_TYPE_MEDIA, _volume_changed_callback, soundalive)) {
		GST_ERROR_OBJECT(soundalive, "failed to vconf_notify_key_changed() for volume");
	}
	ret = mm_sound_add_active_device_changed_callback(SA_PULGIN_NAME, _active_device_changed_callback, soundalive);
	if (ret) {
		GST_ERROR_OBJECT(soundalive, "failed to mm_sound_add_active_device_changed_callback(), ret(0x%x)", ret);
	}
#ifdef USE_PA_AUDIO_FILTER
	soundalive->use_pa_audio_filter = FALSE;
#endif

	g_static_mutex_lock(&_gst_soundalive_mutex);
	_gst_soundalive_valid = TRUE;
	g_static_mutex_unlock(&_gst_soundalive_mutex);
}

static gboolean
gst_soundalive_update_sa_samplingrate (Gstsoundalive * soundalive)
{
	int filter_ret = 0;

#ifdef USE_PA_AUDIO_FILTER
	if (soundalive->use_pa_audio_filter == TRUE)
		return TRUE;
#endif

	GST_DEBUG("samplerate:%d channels:%d", soundalive->samplerate, soundalive->channels);

#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA
	filter_ret = SoundAlive_Set_SamplingRate(soundalive->sa_handle, soundalive->samplerate);
	if (filter_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SA]Set_SamplingRate failed %d", filter_ret);
		return FALSE;
	}
#endif

	return TRUE;
}

static gboolean
gst_soundalive_update_sa_filter_output_mode (Gstsoundalive * soundalive)
{
	int filter_ret = 0;
	short sa_output[OUTPUT_NUM] = {SA_SPK, SA_EAR};
	char sa_output_str[OUTPUT_NUM][64] = {"SPEAKER", "EARPHONE"};

#ifdef USE_PA_AUDIO_FILTER
	if (soundalive->use_pa_audio_filter == TRUE)
		return TRUE;
#endif

	if (soundalive->filter_output_mode >= OUTPUT_NUM) {
		GST_ERROR_OBJECT(soundalive, "soundalive->filter_output_mode(%d) is invalid",
			soundalive->filter_output_mode);
		return FALSE;
	}
	if (soundalive->filter_output_mode == OUTPUT_OTHERS) {
		GST_WARNING_OBJECT(soundalive, "OTHERS(HDMI/MIRRORING) output mode, skip it..");
		return TRUE;
	}
	GST_DEBUG("output:%d(%s)", sa_output[soundalive->filter_output_mode], sa_output_str[soundalive->filter_output_mode]);

#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA
	filter_ret = SoundAlive_Set_OutDev(soundalive->sa_handle, sa_output[soundalive->filter_output_mode]);
	if (filter_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SA]Set_OutDev failed (%d)", filter_ret);
		return FALSE;
	}
#endif

	return TRUE;
}

static gboolean
gst_soundalive_update_sa_preset_mode (Gstsoundalive * soundalive)
{
	int filter_ret = 0;
	int sa_preset[PRESET_NUM] = {SA_NORMAL, SA_POP, SA_ROCK, SA_DANCE, SA_JAZZ,
		SA_CLASSIC, SA_VOCAL, SA_BASS_BOOST, SA_TREBLE_BOOST, SA_MTHEATER,
		SA_EXTERNALIZATION, SA_CAFE, SA_CONCERT_HALL, SA_VOICE, SA_MOVIE, SA_VIRT51, SA_HIPHOP, SA_RNB, SA_FLAT};

#ifdef USE_PA_AUDIO_FILTER
	if (soundalive->use_pa_audio_filter == TRUE)
		return TRUE;
#endif

	if (soundalive->preset_mode >= PRESET_NUM) {
		GST_ERROR_OBJECT(soundalive, "soundalive->preset_mode(%d) is invalid",
			soundalive->preset_mode);
		return FALSE;
	}
	GST_INFO("preset:%d", sa_preset[soundalive->preset_mode]);

#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA
	filter_ret = SoundAlive_Set_Preset(soundalive->sa_handle, sa_preset[soundalive->preset_mode]);
	if (filter_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "Set_Preset failed (%d)", filter_ret);
		return FALSE;
	}
#endif

	return TRUE;
}

static gboolean
gst_soundalive_update_sa_custom_eq (Gstsoundalive * soundalive)
{
	int filter_ret = 0;

#ifdef USE_PA_AUDIO_FILTER
	if (soundalive->use_pa_audio_filter == TRUE)
		return TRUE;
#endif

	GST_INFO_OBJECT(soundalive, "custom_eq:%d,%d,%d,%d,%d,%d,%d",
		soundalive->custom_eq[0], soundalive->custom_eq[1],
		soundalive->custom_eq[2], soundalive->custom_eq[3],
		soundalive->custom_eq[4], soundalive->custom_eq[5],
		soundalive->custom_eq[6]);

#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA
	filter_ret = SoundAlive_Set_User_EQ(soundalive->sa_handle, (int *)soundalive->custom_eq);
	if (filter_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SA]Set_User_EQ failed (%d)", filter_ret);
		return FALSE;
	}
#endif

	return TRUE;
}

static gboolean
gst_soundalive_update_sa_custom_ext (Gstsoundalive * soundalive)
{
	int filter_ret = 0;

#ifdef USE_PA_AUDIO_FILTER
	if (soundalive->use_pa_audio_filter == TRUE)
		return TRUE;
#endif

	GST_INFO_OBJECT(soundalive, "custom_ext: 3D(%d), BE(%d), CHlevel(%d), CHroomsize(%d), Clarity(%d)",
		soundalive->custom_ext[CUSTOM_EXT_3D_LEVEL],
		soundalive->custom_ext[CUSTOM_EXT_BASS_LEVEL],
		soundalive->custom_ext[CUSTOM_EXT_CONCERT_HALL_LEVEL],
		soundalive->custom_ext[CUSTOM_EXT_CONCERT_HALL_VOLUME],
		soundalive->custom_ext[CUSTOM_EXT_CLARITY_LEVEL]);

#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA
	filter_ret = SoundAlive_Set_User_3D( soundalive->sa_handle, (int)soundalive->custom_ext[CUSTOM_EXT_3D_LEVEL]);
	if (filter_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SA]Set_User_3D failed (%d)", filter_ret);
		return FALSE;
	}
	filter_ret = SoundAlive_Set_User_BE( soundalive->sa_handle, (int)soundalive->custom_ext[CUSTOM_EXT_BASS_LEVEL]);
	if (filter_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SA]Set_User_BE failed (%d)", filter_ret);
		return FALSE;
	}
	filter_ret = SoundAlive_Set_User_CHlevel( soundalive->sa_handle, (int)soundalive->custom_ext[CUSTOM_EXT_CONCERT_HALL_LEVEL]);
	if (filter_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SA]Set_User_CHlevel failed (%d)", filter_ret);
		return FALSE;
	}
	filter_ret = SoundAlive_Set_User_CHroomsize( soundalive->sa_handle, (int)soundalive->custom_ext[CUSTOM_EXT_CONCERT_HALL_VOLUME]);
	if (filter_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SA]Set_User_CHroomsize failed (%d)", filter_ret);
		return FALSE;
	}
	filter_ret = SoundAlive_Set_User_Cla( soundalive->sa_handle, (int)soundalive->custom_ext[CUSTOM_EXT_CLARITY_LEVEL]);
	if (filter_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SA]Set_User_Cla failed (%d)", filter_ret);
		return FALSE;
	}

#endif
	return TRUE;
}

static gboolean
gst_soundalive_update_sa_filter (Gstsoundalive * soundalive)
{
	GST_ERROR_OBJECT(soundalive, "");

#ifdef USE_PA_AUDIO_FILTER
	if (soundalive->use_pa_audio_filter == TRUE)
		return TRUE;
#endif

	gst_soundalive_clear_buffer(soundalive);

	/* update sampling rate */
	if (!gst_soundalive_update_sa_samplingrate(soundalive))
		goto error_exit;

	/* update output mode */
	if (!gst_soundalive_update_sa_filter_output_mode(soundalive))
		goto error_exit;

	if (soundalive->filter_action == FILTER_PRESET) {
		/* update preset mode */
		if (!gst_soundalive_update_sa_preset_mode(soundalive))
			goto error_exit;
	} else if (soundalive->filter_action == FILTER_ADVANCED_SETTING) {
		int filter_ret = 0;
		/* set preset mode to SA_USER for activating custom eq/ext */
#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA
		filter_ret = SoundAlive_Set_Preset(soundalive->sa_handle, SA_USER);
		if (filter_ret < 0) {
			GST_ERROR_OBJECT(soundalive, "Set_Preset(SA_USER) failed (%d)", filter_ret);
			goto error_exit;
		}
#endif

		/* update custom eq */
		if (!gst_soundalive_update_sa_custom_eq(soundalive))
			goto error_exit;
		/* update custom ext */
		if (!gst_soundalive_update_sa_custom_ext(soundalive))
			goto error_exit;
	}

	return TRUE;
error_exit:
	return FALSE;
}

static gboolean
gst_soundalive_check_bypass_sa_filter (Gstsoundalive * soundalive)
{
	if ((soundalive->channels > 2)
#ifdef USE_PA_AUDIO_FILTER
		|| (soundalive->use_pa_audio_filter == TRUE)
#endif
		|| (soundalive->filter_output_mode == OUTPUT_OTHERS)
		|| (soundalive->filter_action == FILTER_NONE)
		|| ((soundalive->filter_action == FILTER_PRESET) && (soundalive->preset_mode == PRESET_NORMAL)))
		return TRUE;
	else
		return FALSE;
}

static gboolean
gst_soundalive_process_sa_filter (Gstsoundalive * soundalive, GstBuffer * inbuf, GstBuffer * outbuf, gboolean mono)
{
	int sa_ret = 0;
	gboolean result = TRUE;
	guint remain_size, exe_size, offset = 0;
	guint8 * stereo_inbuf,*stereo_outbuf;

#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA

	if(mono) {
		stereo_inbuf = g_malloc(GST_BUFFER_SIZE(inbuf) << 1);
		if(!stereo_inbuf) {
			GST_ERROR_OBJECT(soundalive,"g_malloc failed.");
			return FALSE;
		}

		/* convert mono to stereo */
		gint i;
		short * mono_buffer = (short*)(GST_BUFFER_DATA(inbuf));
		short (*stereo_buffer)[2] = (short(*)[2])stereo_inbuf;

		for( i = (GST_BUFFER_SIZE(inbuf) >> 1) - 1 ; i >=0 ; --i )
		{
			stereo_buffer[i][0] = mono_buffer[i];
			stereo_buffer[i][1] = mono_buffer[i];
		}

		stereo_outbuf = stereo_inbuf;

		GST_DEBUG_OBJECT(soundalive,"mono to stereo %d",GST_BUFFER_SIZE(inbuf));
	}
	else {
		/* in case  inbuf is different from outbuf */
		stereo_inbuf = GST_BUFFER_DATA(inbuf);
		stereo_outbuf = GST_BUFFER_DATA(outbuf);
	}

	remain_size = mono ? (GST_BUFFER_SIZE(inbuf) << 1) : GST_BUFFER_SIZE(inbuf);
	do {
		exe_size = (remain_size > SOUNDALIVE_SA_BUFFER_MAX_SIZE) ? SOUNDALIVE_SA_BUFFER_MAX_SIZE : remain_size;
		GST_LOG_OBJECT(soundalive, "[SA]Exe in_size:%d out_size:%d remain_size:%d exe_size:%d in_data:%p current_volume:%d",
			GST_BUFFER_SIZE(inbuf), GST_BUFFER_SIZE(outbuf), remain_size, exe_size,
			(GST_BUFFER_DATA(inbuf) + offset),soundalive->current_volume);

		sa_ret = SoundAlive_Exe(soundalive->sa_handle, (short(*)[2])(stereo_outbuf + offset),
			(short(*)[2])(stereo_inbuf + offset), exe_size / SOUNDALIVE_SA_DEFAULT_CHANNEL / DEFAULT_SAMPLE_SIZE, soundalive->current_volume);
		if (sa_ret < 0) {
			GST_ERROR_OBJECT(soundalive, "[SA]Ext failed %d\n", sa_ret);
			result = FALSE;
			break;
		}

		offset += exe_size;
		remain_size -= exe_size;
	} while (remain_size > 0);

	if(mono) {
		if(result) {
			short * mono_buffer = (short*)(GST_BUFFER_DATA(outbuf));
			short (*stereo_buffer)[2] = (short(*)[2])stereo_outbuf;
			gint i;
			for( i = 0; i < (GST_BUFFER_SIZE(outbuf) >> 1) ; ++i )
			{
				mono_buffer[i] = stereo_buffer[i][0];
			}

			GST_DEBUG_OBJECT(soundalive,"stereo to mono %d",GST_BUFFER_SIZE(outbuf));
		}
		/* free memory anyway */
		g_free(stereo_inbuf);
	}

#endif

	return result;
}

static gboolean
gst_soundalive_update_sb_am_filter (Gstsoundalive * soundalive)
{
	int sb_ret = 0;
	short samplerate_for_sb = SAMPLERATE_48000Hz;
	short sb_samplerate[SAMPLERATE_NUM] = {SAMSUNGSB_48000Hz, SAMSUNGSB_44100Hz, SAMSUNGSB_32000Hz,
		SAMSUNGSB_24000Hz, SAMSUNGSB_22050Hz, SAMSUNGSB_16000Hz, SAMSUNGSB_11025Hz, SAMSUNGSB_8000Hz};
	short sb_mode = (soundalive->channels == 1) ? SAMSUNGSB_MONO : SAMSUNGSB_INTERLEAVING_STEREO;

#ifdef USE_PA_AUDIO_FILTER
	if (soundalive->use_pa_audio_filter == TRUE)
		return TRUE;
#endif

	if (soundalive->sb_onoff == FALSE) {
		GST_LOG_OBJECT(soundalive, "[SB_AM]Skip updating filter");
		return TRUE;
	}

	switch (soundalive->samplerate) {
	case 48000:
		samplerate_for_sb = SAMPLERATE_48000Hz;
		break;
	case 44100:
		samplerate_for_sb = SAMPLERATE_44100Hz;
		break;
	case 32000:
		samplerate_for_sb = SAMPLERATE_32000Hz;
		break;
	case 24000:
		samplerate_for_sb = SAMPLERATE_24000Hz;
		break;
	case 22050:
		samplerate_for_sb = SAMPLERATE_22050Hz;
		break;
	case 16000:
		samplerate_for_sb = SAMPLERATE_16000Hz;
		break;
	case 11025:
		samplerate_for_sb = SAMPLERATE_11025Hz;
		break;
	case 8000:
		samplerate_for_sb = SAMPLERATE_8000Hz;
		break;
	default:
		GST_WARNING_OBJECT(soundalive, "[SB_AM]invalid soundalive->samplerate value(%d), we use default value (%d)",
				soundalive->samplerate, samplerate_for_sb);
		break;
    }

	GST_INFO_OBJECT(soundalive, "[SB_AM]lowcut_freq:%d gain_level:%d clarity_level:%d samplerate:%d sb_mode:%d",
		soundalive->lowcut_freq, soundalive->gain_level, soundalive->clarity_level,
		sb_samplerate[samplerate_for_sb], sb_mode);

	sb_ret = SamsungSBConfigAll(soundalive->sb_struct,
							NULL,
							NULL,
							(short)sb_samplerate[samplerate_for_sb],
							0,
							sb_mode,
							(short)soundalive->lowcut_freq,
							(short)soundalive->gain_level,
							(short)soundalive->clarity_level);
	if (sb_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SB_AM]SamsungSBConfigAll failed %d\n", sb_ret);
		return FALSE;
	}
#if 0
	/* Set mode */
	sb_ret = SamsungSBModeConfig(soundalive->sb_struct, sb_mode);
	if (sb_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SB_AM]ModeConfig failed %d\n", sb_ret);
		return FALSE;
	}

	/* Set gain level */
	sb_ret = SamsungSBGainConfig(soundalive->sb_struct, (short)soundalive->gain_level);
	if (sb_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SB_AM]GainConfig failed %d\n", sb_ret);
		return FALSE;
	}

	/* Set clarity level */
	sb_ret = SamsungSBClarityLevelConfig(soundalive->sb_struct, (short)soundalive->clarity_level);
	if (sb_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SB_AM]ClarityLevelConfig failed %d\n", sb_ret);
		return FALSE;
	}

	/* update sampling rate */
	sb_ret = SamsungSBSamplingRateConfig(soundalive->sb_struct, (short)sb_samplerate[samplerate_for_sb]);
	if (sb_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SB_AM]SamplingRateConfig failed %d\n", sb_ret);
		return FALSE;
	}

	/* Set lowcut freq */
	sb_ret = SamsungSBLowCutConfig(soundalive->sb_struct, (short)soundalive->lowcut_freq);
	if (sb_ret < 0) {
		GST_ERROR_OBJECT(soundalive, "[SB_AM]LowCutConfig failed %d\n", sb_ret);
		return FALSE;
	}
#endif

	return TRUE;
}

static gboolean
gst_soundalive_check_bypass_sb_am_filter (Gstsoundalive * soundalive)
{
	if ((soundalive->channels > 2)
#ifdef USE_PA_AUDIO_FILTER
		|| (soundalive->use_pa_audio_filter == TRUE)
#endif
		|| (soundalive->filter_output_mode != OUTPUT_SPK)
		|| ((soundalive->sb_onoff == FALSE) && (soundalive->am_onoff == FALSE)))
		return TRUE;
	else
		return FALSE;
}

static gboolean
gst_soundalive_process_sb_am_filter (Gstsoundalive * soundalive, GstBuffer * inbuf, GstBuffer * outbuf)
{
	int sb_ret = 0;
	guint remain_size, exe_size, offset = 0;
	int sb_buf_size;

	switch (soundalive->samplerate) {
	case 48000:
	case 44100:
		sb_buf_size = SOUNDALIVE_SB_AM_BUFFER_MAX_SIZE;
		break;
	case 32000:
	case 24000:
	case 22050:
	case 16000:
		sb_buf_size = SOUNDALIVE_SB_AM_BUFFER_HALF;
		break;
	case 11025:
	case 8000:
		sb_buf_size = SOUNDALIVE_SB_AM_BUFFER_QUATER;
		break;
	default:
		sb_buf_size = SOUNDALIVE_SB_AM_BUFFER_MAX_SIZE;
		break;
	}

	GST_LOG_OBJECT(soundalive, "[%d], %d", soundalive->samplerate, sb_buf_size);

	remain_size = GST_BUFFER_SIZE(inbuf);
	do {
		exe_size = (remain_size > sb_buf_size) ? sb_buf_size : remain_size;
		GST_LOG_OBJECT(soundalive, "[SB_AM]Exe sb_onoff:%d am_on_off:%d in_size:%d out_size:%d remain_size:%d exe_size:%d",
			soundalive->sb_onoff, soundalive->am_onoff, GST_BUFFER_SIZE(inbuf), GST_BUFFER_SIZE(outbuf),
			remain_size, exe_size);

		sb_ret = SamsungSBInOutConfig(soundalive->sb_struct,
			(gshort *)(GST_BUFFER_DATA(inbuf) + offset),
			(gshort *)(GST_BUFFER_DATA(outbuf) + offset));
		if (sb_ret < 0) {
			GST_ERROR_OBJECT(soundalive, "[SB_AM]InOutConfig failed %d\n", sb_ret);
			return FALSE;
		}

		sb_ret = SamsungSBFrameLengthConfig(soundalive->sb_struct, exe_size / soundalive->channels);
		if (sb_ret < 0) {
			GST_ERROR_OBJECT(soundalive, "[SB_AM]FrameLengthConfig failed %d\n", sb_ret);
			return FALSE;
		}

		sb_ret = SamsungSBExe(soundalive->sb_struct,
			(short)soundalive->sb_onoff, (short)soundalive->am_onoff,
			(short)soundalive->am_band_onoff[AM_BAND_01],
			(short)soundalive->am_band_onoff[AM_BAND_02],
			(short)soundalive->am_band_onoff[AM_BAND_03],
			(int *)soundalive->am_band_coef[AM_BAND_01],
			(int *)soundalive->am_band_coef[AM_BAND_02],
			(int *)soundalive->am_band_coef[AM_BAND_03], 0, 0, 0);
		if (sb_ret < 0) {
			GST_ERROR_OBJECT(soundalive, "[SB_AM]Exe failed %d\n", sb_ret);
			return FALSE;
		}

		offset += exe_size;
		remain_size -= exe_size;
	} while (remain_size > 0);

	return TRUE;
}

#ifdef SOUNDALIVE_ENABLE_RESAMPLER
static gshort
gst_soundalive_src_convert_samplerate (gint samplerate)
{
	switch(samplerate)
	{
		case 8000:
			return SR08k;
		case 11025:
			return SR11k;
		case 16000:
			return SR16k;
		case 22050:
			return SR22k;
		case 24000:
			return SR24k;
		case 32000:
			return SR32k;
		case 44100:
			return SR44k;
		case 48000:
			return SR48k;
		default:
			return SR08k;
	}
}

static gboolean
gst_soundavlie_init_src_filter (Gstsoundalive * soundalive)
{
	gshort samplerate_in = gst_soundalive_src_convert_samplerate(soundalive->samplerate);
	gshort samplerate_out = gst_soundalive_src_convert_samplerate(DEFAULT_SAMPLE_RATE);

	GST_INFO_OBJECT(soundalive, "[SRC]Initialize filter");

	if (soundalive->src_state) {
		SRC_Reset(soundalive->src_state);
	}
	if ((soundalive->src_state = SRC_Init((short)soundalive->channels, (short)samplerate_in, (short)samplerate_out, Q_MID)) == NULL) {
		GST_ERROR("[SRC] failed to init resampler");
		return FALSE;
	}

	return TRUE;
}

static gboolean
gst_soundalive_check_bypass_src_filter (Gstsoundalive * soundalive)
{
	if (soundalive->samplerate == DEFAULT_SAMPLE_RATE)
		return TRUE;
	else
		return FALSE;
}

static gboolean
gst_soundalive_process_src_filter (Gstsoundalive * soundalive, GstBuffer * inbuf, GstBuffer * outbuf)
{
	int src_ret = 0;
	short outsamples;
	guint remain_size, exe_size, offset_inbuf = 0, offset_outbuf = 0;

#ifdef SOUNDALIVE_ENABLE_RESAMPLER
	if (GST_BUFFER_IS_DISCONT (inbuf)) {
		gst_soundavlie_init_src_filter(soundalive);
	}
#endif

	remain_size = GST_BUFFER_SIZE(inbuf);
	do {
		exe_size = (remain_size > SOUNDALIVE_SRC_BUFFER_MAX_SIZE) ? SOUNDALIVE_SRC_BUFFER_MAX_SIZE : remain_size;

		src_ret = SRC_InoutConfig(soundalive->src_state, (short *)(GST_BUFFER_DATA(inbuf) + offset_inbuf),
			(short *)(GST_BUFFER_DATA(outbuf) + offset_outbuf));
		if (src_ret < 0) {
			GST_ERROR_OBJECT(soundalive, "[SRC]InOutConfig failed %d\n", src_ret);
			return FALSE;
		}
		outsamples = SRC_Exe(soundalive->src_state, (long)(exe_size / DEFAULT_SAMPLE_SIZE),
			(long)(exe_size * SOUNDALIVE_SRC_MULTIPLIER_OUTPUT / DEFAULT_SAMPLE_SIZE));
		offset_inbuf += exe_size;
		offset_outbuf += outsamples * DEFAULT_SAMPLE_SIZE;
		remain_size -= exe_size;
	} while (remain_size > 0);
	GST_LOG_OBJECT(soundalive, "[SRC]Exe in_size:%d out_size:%d exe_size:%d",
		GST_BUFFER_SIZE(inbuf), GST_BUFFER_SIZE(outbuf), offset_outbuf);
	GST_BUFFER_SIZE(outbuf) = offset_outbuf;

	return TRUE;
}

static void
gst_soundalive_update_passthrough_mode (Gstsoundalive * soundalive)
{
	if (gst_soundalive_check_bypass_sa_filter(soundalive)
		&& gst_soundalive_check_bypass_sb_am_filter(soundalive)
		&& gst_soundalive_check_bypass_src_filter(soundalive)) {
		GST_INFO_OBJECT(soundalive, "passthrough mode is set");
		gst_base_transform_set_passthrough (GST_BASE_TRANSFORM(soundalive), TRUE);
	}
	else {
		GST_INFO_OBJECT(soundalive, "passthrough mode is unset");
		gst_base_transform_set_passthrough (GST_BASE_TRANSFORM(soundalive), FALSE);
	}
}

static GstCaps *
gst_soundalive_transform_caps (GstBaseTransform * base, GstPadDirection direction, GstCaps * caps)
{
	const GValue *val;
	GstStructure *s;
	GstCaps *res;

	res = gst_caps_copy (caps);
	s = gst_caps_get_structure (res, 0);
	val = gst_structure_get_value (s, "rate");

	if (direction == GST_PAD_SRC) {
		/* overwrite existing range, or add field if it doesn't exist yet */
		gst_structure_set (s, "rate", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
	} else {
		s = gst_structure_copy (s);
		gst_structure_set (s, "rate", G_TYPE_INT, 44100, NULL);
		gst_caps_append_structure (res, s);
	}

	return res;
}

static gboolean
gst_soundalive_transform_size (GstBaseTransform * base,
	GstPadDirection direction, GstCaps * caps, guint size, GstCaps * othercaps, guint * othersize)
{
	Gstsoundalive *soundalive = GST_SOUNDALIVE(base);
	GstStructure *structure;
	gint inrate, outrate, framesize;

	if (gst_soundalive_check_bypass_src_filter(soundalive)) {
		*othersize = size;
	} else {
		structure = gst_caps_get_structure (caps, 0);
		gst_structure_get_int (structure, "rate", &inrate);
		structure = gst_caps_get_structure (othercaps, 0);
		gst_structure_get_int (structure, "rate", &outrate);
		framesize = DEFAULT_SAMPLE_SIZE * soundalive->channels;

//		*othersize = (((guint64)(size / framesize * outrate) / inrate) + SOUNDALIVE_SRC_EXTRA_FRAMES) * framesize;
		*othersize = size * 6;
	}

	return TRUE;
}

static GstFlowReturn
gst_soundalive_transform (GstBaseTransform * base, GstBuffer * inbuf, GstBuffer * outbuf)
{
	GstFlowReturn ret = GST_FLOW_OK;
	Gstsoundalive *soundalive = GST_SOUNDALIVE(base);
	gboolean bypass_sa_filter, bypass_sb_am_filter, bypass_src_filter;
	GstBuffer *sa_inbuf = NULL, *sa_outbuf = NULL;
	GstBuffer *sb_am_inbuf = NULL, *sb_am_outbuf = NULL;
	GstBuffer *src_inbuf = NULL, *src_outbuf = NULL;
	gboolean mono;

	gst_buffer_copy_metadata(outbuf, inbuf, GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS);

	if (!inbuf || GST_BUFFER_SIZE(inbuf) == 0) {
		GST_WARNING("Input buffer is invalid, so skipping");
		return ret;
	}

	/* Update config variables if needed */
	if (soundalive->need_update_filter) {
		if (!gst_soundalive_update_sa_filter(soundalive))
			return GST_FLOW_ERROR;
		if (!gst_soundalive_update_sb_am_filter(soundalive))
			return GST_FLOW_ERROR;
		soundalive->need_update_filter = FALSE;
	}

#ifdef SOUNDALIVE_ENABLE_DUMP
	if (soundalive->dump_input_fp)
		fwrite(GST_BUFFER_DATA(inbuf), 1, GST_BUFFER_SIZE(inbuf), soundalive->dump_input_fp);
#endif

	bypass_sa_filter = gst_soundalive_check_bypass_sa_filter(soundalive);
	bypass_sb_am_filter = gst_soundalive_check_bypass_sb_am_filter(soundalive);
	bypass_src_filter = gst_soundalive_check_bypass_src_filter(soundalive);
	mono = (soundalive->channels == 1);

	if (!bypass_sa_filter) {
		sa_inbuf = inbuf;
	}
	if (!bypass_sb_am_filter) {
		sb_am_inbuf = (sa_inbuf) ? gst_buffer_new_and_alloc(GST_BUFFER_SIZE(inbuf)) : inbuf;
	}
	if (!bypass_src_filter) {
		src_inbuf = (sa_inbuf || sb_am_inbuf) ? gst_buffer_new_and_alloc(GST_BUFFER_SIZE(inbuf)) : inbuf;
	}

	if (!bypass_src_filter) {
		src_outbuf = outbuf;
		if (!bypass_sb_am_filter) {
			sb_am_outbuf = src_inbuf;
			if (!bypass_sa_filter) {
				sa_outbuf = sb_am_inbuf;
			}
		} else if (sa_inbuf) {
			sa_outbuf = src_inbuf;
		}
	} else if (!bypass_sb_am_filter) {
		sb_am_outbuf = outbuf;
		if (!bypass_sa_filter) {
			sa_outbuf = sb_am_inbuf;
		}
	} else if (!bypass_sa_filter) {
		sa_outbuf = outbuf;
	} else {
		GST_WARNING_OBJECT(soundalive, "cannot transform because there is no filter available");
	}

	GST_LOG_OBJECT(soundalive, "buffer size:%d, filter action:%d, output:%d\n",
		GST_BUFFER_SIZE(inbuf), soundalive->filter_action, soundalive->filter_output_mode);

	if (bypass_sa_filter == FALSE) {
		if (!gst_soundalive_process_sa_filter(soundalive, sa_inbuf, sa_outbuf, mono)) {
			ret = GST_FLOW_ERROR;
			goto exit;
		}
	}

	/* Sound Booster & Acoustic Equalizer Filter */
	if (!bypass_sb_am_filter) {
		if (!gst_soundalive_process_sb_am_filter(soundalive, sb_am_inbuf, sb_am_outbuf)) {
			ret = GST_FLOW_ERROR;
			goto exit;
		}
	}

	if (!bypass_src_filter) {
		if (!gst_soundalive_process_src_filter(soundalive, src_inbuf, src_outbuf)) {
			ret = GST_FLOW_ERROR;
			goto exit;
		}
	}

exit:
#ifdef SOUNDALIVE_ENABLE_DUMP
	if (soundalive->dump_output_fp)
		fwrite(GST_BUFFER_DATA(outbuf), 1, GST_BUFFER_SIZE(outbuf), soundalive->dump_output_fp);
#endif

	if (sb_am_inbuf && (sb_am_inbuf != inbuf)) {
		gst_buffer_unref(sb_am_inbuf);
	}
	if (src_inbuf && (src_inbuf != inbuf)) {
		gst_buffer_unref(src_inbuf);
	}

	return ret;
}

static gboolean gst_soundalive_sink_event (GstBaseTransform * base, GstEvent * event)
{
	Gstsoundalive *soundalive = GST_SOUNDALIVE(base);

	switch (GST_EVENT_TYPE (event)) {
		case GST_EVENT_FLUSH_STOP:
#ifdef SOUNDALIVE_ENABLE_RESAMPLER
		gst_soundavlie_init_src_filter(soundalive);
#endif
		break;
	default:
		break;
	}

	return parent_class->event (base, event);
}

#else
static GstFlowReturn
gst_soundalive_transform_ip (GstBaseTransform * base, GstBuffer * buf)
{
	GstFlowReturn ret = GST_FLOW_OK;
	Gstsoundalive *soundalive = GST_SOUNDALIVE(base);
	gboolean bypass_sa_filter, bypass_sb_am_filter;
	gboolean mono;

	if (!buf || GST_BUFFER_SIZE(buf) == 0) {
		GST_WARNING("Input buffer is invalid, so skipping");
		return ret;
	}

	/* Update config variables if needed */
	if (soundalive->need_update_filter) {
		if (!gst_soundalive_update_sa_filter(soundalive))
			return GST_FLOW_ERROR;
		if (!gst_soundalive_update_sb_am_filter(soundalive))
			return GST_FLOW_ERROR;
		soundalive->need_update_filter = FALSE;
	}

#ifdef SOUNDALIVE_ENABLE_DUMP
	if (soundalive->dump_input_fp)
		fwrite(GST_BUFFER_DATA(buf), 1, GST_BUFFER_SIZE(buf), soundalive->dump_input_fp);
#endif

	bypass_sa_filter = gst_soundalive_check_bypass_sa_filter(soundalive);
	bypass_sb_am_filter = gst_soundalive_check_bypass_sb_am_filter(soundalive);
	mono = (soundalive->channels == 1);

	if ((bypass_sa_filter == TRUE) && (bypass_sb_am_filter == TRUE)) {
		GST_LOG_OBJECT(soundalive, "Bypassing SA,SB_AM Filter");
		goto exit;
	} else if (bypass_sa_filter == TRUE) {
		GST_LOG_OBJECT(soundalive, "Bypassing SA Filter");
	} else if (bypass_sb_am_filter == TRUE) {
		GST_LOG_OBJECT(soundalive, "Bypassing SB_AM Filter");
	}

	GST_LOG_OBJECT(soundalive, "buffer size:%d, filter action:%d, output:%d\n",
		GST_BUFFER_SIZE(buf), soundalive->filter_action, soundalive->filter_output_mode);

	/* Sound Alive Filter */
	if (!bypass_sa_filter) {
		if (!gst_soundalive_process_sa_filter(soundalive, buf, buf, mono)) {
			ret = GST_FLOW_ERROR;
			goto error_exit;
		}
	}

	/* Sound Booster & Acoustic Equalizer Filter */
	if (!bypass_sb_am_filter) {
		if (!gst_soundalive_process_sb_am_filter(soundalive, buf, buf)) {
			ret = GST_FLOW_ERROR;
			goto error_exit;
		}
	}

exit:
#ifdef SOUNDALIVE_ENABLE_DUMP
	if (soundalive->dump_output_fp)
		fwrite(GST_BUFFER_DATA(buf), 1, GST_BUFFER_SIZE(buf), soundalive->dump_output_fp);
#endif

error_exit:
	return ret;
}
#endif

static gboolean
gst_soundalive_set_caps (GstBaseTransform * base, GstCaps * incaps,
	GstCaps * outcaps)
{
	Gstsoundalive *soundalive = GST_SOUNDALIVE(base);
	GstStructure *ins;
	gint samplerate;
	gint channels;
	gshort old_samplerate;
	gshort old_channels;
#ifndef SOUNDALIVE_ENABLE_RESAMPLER
	GstPad *pad;

	pad = gst_element_get_static_pad(GST_ELEMENT(soundalive), "src");

	/* forward-negotiate */
	if(!gst_pad_set_caps(pad, incaps)) {
		gst_object_unref(pad);
		return FALSE;
	}
#endif

	/* negotiation succeeded, so now configure ourselves */
	ins = gst_caps_get_structure(incaps, 0);

	/* get samplerate from caps & convert */
	old_samplerate = soundalive->samplerate;
	old_channels = soundalive->channels;
	gst_structure_get_int(ins, "rate", &samplerate);
	switch (samplerate) {
	case 48000:
	case 44100:
	case 32000:
	case 24000:
	case 22050:
	case 16000:
	case 12000:
	case 11025:
	case 8000:
		soundalive->samplerate = samplerate;
		break;
	default:
		if (samplerate < 8000) {
			soundalive->samplerate = 8000;
		} else if (samplerate > 48000) {
			soundalive->samplerate = 48000;
		}
		break;
	}
	/* get number of channels from caps */
	gst_structure_get_int(ins, "channels", &channels);
	soundalive->channels = (gshort)channels;

	if ((old_samplerate != soundalive->samplerate)
		|| (old_channels != soundalive->channels)) {
		soundalive->need_update_filter = TRUE;
	}

#ifdef SOUNDALIVE_ENABLE_RESAMPLER
	gst_soundavlie_init_src_filter(soundalive);
	gst_soundalive_update_passthrough_mode(soundalive);
#endif
	GST_INFO_OBJECT(soundalive, "set samplerate(%d), channels(%d)", soundalive->samplerate, soundalive->channels);

#ifndef SOUNDALIVE_ENABLE_RESAMPLER
	gst_object_unref (pad);
#endif

	return TRUE;
}

static void
gst_soundalive_set_property (GObject * object, guint prop_id,
	const GValue * value, GParamSpec * pspec)
{
	Gstsoundalive *soundalive = GST_SOUNDALIVE (object);
	gshort *pointer;

	switch (prop_id) {
	case PROP_FILTER_ACTION:
		GST_INFO_OBJECT(soundalive, "request setting filter_action:%d (current:%d)",
			g_value_get_uint(value), soundalive->filter_action);
		if (g_value_get_uint(value) == soundalive->filter_action) {
			GST_DEBUG_OBJECT(soundalive, "requested filter_action variable is same as previous, so skip");
			break;
		}
		soundalive->filter_action = g_value_get_uint(value);
		soundalive->need_update_filter = TRUE;
#ifdef SOUNDALIVE_ENABLE_RESAMPLER
		gst_soundalive_update_passthrough_mode(soundalive);
#endif
#ifdef USE_PA_AUDIO_FILTER
		if (soundalive->use_pa_audio_filter == TRUE) {
			g_object_set(soundalive->sink, "filter-action", soundalive->filter_action, NULL);
		}
#endif
		break;

	case PROP_FILTER_OUTPUT_MODE:
		GST_INFO_OBJECT(soundalive, "request setting filter_output_mode:%d (current:%d)",
			g_value_get_uint(value), soundalive->filter_output_mode);
		if (g_value_get_uint(value) == soundalive->filter_output_mode) {
			GST_DEBUG_OBJECT(soundalive, "requested filter_output_mode variable is same as previous, so skip");
			break;
		}
		soundalive->filter_output_mode = g_value_get_uint(value);
		gst_soundalive_update_sa_filter_output_mode(soundalive);
		break;

	case PROP_PRESET_MODE:
		GST_INFO_OBJECT(soundalive, "request setting preset_mode:%d (current:%d)",
			g_value_get_uint(value), soundalive->preset_mode);
		if (g_value_get_uint(value) == soundalive->preset_mode) {
			GST_DEBUG_OBJECT(soundalive, "requested preset_mode variable is same as previous, so skip");
			break;
		}
		soundalive->preset_mode = g_value_get_uint(value);
		if (soundalive->filter_action == FILTER_PRESET) {
			soundalive->need_update_filter = TRUE;
//			gst_soundalive_update_sa_preset_mode(soundalive);
#ifdef SOUNDALIVE_ENABLE_RESAMPLER
			gst_soundalive_update_passthrough_mode(soundalive);
#endif
		}
#ifdef USE_PA_AUDIO_FILTER
		if (soundalive->use_pa_audio_filter == TRUE) {
			g_object_set(soundalive->sink, "preset-mode", soundalive->preset_mode, NULL);
		}
#endif
		break;

	case PROP_CUSTOM_EQ:
		pointer = g_value_get_pointer(value);
#ifdef USE_PA_AUDIO_FILTER
		if (soundalive->use_pa_audio_filter == TRUE) {
			g_object_set(soundalive->sink, "custom-eq", pointer, NULL);
		}
#endif
		if (pointer) {
			gint *custom_eq = (gint *)pointer;
			GST_INFO_OBJECT(soundalive, "request setting custom_eq:%d,%d,%d,%d,%d,%d,%d (current:%d,%d,%d,%d,%d,%d,%d)",
				custom_eq[0], custom_eq[1], custom_eq[2], custom_eq[3], custom_eq[4], custom_eq[5], custom_eq[6],
				soundalive->custom_eq[0], soundalive->custom_eq[1], soundalive->custom_eq[2], soundalive->custom_eq[3],
				soundalive->custom_eq[4], soundalive->custom_eq[5], soundalive->custom_eq[6]);
			if (!memcmp(custom_eq, soundalive->custom_eq, sizeof(gint) * CUSTOM_EQ_BAND_MAX)) {
				GST_DEBUG_OBJECT(soundalive, "requested custom_eq variables are same as previous, so skip");
				break;
			}
			memcpy(soundalive->custom_eq, pointer, sizeof(gint) * CUSTOM_EQ_BAND_MAX);
			if (soundalive->filter_action == FILTER_ADVANCED_SETTING) {
				gst_soundalive_update_sa_custom_eq(soundalive);
			}
		}
		break;

	case PROP_CUSTOM_EXT:
		pointer = g_value_get_pointer(value);
#ifdef USE_PA_AUDIO_FILTER
		if (soundalive->use_pa_audio_filter == TRUE) {
			g_object_set(soundalive->sink, "custom-ext", pointer, NULL);
		}
#endif
		if (pointer) {
			gint *custom_ext = (gint *)pointer;
			GST_INFO_OBJECT(soundalive, "request setting custom_ext:%d,%d,%d,%d,%d (current:%d,%d,%d,%d,%d)",
				custom_ext[CUSTOM_EXT_3D_LEVEL], custom_ext[CUSTOM_EXT_BASS_LEVEL],
				custom_ext[CUSTOM_EXT_CONCERT_HALL_LEVEL], custom_ext[CUSTOM_EXT_CONCERT_HALL_VOLUME],
				custom_ext[CUSTOM_EXT_CLARITY_LEVEL],
				soundalive->custom_ext[CUSTOM_EXT_3D_LEVEL], soundalive->custom_ext[CUSTOM_EXT_BASS_LEVEL],
				soundalive->custom_ext[CUSTOM_EXT_CONCERT_HALL_LEVEL], soundalive->custom_ext[CUSTOM_EXT_CONCERT_HALL_VOLUME],
				soundalive->custom_ext[CUSTOM_EXT_CLARITY_LEVEL]);
			if (!memcmp(custom_ext, soundalive->custom_ext, sizeof(gint) * CUSTOM_EXT_PARAM_MAX)) {
				GST_DEBUG_OBJECT(soundalive, "requested custom_ext variables are same as previous, so skip");
				break;
			}
			memcpy(soundalive->custom_ext, pointer, sizeof(gint) * CUSTOM_EXT_PARAM_MAX);
			if (soundalive->filter_action == FILTER_ADVANCED_SETTING) {
				gst_soundalive_update_sa_custom_ext(soundalive);
			}
		}
		break;

	default:
		break;
	}
}

static void
gst_soundalive_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
	Gstsoundalive *soundalive = GST_SOUNDALIVE (object);

	switch (prop_id) {
	case PROP_FILTER_ACTION:
		g_value_set_uint(value, soundalive->filter_action);
		break;

	case PROP_FILTER_OUTPUT_MODE:
		g_value_set_uint(value, soundalive->filter_output_mode);
		break;

	case PROP_PRESET_MODE:
		g_value_set_uint(value, soundalive->preset_mode);
		break;

	case PROP_CUSTOM_EQ:
		g_value_set_pointer(value, soundalive->custom_eq);
		break;

	case PROP_CUSTOM_EXT:
		g_value_set_pointer(value, soundalive->custom_ext);
		break;

	case PROP_CUSTOM_EQ_BAND_NUM:
		g_value_set_uint(value, soundalive->custom_eq_band_num);
		break;

	case PROP_CUSTOM_EQ_BAND_FQ:
		g_value_set_pointer(value, soundalive->custom_eq_band_freq);
		break;

	case PROP_CUSTOM_EQ_BAND_WIDTH:
		g_value_set_pointer(value, soundalive->custom_eq_band_width);
		break;

	default:
		break;
	}
}


static gboolean
plugin_init (GstPlugin * plugin)
{
	GST_DEBUG_CATEGORY_INIT(gst_soundalive_debug, "soundalive", 0, "Sound Alive Audio Filter plugin");
	return gst_element_register(plugin, "soundalive", GST_RANK_NONE, GST_TYPE_SOUNDALIVE);
}


GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	"soundalive",
	"Sound Alive Audio Filter plugin",
	plugin_init,
	VERSION,
	"Proprietary",
	"Samsung Electronics Co",
	"http://www.samsung.com")

