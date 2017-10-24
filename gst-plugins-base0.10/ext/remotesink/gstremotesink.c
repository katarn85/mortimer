/*  GStreamer remote sink class
 *  Copyright (C) <2014> Liu Yang(yang010.liu@samsung.com)
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
#include "config.h"
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <stdarg.h>
#include <limits.h>
#include "gstremotesink.h"

enum
{
  PROP_0,
  PROP_REMOTE_IP,
  PROP_REMOTE_PORT
};

#define REMOTE_IP_SIZE (128)

#define REMOTE_INVALID_PORT (-1)
#define REMOTE_INVALID_HANDLE (-1)

//#define REMOTE_DEFAULT_IP ("10.123.175.123")
#define REMOTE_DEFAULT_IP ("109.123.121.163")
#define REMOTE_DEFAULT_PORT (40002)

struct _GstRemoteSinkPrivate
{
	gchar remote_ip[REMOTE_IP_SIZE];
	gint remote_port;
	gint remote_handle;
	struct sockaddr_in remote_addr;
	gboolean session_created;
};

GST_DEBUG_CATEGORY_STATIC (remote_sink_debug);
#define GST_CAT_DEFAULT remote_sink_debug

static GstBaseSinkClass *parent_class = NULL;

static void gst_remote_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_remote_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_remote_sink_preroll (GstBaseSink * bsink,
    GstBuffer * buf);
static GstFlowReturn gst_remote_sink_render (GstBaseSink * bsink,
    GstBuffer * buf);
static gboolean gst_remote_sink_event(GstBaseSink *bsink, GstEvent *event);

static GstStateChangeReturn
gst_remote_sink_change_state (GstElement * element, GstStateChange transition);
static gboolean 
gst_remote_sink_remote_push_internal(GstRemoteSink *remote_sink, gpointer buffer, gint push_size, gint *real_size);
static gboolean
gst_remote_sink_remote_pull_internal(GstRemoteSink *remote_sink, gpointer buffer, gint pull_size, gint *real_size);
static gboolean
gst_remote_sink_create_session(GstRemoteSink *remote_sink);
static gboolean
gst_remote_sink_destroy_session(GstRemoteSink *remote_sink);
static gboolean
gst_remote_sink_prepare_network(GstRemoteSink *remote_sink);
static gboolean
gst_remote_sink_unprepare_network(GstRemoteSink *remote_sink);
static void
gst_remote_sink_prepare_network_configure(GstRemoteSink *remote_sink);

static GstStateChangeReturn
gst_remote_sink_change_state (GstElement * element, GstStateChange transition)
{
	gint result = TRUE;
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  	GstRemoteSink *remote_sink = GST_REMOTE_SINK (element);

	switch (transition) {
    	case GST_STATE_CHANGE_NULL_TO_READY:
      		break;
    	case GST_STATE_CHANGE_READY_TO_PAUSED:
			result = gst_remote_sink_create_session(remote_sink);
      		break;
    	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
	  		break;
		default:
			break;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

	switch(transition) {
		case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			gst_remote_sink_destroy_session(remote_sink);
			break;
		case GST_STATE_CHANGE_READY_TO_NULL:
			break;
		default:
			break;
    }

	if(result == FALSE)
		ret = GST_STATE_CHANGE_FAILURE;
	
	return ret;
}

static gboolean 
gst_remote_sink_remote_push_internal(GstRemoteSink *remote_sink, gpointer buffer, gint push_size, gint *real_size)
{
	gboolean ret = TRUE;
	gint remain_size = push_size, sent_size = 0, total_size = 0;
	GstRemoteSinkPrivate *remote_sink_priv = remote_sink->priv;

	g_return_val_if_fail((remote_sink_priv != NULL), FALSE);
	g_return_val_if_fail((push_size > 0), FALSE);

	while(remain_size > 0) {
		sent_size = send(remote_sink_priv->remote_handle, buffer + total_size, remain_size, 0);
		if(sent_size <= 0) {
			ret = FALSE;
			break;
		}
		total_size += sent_size;
		remain_size -= sent_size;
	}

	if(real_size)
		*real_size = total_size;

	return ret;
}

static gboolean
gst_remote_sink_remote_pull_internal(GstRemoteSink *remote_sink, gpointer buffer, gint pull_size, gint *real_size)
{
	gboolean ret = TRUE;
	gint remain_size = pull_size, recv_size = 0, total_size = 0;
	GstRemoteSinkPrivate *remote_sink_priv = remote_sink->priv;

	g_return_val_if_fail((remote_sink_priv != NULL), FALSE);
	g_return_val_if_fail((pull_size > 0), FALSE);

	while(remain_size > 0) {
		recv_size = recv(remote_sink_priv->remote_handle, buffer + total_size, remain_size, 0);
		if(recv_size <= 0) {
			ret = FALSE;
			break;
		}
		total_size += recv_size;
		remain_size -= recv_size;
	}

	if(real_size)
		*real_size = total_size;

	return ret;
}

static gboolean
gst_remote_sink_create_session(GstRemoteSink *remote_sink)
{
	gboolean ret = TRUE;
	
	GstRemoteSinkPrivate *remote_sink_priv = NULL;

	g_return_val_if_fail((remote_sink != NULL), FALSE);
	remote_sink_priv = remote_sink->priv;
	g_return_val_if_fail((remote_sink_priv != NULL), FALSE);
	
	if(remote_sink_priv->session_created == TRUE) {
		g_print("remote sink session has been created before!\n");
		return TRUE;
	}

	ret = gst_remote_sink_prepare_network(remote_sink);
	if(ret == FALSE)
		g_print("please check network environment is well, and remote audio server whether is working fine!\n");
	
	g_return_val_if_fail((ret == TRUE), FALSE); //high, big, up??? @_@, I do not think so actually, just different code style
	
	remote_sink_priv->session_created = TRUE;

	return TRUE;
}

static gboolean
gst_remote_sink_destroy_session(GstRemoteSink *remote_sink)
{
	GstRemoteSinkPrivate *remote_sink_priv = NULL;
	
	g_return_val_if_fail((remote_sink != NULL), FALSE);
	remote_sink_priv = remote_sink->priv;
	g_return_val_if_fail((remote_sink_priv != NULL), FALSE);

	if(remote_sink_priv->session_created == FALSE) {
		g_print("remote sink session has been destroyed before!\n");
		return TRUE;
	}

	gst_remote_sink_unprepare_network(remote_sink);
	
	remote_sink_priv->session_created = FALSE;

	return TRUE;
}

static gboolean
gst_remote_sink_prepare_network(GstRemoteSink *remote_sink)
{
	gint resume_addr_flag = 1;
	gint tcp_no_dealy = 1;
	gint remote_handle = REMOTE_INVALID_HANDLE;
	gint size = 0;
	
	g_return_val_if_fail((remote_sink != NULL), FALSE);
	
	remote_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(remote_handle < 0) {
		remote_handle = REMOTE_INVALID_HANDLE;
		g_print("SOCKET CREATE ERROR!\n");
		goto FAIL;
	}
	
	g_return_val_if_fail((remote_handle > 0), FALSE);

	if (fcntl(remote_handle, F_SETFL, fcntl(remote_handle, F_GETFL) & (~O_NONBLOCK)) != 0) {
		g_print("SET SOCKET NONBLOCK FAIL!\n");
		goto FAIL;
	}

	if (setsockopt(remote_handle, SOL_SOCKET, SO_REUSEADDR, (char *)(&resume_addr_flag), sizeof(int)) != 0) {
		g_print("SET SOCKET REUSEADDR FAIL!\n");
		goto FAIL;
	}

	if (setsockopt(remote_handle, IPPROTO_TCP, tcp_no_dealy, (char *)(&tcp_no_dealy), sizeof(int)) != 0) {
		g_print("SET SOCKET TCP NO DELAY FAIL!\n");
		goto FAIL;
	}

	gst_remote_sink_prepare_network_configure(remote_sink);
	
	remote_sink->priv->remote_addr.sin_family = AF_INET;
	remote_sink->priv->remote_addr.sin_addr.s_addr = inet_addr(remote_sink->priv->remote_ip);
	remote_sink->priv->remote_addr.sin_port = htons(remote_sink->priv->remote_port);
	size = sizeof(remote_sink->priv->remote_addr);
	
	if (connect(remote_handle, (struct sockaddr*)(&(remote_sink->priv->remote_addr)), size) < 0) {
		g_print("CONNECT TO REMOTE AUDIO SERVER FAIL!\n");
		goto FAIL;
	}

	remote_sink->priv->remote_handle = remote_handle;
	g_print("REMOTE SINK PREPARE NETWORK SUCCESS!\n");
	
	return TRUE;
FAIL:
	if(REMOTE_INVALID_HANDLE != remote_handle)
	{
		close(remote_handle);
		remote_handle = REMOTE_INVALID_HANDLE;
	}
	return FALSE;
}

static gboolean
gst_remote_sink_unprepare_network(GstRemoteSink *remote_sink)
{
	g_return_val_if_fail((remote_sink != NULL), FALSE);

	if(REMOTE_INVALID_HANDLE != remote_sink->priv->remote_handle)
	{
		close(remote_sink->priv->remote_handle);
		remote_sink->priv->remote_handle = REMOTE_INVALID_HANDLE;
	}

	return TRUE;
}

static void
gst_remote_sink_prepare_network_configure(GstRemoteSink *remote_sink)
{
	if((remote_sink != NULL) && (remote_sink->priv != NULL)) {
		if((remote_sink->priv->remote_port == REMOTE_INVALID_PORT) ||
			(remote_sink->priv->remote_ip[0] == '\0')) {
			remote_sink->priv->remote_port = REMOTE_DEFAULT_PORT;
			memset(remote_sink->priv->remote_ip, 0x0, REMOTE_IP_SIZE);
			strncpy(remote_sink->priv->remote_ip, REMOTE_DEFAULT_IP, strlen(REMOTE_DEFAULT_IP));
			g_print("REMOTE SINK PREPARE NETWORK CONFIGURE SUCCESS!\n");
		}
	}
}


static void
gst_remote_sink_init (GstRemoteSink * remotesink)
{
  remotesink->priv = G_TYPE_INSTANCE_GET_PRIVATE (remotesink,
      GST_TYPE_REMOTE_SINK, GstRemoteSinkPrivate);
  memset(remotesink->priv->remote_ip, 0x0, REMOTE_IP_SIZE);
  remotesink->priv->remote_port = REMOTE_INVALID_PORT;
  remotesink->priv->remote_handle = REMOTE_INVALID_HANDLE;
}

static void
gst_remote_sink_class_init (GstRemoteSinkClass * klass)
{
  GstBaseSinkClass *basesink_class = (GstBaseSinkClass *) klass;
  GstElementClass *gelement_class = (GstElementClass *) klass;
  GObjectClass *gobject_class = (GObjectClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_remote_sink_set_property;
  gobject_class->get_property = gst_remote_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_REMOTE_IP,
      g_param_spec_string ("remote-address", "remote device ip address",
          "remote device ip address",
          REMOTE_DEFAULT_IP,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_REMOTE_PORT,
      g_param_spec_int ("remote-port", "remote device port",
          "remote device port", 0, G_MAXINT,
          REMOTE_DEFAULT_PORT,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  gelement_class->change_state = GST_DEBUG_FUNCPTR(gst_remote_sink_change_state);
  basesink_class->render = GST_DEBUG_FUNCPTR (gst_remote_sink_render);
  basesink_class->preroll =
      GST_DEBUG_FUNCPTR (gst_remote_sink_preroll);
  basesink_class->event = GST_DEBUG_FUNCPTR (gst_remote_sink_event);

  g_type_class_add_private (klass, sizeof (GstRemoteSinkPrivate));
}

static void
gst_remote_sink_base_init (gpointer g_class)
{
  GST_DEBUG_CATEGORY_INIT (remote_sink_debug, "remotesink", 0, "GstRemoteSink");
}

static GstFlowReturn
gst_remote_sink_preroll (GstBaseSink * bsink, GstBuffer * buf)
{
  GstRemoteSinkClass *klass;

  klass = GST_REMOTE_SINK_GET_CLASS (bsink);

  if (klass->remote_render == NULL) {
  	 if(parent_class->render != NULL)
	 	return parent_class->render(bsink, buf);
	 else
	 	return GST_FLOW_OK; //this case, it is similar with fake sink, yang010.liu added
  }
  
  return klass->remote_render (GST_REMOTE_SINK_CAST (bsink), buf);
}

static GstFlowReturn
gst_remote_sink_render (GstBaseSink * bsink, GstBuffer * buf)
{
  GstRemoteSinkClass *klass = NULL;

  klass = GST_REMOTE_SINK_GET_CLASS (bsink);

  if (klass->remote_render == NULL) {
    if (parent_class->render != NULL)
      return parent_class->render (bsink, buf);
    else
      return GST_FLOW_OK; //this case, it is similar with fake sink, yang010.liu added
  }

  return klass->remote_render (GST_REMOTE_SINK_CAST (bsink), buf);
}

static gboolean gst_remote_sink_event(GstBaseSink *bsink, GstEvent *event)
{
	GstRemoteSink *remote_sink = NULL;
	GstRemoteSinkClass *klass = NULL;
	gboolean result = TRUE;
	
	remote_sink = GST_REMOTE_SINK (bsink);
	klass = GST_REMOTE_SINK_GET_CLASS (bsink);

	switch (GST_EVENT_TYPE (event)) {
    	case GST_EVENT_FLUSH_START:
			{
    		}
      		break;
    	case GST_EVENT_FLUSH_STOP:
			{
				if(klass->remote_flush)
					result = klass->remote_flush(remote_sink); // we should let remote device do flush operation
    		}
      		break;
    	case GST_EVENT_EOS:
			{
    		}
      		break;
    	case GST_EVENT_NEWSEGMENT:
			{
    		}
    		break;
    	default:
     		break;
  }
	
  return result;
}

static void
gst_remote_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRemoteSink *rsink;

  rsink = GST_REMOTE_SINK (object);

  switch (prop_id) {
    case PROP_REMOTE_IP:
	  {
	  	int len = strlen(g_value_get_string(value));
	  	memset(rsink->priv->remote_ip, 0x0, sizeof(rsink->priv->remote_ip));
	  	strncpy(rsink->priv->remote_ip, g_value_get_string(value), len);
		g_print("SET_PROPERTY:rsink->priv->remote_ip = %s\n", rsink->priv->remote_ip);
      }
      break;
    case PROP_REMOTE_PORT:
	  {
	  	rsink->priv->remote_port = g_value_get_int(value);
		g_print("SET_PROPERTY:rsink->priv->remote_port = %d\n", rsink->priv->remote_port);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_remote_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRemoteSink *rsink;

  rsink = GST_REMOTE_SINK (object);

  switch (prop_id) {
    case PROP_REMOTE_IP:
	  {
	  	g_print("GET_PROPERTY: rsink->priv->remote_ip = %s\n", rsink->priv->remote_ip);
	  	g_value_set_string(value, rsink->priv->remote_ip);
      }
      break;
    case PROP_REMOTE_PORT:
	  {
	  	g_print("GET_PROPERTY: rsink->priv->remote_port = %d\n", rsink->priv->remote_port);
	  	g_value_set_int(value, rsink->priv->remote_port);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Public methods */

