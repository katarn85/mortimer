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

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "sound/sdp_srp_ioctl.h"

/* Object header */
#include "spdifsink.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static const unsigned long buffer_size = 32 * 1024;

unsigned int frag = 8;

static unsigned long long get_time()
{
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	return ((ts.tv_sec * GST_SECOND + ts.tv_nsec * GST_NSECOND)/1000);
}

GST_DEBUG_CATEGORY_STATIC (gst_debug_spdifsink);
#define GST_CAT_DEFAULT gst_debug_spdifsink

static void
_do_init (GType object_type)
{
}

#ifdef INHERIT_GSTBASESINK
GST_BOILERPLATE_FULL (GstSpdifSink, gst_spdifsink, GstBaseSink,
    GST_TYPE_BASE_SINK, _do_init);

enum
{
  PROP_0,
  PROP_FAST_RENDERING,
  PROP_LAST
};

enum
{
  FAST_RENDERING_OFF,
  FAST_RENDERING_REQUESTED,
  FAST_RENDERING_APPLIED,
};

static GstStateChangeReturn gst_spdifsink_change_state (GstElement * element, GstStateChange transition);

static void gst_spdifsink_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_spdifsink_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean gst_spdifsink_event (GstBaseSink * bsink, GstEvent * event);

#else
GST_BOILERPLATE_FULL (GstSpdifSink, gst_spdifsink, GstAudioSink,
    GST_TYPE_AUDIO_SINK, _do_init);

//static void gst_spdifsink_finalise (GObject * object);
//static void gst_spdifsink_set_property (GObject * object,
//    guint prop_id, const GValue * value, GParamSpec * pspec);
//static void gst_spdifsink_get_property (GObject * object,
//    guint prop_id, GValue * value, GParamSpec * pspec);

//static GstCaps *gst_spdifsink_getcaps (GstBaseSink * bsink);

static gboolean gst_spdifsink_open (GstAudioSink * asink);
// static gboolean gst_spdifsink_prepare (GstAudioSink * asink, GstRingBufferSpec * spec);
//static gboolean gst_spdifsink_unprepare (GstAudioSink * asink);
static gboolean gst_spdifsink_close (GstAudioSink * asink);
static guint gst_spdifsink_write (GstAudioSink * asink, gpointer data,
    guint length);
//static guint gst_spdifsink_delay (GstAudioSink * asink);
static void gst_spdifsink_reset (GstAudioSink * asink);
#endif


static GstStaticPadTemplate gst_spdifsink_sink_template_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
	    GST_PAD_SINK,
	    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-spdif-es; ")
    );

static void
gst_spdifsink_finalize (GObject * object)
{
	GST_DEBUG_OBJECT(object, "");
	//GstSpdifSink *spdifsink = (GstSpdifSink*) object;
	return;
}

static gboolean
gst_spdifsink_setcaps (GstPad * pad, GstCaps * caps)
{
	gboolean bRet = FALSE;
	GstSpdifSink* parent = (GstSpdifSink*)(gst_pad_get_parent (pad));
	GST_DEBUG_OBJECT(pad, "begin[ %s ]", GST_PAD_NAME(pad));
	return bRet;
}

static void
gst_spdifsink_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (gstelement_class, "TV SPDIF OUTPUT",
      "Sink/Audio",
      "For TV SPDIF OUTPUT",
      "author< >");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_spdifsink_sink_template_factory);
}


