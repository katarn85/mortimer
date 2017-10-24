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

/* Object header */
#include "audiodatasplitter.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define PAD_NAME_MAIN_PCM "mainpcm"    /* base data */
#define PAD_NAME_SUB_PCM "subpcm"
#define PAD_NAME_SPDIF_ES "spdif_es"

static gboolean gst_audiodata_splitter_handle_sink_event(GstPad *pad, GstEvent *event);
static gboolean gst_audiodata_splitter_srcpad_query(GstPad *pad, GstQuery *query);

#define MIN_LATENCY 5000
static unsigned long long get_time()
{
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	return ((ts.tv_sec * GST_SECOND + ts.tv_nsec * GST_NSECOND)/1000); // usec
}

static void
_do_init (GType object_type)
{
}

GST_BOILERPLATE_FULL (GstAudioDataSplitter, gst_audiodata_splitter, GstElement, GST_TYPE_ELEMENT, _do_init);

GST_DEBUG_CATEGORY_STATIC (gst_debug_audiodata_splitter);
#define GST_CAT_DEFAULT gst_debug_audiodata_splitter

static void
gst_audiodata_splitter_finalize (GObject * object)
{
	GST_DEBUG_OBJECT(object, "");
	return;
}

static gboolean
gst_audiodata_splitter_setcaps (GstPad * pad, GstCaps * caps)
{
	return TRUE;
}

static GstPad* 
gst_audiodata_splitter_add_new_srcpad(GstAudioDataSplitter * audiodata_splitter, const char* templateName, const char* padName)
{
	GstPad* src = NULL;
	GstCaps* caps = NULL;
	GST_LOG_OBJECT(audiodata_splitter, "template[ %s ] pad[ %s ]", templateName, padName);
	GstElementClass *klass = GST_ELEMENT_GET_CLASS(GST_ELEMENT(audiodata_splitter));
	if (!klass) {
		GST_ERROR_OBJECT(audiodata_splitter, "can't get klass");
		return NULL;
	}

	GstPadTemplate *templ = gst_element_class_get_pad_template (klass, templateName);
	if (!templ) {
		GST_ERROR_OBJECT(audiodata_splitter, "can't get pad template[ %s ]", templateName);
		return NULL;
	}

	src = gst_pad_new_from_template (templ, padName);
	if (!src) {
		GST_ERROR_OBJECT(audiodata_splitter, "can't create new pad[ %s ]", padName);
		return NULL;
	}
	gst_pad_set_query_function(src, gst_audiodata_splitter_srcpad_query);

	audiodata_splitter->srcpads = g_list_prepend(audiodata_splitter->srcpads, src);
	if (g_str_equal(padName, PAD_NAME_SPDIF_ES))
	{
		caps = gst_caps_new_simple ("audio/x-spdif-es", NULL);
	}
	else if (g_str_equal(padName, PAD_NAME_SUB_PCM))
	{
		caps = gst_caps_new_simple ("audio/x-raw-int", "postprocessed", G_TYPE_BOOLEAN, TRUE, NULL);
	}

	GST_INFO_OBJECT(audiodata_splitter, "caps : %"GST_PTR_FORMAT"", caps);

	if (gst_pad_set_caps (src, caps) == FALSE)
		GST_WARNING_OBJECT(audiodata_splitter, "Failed to gst_pad_set_caps");

	gst_pad_set_active (src, TRUE);
	guint64 T1 = get_time();
	gst_element_add_pad (GST_ELEMENT (audiodata_splitter), src);
	guint64 T2 = get_time();
	if (T2-T1 > MIN_LATENCY)
		GST_ERROR_OBJECT(audiodata_splitter, "gst_element_add_pad[ %lld ms ], padName[ %s ]", (T2-T1)/1000, padName);
		
		
	gst_caps_unref(caps);
	return src;
}

