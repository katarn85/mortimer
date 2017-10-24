/*
* secrecord
*
* Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd. All rights reserved.
*
* Contact: Hyunseok Lee <hs7388.lee@samsung.com>
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*
* Alternatively, the contents of this file may be used under the
* GNU Lesser General Public License Version 2.1 (the "LGPL"), in
* which case the following provisions apply instead of the ones
* mentioned above:
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




#ifndef __GST_SECRECORD_H__
#define __GST_SECRECORD_H__

#include <stdio.h>
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <samsungrec.h>
#include <DNSe_NRSS.h>

G_BEGIN_DECLS

#define GST_TYPE_SECRECORD \
	(gst_secrecord_get_type())
#define GST_SECRECORD(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SECRECORD,Gstsecrecord))
#define GST_SECRECORD_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SECRECORD,GstsecrecordClass))
#define GST_IS_SECRECORD(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SECRECORD))
#define GST_IS_SECRECORD_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SECRECORD))

typedef struct _Gstsecrecord      Gstsecrecord;
typedef struct _GstsecrecordClass GstsecrecordClass;

#define TEMPORALLY

enum {
	REC_FILTER_01,
	REC_FILTER_02,
	REC_FILTER_NUM
};

#define REC_FREQ_NUM 7
#define REC_COEF_NUM 6

struct _Gstsecrecord {
	GstBaseTransform element;

	guint samplerate;
	gshort rec_mode;
	gboolean rec_onoff;
	gboolean nrss_onoff;
	gboolean sb_onoff;
	gboolean bf_interview_onoff;
	gboolean bf_conversation_onoff;

	gboolean need_dump_input;
	FILE *dump_input_fp;
	gboolean need_dump_output;
	FILE *dump_output_fp;

	/* parameters for rec interface */
	gshort amr_pregain;
	gshort aac_l_pregain;
	gshort aac_r_pregain;
	gboolean rec_filter_onoff[REC_FILTER_NUM];
	gshort rec_filter_coef[REC_FILTER_NUM][REC_FREQ_NUM][REC_COEF_NUM];

#ifdef TEMPORALLY
	guint debug_frame_num;
#endif
};

struct _GstsecrecordClass {
	GstBaseTransformClass parent_class;
};

GType gst_secrecord_get_type (void);

G_END_DECLS

#endif /* __GST_SECRECORD_H__ */
