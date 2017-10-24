/*
* secrecord
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
#include "config.h"
#endif

#include <iniparser.h>
#include <vconf.h>
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

#include "gstsecrecord.h"

GST_DEBUG_CATEGORY_STATIC (gst_secrecord_debug);
#define GST_CAT_DEFAULT gst_secrecord_debug

#define SECRECORD_ENABLE_DUMP

#define SECRECORD_REC_INI_CSC_PATH			"/opt/system/csc-default/usr/tuning/mmfw_audio_record.ini"
#define SECRECORD_REC_INI_DEFAULT_PATH		"/usr/etc/mmfw_audio_record.ini"
#define SECRECORD_REC_INI_TEMP_PATH			"/opt/system/mmfw_audio_record.ini"

/* dump flag is defined in module-policy.c */
#define SECRECORD_DUMP_VCONF_KEY			"memory/private/sound/pcm_dump"
#define SECRECORD_DUMP_INPUT_FLAG			0x20000000	/* PA_DUMP_CAPTURE_SEC_RECORD_IN */
#define SECRECORD_DUMP_OUTPUT_FLAG			0x40000000	/* PA_DUMP_CAPTURE_SEC_RECORD_OUT */
#define SECRECORD_DUMP_INPUT_PATH_PREFIX	"/tmp/dump_sec_record_in_"
#define SECRECORD_DUMP_OUTPUT_PATH_PREFIX	"/tmp/dump_sec_record_out_"

#ifdef TEMPORALLY
#define SCERECORD_DEBUG_FRAME_NUM	10
#define DEBUG(x) (x <= SCERECORD_DEBUG_FRAME_NUM)
#endif


/* Filter signals and args */
enum
{
	/* FILL ME */
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_FILTER_RECORD,
	PROP_FILTER_SOUND_BOOSTER,
	PROP_FILTER_NOISE,
	PROP_FILTER_BEAM_FORMING_INTERVIEW,
	PROP_FILTER_BEAM_FORMING_CONVERSATION,
};

enum FilterActionType
{
	FILTER_OFF,
	FILTER_ON,
};

enum RecordMode
{
	RECORDMODE_BYPASS,
	RECORDMODE_MONO,
	RECORDMODE_STEREO,
	RECORDMODE_NUM
};

enum SampleRate
{
	SAMPLERATE_48000Hz,
	SAMPLERATE_44100Hz,
	SAMPLERATE_32000Hz,
	SAMPLERATE_24000Hz,
	SAMPLERATE_22050Hz,
	SAMPLERATE_16000Hz,
	SAMPLERATE_8000Hz,

	SAMPLERATE_NUM
};

#define DEFAULT_NRSS_MODE				3
#define DEFAULT_RECORD_MODE				RECORDMODE_BYPASS
#define DEFAULT_SAMPLE_RATE				SAMPLERATE_44100Hz
#define DEFAULT_SAMPLE_SIZE				2
#define DEFAULT_FILTER_ACTION			FILTER_OFF

#define SECRECORD_KEY_MAX_LEN 256

#define SECRECORD_MONO_SAMPLE_THREASHOLD		1024
#define SECRECORD_STEREO_SAMPLE_THREASHOLD		2048

#define SECRECORD_INI_GET_BOOLEAN(dict, key, value, default) \
do { \
	value = iniparser_getboolean(dict, key, default); \
	GST_DEBUG("get %s = %s", key, (value) ? "y" : "n"); \
} while(0)

#define SECRECORD_INI_GET_INT(dict, key, value, default) \
do { \
	value = iniparser_getint(dict, key, default); \
	GST_DEBUG("get %s = %d", key, value); \
} while(0)

#define SECRECORD_INI_GET_DOUBLE(dict, key, value, default) \
do { \
	value = iniparser_getdouble(dict, key, default); \
	GST_DEBUG("get %s = %lf", key, value); \
} while(0)

#define SECRECORD_INI_GET_INT_LIST(dict, key, values, max_count, default) \
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

