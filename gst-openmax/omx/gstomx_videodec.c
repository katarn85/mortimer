/*
 * Copyright (C) 2007-2009 Nokia Corporation.
 *
 * Author: Felipe Contreras <felipe.contreras@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "gstomx_videodec.h"
#include "gstomx.h"

GSTOMX_BOILERPLATE (GstOmxVideoDec, gst_omx_videodec, GstOmxBaseVideoDec,
    GST_OMX_BASE_VIDEODEC_TYPE);

GSTOMX_SIMPLE_BOILERPLATE (GstOmxVideoDec1, gst_omx_videodec1, GstOmxBaseVideoDec,
    GST_OMX_BASE_VIDEODEC_TYPE);

GSTOMX_SIMPLE_BOILERPLATE (GstOmxVideoDec2, gst_omx_videodec2, GstOmxBaseVideoDec,
    GST_OMX_BASE_VIDEODEC_TYPE);

GSTOMX_SIMPLE_BOILERPLATE (GstOmxVideoDec3, gst_omx_videodec3, GstOmxBaseVideoDec,
    GST_OMX_BASE_VIDEODEC_TYPE);

GSTOMX_SIMPLE_BOILERPLATE (GstOmxVideoDec4, gst_omx_videodec4, GstOmxBaseVideoDec,
    GST_OMX_BASE_VIDEODEC_TYPE);

GSTOMX_SIMPLE_BOILERPLATE (GstOmxUhdVideoDec, gst_omx_uhd_videodec, GstOmxBaseVideoDec,
    GST_OMX_BASE_VIDEODEC_TYPE);

static void
set_property (GObject * obj,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOmxBaseVideoDec *videodec;
  GstOmxBaseFilter *basefilter;
  videodec = GST_OMX_BASE_VIDEODEC (obj);
  basefilter = GST_OMX_BASE_FILTER (obj);

  switch (prop_id) {
	/* Decoding Type */
    case ARG_DECODING_TYPE:
      g_mutex_lock (basefilter->ready_lock); // Because the omx_state will be changed after omx_setup(). and decoding_type will be applied inside this omx_setup().
      if (basefilter->gomx
	  	&& (basefilter->gomx->omx_state == OMX_StateLoaded || basefilter->gomx->omx_state == OMX_StateInvalid))
      {
        videodec->decoding_type = g_value_get_int(value);
        if ((CLIP_MODE & videodec->decoding_type) && (SEAMLESS_MODE & videodec->decoding_type))
          GST_WARNING_OBJECT(videodec, "CLIP & SEAMLESS cannot be choosed together, In this case, only CLIP will be applied");
      }
      else
        GST_ERROR_OBJECT(videodec, "Cannot set decoding type, gomx[ %p ]  omx_state[ 0x%x ]  last decoding_type[ 0x%x ]", basefilter->gomx, (basefilter->gomx ? basefilter->gomx->omx_state:(-1)), videodec->decoding_type);
      g_mutex_unlock (basefilter->ready_lock);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static void
get_property (GObject * obj, guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOmxBaseVideoDec *videodec;
  videodec = GST_OMX_BASE_VIDEODEC (obj);

  switch (prop_id) {
    /* Decoding Type */
    case ARG_DECODING_TYPE:
      g_value_set_int(value, videodec->decoding_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static void
type_base_init (gpointer g_class)
{
  GstElementClass *element_class;

  element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (element_class,
      "OpenMAX IL Multi video decoder",
      "Codec/Decoder/Video",
      "Decodes video in various format with OpenMAX IL", "Felipe Contreras");

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gstomx_template_caps (G_TYPE_FROM_CLASS (g_class), "sink")));

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gstomx_template_caps (G_TYPE_FROM_CLASS (g_class), "src")));
}

static void
type_class_init (gpointer g_class, gpointer class_data)
{
  GObjectClass *gobject_class;
  gobject_class = G_OBJECT_CLASS (g_class);
  gobject_class->set_property = set_property;
  gobject_class->get_property = get_property;
	
  g_object_class_install_property (gobject_class, ARG_DECODING_TYPE,
      g_param_spec_int ("decoding-type", "Decoding Type",
        "To select decoding type, multiple selection(OR operation) is possible except Clip & Seamless.)\n\
                        Available only on NULL,READY STATE.\n\
                        0x01 : Clip(only for I,P frame)\n\
                        0x02 : Seamless\n\
                        0x04 : MultiDecoding(up-to 640x480)", 0, 7, 0,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
type_instance_init (GTypeInstance * instance, gpointer g_class)
{
  GstOmxBaseVideoDec *omx_base;
  GstOmxBaseFilter *omx_base_filter;

  omx_base = GST_OMX_BASE_VIDEODEC (instance);
  omx_base_filter = GST_OMX_BASE_FILTER (instance);

  omx_base->compression_format = OMX_VIDEO_CodingAutoDetect;
}
