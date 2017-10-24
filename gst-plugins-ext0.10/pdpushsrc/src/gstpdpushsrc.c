/*
 * pdpushsrc
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Seungbae Shin <seungbae.shin@samsung.com>
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
#  include "config.h"
#endif

#include <gst/gst.h>
#include "gstpdpushsrc.h"
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef G_OS_WIN32
#include <io.h>                 /* lseek, open, close, read */
/* On win32, stat* default to 32 bit; we need the 64-bit
 * variants, so explicitly define it that way. */
#define stat __stat64
#define fstat _fstat64
#undef lseek
#define lseek _lseeki64
#undef off_t
#define off_t guint64
/* Prevent stat.h from defining the stat* functions as
 * _stat*, since we're explicitly overriding that */
#undef _INC_STAT_INL
#endif
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include <errno.h>
#include <string.h>

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

/* FIXME we should be using glib for this */
#ifndef S_ISREG
#define S_ISREG(mode) ((mode)&_S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode)&_S_IFDIR)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(x) (0)
#endif
#ifndef O_BINARY
#define O_BINARY (0)
#endif

static int
file_open (const gchar * filename, int flags, int mode)
{
  return open (filename, flags, mode);
}

GST_DEBUG_CATEGORY_STATIC (gst_pd_pushsrc_debug);
#define GST_CAT_DEFAULT gst_pd_pushsrc_debug

/* FileSrc signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

#define DEFAULT_BLOCKSIZE       4*1024

enum
{
  ARG_0,
  ARG_LOCATION,
  ARG_EOS,
};

static void gst_pd_pushsrc_finalize (GObject * object);

static void gst_pd_pushsrc_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_pd_pushsrc_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_pd_pushsrc_start (GstBaseSrc * basesrc);
static gboolean gst_pd_pushsrc_stop (GstBaseSrc * basesrc);

static gboolean gst_pd_pushsrc_is_seekable (GstBaseSrc * src);
static gboolean gst_pd_pushsrc_get_size (GstBaseSrc * src, guint64 * size);
static gboolean gst_pd_pushsrc_query (GstBaseSrc * src, GstQuery * query);
static void gst_pd_pushsrc_uri_handler_init (gpointer g_iface, gpointer iface_data);
static GstFlowReturn gst_pd_pushsrc_create (GstBaseSrc * basesrc, guint64 offset, guint length, GstBuffer ** buffer);
static gboolean gst_pd_pushsrc_checkgetrange (GstPad * pad);

static void
_do_init (GType pd_pushsrc_type)
{
  static const GInterfaceInfo urihandler_info = {
    gst_pd_pushsrc_uri_handler_init,
    NULL,
    NULL
  };

  g_type_add_interface_static (pd_pushsrc_type, GST_TYPE_URI_HANDLER, &urihandler_info);
  GST_DEBUG_CATEGORY_INIT (gst_pd_pushsrc_debug, "pdpushsrc", 0, "PD push source element");
}

GST_BOILERPLATE_FULL (GstPDPushSrc, gst_pd_pushsrc, GstBaseSrc, GST_TYPE_BASE_SRC, _do_init);

static void
gst_pd_pushsrc_base_init (gpointer g_class)
{
  GST_LOG ("IN");

  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (gstelement_class,
      "PD push source",
      "Source/File",
      "Read from arbitrary point in a file",
      "Naveen Ch <naveen.ch@samsung.com>");
  gst_element_class_add_pad_template (gstelement_class, gst_static_pad_template_get (&srctemplate));

  GST_LOG ("OUT");
}

static void
gst_pd_pushsrc_class_init (GstPDPushSrcClass * klass)
{
  GST_LOG ("IN");

  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS (klass);

  gobject_class->set_property = gst_pd_pushsrc_set_property;
  gobject_class->get_property = gst_pd_pushsrc_get_property;
  gobject_class->finalize = gst_pd_pushsrc_finalize;

  g_object_class_install_property (gobject_class, ARG_LOCATION,
      g_param_spec_string ("location", "File Location",
          "Location of the file to read", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));
   
    g_object_class_install_property (gobject_class, ARG_EOS,
                                    g_param_spec_boolean ("eos",
                                        "EOS recived on downloading pipeline",
                                        "download of clip is over",
                                        0,
                                        G_PARAM_READWRITE));
   
  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_pd_pushsrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_pd_pushsrc_stop);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_pd_pushsrc_is_seekable);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_pd_pushsrc_get_size);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_pd_pushsrc_query);
  gstbasesrc_class->create = GST_DEBUG_FUNCPTR (gst_pd_pushsrc_create);

  if (sizeof (off_t) < 8) {
    GST_LOG ("No large file support, sizeof (off_t) = %" G_GSIZE_FORMAT "!",
        sizeof (off_t));
  }

  GST_LOG ("OUT");
 
}

static void
gst_pd_pushsrc_init (GstPDPushSrc * src, GstPDPushSrcClass * g_class)
{
  GST_LOG ("IN");
  GstBaseSrc *basesrc = GST_BASE_SRC (src);

  src->filename = NULL;
  src->fd = 0;
  src->uri = NULL;
  src->is_regular = FALSE;
  src->is_eos = FALSE;
  
  gst_pad_set_checkgetrange_function (basesrc->srcpad, GST_DEBUG_FUNCPTR (gst_pd_pushsrc_checkgetrange));

  GST_LOG ("OUT");
}

static void
gst_pd_pushsrc_finalize (GObject * object)
{
  GST_LOG ("IN");

  GstPDPushSrc *src;

  src = GST_PD_PUSHSRC (object);

  g_free (src->filename);
  g_free (src->uri);

  G_OBJECT_CLASS (parent_class)->finalize (object);

  GST_LOG ("OUT");
}

static gboolean
gst_pd_pushsrc_set_location (GstPDPushSrc * src, const gchar * location)
{
  GST_LOG ("IN");

  GstState state;

  /* the element must be stopped in order to do this */
  GST_OBJECT_LOCK (src);
  state = GST_STATE (src);
  if (state != GST_STATE_READY && state != GST_STATE_NULL)
    goto wrong_state;
  GST_OBJECT_UNLOCK (src);

  g_free (src->filename);
  g_free (src->uri);

  /* clear the filename if we get a NULL (is that possible?) */
  if (location == NULL) {
    src->filename = NULL;
    src->uri = NULL;
  } else {
    /* we store the filename as received by the application. On Windows this
     * should be UTF8 */
    src->filename = g_strdup (location);
    src->uri = gst_filename_to_uri (location, NULL);
    GST_INFO ("filename : %s", src->filename);
    GST_INFO ("uri      : %s", src->uri);
  }
  g_object_notify (G_OBJECT (src), "location");
  gst_uri_handler_new_uri (GST_URI_HANDLER (src), src->uri);
  
  GST_LOG ("OUT");

  return TRUE;

  /* ERROR */
