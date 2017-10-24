/*
 * SLP2.0
 * Copyright (c) 2011 Samsung Electronics, Inc.
 * All rights reserved.
 *
 * This software is a confidential and proprietary information
 * of Samsung Electronics, Inc. ("Confidential Information").  You
 * shall not disclose such Confidential Information and shall use
 * it only in accordance with the terms of the license agreement
 * you entered into with Samsung Electronics.
 */

/*
 * @file        gstsubmux.c
 * @author      Deepak Singh (deep.singh@samsung.com)
 * @version     1.0
 * @brief       This source code implements the gstreamer plugin for subtitle muxing requirement in media player.
 *
 */

/*! Revision History:
 *! ---------------------------------------------------------------------------
 *!     DATE     |         AUTHOR               |             COMMENTS
 *! ---------------------------------------------------------------------------
 *! 1-4-2014        deep.singh@samsung.com                   created.
 */

#include "config.h"
#include "gstsubmux.h"
#include <gst/base/gstadapter.h>
#include <glib/gstdio.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gst/tag/tag.h>
#include <fcntl.h>
#include <unistd.h>
#include <gst/gst.h>
static const GstElementDetails gst_submux_plugin_details = GST_ELEMENT_DETAILS(
    "submux",
    "Codec/Parser/Subtitle",
    "muxing of different subtitle stream",
    "Samsung Electronics <www.samsung.com>"
);
static GstStaticPadTemplate gst_submux_sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink%d",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS("video/x-dvd-subpicture; text/plain; application/x-subtitle; "
                    "text/x-pango-markup;"
                    "application/x-usf; subpicture/x-pgs; subtitle/x-kate; application/x-subtitle; "
                    "application/x-subtitle-sami; application/x-subtitle-tmplayer; "
                    "application/x-subtitle-mpl2; application/x-subtitle-dks; "
                    "application/x-subtitle-qttext")
);
static GstStaticPadTemplate gst_submux_src_template = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/plain; text/x-pango-markup")
);
enum
{
  PROP_0,
  PROP_ENCODING,
  PROP_VIDEOFPS,
  PROP_EXTSUB_CURRENT_LANGUAGE,
  PROP_IS_INTERNAL,
  PROP_LANG_LIST

};

GST_DEBUG_CATEGORY_STATIC (gst_submux_debug);
#define GST_CAT_DEFAULT gst_submux_debug
#define _do_init(bla) \
    GST_DEBUG_CATEGORY_INIT (gst_submux_debug, "submux", 0, "submux");
////////////////////////////////////////////////////////
//        Gstreamer Base Prototype                    //
////////////////////////////////////////////////////////

GST_BOILERPLATE_FULL(Gstsubmux, gst_submux, GstElement, GST_TYPE_ELEMENT, _do_init);

#define GST_SUBMUX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_SUBMUX, GstsubmuxPrivate))
#define MAX_LANGUAGE 10
static gint gst_submux_buffer_list_sorting (gconstpointer a, gconstpointer b);
static gboolean gst_submux_create_pipelines(Gstsubmux *self,GstPad * pad);
static GstPad*  gst_submux_request_new_pad (GstElement * element,
       GstPadTemplate * templ, const gchar * req_name);
static void gst_submux_release_pad (GstElement * element, GstPad * pad);
static gchar* gst_submux_extract_data (Gstsubmux *submux);
static gboolean gst_create_own_language_list (Gstsubmux *submux) ;
static GstSubMuxFormat gst_submux_data_format_autodetect (gchar * match_str);
static gpointer gst_submux_data_format_autodetect_regex_once (GstSubMuxRegex regtype);
static gboolean gst_submux_format_autodetect (Gstsubmux * self);
static void gst_submux_base_init(gpointer klass);
static void gst_submux_class_init(GstsubmuxClass *klass);
static GstStateChangeReturn gst_submux_change_state (GstElement * element, GstStateChange transition);
static void gst_submux_init(Gstsubmux *submux, GstsubmuxClass *klass);
static gboolean gst_submux_setcaps(GstPad *pad, GstCaps *caps);
static GstFlowReturn gst_submux_chain (GstPad *pad, GstBuffer *buffer);
static void gst_submux_dispose(GObject *object);
static void gst_submux_loop (Gstsubmux * submux);
static gboolean gst_submux_stream_init(GstSubmuxStream * stream);
static void gst_submux_stream_deinit(GstSubmuxStream * stream,Gstsubmux * submux);
static void gst_submux_on_new_buffer (GstElement *appsink, void *data);
static gboolean gst_submux_handle_src_event (GstPad * pad, GstEvent * event);

static gboolean gst_submux_handle_sink_event (GstPad * pad, GstEvent * event);
////////////////////////////////////////////////////////
//        Plugin Utility Prototype                    //
////////////////////////////////////////////////////////
static void gst_submux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_submux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_submux_deinit_private_values(Gstsubmux *submux);
static gchar *convert_to_utf8 (const gchar * str, gsize len, const gchar * encoding,
    gsize * consumed, GError ** err, Gstsubmux * self);
#define DEFAULT_ENCODING           NULL
#define DEFAULT_CURRENT_LANGUAGE   NULL

////////////////////////////////////////////////////////
//        Gstreamer Base Functions                    //
////////////////////////////////////////////////////////

/*
**
**  Description : base init
**  Params      : (1) instance of gclass
**  return      : none
**  Comments    : The following code registers templates for src and sink pad.
**
*/
static void
gst_submux_base_init(gpointer g_class)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_submux_sink_template));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_submux_src_template));
    gst_element_class_set_details(element_class, &gst_submux_plugin_details);
}

