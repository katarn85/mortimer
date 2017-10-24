/*
 *
 * GStreamer
 * Copyright (C) 1999-2001 Erik Walthinsen <omega@cse.ogi.edu>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "gstmms.h"

#define DEFAULT_CONNECTION_SPEED    0
#define DEFAULT_CONNECTION_TIMEOUT    10

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_CONNECTION_SPEED,
  PROP_CONNECTION_TIMEOUT
};

typedef enum
{
  IO_READY_READ,
  IO_READY_WRITE,
} io_state_t;


GST_DEBUG_CATEGORY_STATIC (mmssrc_debug);
#define GST_CAT_DEFAULT mmssrc_debug

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-ms-asf")
    );

static void gst_mms_finalize (GObject * gobject);
static void gst_mms_uri_handler_init (gpointer g_iface, gpointer iface_data);

static void gst_mms_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_mms_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_mms_query (GstBaseSrc * src, GstQuery * query);

static gboolean gst_mms_start (GstBaseSrc * bsrc);
static gboolean gst_mms_stop (GstBaseSrc * bsrc);
static gboolean gst_mms_is_seekable (GstBaseSrc * src);
static gboolean gst_mms_get_size (GstBaseSrc * src, guint64 * size);
static gboolean gst_mms_prepare_seek_segment (GstBaseSrc * src,
    GstEvent * event, GstSegment * segment);
static gboolean gst_mms_do_seek (GstBaseSrc * src, GstSegment * segment);

static GstFlowReturn gst_mms_create (GstPushSrc * psrc, GstBuffer ** buf);

static gboolean gst_mms_uri_set_uri (GstURIHandler * handler,
    const gchar * uri);

static int _do_select (io_state_t state, void *timeout_data);
static int _do_connect (int fd, const struct sockaddr * saptr, int salen, void * data);
static int _timedout_tcp_connect (void * data, const char * host, int port);
static mms_off_t _timedout_tcp_read (void *data, int socket, char *buf, mms_off_t num);
static mms_off_t _timedout_tcp_write (void *data, int socket, char *buf, mms_off_t num);

static GstStateChangeReturn
gst_mms_change_state (GstElement * element, GstStateChange transition);

static void
gst_mms_urihandler_init (GType mms_type)
{
  static const GInterfaceInfo urihandler_info = {
    gst_mms_uri_handler_init,
    NULL,
    NULL
  };

  g_type_add_interface_static (mms_type, GST_TYPE_URI_HANDLER,
      &urihandler_info);
}

GST_BOILERPLATE_FULL (GstMMS, gst_mms, GstPushSrc, GST_TYPE_PUSH_SRC,
    gst_mms_urihandler_init);

static void
gst_mms_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_set_details_simple (element_class, "MMS streaming source",
      "Source/Network",
      "Receive data streamed via MSFT Multi Media Server protocol",
      "Maciej Katafiasz <mathrick@users.sourceforge.net>");

  GST_DEBUG_CATEGORY_INIT (mmssrc_debug, "mmssrc", 0, "MMS Source Element");
}

/* initialize the plugin's class */
static void
gst_mms_class_init (GstMMSClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = (GstBaseSrcClass *) klass;
  GstPushSrcClass *gstpushsrc_class = (GstPushSrcClass *) klass;

  gobject_class->set_property = gst_mms_set_property;
  gobject_class->get_property = gst_mms_get_property;
  gobject_class->finalize = gst_mms_finalize;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "location",
          "Host URL to connect to. Accepted are mms://, mmsu://, mmst:// URL types",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CONNECTION_SPEED,
      g_param_spec_uint ("connection-speed", "Connection Speed",
          "Network connection speed in kbps (0 = unknown)",
          0, G_MAXINT / 1000, DEFAULT_CONNECTION_SPEED,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /* Note: connection-speed is intentionaly limited to G_MAXINT as libmms use int for it */

  g_object_class_install_property (gobject_class, PROP_CONNECTION_TIMEOUT,
      g_param_spec_uint ("connection-timeout", "Connection Timeout",
          "Network connection timeout in seconds (0 = system default)",
          0, 3600, DEFAULT_CONNECTION_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_mms_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_mms_stop);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_mms_create);

  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_mms_is_seekable);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_mms_get_size);
  gstbasesrc_class->prepare_seek_segment =
      GST_DEBUG_FUNCPTR (gst_mms_prepare_seek_segment);
  gstbasesrc_class->do_seek = GST_DEBUG_FUNCPTR (gst_mms_do_seek);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_mms_query);
  
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_mms_change_state);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
gst_mms_init (GstMMS * mmssrc, GstMMSClass * g_class)
{
  mmssrc->uri_name = NULL;
  mmssrc->current_connection_uri_name = NULL;
  mmssrc->connection = NULL;
  mmssrc->connection_speed = DEFAULT_CONNECTION_SPEED;
  mmssrc->connection_timeout = DEFAULT_CONNECTION_TIMEOUT;
  mmssrc->io = NULL;

  mmssrc->stream_rec_lock = g_new (GStaticRecMutex, 1);
  g_static_rec_mutex_init (mmssrc->stream_rec_lock);

  mmssrc->poll = gst_poll_new(TRUE);
}

