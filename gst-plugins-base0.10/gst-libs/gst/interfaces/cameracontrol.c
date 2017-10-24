/*
 * GStreamer Camera Control Interface
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd.
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

/*============================================================================================
EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

when            who                             what, where, why
---------       ------------------------        ----------------------------------------------
12/09/08        jm80.yang@samsung.com           Created

============================================================================================*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cameracontrol.h"
#include "interfaces-marshal.h"

/**
 * SECTION:gstcameracontrol
 * @short_description: Interface for camera control
 */

enum {
	CONTROL_VALUE_CHANGED,
	CONTROL_LAST_SIGNAL
};

static void gst_camera_control_class_init(GstCameraControlClass *klass);

static guint gst_camera_control_signals[CONTROL_LAST_SIGNAL] = { 0 };

GType gst_camera_control_get_type(void)
{
	static GType gst_camera_control_type = 0;

	if (!gst_camera_control_type) {
		static const GTypeInfo gst_camera_control_info =
		{
			sizeof(GstCameraControlClass),
			(GBaseInitFunc)gst_camera_control_class_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL,
		};

		gst_camera_control_type = g_type_register_static(G_TYPE_INTERFACE,
		                                                 "GstCameraControl",
		                                                 &gst_camera_control_info,
		                                                 0);
		g_type_interface_add_prerequisite(gst_camera_control_type, GST_TYPE_IMPLEMENTS_INTERFACE);
	}

	return gst_camera_control_type;
}

static void gst_camera_control_class_init(GstCameraControlClass *klass)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		gst_camera_control_signals[CONTROL_VALUE_CHANGED] =
			g_signal_new("control-value-changed",
			             GST_TYPE_CAMERA_CONTROL, G_SIGNAL_RUN_LAST,
			             G_STRUCT_OFFSET(GstCameraControlClass, value_changed),
			             NULL, NULL,
			             gst_interfaces_marshal_VOID__OBJECT_INT,
			             G_TYPE_NONE, 2, GST_TYPE_CAMERA_CONTROL_CHANNEL, G_TYPE_INT);

		initialized = TRUE;
	}

	// TODO :
	klass->camera_control_type = 0;

	/* defauld virtual functions */
	klass->list_channels = NULL;
	klass->set_exposure = NULL;
	klass->get_exposure = NULL;
	klass->set_capture_mode = NULL;
	klass->get_capture_mode = NULL;
	klass->set_strobe = NULL;
	klass->get_strobe = NULL;
	klass->set_detect = NULL;
	klass->get_detect = NULL;
	klass->set_value = NULL;
	klass->get_value = NULL;
	klass->set_zoom	 = NULL;
	klass->get_zoom	 = NULL;
	klass->set_focus = NULL;
	klass->get_focus = NULL;
	klass->start_auto_focus = NULL;
	klass->stop_auto_focus = NULL;
	klass->set_focus_level = NULL;
	klass->get_focus_level = NULL;
	klass->set_auto_focus_area = NULL;
	klass->get_auto_focus_area = NULL;
	klass->set_wdr = NULL;
	klass->get_wdr = NULL;
	klass->set_ahs = NULL;
	klass->get_ahs = NULL;
	klass->set_part_color = NULL;
	klass->get_part_color = NULL;
	klass->get_exif_info = NULL;
	klass->set_capture_command = NULL;
        klass->set_record_command = NULL;
	klass->start_face_zoom = NULL;
	klass->stop_face_zoom = NULL;
	klass->set_ae_lock = NULL;
	klass->get_ae_lock = NULL;
	klass->set_awb_lock = NULL;
	klass->get_awb_lock = NULL;
}

const GList* gst_camera_control_list_channels(GstCameraControl *control)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->list_channels) {
		return klass->list_channels(control);
	}

	return NULL;
}


gboolean gst_camera_control_set_value(GstCameraControl *control, GstCameraControlChannel *control_channel, gint value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_value) {
		return klass->set_value(control, control_channel, value);
	}

	return FALSE;
}

