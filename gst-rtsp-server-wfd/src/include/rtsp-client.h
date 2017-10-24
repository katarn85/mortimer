/* GStreamer
 * Copyright (C) 2008 Wim Taymans <wim.taymans at gmail.com>

* Copyright (c) 2012, 2013 Samsung Electronics Co., Ltd.
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

* * Modifications by Samsung Electronics Co., Ltd.

* 1. Applied Miracast WFD Server function
*/

#include <gst/gst.h>
#include <gst/rtsp/gstrtspconnection.h>


#ifndef __GST_RTSP_CLIENT_H__
#define __GST_RTSP_CLIENT_H__

G_BEGIN_DECLS

typedef struct _GstRTSPClient GstRTSPClient;
typedef struct _GstRTSPClientClass GstRTSPClientClass;
typedef struct _GstRTSPClientState GstRTSPClientState;
typedef struct _GstRTSPClientSrcBin GstRTSPClientSrcBin;
typedef struct _UIBC UIBC;

#include "mmf/wfdconfigmessage.h"
#include "mmf/uibcmessage.h"
#include "rtsp-server.h"
#include "rtsp-media.h"
#include "rtsp-media-mapping.h"
#include "rtsp-session-pool.h"
#include "rtsp-auth.h"
#include<linux/tcp.h>

#define GST_TYPE_RTSP_CLIENT              (gst_rtsp_client_get_type ())
#define GST_IS_RTSP_CLIENT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_RTSP_CLIENT))
#define GST_IS_RTSP_CLIENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_RTSP_CLIENT))
#define GST_RTSP_CLIENT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_RTSP_CLIENT, GstRTSPClientClass))
#define GST_RTSP_CLIENT(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_RTSP_CLIENT, GstRTSPClient))
#define GST_RTSP_CLIENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_RTSP_CLIENT, GstRTSPClientClass))
#define GST_RTSP_CLIENT_CAST(obj)         ((GstRTSPClient*)(obj))
#define GST_RTSP_CLIENT_CLASS_CAST(klass) ((GstRTSPClientClass*)(klass))

typedef enum {
  CLIENT_STATE_UNKNOWN = -1,
  CLIENT_STATE_CONNECTING = 0,
  CLIENT_STATE_CONNECTED = 1,
}ClientState;

typedef enum {
  UIBC_START = 0,
  UIBC_STOP
} UIBCState;

typedef enum {
  WFD_MESSAGE_UNKNOWN,
  WFD_MESSAGE_1,
  WFD_MESSAGE_2,
  WFD_MESSAGE_3,
  WFD_MESSAGE_4,
  WFD_MESSAGE_5,
  WFD_MESSAGE_6,
  WFD_MESSAGE_7,
  WFD_MESSAGE_8,
  WFD_MESSAGE_9,
  WFD_MESSAGE_10,
  WFD_MESSAGE_11,
  WFD_MESSAGE_12,
  WFD_MESSAGE_13,
  WFD_MESSAGE_14,
  WFD_MESSAGE_15,
  WFD_MESSAGE_16,
#ifdef ENABLE_WFD_EXTENDED_FEATURES
  WFD_MESSAGE_T1,
  WFD_MESSAGE_T2_A,
  WFD_MESSAGE_T2_B,
  WFD_MESSAGE_T2_F_A,
  WFD_MESSAGE_T2_F_B,
  WFD_MESSAGE_T2_C,
  WFD_MESSAGE_T2_D,
  WFD_MESSAGE_T2_E,
  WFD_MESSAGE_T2_F,
  WFD_MESSAGE_T3,
  WFD_MESSAGE_U1,
  WFD_MESSAGE_U2
#endif
}WFDMessageType;

#ifdef ENABLE_WFD_EXTENDED_FEATURES
typedef enum {
  WFD_T1_A_MODE,
  WFD_T1_B_MODE
}WFDT1Mode;