static void
gst_mms_finalize (GObject * gobject)
{
  GstMMS *mmssrc = GST_MMS (gobject);

  /* We may still have a connection open, as we preserve unused / pristine
     open connections in stop to reuse them in start. */
  if (mmssrc->connection) {
    mmsx_close (mmssrc->connection);
    mmssrc->connection = NULL;
  }

  if (mmssrc->current_connection_uri_name) {
    g_free (mmssrc->current_connection_uri_name);
    mmssrc->current_connection_uri_name = NULL;
  }

  if (mmssrc->uri_name) {
    g_free (mmssrc->uri_name);
    mmssrc->uri_name = NULL;
  }

  if(mmssrc->io) {
    g_free (mmssrc->io);
    mmssrc->io = NULL;
  }

  if (mmssrc->poll) {
      gst_poll_free(mmssrc->poll);
      mmssrc->poll = NULL;
  }

  if (mmssrc->stream_rec_lock) {
      g_static_rec_mutex_free(mmssrc->stream_rec_lock);
      g_free(mmssrc->stream_rec_lock);
      mmssrc->stream_rec_lock = NULL;
    }
  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

/* FIXME operating in TIME rather than BYTES could remove this altogether
 * and be more convenient elsewhere */
static gboolean
gst_mms_query (GstBaseSrc * src, GstQuery * query)
{
  GstMMS *mmssrc = GST_MMS (src);
  gboolean res = TRUE;
  GstFormat format;
  gint64 value;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
      gst_query_parse_position (query, &format, &value);
      if (format != GST_FORMAT_BYTES) {
        res = FALSE;
        break;
      }
      value = (gint64) mmsx_get_current_pos (mmssrc->connection);
      gst_query_set_position (query, format, value);
      break;
    case GST_QUERY_DURATION:
      if (!mmsx_get_seekable (mmssrc->connection)) {
        res = FALSE;
        break;
      }
      gst_query_parse_duration (query, &format, &value);
      switch (format) {
        case GST_FORMAT_BYTES:
          value = (gint64) mmsx_get_length (mmssrc->connection);
          gst_query_set_duration (query, format, value);
          break;
        case GST_FORMAT_TIME:
          value = mmsx_get_time_length (mmssrc->connection) * GST_SECOND;
          gst_query_set_duration (query, format, value);
          break;
        default:
          res = FALSE;
      }
      break;
    default:
      /* chain to parent */
      res =
          GST_BASE_SRC_CLASS (parent_class)->query (GST_BASE_SRC (src), query);
      break;
  }

  return res;
}


static gboolean
gst_mms_prepare_seek_segment (GstBaseSrc * src, GstEvent * event,
    GstSegment * segment)
{
  GstSeekType cur_type, stop_type;
  gint64 cur, stop;
  GstSeekFlags flags;
  GstFormat seek_format;
  gdouble rate;

  gst_event_parse_seek (event, &rate, &seek_format, &flags,
      &cur_type, &cur, &stop_type, &stop);

  if (seek_format != GST_FORMAT_BYTES && seek_format != GST_FORMAT_TIME) {
    GST_LOG_OBJECT (src, "Only byte or time seeking is supported");
    return FALSE;
  }

  if (stop_type != GST_SEEK_TYPE_NONE) {
    GST_LOG_OBJECT (src, "Stop seeking not supported");
    return FALSE;
  }

  if (cur_type != GST_SEEK_TYPE_NONE && cur_type != GST_SEEK_TYPE_SET) {
    GST_LOG_OBJECT (src, "Only absolute seeking is supported");
    return FALSE;
  }

  /* We would like to convert from GST_FORMAT_TIME to GST_FORMAT_BYTES here
     when needed, but we cannot as to do that we need to actually do the seek,
     so we handle this in do_seek instead. */

  /* FIXME implement relative seeking, we could do any needed relevant
     seeking calculations here (in seek_format metrics), before the re-init
     of the segment. */

  gst_segment_init (segment, seek_format);
  gst_segment_set_seek (segment, rate, seek_format, flags, cur_type, cur,
      stop_type, stop, NULL);

  return TRUE;
}