static const char *g_filter_str[] = {
	"rec_filter_01",
	"rec_filter_02",
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

static const char *g_band_str[] = {
	"am_band_01",
	"am_band_02",
	"am_band_03"
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


GST_BOILERPLATE (Gstsecrecord, gst_secrecord, GstBaseTransform, GST_TYPE_BASE_TRANSFORM);


static void gst_secrecord_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_secrecord_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_secrecord_transform_ip (GstBaseTransform * base, GstBuffer * buf);
static gboolean gst_secrecord_set_caps (GstBaseTransform * base, GstCaps * incaps, GstCaps * outcaps);

static void
gst_secrecord_init_rec_config (Gstsecrecord * secrecord)
{
	dictionary * dict = NULL;
	char key[SECRECORD_KEY_MAX_LEN] = {0, };
	int i, j;
	dict = iniparser_load(SECRECORD_REC_INI_TEMP_PATH);
	if (!dict) {
		GST_DEBUG("%s load failed. Use default temporary file", SECRECORD_REC_INI_TEMP_PATH);
		dict = iniparser_load(SECRECORD_REC_INI_CSC_PATH);
		if (!dict) {
			GST_INFO("%s load failed. Use temporary file", SECRECORD_REC_INI_CSC_PATH);
			dict = iniparser_load(SECRECORD_REC_INI_DEFAULT_PATH);
			if (!dict) {
				GST_WARNING("%s load failed", SECRECORD_REC_INI_DEFAULT_PATH);
				return;
			}
		}
	}
	SECRECORD_INI_GET_BOOLEAN(dict, "rec:enable", secrecord->rec_onoff, 0);
	SECRECORD_INI_GET_INT(dict, "rec:amr_pregain", secrecord->amr_pregain, 0);
	SECRECORD_INI_GET_INT(dict, "rec:aac_l_pregain", secrecord->aac_l_pregain, 0);
	SECRECORD_INI_GET_INT(dict, "rec:aac_r_pregain", secrecord->aac_r_pregain, 0);
	for (i = 0; i < REC_FILTER_NUM; i++) {
		sprintf(key, "%s:enable", g_filter_str[i]);
		SECRECORD_INI_GET_BOOLEAN(dict, key, secrecord->rec_filter_onoff[i], 0);
		for (j = 0; j < REC_FREQ_NUM; j++) {
			sprintf(key, "%s:%s", g_filter_str[i], g_freq_str[j]);
			SECRECORD_INI_GET_INT_LIST(dict,
				key,
				secrecord->rec_filter_coef[i][j], REC_COEF_NUM, 0);
		}
	}


	/*SB*/
	secrecord->sb_onoff = DEFAULT_FILTER_ACTION;
	iniparser_freedict(dict);
}

#ifdef SECRECORD_ENABLE_DUMP
static void
gst_secrecord_init_dump_config (Gstsecrecord * secrecord)
{
	int vconf_dump = 0;

	if (vconf_get_int(SECRECORD_DUMP_VCONF_KEY, &vconf_dump)) {
		GST_WARNING("vconf_get_int %s failed", SECRECORD_DUMP_VCONF_KEY);
	}

	/* input */
	secrecord->need_dump_input = (vconf_dump & SECRECORD_DUMP_INPUT_FLAG) ? TRUE : FALSE;
	secrecord->dump_input_fp = NULL;
	/* output */
	secrecord->need_dump_output = (vconf_dump & SECRECORD_DUMP_OUTPUT_FLAG) ? TRUE : FALSE;
	secrecord->dump_output_fp = NULL;
}

static void
gst_secrecord_open_dump_file (Gstsecrecord * secrecord)
{
	char *suffix, *dump_path;
	GDateTime *time = g_date_time_new_now_local();

	suffix = g_date_time_format(time, "%m%d_%H%M%S.pcm");
	/* input */
	if (secrecord->need_dump_input) {
		dump_path = g_strjoin(NULL, SECRECORD_DUMP_INPUT_PATH_PREFIX, suffix, NULL);
		secrecord->dump_input_fp = fopen(dump_path, "w+");
		g_free(dump_path);
	}

	/* output */
	if (secrecord->need_dump_output) {
		dump_path = g_strjoin(NULL, SECRECORD_DUMP_OUTPUT_PATH_PREFIX, suffix, NULL);
		secrecord->dump_output_fp = fopen(dump_path, "w+");
		g_free(dump_path);
	}
	g_free(suffix);

	g_date_time_unref(time);
}

static void
gst_secrecord_close_dump_file (Gstsecrecord * secrecord)
{
	/* input */
	if (secrecord->dump_input_fp) {
		fclose(secrecord->dump_input_fp);
		secrecord->dump_input_fp = NULL;
	}

	/* output */
	if (secrecord->dump_output_fp) {
		fclose(secrecord->dump_output_fp);
		secrecord->dump_output_fp = NULL;
	}
}
#endif

static GstStateChangeReturn
gst_secrecord_change_state (GstElement * element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	Gstsecrecord *secrecord = GST_SECRECORD (element);

	switch (transition) {
	case GST_STATE_CHANGE_NULL_TO_READY:
		GST_WARNING_OBJECT(secrecord, "NULL_TO_READY");
		break;
	case GST_STATE_CHANGE_READY_TO_PAUSED:
		GST_WARNING_OBJECT(secrecord, "READY_TO_PAUSED");
		break;
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		GST_WARNING_OBJECT(secrecord, "start PAUSED_TO_PLAYING");
#ifdef SECRECORD_ENABLE_DUMP
	gst_secrecord_open_dump_file(secrecord);
#endif
		GST_WARNING_OBJECT(secrecord, "done PAUSED_TO_PLAYING");
		break;
	default:
		break;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

	if (ret == GST_STATE_CHANGE_FAILURE)
		return ret;

	switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		GST_WARNING_OBJECT(secrecord, "start PLAYING_TO_PAUSED");
#ifdef SECRECORD_ENABLE_DUMP
		gst_secrecord_close_dump_file(secrecord);
#endif
		GST_WARNING_OBJECT(secrecord, "done PLAYING_TO_PAUSED");
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		GST_WARNING_OBJECT(secrecord, "PAUSED_TO_READY");
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		GST_WARNING_OBJECT(secrecord, "READY_TO_NULL");
		break;
	default:
		break;
	}

	return ret;
}

static void
gst_secrecord_base_init (gpointer gclass)
{
	static GstElementDetails element_details = {
		"Samsung Recording Audio Filter",
		"Filter/Effect/Audio",
		"Adjust quality of recorded audio/raw streams",
		"Samsung Electronics <www.samsung.com>"
	};

	GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

	gst_element_class_add_pad_template(element_class,
		gst_static_pad_template_get (&srctemplate));
	gst_element_class_add_pad_template(element_class,
		gst_static_pad_template_get (&sinktemplate));

	gst_element_class_set_details(element_class, &element_details);
}

/* initialize the secrecord's class */
static void
gst_secrecord_class_init (GstsecrecordClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	GstBaseTransformClass *basetransform_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gstelement_class = GST_ELEMENT_CLASS (klass);
	basetransform_class = GST_BASE_TRANSFORM_CLASS(klass);

	gobject_class->set_property = GST_DEBUG_FUNCPTR(gst_secrecord_set_property);
	gobject_class->get_property = GST_DEBUG_FUNCPTR(gst_secrecord_get_property);

	gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_secrecord_change_state);

	g_object_class_install_property(gobject_class, PROP_FILTER_RECORD,
		g_param_spec_uint("record-filter", "record filter", "(0)off (1)on",
		FILTER_OFF, FILTER_ON, DEFAULT_FILTER_ACTION, G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_FILTER_SOUND_BOOSTER,
		g_param_spec_uint("sound-booster", "sound booster filter", "(0)off (1)on",
		FILTER_OFF, FILTER_ON, DEFAULT_FILTER_ACTION, G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_FILTER_NOISE,
		g_param_spec_uint("noise-reduction", "noise redutcion filter", "(0)off (1)on",
		FILTER_OFF, FILTER_ON, DEFAULT_FILTER_ACTION, G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_FILTER_BEAM_FORMING_INTERVIEW,
		g_param_spec_uint("beam-forming-interview", "beam forming interview filter", "(0)off (1)on",
		FILTER_OFF, FILTER_ON, DEFAULT_FILTER_ACTION, G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_FILTER_BEAM_FORMING_CONVERSATION,
		g_param_spec_uint("beam-forming-conversation", "beam forming conversation filter", "(0)off (1)on",
		FILTER_OFF, FILTER_ON, DEFAULT_FILTER_ACTION, G_PARAM_READWRITE));

	basetransform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_secrecord_transform_ip);
	basetransform_class->set_caps = GST_DEBUG_FUNCPTR(gst_secrecord_set_caps);

}

static void
gst_secrecord_init (Gstsecrecord *secrecord, GstsecrecordClass * klass)
{
	GST_WARNING_OBJECT(secrecord, "start");

	secrecord->rec_mode = DEFAULT_RECORD_MODE;
	secrecord->samplerate = DEFAULT_SAMPLE_RATE;
	secrecord->bf_interview_onoff = FALSE;
	secrecord->bf_interview_onoff = FALSE;

#ifdef TEMPORALLY
	secrecord->debug_frame_num = 0;
#endif

#ifdef SECRECORD_ENABLE_DUMP
	gst_secrecord_init_dump_config(secrecord);
#endif
	gst_secrecord_init_rec_config(secrecord);

	SamsungRecInit();

	GST_WARNING_OBJECT(secrecord, "done");
}

static GstFlowReturn
gst_secrecord_transform_ip (GstBaseTransform *base, GstBuffer *buf)
{
	Gstsecrecord *secrecord = GST_SECRECORD (base);
	int rec_ret = 0;
	guint sample_per_ch;
	gint sample_to_exe;
	gint exe_offset;
	gint exe_sample;
	gint exe_sample_threshold[RECORDMODE_NUM] = {0,
		SECRECORD_MONO_SAMPLE_THREASHOLD,SECRECORD_STEREO_SAMPLE_THREASHOLD};
	GstFlowReturn ret = GST_FLOW_OK;
	typedef int (*SamsungRecProcFun)(short,short,short,short,short,short,short*,short*);
	SamsungRecProcFun SamsungRecProc = NULL;
	gboolean normal_mode = !(secrecord->bf_interview_onoff || secrecord->bf_conversation_onoff);

#ifdef TEMPORALLY
	if(DEBUG(++secrecord->debug_frame_num)) {
		GST_WARNING("enter samsamung recording : Gstreamer timestamp%"GST_TIME_FORMAT,
			GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buf)));
	}
#endif

#ifdef SECRECORD_ENABLE_DUMP
	if (secrecord->dump_input_fp)
		fwrite(GST_BUFFER_DATA(buf), 1, GST_BUFFER_SIZE(buf), secrecord->dump_input_fp);
#endif

	if (secrecord->rec_onoff == FALSE
		|| (normal_mode && secrecord->sb_onoff == FALSE)) {
		goto done;
	}

	sample_per_ch = GST_BUFFER_SIZE(buf) / DEFAULT_SAMPLE_SIZE;

	if (secrecord->rec_mode == RECORDMODE_STEREO)
		sample_per_ch /= 2;

	GST_LOG_OBJECT(secrecord, "buffer size:%d, record mode:%d\n",
		GST_BUFFER_SIZE(buf), secrecord->rec_mode);

	SamsungRecProc = normal_mode ? SamsungRecExe : SamsungRecExeForInterview;

	exe_offset = 0;
	sample_to_exe = sample_per_ch;
	do {
		exe_sample = sample_to_exe > exe_sample_threshold[secrecord->rec_mode] ?
			exe_sample_threshold[secrecord->rec_mode] : sample_to_exe;

		SamsungRecConfigAll(secrecord->samplerate,
			exe_sample * 2,
			(short *)(GST_BUFFER_DATA(buf) + exe_offset),
			(short *)(GST_BUFFER_DATA(buf) + exe_offset),
			secrecord->rec_mode);

		rec_ret = SamsungRecProc(1, (short)secrecord->amr_pregain,
			(short)secrecord->aac_l_pregain, (short)secrecord->aac_r_pregain,
			(short)secrecord->rec_filter_onoff[REC_FILTER_01],
			(short)secrecord->rec_filter_onoff[REC_FILTER_02],
			(short *)secrecord->rec_filter_coef[REC_FILTER_01],
			(short *)secrecord->rec_filter_coef[REC_FILTER_02]);
		if (rec_ret < 0) {
			GST_ERROR("SamsungRecExe failed %d", rec_ret);
			ret = GST_FLOW_ERROR;
			goto done;
		}

		sample_to_exe -= exe_sample;
		exe_offset += exe_sample * DEFAULT_SAMPLE_SIZE * secrecord->rec_mode;
	} while(sample_to_exe > 0);

	if (secrecord->nrss_onoff) {
		GST_LOG_OBJECT(secrecord, "nb_sample %d channel %d", sample_per_ch, secrecord->rec_mode);

		DNSe_NRSS_EXE((short *)GST_BUFFER_DATA(buf), sample_per_ch, secrecord->rec_mode);
	}

#ifdef SECRECORD_ENABLE_DUMP
	if (secrecord->dump_output_fp)
		fwrite(GST_BUFFER_DATA(buf), 1, GST_BUFFER_SIZE(buf), secrecord->dump_output_fp);
#endif

done:
#ifdef TEMPORALLY
	if(DEBUG(secrecord->debug_frame_num)) {
		GST_WARNING("leave samsamung recording : Gstreamer timestamp%"GST_TIME_FORMAT,
			GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buf)));
	}
#endif
	return ret;
}

