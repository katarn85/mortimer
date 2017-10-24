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

#include "linux/videodev2.h"

/* Object header */
#include "framecollector.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Debugging category */
#include <gst/gstinfo.h>


static void
_do_init (GType object_type)
{
}

GST_BOILERPLATE_FULL (GstFrameCollector, gst_framecollector, GstElement,
    GST_TYPE_ELEMENT, _do_init);

GST_DEBUG_CATEGORY_STATIC (gst_debug_framecollector);
#define GST_CAT_DEFAULT gst_debug_framecollector
GST_DEBUG_CATEGORY_STATIC (GST_CAT_PERFORMANCE);


static GstStaticPadTemplate gst_framecollector_sink_template_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
	    GST_PAD_SINK,
	    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        "video/x-raw-yuv, "
        "format=(fourcc){STV0, I420}, "
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
    );

static GstStaticPadTemplate gst_framecollector_src_template_factory =
    GST_STATIC_PAD_TEMPLATE ("src",
	    GST_PAD_SRC,
	    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        "video/x-raw-yuv, "
        "format=(fourcc){STV0, I420}, "
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]")
    );
static GstFlowReturn
gst_framecollector_collected (GstCollectPads * pads, gpointer user_data)
{
	GstFrameCollector* framecollector = (GstFrameCollector*) user_data;
	GstClockTime firstTime = GST_CLOCK_TIME_NONE;
	GstClockTime currTime = GST_CLOCK_TIME_NONE;
	GstCollectData* firstData = NULL;
	guint i=0;
	GSList* collected = NULL;
	if (!framecollector)
		return GST_FLOW_ERROR;
	
	for(i=0, collected = framecollector->collect->data; collected; i++, collected = g_slist_next(collected))
	{
		if (i >= 2)
		{
			GST_ERROR_OBJECT(pads, "i = %d ", i);
			break;
		}
		
		GstBuffer* buffer = NULL;
 		buffer = gst_collect_pads_peek (framecollector->collect, (GstCollectData *)collected->data);
		if (buffer)
		{
#if 0 // TEST
			if (i==0 && buffer->timestamp % 10 == 0)
				buffer->timestamp++;
#endif
			GST_DEBUG_OBJECT(pads, "buffer[ %p ] data[ %p ] size[ %d ] pts[ %lld ] pad[ %s ]", buffer, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer), GST_BUFFER_TIMESTAMP(buffer), GST_PAD_NAME (((GstCollectData *)(collected->data))->pad));
			if (i==0)
			{
				firstTime = GST_BUFFER_TIMESTAMP(buffer);
				firstData = collected->data;
				gst_buffer_unref (buffer);
				buffer = NULL;
			}
			else
			{
				currTime = GST_BUFFER_TIMESTAMP(buffer);
				gst_buffer_unref (buffer);
				buffer = NULL;
				if (firstTime > currTime)
				{
					buffer = gst_collect_pads_pop (framecollector->collect, (GstCollectData *)(collected->data));
					GST_LOG_OBJECT(pads, "free curr buffer");
				      gst_buffer_unref (buffer);
					buffer = NULL;
					return GST_FLOW_OK;
				}
				else if (firstTime < currTime)
				{
					buffer = gst_collect_pads_pop (framecollector->collect, firstData);
					GST_LOG_OBJECT(pads, "free prev buffer");
				      gst_buffer_unref (buffer);
					buffer = NULL;
					return GST_FLOW_OK;
				}
				else
				{
					GstBuffer *bufferL, *bufferR;
					struct v4l2_drm *pFrameL, *pFrameR;
					GST_LOG_OBJECT(pads, "merge and push to next element");

					if (!strcmp(GST_PAD_NAME(((GstCollectData *)(collected->data))->pad), "sink"))
					{
						bufferL = gst_collect_pads_pop (framecollector->collect, (GstCollectData *)(collected->data));
						bufferR = gst_collect_pads_pop (framecollector->collect, (GstCollectData *)(firstData));
					}
					else
					{
						bufferL = gst_collect_pads_pop (framecollector->collect, (GstCollectData *)(firstData));
						bufferR = gst_collect_pads_pop (framecollector->collect, (GstCollectData *)(collected->data));
					}
					
					if (bufferL &&  bufferR)
					{
						pFrameL = (struct v4l2_drm *)bufferL->data;
						pFrameR = (struct v4l2_drm *)bufferR->data;
						if (pFrameL && pFrameR)
							memcpy((void*)&(pFrameL->u.dec_info.pFrame[1]), (const void*)&(pFrameR->u.dec_info.pFrame[0]), sizeof(struct v4l2_private_frame_info));
						else
							GST_ERROR_OBJECT("v4l2_drm  pFrameL[ %x ] pFrameR[ %x ]", pFrameL, pFrameR);
						GST_INFO_OBJECT(pads, "gst_pad_push buffer[ %p ] pts[ %lld ] dur[ %lld ], pad[ %s ]", bufferL, GST_BUFFER_TIMESTAMP(bufferL), GST_BUFFER_DURATION(bufferL), GST_PAD_NAME (framecollector->srcpad));
						gst_pad_push(framecollector->srcpad, bufferL);
						gst_buffer_unref(bufferR);
					}
					else
						GST_ERROR_OBJECT(" bufferL[ %x ] bufferR[ %x ]", bufferL, bufferR);
					// same, push_pad					
				}
			}
		}
		else
		{
			GST_ERROR_OBJECT(pads, "buffer is NULL");
		}
	}
	return GST_FLOW_OK;
}

