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

#include "camerasrc-common.h"
#include "camerasrc-internal.h"
#include <semaphore.h>

#define BUF_LEN 64
#define ITLV_DATA_INFO_OFFSET   0x1000
#define EMBEDDED_OFFSET         (2040*2+4)
#define USE_SUPPORT_RELEASE_BUFFER


/* extended V4L2 API */
#ifndef V4L2_PIX_FMT_INTERLEAVED
#define V4L2_PIX_FMT_INTERLEAVED                v4l2_fourcc('I', 'T', 'L', 'V')
#endif /* V4L2_PIX_FMT_INTERLEAVED */

#ifndef V4L2_CID_IS_S_SCENARIO_MODE
#define V4L2_CID_IS_S_SCENARIO_MODE             (V4L2_CID_FIMC_IS_BASE + 15)
#define V4L2_CID_IS_S_FORMAT_SCENARIO           (V4L2_CID_FIMC_IS_BASE + 16)
enum scenario_mode {
    IS_MODE_PREVIEW_STILL,
    IS_MODE_PREVIEW_VIDEO,
    IS_MODE_CAPTURE_STILL,
    IS_MODE_CAPTURE_VIDEO,
    IS_MODE_MAX
};
#endif /* V4L2_CID_IS_S_SCENARIO_MODE */

enum is_rotation_mode {
	IS_ROTATION_0,
	IS_ROTATION_90,
	IS_ROTATION_180,
	IS_ROTATION_270,
	IS_ROTATION_MAX
};

#ifndef V4L2_CID_EMBEDDEDDATA_ENABLE
#define V4L2_CID_EMBEDDEDDATA_ENABLE            (V4L2_CID_PRIVATE_BASE + 130)
#endif /* V4L2_CID_EMBEDDEDDATA_ENABLE */

/*
 * LOCAL DEFINITIONS
 */
#ifndef EXPORT_API
#define EXPORT_API __attribute__((__visibility__("default")))
#endif

#define LOCK(p) {\
    if(0 != pthread_mutex_lock(&(p->mutex))) {\
        camsrc_error("Mutex locking error");\
        camsrc_assert(0);\
    }\
}

#define UNLOCK(p) {\
    if(0 != pthread_mutex_unlock(&(p->mutex))) {\
        camsrc_error("Mutex unlocking error");\
        camsrc_assert(0);\
    }\
}

extern const CAMERASRC_DEV_DEPENDENT_MISC_FUNC *dev_misc_func;

/** proto type of internal function **/
int _camerasrc_set_jpeg_compress_ratio(camerasrc_handle_t *handle, unsigned int quality);
int _camerasrc_ioctl(camerasrc_handle_t *handle, int request, void *arg);
int _camerasrc_ioctl_once(camerasrc_handle_t *handle, int request, void *arg);
int _camerasrc_skip_frame(camerasrc_handle_t *handle, long int timeout, int skip_frame);

//int _camerasrc_get_exif_info(camerasrc_handle_t *handle, camerasrc_buffer_t *exif_string);
int _camerasrc_query_ctrl_menu(camerasrc_handle_t *handle, struct v4l2_queryctrl queryctrl, camerasrc_ctrl_info_t *ctrl_info);


#if 1
typedef struct {
	char *key_path;
	int   key_prefix;
} camerasrc_sync_param_t;

int camsrc_create_sync(const camerasrc_sync_param_t *param)
{
	sem_t *sem = NULL;

	sem = sem_open(param->key_path, O_CREAT, 0666, 1);
	if (sem == SEM_FAILED) {
		switch (errno) {
		case EACCES:
			camsrc_error("The semaphore already exist, but caller does not have permission %s", param->key_path);
			break;
		case ENOMEM:
			camsrc_error("Insufficient memory");
			break;
		case ENFILE:
			camsrc_error("Too many open files in system");
			break;
		default:
			camsrc_critical("Semaphore create fail! (name:%s, errno %d)", param->key_path, errno);
		}

		return CAMERASRC_ERR_INTERNAL;
	}

	return CAMERASRC_SUCCESS;
}

int camsrc_remove_sync(const camerasrc_sync_param_t *param)
{
	int err = 0;

	err = sem_unlink(param->key_path);
	if (err == -1) {
		camsrc_critical("Semaphore destroy Fail! (name:%s, errno %d)", param->key_path, errno);
		return CAMERASRC_ERR_INTERNAL;
	}

	return CAMERASRC_SUCCESS;
}

int camsrc_lock_sync(const camerasrc_sync_param_t *param)
{
	sem_t *sem = NULL;
	int ret;
	int err = CAMERASRC_SUCCESS;
	int sem_value = -1;
	struct timespec wait_time;

	sem = sem_open(param->key_path, O_CREAT, 0666, 1);
	if (sem == SEM_FAILED)
	{
		camsrc_critical("Semaphore open Fail! (name:%s, errno %d)", param->key_path, errno);
		return CAMERASRC_ERR_INTERNAL;
	}

retry_lock:
	wait_time.tv_sec = (long int)(time(NULL)) + 6;
	wait_time.tv_nsec = 0;

	ret = sem_timedwait(sem, &wait_time);
	if (ret == -1) {
		switch (errno) {
		case EINTR:
			camsrc_critical("Lock RETRY LOCK");
			goto retry_lock;
			break;
		case EINVAL:
			camsrc_critical("Invalid semaphore");
			err = CAMERASRC_ERR_INTERNAL;
			break;
		case EAGAIN:
			camsrc_critical("EAGAIN");
			err = CAMERASRC_ERR_INTERNAL;
			break;
		case ETIMEDOUT:
			camsrc_critical("sem_wait leached %d seconds timeout.", 6);
			/* Recovery of sem_wait lock....in abnormal condition */
			if (0 == sem_getvalue(sem, &sem_value)) {
				camsrc_critical("%s sem value is %d",param->key_path, sem_value);
				if (sem_value == 0) {
					ret = sem_post(sem);
					if (ret == -1) {
						camsrc_critical("sem_post error %s : %d", param->key_path, sem_value);
					} else {
						camsrc_critical("lock recovery success...try lock again");
						goto retry_lock;
					}
				} else {
					camsrc_critical("sem value is not 0. but failed sem_timedwait so retry.. : %s",param->key_path);
					usleep(5);
					goto retry_lock;
				}
			} else {
				camsrc_critical("sem_getvalue failed : %s",param->key_path);
			}
			err = CAMERASRC_ERR_INTERNAL;
			break;
		}
	}
	sem_close(sem);
	return err;
}

int camsrc_unlock_sync(const camerasrc_sync_param_t *param)
{
	sem_t *sem = NULL;
	int ret;
	int err = CAMERASRC_SUCCESS;

	sem = sem_open(param->key_path, O_CREAT, 0666, 1);
	if (sem == SEM_FAILED) {
		camsrc_critical("Semaphore open Fail! (name:%s, errno %d)", param->key_path, errno);
		return CAMERASRC_ERR_INTERNAL;
	}

	ret = sem_post(sem);
	if (ret == -1) {
		camsrc_critical("UNLOCK FAIL");
		err = CAMERASRC_ERR_INTERNAL;
	}

	sem_close(sem);
	return err;
}
#endif


#if defined(USE_CAMERASRC_FRAME_DUMP)
#define WRITE_UNIT  4096
static int seq_no = 0;

static void write_buffer_into_path(camerasrc_buffer_t *buffer, char *path, char *name)
{
	int fd = -1;
	int write_len = 0;
	char temp_fn[255];
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};

	sprintf(temp_fn, "%s/%s", path, name);

	fd = open(temp_fn, O_CREAT | O_WRONLY | O_SYNC, S_IRUSR|S_IWUSR);

	if(fd == -1) {
		strerror_r(errno, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_error("OPEN ERR!!!! : %s", err_msg);
		camsrc_assert(0);
	} else {
		camsrc_info("Open success, FD = %d, seq = %d", fd, seq_no);
	}

	int write_remain = buffer->length;
	int offset = 0;

	while (write_remain > 0) {
		write_len = write(fd, buffer->start + offset, write_remain<WRITE_UNIT?write_remain:WRITE_UNIT);
		if (write_len < 0) {
			strerror_r(errno, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
			camsrc_error("WRITE ERR!!!! : %s", err_msg);
			camsrc_assert(0);
		} else {
			write_remain -= write_len;
			offset+= write_len;
			camsrc_info("%d written, %d left", write_len, write_remain);
		}
	}

	close(fd);
}
#endif

#define CAMERASRC_SET_CMD(cmd, value) _camerasrc_set_cmd(handle, cmd, (void*)value);
#define CAMERASRC_GET_CMD(cmd, value) _camerasrc_get_cmd(handle, cmd, (void*)value);


static int _camerasrc_set_cmd(camsrc_handle_t handle, _camsrc_cmd_t cmd, void *value)
{
	camerasrc_handle_t *p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (dev_misc_func->_set_cmd == NULL) {
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
	}

	err = dev_misc_func->_set_cmd(p, cmd, value);

	return err;
}

static int _camerasrc_get_cmd(camsrc_handle_t handle, _camsrc_cmd_t cmd, void *value)
{
	camerasrc_handle_t *p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	if(handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if(dev_misc_func->_get_cmd == NULL)
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;

	err = dev_misc_func->_get_cmd(p, cmd, value);

	return err;
}


int camerasrc_support_jpeg_encoding(camsrc_handle_t handle, int *support_jpeg_encoding)
{
	int err = CAMERASRC_ERR_UNKNOWN;

	err = CAMERASRC_GET_CMD(_CAMERASRC_CMD_SUPPORT_JPEG_ENCODING, support_jpeg_encoding);
	if (err != CAMERASRC_SUCCESS) {
		*support_jpeg_encoding = 0;
	}

	return err;
}

int camerasrc_set_control(camsrc_handle_t handle, camerasrc_ctrl_t ctrl_id, int value)
{
	_camerasrc_ctrl_t ctrl;

	CLEAR(ctrl);

	ctrl.cid = ctrl_id;
	ctrl.value = value;

	return CAMERASRC_SET_CMD(_CAMERASRC_CMD_CTRL, &ctrl);
}


int camerasrc_get_control(camsrc_handle_t handle, camerasrc_ctrl_t ctrl_id, int *value)
{
	int err = CAMERASRC_ERR_UNKNOWN;
	_camerasrc_ctrl_t ctrl;

	CLEAR(ctrl);

	ctrl.cid = ctrl_id;

	err = CAMERASRC_GET_CMD(_CAMERASRC_CMD_CTRL, &ctrl);
	if (err != CAMERASRC_SUCCESS) {
		return err;
	}

	*value = ctrl.value;

	return err;
}


int camerasrc_query_control(camsrc_handle_t handle, camerasrc_ctrl_t ctrl_id, camerasrc_ctrl_info_t *ctrl_info)
{
	struct v4l2_queryctrl queryctrl;

	if (ctrl_id < CAMERASRC_CTRL_BRIGHTNESS || ctrl_id >= CAMERASRC_CTRL_NUM) {
		camsrc_warning("invalid ctrl_id [%d]", ctrl_id);
		return CAMERASRC_ERR_INVALID_PARAMETER;
	}

	CLEAR(queryctrl);

	queryctrl.id = _CAMERASRC_GET_CID(ctrl_id, CAMERASRC_HANDLE(handle)->cur_dev_id);
	if (queryctrl.id == -1 ) {
		camsrc_warning("not supported ctrl_id [%d]", ctrl_id);
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
	}

	if (0 == _camerasrc_ioctl( handle, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
			camsrc_info("ID [%8x] disabled", queryctrl.id);
		}

		ctrl_info->v4l2_ctrl_id = queryctrl.id;
		ctrl_info->camsrc_ctrl_id = ctrl_id;

		switch (queryctrl.type) {
		case V4L2_CTRL_TYPE_INTEGER:
			ctrl_info->ctrl_type = CTRL_TYPE_RANGE;
			break;
		case V4L2_CTRL_TYPE_BOOLEAN:
			ctrl_info->ctrl_type = CTRL_TYPE_BOOL;
			break;
		case V4L2_CTRL_TYPE_MENU:
			ctrl_info->ctrl_type = CTRL_TYPE_ARRAY;
			_camerasrc_query_ctrl_menu (handle, queryctrl, ctrl_info);
			break;
		default:
			ctrl_info->ctrl_type = CTRL_TYPE_UNKNOWN;
			break;
		}

		memcpy(ctrl_info->ctrl_name, queryctrl.name, MAX_SZ_CTRL_NAME_STRING);
		ctrl_info->min = queryctrl.minimum;
		ctrl_info->max = queryctrl.maximum;
		ctrl_info->step = queryctrl.step;
		ctrl_info->default_val = queryctrl.default_value;

		camsrc_info("Control name : %s, type %d, min %d, max %d, step %d, default %d",
		                       queryctrl.name, queryctrl.type,
		                       queryctrl.minimum, queryctrl.maximum,
		                       queryctrl.step, queryctrl.default_value);
	} else {
		camsrc_error("VIDIOC_QUERYCTRL error");

		return CAMERASRC_ERR_IO_CONTROL;
	}

	return CAMERASRC_SUCCESS;
}


int camerasrc_support_control(camsrc_handle_t handle, camerasrc_ctrl_t ctrl_id)
{
	return CAMERASRC_SUCCESS;
}



int _camerasrc_set_jpeg_compress_ratio(camerasrc_handle_t *handle, unsigned int quality)
{
	return CAMERASRC_SET_CMD(_CAMERASRC_CMD_JPEG_COMPRESS_RATIO, &quality);
}


int _camerasrc_ioctl(camerasrc_handle_t *handle, int request, void *arg)
{
	if (dev_misc_func->_ioctl == NULL) {
#if USE_NOT_SUPPORT_ERR
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
		return CAMERASRC_SUCCESS;
#endif
	}

	return dev_misc_func->_ioctl(handle, request, arg);
}


int _camerasrc_ioctl_once(camerasrc_handle_t *handle, int request, void *arg)
{
	if (dev_misc_func->_ioctl_once == NULL) {
#if USE_NOT_SUPPORT_ERR
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
		return CAMERASRC_SUCCESS;
#endif
	}

	return dev_misc_func->_ioctl_once(handle, request, arg);
}

int _camerasrc_skip_frame(camerasrc_handle_t *handle, long int timeout, int skip_frame)
{
	if (dev_misc_func->_skip_frame == NULL) {
#if USE_NOT_SUPPORT_ERR
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
		return CAMERASRC_SUCCESS;
#endif
	}

	return dev_misc_func->_skip_frame(handle, timeout, skip_frame);
}

static int __camerasrc_open_device(camerasrc_handle_t *p, camerasrc_dev_id_t camera_id)
{
	#define MAX_DEVICE_COUNT 4

	int i = 0;
	int ret = CAMERASRC_ERR_DEVICE_NOT_FOUND;
	char device_name[MAX_SZ_DEV_NAME_STRING] = {'\0',};
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};
	struct v4l2_capability capability;

//	for (i = 0 ; i < MAX_DEVICE_COUNT ; i++) 
	{
		snprintf(device_name, MAX_SZ_DEV_NAME_STRING, "%s%d", CAMERASRC_DEV_NODE_PREFIX, (int)camera_id);
		camsrc_info("try to open %s", device_name);

		p->dev_fd = open(device_name, O_RDWR | O_NONBLOCK, 0);
		if (p->dev_fd < 0) {
			/* open failed */
			strerror_r(errno, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
			camsrc_error("Device open fail [%d][%s]", errno, err_msg);

			switch (errno) {
			case EBUSY:
				ret = CAMERASRC_ERR_DEVICE_BUSY;
				break;
			case ENOENT:
			case ENODEV:
				ret = CAMERASRC_ERR_DEVICE_NOT_FOUND;
				break;
			default:
				ret = CAMERASRC_ERR_DEVICE_OPEN;
				break;
			}

//			break;
		} else {
			int ioctl_ret = 0;

			camsrc_info("opened device fd %d", p->dev_fd);

			/* open success and check capabilities */
			ioctl_ret = _camerasrc_ioctl((camerasrc_handle_t *)p, VIDIOC_QUERYCAP, &capability);
			if (ioctl_ret == CAMERASRC_SUCCESS) {
				/* succeeded : VIDIOC_QUERYCAP */
				camsrc_info("capabilities %x", capability.capabilities);
				if (capability.capabilities & V4L2_CAP_VIDEO_CAPTURE || capability.capabilities & V4L2_CAP_STREAMING) {
					camsrc_info("found capture device %s", device_name);

#ifndef V4L2_CAP_SHARE_PADDR
#define V4L2_CAP_SHARE_PADDR   0x10000000
#define V4L2_CAP_SHARE_FD      0x20000000
#define V4L2_CAP_SHARE_ION     0x40000000
#endif /* V4L2_CAP_SHARE_PADDR */

					/* check buffer share method */
					if (capability.capabilities & V4L2_CAP_SHARE_PADDR) {
						p->buf_share_method = BUF_SHARE_METHOD_PADDR;
					} else if (capability.capabilities & V4L2_CAP_SHARE_FD) {
						p->buf_share_method = BUF_SHARE_METHOD_FD;
					} else {
						p->buf_share_method = BUF_SHARE_METHOD_PADDR;
					}

					camsrc_info("buffer share method %d, capabilities 0x%x",
					            p->buf_share_method, capability.capabilities);

					return CAMERASRC_SUCCESS;
				}
			} else {
				camsrc_warning("VIDIOC_QUERYCAP error %x", ioctl_ret);
			}

			close(p->dev_fd);
			p->dev_fd = -1;
		}
	}

	camsrc_error("failed to find capture device %x", ret);

	return ret;
}

static int _camerasrc_initialize_handle(camerasrc_handle_t *handle)
{
	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	/* Initialize handle variables */
	handle->buffer = NULL;
	handle->is_async_open = 0;
	handle->dev_fd = -1;
	handle->cur_dev_id = CAMERASRC_DEV_ID_UNKNOWN;
	handle->is_preset = 0;
	handle->resolution = 0;
	handle->buffer_idx = 0;
	handle->num_buffers = 0;
	handle->buffer = NULL;
	handle->present_buf = NULL;
	handle->io_method = CAMERASRC_IO_METHOD_MMAP;
	handle->skip_frame_func = NULL;
	handle->first_frame = 1;

	CLEAR(handle->queued_buf_list);
	CLEAR(handle->format);

	return CAMERASRC_SUCCESS;
}


static int _camerasrc_wait_frame_available(camerasrc_handle_t *handle, long int timeout)
{
	camerasrc_handle_t *p = handle;
	fd_set fds;
	struct timeval tv;
	int r;

#if defined (USE_SKIP_FRAME)
	int skip_frame = CAMERASRC_JPG_STILL_SKIP_FRAME;
#endif

	if (p->dev_fd == CAMERASRC_DEV_FD_EMERGENCY_CLOSED) {
		/* Wait time FPS time */
		if (p->timeperframe.denominator == 0 && p->timeperframe.numerator == 0) {
			usleep((int)1000000/30); /* supposed to 1/30 sec */
		} else {
			usleep((int)(1000000 * p->timeperframe.numerator / p->timeperframe.denominator));
		}

		camsrc_info("Device emergency closed. wait will return proper time");

		return CAMERASRC_SUCCESS;
	}

	/**
	 * @note In preview or video mode, skip_frame_func, signaling_func will be NULL
	 * If unnecessary, just input them to NULL in start_still/preview/video_stream()
	 */

	FD_ZERO(&fds);
	FD_SET(p->dev_fd, &fds);

	/* skip frame */
#if defined (USE_SKIP_FRAME)
	if(p->skip_frame_func != NULL) {
		camsrc_info("Skip frame function enabled!");
		err = p->skip_frame_func(p, timeout, skip_frame);
	}
#endif

	/* set timeout */
	tv.tv_sec = (long int)timeout/1000;
	tv.tv_usec = timeout * 1000 - tv.tv_sec * 1000000;

	/* select waiting */
	r = select(p->dev_fd + 1, &fds, NULL, NULL, &tv);
	if (-1 == r) {
		if (EINTR == errno) {
			return CAMERASRC_SUCCESS;
		}
		camsrc_error("select() failed.");

		return CAMERASRC_ERR_INTERNAL;
	}

	if (0 == r) {
		return CAMERASRC_ERR_DEVICE_WAIT_TIMEOUT;
	}

	return CAMERASRC_SUCCESS;
}


struct v4l2_buffer *_camerasrc_get_correct_buf(camerasrc_handle_t *handle, struct v4l2_buffer *queued_buf_list, camerasrc_buffer_t *buffer)
{
	unsigned int num_buffers = 0;
	int cnt = 0;

	if (camerasrc_get_num_buffer(handle, &num_buffers) == CAMERASRC_SUCCESS) {
		for (cnt = 0 ; cnt < num_buffers ; cnt++) {
			if (queued_buf_list[cnt].m.userptr == (unsigned long)buffer->start) {
				return &(queued_buf_list[cnt]);
			}
		}
	}

	return NULL;
}


static int _camerasrc_queue_buffer(camerasrc_handle_t *handle, int buf_index, camerasrc_buffer_t *buffer)
{
	camerasrc_handle_t *p = handle;
	struct v4l2_buffer vbuf;
	struct v4l2_buffer *pvbuf;
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};

	if (p->dev_fd == CAMERASRC_DEV_FD_EMERGENCY_CLOSED) {
		camsrc_info("Device emergency closed. Q will return successfully");
		return CAMERASRC_SUCCESS;
	}

	if (buf_index >= p->num_buffers) {
		camsrc_info("Late buffer won't be queued, buf_index = %d, p->num_buffers = %d", buf_index, p->num_buffers);
		return CAMERASRC_SUCCESS;
	}

	CLEAR(vbuf);

	switch (p->io_method) {
	case CAMERASRC_IO_METHOD_MMAP:
		if (p->buffer[buf_index].queued_status == CAMERASRC_BUFFER_QUEUED) {
			camsrc_info("buffer[index:%d] is already queued. return success.", buf_index);
			return CAMERASRC_SUCCESS;
		}

		vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vbuf.memory = V4L2_MEMORY_MMAP;
		vbuf.index = buf_index;
		pvbuf = &vbuf;
		break;
	case CAMERASRC_IO_METHOD_USRPTR:
		if (&(p->queued_buf_list[buf_index])) {
			pvbuf = _camerasrc_get_correct_buf(handle, p->queued_buf_list, buffer);
		}

		if (pvbuf == NULL) {
			camsrc_error("Wrong address tried to queueing!, It'll be queued as input index..");
			pvbuf = &(p->queued_buf_list[buf_index]);
		}
		break;
	case CAMERASRC_IO_METHOD_READ:
	default:
		camsrc_error("Device not support that io-method");
#if USE_NOT_SUPPORT_ERR
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
		return CAMERASRC_SUCCESS;
#endif
	}

	if (CAMERASRC_STATE(handle) == CAMERASRC_STATE_READY) {
		camsrc_warning("Q after ready state will success");
		return CAMERASRC_SUCCESS;
	}

	if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_QBUF, pvbuf)) {
		strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_info("VIDIOC_QBUF failed : %s", err_msg);
		return CAMERASRC_ERR_IO_CONTROL;
	}

	LOCK(p);
	if (p->io_method == CAMERASRC_IO_METHOD_MMAP) {
		p->buffer[buf_index].queued_status = CAMERASRC_BUFFER_QUEUED;
		p->queued_buffer_count++;
	}
	UNLOCK(p);

	return CAMERASRC_SUCCESS;
}