gboolean gst_camera_control_get_value(GstCameraControl *control, GstCameraControlChannel *control_channel, gint *value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_value) {
		return klass->get_value(control, control_channel, value);
	}

	return FALSE;
}



gboolean gst_camera_control_set_exposure(GstCameraControl *control, gint type, gint value1, gint value2)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_exposure) {
		return klass->set_exposure(control, type, value1, value2);
	}

	return FALSE;
}

gboolean gst_camera_control_get_exposure(GstCameraControl *control, gint type, gint *value1, gint *value2)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_exposure) {
		return klass->get_exposure(control, type, value1, value2);
	}

	return FALSE;
}

gboolean gst_camera_control_set_capture_mode(GstCameraControl *control, gint type, gint value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_capture_mode) {
		return klass->set_capture_mode(control, type, value);
	}

	return FALSE;
}

gboolean gst_camera_control_get_capture_mode(GstCameraControl *control, gint type, gint *value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_capture_mode) {
		return klass->get_capture_mode(control, type, value);
	}

	return FALSE;
}

gboolean gst_camera_control_set_strobe(GstCameraControl *control, gint type, gint value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_strobe) {
		return klass->set_strobe(control, type, value);
	}

	return FALSE;
}

gboolean gst_camera_control_get_strobe(GstCameraControl *control, gint type, gint *value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_strobe) {
		return klass->get_strobe(control, type, value);
	}

	return FALSE;
}

gboolean gst_camera_control_set_detect(GstCameraControl *control, gint type, gint value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_detect) {
		return klass->set_detect(control, type, value);
	}

	return FALSE;
}

gboolean gst_camera_control_get_detect(GstCameraControl *control, gint type, gint *value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_detect) {
		return klass->get_detect(control, type, value);
	}

	return FALSE;
}

gboolean gst_camera_control_set_zoom(GstCameraControl *control, gint type, gint value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_zoom) {
		return klass->set_zoom(control, type, value);
	}

	return FALSE;
}

gboolean gst_camera_control_get_zoom(GstCameraControl *control, gint type, gint *value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_zoom) {
		return klass->get_zoom(control, type, value);
	}

	return FALSE;
}

gboolean gst_camera_control_set_focus(GstCameraControl *control, gint mode, gint range)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_focus) {
		return klass->set_focus(control, mode, range);
	}

	return FALSE;
}

gboolean gst_camera_control_get_focus(GstCameraControl *control, gint *mode, gint *range)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_focus) {
		return klass->get_focus(control, mode, range);
	}

	return FALSE;
}

gboolean gst_camera_control_start_auto_focus(GstCameraControl *control)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->start_auto_focus) {
		return klass->start_auto_focus(control);
	}

	return FALSE;
}

gboolean gst_camera_control_stop_auto_focus(GstCameraControl *control)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->stop_auto_focus) {
		return klass->stop_auto_focus(control);
	}

	return FALSE;
}

gboolean gst_camera_control_set_focus_level(GstCameraControl *control, gint manual_level)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_focus_level) {
		return klass->set_focus_level(control, manual_level);
	}

	return FALSE;
}

gboolean gst_camera_control_get_focus_level(GstCameraControl *control, gint *manual_level)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_focus_level) {
		return klass->get_focus_level(control, manual_level);
	}

	return FALSE;
}

gboolean gst_camera_control_set_auto_focus_area(GstCameraControl *control, GstCameraControlRectType rect)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_auto_focus_area) {
		return klass->set_auto_focus_area(control, rect);
	}

	return FALSE;
}

gboolean gst_camera_control_get_auto_focus_area(GstCameraControl *control, GstCameraControlRectType *rect)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_auto_focus_area) {
		return klass->get_auto_focus_area(control, rect);
	}

	return FALSE;
}

gboolean gst_camera_control_set_wdr(GstCameraControl *control, gint value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_wdr) {
		return klass->set_wdr(control, value);
	}

	return FALSE;
}

gboolean gst_camera_control_get_wdr(GstCameraControl *control, gint *value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_wdr) {
		return klass->get_wdr(control, value);
	}

	return FALSE;
}