static gboolean spdif_open(GstSpdifSink* ssink)
{
	unsigned int card;
	unsigned int spdif_device;
	if (ssink->srp_dev_spdif == -1) {
		ssink->spdif_codec.id = ssink->spdif_codec_type;
		ssink->spdif_codec.ch_in = 2;
		ssink->spdif_codec.ch_out = 2;
		ssink->spdif_codec.sample_rate = 44100;
		ssink->spdif_codec.bit_rate = 16;
		ssink->spdif_codec.rate_control = 0;
		ssink->spdif_codec.profile = 0;
		ssink->spdif_codec.level = 0;
		ssink->spdif_codec.ch_mode = 0;
		ssink->spdif_codec.format = 0;
		if ((buffer_size != 0) && (frag != 0)) {
			ssink->spdif_config.fragment_size = buffer_size/frag;
			ssink->spdif_config.fragments = frag;
		} else {
			/* use driver defaults */
			ssink->spdif_config.fragment_size = 0;
			ssink->spdif_config.fragments = 0;
		}
		ssink->spdif_config.codec = &ssink->spdif_codec;
		card=2;
		spdif_device=7;
		ssink->sdp_spdif_compress = compress_open(card, spdif_device, COMPRESS_IN, &ssink->spdif_config);
		
		if (!ssink->sdp_spdif_compress || !is_compress_ready(ssink->sdp_spdif_compress)) {
			GST_DEBUG_OBJECT(ssink, "Unable to open Compress device %d:%d",
					card, spdif_device);
			GST_ERROR_OBJECT(ssink, "ERR: %s", compress_get_error(ssink->sdp_spdif_compress));
			return -1;
		};
		
		ssink->srp_dev_spdif = ssink->sdp_spdif_compress->fd;
		return TRUE;
	}
	return TRUE;
}

static gboolean spdif_close(GstSpdifSink* ssink)
{
	guint64 T1 = get_time();
	 if (ssink->sdp_spdif_compress) {
		if (ssink->sdp_spdif_compress->fd != -1) {
			compress_close(ssink->sdp_spdif_compress);
			ssink->sdp_spdif_compress = NULL; 
		}
    } else {
		GST_ERROR_OBJECT(ssink, "Device is not opened");
	}
	guint64 T2 = get_time();
	if((T2-T1)>100000)
		GST_ERROR_OBJECT(ssink, "compress_close[ %lld ms ]", (T2-T1)/1000);
	return TRUE;
}

static guint spdifsink_write (GstSpdifSink * ssink, gpointer data, guint length, GstClockTime timestamp)
{
	int ret = length;
	guint64 T1 = get_time();
	if (data == NULL) {
		GST_ERROR_OBJECT(ssink, "Input data is NULL");
		return length;
	}

	if (ssink->sdp_spdif_compress) {
		GST_LOG_OBJECT(ssink, "data[ %x ], length[ %d ]", data, length);
		ret = compress_write(ssink->sdp_spdif_compress, data, length);
		if (ret < 0) {
			GST_WARNING_OBJECT(ssink, "Error playing sample, ret[ %d ], But, ignore it as driver's requirement.", ret);
			ret = length;
		}

		if (ret != length) {
			/* TODO: Buffer pointer needs to be set here */
			GST_ERROR_OBJECT(ssink, "Error in write:size[%d] ret[%d] %d", length, ret);
		}

		if (!ssink->compress_spdif_start) {
			ret = compress_start(ssink->sdp_spdif_compress);
			if (ret < 0) {
				return ret;
			}
			ssink->compress_spdif_start = 1;
		}
	}
	guint64 T2 = get_time();
	GST_DEBUG_OBJECT(ssink, "ts[ %"GST_TIME_FORMAT" ], data[ %x ], length[ %d ]  [ %lld ms ]", GST_TIME_ARGS(timestamp), data, length, (T2-T1)/1000);
	if((T2-T1)/1000>1000)
		GST_ERROR_OBJECT(ssink,"spdifsink_write[ %lld ms ]", (T2-T1)/1000);
	return length; 
}

static gboolean spdifsink_reset (GstSpdifSink * ssink)
{
	//Current: nothing to do here... , 
	//TODO: need to check the API sequence
	return TRUE;
}


GST_DEBUG_CATEGORY_EXTERN(GST_CAT_AREN);

#ifdef INHERIT_GSTBASESINK

