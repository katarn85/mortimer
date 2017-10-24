/*
 * wfdrtspmanager
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

#ifndef __GST_WFD_RTSP_MANAGER_H__
#define __GST_WFD_RTSP_MANAGER_H__

#include <gst/gst.h>
#include <gst/rtsp/gstrtspconnection.h>
#include <gst/rtsp/gstrtspurl.h>
#include <sys/socket.h>


G_BEGIN_DECLS


typedef struct _WFDRTSPManager WFDRTSPManager;
typedef struct _WFDRTSPManagerClass WFDRTSPManagerClass;

#define WFD_TYPE_RTSP_MANAGER             (wfd_rtsp_manager_get_type())
#define WFD_RTSP_MANAGER(ext)             (G_TYPE_CHECK_INSTANCE_CAST((ext),WFD_TYPE_RTSP_MANAGER, WFDRTSPManager))
#define WFD_RTSP_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),WFD_TYPE_RTSP_MANAGER,WFDRTSPManagerClass))
#define WFD_IS_RTSP_MANAGER(ext)          (G_TYPE_CHECK_INSTANCE_TYPE((ext),WFD_TYPE_RTSP_MANAGER))
#define WFD_IS_RTSP_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),WFD_TYPE_RTSP_MANAGER))
#define WFD_RTSP_MANAGER_CAST(ext)        ((WFDRTSPManager *)(ext))


#define WFD_RTSP_MANAGER_STATE_GET_LOCK(manager)    (WFD_RTSP_MANAGER_CAST(manager)->state_rec_lock)
#define WFD_RTSP_MANAGER_STATE_LOCK(manager)        (g_static_rec_mutex_lock (WFD_RTSP_MANAGER_STATE_GET_LOCK(manager)))
#define WFD_RTSP_MANAGER_STATE_UNLOCK(manager)      (g_static_rec_mutex_unlock (WFD_RTSP_MANAGER_STATE_GET_LOCK(manager)))

typedef struct _GstWFDRTSPConnInfo GstWFDRTSPConnInfo;

struct _GstWFDRTSPConnInfo {
  gchar              *location;
  GstRTSPUrl         *url;
  gchar              *url_str;
  GstRTSPConnection  *connection;
  gboolean            connected;
};

struct _WFDRTSPManager {
  GObject       object;

  GstElement   *wfdrtspsrc; /* parent, no extra ref to parent is taken */

  /* pad we expose or NULL when it does not have an actual pad */
  GstPad       *srcpad;
  gboolean      eos;
  gboolean      discont;
  gboolean         need_activate;
  GStaticRecMutex *state_rec_lock;

  /* for interleaved mode */
  GstCaps      *caps;
  GstPad       *channelpad[2];

  /* our udp sources */
  GstElement   *udpsrc[2];
  GstPad       *blockedpad;
  gboolean      is_ipv6;

  /* our udp sinks back to the server */
  GstElement   *udpsink[2];
  GstPad       *rtcppad;

  /* fakesrc for sending dummy data */
  GstElement   *fakesrc;

  /* original control url */
  gchar        *control_url;
  GstRTSPConnection *control_connection;

  guint64       seqbase;
  guint64       timebase;

  /* per manager connection */
  GstWFDRTSPConnInfo  conninfo;

  /* pipeline */
  GstElement      *requester;
  GstElement      *session;
  GstElement      *wfdrtpbuffer;

  /* properties */
  gboolean          do_rtcp;
  guint             latency;
  gint              udp_buffer_size;
  guint64           udp_timeout;
  gboolean          enable_pad_probe;

  GstRTSPLowerTrans  protocol;
};

struct _WFDRTSPManagerClass {
  GObjectClass   parent_class;
};

GType wfd_rtsp_manager_get_type (void);


WFDRTSPManager*
wfd_rtsp_manager_new  (GstElement *wfdrtspsrc);

gboolean
wfd_rtsp_manager_configure_transport (WFDRTSPManager * manager,
    GstRTSPTransport * transport, gint rtpport, gint rtcpport);

GstRTSPResult
wfd_rtsp_manager_message_dump (GstRTSPMessage * msg);
void
wfd_rtsp_manager_enable_pad_probe(WFDRTSPManager * manager);
void
wfd_rtsp_manager_flush (WFDRTSPManager * manager, gboolean flush);

G_END_DECLS

#endif /* __GST_WFD_RTSP_MANAGER_H__ */
