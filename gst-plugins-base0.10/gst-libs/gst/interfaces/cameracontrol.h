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

/* ===========================================================================================
EDIT HISTORY FOR MODULE

	This section contains comments describing changes made to the module.
	Notice that changes are listed in reverse chronological order.

when		who							what, where, why
---------	------------------------	------------------------------------------------------
12/09/08	jm80.yang@samsung.com		Created

=========================================================================================== */

#ifndef __GST_CAMERA_CONTROL_H__
#define __GST_CAMERA_CONTROL_H__

#include <gst/gst.h>
#include <gst/interfaces/cameracontrolchannel.h>
#include <gst/interfaces/interfaces-enumtypes.h>

G_BEGIN_DECLS

#define GST_TYPE_CAMERA_CONTROL \
	(gst_camera_control_get_type())
#define GST_CAMERA_CONTROL(obj) \
	(GST_IMPLEMENTS_INTERFACE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_CAMERA_CONTROL, GstCameraControl))
#define GST_CAMERA_CONTROL_GET_CLASS(inst) \
	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), GST_TYPE_CAMERA_CONTROL, GstCameraControlClass))
#define GST_CAMERA_CONTROL_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_CAMERA_CONTROL, GstCameraControlClass))
#define GST_IS_CAMERA_CONTROL(obj) \
	(GST_IMPLEMENTS_INTERFACE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_CAMERA_CONTROL))
#define GST_IS_CAMERA_CONTROL_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_CAMERA_CONTROL))
#define GST_CAMERA_CONTROL_TYPE(klass) (klass->camera_control_type)


typedef struct _GstCameraControl GstCameraControl;

typedef enum
{
	// TODO : V4L2 Extend
	GST_CAMERA_CONTROL_HARDWARE,
	GST_CAMERA_CONTROL_SOFTWARE,
} GstCameraControlType;

/**
 * Enumerations for Camera control Exposure types.
 */
typedef enum
{
	GST_CAMERA_CONTROL_F_NUMBER,
	GST_CAMERA_CONTROL_SHUTTER_SPEED,
	GST_CAMERA_CONTROL_ISO,
	GST_CAMERA_CONTROL_PROGRAM_MODE,
	GST_CAMERA_CONTROL_EXPOSURE_MODE,
	GST_CAMERA_CONTROL_EXPOSURE_VALUE,
} GstCameraControlExposureType;

/**
 * Enumerations for Camera control Capture mode types.
 */
typedef enum
{
	GST_CAMERA_CONTROL_CAPTURE_MODE,
	GST_CAMERA_CONTROL_OUTPUT_MODE,
	GST_CAMERA_CONTROL_FRAME_COUNT,
	GST_CAMERA_CONTROL_JPEG_QUALITY,
} GstCameraControlCaptureModeType;

/**
 * Enumerations for Capture mode types.
 */
typedef enum
{
	CAPTURE_MODE_SINGLE_SHOT,
	CAPTURE_MODE_MULTI_SHOT,
	CAPTURE_MODE_ANTI_HANDSHAKE,
} CaptureModeType;

/**
 * Enumerations for Camera control Strobe types.
 */
typedef enum
{
	GST_CAMERA_CONTROL_STROBE_CONTROL,
	GST_CAMERA_CONTROL_STROBE_CAPABILITIES,
	GST_CAMERA_CONTROL_STROBE_MODE,
	GST_CAMERA_CONTROL_STROBE_STATUS,
	GST_CAMERA_CONTROL_STROBE_EV,
} GstCameraControlStrobeType;

/**
 * Enumerations for Camera control Face detection types.
 */
typedef enum
{
	GST_CAMERA_CONTROL_FACE_DETECT_MODE,
	GST_CAMERA_CONTROL_FACE_DETECT_NUMBER,
	GST_CAMERA_CONTROL_FACE_FOCUS_SELECT,
	GST_CAMERA_CONTROL_FACE_SELECT_NUMBER,
	GST_CAMERA_CONTROL_FACE_DETECT_STATUS,
} GstCameraControlFaceDetectType;

/**
 * Enumerations for Camera control Zoom types.
 */
typedef enum
{
	GST_CAMERA_CONTROL_DIGITAL_ZOOM,
	GST_CAMERA_CONTROL_OPTICAL_ZOOM,
} GstCameraControlZoomType;

/**
 * Enumerations for Camera control Part color.
 */
typedef enum
{
	GST_CAMERA_CONTROL_PART_COLOR_SRC,
	GST_CAMERA_CONTROL_PART_COLOR_DST,
	GST_CAMERA_CONTROL_PART_COLOR_MODE,
} GstCameraControlPartColorType;

