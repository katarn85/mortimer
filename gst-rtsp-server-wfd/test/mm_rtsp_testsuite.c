/* GStreamer
 * Copyright (C) 2008 Wim Taymans <wim.taymans at gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gst/gst.h>

#include <rtsp-server.h>
#include <rtsp-auth.h>


#define SERVER_PORT "2022"
#define SERVER_IP "127.0.0.1"
#define WITH_AUTH

GMainLoop *loop;
void *server;
void *client;
void *mapping;
void *factory;

void __streaming_server_msg_cb(gboolean is_error, void *userdata)
{

	if(is_error) {
		g_print("Error occured\n");
	}
}

static gboolean __streaming_server_start(void *data)
{
	int ret = 0;
	GstRTSPAuth *auth;
	gchar *basic;

	server = (void*)gst_rtsp_server_new(__streaming_server_msg_cb, (void*)server);

	if (NULL == server) {
		g_print("Failed to create server...\n");
		return FALSE;
	}

	/* Gets the media mapping object for the server */
	mapping = (void*)gst_rtsp_server_get_media_mapping (server);
	if (mapping == NULL) {
		g_print("No media mapping...\n");
		return FALSE;
	}

	factory = (void*)gst_rtsp_media_factory_new();
	if (factory == NULL) {
		g_print("Failed to create factory...\n");
		return FALSE;
	}


	gst_rtsp_media_factory_set_launch (factory, "( audiotestsrc ! audioconvert !rtpL16pay name=pay0 pt=96 )");
//	gst_rtsp_media_factory_set_launch (factory, "( v4l2src ! ffmpegcolorspace ! omx_h264enc ! rtph264pay name=pay0 pt=96 )");


	/* attach the test factory to the /test url */
	gst_rtsp_media_mapping_add_factory (mapping, "/abc", factory);


	/* set service address on rtsp-server */
	gst_rtsp_server_set_address(server, SERVER_IP);
	gst_rtsp_server_set_service (server, SERVER_PORT);


#ifdef WITH_AUTH  //gst-launch rtspsrc location=rtsp://yoon:merong@127.0.0.1:2022/rtspserver1.0 debug=TRUE latency=0 ! rtpL16depay ! audioconvert ! alsasink
	auth = gst_rtsp_auth_new ();
	basic = gst_rtsp_auth_make_basic ("yoon", "merong");
	gst_rtsp_auth_set_basic (auth, basic);
	g_free (basic);
	gst_rtsp_media_factory_set_auth (factory, auth);
#endif

	/* attach the test factory to the /test url */
	gst_rtsp_media_mapping_add_factory (mapping, "/rtspserver1.0", factory);


	/* set max clients allowed on rtsp-server */
	gst_rtsp_server_set_backlog (server, 5);

	/* Set connection mode */
	gst_rtsp_server_set_connection_mode(server, RTSP_NORMAL);

	/* attach the server to the default maincontext */
	if (gst_rtsp_server_attach (server, NULL) == 0) {
		g_print ("Failed to attach server to context\n");
		return FALSE;
	}

	/* start accepting the client.. this call is a blocking call & keep on waiting on accept () sys API */
	client = (void*)gst_rtsp_server_accept_client (server);
	if (client == NULL) {
		g_print ("Error in client accept\n");
		return FALSE;
	}

	return FALSE;
}

int main (int argc, char *argv[])
{
  guint id;

  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);
  g_idle_add(__streaming_server_start, NULL);

  /* start serving */
  g_main_loop_run (loop);

  /* cleanup */
  g_source_remove (id);
  g_object_unref (server);
  g_main_loop_unref (loop);

  return 0;

  /* ERRORS */
failed:
  {
    g_print ("failed to attach the server\n");
    return FALSE;
  }
}