static int _camerasrc_get_proper_index(camerasrc_handle_t *handle, void *data)
{
	int i = 0;

	for (i = 0; i < handle->num_buffers; i++) {
		if (handle->buffer[i].start == data) {
			return i;
		}
	}

	camsrc_info("Index NOT found. new buffer... go to error");

	return -1;
}


static int _camerasrc_dequeue_buffer(camerasrc_handle_t *handle, int *buf_index, camerasrc_buffer_t *buffer)
{
	camerasrc_handle_t *p = handle;
	int err = CAMERASRC_ERR_UNKNOWN;
	struct v4l2_buffer vbuf;
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};

	switch(p->io_method) {
	case CAMERASRC_IO_METHOD_MMAP:
		CLEAR(vbuf);
		vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vbuf.memory = V4L2_MEMORY_MMAP;
		break;
	case CAMERASRC_IO_METHOD_USRPTR:
		CLEAR(vbuf);
		vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vbuf.memory = V4L2_MEMORY_USERPTR;
		break;
	case CAMERASRC_IO_METHOD_READ:
	default:
		camsrc_error("Device not support that io-method");
#if USE_NOT_SUPPORT_ERR
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
		return CAMERASRC_SUCCESS;
#endif
	}

	if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_DQBUF, &vbuf)) {
		switch (p->errnum) {
		case EAGAIN:
			camsrc_info("VIDIOC_DQBUF [EAGAIN]");
			return CAMERASRC_SUCCESS;
		case EIO:
			/* Could ignore EIO, see spec. */
			/* fall through */
			camsrc_info("VIDIOC_DQBUF [EIO]");
			break;
		case EINVAL:
			camsrc_info("VIDIOC_DQBUF [EINVAL]");
			printf("VIDIOC_DQBUF [EINVAL]");
			return CAMERASRC_ERR_INVALID_VALUE;
		default:
			strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
			camsrc_info("VIDIOC_DQBUF [%s]", err_msg);
			break;
		}
	}

	if (CAMERASRC_STATE(p) == CAMERASRC_STATE_PREVIEW || CAMERASRC_STATE(p) == CAMERASRC_STATE_VIDEO ||
	    (CAMERASRC_STATE(p) == CAMERASRC_STATE_STILL && p->format.colorspace == CAMERASRC_COL_RAW)) {
		switch(p->io_method) {
		case CAMERASRC_IO_METHOD_MMAP:
			*buf_index = vbuf.index;
			buffer->start = p->buffer[vbuf.index].start;
			LOCK(p);
			p->buffer[vbuf.index].queued_status = CAMERASRC_BUFFER_DEQUEUED;
			p->queued_buffer_count--;
			UNLOCK(p);
#if 1
			switch (p->format.pix_format ) {
			case CAMERASRC_PIX_YUV422P:
			case CAMERASRC_PIX_YUY2:
			case CAMERASRC_PIX_UYVY:
			case CAMERASRC_PIX_RGB565:
				buffer->length = YUV422_SIZE(p);
				break;
			case CAMERASRC_PIX_YUV420P:
			case CAMERASRC_PIX_SN12:
			case CAMERASRC_PIX_ST12:
			case CAMERASRC_PIX_NV12:
				buffer->length = YUV420_SIZE(p);
				break;
			case CAMERASRC_PIX_INTERLEAVED:
			{
				unsigned char *p_size_info = NULL;
#if 0
				if (vbuf.index == 2) {
					FILE *fp = fopen("./test2.dump", "w+");
					fwrite(buffer->start, p->buffer[vbuf.index].length, 1, fp);
					fflush(fp);
					fclose(fp);
				}
#endif
				p_size_info = (unsigned char *)(buffer->start + (p->buffer[vbuf.index].length - ITLV_DATA_INFO_OFFSET));
				buffer->length = (unsigned int)(p_size_info[EMBEDDED_OFFSET]<<24) + (p_size_info[EMBEDDED_OFFSET+1]<<16) + (p_size_info[EMBEDDED_OFFSET+2]<<8) + p_size_info[EMBEDDED_OFFSET+3];
				camsrc_info("total %d, addr %x, data size %d",
				                       (int)(p->buffer[vbuf.index].length), buffer->start, buffer->length);
				break;
			}
			case CAMERASRC_PIX_MJPEG:
				buffer->length = vbuf.bytesused;
				camsrc_info("[CAMERASRC_PIX_MJPEG] total %d, addr %x, data size %d",
				                       (int)(p->buffer[vbuf.index].length), buffer->start, buffer->length);

#if 0
				if (vbuf.index == 2) {
					FILE *fp = fopen("/tmp/test2.jpg", "wb");
					fwrite(buffer->start, buffer->length, 1, fp);
					fflush(fp);
					fclose(fp);
				}
#endif   
				break;
			
			case CAMERASRC_PIX_RGGB10:
			case CAMERASRC_PIX_RGGB8:
			default:
				buffer->length = -1;
				break;
			}
#else
			buffer->length = vbuf.bytesused;
#endif
			break;
		case CAMERASRC_IO_METHOD_USRPTR:
		{
			int i = 0;
			int dequeued = 0;

			for (i = 0; i < p->present_buf->num_buffer; ++i) {
#if defined (USE_USERPTR_DEBUG)
				camsrc_info("buffers[%d].start: %p", i, p->buffer[i].start);
				camsrc_info("buffers[%d].length: %d", i, p->buffer[i].length);
				camsrc_info("buf.m.userptr: %p", vbuf.m.userptr);
				camsrc_info("buf.length: %d", vbuf.length);
#endif
				if (vbuf.m.userptr == (unsigned long) p->buffer[i].start) {
					dequeued = 1;
					break;
				}
			}

			if (!dequeued) {
				camsrc_error("Dequeued buffer not matched to user ptr!");
				return CAMERASRC_ERR_IO_CONTROL;
			}

			*buf_index = _camerasrc_get_proper_index(p, (void*)vbuf.m.userptr);
			buffer->start = (unsigned char*)vbuf.m.userptr;
			buffer->length = vbuf.length;

			break;
		}
		case CAMERASRC_IO_METHOD_READ:
		default:
			camsrc_error("Device not support that io-method");
#if USE_NOT_SUPPORT_ERR
			return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
			return CAMERASRC_SUCCESS;
#endif
			break;
		}
	} 
	else if ((CAMERASRC_STATE(p) == CAMERASRC_STATE_PREVIEW || CAMERASRC_STATE(p) == CAMERASRC_STATE_VIDEO ||
	 	     CAMERASRC_STATE(p) == CAMERASRC_STATE_STILL) && p->format.colorspace == CAMERASRC_COL_JPEG) {
		/* JPEG Capture */
		switch (p->io_method) {
		case CAMERASRC_IO_METHOD_MMAP:
			camsrc_info("JPEG frame dequeueing..");
			*buf_index = vbuf.index;
			buffer->start = p->buffer[vbuf.index].start;
			// TODO:
			// buffer->length = _camerasrc_get_JPG_length(p);
			LOCK(p);
			p->buffer[vbuf.index].queued_status = CAMERASRC_BUFFER_DEQUEUED;
			p->queued_buffer_count--;
			UNLOCK(p);

			/**
			 * In some module(or along firmware), it doesn't support jpg image
			 * splitted with thumbnail And its size always reported as bytesused field.
			 * "buffer->length == 0" means it won't support splitted jpg output
			 */
			camsrc_info("buffer->length = %d, vbuf.bytesused=%d", buffer->length, vbuf.bytesused);
			if (buffer->length == 0) {
				camsrc_info("it'll be copied as size = %d", vbuf.bytesused);
				buffer->length = vbuf.bytesused;
			}

			break;
		case CAMERASRC_IO_METHOD_USRPTR:
		{
			int i = 0;
			int dequeued = 0;

			for (i = 0; i < p->present_buf->num_buffer; ++i) {
#if defined (USE_USERPTR_DEBUG)
				camsrc_info("buffers[%d].start: %d", i, p->buffer[i].start);
				camsrc_info("buffers[%d].length: %d", i, p->buffer[i].length);
				camsrc_info("buf.m.userptr: %d", vbuf.m.userptr);
				camsrc_info("buf.length: %d", vbuf.length);
#endif
				if (vbuf.m.userptr == (unsigned long) p->buffer[i].start) {
					dequeued = 1;
					break;
				}
			}

			if (!dequeued) {
				camsrc_error("Dequeued buffer not matched to user ptr!");
				return CAMERASRC_ERR_IO_CONTROL;
			}

			camsrc_info("JPEG frame dequeueing..");
			*buf_index = vbuf.index;
			buffer->start = (unsigned char*)vbuf.m.userptr;
			// TODO:
			// buffer->length = _camerasrc_get_JPG_length(p);

			if (buffer->length == 0) {
				return CAMERASRC_ERR_INVALID_PARAMETER;
			}
			camsrc_info("JPEG frame length = %d", buffer->length);

			break;
		}
		case CAMERASRC_IO_METHOD_READ:
		default:
			camsrc_error("Device not support that io-method");
#if USE_NOT_SUPPORT_ERR
			return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
			return CAMERASRC_SUCCESS;
#endif
			break;
		}
	} else {
		camsrc_error("Invalid state transition");
		return CAMERASRC_ERR_INVALID_STATE;
	}

	return CAMERASRC_SUCCESS;
}