static gboolean
gst_mms_do_seek (GstBaseSrc * src, GstSegment * segment)
{
  gint64 start;
  GstMMS *mmssrc = GST_MMS (src);

  if (segment->format == GST_FORMAT_TIME) {
    if (!mmsx_time_seek (mmssrc->io, mmssrc->connection,
            (double) segment->start / GST_SECOND)) {
      GST_LOG_OBJECT (mmssrc, "mmsx_time_seek() failed");
      return FALSE;
    }
    start = mmsx_get_current_pos (mmssrc->connection);
    GST_INFO_OBJECT (mmssrc, "sought to %" GST_TIME_FORMAT ", offset after "
        "seek: %" G_GINT64_FORMAT, GST_TIME_ARGS (segment->start), start);
  } else if (segment->format == GST_FORMAT_BYTES) {
    start = mmsx_seek (mmssrc->io, mmssrc->connection, segment->start, SEEK_SET);
    /* mmsx_seek will close and reopen the connection when seeking with the
       mmsh protocol, if the reopening fails this is indicated with -1 */
    if (start == -1) {
      GST_DEBUG_OBJECT (mmssrc, "connection broken during seek");
      return FALSE;
    }
    GST_INFO_OBJECT (mmssrc, "sought to: %" G_GINT64_FORMAT " bytes, "
        "result: %" G_GINT64_FORMAT, segment->start, start);
  } else {
    GST_DEBUG_OBJECT (mmssrc, "unsupported seek segment format: %s",
        GST_STR_NULL (gst_format_get_name (segment->format)));
    return FALSE;
  }
  gst_segment_init (segment, GST_FORMAT_BYTES);
  gst_segment_set_seek (segment, segment->rate, GST_FORMAT_BYTES,
      segment->flags, GST_SEEK_TYPE_SET, start, GST_SEEK_TYPE_NONE,
      segment->stop, NULL);
  return TRUE;
}


/* get function
 * this function generates new data when needed
 */


static GstFlowReturn
gst_mms_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstMMS *mmssrc = GST_MMS (psrc);
  guint8 *data;
  guint blocksize;
  gint result;
  mms_off_t offset;

  *buf = NULL;

  offset = mmsx_get_current_pos (mmssrc->connection);

  /* Check if a seek perhaps has wrecked our connection */
  if (offset == -1) {
    GST_ERROR_OBJECT (mmssrc,
        "connection broken (probably an error during mmsx_seek_time during a convert query) returning FLOW_ERROR");
    return GST_FLOW_ERROR;
  }

  /* Choose blocksize best for optimum performance */
  if (offset == 0)
    blocksize = mmsx_get_asf_header_len (mmssrc->connection);
  else
    blocksize = mmsx_get_asf_packet_len (mmssrc->connection);

  *buf = gst_buffer_try_new_and_alloc (blocksize);
  if (!*buf) {
    GST_ERROR_OBJECT (mmssrc, "Failed to allocate %u bytes", blocksize);
    return GST_FLOW_ERROR;
  }

  data = GST_BUFFER_DATA (*buf);
  GST_BUFFER_SIZE (*buf) = 0;
  GST_LOG_OBJECT (mmssrc, "reading %d bytes", blocksize);
  result = mmsx_read (mmssrc->io, mmssrc->connection, (char *) data, blocksize);

  /* EOS? */
  if (result == 0)
    goto eos;

  GST_BUFFER_OFFSET (*buf) = offset;
  GST_BUFFER_SIZE (*buf) = result;

  GST_LOG_OBJECT (mmssrc, "Returning buffer with offset %" G_GINT64_FORMAT
      " and size %u", GST_BUFFER_OFFSET (*buf), GST_BUFFER_SIZE (*buf));

  gst_buffer_set_caps (*buf, GST_PAD_CAPS (GST_BASE_SRC_PAD (mmssrc)));

  return GST_FLOW_OK;

