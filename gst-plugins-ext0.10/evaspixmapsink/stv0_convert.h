/*
 * stv0_convert.h
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
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
#ifndef STV0_CONVERT_H
#define STV0_CONVERT_H

#include <X11/Xlib.h>

#include "linux/videodev2.h"

typedef struct _STV0ConversionCtx *STV0ConversionCtx;

STV0ConversionCtx stv0_create_context(int drmfd);
void stv0_destroy_context(STV0ConversionCtx ctx);

Bool stv0_scale_convert(STV0ConversionCtx ctx, struct v4l2_drm *in, Pixmap pix);

#endif // STV0_CONVERT_H
