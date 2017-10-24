/*
 * drmsrc
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
#include "gstdrmsrc.h"

#include "drm_util.h"
#include <drm_client.h>
#include <drm_client_types.h>

#ifdef CONTROL_PAGECACHE
#include <fcntl.h>
#define DEFAULT_DO_FADVISE_THRESHOLD    (100 * 1024 * 1024)    /* 100 MB */
#endif

#define LOG_TRACE(message)  //g_print("DRM_SRC: %s: %d: %s - %s \n", __FILE__, __LINE__, __FUNCTION__, message);

#define GST_TAG_PLAYREADY "playready_file_path"

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,GST_STATIC_CAPS_ANY);

#ifdef CONTROL_PAGECACHE
#if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64)
#define DRMSRC_FADVISE_DONT(x_fd, x_offset, x_length) \
	do \
	{ \
		if (posix_fadvise64(x_fd, x_offset, x_length, POSIX_FADV_DONTNEED) != 0) \
		{ \
			GST_ERROR("Set posix_fadvise with POSIX_FADV_DONTNEED failed"); \
		} \
	}while (0);
#else
#define DRMSRC_FADVISE_DONT(x_fd, x_offset, x_length) \
	do \
	{ \
		if (posix_fadvise(x_fd, x_offset, x_length, POSIX_FADV_DONTNEED) != 0) \
		{ \
			GST_ERROR("Set posix_fadvise with POSIX_FADV_DONTNEED failed"); \
		} \
	}while (0);
#endif
#endif

GST_DEBUG_CATEGORY_STATIC (gst_drm_src_debug);
#define GST_CAT_DEFAULT gst_drm_src_debug

enum
{
  ARG_0,
  ARG_LOCATION,
  ARG_FD,
  IS_DRM
};
static void gst_drm_src_finalize (GObject * object);
static void gst_drm_src_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_drm_src_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean gst_drm_src_start (GstBaseSrc * basesrc);
static gboolean gst_drm_src_stop (GstBaseSrc * basesrc);
static gboolean gst_drm_src_is_seekable (GstBaseSrc * src);
static gboolean gst_drm_src_get_size (GstBaseSrc * src, guint64 * size);
static GstFlowReturn gst_drm_src_create (GstBaseSrc * src, guint64 offset, guint length, GstBuffer ** buffer);
static void gst_drm_src_uri_handler_init (gpointer g_iface, gpointer iface_data);
static gboolean gst_drm_src_query (GstBaseSrc * src, GstQuery * query);
static GstStateChangeReturn gst_drm_src_change_state (GstElement * element,
    GstStateChange transition);
/**
 * This function does the following:
 *  1. Initializes GstDrmSrc ( defines gst_drm_get_type)
 *
 * @param   drmsrc_type    [out]  GType
 *
 * @return  void
 */
static void _do_init (GType drmsrc_type)
{
  // 1. Initializes GstDrmSrc ( defines gst_drm_get_type)
  static const GInterfaceInfo urihandler_info = {
    gst_drm_src_uri_handler_init,
    NULL,
    NULL
  };

  g_type_add_interface_static (drmsrc_type, GST_TYPE_URI_HANDLER, &urihandler_info);
  GST_DEBUG_CATEGORY_INIT (gst_drm_src_debug, "drmsrc", 0, "drmsrc element");
}
GST_BOILERPLATE_FULL (GstDrmSrc, gst_drm_src, GstBaseSrc, GST_TYPE_BASE_SRC,   _do_init);
/**
 * This function does the following:
 *  1. Sets the class details
 *  2. Adds the source pad template
 *
 * @param   g_class    [out]   gpointer
 *
 * @return  void
 */
static void gst_drm_src_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  /*Sets the class details */
  gst_element_class_set_details_simple (gstelement_class,
    "DRM Source",
    "Source/File",
    "Read from arbitrary point in a standard/DRM file",
    "Kishore Arepalli  <kishore.a@samsung.com> and Sadanand Dodawadakar <sadanand.d@samsung.com>");

  /*Adds the source pad template */
  gst_element_class_add_pad_template (gstelement_class, gst_static_pad_template_get (&srctemplate));
}
/**
 * This function does the following:
 *  1. Installs the properties
 *  2. Assigns the function pointers GObject class attributes
 *
 * @param   klass    [out]   GstDrmSrcClass Structure
 *
 * @return  void
 */
