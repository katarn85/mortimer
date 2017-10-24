/* GStreamer Camera Control Channel Interface
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd.
 *
 * Contact: Jeongmo Yang <jm80.yang@samsung.com>
 *
 * cameracontrolchannel.h: individual channel object
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

#ifndef __GST_CAMERA_CONTROL_CHANNEL_H__
#define __GST_CAMERA_CONTROL_CHANNEL_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_CAMERA_CONTROL_CHANNEL \
  (gst_camera_control_channel_get_type ())
#define GST_CAMERA_CONTROL_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CAMERA_CONTROL_CHANNEL, \
                               GstCameraControlChannel))
#define GST_CAMERA_CONTROL_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CAMERA_CONTROL_CHANNEL, \
                            GstCameraControlChannelClass))
#define GST_IS_CAMERA_CONTROL_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CAMERA_CONTROL_CHANNEL))
#define GST_IS_CAMERA_CONTROL_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CAMERA_CONTROL_CHANNEL))

typedef struct _GstCameraControlChannel {
	GObject parent;
	gchar *label;
	gint min_value;
	gint max_value;
} GstCameraControlChannel;

typedef struct _GstCameraControlChannelClass {
	GObjectClass parent;

	/* signals */
	void (*value_changed)(GstCameraControlChannel *control_channel, gint value);

	gpointer _gst_reserved[GST_PADDING];
} GstCameraControlChannelClass;

GType gst_camera_control_channel_get_type(void);

G_END_DECLS

#endif /* __GST_CAMERA_CONTROL_CHANNEL_H__ */
