/* GStreamer Camera Control Channel Interface
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd.
 *
 * Contact: Jeongmo Yang <jm80.yang@samsung.com>
 *
 * cameracontrolchannel.c: cameracontrol channel object design
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

#include "cameracontrolchannel.h"

enum {
	/* FILL ME */
	SIGNAL_VALUE_CHANGED,
	LAST_SIGNAL
};

static void gst_camera_control_channel_class_init(GstCameraControlChannelClass* klass);
static void gst_camera_control_channel_init(GstCameraControlChannel* control_channel);
static void gst_camera_control_channel_dispose(GObject* object);

static GObjectClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

GType gst_camera_control_channel_get_type(void)
{
	static GType gst_camera_control_channel_type = 0;

	if (!gst_camera_control_channel_type) {
		static const GTypeInfo camera_control_channel_info = {
			sizeof (GstCameraControlChannelClass),
			NULL,
			NULL,
			(GClassInitFunc) gst_camera_control_channel_class_init,
			NULL,
			NULL,
			sizeof (GstCameraControlChannel),
			0,
			(GInstanceInitFunc) gst_camera_control_channel_init,
			NULL
		};

		gst_camera_control_channel_type = \
			g_type_register_static(G_TYPE_OBJECT,
			                       "GstCameraControlChannel",
			                       &camera_control_channel_info,
			                       0);
	}

	return gst_camera_control_channel_type;
}

static void gst_camera_control_channel_class_init(GstCameraControlChannelClass* klass)
{
	GObjectClass *object_klass = (GObjectClass*) klass;

	parent_class = g_type_class_peek_parent (klass);

	signals[SIGNAL_VALUE_CHANGED] = \
		g_signal_new("control-value-changed",
		             G_TYPE_FROM_CLASS (klass),
		             G_SIGNAL_RUN_LAST,
		             G_STRUCT_OFFSET (GstCameraControlChannelClass, value_changed),
		             NULL,
		             NULL,
		             g_cclosure_marshal_VOID__INT,
		             G_TYPE_NONE,
		             1,
		             G_TYPE_INT);

	object_klass->dispose = gst_camera_control_channel_dispose;
}

static void gst_camera_control_channel_init(GstCameraControlChannel* control_channel)
{
	control_channel->label = NULL;
	control_channel->min_value = control_channel->max_value = 0;
}

static void gst_camera_control_channel_dispose(GObject* object)
{
	GstCameraControlChannel *control_channel = GST_CAMERA_CONTROL_CHANNEL(object);

	if (control_channel->label) {
		g_free (control_channel->label);
	}

	control_channel->label = NULL;

	if (parent_class->dispose) {
		parent_class->dispose (object);
	}
}
