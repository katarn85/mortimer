/*
 * audiotp
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

#include "gstaudiotp.h"

/* Plugin Detaills for gstreamer */
static const GstElementDetails gst_audiotp_plugin_details = GST_ELEMENT_DETAILS (
 "Audio timestamp reversal plugin",
 "Utility/Audio",
 "Reverses audio timestamps for reverse playback",
 "Samsung Electronics <www.samsung.com>"
 );

/*** GSTREAMER PROTOTYPES *****************************************************/

#define STATIC_CAPS \
GST_STATIC_CAPS ( \
  "audio/x-raw-float, " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, MAX ], " \
    "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, " \
    "width = (int) 64;" \
  "audio/x-raw-float, " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, MAX ], " \
    "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, " \
    "width = (int) 32;" \
  "audio/x-raw-int, " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, MAX ], " \
    "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, " \
    "width = (int) 32, " \
    "depth = (int) [ 1, 32 ], " \
    "signed = (boolean) { true, false }; " \
  "audio/x-raw-int, "   \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, MAX ], "       \
    "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, "        \
    "width = (int) 24, "        \
    "depth = (int) [ 1, 24 ], " "signed = (boolean) { true, false }; "  \
  "audio/x-raw-int, " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, MAX ], " \
    "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, " \
    "width = (int) 16, " \
    "depth = (int) [ 1, 16 ], " \
    "signed = (boolean) { true, false }; " \
  "audio/x-raw-int, " \
    "rate = (int) [ 1, MAX ], " \
    "channels = (int) [ 1, MAX ], " \
    "endianness = (int) { LITTLE_ENDIAN, BIG_ENDIAN }, " \
    "width = (int) 8, " \
    "depth = (int) [ 1, 8 ], " \
    "signed = (boolean) { true, false } " \
)

/* Element sink pad template */
static GstStaticPadTemplate gst_audiotp_sink_template = GST_STATIC_PAD_TEMPLATE (
 "sink",
 GST_PAD_SINK,
 GST_PAD_ALWAYS,
 STATIC_CAPS);

/* Element Source Pad template */
static GstStaticPadTemplate gst_audiotp_src_template = GST_STATIC_PAD_TEMPLATE (
 "src",
 GST_PAD_SRC,
 GST_PAD_ALWAYS,
 STATIC_CAPS);


////////////////////////////////////////////////////////
//        Gstreamer Base Prototype                    //
////////////////////////////////////////////////////////

GST_DEBUG_CATEGORY_STATIC(gst_audiotp_debug);
#define GST_CAT_DEFAULT gst_audiotp_debug
#define _do_init(bla) \
 GST_DEBUG_CATEGORY_INIT(GST_CAT_DEFAULT, "audiotp", 0, "Audio trickplay plugin"); \
 GST_DEBUG("audiotp is registered");

GST_BOILERPLATE_FULL(Gstaudiotp, gst_audiotp, GstElement, GST_TYPE_ELEMENT, _do_init);

static void gst_audiotp_base_init(gpointer klass);
static void gst_audiotp_class_init(GstaudiotpClass *klass);
static void gst_audiotp_init(Gstaudiotp *dec, GstaudiotpClass *klass);
static GstFlowReturn gst_audiotp_chain(GstPad *pad, GstBuffer *buf);
static GstStateChangeReturn gst_audiotp_change_state(GstElement *element, GstStateChange transition);
static void gst_audiotp_finalize(GObject *object);
static gboolean gst_audiotp_sink_event (GstPad *pad, GstEvent *event);
static GstFlowReturn gst_audiotp_push_silent_frame (Gstaudiotp *audiotp, GstBuffer *MetaDataBuf);



////////////////////////////////////////////////////////
//        Gstreamer Base Functions                    //
////////////////////////////////////////////////////////

/**
 **
 **  Description: The element details and pad templates are registered with the plugin
 **  In Params    : @ gclass instance of Element class
 **  return    : None
 **  Comments    : 1. Adding templates of source and sink pad to element
 **           2. Setting element class deatils to element
 **
 */
static void
gst_audiotp_base_init(gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_audiotp_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_audiotp_src_template));
  gst_element_class_set_details(element_class, &gst_audiotp_plugin_details);
}