wrong_state:
  {
    g_warning ("Changing the `location' property on filesrc when a file is "
        "open is not supported.");
    GST_OBJECT_UNLOCK (src);
    GST_LOG ("OUT :: wrong_state");
    return FALSE;
  }
}

static void
gst_pd_pushsrc_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GST_LOG ("IN");

  GstPDPushSrc *src;

  g_return_if_fail (GST_IS_PD_PUSHSRC (object));

  src = GST_PD_PUSHSRC (object);

  switch (prop_id) {
    case ARG_LOCATION:
      gst_pd_pushsrc_set_location (src, g_value_get_string (value));
      break;
    case ARG_EOS:
      src->is_eos = g_value_get_boolean (value);
      g_print ("\n\n\nis_eos is becoming %d\n\n\n", src->is_eos);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_LOG ("OUT");
}

static void
gst_pd_pushsrc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GST_LOG ("IN");
  GstPDPushSrc *src;

  g_return_if_fail (GST_IS_PD_PUSHSRC (object));

  src = GST_PD_PUSHSRC (object);

  switch (prop_id) {
    case ARG_LOCATION:
      g_value_set_string (value, src->filename);
      break;
    case ARG_EOS:
      g_value_set_boolean (value, src->is_eos);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_LOG ("OUT");
}