typedef struct _WFDBModeParams WFDBModeParams;
struct _WFDBModeParams {
  guint32 curr_rtt;
  guint32 rtt_moving_avg;
  guint32 prev_rtt_when_mode_changed;
  guint32 prev_mode_changed_time;
  guint32 qos_count;
  guint32 prev_cwnd;
  guint32 prev_buffer_left;
  guint32 max_buffer_size;
  guint32 prev_network_status[10];
  guint32 prev_setting_bps;
  guint32 init_bitrate;
  guint32 min_bitrate;
  guint32 max_bitrate;
};

typedef enum {
  SCP_NULL,
  SCP_PAUSE,
  SCP_PLAY,
  SCP_FLUSH_PAUSE,
  SCP_FLUSH_PLAY
} SCPState;

typedef struct tcp_info TCP_Info;

typedef struct _WFDBModeInfoBuffer WFDBModeInfoBuffer;
struct _WFDBModeInfoBuffer {
  TCP_Info tcp_info;
  guint32 socket_buffer_size;
  guint32 socket_buffer_size_left;
};
#endif

typedef enum __wfd_ini_videosink_element
{
  WFD_INI_VSINK_V4l2SINK = 0,
  WFD_INI_VSINK_XIMAGESINK,
  WFD_INI_VSINK_XVIMAGESINK,
  WFD_INI_VSINK_FAKESINK,
  WFD_INI_VSINK_EVASIMAGESINK,
  WFD_INI_VSINK_GLIMAGESINK,
  WFD_INI_VSINK_NUM
}WFD_INI_VSINK_ELEMENT;

typedef enum __wfd_ini_videosrc_element
{
  WFD_INI_VSRC_XVIMAGESRC,
  WFD_INI_VSRC_FILESRC,
  WFD_INI_VSRC_CAMERASRC,
  WFD_INI_VSRC_VIDEOTESTSRC,
  WFD_INI_VSRC_NUM
}WFD_INI_VSRC_ELEMENT;

typedef enum __wfd_ini_session_mode
{
  WFD_INI_AUDIO_VIDEO_MUXED,
  WFD_INI_VIDEO_ONLY,
  WFD_INI_AUDIO_ONLY,
  WFD_INI_AUDIO_VIDEO_SAPERATE
}WFD_INI_SESSION_MODE;

/**
 * GstRTSPClientState:
 * @request: the complete request
 * @uri: the complete url parsed from @request
 * @method: the parsed method of @uri
 * @session: the session, can be NULL
 * @sessmedia: the session media for the url can be NULL
 * @factory: the media factory for the url, can be NULL.
 * @media: the session media for the url can be NULL
 * @response: the response
 *
 * Information passed around containing the client state of a request.
 */
struct _GstRTSPClientState{
  GstRTSPMessage      *request;
  GstRTSPUrl          *uri;
  GstRTSPMethod        method;
  GstRTSPSession      *session;
  GstRTSPSessionMedia *sessmedia;
  GstRTSPMediaFactory *factory;
  GstRTSPMedia        *media;
  GstRTSPMessage      *response;
};

struct _GstRTSPClientSrcBin{
  GstBin *srcbin;
  GstElement *vqueue;
  GstElement *aqueue;
  GstElement *tee_0;
  GstElement *venc;
  GstElement *tsmux;
  GstPad *mux_vsinkpad;
  GstPad *mux_asinkpad;
  GstElement *vparse;

};

/*uibc related variables*/
struct _UIBC {
  guint8 neg_input_cat;
  guint8 gen_capability;
  guint8 neg_gen_capability;
  WFDHIDCTypePathPair *hidc_cap_list;
  WFDHIDCTypePathPair *neg_hidc_cap_list;
  guint neg_hidc_cap_count;
  gboolean setting;
  GThread * thread;
  gint mainfd;
  gint fd;
  guint port;
  wfd_uibc_control_cb control_cb;
  wfd_uibc_send_event_cb event_cb;
  void *user_param;
};

