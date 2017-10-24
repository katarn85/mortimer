/* GStreamer
 * Copyright (C) <2007> Wim Taymans <wim.taymans@gmail.com>
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

#include <string.h>

#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtcpbuffer.h>
#include <gst/netbuffer/gstnetbuffer.h>

#include <gst/glib-compat-private.h>

#include "gstrtpbin-marshal.h"
#include "rtpsession.h"

GST_DEBUG_CATEGORY_STATIC (rtp_session_debug);
#define GST_CAT_DEFAULT rtp_session_debug
#define INVALID_OR_RESET_WITH_MINUS_ONE -1
#define ZERO 0
#define POINT_FIVE 0.5
#define PLUS_ONE 1
#define ONE_POINT_FIVE 1.5
#define TWO 2
#define FOUR 4
#define FIVE 5
#define EIGHT 8
#define TEN 10
#define DEFAULT_AVG_RTCP_PACKET_SIZE 100
#define TWO_FIFTY_FIVE 255
#define SEND_RTCP_PLIFIR_MAX_DEFAULT_TIME 200

/* signals and args */
enum
{
  SIGNAL_GET_SOURCE_BY_SSRC,
  SIGNAL_ON_NEW_SSRC,
  SIGNAL_ON_SSRC_COLLISION,
  SIGNAL_ON_SSRC_VALIDATED,
  SIGNAL_ON_SSRC_ACTIVE,
  SIGNAL_ON_SSRC_SDES,
  SIGNAL_ON_BYE_SSRC,
  SIGNAL_ON_BYE_TIMEOUT,
  SIGNAL_ON_TIMEOUT,
  SIGNAL_ON_SENDER_TIMEOUT,
  SIGNAL_ON_SENDING_RTCP,
  SIGNAL_ON_FEEDBACK_RTCP,
  SIGNAL_SEND_RTCP,
  LAST_SIGNAL
};

#define DEFAULT_INTERNAL_SOURCE      NULL
#define DEFAULT_BANDWIDTH            RTP_STATS_BANDWIDTH
#define DEFAULT_DO_RETRANSMISSION    FALSE   			//for #RET change
#define DEFAULT_RTCP_FRACTION        (RTP_STATS_RTCP_FRACTION * RTP_STATS_BANDWIDTH)
#define DEFAULT_RTCP_RR_BANDWIDTH    -1
#define DEFAULT_RTCP_RS_BANDWIDTH    -1
#define DEFAULT_RTCP_MTU             1400
#define DEFAULT_SDES                 NULL
#define DEFAULT_NUM_SOURCES          0
#define DEFAULT_NUM_ACTIVE_SOURCES   0
#define DEFAULT_SOURCES              NULL
#define DEFAULT_RTCP_MIN_INTERVAL    (RTP_STATS_MIN_INTERVAL * GST_SECOND)
#define DEFAULT_RTCP_FEEDBACK_RETENTION_WINDOW (2 * GST_SECOND)
#define DEFAULT_RTCP_IMMEDIATE_FEEDBACK_THRESHOLD (3)
#define DEFAULT_PROBATION            RTP_DEFAULT_PROBATION //for #RET change

enum
{
  PROP_0,
  PROP_INTERNAL_SSRC,
  PROP_INTERNAL_SOURCE,
  PROP_BANDWIDTH,
  PROP_DO_RETRANSMISSION,			//for #RET change
  PROP_RTCP_FRACTION,
  PROP_RTCP_RR_BANDWIDTH,
  PROP_RTCP_RS_BANDWIDTH,
  PROP_RTCP_MTU,
  PROP_SDES,
  PROP_NUM_SOURCES,
  PROP_NUM_ACTIVE_SOURCES,
  PROP_SOURCES,
  PROP_FAVOR_NEW,
  PROP_RTCP_MIN_INTERVAL,
  PROP_RTCP_FEEDBACK_RETENTION_WINDOW,
  PROP_RTCP_IMMEDIATE_FEEDBACK_THRESHOLD,
  PROP_PROBATION,					//for #RET change
  PROP_LAST
};

/* update average packet size */
#define INIT_AVG(avg, val) \
   (avg) = (val);
#define UPDATE_AVG(avg, val)            \
  if ((avg) == 0)                       \
   (avg) = (val);                       \
  else                                  \
   (avg) = ((val) + (15 * (avg))) >> 4;


/* The number RTCP intervals after which to timeout entries in the
 * collision table
 */
#define RTCP_INTERVAL_COLLISION_TIMEOUT 10

/* GObject vmethods */
static void rtp_session_finalize (GObject * object);
static void rtp_session_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void rtp_session_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

//for #RET change
//static gboolean rtp_session_on_sending_rtcp (RTPSession *sess,
//    GstBuffer * buffer, gboolean early);
/**
	* @fn		rtp_session_send_rtcp
	* @brief
	*
	* @param[in]/[out]	:RTPSession *sess,GstClockTime max_delay
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/
static void rtp_session_send_rtcp (RTPSession *sess,
    GstClockTime max_delay);   //for #RET change


static guint rtp_session_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (RTPSession, rtp_session, G_TYPE_OBJECT);

//for #RET change
/**
	* @fn		rtp_session_create_new_ssrc
	* @brief
	*
	* @param[in]/[out]	:RTPSession *sess
	* @return			: guint32
	* @warning			: None
	* @exception		: None
	* @see
	*/
static guint32 rtp_session_create_new_ssrc (RTPSession *sess);
static RTPSource *obtain_source (RTPSession * sess, guint32 ssrc,
    gboolean * created, RTPArrivalStats * arrival, gboolean rtp);
//for #RET change
/**
	* @fn		obtain_internal_source
	* @brief
	*
	* @param[in]/[out]	:RTPSession *sess,guint32 ssrc, gboolean *created
	* @return			: RTPSource*
	* @warning			: None
	* @exception		: None
	* @see
	*/
static RTPSource* obtain_internal_source (RTPSession *sess,
    guint32 ssrc, gboolean *created);

/**
	* @fn		rtp_session_schedule_bye_locked
	* @brief
	*
	* @param[in]/[out]	: RTPSession *sess,GstClockTime current_time
	* @return			: GstFlowReturn
	* @warning			: None
	* @exception		: None
	* @see
	*/
static GstFlowReturn rtp_session_schedule_bye_locked (RTPSession *sess,
     GstClockTime current_time);
static GstClockTime calculate_rtcp_interval (RTPSession * sess,
    gboolean deterministic, gboolean first);

static gboolean
accumulate_trues (GSignalInvocationHint * ihint, GValue * return_accu,
    const GValue * handler_return, gpointer data)
{
  if (g_value_get_boolean (handler_return))
    g_value_set_boolean (return_accu, TRUE);

  return TRUE;
}

static void
gst_rtp_bin_marshal_BOOLEAN__MINIOBJECT_BOOLEAN (GClosure * closure,
    GValue * return_value G_GNUC_UNUSED, guint n_param_values,
    const GValue * param_values, gpointer invocation_hint G_GNUC_UNUSED,
    gpointer marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__MINIOBJECT_BOOLEAN) (gpointer data1,
      gpointer arg_1, gboolean arg_2, gpointer data2);
  register GMarshalFunc_BOOLEAN__MINIOBJECT_BOOLEAN callback;
  register GCClosure *cc = (GCClosure *) closure;
  register gpointer data1, data2;
  gboolean v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure)) {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
  } else {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
  }
  callback =
      (GMarshalFunc_BOOLEAN__MINIOBJECT_BOOLEAN) (marshal_data ? marshal_data :
      cc->callback);

  v_return = callback (data1,
      gst_value_get_mini_object (param_values + 1),
      g_value_get_boolean (param_values + 2), data2);

  g_value_set_boolean (return_value, v_return);
}

static void
gst_rtp_bin_marshal_VOID__UINT_UINT_UINT_UINT_MINIOBJECT (GClosure * closure,
    GValue * return_value G_GNUC_UNUSED, guint n_param_values,
    const GValue * param_values, gpointer invocation_hint G_GNUC_UNUSED,
    gpointer marshal_data)
{
  typedef void (*GMarshalFunc_VOID__UINT_UINT_UINT_UINT_MINIOBJECT) (gpointer
      data1, guint arg_1, guint arg_2, guint arg_3, guint arg_4, gpointer arg_5,
      gpointer data2);
  register GMarshalFunc_VOID__UINT_UINT_UINT_UINT_MINIOBJECT callback;
  register GCClosure *cc = (GCClosure *) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 6);

  if (G_CCLOSURE_SWAP_DATA (closure)) {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
  } else {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
  }
  callback =
      (GMarshalFunc_VOID__UINT_UINT_UINT_UINT_MINIOBJECT) (marshal_data ?
      marshal_data : cc->callback);

  callback (data1,
      g_value_get_uint (param_values + 1),
      g_value_get_uint (param_values + 2),
      g_value_get_uint (param_values + 3),
      g_value_get_uint (param_values + 4),
      gst_value_get_mini_object (param_values + 5), data2);
}