static GstFlowReturn
gst_pd_pushsrc_create_read (GstBaseSrc * basesrc, guint64 offset, guint length, GstBuffer ** buffer)
{
  GST_LOG ("IN");

  int ret;
  GstBuffer *buf = NULL;
  struct stat stat_results;
  GstPDPushSrc *src;

  if(length < 1)
    return GST_FLOW_ERROR;

  src = GST_PD_PUSHSRC_CAST (basesrc);
  
  GST_LOG_OBJECT (src, "read position = %"G_GUINT64_FORMAT ", offset = %"G_GUINT64_FORMAT", length = %d", 
    src->read_position, offset, length);
  
  memset (&stat_results, 0, sizeof (stat_results));
  
  if (fstat (src->fd, &stat_results) < 0)
    goto could_not_stat;

  GST_LOG_OBJECT (src, "offset + length = %"G_GUINT64_FORMAT " and filesize = %"G_GUINT64_FORMAT, offset + length, stat_results.st_size);

  while ((offset + length) > stat_results.st_size)
  {
    fd_set fds;
    int ret;
    struct timeval timeout = {0,};
    guint64 avail_size = 0;

    if (src->is_eos)
  	goto eos;

    FD_ZERO (&fds);
    FD_SET (src->fd, &fds);

    timeout.tv_sec = 0;
    timeout.tv_usec = ((basesrc->blocksize * 8 * 1000) / 64000); // wait_time = (blocksize * 8) / (min downloadratei.e. 64Kbps)

    GST_DEBUG_OBJECT (src, "Going to wait for %ld msec", timeout.tv_usec);

    ret = select (src->fd + 1, &fds, NULL, NULL, &timeout);
    if (-1 == ret) 
    {
      GST_ERROR_OBJECT (src, "ERROR in select () : reason - %s...\n", strerror(errno));
      return GST_FLOW_ERROR;
    }
    else if (0 == ret)
    {
      GST_WARNING_OBJECT (src, "select () timeout happened...");
    }
    else
    {
      memset (&stat_results, 0, sizeof (stat_results));
  
      if (fstat (src->fd, &stat_results) < 0)
        goto could_not_stat;

      avail_size = stat_results.st_size;

      GST_LOG_OBJECT (src, "Available data size in file = %"G_GUINT64_FORMAT, avail_size);

      if ((offset + length) > avail_size)
      {
        GST_LOG_OBJECT (src, "Enough data is NOT available...");
      }
      else
      {
        GST_LOG_OBJECT (src, "Enough data is available...");
      }
    }
  }

  if (G_UNLIKELY (src->read_position != offset)) {
    off_t res;

    res = lseek (src->fd, offset, SEEK_SET);
    if (G_UNLIKELY (res < 0 || res != offset))
      goto seek_failed;
    src->read_position = offset;
  }
  
  buf = gst_buffer_try_new_and_alloc (length);
  if (G_UNLIKELY (buf == NULL)) {
    GST_ERROR_OBJECT (src, "Failed to allocate %u bytes", length);
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (src, "Reading %d bytes at offset 0x%" G_GINT64_MODIFIER "x",
      length, offset);
  ret = read (src->fd, GST_BUFFER_DATA (buf), length);
  if (G_UNLIKELY (ret < 0))
    goto could_not_read;

  /* seekable regular files should have given us what we expected */
  if (G_UNLIKELY ((guint) ret < length && src->seekable))
    goto unexpected_eos;

  /* other files should eos if they read 0 and more was requested */
  if (G_UNLIKELY (ret == 0 && length > 0))
    goto eos;

  length = ret;
  GST_BUFFER_SIZE (buf) = length;
  GST_BUFFER_OFFSET (buf) = offset;
  GST_BUFFER_OFFSET_END (buf) = offset + length;

  src->read_position += length;

  *buffer = buf;
  GST_LOG ("OUT");

  return GST_FLOW_OK;

  /* ERROR */
seek_failed:
  {
    GST_ERROR_OBJECT (src, "Seek failed...");
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    return GST_FLOW_ERROR;
  }
could_not_stat:
  {
    GST_ERROR_OBJECT (src, "Could not stat");
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    return GST_FLOW_ERROR;
  }
could_not_read:
  {
    GST_ERROR_OBJECT (src, "Could not read...");
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), GST_ERROR_SYSTEM);
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
unexpected_eos:
  {
    GST_ERROR_OBJECT (src, "Unexpected EOS occured...");
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), ("unexpected end of file."));
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
eos:
  {
    GST_ERROR_OBJECT (src, "non-regular file hits EOS");
    if (buf)
      gst_buffer_unref (buf);
    return GST_FLOW_UNEXPECTED;
  }
}

