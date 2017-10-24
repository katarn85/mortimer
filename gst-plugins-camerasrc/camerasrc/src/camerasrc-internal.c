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

#include <math.h>
#include "camerasrc-internal.h"

/*#define USE_IOCTL_DEBUG*/
#if defined (USE_IOCTL_DEBUG)
static char* get_request_name(int request, char* res_str) {
	switch (request) {
	case VIDIOC_QBUF:
		sprintf(res_str, "[VIDIOC_QBUF]");
		break;
	case VIDIOC_DQBUF:
		sprintf(res_str, "[VIDIOC_DQBUF]");
		break;
	case VIDIOC_S_INPUT:
		sprintf(res_str, "[VIDIOC_S_INPUT]");
		break;
	case VIDIOC_G_INPUT:
		sprintf(res_str, "[VIDIOC_G_INPUT]");
		break;
	case VIDIOC_S_PARM:
		sprintf(res_str, "[VIDIOC_S_PARM]");
		break;
	case VIDIOC_G_PARM:
		sprintf(res_str, "[VIDIOC_G_PARM]");
		break;
	case VIDIOC_S_FMT:
		sprintf(res_str, "[VIDIOC_S_FMT]");
		break;
	case VIDIOC_G_FMT:
		sprintf(res_str, "[VIDIOC_G_FMT]");
		break;
	case VIDIOC_REQBUFS:
		sprintf(res_str, "[VIDIOC_REQBUFS]");
		break;
	case VIDIOC_QUERYBUF:
		sprintf(res_str, "[VIDIOC_QUERYBUF]");
		break;
	case VIDIOC_STREAMON:
		sprintf(res_str, "[VIDIOC_STREAMON]");
		break;
	case VIDIOC_STREAMOFF:
		sprintf(res_str, "[VIDIOC_STREAMOFF]");
		break;
	case VIDIOC_S_CTRL:
		sprintf(res_str, "[VIDIOC_S_CTRL] ");
		break;
	case VIDIOC_G_CTRL:
		sprintf(res_str, "[VIDIOC_G_CTRL]");
		break;
	case VIDIOC_ENUMINPUT:
		sprintf(res_str, "[VIDIOC_ENUMINPUT]");
		break;
	case VIDIOC_S_JPEGCOMP:
		sprintf(res_str, "[VIDIOC_S_JPEGCOMP]");
		break;
	case VIDIOC_G_JPEGCOMP:
		sprintf(res_str, "[VIDIOC_G_JPEGCOMP]");
		break;
	/* Extension */
	case VIDIOC_S_STROBE:
		sprintf(res_str, "[VIDIOC_S_STROBE]");
		break;
	case VIDIOC_G_STROBE:
		sprintf(res_str, "[VIDIOC_G_STROBE]");
		break;
	case VIDIOC_S_RECOGNITION:
		sprintf(res_str, "[VIDIOC_S_RECOGNITION]");
		break;
	case VIDIOC_G_RECOGNITION:
		sprintf(res_str, "[VIDIOC_G_RECOGNITION]");
		break;
	case VIDIOC_G_EXIF:
		sprintf(res_str, "[VIDIOC_G_EXIF]");
		break;
	default:
		sprintf(res_str, "[UNKNOWN IOCTL(%x)]", request);
		break;
	}

	return 0;
}

#define PRINT_IOCTL_INFO(request, arg) {\
	char res_str[255];\
	get_request_name(request, res_str);\
	camsrc_info("[request : %s, argument address : %x]", res_str, arg);\
}
#else

#define PRINT_IOCTL_INFO(request, arg)

#endif


#define LOCK(x) {\
	if(0 != pthread_mutex_lock(&(x->mutex))) {\
		camsrc_error("Mutex lock error");\
		camsrc_assert(0);\
	}\
}

#define UNLOCK(x) {\
	if(0 != pthread_mutex_unlock(&(x->mutex))) {\
		camsrc_error("Mutex unlock error");\
		camsrc_assert(0);\
	}\
}