/**
 * GstRTSPClient:
 *
 * @connection: the connection object handling the client request.
 * @watch: watch for the connection
 * @watchid: id of the watch
 * @ip: ip address used by the client to connect to us
 * @session_pool: handle to the session pool used by the client.
 * @media_mapping: handle to the media mapping used by the client.
 * @uri: cached uri
 * @media: cached media
 * @streams: a list of streams using @connection.
 * @sessions: a list of sessions managed by @connection.
 *
 * The client structure.
 */
struct _GstRTSPClient {
  GObject       parent;

  GstRTSPConnection *connection;
  GstRTSPWatch      *watch;
  guint            watchid;
  gchar           *server_ip;
  gboolean is_ipv6;
  gchar  *wfdsink_ip;
  gchar  *client_ip;

  GstRTSPServer        *server;
  GstRTSPSessionPool   *session_pool;
  gchar * sessionid;
  GstRTSPMediaMapping  *media_mapping;
  GstRTSPAuth          *auth;

  GstRTSPUrl     *uri;
  GstRTSPMedia   *media;

  GList *streams;
  GList *sessions;

  /* supported methods */
  gint               methods;

  gint connection_mode;
  GstRTSPClientSrcBin *srcbin;

  int videosrc_type;
  gint session_mode;
  gchar *infile;

  gchar *audio_device;
  gint audio_latency_time;
  gint audio_buffer_time;
  gint audio_do_timestamp;

  GCond *state_wait;
  ClientState client_state;

  WFDAudioFormats caCodec;
  WFDAudioFreq cFreq;
  WFDAudioChannels cChanels;
  guint cBitwidth;
  guint caLatency;
  WFDVideoCodecs cvCodec;
  WFDVideoNativeResolution cNative;
  guint64 cNativeResolution;
  guint64 cVideo_reso_supported;
  guint decide_udp_bitrate[21];
  gint cSrcNative;
  WFDVideoCEAResolution cCEAResolution;
  WFDVideoVESAResolution cVESAResolution;
  WFDVideoHHResolution cHHResolution;
  WFDVideoH264Profile cProfile;
  WFDVideoH264Level cLevel;
  guint32 cMaxHeight;
  guint32 cMaxWidth;
  guint32 cFramerate;
  guint32 init_udp_bitrate;
  guint32 max_udp_bitrate;
  guint32 min_udp_bitrate;
  guint32 cInterleaved;
  guint32 cmin_slice_size;
  guint32 cslice_enc_params;
  guint cframe_rate_control;
  guint bitrate;
  guint MTUsize;
  guint cvLatency;
  WFDRTSPTransMode ctrans;
  WFDRTSPProfile cprofile;
  WFDRTSPLowerTrans clowertrans;
  guint32 crtp_port0;
  guint32 crtp_port1;
  gint tcpsock;
  gchar *sessionID;
#ifdef USE_HDCP
  void *hdcp_handle;
#endif
  WFDHDCPProtection hdcp_version;
  guint32 hdcp_tcpport;
  gboolean hdcp_support;
  gint hdcp_enabled;
  gint display_rotate;
  UIBC *uibc;
  gboolean finalize_call;
  gboolean edid_supported;
  guint32 edid_hres;
  guint32 edid_vres;
#ifdef STANDBY_RESUME_CAPABILITY
  gboolean standby_resume_capability_support;
#endif
  gboolean waiting_teardown;
  guint teardown_timerid;
  gboolean keep_alive_flag;
  GMutex *keep_alive_lock;
  GstStructure *mpeg_ts_pid;
  GstPad *rtcp_udpsrc_pad;
  guint rtcp_pad_handle_id;
  guint consecutive_low_bitrate_count;
  GstClockTime prev_noti_time;
  guint prev_fraction_lost;
  guint32 prev_max_packet_lost;
  guint16 prev_max_seq_num;
  gboolean first_rtcp;
  gint timer_count;

#ifdef ENABLE_WFD_EXTENDED_FEATURES
  guint packets_resend;
  GstRTSPWatch      *datawatch;
  guint              datawatchid;
  GstRTSPConnection *data_conn;
  gchar     *uristr;
  gchar *user_agent;
  guint eos;

