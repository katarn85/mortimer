/*
 * drm-util.c
 *
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
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
#include <string.h>
#include <drm_trusted_client.h>
#include "drm_util.h"

gboolean drm_util_open (DRM_DECRYPT_HANDLE *phandle, char* file_path, int file_type)
{
	drm_trusted_result_e drm_trusted_result;
	drm_trusted_open_decrypt_info_s open_decrypt_info;
	drm_trusted_open_decrypt_resp_data_s open_decrypt_resp;
	drm_trusted_set_consumption_state_info_s decrypt_state_data = { DRM_CONSUMPTION_STARTED };

	if (phandle == NULL || file_path == NULL) {
		GST_ERROR ("Invalid parameter, phandle=%p, file_path=%p", phandle, file_path);
		return FALSE;
	}

	/* Fill parameter structure for opening decrypt session */
	memset (&open_decrypt_info, 0, sizeof (drm_trusted_open_decrypt_info_s));
	memset (&open_decrypt_resp, 0, sizeof (drm_trusted_open_decrypt_resp_data_s));

	strncpy (open_decrypt_info.filePath, file_path, sizeof (open_decrypt_info.filePath)-1);
	open_decrypt_info.file_type = file_type;
	open_decrypt_info.permission = DRM_TRUSTED_PERMISSION_TYPE_PLAY;

	/* Open decrypt session */
	drm_trusted_result = drm_trusted_open_decrypt_session(&open_decrypt_info, &open_decrypt_resp, phandle);
	if (drm_trusted_result != DRM_TRUSTED_RETURN_SUCCESS || *phandle == NULL) {
		GST_ERROR ("Error in drm_trusted_open_decrypt_session(), error=%x, *phandle=%p,", drm_trusted_result, *phandle);
		return FALSE;
	}
	GST_DEBUG ("drm_trusted_open_decrypt_session () success!!, ret=%x, *phandle=%p", drm_trusted_result, *phandle);

	/* Set decryption state to STARTED */
	drm_trusted_result = drm_trusted_set_decrypt_state(*phandle, &decrypt_state_data);
	if (drm_trusted_result != DRM_TRUSTED_RETURN_SUCCESS) {
		GST_ERROR ("Error in drm_trusted_set_decrypt_state(), error=%x", drm_trusted_result);
		drm_trusted_close_decrypt_session (phandle);
		*phandle = NULL;
		return FALSE;
	}
	GST_DEBUG ("drm_trusted_set_decrypt_state () success!!, ret=%x, *phandle=%p", drm_trusted_result, *phandle);

	return TRUE;
}

gboolean drm_util_read (DRM_DECRYPT_HANDLE handle, unsigned char* buf, unsigned int buf_length, unsigned int *read_size)
{
	drm_trusted_result_e drm_trusted_result;
	drm_trusted_payload_info_s payload_info;
	drm_trusted_read_decrypt_resp_data_s decrypt_resp;

	if (handle == NULL || buf == NULL) {
		GST_ERROR ("Invalid parameter, handle=%p, buf=%p", handle, buf);
		return FALSE;
	}

	GST_DEBUG ("Enter [%s] buf=%p, length=%d, read_size=%p", __func__, buf, buf_length, read_size);

	/* fill input/output data */
	memset (&payload_info, 0, sizeof (drm_trusted_payload_info_s));
	memset (&decrypt_resp, 0, sizeof (drm_trusted_read_decrypt_resp_data_s));

	payload_info.payload_data = buf;
	payload_info.payload_data_len = buf_length;
	payload_info.payload_data_output = buf;

	drm_trusted_result = drm_trusted_read_decrypt_session (handle, &payload_info, &decrypt_resp);
	if(drm_trusted_result != DRM_TRUSTED_RETURN_SUCCESS) {
		GST_ERROR ("Error in drm_trusted_read_decrypt_session() [%x]", drm_trusted_result);
		return FALSE;
	}

	GST_DEBUG ("Leave [%s], drm_trusted_read_decrypt_session() success, requested=%d, read_size=%d", __func__, buf_length, decrypt_resp.read_size);
	if (read_size)
		*read_size = decrypt_resp.read_size;

	return TRUE;
}

gboolean drm_util_seek (DRM_DECRYPT_HANDLE handle, int offset, int mode)
{
	drm_trusted_result_e drm_trusted_result;
	drm_trusted_seek_decrypt_info_s seek_decrypt_info;
	memset (&seek_decrypt_info, 0, sizeof(drm_trusted_seek_decrypt_info_s));

	GST_DEBUG ("Enter [%s] offset=%d, mode=%d", __func__, offset, mode);

	seek_decrypt_info.offset = offset;
	seek_decrypt_info.seek_mode = mode;

	drm_trusted_result = drm_trusted_seek_decrypt_session(handle, &seek_decrypt_info);
	if (drm_trusted_result != DRM_TRUSTED_RETURN_SUCCESS) {
		GST_ERROR ("Error in drm_trusted_seek_decrypt_session(), error=%x", drm_trusted_result);
		return FALSE;
	}

	GST_DEBUG ("Leave [%s], drm_trusted_seek_decrypt_session () success!!", __func__);
	return TRUE;
}

gboolean drm_util_tell (DRM_DECRYPT_HANDLE handle, unsigned int *offset)
{
	drm_trusted_result_e drm_trusted_result;
	drm_trusted_tell_decrypt_resp_data_s tell_decrypt_resp_data;

	if (handle == NULL || offset == NULL) {
		GST_ERROR ("Invalid parameter, handle=%p, offset=%p", handle, offset);
		return FALSE;
	}

	GST_DEBUG ("Enter [%s] offset=%p", __func__, offset);

	memset (&tell_decrypt_resp_data, 0, sizeof(drm_trusted_tell_decrypt_resp_data_s));

	drm_trusted_result = drm_trusted_tell_decrypt_session(handle, &tell_decrypt_resp_data);
	if (drm_trusted_result != DRM_TRUSTED_RETURN_SUCCESS) {
		GST_ERROR ("Error in drm_trusted_tell_decrypt_session(), error=%x", drm_trusted_result);
		return FALSE;
	}

	if (offset)
		*offset = tell_decrypt_resp_data.offset;

	GST_DEBUG ("Leave [%s], drm_trusted_tell_decrypt_session () success!!", __func__);

	return TRUE;
}

gboolean drm_util_close (DRM_DECRYPT_HANDLE *phandle)
{
	drm_trusted_result_e drm_trusted_result;
	drm_trusted_set_consumption_state_info_s decrypt_state_data = { DRM_CONSUMPTION_STOPPED };

	if (phandle == NULL) {
		GST_ERROR ("Invalid parameter, phandle=%p", phandle);
		return FALSE;
	}

	/* Set decryption state to STOPPED */
	drm_trusted_result = drm_trusted_set_decrypt_state(*phandle, &decrypt_state_data);
	if (drm_trusted_result != DRM_TRUSTED_RETURN_SUCCESS) {
		GST_ERROR ("Error in drm_trusted_set_decrypt_state(), error=%x", drm_trusted_result);
	} else {
		GST_DEBUG ("drm_trusted_set_decrypt_state () success!!");
	}

	/* Close decrypt session */
	drm_trusted_result = drm_trusted_close_decrypt_session(phandle);
	if(drm_trusted_result != DRM_TRUSTED_RETURN_SUCCESS) {
		GST_ERROR ("Error in drm_trusted_close_decrypt_session() error=%x", drm_trusted_result);
		return FALSE;
	}
	GST_DEBUG ("drm_trusted_close_decrypt_session() success!!!");

	return TRUE;
}