#define SET_CTRL_VAL(cid, in_value) {\
	int err = CAMERASRC_ERR_UNKNOWN;\
	struct v4l2_control control;\
	control.id = cid;\
	control.value = in_value;\
	camsrc_info("[VIDIOC_S_CTRL] >> [%x] request with value %d", cid, in_value); \
	err = _camerasrc_ioctl(handle, VIDIOC_S_CTRL, &control);\
	if(err != CAMERASRC_SUCCESS) {\
		return err;\
	}\
}

#define GET_CTRL_VAL(cid, ret_value) {\
	int err = CAMERASRC_ERR_UNKNOWN;\
	struct v4l2_control control;\
	control.id = cid;\
	err = _camerasrc_ioctl(handle, VIDIOC_G_CTRL, &control);\
	if(err != CAMERASRC_SUCCESS) {\
		return err;\
	}\
	ret_value = control.value;\
	camsrc_info("[VIDIOC_G_CTRL] << [%x] request with value %d", cid, ret_value); \
}

#define SET_CTRL_VAL_ERR(cid, in_value, err) {\
	struct v4l2_control control;\
	control.id = cid;\
	control.value = in_value;\
	camsrc_info("[VIDIOC_S_CTRL] >> [%x] request with value %d", cid, in_value); \
	_camerasrc_ioctl_with_err(handle, VIDIOC_S_CTRL, &control, &err);\
}

#define GET_CTRL_VAL_ERR(cid, ret_value, err) {\
	struct v4l2_control control;\
	control.id = cid;\
	_camerasrc_ioctl_with_err(handle, VIDIOC_G_CTRL, &control, &err);\
	ret_value = control.value;\
	camsrc_info("[VIDIOC_G_CTRL] << [%x] request with value %d", cid, ret_value); \
}

static int _camerasrc_ioctl(camerasrc_handle_t *handle, int request, void *arg)
{
	int fd = handle->dev_fd;
	int err;
	int nAgain = 10;
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};

	LOCK(handle);

	if (handle->dev_fd == CAMERASRC_DEV_FD_EMERGENCY_CLOSED) {
		camsrc_warning("Device emergency closed. All device control will success");
		UNLOCK(handle);
		return CAMERASRC_SUCCESS;
	}

	PRINT_IOCTL_INFO(request, arg);

again:
	do {
		err = ioctl (fd, request, arg);
	} while (-1 == err && EINTR == errno);

	if (err != 0) {
		handle->errnum = errno;
		err = errno;
		strerror_r(err, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_error("ioctl[%x] err : %s", request, err_msg);
		if (err == EEXIST) {
			camsrc_info("EEXIST occured, but can go.");
			err = 0;
		} else if (err == ENOENT) {
			camsrc_info("ENOENT occured, but can go.");
			err = 0;
#if defined (ENABLE_Q_ERROR)
#warning "ENABLE_Q_ERROR enabled"
		} else if (request == VIDIOC_DQBUF) {
			goto DQ_ERROR;
		} else if (request == VIDIOC_QBUF) {
			goto ENQ_ERROR;
#endif
		} else if (err == EINVAL) {
			camsrc_error("EINVAL occured, Shutdown");
			UNLOCK(handle);
			return CAMERASRC_ERR_INVALID_PARAMETER;
		} else if (err == EBUSY) {
			camsrc_error("EBUSY occured, Shutdown");
			UNLOCK(handle);
			return CAMERASRC_ERR_PRIVILEGE;
		} else if (err == EAGAIN && nAgain--) {
			goto again;
		} else {
			/* Why does this return SUCCESS? */
			camsrc_error("Unhandled exception occured on IOCTL");
		}
	}

	UNLOCK(handle);

	return CAMERASRC_SUCCESS;

#if defined (ENABLE_Q_ERROR)
DQ_ERROR:
	camsrc_error("DQ Frame error occured");
	printf("DQ Frame error occured");
	UNLOCK(handle);
	return CAMERASRC_ERR_INTERNAL;

ENQ_ERROR:
	camsrc_error("Q Frame error occured");
	printf("Q Frame error occured");
	UNLOCK(handle);
	return CAMERASRC_ERR_INTERNAL;
#endif
}