static GstFlowReturn
gst_pd_pushsrc_create (GstBaseSrc * basesrc, guint64 offset, guint length, GstBuffer ** buffer)
{
  GST_LOG ("IN");

  GstPDPushSrc *pdsrc;
  GstFlowReturn ret;

  pdsrc = GST_PD_PUSHSRC_CAST (basesrc);
  ret = gst_pd_pushsrc_create_read (basesrc, offset, length, buffer);
  GST_LOG ("OUT");

  return ret;
}

static gboolean
gst_pd_pushsrc_query (GstBaseSrc * basesrc, GstQuery * query)
{
  GST_LOG ("IN");

  gboolean ret = FALSE;
  GstPDPushSrc *src = GST_PD_PUSHSRC (basesrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
      gst_query_set_uri (query, src->uri);
      ret = TRUE;
      break;
    default:
      ret = FALSE;
      break;
  }

  if (!ret)
    ret = GST_BASE_SRC_CLASS (parent_class)->query (basesrc, query);
  GST_LOG ("OUT");

  return ret;
}

static gboolean
gst_pd_pushsrc_checkgetrange (GstPad * pad)
{
  GST_LOG ("IN");

  GST_LOG ("OUT");

  return FALSE;
}

static gboolean
gst_pd_pushsrc_is_seekable (GstBaseSrc * basesrc)
{
  GST_LOG ("IN");

  GstPDPushSrc *src = GST_PD_PUSHSRC (basesrc);

  GST_DEBUG_OBJECT (src, "seekable = %d", src->seekable);
  GST_LOG ("OUT");

  return src->seekable;

}

static gboolean
gst_pd_pushsrc_get_size (GstBaseSrc * basesrc, guint64 * size)
{
  GST_LOG ("IN");

  struct stat stat_results;
  GstPDPushSrc *src;

  src = GST_PD_PUSHSRC (basesrc);

  if (!src->seekable) {
    /* If it isn't seekable, we won't know the length (but fstat will still
     * succeed, and wrongly say our length is zero. */
    return FALSE;
  }

  if (fstat (src->fd, &stat_results) < 0)
    goto could_not_stat;

  //*size = stat_results.st_size;
  /* Naveen : Intentionally, doing this because we dont know the file size...because its keep on increasing in PD case */
  *size = G_MAXUINT64;

  GST_DEBUG ("size of the file = %"G_GUINT64_FORMAT, *size);
  
  GST_LOG ("OUT");

  return TRUE;

  /* ERROR */
could_not_stat:
  {
    GST_ERROR_OBJECT (src, "Could not stat");
    return FALSE;
  }
}

/* open the file, necessary to go to READY state */
static gboolean
gst_pd_pushsrc_start (GstBaseSrc * basesrc)
{
  GST_LOG ("IN");

  GstPDPushSrc *src = GST_PD_PUSHSRC (basesrc);
  struct stat stat_results;

  if (src->filename == NULL || src->filename[0] == '\0')
    goto no_filename;

  GST_INFO_OBJECT (src, "opening file %s", src->filename);

  /* open the file */
  src->fd = file_open (src->filename, O_RDONLY | O_BINARY, 0);

  if (src->fd < 0)
    goto open_failed;

  /* check if it is a regular file, otherwise bail out */
  if (fstat (src->fd, &stat_results) < 0)
    goto no_stat;

  if (S_ISDIR (stat_results.st_mode))
    goto was_directory;

  if (S_ISSOCK (stat_results.st_mode))
    goto was_socket;

  src->read_position = 0;

  /* record if it's a regular (hence seekable and lengthable) file */
  if (S_ISREG (stat_results.st_mode))
    src->is_regular = TRUE;

  {
    /* we need to check if the underlying file is seekable. */
    off_t res = lseek (src->fd, 0, SEEK_END);

    if (res < 0) {
      GST_LOG_OBJECT (src, "disabling seeking, not in mmap mode and lseek "
          "failed: %s", g_strerror (errno));
      src->seekable = FALSE;
    } else {
      src->seekable = TRUE;
    }
    lseek (src->fd, 0, SEEK_SET);
  }

  /* We can only really do seeking on regular files - for other file types, we
   * don't know their length, so seeking isn't useful/meaningful */
  src->seekable = src->seekable && src->is_regular;
  GST_LOG ("OUT");

  return TRUE;

  /* ERROR */
no_filename:
  {
    GST_ERROR_OBJECT (src, "No file name specified for reading...");
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
        ("No file name specified for reading."), (NULL));
    return FALSE;
  }
