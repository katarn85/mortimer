/*
 * drm_util.h
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

#ifndef __DRM_UTIL_H__
#define __DRM_UTIL_H__

#include <gst/gst.h>

#include <drm_trusted_client_types.h>

gboolean drm_util_open (DRM_DECRYPT_HANDLE *phandle, char* file_path, int file_type);
gboolean drm_util_read (DRM_DECRYPT_HANDLE handle, unsigned char* buf, unsigned int buf_length, unsigned int *read_size);
gboolean drm_util_seek (DRM_DECRYPT_HANDLE handle, int offset, int mode);
gboolean drm_util_tell (DRM_DECRYPT_HANDLE handle, unsigned int *offset);
gboolean drm_util_close (DRM_DECRYPT_HANDLE *phandle);

#endif /* __DRM_UTIL_H__ */