static int _camerasrc_ioctl_with_err(camerasrc_handle_t *handle, int request, void *arg, int *error)
{
	int fd = handle->dev_fd;
	int err;
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};

	LOCK(handle);

	*error = 0;

	if (handle->dev_fd == CAMERASRC_DEV_FD_EMERGENCY_CLOSED) {
		camsrc_warning("Device emergency closed. All device control will success");
		UNLOCK(handle);
		return CAMERASRC_SUCCESS;
	}

	PRINT_IOCTL_INFO(request, arg);

	do {
		err = ioctl (fd, request, arg);
	} while (-1 == err && EINTR == errno);

	if (err != 0) {
		handle->errnum = errno;
		*error = errno;
		strerror_r(*error, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_error("ioctl[%x] err : %s", request, err_msg);
		UNLOCK(handle);
		return CAMERASRC_ERR_IO_CONTROL;
	}

	UNLOCK(handle);

	return CAMERASRC_SUCCESS;
}


static int _camerasrc_ioctl_once(camerasrc_handle_t *handle, int request, void *arg)
{
	int fd = handle->dev_fd;
	int err = -1;
	int nAgain = 10;
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};

	LOCK(handle);

	if (handle->dev_fd == CAMERASRC_DEV_FD_EMERGENCY_CLOSED) {
		camsrc_warning("Device emergency closed. All device control will success");
		UNLOCK(handle);
		return CAMERASRC_SUCCESS;
	}

	PRINT_IOCTL_INFO(request, arg);

again:
	err =  ioctl (fd, request, arg);

	if (err != 0) {
		handle->errnum = errno;
		err = errno;
		strerror_r(err, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_error("ioctl[%x] err : %s", request, err_msg);
		if (err == EEXIST) {
			camsrc_info("EEXIST occured, but can go.");
			err = 0;
		} else if (err == ENOENT) {
			camsrc_info("ENOENT occured, but can go.");
			err = 0;
#if defined (ENABLE_Q_ERROR)
#warning "ENABLE_Q_ERROR enabled"
		} else if (request == VIDIOC_DQBUF) {
			goto DQ_ERROR;
		} else if (request == VIDIOC_QBUF) {
			goto ENQ_ERROR;
#endif
		} else if (err == EINVAL) {
			camsrc_error("EINVAL occured, Shutdown");
			UNLOCK(handle);
			return CAMERASRC_ERR_INVALID_PARAMETER;
		} else if (err == EAGAIN && nAgain--) {
			goto again;
		} else {
			camsrc_error("Unhandled exception occured on IOCTL");
		}
	}

	UNLOCK(handle);

	return CAMERASRC_SUCCESS;

#if defined (ENABLE_Q_ERROR)
DQ_ERROR:
	camsrc_error("DQ Frame error occured");
	printf("DQ Frame error occured");
	UNLOCK(handle);
	return CAMERASRC_ERR_INTERNAL;

ENQ_ERROR:
	camsrc_error("Q Frame error occured");
	printf("Q Frame error occured");
	UNLOCK(handle);
	return CAMERASRC_ERR_INTERNAL;
#endif
}