static int _camerasrc_dump_format(camsrc_handle_t handle);

/**** M A I N    O P E R A T I O N ****/
int camerasrc_create(camsrc_handle_t *phandle)
{
	camerasrc_handle_t *p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};

	camsrc_info("enter");

	if (phandle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = (camerasrc_handle_t *)malloc(sizeof(camerasrc_handle_t));
	if(p == NULL) {
		camsrc_error("malloc fail");
		return CAMERASRC_ERR_ALLOCATION;
	}

	memset(p, '\0', sizeof(camerasrc_handle_t));

	/* STATE TO MEANINGFUL STATE */
	p->cur_state = CAMERASRC_STATE_NONE;
	CAMERASRC_SET_STATE(p, CAMERASRC_STATE_CREATED);
	CAMERASRC_SET_PHASE(p, CAMERASRC_PHASE_NON_RUNNING);
	camsrc_info("Transit to non-running phase");

	/* INIT VARIABLES */
	err = _camerasrc_initialize_handle(p);
	if(err != CAMERASRC_SUCCESS) {
		camsrc_error("Invalid handle");
		return CAMERASRC_ERR_INVALID_HANDLE;
	}

	err = pthread_mutex_init(&(p->mutex), NULL);
	if(err) {
		strerror_r(err, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_info("Mutex creating status : %s", err_msg);
	}

	*phandle = (camsrc_handle_t)p;
	err = CAMERASRC_SUCCESS;
	return err;
}


int camerasrc_destroy(camsrc_handle_t handle)
{
	camerasrc_handle_t* p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};

	camsrc_info("enter");

	if(handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if(CAMERASRC_STATE(p) != CAMERASRC_STATE_UNREALIZED) {
		camsrc_warning("Invalid state transition");
	}

	err = pthread_mutex_destroy(&(p->mutex));
	if(err) {
		strerror_r(err, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_info("Mutex destroying error : %s", err_msg);
	}

	free((void*)p);
	handle = NULL;
	err = CAMERASRC_SUCCESS;
	return err;
}


int camerasrc_close_device(camsrc_handle_t handle)
{
	camerasrc_handle_t* p = NULL;

	camsrc_info("enter");

	if(handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	p->is_async_open = 0;

	if(!CAMERASRC_IS_DEV_CLOSED(p)) {

		if( !(p->is_async_open) ) {
			close(p->dev_fd);
		}

		p->dev_fd = CAMERASRC_DEV_FD_EMERGENCY_CLOSED;
	}

	return CAMERASRC_SUCCESS;
}


int camerasrc_get_state(camsrc_handle_t handle, camerasrc_state_t* state)
{
	camerasrc_handle_t* p = NULL;

	if(handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	*state = CAMERASRC_STATE(p);
	return CAMERASRC_SUCCESS;
}


int camerasrc_realize(camsrc_handle_t handle, camerasrc_dev_id_t camera_id)
{
	camerasrc_handle_t* p = NULL;
	int ret = CAMERASRC_SUCCESS;

	camerasrc_sync_param_t param = {CAMERASRC_SYNC_KEY_PATH, CAMERASRC_SYNC_KEY_PREFIX};
	mode_t mask = 0;

	camsrc_info("enter");

	if(handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	/* STATE OR PHASE CHECKING */
	if(CAMERASRC_STATE(p) == CAMERASRC_STATE_REALIZED) {
		return CAMERASRC_SUCCESS;
	}
	if(CAMERASRC_STATE(p) != CAMERASRC_STATE_CREATED &&
	   CAMERASRC_STATE(p) != CAMERASRC_STATE_UNREALIZED) {
		camsrc_warning("Invalid state transition");
	}

	mask = umask(0);

	/* CREATE SEMAPHORE */
	camsrc_create_sync(&param);

	/* O P E N   D E V I C E */
	if (CAMERASRC_IS_DEV_CLOSED(p)) {
		ret = __camerasrc_open_device(p, camera_id);
		if (ret != CAMERASRC_SUCCESS) {
			camsrc_error("camera open failed %x", ret);
		} else {
			p->is_async_open = 0;
		}
	} else {
		p->is_async_open = 1;
	}

	umask(mask);

	if (ret == CAMERASRC_SUCCESS) {
		CAMERASRC_SET_STATE(p, CAMERASRC_STATE_REALIZED);
	}

	return ret;
}


int camerasrc_unrealize(camsrc_handle_t handle)
{
	camerasrc_handle_t* p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	camsrc_info("enter");

	if(handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if(CAMERASRC_STATE(p) == CAMERASRC_STATE_UNREALIZED ||
			CAMERASRC_STATE(p) == CAMERASRC_STATE_DESTROYED) {
		return CAMERASRC_SUCCESS;
	}
	if(CAMERASRC_STATE(p) != CAMERASRC_STATE_READY) {
		camsrc_warning("Invalid state transition");
	}

	if(!CAMERASRC_IS_DEV_CLOSED(p)) {
		if( !(p->is_async_open) ) {
			close(p->dev_fd);
		}
	}

	p->dev_fd = -1;

	CAMERASRC_SET_STATE(p, CAMERASRC_STATE_UNREALIZED);
	CAMERASRC_SET_PHASE(p, CAMERASRC_PHASE_NON_RUNNING);
	camsrc_info("Transit to non-running phase");
	err = CAMERASRC_SUCCESS;
	return err;
}


int camerasrc_start(camsrc_handle_t handle)
{
	camerasrc_handle_t* p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	camsrc_info("enter");

	if(handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	/* STATE OR PHASE CHECKING */
	if(CAMERASRC_STATE(p) == CAMERASRC_STATE_READY) {
		return CAMERASRC_SUCCESS;
	}
	if(CAMERASRC_STATE(p) != CAMERASRC_STATE_REALIZED) {
		camsrc_warning("Invalid state transition");
	}

	CAMERASRC_SET_STATE(p, CAMERASRC_STATE_READY);
	CAMERASRC_SET_PHASE(p, CAMERASRC_PHASE_RUNNING);

	camsrc_info("Transit to running phase");
	err = CAMERASRC_SUCCESS;
	return err;
}


int camerasrc_present_usr_buffer(camsrc_handle_t handle, camerasrc_usr_buf_t *present_buf, camerasrc_io_method_t io_method)
{
	camerasrc_handle_t *p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_STATE(p) != CAMERASRC_STATE_READY) {
		camsrc_warning("Invalid state transition");
	}

	switch(io_method) {
	case CAMERASRC_IO_METHOD_MMAP:
		camsrc_info("MMAP mode");
		if (present_buf != NULL) {
			camsrc_error("present_buf should be NULL in MMAP Mode");
#if USE_NOT_SUPPORT_ERR
			return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
			return CAMERASRC_SUCCESS;
#endif
		}
		p->io_method = CAMERASRC_IO_METHOD_MMAP;
		break;
	case CAMERASRC_IO_METHOD_USRPTR:
		camsrc_info("USRPTR mode");
		if (present_buf == NULL) {
			camsrc_error("present_buf should NOT be NULL in USRPTR Mode");
			return CAMERASRC_ERR_ALLOCATION;
		}

		if (present_buf->num_buffer > CAMERASRC_USRPTR_MAX_BUFFER_NUM) {
			camsrc_warning("Use up tp MAX %d buffer only", CAMERASRC_USRPTR_MAX_BUFFER_NUM);
			present_buf->num_buffer = CAMERASRC_USRPTR_MAX_BUFFER_NUM;
		}
		p->present_buf = present_buf;
		p->io_method = CAMERASRC_IO_METHOD_USRPTR;
		break;
	case CAMERASRC_IO_METHOD_READ:
	default:
#if USE_NOT_SUPPORT_ERR
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
		return CAMERASRC_SUCCESS;
#endif
		break;
	}

	err = CAMERASRC_SUCCESS;
	return err;
}


int camerasrc_get_num_buffer(camsrc_handle_t handle, unsigned int *num_buffer)
{
	camerasrc_handle_t *p = NULL;

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	camerasrc_state_t state = CAMERASRC_STATE_NONE;
	camerasrc_get_state(handle, &state);

	switch(state) {
	case CAMERASRC_STATE_NONE:
	case CAMERASRC_STATE_CREATED:
	case CAMERASRC_STATE_REALIZED:
	case CAMERASRC_STATE_READY:
	case CAMERASRC_STATE_UNREALIZED:
	case CAMERASRC_STATE_DESTROYED:
		camsrc_warning("Current buffer number not initialized.");
		*num_buffer = 0;
		break;
	case CAMERASRC_STATE_PREVIEW:
	case CAMERASRC_STATE_STILL:
	case CAMERASRC_STATE_VIDEO:
		*num_buffer = p->num_buffers;
		break;
	default:
		camsrc_error("Unknown state");
		*num_buffer = -1;
		return CAMERASRC_ERR_INVALID_STATE;
	}

	return CAMERASRC_SUCCESS;
}


int camerasrc_get_io_method(camsrc_handle_t handle, camerasrc_io_method_t* io_method)
{
	camerasrc_handle_t *p = NULL;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	*io_method = p->io_method;

	return CAMERASRC_SUCCESS;
}


int camerasrc_get_buffer_share_method(camsrc_handle_t handle, buf_share_method_t *buf_share_method)
{
	camerasrc_handle_t *p = NULL;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	*buf_share_method = p->buf_share_method;

	return CAMERASRC_SUCCESS;
}


int camerasrc_start_still_stream(camsrc_handle_t handle)
{
	camerasrc_handle_t* p = NULL;
	struct v4l2_streamparm vstreamparm;
	struct v4l2_format vformat;
	struct v4l2_requestbuffers	vreq_bufs;
	enum v4l2_buf_type vtype;
	int cnt = 0;
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_STATE(p) == CAMERASRC_STATE_STILL) {
		return CAMERASRC_SUCCESS;
	}

	if (CAMERASRC_STATE(p) != CAMERASRC_STATE_READY) {
		camsrc_warning("Invalid state transition");
	}

	/* S T I L L   M O D E   S E T T I N G */
	/*
	 * If driver can control thmbnl size, we do that here, if not support, it's OK
	 * Driver should change jpeg compress ration before start stream
	 */
	CLEAR(vstreamparm);
	vstreamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	__ta__( "                Stillshot VIDIOC_G_PARM",
	if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_G_PARM, &vstreamparm)) {
		strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_error("VIDIOC_G_PARAM failed : %s", err_msg);
		return CAMERASRC_ERR_IO_CONTROL;
	}
	);

	if (p->format.is_highquality_mode == 1) {
		camsrc_info("High quality frame will be captured");
		vstreamparm.parm.capture.capturemode = V4L2_MODE_HIGHQUALITY;
	} else if(p->format.is_highquality_mode == 0) {
		camsrc_info("Normal quality frame will be captured");
		vstreamparm.parm.capture.capturemode = 0;
	} else {
		camsrc_info("Quality not initialized, or incorrect value. set high quality...");
		vstreamparm.parm.capture.capturemode = V4L2_MODE_HIGHQUALITY;
	}

	vstreamparm.parm.capture.timeperframe.numerator = p->timeperframe.numerator;
	vstreamparm.parm.capture.timeperframe.denominator = p->timeperframe.denominator;
	camsrc_info("[FPS] timeperframe.numerator = %d", p->timeperframe.numerator);
	camsrc_info("[FPS] timeperframe.denominator = %d", p->timeperframe.denominator);
	camsrc_info("[QUA] vstreamparm.parm.capture.capturemode = %d", vstreamparm.parm.capture.capturemode);
	__ta__( "                Stillshot VIDIOC_S_PARM",
	if(CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_S_PARM, &vstreamparm)) {
		strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_error("VIDIOC_S_PARAM failed : %s", err_msg);
		return CAMERASRC_ERR_IO_CONTROL;
	}
	);

	/* S T I L L   F O R M A T   S E T T I N G */
	CLEAR(vformat);
	_camerasrc_dump_format(p);
	vformat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	switch (p->format.pix_format) {
	case CAMERASRC_PIX_YUV420P:
		camsrc_info("420P capture activated..");
		vformat.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
		break;
	case CAMERASRC_PIX_YUV422P:
		camsrc_info("422P capture activated..");
		vformat.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV422P;
		break;
	case CAMERASRC_PIX_SN12:
	case CAMERASRC_PIX_NV12:
		camsrc_info("NV12(SN12) capture activated..");
		vformat.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
		break;
	case CAMERASRC_PIX_YUY2:
		camsrc_info("YUY2 capture activated..");
		vformat.fmt.pix.pixelformat = CAMERASRC_V4L2_PREVIEW_PIX_FMT_DEFAULT;
		break;
		/* Kessler capture path */
	case CAMERASRC_PIX_RGGB8:
		camsrc_info("JPEG+JPEG capture activated..");
		vformat.fmt.pix.pixelformat = CAMERASRC_V4L2_JPEG_CAPTURE_PIX_FMT_DEFAULT;
		break;
	case CAMERASRC_PIX_RGGB10:
		camsrc_info("JPEG+YUV capture activated..");
		vformat.fmt.pix.pixelformat = CAMERASRC_V4L2_JPG_YUV_CAPTURE_PIX_FMT_DEFAULT;
		break;
	case CAMERASRC_PIX_MJPEG:
		camsrc_info("JPEG capture activated..");
		vformat.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
		break;
	case CAMERASRC_PIX_ST12:	
	default:
		camsrc_error("Not supported format!");
		break;
	}

	camsrc_info("JPG unified size capture avtivated!");
	vformat.fmt.pix.width = p->format.img_size.dim.width;		/*capture width*/
	vformat.fmt.pix.height = p->format.img_size.dim.height;	/*capture height*/
	vformat.fmt.pix_mp.field = V4L2_FIELD_NONE;
	__ta__( "                Stillshot VIDIOC_S_FMT",
	if(CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_S_FMT, &vformat)) {
		strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_error("VIDIOC_S_FMT failed : %s", err_msg);
		return CAMERASRC_ERR_IO_CONTROL;
	}
	);

	camsrc_info("Stillshot VIDIOC_S_FMT done");

	CLEAR(vreq_bufs);

	/* M E M O R Y   M A P P I N G */
	switch (p->io_method) {
	case CAMERASRC_IO_METHOD_MMAP:
		camsrc_info("MMAP mode");

		if (vformat.fmt.pix.pixelformat == CAMERASRC_V4L2_JPEG_CAPTURE_PIX_FMT_DEFAULT ) {
			vreq_bufs.count = CAMERASRC_STILL_BUFFER_NUM;
		} else {
			vreq_bufs.count = CAMERASRC_PREVIEW_BUFFER_NUM;
		}
		vreq_bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vreq_bufs.memory = V4L2_MEMORY_MMAP;

		camsrc_info("VIDIOC_REQBUFS count:%d, type:%d, memory:%d",
					vreq_bufs.count, vreq_bufs.type, vreq_bufs.memory);

		__ta__( "                Stillshot VIDIOC_REQBUFS",
		if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_REQBUFS, &vreq_bufs)) {
			strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
			camsrc_error("VIDIOC_REQBUFS failed : %s", err_msg);
			return CAMERASRC_ERR_IO_CONTROL;
		}
		);

		camsrc_info("Stillshot VIDIOC_REQBUFS done");
		camsrc_assert(vreq_bufs.count >= 1);

		p->num_buffers = vreq_bufs.count;
		p->buffer = calloc (vreq_bufs.count, sizeof (camerasrc_buffer_t));

		if (!p->buffer) {
			camsrc_error("calloc() failed");
			return CAMERASRC_ERR_ALLOCATION;
		}

		__ta__( "                Stillshot VIDIOC_QUERYBUF,mmap",
		for (p->buffer_idx = 0 ; p->buffer_idx < vreq_bufs.count ; ++(p->buffer_idx)) {
			struct v4l2_buffer vbuf;
			CLEAR(vbuf);

			vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			vbuf.memory = V4L2_MEMORY_MMAP;
			vbuf.index = p->buffer_idx;
			if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_QUERYBUF, &vbuf)) {
				strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
				camsrc_error("VIDIOC_QUERYBUF failed : %s", err_msg);
				return CAMERASRC_ERR_IO_CONTROL;
			}

			p->buffer[p->buffer_idx].length = vbuf.length;
			p->buffer[p->buffer_idx].start = mmap(NULL /* start anywhere */,
			                                      vbuf.length,
			                                      PROT_READ | PROT_WRITE,
			                                      MAP_SHARED,
			                                      p->dev_fd, vbuf.m.offset);
			if (MAP_FAILED == p->buffer[p->buffer_idx].start) {
				camsrc_error("mmap failed.");
				return CAMERASRC_ERR_ALLOCATION;
			}

			camsrc_info("MMAP BUF: addr[%p] size[%d]",
			                       p->buffer[p->buffer_idx].start, p->buffer[p->buffer_idx].length);
		}
		);

		camsrc_info("Stillshot VIDIOC_QUERYBUF and mmap done");
		break;

	case CAMERASRC_IO_METHOD_USRPTR:
		camsrc_info("USRPTR mode");

		vreq_bufs.count = p->present_buf->num_buffer;
		vreq_bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vreq_bufs.memory = V4L2_MEMORY_USERPTR;
		if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_REQBUFS, &vreq_bufs)) {
			strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
			camsrc_error("VIDIOC_REQBUFS failed : %s", err_msg);
			return CAMERASRC_ERR_IO_CONTROL;
		}

		camsrc_assert(vreq_bufs.count >= 1);

		p->num_buffers = vreq_bufs.count;
		p->buffer = calloc (vreq_bufs.count, sizeof (camerasrc_buffer_t));

		if (!p->buffer) {
			camsrc_error("calloc() failed");
			return CAMERASRC_ERR_ALLOCATION;
		}

		if (!p->present_buf->present_buffer) {
			camsrc_error("No Usrptr buffer presented!");
			return CAMERASRC_ERR_ALLOCATION;
		}

		for (p->buffer_idx = 0; p->buffer_idx < vreq_bufs.count; ++(p->buffer_idx)) {
			p->buffer[p->buffer_idx].length = p->present_buf->present_buffer[p->buffer_idx].length;
			p->buffer[p->buffer_idx].start = p->present_buf->present_buffer[p->buffer_idx].start;

			if (!p->buffer[p->buffer_idx].start) {
				camsrc_error("presented usrptr buffer is NULL");
				return CAMERASRC_ERR_ALLOCATION;
			}

			camsrc_info("USERPTR BUF: addr[%p] size[%d]",
			                       p->buffer[p->buffer_idx].start, p->buffer[p->buffer_idx].length);
		}
		break;

	case CAMERASRC_IO_METHOD_READ:
	default:
		camsrc_error("Device not support that io-method");
