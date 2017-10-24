/*
 * wfdrtspmanagerext
 *
 * Copyright (c) 2011 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: ByungWook Jang <bw.jang@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
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
  GstPad       *channelpad[3];

  /* our udp sources */
  GstElement   *udpsrc[3];
  GstPad       *blockedpad;
  gboolean      is_ipv6;

  /* our udp sinks back to the server */
  GstElement   *udpsink[3];
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

  /*For tcp*/
  GStaticRecMutex tcp_task_lock;
  GstTask *tcp_task;
  GStaticRecMutex tcp_status_task_lock;
  GstTask *tcp_status_report_task;
  int tcp_socket;
  gint32 audio_data_per_sec;

  /* properties */
  gboolean          do_rtcp;
  guint             latency;
  gint              udp_buffer_size;
  guint64           udp_timeout;
  gchar *device_name;
  gboolean do_request;
  GstElement *demux_handle;
  GstElement *audio_queue_handle;
  gboolean buffering_animation;
  gboolean enable_pad_probe;

  GstRTSPLowerTrans  protocol;


  gboolean extended_feature_support;
  void *extended_handle;
  gint rtcp_fb_socketfd;

  gboolean idr_request_is_avilable;
  guint idr_request_availability_checker;
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
wfd_rtsp_manager_hadle_request (WFDRTSPManager * manager, guint8 *data, GstRTSPMessage * request, gboolean *check_result, gboolean *need_idr);
GstRTSPResult
wfd_rtsp_manager_message_dump (GstRTSPMessage * msg);
void
wfd_rtsp_manager_enable_pad_probe(WFDRTSPManager * manager);
void
wfd_rtsp_manager_flush (WFDRTSPManager * manager, gboolean flush);
G_END_DECLS

#endif /* __GST_WFD_RTSP_MANAGER_H__ */