static GstStateChangeReturn
gst_framecollector_change_state (GstElement * element, GstStateChange transition)
{
  GstFlowReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstFrameCollector *framecollector = (GstFrameCollector *) (element);
//  GstElementClass *klass = GST_ELEMENT_GET_CLASS (element);

  GST_DEBUG_OBJECT(element, "");

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_collect_pads_start (framecollector->collect);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_collect_pads_stop (framecollector->collect);
      break;
    default:
      break;
  }

  if (parent_class && parent_class->change_state)
	  ret = parent_class->change_state (element, transition);
  return ret;
}

static void
gst_framecollector_finalize (GObject * object)
{
	GST_DEBUG_OBJECT(object, "");
	//GstFrameCollector *framecollector = (GstFrameCollector*) object;
	return;
}

static gboolean
gst_framecollector_setcaps (GstPad * pad, GstCaps * caps)
{
	gboolean bRet = FALSE;
	GstFrameCollector* parent = (GstFrameCollector*)(gst_pad_get_parent (pad));
	GST_DEBUG_OBJECT(pad, "begin[ %s ]", GST_PAD_NAME(pad));
	if (!strcmp(GST_PAD_NAME(pad), "sink2"))
	{
		GST_DEBUG_OBJECT(pad, "Just skip this function for sink2");
		return TRUE; // JUST SKIP, THIS IS NOT ERROR. SINK1 WILL DO ALL INSTEAD OF SINK2.
	}

	if (parent)
	{
		GstPad* peerOfSrcPad = gst_pad_get_peer(parent->srcpad);
		bRet = gst_pad_set_caps(parent->srcpad, caps);
		GST_DEBUG_OBJECT(pad, "set_caps to srcpad is done");
		if (peerOfSrcPad && bRet)
		{
			bRet = gst_pad_set_caps(peerOfSrcPad, caps);
			GST_DEBUG_OBJECT(pad, "set_caps to peer of srcpad is done");
		}
		else
			GST_ERROR_OBJECT(pad, "no peer of src pad");
	}
	else
		GST_ERROR_OBJECT(pad, "no parent");

	GST_DEBUG_OBJECT(pad, "end");
	return bRet;
}

#if 1
static gboolean
gst_framecollector_sink_event(GstPad * pad, GstEvent * event)
{
	GstFrameCollector* parent = (GstFrameCollector*)(gst_pad_get_parent (pad));
	if (!parent)
	{
		GST_ERROR_OBJECT(pad, "No Parent");
		return FALSE;
	}

	GST_INFO_OBJECT(pad, "Got %s event on pad %s:%s", GST_EVENT_TYPE_NAME (event), GST_DEBUG_PAD_NAME (pad));
 
	switch (GST_EVENT_TYPE (event)) {
		case GST_EVENT_FLUSH_START:
			gst_collect_pads_set_flushing (parent->collect, TRUE);
			break;
		case GST_EVENT_FLUSH_STOP:
			gst_collect_pads_set_flushing (parent->collect, FALSE);
			break;
		default:
			/* All the other event type will be processed by original collectpad_event function. Such as releasing the blocked pad for FLUSHING */
			break;
	}

	/*Do not send  this event to collect_event, If we sent, it would be unrefed from collect_event.*/
	return gst_pad_event_default (pad, event); //parent->collect_event(pad, event);
}
#endif

static void
gst_framecollector_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (gstelement_class, "MVC Video filter",
      "Filter/Converter/Video",
      "For MVC 3D Video",
      "author< >");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_framecollector_sink_template_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_framecollector_src_template_factory);
}