  /* For T1 */
  gboolean enable_T1_switching;
  WFDT1Mode T1_mode;
  gboolean B_mode_requested;
  WFDBModeParams *B_mode_params;
  GCond *B_mode_cond;
  GMutex *B_mode_cond_lock;
  gboolean B_mode_eos;
  guint decide_tcp_bitrate[21];
  guint32     tcp_frag_num;
  TCP_Info *tcp_var;
  guint vconf_web_video_state;
  guint32 socket_buffer_size;
  guint32 socket_buffer_size_left;
  guint scp_state;
  guint vconf_state;

  /* For T2 */
  gboolean sent_T2_E_msg;
  gdouble volume;

  /* For T3 */
  guint64 prev_aud_pts;
  gboolean T3_message_supported;

  /* For U1 */
  gchar *upgrade_version;
  gchar *upgrade_URL;

  /* For U2 */
  gchar *rename_dongle;


  gboolean enable_spec_features;
  void *ext_handle;
  gboolean extended_feature_support;
#endif
};

struct _GstRTSPClientClass {
  GObjectClass  parent_class;

  /* signals */
  void     (*closed)        (GstRTSPClient *client);
  void     (*on_error)      (GstRTSPClient *client);
  void     (*on_teardown)      (GstRTSPClient *client);
};

GType                 gst_rtsp_client_get_type          (void);

GstRTSPClient *       gst_rtsp_client_new               (void);

void                  gst_rtsp_client_set_server        (GstRTSPClient * client, GstRTSPServer * server);
GstRTSPServer *       gst_rtsp_client_get_server        (GstRTSPClient * client);

void                  gst_rtsp_client_set_session_pool  (GstRTSPClient *client,
                                                         GstRTSPSessionPool *pool);
GstRTSPSessionPool *  gst_rtsp_client_get_session_pool  (GstRTSPClient *client);

void                  gst_rtsp_client_set_media_mapping (GstRTSPClient *client,
                                                         GstRTSPMediaMapping *mapping);
GstRTSPMediaMapping * gst_rtsp_client_get_media_mapping (GstRTSPClient *client);

void                  gst_rtsp_client_set_auth          (GstRTSPClient *client, GstRTSPAuth *auth);
GstRTSPAuth *         gst_rtsp_client_get_auth          (GstRTSPClient *client);

void                  gst_rtsp_client_set_connection_mode      (GstRTSPClient *client, gint mode);

gboolean
gst_rtsp_client_accept (GstRTSPClient * client, GIOChannel * channel, GSource *source);

void send_response (GstRTSPClient * client, GstRTSPSession * session,
    GstRTSPMessage * response);

void client_emit_signal(GstRTSPClient * client, int signo);
void unlink_session_streams (GstRTSPClient * client, GstRTSPSession * session,
    GstRTSPSessionMedia * media);
void client_watch_session (GstRTSPClient * client, GstRTSPSession * session);
void do_keepalive (GstRTSPSession * session);
void send_generic_response (GstRTSPClient * client, GstRTSPStatusCode code,
    GstRTSPClientState * state);
GstRTSPResult message_received (GstRTSPWatch * watch, GstRTSPMessage * message,
    gpointer user_data);
GstRTSPResult message_sent (GstRTSPWatch * watch, guint cseq, gpointer user_data);
GstRTSPResult error (GstRTSPWatch * watch, GstRTSPResult result, gpointer user_data);

void send_request (GstRTSPClient * client, GstRTSPSession * session, GstRTSPMessage * request);
GstRTSPResult prepare_request (GstRTSPClient *client, GstRTSPMessage *request,
  GstRTSPMethod method, gchar *url, WFDMessageType message_type, WFDTrigger trigger_type);
GstRTSPResult prepare_response (GstRTSPClient *client, GstRTSPMessage *request, GstRTSPMessage *response, GstRTSPMethod method);

G_END_DECLS

#endif /* __GST_RTSP_CLIENT_H__ */