static void
rtp_session_class_init (RTPSessionClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = rtp_session_finalize;
  gobject_class->set_property = rtp_session_set_property;
  gobject_class->get_property = rtp_session_get_property;

  /**
   * RTPSession::get-source-by-ssrc:
   * @session: the object which received the signal
   * @ssrc: the SSRC of the RTPSource
   *
   * Request the #RTPSource object with SSRC @ssrc in @session.
   */
  rtp_session_signals[SIGNAL_GET_SOURCE_BY_SSRC] =
      g_signal_new ("get-source-by-ssrc", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, G_STRUCT_OFFSET (RTPSessionClass,
          get_source_by_ssrc), NULL, NULL, gst_rtp_bin_marshal_OBJECT__UINT,
      RTP_TYPE_SOURCE, 1, G_TYPE_UINT);

  /**
   * RTPSession::on-new-ssrc:
   * @session: the object which received the signal
   * @src: the new RTPSource
   *
   * Notify of a new SSRC that entered @session.
   */
  rtp_session_signals[SIGNAL_ON_NEW_SSRC] =
      g_signal_new ("on-new-ssrc", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (RTPSessionClass, on_new_ssrc),
      NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
      RTP_TYPE_SOURCE);
  /**
   * RTPSession::on-ssrc-collision:
   * @session: the object which received the signal
   * @src: the #RTPSource that caused a collision
   *
   * Notify when we have an SSRC collision
   */
  rtp_session_signals[SIGNAL_ON_SSRC_COLLISION] =
      g_signal_new ("on-ssrc-collision", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (RTPSessionClass, on_ssrc_collision),
      NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
      RTP_TYPE_SOURCE);
  /**
   * RTPSession::on-ssrc-validated:
   * @session: the object which received the signal
   * @src: the new validated RTPSource
   *
   * Notify of a new SSRC that became validated.
   */
  rtp_session_signals[SIGNAL_ON_SSRC_VALIDATED] =
      g_signal_new ("on-ssrc-validated", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (RTPSessionClass, on_ssrc_validated),
      NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
      RTP_TYPE_SOURCE);
  /**
   * RTPSession::on-ssrc-active:
   * @session: the object which received the signal
   * @src: the active RTPSource
   *
   * Notify of a SSRC that is active, i.e., sending RTCP.
   */
  rtp_session_signals[SIGNAL_ON_SSRC_ACTIVE] =
      g_signal_new ("on-ssrc-active", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (RTPSessionClass, on_ssrc_active),
      NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
      RTP_TYPE_SOURCE);
  /**
   * RTPSession::on-ssrc-sdes:
   * @session: the object which received the signal
   * @src: the RTPSource
   *
   * Notify that a new SDES was received for SSRC.
   */
  rtp_session_signals[SIGNAL_ON_SSRC_SDES] =
      g_signal_new ("on-ssrc-sdes", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (RTPSessionClass, on_ssrc_sdes),
      NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
      RTP_TYPE_SOURCE);
  /**
   * RTPSession::on-bye-ssrc:
   * @session: the object which received the signal
   * @src: the RTPSource that went away
   *
   * Notify of an SSRC that became inactive because of a BYE packet.
   */
  rtp_session_signals[SIGNAL_ON_BYE_SSRC] =
      g_signal_new ("on-bye-ssrc", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (RTPSessionClass, on_bye_ssrc),
      NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
      RTP_TYPE_SOURCE);
  /**
   * RTPSession::on-bye-timeout:
   * @session: the object which received the signal
   * @src: the RTPSource that timed out
   *
   * Notify of an SSRC that has timed out because of BYE
   */
  rtp_session_signals[SIGNAL_ON_BYE_TIMEOUT] =
      g_signal_new ("on-bye-timeout", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (RTPSessionClass, on_bye_timeout),
      NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
      RTP_TYPE_SOURCE);
  /**
   * RTPSession::on-timeout:
   * @session: the object which received the signal
   * @src: the RTPSource that timed out
   *
   * Notify of an SSRC that has timed out
   */
  rtp_session_signals[SIGNAL_ON_TIMEOUT] =
      g_signal_new ("on-timeout", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (RTPSessionClass, on_timeout),
      NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
      RTP_TYPE_SOURCE);
  /**
   * RTPSession::on-sender-timeout:
   * @session: the object which received the signal
   * @src: the RTPSource that timed out
   *
   * Notify of an SSRC that was a sender but timed out and became a receiver.
   */
  rtp_session_signals[SIGNAL_ON_SENDER_TIMEOUT] =
      g_signal_new ("on-sender-timeout", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (RTPSessionClass, on_sender_timeout),
      NULL, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
      RTP_TYPE_SOURCE);

  /**
   * RTPSession::on-sending-rtcp
   * @session: the object which received the signal
   * @buffer: the #GstBuffer containing the RTCP packet about to be sent
   * @early: %TRUE if the packet is early, %FALSE if it is regular
   *
   * This signal is emitted before sending an RTCP packet, it can be used
   * to add extra RTCP Packets.
   *
   * Returns: %TRUE if the RTCP buffer should NOT be suppressed, %FALSE
   * if suppressing it is acceptable
   */
  rtp_session_signals[SIGNAL_ON_SENDING_RTCP] =
      g_signal_new ("on-sending-rtcp", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (RTPSessionClass, on_sending_rtcp),
      accumulate_trues, NULL, gst_rtp_bin_marshal_BOOLEAN__MINIOBJECT_BOOLEAN,
      G_TYPE_BOOLEAN, 2, GST_TYPE_BUFFER, G_TYPE_BOOLEAN);

  /**
   * RTPSession::on-feedback-rtcp:
   * @session: the object which received the signal
   * @type: Type of RTCP packet, will be %GST_RTCP_TYPE_RTPFB or
   *  %GST_RTCP_TYPE_RTPFB
   * @fbtype: The type of RTCP FB packet, probably part of #GstRTCPFBType
   * @sender_ssrc: The SSRC of the sender
   * @media_ssrc: The SSRC of the media this refers to
   * @fci: a #GstBuffer with the FCI data from the FB packet or %NULL if
   * there was no FCI
   *
   * Notify that a RTCP feedback packet has been received
   */

  rtp_session_signals[SIGNAL_ON_FEEDBACK_RTCP] =
      g_signal_new ("on-feedback-rtcp", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (RTPSessionClass, on_feedback_rtcp),
      NULL, NULL, gst_rtp_bin_marshal_VOID__UINT_UINT_UINT_UINT_MINIOBJECT,
      G_TYPE_NONE, 5, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT,
      GST_TYPE_BUFFER);

  /**
   * RTPSession::send-rtcp:
   * @session: the object which received the signal
   * @max_delay: The maximum delay after which the feedback will not be useful
   *  anymore
   *
   * Requests that the #RTPSession initiate a new RTCP packet as soon as
   * possible within the requested delay.
   */

  rtp_session_signals[SIGNAL_SEND_RTCP] =
      g_signal_new ("send-rtcp", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (RTPSessionClass, send_rtcp), NULL, NULL,
      gst_rtp_bin_marshal_VOID__UINT64, G_TYPE_NONE, 1, G_TYPE_UINT64);

  g_object_class_install_property (gobject_class, PROP_INTERNAL_SSRC,
      g_param_spec_uint ("internal-ssrc", "Internal SSRC",
          "The internal SSRC used for the session",
          0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INTERNAL_SOURCE,
      g_param_spec_object ("internal-source", "Internal Source",
          "The internal source element of the session",
          RTP_TYPE_SOURCE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BANDWIDTH,
      g_param_spec_double ("bandwidth", "Bandwidth",
          "The bandwidth of the session (0 for auto-discover)",
          0.0, G_MAXDOUBLE, DEFAULT_BANDWIDTH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
//for RET change
  g_object_class_install_property (gobject_class, PROP_DO_RETRANSMISSION,
      g_param_spec_boolean ("do-retransmission", "Do retransmission",
          "Send an event downstream to request packet retransmission",
          DEFAULT_DO_RETRANSMISSION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_RTCP_FRACTION,
      g_param_spec_double ("rtcp-fraction", "RTCP Fraction",
          "The fraction of the bandwidth used for RTCP (or as a real fraction of the RTP bandwidth if < 1)",
          0.0, G_MAXDOUBLE, DEFAULT_RTCP_FRACTION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RTCP_RR_BANDWIDTH,
      g_param_spec_int ("rtcp-rr-bandwidth", "RTCP RR bandwidth",
          "The RTCP bandwidth used for receivers in bytes per second (-1 = default)",
          -1, G_MAXINT, DEFAULT_RTCP_RR_BANDWIDTH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RTCP_RS_BANDWIDTH,
      g_param_spec_int ("rtcp-rs-bandwidth", "RTCP RS bandwidth",
          "The RTCP bandwidth used for senders in bytes per second (-1 = default)",
          -1, G_MAXINT, DEFAULT_RTCP_RS_BANDWIDTH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RTCP_MTU,
      g_param_spec_uint ("rtcp-mtu", "RTCP MTU",
          "The maximum size of the RTCP packets",
          16, G_MAXINT16, DEFAULT_RTCP_MTU,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SDES,
      g_param_spec_boxed ("sdes", "SDES",
          "The SDES items of this session",
          GST_TYPE_STRUCTURE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_SOURCES,
      g_param_spec_uint ("num-sources", "Num Sources",
          "The number of sources in the session", 0, G_MAXUINT,
          DEFAULT_NUM_SOURCES, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_ACTIVE_SOURCES,
      g_param_spec_uint ("num-active-sources", "Num Active Sources",
          "The number of active sources in the session", 0, G_MAXUINT,
          DEFAULT_NUM_ACTIVE_SOURCES,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  /**
   * RTPSource::sources
   *
   * Get a GValue Array of all sources in the session.
   *
   * <example>
   * <title>Getting the #RTPSources of a session
   * <programlisting>
   * {
   *   GValueArray *arr;
   *   GValue *val;
   *   guint i;
   *
   *   g_object_get (sess, "sources", &arr, NULL);
   *
   *   for (i = 0; i < arr->n_values; i++) {
   *     RTPSource *source;
   *
   *     val = g_value_array_get_nth (arr, i);
   *     source = g_value_get_object (val);
   *   }
   *   g_value_array_free (arr);
   * }
   * </programlisting>
   * </example>
   */
  g_object_class_install_property (gobject_class, PROP_SOURCES,
      g_param_spec_boxed ("sources", "Sources",
          "An array of all known sources in the session",
          G_TYPE_VALUE_ARRAY, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FAVOR_NEW,
      g_param_spec_boolean ("favor-new", "Favor new sources",
          "Resolve SSRC conflict in favor of new sources", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RTCP_MIN_INTERVAL,
      g_param_spec_uint64 ("rtcp-min-interval", "Minimum RTCP interval",
          "Minimum interval between Regular RTCP packet (in ns)",
          0, G_MAXUINT64, DEFAULT_RTCP_MIN_INTERVAL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_RTCP_FEEDBACK_RETENTION_WINDOW,
      g_param_spec_uint64 ("rtcp-feedback-retention-window",
          "RTCP Feedback retention window",
          "Duration during which RTCP Feedback packets are retained (in ns)",
          0, G_MAXUINT64, DEFAULT_RTCP_FEEDBACK_RETENTION_WINDOW,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_RTCP_IMMEDIATE_FEEDBACK_THRESHOLD,
      g_param_spec_uint ("rtcp-immediate-feedback-threshold",
          "RTCP Immediate Feedback threshold",
          "The maximum number of members of a RTP session for which immediate"
          " feedback is used",
          0, G_MAXUINT, DEFAULT_RTCP_IMMEDIATE_FEEDBACK_THRESHOLD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

//for #RET change
  g_object_class_install_property (gobject_class, PROP_PROBATION,
      g_param_spec_uint ("probation", "Number of probations",
          "Consecutive packet sequence numbers to accept the source",
		ZERO, G_MAXUINT, DEFAULT_PROBATION,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  klass->get_source_by_ssrc =
      GST_DEBUG_FUNCPTR (rtp_session_get_source_by_ssrc);
//for #RET change  klass->on_sending_rtcp = GST_DEBUG_FUNCPTR (rtp_session_on_sending_rtcp);
  klass->send_rtcp = GST_DEBUG_FUNCPTR (rtp_session_send_rtcp);

  GST_DEBUG_CATEGORY_INIT (rtp_session_debug, "rtpsession", 0, "RTP Session");
}

static void
rtp_session_init (RTPSession * sess)
{
  gint i;
  gchar *str;

  sess->lock = g_mutex_new ();
  sess->key = g_random_int ();
  sess->mask_idx = 0;
  sess->mask = 0;
//for #RET change
  sess->main_ssrc_pt = INVALID_OR_RESET_WITH_MINUS_ONE;
  sess->main_ssrc=ZERO;
  sess->rtx_ssrc=ZERO;
  for (i = 0; i < 32; i++) {
    sess->ssrcs[i] =
        g_hash_table_new_full (NULL, NULL, NULL,
        (GDestroyNotify) g_object_unref);
  }
//for #RET change  sess->cnames = g_hash_table_new_full (NULL, NULL, g_free, NULL);

  rtp_stats_init_defaults (&sess->stats);

//for #RET change
  INIT_AVG (sess->stats.avg_rtcp_packet_size, DEFAULT_AVG_RTCP_PACKET_SIZE);
  rtp_stats_set_min_interval (&sess->stats,
      (gdouble) DEFAULT_RTCP_MIN_INTERVAL / GST_SECOND);
  sess->recalc_bandwidth = TRUE;
  sess->bandwidth = DEFAULT_BANDWIDTH;
  sess->do_retransmission = DEFAULT_DO_RETRANSMISSION;  //for #RET change
  sess->rtcp_bandwidth = DEFAULT_RTCP_FRACTION;
  sess->rtcp_rr_bandwidth = DEFAULT_RTCP_RR_BANDWIDTH;
  sess->rtcp_rs_bandwidth = DEFAULT_RTCP_RS_BANDWIDTH;

  /* create an active SSRC for this session manager */
/*  sess->source = rtp_session_create_source (sess);
  sess->source->validated = TRUE;
  sess->source->internal = TRUE;
  sess->stats.active_sources++;
  INIT_AVG (sess->stats.avg_rtcp_packet_size, 100);
  sess->source->stats.prev_rtcptime = 0;
  sess->source->stats.last_rtcptime = 1;

  rtp_stats_set_min_interval (&sess->stats,
      (gdouble) DEFAULT_RTCP_MIN_INTERVAL / GST_SECOND);
*/
  /* default UDP header length */
  sess->header_len = 28;
  sess->mtu = DEFAULT_RTCP_MTU;

  sess->probation = DEFAULT_PROBATION; //for #RET change
  /* some default SDES entries */
  sess->sdes = gst_structure_new ("application/x-rtp-source-sdes",NULL); //for #RET change

  /* we do not want to leak details like the username or hostname here */
  str = g_strdup_printf ("user%u@host-%x", g_random_int (), g_random_int ());
  gst_structure_set (sess->sdes, "cname", G_TYPE_STRING, str, NULL);
  g_free (str);

#if 0
  /* we do not want to leak the user's real name here */
  str = g_strdup_printf ("Anon%u", g_random_int ());
  rtp_source_set_sdes_string (sess->source, GST_RTCP_SDES_NAME, str);
  g_free (str);
#endif

//for #RET change
   //rtp_source_set_sdes_string (sess->source, GST_RTCP_SDES_TOOL, "GStreamer");
   gst_structure_set (sess->sdes, "tool", G_TYPE_STRING, "GStreamer", NULL);
  sess->suggested_ssrc = rtp_session_create_new_ssrc (sess);

  sess->first_rtcp = TRUE;
  sess->next_rtcp_check_time = GST_CLOCK_TIME_NONE;  //for #RET change
  sess->allow_early = TRUE;
  sess->next_early_rtcp_time = GST_CLOCK_TIME_NONE;  //for #RET change
  sess->rtcp_feedback_retention_window = DEFAULT_RTCP_FEEDBACK_RETENTION_WINDOW;
  sess->rtcp_immediate_feedback_threshold =
      DEFAULT_RTCP_IMMEDIATE_FEEDBACK_THRESHOLD;

  sess->last_keyframe_request = GST_CLOCK_TIME_NONE;

  GST_DEBUG ("%p: session using SSRC: %08x", sess, sess->suggested_ssrc);
}

static void
rtp_session_finalize (GObject * object)
{
  RTPSession *sess;
  gint i;

  sess = RTP_SESSION_CAST (object);
  gst_structure_free (sess->sdes);
	for (i = 0; i < 32; i++)
	{
    	g_hash_table_destroy (sess->ssrcs[i]);
	}

  g_mutex_free (sess->lock);
  G_OBJECT_CLASS (rtp_session_parent_class)->finalize (object);
}

static void
copy_source (gpointer key, RTPSource * source, GValueArray * arr)
{
  GValue value = { 0 };

  g_value_init (&value, RTP_TYPE_SOURCE);
  g_value_take_object (&value, source);
  /* copies the value */
  g_value_array_append (arr, &value);
}

static GValueArray *
rtp_session_create_sources (RTPSession * sess)
{
  GValueArray *res;
  guint size;

  RTP_SESSION_LOCK (sess);
  /* get number of elements in the table */
  size = g_hash_table_size (sess->ssrcs[sess->mask_idx]);
  /* create the result value array */
  res = g_value_array_new (size);

  /* and copy all values into the array */
  g_hash_table_foreach (sess->ssrcs[sess->mask_idx], (GHFunc) copy_source, res);
  RTP_SESSION_UNLOCK (sess);

  return res;
}

static void
rtp_session_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  RTPSession *sess;

  sess = RTP_SESSION (object);
//editted for #RET change
  switch (prop_id)
  {
	case PROP_INTERNAL_SSRC:
		//rtp_session_set_internal_ssrc (sess, g_value_get_uint (value));
		break;
	case PROP_BANDWIDTH:
		RTP_SESSION_LOCK (sess);
		sess->bandwidth = g_value_get_double (value);
		sess->recalc_bandwidth = TRUE;
		RTP_SESSION_UNLOCK (sess);
		break;
	case PROP_DO_RETRANSMISSION:
		RTP_SESSION_LOCK (sess);
		sess->do_retransmission = g_value_get_boolean (value);
		RTP_SESSION_UNLOCK (sess);
		break;
	case PROP_RTCP_FRACTION:
		RTP_SESSION_LOCK (sess);
		sess->rtcp_bandwidth = g_value_get_double (value);
		sess->recalc_bandwidth = TRUE;
		RTP_SESSION_UNLOCK (sess);
		break;
	case PROP_RTCP_RR_BANDWIDTH:
		RTP_SESSION_LOCK (sess);
		sess->rtcp_rr_bandwidth = g_value_get_int (value);
		sess->recalc_bandwidth = TRUE;
		RTP_SESSION_UNLOCK (sess);
		break;
	case PROP_RTCP_RS_BANDWIDTH:
		RTP_SESSION_LOCK (sess);
		sess->rtcp_rs_bandwidth = g_value_get_int (value);
		sess->recalc_bandwidth = TRUE;
		RTP_SESSION_UNLOCK (sess);
		break;
	case PROP_RTCP_MTU:
		sess->mtu = g_value_get_uint (value);
		break;
	case PROP_SDES:
		rtp_session_set_sdes_struct (sess, g_value_get_boxed (value));
		break;
	case PROP_FAVOR_NEW:
		sess->favor_new = g_value_get_boolean (value);
		break;
	case PROP_RTCP_MIN_INTERVAL:
		rtp_stats_set_min_interval (&sess->stats,
		(gdouble) g_value_get_uint64 (value) / GST_SECOND);
		/* trigger reconsideration */
		RTP_SESSION_LOCK (sess);
		sess->next_rtcp_check_time = 0;
		RTP_SESSION_UNLOCK (sess);
		if (sess->callbacks.reconsider)
		{
			sess->callbacks.reconsider (sess, sess->reconsider_user_data);
		}
		break;
	case PROP_RTCP_IMMEDIATE_FEEDBACK_THRESHOLD:
		sess->rtcp_immediate_feedback_threshold = g_value_get_uint (value);
		break;
	case PROP_PROBATION:
		sess->probation = g_value_get_uint (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
  }
}

static void
rtp_session_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  RTPSession *sess;

  sess = RTP_SESSION (object);

  switch (prop_id)
  {
	case PROP_INTERNAL_SSRC:
		g_value_set_uint (value, rtp_session_suggest_ssrc (sess)); //for #RET change
		break;
	case PROP_INTERNAL_SOURCE:
		//g_value_take_object (value, rtp_session_get_internal_source (sess));
		/* FIXME, return a random source */
		g_value_set_object (value, NULL);
		break;
	case PROP_BANDWIDTH:
		g_value_set_double (value, sess->bandwidth);
		break;
	case PROP_DO_RETRANSMISSION:   //for #RET change
		g_value_set_boolean (value, sess->do_retransmission);
		break;
	case PROP_RTCP_FRACTION:
		g_value_set_double (value, sess->rtcp_bandwidth);
		break;
	case PROP_RTCP_RR_BANDWIDTH:
		g_value_set_int (value, sess->rtcp_rr_bandwidth);
		break;
	case PROP_RTCP_RS_BANDWIDTH:
		g_value_set_int (value, sess->rtcp_rs_bandwidth);
		break;
	case PROP_RTCP_MTU:
		g_value_set_uint (value, sess->mtu);
		break;
	case PROP_SDES:
		g_value_take_boxed (value, rtp_session_get_sdes_struct (sess));
		break;
	case PROP_NUM_SOURCES:
		g_value_set_uint (value, rtp_session_get_num_sources (sess));
		break;
	case PROP_NUM_ACTIVE_SOURCES:
		g_value_set_uint (value, rtp_session_get_num_active_sources (sess));
		break;
	case PROP_SOURCES:
		g_value_take_boxed (value, rtp_session_create_sources (sess));
		break;
	case PROP_FAVOR_NEW:
		g_value_set_boolean (value, sess->favor_new);
		break;
	case PROP_RTCP_MIN_INTERVAL:
		g_value_set_uint64 (value, sess->stats.min_interval * GST_SECOND);
		break;
	case PROP_RTCP_IMMEDIATE_FEEDBACK_THRESHOLD:
		g_value_set_uint (value, sess->rtcp_immediate_feedback_threshold);
		break;
	case PROP_PROBATION:  //for #RET change
		g_value_set_uint (value, sess->probation);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
  }
}

static void
on_new_ssrc (RTPSession * sess, RTPSource * source)
{
  g_object_ref (source);
  RTP_SESSION_UNLOCK (sess);
  g_signal_emit (sess, rtp_session_signals[SIGNAL_ON_NEW_SSRC], 0, source);
  RTP_SESSION_LOCK (sess);
  g_object_unref (source);
}

static void
on_ssrc_collision (RTPSession * sess, RTPSource * source)
{
  g_object_ref (source);
  RTP_SESSION_UNLOCK (sess);
  g_signal_emit (sess, rtp_session_signals[SIGNAL_ON_SSRC_COLLISION], 0,
      source);
  RTP_SESSION_LOCK (sess);
  g_object_unref (source);
}

static void
on_ssrc_validated (RTPSession * sess, RTPSource * source)
{
  g_object_ref (source);
  RTP_SESSION_UNLOCK (sess);
  g_signal_emit (sess, rtp_session_signals[SIGNAL_ON_SSRC_VALIDATED], 0,
      source);
  RTP_SESSION_LOCK (sess);
  g_object_unref (source);
}

static void
on_ssrc_active (RTPSession * sess, RTPSource * source)
{
  g_object_ref (source);
  RTP_SESSION_UNLOCK (sess);
  g_signal_emit (sess, rtp_session_signals[SIGNAL_ON_SSRC_ACTIVE], 0, source);
  RTP_SESSION_LOCK (sess);
  g_object_unref (source);
}

static void
on_ssrc_sdes (RTPSession * sess, RTPSource * source)
{
  g_object_ref (source);
  GST_DEBUG ("SDES changed for SSRC %08x", source->ssrc);
  RTP_SESSION_UNLOCK (sess);
  g_signal_emit (sess, rtp_session_signals[SIGNAL_ON_SSRC_SDES], 0, source);
  RTP_SESSION_LOCK (sess);
  g_object_unref (source);
}

static void
on_bye_ssrc (RTPSession * sess, RTPSource * source)
{
  g_object_ref (source);
  RTP_SESSION_UNLOCK (sess);
  g_signal_emit (sess, rtp_session_signals[SIGNAL_ON_BYE_SSRC], 0, source);
  RTP_SESSION_LOCK (sess);
  g_object_unref (source);
}

static void
on_bye_timeout (RTPSession * sess, RTPSource * source)
{
  g_object_ref (source);
  RTP_SESSION_UNLOCK (sess);
  g_signal_emit (sess, rtp_session_signals[SIGNAL_ON_BYE_TIMEOUT], 0, source);
  RTP_SESSION_LOCK (sess);
  g_object_unref (source);
}

static void
on_timeout (RTPSession * sess, RTPSource * source)
{
  g_object_ref (source);
  RTP_SESSION_UNLOCK (sess);
  g_signal_emit (sess, rtp_session_signals[SIGNAL_ON_TIMEOUT], 0, source);
  RTP_SESSION_LOCK (sess);
  g_object_unref (source);
}

static void
on_sender_timeout (RTPSession * sess, RTPSource * source)
{
  g_object_ref (source);
  RTP_SESSION_UNLOCK (sess);
  g_signal_emit (sess, rtp_session_signals[SIGNAL_ON_SENDER_TIMEOUT], 0,
      source);
  RTP_SESSION_LOCK (sess);
  g_object_unref (source);
}

/**
 * rtp_session_new:
 *
 * Create a new session object.
 *
 * Returns: a new #RTPSession. g_object_unref() after usage.
 */
RTPSession *
rtp_session_new (void)
{
  RTPSession *sess;

  sess = g_object_new (RTP_TYPE_SESSION, NULL);

  return sess;
}

/**
 * rtp_session_set_callbacks:
 * @sess: an #RTPSession
 * @callbacks: callbacks to configure
 * @user_data: user data passed in the callbacks
 *
 * Configure a set of callbacks to be notified of actions.
 */
void
rtp_session_set_callbacks (RTPSession * sess, RTPSessionCallbacks * callbacks,
    gpointer user_data)
{
  g_return_if_fail (RTP_IS_SESSION (sess));

  if (callbacks->process_rtp) {
    sess->callbacks.process_rtp = callbacks->process_rtp;
    sess->process_rtp_user_data = user_data;
  }
  if (callbacks->send_rtp) {
    sess->callbacks.send_rtp = callbacks->send_rtp;
    sess->send_rtp_user_data = user_data;
  }
  if (callbacks->send_rtcp) {
    sess->callbacks.send_rtcp = callbacks->send_rtcp;
    sess->send_rtcp_user_data = user_data;
  }
  if (callbacks->sync_rtcp) {
    sess->callbacks.sync_rtcp = callbacks->sync_rtcp;
    sess->sync_rtcp_user_data = user_data;
  }
  if (callbacks->clock_rate) {
    sess->callbacks.clock_rate = callbacks->clock_rate;
    sess->clock_rate_user_data = user_data;
  }
	if (callbacks->rtx_src)
	{
		//for #RET change
		sess->callbacks.rtx_src = callbacks->rtx_src;
		sess->rtx_src_user_data = user_data;
	}
  if (callbacks->reconsider) {
    sess->callbacks.reconsider = callbacks->reconsider;
    sess->reconsider_user_data = user_data;
  }
  if (callbacks->request_key_unit) {
    sess->callbacks.request_key_unit = callbacks->request_key_unit;
    sess->request_key_unit_user_data = user_data;
  }
	if (callbacks->request_time)
	{
		sess->callbacks.request_time = callbacks->request_time;
		sess->request_time_user_data = user_data;
	}
	if (callbacks->notify_nack)
	{
		//for #NACK support
	    sess->callbacks.notify_nack = callbacks->notify_nack;
    	sess->notify_nack_user_data = user_data;
  	}
}

/**
 * rtp_session_set_process_rtp_callback:
 * @sess: an #RTPSession
 * @callback: callback to set
 * @user_data: user data passed in the callback
 *
 * Configure only the process_rtp callback to be notified of the process_rtp action.
 */
void
rtp_session_set_process_rtp_callback (RTPSession * sess,
    RTPSessionProcessRTP callback, gpointer user_data)
{
  g_return_if_fail (RTP_IS_SESSION (sess));

  sess->callbacks.process_rtp = callback;
  sess->process_rtp_user_data = user_data;
}

/**
 * rtp_session_set_send_rtp_callback:
 * @sess: an #RTPSession
 * @callback: callback to set
 * @user_data: user data passed in the callback
 *
 * Configure only the send_rtp callback to be notified of the send_rtp action.
 */
void
rtp_session_set_send_rtp_callback (RTPSession * sess,
    RTPSessionSendRTP callback, gpointer user_data)
{
  g_return_if_fail (RTP_IS_SESSION (sess));

  sess->callbacks.send_rtp = callback;
  sess->send_rtp_user_data = user_data;
}

/**
 * rtp_session_set_send_rtcp_callback:
 * @sess: an #RTPSession
 * @callback: callback to set
 * @user_data: user data passed in the callback
 *
 * Configure only the send_rtcp callback to be notified of the send_rtcp action.
 */
void
rtp_session_set_send_rtcp_callback (RTPSession * sess,
    RTPSessionSendRTCP callback, gpointer user_data)
{
  g_return_if_fail (RTP_IS_SESSION (sess));

  sess->callbacks.send_rtcp = callback;
  sess->send_rtcp_user_data = user_data;
}

/**
 * rtp_session_set_sync_rtcp_callback:
 * @sess: an #RTPSession
 * @callback: callback to set
 * @user_data: user data passed in the callback
 *
 * Configure only the sync_rtcp callback to be notified of the sync_rtcp action.
 */
void
rtp_session_set_sync_rtcp_callback (RTPSession * sess,
    RTPSessionSyncRTCP callback, gpointer user_data)
{
  g_return_if_fail (RTP_IS_SESSION (sess));

  sess->callbacks.sync_rtcp = callback;
  sess->sync_rtcp_user_data = user_data;
}

/**
 * rtp_session_set_clock_rate_callback:
 * @sess: an #RTPSession
 * @callback: callback to set
 * @user_data: user data passed in the callback
 *
 * Configure only the clock_rate callback to be notified of the clock_rate action.
 */
void
rtp_session_set_clock_rate_callback (RTPSession * sess,
    RTPSessionClockRate callback, gpointer user_data)
{
  g_return_if_fail (RTP_IS_SESSION (sess));

  sess->callbacks.clock_rate = callback;
  sess->clock_rate_user_data = user_data;
}
//for #RET change
/**
	* @fn		rtp_session_set_rtx_src_callback
	* @brief
	*
	* @param[in]/[out]	:RTPSession *sess,RTPSessionClockRate callback, gpointer user_data
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/
void rtp_session_set_rtx_src_callback (RTPSession *sess,RTPSessionClockRate callback, gpointer user_data)
{
  g_return_if_fail (RTP_IS_SESSION (sess));
  sess->callbacks.rtx_src = callback;
  sess->rtx_src_user_data = user_data;
}

/**
 * rtp_session_set_reconsider_callback:
 * @sess: an #RTPSession
 * @callback: callback to set
 * @user_data: user data passed in the callback
 *
 * Configure only the reconsider callback to be notified of the reconsider action.
 */
void
rtp_session_set_reconsider_callback (RTPSession * sess,
    RTPSessionReconsider callback, gpointer user_data)
{
  g_return_if_fail (RTP_IS_SESSION (sess));

  sess->callbacks.reconsider = callback;
  sess->reconsider_user_data = user_data;
}

/**
 * rtp_session_set_request_time_callback:
 * @sess: an #RTPSession
 * @callback: callback to set
 * @user_data: user data passed in the callback
 *
 * Configure only the request_time callback
 */
void
rtp_session_set_request_time_callback (RTPSession * sess,
    RTPSessionRequestTime callback, gpointer user_data)
{
  g_return_if_fail (RTP_IS_SESSION (sess));

  sess->callbacks.request_time = callback;
  sess->request_time_user_data = user_data;
}

/**
 * rtp_session_set_bandwidth:
 * @sess: an #RTPSession
 * @bandwidth: the bandwidth allocated
 *
 * Set the session bandwidth in bytes per second.
 */
void
rtp_session_set_bandwidth (RTPSession * sess, gdouble bandwidth)
{
  g_return_if_fail (RTP_IS_SESSION (sess));

  RTP_SESSION_LOCK (sess);
  sess->stats.bandwidth = bandwidth;
  RTP_SESSION_UNLOCK (sess);
}

/**
 * rtp_session_get_bandwidth:
 * @sess: an #RTPSession
 *
 * Get the session bandwidth.
 *
 * Returns: the session bandwidth.
 */
gdouble
rtp_session_get_bandwidth (RTPSession * sess)
{
  gdouble result;

  g_return_val_if_fail (RTP_IS_SESSION (sess), 0);

  RTP_SESSION_LOCK (sess);
  result = sess->stats.bandwidth;
  RTP_SESSION_UNLOCK (sess);

  return result;
}

/**
 * rtp_session_set_rtcp_fraction:
 * @sess: an #RTPSession
 * @bandwidth: the RTCP bandwidth
 *
 * Set the bandwidth in bytes per second that should be used for RTCP
 * messages.
 */
void
rtp_session_set_rtcp_fraction (RTPSession * sess, gdouble bandwidth)
{
  g_return_if_fail (RTP_IS_SESSION (sess));

  RTP_SESSION_LOCK (sess);
  sess->stats.rtcp_bandwidth = bandwidth;
  RTP_SESSION_UNLOCK (sess);
}

/**
 * rtp_session_get_rtcp_fraction:
 * @sess: an #RTPSession
 *
 * Get the session bandwidth used for RTCP.
 *
 * Returns: The bandwidth used for RTCP messages.
 */
gdouble
rtp_session_get_rtcp_fraction (RTPSession * sess)
{
  gdouble result;

  g_return_val_if_fail (RTP_IS_SESSION (sess), 0.0);

  RTP_SESSION_LOCK (sess);
  result = sess->stats.rtcp_bandwidth;
  RTP_SESSION_UNLOCK (sess);

  return result;
}

/**
 * rtp_session_set_sdes_string:
 * @sess: an #RTPSession
 * @type: the type of the SDES item
 * @item: a null-terminated string to set.
 *
 * Store an SDES item of @type in @sess.
 *
 * Returns: %FALSE if the data was unchanged @type is invalid.
 */
/*gboolean
rtp_session_set_sdes_string (RTPSession * sess, GstRTCPSDESType type,
    const gchar * item)
{
  gboolean result;

  g_return_val_if_fail (RTP_IS_SESSION (sess), FALSE);

  RTP_SESSION_LOCK (sess);
  result = rtp_source_set_sdes_string (sess->source, type, item);
  RTP_SESSION_UNLOCK (sess);

  return result;
}
*/
/**
 * rtp_session_get_sdes_string:
 * @sess: an #RTPSession
 * @type: the type of the SDES item
 *
 * Get the SDES item of @type from @sess.
 *
 * Returns: a null-terminated copy of the SDES item or NULL when @type was not
 * valid. g_free() after usage.
 */
/*gchar *
rtp_session_get_sdes_string (RTPSession * sess, GstRTCPSDESType type)
{
  gchar *result;

  g_return_val_if_fail (RTP_IS_SESSION (sess), NULL);

  RTP_SESSION_LOCK (sess);
  result = rtp_source_get_sdes_string (sess->source, type);
  RTP_SESSION_UNLOCK (sess);

  return result;
}
*/
/**
 * rtp_session_get_sdes_struct:
 * @sess: an #RTSPSession
 *
 * Get the SDES data as a #GstStructure
 *
 * Returns: a GstStructure with SDES items for @sess. This function returns a
 * copy of the SDES structure, use gst_structure_free() after usage.
 */
GstStructure *
rtp_session_get_sdes_struct (RTPSession * sess)
{

  GstStructure *result = NULL;

  g_return_val_if_fail (RTP_IS_SESSION (sess), NULL);

  RTP_SESSION_LOCK (sess);

	if (sess->sdes)
	{
    	result = gst_structure_copy (sess->sdes);
	}
  RTP_SESSION_UNLOCK (sess);

  return result;
}

/**
 * rtp_session_set_sdes_struct:
 * @sess: an #RTSPSession
 * @sdes: a #GstStructure
 *
 * Set the SDES data as a #GstStructure. This function makes a copy of @sdes.
 */
void
rtp_session_set_sdes_struct (RTPSession * sess, const GstStructure * sdes)
{
  g_return_if_fail (sdes);
  g_return_if_fail (RTP_IS_SESSION (sess));

  RTP_SESSION_LOCK (sess);
	if (sess->sdes)
	{
    	gst_structure_free (sess->sdes);
	}
	sess->sdes = gst_structure_copy (sdes);
	RTP_SESSION_UNLOCK (sess);
}

static GstFlowReturn
source_push_rtp (RTPSource * source, gpointer data, RTPSession * session)
{
	GstFlowReturn result = GST_FLOW_OK;
	if (source->internal)
	{
		//for #RET change
	    GST_LOG ("source %08x pushed sender RTP packet", source->ssrc);
	    RTP_SESSION_UNLOCK (session);
    	if (session->callbacks.send_rtp)
		{
			result = session->callbacks.send_rtp (session, source, data,
          	session->send_rtp_user_data);
		}
		else
		{
			gst_mini_object_unref (GST_MINI_OBJECT_CAST (data));
    	}
  	}
	else
	{
		GST_LOG ("source %08x pushed receiver RTP packet", source->ssrc);
		RTP_SESSION_UNLOCK (session);
		if (session->callbacks.process_rtp)
		{
		result =
          session->callbacks.process_rtp (session, source,
          GST_BUFFER_CAST (data), session->process_rtp_user_data);
		}
		else
		{
			gst_buffer_unref (GST_BUFFER_CAST (data));
		}
	}
	RTP_SESSION_LOCK (session);

	return result;
}

static gint
source_clock_rate (RTPSource * source, guint8 pt, RTPSession * session)
{
  gint result;

  RTP_SESSION_UNLOCK (session);

  if (session->callbacks.clock_rate)
    result =
        session->callbacks.clock_rate (session, pt,
        session->clock_rate_user_data);
  else
    result = -1;

  RTP_SESSION_LOCK (session);

  GST_DEBUG ("got clock-rate %d for pt %d", result, pt);

  return result;
}
/**
	* @fn		source_rtx_src
	* @brief
	*
	* @param[in]/[out]	:RTPSource *source, guint8 pt, RTPSession *session
	* @return			: gint
		* @warning		: None
	* @exception		: None
	* @see
	*/
static gint source_rtx_src (RTPSource *source, guint8 pt, RTPSession *session)
{
  gint result=ZERO;

  RTP_SESSION_UNLOCK (session);

	if (session->callbacks.rtx_src)
	{
		result = session->callbacks.rtx_src (session, pt,session->rtx_src_user_data);
	}
	else
	{
		result = INVALID_OR_RESET_WITH_MINUS_ONE;
	}

	RTP_SESSION_LOCK (session);
	GST_DEBUG ("got rtx-src %d for pt %d Source is %x", result, pt,source);
	return result;
}
static RTPSourceCallbacks callbacks = {
  (RTPSourcePushRTP) source_push_rtp,
  (RTPSourceClockRate) source_clock_rate,
  (RTPSourceRtxSrc) source_rtx_src,
};

static gboolean
check_collision (RTPSession * sess, RTPSource * source,
    RTPArrivalStats * arrival, gboolean rtp)
{
   guint32 ssrc=ZERO;
  /* If we have no arrival address, we can't do collision checking */
	if (!arrival->have_address)
    {
		return FALSE;
	}

  ssrc = rtp_source_get_ssrc (source);
	if (!source->internal)
	{
		//for #RET change
		GstNetAddress *from;
		gboolean have_from;

    /* This is not our local source, but lets check if two remote
     * source collide
     */

    if (rtp) {
      from = &source->rtp_from;
      have_from = source->have_rtp_from;
    } else {
      from = &source->rtcp_from;
      have_from = source->have_rtcp_from;
    }

    if (have_from) {
      if (gst_netaddress_equal (from, &arrival->address)) {
        /* Address is the same */
        return FALSE;
      } else {
        GST_LOG ("we have a third-party collision or loop ssrc:%x",
            rtp_source_get_ssrc (source));
        if (sess->favor_new) {
          if (rtp_source_find_conflicting_address (source,
                  &arrival->address, arrival->current_time)) {
            gchar buf1[40];
            gst_netaddress_to_string (&arrival->address, buf1, 40);
            GST_LOG ("Known conflict on %x for %s, dropping packet",
                rtp_source_get_ssrc (source), buf1);
            return TRUE;
          } else {
            gchar buf1[40], buf2[40];

            /* Current address is not a known conflict, lets assume this is
             * a new source. Save old address in possible conflict list
             */
            rtp_source_add_conflicting_address (source, from,
                arrival->current_time);

            gst_netaddress_to_string (from, buf1, 40);
            gst_netaddress_to_string (&arrival->address, buf2, 40);
            GST_DEBUG ("New conflict for ssrc %x, replacing %s with %s,"
                " saving old as known conflict",
                rtp_source_get_ssrc (source), buf1, buf2);

            if (rtp)
              rtp_source_set_rtp_from (source, &arrival->address);
            else
              rtp_source_set_rtcp_from (source, &arrival->address);
            return FALSE;
          }
        } else {
          /* Don't need to save old addresses, we ignore new sources */
          return TRUE;
        }
      }
    } else {
      /* We don't already have a from address for RTP, just set it */
      if (rtp)
        rtp_source_set_rtp_from (source, &arrival->address);
      else
        rtp_source_set_rtcp_from (source, &arrival->address);
      return FALSE;
    }

    /* FIXME: Log 3rd party collision somehow
     * Maybe should be done in upper layer, only the SDES can tell us
     * if its a collision or a loop
     */

    /* If the source has been inactive for some time, we assume that it has
     * simply changed its transport source address. Hence, there is no true
     * third-party collision - only a simulated one. */
    if (arrival->current_time > source->last_activity) {
      GstClockTime inactivity_period =
          arrival->current_time - source->last_activity;
      if (inactivity_period > 1 * GST_SECOND) {
        /* Use new network address */
        if (rtp) {
          g_assert (source->have_rtp_from);
          rtp_source_set_rtp_from (source, &arrival->address);
        } else {
          g_assert (source->have_rtcp_from);
          rtp_source_set_rtcp_from (source, &arrival->address);
        }
        return FALSE;
      }
    }
  } else {
    /* This is sending with our ssrc, is it an address we already know */

    if (rtp_source_find_conflicting_address (source, &arrival->address,
            arrival->current_time)) {
      /* Its a known conflict, its probably a loop, not a collision
       * lets just drop the incoming packet
       */
      GST_DEBUG ("Our packets are being looped back to us, dropping");
    } else {
      /* Its a new collision, lets change our SSRC */

      rtp_source_add_conflicting_address (source, &arrival->address,
          arrival->current_time);
//for #RET change
       GST_DEBUG ("Collision for SSRC %x", ssrc);
      /* mark the source BYE */
      rtp_source_mark_bye (source, "SSRC Collision");
      /* if we were suggesting this SSRC, change to something else */
	if (sess->suggested_ssrc == ssrc)
	{
		sess->suggested_ssrc = rtp_session_create_new_ssrc (sess);
	}
    on_ssrc_collision (sess, source);
    rtp_session_schedule_bye_locked (sess, arrival->current_time);
    //  sess->change_ssrc = TRUE;
    }
  }

  return TRUE;
}

//for #RET change
/**
	* @fn		find_source
	* @brief
	*
	* @param[in]/[out]	: RTPSession *sess, guint32 ssrc
	* @return			: RTPSource*
	* @warning			: None
	* @exception		: None
	* @see
	*/
static RTPSource* find_source (RTPSession *sess, guint32 ssrc)
{
  return g_hash_table_lookup (sess->ssrcs[sess->mask_idx],
      GINT_TO_POINTER (ssrc));
}
/**
	* @fn		add_source
	* @brief
	*
	* @param[in]/[out]	: RTPSession *sess,RTPSource *src
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/
static void add_source (RTPSession *sess, RTPSource *src)
{
  g_hash_table_insert (sess->ssrcs[sess->mask_idx],GINT_TO_POINTER (src->ssrc), src);
  /* report the new source ASAP */
  src->generation = sess->generation;
  /* we have one more source now */
  sess->total_sources++;
	if (RTP_SOURCE_IS_ACTIVE (src))
	{
		sess->stats.active_sources++;
	}
	if (src->internal)
	{
		sess->stats.internal_sources++;
		if (sess->suggested_ssrc != src->ssrc)
		{
			sess->suggested_ssrc = src->ssrc;
		}
	}
}
/* must be called with the session lock, the returned source needs to be
 * unreffed after usage. */
static RTPSource *
obtain_source (RTPSession * sess, guint32 ssrc, gboolean * created,
    RTPArrivalStats * arrival, gboolean rtp)
{
  RTPSource *source;

  source =  find_source (sess, ssrc);

  if (source == NULL) {
    /* make new Source in probation and insert */
    source = rtp_source_new (ssrc);

    /* for RTP packets we need to set the source in probation. Receiving RTCP
     * packets of an SSRC, on the other hand, is a strong indication that we
     * are dealing with a valid source. */
	if (rtp)
	{
		g_object_set (source, "probation", sess->probation, NULL);
	}
	else
	{
		g_object_set(source, "probation", ZERO, NULL);
	}

    /* store from address, if any */
    if (arrival->have_address)
	{
      if (rtp)
		{
			rtp_source_set_rtp_from (source, &arrival->address);
		}
      else
		{
			rtp_source_set_rtcp_from (source, &arrival->address);
		}
    }

    /* configure a callback on the source */
    rtp_source_set_callbacks (source, &callbacks, sess);

    add_source (sess, source);

    /* we have one more source now */
    *created = TRUE;
  }
  else
  {
    *created = FALSE;
    /* check for collision, this updates the address when not previously set */
    if (check_collision (sess, source, arrival, rtp)) {
      return NULL;
    }
	/* Receiving RTCP packets of an SSRC is a strong indication that we
     * are dealing with a valid source. */
	if (!rtp)
	{
		g_object_set(source, "probation", ZERO, NULL);
	}
  }
  /* update last activity */
  source->last_activity = arrival->current_time;
	if (rtp)
	{
		source->last_rtp_activity = arrival->current_time;
	}
	g_object_ref (source);

	return source;
}

/* must be called with the session lock, the returned source needs to be
 * unreffed after usage. */
/**
	* @fn		obtain_internal_source
	* @brief
	*
	* @param[in]/[out]	: RTPSession *sess, guint32 ssrc, gboolean *created
	* @return			: RTPSource*
	* @warning			: None
	* @exception		: None
	* @see
	*/
static RTPSource* obtain_internal_source (RTPSession *sess, guint32 ssrc, gboolean *created)
{
  RTPSource *source=NULL;

  source = find_source (sess, ssrc);
	if (source == NULL)
	{
		/* make new internal Source and insert */
		source = rtp_source_new (ssrc);
		GST_DEBUG ("creating new internal source %08x %p", ssrc, source);
		source->validated = TRUE;
		source->internal = TRUE;
		rtp_source_set_sdes_struct (source, gst_structure_copy (sess->sdes));
		rtp_source_set_callbacks (source, &callbacks, sess);
		add_source (sess, source);
		*created = TRUE;
	}
	else
	{
		*created = FALSE;
	}
	g_object_ref (source);

  return source;
}

/**
	* @fn		rtp_session_suggest_ssrc
	* @brief
	*
	* @param[in]/[out]	: RTPSession *sess
	* @return			: guint32
	* @warning			: None
	* @exception		: None
	* @see
	*/

guint32
rtp_session_suggest_ssrc (RTPSession *sess)
{
  guint32 result =ZERO;

  g_return_val_if_fail (RTP_IS_SESSION (sess),ZERO);

  RTP_SESSION_LOCK (sess);
  result = sess->suggested_ssrc;
  RTP_SESSION_UNLOCK (sess);

  return result;
}

/**
 * rtp_session_add_source:
 * @sess: a #RTPSession
 * @src: #RTPSource to add
 *
 * Add @src to @session.
 *
 * Returns: %TRUE on success, %FALSE if a source with the same SSRC already
 * existed in the session.
 */
gboolean
rtp_session_add_source (RTPSession * sess, RTPSource * src)
{
  gboolean result = FALSE;
  RTPSource *find;

  g_return_val_if_fail (RTP_IS_SESSION (sess), FALSE);
  g_return_val_if_fail (src != NULL, FALSE);

  RTP_SESSION_LOCK (sess);
  find = find_source (sess, src->ssrc);
	if (find == NULL)
	{
		add_source (sess, src);
		/* we have one more source now */
		result = TRUE;
	}
  RTP_SESSION_UNLOCK (sess);

  return result;
}

/**
 * rtp_session_get_num_sources:
 * @sess: an #RTPSession
 *
 * Get the number of sources in @sess.
 *
 * Returns: The number of sources in @sess.
 */
guint
rtp_session_get_num_sources (RTPSession * sess)
{
  guint result;

  g_return_val_if_fail (RTP_IS_SESSION (sess), FALSE);

  RTP_SESSION_LOCK (sess);
  result = sess->total_sources;
  RTP_SESSION_UNLOCK (sess);

  return result;
}

/**
 * rtp_session_get_num_active_sources:
 * @sess: an #RTPSession
 *
 * Get the number of active sources in @sess. A source is considered active when
 * it has been validated and has not yet received a BYE RTCP message.
 *
 * Returns: The number of active sources in @sess.
 */
guint
rtp_session_get_num_active_sources (RTPSession * sess)
{
  guint result;

  g_return_val_if_fail (RTP_IS_SESSION (sess), 0);

  RTP_SESSION_LOCK (sess);
  result = sess->stats.active_sources;
  RTP_SESSION_UNLOCK (sess);

  return result;
}

/**
 * rtp_session_get_source_by_ssrc:
 * @sess: an #RTPSession
 * @ssrc: an SSRC
 *
 * Find the source with @ssrc in @sess.
 *
 * Returns: a #RTPSource with SSRC @ssrc or NULL if the source was not found.
 * g_object_unref() after usage.
 */
RTPSource *
rtp_session_get_source_by_ssrc (RTPSession * sess, guint32 ssrc)
{
  RTPSource *result;

  g_return_val_if_fail (RTP_IS_SESSION (sess), NULL);

  RTP_SESSION_LOCK (sess);
  result = find_source (sess, ssrc);
	if (result)
    {
		g_object_ref (result);
	}
  RTP_SESSION_UNLOCK (sess);

  return result;
}
/* should be called with the SESSION lock */
static guint32
rtp_session_create_new_ssrc (RTPSession * sess)
{
  guint32 ssrc;

  while (TRUE) {
    ssrc = g_random_int ();

    /* see if it exists in the session, we're done if it doesn't */
    if (find_source(sess, ssrc)== NULL)
	{
		break;
	}
  }
  return ssrc;
}


/**
 * rtp_session_create_source:
 * @sess: an #RTPSession
 *
 * Create an #RTPSource for use in @sess. This function will create a source
 * with an ssrc that is currently not used by any participants in the session.
 *
 * Returns: an #RTPSource.
 */
RTPSource *
rtp_session_create_source (RTPSession * sess)
{
  guint32 ssrc;
  RTPSource *source;

  RTP_SESSION_LOCK (sess);
  ssrc = rtp_session_create_new_ssrc (sess);
  source = rtp_source_new (ssrc);
  rtp_source_set_callbacks (source, &callbacks, sess);
  /* we need an additional ref for the source in the hashtable */
  g_object_ref (source);
  add_source (sess, source);
  /* we have one more source now */
  RTP_SESSION_UNLOCK (sess);

  return source;
}

/* update the RTPArrivalStats structure with the current time and other bits
 * about the current buffer we are handling.
 * This function is typically called when a validated packet is received.
 * This function should be called with the SESSION_LOCK
 */
static void
update_arrival_stats (RTPSession * sess, RTPArrivalStats * arrival,
    gboolean rtp, GstBuffer * buffer, GstClockTime current_time,
    GstClockTime running_time, guint64 ntpnstime)
{
  /* get time of arrival */
  arrival->current_time = current_time;
  arrival->running_time = running_time;
  arrival->ntpnstime = ntpnstime;

  /* get packet size including header overhead */
  arrival->bytes = GST_BUFFER_SIZE (buffer) + sess->header_len;

  if (rtp) {
    arrival->payload_len = gst_rtp_buffer_get_payload_len (buffer);
  } else {
    arrival->payload_len = 0;
  }

  /* for netbuffer we can store the IP address to check for collisions */
  arrival->have_address = GST_IS_NETBUFFER (buffer);
  if (arrival->have_address) {
    GstNetBuffer *netbuf = (GstNetBuffer *) buffer;

    memcpy (&arrival->address, &netbuf->from, sizeof (GstNetAddress));
  }//else
   // arrival->address = 0;     to be removed
}
//for #RET change
/**
	* @fn		clean_arrival_stats
	* @brief
	*
	* @param[in]/[out]	: RTPArrivalStats *arrival
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/
static void
clean_arrival_stats (RTPArrivalStats *arrival)
{
	if (arrival->have_address)
	{
		//arrival->address = 0;
		arrival->have_address = FALSE;
	}
}
/**
	* @fn		source_update_active
	* @brief
	*
	* @param[in]/[out]	: RTPSession *sess, RTPSource *source,gboolean prevactive
	* @return			: gboolean
	* @warning			: None
	* @exception		: None
	* @see
	*/
static gboolean
source_update_active (RTPSession *sess, RTPSource *source,gboolean prevactive)
{
	gboolean active = RTP_SOURCE_IS_ACTIVE (source);
	guint32 ssrc = source->ssrc;
	if (prevactive == active)
	{
		return FALSE;
	}

	if (active)
	{
		sess->stats.active_sources++;
		GST_DEBUG ("source: %08x became active, %d active sources", ssrc,
        sess->stats.active_sources);
	}
	else
	{
		sess->stats.active_sources--;
		GST_DEBUG ("source: %08x became inactive, %d active sources", ssrc,
        sess->stats.active_sources);
	}
	return TRUE;
}
/**
	* @fn		source_update_sender
	* @brief
	*
	* @param[in]/[out]	:RTPSession *sess, RTPSource *source, gboolean prevsender
	* @return			: gboolean
	* @warning			: None
	* @exception		: None
	* @see
	*/

static gboolean
source_update_sender (RTPSession *sess, RTPSource *source, gboolean prevsender)
{
	gboolean sender = RTP_SOURCE_IS_SENDER (source);
	guint32 ssrc = source->ssrc;
	if (prevsender == sender)
	{
		return FALSE;
	}
	if (sender)
	{
		sess->stats.sender_sources++;
		if (source->internal)
		{
			sess->stats.internal_sender_sources++;
		}
		GST_DEBUG ("source: %08x became sender, %d sender sources", ssrc,
		sess->stats.sender_sources);
	}
	else
	{
		sess->stats.sender_sources--;
		if (source->internal)
		{
			sess->stats.internal_sender_sources--;
		}
		GST_DEBUG ("source: %08x became non sender, %d sender sources", ssrc,
		sess->stats.sender_sources);
	}
		return TRUE;
}

/**
	* @fn		Make_Original
	* @brief
	*
	* @param[in]/[out]	: GstBuffer *buffer
	* @return			: GstBuffer*
	* @warning			: None
	* @exception		: None
	* @see
	*/


static GstBuffer* Make_Original(GstBuffer *buffer)
{
  GstBuffer *outbuf=NULL;
  guint size=ZERO;
  guint header_len=ZERO,payload_len=ZERO;
  guint8 *data =NULL, *outdata=NULL;
  guint8 *payload=NULL;
  guint16 new_seq_num=ZERO;

  size = GST_BUFFER_SIZE (buffer);
  data = GST_BUFFER_DATA (buffer);
  header_len = gst_rtp_buffer_get_header_len(buffer);
  payload_len = gst_rtp_buffer_get_payload_len(buffer);
  payload = gst_rtp_buffer_get_payload (buffer);

  outbuf = gst_buffer_new_and_alloc (size-TWO);
  gst_buffer_copy_metadata(outbuf,buffer,GST_BUFFER_COPY_ALL);

  new_seq_num = ((payload[ZERO]<<EIGHT) | payload[PLUS_ONE]);

  GST_DEBUG ("THIS SEQ_NUM WAS LOST %d",new_seq_num);

  outdata = GST_BUFFER_DATA (outbuf);
  memcpy(outdata,data,header_len);
  memcpy((outdata+header_len),(payload+TWO),(payload_len-TWO));

  gst_rtp_buffer_set_seq (outbuf,new_seq_num);

  gst_buffer_unref(buffer); //deallocating memory

	if (G_UNLIKELY(!gst_rtp_buffer_validate (outbuf)))
	{
		GST_DEBUG("--->>>no valid buffer");
	}
	else
	{
		GST_DEBUG("-->> valid buffer");
	}
	return outbuf;
}

/**
 * rtp_session_process_rtp:
 * @sess: and #RTPSession
 * @buffer: an RTP buffer
 * @current_time: the current system time
 * @running_time: the running_time of @buffer
 *
 * Process an RTP buffer in the session manager. This function takes ownership
 * of @buffer.
 *
 * Returns: a #GstFlowReturn.
 */
GstFlowReturn
rtp_session_process_rtp (RTPSession * sess, GstBuffer * buffer,
    GstClockTime current_time, GstClockTime running_time)
{
  GstFlowReturn result;
  guint32 ssrc;
  RTPSource *source;
  gboolean created;
  gboolean prevsender, prevactive;
  RTPArrivalStats arrival;
  guint32 csrcs[16];
  guint8 i, count;
  guint64 oldrate;
  guint8 pt=ZERO;  				//for #RET change
  gint rtx_src=INVALID_OR_RESET_WITH_MINUS_ONE; 			//for #RET change

  g_return_val_if_fail (RTP_IS_SESSION (sess), GST_FLOW_ERROR);
  g_return_val_if_fail (GST_IS_BUFFER (buffer), GST_FLOW_ERROR);

  if (!gst_rtp_buffer_validate (buffer))
  {
	goto invalid_packet;
  }

  RTP_SESSION_LOCK (sess);
  /* update arrival stats */
  update_arrival_stats (sess, &arrival, TRUE, buffer, current_time,running_time, INVALID_OR_RESET_WITH_MINUS_ONE);

	/* get SSRC and look up in session database */
	ssrc = gst_rtp_buffer_get_ssrc (buffer);

        GST_DEBUG ("SSRC_VALUE  %x ",ssrc);

	//for #RET change
	if (sess->do_retransmission)
	{
		pt = gst_rtp_buffer_get_payload_type(buffer);
		//CODE FOR SSRC multiplexing just a check to see if it would be suffice for us or not
		if(sess->main_ssrc ==ZERO)
		{
			GST_DEBUG ("got main_ssrc %d ",sess->main_ssrc);
			sess->main_ssrc = ssrc;
			sess->main_ssrc_pt = pt;
		}
		if(sess->main_ssrc != ssrc)
		{
			if((sess->rtx_ssrc == ZERO) || (sess->rtx_ssrc != ssrc))
			{
			GST_DEBUG ("got inside rtx_ssrc ");
				pt = gst_rtp_buffer_get_payload_type (buffer);
				RTP_SESSION_UNLOCK (sess);
				if (sess->callbacks.rtx_src)
				{
					rtx_src = sess->callbacks.rtx_src (sess, pt, sess->rtx_src_user_data);
				}

			GST_DEBUG ("got rtx_src %d ",rtx_src);
				RTP_SESSION_LOCK (sess);
				if (rtx_src == PLUS_ONE)
				{
			GST_DEBUG ("rtx_ssrc %d ",ssrc);
					sess->rtx_ssrc = ssrc;
				}
			}
		}
		if((sess->rtx_ssrc == ssrc) && (sess->rtx_ssrc != ZERO))
		{
			GST_DEBUG ("replacing SSRC value check this ");
			gst_rtp_buffer_set_ssrc (buffer,sess->main_ssrc);
			ssrc = sess->main_ssrc;
			gst_rtp_buffer_set_payload_type (buffer, sess->main_ssrc_pt);
			pt = sess->main_ssrc_pt;
			buffer = Make_Original(buffer);
		}

		if((sess->main_ssrc != ssrc) && (sess->rtx_ssrc != ssrc))
		{
			sess->main_ssrc = ssrc;
			sess->main_ssrc_pt = pt;
		}
	}
  source = obtain_source (sess, ssrc, &created, &arrival, TRUE);
	if (!source)
	{
		goto collision;
	}

  prevsender = RTP_SOURCE_IS_SENDER (source);
  prevactive = RTP_SOURCE_IS_ACTIVE (source);
  oldrate = source->bitrate;

  /* copy available csrc for later */
  count = gst_rtp_buffer_get_csrc_count (buffer);
  /* make sure to not overflow our array. An RTP buffer can maximally contain
   * 16 CSRCs */
  count = MIN (count, 16);

  for (i = 0; i < count; i++)
  {
	csrcs[i] = gst_rtp_buffer_get_csrc (buffer, i);
  }

  /* let source process the packet */
  result = rtp_source_process_rtp (source, buffer, &arrival);

  /* source became active */ //for #RET change
	if (source_update_active (sess, source, prevactive))
	{
		on_ssrc_validated (sess, source);
	}
	source_update_sender (sess, source, prevsender);

	if (oldrate != source->bitrate)
    {
		sess->recalc_bandwidth = TRUE;
	}

	if (created)
    {
		on_new_ssrc (sess, source);
	}

	if (source->validated)
	{
		gboolean created;

		/* for validated sources, we add the CSRCs as well */
		for (i = 0; i < count; i++)
		{
			guint32 csrc;
			RTPSource *csrc_src;

			csrc = csrcs[i];

			/* get source */
			csrc_src = obtain_source (sess, csrc, &created, &arrival, TRUE);
			if (!csrc_src)
			{
				continue;
			}

			if (created)
			{
				GST_DEBUG ("created new CSRC: %08x", csrc);
				rtp_source_set_as_csrc (csrc_src);
				source_update_active (sess, csrc_src, FALSE);
				on_new_ssrc (sess, csrc_src);
			}
			g_object_unref (csrc_src);
		}
	}
  g_object_unref (source);

  RTP_SESSION_UNLOCK (sess);
  clean_arrival_stats (&arrival);

  return result;

  /* ERRORS */
invalid_packet:
  {
    gst_buffer_unref (buffer);
    GST_DEBUG ("invalid RTP packet received");
    return GST_FLOW_OK;
  }
collision:
  {
    gst_buffer_unref (buffer);
    RTP_SESSION_UNLOCK (sess);
    GST_DEBUG ("ignoring packet because its collisioning");
    return GST_FLOW_OK;
  }
}

static void
rtp_session_process_rb (RTPSession * sess, RTPSource * source,
    GstRTCPPacket * packet, RTPArrivalStats * arrival)
{
  guint count, i;

  count = gst_rtcp_packet_get_rb_count (packet);
  for (i = 0; i < count; i++) {
    guint32 ssrc, exthighestseq, jitter, lsr, dlsr;
    guint8 fractionlost;
    gint32 packetslost;
    RTPSource *src=NULL;

    gst_rtcp_packet_get_rb (packet, i, &ssrc, &fractionlost,
        &packetslost, &exthighestseq, &jitter, &lsr, &dlsr);

    GST_DEBUG ("RB %d: SSRC %08x, jitter %" G_GUINT32_FORMAT, i, ssrc, jitter);

    src = find_source(sess, ssrc);  //for #RET change
	if (src == NULL)
	{
		continue;
	}

	if (src->internal && RTP_SOURCE_IS_ACTIVE (src))
	{
      /* only deal with report blocks for our session, we update the stats of
       * the sender of the RTCP message. We could also compare our stats against
       * the other sender to see if we are better or worse. */
		rtp_source_process_rb (source, arrival->ntpnstime, fractionlost,
          packetslost, exthighestseq, jitter, lsr, dlsr);
	}
  }
  on_ssrc_active (sess, source);
}

/* A Sender report contains statistics about how the sender is doing. This
 * includes timing informataion such as the relation between RTP and NTP
 * timestamps and the number of packets/bytes it sent to us.
 *
 * In this report is also included a set of report blocks related to how this
 * sender is receiving data (in case we (or somebody else) is also sending stuff
 * to it). This info includes the packet loss, jitter and seqnum. It also
 * contains information to calculate the round trip time (LSR/DLSR).
 */
static void
rtp_session_process_sr (RTPSession * sess, GstRTCPPacket * packet,
    RTPArrivalStats * arrival, gboolean * do_sync)
{
  guint32 senderssrc, rtptime, packet_count, octet_count;
  guint64 ntptime;
  RTPSource *source;
  gboolean created, prevsender;

  gst_rtcp_packet_sr_get_sender_info (packet, &senderssrc, &ntptime, &rtptime,
      &packet_count, &octet_count);

  GST_DEBUG ("got SR packet: SSRC %08x, time %" GST_TIME_FORMAT,
      senderssrc, GST_TIME_ARGS (arrival->current_time));

  source = obtain_source (sess, senderssrc, &created, arrival, FALSE);
	if (!source)
	{
		return;
	}

  /* don't try to do lip-sync for sources that sent a BYE */
	if (RTP_SOURCE_IS_MARKED_BYE (source))
	{
		*do_sync = FALSE;
	}
	else
	{
		*do_sync = TRUE;
	}

  prevsender = RTP_SOURCE_IS_SENDER (source);

  /* first update the source */
  rtp_source_process_sr (source, arrival->current_time, ntptime, rtptime,
      packet_count, octet_count);

  source_update_sender (sess, source, prevsender);

	if (created)
	{
		on_new_ssrc (sess, source);
	}

  rtp_session_process_rb (sess, source, packet, arrival);
  g_object_unref (source);
}

/* A receiver report contains statistics about how a receiver is doing. It
 * includes stuff like packet loss, jitter and the seqnum it received last. It
 * also contains info to calculate the round trip time.
 *
 * We are only interested in how the sender of this report is doing wrt to us.
 */
static void
rtp_session_process_rr (RTPSession * sess, GstRTCPPacket * packet,
    RTPArrivalStats * arrival)
{
  guint32 senderssrc;
  RTPSource *source;
  gboolean created;

  senderssrc = gst_rtcp_packet_rr_get_ssrc (packet);

  GST_DEBUG ("got RR packet: SSRC %08x", senderssrc);

  source = obtain_source (sess, senderssrc, &created, arrival, FALSE);
  if (!source)
    return;

  if (created)
    on_new_ssrc (sess, source);

  rtp_session_process_rb (sess, source, packet, arrival);
  g_object_unref (source);
}

/* Get SDES items and store them in the SSRC */
static void
rtp_session_process_sdes (RTPSession * sess, GstRTCPPacket * packet,
    RTPArrivalStats * arrival)
{
  guint items, i, j;
  gboolean more_items, more_entries;

  items = gst_rtcp_packet_sdes_get_item_count (packet);
  GST_DEBUG ("got SDES packet with %d items", items);

  more_items = gst_rtcp_packet_sdes_first_item (packet);
  i = 0;
  while (more_items) {
    guint32 ssrc;
    gboolean changed=FALSE, created=FALSE, prevactive=FALSE;
    RTPSource *source;
    GstStructure *sdes;

    ssrc = gst_rtcp_packet_sdes_get_ssrc (packet);

    GST_DEBUG ("item %d, SSRC %08x", i, ssrc);

    changed = FALSE;

    /* find src, no probation when dealing with RTCP */
    source = obtain_source (sess, ssrc, &created, arrival, FALSE);
    if (!source)
      return;

    sdes = gst_structure_new ("application/x-rtp-source-sdes", NULL);

    more_entries = gst_rtcp_packet_sdes_first_entry (packet);
    j = 0;
    while (more_entries) {
      GstRTCPSDESType type;
      guint8 len;
      guint8 *data;
      gchar *name;
      gchar *value;

      gst_rtcp_packet_sdes_get_entry (packet, &type, &len, &data);

      GST_DEBUG ("entry %d, type %d, len %d, data %.*s", j, type, len, len,
          data);

      if (type == GST_RTCP_SDES_PRIV) {
        name = g_strndup ((const gchar *) &data[1], data[0]);
        len -= data[0] + 1;
        data += data[0] + 1;
      } else {
        name = g_strdup (gst_rtcp_sdes_type_to_name (type));
      }

      value = g_strndup ((const gchar *) data, len);

      gst_structure_set (sdes, name, G_TYPE_STRING, value, NULL);

      g_free (name);
      g_free (value);

      more_entries = gst_rtcp_packet_sdes_next_entry (packet);
      j++;
    }

    /* takes ownership of sdes */
    changed = rtp_source_set_sdes_struct (source, sdes);

	prevactive = RTP_SOURCE_IS_ACTIVE (source);
    source->validated = TRUE;


		if (created)
		{
			on_new_ssrc (sess, source);
		}
		/* source became active */
		if (source_update_active (sess, source, prevactive))
		{
			on_ssrc_validated (sess, source);
		}
    if (changed)
	{
      on_ssrc_sdes (sess, source);
	}

    g_object_unref (source);

    more_items = gst_rtcp_packet_sdes_next_item (packet);
    i++;
  }
}

/* BYE is sent when a client leaves the session
 */
static void
rtp_session_process_bye (RTPSession * sess, GstRTCPPacket * packet,
    RTPArrivalStats * arrival)
{
  guint count, i;
  gchar *reason;
  gboolean reconsider = FALSE;

  reason = gst_rtcp_packet_bye_get_reason (packet);
  GST_DEBUG ("got BYE packet (reason: %s)", GST_STR_NULL (reason));

  count = gst_rtcp_packet_bye_get_ssrc_count (packet);
  for (i = 0; i < count; i++) {
    guint32 ssrc;
    RTPSource *source;
    gboolean created, prevactive, prevsender;
    guint pmembers, members;

    ssrc = gst_rtcp_packet_bye_get_nth_ssrc (packet, i);
    GST_DEBUG ("SSRC: %08x", ssrc);


    /* find src and mark bye, no probation when dealing with RTCP */
    source = obtain_source (sess, ssrc, &created, arrival, FALSE);
    if (!source)
	{
      return;
	}

	if (source->internal)
	{
		/* our own source, something weird with this packet */
		g_object_unref (source);
		continue;
    }
    /* store time for when we need to time out this source */
    source->bye_time = arrival->current_time;

    prevactive = RTP_SOURCE_IS_ACTIVE (source);
    prevsender = RTP_SOURCE_IS_SENDER (source);

    /* mark the source BYE */
    rtp_source_mark_bye (source, reason);

    pmembers = sess->stats.active_sources;

    source_update_active (sess, source, prevactive);
    source_update_sender (sess, source, prevsender);
    members = sess->stats.active_sources;

	if (!sess->scheduled_bye && members < pmembers)
	{
		/* some members went away since the previous timeout estimate.
		* Perform reverse reconsideration but only when we are not scheduling a
		* BYE ourselves. */
		if (sess->next_rtcp_check_time != GST_CLOCK_TIME_NONE &&
          arrival->current_time < sess->next_rtcp_check_time)
		{
			GstClockTime time_remaining;
			time_remaining = sess->next_rtcp_check_time - arrival->current_time;
			sess->next_rtcp_check_time =
				gst_util_uint64_scale (time_remaining, members, pmembers);
			GST_DEBUG ("reverse reconsideration %" GST_TIME_FORMAT,
            GST_TIME_ARGS (sess->next_rtcp_check_time));
			sess->next_rtcp_check_time += arrival->current_time;
			/* mark pending reconsider. We only want to signal the reconsideration
			* once after we handled all the source in the bye packet */
			reconsider = TRUE;
		}
	}

    if (created)
      on_new_ssrc (sess, source);

    on_bye_ssrc (sess, source);

    g_object_unref (source);
  }
  if (reconsider) {
    RTP_SESSION_UNLOCK (sess);
    /* notify app of reconsideration */
    if (sess->callbacks.reconsider)
      sess->callbacks.reconsider (sess, sess->reconsider_user_data);
    RTP_SESSION_LOCK (sess);
  }
  g_free (reason);
}

static void
rtp_session_process_app (RTPSession * sess, GstRTCPPacket * packet,
    RTPArrivalStats * arrival)
{
  GST_DEBUG ("received APP");
}

static gboolean
rtp_session_request_local_key_unit (RTPSession * sess, RTPSource * src,
    gboolean fir, GstClockTime current_time)
{
  guint32 round_trip = 0;

  rtp_source_get_last_rb (src, NULL, NULL, NULL, NULL, NULL, NULL, &round_trip);

  if (sess->last_keyframe_request != GST_CLOCK_TIME_NONE && round_trip) {
    GstClockTime round_trip_in_ns = gst_util_uint64_scale (round_trip,
        GST_SECOND, 65536);

	if (current_time - sess->last_keyframe_request < TWO * round_trip_in_ns)
	{
		GST_DEBUG ("Ignoring %s request because one was send without one "
		"RTT (%" GST_TIME_FORMAT " < %" GST_TIME_FORMAT ")",fir ? "FIR" : "PLI",
         GST_TIME_ARGS (current_time - sess->last_keyframe_request),
         GST_TIME_ARGS (round_trip_in_ns));;
		return FALSE;
	}
}

  sess->last_keyframe_request = current_time;

  GST_LOG ("received %s request from %X %p(%p)", fir ? "FIR" : "PLI",
      rtp_source_get_ssrc (src), sess->callbacks.process_rtp,
      sess->callbacks.request_key_unit);

  RTP_SESSION_UNLOCK (sess);
  sess->callbacks.request_key_unit (sess, fir,
      sess->request_key_unit_user_data);
  RTP_SESSION_LOCK (sess);

  return TRUE;
}

static void
rtp_session_process_pli (RTPSession * sess, guint32 sender_ssrc,
    guint32 media_ssrc, GstClockTime current_time)
{
  RTPSource *src;

	if (!sess->callbacks.request_key_unit)
	{
		return;
	}

  src = find_source (sess, sender_ssrc);
	if (!src)
	{
		return;
	}

  rtp_session_request_local_key_unit (sess, src, FALSE, current_time);
}

static void
rtp_session_process_fir (RTPSession * sess, guint32 sender_ssrc,
    guint8 * fci_data, guint fci_length, GstClockTime current_time)
{
  RTPSource *src;
  guint32 ssrc;
  guint position = 0;
  gboolean our_request = FALSE;

	if (!sess->callbacks.request_key_unit)
	{
		return;
	}
	if (fci_length < 8)
    {
		return;
	}

  src = find_source (sess, sender_ssrc);

  /* Hack because Google fails to set the sender_ssrc correctly */
  if (!src && sender_ssrc == 1) {
    GHashTableIter iter;

	if (sess->stats.sender_sources > sess->stats.internal_sender_sources + PLUS_ONE)
	{
		return;
	}

    g_hash_table_iter_init (&iter, sess->ssrcs[sess->mask_idx]);

	while (g_hash_table_iter_next (&iter, NULL, (gpointer *) & src))
	{
		if (!src->internal && rtp_source_is_sender (src))
		{
			break;
		}
		src = NULL;
    }
  }

	if (!src)
    {
		return;
	}

  for (position = 0; position < fci_length; position += 8) {
    guint8 *data = fci_data + position;
    RTPSource *own=NULL;

    ssrc = GST_READ_UINT32_BE (data);


      own = find_source (sess, ssrc);
	if (own->internal)
	{
		our_request = TRUE;
		break;
	}
  }
	if (!our_request)
    {
		return;
	}

  rtp_session_request_local_key_unit (sess, src, TRUE, current_time);
}
/**
	* @fn		rtp_session_process_nack
	* @brief
	*
	* @param[in]/[out]	:RTPSession *sess, guint32 sender_ssrc,guint32 media_ssrc, guint8 *fci_data, guint fci_length,
							GstClockTime current_time
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/

static void
rtp_session_process_nack (RTPSession *sess, guint32 sender_ssrc,
    guint32 media_ssrc, guint8 *fci_data, guint fci_length,
    GstClockTime current_time)
{
	if (!sess->callbacks.notify_nack)
	{
		return;
	}

	while (fci_length > ZERO)
	{
		guint16 seqnum=ZERO, blp=ZERO;
		seqnum = GST_READ_UINT16_BE (fci_data);
		blp = GST_READ_UINT16_BE (fci_data + TWO);
		GST_DEBUG ("NACK #%u, blp %04x", seqnum, blp);
		GST_DEBUG ("Sender_ssrc #%d, media_ssrc %d,current_time  %d",sender_ssrc,media_ssrc,current_time);
		RTP_SESSION_UNLOCK (sess);
		sess->callbacks.notify_nack (sess, seqnum, blp,
        sess->notify_nack_user_data);
		RTP_SESSION_LOCK (sess);
		fci_data += FOUR;
		fci_length -=FOUR;
	}
}


static void
rtp_session_process_feedback (RTPSession * sess, GstRTCPPacket * packet,
    RTPArrivalStats * arrival, GstClockTime current_time)
{
  GstRTCPType type = gst_rtcp_packet_get_type (packet);
  GstRTCPFBType fbtype = gst_rtcp_packet_fb_get_type (packet);
  guint32 sender_ssrc = gst_rtcp_packet_fb_get_sender_ssrc (packet);
  guint32 media_ssrc = gst_rtcp_packet_fb_get_media_ssrc (packet);
  guint8 *fci_data = gst_rtcp_packet_fb_get_fci (packet);
  guint fci_length = FOUR * gst_rtcp_packet_fb_get_fci_length (packet);
  RTPSource *src=NULL;

  GST_DEBUG ("received feedback %d:%d from %08X about %08X with FCI of "
      "length %d", type, fbtype, sender_ssrc, media_ssrc, fci_length);

  if (g_signal_has_handler_pending (sess,
          rtp_session_signals[SIGNAL_ON_FEEDBACK_RTCP], 0, TRUE)) {
    GstBuffer *fci_buffer = NULL;

    if (fci_length > 0) {
      fci_buffer = gst_buffer_create_sub (packet->buffer,
          fci_data - GST_BUFFER_DATA (packet->buffer), fci_length);
      GST_BUFFER_TIMESTAMP (fci_buffer) = arrival->running_time;
    }

    RTP_SESSION_UNLOCK (sess);
    g_signal_emit (sess, rtp_session_signals[SIGNAL_ON_FEEDBACK_RTCP], 0,
        type, fbtype, sender_ssrc, media_ssrc, fci_buffer);
    RTP_SESSION_LOCK (sess);

    if (fci_buffer)
	{
      gst_buffer_unref (fci_buffer);
	}
  }

  src = find_source (sess, media_ssrc);
	if (!src)
	{
		return;
	}

  if (sess->rtcp_feedback_retention_window) {
      rtp_source_retain_rtcp_packet (src, packet, arrival->running_time);
  }

  if (src->internal ||   /* PSFB FIR puts the media ssrc inside the FCI */
      (type == GST_RTCP_TYPE_PSFB && fbtype == GST_RTCP_PSFB_TYPE_FIR))
  {
	switch (type)
	{
		case GST_RTCP_TYPE_PSFB:
			switch (fbtype)
			{
				case GST_RTCP_PSFB_TYPE_PLI:
					rtp_session_process_pli (sess, sender_ssrc, media_ssrc,
					current_time);
					break;
				case GST_RTCP_PSFB_TYPE_FIR:
					rtp_session_process_fir (sess, sender_ssrc, fci_data, fci_length,
					current_time);
				break;
				default:
					break;
			}
			break;
		case GST_RTCP_TYPE_RTPFB:
			switch (fbtype)
			{
				// FIX ME here RTPSource has to be added see 1.1.4
				case GST_RTCP_RTPFB_TYPE_NACK:
					rtp_session_process_nack (sess, sender_ssrc, media_ssrc,
					fci_data, fci_length, current_time);
					break;
				default:
					break;
			}
			break;
		default:
			break;
	}
  }
}

/**
 * rtp_session_process_rtcp:
 * @sess: and #RTPSession
 * @buffer: an RTCP buffer
 * @current_time: the current system time
 * @ntpnstime: the current NTP time in nanoseconds
 *
 * Process an RTCP buffer in the session manager. This function takes ownership
 * of @buffer.
 *
 * Returns: a #GstFlowReturn.
 */
GstFlowReturn
rtp_session_process_rtcp (RTPSession * sess, GstBuffer * buffer,
    GstClockTime current_time, guint64 ntpnstime)
{
  GstRTCPPacket packet;
  gboolean more, is_bye = FALSE, do_sync = FALSE;
  RTPArrivalStats arrival;
  GstFlowReturn result = GST_FLOW_OK;

  g_return_val_if_fail (RTP_IS_SESSION (sess), GST_FLOW_ERROR);
  g_return_val_if_fail (GST_IS_BUFFER (buffer), GST_FLOW_ERROR);

	if (!gst_rtcp_buffer_validate (buffer))
	{
		goto invalid_packet;
	}

  GST_DEBUG ("received RTCP packet");

  RTP_SESSION_LOCK (sess);
  /* update arrival stats */
  update_arrival_stats (sess, &arrival, FALSE, buffer, current_time, -1,
      ntpnstime);

//  if (sess->sent_bye)
//    goto ignore;

  /* start processing the compound packet */
  more = gst_rtcp_buffer_get_first_packet (buffer, &packet);
  while (more) {
    GstRTCPType type;

    type = gst_rtcp_packet_get_type (&packet);

    /* when we are leaving the session, we should ignore all non-BYE messages */
	if (sess->scheduled_bye && type != GST_RTCP_TYPE_BYE)
	{
		GST_DEBUG ("ignoring non-BYE RTCP packet because we are leaving");
		goto next;
    }

    switch (type) {
      case GST_RTCP_TYPE_SR:
        rtp_session_process_sr (sess, &packet, &arrival, &do_sync);
        break;
      case GST_RTCP_TYPE_RR:
        rtp_session_process_rr (sess, &packet, &arrival);
        break;
      case GST_RTCP_TYPE_SDES:
        rtp_session_process_sdes (sess, &packet, &arrival);
        break;
      case GST_RTCP_TYPE_BYE:
        is_bye = TRUE;
        /* don't try to attempt lip-sync anymore for streams with a BYE */
        do_sync = FALSE;
        rtp_session_process_bye (sess, &packet, &arrival);
        break;
      case GST_RTCP_TYPE_APP:
        rtp_session_process_app (sess, &packet, &arrival);
        break;
      case GST_RTCP_TYPE_RTPFB:
      case GST_RTCP_TYPE_PSFB:
        rtp_session_process_feedback (sess, &packet, &arrival, current_time);
        break;
      default:
        GST_WARNING ("got unknown RTCP packet");
        break;
    }
  next:
    more = gst_rtcp_packet_move_to_next (&packet);
  }

  /* if we are scheduling a BYE, we only want to count bye packets, else we
   * count everything */
	if (sess->scheduled_bye)
	{
		if (is_bye)
		{
			sess->stats.bye_members++;
			UPDATE_AVG (sess->stats.avg_rtcp_packet_size, arrival.bytes);
		}
	}
	else
	{
		/* keep track of average packet size */
		UPDATE_AVG (sess->stats.avg_rtcp_packet_size, arrival.bytes);
	}
  GST_DEBUG ("%p, received RTCP packet, avg size %u, %u", &sess->stats,
      sess->stats.avg_rtcp_packet_size, arrival.bytes);
  RTP_SESSION_UNLOCK (sess);

  clean_arrival_stats (&arrival);

  /* notify caller of sr packets in the callback */
  if (do_sync && sess->callbacks.sync_rtcp) {
    /* make writable, we might want to change the buffer */
    buffer = gst_buffer_make_metadata_writable (buffer);

    result = sess->callbacks.sync_rtcp (sess, buffer,
        sess->sync_rtcp_user_data);
  }
  else
  {
    gst_buffer_unref (buffer);
  }

  return result;

  /* ERRORS */
invalid_packet:
  {
    GST_DEBUG ("invalid RTCP packet received");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }
}


/**
 * rtp_session_update_send_caps:
 * @sess: an #RTPSession
 * @caps: a #GstCaps
 *
 * Update the caps of the sender in the rtp session.
 */
 /**
	* @fn		rtp_session_update_send_caps
	* @brief
	*
	* @param[in]/[out]	:RTPSession *sess, GstCaps *caps
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/

void
rtp_session_update_send_caps (RTPSession *sess, GstCaps *caps)
{
  GstStructure *s=NULL;
  guint ssrc=ZERO;

  g_return_if_fail (RTP_IS_SESSION (sess));
  g_return_if_fail (GST_IS_CAPS (caps));

  GST_LOG ("received caps %" GST_PTR_FORMAT, caps);

  s = gst_caps_get_structure (caps,ZERO);

	if(gst_structure_get_uint (s, "ssrc", &ssrc))
	{
		RTPSource *source=NULL;
		gboolean created=FALSE;
		RTP_SESSION_LOCK (sess);
		source = obtain_internal_source (sess, ssrc, &created);
		if (source)
		{
			rtp_source_update_caps (source, caps);
			g_object_unref (source);
		}
		RTP_SESSION_UNLOCK (sess);
	}
}

/**
 * rtp_session_send_rtp:
 * @sess: an #RTPSession
 * @data: pointer to either an RTP buffer or a list of RTP buffers
 * @is_list: TRUE when @data is a buffer list
 * @current_time: the current system time
 * @running_time: the running time of @data
 *
 * Send the RTP buffer in the session manager. This function takes ownership of
 * @buffer.
 *
 * Returns: a #GstFlowReturn.
 */
GstFlowReturn
rtp_session_send_rtp (RTPSession * sess, gpointer data, gboolean is_list,
    GstClockTime current_time, GstClockTime running_time)
{
  GstFlowReturn result=ZERO;
  RTPSource *source=NULL;
  gboolean prevsender=FALSE;
  gboolean valid_packet=FALSE;
  guint64 oldrate=ZERO;

  GstBuffer *buffer=NULL;
  guint32 ssrc=ZERO;
  gboolean created=FALSE;


  g_return_val_if_fail (RTP_IS_SESSION (sess), GST_FLOW_ERROR);
  g_return_val_if_fail (is_list || GST_IS_BUFFER (data), GST_FLOW_ERROR);

	if (is_list)
	{
		GstBufferList *list = GST_BUFFER_LIST_CAST (data);
		valid_packet = gst_rtp_buffer_list_validate (GST_BUFFER_LIST_CAST (data));
		buffer = gst_buffer_list_get (list,ZERO,ZERO);
		if (!buffer)
		{
			goto no_buffer;
		}
	}
	else
	{
		valid_packet = gst_rtp_buffer_validate (GST_BUFFER_CAST (data));
		buffer = GST_BUFFER_CAST (data);
	}

  if (!valid_packet)
    goto invalid_packet;

  GST_LOG ("received RTP %s for sending", is_list ? "list" : "packet");


  ssrc = gst_rtp_buffer_get_ssrc (buffer);

  RTP_SESSION_LOCK (sess);

  source = obtain_internal_source (sess, ssrc, &created);

  /* update last activity */
  source->last_rtp_activity = current_time;

  prevsender = RTP_SOURCE_IS_SENDER (source);
  oldrate = source->bitrate;

  /* we use our own source to send */
  result = rtp_source_send_rtp (source, data, is_list, running_time);

  source_update_sender (sess, source, prevsender);

  if (oldrate != source->bitrate)
    sess->recalc_bandwidth = TRUE;
  RTP_SESSION_UNLOCK (sess);

  g_object_unref (source);

  return result;

  /* ERRORS */
invalid_packet:
  {
    gst_mini_object_unref (GST_MINI_OBJECT_CAST (data));
    GST_DEBUG ("invalid RTP packet received");
    return GST_FLOW_OK;
  }

no_buffer:
	{
		gst_mini_object_unref (GST_MINI_OBJECT_CAST (data));
		GST_DEBUG ("no buffer in list");
		return GST_FLOW_OK;
	}


}

static void
add_bitrates (gpointer key, RTPSource * source, gdouble * bandwidth)
{
  *bandwidth += source->bitrate;
}

static GstClockTime
calculate_rtcp_interval (RTPSession * sess, gboolean deterministic,
    gboolean first)
{
  GstClockTime result;

  /* recalculate bandwidth when it changed */
  if (sess->recalc_bandwidth) {
    gdouble bandwidth;

    if (sess->bandwidth > 0)
      bandwidth = sess->bandwidth;
    else {
      /* If it is <= 0, then try to estimate the actual bandwidth */
      bandwidth = ZERO;

      g_hash_table_foreach (sess->ssrcs[sess->mask_idx],
          (GHFunc)add_bitrates, &bandwidth);
      bandwidth /= 8.0;
    }
    if (bandwidth < 8000)
      bandwidth = RTP_STATS_BANDWIDTH;

    rtp_stats_set_bandwidths (&sess->stats, bandwidth,
        sess->rtcp_bandwidth, sess->rtcp_rs_bandwidth, sess->rtcp_rr_bandwidth);

    sess->recalc_bandwidth = FALSE;
  }

	if (sess->scheduled_bye)
	{
		result = rtp_stats_calculate_bye_interval (&sess->stats);
	}
	else
	{
		result = rtp_stats_calculate_rtcp_interval (&sess->stats,
			sess->stats.internal_sender_sources > ZERO, first);
	}

  GST_DEBUG ("next deterministic interval: %" GST_TIME_FORMAT ", first %d",
      GST_TIME_ARGS (result), first);

  if (!deterministic && result != GST_CLOCK_TIME_NONE)
    result = rtp_stats_add_rtcp_jitter (&sess->stats, result);

  GST_DEBUG ("next interval: %" GST_TIME_FORMAT, GST_TIME_ARGS (result));

  return result;
}
/**
	* @fn		source_mark_bye
	* @brief
	*
	* @param[in]/[out]	:const gchar *key, RTPSource *source, const gchar *reason
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/

static void
source_mark_bye (const gchar *key, RTPSource *source, const gchar *reason)
{
	GST_DEBUG("key is %x",key);
	if (source->internal)
	{
		rtp_source_mark_bye (source, reason);
	}
}

/**
 * rtp_session_mark_all_bye:
 * @sess: an #RTPSession
 * @reason: a reason
 *
 * Mark all internal sources of the session as BYE with @reason.
 */
/**
	* @fn		rtp_session_mark_all_bye
	* @brief
	*
	* @param[in]/[out]	:RTPSession *sess, const gchar *reason
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/
void
rtp_session_mark_all_bye (RTPSession *sess, const gchar *reason)
{
  g_return_if_fail (RTP_IS_SESSION (sess));
  RTP_SESSION_LOCK(sess);
  g_hash_table_foreach (sess->ssrcs[sess->mask_idx],(GHFunc)source_mark_bye,(gpointer) reason);
  RTP_SESSION_UNLOCK (sess);
}
/* Stop the current @sess and schedule a BYE message for the other members.
 * One must have the session lock to call this function
 */
 /**
	* @fn		rtp_session_schedule_bye_locked
	* @brief
	*
	* @param[in]/[out]	: RTPSession * sess, GstClockTime current_time
	* @return			: GstFlowReturn
	* @warning			: None
	* @exception		: None
	* @see
	*/
static GstFlowReturn
rtp_session_schedule_bye_locked (RTPSession * sess, GstClockTime current_time)
{
  GstFlowReturn result = GST_FLOW_OK;
  GstClockTime interval;

  /* nothing to do it we already scheduled bye */
	if (sess->scheduled_bye)
	{
		goto done;
	}

  /* we schedule BYE now */
  sess->scheduled_bye = TRUE;
  /* at least one member wants to send a BYE */
  INIT_AVG (sess->stats.avg_rtcp_packet_size, 100);
  sess->stats.bye_members = 1;
  sess->first_rtcp = TRUE;
  sess->allow_early = TRUE;

  /* reschedule transmission */
  sess->last_rtcp_send_time = current_time;
  interval = calculate_rtcp_interval (sess, FALSE, TRUE);

	if(interval != GST_CLOCK_TIME_NONE)
	{
		sess->next_rtcp_check_time = current_time + interval;
	}
	else
	{
		sess->next_rtcp_check_time = GST_CLOCK_TIME_NONE;
	}

  GST_DEBUG ("Schedule BYE for %" GST_TIME_FORMAT ", %" GST_TIME_FORMAT,
      GST_TIME_ARGS (interval), GST_TIME_ARGS (sess->next_rtcp_check_time));

  RTP_SESSION_UNLOCK (sess);
  /* notify app of reconsideration */
  if (sess->callbacks.reconsider)
    sess->callbacks.reconsider (sess, sess->reconsider_user_data);
  RTP_SESSION_LOCK (sess);
done:

  return result;
}

/**
 * rtp_session_schedule_bye:
 * @sess: an #RTPSession
 * @reason: a reason or NULL
 * @current_time: the current system time
 *
 * Stop the current @sess and schedule a BYE message for the other members.
 *
 * Returns: a #GstFlowReturn.
 */

 /**
	* @fn			rtp_session_schedule_bye
	* @brief
	*
	* @param[in]/[out]	:RTPSession * sess, GstClockTime current_time
	* @return			: GstFlowReturn
	* @warning			: None
	* @exception		: None
	* @see
	*/
GstFlowReturn
rtp_session_schedule_bye (RTPSession * sess, GstClockTime current_time)
{
  GstFlowReturn result = GST_FLOW_OK;

  g_return_val_if_fail (RTP_IS_SESSION (sess), GST_FLOW_ERROR);

  RTP_SESSION_LOCK (sess);
  result = rtp_session_schedule_bye_locked (sess, current_time);
  RTP_SESSION_UNLOCK (sess);

  return result;
}

/**
 * rtp_session_next_timeout:
 * @sess: an #RTPSession
 * @current_time: the current system time
 *
 * Get the next time we should perform session maintenance tasks.
 *
 * Returns: a time when rtp_session_on_timeout() should be called with the
 * current system time.
 */
GstClockTime
rtp_session_next_timeout (RTPSession * sess, GstClockTime current_time)
{
  GstClockTime result, interval = 0;

  g_return_val_if_fail (RTP_IS_SESSION (sess), GST_CLOCK_TIME_NONE);

  RTP_SESSION_LOCK (sess);

	if (GST_CLOCK_TIME_IS_VALID (sess->next_early_rtcp_time))
	{
		GST_DEBUG ("have early rtcp time");
		result = sess->next_early_rtcp_time;
		goto early_exit;
	}

  result = sess->next_rtcp_check_time;

  GST_DEBUG ("current time: %" GST_TIME_FORMAT ", next :%" GST_TIME_FORMAT,
      GST_TIME_ARGS (current_time), GST_TIME_ARGS (result));

	if (result == GST_CLOCK_TIME_NONE || result < current_time)
	{
		GST_DEBUG ("take current time as base");
		/* our previous check time expired, start counting from the current time
		* again. */
		result = current_time;
	}

	if (sess->scheduled_bye)
	{
		if (sess->stats.active_sources >= 50)
		{
			GST_DEBUG ("reconsider BYE, more than 50 sources");
			/* reconsider BYE if members >= 50 */
			interval = calculate_rtcp_interval (sess, FALSE, TRUE);
		}
	}
	else {
    if (sess->first_rtcp) {
      GST_DEBUG ("first RTCP packet");
      /* we are called for the first time */
      interval = calculate_rtcp_interval (sess, FALSE, TRUE);
    } else if (sess->next_rtcp_check_time < current_time) {
      GST_DEBUG ("old check time expired, getting new timeout");
      /* get a new timeout when we need to */
      interval = calculate_rtcp_interval (sess, FALSE, FALSE);
    }
  }

  if (interval != GST_CLOCK_TIME_NONE)
    result += interval;
  else
    result = GST_CLOCK_TIME_NONE;

  sess->next_rtcp_check_time = result;

early_exit:

  GST_DEBUG ("current time: %" GST_TIME_FORMAT
      ", next time: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (current_time), GST_TIME_ARGS (result));
  RTP_SESSION_UNLOCK (sess);

  return result;
}

typedef struct
{
  RTPSource *source;
  gboolean is_bye;
  GstBuffer *buffer;
} ReportOutput;

typedef struct
{
  //GstRTCPBuffer rtcpbuf;
  RTPSession *sess;
  RTPSource *source;
  guint num_to_report;
  gboolean have_fir;
  gboolean have_pli;
  gboolean have_nack;
  GstBuffer *rtcp;
  GstClockTime current_time;
  guint64 ntpnstime;
  GstClockTime running_time;
  GstClockTime interval;
  GstRTCPPacket packet;
  gboolean has_sdes;
  gboolean is_early;
  gboolean may_suppress;
  GQueue output;
} ReportData;

static void
session_start_rtcp (RTPSession * sess, ReportData * data)
{
  GstRTCPPacket *packet = &data->packet;
  RTPSource *own = data->source;
  data->rtcp = gst_rtcp_buffer_new (sess->mtu);
  data->has_sdes = FALSE;


  if (RTP_SOURCE_IS_SENDER (own)) {
    guint64 ntptime;
    guint32 rtptime;
    guint32 packet_count, octet_count;

    /* we are a sender, create SR */
    GST_DEBUG ("create SR for SSRC %08x", own->ssrc);
    gst_rtcp_buffer_add_packet (data->rtcp, GST_RTCP_TYPE_SR, packet);

    /* get latest stats */
    rtp_source_get_new_sr (own, data->ntpnstime, data->running_time,
        &ntptime, &rtptime, &packet_count, &octet_count);
    /* store stats */
    rtp_source_process_sr (own, data->current_time, ntptime, rtptime,
        packet_count, octet_count);

    /* fill in sender report info */
    gst_rtcp_packet_sr_set_sender_info (packet, own->ssrc,
        ntptime, rtptime, packet_count, octet_count);
  } else {
    /* we are only receiver, create RR */
    GST_DEBUG ("create RR for SSRC %08x", own->ssrc);
    gst_rtcp_buffer_add_packet (data->rtcp, GST_RTCP_TYPE_RR, packet);
    gst_rtcp_packet_rr_set_ssrc (packet, own->ssrc);
  }
}

/* construct a Sender or Receiver Report */
static void
session_report_blocks (const gchar * key, RTPSource * source, ReportData * data)
{
  RTPSession *sess = data->sess;
  GstRTCPPacket *packet = &data->packet;
  guint8 fractionlost;
  gint32 packetslost;
  guint32 exthighestseq, jitter;
  guint32 lsr, dlsr;

  /* don't report for sources in future generations */
	if(((gint16)(source->generation - sess->generation)) > ZERO)
	{
		GST_DEBUG ("source %08x generation %u > %u", source->ssrc,
		source->generation, sess->generation);
		return;
	}

  /* only report about other sender */
	if (source == data->source)
	{
		goto reported;
	}

	if(gst_rtcp_packet_get_rb_count (packet) == GST_RTCP_MAX_RB_COUNT)
	{
		GST_DEBUG ("max RB count reached");
		return;
	}

	if (!RTP_SOURCE_IS_SENDER (source))
	{
		GST_DEBUG ("source %08x not sender", source->ssrc);
		goto reported;
	}

  GST_DEBUG ("create RB for SSRC %08x", source->ssrc);

  /* get new stats */
  rtp_source_get_new_rb (source, data->current_time, &fractionlost,
      &packetslost, &exthighestseq, &jitter, &lsr, &dlsr);

      /* store last generated RR packet */
      source->last_rr.is_valid = TRUE;
      source->last_rr.fractionlost = fractionlost;
      source->last_rr.packetslost = packetslost;
      source->last_rr.exthighestseq = exthighestseq;
      source->last_rr.jitter = jitter;
      source->last_rr.lsr = lsr;
      source->last_rr.dlsr = dlsr;

  /* packet is not yet filled, add report block for this source. */
  gst_rtcp_packet_add_rb (packet, source->ssrc, fractionlost, packetslost,
      exthighestseq, jitter, lsr, dlsr);

reported:
  /* source is reported, move to next generation */
  source->generation = sess->generation + PLUS_ONE;

  /* if we reported all sources in this generation, move to next */
	if(--data->num_to_report == ZERO)
	{
		sess->generation++;
		GST_DEBUG ("all reported, generation now %u", sess->generation);
	}
}

/* construct FIR */
/**
	* @fn		session_add_fir
	* @brief
	*
	* @param[in]/[out]	:const gchar *key, RTPSource *source, ReportData *data
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/
static void
session_add_fir (const gchar *key, RTPSource *source, ReportData *data)
{
  GstRTCPPacket *packet = &data->packet;
  guint16 len=ZERO;
  guint8 *fci_data=NULL;

	if (!source->send_fir)
	{
		return;
	}

  len = gst_rtcp_packet_fb_get_fci_length (packet);
  if (!gst_rtcp_packet_fb_set_fci_length (packet, len + TWO))
    {
    	/* exit because the packet is full, will put next request in a
     	* further packet */
    	return;
	}

  fci_data = gst_rtcp_packet_fb_get_fci (packet) + (len * FOUR);

  GST_WRITE_UINT32_BE (fci_data, source->ssrc);
  fci_data += FOUR;
  fci_data[ZERO] = source->current_send_fir_seqnum;
  fci_data[PLUS_ONE] = fci_data[2] = fci_data[3] = ZERO;

  source->send_fir = FALSE;
  GST_DEBUG(" key is %x and  report data is %x",key,data);
}
/**
	* @fn		session_fir
	* @brief
	*
	* @param[in]/[out]	: RTPSession *sess, ReportData *data
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/

static void
session_fir (RTPSession *sess, ReportData *data)
{
  GstRTCPPacket *packet = &data->packet;
	if (!gst_rtcp_buffer_add_packet (data->rtcp, GST_RTCP_TYPE_PSFB, packet))
	{
		return;
	}

  gst_rtcp_packet_fb_set_type (packet, GST_RTCP_PSFB_TYPE_FIR);
  gst_rtcp_packet_fb_set_sender_ssrc (packet, data->source->ssrc);
  gst_rtcp_packet_fb_set_media_ssrc (packet, ZERO);

  g_hash_table_foreach (sess->ssrcs[sess->mask_idx],
      (GHFunc)session_add_fir, data);
	if (gst_rtcp_packet_fb_get_fci_length (packet) == ZERO)
	{
		gst_rtcp_packet_remove (packet);
	}
	else
	{
		data->may_suppress = FALSE;
	}
}

/**
	* @fn		has_pli_compare_func
	* @brief
	*
	* @param[in]/[out]	: gconstpointer a, gconstpointer ignored
	* @return			: gboolean
	* @warning			: None
	* @exception		: None
	* @see
	*/

static gboolean
has_pli_compare_func (gconstpointer a, gconstpointer ignored)
{
  GstRTCPPacket packet={0,};

  packet.buffer = (GstBuffer *) a;
  packet.offset = 0;

  if (gst_rtcp_packet_get_type (&packet) == GST_RTCP_TYPE_PSFB &&
      gst_rtcp_packet_fb_get_type (&packet) == GST_RTCP_PSFB_TYPE_PLI)
    return TRUE;
  else
    return FALSE;

}

/* construct PLI */
/**
	* @fn		session_pli
	* @brief
	*
	* @param[in]/[out]	: const gchar *key, RTPSource *source, ReportData *data
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/
static void
session_pli (const gchar *key, RTPSource *source, ReportData *data)
{
  GstRTCPPacket *packet = &data->packet;

	if (!source->send_pli)
	{
		return;
	}

	if (rtp_source_has_retained (source, has_pli_compare_func, NULL))
	{
		return;
	}

	if(!gst_rtcp_buffer_add_packet (data->rtcp, GST_RTCP_TYPE_PSFB, packet))
	{
		/* exit because the packet is full, will put next request in a
		* further packet */
		return;
	}

  gst_rtcp_packet_fb_set_type (packet, GST_RTCP_PSFB_TYPE_PLI);
  gst_rtcp_packet_fb_set_sender_ssrc (packet, data->source->ssrc);
  gst_rtcp_packet_fb_set_media_ssrc (packet, source->ssrc);

  source->send_pli = FALSE;
  data->may_suppress = FALSE;
  GST_DEBUG(" the key is %x",key);
}

/* construct NACK */
/**
	* @fn		session_nack
	* @brief
	*
	* @param[in]/[out]	: const gchar *key, RTPSource *source, ReportData *data
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/
static void
session_nack (const gchar *key, RTPSource *source, ReportData *data)
{
  GstRTCPPacket *packet = &data->packet;
  guint32 *nacks=NULL;
  guint n_nacks=ZERO, i=ZERO;
  guint8 *fci_data=NULL;
	if(!source->send_nack)
	{
		return;
	}

	if (!gst_rtcp_buffer_add_packet (data->rtcp, GST_RTCP_TYPE_RTPFB, packet))
	{
		/* exit because the packet is full, will put next request in a
		* further packet */
		return;
	}

  gst_rtcp_packet_fb_set_type (packet, GST_RTCP_RTPFB_TYPE_NACK);
  gst_rtcp_packet_fb_set_sender_ssrc (packet, data->source->ssrc);
  gst_rtcp_packet_fb_set_media_ssrc (packet, source->ssrc);

  nacks = rtp_source_get_nacks (source, &n_nacks);
  GST_DEBUG ("%u NACKs", n_nacks);
	if (!gst_rtcp_packet_fb_set_fci_length (packet, n_nacks))
	{
		return;
	}

  fci_data = gst_rtcp_packet_fb_get_fci (packet);
	for (i = ZERO; i < n_nacks; i++)
	{
		GST_WRITE_UINT32_BE (fci_data, nacks[i]);
		fci_data += FOUR;
	}

  rtp_source_clear_nacks (source);
  data->may_suppress = FALSE;
  GST_DEBUG("key is %x",key);
}
/* perform cleanup of sources that timed out */
/**
	* @fn		session_cleanup
	* @brief
	*
	* @param[in]/[out]	:const gchar *key, RTPSource *source, ReportData *data
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/
static void
session_cleanup (const gchar *key, RTPSource *source, ReportData *data)
{
  gboolean remove = FALSE;
  gboolean byetimeout = FALSE;
  gboolean sendertimeout = FALSE;
  gboolean is_sender=FALSE, is_active=FALSE;
  RTPSession *sess = data->sess;
  GstClockTime interval=ZERO, binterval=ZERO;
  GstClockTime btime=ZERO;

  GST_DEBUG(" key is %x",key);
  GST_DEBUG ("look at %08x, generation %u", source->ssrc, source->generation);

  /* check for outdated collisions */
	if (source->internal)
	{
		GST_DEBUG ("Timing out collisions for %x", source->ssrc);
		rtp_source_timeout (source, data->current_time,
        /* "a relatively long time" -- RFC 3550 section 8.2 */
        RTP_STATS_MIN_INTERVAL * GST_SECOND * TEN,
        data->running_time - sess->rtcp_feedback_retention_window);
	}

	/* nothing else to do when without RTCP */
	if (data->interval == GST_CLOCK_TIME_NONE)
	{
		return;
	}



  is_sender = RTP_SOURCE_IS_SENDER (source);
  is_active = RTP_SOURCE_IS_ACTIVE (source);

  /* our own rtcp interval may have been forced low by secondary configuration,
   * while sender side may still operate with higher interval,
   * so do not just take our interval to decide on timing out sender,
   * but take (if data->interval <= 5 * GST_SECOND):
   *   interval = CLAMP (sender_interval, data->interval, 5 * GST_SECOND)
   * where sender_interval is difference between last 2 received RTCP reports
   */
	if (data->interval >= FIVE * GST_SECOND || (source->internal))
	{
		binterval = data->interval;
	}
	else
	{
		GST_LOG ("prev_rtcp %" GST_TIME_FORMAT ", last_rtcp %" GST_TIME_FORMAT,
        GST_TIME_ARGS (source->stats.prev_rtcptime),
        GST_TIME_ARGS (source->stats.last_rtcptime));
		/* if not received enough yet, fallback to larger default */
		if (source->stats.last_rtcptime > source->stats.prev_rtcptime)
		{
			binterval = source->stats.last_rtcptime - source->stats.prev_rtcptime;
		}
		else
		{
			binterval = FIVE * GST_SECOND;
		}
		binterval = CLAMP (binterval, data->interval, FIVE * GST_SECOND);
	}
	GST_LOG ("timeout base interval %" GST_TIME_FORMAT, GST_TIME_ARGS (binterval));

  /* check for our own source, we don't want to delete our own source. */
	if (!(source->internal))
	{
		if (source->marked_bye)
		{
			/* if we received a BYE from the source, remove the source after some
			* time. */
			if (data->current_time > source->bye_time &&
			data->current_time - source->bye_time > sess->stats.bye_timeout)
			{
				GST_DEBUG ("removing BYE source %08x", source->ssrc);
				remove = TRUE;
				byetimeout = TRUE;
			}
		}
		/* sources that were inactive for more than 5 times the deterministic reporting
		* interval get timed out. the min timeout is 5 seconds. */
		/* mind old time that might pre-date last time going to PLAYING */
		if(source->rtx_src != PLUS_ONE)
		{
			//issue fixed need to fix issue here for main_ssrc to go into this loop //FIX ME
			btime = MAX (source->last_activity, sess->start_time);
			if (data->current_time > btime)
			{
				interval = MAX (binterval * FIVE, FIVE * GST_SECOND);
				if (data->current_time - btime > interval)
				{
					GST_DEBUG ("removing timeout source %08x, last %" GST_TIME_FORMAT,
					source->ssrc, GST_TIME_ARGS (btime));
					remove = TRUE;
				}
			}
		}
	}

	/* senders that did not send for a long time become a receiver, this also
	* holds for our own source. */
	if (is_sender)
	{
		/* mind old time that might pre-date last time going to PLAYING */
		btime = MAX (source->last_rtp_activity, sess->start_time);
		if (data->current_time > btime)
		{
			interval = MAX (binterval * TWO, FIVE* GST_SECOND);
			if (data->current_time - btime > interval)
			{
				if (source->internal && source->sent_bye)
				{
					/* an internal source is BYE and stopped sending RTP, remove */
					GST_DEBUG ("internal BYE source %08x timed out, last %"
					GST_TIME_FORMAT, source->ssrc, GST_TIME_ARGS (btime));
					remove = TRUE;
				}
				else
				{
					GST_DEBUG ("sender source %08x timed out and became receiver, last %"
					GST_TIME_FORMAT, source->ssrc, GST_TIME_ARGS (btime));
					sendertimeout = TRUE;
				}
			}
		}
	}

	if (remove)
	{
		sess->total_sources--;
		if(is_sender)
		{
			sess->stats.sender_sources--;
			if (source->internal)
			{
				sess->stats.internal_sender_sources--;
			}
		}
		if (is_active)
		{
			sess->stats.active_sources--;
		}
		if (source->internal)
		{
			sess->stats.internal_sources--;
		}
		if (byetimeout)
		{
			on_bye_timeout (sess, source);
		}
		else
		{
			on_timeout (sess, source);
		}
	}
	else
	{
		if (sendertimeout)
		{
			source->is_sender = FALSE;
			sess->stats.sender_sources--;
			if (source->internal)
			{
				sess->stats.internal_sender_sources--;
			}
			on_sender_timeout (sess, source);
		}
		/* count how many source to report in this generation */
		if(((gint16) (source->generation - sess->generation)) <= ZERO)
		{
			data->num_to_report++;
		}
	}

  source->closing = remove;
}
/**
	* @fn		session_sdes
	* @brief
	*
	* @param[in]/[out]	:RTPSession *sess, ReportData *data
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/
static void
session_sdes (RTPSession *sess, ReportData *data)
{
  GstRTCPPacket *packet = &data->packet;
  const GstStructure *SDES=NULL;
  gint i=ZERO, n_fields=ZERO;
  GST_DEBUG("Session is %x",sess);
  /* add SDES packet */
  gst_rtcp_buffer_add_packet (data->rtcp, GST_RTCP_TYPE_SDES, packet);

  gst_rtcp_packet_sdes_add_item (packet, data->source->ssrc);

  SDES = rtp_source_get_sdes_struct (data->source);

  /* add all fields in the structure, the order is not important. */
  n_fields = gst_structure_n_fields (SDES);
	for (i = ZERO; i < n_fields; ++i)
	{
		const gchar *field=NULL;
		const gchar *value=NULL;
		GstRTCPSDESType type=ZERO;
		field = gst_structure_nth_field_name (SDES, i);
		if (field == NULL)
		{
			continue;
		}
		value = gst_structure_get_string (SDES, field);
		if (value == NULL)
		{
			continue;
		}
		type = gst_rtcp_sdes_name_to_type (field);
		/* Early packets are minimal and only include the CNAME */
		if (data->is_early && type != GST_RTCP_SDES_CNAME)
		{
			continue;
		}
		if (type > GST_RTCP_SDES_END && type < GST_RTCP_SDES_PRIV)
		{
			gst_rtcp_packet_sdes_add_entry (packet, type, strlen (value),
			(const guint8 *) value);
		}
		else if(type == GST_RTCP_SDES_PRIV)
		{
			gsize prefix_len=ZERO;
			gsize value_len=ZERO;
			gsize data_len=ZERO;
			guint8 data[256];
			/* don't accept entries that are too big */
			prefix_len = strlen (field);
			if (prefix_len > TWO_FIFTY_FIVE)
			{
				continue;
			}
			value_len = strlen (value);
			if (value_len > TWO_FIFTY_FIVE)
			{
				continue;
			}
			data_len = PLUS_ONE + prefix_len + value_len;
			if (data_len > TWO_FIFTY_FIVE)
			{
				continue;
			}
			data[0] = prefix_len;
			memcpy (&data[1], field, prefix_len);
      if (prefix_len + value_len > TWO_FIFTY_FIVE)
      {
        GST_WARNING("data enctries are too big %d",prefix_len + value_len);
      }
      else
      {
        if ((1 + prefix_len) < TWO_FIFTY_FIVE)
        {
          memcpy (&data[1 + prefix_len], value, value_len);
        }
        else
        {
           GST_WARNING("data enctries are too big %d",prefix_len + 1);
        }
      }
			gst_rtcp_packet_sdes_add_entry (packet, type, data_len, data);
		}
	}

	data->has_sdes = TRUE;
}

/* schedule a BYE packet */
/**
	* @fn		make_source_bye
	* @brief
	*
	* @param[in]/[out]	:RTPSession *sess, RTPSource *source, ReportData *data
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
**/

static void
make_source_bye (RTPSession *sess, RTPSource *source, ReportData *data)
{
  GstRTCPPacket *packet = &data->packet;

  /* add SDES */
  session_sdes (sess, data);

  /* add a BYE packet */
  gst_rtcp_buffer_add_packet (data->rtcp, GST_RTCP_TYPE_BYE, packet);
  gst_rtcp_packet_bye_add_ssrc (packet, source->ssrc);
	if (source->bye_reason)
	{
		gst_rtcp_packet_bye_set_reason (packet, source->bye_reason);
	}
		/* we have a BYE packet now */
		source->sent_bye = TRUE;
}

/**
	* @fn		is_rtcp_time
	* @brief
	*
	* @param[in]/[out]	: RTPSession *sess, GstClockTime current_time, ReportData *data
	* @return			: gboolean
	* @warning			: None
	* @exception		: None
	* @see
**/


static gboolean
is_rtcp_time (RTPSession *sess, GstClockTime current_time, ReportData *data)
{
  GstClockTime new_send_time=ZERO, elapsed=ZERO;
  GstClockTime interval=ZERO;

  GST_DEBUG(" Printing Session is %x",sess);

	if (GST_CLOCK_TIME_IS_VALID (sess->next_early_rtcp_time))
	{
		data->is_early = TRUE;
	}
	else
	{
		data->is_early = FALSE;
	}

	if (data->is_early && sess->next_early_rtcp_time < current_time)
	{
		GST_DEBUG ("early feedback %" GST_TIME_FORMAT " < now %"
        GST_TIME_FORMAT, GST_TIME_ARGS (sess->next_early_rtcp_time),
        GST_TIME_ARGS (current_time));
		goto early;
	}

	/* no need to check yet */
	if (sess->next_rtcp_check_time == GST_CLOCK_TIME_NONE ||
      sess->next_rtcp_check_time > current_time)
	{
		GST_DEBUG ("no check time yet, next %" GST_TIME_FORMAT " > now %"
        GST_TIME_FORMAT, GST_TIME_ARGS (sess->next_rtcp_check_time),
        GST_TIME_ARGS (current_time));
		return FALSE;
	}

early:
  /* get elapsed time since we last reported */
	elapsed = current_time - sess->last_rtcp_send_time;
	/* take interval and add jitter */
	interval = data->interval;
	if (interval != GST_CLOCK_TIME_NONE)
	{
		interval = rtp_stats_add_rtcp_jitter (&sess->stats, interval);
	}
	/* perform forward reconsideration */
	if (interval != GST_CLOCK_TIME_NONE)
	{
		GST_DEBUG ("forward reconsideration %" GST_TIME_FORMAT ", elapsed %"
        GST_TIME_FORMAT, GST_TIME_ARGS (interval), GST_TIME_ARGS (elapsed));
		new_send_time = interval + sess->last_rtcp_send_time;
	}
	else
	{
		new_send_time = sess->last_rtcp_send_time;
	}

	if (!data->is_early)
	{
		/* check if reconsideration */
		if (new_send_time == GST_CLOCK_TIME_NONE || current_time < new_send_time)
		{
			GST_DEBUG ("reconsider RTCP for %" GST_TIME_FORMAT,
			GST_TIME_ARGS (new_send_time));
			/* store new check time */
			sess->next_rtcp_check_time = new_send_time;
			return FALSE;
		}
		sess->next_rtcp_check_time = current_time + interval;
	}
	else if (interval != GST_CLOCK_TIME_NONE)
	{
		/* Apply the rules from RFC 4585 section 3.5.3 */
		if (sess->stats.min_interval != ZERO && !sess->first_rtcp)
		{
			GstClockTime T_rr_current_interval =
			g_random_double_range (POINT_FIVE,ONE_POINT_FIVE) * sess->stats.min_interval;

			/* This will caused the RTCP to be suppressed if no FB packets are added */
			if (sess->last_rtcp_send_time + T_rr_current_interval > new_send_time)
			{
				GST_DEBUG ("RTCP packet could be suppressed min: %" GST_TIME_FORMAT
				" last: %" GST_TIME_FORMAT
				" + T_rr_current_interval: %" GST_TIME_FORMAT
				" >  new_send_time: %" GST_TIME_FORMAT,
				GST_TIME_ARGS (sess->stats.min_interval),
				GST_TIME_ARGS (sess->last_rtcp_send_time),
				GST_TIME_ARGS (T_rr_current_interval),
				GST_TIME_ARGS (new_send_time));
				data->may_suppress = TRUE;
			}
		}
	}

	GST_DEBUG ("can send RTCP now, next interval %" GST_TIME_FORMAT,
      GST_TIME_ARGS (new_send_time));

  return TRUE;
}

/**
	* @fn		clone_ssrcs_hashtable
	* @brief
	*
	* @param[in]/[out]	:gchar *key, RTPSource *source, GHashTable *hash_table
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
**/

static void
clone_ssrcs_hashtable (gchar *key, RTPSource *source, GHashTable *hash_table)
{
  g_hash_table_insert (hash_table, key, g_object_ref (source));
}

/**
	* @fn		remove_closing_sources
	* @brief
	*
	* @param[in]/[out]	:const gchar *key, RTPSource *source, ReportData *data
	* @return			: gboolean
	* @warning			: None
	* @exception		: None
	* @see
**/

static gboolean
remove_closing_sources (const gchar *key, RTPSource *source, ReportData *data)
{
	GST_DEBUG("Printing key is %x",key);
	if (source->closing)
	{
		return TRUE;
	}
	if (source->send_fir)
	{
		data->have_fir = TRUE;
	}
	if (source->send_pli)
	{
		data->have_pli = TRUE;
	}
	if (source->send_nack)
	{
		data->have_nack = TRUE;
	}
	return FALSE;
}

/**
	* @fn		generate_rtcp
	* @brief
	*
	* @param[in]/[out]	:const gchar *key, RTPSource *source, ReportData *data
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
**/
static void
generate_rtcp (const gchar *key, RTPSource *source, ReportData *data)
{
  RTPSession *sess = data->sess;
  gboolean is_bye = FALSE;
  ReportOutput *output=NULL;

  GST_DEBUG("Printing key is %x",key);

  /* only generate RTCP for active internal sources */
	if (!source->internal || source->sent_bye)
	{
		return;
	}

  data->source = source;

  /* open packet */
  session_start_rtcp (sess, data);

	if (source->marked_bye)
	{
		/* send BYE */
		make_source_bye (sess, source, data);
		is_bye = TRUE;
	}
	else if(!data->is_early)
	{
		/* loop over all known sources and add report blocks. If we are early, we
		* just make a minimal RTCP packet and skip this step */
		g_hash_table_foreach(sess->ssrcs[sess->mask_idx],(GHFunc)session_report_blocks, data);
	}
	if (!data->has_sdes)
	{
		session_sdes (sess, data);
	}

	if (data->have_fir)
	{
		session_fir (sess, data);
	}
	if (data->have_pli)
	{
		g_hash_table_foreach(sess->ssrcs[sess->mask_idx],(GHFunc)session_pli, data);
	}
	if (data->have_nack)
	{
		g_hash_table_foreach(sess->ssrcs[sess->mask_idx],(GHFunc)session_nack, data);
	}


  output = g_slice_new (ReportOutput);
  output->source = g_object_ref (source);
  output->is_bye = is_bye;
  output->buffer = data->rtcp;
  /* queue the RTCP packet to push later */
  g_queue_push_tail (&data->output, output);
}



/**
 * rtp_session_on_timeout:
 * @sess: an #RTPSession
 * @current_time: the current system time
 * @ntpnstime: the current NTP time in nanoseconds
 * @running_time: the current running_time of the pipeline
 *
 * Perform maintenance actions after the timeout obtained with
 * rtp_session_next_timeout() expired.
 *
 * This function will perform timeouts of receivers and senders, send a BYE
 * packet or generate RTCP packets with current session stats.
 *
 * This function can call the #RTPSessionSendRTCP callback, possibly multiple
 * times, for each packet that should be processed.
 *
 * Returns: a #GstFlowReturn.
 */
 /**
	* @fn		rtp_session_on_timeout
	* @brief
	*
	* @param[in]/[out]	:RTPSession *sess, GstClockTime current_time,
						guint64 ntpnstime, GstClockTime running_time
	* @return			: GstFlowReturn
	* @warning			: None
	* @exception		: None
	* @see
	*/

GstFlowReturn
rtp_session_on_timeout (RTPSession *sess, GstClockTime current_time,
    guint64 ntpnstime, GstClockTime running_time)
{
  GstFlowReturn result = GST_FLOW_OK;
  ReportData data;
  GHashTable *table_copy=NULL;
  ReportOutput *output=NULL;

  g_return_val_if_fail (RTP_IS_SESSION (sess), GST_FLOW_ERROR);

  GST_DEBUG ("reporting at %" GST_TIME_FORMAT ", NTP time %" GST_TIME_FORMAT
      ", running-time %" GST_TIME_FORMAT, GST_TIME_ARGS (current_time),
      GST_TIME_ARGS (ntpnstime), GST_TIME_ARGS (running_time));

  data.sess = sess;
  data.current_time = current_time;
  data.ntpnstime = ntpnstime;
  data.running_time = running_time;
  data.num_to_report = ZERO;
  data.may_suppress = FALSE;
  g_queue_init (&data.output);

  RTP_SESSION_LOCK (sess);
  /* get a new interval, we need this for various cleanups etc */
  data.interval = calculate_rtcp_interval (sess, TRUE, sess->first_rtcp);

  GST_DEBUG ("interval %" GST_TIME_FORMAT, GST_TIME_ARGS (data.interval));

  /* we need an internal source now */
	if (sess->stats.internal_sources == ZERO)
	{
		RTPSource *source=NULL;
		gboolean created=FALSE;
		source = obtain_internal_source (sess, sess->suggested_ssrc, &created);
		g_object_unref (source);
	}

/* Make a local copy of the hashtable. We need to do this because the
   * cleanup stage below releases the session lock. */
	table_copy = g_hash_table_new_full (NULL, NULL, NULL,(GDestroyNotify)g_object_unref);
	g_hash_table_foreach (sess->ssrcs[sess->mask_idx],
      (GHFunc)clone_ssrcs_hashtable, table_copy);

  /* Clean up the session, mark the source for removing, this might release the
   * session lock. */
  g_hash_table_foreach(table_copy,(GHFunc)session_cleanup, &data);
  g_hash_table_destroy (table_copy);

  /* Now remove the marked sources */
  g_hash_table_foreach_remove (sess->ssrcs[sess->mask_idx],
      (GHRFunc)remove_closing_sources, &data);

  /* see if we need to generate SR or RR packets */
	if (!is_rtcp_time (sess, current_time, &data))
	{
		goto done;
	}

  GST_DEBUG ("doing RTCP generation %u for %u sources, early %d",
      sess->generation, data.num_to_report, data.is_early);

  /* generate RTCP for all internal sources */
  g_hash_table_foreach (sess->ssrcs[sess->mask_idx],
      (GHFunc)generate_rtcp, &data);

  /* we keep track of the last report time in order to timeout inactive
   * receivers or senders */
	if (!data.is_early && !data.may_suppress)
	{
		sess->last_rtcp_send_time = data.current_time;
	}
	sess->first_rtcp = FALSE;
	sess->next_early_rtcp_time = GST_CLOCK_TIME_NONE;

done:
  RTP_SESSION_UNLOCK (sess);

/* push out the RTCP packets */
	while ((output = g_queue_pop_head (&data.output)))
	{
		gboolean do_not_suppress=ZERO;
		GstBuffer *buffer = output->buffer;
		RTPSource *source = output->source;
		/* Give the user a change to add its own packet */
		g_signal_emit (sess, rtp_session_signals[SIGNAL_ON_SENDING_RTCP], ZERO,
			buffer, data.is_early, &do_not_suppress);
		if (sess->callbacks.send_rtcp && (do_not_suppress || !data.may_suppress))
		{
			guint packet_size=ZERO;
			packet_size = /*gst_buffer_get_size (buffer)*/ GST_BUFFER_SIZE (buffer) + sess->header_len;
			UPDATE_AVG (sess->stats.avg_rtcp_packet_size, packet_size);
			GST_DEBUG ("%p, sending RTCP packet, avg size %u, %u", &sess->stats,
			sess->stats.avg_rtcp_packet_size, packet_size);
			result =
			sess->callbacks.send_rtcp (sess, source, buffer, output->is_bye,
			sess->send_rtcp_user_data);
		}
		else
		{
			GST_DEBUG ("freeing packet callback: %p"
			" do_not_suppress: %d may_suppress: %d",
			sess->callbacks.send_rtcp, do_not_suppress, data.may_suppress);
			gst_buffer_unref (buffer);
		}
		g_object_unref (source);
		g_slice_free (ReportOutput, output);
	}
	return result;
}


/**
 * rtp_session_request_early_rtcp:
 * @sess: an #RTPSession
 * @current_time: the current system time
 * @max_delay: maximum delay
 *
 * Request transmission of early RTCP
 */
 /**
	* @fn		rtp_session_request_early_rtcp
	* @brief
	*
	* @param[in]/[out]	:RTPSession *sess, GstClockTime current_time,
							GstClockTime max_delay
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/

void
rtp_session_request_early_rtcp (RTPSession *sess, GstClockTime current_time,
    GstClockTime max_delay)
{
  GstClockTime T_dither_max=ZERO;

  /* Implements the algorithm described in RFC 4585 section 3.5.2 */

  RTP_SESSION_LOCK (sess);

  /* Check if already requested */
  /*  RFC 4585 section 3.5.2 step 2 */
	if (GST_CLOCK_TIME_IS_VALID (sess->next_early_rtcp_time))
	{
		GST_LOG_OBJECT (sess, "already have next early rtcp time");
		goto dont_send;
	}

	if (!GST_CLOCK_TIME_IS_VALID (sess->next_rtcp_check_time))
	{
		GST_LOG_OBJECT (sess, "no next RTCP check time");
		goto dont_send;
	}

  /* Ignore the request a scheduled packet will be in time anyway */
	if (current_time + max_delay > sess->next_rtcp_check_time)
	{
		GST_LOG_OBJECT (sess, "next scheduled time is soon %" GST_TIME_FORMAT " + %"
        GST_TIME_FORMAT " > %" GST_TIME_FORMAT,
        GST_TIME_ARGS (current_time),
        GST_TIME_ARGS (max_delay), GST_TIME_ARGS (sess->next_rtcp_check_time));
		goto dont_send;
	}

  /*  RFC 4585 section 3.5.2 step 2b */
  /* If the total sources is <=2, then there is only us and one peer */
	if (sess->total_sources <= TWO)
	{
		T_dither_max = ZERO;
	}
	else
	{
		/* Divide by 2 because l = 0.5 */
		T_dither_max = sess->next_rtcp_check_time - sess->last_rtcp_send_time;
		T_dither_max /= TWO;
	}

	/*  RFC 4585 section 3.5.2 step 3 */
	if (current_time + T_dither_max > sess->next_rtcp_check_time)
	{
		GST_LOG_OBJECT (sess, "don't send because of dither");
		goto dont_send;
	}

	/*  RFC 4585 section 3.5.2 step 4
	* Don't send if allow_early is FALSE, but not if we are in
	* immediate mode, meaning we are part of a group of at most the
	* application-specific threshold.
	*/
	if (sess->total_sources > sess->rtcp_immediate_feedback_threshold &&
      sess->allow_early == FALSE)
	{
		GST_LOG_OBJECT (sess, "can't allow early feedback");
		goto dont_send;
	}
	if (T_dither_max)
	{
		/* Schedule an early transmission later */
		sess->next_early_rtcp_time = g_random_double () * T_dither_max +
		current_time;
	}
	else
	{
		/* If no dithering, schedule it for NOW */
		sess->next_early_rtcp_time = current_time;
	}

	GST_LOG_OBJECT (sess, "next early RTCP time %" GST_TIME_FORMAT,
      GST_TIME_ARGS (sess->next_early_rtcp_time));
	RTP_SESSION_UNLOCK (sess);
	/* notify app of need to send packet early
	* and therefore of timeout change */
	if (sess->callbacks.reconsider)
	{
		sess->callbacks.reconsider (sess, sess->reconsider_user_data);
	}

	return;

dont_send:

  RTP_SESSION_UNLOCK (sess);
}

/**
	* @fn		rtp_session_send_rtcp
	* @brief
	*
	* @param[in]/[out]	: RTPSession *sess, GstClockTime max_delay
	* @return			: void
	* @warning			: None
	* @exception		: None
	* @see
	*/


static void
rtp_session_send_rtcp (RTPSession *sess, GstClockTime max_delay)
{
  GstClockTime now=ZERO;

	if (!sess->callbacks.send_rtcp)
	{
		return;
	}
	now = sess->callbacks.request_time (sess, sess->request_time_user_data);
	rtp_session_request_early_rtcp (sess, now, max_delay);
}
/**
	* @fn		rtp_session_request_key_unit
	* @brief
	*
	* @param[in]/[out]	:RTPSession *sess, guint32 ssrc,
						gboolean fir, gint count
	* @return			: gboolean
	* @warning			: None
	* @exception		: None
	* @see
	*/

gboolean
rtp_session_request_key_unit (RTPSession *sess, guint32 ssrc,
    gboolean fir, gint count)
{
  RTPSource *src=NULL;
  RTP_SESSION_LOCK (sess);
  src = find_source (sess, ssrc);
	if (!src)
	{
		goto no_source;
	}

	if (fir)
	{
		src->send_pli = FALSE;
		src->send_fir = TRUE;
		if (count == INVALID_OR_RESET_WITH_MINUS_ONE || count != src->last_fir_count)
		{
			src->current_send_fir_seqnum++;
		}
		src->last_fir_count = count;
	}
	else if (!src->send_fir)
	{
		src->send_pli = TRUE;
	}

  RTP_SESSION_UNLOCK (sess);

  rtp_session_send_rtcp (sess, SEND_RTCP_PLIFIR_MAX_DEFAULT_TIME * GST_MSECOND);

  return TRUE;

no_source:
  {
    RTP_SESSION_UNLOCK (sess);
    return FALSE;
  }
}


/**
 * rtp_session_request_nack:
 * @sess: a #RTPSession
 * @ssrc: the SSRC
 * @seqnum: the missing seqnum
 * @max_delay: max delay to request NACK
 *
 * Request scheduling of a NACK feedback packet for @seqnum in @ssrc.
 *
 * Returns: %TRUE if the NACK feedback could be scheduled
 */

 /**
* @fn		rtp_session_request_nack
* @brief
*
* @param[in]/[out]	: RTPSession *sess, guint32 ssrc, guint16 seqnum,
					  GstClockTime max_delay
* @return		: gboolean
* @warning		: None
* @exception		: None
* @see
*/

gboolean
rtp_session_request_nack (RTPSession *sess, guint32 ssrc, guint16 seqnum,
    GstClockTime max_delay)
{
  RTPSource *source=NULL;
  RTP_SESSION_LOCK (sess);
  source = find_source (sess, ssrc);
	if (source == NULL)
	{
		goto no_source;
	}
	GST_DEBUG ("request NACK for %08x, #%u", ssrc, seqnum);
	rtp_source_register_nack (source, seqnum);
	RTP_SESSION_UNLOCK (sess);
	rtp_session_send_rtcp (sess, max_delay);

	return TRUE;
	/* ERRORS */
no_source:
  {
    RTP_SESSION_UNLOCK (sess);
    return FALSE;
  }
}
