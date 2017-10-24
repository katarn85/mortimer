/**************************************************************************

Copyright 2010 - 2013 Samsung Electronics co., Ltd. All Rights Reserved.

Contact: Boram Park <boram1288.park@samsung.com>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

#ifndef __XV_COLORCONVERTION_H__
#define __XV_COLORCONVERTION_H__

#include <glib.h>
#include <gst/gst.h>
void xv_colorconversion_init(void);
void xv_colorconversion_deinit(void);
void convert_yuv420_interleaved_to_argb(unsigned char *y, unsigned char *CbCr,
	int yLineSize, int uvLineSize, int width, int height, unsigned char *rgbaOut);
void convert_yuv420_interleaved_to_argb_factor4(unsigned char *y, unsigned char *CbCr,
	int yLineSize, int uvLineSize, int width, int height, unsigned char *rgbaOut);
void convert_yuv420_to_argb(guchar *y, guchar *u, guchar *v,
	guint yLineSize, guint uvLineSize, guint width, guint height, guchar *rgbaOut);

#endif //__XV_COLORCONVERTION_H__