static void
gst_framecollector_init (GstFrameCollector * framecollector,
    GstFrameCollectorClass * g_class)
{
	GstElementClass *klass = GST_ELEMENT_CLASS (g_class);
	GstPadTemplate *templ = gst_element_class_get_pad_template (klass, "src");
	
	framecollector->srcpad = gst_pad_new_from_template (templ, "src");
	if (framecollector->srcpad)
	{
		gst_pad_set_caps (framecollector->srcpad, gst_pad_template_get_caps (templ));
		gst_element_add_pad (GST_ELEMENT (framecollector), framecollector->srcpad);
	}

	framecollector->collect = gst_collect_pads_new ();
	if (framecollector->collect)
	{
		gst_collect_pads_set_function (framecollector->collect,
			(GstCollectPadsFunction) gst_framecollector_collected, framecollector);

		templ = gst_element_class_get_pad_template (klass, "sink");

		/* 1st sink pad */
		GstPad* sinkpad = gst_pad_new_from_template (templ, "sink");
#if 0
		if (gst_pad_set_caps (sinkpad, gst_pad_template_get_caps (templ)) == FALSE)
			GST_ERROR_OBJECT(framecollector, "can not set caps to 'sink0' pad");
		gst_pad_set_setcaps_function (sinkpad, GST_DEBUG_FUNCPTR (gst_framecollector_sink_setcaps));
#endif
		if (gst_element_add_pad (GST_ELEMENT(framecollector), sinkpad) == FALSE)
			GST_ERROR_OBJECT(framecollector, "can not add 'sink' pad to element");
		if (gst_collect_pads_add_pad (framecollector->collect, sinkpad, sizeof(GstCollectData)) == NULL)
			GST_ERROR_OBJECT(framecollector, "can not add 'sink' pad to collectpads");
		gst_pad_set_setcaps_function (sinkpad, gst_framecollector_setcaps);

		/* Inside this  gst_collect_pads_add_pad(),
		* chain and event of this sinkpad will be replaced to collect_pads_chain and collect_pads_event.
		* So, now, I'll intercept the event function again, and set my own event function  (gst_framecollector_sink_event).
		* This is only for one of sinkpads to send the new_segment event to downstream element.
		* Because we don't need to push same new_segment event to srcpad.   gstcollectpads::gst_collect_pads_event(GST_EVENT_NEWSEGMENT) */
		framecollector->collect_event = (GstPadEventFunction) GST_PAD_EVENTFUNC (sinkpad);
		gst_pad_set_event_function (sinkpad, GST_DEBUG_FUNCPTR (gst_framecollector_sink_event));
	
		/* 2nd sink pad */
		sinkpad = gst_pad_new_from_template (templ, "sink2");
#if 0
		if (gst_pad_set_caps (sinkpad, gst_pad_template_get_caps (templ)) == FALSE)
			GST_ERROR_OBJECT(framecollector, "can not set caps to 'sink1' pad");
		gst_pad_set_setcaps_function (sinkpad, GST_DEBUG_FUNCPTR (gst_framecollector_sink_setcaps));
#endif		
		if (gst_element_add_pad (GST_ELEMENT(framecollector), sinkpad) == FALSE)
			GST_ERROR_OBJECT(framecollector, "can not add 'sink2' pad to element");
		if (gst_collect_pads_add_pad (framecollector->collect, sinkpad, sizeof(GstCollectData)) == NULL)
			GST_ERROR_OBJECT(framecollector, "can not add 'sink2' pad to collectpads");
		gst_pad_set_setcaps_function (sinkpad, gst_framecollector_setcaps);
	}
}

static void
gst_framecollector_class_init (GstFrameCollectorClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
#if 0
  gobject_class->set_property = gst_framecollector_set_property;
  gobject_class->get_property = gst_framecollector_get_property;
#endif
  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_framecollector_change_state);
  gobject_class->finalize = gst_framecollector_finalize;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
	GST_DEBUG("begin");
  if (!gst_element_register (plugin, "framecollector",
          GST_RANK_PRIMARY, GST_TYPE_FRAMECOLLECTOR))
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (gst_debug_framecollector, "framecollector", 0,
      "framecollector element");
  GST_DEBUG_CATEGORY_GET (GST_CAT_PERFORMANCE, "GST_PERFORMANCE");

GST_DEBUG("end");
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "framecollector",
    "MVC 3D Video frame collector",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)