static void gst_drm_src_class_init (GstDrmSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  gobject_class = G_OBJECT_CLASS (klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstElementClass *gstelement_class;
  gstelement_class = (GstElementClass *) klass;
  /* Assigns the function pointers GObject class attributes */
  gobject_class->set_property = gst_drm_src_set_property;
  gobject_class->get_property = gst_drm_src_get_property;

  /* Installs the properties*/
  g_object_class_install_property (gobject_class, ARG_FD,
    g_param_spec_int ("fd", "File-descriptor",
      "File-descriptor for the file being mmap()d", 0, G_MAXINT, 0,
      G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, ARG_LOCATION,
    g_param_spec_string ("location", "File Location",
      "Location of the file to read", NULL,
      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, IS_DRM,
    g_param_spec_boolean ("is-drm", "whether selected file type is drm or not",
      "true, false", FALSE,
      G_PARAM_READABLE));

   /*Assigns the function pointers GObject class attributes */
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_drm_src_finalize);
  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_drm_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_drm_src_stop);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_drm_src_is_seekable);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_drm_src_get_size);
  gstbasesrc_class->create = GST_DEBUG_FUNCPTR (gst_drm_src_create);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_drm_src_query);
  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_drm_src_change_state);
  gst_tag_register (GST_TAG_PLAYREADY, GST_TAG_FLAG_META,
    G_TYPE_STRING,
    "PlayReady File Path",
    "a tag that is specific to PlayReady File",
     NULL);
}
/**
 * This function does the following:
 *  1. Initilizes the parameters of GstDrmSrc
 *
 * @param   src    [out]   GstDrmSrc structure
 * @param   g_class    [in]   GstDrmSrcClass structure
 *
 * @return  gboolean   Returns TRUE on success and FALSE on ERROR
 */
static void gst_drm_src_init (GstDrmSrc * src, GstDrmSrcClass * g_class)
{
  /* Initilizes the parameters of GstDrmSrc */
  src->filename = NULL;
  src->fd = 0;
  src->uri = NULL;
  src->is_regular = FALSE;
  src->is_oma = FALSE;
  src->seekable = FALSE;
  src->hfile = NULL;
  src->event_posted = FALSE;
  src->is_playready = FALSE;
  src->is_drm = FALSE;
  src->isopen = FALSE;
#ifdef CONTROL_PAGECACHE
  src->accum = 0;
#endif
}
/**
 * This function does the following:
 *  1. deallocates the filename and uri
 *  2. calls the parent class->finalize
 *
 * @param   object    [in]   GObject Structure
 *
 * @return  void
 */
static void gst_drm_src_finalize (GObject * object)
{
  GstDrmSrc *src;

  src = GST_DRM_SRC (object);
  /*. deallocates the filename and uri */
  g_free (src->filename);
  g_free (src->uri);
  /* calls the parent class->finalize */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}
/**
 * This function does the following:
 *  1. Checks the state
 *  2. Checks the filename
 *  3. Sets the filename
 *
 * @param   src    [in]   GstDrmSrc Structure
 * @param   location    [in]   location of the file
 *
 * @return  gboolean   Returns TRUE on success and FALSE on ERROR
 */
