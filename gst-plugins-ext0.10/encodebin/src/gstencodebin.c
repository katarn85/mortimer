/*
 * GStreamer encodebin
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>
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
#include "config.h"
#endif

#include <string.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstencodebin.h"

#ifdef GST_EXT_PAD_LINK_UNCHECKED
#define _GST_ELEMENT_LINK_MANY 		gst_element_link_many_unchecked
#define _GST_ELEMENT_LINK 			gst_element_link_unchecked
#define _GST_PAD_LINK 				gst_pad_link_unchecked
#else
#define _GST_ELEMENT_LINK_MANY 		gst_element_link_many
#define _GST_ELEMENT_LINK 			gst_element_link
#define _GST_PAD_LINK 				gst_pad_link
#endif

#ifndef VIDEO_ENC_QUE_TIME
#define VIDEO_ENC_QUE_TIME		1
#endif
#ifndef AUDIO_ENC_QUE_TIME
#define AUDIO_ENC_QUE_TIME	1
#endif

//define USE_ENCODER_QUEUE_SET
#ifdef USE_ENCODER_QUEUE_SET
#define ENCODER_QUEUE_SET(x_queue, x_byte, x_buffer, x_time /*sec*/) \
{\
g_object_set(G_OBJECT(x_queue), \
			"max-size-bytes", (guint)x_byte, \
			"max-size-buffers", (guint)x_buffer, \ 	
			"max-size-time", (guint64)(x_time*GST_SECOND), \
			NULL); \
			GST_INFO("Set to [%s], max [%d] byte, max [%d] buffer, max [%d] time(sec) ", GST_OBJECT_NAME(x_queue), x_byte, x_buffer, x_time);\
}
#else
#define ENCODER_QUEUE_SET(x_queue, x_byte, x_buffer, x_time) 
#endif

#define _GST_PAD_LINK_UNREF(srcpad, sinkpad, if_fail_goto)\
{\
      GstPadLinkReturn ret = _GST_PAD_LINK(srcpad, sinkpad);\
      if(ret != GST_PAD_LINK_OK) { \
            GstObject *src_parent = gst_pad_get_parent(srcpad);\
            GstObject *sink_parent = gst_pad_get_parent(sinkpad);\
            char *src_name = NULL;\
            char *sink_name = NULL;\
            g_object_get((GObject *)src_parent, "name", &src_name, NULL);\
            g_object_get((GObject *)sink_parent, "name", &sink_name, NULL);\
            GST_ERROR("src[%s] - sink[%s] link failed", src_name, sink_name);\
            gst_object_unref(src_parent); src_parent = NULL;\
            gst_object_unref(sink_parent); sink_parent = NULL;\
            if (src_name) {\
                  free(src_name); src_name = NULL;\
            }\
            if (sink_name) {\
                  free(sink_name); sink_name = NULL;\
            }\
            gst_object_unref(srcpad); srcpad = NULL;\
            gst_object_unref(sinkpad); sinkpad = NULL;\
            goto if_fail_goto;\
      }\
      gst_object_unref(srcpad); srcpad = NULL;\
      gst_object_unref(sinkpad); sinkpad = NULL;\
}

#define _GST_PAD_UNLINK_UNREF(srcpad, sinkpad)\
{\
      gst_pad_unlink(srcpad, sinkpad);\
      gst_object_unref(srcpad); srcpad = NULL;\
      gst_object_unref(sinkpad); sinkpad = NULL;\
}

#define DEFAULT_PROP_PROFILE            0
#define DEFAULT_PROP_HIGH_SPEED         0
#define DEFAULT_PROP_VENC_NAME          "ffenc_h263"
#define DEFAULT_PROP_AENC_NAME          "secenc_amr"
#define DEFAULT_PROP_IENC_NAME          "jpegenc"
#define DEFAULT_PROP_MUX_NAME           "ffmux_3gp"
#define DEFAULT_PROP_VCONV_NAME         "ffmpegcolorspace"

/* props */
enum
{
	PROP_0,
	// encodebin mode :  a/v, audio only, stillshot
	PROP_PROFILE,
	//support slow motion capture
	PROP_HIGH_SPEED,
	//elements name
	PROP_VENC_NAME,
	PROP_AENC_NAME,
	PROP_IENC_NAME,
	PROP_MUX_NAME,
	PROP_VCONV_NAME,
	//caps
	PROP_VCAPS,
	PROP_ACAPS,
	PROP_ICAPS,
	//functions
	PROP_AUTO_AUDIO_CONVERT,
	PROP_AUTO_AUDIO_RESAMPLE,
	PROP_AUTO_COLORSPACE,
	PROP_BLOCK,
	PROP_PAUSE,
	PROP_VENC_QUEUE,
	PROP_AENC_QUEUE,
	//elements pointer
	PROP_VIDEO_ENC,
	PROP_AUDIO_ENC,
	PROP_IMAGE_ENC,
	PROP_MUX,
	PROP_VIDEO_CONV,
	//options
	PROP_USE_VIDEO_TOGGLE,
	PROP_USE_MUX,
};

#ifdef GST_ENCODE_BIN_SIGNAL_ENABLE
/* signals */
enum
{
  SIGNAL_STREAM_BLOCK,
  SIGNAL_STREAM_UNBLOCK,  	
  SIGNAL_STREAM_PAUSE,
  SIGNAL_STREAM_RESUME,  
  LAST_SIGNAL
};
#endif

typedef enum {
	ENCODEBIN_ELEMENT_VENC,
	ENCODEBIN_ELEMENT_AENC,
	ENCODEBIN_ELEMENT_IENC,
	ENCODEBIN_ELEMENT_MUX,
	ENCODEBIN_ELEMENT_VIDEO_CONV
}GstEncodeBinElement;

typedef enum {
	ENCODEBIN_MUX_AUDIO_SINK,
	ENCODEBIN_MUX_VIDEO_SINK,
}GstEncodeBinMuxSinkPad;


/* FIX ME */

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
# define ENDIANNESS   "LITTLE_ENDIAN, BIG_ENDIAN"
#else
# define ENDIANNESS   "BIG_ENDIAN, LITTLE_ENDIAN"
#endif

/* generic templates */
#define STATIC_AUDIO_CAPS \
GST_STATIC_CAPS ( \
  "audio/x-raw-float, " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, 8 ], " \
    "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, " \
    "width = (int) 64;" \
  "audio/x-raw-float, " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, 8 ], " \
    "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, " \
    "width = (int) 32;" \
  "audio/x-raw-int, " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, 8 ], " \
    "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, " \
    "width = (int) 32, " \
    "depth = (int) [ 1, 32 ], " \
    "signed = (boolean) { true, false }; " \
  "audio/x-raw-int, "   \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, 8 ], "       \
    "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, "        \
    "width = (int) 24, "        \
    "depth = (int) [ 1, 24 ], " "signed = (boolean) { true, false }; "  \
  "audio/x-raw-int, " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, 8 ], " \
    "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, " \
    "width = (int) 16, " \
    "depth = (int) [ 1, 16 ], " \
    "signed = (boolean) { true, false }; " \
  "audio/x-raw-int, " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, 8 ], " \
    "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, " \
    "width = (int) 8, " \
    "depth = (int) [ 1, 8 ], " \
    "signed = (boolean) { true, false } " \
)

#define STATIC_VIDEO_CAPS \
GST_STATIC_CAPS ( \
  "video/x-raw-yuv," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "format = (fourcc) I420;" \
  "video/x-raw-yuv," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "format = (fourcc) YV12;" \
  "video/x-raw-yuv," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "format = (fourcc) YUY2;" \
  "video/x-raw-rgb," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 24," \
    "depth = (int) 24," \
    "red_mask = (int) 16711680," \
    "green_mask = (int) 65280," \
    "blue_mask = (int) 255," \
    "endianness = (int) 4321;" \
  "video/x-raw-rgb," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 24," \
    "depth = (int) 24," \
    "red_mask = (int) 255," \
    "green_mask = (int) 65280," \
    "blue_mask = (int) 16711680," \
    "endianness = (int) 4321;" \
  "video/x-raw-yuv," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "format = (fourcc) Y42B;" \
    "video/x-raw-yuv," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "format = (fourcc) Y444;" \
  "video/x-raw-rgb," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 32," \
    "depth = (int) 32," \
    "red_mask = (int) 65280," \
    "green_mask = (int) 16711680," \
    "blue_mask = (int) -16777216," \
    "alpha_mask = (int) 255," \
    "endianness = (int) 4321;" \
  "video/x-raw-rgb," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 32," \
    "depth = (int) 32," \
    "red_mask = (int) 16711680," \
    "green_mask = (int) 65280," \
    "blue_mask = (int) 255," \
    "alpha_mask = (int) -16777216," \
    "endianness = (int) 4321;" \
  "video/x-raw-rgb," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 32," \
    "depth = (int) 32," \
    "red_mask = (int) 255," \
    "green_mask = (int) 65280," \
    "blue_mask = (int) 16711680," \
    "alpha_mask = (int) -16777216," \
    "endianness = (int) 4321;" \
  "video/x-raw-rgb," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 32," \
    "depth = (int) 32," \
    "red_mask = (int) -16777216," \
    "green_mask = (int) 16711680," \
    "blue_mask = (int) 65280," \
    "alpha_mask = (int) 255," \
    "endianness = (int) 4321;" \
  "video/x-raw-rgb," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 32," \
    "depth = (int) 24," \
    "red_mask = (int) 65280," \
    "green_mask = (int) 16711680," \
    "blue_mask = (int) -16777216," \
    "endianness = (int) 4321;" \
  "video/x-raw-rgb," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 32," \
    "depth = (int) 24," \
    "red_mask = (int) 255," \
    "green_mask = (int) 65280," \
    "blue_mask = (int) 16711680," \
    "endianness = (int) 4321;" \
  "video/x-raw-rgb," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 32," \
    "depth = (int) 24," \
    "red_mask = (int) 16711680," \
    "green_mask = (int) 65280," \
    "blue_mask = (int) 255," \
    "endianness = (int) 4321;" \
  "video/x-raw-rgb," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 32," \
    "depth = (int) 24," \
    "red_mask = (int) -16777216," \
    "green_mask = (int) 16711680," \
    "blue_mask = (int) 65280," \
    "endianness = (int) 4321;" \
  "video/x-raw-yuv," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "format = (fourcc) YUV9;" \
  "video/x-raw-yuv," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "format = (fourcc) YVU9;" \
  "video/x-raw-yuv," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "format = (fourcc) Y41B;" \
  "video/x-raw-rgb," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 16," \
    "depth = (int) 16," \
    "red_mask = (int) 63488," \
    "green_mask = (int) 2016," \
    "blue_mask = (int) 31," \
    "endianness = (int) 1234;" \
  "video/x-raw-rgb," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 16," \
    "depth = (int) 15," \
    "red_mask = (int) 31744," \
    "green_mask = (int) 992," \
    "blue_mask = (int) 31," \
    "endianness = (int) 1234;" \
  "video/x-raw-gray," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 8," \
    "depth = (int) 8;" \
  "video/x-raw-rgb," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "bpp = (int) 8," \
    "depth = (int) 8," \
    "endianness = (int) 1234;" \
  "video/x-raw-yuv," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "format = (fourcc) UYVY;" \
  "video/x-raw-yuv," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "format = (fourcc) IYU1;" \
  "video/x-raw-yuv," \
    "width = (int) [ 1, 2147483647 ]," \
    "height = (int) [ 1, 2147483647 ]," \
    "framerate = (fraction) [ 0/1, 2147483647/1 ]," \
    "format = (fourcc) AYUV " \
)	


static GstStaticPadTemplate encoder_bin_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate encoder_bin_video_sink_template =
GST_STATIC_PAD_TEMPLATE ("video",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    STATIC_VIDEO_CAPS
    );

static GstStaticPadTemplate encoder_bin_audio_sink_template =
GST_STATIC_PAD_TEMPLATE ("audio",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    STATIC_AUDIO_CAPS
    );

static GstStaticPadTemplate encoder_bin_image_sink_template =
GST_STATIC_PAD_TEMPLATE ("image",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("I420"))
    );

GST_DEBUG_CATEGORY_STATIC (gst_encode_bin_debug);
#define GST_CAT_DEFAULT gst_encode_bin_debug

