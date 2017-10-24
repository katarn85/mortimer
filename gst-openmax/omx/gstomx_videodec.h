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

#ifndef GSTOMX_VIDEODEC_H
#define GSTOMX_VIDEODEC_H

#include <gst/gst.h>

G_BEGIN_DECLS

/********************  FHD VideoDec 0 **********************/
#define GST_OMX_VIDEODEC(obj) (GstOmxVideoDec *) (obj)
#define GST_OMX_VIDEODEC_TYPE (gst_omx_videodec_get_type ())

typedef struct GstOmxVideoDec GstOmxVideoDec;
typedef struct GstOmxVideoDecClass GstOmxVideoDecClass;

#include "gstomx_base_videodec.h"
#include "gstomx_videodec.h"

struct GstOmxVideoDec
{
  GstOmxBaseVideoDec omx_base;
};

struct GstOmxVideoDecClass
{
  GstOmxBaseVideoDecClass parent_class;
};

GType gst_omx_videodec_get_type (void);


/********************  FHD VideoDec 1 **********************/
#define GST_OMX_VIDEODEC1(obj) (GstOmxVideoDec1 *) (obj)
#define GST_OMX_VIDEODEC1_TYPE (gst_omx_videodec1_get_type ())

typedef struct GstOmxVideoDec1 GstOmxVideoDec1;
typedef struct GstOmxVideoDec1Class GstOmxVideoDec1Class;

struct GstOmxVideoDec1
{
  GstOmxBaseVideoDec omx_base;
};

struct GstOmxVideoDec1Class
{
  GstOmxBaseVideoDecClass parent_class;
};

GType gst_omx_videodec1_get_type (void);


/********************  FHD VideoDec 2 **********************/
#define GST_OMX_VIDEODEC2(obj) (GstOmxVideoDec2 *) (obj)
#define GST_OMX_VIDEODEC2_TYPE (gst_omx_videodec2_get_type ())

typedef struct GstOmxVideoDec2 GstOmxVideoDec2;
typedef struct GstOmxVideoDec2Class GstOmxVideoDec2Class;

struct GstOmxVideoDec2
{
  GstOmxBaseVideoDec omx_base;
};

struct GstOmxVideoDec2Class
{
  GstOmxBaseVideoDecClass parent_class;
};

GType gst_omx_videodec2_get_type (void);

/********************  FHD VideoDec 3 **********************/
#define GST_OMX_VIDEODEC3(obj) (GstOmxVideoDec3 *) (obj)
#define GST_OMX_VIDEODEC3_TYPE (gst_omx_videodec3_get_type ())

typedef struct GstOmxVideoDec3 GstOmxVideoDec3;
typedef struct GstOmxVideoDec3Class GstOmxVideoDec3Class;

struct GstOmxVideoDec3
{
  GstOmxBaseVideoDec omx_base;
};

struct GstOmxVideoDec3Class
{
  GstOmxBaseVideoDecClass parent_class;
};

GType gst_omx_videodec3_get_type (void);

/********************  FHD VideoDec 4 **********************/
#define GST_OMX_VIDEODEC4(obj) (GstOmxVideoDec4 *) (obj)
#define GST_OMX_VIDEODEC4_TYPE (gst_omx_videodec4_get_type ())

typedef struct GstOmxVideoDec4 GstOmxVideoDec4;
typedef struct GstOmxVideoDec4Class GstOmxVideoDec4Class;

struct GstOmxVideoDec4
{
  GstOmxBaseVideoDec omx_base;
};

struct GstOmxVideoDec4Class
{
  GstOmxBaseVideoDecClass parent_class;
};

GType gst_omx_videodec4_get_type (void);

/********************  UHD Video Dec 0 **********************/
#define GST_OMX_UHD_VIDEODEC(obj) (GstOmxUhdVideoDec *) (obj)
#define GST_OMX_UHD_VIDEODEC_TYPE (gst_omx_uhd_videodec_get_type ())

typedef struct GstOmxUhdVideoDec GstOmxUhdVideoDec;
typedef struct GstOmxUhdVideoDecClass GstOmxUhdVideoDecClass;

struct GstOmxUhdVideoDec
{
  GstOmxBaseVideoDec omx_base;
};

struct GstOmxUhdVideoDecClass
{
  GstOmxBaseVideoDecClass parent_class;
};

GType gst_omx_uhd_videodec_get_type (void);

G_END_DECLS
#endif /* GSTOMX_VIDEODEC_H */