static int _camerasrc_skip_frame(camerasrc_handle_t *handle, long int timeout, int skip_frame)
{
	camerasrc_handle_t *p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;
	fd_set fds;
	struct timeval tv;
	int r;
	int skip_frame_num = skip_frame;

	camsrc_error("enter");

	p = handle;

	while (skip_frame_num--) {
		camsrc_error("SKIP FRAME #%d", skip_frame-skip_frame_num+1);

		struct v4l2_buffer buf;

		FD_ZERO (&fds);
		FD_SET (p->dev_fd, &fds);

		camsrc_error("************Still capture wait frame start");

		/* Timeout. */
		tv.tv_sec = (long int)timeout/1000;
		tv.tv_usec = timeout - tv.tv_sec * 1000;

		r = select (p->dev_fd + 1, &fds, NULL, NULL, &tv);

		if (-1 == r) {
			if (EINTR == errno) {
				return CAMERASRC_SUCCESS;
			}
			camsrc_error("select() failed.");
			return CAMERASRC_ERR_INTERNAL;
		}

		if (0 == r) {
			camsrc_error("select() timeout.");
			return CAMERASRC_ERR_DEVICE_WAIT_TIMEOUT;
		}

		/**@note from samsung camera sample. *****************************************/
		/*SKIP FRAME START*/
		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = p->buffer_idx;

		if (-1 == _camerasrc_ioctl (p, VIDIOC_DQBUF, &buf)) {
			switch (errno) {
			case EAGAIN:
				camsrc_info("VIDIOC_DQBUF [EAGAIN]");
				return CAMERASRC_SUCCESS;
			case EIO:
				/* Could ignore EIO, see spec. */
				/* fall through */
				camsrc_info("VIDIOC_DQBUF [EIO]");
			default:
				camsrc_info("VIDIOC_DQBUF [%d]", errno);
				return CAMERASRC_ERR_IO_CONTROL;
			}
		}

		if (-1 == _camerasrc_ioctl (p, VIDIOC_QBUF, &buf)) {
			return CAMERASRC_ERR_IO_CONTROL;
		}
	}

	camsrc_error("leave");
	err = CAMERASRC_SUCCESS;

	return err;
}

static int _camerasrc_set_cmd(camerasrc_handle_t *handle, _camsrc_cmd_t cmd, void *value)
{
	int err = CAMERASRC_ERR_UNKNOWN;
	struct v4l2_jpegcompression comp_arg;

	switch (cmd) {
	case _CAMERASRC_CMD_CTRL:
		camsrc_info("[_CAMERASRC_CMD_CTRL] cmd set");
		SET_CTRL_VAL(_CAMERASRC_GET_CID(((_camerasrc_ctrl_t *) value)->cid, handle->cur_dev_id), ((_camerasrc_ctrl_t *) value)->value);
		break;
	case _CAMERASRC_CMD_SUPPORT_JPEG_ENCODING:
		camsrc_warning("[_CAMERASRC_CMD_SUPPORT_JPEG_ENCODING] cmd set isn't supported");
		break;
	case _CAMERASRC_CMD_JPEG_COMPRESS_RATIO:
		/* WRITEME */
		camsrc_info("[_CAMERASRC_CMD_JPEG_COMPRESS_RATIO] cmd set, val = %d", (int)(*((unsigned int*) value)));
#ifdef CAMERASRC_SUPPORT_JPEG_CAMERA
//		if (handle->cur_dev_id == CAMERASRC_DEV_ID_PRIMARY) {
			comp_arg.quality = (int)(*((unsigned int*) value));
			err = _camerasrc_ioctl(handle, VIDIOC_S_JPEGCOMP, &comp_arg);
			if (err != CAMERASRC_SUCCESS) {
				goto ERROR;
			}
//		} else if (handle->cur_dev_id == CAMERASRC_DEV_ID_SECONDARY) {
//			err = CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
//			goto ERROR;
//		}
#else
		err = CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
		goto ERROR;
#endif
		break;
#ifdef SUPPORT_CAMERA_SENSOR_MODE		
	case _CAMERASRC_CMD_SENSOR_MODE:
	{
		camerasrc_sensor_mode_t *mode = (camerasrc_sensor_mode_t *)value;
		camsrc_info("[_CAMERASRC_CMD_SENSOR_MODE] cmd set : %d", *mode);
		SET_CTRL_VAL(V4L2_CID_CAMERA_SENSOR_MODE, (int)*mode);
	}
		break;
#endif		
	case _CAMERASRC_CMD_VFLIP:
	{
		int *vflip = (int *)value;
		camsrc_info("[_CAMERASRC_CMD_VFLIP] cmd set : %d", *vflip);
		SET_CTRL_VAL(V4L2_CID_VFLIP, (int)*vflip);
	}
		break;
	case _CAMERASRC_CMD_HFLIP:
	{
		int *hflip = (int *)value;
		camsrc_info("[_CAMERASRC_CMD_HFLIP] cmd set : %d", *hflip);
		SET_CTRL_VAL(V4L2_CID_HFLIP, (int)*hflip);
	}
		break;
	default:
		camsrc_error("[_CAMERASRC_CMD_UNKNOWN] cmd set");
		err = CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
		goto ERROR;
	}

	return CAMERASRC_SUCCESS;

ERROR:
	camsrc_error("cmd execution error occured");

	return err;
}