static gboolean gst_drm_src_set_location (GstDrmSrc * src, const gchar * location)
{
  GstState state;

  GST_OBJECT_LOCK (src);

  /* Checks the state */
  state = GST_STATE (src);
  if (state != GST_STATE_READY && state != GST_STATE_NULL) {
    GST_DEBUG_OBJECT (src, "setting location in wrong state");
    GST_OBJECT_UNLOCK (src);
    return FALSE;
  }
  GST_OBJECT_UNLOCK (src);

  g_free (src->filename);
  g_free (src->uri);

  /* Checks the filename */
  if (location == NULL) {
    src->filename = NULL;
    src->uri = NULL;
  } else {
    /*. Sets the filename */
    src->filename = g_strdup (location);
#if 0
    /*This below API is changing the filename in case of punctuation marks in filename*/
    src->uri = gst_filename_to_uri (location, NULL);
#else
    src->uri = g_strdup_printf ("%s://%s", "file", src->filename);

    drm_bool_type_e is_drm = DRM_FALSE;
    if (drm_is_drm_file(src->filename, &is_drm) == DRM_RETURN_SUCCESS) {
      if (is_drm == DRM_TRUE) {
	src->is_drm = TRUE;
      }
    }
    GST_DEBUG_OBJECT (src, "is drm : %d", src->is_drm);
#endif
    GST_INFO_OBJECT(src, "filename : %s", src->filename);
    GST_INFO_OBJECT(src, "uri      : %s", src->uri);
  }
  g_object_notify (G_OBJECT (src), "location");
  gst_uri_handler_new_uri (GST_URI_HANDLER (src), src->uri);

  return TRUE;
}
/**
 * This function does the following:
 *  1. Sets the location of the file.
 *
 * @param   object    [in]   GObject Structure
 * @param   prop_id    [in]   id of the property
 * @param   value    [in]   property value
 * @param   pspec    [in]  GParamSpec Structure
 *
 * @return  void
 */