static GstFlowReturn
gst_spdifsink_render(GstBaseSink *sink, GstBuffer *buffer)
{
	GstSpdifSink* ssink = GST_SPDIFSINK(sink);
	guint size = GST_BUFFER_SIZE(buffer);
	guint ret = spdifsink_write(ssink, GST_BUFFER_DATA(buffer), size, GST_BUFFER_TIMESTAMP(buffer));
	if (ret < size)
		return GST_FLOW_ERROR;

	if (ssink->start_fast_rendering == FAST_RENDERING_REQUESTED) {
		/* MUST BE SET AFTER FIRST RENDERING FOR AV-SYNC */
		ssink->start_fast_rendering = FAST_RENDERING_APPLIED;
		gst_base_sink_set_max_lateness (sink, ssink->max_fast_rendering_size);
		gst_base_sink_set_ts_offset (sink, ssink->max_fast_rendering_size*(-1));
	}

	return GST_FLOW_OK;
}


static void
gst_spdifsink_init (GstSpdifSink * spdifsink,
    GstSpdifSinkClass * g_class)
{
	GstBaseSink* basesink = GST_BASE_SINK(spdifsink);
	spdifsink->device_fd = -1;
	spdifsink->sdp_spdif_compress = NULL;
	spdifsink->srp_dev_spdif = -1;
	spdifsink->compress_spdif_start=0;
	spdifsink->spdif_codec_type = AUDIO_AC3;
	spdifsink->start_fast_rendering = FAST_RENDERING_OFF;
	spdifsink->max_fast_rendering_size = 0;
	basesink->debugCategory = GST_CAT_AREN;

	gst_base_sink_set_max_lateness (GST_BASE_SINK (spdifsink), 40 * GST_MSECOND);	// MANDATORY, make gstbasesink to be able to drop any data if it's late.
	GST_INFO_OBJECT(spdifsink, "set max_lateness[ 40 ms ]");
}

