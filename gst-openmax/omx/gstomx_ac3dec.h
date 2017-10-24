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

#ifndef GSTOMX_AC3DEC_H
#define GSTOMX_AC3DEC_H

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_OMX_AC3DEC(obj) (GstOmxAc3Dec *) (obj)
#define GST_OMX_AC3DEC_TYPE (gst_omx_ac3dec_get_type ())
typedef struct GstOmxAc3Dec GstOmxAc3Dec;
typedef struct GstOmxAc3DecClass GstOmxAc3DecClass;

#include "gstomx_base_audiodec.h"

struct GstOmxAc3Dec
{
  GstOmxBaseAudioDec omx_base;
};

struct GstOmxAc3DecClass
{
  GstOmxBaseAudioDecClass parent_class;
};

GType gst_omx_ac3dec_get_type (void);

G_END_DECLS
#endif /* GSTOMX_AC3DEC_H */