/**
 * Enumerations for Camera capture command.
 */
typedef enum
{
	GST_CAMERA_CONTROL_CAPTURE_COMMAND_NONE,
	GST_CAMERA_CONTROL_CAPTURE_COMMAND_START,
	GST_CAMERA_CONTROL_CAPTURE_COMMAND_STOP,
	GST_CAMERA_CONTROL_CAPTURE_COMMAND_STOP_MULTISHOT,
} GstCameraControlCaptureCommand;

/**
 * Enumerations for Camera record command.
 */
typedef enum
{
        GST_CAMERA_CONTROL_RECORD_COMMAND_NONE,
        GST_CAMERA_CONTROL_RECORD_COMMAND_START,
        GST_CAMERA_CONTROL_RECORD_COMMAND_STOP,
} GstCameraControlRecordCommand;

/////////////////////////////////
//  For Query functionalities  //
//  For Querying capabilities  //
/////////////////////////////////
/*! Use static size of structures for querying because of performance
 */
/**
 * Don't modify this sizes
 * If want to modify this size, consider also under layer structure size
 */
#define MAX_NUM_FMT_DESC        32
#define MAX_NUM_RESOLUTION      32
#define MAX_NUM_AVAILABLE_TPF   16
#define MAX_NUM_AVAILABLE_FPS   16
#define MAX_NUM_CTRL_LIST_INFO  64
#define MAX_NUM_CTRL_MENU       64
#define MAX_NUM_DETECTED_FACES  16
#define MAX_SZ_CTRL_NAME_STRING 32
#define MAX_SZ_DEV_NAME_STRING  32

/*! @struct GstCameraControlFracType
 *  @brief For timeperframe as fraction type
 *  Elapse time consumed by one frame, reverse of FPS
 */
typedef struct _GstCameraControlFracType {
	gint num;
	gint den;
} GstCameraControlFracType;

/*! @struct GstCameraControlRectType
 *  @brief For touch auto focusing area and face detection area
 */
typedef struct _GstCameraControlRectType {
	gint x;
	gint y;
	gint width;
	gint height;
} GstCameraControlRectType;

/*! @struct GstCameraControlResolutionType
 *  @brief For querying supported resolutions
 */
typedef struct _GstCameraControlResolutionType {
    gint w;
    gint h;

    /* Available time per frame(tpf) as each pixelformat */
    gint num_avail_tpf;
    GstCameraControlFracType tpf[MAX_NUM_AVAILABLE_TPF];
} GstCameraControlResolutionType;

/*! @struct GstCameraControlFraction
 *  @brief Time per frame or frame per second will be expressed by this structure
 *  Time per frame or frame per second will be expressed by this structure
 */
typedef struct _GstCameraControlFraction {
    int numerator;             /**< Upper number of fraction*/
    int denominator;           /**< Lower number of fraction*/
} GstCameraControlFraction;

/*! @struct GstCameraControlFmtDescType
 *  @brief For querying supported format type
 */
typedef struct _GstCameraControlFmtDescType {
    /* fourcc name of each pixelformat */
    guint fcc;
    gint fcc_use;

    /* Available resolutions as each pixelformat */
    gint num_resolution;
    GstCameraControlResolutionType resolutions[MAX_NUM_RESOLUTION];
} GstCameraControlFmtDescType;

/*! @struct GstCameraControlCapsInfoType
 *  @brief For querying image input capabilities
 */
typedef struct _GstCameraControlCapsInfoType {
    char dev_name[MAX_SZ_DEV_NAME_STRING];
    int input_idx;
    gint num_fmt_desc;
    GstCameraControlFmtDescType fmt_desc[MAX_NUM_FMT_DESC];

    int num_preview_resolution;
    int preview_resolution_width[MAX_NUM_RESOLUTION];
    int preview_resolution_height[MAX_NUM_RESOLUTION];

    int num_capture_resolution;
    int capture_resolution_width[MAX_NUM_RESOLUTION];
    int capture_resolution_height[MAX_NUM_RESOLUTION];

    int num_preview_fmt;
    unsigned int preview_fmt[MAX_NUM_FMT_DESC];

    int num_capture_fmt;
    unsigned int capture_fmt[MAX_NUM_FMT_DESC];

    int num_fps;
    GstCameraControlFraction fps[MAX_NUM_AVAILABLE_FPS];
} GstCameraControlCapsInfoType;

/*! @struct GstCameraControlFaceInfo
 *  @brief For face information
 */