static void gst_encode_bin_class_init (GstEncodeBinClass *klass);
static void gst_encode_bin_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_encode_bin_set_property (GObject * object,  guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_encode_bin_init (GstEncodeBin * encodebin);
static void gst_encode_bin_dispose (GObject * object);
static void gst_encode_bin_finalize (GObject * object);
static GstPad *gst_encode_bin_request_new_pad (GstElement * element, GstPadTemplate * templ, const gchar * name);

static GstStateChangeReturn gst_encode_bin_change_state (GstElement * element, GstStateChange transition);
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void gst_encode_bin_release_pad (GstElement * element, GstPad * pad);
static gint pad_compare_name (GstPad * pad1, const gchar * name);
static gboolean gst_encode_bin_add_element_by_name (GstEncodeBin *encodebin, GstEncodeBinElement type, const gchar *name);
#if 0 //disable unused function
static gboolean gst_encode_bin_change_profile(GstEncodeBin *encodebin, gboolean profile);
static void gst_encode_bin_replace_element (GstEncodeBin *encodebin, gint type, GstElement * newelement);
static gboolean gst_encode_bin_replace_element_by_name(GstEncodeBin *encodebin, GstEncodeBinElement type, const gchar *name);
static gboolean gst_encode_bin_replace_element_by_object(GstEncodeBin *encodebin, GstEncodeBinElement type, GstElement * element);
#endif //disable unused function
static gboolean gst_encode_bin_remove_element (GstEncodeBin *encodebin, GstElement * element);
static gboolean gst_encode_bin_link_elements (GstEncodeBin *encodebin);
static gboolean gst_encode_bin_unlink_elements (GstEncodeBin *encodebin);
static gboolean gst_encode_bin_init_video_elements (GstElement *element, gpointer user_data);
static gboolean gst_encode_bin_init_audio_elements (GstElement *element, gpointer user_data);
static gboolean gst_encode_bin_init_image_elements (GstElement *element, gpointer user_data);
static gboolean gst_encode_bin_block(GstEncodeBin *encodebin, gboolean value);
static gboolean gst_encode_bin_pause(GstEncodeBin *encodebin, gboolean value);
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static gboolean gst_encode_bin_release_pipeline (GstElement *element, gpointer user_data);
static gboolean gst_encode_bin_audsink_set_caps (GstPad * pad, GstCaps * vscaps);
static gboolean gst_encode_bin_vidsink_set_caps (GstPad * pad, GstCaps * vscaps);
static gboolean gst_encode_bin_imgsink_set_caps (GstPad * pad, GstCaps * vscaps);
static GstPad*	gst_encode_bin_get_mux_sink_pad(GstElement *mux, GstEncodeBinMuxSinkPad type);

//Data probe
static gboolean gst_encode_bin_video_probe(GstPad *pad, GstBuffer *buffer, GstEncodeBin *encodebin);
static gboolean gst_encode_bin_audio_probe(GstPad *pad, GstBuffer *buffer, GstEncodeBin *encodebin);
static gboolean gst_encode_bin_video_probe_hs(GstPad *pad, GstBuffer *buffer, GstEncodeBin *encodebin);

static GstElementClass *parent_class;

#ifdef GST_ENCODE_BIN_SIGNAL_ENABLE
static guint gst_encode_bin_signals[LAST_SIGNAL] = { 0 };
#endif

static const GstElementDetails gst_encode_bin_details =
GST_ELEMENT_DETAILS ("Samsung Electronics Co. Encoder Bin",
	"Generic/Bin/Encoder",
	"Autoplug and encode to muxed media",
	"Jeonghoon Park <jh1979.park@samsung.com>, Wonhyung Cho <wh01.cho@samsung.com>, Sangho Park <sangho.g.park@samsung.com>");

typedef enum {
	GST_ENCODE_BIN_PROFILE_AV,
	GST_ENCODE_BIN_PROFILE_AUDIO,
	GST_ENCODE_BIN_PROFILE_IMAGE,
} GstEncodeBinProfile;

GType
gst_encode_bin_profile_get_type (void)
{
	static GType encode_bin_profile_type = 0;
	static const GEnumValue profile_types[] = {
		{GST_ENCODE_BIN_PROFILE_AV, "Audio and Video Recording", "A/V"},
		{GST_ENCODE_BIN_PROFILE_AUDIO, "Audio Only Recording", "Audio"},
		{GST_ENCODE_BIN_PROFILE_IMAGE, "Image Stillshot", "Image"},
		{0, NULL, NULL}
  	};

	if (!encode_bin_profile_type) {
		encode_bin_profile_type =
		g_enum_register_static ("GstEncodeBinProfile",profile_types);
	}
	return encode_bin_profile_type;
}

GType
gst_encode_bin_get_type (void)
{
	static GType gst_encode_bin_type = 0;

	if (!gst_encode_bin_type) {
		static const GTypeInfo gst_encode_bin_info = {
			sizeof (GstEncodeBinClass),
			NULL,
			NULL,
			(GClassInitFunc) gst_encode_bin_class_init,
			NULL,
			NULL,
			sizeof (GstEncodeBin),
			0,
			(GInstanceInitFunc) gst_encode_bin_init,
			NULL
	};

	gst_encode_bin_type =
		g_type_register_static (GST_TYPE_BIN, "GstEncodeBin",
		&gst_encode_bin_info, 0);
  }

	return gst_encode_bin_type;
}

static void
queue_overun_cb (GstElement * queue, GstEncodeBin *encodebin)
{
#if 0
	guint queue_size = 0;
	guint queue_bufnum = 0;
//	guint64 queue_time= (guint64)0;

	GstClockTime now = gst_util_get_timestamp ();

	g_object_get(G_OBJECT(queue), "current-level-bytes", &queue_size, 
								"current-level-buffers", &queue_bufnum,
	//							"current-level-time", &queue_time,
								NULL);
       GST_ELEMENT_WARNING (encodebin, STREAM, TOO_LAZY,
           ("[%" GST_TIME_FORMAT "][%s], [%u b], [%u]", 
           GST_TIME_ARGS(now), GST_OBJECT_NAME(queue), queue_size, queue_bufnum), (NULL)); 
#else
       GST_ELEMENT_WARNING (encodebin, STREAM, TOO_LAZY,
           ("%s overrun", GST_OBJECT_NAME(queue)), (NULL)); 
#endif
}

static void
gst_encode_bin_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
	GstEncodeBin *encodebin;

	encodebin = GST_ENCODE_BIN (object);

	switch (prop_id) {
		case PROP_PROFILE:
			g_value_set_enum (value, encodebin->profile);
			break;
		case PROP_HIGH_SPEED:
			g_value_set_int (value, encodebin->high_speed_fps);
			break;
		//elements name
		case PROP_VENC_NAME:
			g_value_set_string (value, encodebin->venc_name);
			break;
		case PROP_AENC_NAME:
			g_value_set_string (value, encodebin->aenc_name);
			break;
		case PROP_IENC_NAME:
			g_value_set_string (value, encodebin->ienc_name);
			break; 
		case PROP_MUX_NAME:
			g_value_set_string (value, encodebin->mux_name);
			break;
		case PROP_VCONV_NAME:
			g_value_set_string (value, encodebin->vconv_name);
			break;
		//caps
		case PROP_VCAPS:
			gst_value_set_caps (value, encodebin->vcaps);
			break;
		case PROP_ACAPS:
			gst_value_set_caps (value, encodebin->acaps);
			break;
		case PROP_ICAPS:
			gst_value_set_caps (value, encodebin->icaps);
			break;
		//functions
		case PROP_AUTO_AUDIO_CONVERT:
			g_value_set_boolean (value, encodebin->auto_audio_convert);
			break;
		case PROP_AUTO_AUDIO_RESAMPLE:
			g_value_set_boolean (value, encodebin->auto_audio_resample);
			break;
		case PROP_AUTO_COLORSPACE:
			g_value_set_boolean (value, encodebin->auto_color_space);
			break;
		case PROP_BLOCK:
			g_value_set_boolean (value, encodebin->block);
			break;
		case PROP_PAUSE:
			g_value_set_boolean (value, encodebin->pause);
			break;
		case PROP_VENC_QUEUE:
//			g_value_set_boolean (value, encodebin->use_venc_queue);
			if((encodebin->video_encode_queue == NULL) && (encodebin->profile == GST_ENCODE_BIN_PROFILE_AV)) {
				encodebin->video_encode_queue = gst_element_factory_make ("queue", "video_encode_queue");
				if(encodebin->video_encode_queue != NULL) 
					gst_bin_add(GST_BIN(encodebin), encodebin->video_encode_queue);
			}
			g_value_set_object (value, encodebin->video_encode_queue);		
			break;		
		case PROP_AENC_QUEUE:
//			g_value_set_boolean (value, encodebin->use_aenc_queue);
			if((encodebin->audio_encode_queue == NULL) && (encodebin->profile <= GST_ENCODE_BIN_PROFILE_AUDIO)) {
				encodebin->audio_encode_queue = gst_element_factory_make ("queue", "audio_encode_queue");
				if(encodebin->audio_encode_queue != NULL)
					gst_bin_add(GST_BIN(encodebin), encodebin->audio_encode_queue);
			}
			g_value_set_object (value, encodebin->audio_encode_queue);
			break;
		//elements pointer
		case PROP_VIDEO_ENC:
			if((encodebin->video_encode == NULL) && (encodebin->profile == GST_ENCODE_BIN_PROFILE_AV)) {
				encodebin->video_encode = gst_element_factory_make (encodebin->venc_name, NULL);
				if(encodebin->video_encode != NULL)
					gst_bin_add(GST_BIN(encodebin), encodebin->video_encode);
			}
			g_value_set_object (value, encodebin->video_encode);
			break;
		case PROP_AUDIO_ENC:
			if(encodebin->audio_encode == NULL && (encodebin->profile <= GST_ENCODE_BIN_PROFILE_AUDIO)) {
				encodebin->audio_encode = gst_element_factory_make (encodebin->aenc_name, "audio_encode");
				if(encodebin->audio_encode != NULL)
					gst_bin_add(GST_BIN(encodebin), encodebin->audio_encode);
			}
			g_value_set_object (value, encodebin->audio_encode);
			break;
		case PROP_IMAGE_ENC:
			if(encodebin->image_encode == NULL && (encodebin->profile == GST_ENCODE_BIN_PROFILE_IMAGE)) {
				encodebin->image_encode = gst_element_factory_make (encodebin->ienc_name, "image_encode");
				if(encodebin->image_encode != NULL)
					gst_bin_add(GST_BIN(encodebin), encodebin->image_encode);
			}
			g_value_set_object (value, encodebin->image_encode);
			break;  
		case PROP_MUX:
			if(encodebin->mux == NULL && (encodebin->profile <= GST_ENCODE_BIN_PROFILE_AUDIO)) {
				encodebin->mux = gst_element_factory_make (encodebin->mux_name, "mux");
				if(encodebin->mux != NULL)
					gst_bin_add(GST_BIN(encodebin), encodebin->mux);
			}
			g_value_set_object (value, encodebin->mux);
			break;
		case PROP_VIDEO_CONV:
			if(encodebin->color_space == NULL && (encodebin->profile != GST_ENCODE_BIN_PROFILE_AUDIO)) {
				encodebin->color_space = gst_element_factory_make (encodebin->vconv_name, "video_convert");
				if(encodebin->color_space != NULL)
					gst_bin_add(GST_BIN(encodebin), encodebin->color_space);
			}
			g_value_set_object (value, encodebin->color_space);
			break;
		case PROP_USE_VIDEO_TOGGLE:
			g_value_set_boolean( value, encodebin->use_video_toggle );
			break;
		case PROP_USE_MUX:
			g_value_set_boolean (value, encodebin->enable_mux);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gst_encode_bin_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
	GstEncodeBin *encodebin;

	encodebin = GST_ENCODE_BIN (object);

	switch (prop_id) {
		case PROP_PROFILE:
			encodebin->profile = g_value_get_enum (value);
			/*
			gboolean newprofile = g_value_get_enum (value);
			if(encodebin->profile != newprofile) {
			  gst_encode_bin_change_profile(encodebin, newprofile);
			encodebin->profile = newprofile;
				}      
			*/
			break;
		case PROP_HIGH_SPEED:
			encodebin->high_speed_fps = g_value_get_int (value);
			break;
		case PROP_VENC_NAME: {
			const gchar  *new_name;
			if(encodebin->profile > GST_ENCODE_BIN_PROFILE_AV) {
				GST_WARNING_OBJECT(encodebin, "Profile isn't match, change profile first!");
				break;
			}
			new_name = g_value_get_string (value);

			if(encodebin->video_encode == NULL) {
				if(gst_encode_bin_add_element_by_name(encodebin, ENCODEBIN_ELEMENT_VENC, new_name))
					encodebin->venc_name = g_strdup (new_name);
			} else {
				if(strcmp (encodebin->venc_name, new_name)) {
					gst_encode_bin_remove_element(encodebin, encodebin->video_encode);
					if(gst_encode_bin_add_element_by_name(encodebin, ENCODEBIN_ELEMENT_VENC, new_name))
						encodebin->venc_name = g_strdup (new_name);
				}
			}
			break;
		}  
		case PROP_AENC_NAME: {
			const gchar  *new_name;
			if(encodebin->profile > GST_ENCODE_BIN_PROFILE_AUDIO) {
				GST_WARNING_OBJECT(encodebin, "Profile isn't match, change profile first!");
				break;
			}
			new_name = g_value_get_string (value);

			if(encodebin->audio_encode == NULL) {
				if(gst_encode_bin_add_element_by_name(encodebin, ENCODEBIN_ELEMENT_AENC, new_name))		
					encodebin->aenc_name = g_strdup (new_name);
			} else {
				if(strcmp (encodebin->aenc_name, new_name)) {
					gst_encode_bin_remove_element(encodebin, encodebin->audio_encode);
					if(gst_encode_bin_add_element_by_name(encodebin, ENCODEBIN_ELEMENT_AENC, new_name))		
						encodebin->aenc_name = g_strdup (new_name);
				}
			}
			break;
		}
		case PROP_IENC_NAME: {
			const gchar  *new_name;
			if(encodebin->profile < GST_ENCODE_BIN_PROFILE_IMAGE) {
				GST_WARNING_OBJECT(encodebin, "Profile isn't match, change profile first!");
				break;
			}
			new_name = g_value_get_string (value);

			if(encodebin->image_encode == NULL) {
				if(gst_encode_bin_add_element_by_name(encodebin, ENCODEBIN_ELEMENT_IENC, new_name))		
					encodebin->ienc_name = g_strdup (new_name);
			} else {
				if(strcmp (encodebin->ienc_name, new_name)) {
					gst_encode_bin_remove_element(encodebin, encodebin->image_encode);
					if(gst_encode_bin_add_element_by_name(encodebin, ENCODEBIN_ELEMENT_IENC, new_name))		
						encodebin->ienc_name = g_strdup (new_name);
				}
			}
			break; 
		}
		case PROP_MUX_NAME: {
			const gchar  *new_name;
			if(encodebin->profile > GST_ENCODE_BIN_PROFILE_AUDIO) {
				GST_WARNING_OBJECT(encodebin, "Profile isn't match");
				break;
			}
			new_name = g_value_get_string (value);

			if(encodebin->mux == NULL) {
				if(gst_encode_bin_add_element_by_name(encodebin, ENCODEBIN_ELEMENT_MUX, new_name))
					encodebin->mux_name = g_strdup (new_name);
			} else {
				if(strcmp (encodebin->mux_name, new_name)) {
					gst_encode_bin_remove_element(encodebin, encodebin->mux);
					if(gst_encode_bin_add_element_by_name(encodebin, ENCODEBIN_ELEMENT_MUX, new_name))
						encodebin->mux_name = g_strdup (new_name);
				}
			}
			break;
		}
		case PROP_VCONV_NAME: {
			const gchar  *new_name;
			if (encodebin->profile == GST_ENCODE_BIN_PROFILE_AUDIO) {
				GST_WARNING_OBJECT(encodebin, "Profile isn't match");
				break;
			}
			new_name = g_value_get_string(value);

			if (encodebin->color_space == NULL) {
				if(gst_encode_bin_add_element_by_name(encodebin, ENCODEBIN_ELEMENT_VIDEO_CONV, new_name))
					encodebin->vconv_name = g_strdup (new_name);
			} else {
				if(strcmp (encodebin->vconv_name, new_name)) {
					gst_encode_bin_remove_element(encodebin, encodebin->color_space);
					if(gst_encode_bin_add_element_by_name(encodebin, ENCODEBIN_ELEMENT_VIDEO_CONV, new_name))
						encodebin->vconv_name = g_strdup (new_name);
				}
			}
			break;
		}
		//caps
		case PROP_VCAPS: {
			GstCaps *new_caps;
			GstCaps *old_caps;
			const GstCaps *new_caps_val = gst_value_get_caps (value);

			if (new_caps_val == NULL) {
				new_caps = gst_caps_new_any ();
			} else {
				new_caps = (GstCaps *) new_caps_val;
				gst_caps_ref (new_caps);
			}

			old_caps = encodebin->vcaps;
			encodebin->vcaps = new_caps;
			gst_caps_unref (old_caps);
			break;
		}
		case PROP_ACAPS: {
			GstCaps *new_caps;
			GstCaps *old_caps;
			const GstCaps *new_caps_val = gst_value_get_caps (value);

			if (new_caps_val == NULL) {
				new_caps = gst_caps_new_any ();
			} else {
				new_caps = (GstCaps *) new_caps_val;
				gst_caps_ref (new_caps);
			}

			old_caps = encodebin->acaps;
			encodebin->acaps = new_caps;
			gst_caps_unref (old_caps);
			break;
		}
		case PROP_ICAPS: {
			GstCaps *new_caps;
			GstCaps *old_caps;
			const GstCaps *new_caps_val = gst_value_get_caps (value);

			if (new_caps_val == NULL) {
				new_caps = gst_caps_new_any ();
			} else {
				new_caps = (GstCaps *) new_caps_val;
				gst_caps_ref (new_caps);
			}

			old_caps = encodebin->icaps;
			encodebin->icaps = new_caps;
			gst_caps_unref (old_caps);
			break;
		}
		//functions
		case PROP_AUTO_AUDIO_CONVERT:
		   encodebin->auto_audio_convert = g_value_get_boolean (value);
		  break;
		case PROP_AUTO_AUDIO_RESAMPLE:
		  encodebin->auto_audio_resample = g_value_get_boolean (value);
		  break;
		case PROP_AUTO_COLORSPACE:
		  encodebin->auto_color_space = g_value_get_boolean (value);
		  break;
		case PROP_BLOCK: {
			gboolean newval = g_value_get_boolean (value);
			if(encodebin->block != newval) {
				if(!gst_encode_bin_block(encodebin, newval)) {
#ifdef GST_ENCODE_BIN_SIGNAL_ENABLE		
					if(newval) {
						g_signal_emit (G_OBJECT (encodebin), gst_encode_bin_signals[SIGNAL_STREAM_BLOCK], 0, FALSE);
					} else {
						g_signal_emit (G_OBJECT (encodebin), gst_encode_bin_signals[SIGNAL_STREAM_UNBLOCK], 0, FALSE);
					}
#endif					
					break;
				}				
			}
#ifdef GST_ENCODE_BIN_SIGNAL_ENABLE		
			if(newval) {
				g_signal_emit (G_OBJECT (encodebin), gst_encode_bin_signals[SIGNAL_STREAM_BLOCK], 0, TRUE);
			} else {
				g_signal_emit (G_OBJECT (encodebin), gst_encode_bin_signals[SIGNAL_STREAM_UNBLOCK], 0, TRUE);
			}
#endif			
			break;
		}
		case PROP_PAUSE: {
			gboolean newval = g_value_get_boolean (value);
			if(encodebin->pause != newval) {
				if(!gst_encode_bin_pause(encodebin, newval))
					break;
			}
#ifdef GST_ENCODE_BIN_SIGNAL_ENABLE		
			if(newval) {
				g_signal_emit (G_OBJECT (encodebin), gst_encode_bin_signals[SIGNAL_STREAM_PAUSE], 0, TRUE);
			} else {
				g_signal_emit (G_OBJECT (encodebin), gst_encode_bin_signals[SIGNAL_STREAM_RESUME], 0, TRUE);
			}
#endif			
			break;
		}
		case PROP_VENC_QUEUE:
//		  encodebin->use_venc_queue = g_value_get_boolean (value);
		{
			GstElement *newelement = g_value_get_object (value);
			if(encodebin->profile > GST_ENCODE_BIN_PROFILE_AV) {
				GST_WARNING_OBJECT(encodebin, "Profile isn't match, change profile first!");
				break;
			}
			if(newelement != NULL) {
				gst_encode_bin_remove_element(encodebin, encodebin->video_encode_queue);
				encodebin->video_encode_queue = newelement;
				gst_object_ref (encodebin->video_encode_queue);
				gst_object_sink (GST_OBJECT_CAST (encodebin->video_encode_queue)); // take ownership ??
				gst_bin_add(GST_BIN(encodebin), encodebin->video_encode_queue);
			}
			break;
		}
		  break;		
		case PROP_AENC_QUEUE:
//		  encodebin->use_aenc_queue = g_value_get_boolean (value);
		{
			GstElement *newelement = g_value_get_object (value);
			if(encodebin->profile > GST_ENCODE_BIN_PROFILE_AUDIO) {
				GST_WARNING_OBJECT(encodebin, "Profile isn't match, change profile first!");
				break;
			}
			if(newelement != NULL) {
				gst_encode_bin_remove_element(encodebin, encodebin->audio_encode_queue);
				encodebin->audio_encode_queue = newelement;
				gst_object_ref (encodebin->audio_encode_queue);
				gst_object_sink (GST_OBJECT_CAST (encodebin->audio_encode_queue));
				gst_bin_add(GST_BIN(encodebin), encodebin->audio_encode_queue);
			}
			break;
		}
		  break;		  
		case PROP_VIDEO_ENC: 
		{
			GstElement *newelement = g_value_get_object (value);
			if(encodebin->profile > GST_ENCODE_BIN_PROFILE_AV) {
				GST_WARNING_OBJECT(encodebin, "Profile isn't match, change profile first!");
				break;
			}
			if(newelement != NULL) {
				gst_encode_bin_remove_element(encodebin, encodebin->video_encode);
				encodebin->video_encode = newelement;
				gst_object_ref (encodebin->video_encode);
				gst_object_sink (GST_OBJECT_CAST (encodebin->video_encode)); // take ownership ??
				gst_bin_add(GST_BIN(encodebin), encodebin->video_encode);
			}
			break;
		}
		case PROP_AUDIO_ENC: 
		{
			GstElement *newelement = g_value_get_object (value);
			if(encodebin->profile > GST_ENCODE_BIN_PROFILE_AUDIO) {
				GST_WARNING_OBJECT(encodebin, "Profile isn't match, change profile first!");
				break;
			}
			if(newelement != NULL) {
				gst_encode_bin_remove_element(encodebin, encodebin->audio_encode);
				encodebin->audio_encode = newelement;
				gst_object_ref (encodebin->audio_encode);
				gst_object_sink (GST_OBJECT_CAST (encodebin->audio_encode));
				gst_bin_add(GST_BIN(encodebin), encodebin->audio_encode);
			}
			break;
		}
		case PROP_IMAGE_ENC: {
			GstElement *newelement = g_value_get_object (value);
			if(encodebin->profile < GST_ENCODE_BIN_PROFILE_IMAGE) {
				GST_WARNING_OBJECT(encodebin, "Profile isn't match, change profile first!");
				break;
			}
			if(newelement != NULL) {
				gst_encode_bin_remove_element(encodebin, encodebin->image_encode);
				encodebin->image_encode = newelement;
				gst_object_ref (encodebin->image_encode);
				gst_object_sink (GST_OBJECT_CAST (encodebin->image_encode));
				gst_bin_add(GST_BIN(encodebin), encodebin->image_encode);
			}
			break;
		}
		case PROP_MUX: {
			GstElement *newelement = g_value_get_object (value);
			if(encodebin->profile > GST_ENCODE_BIN_PROFILE_AUDIO) {
				GST_WARNING_OBJECT(encodebin, "Profile isn't match, change profile first!");
				break;
			}
			if(newelement != NULL) {
				gst_encode_bin_remove_element(encodebin, encodebin->mux);
				encodebin->mux = newelement;
				gst_object_ref (encodebin->mux);
				gst_object_sink (GST_OBJECT_CAST (encodebin->mux));
				gst_bin_add(GST_BIN(encodebin), encodebin->mux);
			}
			break;
		}
		case PROP_VIDEO_CONV: {
			GstElement *newelement = g_value_get_object (value);
			if(encodebin->profile == GST_ENCODE_BIN_PROFILE_AUDIO) {
				GST_WARNING_OBJECT(encodebin, "Profile isn't match, change profile first!");
				break;
			}
			if(newelement != NULL) {
				gst_encode_bin_remove_element(encodebin, encodebin->color_space);
				encodebin->color_space = newelement;
				gst_object_ref (encodebin->color_space);
				gst_object_sink (GST_OBJECT_CAST (encodebin->color_space));
				gst_bin_add(GST_BIN(encodebin), encodebin->color_space);
			}
			break;
		}
		case PROP_USE_VIDEO_TOGGLE:
			encodebin->use_video_toggle = g_value_get_boolean( value );
			break;
		case PROP_USE_MUX:
			encodebin->enable_mux = g_value_get_boolean( value );
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static GstPad *
gst_encode_bin_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name)
{
	GstEncodeBin *encodebin = NULL;
	GstElementClass *klass = GST_ELEMENT_GET_CLASS (element);
	GstPad *pad = NULL;

	g_return_val_if_fail (templ != NULL, NULL);

	if (templ->direction != GST_PAD_SINK) {
		GST_WARNING_OBJECT (GST_IS_ENCODE_BIN (element), "encodebin: request pad that is not a SINK pad\n");
		return NULL;
	}

	g_return_val_if_fail (GST_IS_ENCODE_BIN (element), NULL);

	encodebin = GST_ENCODE_BIN (element);
  
 /* FIXME */
	if (templ == gst_element_class_get_pad_template (klass, "audio")) {
		if (encodebin->profile <= GST_ENCODE_BIN_PROFILE_AUDIO) {
			gst_encode_bin_init_audio_elements(element, NULL); //??
			
			if(encodebin->audio_sinkpad == NULL)
			{
				pad = gst_element_get_static_pad (encodebin->audio_queue, "sink");
				encodebin->audio_sinkpad = gst_ghost_pad_new ("audio", pad);
				gst_object_unref(pad);
				pad = NULL;
			}
			else
			{
				GST_WARNING_OBJECT (GST_IS_ENCODE_BIN (element), "encodebin: audio pad is aleady existed, return existing audio pad\n");			
				return encodebin->audio_sinkpad;
			}
			
			gst_element_add_pad (element, encodebin->audio_sinkpad);
			gst_pad_set_setcaps_function (encodebin->audio_sinkpad,
				GST_DEBUG_FUNCPTR (gst_encode_bin_audsink_set_caps));
			return encodebin->audio_sinkpad;
		} else
			return NULL;
	} else if (templ == gst_element_class_get_pad_template (klass, "video")) {
		if (encodebin->profile == GST_ENCODE_BIN_PROFILE_AV) {
			gst_encode_bin_init_video_elements(element, NULL); //??

			if(encodebin->video_sinkpad == NULL)
			{
				pad = gst_element_get_static_pad (encodebin->video_queue, "sink");
				encodebin->video_sinkpad = gst_ghost_pad_new ("video", pad);	    
				gst_object_unref(pad);
				pad = NULL;
			}
			else
			{
				GST_WARNING_OBJECT (GST_IS_ENCODE_BIN (element), "encodebin: video pad is aleady existed, return existing video pad\n");						
				return encodebin->video_sinkpad;
			}
			
			gst_element_add_pad (element, encodebin->video_sinkpad);
			gst_pad_set_setcaps_function (encodebin->video_sinkpad,
			GST_DEBUG_FUNCPTR (gst_encode_bin_vidsink_set_caps));
			return encodebin->video_sinkpad;
		} else if (encodebin->profile == GST_ENCODE_BIN_PROFILE_IMAGE) {
			gst_encode_bin_init_image_elements(element, NULL); //??

			if(encodebin->image_sinkpad == NULL)
			{
				pad = gst_element_get_static_pad (encodebin->image_queue, "sink");
				encodebin->image_sinkpad = gst_ghost_pad_new ("image", pad);
				gst_object_unref(pad);
				pad = NULL;				
			}
			else
			{
				GST_WARNING_OBJECT (GST_IS_ENCODE_BIN (element), "encodebin: image pad is aleady existed, return existing image pad\n");			
				return encodebin->image_sinkpad;
			}
			
			gst_element_add_pad (element, encodebin->image_sinkpad);
			gst_pad_set_setcaps_function (encodebin->image_sinkpad,
			GST_DEBUG_FUNCPTR (gst_encode_bin_imgsink_set_caps));
			return encodebin->image_sinkpad;
		} else
			return NULL;
	} else {
		if (encodebin->profile == GST_ENCODE_BIN_PROFILE_IMAGE) {
			gst_encode_bin_init_image_elements(element, NULL); //??
			
			if(encodebin->image_sinkpad == NULL)
			{
				pad = gst_element_get_static_pad (encodebin->image_queue, "sink");
				encodebin->image_sinkpad = gst_ghost_pad_new ("image", pad);
				gst_object_unref(pad);
				pad = NULL;				
			}
			else
			{
				GST_WARNING_OBJECT (GST_IS_ENCODE_BIN (element), "encodebin: image pad is aleady existed, return existing image pad\n");			
				return encodebin->image_sinkpad;
			}
			
			gst_element_add_pad (element, encodebin->image_sinkpad);
			gst_pad_set_setcaps_function (encodebin->image_sinkpad,
			GST_DEBUG_FUNCPTR (gst_encode_bin_imgsink_set_caps));
			return encodebin->image_sinkpad;
		} else
			return NULL;        
	}  
}

static void
gst_encode_bin_class_init (GstEncodeBinClass *klass)
{
	GObjectClass *gobject_klass;
	GstElementClass *gstelement_klass;
	GstBinClass *gstbin_klass;

	gobject_klass = (GObjectClass *) klass;
	gstelement_klass = (GstElementClass *) klass;
	gstbin_klass = (GstBinClass *) klass;
	
	parent_class = g_type_class_peek_parent (klass);

	gobject_klass->get_property = gst_encode_bin_get_property;
	gobject_klass->set_property = gst_encode_bin_set_property;
	gobject_klass->dispose = GST_DEBUG_FUNCPTR (gst_encode_bin_dispose);	
	gobject_klass->finalize = GST_DEBUG_FUNCPTR (gst_encode_bin_finalize);


	g_object_class_install_property (gobject_klass, PROP_PROFILE,
	  g_param_spec_enum ("profile", "PROFILE", "Profile of the media to record",
	      GST_TYPE_ENCODE_BIN_PROFILE, DEFAULT_PROP_PROFILE,
	      G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_HIGH_SPEED,
	  g_param_spec_int ("high-speed-fps", "high speed rec. fps", "framerate for high speed recording", 0, G_MAXINT, 
		DEFAULT_PROP_HIGH_SPEED, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_VENC_NAME,
	  g_param_spec_string ("venc-name", "video encoder name", "the name of video encoder to use",
	      DEFAULT_PROP_VENC_NAME, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_AENC_NAME,
	  g_param_spec_string ("aenc-name", "audio encoder name", "the name of audio encoder to use",
	      DEFAULT_PROP_AENC_NAME, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_IENC_NAME,
	  g_param_spec_string ("ienc-name", "image encoder name", "the name of image encoder to use",
	      DEFAULT_PROP_IENC_NAME, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_MUX_NAME,
	  g_param_spec_string ("mux-name", "muxer name", "the name of muxer to use",
	      DEFAULT_PROP_MUX_NAME, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_VCONV_NAME,
	  g_param_spec_string ("vconv-name", "Video converter name", "the name of video color converter to use",
	      DEFAULT_PROP_VCONV_NAME, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_VCAPS,
	  g_param_spec_boxed ("vcaps", "caps for video","caps for video recording",
	      GST_TYPE_CAPS, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_ACAPS,
	  g_param_spec_boxed ("acaps", "caps for audio","caps for audio recording",
	      GST_TYPE_CAPS, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_ICAPS,
	  g_param_spec_boxed ("icaps", "caps for image","caps for image stillshot",
	      GST_TYPE_CAPS, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_AUTO_AUDIO_CONVERT,
	  g_param_spec_boolean ("auto-audio-convert", "auto audio convert",
	      "Support for auto audio convert", TRUE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_AUTO_AUDIO_RESAMPLE,
	  g_param_spec_boolean ("auto-audio-resample", "auto audio resample",
	      "Support for auto audio resample", TRUE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_AUTO_COLORSPACE,
	  g_param_spec_boolean ("auto-colorspace", "auto colorspace",
	      "Support for auto colorspace", TRUE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_BLOCK,
	  g_param_spec_boolean ("block", "stream block",
	      "Support for stream block", FALSE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_PAUSE,
	  g_param_spec_boolean ("runtime-pause", "recording pause",
	      "Support for recording pause/resume", FALSE, G_PARAM_READWRITE));

#if 0
	g_object_class_install_property (gobject_klass, PROP_VENC_QUEUE,
	  g_param_spec_boolean ("use-venc-queue", "use queue between venc and mux",
	      "add queue between venc and mux(only for custom optimization)", FALSE, G_PARAM_READWRITE));	

	g_object_class_install_property (gobject_klass, PROP_AENC_QUEUE,
	  g_param_spec_boolean ("use-aenc-queue", "use queue between aenc and mux",
	      "add queue between aenc and mux(only for custom optimization)", FALSE, G_PARAM_READWRITE));		
#else
	g_object_class_install_property (gobject_klass, PROP_VENC_QUEUE,
	  g_param_spec_object ("use-venc-queue", "Video Encoder queue",
	      "add queue between venc and mux(only for custom optimization)",
	      GST_TYPE_ELEMENT, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_AENC_QUEUE,
	  g_param_spec_object ("use-aenc-queue", "Audio Encoder queue",
	      "add queue between aenc and mux(only for custom optimization)",
	      GST_TYPE_ELEMENT, G_PARAM_READWRITE));
#endif

	g_object_class_install_property (gobject_klass, PROP_VIDEO_ENC,
	  g_param_spec_object ("video-encode", "Video Encoder",
	      "the video encoder element to use",
	      GST_TYPE_ELEMENT, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_AUDIO_ENC,
	  g_param_spec_object ("audio-encode", "Audio Encoder",
	      "the audio encoder element to use",
	      GST_TYPE_ELEMENT, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_IMAGE_ENC,
	  g_param_spec_object ("image-encode", "Image Encoder",
	      "the Image encoder element to use",
	      GST_TYPE_ELEMENT, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_MUX,
	  g_param_spec_object ("mux", "Muxer",
	      "the muxer element to use",
	      GST_TYPE_ELEMENT, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_VIDEO_CONV,
	  g_param_spec_object ("video-convert", "Video converter",
	      "the video converter element to use",
	      GST_TYPE_ELEMENT, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_USE_VIDEO_TOGGLE,
		g_param_spec_boolean ("use-video-toggle", "Use video toggle",
		"Use video toggle while AV recording", TRUE, G_PARAM_READWRITE));

	g_object_class_install_property (gobject_klass, PROP_USE_MUX,
		g_param_spec_boolean ("use-mux", "Use mux",
		"Use mux", TRUE, G_PARAM_READWRITE));

#ifdef GST_ENCODE_BIN_SIGNAL_ENABLE
	gst_encode_bin_signals[SIGNAL_STREAM_BLOCK] =
		g_signal_new ("stream-block", G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstEncodeBinClass, stream_block), 
		NULL, NULL, gst_marshal_VOID__BOOLEAN, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

	gst_encode_bin_signals[SIGNAL_STREAM_UNBLOCK] =
		g_signal_new ("stream-unblock", G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstEncodeBinClass, stream_unblock), 
		NULL, NULL, gst_marshal_VOID__BOOLEAN, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

	gst_encode_bin_signals[SIGNAL_STREAM_PAUSE] =
		g_signal_new ("stream-pause", G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstEncodeBinClass, stream_pause), 
		NULL, NULL, gst_marshal_VOID__BOOLEAN, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

	gst_encode_bin_signals[SIGNAL_STREAM_RESUME] =
		g_signal_new ("stream-resume", G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstEncodeBinClass, stream_resume), 
		NULL, NULL, gst_marshal_VOID__BOOLEAN, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);	
#endif

	gst_element_class_add_pad_template (gstelement_klass,
	  gst_static_pad_template_get (&encoder_bin_src_template));
	gst_element_class_add_pad_template (gstelement_klass,
	  gst_static_pad_template_get (&encoder_bin_audio_sink_template));
	gst_element_class_add_pad_template (gstelement_klass,
	  gst_static_pad_template_get (&encoder_bin_video_sink_template));
	gst_element_class_add_pad_template (gstelement_klass,
	  gst_static_pad_template_get (&encoder_bin_image_sink_template));

	gst_element_class_set_details (gstelement_klass, &gst_encode_bin_details);

	gstelement_klass->request_new_pad =
	  GST_DEBUG_FUNCPTR (gst_encode_bin_request_new_pad);
	gstelement_klass->release_pad = 
	  GST_DEBUG_FUNCPTR (gst_encode_bin_release_pad);
	gstelement_klass->change_state =
	  GST_DEBUG_FUNCPTR (gst_encode_bin_change_state);
}

static void
gst_encode_bin_init (GstEncodeBin *encodebin)
{
	encodebin->mutex = g_mutex_new();

	if (encodebin->srcpad == NULL) {
		encodebin->srcpad = gst_ghost_pad_new_no_target ("src", GST_PAD_SRC);
		gst_element_add_pad (GST_ELEMENT(encodebin), encodebin->srcpad);
	}

	encodebin->video_sinkpad = NULL;
	encodebin->audio_sinkpad = NULL;
	encodebin->image_sinkpad = NULL;
	encodebin->mux_audio_sinkpad = NULL;
	encodebin->mux_video_sinkpad = NULL;
	
	encodebin->profile = DEFAULT_PROP_PROFILE;
	encodebin->fps = 0;
	encodebin->high_speed_fps = DEFAULT_PROP_HIGH_SPEED;
	encodebin->multiple = 1;

	encodebin->auto_audio_convert = TRUE;
	encodebin->auto_audio_resample = TRUE;
	encodebin->auto_color_space = TRUE;
	encodebin->block = FALSE;
	encodebin->pause= FALSE;
	encodebin->use_video_toggle = TRUE;
	encodebin->use_venc_queue= FALSE;
	encodebin->use_aenc_queue= FALSE;

	encodebin->venc_name = g_strdup(DEFAULT_PROP_VENC_NAME);
	encodebin->aenc_name = g_strdup(DEFAULT_PROP_AENC_NAME);
	encodebin->ienc_name = g_strdup(DEFAULT_PROP_IENC_NAME);
	encodebin->mux_name = g_strdup(DEFAULT_PROP_MUX_NAME);
	encodebin->vconv_name = g_strdup(DEFAULT_PROP_VCONV_NAME);

	encodebin->vcaps = gst_caps_new_any ();
	encodebin->acaps = gst_caps_new_any ();
	encodebin->icaps = gst_caps_new_any ();

	encodebin->audio_queue = NULL;
	encodebin->video_queue = NULL;
	encodebin->video_encode_queue = NULL;
	encodebin->image_queue = NULL;

	encodebin->audio_encode = NULL;
	encodebin->video_encode = NULL;
	encodebin->image_encode = NULL;

	encodebin->vcapsfilter = NULL;
	encodebin->acapsfilter = NULL;
	encodebin->icapsfilter = NULL;

	encodebin->video_toggle = NULL;
	encodebin->image_toggle = NULL;
	encodebin->color_space = NULL;
	encodebin->audio_conv = NULL;
	encodebin->audio_sample = NULL;

	encodebin->mux = NULL;

	encodebin->paused_time = 0;
	encodebin->total_offset_time = 0;

	encodebin->vsink_probeid = 0;
	encodebin->vsink_hs_probeid = 0;
	encodebin->asink_probeid = 0;
	encodebin->veque_sig_id = 0;
	encodebin->aeque_sig_id = 0;

	encodebin->enable_mux = TRUE;
}

static void
gst_encode_bin_dispose (GObject * object)
{
	GstEncodeBin *encodebin = GST_ENCODE_BIN (object);

	g_free(encodebin->venc_name);
	encodebin->venc_name = NULL;

	g_free(encodebin->aenc_name);
	encodebin->aenc_name = NULL;	

	g_free(encodebin->ienc_name);
	encodebin->ienc_name = NULL;

	g_free(encodebin->mux_name);
	encodebin->mux_name = NULL;

	g_free(encodebin->vconv_name);
	encodebin->vconv_name = NULL;

	gst_caps_replace (&encodebin->vcaps, NULL);
	gst_caps_replace (&encodebin->acaps, NULL);
	gst_caps_replace (&encodebin->icaps, NULL);

	if (encodebin->srcpad != NULL) {
		gst_element_remove_pad(GST_ELEMENT(encodebin), encodebin->srcpad);
		encodebin->srcpad = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);

	encodebin->video_sinkpad = NULL;
	encodebin->audio_sinkpad = NULL;
	encodebin->image_sinkpad = NULL;
	encodebin->mux_audio_sinkpad = NULL;
	encodebin->mux_video_sinkpad = NULL;

	encodebin->audio_queue = NULL;
	encodebin->video_queue = NULL;
	encodebin->image_queue = NULL;

	encodebin->audio_encode = NULL;
	encodebin->video_encode = NULL;
	encodebin->video_encode_queue = NULL;
	encodebin->image_encode = NULL;

	encodebin->vcapsfilter = NULL;
	encodebin->acapsfilter = NULL;
	encodebin->icapsfilter = NULL;

	encodebin->video_toggle = NULL;
	encodebin->image_toggle = NULL;
	encodebin->color_space = NULL;
	encodebin->audio_conv = NULL;
	encodebin->audio_sample = NULL;
	
	if (encodebin->mux && GST_IS_ELEMENT(encodebin->mux)) {
		int remain_count= 0;
		remain_count = GST_OBJECT_REFCOUNT_VALUE(encodebin->mux);
		while (remain_count) {
			gst_object_unref(encodebin->mux);
			remain_count--;
		}
	}

	encodebin->mux = NULL;
}

static void
gst_encode_bin_finalize (GObject * object)
{
	GstEncodeBin *encodebin = GST_ENCODE_BIN (object);

	g_mutex_free (encodebin->mutex);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstStateChangeReturn
gst_encode_bin_change_state (GstElement * element, GstStateChange transition)
{
	GstStateChangeReturn ret;
	GstEncodeBin *encode_bin;

	encode_bin = GST_ENCODE_BIN (element);

	switch (transition) {
		case GST_STATE_CHANGE_NULL_TO_READY:
			gst_encode_bin_link_elements(encode_bin);
			break;
		case GST_STATE_CHANGE_READY_TO_PAUSED:
			/* reset time related values */
			encode_bin->paused_time = 0;
			encode_bin->total_offset_time = 0;
			break;
		case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			break;
		default:
			break;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

	switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		gst_encode_bin_unlink_elements(encode_bin);	
		break;		
	default:
		break;
	}

//  if (ret == GST_STATE_CHANGE_FAILURE)
//    goto done;
//done:
	return ret;
}

static void
gst_encode_bin_release_all_pads (GstEncodeBin *encodebin)
{
	gst_element_remove_pad(GST_ELEMENT(encodebin), encodebin->video_sinkpad);
	gst_element_remove_pad(GST_ELEMENT(encodebin), encodebin->audio_sinkpad);
	gst_element_remove_pad(GST_ELEMENT(encodebin), encodebin->image_sinkpad);
}

static void
gst_encode_bin_release_pad (GstElement * element, GstPad * pad)
{
	GstEncodeBin *encodebin = GST_ENCODE_BIN (element);
	GstPad *muxpad = NULL;
	
	if(!pad_compare_name(pad, "video")) {
#if 0		
		gst_encode_bin_remove_element(encodebin, encodebin->video_queue);
		encodebin->video_queue = NULL;
		gst_encode_bin_remove_element(encodebin, encodebin->video_toggle);
		encodebin->video_toggle = NULL;
	    	gst_encode_bin_remove_element(encodebin, encodebin->color_space);
		encodebin->color_space = NULL;
		gst_encode_bin_remove_element(encodebin, encodebin->vcapsfilter);
		encodebin->vcapsfilter = NULL;
		gst_encode_bin_remove_element(encodebin, encodebin->video_encode_queue);
		encodebin->video_encode_queue =  NULL;		
		gst_encode_bin_remove_element(encodebin, encodebin->video_encode);
		encodebin->video_encode =  NULL;

		gst_element_release_request_pad(encodebin->mux, encodebin->mux_video_sinkpad);	
		encodebin->mux_video_sinkpad = NULL;

		if(encodebin->mux_audio_sinkpad == NULL) {
			gst_encode_bin_remove_element(encodebin, encodebin->mux);
			encodebin->mux = NULL;
		}
		else 
		{
			encodebin->mux_audio_sinkpad = NULL;
		}
#endif	

		if(encodebin->mux_video_sinkpad != NULL)
		{	
			gst_element_release_request_pad(encodebin->mux, encodebin->mux_video_sinkpad);
			encodebin->mux_video_sinkpad = NULL;
		}
		
		gst_pad_set_active (pad, FALSE); //??
		gst_element_remove_pad(element, pad);
		encodebin->video_sinkpad = NULL;
	} else if(!pad_compare_name(pad, "audio")) {
#if 0
		gst_encode_bin_remove_element(encodebin, encodebin->audio_queue);
		encodebin->audio_queue = NULL;
		gst_encode_bin_remove_element(encodebin, encodebin->audio_sample);
		encodebin->audio_sample = NULL;
		gst_encode_bin_remove_element(encodebin, encodebin->audio_conv);
		encodebin->audio_conv = NULL;
		gst_encode_bin_remove_element(encodebin, encodebin->acapsfilter);
		encodebin->acapsfilter = NULL;
		gst_encode_bin_remove_element(encodebin, encodebin->audio_encode_queue);
		encodebin->audio_encode_queue =  NULL;			
		gst_encode_bin_remove_element(encodebin, encodebin->audio_encode);
		encodebin->audio_encode = NULL;

		encodebin->mux_audio_sinkpad;
		gst_element_release_request_pad(encodebin->mux, encodebin->mux_audio_sinkpad);
		muxpad = NULL;

		if(encodebin->mux_video_sinkpad == NULL) {
			gst_encode_bin_remove_element(encodebin, encodebin->mux);
			encodebin->mux = NULL;
		}
		else 
		{
			encodebin->mux_video_sinkpad = NULL;
		}
#endif		
		if(encodebin->mux_audio_sinkpad != NULL)
		{
			gst_element_release_request_pad(encodebin->mux, encodebin->mux_audio_sinkpad);
//			gst_object_unref(encodebin->mux_audio_sinkpad);		//***
			encodebin->mux_audio_sinkpad = NULL;
		}

		gst_pad_set_active (pad, FALSE); //??		
		gst_element_remove_pad(element, pad);
		encodebin->audio_sinkpad = NULL;
	} else {
#if 0
		gst_encode_bin_remove_element(encodebin, encodebin->image_queue);
		encodebin->image_queue = NULL;
		gst_encode_bin_remove_element(encodebin, encodebin->image_toggle);
		encodebin->image_toggle = NULL;
		gst_encode_bin_remove_element(encodebin, encodebin->color_space);
		encodebin->color_space = NULL;
		gst_encode_bin_remove_element(encodebin, encodebin->icapsfilter);
		encodebin->icapsfilter = NULL;
		gst_encode_bin_remove_element(encodebin, encodebin->image_encode);
		encodebin->image_encode = NULL;
#endif		
		gst_pad_set_active (pad, FALSE); //??		
		gst_element_remove_pad(element, pad);
		encodebin->image_sinkpad = NULL;
	}
}

static gint
pad_compare_name (GstPad * pad1, const gchar * name)
{
	gint result;

	GST_OBJECT_LOCK (pad1);
		result = strcmp (GST_PAD_NAME (pad1), name);
	GST_OBJECT_UNLOCK (pad1);

	return result;
}

static gboolean 
gst_encode_bin_add_element_by_name (GstEncodeBin *encodebin, GstEncodeBinElement type, const gchar *name)
{
	switch(type) {
		case ENCODEBIN_ELEMENT_VENC:
			encodebin->video_encode = gst_element_factory_make (name, NULL);
			if(encodebin->video_encode != NULL) {
				gst_bin_add(GST_BIN(encodebin), encodebin->video_encode);
				g_free(encodebin->venc_name);
				encodebin->venc_name = NULL;
			} else {
				goto element_make_fail;
			}
			break;
		case ENCODEBIN_ELEMENT_AENC:
			encodebin->audio_encode = gst_element_factory_make (name, "audio_encode");
			if(encodebin->audio_encode != NULL) {
				gst_bin_add(GST_BIN(encodebin), encodebin->audio_encode);
				g_free(encodebin->aenc_name);
				encodebin->aenc_name = NULL;
			} else {
				goto element_make_fail;
			}
			break;
		case ENCODEBIN_ELEMENT_IENC:
			encodebin->image_encode = gst_element_factory_make (name, "image_encode");
			if(encodebin->image_encode != NULL) {
				gst_bin_add(GST_BIN(encodebin), encodebin->image_encode);
				g_free(encodebin->ienc_name);
				encodebin->ienc_name = NULL;
			} else {
				goto element_make_fail;
			}
			break;
		case ENCODEBIN_ELEMENT_MUX:
			encodebin->mux = gst_element_factory_make (name, "mux");
			if(encodebin->mux != NULL) {
				gst_bin_add(GST_BIN(encodebin), encodebin->mux);
				g_free(encodebin->mux_name);
				encodebin->mux_name = NULL;
			} else {
				goto element_make_fail;
			}
			break;
		case ENCODEBIN_ELEMENT_VIDEO_CONV:
			encodebin->color_space = gst_element_factory_make(name, "video_convert");
			if (encodebin->color_space != NULL) {
				gst_bin_add(GST_BIN(encodebin), encodebin->color_space);
				g_free(encodebin->vconv_name);
				encodebin->vconv_name = NULL;
			} else {
				goto element_make_fail;
			}
			break;
		default:
			GST_WARNING_OBJECT(encodebin, "Invalid element type = %d", type);
			break;
	}

	return TRUE;
	
element_make_fail:
	GST_WARNING_OBJECT(encodebin, "no such element factory \"%s\"!", name);
	return FALSE;
}

#if 0 //disable unused function
static gboolean 
gst_encode_bin_change_profile(GstEncodeBin *encodebin, gboolean newprofile)
{

  gst_encode_bin_remove_element(encodebin, encodebin->video_encode);
  gst_encode_bin_remove_element(encodebin, encodebin->audio_encode);
  gst_encode_bin_remove_element(encodebin, encodebin->image_encode);
  gst_encode_bin_remove_element(encodebin, encodebin->mux);

  switch (newprofile) {
    case GST_ENCODE_BIN_PROFILE_AV :
	encodebin->audio_encode = gst_element_factory_make (encodebin->aenc_name, "audio_encode");
	encodebin->video_encode = gst_element_factory_make (encodebin->venc_name,"video_encode");
	encodebin->mux = gst_element_factory_make (encodebin->mux_name,"mux");

	  gst_bin_add_many (GST_BIN (encodebin),
  	                    encodebin->audio_encode,
	                    encodebin->video_encode,
	                    encodebin->mux,
                           NULL);
	break;
    case GST_ENCODE_BIN_PROFILE_AUDIO :
	encodebin->audio_encode = gst_element_factory_make (encodebin->aenc_name, "audio_encode");
	encodebin->mux = gst_element_factory_make (encodebin->mux_name,"mux");

	  gst_bin_add_many (GST_BIN (encodebin),
  	                    encodebin->audio_encode,
	                    encodebin->mux,
                           NULL);
	break;
    case GST_ENCODE_BIN_PROFILE_IMAGE :
	encodebin->image_encode = gst_element_factory_make (encodebin->ienc_name,"image_encode");

	gst_bin_add_many (GST_BIN (encodebin),
  	                    encodebin->image_encode,
                           NULL);
	break;
    default:
      GST_WARNING_OBJECT(encodebin, "Invalid profile number = %d", encodebin->profile);
      return FALSE;
      break;
  	}
  return TRUE;

}

static void 
gst_encode_bin_replace_element (GstEncodeBin *encodebin, gint type, GstElement * newelement)
{
  if(newelement == NULL) {
  	GST_ERROR_OBJECT(encodebin, "some elements are null\n");
	return;
  }
  switch(type) {
    case PROP_VIDEO_ENC:
	gst_encode_bin_remove_element(encodebin, encodebin->video_encode);
	encodebin->video_encode = newelement;
	gst_object_ref (encodebin->video_encode);
       gst_object_sink (GST_OBJECT_CAST (encodebin->video_encode)); // take ownership ??
	gst_bin_add(GST_BIN(encodebin),  encodebin->video_encode);
	break;
    case PROP_AUDIO_ENC:
	gst_encode_bin_remove_element(encodebin, encodebin->audio_encode);
	encodebin->audio_encode = newelement;
	gst_object_ref (encodebin->audio_encode);
       gst_object_sink (GST_OBJECT_CAST (encodebin->audio_encode));
	gst_bin_add(GST_BIN(encodebin),  encodebin->audio_encode);
	break;  
    case PROP_IMAGE_ENC:
	gst_encode_bin_remove_element(encodebin, encodebin->image_encode);
	encodebin->image_encode = newelement;
	gst_object_ref (encodebin->image_encode);
       gst_object_sink (GST_OBJECT_CAST (encodebin->image_encode));
	gst_bin_add(GST_BIN(encodebin), encodebin->image_encode);
	break;
    case PROP_MUX:
	gst_encode_bin_remove_element(encodebin, encodebin->mux);
	encodebin->mux = newelement;
	gst_object_ref (encodebin->mux);
       gst_object_sink (GST_OBJECT_CAST (encodebin->mux));
	gst_bin_add(GST_BIN(encodebin),  encodebin->mux);
	break;
    default:
	GST_WARNING_OBJECT(encodebin, "Invalid type = %d", type);
      return;
	break;
  }
}

static gboolean 
gst_encode_bin_replace_element_by_name(GstEncodeBin *encodebin, GstEncodeBinElement type, const gchar *name)
{
	GstPad *sink1, *sink2, *src, *peersink1, *peersink2, *peersrc;
	
	switch(type) {
		case ENCODEBIN_ELEMENT_VENC:
			if(encodebin->video_encode == NULL) {
				encodebin->video_encode = gst_element_factory_make (name, "video_encode");
				gst_bin_add(GST_BIN(encodebin), encodebin->video_encode);
			} else {
				sink1 = gst_element_get_static_pad(encodebin->video_encode, "sink");
				src = gst_element_get_static_pad(encodebin->video_encode, "src");
				if(sink1 != NULL) {
					peersink1 = gst_pad_get_peer(sink1);
					if(peersink1 != NULL) {
						if(!gst_pad_unlink(peersink1, sink1)) {
							goto unlink_fail;
						}
					}
				}

				if(src !=NULL) {
					peersrc = gst_pad_get_peer(src);
					if(peersrc != NULL) {
						if(!gst_pad_unlink(src, peersrc)) {
							goto unlink_fail;
						}
					}
				}
				
				if(gst_encode_bin_remove_element(encodebin, encodebin->video_encode)) {
					if(encodebin->video_encode = gst_element_factory_make (name, "video_encode") != NULL) {
						gst_bin_add(GST_BIN(encodebin), encodebin->video_encode);
						if(peersink1 != NULL) {
							if(!gst_pad_link(peersink1, gst_element_get_pad(encodebin->video_encode, "sink"))) {
								goto link_fail;
							}
						}
						
						if(peersrc != NULL) {
							if(!gst_pad_link(gst_element_get_pad(encodebin->video_encode, "src"), peersrc)) {
								goto link_fail;
							}
						}
					} else {
						GST_ERROR_OBJECT(encodebin, "gst_encode_bin_replace_element_by_name() new element[%d] make fail\n", type);
						return FALSE;					
					}
				} else {
					GST_ERROR_OBJECT(encodebin, "gst_encode_bin_replace_element_by_name() old element[%d] remove fail\n", type);
					return FALSE;
				}				
			}			
			break;
		case ENCODEBIN_ELEMENT_AENC:
			break;
		case ENCODEBIN_ELEMENT_IENC:
			break;
		case ENCODEBIN_ELEMENT_MUX:
			break;
		default :
			GST_WARNING_OBJECT(encodebin, "Invalid element type = %d", type);
			break;
	}
	gst_object_unref(sink1);
	gst_object_unref(sink2);
	gst_object_unref(src);
	gst_object_unref(peersink1);	
	gst_object_unref(peersink2);	
	gst_object_unref(peersrc);		
	return TRUE;

unlink_fail:
	gst_object_unref(sink1);
	gst_object_unref(sink2);
	gst_object_unref(src);
	gst_object_unref(peersink1);	
	gst_object_unref(peersink2);	
	gst_object_unref(peersrc);	
	GST_ERROR_OBJECT(encodebin, "gst_encode_bin_replace_element_by_name() old element[%d] unlink fail\n", type);
	return FALSE;

	
link_fail:
	gst_object_unref(sink1);
	gst_object_unref(sink2);
	gst_object_unref(src);
	gst_object_unref(peersink1);	
	gst_object_unref(peersink2);	
	gst_object_unref(peersrc);	
	GST_ERROR_OBJECT(encodebin, "gst_encode_bin_replace_element_by_name() new element[%d] link fail\n", type);
	return FALSE;			
}

static gboolean 
gst_encode_bin_replace_element_by_object(GstEncodeBin *encodebin, GstEncodeBinElement type, GstElement * element)
{
	GstPad *sink1, *sink2, *src, *peersink1, *peersink2, *peersrc;
	
	switch(type)
		case ENCODEBIN_ELEMENT_VENC:
			if(encodebin->video_encode == NULL) {
				encodebin->video_encode = element
			}
			break;
		case ENCODEBIN_ELEMENT_AENC:
			break;
		case ENCODEBIN_ELEMENT_IENC:
			break;
		case ENCODEBIN_ELEMENT_MUX:
			break;
		default :
			GST_WARNING_OBJECT (encodebin,"Invalid element type = %d", type);
			break;
}
#endif //disable unused function

static gboolean 
gst_encode_bin_remove_element (GstEncodeBin *encodebin, GstElement * element)
{
	GstObject *parent;
	gchar *ename = NULL;
	GST_INFO_OBJECT (encodebin, "gst_encode_bin_remove_element");

	if (element == NULL) {
		GST_INFO_OBJECT (encodebin, "element is already NULL");
		return TRUE;
	}

	gst_element_set_state (element, GST_STATE_NULL);
	parent = gst_element_get_parent (element);

	if (parent != NULL) {
		if(!gst_bin_remove (GST_BIN_CAST (parent), element)) {
			gst_object_unref (parent);
			ename = gst_element_get_name (element);
			GST_ERROR_OBJECT (encodebin, "gst_encode_bin_remove_element() [%s] remove fail", ename);
			g_free (ename);
			return FALSE;
		} else {
			gst_object_unref(parent);
		}
	} else {
		gst_object_unref(element);
	}

	return TRUE;
  }

static gboolean 
gst_encode_bin_link_elements (GstEncodeBin *encodebin)  // need to return ????
{
	GstPad *srcpad = NULL, *sinkpad = NULL;
	switch(encodebin->profile) {
		case GST_ENCODE_BIN_PROFILE_AV :
			if (!gst_caps_is_any(encodebin->vcaps)) {
				gchar *caps_str = NULL;
				caps_str = gst_caps_to_string(encodebin->vcaps);
				if (caps_str) {
					GST_INFO_OBJECT(encodebin, "vconv caps [%s]", caps_str);
					g_free(caps_str);
					caps_str = NULL;
				}

				g_object_set(encodebin->vcapsfilter, "caps", encodebin->vcaps, NULL);
			}

			if (encodebin->auto_color_space) {
				if(encodebin->color_space == NULL) {
					encodebin->color_space = gst_element_factory_make (encodebin->vconv_name, "video_convert");
					gst_bin_add (GST_BIN (encodebin), encodebin->color_space);
				}

				srcpad = gst_element_get_static_pad(encodebin->video_queue, "src");
				if( encodebin->video_toggle )
				{
					sinkpad = gst_element_get_static_pad(encodebin->video_toggle, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);
					
					srcpad = gst_element_get_static_pad(encodebin->video_toggle, "src");
				}
				sinkpad = gst_element_get_static_pad(encodebin->color_space, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);

				srcpad = gst_element_get_static_pad(encodebin->color_space, "src");
				sinkpad = gst_element_get_static_pad(encodebin->vcapsfilter, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);

				srcpad = gst_element_get_static_pad(encodebin->vcapsfilter, "src");
				sinkpad = gst_element_get_static_pad(encodebin->video_encode, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);
#if 0
				if(encodebin->use_venc_queue)
				{
					if(encodebin->video_encode_queue == NULL) {
					    encodebin->video_encode_queue  = gst_element_factory_make ("queue","video_encode_queue");
					    gst_bin_add (GST_BIN (encodebin), encodebin->video_encode_queue);
						
					    ENCODER_QUEUE_SET(encodebin->video_encode_queue, 0, 0, VIDEO_ENC_QUE_TIME);
					    encodebin->veque_sig_id = g_signal_connect( G_OBJECT(encodebin->video_encode_queue), "overrun", 
																G_CALLBACK(queue_overun_cb), encodebin);
							
					}	

					srcpad = gst_element_get_static_pad(encodebin->video_encode, "src");
					sinkpad = gst_element_get_static_pad(encodebin->video_encode_queue, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);	
				}
#else
				if(encodebin->video_encode_queue)
				{
				    ENCODER_QUEUE_SET(encodebin->video_encode_queue, 0, 0, VIDEO_ENC_QUE_TIME);
				    encodebin->veque_sig_id = g_signal_connect( G_OBJECT(encodebin->video_encode_queue), "overrun", 
																G_CALLBACK(queue_overun_cb), encodebin);
#if 0
				    g_object_set(G_OBJECT(encodebin->video_queue), 
						"max-size-bytes", (guint)0, 
						"max-size-buffers", (guint)1, 	
						"max-size-time", (guint64)0, 
						NULL); 
#endif
					srcpad = gst_element_get_static_pad(encodebin->video_encode, "src");
					sinkpad = gst_element_get_static_pad(encodebin->video_encode_queue, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);	
				}
#endif
				                                 
			}
			else {
				srcpad = gst_element_get_static_pad(encodebin->video_queue, "src");
				if( encodebin->video_toggle )
				{
					sinkpad = gst_element_get_static_pad(encodebin->video_toggle, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);	

					srcpad = gst_element_get_static_pad(encodebin->video_toggle, "src");
				}
				sinkpad = gst_element_get_static_pad(encodebin->vcapsfilter, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);	

				srcpad = gst_element_get_static_pad(encodebin->vcapsfilter, "src");
				sinkpad = gst_element_get_static_pad(encodebin->video_encode, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);					
 #if 0
				if(encodebin->use_venc_queue)
				{
					if(encodebin->video_encode_queue == NULL) {
					    encodebin->video_encode_queue  = gst_element_factory_make ("queue","video_encode_queue");
					    gst_bin_add (GST_BIN (encodebin), encodebin->video_encode_queue);
						
					    ENCODER_QUEUE_SET(encodebin->video_encode_queue, 0, 0, VIDEO_ENC_QUE_TIME);
					    encodebin->veque_sig_id = g_signal_connect( G_OBJECT(encodebin->video_encode_queue), "overrun", 
																G_CALLBACK(queue_overun_cb), encodebin);							
					}
					
					srcpad = gst_element_get_static_pad(encodebin->video_encode, "src");
					sinkpad = gst_element_get_static_pad(encodebin->video_encode_queue, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);
					
				}			
#else
				if(encodebin->video_encode_queue)
				{
				    ENCODER_QUEUE_SET(encodebin->video_encode_queue, 0, 0, VIDEO_ENC_QUE_TIME);
				    encodebin->veque_sig_id = g_signal_connect( G_OBJECT(encodebin->video_encode_queue), "overrun", 
																G_CALLBACK(queue_overun_cb), encodebin);
#if 0					
				    g_object_set(G_OBJECT(encodebin->video_queue), 
						"max-size-bytes", (guint)0, 
						"max-size-buffers", (guint)1, 	
						"max-size-time", (guint64)0, 
						NULL); 					
#endif

					srcpad = gst_element_get_static_pad(encodebin->video_encode, "src");
					sinkpad = gst_element_get_static_pad(encodebin->video_encode_queue, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);	
				}
#endif				
		
			}

//			gst_element_get_request_pad (encodebin->mux, "video_%d");
#if 0
			if(encodebin->use_venc_queue)
			{
				srcpad = gst_element_get_static_pad(encodebin->video_encode_queue, "src");
				sinkpad = encodebin->mux_video_sinkpad = gst_encode_bin_get_mux_sink_pad(encodebin->mux, ENCODEBIN_MUX_VIDEO_SINK);
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);				
			}
#endif
			if (encodebin->enable_mux)
			{
				if(encodebin->video_encode_queue)
				{
					srcpad = gst_element_get_static_pad(encodebin->video_encode_queue, "src");
					sinkpad = encodebin->mux_video_sinkpad = gst_encode_bin_get_mux_sink_pad(encodebin->mux, ENCODEBIN_MUX_VIDEO_SINK);
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);				
				}
				else
				{
					srcpad = gst_element_get_static_pad(encodebin->video_encode, "src");
					sinkpad = encodebin->mux_video_sinkpad = gst_encode_bin_get_mux_sink_pad(encodebin->mux, ENCODEBIN_MUX_VIDEO_SINK);
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, video_link_fail);			
				}

				srcpad = gst_element_get_static_pad(encodebin->mux, "src");
			}
			else
			{
				if(encodebin->video_encode_queue)
				{
					srcpad = gst_element_get_static_pad(encodebin->video_encode_queue, "src");
				}
				else
				{
					srcpad = gst_element_get_static_pad(encodebin->video_encode, "src");
				}
			}
			
			if(gst_ghost_pad_get_target(GST_GHOST_PAD (encodebin->srcpad)) != srcpad)
				gst_ghost_pad_set_target(GST_GHOST_PAD (encodebin->srcpad), srcpad);
			gst_object_unref(srcpad);
			srcpad = NULL;			
		
			/* For pause/resume control */
//			encodebin->vsink_probeid = gst_pad_add_data_probe (gst_element_get_static_pad (encodebin->video_queue, "sink"),
			sinkpad = gst_element_get_static_pad (encodebin->video_queue, "sink");
			encodebin->vsink_probeid = gst_pad_add_buffer_probe (sinkpad, G_CALLBACK (gst_encode_bin_video_probe), encodebin);
			gst_object_unref(sinkpad);
			sinkpad = NULL;
    
			if(encodebin->high_speed_fps > DEFAULT_PROP_HIGH_SPEED)
			{
//				encodebin->vsink_hs_probeid = gst_pad_add_data_probe (gst_element_get_static_pad (encodebin->video_encode, "sink"),
				sinkpad = gst_element_get_static_pad (encodebin->video_encode, "sink");
				encodebin->vsink_hs_probeid = gst_pad_add_buffer_probe (sinkpad,	G_CALLBACK (gst_encode_bin_video_probe_hs), encodebin);
				gst_object_unref(sinkpad);
				sinkpad = NULL;				
			}			
			
			if(encodebin->audio_queue == NULL)
			{
				GST_WARNING_OBJECT(encodebin, "Audio pad isn't requested, recording video only mode");
				break;
			}
		case GST_ENCODE_BIN_PROFILE_AUDIO :
			if(!gst_caps_is_any(encodebin->acaps))
			{			
				g_object_set(encodebin->acapsfilter, "caps", encodebin->acaps, NULL);
			}
			if (encodebin->auto_audio_convert ||encodebin->auto_audio_resample) {
				if (!encodebin->auto_audio_convert) {
					if(encodebin->audio_sample == NULL) {
					  	encodebin->audio_sample = gst_element_factory_make ("audioresample","audio_sample");
					       gst_bin_add (GST_BIN (encodebin), encodebin->audio_sample);
					}
					srcpad = gst_element_get_static_pad(encodebin->audio_queue, "src");
					sinkpad = gst_element_get_static_pad(encodebin->audio_sample, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);

					srcpad = gst_element_get_static_pad(encodebin->audio_sample, "src");
					sinkpad = gst_element_get_static_pad(encodebin->acapsfilter, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);

					srcpad = gst_element_get_static_pad(encodebin->acapsfilter, "src");
					sinkpad = gst_element_get_static_pad(encodebin->audio_encode, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);					
#if 0					
					if(encodebin->use_aenc_queue)
					{
						if(encodebin->audio_encode_queue == NULL) {
						    encodebin->audio_encode_queue  = gst_element_factory_make ("queue","audio_encode_queue");
						    gst_bin_add (GST_BIN (encodebin), encodebin->audio_encode_queue);
						  
 						   ENCODER_QUEUE_SET(encodebin->audio_encode_queue, 0, 0, AUDIO_ENC_QUE_TIME);
					    	   encodebin->aeque_sig_id = g_signal_connect( G_OBJECT(encodebin->audio_encode_queue), "overrun", 
							   										G_CALLBACK(queue_overun_cb), encodebin);					   
						}	

						srcpad = gst_element_get_static_pad(encodebin->audio_encode, "src");
						sinkpad = gst_element_get_static_pad(encodebin->audio_encode_queue, "sink");
						_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);	
					}					
#else
					if(encodebin->audio_encode_queue)
					{
				  
						ENCODER_QUEUE_SET(encodebin->audio_encode_queue, 0, 0, AUDIO_ENC_QUE_TIME);
						encodebin->aeque_sig_id = g_signal_connect( G_OBJECT(encodebin->audio_encode_queue), "overrun", 
															G_CALLBACK(queue_overun_cb), encodebin);					   

						srcpad = gst_element_get_static_pad(encodebin->audio_encode, "src");
						sinkpad = gst_element_get_static_pad(encodebin->audio_encode_queue, "sink");
						_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);	
					}
#endif
					
				} else if (!encodebin->auto_audio_resample) {
					  if (encodebin->audio_conv == NULL) {
					  	encodebin->audio_conv = gst_element_factory_make ("audioconvert","audio_conv");
					      gst_bin_add (GST_BIN (encodebin), encodebin->audio_conv);
					   }	

					srcpad = gst_element_get_static_pad(encodebin->audio_queue, "src");
					sinkpad = gst_element_get_static_pad(encodebin->audio_conv, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);

					srcpad = gst_element_get_static_pad(encodebin->audio_conv, "src");
					sinkpad = gst_element_get_static_pad(encodebin->acapsfilter, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);

					srcpad = gst_element_get_static_pad(encodebin->acapsfilter, "src");
					sinkpad = gst_element_get_static_pad(encodebin->audio_encode, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);					
#if 0					
					if(encodebin->use_aenc_queue)
					{
						if(encodebin->audio_encode_queue == NULL) {
						    encodebin->audio_encode_queue  = gst_element_factory_make ("queue","audio_encode_queue");
						    gst_bin_add (GST_BIN (encodebin), encodebin->audio_encode_queue);

						    ENCODER_QUEUE_SET(encodebin->audio_encode_queue, 0, 0, AUDIO_ENC_QUE_TIME);
					    	   encodebin->aeque_sig_id = g_signal_connect( G_OBJECT(encodebin->audio_encode_queue), "overrun", 
							   										G_CALLBACK(queue_overun_cb), encodebin);							
						}

						srcpad = gst_element_get_static_pad(encodebin->audio_encode, "src");
						sinkpad = gst_element_get_static_pad(encodebin->audio_encode_queue, "sink");
						_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);						
				
					}					
#else
					if(encodebin->audio_encode_queue)
					{
				  
						ENCODER_QUEUE_SET(encodebin->audio_encode_queue, 0, 0, AUDIO_ENC_QUE_TIME);
						encodebin->aeque_sig_id = g_signal_connect( G_OBJECT(encodebin->audio_encode_queue), "overrun", 
															G_CALLBACK(queue_overun_cb), encodebin);					   

						srcpad = gst_element_get_static_pad(encodebin->audio_encode, "src");
						sinkpad = gst_element_get_static_pad(encodebin->audio_encode_queue, "sink");
						_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);	
					}
#endif					
					
				} else {
					if(encodebin->audio_sample == NULL) {
					  	encodebin->audio_sample = gst_element_factory_make ("audioresample","audio_sample");
					       gst_bin_add (GST_BIN (encodebin), encodebin->audio_sample);
					}
					if (encodebin->audio_conv == NULL) {
					  	encodebin->audio_conv = gst_element_factory_make ("audioconvert","audio_conv");
					      gst_bin_add (GST_BIN (encodebin), encodebin->audio_conv);
					}

					srcpad = gst_element_get_static_pad(encodebin->audio_queue, "src");
					sinkpad = gst_element_get_static_pad(encodebin->audio_conv, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);	

					srcpad = gst_element_get_static_pad(encodebin->audio_conv, "src");
					sinkpad = gst_element_get_static_pad(encodebin->audio_sample, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);

					srcpad = gst_element_get_static_pad(encodebin->audio_sample, "src");
					sinkpad = gst_element_get_static_pad(encodebin->acapsfilter, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);						

					srcpad = gst_element_get_static_pad(encodebin->acapsfilter, "src");
					sinkpad = gst_element_get_static_pad(encodebin->audio_encode, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);	
#if 0					
					if(encodebin->use_aenc_queue)
					{
						if(encodebin->audio_encode_queue == NULL) {
						    encodebin->audio_encode_queue  = gst_element_factory_make ("queue","audio_encode_queue");
						    gst_bin_add (GST_BIN (encodebin), encodebin->audio_encode_queue);
							
						    ENCODER_QUEUE_SET(encodebin->audio_encode_queue, 0, 0, AUDIO_ENC_QUE_TIME);
					    	   encodebin->aeque_sig_id = g_signal_connect( G_OBJECT(encodebin->audio_encode_queue), "overrun", 
							   										G_CALLBACK(queue_overun_cb), encodebin);						
						}	

						srcpad = gst_element_get_static_pad(encodebin->audio_encode, "src");
						sinkpad = gst_element_get_static_pad(encodebin->audio_encode_queue, "sink");
						_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);
					
					}			
#else
					if(encodebin->audio_encode_queue)
					{
				  
						ENCODER_QUEUE_SET(encodebin->audio_encode_queue, 0, 0, AUDIO_ENC_QUE_TIME);
						encodebin->aeque_sig_id = g_signal_connect( G_OBJECT(encodebin->audio_encode_queue), "overrun", 
															G_CALLBACK(queue_overun_cb), encodebin);					   

						srcpad = gst_element_get_static_pad(encodebin->audio_encode, "src");
						sinkpad = gst_element_get_static_pad(encodebin->audio_encode_queue, "sink");
						_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);	
					}
#endif					
					
				}
			}else {

				srcpad = gst_element_get_static_pad(encodebin->audio_queue, "src");
				sinkpad = gst_element_get_static_pad(encodebin->acapsfilter, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);			

				srcpad = gst_element_get_static_pad(encodebin->acapsfilter, "src");
				sinkpad = gst_element_get_static_pad(encodebin->audio_encode, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);					
#if 0
				if(encodebin->use_aenc_queue)
				{
					if(encodebin->audio_encode_queue == NULL) {
					    encodebin->audio_encode_queue  = gst_element_factory_make ("queue","audio_encode_queue");
					    gst_bin_add (GST_BIN (encodebin), encodebin->audio_encode_queue);
						
					    ENCODER_QUEUE_SET(encodebin->audio_encode_queue, 0, 0, AUDIO_ENC_QUE_TIME);
				    	   encodebin->aeque_sig_id = g_signal_connect( G_OBJECT(encodebin->audio_encode_queue), "overrun", 
						   										G_CALLBACK(queue_overun_cb), encodebin);				
					}		

					srcpad = gst_element_get_static_pad(encodebin->audio_encode, "src");
					sinkpad = gst_element_get_static_pad(encodebin->audio_encode_queue, "sink");
					_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);					
				}				
#else
					if(encodebin->audio_encode_queue)
					{
				  
						ENCODER_QUEUE_SET(encodebin->audio_encode_queue, 0, 0, AUDIO_ENC_QUE_TIME);
						encodebin->aeque_sig_id = g_signal_connect( G_OBJECT(encodebin->audio_encode_queue), "overrun", 
															G_CALLBACK(queue_overun_cb), encodebin);					   

						srcpad = gst_element_get_static_pad(encodebin->audio_encode, "src");
						sinkpad = gst_element_get_static_pad(encodebin->audio_encode_queue, "sink");
						_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);	
					}
#endif					
				
			}
#if 0			
			if(encodebin->use_aenc_queue)
			{
				srcpad = gst_element_get_static_pad(encodebin->audio_encode_queue, "src");
				sinkpad = encodebin->mux_audio_sinkpad = gst_encode_bin_get_mux_sink_pad(encodebin->mux, ENCODEBIN_MUX_AUDIO_SINK);
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);	

			}
#else

			if(encodebin->audio_encode_queue)
			{
				srcpad = gst_element_get_static_pad(encodebin->audio_encode_queue, "src");
				sinkpad = encodebin->mux_audio_sinkpad = gst_encode_bin_get_mux_sink_pad(encodebin->mux, ENCODEBIN_MUX_AUDIO_SINK);
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);	

			}
#endif
			else
			{	
				srcpad = gst_element_get_static_pad(encodebin->audio_encode, "src");
				sinkpad = encodebin->mux_audio_sinkpad = gst_encode_bin_get_mux_sink_pad(encodebin->mux, ENCODEBIN_MUX_AUDIO_SINK);
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, audio_link_fail);	
 
			}

			srcpad = gst_element_get_static_pad(encodebin->mux, "src");
			if(gst_ghost_pad_get_target(GST_GHOST_PAD (encodebin->srcpad)) != srcpad)
				gst_ghost_pad_set_target(GST_GHOST_PAD (encodebin->srcpad), srcpad);
			gst_object_unref(srcpad);
			srcpad = NULL;

		    	/* For pause/resume control */
			sinkpad = gst_element_get_static_pad (encodebin->audio_queue, "sink");
			encodebin->asink_probeid = gst_pad_add_buffer_probe (sinkpad, G_CALLBACK (gst_encode_bin_audio_probe), encodebin);
			gst_object_unref(sinkpad);
			sinkpad = NULL;

			break;
		case GST_ENCODE_BIN_PROFILE_IMAGE :
			if(!gst_caps_is_any(encodebin->icaps))
			{				
				g_object_set(encodebin->icapsfilter, "caps", encodebin->icaps, NULL);
			}
				
			if (encodebin->auto_color_space) {
				if(encodebin->color_space == NULL) {
				    encodebin->color_space  = gst_element_factory_make ("ffmpegcolorspace","color_space");
				    gst_bin_add (GST_BIN (encodebin), encodebin->color_space);
				}

				srcpad = gst_element_get_static_pad(encodebin->image_queue, "src");
				sinkpad = gst_element_get_static_pad(encodebin->image_toggle, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, image_link_fail);

				srcpad = gst_element_get_static_pad(encodebin->image_toggle, "src");
				sinkpad = gst_element_get_static_pad(encodebin->color_space, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, image_link_fail);		

				srcpad = gst_element_get_static_pad(encodebin->color_space, "src");
				sinkpad = gst_element_get_static_pad(encodebin->icapsfilter, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, image_link_fail);	

				srcpad = gst_element_get_static_pad(encodebin->icapsfilter, "src");
				sinkpad = gst_element_get_static_pad(encodebin->image_encode, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, image_link_fail);					

			}
			else {
				
				srcpad = gst_element_get_static_pad(encodebin->image_queue, "src");
				sinkpad = gst_element_get_static_pad(encodebin->image_toggle, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, image_link_fail);

				srcpad = gst_element_get_static_pad(encodebin->image_toggle, "src");
				sinkpad = gst_element_get_static_pad(encodebin->icapsfilter, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, image_link_fail);		

				srcpad = gst_element_get_static_pad(encodebin->icapsfilter, "src");
				sinkpad = gst_element_get_static_pad(encodebin->image_encode, "sink");
				_GST_PAD_LINK_UNREF(srcpad, sinkpad, image_link_fail);					

			}
			srcpad =  gst_element_get_static_pad (encodebin->image_encode, "src");
			if(gst_ghost_pad_get_target(GST_GHOST_PAD (encodebin->srcpad)) != srcpad)
				gst_ghost_pad_set_target(GST_GHOST_PAD (encodebin->srcpad), srcpad);
			gst_object_unref(srcpad);
			srcpad = NULL;
			break;
		default:
		      GST_WARNING_OBJECT(encodebin, "Invalid profile number = %d", encodebin->profile);
		      return FALSE;
		      break;
  	}
//	gst_pad_set_active(encodebin->srcpad, TRUE);	
	return TRUE;

video_link_fail:
	// remove elements
	gst_encode_bin_remove_element(encodebin, encodebin->color_space);
	GST_WARNING_OBJECT(encodebin, "encodebin link video elements fail");
	return FALSE;
	
audio_link_fail:
	// remove element 
	gst_encode_bin_remove_element(encodebin, encodebin->audio_conv);	
	gst_encode_bin_remove_element(encodebin, encodebin->audio_sample);	
	GST_WARNING_OBJECT(encodebin, "encodebin link audio elements fail");
	return FALSE;

image_link_fail:
	// remove element 
	gst_encode_bin_remove_element(encodebin, encodebin->color_space);	
	GST_WARNING_OBJECT(encodebin, "encodebin link image elements fail");
	return FALSE;

	
}

static gboolean 
gst_encode_bin_unlink_elements (GstEncodeBin *encodebin)
{
	GstPad *pad = NULL, *muxpad = NULL;
	
	switch(encodebin->profile) {
		case GST_ENCODE_BIN_PROFILE_AV :
			if (encodebin->auto_color_space) {
				if (encodebin->video_toggle) {
					gst_element_unlink_many(
						encodebin->video_queue,
						encodebin->video_toggle,
						encodebin->color_space,
						encodebin->vcapsfilter,
						encodebin->video_encode,
						//encodebin->video_encode_queue,
						NULL);
				} else {
					gst_element_unlink_many(
						encodebin->video_queue,
						encodebin->color_space,
						encodebin->vcapsfilter,
						encodebin->video_encode,
						//encodebin->video_encode_queue,
						NULL);
				}
			} else {
				if (encodebin->video_toggle) {
					gst_element_unlink_many(
						encodebin->video_queue,
						encodebin->video_toggle,
						encodebin->vcapsfilter,
						encodebin->video_encode,
						//encodebin->video_encode_queue,
						NULL);
				} else {
					gst_element_unlink_many(
						encodebin->video_queue,
						encodebin->vcapsfilter,
						encodebin->video_encode,
						//encodebin->video_encode_queue,
						NULL);
				}
			}

			if(encodebin->mux_video_sinkpad != NULL)
			{
#if 0			
				if(encodebin->use_venc_queue)
				{
					gst_element_unlink(encodebin->video_encode, encodebin->video_encode_queue);

					pad = gst_element_get_static_pad (encodebin->video_encode_queue, "src");
					gst_pad_unlink(pad, muxpad);
					gst_object_unref(pad);
					pad = NULL;

					if ( g_signal_handler_is_connected ( encodebin->video_encode_queue, encodebin->veque_sig_id) )
					{
						g_signal_handler_disconnect (  encodebin->video_encode_queue, encodebin->veque_sig_id );
					}
				}
#else
				if(encodebin->video_encode_queue)
				{
					gst_element_unlink(encodebin->video_encode, encodebin->video_encode_queue);

					pad = gst_element_get_static_pad (encodebin->video_encode_queue, "src");
					gst_pad_unlink(pad, encodebin->mux_video_sinkpad);
					gst_object_unref(pad);
					pad = NULL;

					if ( g_signal_handler_is_connected ( encodebin->video_encode_queue, encodebin->veque_sig_id) )
					{
						g_signal_handler_disconnect (  encodebin->video_encode_queue, encodebin->veque_sig_id );
					}
				}
#endif
				else
				{
					pad = gst_element_get_static_pad (encodebin->video_encode, "src");
					gst_pad_unlink(pad, encodebin->mux_video_sinkpad);
					gst_object_unref(pad);
					pad = NULL;
				}

				gst_element_release_request_pad(encodebin->mux, encodebin->mux_video_sinkpad);
//				gst_object_unref(encodebin->mux_video_sinkpad);	//***
				encodebin->mux_video_sinkpad = NULL;
			}
			else
			{
				if (encodebin->enable_mux == FALSE)
				{
					if(encodebin->video_encode_queue)
					{
						gst_element_unlink(encodebin->video_encode, encodebin->video_encode_queue);

						if ( g_signal_handler_is_connected ( encodebin->video_encode_queue, encodebin->veque_sig_id) )
						{
							g_signal_handler_disconnect (  encodebin->video_encode_queue, encodebin->veque_sig_id );
						}			
					}
				}
			}

			if(encodebin->vsink_probeid)
			{
				pad = gst_element_get_static_pad (encodebin->video_queue, "sink");
				gst_pad_remove_buffer_probe(pad, encodebin->vsink_probeid);
				encodebin->vsink_probeid = 0;
				gst_object_unref(pad);
				pad = NULL;					
			}
		

			if(encodebin->vsink_hs_probeid)
			{
				pad = gst_element_get_static_pad (encodebin->video_encode, "sink");
				gst_pad_remove_buffer_probe(pad, encodebin->vsink_hs_probeid);
				encodebin->vsink_hs_probeid = 0;
				gst_object_unref(pad);
				pad = NULL;
			}

			if(encodebin->audio_queue == NULL)
			{
				break;
			}
		case GST_ENCODE_BIN_PROFILE_AUDIO :
			if (encodebin->auto_audio_convert ||encodebin->auto_audio_resample) {
				if (!encodebin->auto_audio_convert) {
						gst_element_unlink_many  (
							encodebin->audio_queue,
							encodebin->audio_sample,
							encodebin->acapsfilter,
							encodebin->audio_encode,
							NULL);	
				} else if (!encodebin->auto_audio_resample) {
						gst_element_unlink_many  (
							encodebin->audio_queue,
							encodebin->audio_conv,					
							encodebin->acapsfilter,
							encodebin->audio_encode,
							NULL);	
				} else {
						gst_element_unlink_many  (
							encodebin->audio_queue,
							encodebin->audio_conv,
							encodebin->audio_sample,										
							encodebin->acapsfilter,
							encodebin->audio_encode,
							NULL);					
				}
			}
			else {
				gst_element_unlink_many  (
					encodebin->audio_queue,
					encodebin->acapsfilter,
					encodebin->audio_encode,
					NULL);	
			}

			if(encodebin->mux_audio_sinkpad != NULL)
			{
#if 0			
				if(encodebin->use_aenc_queue)
				{
					gst_element_unlink(encodebin->audio_encode, encodebin->audio_encode_queue);

					pad = gst_element_get_static_pad (encodebin->audio_encode_queue, "src");
					gst_pad_unlink(pad, muxpad);
					gst_object_unref(pad);
					pad = NULL;	

					if ( g_signal_handler_is_connected ( encodebin->audio_encode_queue, encodebin->veque_sig_id) )
					{
						g_signal_handler_disconnect (  encodebin->audio_encode_queue, encodebin->veque_sig_id );
					}					
				}
#else
				if(encodebin->audio_encode_queue)
				{
					gst_element_unlink(encodebin->audio_encode, encodebin->audio_encode_queue);

					pad = gst_element_get_static_pad (encodebin->audio_encode_queue, "src");
					gst_pad_unlink(pad, encodebin->mux_audio_sinkpad);
					gst_object_unref(pad);
					pad = NULL;	

					if ( g_signal_handler_is_connected ( encodebin->audio_encode_queue, encodebin->veque_sig_id) )
					{
						g_signal_handler_disconnect (  encodebin->audio_encode_queue, encodebin->veque_sig_id );
					}					
				}
#endif
				else
				{
					pad = gst_element_get_static_pad (encodebin->audio_encode, "src");
					gst_pad_unlink(pad, encodebin->mux_audio_sinkpad);
					gst_object_unref(pad);
					pad = NULL;
				}			

				gst_element_release_request_pad(encodebin->mux, encodebin->mux_audio_sinkpad);
//				gst_object_unref(encodebin->mux_audio_sinkpad);		//***
				encodebin->mux_audio_sinkpad = NULL;
			}

			if(encodebin->asink_probeid)
			{
				pad = gst_element_get_static_pad (encodebin->audio_queue, "sink"); 
				gst_pad_remove_buffer_probe(pad, encodebin->asink_probeid);
				encodebin->asink_probeid =0;
				gst_object_unref(pad);
				pad = NULL;
			}

			break;
		case GST_ENCODE_BIN_PROFILE_IMAGE :
			if (encodebin->auto_color_space) {
				gst_element_unlink_many  (
					encodebin->image_queue,
					encodebin->image_toggle,
					encodebin->color_space,
					encodebin->icapsfilter,								
					encodebin->image_encode,
					NULL);
			}
			else {
				gst_element_unlink_many  (
					encodebin->image_queue,
					encodebin->image_toggle,	
					encodebin->icapsfilter,																
					encodebin->image_encode,
					NULL);	
			}
			break;
		default:
			  GST_WARNING_OBJECT(encodebin, "Invalid profile number = %d", encodebin->profile);
			  return FALSE;
		  break;
	}
	//	gst_pad_set_active(encodebin->srcpad, TRUE);	
	return TRUE;

}

static gboolean 
gst_encode_bin_init_video_elements (GstElement *element, gpointer user_data)
{
	GstEncodeBin *encodebin = GST_ENCODE_BIN (element);

	if(encodebin->profile != GST_ENCODE_BIN_PROFILE_AV)
		return FALSE;

	if(encodebin->video_queue == NULL) {
		encodebin->video_queue = gst_element_factory_make ("queue","video_queue");
		gst_bin_add (GST_BIN (element), encodebin->video_queue);
	}

	if( encodebin->use_video_toggle )
	{
		if( encodebin->video_toggle == NULL )
		{
			encodebin->video_toggle = gst_element_factory_make ("toggle","video_toggle");
			gst_bin_add (GST_BIN (element), encodebin->video_toggle);
		}
		GST_INFO_OBJECT( encodebin, "Video toggle is Enabled" );
	}
	else
	{
		GST_INFO_OBJECT( encodebin, "Video toggle is Disabled" );
	}

	if(encodebin->vcapsfilter == NULL) {
		encodebin->vcapsfilter = gst_element_factory_make ("capsfilter","vcapsfilter");
		gst_bin_add (GST_BIN (element), encodebin->vcapsfilter);
	}  
#if 0
	encodebin->vcaps = gst_caps_new_simple("video/x-raw-yuv",
		"format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('I', '4', '2', '0'),
		"width", G_TYPE_INT, 320,
		"height", G_TYPE_INT, 240,
		"framerate", GST_TYPE_FRACTION, 30, 1,
		NULL);  

#endif
	if(encodebin->video_encode == NULL) {
		encodebin->video_encode = gst_element_factory_make (encodebin->venc_name, NULL);
		gst_bin_add (GST_BIN (element), encodebin->video_encode);
	}

#if 0
	if(encodebin->video_encode_queue == NULL) {
		encodebin->video_encode_queue = gst_element_factory_make ("queue", "video_encode_queue");
		gst_bin_add (GST_BIN (element), encodebin->video_encode_queue);
	}

	g_object_set(G_OBJECT(encodebin->video_encode_queue), "max-size-bytes", (unsigned int)0, NULL);
#endif

	if(encodebin->mux == NULL) {
		encodebin->mux = gst_element_factory_make (encodebin->mux_name, "mux");
		gst_bin_add (GST_BIN (element), encodebin->mux);
	}

	if (!encodebin->video_encode 
//		|| !encodebin->video_encode_queue 
		|| !encodebin->mux 
		|| !encodebin->video_queue 
		|| !encodebin->vcapsfilter
		|| !encodebin->srcpad )
	{
		GST_ERROR_OBJECT(encodebin, "Faild create element \n");
		return FALSE;
	}
	
	if( encodebin->use_video_toggle	&& !encodebin->video_toggle )
	{
		GST_ERROR_OBJECT(encodebin, "Faild create video toggle element \n");
		return FALSE;
	}

#if 0  
  if (encodebin->auto_color_space && (encodebin->color_space == NULL)) {
  	encodebin->color_space = gst_element_factory_make ("ffmpegcolorspace","color_space");
      gst_bin_add (GST_BIN (element), encodebin->color_space);
   }  
#endif  
  return TRUE;
}

static gboolean 
gst_encode_bin_init_audio_elements (GstElement *element, gpointer user_data)
{
	GstEncodeBin *encodebin = GST_ENCODE_BIN (element);

	if(encodebin->profile > GST_ENCODE_BIN_PROFILE_AUDIO)
		return FALSE;

	if(encodebin->audio_queue == NULL) {
		encodebin->audio_queue = gst_element_factory_make ("queue","audio_queue");
		gst_bin_add (GST_BIN (element), encodebin->audio_queue);
	}

	if(encodebin->acapsfilter == NULL) {
		encodebin->acapsfilter = gst_element_factory_make ("capsfilter","acapsfilter");
		gst_bin_add (GST_BIN (element), encodebin->acapsfilter);
	}    
#if 0
encodebin->acaps = gst_caps_new_simple("audio/x-raw-int",
	"rate", G_TYPE_INT, 8000,
	"channels", G_TYPE_INT, 2,
	"depth", G_TYPE_INT, 16,
	NULL);
#endif

	if(encodebin->audio_encode == NULL) {
		encodebin->audio_encode = gst_element_factory_make (encodebin->aenc_name, "audio_encode");
		gst_bin_add (GST_BIN (element), encodebin->audio_encode);
	}

	if(encodebin->mux == NULL) {
		encodebin->mux = gst_element_factory_make (encodebin->mux_name, "mux");
		gst_bin_add (GST_BIN (element), encodebin->mux);
	}

	if (!encodebin->audio_encode 
		|| !encodebin->audio_queue
		|| !encodebin->mux
		|| !encodebin->acapsfilter    	
		|| !encodebin->srcpad )
	{
		GST_ERROR_OBJECT(encodebin, "Faild create element \n");
		return FALSE;
	}
#if 0
	if (encodebin->auto_audio_convert && (encodebin->audio_conv == NULL)) {
		encodebin->audio_conv = gst_element_factory_make ("audioconvert","audio_conv");
		gst_bin_add (GST_BIN (element), encodebin->audio_conv);
	} 

	if (encodebin->auto_audio_resample && (encodebin->audio_sample == NULL)) {
		encodebin->audio_sample = gst_element_factory_make ("audioresample","audio_sample");
		gst_bin_add (GST_BIN (element), encodebin->audio_sample);
	} 
#endif
	return TRUE;
}


static gboolean 
gst_encode_bin_init_image_elements (GstElement *element, gpointer user_data)
{
	GstEncodeBin *encodebin = GST_ENCODE_BIN (element);

	if(encodebin->profile < GST_ENCODE_BIN_PROFILE_IMAGE)
		return FALSE;

	if(encodebin->image_queue == NULL) {
		encodebin->image_queue = gst_element_factory_make ("queue","image_queue");
		gst_bin_add (GST_BIN (element), encodebin->image_queue);
	}

	if(encodebin->image_toggle == NULL) {
		encodebin->image_toggle = gst_element_factory_make ("toggle","image_toggle");
		gst_bin_add (GST_BIN (element), encodebin->image_toggle);
	}

	if(encodebin->icapsfilter == NULL) {
		encodebin->icapsfilter = gst_element_factory_make ("capsfilter","icapsfilter");
		gst_bin_add (GST_BIN (element), encodebin->icapsfilter);
	}  

	if(encodebin->image_encode == NULL) {
		encodebin->image_encode = gst_element_factory_make (encodebin->ienc_name, "image_encode");
		gst_bin_add (GST_BIN (element), encodebin->image_encode);
	}  

	if (!encodebin->image_encode 
		|| !encodebin->image_queue
		|| !encodebin->image_toggle
		|| !encodebin->icapsfilter
		|| !encodebin->srcpad )
	{
		GST_ERROR_OBJECT(encodebin, "Faild create element \n");
		return FALSE;
	}
#if 0
	if (encodebin->auto_color_space && (encodebin->color_space == NULL)) {
		encodebin->color_space = gst_element_factory_make ("ffmpegcolorspace","color_space");
		gst_bin_add (GST_BIN (element), encodebin->color_space);
	}  
#endif
	return TRUE;
}

static gboolean gst_encode_bin_block(GstEncodeBin *encodebin, gboolean value)
{
  
	if(value) { //block stream
		switch(encodebin->profile) {
			case GST_ENCODE_BIN_PROFILE_AV:
				if(encodebin->audio_queue == NULL && encodebin->video_queue == NULL) {
					goto block_fail;
				} else {
					if(g_object_class_find_property(G_OBJECT_GET_CLASS(GST_OBJECT(encodebin->video_queue)), 
						"empty-buffers") == NULL) {
						GST_ERROR_OBJECT(encodebin, "The queue element doesn't support 'empty-buffers' property");
						goto block_fail;
					}
					if( encodebin->video_toggle )
					{
						g_object_set(encodebin->video_toggle, "block-data", TRUE , NULL);
						GST_INFO_OBJECT( encodebin, "video_toggle block-data TRUE" );
					}
					
					g_object_set(encodebin->video_queue, "empty-buffers", TRUE , NULL);
					GST_INFO_OBJECT( encodebin, "video_queue empty-buffers TRUE" );
					if(encodebin->audio_queue != NULL)
					{
						g_object_set(encodebin->audio_queue, "empty-buffers", TRUE , NULL);
						GST_INFO_OBJECT( encodebin, "audio_queue empty-buffers TRUE" );
					}
				}
				break;
			case GST_ENCODE_BIN_PROFILE_AUDIO:
				if(encodebin->audio_queue == NULL) {
					goto block_fail;					
				} else {
					if(g_object_class_find_property(G_OBJECT_GET_CLASS(GST_OBJECT(encodebin->audio_queue)), 
						"empty-buffers") == NULL) {
						GST_ERROR_OBJECT(encodebin, "The queue element doesn't support 'empty-buffers' property");
						goto block_fail;
					}				
					g_object_set(encodebin->audio_queue, "empty-buffers", TRUE , NULL);
					GST_INFO_OBJECT( encodebin, "audio_queue empty-buffers TRUE" );
				}
				break;
			case GST_ENCODE_BIN_PROFILE_IMAGE:
				if(encodebin->image_toggle == NULL) {
					goto block_fail;
				} else {
					g_object_set(encodebin->image_toggle, "block_data", TRUE, NULL);
					GST_INFO_OBJECT( encodebin, "image_toggle block_data TRUE" );
				}
				break;	 
			default:
				GST_WARNING_OBJECT (encodebin,"Invalid profile number = %d", encodebin->profile);
				goto block_fail;
				break;	  
		}
	} else { //release blocked-stream
		switch(encodebin->profile) {
			case GST_ENCODE_BIN_PROFILE_AV:
				if(encodebin->audio_queue == NULL && encodebin->video_queue == NULL) {
					goto unblock_fail;
				} else {
					if(g_object_class_find_property(G_OBJECT_GET_CLASS(GST_OBJECT(encodebin->video_queue)), 
						"empty-buffers") == NULL) {
						GST_ERROR_OBJECT(encodebin, "The queue element doesn't support 'empty-buffers' property");
						goto unblock_fail;
					}				
					if( encodebin->video_toggle )
					{
						g_object_set(encodebin->video_toggle, "block-data", FALSE , NULL);
						GST_INFO_OBJECT( encodebin, "video_toggle block-data FALSE" );
					}
					
					if(encodebin->audio_queue != NULL)
					{					
						g_object_set(encodebin->audio_queue, "empty-buffers", FALSE , NULL);
						GST_INFO_OBJECT( encodebin, "audio_queue empty-buffers FALSE" );
					}
					g_object_set(encodebin->video_queue, "empty-buffers", FALSE , NULL);
					GST_INFO_OBJECT( encodebin, "video_queue empty-buffers FALSE" );
				}
				break;
			case GST_ENCODE_BIN_PROFILE_AUDIO:
				if(encodebin->audio_queue == NULL) {
					goto unblock_fail;					
				} else {		
					if(g_object_class_find_property(G_OBJECT_GET_CLASS(GST_OBJECT(encodebin->audio_queue)), 
						"empty-buffers") == NULL) {
						GST_ERROR_OBJECT(encodebin, "The queue element doesn't support 'empty-buffers' property");
						goto unblock_fail;
					}				
					g_object_set(encodebin->audio_queue, "empty-buffers", FALSE , NULL);
					GST_INFO_OBJECT( encodebin, "audio_queue empty-buffers FALSE" );
				}
				break;
			case GST_ENCODE_BIN_PROFILE_IMAGE:
				if(encodebin->image_toggle == NULL) {
					goto unblock_fail;
				} else {				
					g_object_set(encodebin->image_toggle, "block_data", FALSE, NULL);
					GST_INFO_OBJECT( encodebin, "image_toggle block_data FALSE" );
				}
				break;	 
			default:
				GST_WARNING_OBJECT (encodebin,"Invalid profile number = %d", encodebin->profile);
				goto unblock_fail;
				break;	  
		}	
	}
	encodebin->block = value;
	return TRUE;

block_fail:
	GST_ERROR_OBJECT(encodebin, "encodebin block failed");
	return FALSE;
	
unblock_fail:	
	GST_ERROR_OBJECT(encodebin, "encodebin unblock failed");	
	return FALSE;  
}

static gboolean gst_encode_bin_pause(GstEncodeBin *encodebin, gboolean value)
{
	GstClock *clock = NULL;

	if(value) { 
		/* pause stream*/
		//Block src of encode bin
		if (!gst_encode_bin_block(encodebin, TRUE))
		{
			GST_WARNING_OBJECT (encodebin, "Fail to block Encodebin.");
			goto pause_fail;
		}

		if (encodebin->paused_time == 0)
		{
			//get steam time
			if (clock = GST_ELEMENT_CLOCK(encodebin))		//before PLAYING, this would be NULL. Need to check.
			{
				GstClockTime current_time, base_time;

				current_time = gst_clock_get_time(clock);
				base_time = gst_element_get_base_time(GST_ELEMENT(encodebin));

				encodebin->paused_time = current_time - base_time;
				
				GST_INFO_OBJECT (encodebin, "Encodebin is in running-pause at [%"GST_TIME_FORMAT"]."
					, GST_TIME_ARGS(encodebin->paused_time));
			}
			else
			{
				encodebin->paused_time = 0;
				encodebin->total_offset_time = 0;
				
				GST_WARNING_OBJECT (encodebin, "There is no clock in Encodebin.");
			}
		}
#if 0 //def GST_ENCODE_BIN_SIGNAL_ENABLE		
		g_signal_emit (G_OBJECT (encodebin), gst_encode_bin_signals[SIGNAL_STREAM_PAUSE], 0, TRUE);
#endif		
	}
	else { 
		/* release paused-stream*/
		if (encodebin->paused_time != 0)
		{
			if (clock = GST_ELEMENT_CLOCK(encodebin))
			{
				GstClockTime current_time, base_time;
				GstClockTime paused_gap;

				current_time = gst_clock_get_time(clock);
				base_time = gst_element_get_base_time(GST_ELEMENT(encodebin));
				paused_gap = current_time - base_time - encodebin->paused_time;
				
				encodebin->total_offset_time += paused_gap;
				encodebin->paused_time = 0;
				
				GST_INFO_OBJECT (encodebin, "Encodebin now resumes. Offset delay [%"GST_TIME_FORMAT"], Total offset delay [%"GST_TIME_FORMAT"]"
					, GST_TIME_ARGS(paused_gap) , GST_TIME_ARGS(encodebin->total_offset_time));
			}
			else
			{
				encodebin->paused_time = 0;
				encodebin->total_offset_time = 0;
				
				GST_WARNING_OBJECT (encodebin, "There is no clock in Encodebin.");
			}
		}

		//TODO : How about qos?

		//Unblock src of encode bin
		if (!gst_encode_bin_block(encodebin, FALSE))
		{
			GST_WARNING_OBJECT (encodebin, "Fail to Unblock Encodebin.");
			goto resume_fail;
		}
#if 0 //def GST_ENCODE_BIN_SIGNAL_ENABLE		
		g_signal_emit (G_OBJECT (encodebin), gst_encode_bin_signals[SIGNAL_STREAM_RESUME], 0, TRUE);
#endif		
	}
	encodebin->pause = value;
	return TRUE;

pause_fail:
	GST_WARNING_OBJECT (encodebin, "Fail to pause Encodebin");
#ifdef GST_ENCODE_BIN_SIGNAL_ENABLE		
		g_signal_emit (G_OBJECT (encodebin), gst_encode_bin_signals[SIGNAL_STREAM_PAUSE], 0, FALSE);
#endif	
	return FALSE;

resume_fail:	
	GST_WARNING_OBJECT (encodebin, "Fail to resume Encodebin");
#ifdef GST_ENCODE_BIN_SIGNAL_ENABLE		
		g_signal_emit (G_OBJECT (encodebin), gst_encode_bin_signals[SIGNAL_STREAM_RESUME], 0, FALSE);
#endif		
	return FALSE;	
}

static gboolean
gst_encode_bin_release_pipeline (GstElement *element,
      gpointer user_data)
{
#if 0
  GstEncodeBin *encodebin = GST_ENCODE_BIN (element);

  gst_element_set_state (encodebin->audio_queue, GST_STATE_NULL);
  gst_element_set_state (encodebin->audio_encode, GST_STATE_NULL);
  gst_element_set_state (encodebin->video_queue, GST_STATE_NULL);
  gst_element_set_state (encodebin->video_encode, GST_STATE_NULL);
  gst_element_set_state (encodebin->mux, GST_STATE_NULL);

  if (encodebin->auto_video_scale) {
    gst_element_set_state (encodebin->video_scale, GST_STATE_NULL);
    gst_element_unlink (encodebin->video_queue, encodebin->video_scale);
    gst_element_unlink (encodebin->video_scale, encodebin->video_encode);
    gst_bin_remove (GST_BIN (element), encodebin->video_scale);

    encodebin->video_scale = NULL;
  } else {
    gst_element_unlink (encodebin->video_queue, encodebin->video_encode);
  }

  gst_pad_unlink (gst_element_get_pad (encodebin->audio_encode, "src"), 
                       encodebin->mux_audio_sinkpad);
  gst_pad_unlink (gst_element_get_pad (encodebin->video_encode, "src"), 
                       encodebin->mux_video_sinkpad);

  gst_bin_remove_many (GST_BIN (element),
                               encodebin->audio_queue,
                               encodebin->audio_encode,
                               encodebin->video_queue,
                               encodebin->video_encode,
                               encodebin->mux,
                               NULL);

  encodebin->audio_queue = NULL;
  encodebin->audio_encode = NULL;
  encodebin->video_queue = NULL;
  encodebin->video_encode = NULL;
  encodebin->mux = NULL;
#endif
  return TRUE;
}

static gboolean
gst_encode_bin_audsink_set_caps (GstPad * pad, GstCaps * vscaps)
{
	GstEncodeBin *encodebin;
	GstStructure *structure;
//	const gchar *mimetype;
//	const GValue *codec_data;
	gint channels, rate;

	encodebin = GST_ENCODE_BIN (gst_pad_get_parent (pad));  

	structure = gst_caps_get_structure (vscaps, 0);
//	mimetype = gst_structure_get_name (structure);

	/* we want these for all */
	if (!gst_structure_get_int (structure, "channels", &channels) ||
		!gst_structure_get_int (structure, "rate", &rate)) {
			goto refuse_caps;
	}

//	codec_data = gst_structure_get_value (structure, "codec_data");
	/* FIXME */
	//  g_return_val_if_fail (!strcmp (mimetype, "audio/x-raw-int"), FALSE);
	gst_object_unref (encodebin);
	return TRUE;

refuse_caps:
	{
		GST_WARNING_OBJECT (encodebin, "refused caps %" GST_PTR_FORMAT, vscaps);
		gst_object_unref (encodebin);
		return FALSE;
	}
}

static gboolean
gst_encode_bin_vidsink_set_caps (GstPad * pad, GstCaps * vscaps)
{
	GstEncodeBin *encodebin;
	GstStructure *structure;
//	const gchar *mimetype;
	const GValue *fps;
//	const GValue *codec_data;
	gint width, height;

	encodebin = GST_ENCODE_BIN (gst_pad_get_parent (pad));

	structure = gst_caps_get_structure (vscaps, 0);
//	mimetype = gst_structure_get_name (structure);

	if (!gst_structure_get_int (structure, "width", &width) ||
		!gst_structure_get_int (structure, "height", &height)) {
			goto refuse_caps;
	}

//	codec_data = gst_structure_get_value (structure, "codec_data");

	fps = gst_structure_get_value (structure, "framerate");
	if (fps == NULL)
		goto refuse_caps;

	encodebin->fps = gst_value_get_fraction_numerator(fps);

	if(encodebin->high_speed_fps > 0)
	{
		encodebin->multiple =(encodebin->high_speed_fps)/(encodebin->fps);
	}
	
	{
		guint32 format;

		gst_structure_get_fourcc (structure, "format", &format);

		switch (format) {
			case GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'):
				break;
			case GST_MAKE_FOURCC ('I', '4', '2', '0'):
				break;
		}
	}
/* FIXME */
//  g_return_val_if_fail (!strcmp (mimetype, "video/x-raw-yuv"), FALSE);

#if 0
	switch (encodebin->profile) {
	case GST_ENCODE_BIN_PROFILE_3GP:
	//   need_videoscale
		break;
	case GST_ENCODE_BIN_PROFILE_MP4:
		break;
	default :
		break;
	}
#endif
	gst_object_unref (encodebin);
	return TRUE;

refuse_caps:
	{
		GST_WARNING_OBJECT (encodebin, "refused caps %" GST_PTR_FORMAT, vscaps);
		gst_object_unref (encodebin);
		return FALSE;
	}
}

static gboolean gst_encode_bin_imgsink_set_caps (GstPad * pad, GstCaps * vscaps)
{
	GstEncodeBin *encodebin;
	GstStructure *structure;
//	const gchar *mimetype;
//	const GValue *codec_data;
	gint width, height;

	encodebin = GST_ENCODE_BIN (gst_pad_get_parent (pad));

	structure = gst_caps_get_structure (vscaps, 0);
//	mimetype = gst_structure_get_name (structure);

	if (!gst_structure_get_int (structure, "width", &width) ||
		!gst_structure_get_int (structure, "height", &height)) {
			goto refuse_caps;
	}

//	codec_data = gst_structure_get_value (structure, "codec_data");

	{
		guint32 format;

		gst_structure_get_fourcc (structure, "format", &format);

		switch (format) {
			case GST_MAKE_FOURCC ('Y', 'U', 'Y', '2'):
				break;
			case GST_MAKE_FOURCC ('I', '4', '2', '0'):
				break;
		}
	}
	gst_object_unref (encodebin);	
	return TRUE;
	
refuse_caps:
	{
		GST_WARNING_OBJECT (encodebin, "refused caps %" GST_PTR_FORMAT, vscaps);
		gst_object_unref (encodebin);
		return FALSE;
	}  

}

static gboolean
gst_encode_bin_video_probe(GstPad *pad, GstBuffer *buffer, GstEncodeBin *encodebin)
{
	if (!encodebin)
	{
		GST_WARNING_OBJECT (encodebin, "encodebin is Null.");
		return TRUE;
	}
	
	//Adjusting timestamp of video source
	GST_BUFFER_TIMESTAMP(buffer) -= encodebin->total_offset_time;

	return TRUE;
}

static gboolean
gst_encode_bin_video_probe_hs(GstPad *pad, GstBuffer *buffer, GstEncodeBin *encodebin)
{
	if (!encodebin)
	{
		GST_WARNING_OBJECT (encodebin, "encodebin is Null.");
		return TRUE;
	}
	
	GST_BUFFER_TIMESTAMP(buffer)	*= encodebin->multiple;
	return TRUE;
}

static gboolean
gst_encode_bin_audio_probe(GstPad *pad, GstBuffer *buffer, GstEncodeBin *encodebin)
{
	if (!encodebin)
	{
		GST_WARNING_OBJECT (encodebin, "encodebin is Null.");
		return TRUE;
	}
	
	//Adjusting timestamp of video source
	GST_BUFFER_TIMESTAMP(buffer) -= encodebin->total_offset_time;

	return TRUE;
}

static GstPad*
gst_encode_bin_get_mux_sink_pad(GstElement *mux, GstEncodeBinMuxSinkPad type)
{
	GstElementClass *elemclass = NULL;
	GList *walk = NULL;
	GstPad *pad = NULL;

	elemclass = GST_ELEMENT_GET_CLASS (mux);

	walk = gst_element_class_get_pad_template_list (elemclass);

	while (walk) {
		GstPadTemplate *templ;

		templ = (GstPadTemplate *) walk->data;
		if (GST_PAD_TEMPLATE_DIRECTION (templ) == GST_PAD_SINK) {
			/* ENHANCE ME: Please add other specific mux's case */
			if (((type == ENCODEBIN_MUX_AUDIO_SINK) && strstr(GST_PAD_TEMPLATE_NAME_TEMPLATE (templ), "audio")) ||	//audio, audio_%d,... ex)ffmux_3gp
				((type == ENCODEBIN_MUX_VIDEO_SINK) && strstr(GST_PAD_TEMPLATE_NAME_TEMPLATE (templ), "video")) ||	//video, video_%d,... ex)ffmux_3gp
				strstr(GST_PAD_TEMPLATE_NAME_TEMPLATE (templ), "sink")	//sink, sink_%d, wavparse_sink, ... ex)oggmux, wavparse
			) {
				g_print("PRINT TEMPLATE(%s)\n", GST_PAD_TEMPLATE_NAME_TEMPLATE (templ));
				pad = gst_element_get_request_pad (mux, GST_PAD_TEMPLATE_NAME_TEMPLATE (templ));
				break;
			}
		}
		walk = g_list_next (walk);
	}

	return pad;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_encode_bin_debug, "encodebin", 0, "encoder bin");

#ifdef ENABLE_NLS
  GST_DEBUG ("binding text domain %s to locale dir %s", GETTEXT_PACKAGE,
      LOCALEDIR);
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
#endif /* ENABLE_NLS */

  return gst_element_register (plugin, "encodebin", GST_RANK_NONE,
      GST_TYPE_ENCODE_BIN);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "encodebin",
    "EXT encoder bin", 
    plugin_init, VERSION, "LGPL", "Samsung Electronics Co", "http://www.samsung.com/")