static void
gst_audiodata_splitter_base_init (gpointer g_class)
{
	static GstStaticPadTemplate gst_audiodata_splitter_pcm_src_template_factory =
			GST_STATIC_PAD_TEMPLATE ("pcmsrc",
					GST_PAD_SRC,
					GST_PAD_ALWAYS,
					GST_STATIC_CAPS ("audio/x-raw-int") );
	static GstStaticPadTemplate gst_audiodata_splitter_spdif_src_template_factory =
			GST_STATIC_PAD_TEMPLATE ("spdifsrc",
					GST_PAD_SRC,
					GST_PAD_SOMETIMES,
					GST_STATIC_CAPS ("audio/x-spdif-es") );
	static GstStaticPadTemplate gst_audiodata_splitter_sink_template_factory =
			GST_STATIC_PAD_TEMPLATE ("sink",
					GST_PAD_SINK,
					GST_PAD_ALWAYS,
					GST_STATIC_CAPS ("audio/x-raw-int, "
					"hassubdata = true") );

	GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);
	gst_element_class_set_details_simple (gstelement_class, "OMX audio decoder extradata splitter",
			"Filter/Audio",
			"For SAMSUNG VD omx audio decoder",
			"author< >");
	
	gst_element_class_add_static_pad_template (gstelement_class, &gst_audiodata_splitter_sink_template_factory);
	gst_element_class_add_static_pad_template (gstelement_class, &gst_audiodata_splitter_pcm_src_template_factory);
	gst_element_class_add_static_pad_template (gstelement_class, &gst_audiodata_splitter_spdif_src_template_factory);
}

static GstFlowReturn
gst_audiodata_splitter_pad_chain (GstPad * pad, GstBuffer * buf)
{
	GstAudioDataSplitter *object = GST_AUDIODATA_SPLITTER(GST_PAD_PARENT(pad));
	GstFlowReturn ret = GST_FLOW_ERROR;
	GstPad* srcpad = NULL;
	const GstStructure* structure = NULL;
	const char* padname = NULL;
	GstBuffer* buffer = NULL;
	guint64 T1=0, T2=0, T3=0, T4=0;
	if (buf == NULL)
	{
		GST_ERROR_OBJECT(object, "srcpads == NULL");
		return GST_FLOW_UNEXPECTED;
	}

	gst_buffer_ref(buf); // to prevent unreferencing of this buffer from first gst_pad_push().

	/* push main pcm  */
	T1 = get_time();
	padname = PAD_NAME_MAIN_PCM;
	srcpad = gst_element_get_static_pad(GST_ELEMENT(object), padname);
	if (srcpad)
	{
		ret = gst_pad_push(GST_PAD_CAST(srcpad), buf);
		if (ret != GST_FLOW_OK)
			GST_ERROR_OBJECT(object, "Fail to pad_push[ %s ], ret[ %d ]", padname, ret);
	}
	else
	{
		GST_ERROR_OBJECT(object, "srcpad == NULL[ %s ]", padname);
	}

	T2 = get_time();
	/* find & push sub pcm */
	structure = gst_buffer_get_qdata(buf, g_quark_from_static_string("subdata"));
	padname = PAD_NAME_SUB_PCM;
	if (structure && gst_structure_has_field(structure, padname))
	{
		srcpad = gst_element_get_static_pad(GST_ELEMENT(object), padname);
		if (!srcpad) {
			srcpad = gst_audiodata_splitter_add_new_srcpad(object, "pcmsrc", padname);
                        object->sub_pcm_pad = srcpad;
                }
		if (srcpad)
		{
                        if (object->send_newseg_sub_pcm == FALSE) {
                          gst_pad_push_event(GST_PAD_CAST(srcpad),
                               gst_event_new_new_segment(object->newseg_update,
                                  object->newseg_rate, object->newseg_format,
                                  object->newseg_start,object->newseg_stop,
                                  object->newseg_start));
                          object->send_newseg_sub_pcm = TRUE;
                        }
			gst_structure_get(structure, padname, GST_TYPE_BUFFER, &buffer, NULL);
			ret = gst_pad_push(GST_PAD_CAST(srcpad), buffer);
			if (ret != GST_FLOW_OK)
				GST_ERROR_OBJECT(object, "Fail to pad_push[ %s ], ret[ %d ]", padname, ret);
		}
		else
		{
			GST_ERROR_OBJECT(object, "srcpad == NULL[ %s ]", padname);
		}
	}
	else
		GST_DEBUG_OBJECT(object, "No [ %s ] field in subdata structure[ %p ]", padname, structure);

	T3 = get_time();
	/* find & push spdif es */
	padname = PAD_NAME_SPDIF_ES;
	if (structure && gst_structure_has_field(structure, padname))
	{
		srcpad = gst_element_get_static_pad(GST_ELEMENT(object), padname);
		if (!srcpad) {
			srcpad = gst_audiodata_splitter_add_new_srcpad(object, "spdifsrc", padname);
                        object->spdif_pcm_pad = srcpad;
                }
                if (object->send_newseg_spdif == FALSE) {
                    gst_pad_push_event(GST_PAD_CAST(srcpad),
                         gst_event_new_new_segment(object->newseg_update,
                            object->newseg_rate, object->newseg_format,
                            object->newseg_start,object->newseg_stop,
                            object->newseg_start));
                    object->send_newseg_spdif = TRUE;
                  }

		gst_structure_get(structure, padname, GST_TYPE_BUFFER, &buffer, NULL);
		ret = gst_pad_push(GST_PAD_CAST(srcpad), buffer);
		if (ret != GST_FLOW_OK)
			GST_ERROR_OBJECT(object, "Fail to pad_push[ %s ], ret[ %d ]", padname, ret);
	}
	else
		GST_DEBUG_OBJECT(object, "No [ %s ] field in subdata structure[ %p ]", padname, structure);
	T4 = get_time();
	if (T4-T1 > MIN_LATENCY)
		GST_DEBUG_OBJECT(object, "[ %lld ms ]=[ %lld + %lld + %lld ]", (T4-T1)/1000, (T2-T1)/1000, (T3-T2)/1000, (T4-T3)/1000);
	gst_buffer_unref(buf);
	return ret;
}

