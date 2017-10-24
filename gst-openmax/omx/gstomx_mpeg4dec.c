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

#include "gstomx_mpeg4dec.h"
#include "gstomx.h"

// #define DIVX_DRM

#ifdef DIVX_DRM
#include "divxdrm/divx_drm.h"
#endif

GSTOMX_BOILERPLATE (GstOmxMpeg4Dec, gst_omx_mpeg4dec, GstOmxBaseVideoDec,
    GST_OMX_BASE_VIDEODEC_TYPE);

GSTOMX_SIMPLE_BOILERPLATE (GstOmxMpeg4Dec1, gst_omx_mpeg4dec1, GstOmxBaseVideoDec,
    GST_OMX_BASE_VIDEODEC_TYPE);

#ifdef DIVX_DRM
static gboolean load_divx_symbol (GstOmxMpeg4Dec * self)
{
  GST_LOG_OBJECT (self, "mpeg4dec load_divx_symbol enter");
  if(self->func.open_divx_drm_manager== 1)
  {
  		GST_WARNING_OBJECT (self, "Divx Drm Manager has been initialized");
		return TRUE;
  }
	self->func.handle_dl= dlopen("libdivxdrm.so.0",RTLD_LAZY);
	if (self->func.handle_dl== NULL)
	{
		GST_ERROR_OBJECT (self, "Divx HwCrypto is not initialized");
		goto open_failed;
	}
	else
	{
		self->func.divx_drm_get_manager= dlsym(self->func.handle_dl, "divx_drm_get_manager");
		if(NULL ==self->func.divx_drm_get_manager)
		{
			GST_ERROR_OBJECT (self, "loading divx_drm_get_manager failed : %s", dlerror());
			goto open_failed;
		}
		self->func.divx_drm_close= dlsym(self->func.handle_dl, "divx_drm_close");
		if(NULL == self->func.divx_drm_close)
		{
			GST_ERROR_OBJECT (self, "loading divx_drm_close failed : %s", dlerror());
			goto open_failed;
		}
		self->func.divx_drm_decoder_commit= dlsym(self->func.handle_dl, "divx_drm_decoder_commit");
		if(NULL == self->func.divx_drm_decoder_commit)
		{
			GST_ERROR_OBJECT (self, "loading divx_drn_decoder_commit failed : %s", dlerror());
			goto open_failed;
		}
		self->func.divx_drm_decoder_decrypt_video= dlsym(self->func.handle_dl, "divx_drm_decoder_decrypt_video");
		if(NULL == self->func.divx_drm_decoder_decrypt_video)
		{
			GST_ERROR_OBJECT (self, "loading divx_drm_decoder_decrypt_video failed : %s", dlerror());
			goto open_failed;
		}

	}
	GST_WARNING_OBJECT (self, "load_divx_symbol ok");
	self->func.open_divx_drm_manager= 1;
	return TRUE;
open_failed:
	if(self->func.handle_dl!=NULL)
	{
		dlclose(self->func.handle_dl);
		self->func.handle_dl =NULL;
	}
	return FALSE;
}

static gboolean
init_divx_drm (GstOmxMpeg4Dec * self)
{
  int error = 0;
  GST_LOG_OBJECT (self, "mpeg4dec init_divx_drm enter");

#if 0
  if (load_divx_symbol(self) == FALSE) {
    GST_ERROR_OBJECT (self, "loading symbol failed....");
    goto error_exit;
  }
  if (self->func.open_divx_drm_manager==1)
  {
	if (DIVX_DRM_ERROR_NONE !=self->func.divx_drm_get_manager(&self->divx_manager))
	{
		GST_ERROR_OBJECT (self, "DivxDrmGetManager error \n" );
		self->divx_manager = NULL;
		goto error_exit;
	}
	else
	{
		GST_DEBUG_OBJECT (self, "DivxDrmGetManager ok \n");
		if (DIVX_DRM_ERROR_NONE !=self->func.divx_drm_decoder_commit(self->divx_manager))
		{
			GST_ERROR_OBJECT (self, "DivxDrmCommit error \n" );
			goto error_exit;
		}
		else
		{
			GST_DEBUG_OBJECT (self, "DivxDrmCommit ok \n");
		}
	}

  }
#endif
	if (DIVX_DRM_ERROR_NONE != divx_drm_get_manager(&self->divx_manager))
	{
	    GST_ERROR_OBJECT (self, "DivxDrmGetManager error \n" ); 
	    goto error_exit;
	}
	if (DIVX_DRM_ERROR_NONE !=divx_drm_decoder_commit(self->divx_manager))
	{
	    GST_ERROR_OBJECT (self, "divx_drm_decoder_commit error \n" ); 
		divx_drm_close(self->divx_manager);
	    goto error_exit;
	}
  return TRUE;

error_exit:
	if (self->divx_manager != NULL)
		self->divx_manager = NULL;

  return FALSE;
}
#endif