/**
 **
 **  Description: Initialization of the Element Class
 **  In Param    : @ gclass instance of Element class
 **  return    : None
 **  Comments    : 1. Overwriting base class virtual functions
 **                2. Installing the properties of the element
 **
 */
static void
gst_audiotp_class_init(GstaudiotpClass *klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  parent_class = g_type_class_peek_parent(klass);

  gobject_class->finalize = gst_audiotp_finalize;
  gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_audiotp_change_state);
}


/**
 **
 **  Description: Initialization of the Element instance
 **  In Params    : @ audio tp element instance
 **           @ gclass instance of Element class
 **  return    : None
 **  Comments    : 1. Creating new source & sink pads using templates
 **           2. Setting the callback functions to the pads
 **          3. Local data initialization.
 **
 */
static void
gst_audiotp_init(Gstaudiotp *audiotp, GstaudiotpClass *klass)
{

  audiotp->sinkpad = gst_pad_new_from_static_template(&gst_audiotp_sink_template, "sink");
  audiotp->srcpad = gst_pad_new_from_static_template(&gst_audiotp_src_template, "src");

  gst_pad_set_chain_function (audiotp->sinkpad, GST_DEBUG_FUNCPTR(gst_audiotp_chain));
  gst_pad_set_event_function (audiotp->sinkpad, GST_DEBUG_FUNCPTR(gst_audiotp_sink_event));

  gst_pad_use_fixed_caps(audiotp->srcpad);

  gst_element_add_pad(GST_ELEMENT(audiotp), audiotp->sinkpad);
  gst_element_add_pad(GST_ELEMENT(audiotp), audiotp->srcpad);

  audiotp->reverse = g_queue_new ();
  audiotp->head_prev = GST_CLOCK_TIME_NONE;
  audiotp->tail_prev = GST_CLOCK_TIME_NONE;

}


/**
 **
 **  Description: Finalization of the Element instance (object)
 **  In Params    : @ audiotp element instance in the form of GObject
 **  return    : None
 **  Comments    : 1. Local data Deinitialization.
 **
 **
 */
static void
gst_audiotp_finalize(GObject *object)
{
  Gstaudiotp *audiotp = GST_AUDIOTP(object);

  while (!g_queue_is_empty (audiotp->reverse)) {
    GstMiniObject *data = g_queue_pop_head (audiotp->reverse);
    gst_mini_object_unref (data);
  }
  /* freeing dealy queue */
  g_queue_free(audiotp->reverse);
  audiotp->reverse = NULL;

  G_OBJECT_CLASS(parent_class)->finalize(object);
}


/**
 **
 **  Description: Callback function when the element's state gets changed
 **  In Params    : @ audiotp plugin element
 **          @ type of state change
 **  return    : status of the state change processing
 **  Comments    :
 **
 **
 */
static GstStateChangeReturn
gst_audiotp_change_state(GstElement *element, GstStateChange transition)
{
  GstStateChangeReturn res = GST_FLOW_ERROR;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  res = parent_class->change_state(element, transition);
  if ( res != GST_STATE_CHANGE_SUCCESS ) {
    GST_ERROR ("change state error in parent class\n");
    return GST_STATE_CHANGE_FAILURE;
  }

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return res;
}


/**
 **
 **  Description: Callback function when sinkpad gets an event
 **  In Params    : @ Sinkpad on which the event arrives
 **          @ event type
 **  return    : TRUE/FALSE on success/failure of the event processing.
 **  Comments    : 1. Process the event and push it to the source pad.
 **
 **
 */