static void gst_drm_src_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstDrmSrc *src;

  g_return_if_fail (GST_IS_DRM_SRC (object));

  src = GST_DRM_SRC (object);

  switch (prop_id) {
  //  1. Sets the location of the file.
  case ARG_LOCATION:
    gst_drm_src_set_location (src, g_value_get_string (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}
/**
 * This function does the following:
 *  1. Provides the location of the file.
 *  2. Provides the file descriptor.
 *
 * @param   object    [in]   GObject Structure
 * @param   prop_id    [in]   id of the property
 * @param   value    [out]   property value
 * @param   pspec    [in]  GParamSpec Structure
 *
 * @return  void
 */
static void gst_drm_src_get_property (GObject * object, guint prop_id, GValue * value,GParamSpec * pspec)
{
  GstDrmSrc *src;

  g_return_if_fail (GST_IS_DRM_SRC (object));
  src = GST_DRM_SRC (object);
  switch (prop_id) {
    case ARG_LOCATION:
    g_value_set_string (value, src->filename);
    break;
    case ARG_FD:
    g_value_set_int (value, src->fd);
    break;
    case IS_DRM:
    g_value_set_boolean(value, src->is_drm);
    break;
    default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static GstStateChangeReturn
gst_drm_src_change_state (GstElement * element, GstStateChange transition)
{

  GstDrmSrc *src = GST_DRM_SRC (element);
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;
  gboolean ret = FALSE;
  drm_file_type_e file_type = DRM_TYPE_UNDEFINED;
  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:{
      GST_INFO_OBJECT(src,"change state from NULL to ready");
      drm_result_e drm_result;
      drm_result = drm_get_file_type (src->filename, &file_type);
      if (file_type == DRM_TYPE_OMA_V1) {
      /* Opens the DRM file if it is DRM */
        if (!src->isopen) {
          GST_DEBUG_OBJECT (src, "trying  to open drm util");
          if (drm_util_open (&src->hfile, src->filename, file_type) == FALSE) {
            GST_ERROR_OBJECT (src, "failed to open drm util");
            return FALSE;
          }
          src->isopen=TRUE;
        }
      }
    break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:{
      GST_INFO_OBJECT(src,"change state from ready to paused");
      drm_result_e drm_result;
      drm_result = drm_get_file_type (src->filename, &file_type);
      if (file_type == DRM_TYPE_OMA_V1){
        ret = drm_process_request(DRM_REQUEST_TYPE_CLIENT_CLEAN_UP, NULL, NULL);
        if (DRM_RETURN_SUCCESS == ret) {
          GST_INFO("Clean Up successful!!");
        } else {
          GST_ERROR("Clean Up Failed!!, ret = 0x%x", ret);
        }
        ret = drm_trusted_handle_request(DRM_TRUSTED_REQ_TYPE_CLIENT_CLEAN_UP, NULL, NULL);
        if (DRM_RETURN_SUCCESS == ret) {
          GST_INFO("Clean Up successful!!");
        } else {
          GST_ERROR("Clean Up Failed!!, ret = 0x%x", ret);
        }
      }
      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:{
      GST_INFO_OBJECT(src,"change state from paused to playing");
      break;
    }
    default:
      break;
  }
  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:{
      GST_INFO_OBJECT(src,"change state from playing  to paused");
      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      GST_INFO_OBJECT(src,"change state from paused to ready");
      if (src->hfile) {
        if (src->isopen)
          if (drm_util_close(&src->hfile))
            src->isopen=FALSE;
            src->hfile = NULL;
#ifdef CONTROL_PAGECACHE
	    DRMSRC_FADVISE_DONT(src->hfile, 0, 0);
	    src->accum = 0;
#endif
      }
      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:{
      GST_INFO_OBJECT(src,"change state from ready to null");
      break;
    }
    default:
    break;

  }
  return result;
}




/**
 * This function does the following:
 *  1. Seeks to the specified position for DRM file.
 *  2. Allocates a buffer to push the data for DRM file.
 *  3. Reads from the file and sets the related params for DRM file.
 *
 * @param   i_pDrmSrc    [in]   GstDrmSrc Structure
 * @param   i_uiOffset    [in]   offset of the file to seek
 * @param   length    [in]   size of the data in bytes
 * @param   o_pBbuffer    [out]   GstBuffer to hold the contents
 *
 * @return  GstFlowReturn   Returns GST_FLOW_OK on success and ERROR on failure
 */
static GstFlowReturn  gst_drm_src_create_read_drm_file (GstDrmSrc* src, guint64 i_uiOffset, guint length, GstBuffer ** o_pBbuffer)
{
  GstBuffer *buf = NULL;
  unsigned int readSize;

  /* Seeks to the specified position for DRM file. */
  if (G_UNLIKELY (src->read_position != i_uiOffset)) {
    if (drm_util_seek (src->hfile, i_uiOffset, DRM_SEEK_SET) == FALSE)
      goto FAILED;

    src->read_position = i_uiOffset;
  }

  /* Allocates a buffer to push the data for DRM file. */
  buf = gst_buffer_new_and_alloc (length);
  if(buf == NULL) {
    LOG_TRACE("Exit on error");
    return GST_FLOW_ERROR;
  }

  /*. Reads from the file and sets the related params for DRM file. */
  if (drm_util_read (src->hfile, GST_BUFFER_DATA(buf), GST_BUFFER_SIZE(buf), &readSize) == FALSE)
    goto FAILED;

  if(readSize <= 0) {
    LOG_TRACE("Exit on error");
    return GST_FLOW_ERROR;
  }

  #if 0 // Drm service can give lesser size block than requested thing.
  if (G_UNLIKELY ((guint) readSize < length && i_pDrmSrc->seekable)) {
  GST_ELEMENT_ERROR (i_pDrmSrc, RESOURCE, READ, (NULL),("unexpected end of file."));
  gst_buffer_unref (buf);
  return GST_FLOW_ERROR;
  }
  #endif

  if (G_UNLIKELY (readSize == 0 && length > 0)) {
    GST_DEBUG ("non-regular file hits EOS");
    gst_buffer_unref (buf);
    return GST_FLOW_UNEXPECTED;
  }

#ifdef CONTROL_PAGECACHE
	src->accum += readSize;
	if (src->accum >= DEFAULT_DO_FADVISE_THRESHOLD) {
		DRMSRC_FADVISE_DONT(src->hfile, 0, 0);
		src->accum = 0;
	}
#endif

  length = readSize;
  GST_BUFFER_SIZE (buf) = length;
  GST_BUFFER_OFFSET (buf) = i_uiOffset;
  GST_BUFFER_OFFSET_END (buf) = i_uiOffset + length;
  *o_pBbuffer = buf;
  src->read_position += length;

  return GST_FLOW_OK;

FAILED:
{
  GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
  return GST_FLOW_ERROR;
}
}
/**
 * This function does the following:
 *  1. Seeks to the specified position.
 *  2. Allocates a buffer to push the data
 *  3. Reads from the file and sets the related params
 *
 * @param   src    [in]   GstDrmSrc Structure
 * @param   offset    [in]   offset of the file to seek
 * @param   length    [in]   size of the data in bytes
 * @param   buffer    [out]   GstBuffer to hold the contents
 *
 * @return  GstFlowReturn   Returns GST_FLOW_OK on success and ERROR on failure
 */
static GstFlowReturn gst_drm_src_create_read (GstDrmSrc * src, guint64 offset, guint length, GstBuffer ** buffer)
{
  int ret;
  GstBuffer *buf;

  /* Seeks to the specified position. */
  if (G_UNLIKELY (src->read_position != offset)) {
    off_t res;
    res = lseek (src->fd, offset, SEEK_SET);
    if (G_UNLIKELY (res < 0 || res != offset)) {
      GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
      return GST_FLOW_ERROR;
    }
    src->read_position = offset;
  }

  /* Allocates a buffer to push the data */
  buf = gst_buffer_new_and_alloc (length);
  if (NULL == buf) {
    GST_ERROR_OBJECT (src, "failed to allocate memory..");
    GST_ELEMENT_ERROR (src, RESOURCE, NO_SPACE_LEFT, (NULL), ("failed to allocate memory"));
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (src, "Reading %d bytes", length);

  /*  Reads from the file and sets the related params */
  ret = read (src->fd, GST_BUFFER_DATA (buf), length);
  if (G_UNLIKELY (ret < 0)) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }

  if (G_UNLIKELY ((guint) ret < length && src->seekable)) {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),("unexpected end of file."));
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }

  if (G_UNLIKELY (ret == 0 && length > 0)) {
    GST_DEBUG ("non-regular file hits EOS");
    gst_buffer_unref (buf);
    return GST_FLOW_UNEXPECTED;
  }

#ifdef CONTROL_PAGECACHE
	src->accum += ret;
	if (src->accum >= DEFAULT_DO_FADVISE_THRESHOLD) {
		DRMSRC_FADVISE_DONT(src->fd, 0, 0);
		src->accum = 0;
	}
	//DRMSRC_FADVISE_DONT(src->fd, offset, ret);
#endif

  length = ret;
  GST_BUFFER_SIZE (buf) = length;
  GST_BUFFER_OFFSET (buf) = offset;
  GST_BUFFER_OFFSET_END (buf) = offset + length;
  *buffer = buf;
  src->read_position += length;

  return GST_FLOW_OK;
}
/**
 * This function does the following:
 *  1. Calls DRM file read chain method for drm files.
 *  2. Calls normal file read chain method for standard files.
 *
 * @param   basesrc    [in]   BaseSrc Structure
 * @param   size    [out]   Size of the file
 *
 * @return  gboolean   Returns TRUE on success and FALSE on ERROR
 */
static GstFlowReturn gst_drm_src_create (GstBaseSrc * basesrc, guint64 offset, guint length, GstBuffer ** buffer)
{
  GstDrmSrc *src = GST_DRM_SRC (basesrc);

  if (src->is_playready && src->event_posted == FALSE) {
    GstTagList *tags = NULL;
    GST_DEBUG_OBJECT (src, "posting playready tags");
    tags =  gst_tag_list_new_full (GST_TAG_PLAYREADY, src->filename, NULL);
    if (tags) {
      GstPad* src_pad = gst_element_get_static_pad (src, "src");
      if (src_pad) {
        if(!gst_pad_push_event (src_pad, gst_event_new_tag (tags))) {
          GST_ERROR_OBJECT (src, "failed to push tags..");
          gst_object_unref (src_pad);
          return GST_FLOW_ERROR;
        }
        GST_DEBUG_OBJECT (src, "posting tags returns [%d]", src->event_posted);
        src->event_posted = TRUE;
        gst_object_unref (src_pad);
      }
    }
  }

  if(src->is_oma == TRUE) /* Calls DRM file read chain method for drm files. */
    return gst_drm_src_create_read_drm_file (src, offset, length, buffer);
  else /* Calls normal file read chain method for standard files. */
    return gst_drm_src_create_read (src, offset, length, buffer);
}

static gboolean
gst_drm_src_query (GstBaseSrc * basesrc, GstQuery * query)
{
  gboolean ret = FALSE;
  GstDrmSrc *src = GST_DRM_SRC (basesrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
      gst_query_set_uri (query, src->uri);
      ret = TRUE;
      break;
    case GST_QUERY_CUSTOM:
    {
      GstStructure *s;
      guint64 size = 0;
      GValue v = { 0, { { 0 } } };
      g_value_init(&v, G_TYPE_UINT64);

      s = gst_query_get_structure (query);
      if (gst_structure_has_name (s, "dynamic-size")) {
        if (gst_drm_src_get_size (basesrc, &size)){
          /* succedded. take size */
          g_value_set_uint64 (&v, size);
          gst_structure_set_value(s, "size", &v);
          ret = TRUE;
        }
      }
      break;
    }
    default:
      ret = FALSE;
      break;
  }

  if (!ret)
    ret = GST_BASE_SRC_CLASS (parent_class)->query (basesrc, query);

  return ret;
}

/**
 *
 * @param   basesrc    [in]   BaseSrc Structure
 *
 * @return  gboolean   Returns TRUE if the file is seekable and FALSE if the file is not seekable
 */
static gboolean gst_drm_src_is_seekable (GstBaseSrc * basesrc)
{
  GstDrmSrc *src = GST_DRM_SRC (basesrc);
  return src->seekable;
}
/**
 * This function does the following:
 *  1. Gets the filesize for drm file by using seek oprations
 *  2. Gets the file size for standard file by using statistics
 *
 * @param   basesrc    [in]   BaseSrc Structure
 * @param   size    [in]   Size of the file
 *
 * @return  gboolean   Returns TRUE on success and FALSE on ERROR
 */
static gboolean gst_drm_src_get_size (GstBaseSrc * basesrc, guint64 * size)
{
  struct stat stat_results;
  GstDrmSrc *src = GST_DRM_SRC (basesrc);
  unsigned int offset;

  /* Gets the filesize for drm file by using seek oprations */
  if(src->is_oma==TRUE) {
    drm_util_seek (src->hfile, 0, DRM_SEEK_END);
    if (drm_util_tell(src->hfile, &offset) == TRUE) {
      /* FIXME : drm doesn't support 64 */
      *size = offset;
    }
    drm_util_seek (src->hfile, 0, DRM_SEEK_SET);
    src->read_position = 0;
    return TRUE;
  }

  if (!src->seekable) {
    GST_DEBUG_OBJECT (src, "non-seekable");
    return FALSE;
  }

  /* Gets the file size for standard file by using statistics */
  if (fstat (src->fd, &stat_results) < 0)
    return FALSE;

  *size = stat_results.st_size;
  GST_DEBUG_OBJECT (src, "size : %"G_GUINT64_FORMAT, *size);
  return TRUE;
}
/**
 * This function does the following:
 *  1. Checks the filename
 *  2. Opens the file and check statistics of the file
 *  3. Checks whether DRM file or not.
 *  4. Checks the DRM file type (supports only for OMA) if it is DRM
 *  5. Opens the DRM file if it is DRM
 *  6. Gets the DRM_FILE_HANDLE and sets the drm, seekable and regular flag.
 *  7. Checks the seeking for standard files
 *
 * @param   basesrc    [in]   BaseSrc Structure
 *
 * @return  gboolean   Returns TRUE on success and FALSE on ERROR
 */
static gboolean gst_drm_src_start (GstBaseSrc * basesrc)
{
  GstDrmSrc *src = GST_DRM_SRC (basesrc);
  struct stat stat_results;
  drm_result_e drm_result;
  drm_file_type_e file_type;
  off_t ret;

  /* Checks the filename */
  if (src->filename == NULL || src->filename[0] == '\0') {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,("No file name specified for reading."), (NULL));
    return FALSE;
  }

  /*  Opens the file and check statistics of the file */
  GST_INFO_OBJECT (src, "opening file %s", src->filename);
  src->fd = open (src->filename, O_RDONLY | O_BINARY);
  if (src->fd < 0) {
    if(errno == ENOENT) {
      GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL),("No such file \"%s\"", src->filename));
      return FALSE;
    }
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, ("Could not open file \"%s\" for reading.", src->filename), GST_ERROR_SYSTEM);
    return FALSE;
  }

  if (fstat (src->fd, &stat_results) < 0) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, ("Could not get info on \"%s\".", src->filename), (NULL));
    close (src->fd);
    return FALSE;
  }

  if (S_ISDIR (stat_results.st_mode)) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, ("\"%s\" is a directory.", src->filename), (NULL));
    close (src->fd);
    return FALSE;
  }

  if (S_ISSOCK (stat_results.st_mode)) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, ("File \"%s\" is a socket.", src->filename), (NULL));
    close (src->fd);
    return FALSE;
  }

  src->read_position = 0;


  /* Checks whether DRM file or not.*/
  drm_result = drm_get_file_type (src->filename, &file_type);
  if (drm_result != DRM_RETURN_SUCCESS) {
    GST_ERROR_OBJECT (src,"Error in drm_get_file_type(), error=%d", drm_result);
    return FALSE;
  }
  GST_DEBUG_OBJECT (src, "file_path = [%s], file_type = [%d]", src->filename, file_type);