static void
gst_spdifsink_class_init (GstSpdifSinkClass * klass)
{
  GstElementClass *gstelement_class;
  GstBaseSinkClass* basesink_class = GST_BASE_SINK_CLASS(klass);
  GObjectClass *gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_spdifsink_change_state);
  basesink_class->render = GST_DEBUG_FUNCPTR(gst_spdifsink_render);
  basesink_class->event = GST_DEBUG_FUNCPTR (gst_spdifsink_event);
  gobject_class->get_property = GST_DEBUG_FUNCPTR(gst_spdifsink_get_property);
  gobject_class->set_property = GST_DEBUG_FUNCPTR(gst_spdifsink_set_property);


  g_object_class_install_property (gobject_class, PROP_FAST_RENDERING,
      g_param_spec_uint64 ("fast-rendering", "Fast Rendering",
          "Fast rendering to avoid underflow (use buffering into driver)"
          , 0, G_MAXINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static GstStateChangeReturn
gst_spdifsink_change_state (GstElement * element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstSpdifSink* ssink = GST_SPDIFSINK(element);
	GST_INFO_OBJECT(ssink, "transition[ %d -> %d ]", GST_STATE_TRANSITION_CURRENT(transition), GST_STATE_TRANSITION_NEXT(transition));
	switch (transition) {
		case GST_STATE_CHANGE_READY_TO_PAUSED:
			if(spdif_open(ssink) != TRUE)
			{
				GST_ERROR_OBJECT(ssink,"failed to open device");
				return GST_STATE_CHANGE_FAILURE;
			}
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			spdif_close(ssink);
			break;
		default:
			break;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
	GST_DEBUG_OBJECT(ssink, "ret[ %d ], transition[ %d -> %d ]", ret, GST_STATE_TRANSITION_CURRENT(transition), GST_STATE_TRANSITION_NEXT(transition));

	switch (transition) {
		case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
			if (ssink->max_fast_rendering_size > 0) {
				GST_INFO_OBJECT(ssink, "PlayingToPause - reset ts-offset / max-lateness to zero");
				ssink->start_fast_rendering = FAST_RENDERING_REQUESTED;
				gst_base_sink_set_ts_offset(GST_BASE_SINK(ssink), 0);
				gst_base_sink_set_max_lateness(GST_BASE_SINK(ssink), 0);
			}
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
		case GST_STATE_CHANGE_READY_TO_NULL:
		default:
			break;
	}

	return ret;
}

static gboolean
gst_spdifsink_event (GstBaseSink * bsink, GstEvent * event)
{
	GstSpdifSink* spdifsink = GST_SPDIFSINK(bsink);
	switch (GST_EVENT_TYPE (event)) {
		case GST_EVENT_FLUSH_START:
			break;
		case GST_EVENT_FLUSH_STOP:
			if (spdifsink->max_fast_rendering_size > 0) {
				GST_INFO_OBJECT(spdifsink, "FlushStop - reset ts-offset / max-lateness to zero");
				spdifsink->start_fast_rendering = FAST_RENDERING_REQUESTED;
				gst_base_sink_set_ts_offset(bsink, 0);
				gst_base_sink_set_max_lateness(bsink, 0);
			}
			break;
		default:
			break;
	}
	return TRUE;
}

static void
gst_spdifsink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
	GstBaseSink *sink = GST_BASE_SINK (object);
	GstSpdifSink* spdifsink = GST_SPDIFSINK(object);

	switch (prop_id) {
		case PROP_FAST_RENDERING:
			GST_DEBUG_OBJECT(spdifsink, "old max_fast_rendering_size[ %lld nsec]", spdifsink->max_fast_rendering_size);
			spdifsink->max_fast_rendering_size = g_value_get_uint64 (value);
			if (spdifsink->max_fast_rendering_size > 0)
				spdifsink->start_fast_rendering = FAST_RENDERING_REQUESTED;
			else
				spdifsink->start_fast_rendering = FAST_RENDERING_OFF;
			GST_DEBUG_OBJECT(spdifsink, "new max_fast_rendering_size[ %lld nsec]", spdifsink->max_fast_rendering_size);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gst_spdifsink_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
	GstBaseSink *sink = GST_BASE_SINK (object);
	GstSpdifSink* spdifsink = GST_SPDIFSINK(object);

	switch (prop_id) {
		case PROP_FAST_RENDERING:
			g_value_set_uint64 (value, spdifsink->max_fast_rendering_size);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}


#else
static void
gst_spdifsink_init (GstSpdifSink * spdifsink,
    GstSpdifSinkClass * g_class)
{
	GstBaseAudioSink* baseaudiosink = GST_BASE_AUDIO_SINK(spdifsink);
	spdifsink->device_fd = -1;
	gst_base_audio_sink_set_provide_clock(baseaudiosink, FALSE);
}

static gboolean gst_spdifsink_open (GstAudioSink * asink)
{
	GstSpdifSink* ssink = GST_SPDIFSINK(asink);
	return spdif_open(ssink);
}

static gboolean gst_spdifsink_close (GstAudioSink * asink)
{
	GstSpdifSink* ssink = GST_SPDIFSINK(asink);
	return spdif_close(ssink);
}

static guint gst_spdifsink_write (GstAudioSink * asink, gpointer data, guint length)
{
	GstSpdifSink* ssink = GST_SPDIFSINK(asink);
	return spdifsink_write(ssink, data, length);
}

static void gst_spdifsink_reset (GstAudioSink * asink)
{
	GstSpdifSink* ssink = GST_SPDIFSINK(asink);
	return spdifsink_reset(ssink);
}

static void
gst_spdifsink_class_init (GstSpdifSinkClass * klass)
{
  GstAudioSinkClass *gstaudiosink_class = (GstAudioSinkClass *) klass;

  gstaudiosink_class->open = GST_DEBUG_FUNCPTR (gst_spdifsink_open);
  gstaudiosink_class->close = GST_DEBUG_FUNCPTR (gst_spdifsink_close);
  gstaudiosink_class->write = GST_DEBUG_FUNCPTR (gst_spdifsink_write);
  gstaudiosink_class->reset = GST_DEBUG_FUNCPTR (gst_spdifsink_reset);
}
#endif


static gboolean
plugin_init (GstPlugin * plugin)
{
	if (!gst_element_register (plugin, "spdifsink", GST_RANK_PRIMARY, GST_TYPE_SPDIFSINK))
		return FALSE;

	GST_DEBUG_CATEGORY_INIT (gst_debug_spdifsink, "spdifsink", 0, "spdifsink element");
	return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "spdifsink",
    "TV SPDIF OUTPUT",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)

