/*  GStreamer remote audio sink class
 *  Copyright (C) <2014> Liu Yang(yang010.liu@samsung.com)
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

#include "gstremoteaudiosink.h"

#define REMOTE_AUDIO_DEFAULT_SAMPLE_RATE (44100)
#define REMOTE_AUDIO_DEFAULT_CHANNELS (2)
#define REMOTE_AUDIO_DEFAULT_SAMPLE_FORMAT (1)

#define REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE (4)
#define REMOTE_AUDIO_SINK_DEFAULT_DATA_SIZE (4)

enum
{
  PROP_0,
  PROP_REMOTE_AUDIO_SAMPLE_RATE,
  PROP_REMOTE_AUDIO_CHANNELS,
  PROP_REMOTE_AUDIO_SAMPLE_FORMAT
};

typedef struct _GstRemoteAudioParam
{
  int sample_rate;
  int channels;
  int sample_fmt;
} GstRemoteAudioParam;

struct _GstRemoteAudioSinkPrivate
{
	GstRemoteAudioParam param;
};

typedef enum _GstRemoteAudioSinkCommand
{
	GST_REMOTE_AUDIO_SINK_COMMAND_START,
	GST_REMOTE_AUDIO_SINK_COMMAND_STOP,
	GST_REMOTE_AUDIO_SINK_COMMAND_OUTPUT,
	GST_REMOTE_AUDIO_SINK_COMMAND_FLUSH,
	GST_REMOTE_AUDIO_SINK_COMMAND_MUTE,
	GST_REMOTE_AUDIO_SINK_COMMAND_UNMUTE,
	GST_REMOTE_AUDIO_SINK_COMMAND_UPVOLUME,
	GST_REMOTE_AUDIO_SINK_COMMAND_DOWNVOLUME,
	GST_REMOTE_AUDIO_SINK_COMMAND_LAST
}GstRemoteAudioSinkCommand;

GST_DEBUG_CATEGORY_STATIC (remote_audio_sink_debug);
#define GST_CAT_DEFAULT remote_audio_sink_debug

#define _do_init(type) \
{ \
  GST_DEBUG_CATEGORY_INIT (remote_audio_sink_debug, "remoteaudiosink", GST_DEBUG_BOLD, \
      "debugging info for remote audio sink element"); \
}

GST_BOILERPLATE_FULL (GstRemoteAudioSink, gst_remote_audio_sink, GstRemoteSink, GST_TYPE_REMOTE_SINK,
    _do_init);

static void gst_remote_audio_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_remote_audio_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstStateChangeReturn
gst_remote_audio_sink_change_state (GstElement * element, GstStateChange transition);
static gboolean
gst_remote_audio_sink_session_start(GstRemoteAudioSink *rasink);
static gboolean
gst_remote_audio_sink_session_stop(GstRemoteAudioSink *rasink);
static GstFlowReturn
gst_remote_audio_sink_remote_render(GstRemoteSink *remote_sink, GstBuffer *buffer);
static gboolean gst_remote_audio_sink_remote_flush(GstRemoteSink *remote_sink);

static gboolean
gst_remote_audio_sink_set_caps (GstBaseSink * bsink, GstCaps * caps);

static gboolean
gst_remote_audio_sink_session_start(GstRemoteAudioSink *rasink)
{
	gint command_size = REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE;
	gint command_data = 0;
	gint response = 0;
	gboolean result = TRUE;
	gint real_size = 0;
	void *session_start_data = NULL;
	GstRemoteAudioSinkCommand command = GST_REMOTE_AUDIO_SINK_COMMAND_START;
	GstRemoteSink *rsink = (GstRemoteSink*)(rasink);
	
	command_data = command;
	session_start_data = malloc(sizeof(GstRemoteAudioParam) + REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE);
	if(NULL == session_start_data) {
		g_print("REMOTE AUDIO SINK MALLOC SESSION START MEMORY FAIL!\n");
		goto FAIL;
	}

	memset(session_start_data, 0x0, (sizeof(GstRemoteAudioParam) + REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE));
	memcpy(session_start_data, (&command_data), REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE);
	memcpy(session_start_data + REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE, (&(rasink->priv->param)), sizeof(GstRemoteAudioParam));

	result = gst_remote_sink_remote_push(rsink, 
										 session_start_data,
										 sizeof(GstRemoteAudioParam) + REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE,
										 &real_size);
	if(result == FALSE) {
		g_print("REMOTE AUDIO SINK PUSH SESSION START DATA FAIL!\n");
		goto FAIL;
	}

	result = gst_remote_sink_remote_pull(rsink,
										 (&response),
										 4,
										 &real_size);
	if(result == FALSE) {
		g_print("REMOTE AUDIO SINK PULL SESSION START RESPONSE FAIL!\n");
		goto FAIL;
	}

	if(response != GST_REMOTE_SINK_RESPONSE_RESULT_SUCCESS) {
		g_print("REMOTE AUDIO SINK SESSION START FAIL!\n");
		goto FAIL;
	}
	
	free(session_start_data);
	session_start_data = NULL;

	return TRUE;
FAIL:
	if(NULL != session_start_data)
		free(session_start_data);
	session_start_data = NULL;
	return FALSE;
}

static gboolean
gst_remote_audio_sink_session_stop(GstRemoteAudioSink *rasink)
{
	gint command_size = REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE;
	gint command_data = 0;
	gint real_size = 0;
	gint response = 0;
	gboolean result = TRUE;
	GstRemoteAudioSinkCommand command = GST_REMOTE_AUDIO_SINK_COMMAND_STOP;
	GstRemoteSink *rsink = (GstRemoteSink*)(rasink);

	command_data = command;

	result = gst_remote_sink_remote_push(rsink, 
										 (&command_data), 
										 REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE, 
										 &real_size);
	if(result == FALSE) {
		g_print("REMOTE AUDIO SINK PUSH SESSION STOP DATA FAIL!\n");
		goto FAIL;
	}

	result = gst_remote_sink_remote_pull(rsink,
		                                 (&response),
		                                 4,
		                                 &real_size);
	if(result == FALSE) {
		g_print("REMOTE AUDIO SINK PULL SESSION STOP RESPONSE FAIL!\n");
		goto FAIL;
	}

	if(response != GST_REMOTE_SINK_RESPONSE_RESULT_SUCCESS) {
		g_print("REMOTE AUDIO SINK SESSION STOP FAIL!\n");
		goto FAIL;
	}
	return TRUE;
FAIL:
	return FALSE;
}

static GstFlowReturn 
gst_remote_audio_sink_remote_render(GstRemoteSink *remote_sink, GstBuffer *buffer)
{
	gint command_size = REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE;
	gint pcm_size = buffer->size;
	gint command_data = 0;
	gint real_size = 0;
	gint response = 0;
	gboolean result = TRUE;
	void *render_data = NULL;
	GstRemoteAudioSinkCommand command = GST_REMOTE_AUDIO_SINK_COMMAND_OUTPUT;

	command_data = command;
	render_data = malloc(buffer->size + REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE + REMOTE_AUDIO_SINK_DEFAULT_DATA_SIZE);
	if(render_data == NULL) {
		g_print("REMOTE AUDIO SINK MALLOC MEMORY FAIL!\n");
		goto FAIL;
	}

	memset(render_data, 0x0, buffer->size + REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE + REMOTE_AUDIO_SINK_DEFAULT_DATA_SIZE);
	memcpy(render_data, (&command_data), REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE);
	memcpy(render_data + REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE, (&pcm_size), REMOTE_AUDIO_SINK_DEFAULT_DATA_SIZE);
	memcpy(render_data + REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE + REMOTE_AUDIO_SINK_DEFAULT_DATA_SIZE, buffer->data, buffer->size);

	result = gst_remote_sink_remote_push(remote_sink, 
										 render_data, 
										 buffer->size + REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE + REMOTE_AUDIO_SINK_DEFAULT_DATA_SIZE,
										 &real_size);
	if(result == FALSE) {
		g_print("REMOTE AUDIO SINK PUSH AUDIO PCM FAIL!\n");
		goto FAIL;
	}

	result = gst_remote_sink_remote_pull(remote_sink, &response, 4, &real_size);
	if(result == FALSE) {
		g_print("REMOTE AUDIO SINK PULL REPONSE COMMAND FAIL!\n");
		goto FAIL;
	}

	if(response != GST_REMOTE_SINK_RESPONSE_RESULT_SUCCESS) {
		g_print("REMOTE AUDIO SINK OUTPUT PCM FAIL!\n");
		goto FAIL;
	}
	
	free(render_data);
	render_data = NULL;
	
	return GST_FLOW_OK;
FAIL:
	if(NULL != render_data)
		free(render_data);
	render_data = NULL;
	return GST_FLOW_ERROR;
}

static gboolean gst_remote_audio_sink_remote_flush(GstRemoteSink *remote_sink)
{
	gint command_size = REMOTE_AUDIO_SINK_DEFAULT_COMMAND_SIZE;
	gint response = 0;
	gint real_size = 0;
	gint result = TRUE;
	gint command_data = 0;
	GstRemoteAudioSinkCommand command = GST_REMOTE_AUDIO_SINK_COMMAND_OUTPUT;

	command_data = command;

	result = gst_remote_sink_remote_push(remote_sink,
										 (&command_data),
										 command_size,
										 (&real_size));
	if(result == FALSE) {
		g_print("REMOTE AUDIO SINK PUSH FLUSH COMMAND FAIL!\n");
		return FALSE;
	}

	result = gst_remote_sink_remote_pull(remote_sink,
										 (&response),
										 4,
										 (&real_size));
	if(result == FALSE) {
		g_print("REMOTE AUDIO SINK PULL FLUSH COMMAND RESPONSE FAIL!\n");
		return FALSE;
	}

	if(response != GST_REMOTE_SINK_RESPONSE_RESULT_SUCCESS) {
		g_print("REMOTE AUDIO SINK FLUSH RESPONSE NOT SUCCESS!\n");
		return FALSE;
	}
	
	return TRUE;
}

static gboolean
gst_remote_audio_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
	const gchar *mimetype;
	GstStructure *structure;
	gint rate, channels, width, depth, sign;
	GstRemoteAudioSink *rasink = GST_REMOTE_AUDIO_SINK (bsink);
    gboolean result;
	
  	structure = gst_caps_get_structure (caps, 0);

  	/* we have to differentiate between int and float formats */
  	mimetype = gst_structure_get_name (structure);

  	if (g_str_equal (mimetype, "audio/x-raw-int")) {
    	gint endianness;

		if (!(gst_structure_get_int (structure, "rate", &rate) &&
			  gst_structure_get_int (structure, "channels", &channels) &&
			  gst_structure_get_int (structure, "width", &width) &&
			  gst_structure_get_int (structure, "depth", &depth) &&
			  gst_structure_get_boolean (structure, "signed", &sign))) {
			g_print("REMOTE AUDIO SINK SET CAPS FAIL!\n");
			return FALSE;
		}

		rasink->priv->param.sample_rate = rate;
		rasink->priv->param.channels = channels;
		rasink->priv->param.sample_fmt = width;
  	}
	result = gst_remote_audio_sink_session_start(rasink);
	if(result == FALSE)
	{
		g_print("REMOTE AUDIO SINK START SESSION FAIL!\n");
		return FALSE;
	}

	return TRUE;
}