GType
gst_remote_sink_get_type (void)
{
  static GType remotesink_type = 0;

  if (!remotesink_type) {
    static const GTypeInfo remotesink_info = {
      sizeof (GstRemoteSinkClass),
      gst_remote_sink_base_init,
      NULL,
      (GClassInitFunc) gst_remote_sink_class_init,
      NULL,
      NULL,
      sizeof (GstRemoteSink),
      0,
      (GInstanceInitFunc) gst_remote_sink_init,
    };

    remotesink_type = g_type_register_static (GST_TYPE_BASE_SINK,
        "GstRemoteSink", &remotesink_info, 0);
  }

  return remotesink_type;
}

gboolean gst_remote_sink_remote_push(GstRemoteSink *remote_sink, gpointer buffer, gint push_size, gint *real_size)
{
	g_return_val_if_fail((remote_sink != NULL), FALSE);

	return gst_remote_sink_remote_push_internal(remote_sink, buffer, push_size, real_size);
}

gboolean gst_remote_sink_remote_pull(GstRemoteSink *remote_sink, gpointer buffer, gint pull_size, gint *real_size)
{
	g_return_val_if_fail((remote_sink != NULL), FALSE);

	return gst_remote_sink_remote_pull_internal(remote_sink, buffer, pull_size, real_size);
}

