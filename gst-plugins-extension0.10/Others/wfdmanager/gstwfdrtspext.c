/*
 * wfdrtspext
 *
 * Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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


#include "gstwfdrtspext.h"

GST_DEBUG_CATEGORY_STATIC (wfdrtspext_debug);
#define GST_CAT_DEFAULT (wfdrtspext_debug)

static GList *extensions;

static gboolean
gst_wfd_rtsp_ext_list_filter (GstPluginFeature * feature, gpointer user_data)
{
  GstElementFactory *factory;
  guint rank;

  /* we only care about element factories */
  if (!GST_IS_ELEMENT_FACTORY (feature))
    return FALSE;

  factory = GST_ELEMENT_FACTORY (feature);

  if (!gst_element_factory_has_interface (factory, "GstWFDRTSPExtension"))
    return FALSE;

  /* only select elements with autoplugging rank */
  rank = gst_plugin_feature_get_rank (feature);
  if (rank < GST_RANK_MARGINAL)
    return FALSE;

  return TRUE;
}

void
gst_wfd_rtsp_ext_list_init (void)
{
  GST_DEBUG_CATEGORY_INIT (wfdrtspext_debug, "wfdrtspext", 0, "WFD RTSP extension");

  /* get a list of all extensions */
  extensions = gst_registry_feature_filter (gst_registry_get_default (),
      (GstPluginFeatureFilter) gst_wfd_rtsp_ext_list_filter, FALSE, NULL);
}

GstWFDRTSPExtensionList *
gst_wfd_rtsp_ext_list_get (void)
{
  GstWFDRTSPExtensionList *result;
  GList *walk;

  result = g_new0 (GstWFDRTSPExtensionList, 1);

  for (walk = extensions; walk; walk = g_list_next (walk)) {
    GstElementFactory *factory = GST_ELEMENT_FACTORY (walk->data);
    GstElement *element;

    element = gst_element_factory_create (factory, NULL);
    if (!element) {
      GST_ERROR ("could not create extension instance");
      continue;
    }

    GST_DEBUG ("added extension interface for '%s'",
        GST_ELEMENT_NAME (element));
    result->extensions = g_list_prepend (result->extensions, element);
  }
  return result;
}

void
gst_wfd_rtsp_ext_list_free (GstWFDRTSPExtensionList * ext)
{
  GList *walk;

  for (walk = ext->extensions; walk; walk = g_list_next (walk)) {
    GstRTSPExtension *elem = (GstRTSPExtension *) walk->data;

    gst_object_unref (GST_OBJECT_CAST (elem));
  }
  g_list_free (ext->extensions);
  g_free (ext);
}

gboolean
gst_wfd_rtsp_ext_list_detect_server (GstWFDRTSPExtensionList * ext,
    GstRTSPMessage * resp)
{
  GList *walk;
  gboolean res = TRUE;

  for (walk = ext->extensions; walk; walk = g_list_next (walk)) {
    GstRTSPExtension *elem = (GstRTSPExtension *) walk->data;

    res = gst_rtsp_extension_detect_server (elem, resp);
  }
  return res;
}

GstRTSPResult
gst_wfd_rtsp_ext_list_before_send (GstWFDRTSPExtensionList * ext, GstRTSPMessage * req)
{
  GList *walk;
  GstRTSPResult res = GST_RTSP_OK;

  for (walk = ext->extensions; walk; walk = g_list_next (walk)) {
    GstRTSPExtension *elem = (GstRTSPExtension *) walk->data;

    res = gst_rtsp_extension_before_send (elem, req);
  }
  return res;
}

GstRTSPResult
gst_wfd_rtsp_ext_list_after_send (GstWFDRTSPExtensionList * ext, GstRTSPMessage * req,
    GstRTSPMessage * resp)
{
  GList *walk;
  GstRTSPResult res = GST_RTSP_OK;

  for (walk = ext->extensions; walk; walk = g_list_next (walk)) {
    GstRTSPExtension *elem = (GstRTSPExtension *) walk->data;

    res = gst_rtsp_extension_after_send (elem, req, resp);
  }
  return res;
}

GstRTSPResult
gst_wfd_rtsp_ext_list_parse_sdp (GstWFDRTSPExtensionList * ext, GstSDPMessage * sdp,
    GstStructure * s)
{
  GList *walk;
  GstRTSPResult res = GST_RTSP_OK;

  for (walk = ext->extensions; walk; walk = g_list_next (walk)) {
    GstRTSPExtension *elem = (GstRTSPExtension *) walk->data;

    res = gst_rtsp_extension_parse_sdp (elem, sdp, s);
  }
  return res;
}

GstRTSPResult
gst_wfd_rtsp_ext_list_setup_media (GstWFDRTSPExtensionList * ext, GstSDPMedia * media)
{
  GList *walk;
  GstRTSPResult res = GST_RTSP_OK;

  for (walk = ext->extensions; walk; walk = g_list_next (walk)) {
    GstRTSPExtension *elem = (GstRTSPExtension *) walk->data;

    res = gst_rtsp_extension_setup_media (elem, media);
  }
  return res;
}

gboolean
gst_wfd_rtsp_ext_list_configure_stream (GstWFDRTSPExtensionList * ext, GstCaps * caps)
{
  GList *walk;
  gboolean res = TRUE;

  for (walk = ext->extensions; walk; walk = g_list_next (walk)) {
    GstRTSPExtension *elem = (GstRTSPExtension *) walk->data;

    res = gst_rtsp_extension_configure_stream (elem, caps);
    if (!res)
      break;
  }
  return res;
}

GstRTSPResult
gst_wfd_rtsp_ext_list_get_transports (GstWFDRTSPExtensionList * ext,
    GstRTSPLowerTrans protocols, gchar ** transport)
{
  GList *walk;
  GstRTSPResult res = GST_RTSP_OK;

  for (walk = ext->extensions; walk; walk = g_list_next (walk)) {
    GstRTSPExtension *elem = (GstRTSPExtension *) walk->data;

    res = gst_rtsp_extension_get_transports (elem, protocols, transport);
  }
  return res;
}

GstRTSPResult
gst_wfd_rtsp_ext_list_stream_select (GstWFDRTSPExtensionList * ext, GstRTSPUrl * url)
{
  GList *walk;
  GstRTSPResult res = GST_RTSP_OK;

  for (walk = ext->extensions; walk; walk = g_list_next (walk)) {
    GstRTSPExtension *elem = (GstRTSPExtension *) walk->data;

    res = gst_rtsp_extension_stream_select (elem, url);
  }
  return res;
}

void
gst_wfd_rtsp_ext_list_connect (GstWFDRTSPExtensionList * ext,
    const gchar * detailed_signal, GCallback c_handler, gpointer data)
{
  GList *walk;

  for (walk = ext->extensions; walk; walk = g_list_next (walk)) {
    GstRTSPExtension *elem = (GstRTSPExtension *) walk->data;

    g_signal_connect (elem, detailed_signal, c_handler, data);
  }
}

GstRTSPResult
gst_wfd_rtsp_ext_list_receive_request (GstWFDRTSPExtensionList * ext,
    GstRTSPMessage * req)
{
  GList *walk;
  GstRTSPResult res = GST_RTSP_ENOTIMPL;

  for (walk = ext->extensions; walk; walk = g_list_next (walk)) {
    GstRTSPExtension *elem = (GstRTSPExtension *) walk->data;

    res = gst_rtsp_extension_receive_request (elem, req);
    if (res != GST_RTSP_ENOTIMPL)
      break;
  }
  return res;
}