#if USE_NOT_SUPPORT_ERR
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
		return CAMERASRC_SUCCESS;
#endif
		break;
	}

	/* calibrate rotation */
	if (p->lens_rotation != -1) {
		if (p->cur_dev_id == CAMERASRC_DEV_ID_PRIMARY) {
			/* rear camera */
			switch (p->lens_rotation) {
			case IS_ROTATION_180:
			case IS_ROTATION_270:
				p->format.rotation += 180;
				p->format.rotation = p->format.rotation % 360;
				camsrc_info("Calibrate rotate : +180 degree");
				break;
			default:
				camsrc_info("No need to calibrate");
				break;
			}
		} else {
			/* front camera */
			switch (p->lens_rotation) {
			case IS_ROTATION_90:
			case IS_ROTATION_180:
				p->format.rotation += 180;
				p->format.rotation = p->format.rotation % 360;
				camsrc_info("Calibrate rotate : +180 degree");
				break;
			default:
				camsrc_info("No need to calibrate");
				break;
			}
		}
	} else {
		camsrc_warning("Invalid lens rotation. skip rotation calibration.");
	}

	/* Sensor flip */
	__ta__( "            _CAMERASRC_CMD_VFLIP",
	CAMERASRC_SET_CMD(_CAMERASRC_CMD_VFLIP, &(p->vflip));
	);

	__ta__( "            _CAMERASRC_CMD_HFLIP",
	CAMERASRC_SET_CMD(_CAMERASRC_CMD_HFLIP, &(p->hflip));
	);

#if 0
	/* set JPEG quality */
	if (p->format.pix_format == CAMERASRC_PIX_RGGB8) {
		int err = CAMERASRC_ERR_UNKNOWN;
		struct v4l2_control control;

		control.id = V4L2_CID_CAM_JPEG_QUALITY;
		control.value = p->format.quality;
		camsrc_info("[VIDIOC_S_CTRL] V4L2_CID_CAM_JPEG_QUALITY -> %d", control.value);
		__ta__( "                Stillshot V4L2_CID_CAM_JPEG_QUALITY",
		err = _camerasrc_ioctl(p, VIDIOC_S_CTRL, &control);
		);
		if (err != CAMERASRC_SUCCESS) {
			strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
			camsrc_error("V4L2_CID_CAMERA_CAPTURE failed : %s", err_msg);
			return err;
		}
	}
#endif
	/* A C T U A L   S T A R T   S T R E A M */
	switch(p->io_method) {
	case CAMERASRC_IO_METHOD_MMAP:
		camsrc_info("MMAP mode");

		LOCK(p);
		p->queued_buffer_count = 0;
		UNLOCK(p);

		for (cnt = 0; cnt < p->num_buffers; cnt++) {
			struct v4l2_buffer lvbuf;
			CLEAR(lvbuf);

			lvbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			lvbuf.memory = V4L2_MEMORY_MMAP;
			lvbuf.index = cnt;
			__ta__( "                Stillshot VIDIOC_QBUF",
			if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_QBUF, &lvbuf)) {
				strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
				camsrc_error("VIDIOC_QBUF failed : %s", err_msg);
				return CAMERASRC_ERR_IO_CONTROL;
			}
			);

			LOCK(p);
			p->buffer[lvbuf.index].queued_status = CAMERASRC_BUFFER_QUEUED;
			p->queued_buffer_count++;
			UNLOCK(p);
		}

		camsrc_info("Stillshot VIDIOC_QBUF done");

		vtype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		__ta__( "                Stillshot VIDIOC_STREAMON",
		if( CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_STREAMON, &vtype)) {
			strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
			camsrc_error("VIDIOC_STREAMON failed : %s", err_msg);
			return CAMERASRC_ERR_IO_CONTROL;
		}
		);
		camsrc_info("Stillshot VIDIOC_STREAMON done");
		break;

	case CAMERASRC_IO_METHOD_USRPTR:
		camsrc_info("USRPTR mode");
		for (cnt = 0; cnt < p->present_buf->num_buffer; cnt++) {
			struct v4l2_buffer lvbuf;
			CLEAR(lvbuf);

			lvbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			lvbuf.memory = V4L2_MEMORY_USERPTR;
			lvbuf.index = cnt;
			lvbuf.m.userptr = (unsigned long)p->buffer[cnt].start;
			lvbuf.length = p->buffer[cnt].length;

			if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_QBUF, &lvbuf)) {
				strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
				camsrc_error("VIDIOC_QBUF failed : %s", err_msg);
				return CAMERASRC_ERR_IO_CONTROL;
			}
		}

		camsrc_info("Stillshot VIDIOC_QBUF done");

		vtype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_STREAMON, &vtype)) {
			strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
			camsrc_error("VIDIOC_STREAMON failed : %s", err_msg);
			return CAMERASRC_ERR_IO_CONTROL;
		}
		camsrc_info("Stillshot VIDIOC_STREAMON done");
		break;

	case CAMERASRC_IO_METHOD_READ:
	default:
		camsrc_error("Device not support that io-method");
#if USE_NOT_SUPPORT_ERR
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
		return CAMERASRC_SUCCESS;
#endif
		break;
	}

	/* C210 Legacy V4L2 API for capture */
#if 0	
	if (p->format.pix_format == CAMERASRC_PIX_RGGB8) {
		int err = CAMERASRC_ERR_UNKNOWN;
		struct v4l2_control control;

		control.id = V4L2_CID_CAMERA_CAPTURE;
		control.value = 0;
		camsrc_info("[VIDIOC_S_CTRL] V4L2_CID_CAMERA_CAPTURE -> 0");
		__ta__( "                Stillshot V4L2_CID_CAMERA_CAPTURE",
		err = _camerasrc_ioctl(p, VIDIOC_S_CTRL, &control);
		);
		if (err != CAMERASRC_SUCCESS) {
			strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
			camsrc_error("V4L2_CID_CAMERA_CAPTURE failed : %s", err_msg);
			return err;
		}
	}
#endif

	LOCK(p);
	__ta__( "                Stillshot skip_frame_func",
	if (_CAMERASRC_NEED_MISC_FUNCTION(p->cur_dev_id, CAMERASRC_OP_CAPTURE, p->format.colorspace, CAMERASRC_MISC_SKIP_FRAME)) {
		p->skip_frame_func = (camerasrc_skip_frame_func_t)_camerasrc_skip_frame;
	} else {
		p->skip_frame_func = NULL;
	}
	);
	UNLOCK(p);

#if defined(USE_SKIP_FRAME_AT_RAW_FRAME)
	/*< If YUV capture, */
	if (!p->format.is_highquality_mode) {
		if (p->skip_frame_func != NULL) {
			p->skip_frame_func(p, 5000, 3);
		}
	}
#endif

	CAMERASRC_SET_STATE(p, CAMERASRC_STATE_STILL);

	return CAMERASRC_SUCCESS;
}


int camerasrc_start_preview_stream(camsrc_handle_t handle)
{
	camerasrc_handle_t* p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	struct v4l2_streamparm vstreamparm;
	struct v4l2_format vformat;
	struct v4l2_requestbuffers vreq_bufs;
//	struct v4l2_control control;
	enum v4l2_buf_type vtype;
	int cnt = 0;
	int preview_width = 0;
	int preview_height = 0;
/*
	int capture_width = 0;
	int capture_height = 0;
*/
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	/* STATE OR PHASE CHECKING */
	if(CAMERASRC_STATE(p) == CAMERASRC_STATE_PREVIEW) {
		return CAMERASRC_SUCCESS;
	}
	if(CAMERASRC_STATE(p) != CAMERASRC_STATE_READY) {
		camsrc_warning("Invalid state transition");
	}

	/* P R E V I E W   M O D E   S E T T I N G */
	CLEAR(vstreamparm);
	vstreamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	__ta__( "            VIDIOC_G_PARM",
	if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_G_PARM, &vstreamparm)) {
		strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_error("VIDIOC_G_PARAM failed : %s", err_msg);
		return CAMERASRC_ERR_IO_CONTROL;
	}
	);

	vstreamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if ((vstreamparm.parm.capture.timeperframe.numerator != p->timeperframe.numerator) ||
	    (vstreamparm.parm.capture.timeperframe.denominator != p->timeperframe.denominator) ||
	    ((p->format.is_highquality_mode == 1) &&(vstreamparm.parm.capture.capturemode != V4L2_MODE_HIGHQUALITY)) ||
	    ((p->format.is_highquality_mode == 0) &&(vstreamparm.parm.capture.capturemode != 0))) {
		if (p->format.is_highquality_mode == 1) {
			camsrc_info("High quality frame will be captured");
			vstreamparm.parm.capture.capturemode = V4L2_MODE_HIGHQUALITY;
		} else if (p->format.is_highquality_mode == 0) {
			camsrc_info("Normal quality frame will be captured");
			vstreamparm.parm.capture.capturemode = 0;
		} else {
			camsrc_info("Quality not initialized, or incorrect value. set normal quality...");
			vstreamparm.parm.capture.capturemode = 0;
		}

		vstreamparm.parm.capture.timeperframe.numerator = p->timeperframe.numerator;
		vstreamparm.parm.capture.timeperframe.denominator = p->timeperframe.denominator;

		camsrc_info("[FPS] timeperframe.numerator = %d", p->timeperframe.numerator);
		camsrc_info("[FPS] timeperframe.denominator = %d", p->timeperframe.denominator);
		camsrc_info("[QUA] vstreamparm.parm.capture.capturemode = %d", vstreamparm.parm.capture.capturemode);

		__ta__( "            VIDIOC_S_PARM",
		if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_S_PARM, &vstreamparm)) {
			strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
			camsrc_error("VIDIOC_S_PARAM failed : %s", err_msg);
			return CAMERASRC_ERR_IO_CONTROL;
		}
		);
	}

	/* P R E V I E W   F O R M A T   S E T T I N G */
	if (!(p->format.pix_format == CAMERASRC_PIX_YUV422P || p->format.pix_format == CAMERASRC_PIX_YUV420P ||
	      p->format.pix_format == CAMERASRC_PIX_SN12 || p->format.pix_format == CAMERASRC_PIX_ST12 ||
	      p->format.pix_format == CAMERASRC_PIX_NV12 || p->format.pix_format == CAMERASRC_PIX_YUY2 ||
	      p->format.pix_format == CAMERASRC_PIX_UYVY || p->format.pix_format == CAMERASRC_PIX_INTERLEAVED || 
	      p->format.pix_format == CAMERASRC_PIX_MJPEG) ) {
		camsrc_error("Invalid output format.");
		return CAMERASRC_ERR_INVALID_FORMAT;
	}