gboolean gst_camera_control_set_ahs(GstCameraControl *control, gint value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_ahs) {
		return klass->set_ahs(control, value);
	}

	return FALSE;
}

gboolean gst_camera_control_get_ahs(GstCameraControl *control, gint *value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_ahs) {
		return klass->get_ahs(control, value);
	}

	return FALSE;
}

gboolean gst_camera_control_set_part_color(GstCameraControl *control, gint type, gint value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_part_color) {
		return klass->set_part_color(control, type, value);
	}

	return FALSE;
}

gboolean gst_camera_control_get_part_color(GstCameraControl *control, gint type, gint *value)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_part_color) {
		return klass->get_part_color(control, type, value);
	}

	return FALSE;
}

gboolean
gst_camera_control_get_exif_info(GstCameraControl *control, GstCameraControlExifInfo *info)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_exif_info) {
		return klass->get_exif_info(control, info);
	}

	return FALSE;
}


gboolean gst_camera_control_get_basic_dev_info(GstCameraControl *control, gint dev_id, GstCameraControlCapsInfoType *info)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_basic_dev_info) {
		return klass->get_basic_dev_info(control, dev_id, info);
	}

	return FALSE;
}

gboolean gst_camera_control_get_misc_dev_info(GstCameraControl *control, gint dev_id, GstCameraControlCtrlListInfoType *info)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS( control );

	if( klass->get_misc_dev_info )
	{
		return klass->get_misc_dev_info( control, dev_id, info );
	}

	return FALSE;
}

gboolean gst_camera_control_get_extra_dev_info(GstCameraControl *control, gint dev_id, GstCameraControlExtraInfoType *info)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_extra_dev_info) {
		return klass->get_extra_dev_info(control, dev_id, info);
	}

	return FALSE;
}

void gst_camera_control_set_capture_command(GstCameraControl *control, GstCameraControlCaptureCommand cmd)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_capture_command) {
		klass->set_capture_command(control, cmd);
	}

	return;
}
void gst_camera_control_set_record_command(GstCameraControl *control, GstCameraControlRecordCommand cmd)
{
        GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

        if (klass->set_record_command) {
                klass->set_record_command(control, cmd);
        }

        return;
}

gboolean gst_camera_control_start_face_zoom(GstCameraControl *control, gint x, gint y, gint zoom_level)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->start_face_zoom) {
		return klass->start_face_zoom(control, x, y, zoom_level);
	}

	return FALSE;
}

gboolean gst_camera_control_stop_face_zoom(GstCameraControl *control)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->stop_face_zoom) {
		return klass->stop_face_zoom(control);
	}

	return FALSE;
}

gboolean gst_camera_control_set_ae_lock(GstCameraControl *control, gboolean lock)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_ae_lock) {
		return klass->set_ae_lock(control, lock);
	}

	return FALSE;
}

gboolean gst_camera_control_get_ae_lock(GstCameraControl *control, gboolean *lock)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_ae_lock) {
		return klass->get_ae_lock(control, lock);
	}

	return FALSE;
}

gboolean gst_camera_control_set_awb_lock(GstCameraControl *control, gboolean lock)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->set_awb_lock) {
		return klass->set_awb_lock(control, lock);
	}

	return FALSE;
}

gboolean gst_camera_control_get_awb_lock(GstCameraControl *control, gboolean *lock)
{
	GstCameraControlClass *klass = GST_CAMERA_CONTROL_GET_CLASS(control);

	if (klass->get_awb_lock) {
		return klass->get_awb_lock(control, lock);
	}

	return FALSE;
}

void gst_camera_control_value_changed(GstCameraControl *control, GstCameraControlChannel *control_channel, gint value)
{
	g_signal_emit(G_OBJECT(control), gst_camera_control_signals[CONTROL_VALUE_CHANGED], 0, control_channel, value);
	g_signal_emit_by_name(G_OBJECT(control_channel), "control-value-changed", value);
}