static gboolean
gst_audiotp_sink_event (GstPad *pad, GstEvent *event)
{
  Gstaudiotp *audiotp = NULL;
  gboolean res = FALSE;

  audiotp = GST_AUDIOTP(GST_PAD_PARENT(pad));

  switch (GST_EVENT_TYPE(event)) {
    /* Arrives whenever there is a jump in the normal playback. Ex:SEEK */
    case GST_EVENT_NEWSEGMENT: {
      GstFormat format;
      gdouble rate, arate;
      gint64 start, stop, time;
      gboolean update;

      GST_INFO_OBJECT (audiotp, "GST_EVENT_NEWSEGMENT");
      gst_event_parse_new_segment_full(event, &update, &rate, &arate, &format, &start, &stop, &time);

      if (format != GST_FORMAT_TIME) {
        GST_ERROR("Format is not supported\n");
        res = gst_pad_push_event(audiotp->srcpad, event);
        goto done;
      }

      GST_INFO_OBJECT (audiotp, "update: %d, rate: %0.3f, arate: %0.3f\n", update, rate, arate);
      GST_INFO_OBJECT (audiotp, "start : %" GST_TIME_FORMAT, GST_TIME_ARGS(start));
      GST_INFO_OBJECT (audiotp, "stop  : %" GST_TIME_FORMAT, GST_TIME_ARGS(stop));
      GST_INFO_OBJECT (audiotp, "time  : %" GST_TIME_FORMAT, GST_TIME_ARGS(time));

      /* If we receive new_segment without FLUSH events, then we will push all the frame in queue */
      while (!g_queue_is_empty (audiotp->reverse)) {
	    GstBuffer *MetaDataBuf;
        GstFlowReturn ret = GST_FLOW_OK;
		if(audiotp->is_reversed)
          MetaDataBuf = g_queue_pop_head (audiotp->reverse);
		else
		  MetaDataBuf = g_queue_pop_tail (audiotp->reverse);
        ret = gst_audiotp_push_silent_frame (audiotp, MetaDataBuf);
        if (GST_FLOW_OK != ret)
        {
           GST_WARNING_OBJECT (audiotp, "pad_push returned = %s", gst_flow_get_name (ret));
        }
      }
      gst_segment_set_newsegment_full(&audiotp->segment, update, rate, arate, format, start, stop, time);
      res = gst_pad_push_event(audiotp->srcpad, event);
      break;
    }

    /* Indication of the end of the stream */
    case GST_EVENT_EOS: {
      /* queue all buffer timestamps till we receive next discontinuity */
      while (!g_queue_is_empty (audiotp->reverse)) {
	    GstBuffer *MetaDataBuf;
        GstFlowReturn ret = GST_FLOW_OK;
		if(audiotp->is_reversed)
          MetaDataBuf = g_queue_pop_head (audiotp->reverse);
		else
		  MetaDataBuf = g_queue_pop_tail (audiotp->reverse);
        ret = gst_audiotp_push_silent_frame (audiotp, MetaDataBuf);
        if (GST_FLOW_OK != ret) {
          GST_WARNING_OBJECT (audiotp, "pad_push returned = %s", gst_flow_get_name (ret));
        }
      }

      res = gst_pad_push_event(audiotp->srcpad, event);
      break;
    }

    /* Indication of the SEEK operation start */
    case GST_EVENT_FLUSH_START: {
      GST_INFO_OBJECT (audiotp, "GST_EVENT_FLUSH_START");
      res = gst_pad_push_event(audiotp->srcpad, event);
      break;
    }

    /* Indication of the SEEK operation stop */
    case GST_EVENT_FLUSH_STOP: {
      GST_INFO_OBJECT (audiotp, "GST_EVENT_FLUSH_STOP");
      /* make sure that we empty the queue */
      while (!g_queue_is_empty (audiotp->reverse)) {
        GST_DEBUG_OBJECT (audiotp, "Flushing buffers in reverse queue....");
        gst_buffer_unref(g_queue_pop_head (audiotp->reverse));
      }

      res = gst_pad_push_event(audiotp->srcpad, event);
      break;
    }

    default: {
      res = gst_pad_push_event(audiotp->srcpad, event);
      break;
    }
  }

  done:
    return res;
}


/**
 **
 **  Description: Callback function when sinkpad gets a buffer (from the previous element)
 **  In Params    : @ Sinkpad on which the buffer arrives
 **          @ input buffer
 **  return    : status of the buffer processing.
 **  Comments    : 1. Handle the buffer discontinuity ( in terms of tmestamp)
 **          2. Push or pop buffer based on discontinuity.
 **
 **
 */