/*
	if (p->format.colorspace != CAMERASRC_COL_RAW) {
		camsrc_error("Invalid store method.");
		return CAMERASRC_ERR_INVALID_PARAMETER;
	}
*/
	err = _camerasrc_dump_format(handle);
	if (err != CAMERASRC_SUCCESS) {
		camsrc_error("Format dump error");
		return err;
	}

	preview_width = p->format.img_size.dim.width;
	preview_height = p->format.img_size.dim.height;

	CLEAR(vformat);
	vformat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vformat.fmt.pix.width = preview_width;
	vformat.fmt.pix.height = preview_height;
	vformat.fmt.pix.field = V4L2_FIELD_NONE;

	switch (p->format.pix_format) {
	case CAMERASRC_PIX_YUV422P:
		vformat.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV422P;
		vformat.fmt.pix.bytesperline = preview_width << 1;
		vformat.fmt.pix.sizeimage = (preview_width * preview_height) << 1;
		break;
	case CAMERASRC_PIX_YUY2:
		vformat.fmt.pix.pixelformat	= V4L2_PIX_FMT_YUYV;
		vformat.fmt.pix.bytesperline = preview_width << 1;
		vformat.fmt.pix.sizeimage = (preview_width * preview_height) << 1;
		break;
	case CAMERASRC_PIX_UYVY:
		vformat.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
		vformat.fmt.pix.bytesperline = preview_width << 1;
		vformat.fmt.pix.sizeimage = (preview_width * preview_height) << 1;
		break;
	case CAMERASRC_PIX_YUV420P:
		vformat.fmt.pix.pixelformat	= V4L2_PIX_FMT_YUV420;
		vformat.fmt.pix.bytesperline = (preview_width * 3) >> 1;
		vformat.fmt.pix.sizeimage = (preview_width * preview_height * 3) >> 1;
		break;		
	case CAMERASRC_PIX_NV12:
	case CAMERASRC_PIX_SN12:
		vformat.fmt.pix.pixelformat	= V4L2_PIX_FMT_NV12;
		vformat.fmt.pix.bytesperline = (preview_width * 3) >> 1;
		vformat.fmt.pix.sizeimage = (preview_width * preview_height * 3) >> 1;
		break;
	case CAMERASRC_PIX_MJPEG:
		vformat.fmt.pix.pixelformat	= V4L2_PIX_FMT_MJPEG;
		break;
		
	case CAMERASRC_PIX_ST12:
	case CAMERASRC_PIX_INTERLEAVED: /* JPEG + YUYV */
	default:
		camsrc_error("Invalid output format.");
		break;
	}

	camsrc_info("===========================================");
	camsrc_info("| Request output width = %d", vformat.fmt.pix.width);
	camsrc_info("| Request output height = %d", vformat.fmt.pix.height);
	camsrc_info("| Request output field = %d", vformat.fmt.pix.field);
	camsrc_info("| Request output pixel format = %d", vformat.fmt.pix.pixelformat);
	camsrc_info("| Request output bytesperline = %d", vformat.fmt.pix.bytesperline);
	camsrc_info("| Request output image size = %d", vformat.fmt.pix.sizeimage);
	camsrc_info("===========================================");

	__ta__( "            VIDIOC_S_FMT",
	if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_S_FMT, &vformat)) {
		strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_error("VIDIOC_S_FMT failed : %s", err_msg);
		return CAMERASRC_ERR_IO_CONTROL;
	}
	);

	/* M E M O R Y   M A P P I N G */
	CLEAR(vreq_bufs);
	switch(p->io_method) {
	case CAMERASRC_IO_METHOD_MMAP:
		camsrc_info("MMAP mode");
		vreq_bufs.count = CAMERASRC_PREVIEW_BUFFER_NUM;
		vreq_bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vreq_bufs.memory = V4L2_MEMORY_MMAP;
		camsrc_info("MMAP mode (bufs.count:%d)", vreq_bufs.count);
		__ta__( "            VIDIOC_REQBUFS",
		if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_REQBUFS, &vreq_bufs)) {
			strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
			camsrc_error("VIDIOC_REQBUFS failed : %s", err_msg);
			return CAMERASRC_ERR_IO_CONTROL;
		}
		);
		camsrc_assert(vreq_bufs.count >= 1);

		p->num_buffers = vreq_bufs.count;
		p->buffer = calloc (vreq_bufs.count, sizeof (camerasrc_buffer_t));
		if (!p->buffer) {
			camsrc_error("calloc() failed");
			return CAMERASRC_ERR_ALLOCATION;
		}

		for (p->buffer_idx = 0; p->buffer_idx < vreq_bufs.count; ++(p->buffer_idx)) {
			struct v4l2_buffer vbuf;
			CLEAR(vbuf);
			vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			vbuf.memory = V4L2_MEMORY_MMAP;
			vbuf.index = p->buffer_idx;

			__ta__( "            VIDIOC_QUERYBUF",
			if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_QUERYBUF, &vbuf)) {
				strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
				camsrc_error("VIDIOC_QUERYBUF failed : %s", err_msg);
				return CAMERASRC_ERR_IO_CONTROL;
			}
			);

			__ta__( "            mmap",
			p->buffer[p->buffer_idx].length = vbuf.length;
			p->buffer[p->buffer_idx].start = mmap(NULL /* start anywhere */,
			                                      vbuf.length,
			                                      PROT_READ | PROT_WRITE,
			                                      MAP_SHARED,
			                                      p->dev_fd, vbuf.m.offset);
			);
			if (MAP_FAILED == p->buffer[p->buffer_idx].start) {
				camsrc_error("mmap failed.");
				return CAMERASRC_ERR_ALLOCATION;
			}

			GST_INFO("buffer index %d, addr %x, length %d",
			         p->buffer_idx, p->buffer[p->buffer_idx].start, p->buffer[p->buffer_idx].length);

			switch (p->format.pix_format) {
			case CAMERASRC_PIX_YUV422P:
			case CAMERASRC_PIX_YUY2:
			case CAMERASRC_PIX_UYVY:
				if (p->buffer[p->buffer_idx].length < p->format.img_size.dim.width * p->format.img_size.dim.height * 2) {
					camsrc_error("format[%d] device insufficient memory.", p->format.pix_format);
				}
				break;
			case CAMERASRC_PIX_YUV420P:
			case CAMERASRC_PIX_NV12:
				if (p->buffer[p->buffer_idx].length < (p->format.img_size.dim.width * p->format.img_size.dim.height * 3 / 2)) {
					camsrc_error("format[%d] device insufficient memory.", p->format.pix_format);
				}
				break;
			case CAMERASRC_PIX_SN12:
			case CAMERASRC_PIX_ST12:
				if (p->buffer[p->buffer_idx].length < (p->format.img_size.dim.width * p->format.img_size.dim.height)) {
					camsrc_error("SN12,ST12:  device insufficient memory.");
				}
				break;
			case CAMERASRC_PIX_INTERLEAVED:
				if (p->buffer[p->buffer_idx].length < (p->format.img_size.dim.width * p->format.img_size.dim.height)) {
					camsrc_error("INTERLEAVED:  device insufficient memory.");
				}
				break;
			default:
				camsrc_error("Not supported format[%d]", p->format.pix_format);
				break;
			}

			camsrc_info("MMAP BUF: addr[%p] size[%d]",
			                       p->buffer[p->buffer_idx].start, p->buffer[p->buffer_idx].length);
		}
		break;
	case CAMERASRC_IO_METHOD_USRPTR:
		camsrc_info("USRPTR mode");

		vreq_bufs.count = p->present_buf->num_buffer;
		vreq_bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vreq_bufs.memory = V4L2_MEMORY_USERPTR;
		__ta__( "            VIDIOC_REQBUFS",
		if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_REQBUFS, &vreq_bufs)) {
			strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
			camsrc_error("VIDIOC_REQBUFS failed : %s", err_msg);
			return CAMERASRC_ERR_IO_CONTROL;
		}
		);
		camsrc_assert(vreq_bufs.count >= 1);

		p->num_buffers = vreq_bufs.count;
		p->buffer = calloc (vreq_bufs.count, sizeof (camerasrc_buffer_t));

		if(!p->buffer) {
			camsrc_error("calloc() failed");
			return CAMERASRC_ERR_ALLOCATION;
		}

		if(!p->present_buf->present_buffer) {
			camsrc_error("No Usrptr buffer presented!");
			return CAMERASRC_ERR_ALLOCATION;
		}

		for (p->buffer_idx = 0; p->buffer_idx < vreq_bufs.count; ++(p->buffer_idx)) {
			p->buffer[p->buffer_idx].length = p->present_buf->present_buffer[p->buffer_idx].length;
			p->buffer[p->buffer_idx].start = p->present_buf->present_buffer[p->buffer_idx].start;

			if (!p->buffer[p->buffer_idx].start) {
				camsrc_error("presented usrptr buffer is NULL");
				return CAMERASRC_ERR_ALLOCATION;
			}

			camsrc_info("USERPTR BUF: addr[%p] size[%d]",
			                       p->buffer[p->buffer_idx].start, p->buffer[p->buffer_idx].length);
		}
		break;
	case CAMERASRC_IO_METHOD_READ:
	default:
		camsrc_error("Device not support that io-method");
#if USE_NOT_SUPPORT_ERR
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
		return CAMERASRC_SUCCESS;
#endif
		break;
	}

	/* A C T U A L   S T A R T   S T R E A M */
	switch(p->io_method) {
	case CAMERASRC_IO_METHOD_MMAP:
		camsrc_info("MMAP mode");

		LOCK(p);
		p->queued_buffer_count = 0;
		UNLOCK(p);

		for (cnt = 0; cnt < p->num_buffers; cnt++) {
			struct v4l2_buffer lvbuf;
			CLEAR(lvbuf);

			lvbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			lvbuf.memory = V4L2_MEMORY_MMAP;
			lvbuf.index = cnt;
			__ta__( "            VIDIOC_QBUF",
			if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_QBUF, &lvbuf)) {
				strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
				camsrc_error("VIDIOC_QBUF failed : %s", err_msg);
				return CAMERASRC_ERR_IO_CONTROL;
			}
			);

			LOCK(p);
			p->buffer[lvbuf.index].queued_status = CAMERASRC_BUFFER_QUEUED;
			p->queued_buffer_count++;
			UNLOCK(p);
		}
		break;
	case CAMERASRC_IO_METHOD_USRPTR:
		camsrc_info("USRPTR mode");

		for (cnt = 0; cnt < p->present_buf->num_buffer; cnt++) {
			struct v4l2_buffer lvbuf;
			CLEAR(lvbuf);

			lvbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			lvbuf.memory = V4L2_MEMORY_USERPTR;
			lvbuf.index = cnt;
			lvbuf.m.userptr = (unsigned long)p->buffer[cnt].start;
			lvbuf.length = p->buffer[cnt].length;
			__ta__( "            VIDIOC_QBUF",
			if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_QBUF, &lvbuf)) {
				strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
				camsrc_error("VIDIOC_QBUF failed : %s", err_msg);
				return CAMERASRC_ERR_IO_CONTROL;
			}
			);

			camsrc_info("QBUF Address [%d] %p", lvbuf.index, (void*)lvbuf.m.userptr);

			memcpy(&(p->queued_buf_list[cnt]), &lvbuf, sizeof(struct v4l2_buffer));
		}
		break;
	case CAMERASRC_IO_METHOD_READ:
	default:
		camsrc_error("Device not support that io-method");
#if USE_NOT_SUPPORT_ERR
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
		return CAMERASRC_SUCCESS;
#endif
		break;
	}

	/* calibrate rotation */
	if (p->lens_rotation != -1) {
		if (p->cur_dev_id == CAMERASRC_DEV_ID_PRIMARY) {
			/* rear camera */
			switch (p->lens_rotation) {
			case IS_ROTATION_180:
			case IS_ROTATION_270:
				p->format.rotation += 180;
				p->format.rotation = p->format.rotation % 360;
				camsrc_info("Calibrate rotate : +180 degree");
				break;
			default:
				camsrc_info("No need to calibrate");
				break;
			}
		} else {
			/* front camera */
			switch (p->lens_rotation) {
			case IS_ROTATION_90:
			case IS_ROTATION_180:
				p->format.rotation += 180;
				p->format.rotation = p->format.rotation % 360;
				camsrc_info("Calibrate rotate : +180 degree");
				break;
			default:
				camsrc_info("No need to calibrate");
				break;
			}
		}
	} else {
		camsrc_warning("Invalid lens rotation. skip rotation calibration.");
	}

#ifdef SUPPORT_CAMERA_SENSOR_MODE
	/* Sensor mode Set */
	__ta__( "            _CAMERASRC_CMD_SENSOR_MODE",
	CAMERASRC_SET_CMD(_CAMERASRC_CMD_SENSOR_MODE, &(p->sensor_mode));
	);
#endif
	/* Sensor flip */
	__ta__( "            _CAMERASRC_CMD_VFLIP",
	CAMERASRC_SET_CMD(_CAMERASRC_CMD_VFLIP, &(p->vflip));
	);
	__ta__( "            _CAMERASRC_CMD_HFLIP",
	CAMERASRC_SET_CMD(_CAMERASRC_CMD_HFLIP, &(p->hflip));
	);

	vtype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	__ta__( "            VIDIOC_STREAMON",
	if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_STREAMON, &vtype)) {
		strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_error("VIDIOC_STREAMON failed : %s", err_msg);
		return CAMERASRC_ERR_IO_CONTROL;
	}
	);

	/* For noti that it is first frame DQ */
	p->first_frame = 1;

	LOCK(p);
	if (_CAMERASRC_NEED_MISC_FUNCTION(p->cur_dev_id, CAMERASRC_OP_PREVIEW, p->format.colorspace, CAMERASRC_MISC_SKIP_FRAME)) {
		p->skip_frame_func = (camerasrc_skip_frame_func_t)_camerasrc_skip_frame;
	} else {
		p->skip_frame_func = NULL;
	}
	UNLOCK(p);

#if defined(USE_SKIP_FRAME_AT_RAW_FRAME)
	if(p->skip_frame_func != NULL)
		p->skip_frame_func(p, 5000, 3);
#endif

	for (cnt = 0; cnt < p->num_buffers; cnt++) {
		camsrc_info("MMAP/USRPTR BUF: addr[%p] size[%d]", p->buffer[cnt].start, p->buffer[cnt].length);
	}

	CAMERASRC_SET_STATE(p, CAMERASRC_STATE_PREVIEW);

	return CAMERASRC_SUCCESS;
}


int camerasrc_query_img_buf_size(camsrc_handle_t handle, unsigned int* main_img_size, unsigned int* thm_img_size)
{
	camerasrc_handle_t* p = NULL;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_STATE(p) != CAMERASRC_STATE_STILL &&
	    CAMERASRC_STATE(p) != CAMERASRC_STATE_PREVIEW &&
	    CAMERASRC_STATE(p) != CAMERASRC_STATE_VIDEO) {
		camsrc_warning("Invalid state transition");
	}

	camsrc_info("Start to set format");

	if (p->format.colorspace == CAMERASRC_COL_RAW &&
	    (p->format.pix_format == CAMERASRC_PIX_YUV422P ||
	     p->format.pix_format == CAMERASRC_PIX_YUY2 ||
	     p->format.pix_format == CAMERASRC_PIX_UYVY ||
	     p->format.pix_format == CAMERASRC_PIX_INTERLEAVED)) {
		*main_img_size = p->format.img_size.dim.width * p->format.img_size.dim.height * 2;
		if (thm_img_size != NULL) {
			*thm_img_size = 0;
		}
		camsrc_info("422 RAW!! WIDTH = %d, HEIGHT=%d, SIZE = %d",
		                         p->format.img_size.dim.width, p->format.img_size.dim.height, *main_img_size);
	} else if (p->format.colorspace == CAMERASRC_COL_RAW &&
	           (p->format.pix_format == CAMERASRC_PIX_YUV420P ||
	            p->format.pix_format == CAMERASRC_PIX_SN12 ||
	            p->format.pix_format == CAMERASRC_PIX_ST12 ||
	            p->format.pix_format == CAMERASRC_PIX_NV12)) {
		/*Kessler preview path*/
		*main_img_size = p->format.img_size.dim.width * p->format.img_size.dim.height * 3 / 2;
		if (thm_img_size != NULL) {
			*thm_img_size = 0;
		}
		camsrc_info("420 RAW!! WIDTH = %d, HEIGHT=%d, SIZE = %d",
		                         p->format.img_size.dim.width, p->format.img_size.dim.height, *main_img_size);
	} else if (p->format.colorspace == CAMERASRC_COL_JPEG) {
		//camsrc_error("Unknown format set. can't go any more");
		//camsrc_assert(0);
		*main_img_size = p->format.img_size.dim.width * p->format.img_size.dim.height * 3;
		if (thm_img_size != NULL) {
			*thm_img_size = 0;
		}		
	}

	camsrc_info("leave..");

	return CAMERASRC_SUCCESS;
}