typedef struct _GstCameraControlFaceInfo {
	int id;
	int score;
	GstCameraControlRectType rect;
} GstCameraControlFaceInfo;

/*! @struct GstCameraControlFaceDetectInfo
 *  @brief For face detect information
 */
typedef struct _GstCameraControlFaceDetectInfo {
	int num_of_faces;
	GstCameraControlFaceInfo face_info[MAX_NUM_DETECTED_FACES];
} GstCameraControlFaceDetectInfo;

/////////////////////////////
//  For Querying controls  //
/////////////////////////////
enum {
    GST_CAMERA_CTRL_TYPE_RANGE = 0,
    GST_CAMERA_CTRL_TYPE_BOOL,
    GST_CAMERA_CTRL_TYPE_ARRAY,
    GST_CAMERA_CTRL_TYPE_UNKNOWN,
    GST_CAMERA_CTRL_TYPE_NUM,
};

/*! @struct GstCameraControlCtrlMenuType
 *  @brief For querying menu of specified controls
 */
typedef struct _GstCameraControlCtrlMenuType {
    gint menu_index;
    gchar menu_name[MAX_SZ_CTRL_NAME_STRING];
} GstCameraControlCtrlMenuType;

/*! @struct GstCameraControlCtrlInfoType
 *  @brief For querying controls detail
 */
typedef struct _GstCameraControlCtrlInfoType {
    gint avsys_ctrl_id;
    gint v4l2_ctrl_id;
    gint ctrl_type;
    gchar ctrl_name[MAX_SZ_CTRL_NAME_STRING];
    gint min;
    gint max;
    gint step;
    gint default_val;
    gint num_ctrl_menu;
    GstCameraControlCtrlMenuType ctrl_menu[MAX_NUM_CTRL_MENU];
} GstCameraControlCtrlInfoType;

/*! @struct GstCameraControlCtrlListInfoType
 *  @brief For querying controls
 */
typedef struct _GstCameraControlCtrlListInfoType {
    gint num_ctrl_list_info;
    GstCameraControlCtrlInfoType ctrl_info[MAX_NUM_CTRL_LIST_INFO];
} GstCameraControlCtrlListInfoType;

/* capabilities field */
#define GST_CAMERA_STROBE_CAP_NONE              0x0000	/* No strobe supported */
#define GST_CAMERA_STROBE_CAP_OFF               0x0001	/* Always flash off mode */
#define GST_CAMERA_STROBE_CAP_ON                0x0002	/* Always use flash light mode */
#define GST_CAMERA_STROBE_CAP_AUTO              0x0004	/* Flashlight works automatic */
#define GST_CAMERA_STROBE_CAP_REDEYE            0x0008	/* Red-eye reduction */
#define GST_CAMERA_STROBE_CAP_SLOWSYNC          0x0010	/* Slow sync */
#define GST_CAMERA_STROBE_CAP_FRONT_CURTAIN     0x0020	/* Front curtain */
#define GST_CAMERA_STROBE_CAP_REAR_CURTAIN      0x0040	/* Rear curtain */
#define GST_CAMERA_STROBE_CAP_PERMANENT         0x0080	/* keep turned on until turning off */
#define GST_CAMERA_STROBE_CAP_EXTERNAL          0x0100	/* use external strobe */

typedef struct _GstCameraControlExtraInfoType {
    guint strobe_caps;                                   /**< Use above caps field */
    guint detection_caps;                                /**< Just boolean */
    guint reserved[4];
} GstCameraControlExtraInfoType;
/////////////////////////////////////
//  END For Query functionalities  //
/////////////////////////////////////


/**
 * Enumerations for Camera control Part color.
 */
typedef struct _GstCameraControlExifInfo {
	/* Dynamic value */
	guint32 exposure_time_numerator;    /* Exposure time, given in seconds */
	guint32 exposure_time_denominator;
	gint shutter_speed_numerator;       /* Shutter speed, given in APEX(Additive System Photographic Exposure) */
	gint shutter_speed_denominator;
	gint brigtness_numerator;           /* Value of brightness, before firing flash, given in APEX value */
	gint brightness_denominator;
	guint16 iso;                        /* Sensitivity value of sensor */
	guint16 flash;                      /* Whether flash is fired(1) or not(0) */
	gint metering_mode;                 /* metering mode in EXIF 2.2 */
	gint exif_image_width;              /* Size of image */
	gint exif_image_height;
	gint exposure_bias_in_APEX;         /* Exposure bias in APEX standard */
	gint software_used;                 /* Firmware S/W version */
	unsigned char *p_embedded_data;     /* Pointer for embedded data */

	/* Fixed value */
	gint component_configuration;       /* color components arrangement */
	gint colorspace;                    /* colorspace information */
	gint focal_len_numerator;           /* Lens focal length */
	gint focal_len_denominator;
	gint aperture_f_num_numerator;      /* Aperture value */
	gint aperture_f_num_denominator;
	gint aperture_in_APEX;              /* Aperture value in APEX standard */
	gint max_lens_aperture_in_APEX;     /* Max aperture value in APEX standard */
} GstCameraControlExifInfo;