eos:
  {
    GST_DEBUG_OBJECT (mmssrc, "EOS");
    gst_buffer_unref (*buf);
    *buf = NULL;
    return GST_FLOW_UNEXPECTED;
  }
}

static gboolean
gst_mms_is_seekable (GstBaseSrc * src)
{
  GstMMS *mmssrc = GST_MMS (src);

  return mmsx_get_seekable (mmssrc->connection);
}

static gboolean
gst_mms_get_size (GstBaseSrc * src, guint64 * size)
{
  GstMMS *mmssrc = GST_MMS (src);

  /* non seekable usually means live streams, and get_length() returns,
     erm, interesting values for live streams */
  if (!mmsx_get_seekable (mmssrc->connection))
    return FALSE;

  *size = mmsx_get_length (mmssrc->connection);
  return TRUE;
}

static int
_do_select (io_state_t state, void *timeout_data)
{
  GstMMS *mmssrc = (GstMMS *) timeout_data;

  gst_poll_fd_ctl_write(mmssrc->poll, &mmssrc->fdp, FALSE);
  gst_poll_fd_ctl_read(mmssrc->poll, &mmssrc->fdp, FALSE);

  if (state == IO_READY_WRITE) 
    gst_poll_fd_ctl_write(mmssrc->poll, &mmssrc->fdp, TRUE);

  if (state == IO_READY_READ)
    gst_poll_fd_ctl_read(mmssrc->poll, &mmssrc->fdp, TRUE);
  
  return gst_poll_wait(mmssrc->poll, mmssrc->connection_timeout * GST_SECOND);
}

static int
_do_connect (int fd, const struct sockaddr * saptr, int salen, void * timeout_data)
{
  int error, fd_n = 0;
  socklen_t len;

  error = connect(fd, (struct sockaddr *) saptr, salen);
  if (error == 0)
    return 0; /* connect completed immediately no need to wait on select */

  if (error < 0) {
    if (errno != EINPROGRESS) /* error other than async continuation */
      return -1;
  }

  fd_n = _do_select(IO_READY_WRITE, timeout_data);
  if (fd_n == 0) {  /* connection timed out */
    return -1;
  }

  if (fd_n == -1 ) {
    return -1;
  }

  len = sizeof(error);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
    /* error == 0 in case of successful connection otherwise error == errno */
    return -1;
  }

  return (error) ? - 1 : 0;
}

static int
_timedout_tcp_connect (void * data, const char * host, int port)
{
  GstMMS *mmssrc = (GstMMS *) data;
  struct addrinfo *r, *addr_nfo, hints;
  char port_str[16];
  int i, fd;

  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  snprintf(port_str, 16, "%d", port);
  i = getaddrinfo(host, port_str, &hints, &addr_nfo);
  if (i != 0) {
    GST_WARNING_OBJECT(mmssrc, "unable to resolve host: %s\n", host);
    return -1;
  }

  for (r = addr_nfo; r != NULL; r = r->ai_next) {
    fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol);

    if (fd != -1) {
      int flags = fcntl(fd, F_GETFL, 0);
      fcntl(fd, F_SETFL, flags | O_NONBLOCK);

      gst_poll_fd_init(&mmssrc->fdp);
      mmssrc->fdp.fd = fd;
      gst_poll_add_fd(mmssrc->poll, &mmssrc->fdp);

      if (_do_connect(fd, r->ai_addr, r->ai_addrlen, data) != -1) {
        freeaddrinfo(addr_nfo);
        return fd;
      }

      gst_poll_remove_fd(mmssrc->poll, &mmssrc->fdp);
      close(fd);
    }
  }

  freeaddrinfo(addr_nfo);
  return -1;
}

static mms_off_t
_timedout_tcp_read(void *data, int socket, char *buf, mms_off_t num)
{
  int fd_n = 0;
  ssize_t red = 0;
  mms_off_t len = 0;

  while (len < num) {
    fd_n = _do_select(IO_READY_READ, data);
    if (fd_n <= 0) {  /* connection timed out or error (-1)*/
      break;
    }

    red = read(socket, buf + len, num - len);
    if (red == 0)
       break; /* EOF */

    if (red < 0) {
       if(errno == EWOULDBLOCK || errno == EAGAIN)
         continue;
       break;
    }
    len += (mms_off_t) red;
  }

  return len ? len : red;
}