/*
**
**  Description    : Initilizes the Gstsubmux's class
**  Params        : @ klass instance of submux plugin's class
**  return        : None
**  Comments    : Declaring properties and over-writing function pointers
**
*/
static void
gst_submux_class_init(GstsubmuxClass *klass)
{
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    g_type_class_add_private (klass, sizeof (GstsubmuxPrivate));
    gobject_class->set_property = gst_submux_set_property;
    gobject_class->get_property = gst_submux_get_property;
    g_object_class_install_property (gobject_class, PROP_ENCODING,
        g_param_spec_string ("subtitle-encoding", "subtitle charset encoding",
            "Encoding to assume if input subtitles are not in UTF-8 or any other "
            "Unicode encoding. If not set, the GST_SUBTITLE_ENCODING environment "
            "variable will be checked for an encoding to use. If that is not set "
            "either, ISO-8859-15 will be assumed.", DEFAULT_ENCODING,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, PROP_VIDEOFPS,
        gst_param_spec_fraction ("video-fps", "Video framerate",
            "Framerate of the video stream. This is needed by some subtitle "
            "formats to synchronize subtitles and video properly. If not set "
            "and the subtitle format requires it subtitles may be out of sync.",
            0, 1, G_MAXINT, 1, 24000, 1001,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (gobject_class, PROP_EXTSUB_CURRENT_LANGUAGE,
          g_param_spec_string ("current-language", "Current language",
                "Current language of the subtitle in external subtitle case.",
                DEFAULT_CURRENT_LANGUAGE,
                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (gobject_class, PROP_IS_INTERNAL,
          g_param_spec_boolean ("is-internal", "is internal",
              "TRUE for internal subtitle case",
              FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property (gobject_class, PROP_LANG_LIST,
          g_param_spec_pointer ("lang-list", "language list", "List of languages selected/not selected",
               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    parent_class = g_type_class_peek_parent (klass);
    gstelement_class->request_new_pad = GST_DEBUG_FUNCPTR(gst_submux_request_new_pad);
    gobject_class->dispose = GST_DEBUG_FUNCPTR(gst_submux_dispose);
    gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_submux_change_state);
    gstelement_class->release_pad = GST_DEBUG_FUNCPTR (gst_submux_release_pad);
}

/*
**
**  Description    : Initilizes the submux element
**  Params        : (1)instance of submux (2) instance of submux class
**  return        : None
**  Comments    : instantiate pads and add them to element, set pad calback functions
**
*/
static void
gst_submux_init(Gstsubmux *submux, GstsubmuxClass *klass)
{
  GST_DEBUG_OBJECT (submux, "Entering in init");
  submux->priv = GST_SUBMUX_GET_PRIVATE(submux);
  submux->srcpad = gst_pad_new_from_static_template(&gst_submux_src_template, "src");
  gst_pad_set_event_function (submux->srcpad,
            GST_DEBUG_FUNCPTR (gst_submux_handle_src_event));
  submux->priv->first_buffer = FALSE;
  gst_segment_init (&submux->segment, GST_FORMAT_TIME);
  submux->flushing = FALSE;
  submux->msl_streams = NULL;
  submux->stop_loop = FALSE;
  submux->need_segment = TRUE;
  submux->pipeline_made = FALSE;
  submux->external_sinkpad = FALSE;
  submux->detected_encoding = NULL;
  submux->encoding = NULL;
  submux->seek_came = FALSE;
  submux->sinkpads_count = 0;
  submux->langlist_msg_posted = FALSE;
  submux->cur_buf_array = NULL;
  GST_DEBUG_OBJECT (submux, "Making flushing FALSE");
  submux->priv->is_internal = FALSE;
  submux->external_filepath = NULL;
  gst_element_add_pad (GST_ELEMENT (submux), submux->srcpad);
  GST_DEBUG_OBJECT (submux, "Exiting in init");
}

/*
**
**  Description    : for setting the property of submux
**  return        : None
**  Comments    : To set the various properties of submux
**
*/
static void
gst_submux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstsubmux *submux = GST_SUBMUX (object);
  guint length = 0;
  gint i = 0;
  GstLangStruct *cur_language=NULL;
  GstSubmuxStream *cur_stream = NULL;
  GST_OBJECT_LOCK (submux);
  length = g_list_length(submux->priv->lang_list);
  switch (prop_id) {
    case PROP_ENCODING:
      g_free (submux->encoding);
      submux->encoding = g_value_dup_string (value);
      GST_DEBUG_OBJECT (submux, "subtitle encoding set to %s",
          GST_STR_NULL (submux->encoding));
      for(i = 0;i < length;i++) {
        cur_stream = g_list_nth_data(submux->streams,i);
        GST_DEBUG_OBJECT (submux, "setting the subtitle-encoding to %s", submux->encoding);
        g_object_set (G_OBJECT (cur_stream->pipe_struc.parser), "subtitle-encoding", submux->encoding, NULL);
      }
      break;
    case PROP_VIDEOFPS:
    {
      submux->fps_n = gst_value_get_fraction_numerator (value);
      submux->fps_d = gst_value_get_fraction_denominator (value);
      GST_DEBUG_OBJECT (submux, "video framerate set to %d/%d", submux->fps_n, submux->fps_d);
      break;
    }
    case PROP_EXTSUB_CURRENT_LANGUAGE: {
      for (i = 0; i < length; i++) {
        cur_stream = g_list_nth_data(submux->streams, i);
        cur_language = g_list_nth_data(submux->priv->lang_list, i);
        GST_DEBUG_OBJECT (submux, "value of current-language key is %s", cur_language->language_key);
        g_object_set (G_OBJECT (cur_stream->pipe_struc.parser), "current-language",
                      cur_language->language_key, NULL);
      }
      gchar *dup = g_value_dup_string (value);
      GST_DEBUG_OBJECT (submux, "Setting property to %s", dup);
      g_free(dup);
    }
    break;
    case PROP_IS_INTERNAL: {
      submux->priv->is_internal = g_value_get_boolean (value);
      GST_DEBUG_OBJECT (submux, "Setting the is_internal prop to %d", submux->priv->is_internal);
      break;
    }
    case PROP_LANG_LIST: {
      submux->priv->lang_list = (GList*) g_value_get_pointer (value);
      GST_DEBUG_OBJECT (submux, "updating the languages list and length is %d", g_list_length (submux->priv->lang_list));
      submux->msl_streams = g_list_copy (submux->priv->lang_list);

      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (submux);
}

/*
**
**  Description    : for getting the property of submux
**  return        : None
**  Comments    : To get the various properties of submux in case called by MSL
**
*/
static void
gst_submux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstsubmux *submux = GST_SUBMUX (object);

  GST_OBJECT_LOCK (submux);
  switch (prop_id) {
    case PROP_ENCODING:
      g_value_set_string (value, submux->encoding);
      break;
    case PROP_VIDEOFPS:
      gst_value_set_fraction (value, submux->fps_n, submux->fps_d);
      break;
    case PROP_EXTSUB_CURRENT_LANGUAGE:
      GST_DEBUG_OBJECT (submux, "Getting the current language");
      break;
    case PROP_IS_INTERNAL: {
      g_value_set_boolean(value,submux->priv->is_internal);
      break;
    }
    case PROP_LANG_LIST: {
      g_value_set_pointer(value,(gpointer)(submux->priv->lang_list));
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (submux);
}

static void
gst_submux_dispose (GObject *object)
{
  Gstsubmux *submux = GST_SUBMUX(object);
  int i = 0;
  gchar *pad_name = gst_pad_get_name (submux->srcpad);
  if (submux && GST_PAD_TASK(submux->srcpad)) {
    GST_INFO_OBJECT (submux, "Stopping pad task : %s", pad_name);
    GST_DEBUG_OBJECT (submux, "Stopping pad task : on src pad %p", submux->srcpad);
    gst_pad_stop_task (submux->srcpad);
    GST_INFO_OBJECT (submux, "stopped pad task : %s", pad_name);
  }
  g_free(pad_name);
  if (submux->srcpad) {
    gst_element_remove_pad (GST_ELEMENT_CAST (submux), submux->srcpad);
    submux->srcpad = NULL;
  }
  if (submux->priv->is_internal) {
    for (i = 0; i < (submux->sinkpads_count); i++){
      gst_submux_stream_deinit (g_list_nth_data (submux->streams, i), submux);
    }
  } else {
    for (i = 0; i < g_list_length (submux->priv->lang_list); i++) {
      gst_submux_stream_deinit (g_list_nth_data (submux->streams, i), submux);
    }
  }
  gst_submux_deinit_private_values (submux);

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
  GST_DEBUG_OBJECT (submux, "Returning from finalize");
}

static void
gst_submux_stop (Gstsubmux* submux)
{
  GstSubmuxStream *new_stream = NULL;
  guint i = 0;
  submux->stop_loop = TRUE;
  GST_INFO_OBJECT (submux, "stopping the loop");
  if (submux->priv->is_internal) {
    for (i = 0; i < (submux->sinkpads_count); i++) {
      new_stream =  g_list_nth_data (submux->streams, i);
      if (new_stream) {
        g_mutex_lock (new_stream->queue_lock);
        g_cond_signal (new_stream->queue_empty);
        g_mutex_unlock (new_stream->queue_lock);
      }
    }
  } else {
    for (i = 0; i < g_list_length (submux->priv->lang_list); i++) {
      new_stream =  g_list_nth_data (submux->streams, i);
      if (new_stream) {
        g_mutex_lock (new_stream->queue_lock);
        g_cond_signal (new_stream->queue_empty);
        g_mutex_unlock (new_stream->queue_lock);
      }
    }
  }
}

static GstStateChangeReturn
gst_submux_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  Gstsubmux *submux = GST_SUBMUX (element);
  gboolean bret = FALSE;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      GST_INFO_OBJECT (submux,"PAUSED->PLAYING");
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_INFO_OBJECT (submux,"PAUSED->READY");
      gst_submux_stop (submux);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      GST_INFO_OBJECT (submux,"PLAYING->PAUSED");
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      GST_INFO_OBJECT (submux,"PAUSED->READY");
      if(submux->msl_streams) {
        g_list_free(submux->msl_streams);
        submux->msl_streams = NULL;
      }
      if (submux->priv->lang_list && !submux->priv->is_internal) {
        g_list_free (submux->priv->lang_list);
        submux->priv->lang_list = NULL;
      }
      if (submux->external_filepath) {
        g_free (submux->external_filepath);
        submux->external_filepath = NULL;
      }
      GST_WARNING_OBJECT(submux,"stopping has been called ...Moved after change_state");
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      GST_INFO_OBJECT (submux,"READY->NULL");
      break;
    default:
      break;
  }
  return ret;
}

/*
**
**  Description    : Setting the caps on sink pad based on upstream element's src pad
**  Params        : (1) GstPad to set the capabilities of
**                   (2) caps to be set
**  return        : TRUE on success
**  Comments    : this function handles the link with other elements
**
*/
static gboolean
gst_submux_setcaps (GstPad *pad, GstCaps *caps)
{
  return TRUE;
}

/*extracting data for file format detection*/
static gchar* gst_submux_extract_data (Gstsubmux *submux){
  gchar * file_path_type = NULL;
  gchar * file_path = NULL;
  gchar * temp_path = NULL;
  gchar  *line = NULL;
  gboolean is_converted = FALSE;
  gchar *converted = NULL;
  FILE  * fp = NULL;
  guint charCount = 0;
  GError *err = NULL;
  gsize * consumed = NULL;

  GstQuery *cquery;
  GstStructure *structure;
  const GValue *value;
  GstPad *sinkpad = (GstPad *)g_list_nth_data (submux->sinkpad, 0);
  structure = gst_structure_new ("FileSrcURI",
                                 "file-uri", G_TYPE_STRING, NULL, NULL);

  cquery = gst_query_new_application (GST_QUERY_CUSTOM, structure);

  if (!gst_pad_peer_query (sinkpad, cquery))
  {
    GST_ERROR_OBJECT (submux, "Failed to query SMI file path");
    gst_query_unref (cquery);
    return NULL;
  }
  structure = gst_query_get_structure (cquery);
  value = gst_structure_get_value (structure, "file-uri");
  file_path = g_strdup (g_value_get_string (value));

  if (file_path == NULL){
    GST_ERROR_OBJECT (submux, "Could not parse the SMI file path");
    gst_query_unref (cquery);
    return NULL;
  }

  gst_query_unref (cquery);
  temp_path = file_path;
  GST_INFO_OBJECT (submux, "File path comes as %s", file_path);

  file_path_type = g_strndup ((gchar *) file_path, 4);
  GST_INFO_OBJECT (submux, "Received file path by query = %s, %s", file_path, file_path_type);
  if (!g_strcmp0(file_path_type, "file")){
    file_path += 7;
    GST_INFO_OBJECT (submux, "File path comes as %s", file_path);

    fp = fopen (file_path, "r");
    if (!fp){
      GST_ERROR_OBJECT (submux, "Failed to open file");
      g_free(file_path_type);
      g_free(temp_path);
      return NULL;
    }
  } else {
    GST_ERROR_OBJECT (submux, "File is not local");
    g_free(file_path_type);
    g_free(temp_path);
    return NULL;
  }
  line = (gchar*)g_malloc (2049);
  charCount = fread (line, sizeof(char), 2048, fp);
  line[charCount] = '\0';
  if (submux->encoding && strcmp (submux->encoding, "UTF-8"))
    converted = convert_to_utf8 (line, charCount, submux->encoding, consumed, &err, submux);

  if (converted)
  {
    GST_INFO("returned from conversion and length of converted string is[%d]", strlen(converted));
    is_converted = TRUE;
  }
  if (!charCount) {
    GST_WARNING_OBJECT (submux, "fread returned zero bytes");
    fclose (fp);
    g_free(file_path_type);
    g_free(temp_path);
    if(is_converted) {
      g_free(converted);
    }
    g_free(line);
    return NULL;
  }
  g_free(file_path_type);
  g_free(temp_path);
  fclose (fp);
  if(is_converted) {
    return converted;
  }
  return line;
}

static gpointer
gst_submux_data_format_autodetect_regex_once (GstSubMuxRegex regtype)
{
  gpointer result = NULL;
  GError *gerr = NULL;
  switch (regtype) {
    case GST_SUB_PARSE_REGEX_MDVDSUB:
      result =
          (gpointer) g_regex_new ("^\\{[0-9]+\\}\\{[0-9]+\\}",
          G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, &gerr);
      if (result == NULL) {
        g_warning ("Compilation of mdvd regex failed: %s", gerr->message);
        g_error_free (gerr);
      }
      break;
    case GST_SUB_PARSE_REGEX_SUBRIP:
      result = (gpointer) g_regex_new ("^ {0,3}[ 0-9]{1,4}\\s*(\x0d)?\x0a"
          " ?[0-9]{1,2}: ?[0-9]{1,2}: ?[0-9]{1,2}[,.] {0,2}[0-9]{1,3}"
          " +--> +[0-9]{1,2}: ?[0-9]{1,2}: ?[0-9]{1,2}[,.] {0,2}[0-9]{1,2}",
          G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, &gerr);
      if (result == NULL) {
        g_warning ("Compilation of subrip regex failed: %s", gerr->message);
        g_error_free (gerr);
      }
      break;
    case GST_SUB_PARSE_REGEX_DKS:
      result = (gpointer) g_regex_new ("^\\[[0-9]+:[0-9]+:[0-9]+\\].*",
          G_REGEX_RAW | G_REGEX_OPTIMIZE, 0, &gerr);
      if (result == NULL) {
        g_warning ("Compilation of dks regex failed: %s", gerr->message);
        g_error_free (gerr);
      }
      break;
    default:
      GST_WARNING ("Trying to allocate regex of unknown type %u", regtype);
  }
  return result;
}

static GstSubMuxFormat
gst_submux_data_format_autodetect (gchar * match_str)
{
  guint n1, n2, n3;

  static GOnce mdvd_rx_once = G_ONCE_INIT;
  static GOnce subrip_rx_once = G_ONCE_INIT;
  static GOnce dks_rx_once = G_ONCE_INIT;

  GRegex *mdvd_grx;
  GRegex *subrip_grx;
  GRegex *dks_grx;

  g_once (&mdvd_rx_once,
      (GThreadFunc) gst_submux_data_format_autodetect_regex_once,
      (gpointer) GST_SUB_PARSE_REGEX_MDVDSUB);
  g_once (&subrip_rx_once,
      (GThreadFunc) gst_submux_data_format_autodetect_regex_once,
      (gpointer) GST_SUB_PARSE_REGEX_SUBRIP);
  g_once (&dks_rx_once,
      (GThreadFunc) gst_submux_data_format_autodetect_regex_once,
      (gpointer) GST_SUB_PARSE_REGEX_DKS);

  mdvd_grx = (GRegex *) mdvd_rx_once.retval;
  subrip_grx = (GRegex *) subrip_rx_once.retval;
  dks_grx = (GRegex *) dks_rx_once.retval;

  if (g_regex_match (mdvd_grx, match_str, 0, NULL) == TRUE) {
    GST_LOG ("MicroDVD (frame based) format detected");
    return GST_SUB_PARSE_FORMAT_MDVDSUB;
  }
  if (g_regex_match (subrip_grx, match_str, 0, NULL) == TRUE) {
    GST_LOG ("SubRip (time based) format detected");
    return GST_SUB_PARSE_FORMAT_SUBRIP;
  }
  if (g_regex_match (dks_grx, match_str, 0, NULL) == TRUE) {
    GST_LOG ("DKS (time based) format detected");
    return GST_SUB_PARSE_FORMAT_DKS;
  }

  if (!strncmp (match_str, "FORMAT=TIME", 11)) {
    GST_LOG ("MPSub (time based) format detected");
    return GST_SUB_PARSE_FORMAT_MPSUB;
  }
  if (strstr (match_str, "<SAMI>") != NULL ||
      strstr (match_str, "<sami>") != NULL) {
    GST_LOG ("SAMI (time based) format detected");
    return GST_SUB_PARSE_FORMAT_SAMI;
  }
  /* we're boldly assuming the first subtitle appears within the first hour */
  if (sscanf (match_str, "0:%02u:%02u:", &n1, &n2) == 2 ||
      sscanf (match_str, "0:%02u:%02u=", &n1, &n2) == 2 ||
      sscanf (match_str, "00:%02u:%02u:", &n1, &n2) == 2 ||
      sscanf (match_str, "00:%02u:%02u=", &n1, &n2) == 2 ||
      sscanf (match_str, "00:%02u:%02u,%u=", &n1, &n2, &n3) == 3) {
    GST_LOG ("TMPlayer (time based) format detected");
    return GST_SUB_PARSE_FORMAT_TMPLAYER;
  }
  if (sscanf (match_str, "[%u][%u]", &n1, &n2) == 2) {
    GST_LOG ("MPL2 (time based) format detected");
    return GST_SUB_PARSE_FORMAT_MPL2;
  }
  if (strstr (match_str, "[INFORMATION]") != NULL) {
    GST_LOG ("SubViewer (time based) format detected");
    return GST_SUB_PARSE_FORMAT_SUBVIEWER;
  }
  if (strstr (match_str, "{QTtext}") != NULL) {
    GST_LOG ("QTtext (time based) format detected");
    return GST_SUB_PARSE_FORMAT_QTTEXT;
  }

  GST_WARNING ("no subtitle format detected");
  return GST_SUB_PARSE_FORMAT_UNKNOWN;
}

/*checking the type of subtitle*/
static gboolean
gst_submux_format_autodetect (Gstsubmux *self)
{
  gchar *data;
  GstSubMuxFormat format;
  gchar * line = NULL;
  if (self->priv->is_internal) {
    GST_DEBUG_OBJECT (self, "File is of internal type");
    return TRUE;
  }
  line = gst_submux_extract_data (self);
  if (!line)
    return FALSE;
  if (strlen (line) < 30) {
    GST_WARNING_OBJECT (self, "File too small to be a subtitles file");
    g_free(line);
    return FALSE;
  }

  data = g_strndup (line, 35);
  format = gst_submux_data_format_autodetect (data);
  g_free (data);

  self->priv->parser_type = format;
  g_free(line);

  return TRUE;
}

/*to validate the number of languages in case of sami files*/
static gboolean
gst_calculate_number_languages(Gstsubmux *self) {
  gchar* text=NULL;
  gchar *start = NULL;
  gchar *end = NULL;
  gint count = 0;
  gchar* found = NULL;
  gchar * name_temp = NULL;
  int i = 0, j = 0;

  GST_DEBUG_OBJECT (self, "Entering in language number");

  if ((self->priv->parser_type != GST_SUB_PARSE_FORMAT_SAMI) || self->priv->is_internal)
    return TRUE;

  text = gst_submux_extract_data (self);
  start = g_strstr_len (text, strlen (text), "!--");
  if (!start) {
    GST_ERROR_OBJECT (self, "Could not found the language start code in smi file");
    return gst_create_own_language_list(self);
  }
  end =  g_strstr_len (start, strlen (start), "-->");
  if (!end){
    GST_ERROR_OBJECT (self, "Could not found the language end code in smi file");
    goto error;
  }

  found = start + 1;

  while (TRUE) {
    found = (gchar*)strcasestr (found, "lang");
    if (!found)
       break;
    found++;
    count++;
  }

  if (!count)
  {
    return gst_create_own_language_list(self);
  }

  for (i = 0; i < count; i++) {
    gchar *attr_name = NULL, *attr_value = NULL;
    GstLangStruct *new = NULL;

    start = (gchar*)strcasestr (start, "lang:");
    attr_value = (gchar*)malloc (3);
    if (!attr_value) {
      GST_ERROR_OBJECT (self, "memory could not be allocated through malloc call");
      goto error;
    }
    start = start + 5;
    strncpy (attr_value, start, 2);
    attr_value[2] = '\0';
    GST_DEBUG_OBJECT (self, "Language value comes as %s", attr_value);
    name_temp = start;
    while (TRUE) {
      if (*name_temp == '{') {
        int character_count = 0;
        while (TRUE) {
          name_temp--;

          if (*name_temp == '.') {
            attr_name = (gchar*) malloc (character_count + 1);
            break;
          } else if (*name_temp != ' ')
             character_count++;
        }
        break;
      }
      name_temp--;
    }
    if (!attr_name) {
      GST_ERROR_OBJECT (self, "Could not find the languages field in the file");
      free(attr_value);
      goto error;
    }
    name_temp++;
    for (j = 0; *(name_temp + j) != ' '; j++) {
      attr_name[j] = *(name_temp + j);
    }
    attr_name[j] = '\0';
    new = g_new0 (GstLangStruct, 1);
    new->language_code = (gchar*) malloc (strlen (attr_value) + 1);
    if (new->language_code && attr_value)
      strcpy (new->language_code, attr_value);
    new->language_key = (gchar*) malloc (strlen (attr_name) + 1);
    if (new->language_key && attr_name)
      strcpy (new->language_key, attr_name);
    free (attr_name);
    free (attr_value);
    self->priv->lang_list = g_list_append (self->priv->lang_list, new);
  }
  g_free(text);
  return TRUE;
error:
  g_free(text);
  return FALSE;
}

/*to initialize stream*/
static gboolean gst_submux_stream_init(GstSubmuxStream * stream)
{
  stream->duration = 0;
  stream->need_segment = TRUE;
  stream->flushing = FALSE;
  stream->eos_sent = FALSE;
  stream->eos_came = FALSE;
  stream->discont_came = FALSE;
  stream->eos_ts = -1;
  stream->last_ts = -1;
  stream->queue = g_queue_new ();
  stream->queue_empty = g_cond_new ();
  stream->queue_lock = g_mutex_new ();
  stream->flush_done = FALSE;
  return TRUE;
}

/*to create pipelines according to internal and external subtitle*/
gboolean gst_submux_create_pipelines(Gstsubmux *self, GstPad * pad)
{
  int i = 0;
  GstStateChangeReturn ret;
  GstSubmuxStream *new_stream;
  guint length = 0;

  if (!self->priv->is_internal) {
    GstLangStruct *cur_language=NULL;

    GST_DEBUG_OBJECT (self, "creating the pipeline for external pipeline");
    if (self->priv->parser_type == GST_SUB_PARSE_FORMAT_SAMI) {
      if (!self->priv->lang_list) {
        GST_ERROR_OBJECT(self, "failed to get the lang list");
        return FALSE;
      }
      length = g_list_length (self->priv->lang_list);
    } else {
      length = 1;
    }

    GST_DEBUG_OBJECT (self, "number of tentative languages present are %d", length);

    for (i = 0; i < length; i++) {
      new_stream = g_new0 (GstSubmuxStream, 1);
      if (!gst_submux_stream_init(new_stream)) {
        GST_ERROR_OBJECT (self, "stream init is failed");
        return FALSE;
      }
      GST_DEBUG_OBJECT (self, "stream init has been done for stream[%d]", i);

      new_stream->pipe_struc.pipe = gst_pipeline_new ("subtitle-pipeline");
      if (!new_stream->pipe_struc.pipe) {
        GST_ERROR_OBJECT (self, "failed to create pipeline");
        return FALSE;
      }
      GST_DEBUG_OBJECT (self, "creating appsrc");

      /* creating source element */
      new_stream->pipe_struc.appsrc = gst_element_factory_make ("appsrc", "pipe_appsrc");
      if (!new_stream->pipe_struc.appsrc) {
        GST_ERROR_OBJECT (self, "failed to create appsrc");
        return FALSE;
      }

      g_object_set (G_OBJECT (new_stream->pipe_struc.appsrc), "block", 1, NULL);
      g_object_set (G_OBJECT (new_stream->pipe_struc.appsrc), "max-bytes", (guint64)1, NULL);
      /* create sink element */
      new_stream->pipe_struc.appsink =  gst_element_factory_make ("appsink", "pipe_appsink");
      if (!new_stream->pipe_struc.appsink) {
        GST_ERROR_OBJECT (self, "failed to create appsink");
        return FALSE;
      }
      g_object_set (G_OBJECT (new_stream->pipe_struc.appsink), "sync", FALSE, "emit-signals", TRUE, NULL);
      g_object_set(G_OBJECT (new_stream->pipe_struc.appsrc),"emit-signals", TRUE, NULL);


      /* create parsing element */
      new_stream->pipe_struc.parser = gst_element_factory_make("subparse","pipe_parser");
      if (!new_stream->pipe_struc.parser) {
        GST_ERROR_OBJECT (self, "failed to create parser");
        return FALSE;
      }
      if (self->priv->parser_type == GST_SUB_PARSE_FORMAT_SAMI) {
        cur_language = g_list_nth_data(self->priv->lang_list, i);
        g_object_set (G_OBJECT (new_stream->pipe_struc.parser), "current-language",
                              cur_language->language_key, NULL);
      }
      GST_DEBUG_OBJECT (self, "value of subtitle-encoding  is %s", self->encoding);
      g_object_set (G_OBJECT (new_stream->pipe_struc.parser), "subtitle-encoding", self->encoding, NULL);
      g_object_set (G_OBJECT (new_stream->pipe_struc.appsrc), "stream-type",0,"format",GST_FORMAT_TIME, NULL);
      gst_bin_add_many (GST_BIN ( new_stream->pipe_struc.pipe), new_stream->pipe_struc.appsink, new_stream->pipe_struc.appsrc,new_stream->pipe_struc.parser, NULL);
      if (!gst_element_link_many (new_stream->pipe_struc.appsrc, new_stream->pipe_struc.parser,new_stream->pipe_struc.appsink, NULL)) {
        GST_ERROR_OBJECT (self, "failed to link elements");
        return FALSE;
      }

      GST_DEBUG_OBJECT (self, "reached here and linking successful");

      ret = gst_element_set_state (new_stream->pipe_struc.pipe, GST_STATE_PLAYING);
      if (ret == GST_STATE_CHANGE_FAILURE) {
        GST_ERROR_OBJECT (self, "set_state failed...");
        return FALSE;
      }
      GST_DEBUG_OBJECT (self, "state has been changed succesfully");
      self->streams = g_list_append(self->streams, new_stream);
      self->priv->stream_count++;
      g_signal_connect (new_stream->pipe_struc.appsink, "new-buffer",  G_CALLBACK (gst_submux_on_new_buffer), g_list_nth_data(self->streams,i) );
    }
  } else {
    length = self->sinkpads_count;
    for (i = 0; i < length; i++) {
      new_stream = g_new0 (GstSubmuxStream, 1);
      if (!gst_submux_stream_init (new_stream)) {
        GST_ERROR_OBJECT (self, "stream init is failed");
        return FALSE;
     }

      self->streams=g_list_append(self->streams,new_stream);
      self->priv->stream_count++;
    }
    self->pipeline_made  = TRUE;
  }

  self->cur_buf_array = g_malloc0 (self->priv->stream_count * (sizeof (GstBuffer *)));
  if (!self->cur_buf_array) {
    GST_ERROR_OBJECT (self, "failed to allocate memory..");
    return FALSE;
  }
  return TRUE;
}

/* call back on recieving the new buffer in appsink pad */
static void
gst_submux_on_new_buffer (GstElement *appsink, void *data)
{
  GstSubmuxStream *stream = (GstSubmuxStream  *)data;
  GstBuffer *inbuf = NULL;

  if (!stream) {
    GST_WARNING("Stream not available...");
    return;
  }
  g_mutex_lock (stream->queue_lock);
  inbuf = gst_app_sink_pull_buffer ((GstAppSink *)appsink);
  if (!inbuf) {
    GST_WARNING_OBJECT (stream, "Input buffer not available...");
    g_mutex_unlock (stream->queue_lock);
    return;
  }
  if(stream->eos_ts == -1) {
    if (!strcmp ((const char*)GST_BUFFER_DATA (inbuf), "eos") && GST_BUFFER_FLAG_IS_SET(inbuf,GST_BUFFER_FLAG_GAP)){
      stream->eos_ts = stream->last_ts;
      if (stream->eos_ts <= stream->seek_ts) {
        g_queue_push_tail (stream->queue, inbuf);
        g_cond_signal (stream->queue_empty);
        g_mutex_unlock (stream->queue_lock);
        GST_INFO_OBJECT (stream, "signaling queue empty signal as we are seeking beyond last subtitle");
        return;
      }
      gst_buffer_unref(inbuf);
    } else {
      stream->last_ts = GST_BUFFER_DURATION(inbuf) + GST_BUFFER_TIMESTAMP(inbuf);
    }
  } else if (stream->eos_ts <= stream->seek_ts) {
    gst_buffer_unref(inbuf);
    GstBuffer *buf = gst_buffer_new_and_alloc (3 + 1);
    GST_DEBUG_OBJECT(stream, "sending EOS buffer to chain\n");
    GST_DEBUG_OBJECT (stream, "EOS. Pushing remaining text (if any)");
    GST_BUFFER_DATA (buf)[0] = 'e';
    GST_BUFFER_DATA (buf)[1] = 'o';
    GST_BUFFER_DATA (buf)[2] = 's';
    GST_BUFFER_DATA (buf)[3] = '\0';
    /* play it safe */
    GST_BUFFER_SIZE (buf) = 3;
    GST_BUFFER_FLAG_SET(buf,GST_BUFFER_FLAG_GAP);
    g_queue_push_tail (stream->queue, buf);
    g_cond_signal (stream->queue_empty);
    g_mutex_unlock (stream->queue_lock);
    GST_INFO_OBJECT (stream,"signaling queue empty signal as we are seeking beyond last subtitle");
    return;
  }
  if (!stream->discont_came) {
    stream->discont_came = GST_BUFFER_IS_DISCONT (inbuf);
    if (stream->discont_came) {
      GST_DEBUG_OBJECT (stream, "first buffer with discont on new_buffer for stream with ts = %"
                        GST_TIME_FORMAT", dur = %"GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(inbuf)),
                        GST_TIME_ARGS(GST_BUFFER_DURATION(inbuf)));
    }
  }

  if (!stream->discont_came) {
    GST_DEBUG_OBJECT (stream, "rejecting the buffer in appsink on new_buffer for stream with ts = %"
                      GST_TIME_FORMAT", dur = %"GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(inbuf)),
                      GST_TIME_ARGS(GST_BUFFER_DURATION(inbuf)));
    gst_buffer_unref(inbuf);
    g_mutex_unlock (stream->queue_lock);
    return;
  }
  g_queue_push_tail (stream->queue, inbuf);
  g_cond_signal (stream->queue_empty);
  g_mutex_unlock (stream->queue_lock);
  GST_DEBUG_OBJECT (stream, "signaling queue empty signal");
  return;
}

gchar *
convert_to_utf8 (const gchar * str, gsize len, const gchar * encoding,
    gsize * consumed, GError ** err, Gstsubmux * self)
{
  gchar *ret = NULL;

  /* The char cast is necessary in glib < 2.24 */
  ret =
      g_convert_with_fallback (str, len, "UTF-8", encoding, (char *) "*",
      consumed, NULL, err);

  if (ret == NULL)
  {
    GST_DEBUG_OBJECT (self, "g_convert_with_fallback returns NULL");
    return ret;
  }

  /* + 3 to skip UTF-8 BOM if it was added */
  len = strlen (ret);
  if (len >= 3 && (guint8) ret[0] == 0xEF && (guint8) ret[1] == 0xBB
      && (guint8) ret[2] == 0xBF)
    g_memmove (ret, ret + 3, len + 1 - 3);

  return ret;
}

static gchar *
detect_encoding (const gchar * str, gsize len)
{
  if (len >= 3 && (guint8) str[0] == 0xEF && (guint8) str[1] == 0xBB
      && (guint8) str[2] == 0xBF)
    return g_strdup ("UTF-8");

  if (len >= 2 && (guint8) str[0] == 0xFE && (guint8) str[1] == 0xFF)
    return g_strdup ("UTF-16BE");

  if (len >= 2 && (guint8) str[0] == 0xFF && (guint8) str[1] == 0xFE)
    return g_strdup ("UTF-16LE");

  if (len >= 4 && (guint8) str[0] == 0x00 && (guint8) str[1] == 0x00
      && (guint8) str[2] == 0xFE && (guint8) str[3] == 0xFF)
    return g_strdup ("UTF-32BE");

  if (len >= 4 && (guint8) str[0] == 0xFF && (guint8) str[1] == 0xFE
      && (guint8) str[2] == 0x00 && (guint8) str[3] == 0x00)
    return g_strdup ("UTF-32LE");

  return NULL;
}

/* If language list is not present in smi file, check the body and create our own list */
static gboolean
gst_create_own_language_list (Gstsubmux *self)
{
  gchar * file_path_type = NULL;
  gchar * temp_path = NULL;
  gchar * file_path = NULL;
  gsize * consumed = NULL;
  guint keyCount = 0;
  GError *err = NULL;
  GstPad *sinkpad = (GstPad *) g_list_nth_data(self->sinkpad, 0);
  GstQuery *cquery;
  GstStructure *structure;
  const GValue *value;
  gchar* langkey[MAX_LANG];
  gint langKey_length[MAX_LANG];
  FILE *fp=NULL;
  gint i=0;
  structure = gst_structure_new ("FileSrcURI", "file-uri", G_TYPE_STRING, NULL, NULL);

  cquery = gst_query_new_application (GST_QUERY_CUSTOM, structure);

  if (!gst_pad_peer_query (sinkpad, cquery)) {
    GST_ERROR_OBJECT (self, "Failed to query SMI file path");
    gst_query_unref (cquery);
    return FALSE;
  }
  structure = gst_query_get_structure (cquery);
  value = gst_structure_get_value (structure, "file-uri");
  file_path = g_strdup (g_value_get_string (value));

  if (file_path == NULL){
    GST_ERROR_OBJECT (self, "Could not parse the SMI file path");
    gst_query_unref (cquery);
    return FALSE;
  }
  gst_query_unref (cquery);
  temp_path = file_path;
  GST_INFO_OBJECT (self, "File path comes as %s", file_path);

  file_path_type = g_strndup ((gchar *) file_path, 4);
  GST_INFO_OBJECT (self, "Received file path by query = %s, %s", file_path, file_path_type);
  if (!g_strcmp0(file_path_type, "file")){
    file_path += 7;
    GST_INFO_OBJECT (self, "File path comes as %s", file_path);

    fp = fopen (file_path, "r");
    if (!fp){
      GST_ERROR_OBJECT (self, "Failed to open file");
      g_free(temp_path);
      g_free(file_path_type);
      return FALSE;
    }
  }
  for( i=0;i<MAX_LANG;i++){
    langkey[i]=NULL;
    langKey_length[i]=0;
  }
  gboolean lang_found= FALSE;
  while (!feof (fp) ){
    gchar line[1025];
    guint charCount = 0;
    gboolean conversion = TRUE;
    gchar *result = NULL;
    gchar *con_temp = NULL;
    gchar *delimiter = NULL;
    gchar *temp = NULL;
    guint keyLength = 0;

    charCount = fread (line, sizeof(char), 1024, fp);
    line[charCount] = '\0';
    if (!charCount) {
      GST_WARNING_OBJECT (self, "fread returned zero bytes");
      continue;
    }
    GST_DEBUG_OBJECT (self, " Read charCount %d bytes Successfully",charCount);
    GST_DEBUG_OBJECT (self, "value of detected encoding is %s and self encoding is %s",
        self->detected_encoding,self->encoding);
    if (self->detected_encoding && strcmp (self->detected_encoding, "UTF-8") && conversion){
      result = convert_to_utf8 (line, charCount, self->detected_encoding, consumed, &err, self);
      GST_DEBUG_OBJECT (self, " Converted convert_to_utf8  result %d ",result);
    }
    if (result == NULL) {
      result = line;
      conversion =  FALSE;
    }
    con_temp =  result;
    temp = con_temp;

    while (con_temp){
      gchar* tempKey = NULL;
      guint i = 0;

      con_temp =  strcasestr(con_temp,"class=");
      if(con_temp)
        delimiter =  strcasestr(con_temp, ">");
      GST_DEBUG_OBJECT (self, " Delimiter ...Entering if %s",con_temp);
      if (con_temp && (delimiter!=NULL)){
        gchar* tempChar = con_temp + 6;
        GST_DEBUG_OBJECT (self, "Has class= reading string %s",tempChar);
        GST_DEBUG_OBJECT (self, "Has class= ");
        while (*tempChar != '>'){
          keyLength++;
          tempChar++;
          GST_DEBUG_OBJECT (self, " keyLength %d tempChar %c",keyLength,*tempChar);
        }
        GST_DEBUG_OBJECT (self, " keyLength  %d",keyLength);
        tempChar -= keyLength;
        tempKey = (gchar*) g_malloc (keyLength + 1);
        if(!tempKey){
          GST_DEBUG_OBJECT (self, "Error 1");
          goto error;
        }
        gchar* temp1 =tempKey;
        GST_DEBUG_OBJECT (self, "tempChar %s  keyLength  %d",tempChar,keyLength);
        while (*tempChar != '>'){
          *tempKey = *tempChar;
          tempKey++;
          tempChar++;
        }
        tempKey =temp1;
        tempKey[keyLength]='\0';
        GST_DEBUG_OBJECT (self, "tempKey %s  keyLength %d keyCount %d",tempKey,keyLength,keyCount);
        int k =0;
        for (k = 0; k < keyCount; k++){
          if(langkey[k]){
            if (!strcasecmp (tempKey,langkey[k]))
            {
              GST_DEBUG_OBJECT (self, "Has the key already so breaking..Entry %d tempKey %s langkey[i] %s ",k,tempKey,langkey[k]);
              lang_found = TRUE;
              break;
            }
          }
        }
        if(lang_found == FALSE){
          langkey[keyCount] = (gchar*) g_malloc (keyLength);
          if(! langkey[keyCount])
            goto error;
          strcpy(langkey[keyCount],tempKey);
          langKey_length[keyCount]=keyLength;
          keyCount++;
        }
        lang_found =FALSE;
        keyLength =0;
        if(tempKey){
          g_free(tempKey);
          tempKey=NULL;
        }
      } else {
        keyLength =0;
        lang_found =FALSE;
        break;
      }
      con_temp+=6;
      GST_DEBUG_OBJECT (self, " ..increment con_temp %s",con_temp);
    }
  }
  GST_DEBUG_OBJECT (self, " At end keyCount no of langs is %d ",keyCount);
  for(i=0;i<keyCount;i++) {
    if(langkey[i]) {
      GstLangStruct *new = g_new0 (GstLangStruct, 1);
      GST_DEBUG_OBJECT (self, "Adding ign case to the langKey keyCount %d and lang %s ",i, langkey[i]);
      new->language_code = (gchar*)malloc (3);
      if(!(new->language_code)){
        GST_DEBUG_OBJECT (self, " .Error 2");
        goto error;
      }
      gchar *attr_val=new->language_code ;
      strcpy (attr_val, "un");
      attr_val[2]='\0';

      new->language_key = (gchar*) malloc (langKey_length[i] + 1);
      if(!(new->language_key)){
        GST_DEBUG_OBJECT (self, " ..Error 3");
        goto error;
      }
      strcpy (new->language_key, langkey[i]);
      self->priv->lang_list = g_list_append (self->priv->lang_list, new);
      GST_DEBUG_OBJECT (self, " new...Successfull");
      g_free(langkey[i]);
    }
  }
  if (fp) {
    g_free(temp_path);
    g_free(file_path_type);
    fclose(fp);
  }
  return TRUE;
error:
  GST_DEBUG_OBJECT (self, " In Error");
  if (fp) {
    g_free(temp_path);
    g_free(file_path_type);
    fclose(fp);
  }
  return FALSE;
}

gboolean
validate_langlist_body(GList * lang_list, Gstsubmux * self){
  gchar * file_path_type = NULL;
  gchar * file_path = NULL;
  gchar   line[1025];
  FILE  * fp = NULL;
  guint i = 0, found_count = 0,k = 0;
  const guint list_len = g_list_length(lang_list);
  gboolean counter[MAX_LANGUAGE];
  GstPad  *sinkpad = NULL;
  struct LangStruct
  {
      gchar *language_code;
      gchar *language_key;
  } * lang;
  sinkpad = (GstPad *) g_list_nth_data(self->sinkpad, 0);
  GstQuery *cquery;
  GstStructure *structure;
  const GValue *value;
  structure = gst_structure_new ("FileSrcURI", "file-uri", G_TYPE_STRING, NULL, NULL);

  cquery = gst_query_new_application (GST_QUERY_CUSTOM, structure);

  if (!gst_pad_peer_query (sinkpad, cquery)) {
    GST_ERROR_OBJECT (self, "Failed to query SMI file path");
    gst_query_unref (cquery);
    return FALSE;
  }
  structure = gst_query_get_structure (cquery);
  value = gst_structure_get_value (structure, "file-uri");
  file_path = g_strdup (g_value_get_string (value));

  if (file_path == NULL){
    GST_ERROR_OBJECT (self, "Could not parse the SMI file path");
    gst_query_unref (cquery);
    return FALSE;
  }

  if (self->external_filepath == NULL) {
    self->external_filepath = file_path;
  }
  else {
    if (!g_strcmp0 (file_path, self->external_filepath)) {
      GST_INFO_OBJECT (self, "Same external file URI, no need to parse again");
      gst_query_unref (cquery);
      g_free(file_path);
      return TRUE;
    }
    else {
      g_free (self->external_filepath);
      self->external_filepath = NULL;
      self->external_filepath = file_path;
    }
  }

  gst_query_unref (cquery);
  GST_INFO_OBJECT (self, "File path comes as %s", file_path);

  file_path_type = g_strndup ((gchar *) file_path, 4);
  GST_INFO_OBJECT (self, "Received file path by query = %s, %s", file_path, file_path_type);
  if (!g_strcmp0(file_path_type, "file")){
    file_path += 7;
    GST_INFO_OBJECT (self, "File path comes as %s", file_path);

    fp = fopen (file_path, "r");
    if (!fp){
      GST_ERROR_OBJECT (self, "Failed to open file");
      g_free(file_path_type);
      return FALSE;
    }

    for (i = 0; i < list_len; i++){
      counter[i] = FALSE;
    }

    while (!feof (fp) && found_count < list_len){
      GError *err = NULL;
      gsize * consumed = NULL;
      gint gap = 0;
      guint charCount = 0;
      gchar* result = NULL;
      gchar* temp = NULL;
      gchar* temp_lang = NULL;
      gchar* con_temp_end = NULL;
      gchar* con_temp_start = NULL;
      gchar* new_key = NULL;
      gint new_key_length = 0;
      gboolean new_key_found = FALSE;
      gchar * temp1 = NULL;
      gchar *con_temp_lang = NULL;
      gchar *con_temp = NULL;
      gboolean conversion = TRUE;
      charCount = fread (line, sizeof(char), 1024, fp);
      line[charCount] = '\0';
      if (!charCount) {
        GST_WARNING_OBJECT (self, "fread returned zero bytes");
        continue;
      }

      GST_DEBUG_OBJECT (self, "value of detected encoding is %s and self encoding is %s",
                             self->detected_encoding,self->encoding);
      if (self->detected_encoding && strcmp (self->detected_encoding, "UTF-8") && conversion){
        result = convert_to_utf8 (line, charCount, self->detected_encoding, consumed, &err, self);
      }
      if (result == NULL) {
         result = line;
         conversion =  FALSE;
      }
      con_temp = g_utf8_strdown (result, strlen (result));
      temp = con_temp;
      while (con_temp) {
        con_temp = g_strstr_len(con_temp, strlen (con_temp), "class=");
        if (con_temp) {
          temp1 = g_strstr_len(con_temp+1, strlen (con_temp), "class=");
        }
        if (temp1 && con_temp){
          gap = strlen (con_temp) - strlen (temp1);
        } else if (con_temp) {
          gap = strlen (con_temp);
        } else {
          continue;
        }
        if (con_temp){
          for (i = 0; i < list_len; i++){
            if (counter[i] == TRUE) {
              con_temp = con_temp + 1;
              continue;
            }
            lang = (struct LangStruct *) g_list_nth_data (lang_list, i);
            if (lang) {
              temp_lang = (gchar*)g_malloc (strlen (lang->language_key) + 1);
              strcpy (temp_lang, lang->language_key);
              con_temp_lang = g_utf8_strdown (temp_lang, strlen (temp_lang));
              if (g_strstr_len (con_temp, gap, con_temp_lang)) {
                found_count++;
                counter[i] = TRUE;
                GST_INFO_OBJECT (self, "Valid Language in list : [%s]", lang->language_key);
                con_temp = con_temp + 1;
              }
/* Fix Me: Cases where there is no body for a specific language
 * inside a single language .smi file */
#if 0
              else {
                con_temp_start = con_temp;
                con_temp_end = con_temp;
                while(con_temp_end) {
                  if(*con_temp_end == '=') {
                    con_temp_start = con_temp_end+1;
                    con_temp_end++;
                  }else if(*con_temp_end == '>') {
                    con_temp_end = con_temp_end;
                    new_key_found = TRUE;
                    break;
                  }else {
                    con_temp_end++;
                    new_key_found = FALSE;
                  }
                }
                if(new_key_found) {
                  new_key_length = strlen(con_temp_start)-strlen(con_temp_end);
                  new_key = g_malloc(new_key_length +1);
                  for(k=0;k<new_key_length;k++){
                    *(new_key+k)=*(con_temp_start+k);
                  }
                  *(new_key+new_key_length)='\0';
                  GST_INFO("new lang key is %s",lang->language_key);
                  g_free(new_key);
                  found_count++;
                  counter[i] = TRUE;
                  con_temp = con_temp + 1;
                }
              }
#endif
              g_free (temp_lang);
              g_free (con_temp_lang);
            }
          }
        }
      }
      if (conversion)
        g_free (result);
      if (temp)
        g_free (temp);
    }

    if (found_count < list_len) {
      for (i = 0; i < list_len; i++) {
        if (counter[i] == FALSE)
          lang_list = g_list_delete_link (lang_list, g_list_nth (lang_list, i));
      }
    }
  } else {
    GST_ERROR_OBJECT (self, "File is not a local file");
    g_free(file_path_type);
    return FALSE;
  }
  fclose (fp);
  g_free(file_path_type);
  return TRUE;
}

/*
**
**  Description    : Chain function used to push the subtitle buffer to internal pipelines of submux element
**  Params        : (1) sink pad on which buffer is arriving (2) the buffer itself
**  return        : GST_FLOW_OK on successfully pushing subtitle buffer to next element
**
*/
static GstFlowReturn
gst_submux_chain(GstPad *pad, GstBuffer *buffer)
{
  guint length = 0;
  guint i=0;
  GstPad *checkpad = NULL;
  Gstsubmux *submux = GST_SUBMUX(GST_PAD_PARENT(pad));
  gboolean ret = FALSE;
  GstFlowReturn fret = GST_FLOW_ERROR;
  GstSubmuxStream *stream = NULL;
  GstMessage *m = NULL;

  if (GST_BUFFER_IS_DISCONT (buffer))
    GST_DEBUG_OBJECT(submux, "Discont buffer came in chain function");
  GST_DEBUG_OBJECT (submux, "^^^^^entering in chain^^^^^^");
  if (!submux->priv->is_internal) {
    if (!submux->priv->first_buffer) {
      submux->detected_encoding = detect_encoding ((gchar*)GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));
    }
    if (!submux->langlist_msg_posted && submux->priv->lang_list) {
      if (!validate_langlist_body (submux->priv->lang_list, submux)){
        GST_WARNING_OBJECT(submux, "Error occured while validating language list. Posting without validation");
      }
      if (submux->priv->lang_list) {
        GList* temp_list_to_post = NULL;
        temp_list_to_post = g_list_copy (submux->priv->lang_list);
        m = gst_message_new_element (GST_OBJECT_CAST (submux), gst_structure_new("Ext_Sub_Language_List",
                                    "lang_list", G_TYPE_POINTER, temp_list_to_post, NULL));

        gst_element_post_message (GST_ELEMENT_CAST (submux), m);
        submux->langlist_msg_posted = TRUE;
      }
      GST_DEBUG_OBJECT (submux, "LANGLIST POSTED");
    }
    if (submux->need_segment) {
      ret = gst_pad_push_event (submux->srcpad, gst_event_new_new_segment (FALSE, submux->segment.rate,
                                submux->segment.format, submux->segment.start, submux->segment.stop,
                                submux->segment.time));
      GST_DEBUG_OBJECT (submux, "pushing newsegment event with %" GST_SEGMENT_FORMAT, &submux->segment);
      if (!ret) {
        GST_ERROR_OBJECT (submux, "Sending newsegment to next element is failed");
        return GST_FLOW_ERROR;
      }
      GST_DEBUG_OBJECT (submux, "Starting the loop again");
      if (!gst_pad_start_task (submux->srcpad, (GstTaskFunction) gst_submux_loop, submux)) {
         GST_ERROR_OBJECT (submux, "failed to start srcpad task...");
         GST_ELEMENT_ERROR (submux, RESOURCE, FAILED, ("failed to create  push loop"), (NULL));
         return GST_FLOW_ERROR;
      }
      submux->need_segment = FALSE;
    }
    GST_DEBUG_OBJECT (submux, "before pushing buffer to each apprsrc");
    if (!submux->priv->lang_list && submux->priv->parser_type == GST_SUB_PARSE_FORMAT_SAMI) {
      GST_ERROR_OBJECT (submux, "lang list is not there");
      return GST_FLOW_ERROR;
    }
    if (submux->priv->parser_type == GST_SUB_PARSE_FORMAT_SAMI)
      length = g_list_length (submux->priv->lang_list);
    else
      length = 1;

    for (i = 0; i < length; i++) {
      stream = g_list_nth_data(submux->streams, i);
      if ((submux->priv->parser_type == GST_SUB_PARSE_FORMAT_SAMI) && !submux->priv->first_buffer){
        GstLangStruct *lang = g_list_nth_data(submux->priv->lang_list, i);
        if(submux->msl_streams){
           GstLangStruct *lang1 = g_list_nth_data(submux->msl_streams, i);
           lang->active = lang1->active;
        } else {
          if (i == 0)
            lang->active = TRUE;
          else
            lang->active = FALSE;
        }
      }
      GST_DEBUG_OBJECT (submux, "making stream need segment false");
      stream->need_segment = FALSE;
    }

    for (i = 0; i < length; i++) {
      stream = g_list_nth_data(submux->streams, i);
      if (!stream){
        GST_ERROR_OBJECT (submux, "stream not found");
        return GST_FLOW_ERROR;
      }

      if (!stream->pipe_struc.appsrc) {
        GST_ERROR_OBJECT (submux, "appsrc not found");
        return GST_FLOW_ERROR;
      }
      if (i < (length - 1))
        gst_buffer_ref (buffer);

      fret = gst_app_src_push_buffer ((GstAppSrc*)stream->pipe_struc.appsrc, buffer);

      if (fret != GST_FLOW_OK) {
        GST_ERROR_OBJECT (submux, "push buffer failed with fret is %d", fret);
        return fret;
      }
      GST_DEBUG_OBJECT (submux, "pad_push successfull to appsrc %p buffer", buffer);
    }
  } else {
    length = submux->sinkpads_count;
    checkpad = (GstPad *) g_list_nth_data (submux->sinkpad, 0);
    if (checkpad == pad) {
      if (submux->need_segment) {
        ret = gst_pad_push_event (submux->srcpad, gst_event_new_new_segment (FALSE, submux->segment.rate,
                                  submux->segment.format, submux->segment.start, submux->segment.stop,
                                  submux->segment.time));
        GST_DEBUG_OBJECT (submux, "pushing newsegment event with %" GST_SEGMENT_FORMAT, &submux->segment);
        if (!ret) {
          GST_ERROR_OBJECT (submux, "Sending newsegment to next element is failed");
          return GST_FLOW_ERROR;
        }
        GST_DEBUG_OBJECT (submux, "Starting the loop again");
        if (!gst_pad_start_task (submux->srcpad, (GstTaskFunction) gst_submux_loop, submux)) {
          GST_ERROR_OBJECT (submux, "failed to start srcpad task...");
          GST_ELEMENT_ERROR (submux, RESOURCE, FAILED, ("failed to create  push loop"), (NULL));
          return GST_FLOW_ERROR;
        }
        submux->need_segment = FALSE;
      }
    }
    for (i = 0; i < length; i++) {
      checkpad = (GstPad *) g_list_nth_data(submux->sinkpad, i);
      if (checkpad == pad) {
        stream = g_list_nth_data (submux->streams, i);
        if (!stream) {
          GST_ERROR_OBJECT (submux, "Stream not available...");
          return GST_FLOW_ERROR;
        }
        if (stream->flushing){
          GST_DEBUG_OBJECT (submux, "flushing going on in appsink");
          return GST_FLOW_OK ;
        }

        g_mutex_lock (stream->queue_lock);
        g_queue_push_tail (stream->queue, buffer);
        g_cond_signal (stream->queue_empty);
        g_mutex_unlock (stream->queue_lock);
        fret = GST_FLOW_OK;
        GST_DEBUG_OBJECT (submux, "push buffer success to appsrc with fret is %d for stream[%d]", fret, i);
        break;
      }
    }
  }

  if (!submux->priv->first_buffer) {
    GST_DEBUG_OBJECT (submux, "got the first buffer");
    submux->priv->first_buffer = TRUE;
    if (submux->priv->parser_type == GST_SUB_PARSE_FORMAT_SAMI) {

    }
  }
  GST_DEBUG_OBJECT (submux, "^^^^^exiting in chain^^^^^^");
  return fret;
}

/* stream_denit */
static void
gst_submux_stream_deinit (GstSubmuxStream *stream,Gstsubmux *submux)
{
  GstBuffer *buf = NULL;

  if (stream) {
    if (stream->queue) {
      while (!g_queue_is_empty (stream->queue)) {
        buf = g_queue_pop_head (stream->queue);
        gst_buffer_unref (buf);
        buf = NULL;
      }
      g_queue_free (stream->queue);
      stream->queue = NULL;
    }

    if (stream->pipe_struc.pipe) {
      gst_element_set_state (stream->pipe_struc.pipe, GST_STATE_NULL);
      gst_element_get_state (stream->pipe_struc.pipe, NULL, NULL, GST_CLOCK_TIME_NONE);
      gst_object_unref(GST_OBJECT(stream->pipe_struc.appsrc));
      gst_object_unref(GST_OBJECT(stream->pipe_struc.appsink));
      gst_object_unref (stream->pipe_struc.pipe);
    }

    if (stream->queue_lock) {
      g_cond_broadcast(stream->queue_empty);
      g_mutex_free (stream->queue_lock);
      stream->queue_lock = NULL;
    }

    if (stream->queue_empty) {
      g_cond_free (stream->queue_empty);
      stream->queue_empty= NULL;
    }

    g_free (stream);
    GST_DEBUG_OBJECT (submux, "stream deinit completed");
  }
}

/* releasing the requested pad */
static void
gst_submux_release_pad (GstElement * element, GstPad * pad)
{
  Gstsubmux *submux = GST_SUBMUX_CAST (element);
  GstPad *check_pad;
  int i=0;
  guint length;
  GST_INFO_OBJECT(element,"entering in the release pad");
  length = g_list_length(submux->sinkpad);
  GST_DEBUG_OBJECT (element, "Releasing %s:%s", GST_DEBUG_PAD_NAME (pad));

  for (i=1;i<=length;i++) {
	  check_pad = (struct GstPad *) g_list_nth_data(submux->sinkpad,i);
    if (check_pad == pad) {
      /* this is it, remove */
      submux->sinkpad = g_list_remove_link (submux->sinkpad, pad);
      gst_element_remove_pad (element, pad);
      break;
    }
  }
}

/* request new pad */
static GstPad *
gst_submux_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (element);
  Gstsubmux *submux = GST_SUBMUX_CAST (element);
  GstPad *newpad = NULL;
  gchar *name = NULL;

  if (templ->direction != GST_PAD_SINK) {
    GST_ERROR_OBJECT (submux, "templ direction is not sinkpad, returning from here");
    goto wrong_direction;
  }

  if (templ == gst_element_class_get_pad_template (klass, "sink%d")) {
    name = g_strdup_printf ("sink%d", submux->sinkpads_count++);
  }

  GST_DEBUG_OBJECT (submux, "Requested pad: %s", name);
  newpad = (GstPad*)g_new0 (GstPad*, 1);
  /* create pad and add to collections */
  newpad = gst_pad_new_from_template (templ, name);
  g_free (name);
  if(!submux->priv->is_internal) {
    submux->external_sinkpad = TRUE;
  }
  submux->sinkpad = g_list_append (submux->sinkpad, newpad);
  /* set up pad */

  /* set up pad functions */
  gst_pad_set_setcaps_function (newpad, GST_DEBUG_FUNCPTR (gst_submux_setcaps));
  gst_pad_set_event_function (newpad, GST_DEBUG_FUNCPTR (gst_submux_handle_sink_event));
  gst_pad_set_chain_function(newpad, GST_DEBUG_FUNCPTR (gst_submux_chain));
  gst_pad_set_active (newpad, TRUE);
  gst_element_add_pad (element, newpad);

  return newpad;

/* ERRORS */
wrong_direction:
  GST_WARNING_OBJECT (submux, "Request pad that is not a SINK pad.");
  return NULL;
}

static gboolean
gst_submux_handle_src_event (GstPad * pad, GstEvent * event)
{
  Gstsubmux *submux = GST_SUBMUX(GST_PAD_PARENT(pad));
  gboolean ret = FALSE;
  guint length = 0;
  gint i = 0;
  gboolean update;
  GstSubmuxStream *cur_stream = NULL;

  GST_DEBUG_OBJECT (submux, "Handling %s event", GST_EVENT_TYPE_NAME (event));
  length = g_list_length(submux->streams);

  switch (GST_EVENT_TYPE (event)) {
    /* this event indicates speed change or seek */
    case GST_EVENT_SEEK: {
      GstFormat format;
      GstSeekType start_type, stop_type;
      gint64 start, stop;
      gdouble rate;
      GstPad *sinkpad = (GstPad *) g_list_nth_data (submux->sinkpad, 0);
      gst_event_parse_seek (event, &rate, &format, &submux->segment_flags,
                             &start_type, &start, &stop_type, &stop);
      gst_segment_set_seek (&submux->segment, rate, format, submux->segment_flags,
                             start_type, start, stop_type, stop, &update);
      if (submux->priv->is_internal || submux->priv->parser_type != GST_SUB_PARSE_FORMAT_SAMI) {
        length = g_list_length (submux->sinkpad);
      } else {
        length = g_list_length(submux->streams);
      }
      if (!submux->priv->is_internal) {
        ret = gst_pad_push_event (sinkpad, gst_event_new_seek (rate, GST_FORMAT_BYTES, submux->segment_flags,
                                  GST_SEEK_TYPE_SET, 0, GST_SEEK_TYPE_NONE, 0));
        gst_event_unref (event);
      } else {
        GST_DEBUG_OBJECT (submux, "handling seek in case of internal");
        ret = gst_pad_event_default (pad, event);
      }

      if (!ret) {
        GST_ERROR_OBJECT (submux, "sending seek event to sink pad failed");
        break;
      }
      GST_DEBUG_OBJECT (submux, "sending seek event to sink pad passed");

      break;
    }

    default: {
      ret = gst_pad_event_default (pad, event);
      break;
    }
  }

  return ret;
}

static gboolean
gst_submux_handle_sink_event (GstPad * pad, GstEvent * event)
{
  Gstsubmux *submux = GST_SUBMUX (GST_PAD_PARENT (pad));
  gboolean ret = FALSE;
  guint length = 0;
  GstBuffer *buf = NULL;
  GstPad *checkpad = NULL;
  gint i = 0;
  GstSubmuxStream *cur_stream = NULL;

  GST_DEBUG_OBJECT (submux, "Handling %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:{
      length = g_list_length (submux->sinkpad);
      GST_OBJECT_LOCK (submux);
      for (i = 0; i < length; i++) {
        GST_DEBUG_OBJECT(submux, "inside the handling of EOS event");
        cur_stream = g_list_nth_data(submux->streams,i);
        if (!cur_stream->eos_sent) {
          GstBuffer *buf = gst_buffer_new_and_alloc (3 + 1);
          GST_DEBUG_OBJECT(submux, "sending EOS buffer to chain\n");
          GST_DEBUG_OBJECT (submux, "EOS. Pushing remaining text (if any)");
          GST_BUFFER_DATA (buf)[0] = 'e';
          GST_BUFFER_DATA (buf)[1] = 'o';
          GST_BUFFER_DATA (buf)[2] = 's';
          GST_BUFFER_DATA (buf)[3] = '\0';
          /* play it safe */
          GST_BUFFER_SIZE (buf) = 3;
          GST_BUFFER_FLAG_SET(buf,GST_BUFFER_FLAG_GAP);
          gst_submux_chain (g_list_nth_data(submux->sinkpad,i), buf);//
          cur_stream->eos_sent = TRUE;
        }
      }
      GST_OBJECT_UNLOCK (submux);
      gst_event_unref(event);
      ret = TRUE;
      break;
    }
    case GST_EVENT_NEWSEGMENT: {
      GstFormat format;
      gdouble rate,arate;
      gint64 start, stop, time;
      gboolean update;
      GST_OBJECT_LOCK (submux);
      if (!submux->pipeline_made) {
        if (!gst_submux_format_autodetect (submux)) {
          GST_ERROR_OBJECT (submux, "auto detect function failed");
          return FALSE;
        }
        if (!gst_calculate_number_languages(submux)) {
          GST_ERROR_OBJECT (submux, "failed to calculate number of languages");
          return FALSE;
        }
        if (!gst_submux_create_pipelines (submux, pad)) {
          GST_ERROR_OBJECT (submux, "failed to create pipelines");
          return FALSE;
        }
     }

      if (!submux->priv->is_internal) {
        gst_event_unref(event);
        length = g_list_length(submux->streams);
        for (i = 0; i < length; i++) {
          GST_DEBUG_OBJECT (submux, "inside the handling of new_segment event");
          cur_stream = g_list_nth_data(submux->streams,i);
          GST_DEBUG_OBJECT (submux, "pushing newsegment event with %" GST_SEGMENT_FORMAT, &submux->segment);
          if (!cur_stream->pipe_struc.pipe) {
            GST_ERROR_OBJECT (submux, "pipeline is null");
            return FALSE;
          }
          cur_stream = g_list_nth_data(submux->streams,i);
          cur_stream->seek_ts = submux->segment.start;
          ret = gst_element_send_event (cur_stream->pipe_struc.pipe, gst_event_new_new_segment (FALSE,
                                        submux->segment.rate, submux->segment.format,
                                        submux->segment.start, submux->segment.stop, submux->segment.time));
          if (!ret){
            GST_ERROR_OBJECT(submux, "sending newsegment event to stream[%d] failed", i);
            break;
          }
        }
        submux->need_segment = TRUE;
      } else {
        length = g_list_length (submux->sinkpad);
        if (length ==  g_list_length (submux->streams) && submux->need_segment) {
          for (i = 0; i < length; i++) {
            GST_DEBUG_OBJECT (submux, "inside the handling of new_segment event");
            cur_stream = g_list_nth_data(submux->streams, i);
            if (cur_stream->need_segment) {
              gst_event_parse_new_segment_full (event, &update, &rate, &arate, &format, &start, &stop, &time);
              gst_segment_set_newsegment_full (&submux->segment, update, rate, arate, format, start, stop, time);
              GST_DEBUG_OBJECT (submux, "pushing newsegment event with %" GST_SEGMENT_FORMAT, &submux->segment);
              ret = TRUE;
              cur_stream->need_segment = FALSE;
            }
          }
          submux->need_segment = TRUE;
          gst_event_unref(event);
        }
      }
      GST_OBJECT_UNLOCK (submux);
      break;
    }
    case GST_EVENT_FLUSH_START: {
      length = g_list_length(submux->streams);
      if (!submux->priv->is_internal) {
        gst_event_unref(event);
        ret = gst_pad_event_default (pad, gst_event_new_flush_start ());
        for (i = 0;i < length;i++) {
          cur_stream = g_list_nth_data(submux->streams,i);
          cur_stream->flushing = TRUE;
          cur_stream->discont_came = FALSE;
          GST_DEBUG_OBJECT (submux, "making discont false");
          GST_DEBUG_OBJECT (submux, "making flushing TRUE");
          cur_stream->eos_came = FALSE;
          cur_stream->eos_sent = FALSE;
          GST_DEBUG_OBJECT (submux, "making eos_came and eos_sent FALSE");
        }
        for (i = 0; i < length; i++) {
          cur_stream = g_list_nth_data(submux->streams,i);
          ret= gst_element_send_event(cur_stream->pipe_struc.appsrc,gst_event_new_flush_start ());
          ret= gst_element_send_event(cur_stream->pipe_struc.appsrc,gst_event_new_flush_stop ());
          ret = gst_element_send_event(cur_stream->pipe_struc.appsrc,gst_event_new_eos ());
          GST_INFO_OBJECT(cur_stream,"flush stop and start  and eos is done with ret %d",ret);
          submux->flushing =TRUE;
          g_mutex_lock (cur_stream->queue_lock);
          g_cond_signal (cur_stream->queue_empty);
          g_mutex_unlock(cur_stream->queue_lock);
          cur_stream->flush_done = TRUE;
          GST_DEBUG_OBJECT (cur_stream, "signaling queue empty signal from flush start");
          ret = TRUE;
          GST_DEBUG_OBJECT (submux, "sending flush start event to stream[%d] success", i);
        }

        if (!ret){
          GST_ERROR_OBJECT (submux, "sending flush start event to srcpad pad failed");
          break;
        }

        if (submux && GST_PAD_TASK(submux->srcpad)) {
          GST_INFO_OBJECT (submux, "trying acquire srcpad lock");
          GST_PAD_STREAM_LOCK (submux->srcpad);
          GST_INFO_OBJECT (submux, "acquired stream lock");
          GST_PAD_STREAM_UNLOCK (submux->srcpad);
        }
        /*changes for new design*/
        for (i = 0;i < length;i++) {
          cur_stream = g_list_nth_data(submux->streams,i);
          gst_submux_stream_deinit(cur_stream,submux);
        }
        g_list_free(submux->streams);
        submux->streams = NULL;
        gst_submux_deinit_private_values (submux);

        submux->stop_loop = FALSE;
        submux->need_segment = TRUE;
        submux->langlist_msg_posted = FALSE;
        GST_DEBUG_OBJECT (submux, "flush start successfully send to next element");
      } else {
        GST_DEBUG_OBJECT(submux, "flusht start in case of internal subtitle");
        gst_event_unref (event);
        submux->flushing = TRUE;
        checkpad = (GstPad *) g_list_nth_data (submux->sinkpad, length - 1);
        if (checkpad == pad) {
          ret = gst_pad_event_default (pad, gst_event_new_flush_start ());
          for (i = 0; i < length; i++) {
            cur_stream = g_list_nth_data(submux->streams, i);
            cur_stream->flushing = TRUE;
            GST_DEBUG_OBJECT (submux, "in case of internal making discont unchanged");
            GST_DEBUG_OBJECT (submux, "making flushing TRUE");
          }
          for (i = 0; i < length; i++) {
            cur_stream = g_list_nth_data(submux->streams, i);
            submux->flushing = TRUE;
            g_mutex_lock (cur_stream->queue_lock);
            while (!g_queue_is_empty (cur_stream->queue)) {
              buf = g_queue_pop_head (cur_stream->queue);
              gst_buffer_unref (buf);
            }
            GST_DEBUG_OBJECT (submux, "cleared stream cur_stream->queue");
            g_queue_clear (cur_stream->queue);
            g_cond_signal (cur_stream->queue_empty);
            g_mutex_unlock(cur_stream->queue_lock);
            GST_DEBUG_OBJECT (cur_stream, "signaling queue empty signal from flush start");
            cur_stream->eos_came = FALSE;
            cur_stream->eos_sent = FALSE;
            GST_DEBUG_OBJECT (submux, "making eos_came and eos_sent FALSE");
            ret = TRUE;
            GST_DEBUG_OBJECT (submux, "sending flush start event to stream[%d] success", i);
          }
          if (!ret){
            GST_ERROR_OBJECT (submux, "sending flush start event to srcpad pad failed");
            break;
          }

          if (submux && GST_PAD_TASK (submux->srcpad)) {
            GST_INFO_OBJECT (submux, "trying acquire srcpad lock");
            GST_PAD_STREAM_LOCK (submux->srcpad);
            GST_INFO_OBJECT (submux, "acquired srcpad lock");
            GST_PAD_STREAM_UNLOCK (submux->srcpad);
          }
          GST_DEBUG_OBJECT(submux, "flush start successfully send to next element");
         }
      }
      break;
    }
    case GST_EVENT_FLUSH_STOP: {
      gst_event_unref(event);
      if (!submux->priv->is_internal) {
        guint idx = 0;
        submux->flushing = FALSE;
        ret = gst_pad_event_default (pad, gst_event_new_flush_stop ());
        if (!ret){
          GST_ERROR_OBJECT (submux, "sending flush-stop event to srcpad pad failed");
          break;
        }
        for (idx = 0; idx < submux->priv->stream_count; idx++) {
          submux->cur_buf_array[idx] = NULL;
        }
        GST_DEBUG_OBJECT (submux, "flush stop successfully send to next element");
      } else {
        length = g_list_length(submux->streams);
        GST_DEBUG_OBJECT (submux, "flusht stop in case of internal subtitle");
        checkpad = (GstPad *) g_list_nth_data (submux->sinkpad, length - 1);
        if (checkpad == pad) {
          for (i = 0; i < length; i++) {
            cur_stream = g_list_nth_data(submux->streams, i);
            cur_stream->need_segment = TRUE;
            submux->cur_buf_array[i] = NULL;
            submux->need_segment = TRUE;
            GST_DEBUG_OBJECT (submux, "making need_segment true");
            submux->flushing = FALSE;
            cur_stream->flushing = FALSE;
            GST_DEBUG_OBJECT (submux, "making flushing FALSE");
            ret = TRUE;
            GST_DEBUG_OBJECT (submux, "sending %s event to stream[%d] success", GST_EVENT_TYPE_NAME (event), i);
          }
          ret = gst_pad_event_default (pad, gst_event_new_flush_stop ());
          if (!ret){
            GST_ERROR_OBJECT (submux, "sending flush-stop event to srcpad pad failed");
            break;
          }
          GST_DEBUG_OBJECT (submux, "flush stop successfully send to next element");
        }
      }
      break;
    }
    default:{
      if (!submux->priv->is_internal) {
        ret = gst_pad_event_default (pad, event);
      } else {
        checkpad = (GstPad *) g_list_nth_data (submux->sinkpad, length - 1);
        if (checkpad == pad) {
          ret = gst_pad_event_default (pad, event);
        } else {
          ret = TRUE;
        }
      }
      if (!ret){
        GST_ERROR_OBJECT (submux, "sending %s event to srcpad pad failed", GST_EVENT_TYPE_NAME (event));
        break;
      }
      break;
    }
  }
  return ret;
}

static gint gst_submux_buffer_list_sorting (gconstpointer a, gconstpointer b)
{
  GstBuffer *buf_a = (GstBuffer *) a;
  GstBuffer *buf_b = (GstBuffer *) b;
  if (GST_BUFFER_TIMESTAMP(buf_a)>GST_BUFFER_TIMESTAMP(buf_b))
    return -1;
  else if(GST_BUFFER_TIMESTAMP(buf_a)<GST_BUFFER_TIMESTAMP(buf_b))
    return 1;
  else
    return 0;
}

static gboolean
gst_submux_is_muxing_needed (GstBuffer *ref_buffer, GstBuffer *cur_buf)
{
  GstClockTime ref_start = GST_BUFFER_TIMESTAMP(ref_buffer);
  GstClockTime ref_stop = GST_BUFFER_TIMESTAMP(ref_buffer) + GST_BUFFER_DURATION(ref_buffer);
  GstClockTime start = GST_BUFFER_TIMESTAMP(cur_buf);
  GstClockTime stop = GST_BUFFER_TIMESTAMP(cur_buf) + GST_BUFFER_DURATION(cur_buf);

  /* if we have a stop position and a valid start and start is bigger,
   * we're outside of the segment */
  if (G_UNLIKELY (ref_stop != -1 && start != -1 && start >= ref_stop))
    return FALSE;

  /* if a stop position is given and is before the segment start,
   * we're outside of the segment. Special case is were start
   * and stop are equal to the segment start. In that case we
   * are inside the segment. */
  if (G_UNLIKELY (stop != -1 && (stop < ref_start || (start != stop && stop == ref_start))))
    return FALSE;

  return TRUE;
}

/* This function do the actual muxing of buffer on the basis of timestamps */
static GList*
gst_submux_muxing (Gstsubmux *submux)
{
  GstClockTime min_timestamp = 0;
  int min_stream = 0;
  int overlap = 0;
  GstClockTime next_min_time = 0;
  int idx = 0;
  GList *push_list = NULL;

  /* Finding least timestamp of all streams and their stream ID */
  for (idx = 0; idx < submux->priv->stream_count; idx++) {
    if(submux->cur_buf_array[idx] && !min_timestamp) {
      min_timestamp = GST_BUFFER_TIMESTAMP(submux->cur_buf_array[idx]);
      min_stream = idx;
    }
    if(submux->cur_buf_array[idx] && (GST_BUFFER_TIMESTAMP(submux->cur_buf_array[idx]) < min_timestamp)) {
      min_timestamp = GST_BUFFER_TIMESTAMP(submux->cur_buf_array[idx]);
      min_stream = idx;
    }
  }

  GST_DEBUG_OBJECT (submux, "Identified least timestamp: %"GST_TIME_FORMAT" for stream: %d",
    GST_TIME_ARGS(min_timestamp), min_stream);

  /* Finding overlap buffers and next least timestamp */
  for (idx = 0; idx < submux->priv->stream_count; idx++) {
    if(submux->cur_buf_array[idx] && (idx != min_stream) && (!next_min_time)) {
      next_min_time = GST_BUFFER_TIMESTAMP(submux->cur_buf_array[idx]);
    }
    if(submux->cur_buf_array[idx] && (idx != min_stream)) {
      if(gst_submux_is_muxing_needed (submux->cur_buf_array[min_stream], submux->cur_buf_array[idx])) {
        overlap = overlap | (1<<idx);      // bit setting of overlap variable with stream ID
        GST_DEBUG_OBJECT (submux, "overlapped with stream = %d", idx);
        if(GST_BUFFER_TIMESTAMP(submux->cur_buf_array[idx]) < next_min_time)
          next_min_time = GST_BUFFER_TIMESTAMP(submux->cur_buf_array[idx]);
      }
    }
  }

  GST_DEBUG_OBJECT (submux, "Identified overlap: %d next least timestamp: %"GST_TIME_FORMAT" ", overlap, GST_TIME_ARGS(next_min_time));

  /* If no overlap send buffer as it is */
  if(!overlap) {
     GST_DEBUG_OBJECT (submux, "pushing string: %s....", (gchar*)GST_BUFFER_DATA(submux->cur_buf_array[min_stream]));
     push_list = g_list_append(push_list, submux->cur_buf_array[min_stream]);
     GST_DEBUG_OBJECT (submux, "No overlap found pushing buffer of ts = %"GST_TIME_FORMAT", dur = %"GST_TIME_FORMAT,
              GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(submux->cur_buf_array[min_stream])), GST_TIME_ARGS(GST_BUFFER_DURATION(submux->cur_buf_array[min_stream])));
     submux->cur_buf_array[min_stream] = NULL;
  } else {
    GstBuffer *push_buf = NULL;
    GstClockTime stop_time = 0;
    int stop_idx = 0;
    GstBuffer *overlap_buf = NULL;
    guint overlap_buf_length = 0;
    GString *overlap_text = NULL;
    gchar *text = NULL;

    if(next_min_time > GST_BUFFER_TIMESTAMP(submux->cur_buf_array[min_stream])) {
    GST_DEBUG_OBJECT (submux, "Before duration change ts = %"GST_TIME_FORMAT", dur = %"GST_TIME_FORMAT,
             GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(submux->cur_buf_array[min_stream])), GST_TIME_ARGS(GST_BUFFER_DURATION(submux->cur_buf_array[min_stream])));
      push_buf = gst_buffer_copy (submux->cur_buf_array[min_stream]);
      push_buf->duration = next_min_time - GST_BUFFER_TIMESTAMP(submux->cur_buf_array[min_stream]);
      GST_BUFFER_TIMESTAMP(submux->cur_buf_array[min_stream]) = next_min_time;
      GST_BUFFER_DURATION(submux->cur_buf_array[min_stream]) -= push_buf->duration;
      GST_DEBUG_OBJECT (submux, "After duration change ts = %"GST_TIME_FORMAT", dur = %"GST_TIME_FORMAT,
             GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(submux->cur_buf_array[min_stream])), GST_TIME_ARGS(GST_BUFFER_DURATION(submux->cur_buf_array[min_stream])));

      min_timestamp = next_min_time;
      GST_INFO_OBJECT (submux, "pushing string: %s...", (gchar*)GST_BUFFER_DATA(push_buf));
      push_list = g_list_append(push_list, push_buf);
      GST_DEBUG_OBJECT (submux, "Overlap found pushing initial partial buffer of ts = %"GST_TIME_FORMAT", dur = %"GST_TIME_FORMAT,
              GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(push_buf)), GST_TIME_ARGS(GST_BUFFER_DURATION(push_buf)));
    }

    for (idx = 0; idx < submux->priv->stream_count; idx++) {
      if(submux->cur_buf_array[idx] && !stop_time) {
        stop_time = GST_BUFFER_TIMESTAMP(submux->cur_buf_array[idx]) + GST_BUFFER_DURATION(submux->cur_buf_array[idx]);
        stop_idx = idx;
      }
      if(submux->cur_buf_array[idx] &&
          ((GST_BUFFER_TIMESTAMP(submux->cur_buf_array[idx])+ GST_BUFFER_DURATION(submux->cur_buf_array[idx])) < stop_time)) {
        stop_time = GST_BUFFER_TIMESTAMP(submux->cur_buf_array[idx]) + GST_BUFFER_DURATION(submux->cur_buf_array[idx]);
        stop_idx = idx;
      }
    }
    GST_DEBUG_OBJECT (submux, "Identified least stop timestamp: %"GST_TIME_FORMAT" for stream: %d",
            GST_TIME_ARGS(stop_time), stop_idx);

    overlap_text = g_string_new ("");
    overlap = overlap | (1<<min_stream);
    for (idx = 0; idx < submux->priv->stream_count; idx++) {
      int finder = 1<<idx;
      if(overlap & finder) {
        GST_DEBUG_OBJECT (submux, "append string: %s....", (gchar*)GST_BUFFER_DATA(submux->cur_buf_array[idx]));
        g_string_append (overlap_text, (gchar*)GST_BUFFER_DATA (submux->cur_buf_array[idx]));
        GST_BUFFER_TIMESTAMP(submux->cur_buf_array[idx])+= (stop_time - min_timestamp);
        GST_BUFFER_DURATION(submux->cur_buf_array[idx])-= (stop_time - min_timestamp);
        if(overlap > (1<<(idx+1))) g_string_append_c (overlap_text, '\n');
      }
    }
    text = g_string_free (overlap_text, FALSE);
    overlap_buf_length = strlen(text);
    overlap_buf = gst_buffer_new_and_alloc (overlap_buf_length + 1);
    memcpy (GST_BUFFER_DATA (overlap_buf), text, overlap_buf_length + 1);
    overlap_buf->timestamp = min_timestamp;
    overlap_buf->duration = stop_time - min_timestamp;
    g_free (text);
    text = NULL;
    submux->cur_buf_array[stop_idx] = NULL;
    GST_DEBUG_OBJECT (submux, "pushing string: %s....", (gchar*)GST_BUFFER_DATA(overlap_buf));
    push_list = g_list_append(push_list, overlap_buf);
    GST_DEBUG_OBJECT (submux, "Overlap found pushing merged buffer of ts = %"GST_TIME_FORMAT", dur = %"GST_TIME_FORMAT,
          GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(overlap_buf)), GST_TIME_ARGS(GST_BUFFER_DURATION(overlap_buf)));
    GST_DEBUG_OBJECT (submux, "request for new buffer for stream %d", stop_idx);
    for (idx = 0; idx < submux->priv->stream_count; idx++) {
      if(submux->cur_buf_array[idx] &&
           ((GST_BUFFER_TIMESTAMP(submux->cur_buf_array[idx])+ GST_BUFFER_DURATION(submux->cur_buf_array[idx])) <= stop_time)) {
        submux->cur_buf_array[idx] = NULL;
        GST_DEBUG_OBJECT (submux, "request for new buffer for stream %d", idx);
      }
    }
  }
  return push_list;
}