open_failed:
  {
    switch (errno) {
      case ENOENT:
        GST_ERROR_OBJECT (src, "File could not be found");
        GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, (NULL),
            ("No such file \"\""));
        break;
      default:
        GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
            ("Could not open file for reading."),
            GST_ERROR_SYSTEM);
        break;
    }
    return FALSE;
  }
no_stat:
  {
    GST_ERROR_OBJECT (src, "Could not get stat info...");	
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("Could not get info on \"\"."),  (NULL));
    close (src->fd);
    return FALSE;
  }
was_directory:
  {
    GST_ERROR_OBJECT (src, "Is a Directory");
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("\"\" is a directory."), (NULL));
    close (src->fd);
    return FALSE;
  }
was_socket:
  {
   GST_ERROR_OBJECT (src, "Is a Socket");	
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("File \"\" is a socket."), (NULL));
    close (src->fd);
    return FALSE;
  }
}

/* unmap and close the file */
static gboolean
gst_pd_pushsrc_stop (GstBaseSrc * basesrc)
{
  GST_LOG ("IN");

  GstPDPushSrc *src = GST_PD_PUSHSRC (basesrc);

  /* close the file */
  close (src->fd);

  /* zero out a lot of our state */
  src->fd = 0;
  src->is_regular = FALSE;

  GST_LOG ("OUT");

  return TRUE;
}

/*** GSTURIHANDLER INTERFACE *************************************************/

static GstURIType
gst_pd_pushsrc_uri_get_type (void)
{
  GST_LOG ("IN");
  GST_LOG ("OUT");

  return GST_URI_SRC;
}

static gchar **
gst_pd_pushsrc_uri_get_protocols (void)
{
  GST_LOG ("IN");
  static gchar *protocols[] = { (char *) "file", NULL };
  GST_LOG ("OUT");

  return protocols;
}

static const gchar *
gst_pd_pushsrc_uri_get_uri (GstURIHandler * handler)
{
  GST_LOG ("IN");
  GstPDPushSrc *src = GST_PD_PUSHSRC (handler);
  GST_LOG ("OUT");

  return src->uri;
}

static gboolean
gst_pd_pushsrc_uri_set_uri (GstURIHandler * handler, const gchar * uri)
{
  GST_LOG ("IN");

  gchar *location, *hostname = NULL;
  gboolean ret = FALSE;
  GstPDPushSrc *src = GST_PD_PUSHSRC (handler);
  GError *error = NULL;

  if (strcmp (uri, "file://") == 0) {
    /* Special case for "file://" as this is used by some applications
     *  to test with gst_element_make_from_uri if there's an element
     *  that supports the URI protocol. */
    gst_pd_pushsrc_set_location (src, NULL);
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
#ifdef G_OS_WIN32
  /* Unfortunately, g_filename_from_uri() doesn't handle some UNC paths
   * correctly on windows, it leaves them with an extra backslash
   * at the start if they're of the mozilla-style file://///host/path/file 
   * form. Correct this.
   */
  if (location[0] == '\\' && location[1] == '\\' && location[2] == '\\')
    g_memmove (location, location + 1, strlen (location + 1) + 1);
#endif

  ret = gst_pd_pushsrc_set_location (src, location);

  GST_LOG ("OUT");

beach:
  if (location)
    g_free (location);
  if (hostname)
    g_free (hostname);

  return ret;
}

static void
gst_pd_pushsrc_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GST_LOG ("IN");
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_pd_pushsrc_uri_get_type;
  iface->get_protocols = gst_pd_pushsrc_uri_get_protocols;
  iface->get_uri = gst_pd_pushsrc_uri_get_uri;
  iface->set_uri = gst_pd_pushsrc_uri_set_uri;
  GST_LOG ("OUT");
}

static gboolean
gst_pd_pushsrc_plugin_init (GstPlugin *plugin)
{
    if (!gst_element_register (plugin, "pdpushsrc", GST_RANK_NONE, gst_pd_pushsrc_get_type()))
    {
        return FALSE;
    }
    return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
                   GST_VERSION_MINOR,
                   "pdpushsrc",
                   "PD push source",
                   gst_pd_pushsrc_plugin_init,
                   VERSION,
                   "LGPL",
                   "Samsung Electronics Co",
                   "http://www.samsung.com")