static mms_off_t
_timedout_tcp_write(void *data, int socket, char *buf, mms_off_t num)
{
  int fd_n = 0;
  ssize_t written = 0;
  mms_off_t len = 0;

  while (len < num) {
    fd_n = _do_select(IO_READY_WRITE, data);
    if (fd_n <= 0) {  /* connection timed out (0) or eror (-1) */
      break;
    }

    written = write(socket, buf + len, num - len);
    if (written < 0) {
       if (errno == EWOULDBLOCK || errno == EAGAIN)
         continue;
       break;
    }
    len += (mms_off_t) written;
  }

  return len ? len : written;
}

static gboolean
gst_mms_connect (GstMMS *mms)
{
  guint bandwidth_avail;
  gchar *uri_name = NULL;

  GST_OBJECT_LOCK (mms);
  if (!mms->uri_name || *mms->uri_name == '\0') {
    GST_OBJECT_UNLOCK (mms);
    GST_ELEMENT_ERROR (mms, RESOURCE, OPEN_READ,
        ("No URI to open specified"), (NULL));
    goto error;
  }
  uri_name = g_strdup (mms->uri_name);

  if (mms->connection_speed)
    bandwidth_avail = mms->connection_speed;
  else
    bandwidth_avail = G_MAXINT;

   GST_OBJECT_UNLOCK (mms);
  
  /* If we already have a connection, and the uri isn't changed, reuse it,
     as connecting is expensive. */
  if (mms->connection) {
    if (!strcmp (uri_name, mms->current_connection_uri_name)) {
      GST_DEBUG_OBJECT (mms, "Reusing existing connection for %s",
          uri_name);
      goto success;
    } else {
      mmsx_close (mms->connection);
      g_free (mms->current_connection_uri_name);
      mms->current_connection_uri_name = NULL;
    }
  }

  if (!mms->io) {
    mms->io = g_try_malloc (sizeof(mms_io_t));
    if(!mms->io) {
      GST_ELEMENT_ERROR (mms, RESOURCE, OPEN_READ,
          ("Cannot allocate memory"), (NULL));
      goto error;
    }
    *(mms->io) = *mms_get_default_io_impl();
  }
  mms->io->connect = _timedout_tcp_connect;
  mms->io->connect_data = mms;
  mms->io->write = (mms_io_write_func) _timedout_tcp_write;
  mms->io->write_data = mms;
  mms->io->read = (mms_io_read_func) _timedout_tcp_read;
  mms->io->read_data = mms;

  /* FIXME: pass some sane arguments here */
  GST_DEBUG_OBJECT (mms,
      "Trying mms_connect (%s) with bandwidth constraint of %d bps",
      uri_name, bandwidth_avail);

  mms->connection = mmsx_connect (mms->io, NULL, uri_name, bandwidth_avail);
  if (mms->connection) {
    /* Save the uri name so that it can be checked for connection reusing,
       see above. */
    mms->current_connection_uri_name = g_strdup (uri_name);
    GST_DEBUG_OBJECT (mms, "Connect successful");
    goto success;
  } else {
    gchar *url, *location;

    GST_ERROR_OBJECT (mms,
        "Could not connect to this stream, redirecting to rtsp");
    location = strstr (uri_name, "://");
    if (location == NULL || *location == '\0' || *(location + 3) == '\0') {
      GST_ELEMENT_ERROR (mms, RESOURCE, OPEN_READ,
           ("No URI to open specified"), (NULL));
      goto error;
    }
    url = g_strdup_printf ("rtsp://%s", location + 3);

    gst_element_post_message (GST_ELEMENT_CAST (mms),
        gst_message_new_element (GST_OBJECT_CAST (mms),
            gst_structure_new ("redirect", "new-location", G_TYPE_STRING, url,
                NULL)));

    /* post an error message as well, so that applications that don't handle
     * redirect messages get to see a proper error message */
    GST_ELEMENT_ERROR (mms, RESOURCE, OPEN_READ,
        ("Could not connect to streaming server."),
        ("A redirect message was posted on the bus and should have been "
            "handled by the application."));
  }

error:
    if (uri_name)
      g_free(uri_name);
    return FALSE;

success:
    if (uri_name)
      g_free(uri_name);
    return TRUE;
}