#if 0
  if (DRM_TYPE_UNDEFINED != file_type) {
    drm_bool_type_e is_drm = DRM_FALSE;
    if (drm_is_drm_file(src->filename, &is_drm) == DRM_RETURN_SUCCESS) {
      if (is_drm == DRM_TRUE) {
	src->is_drm = TRUE;
      }
    }
    GST_DEBUG_OBJECT (src, "is drm = [%d]", src->is_drm);
  }
#endif

  /* We handles as DRM file if it is drm with OMA type */
  if (file_type == DRM_TYPE_OMA_V1) { /* FIMXE: what about DRM_TYPE_OMA_V2  */
    // Gets the DRM_FILE_HANDLE and sets the drm, seekable and regular flags.
    drm_util_seek (src->hfile, 0, DRM_SEEK_END);
    drm_util_seek (src->hfile, 0, DRM_SEEK_SET);

    src->seekable	= TRUE;
    src->is_regular	= TRUE;
    src->is_oma	= TRUE;

    LOG_TRACE("Exit");
    return TRUE;
  }

  if (file_type == DRM_TYPE_PLAYREADY || file_type == DRM_TYPE_PLAYREADY_ENVELOPE) { /* FIXME: what is envelope?? */
    src->is_playready = TRUE;
    src->event_posted = FALSE;
  }

  /* Checks the seeking for standard files */
  if (S_ISREG (stat_results.st_mode))
    src->is_regular = TRUE;

  ret = lseek (src->fd, 0, SEEK_END);
  if (ret < 0) {
    GST_LOG_OBJECT (src, "disabling seeking, not in mmap mode and lseek failed: %s", g_strerror (errno));
    src->seekable = FALSE;
  } else {
    src->seekable = TRUE;
  }

  lseek (src->fd, 0, SEEK_SET);
  src->seekable = src->seekable && src->is_regular;


  return TRUE;
}
/**
 * This function does the following:
 *  1. Closes the file desciptor and resets the flags
 *
 * @param   basesrc    [in]   BaseSrc Structure
 *
 * @return  gboolean   Returns TRUE on success and FALSE on ERROR
 */