static GstOmxReturn
process_input_buf (GstOmxBaseFilter * omx_base_filter, GstBuffer **buf)
{
  GstOmxMpeg4Dec *self;

  self = GST_OMX_MPEG4DEC (omx_base_filter);

  GST_LOG_OBJECT (self, "mpeg4dec process_input_buf enter");

  omx_base_filter->iNumberOfnotVideoFrameDone++;
  GST_DEBUG_OBJECT(self, "OMX_V_OUTPUT, INPUT not output video frame[%d] !!!", omx_base_filter->iNumberOfnotVideoFrameDone);
  if(omx_base_filter->iNumberOfnotVideoFrameDone > OMX_NO_OUTPUT_FRAME_DONE){
	GST_ERROR_OBJECT(self, "OMX_V_OUTPUT, not output video frame > OMX_NO_OUTPUT_FRAME_DONE !!!");
	return GSTOMX_RETURN_ERROR;
  }

#ifdef DIVX_DRM
  GstCaps* caps = NULL;
  GstStructure *structure = NULL;
  caps = GST_BUFFER_CAPS(*buf);
  if (caps)
  {
		structure = gst_caps_get_structure (caps, 0);
		if(structure)
		{
			int drm_length = 0;
			int drm_offset = 0;
			int drm_index = 0;
			int drm_updated_flag = 0;
			gst_structure_get_int(structure,"divx_drm_length",&drm_length);
			gst_structure_get_int(structure,"divx_drm_offset",&drm_offset);
			gst_structure_get_int(structure,"divx_drm_index",&drm_index);
			gst_structure_get_int(structure,"divx_drm_updated_flag",&drm_updated_flag);
			GST_DEBUG_OBJECT(self,"pts %lld ,divx_drm_length_%d,drm_offset %d,divx_drm_index %d,divx_drm_updated_flag %d\n",
			GST_BUFFER_TIMESTAMP(*buf),drm_length,drm_offset,drm_index,drm_updated_flag);
			if((drm_updated_flag == 1)&&(self->divx_manager!= NULL))
			{
				unsigned char *ddinfo = (unsigned char *)malloc(10);
				if (ddinfo == NULL)
				{
					GST_ERROR_OBJECT(self," ddinfo is NULL");
				}
				else
				{
					unsigned char *tmpddinfo = ddinfo;
					unsigned int size = GST_BUFFER_SIZE(*buf);
					unsigned char * data = (unsigned char*)malloc(size);
					if(data == NULL)
					{
						GST_ERROR_OBJECT(self," data is NULL");
					}
					else
					{
						memcpy(data,GST_BUFFER_DATA(*buf),size);
				
						memcpy(ddinfo,&(drm_index),sizeof(drm_index)/2);//divx sdk key index type is uint_16
						ddinfo+= sizeof(drm_index)/2;
						memcpy(ddinfo,&(drm_offset),sizeof(drm_offset));
						ddinfo+= sizeof(drm_offset);
						memcpy(ddinfo,&(drm_length),sizeof(drm_length));
						ddinfo+= sizeof(drm_length);
						//pkt->size will change after this func
					
						GST_DEBUG_OBJECT (self, "Before divx_drm_demux_add_extra_info : data size = %d",size); 
						if (DIVX_DRM_ERROR_NONE !=divx_drm_demux_add_extra_info(self->divx_manager,tmpddinfo,&data,&size))
						{            
							GST_ERROR_OBJECT(self,"divx_drm_demux_add_extra_info is error");
						}
						else
						{
							GST_DEBUG_OBJECT(self,"divx_drm_demux_add_extra_info is ok ");
						}
						ddinfo = tmpddinfo;
						tmpddinfo = NULL;
				
						GST_DEBUG_OBJECT (self, "Before decrypt video : data size = %d",size); 
						if (DIVX_DRM_ERROR_NONE == divx_drm_decoder_decrypt_video(self->divx_manager, data,size))
						{
							GST_DEBUG_OBJECT (self, "##### DivX DRM Mode ##### decrypt video success : data size = %d",size); 
						}
						else 
						{
							GST_ERROR_OBJECT (self, "##### DivX DRM Mode ##### decrypt video failed : data size = %d", size);
						}
						memcpy(GST_BUFFER_DATA(*buf),data,GST_BUFFER_SIZE(*buf));
					}
					free(data);
				}
				free(ddinfo);
			}
		}
	}
#endif	
/* if you want to use commonly for videodec input, use this */
/*  GST_OMX_BASE_FILTER_CLASS (parent_class)->process_input_buf (omx_base_filter, buf); */

  return GSTOMX_RETURN_OK;
}

