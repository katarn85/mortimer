/*
 * camerasrc
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Jeongmo Yang <jm80.yang@samsung.com>
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

#ifndef __GST_CAMERASRC_CONTROL_H__
#define __GST_CAMERASRC_CONTROL_H__

#include <gst/gst.h>
#include <gst/interfaces/cameracontrol.h>
#include "gstcamerasrc.h"

G_BEGIN_DECLS

#define GST_TYPE_CAMERASRC_CONTROL_CHANNEL (gst_camerasrc_control_channel_get_type ())
#define GST_CAMERASRC_CONTROL_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CAMERASRC_CONTROL_CHANNEL, GstCamerasrcControlChannel))
#define GST_CAMERASRC_CONTROL_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CAMERASRC_CONTROL_CHANNEL, GstCamerasrcControlChannelClass))
#define GST_IS_CAMERASRC_CONTROL_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CAMERASRC_CONTROL_CHANNEL))
#define GST_IS_CAMERASRC_CONTROL_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CAMERASRC_CONTROL_CHANNEL))

typedef struct _GstCamerasrcControlChannel {
GstCameraControlChannel parent;
guint32 id;
} GstCamerasrcControlChannel;

typedef struct _GstCamerasrcControlChannelClass {
GstCameraControlChannelClass parent;
} GstCamerasrcControlChannelClass;

GType gst_camerasrc_control_channel_get_type( void );

const GList*gst_camerasrc_control_list_channels( GstCameraSrc* camera_src );

gboolean    gst_camerasrc_control_set_value          ( GstCameraSrc* camera_src, GstCameraControlChannel* control_channel, gint value );
gboolean    gst_camerasrc_control_get_value          ( GstCameraSrc* camera_src, GstCameraControlChannel* control_channel, gint* value );
gboolean    gst_camerasrc_control_set_capture_mode   ( GstCameraSrc* camera_src, gint type, gint value );
gboolean    gst_camerasrc_control_get_capture_mode   ( GstCameraSrc* camera_src, gint type, gint* value );
gboolean    gst_camerasrc_control_get_basic_dev_info ( GstCameraSrc* camera_src, gint dev_id, GstCameraControlCapsInfoType* info );
gboolean    gst_camerasrc_control_get_misc_dev_info  ( GstCameraSrc* camera_src, gint dev_id, GstCameraControlCtrlListInfoType* info );
void        gst_camerasrc_control_set_capture_command( GstCameraSrc* camera_src, GstCameraControlCaptureCommand cmd );

#define GST_IMPLEMENT_CAMERASRC_CONTROL_METHODS(Type, interface_as_function) \
 \
static const GList* \
interface_as_function ## _control_list_channels( GstCameraControl* control ) \
{ \
	Type* this = (Type*) control; \
	return gst_camerasrc_control_list_channels( this ); \
} \
 \
static gboolean \
interface_as_function ## _control_set_value( GstCameraControl* control, \
                                             GstCameraControlChannel* control_channel, gint value ) \
{ \
	Type* this = (Type*)control; \
	return gst_camerasrc_control_set_value( this, control_channel, value ); \
} \
 \
static gboolean \
interface_as_function ## _control_get_value( GstCameraControl* control, \
                                             GstCameraControlChannel* control_channel, gint* value ) \
{ \
	Type* this = (Type*)control; \
	return gst_camerasrc_control_get_value( this, control_channel, value ); \
} \
 \
static gboolean \
interface_as_function ## _control_set_capture_mode( GstCameraControl* control, \
                                                    gint type, gint value ) \
{ \
	Type* this = (Type*)control; \
	return gst_camerasrc_control_set_capture_mode( this, type, value ); \
} \
 \
static gboolean \
interface_as_function ## _control_get_capture_mode( GstCameraControl* control, \
                                                    gint type, gint* value ) \
{ \
	Type* this = (Type*)control; \
	return gst_camerasrc_control_get_capture_mode( this, type, value ); \
} \
 \
static gboolean \
interface_as_function ## _control_get_basic_dev_info( GstCameraControl* control, \
                                                      gint dev_id, \
                                                      GstCameraControlCapsInfoType* info ) \
{ \
	Type* this = (Type*)control; \
	return gst_camerasrc_control_get_basic_dev_info( this, dev_id, info); \
} \
 \
static gboolean \
interface_as_function ## _control_get_misc_dev_info( GstCameraControl* control, \
                                                     gint dev_id, \
                                                     GstCameraControlCtrlListInfoType* info ) \
{ \
	Type* this = (Type*)control; \
	return gst_camerasrc_control_get_misc_dev_info( this, dev_id, info); \
} \
 \
static void \
interface_as_function ## _control_set_capture_command( GstCameraControl* control, \
                                                       GstCameraControlCaptureCommand cmd ) \
{ \
	Type* this = (Type*)control; \
	gst_camerasrc_control_set_capture_command( this, cmd ); \
	return; \
} \
 \
void \
interface_as_function ## _control_interface_init( GstCameraControlClass *klass ) \
{ \
	GST_CAMERA_CONTROL_TYPE( klass ) = GST_CAMERA_CONTROL_HARDWARE; \
 \
	/* default virtual functions */ \
	klass->list_channels = interface_as_function ## _control_list_channels; \
	klass->set_value = interface_as_function ## _control_set_value; \
	klass->get_value = interface_as_function ## _control_get_value; \
	klass->set_capture_mode = interface_as_function ## _control_set_capture_mode; \
	klass->get_capture_mode = interface_as_function ## _control_get_capture_mode; \
	klass->get_basic_dev_info = interface_as_function ## _control_get_basic_dev_info; \
	klass->get_misc_dev_info = interface_as_function ## _control_get_misc_dev_info; \
	klass->set_capture_command = interface_as_function ## _control_set_capture_command; \
 \
}

#endif /* __GST_CAMERASRC_CONTROL_H__ */
