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


#ifndef __GST_WFD_RTSP_EXT_H__
#define __GST_WFD_RTSP_EXT_H__

#include <gst/gst.h>
#include <gst/rtsp/gstrtspextension.h>

G_BEGIN_DECLS

typedef struct _GstWFDRTSPExtensionList GstWFDRTSPExtensionList;

struct _GstWFDRTSPExtensionList
{
  GList *extensions;
};

void                    gst_wfd_rtsp_ext_list_init    (void);

GstWFDRTSPExtensionList *  gst_wfd_rtsp_ext_list_get     (void);
void                    gst_wfd_rtsp_ext_list_free    (GstWFDRTSPExtensionList *ext);

gboolean      gst_wfd_rtsp_ext_list_detect_server     (GstWFDRTSPExtensionList *ext, GstRTSPMessage *resp);

GstRTSPResult gst_wfd_rtsp_ext_list_before_send       (GstWFDRTSPExtensionList *ext, GstRTSPMessage *req);
GstRTSPResult gst_wfd_rtsp_ext_list_after_send        (GstWFDRTSPExtensionList *ext, GstRTSPMessage *req,
                                                   GstRTSPMessage *resp);
GstRTSPResult gst_wfd_rtsp_ext_list_parse_sdp         (GstWFDRTSPExtensionList *ext, GstSDPMessage *sdp,
                                                   GstStructure *s);
GstRTSPResult gst_wfd_rtsp_ext_list_setup_media       (GstWFDRTSPExtensionList *ext, GstSDPMedia *media);
gboolean      gst_wfd_rtsp_ext_list_configure_stream  (GstWFDRTSPExtensionList *ext, GstCaps *caps);
GstRTSPResult gst_wfd_rtsp_ext_list_get_transports    (GstWFDRTSPExtensionList *ext, GstRTSPLowerTrans protocols,
                                                   gchar **transport);
GstRTSPResult gst_wfd_rtsp_ext_list_stream_select     (GstWFDRTSPExtensionList *ext, GstRTSPUrl *url);

void          gst_wfd_rtsp_ext_list_connect           (GstWFDRTSPExtensionList *ext,
			                           const gchar *detailed_signal, GCallback c_handler,
                                                   gpointer data);
GstRTSPResult gst_wfd_rtsp_ext_list_receive_request   (GstWFDRTSPExtensionList *ext, GstRTSPMessage *req);

G_END_DECLS

#endif /* __GST_RTSP_EXT_H__ */