static GstFlowReturn
gst_audiotp_chain(GstPad *pad, GstBuffer *buf)
{
  Gstaudiotp *audiotp = GST_AUDIOTP(GST_PAD_PARENT(pad));
  GstFlowReturn ret = GST_FLOW_OK;

  if(buf == NULL) {
    ret = GST_FLOW_ERROR;
	goto error_exit;
  }

  GST_LOG_OBJECT (audiotp, "Input buffer : ts =%" GST_TIME_FORMAT ", dur=%" GST_TIME_FORMAT ", size=%d",
         GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buf)),
         GST_TIME_ARGS(GST_BUFFER_DURATION(buf)),
         GST_BUFFER_SIZE(buf), GST_BUFFER_IS_DISCONT (buf) ? " - discont" :"");

  if (audiotp->segment.rate < 0.0) {
    goto send_reverse;
  }


  /* Push the input data to the next element */
  ret = gst_pad_push(audiotp->srcpad, buf);
  if (ret != GST_FLOW_OK ) {
   GST_WARNING("failed to push buffer %p. reason: %s", buf, gst_flow_get_name (ret));
   buf = NULL;
   goto error_exit;
  }
  return ret;

send_reverse:
  {
    GstBuffer *MetaDataBuf = NULL;
    GstClockTime headbuf_ts = GST_CLOCK_TIME_NONE;
	GstClockTime tailbuf_ts = GST_CLOCK_TIME_NONE;

    /* Discont buffers is mostly due to seek, when buffers of seeked timestamp gets pushed */
    if (GST_BUFFER_IS_DISCONT(buf)) {
      if(!g_queue_is_empty (audiotp->reverse)) {
        GstBuffer *headbuf = (GstBuffer*) (audiotp->reverse->head->data);
        GstBuffer *tailbuf = (GstBuffer*) (audiotp->reverse->tail->data);

        headbuf_ts = GST_BUFFER_TIMESTAMP(headbuf);
		tailbuf_ts = GST_BUFFER_TIMESTAMP(tailbuf);

        GST_DEBUG_OBJECT(audiotp,"Headbuf ts =%" GST_TIME_FORMAT ", TailBuf ts =%" GST_TIME_FORMAT "",
            GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(headbuf)),
            GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(tailbuf)));

        /* Check if the decoder is already having the reversal logic */
        if(GST_BUFFER_TIMESTAMP(headbuf) > GST_BUFFER_TIMESTAMP(tailbuf)) {
          GST_INFO_OBJECT (audiotp, "Buffers arrived in reverse order, audiotp NO NEED to reverse...");
          audiotp->is_reversed = TRUE;
        } else {
          GST_INFO_OBJECT (audiotp, "Buffers arrived in forward order, audiotp NEED to reverse...");
          audiotp->is_reversed = FALSE;
        }
      }

      while (!g_queue_is_empty (audiotp->reverse)) {

        if(audiotp->is_reversed)
          MetaDataBuf = g_queue_pop_head (audiotp->reverse);
        else
          MetaDataBuf = g_queue_pop_tail (audiotp->reverse);

        if (NULL == MetaDataBuf) {
          GST_ERROR_OBJECT (audiotp, "NULL pointer...");
          ret = GST_FLOW_ERROR;
          goto error_exit;
        }

	   /* If buffers arrive in forward order, compare the MetaDatabuf with
		* previous head buffer timestamp.
		* If buffers arrive in reverse order, compare the MetaDataBuf with
		* previous tail buffer timestamp */
        if((GST_BUFFER_TIMESTAMP(MetaDataBuf) < audiotp->head_prev && !audiotp->is_reversed)
				|| (GST_BUFFER_TIMESTAMP(MetaDataBuf) < audiotp->tail_prev && audiotp->is_reversed)) {
          ret = gst_audiotp_push_silent_frame (audiotp, MetaDataBuf);
          if (MetaDataBuf) {
            gst_buffer_unref (MetaDataBuf);
            MetaDataBuf = NULL;
          }

          if (GST_FLOW_OK != ret) {
            GST_WARNING_OBJECT (audiotp, "pad_push returned = %s", gst_flow_get_name (ret));
            if (buf) {
              gst_buffer_unref (buf);
              buf = NULL;
            }
            return ret;
          }
        } else {
          GST_DEBUG_OBJECT(audiotp, "Dropping the buffer out of segment with time-stamp %"GST_TIME_FORMAT,
            GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(MetaDataBuf)));
          if (MetaDataBuf) {
            gst_buffer_unref (MetaDataBuf);
            MetaDataBuf = NULL;
          }
        }
      }

      audiotp->head_prev = headbuf_ts;
	  audiotp->tail_prev = tailbuf_ts;
    }

    MetaDataBuf = gst_buffer_new ();
    if (NULL == MetaDataBuf) {
      GST_ERROR_OBJECT (audiotp, "Failed to create memory...");
      ret = GST_FLOW_ERROR;
      goto error_exit;
    }

    /* copy buffer timestamps & FLAGS to metadata buffer */
    gst_buffer_copy_metadata (MetaDataBuf, buf, GST_BUFFER_COPY_TIMESTAMPS | GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_CAPS);
    GST_BUFFER_SIZE(MetaDataBuf) = GST_BUFFER_SIZE(buf);
    GST_DEBUG_OBJECT (audiotp, "Pushing into reverse queue data of size: %d", GST_BUFFER_SIZE(MetaDataBuf));

    /* queue all buffer timestamps till we receive next discontinuity */
    g_queue_push_tail (audiotp->reverse, MetaDataBuf);
    if (buf) {
      gst_buffer_unref (buf);
      buf = NULL;
    }
    return GST_FLOW_OK;
  }