typedef struct _GstCameraControlClass {
	GTypeInterface klass;
	GstCameraControlType camera_control_type;

	/* virtual functions */
	const GList*(*list_channels)                   (GstCameraControl *control);

	gboolean	(*set_value)                   (GstCameraControl *control, GstCameraControlChannel *control_channel, gint value);
	gboolean	(*get_value)                   (GstCameraControl *control, GstCameraControlChannel *control_channel, gint *value);
	gboolean	(*set_exposure)                (GstCameraControl *control, gint type, gint value1, gint value2);
	gboolean	(*get_exposure)                (GstCameraControl *control, gint type, gint *value1, gint *value2);
	gboolean	(*set_capture_mode)            (GstCameraControl *control, gint type, gint value);
	gboolean	(*get_capture_mode)            (GstCameraControl *control, gint type, gint *value);
	gboolean	(*set_strobe)                  (GstCameraControl *control, gint type, gint value);
	gboolean	(*get_strobe)                  (GstCameraControl *control, gint type, gint *value);
	gboolean	(*set_detect)                  (GstCameraControl *control, gint type, gint value);
	gboolean	(*get_detect)                  (GstCameraControl *control, gint type, gint *value);
	gboolean	(*set_zoom)                    (GstCameraControl *control, gint type, gint value);
	gboolean	(*get_zoom)                    (GstCameraControl *control, gint type, gint *value);
	gboolean	(*set_focus)                   (GstCameraControl *control, gint mode, gint range);
	gboolean	(*get_focus)                   (GstCameraControl *control, gint *mode, gint *range);
	gboolean	(*start_auto_focus)            (GstCameraControl *control);
	gboolean	(*stop_auto_focus)             (GstCameraControl *control);
	gboolean	(*set_focus_level)             (GstCameraControl *control, gint manual_level);
	gboolean	(*get_focus_level)             (GstCameraControl *control, gint *manual_level);
	gboolean	(*set_auto_focus_area)         (GstCameraControl *control, GstCameraControlRectType rect);
	gboolean	(*get_auto_focus_area)         (GstCameraControl *control, GstCameraControlRectType *rect);
	gboolean	(*set_wdr)                     (GstCameraControl *control, gint value);
	gboolean	(*get_wdr)                     (GstCameraControl *control, gint *value);
	gboolean	(*set_ahs)                     (GstCameraControl *control, gint value);
	gboolean	(*get_ahs)                     (GstCameraControl *control, gint *value);
	gboolean	(*set_part_color)              (GstCameraControl *control, gint type, gint value);
	gboolean	(*get_part_color)              (GstCameraControl *control, gint type, gint *value);
	gboolean	(*get_exif_info)               (GstCameraControl *control, GstCameraControlExifInfo *info);
	gboolean	(*get_basic_dev_info)          (GstCameraControl *control, gint dev_id, GstCameraControlCapsInfoType *info);
	gboolean	(*get_misc_dev_info)           (GstCameraControl *control, gint dev_id, GstCameraControlCtrlListInfoType *info);
	gboolean	(*get_extra_dev_info)          (GstCameraControl *control, gint dev_id, GstCameraControlExtraInfoType *info);
	void		(*set_capture_command)         (GstCameraControl *control, GstCameraControlCaptureCommand cmd);
	void		(*set_record_command)          (GstCameraControl *control, GstCameraControlRecordCommand cmd);
	gboolean	(*start_face_zoom)             (GstCameraControl *control, gint x, gint y, gint zoom_level);
	gboolean	(*stop_face_zoom)              (GstCameraControl *control);
	gboolean	(*set_ae_lock)                 (GstCameraControl *control, gboolean lock);
	gboolean	(*get_ae_lock)                 (GstCameraControl *control, gboolean *lock);
	gboolean	(*set_awb_lock)                (GstCameraControl *control, gboolean lock);
	gboolean	(*get_awb_lock)                (GstCameraControl *control, gboolean *lock);

	/* signals */
	void (* value_changed)                          (GstCameraControl *control, GstCameraControlChannel *channel, gint value);
} GstCameraControlClass;