static void gst_submux_loop (Gstsubmux *submux)
{
  guint length = 0;
  GstBuffer *src_buffer = NULL;
  GstBuffer *temp_buffer = NULL;
  GstBuffer *check_buffer = NULL;
  GstSubmuxStream *cur_stream = NULL;
  GstSubmuxStream *check_stream = NULL;
  gboolean match = FALSE;
  GstFlowReturn fret = GST_FLOW_OK;
  GstClockTime cur_duration = 0 ;
  GstClockTime cur_ts = 0;
  gboolean eos = TRUE;
  guint i= 0,k = 0;
  GList *push_list = NULL;
  GstBuffer *push_buf = NULL;

  if (!submux->priv->first_buffer) {
    GST_INFO_OBJECT (submux, "exiting from lopp");
    return;
  }

  if (submux->priv->parser_type == GST_SUB_PARSE_FORMAT_SAMI) {
    length = g_list_length (submux->priv->lang_list);
  } else {
    length = submux->sinkpads_count;
  }

  gboolean made = FALSE;
 // length = 2;

  for (i = 0; i < length; i++) {

re_pop:
    cur_stream = g_list_nth_data (submux->streams, i);
    GST_DEBUG_OBJECT (submux, "Before lock acquired in loop stream[%d]", i);
    g_mutex_lock (cur_stream->queue_lock);
    GST_DEBUG_OBJECT (submux, "Lock acquired in loop stream[%d]", i);

    if (g_queue_is_empty (cur_stream->queue) && !submux->flushing) {
      GST_DEBUG_OBJECT (submux, "Queue is empty, waiting for the condition signal stream[%d]", i);
      g_cond_wait (cur_stream->queue_empty, cur_stream->queue_lock);
    }
    GST_DEBUG_OBJECT (submux, "Got the queue condition signal stream[%d]", i);

    if (submux->flushing || submux->stop_loop) {
      GST_DEBUG_OBJECT (submux, "Flushing going on in loop");
      GST_DEBUG_OBJECT (submux, "Got the condition signal");
      g_mutex_unlock (cur_stream->queue_lock);
      goto error;
    }

    check_buffer = g_queue_peek_head (cur_stream->queue);
    if (!strcmp ((const char*)GST_BUFFER_DATA (check_buffer), "eos")){
      cur_stream->eos_came = TRUE;
      GST_DEBUG_OBJECT (submux, "Eos recieved for stream");
    }
    for (k = 0; k < length; k++)  {
      check_stream = g_list_nth_data(submux->streams, k);
      if (!check_stream->eos_came) {
        eos = FALSE;
        break;
      } else {
        eos = TRUE;
      }
    }
    if (eos) {
      GST_DEBUG_OBJECT (submux, "Sending EOS to submux srcpad");
      gst_pad_push_event(submux->srcpad, gst_event_new_eos ());
      g_mutex_unlock (cur_stream->queue_lock);
      goto eos_sent;
    }

    if (!cur_stream->eos_came && (submux->priv->parser_type == GST_SUB_PARSE_FORMAT_SAMI ||
                                  submux->priv->is_internal)) {
      GstLangStruct *lang = NULL;
      if (submux->priv->lang_list) {
        if (submux->cur_buf_array[i] == NULL) {
          check_buffer = g_queue_pop_head (cur_stream->queue);
          lang = g_list_nth_data(submux->priv->lang_list, i);
          if (!lang->active) {
            if(check_buffer) {
              gst_buffer_unref(check_buffer);
              check_buffer = NULL;
              GST_DEBUG_OBJECT (submux, "unreffing the non-active stream[%d] buffer", i);
            }
            submux->cur_buf_array[i] = NULL;
            g_mutex_unlock (cur_stream->queue_lock);
            GST_DEBUG_OBJECT(submux,"rejecting not selected language");
            continue;
          } else {
            if (!check_buffer) {
              GST_WARNING_OBJECT (submux, "checkbuffer null.. repop");
              g_mutex_unlock (cur_stream->queue_lock);
              goto re_pop;
            }
            if (!GST_BUFFER_DURATION(check_buffer)) {
              GST_WARNING_OBJECT (submux, "duration of buffer is zero..re-pop");
              gst_buffer_unref (check_buffer);
              g_mutex_unlock (cur_stream->queue_lock);
              goto re_pop;
            }
            submux->cur_buf_array[i] = check_buffer;
            GST_DEBUG_OBJECT (submux, "consuming active stream [%d] buffer : ts = %"GST_TIME_FORMAT"and dur = %"GST_TIME_FORMAT,
              i, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP (check_buffer)), GST_TIME_ARGS(GST_BUFFER_DURATION (check_buffer)));
          }
        }
      } else {
        g_mutex_unlock (cur_stream->queue_lock);
        GST_DEBUG_OBJECT(submux,"Coming to Else case lang submux->priv->lang_list %x ",submux->priv->lang_list);
        continue;
      }
    } else if (!cur_stream->eos_came) {
      /* External subtitle format other than smi */
      if (submux->sinkpad) {
        if (submux->cur_buf_array[i] == NULL) {
          check_buffer = g_queue_pop_head (cur_stream->queue);
          if (!check_buffer) {
            GST_WARNING_OBJECT (submux, "checkbuffer null.. repop");
            g_mutex_unlock (cur_stream->queue_lock);
            goto re_pop;
          }
          if (!GST_BUFFER_DURATION (check_buffer)) {
            GST_WARNING_OBJECT (submux, "duration of buffer is zero..re-pop");
            gst_buffer_unref (check_buffer);
            g_mutex_unlock (cur_stream->queue_lock);
            goto re_pop;
          }
          submux->cur_buf_array[i] = check_buffer;
          GST_DEBUG_OBJECT (submux, "consuming active stream [%d] buffer : ts = %"GST_TIME_FORMAT"and dur = %"GST_TIME_FORMAT,
            i, GST_TIME_ARGS(GST_BUFFER_TIMESTAMP (check_buffer)), GST_TIME_ARGS(GST_BUFFER_DURATION (check_buffer)));
        }
      } else {
        g_mutex_unlock (cur_stream->queue_lock);
        GST_DEBUG_OBJECT(submux,"Coming to Else case submux->sinkpad %x ",submux->sinkpad);
        continue;
      }
    } else {
      GST_DEBUG_OBJECT (submux, "already received EOS on this stream[%d] and cur_buf_array[idx] = NULL", i);
      submux->cur_buf_array[i] = NULL;
    }

    g_mutex_unlock (cur_stream->queue_lock);
    GST_DEBUG_OBJECT (submux, "After unlocking in loop and signal queue full");
  }

  push_list = gst_submux_muxing (submux);
  if (push_list) {
    guint idx = 0;
    GST_LOG_OBJECT (submux, "length of push list = %d", g_list_length (push_list));

    for (idx = 0; idx < g_list_length (push_list); idx++) {
      push_buf = g_list_nth_data (push_list, idx);

      if (push_buf) {
        GST_DEBUG_OBJECT (submux, "pushing buffer : ts = %"GST_TIME_FORMAT", "
            "dur = %"GST_TIME_FORMAT" and data %s ...",
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (push_buf)),
        GST_TIME_ARGS (GST_BUFFER_DURATION (push_buf)), (gchar*)GST_BUFFER_DATA (push_buf));

        fret = gst_pad_push (submux->srcpad, push_buf);
        if (fret != GST_FLOW_OK) {
          GST_ERROR_OBJECT (submux, "failed to push buffer. reason : %s", gst_flow_get_name (fret));
          /* clean any left buffers in push_list */
          idx++;
          for (; idx < g_list_length (push_list); idx++) {
            push_buf = g_list_nth_data (push_list, idx);
            gst_buffer_unref (push_buf);
          }
          g_list_free (push_list);
          goto error;
        }
      }
    }

    g_list_free (push_list);
  }

  GST_DEBUG_OBJECT (submux, "Exiting from lopp in last");

  return;