int camerasrc_stop_stream(camsrc_handle_t handle)
{
	camerasrc_handle_t *p = NULL;
	enum v4l2_buf_type type;
	struct v4l2_requestbuffers vreq_bufs;
	int cnt = 0;
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};

	camsrc_info("enter");

	if(handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_STATE(p) == CAMERASRC_STATE_READY) {
		return CAMERASRC_SUCCESS;
	}

	if (CAMERASRC_STATE(p) != CAMERASRC_STATE_STILL &&
	    CAMERASRC_STATE(p) != CAMERASRC_STATE_PREVIEW &&
	    CAMERASRC_STATE(p) != CAMERASRC_STATE_VIDEO) {
		camsrc_warning("Stop stream called [STREAM-NOT-STARTED STATE]");
	}

	camsrc_info("Change to READY state first for preventing to check Q/DQ after stop");
	CAMERASRC_SET_STATE(p, CAMERASRC_STATE_READY);

	LOCK(p);
	p->timeperframe.denominator = 0;
	p->timeperframe.numerator = 0;
	p->first_frame = 1;
	UNLOCK(p);

	/* S T O P   S T R E A M */
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	__ta__( "                camerasrc_stop_stream:VIDIOC_STREAMOFF",
	if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_STREAMOFF, &type)) {
		strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_error("VIDIOC_STREAMOFF failed : %s", err_msg);
		return CAMERASRC_ERR_IO_CONTROL;
	}
	);

	/* M E M O R Y   U N M A P P I N G   &   F R E E */
	switch(p->io_method) {
	case CAMERASRC_IO_METHOD_MMAP:
		camsrc_info("Init io method to MMAP mode");
		for (cnt = 0; cnt < p->num_buffers; ++cnt) {
			if (-1 == munmap (p->buffer[cnt].start,p->buffer[cnt].length)) {
				camsrc_error("MUNMAP failed.");
				return CAMERASRC_ERR_INTERNAL;
			}

			p->buffer[cnt].start = NULL;
			p->buffer[cnt].length = 0;
		}
		break;
	case CAMERASRC_IO_METHOD_USRPTR:
		camsrc_info("User ptr mode don't need munmap");
		break;
	case CAMERASRC_IO_METHOD_READ:
	default:
		camsrc_error("Device not support that io-method");
#if USE_NOT_SUPPORT_ERR
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
#else
		return CAMERASRC_SUCCESS;
#endif
	}

	/* release buffer */
#ifdef USE_SUPPORT_RELEASE_BUFFER
	CLEAR(vreq_bufs);
	vreq_bufs.count = 0;
	vreq_bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vreq_bufs.memory = V4L2_MEMORY_MMAP;
	if (CAMERASRC_SUCCESS != _camerasrc_ioctl(p, VIDIOC_REQBUFS, &vreq_bufs)) {
		strerror_r(p->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_error("VIDIOC_REQBUFS failed : %s", err_msg);
		return CAMERASRC_ERR_IO_CONTROL;
	}
#endif	

	/* RETURN BUFFER MANAGE MODE TO DEFAULT */
	p->io_method = CAMERASRC_IO_METHOD_MMAP;
	p->present_buf = NULL;

	if (p->buffer != NULL) {
		free(p->buffer);
	}

	p->buffer = NULL;
	p->buffer_idx = 0;
	p->num_buffers = 0;

	return CAMERASRC_SUCCESS;
}


int camerasrc_wait_frame_available(camsrc_handle_t handle, long int timeout)
{
	camerasrc_handle_t *p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_STATE(p) != CAMERASRC_STATE_STILL &&
	    CAMERASRC_STATE(p) != CAMERASRC_STATE_PREVIEW &&
	    CAMERASRC_STATE(p) != CAMERASRC_STATE_VIDEO) {
		camsrc_warning("Invalid state transition" );
	}

	if ( p->queued_buffer_count > 0 ) {
		/*camsrc_info( CAMERASRC, "Current queue count : %d", p->queued_buffer_count );*/
		err = _camerasrc_wait_frame_available(p, timeout);
	} else {
		camsrc_warning("queued_buffer_count[%d] is unavailable. sleep 100 ms and try again...", p->queued_buffer_count );
		usleep( 100000 );
		err = CAMERASRC_ERR_INVALID_STATE;
	}

	return err;
}

int camerasrc_queue_buffer(camsrc_handle_t handle, int buf_index, camerasrc_buffer_t *buffer)
{
	camerasrc_handle_t *p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_STATE(p) != CAMERASRC_STATE_STILL &&
	    CAMERASRC_STATE(p) != CAMERASRC_STATE_PREVIEW &&
	    CAMERASRC_STATE(p) != CAMERASRC_STATE_VIDEO) {
		camsrc_warning("Invalid state transition");
	}

	err = _camerasrc_queue_buffer(p, buf_index, buffer);

	return err;
}


int camerasrc_dequeue_buffer(camsrc_handle_t handle, int *buf_index, camerasrc_buffer_t *buffer)
{
	camerasrc_handle_t *p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_STATE(p) != CAMERASRC_STATE_STILL &&
	    CAMERASRC_STATE(p) != CAMERASRC_STATE_PREVIEW &&
	    CAMERASRC_STATE(p) != CAMERASRC_STATE_VIDEO) {
		camsrc_warning("Invalid state transition");
	}

	err = _camerasrc_dequeue_buffer(p, buf_index, buffer);

	return err;
}


int camerasrc_read_frame(camsrc_handle_t handle, camerasrc_buffer_t *main_img_buffer, int *buffer_index)
{
	camerasrc_handle_t* p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	camsrc_info("ENTER");

	if (!handle || !main_img_buffer || !buffer_index) {
		camsrc_error("handle(%p)main(%p)index(%p) is null",
		                        handle, main_img_buffer, buffer_index);
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_STATE(p) != CAMERASRC_STATE_STILL &&
	    CAMERASRC_STATE(p) != CAMERASRC_STATE_PREVIEW &&
	    CAMERASRC_STATE(p) != CAMERASRC_STATE_VIDEO) {
		camsrc_warning("Invalid state transition");
	}

#ifndef DUMMY_OUTPUT
	__ta__( "                Stillshot select()",
	err = _camerasrc_wait_frame_available(p, CAMERASRC_TIMEOUT_CRITICAL_VALUE);
	);
	if (err != CAMERASRC_SUCCESS) {
		camsrc_error("Frame waiting error, [%x]", err);
		return err;
	}

	/* Buffer DQ */
	__ta__( "                Stillshot VIDIOC_DQBUF",
	err = _camerasrc_dequeue_buffer(p, buffer_index, main_img_buffer);
	);
	if (err != CAMERASRC_SUCCESS) {
		camsrc_error("Dequeue frame error, [%x]", err);
		return err;
	}
#endif

	camsrc_info("DEQUEUED Index : %d", *buffer_index);

#if defined(USE_CAMERASRC_FRAME_DUMP)
	printf("******************in buf len = %d", main_img_buffer->length);
	write_buffer_into_path(main_img_buffer, CAMERASRC_FRAME_DUMP_PATH, "main.yuv");
#endif

	return CAMERASRC_SUCCESS;
}

/**** M I S C E L L A N E O U S    O P E R A T I O N ****/

/**** I N P U T ( C A M D E V I C E )    O P E R A T I O N ****/

int camerasrc_get_input(camsrc_handle_t handle, camerasrc_dev_id_t *id)
{
	camerasrc_handle_t *p = NULL;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_PHASE(p) != CAMERASRC_PHASE_RUNNING) {
		camsrc_warning("Invalid phase");
	}

	*id = p->cur_dev_id;

	return CAMERASRC_SUCCESS;
}


int camerasrc_set_input(camsrc_handle_t handle, camerasrc_dev_id_t camera_id)
{
	camerasrc_handle_t *p = NULL;
	struct v4l2_control control;
	int err = CAMERASRC_ERR_UNKNOWN;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_STATE(p) != CAMERASRC_STATE_READY) {
		camsrc_error("Invalid state");
		return CAMERASRC_ERR_INVALID_STATE;
	}

	if (p->cur_dev_id == camera_id) {
		return CAMERASRC_SUCCESS;
	}

	camsrc_info("Set Index to %d", _CAMERASRC_GET_DEV_INDEX(camera_id));

	err = _camerasrc_ioctl(p, VIDIOC_S_INPUT, &(_CAMERASRC_GET_DEV_INDEX(camera_id)));
	if (CAMERASRC_SUCCESS != err) {
		camsrc_error("[***DEBUG] ERROR SET INPUT to %dth DEVICE", _CAMERASRC_GET_DEV_INDEX(camera_id));
		return CAMERASRC_ERR_IO_CONTROL;
	} else {
		camsrc_info("[***DEBUG] SET INPUT OK!!! to %dth DEVICE", _CAMERASRC_GET_DEV_INDEX(camera_id));
		camsrc_info("return value of ioctl VIDIOC_S_INPUT = %d", err);
	}

	p->cur_dev_id = camera_id;

#ifdef SUPPORT_PHYSICAL_ROTATION
	/* check physical rotation of camera lens */
	#ifndef V4L2_CID_PHYSICAL_ROTATION
	#define V4L2_CID_PHYSICAL_ROTATION  (V4L2_CID_PRIVATE_BASE + 300)
	#endif /* V4L2_CID_PHYSICAL_ROTATION */

	control.id = V4L2_CID_PHYSICAL_ROTATION;
	err = _camerasrc_ioctl(handle, VIDIOC_G_CTRL, &control);
	if (CAMERASRC_SUCCESS != err) {
		camsrc_warning("failed to get rotation of lens");
		p->lens_rotation = -1;
	} else {
		p->lens_rotation = control.value;
		camsrc_info("lens rotation %d", p->lens_rotation);
	}
#endif
	return err;
}

/**** O U T P U T    C O N T R O L    O P E R A T I O N ****/

int camerasrc_set_timeperframe(camsrc_handle_t handle, camerasrc_frac_t *frac)
{
	camerasrc_handle_t *p = NULL;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_PHASE(p) != CAMERASRC_PHASE_RUNNING) {
		camsrc_warning("Invalid phase, but can go");
	}

	camsrc_info("Numerator = %d, Denominator = %d", frac->numerator, frac->denominator);

	LOCK(p);
	p->timeperframe.numerator = frac->numerator;
	p->timeperframe.denominator = frac->denominator;
	UNLOCK(p);

	return CAMERASRC_SUCCESS;
}


int camerasrc_set_format(camsrc_handle_t handle, camerasrc_format_t *fmt)
{
	camerasrc_handle_t *p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_STATE(p) != CAMERASRC_STATE_READY) {
		camsrc_error("Invalid state");
	}

	err = _camerasrc_dump_format(handle);
	if (err != CAMERASRC_SUCCESS) {
		camsrc_error("Format dump error");
		return err;
	}

	CLEAR(p->format);

	p->format.colorspace = fmt->colorspace;
	p->format.bytesperline = fmt->bytesperline;
	p->format.img_size.dim.width = fmt->img_size.dim.width;
	p->format.img_size.dim.height = fmt->img_size.dim.height;
	p->format.is_highquality_mode = fmt->is_highquality_mode;
	p->format.pix_format = fmt->pix_format;
	p->format.quality = fmt->quality;
	p->format.sizeimage = fmt->sizeimage;
	p->format.rotation = fmt->rotation;
	p->is_preset = 0;

	switch (fmt->pix_format) {
	case CAMERASRC_PIX_YUV422P:
		p->format.num_planes = 3;
		break;
	case CAMERASRC_PIX_YUV420P:
		p->format.num_planes = 3;
		break;
	case CAMERASRC_PIX_SN12:
		p->format.num_planes = 2;
		break;
	case CAMERASRC_PIX_ST12:
		p->format.num_planes = 2;
		break;
	case CAMERASRC_PIX_NV12:
		p->format.num_planes = 2;
		break;
	case CAMERASRC_PIX_YUY2:
		p->format.num_planes = 1;
		break;
	case CAMERASRC_PIX_UYVY:
		p->format.num_planes = 1;
		break;
	case CAMERASRC_PIX_RGGB8:
		p->format.num_planes = 1;
		break;
	case CAMERASRC_PIX_RGGB10:
		p->format.num_planes = 1;
		break;
	case CAMERASRC_PIX_RGB565:
		p->format.num_planes = 1;
		break;
	case CAMERASRC_PIX_MJPEG:
		p->format.num_planes = 1;
		break;

	default:
		p->format.num_planes = 3;
		camsrc_error("Invalid output format.");
		break;
	}

	err = _camerasrc_dump_format(handle);
	if (err != CAMERASRC_SUCCESS) {
		camsrc_error("Format dump error");
		return err;
	}

	camsrc_info("leave");

	return err;
}


int camerasrc_get_format(camsrc_handle_t handle, camerasrc_format_t *fmt)
{
	camerasrc_handle_t *p = NULL;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_PHASE(p) != CAMERASRC_PHASE_RUNNING) {
		camsrc_warning("Invalid phase");
	}

	memcpy(fmt, &(p->format), sizeof(camerasrc_format_t));

	camsrc_info("leave");

	return CAMERASRC_SUCCESS;
}


int camerasrc_try_format(camsrc_handle_t handle, camerasrc_format_t *fmt)
{
	camerasrc_handle_t *p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_STATE(p) != CAMERASRC_STATE_READY) {
		camsrc_error("Invalid state");
		return CAMERASRC_ERR_INVALID_STATE;
	}

	/* size limitaion check */
	if (fmt->img_size.dim.width < 0 || fmt->img_size.dim.width > CAMERASRC_MAX_WIDTH) {
		camsrc_info("Invalid width");
		return CAMERASRC_ERR_INVALID_PARAMETER;
	}
	if (fmt->img_size.dim.height < 0 || fmt->img_size.dim.height > CAMERASRC_MAX_HEIGHT) {
		camsrc_info("Invalid height");
		return CAMERASRC_ERR_INVALID_PARAMETER;
	}

	/* check pixel number & colorspace are in bound */
	if (fmt->pix_format < 0 || fmt->pix_format >= CAMERASRC_PIX_NUM) {
		camsrc_info("Invalid pixel format");
		return CAMERASRC_ERR_INVALID_PARAMETER;
	}
	if (fmt->colorspace < 0 || fmt->colorspace >= CAMERASRC_COL_NUM) {
		camsrc_info("Invalid colorspace");
		return CAMERASRC_ERR_INVALID_PARAMETER;
	}

	/* colorspace & pixel format combinability check */
#ifdef SUPPORT_MULTI_PIXEL_FORMAT	
	if (_CAMERASRC_MATCH_COL_TO_PIX(p->cur_dev_id, fmt->colorspace, fmt->pix_format, CAMERASRC_QUALITY_HIGH) ||
	    _CAMERASRC_MATCH_COL_TO_PIX(p->cur_dev_id, fmt->colorspace, fmt->pix_format, CAMERASRC_QUALITY_NORMAL)) {
		err = CAMERASRC_SUCCESS;
	} else {
		camsrc_info("UNAVAILABLE SETTING");
		err = CAMERASRC_ERR_INVALID_PARAMETER;
	}
#else
	err = CAMERASRC_SUCCESS;
#endif

	camsrc_info("leave");

	return err;
}

static int _camerasrc_dump_format(camsrc_handle_t handle)
{
	camerasrc_handle_t *p = NULL;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	camsrc_info("---------FORMAT SETTING DUMP--------");
	camsrc_info("- Image size : %d x %d", p->format.img_size.dim.width, p->format.img_size.dim.height);
	camsrc_info("- Pixel format : %d", p->format.pix_format);
	camsrc_info("- Bytes per line : %d", p->format.bytesperline);
	camsrc_info("- Image size in bytes : %d", p->format.sizeimage);
	camsrc_info("- Colorspace : %d", p->format.colorspace);
	camsrc_info("- Rotation : %d", p->format.rotation);
	camsrc_info("------------------------------------");

	return CAMERASRC_SUCCESS;
}


