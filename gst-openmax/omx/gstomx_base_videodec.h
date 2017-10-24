/*
 * Copyright (C) 2007-2009 Nokia Corporation.
 *
 * Author: Felipe Contreras <felipe.contreras@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef GSTOMX_BASE_VIDEODEC_H
#define GSTOMX_BASE_VIDEODEC_H

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_OMX_BASE_VIDEODEC(obj) (GstOmxBaseVideoDec *) (obj)
#define GST_OMX_BASE_VIDEODEC_TYPE (gst_omx_base_videodec_get_type ())
#define GST_OMX_BASE_VIDEODEC_CLASS(c) (G_TYPE_CHECK_CLASS_CAST ((c), GST_OMX_BASE_VIDEODEC_TYPE, GstOmxBaseVideoDecClass))
typedef struct GstOmxBaseVideoDec GstOmxBaseVideoDec;
typedef struct GstOmxBaseVideoDecClass GstOmxBaseVideoDecClass;

#include "gstomx_base_filter.h"

enum
{
  ARG_0,
  ARG_USE_STATETUNING, /* STATE_TUNING */
  ARG_DECODING_TYPE, /* Decoding type */
  ARG_ERROR_CONCEALMENT,  /* Error concealment */
  ARG_USERDATA_MODE,
};

enum
{
  CLIP_MODE = 0x01,
  SEAMLESS_MODE = 0x02,   // CLIP_MODE , SEAMLESS_MODE ARE EXCLUSIVE NOW
  MULTI_DECODING_MODE = 0x04,
};

struct GstOmxBaseVideoDec
{
  GstOmxBaseFilter omx_base;

  gint decoding_type;

  OMX_VIDEO_CODINGTYPE compression_format;
  gint framerate_num;
  gint framerate_denom;
  GstPad *dtv_cc_srcpad;
  void *udManager;
  gboolean source_changed;
  gushort rating;
  gboolean got_rating;
  gint need_set_fpa_3dtype;
  gint type_3d;
  gint pre_scantype;
  gint pre_bitrate;
  gboolean error_concealment;

  gboolean seimetadata_filled;
  OMX_VIDEO_USERDATA_ETC seimetadata;
  gboolean sei_mdcv_data_filled;
  OMX_VIDEO_MASTERING_DISPLAY_COLOUR_VOLUME sei_mdcv_data;
  gboolean color_description_filled;
  OMX_VIDEO_COLOUR_DESCRIPTION color_description;
  GstPadEventFunction pad_event;
  gint userdata_extraction_mode;
};

struct GstOmxBaseVideoDecClass
{
  GstOmxBaseFilterClass parent_class;
};

GType gst_omx_base_videodec_get_type (void);

G_END_DECLS
#endif /* GSTOMX_BASE_VIDEODEC_H */