eos_sent:
error:
  GST_WARNING_OBJECT (submux->srcpad, "Pausing the push task...");
  if (fret < GST_FLOW_UNEXPECTED) {
    GST_ERROR_OBJECT (submux, "Crtical error in push loop....");
    GST_ELEMENT_ERROR (submux, CORE, PAD, ("failed to push. reason - %s", gst_flow_get_name (fret)), (NULL));
  }
  gst_pad_pause_task (submux->srcpad);
  GST_DEBUG_OBJECT (submux, "Exiting from lopp in last");
  return;
}

////////////////////////////////////////////////////////
//        Plugin Utility Functions                    //
////////////////////////////////////////////////////////
/*
**
**  Description    : De-Initializing the submux private structure
**  Params        : (1) submux instance
**  return        : TRUE
**  Comments    :
**
*/
static gboolean
gst_submux_deinit_private_values(Gstsubmux *submux)
{
  guint idx = 0;
  GST_DEBUG_OBJECT (submux, "deinit priv values");

  submux->priv->first_buffer = FALSE;
  submux->priv->parser_type = 0;
  if (submux->priv->lang_list && !submux->priv->is_internal) {
    g_list_free (submux->priv->lang_list);
    submux->priv->lang_list = NULL;
  }
  for (idx = 0; idx < submux->priv->stream_count; idx++) {
    submux->cur_buf_array[idx] = NULL;
  }
  if (submux->cur_buf_array) {
    g_free (submux->cur_buf_array);
    submux->cur_buf_array = NULL;
  }

  submux->priv->is_internal = FALSE;
  submux->priv->stream_count = 0;

  return TRUE;
}

static gboolean
gst_submux_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "submux", GST_RANK_PRIMARY, GST_TYPE_SUBMUX);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,GST_VERSION_MINOR,"submux","submux",gst_submux_plugin_init,"0.10.36","Proprietary","Samsung Electronics Co","http://www.samsung.com")
