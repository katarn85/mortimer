/* Mixersink
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
#include <gst/gst.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <gst/video/video.h>
#include "mixersink.h"
#include "config.h"
#include <sys/playermixer/player_mixer.h>
#include <audio-session-manager.h>

GST_DEBUG_CATEGORY_STATIC (gst_debug_mixersink);
#define GST_CAT_DEFAULT gst_debug_mixersink

static void
_do_init (GType object_type)
{
}

GST_BOILERPLATE_FULL (GstMixerSink, gst_mixersink, GstBaseSink,
    GST_TYPE_BASE_SINK, _do_init);

static GstStateChangeReturn gst_mixersink_change_state (GstElement * element, GstStateChange transition);

static GstStaticPadTemplate gst_mixersink_sink_template_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
	    GST_PAD_SINK,
	    GST_PAD_ALWAYS,
        GST_STATIC_CAPS ("video/x-raw-rgb, "
        "format =  (fourcc) { I420, STV0, STV1}, "
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], "
        "height = (int) [ 1, MAX ]; "
        "video/x-raw-yuv, "
        "format = (fourcc) {I420, STV0, STV1}, "
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
);

static void
gst_mixersink_finalize(GObject* object)
{
	GST_DEBUG_OBJECT(object,"");
	return;
}

static GstCaps *
gst_mixersink_getcaps (GstBaseSink * bsink)
{
	gchar *src_caps_str = NULL;
	src_caps_str = gst_caps_to_string(gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD(bsink)));
	GST_DEBUG_OBJECT(bsink,"pad caps[%s]\n ",src_caps_str);
	g_free(src_caps_str);
  return
      gst_caps_copy(gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD(bsink)));
}

static void
gst_mixersink_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);
  gst_element_class_set_details_simple (gstelement_class, 
  	"TV mixer OUTPUT",
      "Sink/Video",
      "For TV mixer OUTPUT",
      "author< >");
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_mixersink_sink_template_factory);
}

static gboolean 
mixer_open(GstMixerSink* mixersink)
{
	GST_DEBUG_OBJECT(mixersink, "");
	player_mixer_change_state(4);
	return TRUE;
}

static gboolean 
mixer_close(GstMixerSink* mixersink)
{
	GST_DEBUG_OBJECT(mixersink,"");
	return TRUE;
}

typedef struct _position
{
	unsigned int x;
	unsigned int y;
	unsigned int w;
	unsigned int h;
}position;

static GstFlowReturn
gst_mixersink_render(GstBaseSink *sink, GstBuffer *buffer)
{
	GstMixerSink* mixersink = GST_MIXERSINK(sink);
	guint size = GST_BUFFER_SIZE(buffer);
	//g_print("gst_mixersink_render+++handle[%d]\n",mixersink->mixerdisplayhandle);
	player_mixer_mixing(mixersink->mixerdisplayhandle,buffer);
	return GST_FLOW_OK;
}

static void
gst_mixersink_init (GstMixerSink * mixersink,
    GstMixerSinkClass * g_class)
{	
	GstBaseSink* basesink = GST_BASE_SINK(mixersink);
	gchar *src_caps_str = NULL;
	src_caps_str = gst_caps_to_string(gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD(mixersink)));
	GST_DEBUG_OBJECT(mixersink,"mixersink pad caps[%s]\n ",src_caps_str);
	g_free(src_caps_str);
	gst_pad_set_caps(GST_BASE_SINK_PAD(basesink),gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD(basesink)));
	gst_pad_set_caps(GST_BASE_SINK_PAD(mixersink),gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD(basesink)));
	mixersink->device_fd = -1;
	mixersink->mixerdisplayhandle = -1;
	g_print("gst_mixersink_init+++handle[%d]\n",mixersink->mixerdisplayhandle);
	basesink->debugCategory = GST_CAT_DEFAULT;
}

static gboolean
gst_mixersink_query(GstBaseSink * bsink, GstQuery * query)
{
	GstStructure* structure = NULL;
	gboolean ret = FALSE;
	Display* pdisp = NULL;
	player_mixer_get_disp(&pdisp);
	GST_DEBUG_OBJECT(bsink,"gst_mixersink_query, pdisp[%p] ",pdisp);
	static bool bscalerused = false;
	switch (query->type)
	{
		case GST_QUERY_CUSTOM:
		{
			structure = gst_query_get_structure (query);
			if(gst_structure_has_name(structure, "tbm_auth") == FALSE)
			{
				return FALSE;
			}	
			
			if(pdisp)
			{  
				gst_structure_set(structure,"TBMauthentication",G_TYPE_POINTER,pdisp,NULL);
				ret = TRUE;
			}
			else
				GST_ERROR_OBJECT(bsink,"pdis is NULL");
		}
		break;
		case GST_QUERY_RESOURCE:
		{
			if(!bscalerused)
			{
				GST_DEBUG_OBJECT(bsink,"RESOURCE QUERY - RESOURCE CATEGORY[ ASM_RESOURCE_SCALER ]");
				gst_query_add_resource(query, ASM_RESOURCE_SCALER);
				ret = TRUE;
				bscalerused = true;
			}
		}
		break;
		default:
		{
			GST_DEBUG_OBJECT(bsink,"Not query for tbm or getting scaler resource ");
			ret = GST_ELEMENT_CLASS (parent_class)->query(GST_ELEMENT(bsink), query);
		}
		break;
	}
	return ret;
}

enum
{
	PROP_0,
	PROP_DISPLAYHANDLE,
	PROP_DEVICE_ATTR_SCALER,
};

static void
gst_mixersink_get_property(GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
	GstMixerSink* mixersink = GST_MIXERSINK (object);
  switch (prop_id) {
    case PROP_DISPLAYHANDLE:
      g_value_set_int (value, mixersink->mixerdisplayhandle);
      break;
    default:
      		g_print("NOT Get property\n");
      break;
  	}
  }
  
static void
gst_mixersink_set_property (GObject * object,guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
	GstMixerSink* mixersink = GST_MIXERSINK (object);
	switch(prop_id){
	case PROP_DISPLAYHANDLE:
	{
		GST_DEBUG_OBJECT(mixersink,"gst_mixersink_set_property+++handle[%d]\n",mixersink->mixerdisplayhandle);
		mixersink->mixerdisplayhandle = g_value_get_int(value);
	}
	break;
	case PROP_DEVICE_ATTR_SCALER:
	{
		GST_DEBUG_OBJECT(mixersink,"gst_mixersink_set_property+++scalerid[%d]\n",g_value_get_int(value));
		player_mixer_set_property(g_value_get_int(value));
	}
	break;
	default:
	{
		GST_DEBUG_OBJECT(mixersink,"NOT Set property\n");
	}
	break;
	}
}

static void
gst_mixersink_class_init (GstMixerSinkClass * klass)
{
	GST_DEBUG_OBJECT(klass,"mixersink_class_init");
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	GstBaseSinkClass* basesink_class;
	
	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;
	basesink_class = (GstBaseSinkClass *)(klass);
	
	gstelement_class->query = gst_mixersink_query;
	gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_mixersink_change_state);
	basesink_class->query = GST_DEBUG_FUNCPTR(gst_mixersink_query);
	basesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_mixersink_getcaps);
	basesink_class->render = GST_DEBUG_FUNCPTR(gst_mixersink_render);
	gobject_class->set_property = gst_mixersink_set_property;
	gobject_class->get_property = gst_mixersink_get_property;
    g_object_class_install_property (gobject_class, PROP_DISPLAYHANDLE,
            g_param_spec_int("mixerdisplayhandle", "MixerDisplayhandle",
          "Mixer Display handle",-1000, 1000, 0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_DEVICE_ATTR_SCALER,
      g_param_spec_int ("device-scaler", "Scaler ID",
          "To select specific scaler(0:Main, 1:Sub, 2:BG)", 0, 2, 0,
          G_PARAM_READWRITE));
}

static GstStateChangeReturn
gst_mixersink_change_state (GstElement * element, GstStateChange transition)
{	
	GST_DEBUG_OBJECT(element,"mixer_changestate");
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstMixerSink* mixersink = GST_MIXERSINK(element);
	GST_INFO_OBJECT(mixersink, "transition[ %d ]", transition);
	switch (transition) {
		case GST_STATE_CHANGE_READY_TO_PAUSED:
			mixer_open(mixersink);
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			mixer_close(mixersink);
			break;
		default:
			break;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
	GST_DEBUG_OBJECT(mixersink, "ret[ %d ], transition[ %d ]", ret, transition);

	switch (transition) {
		case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		case GST_STATE_CHANGE_PAUSED_TO_READY:
		case GST_STATE_CHANGE_READY_TO_NULL:
		default:
			break;
	}

	return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
	GST_DEBUG_OBJECT(plugin,"plugin_init");
	if (!gst_element_register (plugin, "mixersink", GST_RANK_PRIMARY, GST_TYPE_MIXERSINK))
		return FALSE;

	GST_DEBUG_CATEGORY_INIT (gst_debug_mixersink, "mixersink", 0, "mixersink element");
	return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "mixersink",
    "TV mixer OUTPUT",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)