static gboolean gst_drm_src_stop (GstBaseSrc * basesrc)
{
  GstDrmSrc *src = GST_DRM_SRC (basesrc);
  // 1. Closes the file desciptor and resets the flags
  if(src->fd > 0) {
#ifdef CONTROL_PAGECACHE
	DRMSRC_FADVISE_DONT(src->fd, 0, 0);
	src->accum = 0;
#endif
    close (src->fd);
  }

  src->fd = 0;
  src->is_regular = FALSE;
  src->event_posted = FALSE;
  src->is_playready = FALSE;

  return TRUE;
}
/**
 *
 * @param   void
 *
 * @return  GstURIType   Returns GST_URI_SRC
 */

static GstURIType gst_drm_src_uri_get_type (void)
{
  return GST_URI_SRC;
}

/**
 * This function does the following:
 *  1. Defines the list of protocols
 *
 * @param   void
 *
 * @return  gchar **   Returns the protocol list
 */

static gchar ** gst_drm_src_uri_get_protocols (void)
{
  static gchar *protocols[] = { "file", NULL };
  return protocols;
}
/**
 *
 * @param   handler [in] GstURIHandler structure
 *
 * @return  gchar*   Returns the uri
 */
static const gchar * gst_drm_src_uri_get_uri (GstURIHandler *handler)
{
  GstDrmSrc *src = GST_DRM_SRC (handler);
  return src->uri;
}
/**
 * This function does the following:
 *  1. Checks the protocol
 *  2. Checks the whether it is absolute or not
 *  3 sets the location
 *
 * @param   handler [in] GstURIHandler structure
 * @param   uri [in] uri string
 *
 * @return  gboolean   Returns TRUE on success and FALSE on Error
 */