GType gst_camera_control_get_type(void);

/* virtual class function wrappers */
const GList*	gst_camera_control_list_channels        (GstCameraControl *control);

gboolean	gst_camera_control_set_value            (GstCameraControl *control, GstCameraControlChannel *control_channel, gint value);
gboolean	gst_camera_control_get_value            (GstCameraControl *control, GstCameraControlChannel *control_channel, gint *value);
gboolean	gst_camera_control_set_exposure         (GstCameraControl *control, gint type, gint value1, gint value2);
gboolean	gst_camera_control_get_exposure         (GstCameraControl *control, gint type, gint *value1, gint *value2);
gboolean	gst_camera_control_set_capture_mode     (GstCameraControl *control, gint type, gint value);
gboolean	gst_camera_control_get_capture_mode     (GstCameraControl *control, gint type, gint *value);
gboolean	gst_camera_control_set_strobe           (GstCameraControl *control, gint type, gint value);
gboolean	gst_camera_control_get_strobe           (GstCameraControl *control, gint type, gint *value);
gboolean	gst_camera_control_set_detect           (GstCameraControl *control, gint type, gint value);
gboolean	gst_camera_control_get_detect           (GstCameraControl *control, gint type, gint *value);
gboolean	gst_camera_control_set_zoom             (GstCameraControl *control, gint type, gint value);
gboolean	gst_camera_control_get_zoom             (GstCameraControl *control, gint type, gint *value);
gboolean	gst_camera_control_set_focus            (GstCameraControl *control, gint mode, gint range);
gboolean	gst_camera_control_get_focus            (GstCameraControl *control, gint *mode, gint *range);
gboolean	gst_camera_control_start_auto_focus     (GstCameraControl *control);
gboolean	gst_camera_control_stop_auto_focus      (GstCameraControl *control);
gboolean	gst_camera_control_set_focus_level      (GstCameraControl *control, gint manual_level);
gboolean	gst_camera_control_get_focus_level      (GstCameraControl *control, gint *manual_level);
gboolean	gst_camera_control_set_auto_focus_area  (GstCameraControl *control, GstCameraControlRectType rect);
gboolean	gst_camera_control_get_auto_focus_area  (GstCameraControl *control, GstCameraControlRectType *rect);
gboolean	gst_camera_control_set_wdr              (GstCameraControl *control, gint value);
gboolean	gst_camera_control_get_wdr              (GstCameraControl *control, gint *value);
gboolean	gst_camera_control_set_ahs              (GstCameraControl *control, gint value);
gboolean	gst_camera_control_get_ahs              (GstCameraControl *control, gint *value);
gboolean	gst_camera_control_set_part_color       (GstCameraControl *control, gint type, gint value);
gboolean	gst_camera_control_get_part_color       (GstCameraControl *control, gint type, gint *value);
gboolean	gst_camera_control_get_exif_info        (GstCameraControl *control, GstCameraControlExifInfo *info);
gboolean	gst_camera_control_get_basic_dev_info   (GstCameraControl *control, gint dev_id, GstCameraControlCapsInfoType *info);
gboolean	gst_camera_control_get_misc_dev_info    (GstCameraControl *control, gint dev_id, GstCameraControlCtrlListInfoType *info);
gboolean	gst_camera_control_get_extra_dev_info   (GstCameraControl *control, gint dev_id, GstCameraControlExtraInfoType *info);
void		gst_camera_control_set_capture_command  (GstCameraControl *control, GstCameraControlCaptureCommand cmd);
void            gst_camera_control_set_record_command   (GstCameraControl *control, GstCameraControlRecordCommand cmd);
gboolean	gst_camera_control_start_face_zoom      (GstCameraControl *control, gint x, gint y, gint zoom_level);
gboolean	gst_camera_control_stop_face_zoom       (GstCameraControl *control);
gboolean	gst_camera_control_set_ae_lock          (GstCameraControl *control, gboolean lock);
gboolean	gst_camera_control_get_ae_lock          (GstCameraControl *control, gboolean *lock);
gboolean	gst_camera_control_set_awb_lock         (GstCameraControl *control, gboolean lock);
gboolean	gst_camera_control_get_awb_lock         (GstCameraControl *control, gboolean *lock);


/* trigger signal */
void		gst_camera_control_value_changed        (GstCameraControl *control, GstCameraControlChannel *control_channel, gint value);

G_END_DECLS

#endif /* __GST_CAMERA_CONTROL_H__ */