/* the thread where everything happens */
static void
gst_mms_thread (GstMMS *src)
{
  if (src->state == GST_MMS_STATE_CONNECTING) {
    if (gst_mms_connect(src)) {
      src->state = GST_MMS_STATE_CONNECTED;
      GST_STATE (src) = GST_STATE_PAUSED;
      if (mmsx_get_seekable (src->connection))
        GST_STATE_RETURN (src) = GST_STATE_CHANGE_SUCCESS;
      else
        GST_STATE_RETURN (src) = GST_STATE_CHANGE_NO_PREROLL;
    } else {
      src->state = GST_MMS_STATE_CONNECTING;
      GST_STATE (src) = GST_STATE_READY;
      GST_STATE_RETURN (src) = GST_STATE_CHANGE_FAILURE;
    }

    GST_STATE_PENDING (src) = GST_STATE_VOID_PENDING;
    GST_STATE_NEXT (src) = GST_STATE_VOID_PENDING;

    gst_element_post_message (GST_ELEMENT_CAST (src), gst_message_new_async_done (GST_OBJECT_CAST (src)));

    GST_STATE_BROADCAST (src);

    if (src->state == GST_MMS_STATE_CONNECTED)
      GST_ELEMENT_CLASS (parent_class)->change_state (GST_ELEMENT_CAST (src), GST_STATE_CHANGE_READY_TO_PAUSED);

    GST_DEBUG_OBJECT(src, "state broadcasted\n");
    gst_task_pause(src->task);
  }
}

static gboolean
gst_mms_start (GstBaseSrc *bsrc)
{
  GST_DEBUG_OBJECT (GST_MMS (bsrc), "starting");
  return TRUE;
}

static gboolean
gst_mms_stop (GstBaseSrc * bsrc)
{
  GstMMS *mms = GST_MMS (bsrc);

  if (mms->connection != NULL) {
    /* Check if the connection is still pristine, that is if no more then
       just the mmslib cached asf header has been read. If it is still pristine
       preserve it as we often are re-started with the same URL and connecting
       is expensive */
    if (mmsx_get_current_pos (mms->connection) >
        mmsx_get_asf_header_len (mms->connection)) {
      mmsx_close (mms->connection);
      mms->connection = NULL;
      g_free (mms->current_connection_uri_name);
      mms->current_connection_uri_name = NULL;
    }
  }
  return TRUE;
}

