/*
 * wfdrtspsrc
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



#ifndef __GST_WFDRTSPSRC_H__
#define __GST_WFDRTSPSRC_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#include <gst/rtsp/gstrtspconnection.h>
#include <gst/rtsp/gstrtspmessage.h>
#include <gst/rtsp/gstrtspurl.h>
#include <gst/rtsp/gstrtsprange.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "wfdrtspconfigmessage.h"
#ifdef ENABLE_WFD_SINK_UIBC
#include "wfdrtspuibcmessage.h"
#endif
#define ENABLE_WFD_MESSAGE

#include "gstwfdrtspext.h"
#ifdef ENABLE_WFD_EXTENDED_FEATURES
#include "extension/wfdrtspmanagerext.h"
#else
#include "wfdrtspmanager.h"
#endif

#define GST_TYPE_WFDRTSPSRC \
  (gst_wfdrtspsrc_get_type())
#define GST_WFDRTSPSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WFDRTSPSRC,GstWFDRTSPSrc))
#define GST_WFDRTSPSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_WFDRTSPSRC,GstWFDRTSPSrcClass))
#define GST_IS_WFDRTSPSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WFDRTSPSRC))
#define GST_IS_WFDRTSPSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_WFDRTSPSRC))
#define GST_WFDRTSPSRC_CAST(obj) \
  ((GstWFDRTSPSrc *)(obj))

typedef struct _GstWFDRTSPSrc GstWFDRTSPSrc;
typedef struct _GstWFDRTSPSrcClass GstWFDRTSPSrcClass;
typedef struct _GstWFDRTSPSrcPrivate GstWFDRTSPSrcPrivate;

#define GST_WFD_RTSP_TASK_GET_LOCK(wfd)   (GST_WFDRTSPSRC_CAST(wfd)->task_rec_lock)
#define GST_WFD_RTSP_TASK_LOCK(wfd)       (g_static_rec_mutex_lock (GST_WFD_RTSP_TASK_GET_LOCK(wfd)))
#define GST_WFD_RTSP_TASK_UNLOCK(wfd)     (g_static_rec_mutex_unlock (GST_WFD_RTSP_TASK_GET_LOCK(wfd)))

/**
 * GstWFDRTSPNatMethod:
 * @GST_WFD_RTSP_NAT_NONE: none
 * @GST_WFD_RTSP_NAT_DUMMY: send dummy packets
 *
 * Different methods for trying to traverse firewalls.
 */
typedef enum
{
  GST_WFD_RTSP_NAT_NONE,
  GST_WFD_RTSP_NAT_DUMMY
} GstWFDRTSPNatMethod;

/**
 * GstWFDParam:
 * @WFD_ROUTE:Route audio from Primary Sink to Secondary Sink or vice-versa.
 * @WFD_CONNECTOR_TYPE:The WFD Sink uses this parameter to inform the WFD Source of a connector type change and report the currently active connector type.
 * @WFD_STANDBY:Sender is entering WFD Standby mode.
 * @WFD_IDR_REQUEST:WFD Sink requests an IDR picture from the WFD Source.
 * @WFD_UIBC_CAPABILITY:Select UIBC to be used.
 * @WFD_UIBC_SETTING:Enable or disable the UIBC.
*/
typedef enum
{
  WFD_PARAM_NONE,
  WFD_ROUTE,
  WFD_CONNECTOR_TYPE,
  WFD_STANDBY,
  WFD_IDR_REQUEST,
  WFD_UIBC_CAPABILITY,
  WFD_UIBC_SETTING,
  WFD_SINK_STATUS
} GstWFDParam;

#ifdef ENABLE_WFD_SINK_UIBC
typedef struct _UIBC UIBC;
struct _UIBC {
  gint gen_capability;
  WFDHIDCTypePathPair *hidc_cap_list;
  WFDHIDCTypePathPair *hidc_cap_list_src;
  guint32 hidc_cap_count_src;
  gint sockfd;
  guint32 port;
  void *wfd_sink_handle;
  wfd_uibc_state_cb state_cb;
  gboolean is_enabled;
};
#endif

struct _GstWFDRTSPSrc {
  GstBin           parent;

  WFDRTSPManager           *manager;

  /* task and mutex */
  GstTask         *task;
  GStaticRecMutex *task_rec_lock;
  gint             loop_cmd;
  gboolean         waiting;
  gboolean do_stop;
  GstWFDParam wfd_param;

  /* properties */
  GstRTSPLowerTrans protocols;
  gboolean          debug;
  guint             retry;
  GTimeVal          tcp_timeout;
  GTimeVal         *ptcp_timeout;
  GstWFDRTSPNatMethod  nat_method;
  gchar            *proxy_host;
  guint             proxy_port;
  gchar            *proxy_user;
  gchar            *proxy_passwd;
  guint             rtp_blocksize;
  gchar            *user_id;
  gchar            *user_pw;
  GstStructure *audio_param;
  GstStructure *video_param;
  GstStructure *hdcp_param;
  /* Set User-agent */
  gchar *user_agent;
  /* Set RTP port */
  gint primary_rtpport;
  gint secondary_rtpport;

  /* state */
  GstRTSPState       state;
  gboolean           tried_url_auth;
  gboolean           need_redirect;

  /* supported methods */
  gint               methods;

  GstWFDRTSPConnInfo  conninfo;

  /* a list of RTSP extensions as GstElement */
  GstWFDRTSPExtensionList  *extensions;


  GTimeVal ssr_timeout;
  guint video_height;
  guint video_width;
  guint video_framerate;
  gchar * audio_format;
  guint audio_channels;
  guint audio_bitwidth;
  guint audio_frequency;

  gboolean is_paused;
#ifdef ENABLE_WFD_SINK_UIBC
  UIBC *uibc;
  void *uibc_handle;
  gboolean extended_uibc_support;

#endif
#ifdef ENABLE_WFD_SINK_WIDI
  WIDI *widi;
  void *widi_handle;
  gboolean extended_widi_support;

#endif

#ifdef ENABLE_WFD_MESSAGE
  void *message_handle;
  gboolean extended_wfd_message_support;
#endif

};

struct _GstWFDRTSPSrcClass {
  GstBinClass parent_class;

  /* signals */
  void     (*update_media_info)       (GstWFDRTSPSrc *src, GstStructure * str);
  void     (*change_av_format)       (GstWFDRTSPSrc *src, gpointer *need_to_flush);
};

GType gst_wfdrtspsrc_get_type(void);

G_END_DECLS

#endif /* __GST_WFDRTSPSRC_H__ */