static gboolean 
gst_audiodata_splitter_handle_sink_event(GstPad *pad, GstEvent *event)
{
        GstAudioDataSplitter *audiodata_splitter = (GstAudioDataSplitter *)(GST_PAD_PARENT (pad));
        
        /*sends all event to all src pads*/
        if (audiodata_splitter == NULL)
            return FALSE;
        GST_INFO_OBJECT(audiodata_splitter, "%s event: %" GST_PTR_FORMAT,GST_EVENT_TYPE_NAME (event), event->structure);
        switch (GST_EVENT_TYPE (event)) {
          case GST_EVENT_FLUSH_START:
          {
            if (audiodata_splitter->main_pcm_pad) {
              gst_event_ref(event);
              gst_pad_push_event(audiodata_splitter->main_pcm_pad, event);
            }
            if (audiodata_splitter->sub_pcm_pad) {
              gst_event_ref(event);
              gst_pad_push_event(audiodata_splitter->sub_pcm_pad, event);
              audiodata_splitter->flush_event_sub = TRUE;
            }
            if (audiodata_splitter->spdif_pcm_pad) {
              gst_event_ref(event);
              gst_pad_push_event(audiodata_splitter->spdif_pcm_pad, event);
              audiodata_splitter->flush_event_spdif = TRUE;
            }
            gst_event_unref(event);
            break;
          }
          case GST_EVENT_FLUSH_STOP:
          {
            if (audiodata_splitter->main_pcm_pad){
              gst_event_ref(event);
              gst_pad_push_event(audiodata_splitter->main_pcm_pad, event);
            }
            if (audiodata_splitter->sub_pcm_pad && audiodata_splitter->flush_event_sub) {
              gst_event_ref(event);
              gst_pad_push_event(audiodata_splitter->sub_pcm_pad, event);
              audiodata_splitter->flush_event_sub = FALSE;
            }
            if (audiodata_splitter->spdif_pcm_pad && audiodata_splitter->flush_event_spdif) {
              gst_event_ref(event);
              gst_pad_push_event(audiodata_splitter->spdif_pcm_pad, event);
              audiodata_splitter->flush_event_spdif = FALSE;
            }
            gst_event_unref(event);
            break;
          }
          default:
          {
          /*save newsegment, and update audiosink after creatinging new src pad*/
           if (GST_EVENT_TYPE(event) == GST_EVENT_NEWSEGMENT) {
             gst_event_parse_new_segment_full(event, &audiodata_splitter->newseg_update,&audiodata_splitter->newseg_rate,
                                                &audiodata_splitter->newseg_applied_rate, &audiodata_splitter->newseg_format,
                                                &audiodata_splitter->newseg_start, &audiodata_splitter->newseg_stop,
                                                &audiodata_splitter->newseg_position);
           }
           gst_pad_event_default(pad, event);
           break;
          }
        }
     return TRUE;
}
static void
gst_audiodata_splitter_init (GstAudioDataSplitter * audiodata_splitter,
    GstAudioDataSplitterClass * g_class)
{
	GstElementClass *klass = GST_ELEMENT_CLASS (g_class);
	GstPad* src = NULL;
	GstPadTemplate *templ = gst_element_class_get_pad_template (klass, "sink");

        audiodata_splitter->main_pcm_pad = NULL;
        audiodata_splitter->sub_pcm_pad = NULL;
        audiodata_splitter->spdif_pcm_pad = NULL;
        audiodata_splitter->flush_event_sub = FALSE;
        audiodata_splitter->flush_event_spdif = FALSE;

        audiodata_splitter->newseg_update = FALSE;
        audiodata_splitter->newseg_rate = 1.0;
        audiodata_splitter->newseg_applied_rate = 1.0;
        audiodata_splitter->newseg_format = GST_FORMAT_TIME;
        audiodata_splitter->newseg_start = 0;
        audiodata_splitter->newseg_stop = -1;
        audiodata_splitter->newseg_position = -1;
        audiodata_splitter->send_newseg_sub_pcm = FALSE;
        audiodata_splitter->send_newseg_spdif = FALSE;
	/* Create a sink pad*/
	audiodata_splitter->sinkpad = gst_pad_new_from_template (templ, "sink");
	if (audiodata_splitter->sinkpad)
	{
		gst_pad_set_chain_function (audiodata_splitter->sinkpad, gst_audiodata_splitter_pad_chain);
		gst_element_add_pad (GST_ELEMENT (audiodata_splitter), audiodata_splitter->sinkpad);
		gst_pad_set_event_function(audiodata_splitter->sinkpad,GST_DEBUG_FUNCPTR(gst_audiodata_splitter_handle_sink_event));
	}
	else
		GST_ERROR_OBJECT(audiodata_splitter, "can't create sinkpad");


	/* Create a default src pads */
	audiodata_splitter->srcpads = NULL;
	templ = gst_element_class_get_pad_template (klass, "pcmsrc");
	src = gst_pad_new_from_template (templ, PAD_NAME_MAIN_PCM);
	gst_pad_set_query_function(src, gst_audiodata_splitter_srcpad_query);

	if (src)
	{
		audiodata_splitter->srcpads = g_list_prepend(audiodata_splitter->srcpads, src);
		GstCaps* caps = GST_PAD_CAPS(src);
		if (GST_CAPS_IS_SIMPLE(caps))
			gst_caps_set_simple(caps, "postprocessed", G_TYPE_BOOLEAN, FALSE, NULL);
		gst_element_add_pad (GST_ELEMENT (audiodata_splitter), src);
                audiodata_splitter->main_pcm_pad = src;
	}
	else
		GST_ERROR_OBJECT(audiodata_splitter, "can't create srcpad");
}