static gboolean
gst_secrecord_set_caps (GstBaseTransform * base, GstCaps * incaps,
	GstCaps * outcaps)
{
	Gstsecrecord *secrecord = GST_SECRECORD(base);
	GstStructure *ins;
	GstPad *pad;
	gint samplerate;
	gint channels;

	pad = gst_element_get_static_pad(GST_ELEMENT(secrecord), "src");

	/* forward-negotiate */
	if(!gst_pad_set_caps(pad, incaps)) {
		gst_object_unref(pad);
		return FALSE;
	}

	/* negotiation succeeded, so now configure ourselves */
	ins = gst_caps_get_structure(incaps, 0);

	/* get samplerate from caps & convert */
	gst_structure_get_int(ins, "rate", &samplerate);
	switch (samplerate) {
	case 48000:
		secrecord->samplerate = SAMPLERATE_48000Hz;
		break;
	case 44100:
		secrecord->samplerate = SAMPLERATE_44100Hz;
		break;
	case 32000:
		secrecord->samplerate = SAMPLERATE_32000Hz;
		break;
	case 24000:
		secrecord->samplerate = SAMPLERATE_24000Hz;
		break;
	case 22050:
		secrecord->samplerate = SAMPLERATE_22050Hz;
		break;
	case 16000:
		secrecord->samplerate = SAMPLERATE_16000Hz;
		break;
	case 8000:
		secrecord->samplerate = SAMPLERATE_8000Hz;
		break;
	default:
		if (samplerate < 8000) {
			secrecord->samplerate = SAMPLERATE_8000Hz;
		} else if (samplerate > 48000) {
			secrecord->samplerate = SAMPLERATE_48000Hz;
		}
		break;
	}
	/* get number of channels from caps */
	gst_structure_get_int(ins, "channels", &channels);
	secrecord->rec_mode = (channels == 1) ? RECORDMODE_MONO : RECORDMODE_STEREO;

	DNSe_NRSS_Init(DEFAULT_NRSS_MODE, samplerate);

	gst_object_unref (pad);

	return TRUE;
}