#if 0
send_dummy:
  {

    /* Resetting the buffer data to zero */
    memset(GST_BUFFER_DATA(buf), 0, GST_BUFFER_SIZE(buf));
    gst_buffer_set_caps(buf, GST_PAD_CAPS(audiotp->srcpad));

    ret = gst_pad_push(audiotp->srcpad, buf);
    if (ret != GST_FLOW_OK) {
     GST_ERROR("Failed to push buffer. reason: %s\n", gst_flow_get_name(ret));
     buf = NULL;
     goto error_exit;
    }
    return GST_FLOW_OK;
  }
#endif

error_exit:

  GST_WARNING_OBJECT(audiotp, "Returning from audiotp's chain with reason - %s", gst_flow_get_name (ret));
  if (buf) {
   gst_buffer_unref (buf);
   buf = NULL;
  }
  return ret;
}


static GstFlowReturn
gst_audiotp_push_silent_frame (Gstaudiotp *audiotp, GstBuffer *MetaDataBuf)
{

  GstBuffer *out = NULL;
  GstFlowReturn ret = GST_FLOW_OK;

  out = gst_buffer_new_and_alloc(GST_BUFFER_SIZE(MetaDataBuf));
  if(out == NULL) {
    GST_ERROR_OBJECT (audiotp, "Failed to allocate memory...");
    return GST_FLOW_ERROR;
  }

  /* Memset the data of the out buffer so that silent frame is sent */
  memset(GST_BUFFER_DATA(out), 0, GST_BUFFER_SIZE(out));

  gst_buffer_copy_metadata (out, MetaDataBuf, GST_BUFFER_COPY_FLAGS);
  GST_BUFFER_OFFSET (out) = GST_BUFFER_OFFSET_END (out) = 0;
  GST_BUFFER_SIZE(out) = GST_BUFFER_SIZE(MetaDataBuf);
  GST_BUFFER_TIMESTAMP(out) = GST_BUFFER_TIMESTAMP(MetaDataBuf);
  GST_BUFFER_DURATION(out) = GST_BUFFER_DURATION(MetaDataBuf);

  GST_LOG_OBJECT(audiotp, "Out buffer ts =%" GST_TIME_FORMAT ", dur=%" GST_TIME_FORMAT ", size=%d",
       GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(out)),
       GST_TIME_ARGS(GST_BUFFER_DURATION(out)),
       GST_BUFFER_SIZE(out));

  gst_buffer_set_caps(out, GST_PAD_CAPS(audiotp->srcpad));

  ret = gst_pad_push(audiotp->srcpad, out);
  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (audiotp, "Failed to push buffer. reason: %s\n", gst_flow_get_name(ret));
    out = NULL;
  }

  return ret;
}

static gboolean
gst_audiotp_plugin_init (GstPlugin *plugin)
{
  if (!gst_element_register (plugin, "audiotp", GST_RANK_PRIMARY, gst_audiotp_get_type())) {
    return FALSE;
  }
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
                   GST_VERSION_MINOR,
                   "audiotp",
                   "Audio trickplay plugin",
                   gst_audiotp_plugin_init,
                   VERSION,
                   "LGPL",
                   "Samsung Electronics Co",
                   "http://www.samsung.com")