static gboolean
gst_audiodata_splitter_srcpad_query(GstPad *pad, GstQuery *query)
{
	GstAudioDataSplitter *audiodata_splitter = GST_AUDIODATA_SPLITTER((GST_PAD_PARENT (pad)));
	gboolean ret = FALSE;

	if (query == NULL)
	{
	 	GST_ERROR_OBJECT(audiodata_splitter, "query-recieve : failed - NULL");
		return ret;
	} 

	switch (query->type)
	{
		case GST_QUERY_CUSTOM:
	    {
	    	GstStructure *structure = gst_query_get_structure (query);
			ret = gst_structure_has_name(structure, "check-audiosplitter-exist");
	      	if(ret)
	      	{
				GST_DEBUG_OBJECT(audiodata_splitter, "query-reply : %d", ret);
	      	}
			else
			{
				ret = gst_pad_query_default(pad, query);
			}
		   	break;
	   	}
	   	default:
	  	{
			ret = gst_pad_query_default(pad, query);
			break;
	   	}
	}
	
    return ret;
}


static void
gst_audiodata_splitter_class_init (GstAudioDataSplitterClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;
	gobject_class->finalize = gst_audiodata_splitter_finalize;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
	GST_DEBUG("begin");
	if (!gst_element_register (plugin, "audiodatasplitter", GST_RANK_PRIMARY, GST_TYPE_AUDIODATA_SPLITTER))
		return FALSE;

	GST_DEBUG_CATEGORY_INIT (gst_debug_audiodata_splitter, "audiodata_splitter", 0, "audiodata_splitter element");

	GST_DEBUG("end");
	return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		"audiodata_splitter",
		"OMX audio decoder extradata splitter",
		plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)