int camerasrc_get_exif_info(camsrc_handle_t handle, camerasrc_exif_t *exif_struct)
{
	camerasrc_handle_t* p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	if (CAMERASRC_PHASE(p) != CAMERASRC_PHASE_RUNNING) {
		camsrc_warning("Invalid state transition");
	}
/*
	err = _camerasrc_get_exif_info(p, (camerasrc_buffer_t*)exif_struct);
	if (err != CAMERASRC_SUCCESS) {
		camsrc_error("Get exif information string failed");
	}
*/
	return err;
}


int camerasrc_device_is_open(camsrc_handle_t handle)
{
	camerasrc_handle_t *p = handle;

	if (p) {
		if (p->dev_fd > 0) {
			return TRUE;
		}
	}

	return FALSE;
}


int camerasrc_set_videofd(camsrc_handle_t handle, int videofd)
{
	camerasrc_handle_t *p = handle;

	if (p) {
		p->dev_fd = videofd;
		return CAMERASRC_SUCCESS;
	} else {
		return CAMERASRC_ERR_INVALID_HANDLE;
	}
}

#ifdef SUPPORT_CAMERA_SENSOR_MODE
int camerasrc_set_sensor_mode(camsrc_handle_t handle, int mode)
{
	camerasrc_handle_t *p = NULL;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	p->sensor_mode = mode;

	camsrc_info("leave");

	return CAMERASRC_SUCCESS;
}
#endif

int camerasrc_set_vflip(camsrc_handle_t handle, int vflip)
{
	camerasrc_handle_t *p = NULL;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	p->vflip = vflip;

	if (CAMERASRC_STATE(p) > CAMERASRC_STATE_READY) {
		CAMERASRC_SET_CMD(_CAMERASRC_CMD_VFLIP, &(p->vflip));
	}

	camsrc_info("leave");

	return CAMERASRC_SUCCESS;
}