static int _camerasrc_get_cmd(camerasrc_handle_t *handle, _camsrc_cmd_t cmd, void *value)
{
	int err = CAMERASRC_ERR_UNKNOWN;
	struct v4l2_jpegcompression comp_arg;

	if (!value) {
		camsrc_error("value is NULL");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	switch (cmd) {
	case _CAMERASRC_CMD_SUPPORT_JPEG_ENCODING:
		camsrc_info("[_CAMERASRC_CMD_SUPPORT_JPEG_ENCODING] cmd get(cur_dev_id=%d)", handle->cur_dev_id);
#ifdef CAMERASRC_SUPPORT_JPEG_CAMERA	
//		if (handle->cur_dev_id == CAMERASRC_DEV_ID_PRIMARY) {
			*((int*)value) = 1;
//		} else {
//			*((int*)value) = 0;
//		}
#else
		*((int*)value) = 0;
#endif
		break;
	case _CAMERASRC_CMD_CTRL:
		camsrc_info("[_CAMERASRC_CMD_CTRL] cmd get");
		GET_CTRL_VAL(_CAMERASRC_GET_CID(((_camerasrc_ctrl_t *) value)->cid, handle->cur_dev_id), ((_camerasrc_ctrl_t *) value)->value);
		break;
	case _CAMERASRC_CMD_JPEG_COMPRESS_RATIO:
		camsrc_info("[_CAMERASRC_CMD_JPEG_COMPRESS_RATIO] cmd get");
		err = _camerasrc_ioctl(handle, VIDIOC_G_JPEGCOMP, &comp_arg);
		if (err != CAMERASRC_SUCCESS) {
			goto ERROR;
		}
		*((unsigned int*)value) = (int)comp_arg.quality;
		break;
#ifdef SUPPORT_CAMERA_SENSOR_MODE
	case _CAMERASRC_CMD_SENSOR_MODE:
		camsrc_info("[_CAMERASRC_CMD_SENSOR_MODE] cmd get");
		GET_CTRL_VAL(V4L2_CID_CAMERA_SENSOR_MODE, *(int*)value);
		break;
#endif		
	case _CAMERASRC_CMD_VFLIP:
		camsrc_info("[_CAMERASRC_CMD_VFLIP] cmd get");
		GET_CTRL_VAL(V4L2_CID_VFLIP, *(int*)value);
		break;
	case _CAMERASRC_CMD_HFLIP:
		camsrc_info("[_CAMERASRC_CMD_HFLIP] cmd get");
		GET_CTRL_VAL(V4L2_CID_HFLIP, *(int*)value);
		break;
	default:
		camsrc_error("[_CAMERASRC_CMD_UNKNOWN] cmd get");
		err = CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
		goto ERROR;
	}

	return CAMERASRC_SUCCESS;

ERROR:
	camsrc_error("cmd execution error occured");

	return err;
}

static const CAMERASRC_DEV_DEPENDENT_MISC_FUNC dev_misc_functions = {
	._ioctl            = _camerasrc_ioctl,
	._ioctl_once       = _camerasrc_ioctl_once,
	._skip_frame       = _camerasrc_skip_frame,
	._set_cmd          = _camerasrc_set_cmd,
	._get_cmd          = _camerasrc_get_cmd,
};

const CAMERASRC_DEV_DEPENDENT_MISC_FUNC	*dev_misc_func = &dev_misc_functions;
