 /*
 * soundalive
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



#ifndef __GST_SOUNDALIVE_H__
#define __GST_SOUNDALIVE_H__

#include <stdio.h>
#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <iniparser.h>
#include <SoundAlive_layer.h>
#include <SamsungSB.h>
#include <samsung_src.h>
#include <mm_sound.h>

G_BEGIN_DECLS

#define GST_TYPE_SOUNDALIVE \
	(gst_soundalive_get_type())
#define GST_SOUNDALIVE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SOUNDALIVE,Gstsoundalive))
#define GST_soundalive_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SOUNDALIVE,GstsoundaliveClass))
#define GST_IS_SOUNDALIVE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SOUNDALIVE))
#define GST_IS_SOUNDALIVE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SOUNDALIVE))

typedef struct _Gstsoundalive      Gstsoundalive;
typedef struct _GstsoundaliveClass GstsoundaliveClass;

//#define USE_PA_AUDIO_FILTER

#define CUSTOM_EQ_BAND_MAX				9
enum {
	CUSTOM_EXT_3D_LEVEL,
	CUSTOM_EXT_BASS_LEVEL,
	CUSTOM_EXT_CONCERT_HALL_VOLUME,
	CUSTOM_EXT_CONCERT_HALL_LEVEL,
	CUSTOM_EXT_CLARITY_LEVEL,
	CUSTOM_EXT_PARAM_MAX
};
enum {
	AM_BAND_01,
	AM_BAND_02,
	AM_BAND_03,
	AM_BAND_NUM
};
#define AM_FREQ_NUM 7
#define AM_COEF_NUM 6

struct _Gstsoundalive
{
	GstBaseTransform element;

	guint		samplerate;
	guint		channels;

#ifndef GST_EXT_SOUNDALIVE_DISABLE_SA
	SA_Handle	*sa_handle;
#endif

	/* properties */
	guint		filter_action;
	guint		filter_output_mode;
	guint		preset_mode;
	gint		custom_eq[CUSTOM_EQ_BAND_MAX];
	gint		custom_ext[CUSTOM_EXT_PARAM_MAX];
	guint		custom_eq_band_num;
	gint		custom_eq_band_freq[CUSTOM_EQ_BAND_MAX];
	gint		custom_eq_band_width[CUSTOM_EQ_BAND_MAX];

	gboolean	sb_onoff;
	gshort		lowcut_freq;
	gshort		gain_level;
	gshort		clarity_level;
	gboolean	am_onoff;
	gboolean	am_band_onoff[AM_BAND_NUM];
	gint		am_band_coef[AM_BAND_NUM][AM_FREQ_NUM][AM_COEF_NUM];
	SamsungSBMemStruct *sb_struct;

	gboolean	need_update_filter;
	gboolean	need_dump_input;
	FILE		*dump_input_fp;
	gboolean	need_dump_output;
	FILE		*dump_output_fp;

	guint		current_volume;
	SamsungSRCState *src_state;
#ifdef USE_PA_AUDIO_FILTER
	GObject		*sink;
	gboolean	use_pa_audio_filter;
#endif
};

struct _GstsoundaliveClass
{
	GstBaseTransformClass parent_class;
};

GType gst_soundalive_get_type (void);

G_END_DECLS

#endif /* __GST_SOUNDALIVE_H__ */