static GstStateChangeReturn
gst_remote_audio_sink_change_state (GstElement * element, GstStateChange transition)
{
	gboolean result = TRUE;
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  	GstRemoteAudioSink *rasink = GST_REMOTE_AUDIO_SINK (element);

	switch (transition) {
    	case GST_STATE_CHANGE_NULL_TO_READY:
			{
    		}
      		break;
    	case GST_STATE_CHANGE_READY_TO_PAUSED:
			{
    		}
      		break;
    	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
			{
    		}
	  		break;
		case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
			{
			}
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			{
				result = gst_remote_audio_sink_session_stop(rasink);
				if(result == FALSE)
					g_print("REMOTE AUDIO SINK STOP SESSION FAIL!\n");
			}
			break;
		case GST_STATE_CHANGE_READY_TO_NULL:
			{
			}
			break;
		default:
			break;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

	switch(transition) {
		case GST_STATE_CHANGE_NULL_TO_READY:
			{
			}
			break;
		case GST_STATE_CHANGE_READY_TO_PAUSED:
			{
			}
			break;
		case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
			{
			}
			break;
		case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
			{
			}
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			{
			}
			break;
		case GST_STATE_CHANGE_READY_TO_NULL:
			{
			}
			break;
		default:
			break;
    }

	if(result == FALSE)
		ret = GST_STATE_CHANGE_FAILURE;
	
	return ret;
}

static void
gst_remote_audio_sink_init (GstRemoteAudioSink * remoteaudiosink, GstRemoteAudioSinkClass *g_class)
{
  remoteaudiosink->priv = G_TYPE_INSTANCE_GET_PRIVATE (remoteaudiosink,
      GST_TYPE_REMOTE_AUDIO_SINK, GstRemoteAudioSinkPrivate);
}

static void
gst_remote_audio_sink_class_init (GstRemoteAudioSinkClass * klass)
{
  GstRemoteSinkClass *remotesink_class = (GstRemoteSinkClass *) klass;
  GstBaseSinkClass *basesink_class = (GstBaseSinkClass *) klass;
  GstElementClass *gelement_class = (GstElementClass *) klass;
  GObjectClass *gobject_class = (GObjectClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_remote_audio_sink_set_property;
  gobject_class->get_property = gst_remote_audio_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_REMOTE_AUDIO_SAMPLE_RATE,
      g_param_spec_int ("remote-audio-sample-rate", "remote audio sample rate",
          "remote audio sample rate", 0, G_MAXINT,
          REMOTE_AUDIO_DEFAULT_SAMPLE_RATE,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_REMOTE_AUDIO_CHANNELS,
      g_param_spec_int ("remote-audio-channels", "remote audio channels",
          "remote audio channels", 0, G_MAXINT,
          REMOTE_AUDIO_DEFAULT_CHANNELS,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_property (gobject_class, PROP_REMOTE_AUDIO_SAMPLE_FORMAT,
      g_param_spec_int ("remote-audio-sample-format", "remote audio sample format",
          "remote audio channels", 0, G_MAXINT,
          REMOTE_AUDIO_DEFAULT_SAMPLE_FORMAT,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  gelement_class->change_state = GST_DEBUG_FUNCPTR(gst_remote_audio_sink_change_state);
  basesink_class->set_caps = GST_DEBUG_FUNCPTR(gst_remote_audio_sink_set_caps);
  remotesink_class->remote_render = GST_DEBUG_FUNCPTR(gst_remote_audio_sink_remote_render);
  remotesink_class->remote_flush = GST_DEBUG_FUNCPTR(gst_remote_audio_sink_remote_flush);

  g_type_class_add_private (klass, sizeof (GstRemoteAudioSinkPrivate));
}

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
# define REMOTE_SINK_FACTORY_ENDIANNESS	"LITTLE_ENDIAN, BIG_ENDIAN"
#else
# define REMOTE_SINK_FACTORY_ENDIANNESS	"BIG_ENDIAN, LITTLE_ENDIAN"
#endif

static GstStaticPadTemplate remote_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw-int, "
        "endianness = (int) { " REMOTE_SINK_FACTORY_ENDIANNESS " }, "
        "signed = (boolean) { TRUE, FALSE }, "
        "width = (int) 32, "
        "depth = (int) 32, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]; "
        "audio/x-raw-int, "
        "endianness = (int) { " REMOTE_SINK_FACTORY_ENDIANNESS " }, "
        "signed = (boolean) { TRUE, FALSE }, "
        "width = (int) 24, "
        "depth = (int) 24, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]; "
        "audio/x-raw-int, "
        "endianness = (int) { " REMOTE_SINK_FACTORY_ENDIANNESS " }, "
        "signed = (boolean) { TRUE, FALSE }, "
        "width = (int) 32, "
        "depth = (int) 24, "
        "rate = (int) [ 1, MAX ], " "channels = (int) [ 1, MAX ]; "
        "audio/x-raw-int, "
        "endianness = (int) { " REMOTE_SINK_FACTORY_ENDIANNESS " }, "
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
static void
gst_remote_audio_sink_base_init (gpointer g_class)
{
   GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  gst_element_class_set_details_simple (element_class,
      "remote Audio sink", "Sink/Audio",
      "Output pcm to another device via network", "audiorelay@samsung.com");
   gst_element_class_add_static_pad_template (element_class,
      &remote_sink_factory);
  GST_DEBUG_CATEGORY_INIT (remote_audio_sink_debug, "remoteaudiosink", 0, "GstRemoteAudioSink");
}

static void
gst_remote_audio_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRemoteAudioSink *rasink;

  rasink = GST_REMOTE_AUDIO_SINK (object);

  switch (prop_id) {
    case PROP_REMOTE_AUDIO_SAMPLE_RATE:
	  {
	  	rasink->priv->param.sample_rate = g_value_get_int(value);
		g_print("REMOTE AUDIO SINK SET PROPERTY: SAMPLE RATE = %d\n", rasink->priv->param.sample_rate);
      }
      break;
	case PROP_REMOTE_AUDIO_CHANNELS:
	  {
	  	rasink->priv->param.channels = g_value_get_int(value);
		g_print("REMOTE AUDIO SINK SET PROPERTY: CHANNELS = %d\n", rasink->priv->param.channels);
	  }
	  break;
    case PROP_REMOTE_AUDIO_SAMPLE_FORMAT:
	  {
	  	rasink->priv->param.sample_fmt = g_value_get_int(value);
		g_print("REMOTE AUDIO SINK SET PROPERTY: SAMPLE FORMAT = %d\n", rasink->priv->param.sample_fmt);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_remote_audio_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRemoteAudioSink *rasink;

  rasink = GST_REMOTE_AUDIO_SINK (object);

  switch (prop_id) {
    case PROP_REMOTE_AUDIO_SAMPLE_RATE:
	  {
	  	 g_print("REMOTE AUDIO SINK GET PROPERTY: SAMPLE RATE = %d\n", rasink->priv->param.sample_rate);
	  	 g_value_set_int(value, rasink->priv->param.sample_rate);
      }
      break;
    case PROP_REMOTE_AUDIO_CHANNELS:
	  {
	  	 g_print("REMOTE AUDIO SINK GET PROPERTY: CHANNELS = %d\n", rasink->priv->param.channels);
	  	 g_value_set_int(value, rasink->priv->param.channels);
      }
      break;
	case PROP_REMOTE_AUDIO_SAMPLE_FORMAT:
	  {
	  	 g_print("REMOTE AUDIO SINK GET PROPERTY: SAMPLE FORMAT = %d\n", rasink->priv->param.sample_fmt);
	  	 g_value_set_int(value, rasink->priv->param.sample_fmt);
	  }
	  break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Public methods */