int camerasrc_set_hflip(camsrc_handle_t handle, int hflip)
{
	camerasrc_handle_t *p = NULL;

	camsrc_info("enter");

	if (handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	p->hflip = hflip;
	if (CAMERASRC_STATE(p) > CAMERASRC_STATE_READY) {
		CAMERASRC_SET_CMD(_CAMERASRC_CMD_HFLIP, &(p->hflip));
	}

	camsrc_info("leave");

	return CAMERASRC_SUCCESS;
}


/* For Query functionalities */
int camerasrc_read_basic_dev_info(camerasrc_dev_id_t dev_id, camerasrc_caps_info_t* caps_info)
{
	int err = CAMERASRC_ERR_UNKNOWN;
	int nread = 0;
	char* store_path = NULL;
	FILE *fp = NULL;

	camsrc_info("enter");

	if(dev_id == CAMERASRC_DEV_ID_PRIMARY)
		store_path = CAMERASRC_PRIMARY_BASIC_INFO_PATH;
	else if (dev_id == CAMERASRC_DEV_ID_SECONDARY)
		store_path = CAMERASRC_SECONDARY_BASIC_INFO_PATH;
	else
	{
		camsrc_error("Unsupported device ID");
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
	}

	fp = fopen(store_path, "rb");
	if(fp)
	{
		fseek(fp, 0, SEEK_SET);
		nread = fread(caps_info, 1, sizeof(camerasrc_caps_info_t), fp);
		camsrc_info("Need to be read : %d / Actual read : %d", sizeof(camerasrc_caps_info_t), nread);
		fclose(fp);
	}
	else
		return CAMERASRC_ERR_ALLOCATION;

	err = CAMERASRC_SUCCESS;
	camsrc_info("leave");
	return err;
}


int camerasrc_read_misc_dev_info(camerasrc_dev_id_t dev_id, camerasrc_ctrl_list_info_t* ctrl_info)
{
	int err = CAMERASRC_ERR_UNKNOWN;
	camsrc_info("enter");

	int nread = 0;
	FILE *fp = NULL;
	char* store_path = NULL;

	if(dev_id == CAMERASRC_DEV_ID_PRIMARY)
		store_path = CAMERASRC_PRIMARY_MISC_INFO_PATH;
	else if (dev_id == CAMERASRC_DEV_ID_SECONDARY)
		store_path = CAMERASRC_SECONDARY_MISC_INFO_PATH;
	else
	{
		camsrc_error("Unsupported device ID");
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
	}

	fp = fopen(store_path, "rb");
	if(fp)
	{
		fseek(fp, 0, SEEK_SET);
		nread = fread(ctrl_info, 1, sizeof(camerasrc_ctrl_list_info_t), fp);
		camsrc_info("Need to be read : %d / Actual read : %d", sizeof(camerasrc_ctrl_list_info_t), nread);
		fclose(fp);
	}
	else
		return CAMERASRC_ERR_ALLOCATION;

	err = CAMERASRC_SUCCESS;
	camsrc_info("leave");
	return err;
}

int camerasrc_write_basic_dev_info(camsrc_handle_t handle, camerasrc_caps_info_t* caps_info)
{
	camerasrc_handle_t* p = NULL;
	char* store_path = NULL;
	FILE *fp = NULL;

	camsrc_info("enter");

	if(handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	int nwrite = 0;

	if(p->cur_dev_id == CAMERASRC_DEV_ID_PRIMARY)
	{
		camsrc_info("Primary(Mega) camera capabilities info will be written..");
		store_path = CAMERASRC_PRIMARY_BASIC_INFO_PATH;
	}
	else if (p->cur_dev_id == CAMERASRC_DEV_ID_SECONDARY)
	{
		camsrc_info("Secondary(VGA) camera capabilities info will be written..");
		store_path = CAMERASRC_SECONDARY_BASIC_INFO_PATH;
	}
	else
	{
		camsrc_error("Unsupported device ID");
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
	}
	camsrc_info("PATH = %s", store_path);

	fp = fopen(store_path, "wb");
	if(fp)
	{
		fseek(fp, 0, SEEK_SET);
		nwrite = fwrite(caps_info, 1, sizeof(camerasrc_caps_info_t), fp);
		camsrc_info("Need to be written : %d / Actual written : %d", sizeof(camerasrc_caps_info_t), nwrite);
		fclose(fp);
	}
	else
		return CAMERASRC_ERR_ALLOCATION;

	camsrc_info("leave");
	return CAMERASRC_SUCCESS;
}


int camerasrc_write_misc_dev_info(camsrc_handle_t handle, camerasrc_ctrl_list_info_t* ctrl_info)
{
	camerasrc_handle_t* p = NULL;

	camsrc_info("enter");

	if(handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	int nwrite = 0;
	FILE *fp = NULL;

	char* store_path = NULL;

	if(p->cur_dev_id == CAMERASRC_DEV_ID_PRIMARY)
	{
		camsrc_info("Primary(Mega) camera controls info will be written..");
		store_path = CAMERASRC_PRIMARY_MISC_INFO_PATH;
	}
	else if (p->cur_dev_id == CAMERASRC_DEV_ID_SECONDARY)
	{
		camsrc_info("Secondary(VGA) camera controls info will be written..");
		store_path = CAMERASRC_SECONDARY_MISC_INFO_PATH;
	}
	else
	{
		camsrc_error("Unsupported device ID");
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
	}

	fp = fopen(store_path, "wb");
	if(fp)
	{
		fseek(fp, 0, SEEK_SET);
		nwrite = fwrite(ctrl_info, 1, sizeof(camerasrc_ctrl_list_info_t), fp);
		camsrc_info("Need to be written : %d / Actual written : %d", sizeof(camerasrc_ctrl_list_info_t), nwrite);
		fclose(fp);
	}
	else
		return CAMERASRC_ERR_ALLOCATION;

	camsrc_info("leave");
	return CAMERASRC_SUCCESS;
}


void _camerasrc_add_total_resolution_info( camerasrc_caps_info_t* caps_info, int fcc_use, int width, int height )
{
	int i = 0;

	if( 1 || fcc_use == CAMERASRC_FCC_USE_REC_PREVIEW
			|| fcc_use == CAMERASRC_FCC_USE_CAP_PREVIEW )
	{
		for( i = 0 ; i < caps_info->num_preview_resolution ; i++ )
		{
			if( caps_info->preview_resolution_width[i] == width
					&& caps_info->preview_resolution_height[i] == height )
			{
				camsrc_info("preview resolution[%dx%d] is already existed.", width, height);
				return;
			}
		}

		caps_info->preview_resolution_width[i] = width;
		caps_info->preview_resolution_height[i] = height;
		caps_info->num_preview_resolution++;

		camsrc_info("preview resolution[%dx%d] is added.", width, height);
	}
	else if( fcc_use == CAMERASRC_FCC_USE_NORMAL_CAPTURE
			|| fcc_use == CAMERASRC_FCC_USE_CONT_CAPTURE )
	{
		for( i = 0 ; i < caps_info->num_capture_resolution ; i++ )
		{
			if( caps_info->capture_resolution_width[i] == width
					&& caps_info->capture_resolution_height[i] == height )
			{
				camsrc_info("capture resolution[%dx%d] is already existed.", width, height);
				return;
			}
		}

		caps_info->capture_resolution_width[i] = width;
		caps_info->capture_resolution_height[i] = height;
		caps_info->num_capture_resolution++;

		camsrc_info("capture resolution[%dx%d] is added.", width, height);
	}

	return;
}

void _camerasrc_add_total_fps( camerasrc_caps_info_t* caps_info, int numerator, int denominator )
{
	int i = 0;

	for( i = 0 ; i < caps_info->num_fps ; i++ )
	{
		if( caps_info->fps[i].numerator == numerator
				&& caps_info->fps[i].denominator == denominator )
		{
			camsrc_info("fps[%d/%d] is already existed.", numerator, denominator);
			return;
		}
	}

	caps_info->fps[caps_info->num_fps].numerator   = numerator;
	caps_info->fps[caps_info->num_fps].denominator = denominator;
	caps_info->num_fps++;

	camsrc_info("fps[%d/%d] is added.", numerator, denominator);

	return;
}

int _camerasrc_query_pixfmt_timeperframe(camerasrc_handle_t* handle, camerasrc_caps_info_t* caps_info, int fmt_index, int resolution_index )
{
	int tpf_cnt = 0;
	struct v4l2_frmivalenum vfrmivalenum;
	unsigned int fcc = 0;
	camerasrc_resolution_t* resolutions = NULL;

	if( !handle || !caps_info )
	{
		camsrc_info("handle[%p] or caps_info[%p] is NULL.", handle, caps_info);
		return CAMERASRC_ERR_NULL_POINTER;
	}

	if( 0 > fmt_index || 0 > resolution_index )
	{
		camsrc_info("some index is too small. fmt_index[%d], resolution_index[%d]",
		                       fmt_index, resolution_index);
		return CAMERASRC_ERR_RANGE_UNDER;
	}

	fcc = caps_info->fmt_desc[fmt_index].fcc;
	resolutions = &(caps_info->fmt_desc[fmt_index].resolutions[resolution_index]);

	/*
		*How many resolutions are supported in this format
		*/
	CLEAR(vfrmivalenum);
	vfrmivalenum.index = tpf_cnt;
	vfrmivalenum.pixel_format = fcc;
	vfrmivalenum.width = resolutions->w;
	vfrmivalenum.height = resolutions->h;

	while(0 == _camerasrc_ioctl(handle,  VIDIOC_ENUM_FRAMEINTERVALS, &vfrmivalenum))
	{
		resolutions->tpf[tpf_cnt].num = vfrmivalenum.discrete.numerator;
		resolutions->tpf[tpf_cnt].den = vfrmivalenum.discrete.denominator;
		tpf_cnt++;

		_camerasrc_add_total_fps( caps_info, vfrmivalenum.discrete.denominator, vfrmivalenum.discrete.numerator );
	}

	if(tpf_cnt == 0)
	{
		camsrc_info("No timeperframe supported. driver may not support to query");
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
	}

	resolutions->num_avail_tpf = tpf_cnt;

	return CAMERASRC_SUCCESS;
}

int _camerasrc_query_pixfmt_resolution(camerasrc_handle_t* handle, camerasrc_caps_info_t* caps_info, int fmt_index )
{
	int res_cnt = 0;
	int err = CAMERASRC_ERR_UNKNOWN;
	struct v4l2_frmsizeenum vfrmszenum;
	camerasrc_fmt_desc_t* fmt_desc = NULL;

	if( !handle || !caps_info )
	{
		camsrc_info("handle[%p] or caps_info[%p] is NULL.", handle, caps_info);
		return CAMERASRC_ERR_NULL_POINTER;
	}

	if( 0 > fmt_index )
	{
		camsrc_info("fmt_index[%d] is too small.", fmt_index);
		return CAMERASRC_ERR_RANGE_UNDER;
	}

	fmt_desc = &caps_info->fmt_desc[fmt_index];

	/*
		*How many resolutions are supported in this format
		*/
	CLEAR(vfrmszenum);
	vfrmszenum.index = res_cnt;
	vfrmszenum.pixel_format = fmt_desc->fcc;
	while(0 == _camerasrc_ioctl(handle,  VIDIOC_ENUM_FRAMESIZES, &vfrmszenum))
	{
		fmt_desc->resolutions[res_cnt].w = vfrmszenum.discrete.width;
		fmt_desc->resolutions[res_cnt].h = vfrmszenum.discrete.height;

		_camerasrc_add_total_resolution_info( caps_info, caps_info->fmt_desc[fmt_index].fcc_use,
				vfrmszenum.discrete.width, vfrmszenum.discrete.height );

		err = _camerasrc_query_pixfmt_timeperframe(handle, caps_info, fmt_index, res_cnt);
		if(err != CAMERASRC_SUCCESS)
		{
			camsrc_info("timeperframe querying error, err = %d", err);
			return err;
		}

		res_cnt++;
		vfrmszenum.index = res_cnt;
	}

	if(res_cnt == 0)
	{
		camsrc_info("No resolution supported in fcc[0x%8x]. driver may not support to query", fmt_desc->fcc);
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
	}

	fmt_desc->num_resolution = res_cnt;

	return CAMERASRC_SUCCESS;
}

int _camerasrc_query_basic_dev_info(camerasrc_handle_t* handle, camerasrc_caps_info_t* caps_info) {
	struct v4l2_fmtdesc vfmtdesc;
	struct v4l2_input vinput;
	int err = CAMERASRC_ERR_UNKNOWN;
	int fmt_cnt = 0;
	char err_msg[CAMERASRC_ERRMSG_MAX_LEN] = {'\0',};

	caps_info->num_preview_resolution = 0;
	caps_info->num_capture_resolution = 0;
	caps_info->num_preview_fmt = 0;
	caps_info->num_capture_fmt = 0;
	caps_info->num_fps = 0;

	/*
	 * Count how many formats are supported in this dev
	 */
	CLEAR(vfmtdesc);
	CLEAR(vinput);

	/*
	 * What name of module is selected?
	 */
	if(CAMERASRC_SUCCESS != _camerasrc_ioctl(handle, VIDIOC_ENUMINPUT, &vinput)) {
		strerror_r(handle->errnum, err_msg, CAMERASRC_ERRMSG_MAX_LEN);
		camsrc_error("VIDIOC_ENUMINPUT failed : %s", err_msg);
		return CAMERASRC_ERR_IO_CONTROL;
	}
	memcpy(caps_info->dev_name, vinput.name, 32);

	vfmtdesc.index = 0;
	vfmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while(0 == _camerasrc_ioctl(handle,  VIDIOC_ENUM_FMT, &vfmtdesc))
	{
		camsrc_info("pixelformat \"%c%c%c%c\"(%8x) detected",
				(char)(vfmtdesc.pixelformat >> 0),
				(char)(vfmtdesc.pixelformat >> 8),
				(char)(vfmtdesc.pixelformat >> 16),
				(char)(vfmtdesc.pixelformat >> 24),
				vfmtdesc.pixelformat);
		caps_info->fmt_desc[fmt_cnt].fcc = vfmtdesc.pixelformat;
		/*
			* Along the description of the format, we seperate them into each usecase
			*/
		camsrc_info("description \"%s\"", vfmtdesc.description );
		if(!strcmp("rec_prev", (const char*)(vfmtdesc.description)))
		{
			caps_info->fmt_desc[fmt_cnt].fcc_use = CAMERASRC_FCC_USE_REC_PREVIEW;
			caps_info->preview_fmt[caps_info->num_preview_fmt] = vfmtdesc.pixelformat;
			caps_info->num_preview_fmt++;
		}
		else if(!strcmp("cap_prev", (const char*)(vfmtdesc.description)))
		{
			caps_info->fmt_desc[fmt_cnt].fcc_use = CAMERASRC_FCC_USE_CAP_PREVIEW;
			caps_info->preview_fmt[caps_info->num_preview_fmt] = vfmtdesc.pixelformat;
			caps_info->num_preview_fmt++;
		}
		else if(!strcmp("recording", (const char*)(vfmtdesc.description)))
		{
			caps_info->fmt_desc[fmt_cnt].fcc_use = CAMERASRC_FCC_USE_RECORDING;
		}
		else if(!strcmp("capture", (const char*)(vfmtdesc.description)))
		{
			caps_info->fmt_desc[fmt_cnt].fcc_use = CAMERASRC_FCC_USE_NORMAL_CAPTURE;
			caps_info->capture_fmt[caps_info->num_capture_fmt] = vfmtdesc.pixelformat;
			caps_info->num_capture_fmt++;
		}
		else if(!strcmp("cont_cap", (const char*)(vfmtdesc.description)))
		{
			caps_info->fmt_desc[fmt_cnt].fcc_use = CAMERASRC_FCC_USE_CONT_CAPTURE;
			caps_info->capture_fmt[caps_info->num_capture_fmt] = vfmtdesc.pixelformat;
			caps_info->num_capture_fmt++;
		}
		else
		{
			caps_info->fmt_desc[fmt_cnt].fcc_use = CAMERASRC_FCC_USE_NONE;
		}
		err = _camerasrc_query_pixfmt_resolution(handle, caps_info, fmt_cnt);
		if(err != CAMERASRC_SUCCESS)
		{
			camsrc_info("pixel format querying error, err = %x", err);
		}
		fmt_cnt++;
		vfmtdesc.index = fmt_cnt;
		vfmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	}


	/*
		* memory allocation as counted num
		*/
	camsrc_info("Total supported format = %d", fmt_cnt);
	caps_info->num_fmt_desc = fmt_cnt;

	/*
		* If no formats supported
		*/
	if(fmt_cnt == 0)
	{
		camsrc_info("no format supported");
		return CAMERASRC_ERR_DEVICE_NOT_SUPPORT;
	}
	return CAMERASRC_SUCCESS;
}


int camerasrc_query_basic_dev_info(camsrc_handle_t handle, camerasrc_caps_info_t* caps_info) {
	camerasrc_handle_t* p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	camsrc_info("enter");

	if(handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	err = _camerasrc_query_basic_dev_info(p, caps_info);

	err = CAMERASRC_SUCCESS;
	camsrc_info("leave");

	return err;
}

int _camerasrc_query_ctrl_menu (camerasrc_handle_t* handle, struct v4l2_queryctrl queryctrl, camerasrc_ctrl_info_t* ctrl_info)
{
	struct v4l2_querymenu querymenu;

	camsrc_info("  Menu items:");

	memset (&querymenu, 0, sizeof (querymenu));
	querymenu.id = queryctrl.id;

	for (querymenu.index = queryctrl.minimum;
	     querymenu.index <= queryctrl.maximum;
	     querymenu.index++) {
		if (0 == _camerasrc_ioctl (handle, VIDIOC_QUERYMENU, &querymenu)) {
			camsrc_info("  menu name : %s", querymenu.name);
			ctrl_info->ctrl_menu[querymenu.index].menu_index = querymenu.index;
			memcpy(&(ctrl_info->ctrl_menu[querymenu.index].menu_name), querymenu.name, 32);
		} else {
			camsrc_error("VIDIOC_QUERYMENU error");
			return CAMERASRC_ERR_IO_CONTROL;
		}
	}
	return CAMERASRC_SUCCESS;
}

static int _find_camsrc_ctrl_id(camerasrc_handle_t* handle, int v4l2_cid)
{
	int i = 0;
	int tmp_id = 0;
	for(i=0; i<CAMERASRC_CTRL_NUM; i++)
	{
		tmp_id = _CAMERASRC_GET_CID(i, handle->cur_dev_id);
		if(tmp_id == v4l2_cid)
			return i;
	}

	return -1;
}

int _camerasrc_query_misc_dev_info(camerasrc_handle_t* handle, camerasrc_ctrl_list_info_t* ctrl_list_info)
{
	struct v4l2_queryctrl queryctrl;

	int i = 0;
	int ctrl_cnt = 0;


	camsrc_info("QUERY_BASE ");
	for (i = CAMERASRC_CTRL_BRIGHTNESS; i < CAMERASRC_CTRL_NUM; i++) {
		queryctrl.id = _CAMERASRC_GET_CID(i, handle->cur_dev_id);
		if(queryctrl.id == -1)
		{
			continue;
		}
		if (0 == _camerasrc_ioctl (handle, VIDIOC_QUERYCTRL, &queryctrl)) {
			if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
			{
				camsrc_info("ID [%8x] disabled", queryctrl.id);
				/*continue;*/
			}
			camsrc_info("Control %s", queryctrl.name);
			ctrl_list_info->ctrl_info[ctrl_cnt].v4l2_ctrl_id = queryctrl.id;
			ctrl_list_info->ctrl_info[ctrl_cnt].camsrc_ctrl_id = i;
			if(ctrl_list_info->ctrl_info[ctrl_cnt].camsrc_ctrl_id == -1)
			{
				camsrc_warning("This control is not included in camsrc ctrl ID error");
			}
			switch(queryctrl.type)
			{
				case V4L2_CTRL_TYPE_INTEGER:
					ctrl_list_info->ctrl_info[ctrl_cnt].ctrl_type = CTRL_TYPE_RANGE;
					break;
				case V4L2_CTRL_TYPE_BOOLEAN:
					ctrl_list_info->ctrl_info[ctrl_cnt].ctrl_type = CTRL_TYPE_BOOL;
					break;
				case V4L2_CTRL_TYPE_MENU:
					ctrl_list_info->ctrl_info[ctrl_cnt].ctrl_type = CTRL_TYPE_ARRAY;
					_camerasrc_query_ctrl_menu (handle, queryctrl, &(ctrl_list_info->ctrl_info[ctrl_cnt]));
					break;
				default:
					ctrl_list_info->ctrl_info[ctrl_cnt].ctrl_type = CTRL_TYPE_UNKNOWN;
					break;
			}
			memcpy(ctrl_list_info->ctrl_info[ctrl_cnt].ctrl_name,queryctrl.name, 32);
			ctrl_list_info->ctrl_info[ctrl_cnt].min = queryctrl.minimum;
			ctrl_list_info->ctrl_info[ctrl_cnt].max = queryctrl.maximum;
			ctrl_list_info->ctrl_info[ctrl_cnt].step = queryctrl.step;
			ctrl_list_info->ctrl_info[ctrl_cnt].default_val = queryctrl.default_value;
			camsrc_info("Control %s", queryctrl.name);
		} else {
			if (errno == EINVAL)
				continue;
			camsrc_error("VIDIOC_QUERYCTRL error");
			return CAMERASRC_ERR_IO_CONTROL;
		}
		ctrl_cnt++;
	}
	ctrl_list_info->num_ctrl_list_info = ctrl_cnt;
	return CAMERASRC_SUCCESS;
}


int camerasrc_query_misc_dev_info(camsrc_handle_t handle, camerasrc_ctrl_list_info_t* ctrl_list_info)
{
	camerasrc_handle_t* p = NULL;
	int err = CAMERASRC_ERR_UNKNOWN;

	camsrc_info("enter");

	if(handle == NULL) {
		camsrc_error("handle is null");
		return CAMERASRC_ERR_NULL_POINTER;
	}

	p = CAMERASRC_HANDLE(handle);

	err = _camerasrc_query_misc_dev_info(p, ctrl_list_info);

	err = CAMERASRC_SUCCESS;

	camsrc_info("leave");

	return err;
}


int camerasrc_query_misc_dev_ctrl_info(camsrc_handle_t handle, camerasrc_ctrl_t ctrl_id, camerasrc_ctrl_info_t* ctrl_info)
{
	struct v4l2_queryctrl queryctrl;

	CLEAR(queryctrl);

	queryctrl.id = _CAMERASRC_GET_CID(ctrl_id, CAMERASRC_HANDLE(handle)->cur_dev_id);
	if (0 == _camerasrc_ioctl (handle, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
		{
			camsrc_info("ID [%8x] disabled", queryctrl.id);
		}
		camsrc_info("Control %s", queryctrl.name);
		ctrl_info->v4l2_ctrl_id = queryctrl.id;
		ctrl_info->camsrc_ctrl_id = ctrl_id;
		if(ctrl_info->camsrc_ctrl_id == -1)
		{
			camsrc_warning("This control is not included in camsrc ctrl ID error");
		}
		switch(queryctrl.type)
		{
			case V4L2_CTRL_TYPE_INTEGER:
				ctrl_info->ctrl_type = CTRL_TYPE_RANGE;
				break;
			case V4L2_CTRL_TYPE_BOOLEAN:
				ctrl_info->ctrl_type = CTRL_TYPE_BOOL;
				break;
			case V4L2_CTRL_TYPE_MENU:
				ctrl_info->ctrl_type = CTRL_TYPE_ARRAY;
				_camerasrc_query_ctrl_menu (handle, queryctrl, ctrl_info);
				break;
			default:
				ctrl_info->ctrl_type = CTRL_TYPE_UNKNOWN;
				break;
		}
		memcpy(ctrl_info->ctrl_name,queryctrl.name, MAX_SZ_CTRL_NAME_STRING);
		ctrl_info->min = queryctrl.minimum;
		ctrl_info->max = queryctrl.maximum;
		ctrl_info->step = queryctrl.step;
		ctrl_info->default_val = queryctrl.default_value;
		camsrc_info("Control %s", queryctrl.name);
	} else {
		camsrc_error("VIDIOC_QUERYCTRL error");
		return CAMERASRC_ERR_IO_CONTROL;
	}
	return CAMERASRC_SUCCESS;
}


int camerasrc_dump_misc_dev_info(camsrc_handle_t handle, camerasrc_ctrl_list_info_t* ctrl_list_info)
{
	camsrc_info("enter");
	int i=0;
	int j=0;

	camsrc_info("============================================================");
	for(i = 0; i < ctrl_list_info->num_ctrl_list_info; i++)
	{
		camsrc_info("[camsrc ID : %d] [v4l2 ID : 0x%08x] Control name = %s", ctrl_list_info->ctrl_info[i].camsrc_ctrl_id, ctrl_list_info->ctrl_info[i].v4l2_ctrl_id, ctrl_list_info->ctrl_info[i].ctrl_name);
		camsrc_info("                  Control type = %d", ctrl_list_info->ctrl_info[i].ctrl_type);
		camsrc_info("                  Min = %d / Max = %d / Step = %d / default = %d", ctrl_list_info->ctrl_info[i].min, ctrl_list_info->ctrl_info[i].max, ctrl_list_info->ctrl_info[i].step, ctrl_list_info->ctrl_info[i].default_val);
		if(ctrl_list_info->ctrl_info[i].ctrl_type == CTRL_TYPE_ARRAY)
		{
			camsrc_info("\t\t  - Menu list");
			for(j = ctrl_list_info->ctrl_info[i].min; j <= ctrl_list_info->ctrl_info[i].max; j++)
			{
				camsrc_info("\t\t    * %s", ctrl_list_info->ctrl_info[i].ctrl_menu[j].menu_name);
			}
		}
	}
	camsrc_info("============================================================");
	camsrc_info("leave");
	return CAMERASRC_SUCCESS;
}


int camerasrc_dump_basic_dev_info(camsrc_handle_t handle, camerasrc_caps_info_t* caps_info) {
	camsrc_info("enter");
	int i,j,k;

	camsrc_info("===============================================================");
	for(i = 0; i< caps_info->num_fmt_desc; i++)
	{
		camsrc_info("Supported fourcc = \"%c%c%c%c\" (0x%08x) num of resolution = %d",
		                       (char)(caps_info->fmt_desc[i].fcc >> 0),
		                       (char)(caps_info->fmt_desc[i].fcc >> 8),
		                       (char)(caps_info->fmt_desc[i].fcc >> 16),
		                       (char)(caps_info->fmt_desc[i].fcc >> 24),
		                       caps_info->fmt_desc[i].fcc,
		                       caps_info->fmt_desc[i].num_resolution);
		for(j=0; j<caps_info->fmt_desc[i].num_resolution; j++)
		{
			camsrc_info("\tresolution = %d x %d, num of tpf = %d",
			                       caps_info->fmt_desc[i].resolutions[j].w, caps_info->fmt_desc[i].resolutions[j].h,
			                       caps_info->fmt_desc[i].resolutions[j].num_avail_tpf);
			for(k=0; k<caps_info->fmt_desc[i].resolutions[j].num_avail_tpf; k++)
			{
				camsrc_info("\t\ttimeperframe = %d / %d", caps_info->fmt_desc[i].resolutions[j].tpf[k].num, caps_info->fmt_desc[i].resolutions[j].tpf[k].den);
			}
		}
	}
	camsrc_info("===============================================================");
	camsrc_info("total supported preview resolution");
	for( i = 0 ; i < caps_info->num_preview_resolution ; i++ )
	{
		camsrc_info("%5dx%5d", caps_info->preview_resolution_width[i], caps_info->preview_resolution_height[i]);

	}
	camsrc_info("===============================================================");
	camsrc_info("total supported capture resolution");
	for( i = 0 ; i < caps_info->num_capture_resolution ; i++ )
	{
		camsrc_info("%5dx%5d", caps_info->capture_resolution_width[i], caps_info->capture_resolution_height[i]);

	}
	camsrc_info("===============================================================");
	camsrc_info("total supported preview fourcc");
	for( i = 0 ; i < caps_info->num_preview_fmt ; i++ )
	{
		camsrc_info("\"%c%c%c%c\" (0x%08x)",
		                       (char)(caps_info->preview_fmt[i]),
		                       (char)(caps_info->preview_fmt[i]>>8),
		                       (char)(caps_info->preview_fmt[i]>>16),
		                       (char)(caps_info->preview_fmt[i]>>24),
		                       caps_info->preview_fmt[i]);
	}
	camsrc_info("===============================================================");
	camsrc_info("total supported capture fourcc");
	for( i = 0 ; i < caps_info->num_capture_fmt ; i++ )
	{
		camsrc_info("\"%c%c%c%c\" (0x%08x)",
		                       (char)(caps_info->capture_fmt[i]),
		                       (char)(caps_info->capture_fmt[i]>>8),
		                       (char)(caps_info->capture_fmt[i]>>16),
		                       (char)(caps_info->capture_fmt[i]>>24),
		                       caps_info->capture_fmt[i]);
	}
	camsrc_info("===============================================================");
	camsrc_info("total supported fps");
	for( i = 0 ; i < caps_info->num_fps ; i++ )
	{
		camsrc_info("%3d/%3d", caps_info->fps[i].numerator, caps_info->fps[i].denominator);
	}
	camsrc_info("===============================================================");
	camsrc_info("leave");
	return CAMERASRC_SUCCESS;
}


/* END For Query functionalities */