static gboolean gst_drm_src_uri_set_uri (GstURIHandler *handler, const gchar * uri)
{
  gchar *location, *hostname = NULL;
  gboolean ret = FALSE;
  GstDrmSrc *src = GST_DRM_SRC (handler);
  GError *error = NULL;

  if (strcmp (uri, "file://") == 0) {
    /* Special case for "file://" as this is used by some applications
     *  to test with gst_element_make_from_uri if there's an element
     *  that supports the URI protocol. */
    gst_drm_src_set_location (src, NULL);
    return TRUE;
  }

  location = g_filename_from_uri (uri, &hostname, &error);

  if (!location || error) {
    if (error) {
      GST_WARNING_OBJECT (src, "Invalid URI '%s' for filesrc: %s", uri,
          error->message);
      g_error_free (error);
    } else {
      GST_WARNING_OBJECT (src, "Invalid URI '%s' for filesrc", uri);
    }
    goto beach;
  }

  if ((hostname) && (strcmp (hostname, "localhost"))) {
    /* Only 'localhost' is permitted */
    GST_WARNING_OBJECT (src, "Invalid hostname '%s' for filesrc", hostname);
    goto beach;
  }

  ret = gst_drm_src_set_location (src, location);

beach:
  if (location)
    g_free (location);
  if (hostname)
    g_free (hostname);

  return ret;
}


/**
 * This function does the following:
 *  1. Assignes the function pointer for URI related stuff
 *
 * @param   g_iface [in] an interface to URI handler
 * @param   iface_data [in] a gpointer
 *
 * @return  void
 */
static void gst_drm_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  /* Assigning the function pointer for URI related stuff */
  iface->get_type = gst_drm_src_uri_get_type;
  iface->get_protocols = gst_drm_src_uri_get_protocols;
  iface->get_uri = gst_drm_src_uri_get_uri;
  iface->set_uri = gst_drm_src_uri_set_uri;
}
/**
 * This function does the following:
 *  1. Registers an element as drmsrc
 *
 * @param   i_pPlugin [in] a plug-in structure
 *
 * @return  gboolean TRUE on SUCCESS and FALSE on Error
 */
static gboolean plugin_init(GstPlugin* i_pPlugin)
{
  return gst_element_register(i_pPlugin, "drmsrc", GST_RANK_NONE, GST_TYPE_DRM_SRC);
}
/**
 * This function does the following:
 *  1. plugin defination
 *
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  "drmsrc",
  "Plugin to read data from standad/DRM File",
  plugin_init,
  VERSION,
  "LGPL",
  "Samsung Electronics Co",
  "http://www.samsung.com/")