static void
gst_secrecord_set_property (GObject * object, guint prop_id,
	const GValue * value, GParamSpec * pspec)
{
	Gstsecrecord *secrecord = GST_SECRECORD(object);

	switch (prop_id) {
		case PROP_FILTER_RECORD:
			GST_INFO_OBJECT(secrecord, "request setting record filter:%d (current:%d)",
				g_value_get_uint(value), secrecord->rec_onoff);
			if (g_value_get_uint(value) == secrecord->rec_onoff) {
				GST_DEBUG_OBJECT(secrecord, "requested record filter variable is same as previous, so skip");
				break;
			}
			secrecord->rec_onoff = g_value_get_uint(value);
			break;
		case PROP_FILTER_SOUND_BOOSTER:
			GST_INFO_OBJECT(secrecord, "request setting sound booster filter:%d (current:%d)",
				g_value_get_uint(value), secrecord->sb_onoff);
			if (g_value_get_uint(value) == secrecord->sb_onoff) {
				GST_DEBUG_OBJECT(secrecord, "requested setting sound booster variable is same as previous, so skip");
				break;
			}
			secrecord->sb_onoff = g_value_get_uint(value);
			break;
		case PROP_FILTER_NOISE:
			GST_INFO_OBJECT(secrecord, "request setting noise reduction filter:%d (current:%d)",
				g_value_get_uint(value), secrecord->nrss_onoff);
			if (g_value_get_uint(value) == secrecord->nrss_onoff) {
				GST_DEBUG_OBJECT(secrecord, "requested noise reduction filter variable is same as previous, so skip");
				break;
			}
			secrecord->nrss_onoff = g_value_get_uint(value);
			break;
		case PROP_FILTER_BEAM_FORMING_INTERVIEW:
			GST_INFO_OBJECT(secrecord, "request setting beam forming interview filter:%d (current:%d)",
				g_value_get_uint(value), secrecord->bf_interview_onoff);
			if (g_value_get_uint(value) == secrecord->bf_interview_onoff) {
				GST_DEBUG_OBJECT(secrecord, "requested beam forming interview filter variable is same as previous, so skip");
				break;
			}
			secrecord->bf_interview_onoff = g_value_get_uint(value);
			break;
		case PROP_FILTER_BEAM_FORMING_CONVERSATION:
			GST_INFO_OBJECT(secrecord, "request setting beam forming conversation filter:%d (current:%d)",
				g_value_get_uint(value), secrecord->bf_conversation_onoff);
			if (g_value_get_uint(value) == secrecord->bf_conversation_onoff) {
				GST_DEBUG_OBJECT(secrecord, "requested beam forming conversation filter variable is same as previous, so skip");
				break;
			}
			secrecord->bf_conversation_onoff = g_value_get_uint(value);
			break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gst_secrecord_get_property (GObject * object, guint prop_id,
	GValue * value, GParamSpec * pspec)
{
	Gstsecrecord *secrecord = GST_SECRECORD(object);

	switch (prop_id) {
		case PROP_FILTER_RECORD:
			g_value_set_boolean(value, secrecord->rec_onoff);
			break;
		case PROP_FILTER_SOUND_BOOSTER:
			g_value_set_boolean(value, secrecord->sb_onoff);
			break;
		case PROP_FILTER_NOISE:
			g_value_set_boolean(value, secrecord->nrss_onoff);
			break;
		case PROP_FILTER_BEAM_FORMING_INTERVIEW:
			g_value_set_boolean(value, secrecord->bf_interview_onoff);
			break;
		case PROP_FILTER_BEAM_FORMING_CONVERSATION:
			g_value_set_boolean(value, secrecord->bf_conversation_onoff);
			break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


static gboolean
plugin_init (GstPlugin * plugin)
{
	GST_DEBUG_CATEGORY_INIT(gst_secrecord_debug, "secrecord", 0, "Samsung Recording Audio Filter plugin");
	return gst_element_register(plugin, "secrecord", GST_RANK_NONE, GST_TYPE_SECRECORD);
}


GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	"secrecord",
	"Samsung Recording Audio Filter plugin",
	plugin_init,
	VERSION,
	"Proprietary",
	"Samsung Electronics Co",
	"http://www.samsung.com")