static void
gst_mms_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMMS *mmssrc = GST_MMS (object);

  switch (prop_id) {
    case PROP_LOCATION:
      gst_mms_uri_set_uri (GST_URI_HANDLER (mmssrc),
          g_value_get_string (value));
      break;
    case PROP_CONNECTION_SPEED:
      GST_OBJECT_LOCK (mmssrc);
      mmssrc->connection_speed = g_value_get_uint (value) * 1000;
      GST_OBJECT_UNLOCK (mmssrc);
      break;
    case PROP_CONNECTION_TIMEOUT:
      GST_OBJECT_LOCK (mmssrc);
      mmssrc->connection_timeout = g_value_get_uint (value);
      GST_OBJECT_UNLOCK (mmssrc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mms_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMMS *mmssrc = GST_MMS (object);

  GST_OBJECT_LOCK (mmssrc);
  switch (prop_id) {
    case PROP_LOCATION:
      if (mmssrc->uri_name)
        g_value_set_string (value, mmssrc->uri_name);
      break;
    case PROP_CONNECTION_SPEED:
      g_value_set_uint (value, mmssrc->connection_speed / 1000);
      break;
    case PROP_CONNECTION_TIMEOUT:
      g_value_set_uint (value, mmssrc->connection_timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (mmssrc);
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and pad templates
 * register the features
 */
static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "mmssrc", GST_RANK_NONE, GST_TYPE_MMS);
}

static GstURIType
gst_mms_uri_get_type (void)
{
  return GST_URI_SRC;
}

static gchar **
gst_mms_uri_get_protocols (void)
{
  static const gchar *protocols[] = { "mms", "mmsh", "mmst", "mmsu", NULL };

  return (gchar **) protocols;
}

static const gchar *
gst_mms_uri_get_uri (GstURIHandler * handler)
{
  GstMMS *src = GST_MMS (handler);

  return src->uri_name;
}

static gchar *
gst_mms_src_make_valid_uri (const gchar * uri)
{
  gchar *protocol;
  const gchar *colon, *tmp;
  gsize len;

  if (!uri || !gst_uri_is_valid (uri))
    return NULL;

  protocol = gst_uri_get_protocol (uri);

  if ((strcmp (protocol, "mms") != 0) && (strcmp (protocol, "mmsh") != 0) &&
      (strcmp (protocol, "mmst") != 0) && (strcmp (protocol, "mmsu") != 0)) {
    g_free (protocol);
    return FALSE;
  }
  g_free (protocol);

  colon = strstr (uri, "://");
  if (!colon)
    return NULL;

  tmp = colon + 3;
  len = strlen (tmp);
  if (len == 0)
    return NULL;

  /* libmms segfaults if there's no hostname or
   * no / after the hostname
   */
  colon = strstr (tmp, "/");
  if (colon == tmp)
    return NULL;

  if (strstr (tmp, "/") == NULL) {
    gchar *ret;

    len = strlen (uri);
    ret = g_malloc0 (len + 2);
    memcpy (ret, uri, len);
    ret[len] = '/';
    return ret;
  } else {
    return g_strdup (uri);
  }
}

static gboolean
gst_mms_uri_set_uri (GstURIHandler * handler, const gchar * uri)
{
  GstMMS *src = GST_MMS (handler);
  gchar *fixed_uri;

  fixed_uri = gst_mms_src_make_valid_uri (uri);
  if (!fixed_uri && uri)
    return FALSE;

  GST_OBJECT_LOCK (src);
  if (src->uri_name)
    g_free (src->uri_name);
  src->uri_name = fixed_uri;
  GST_OBJECT_UNLOCK (src);

  return TRUE;
}

static void
gst_mms_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_mms_uri_get_type;
  iface->get_protocols = gst_mms_uri_get_protocols;
  iface->get_uri = gst_mms_uri_get_uri;
  iface->set_uri = gst_mms_uri_set_uri;
}

static GstStateChangeReturn
gst_mms_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn result;
  GstMMS *src = GST_MMS(element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      src->state = GST_MMS_STATE_INIT;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      src->state = GST_MMS_STATE_CONNECTING;
      GST_OBJECT_LOCK (src);
      if (src->task == NULL) {
        src->task = gst_task_create ((GstTaskFunction) gst_mms_thread, src);
        if (src->task == NULL)
          goto failure;

        gst_task_set_lock (src->task, GST_MMS_STREAM_GET_LOCK (src));
      }
      GST_OBJECT_UNLOCK (src);
      gst_task_start(src->task);
      goto async;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  if ((result =
          GST_ELEMENT_CLASS (parent_class)->change_state (element,
              transition)) == GST_STATE_CHANGE_FAILURE)
    goto failure;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
      if (src->task != NULL) {
        GST_DEBUG_OBJECT (element, "flushing!!!");
        src->state = GST_MMS_STATE_FLUSHING;
        gst_poll_set_flushing(src->poll, TRUE);
	g_static_rec_mutex_lock(GST_MMS_STREAM_GET_LOCK (src));
        gst_task_stop(src->task);
	g_static_rec_mutex_unlock(GST_MMS_STREAM_GET_LOCK (src));
        GST_DEBUG_OBJECT (element, "Connection task is stopping");
        gst_task_join(src->task);
        GST_DEBUG_OBJECT (element, "Connection task stopped");
        gst_object_unref(src->task);
        src->task = NULL;
      }
      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
      src->state = GST_MMS_STATE_INIT;
      break;
    default:
      break;
  }

  return result;

  /* ERRORS */
failure:
  {
    GST_DEBUG_OBJECT (element, "parent failed state change");
    return result;
  }
async:
  {
    GST_DEBUG_OBJECT (element, "ASYNC!!!");
    return GST_STATE_CHANGE_ASYNC;
  }
}


/* this is the structure that gst-register looks for
 * so keep the name plugin_desc, or you cannot get your plug-in registered */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "mms",
    "Microsoft Multi Media Server streaming protocol support",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