static void
print_tag (const GstTagList * list, const gchar * tag, gpointer data)
{
  gint i, count;
  GstOmxMpeg4Dec *self;

  self = GST_OMX_MPEG4DEC (data);

  count = gst_tag_list_get_tag_size (list, tag);

  for (i = 0; i < count; i++) {
    gchar *str;

    if (gst_tag_get_type (tag) == G_TYPE_STRING) {
      if (!gst_tag_list_get_string_index (list, tag, i, &str))
        g_assert_not_reached ();
      GST_LOG_OBJECT(self,"gst_tag_get_type is G_TYPE_STRING");
    } else if (gst_tag_get_type (tag) == GST_TYPE_BUFFER) {
      GST_LOG_OBJECT(self,"gst_tag_get_type is GST_TYPE_BUFFER");
      GstBuffer *img;

      img = gst_value_get_buffer (gst_tag_list_get_value_index (list, tag, i));
      if (img) {
        gchar *caps_str;

        caps_str = GST_BUFFER_CAPS (img) ?
            gst_caps_to_string (GST_BUFFER_CAPS (img)) : g_strdup ("unknown");
        str = g_strdup_printf ("buffer of %u bytes, type: %s",
            GST_BUFFER_SIZE (img), caps_str);
	  if(caps_str)	
        	g_free (caps_str);
      } else {
        str = g_strdup ("NULL buffer");
      }
    } else {
   	  GST_LOG_OBJECT(self,"gst_tag_get_type is others");
      str = g_strdup_value_contents (gst_tag_list_get_value_index (list, tag, i));
    }
    GST_LOG_OBJECT(self, "%16s: %s", gst_tag_get_nick (tag), str);
#ifdef DIVX_DRM    
    if (strcmp (gst_tag_get_nick(tag), "divx drm") == 0)
    {
		 GST_LOG_OBJECT(self,"divx drm !!!!!!!!!!!!!!!!!!!!!!!!]");
		 if (init_divx_drm (self))
		 {
            GST_LOG_OBJECT(self, "omx_printtag_init_divx_drm() success");
         }
         else
         {
            GST_ERROR_OBJECT(self, "omx_printtag_init_divx_drm() failed");
         }
    }
#endif
    g_free (str);
  }

  GST_LOG_OBJECT(self, "print_tag End");
}

static gboolean
mpeg4_pad_event (GstPad * pad, GstEvent * event)
{
  GstOmxMpeg4Dec *self;
  gboolean ret = TRUE;

  self = GST_OMX_MPEG4DEC (GST_OBJECT_PARENT (pad));

  GST_LOG_OBJECT (self, "begin");

  GST_INFO_OBJECT (self, "event: %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:
    {
      GstTagList *taglist = NULL;

      GST_LOG_OBJECT (self, "GST_EVENT_TAG");

      gst_event_parse_tag (event, &taglist);
      gst_tag_list_foreach (taglist, print_tag, self);
      gst_event_unref (event);
      ret= FALSE;
      break;
    }
    default:
      ret = TRUE;
      break;
  }
  return ret;
}

static void
finalize (GObject * obj)
{
  GstOmxMpeg4Dec *self;

  self = GST_OMX_MPEG4DEC (obj);

  GST_LOG_OBJECT (self, "mpeg4dec finalize enter");

#ifdef DIVX_DRM
  if (self->divx_manager != NULL)
  {
 	if (self->func.divx_drm_close)
	{
		if (DIVX_DRM_ERROR_NONE == self->func.divx_drm_close(self->divx_manager))
			GST_INFO_OBJECT (self, "DivxDrmClose is ok");
		else
			GST_ERROR_OBJECT (self, "DivxDrmClose is error");
	}
  }
#endif

  if (self->func.handle_dl != NULL)
  {
    GST_DEBUG_OBJECT (self, "dlclose m_DivxDrmManagerLib");
    dlclose(self->func.handle_dl);
  }

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
type_base_init (gpointer g_class)
{
  GstElementClass *element_class;

  element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (element_class,
      "OpenMAX IL MPEG-4 video decoder",
      "Codec/Decoder/Video",
      "Decodes video in MPEG-4 format with OpenMAX IL", "Felipe Contreras");

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
  GstOmxBaseFilterClass *basefilter_class;

  gobject_class = G_OBJECT_CLASS (g_class);
  basefilter_class = GST_OMX_BASE_FILTER_CLASS (g_class);

  gobject_class->finalize = finalize;
  basefilter_class->process_input_buf = process_input_buf;//use new definition of process_input_buf
}

static void
type_instance_init (GTypeInstance * instance, gpointer g_class)
{
  GstOmxBaseVideoDec *omx_base;
  GstOmxBaseFilter *omx_base_filter;

  omx_base = GST_OMX_BASE_VIDEODEC (instance);
  omx_base_filter = GST_OMX_BASE_FILTER (instance);

  omx_base->pad_event = mpeg4_pad_event;
  omx_base->compression_format = OMX_VIDEO_CodingMPEG4;
}
