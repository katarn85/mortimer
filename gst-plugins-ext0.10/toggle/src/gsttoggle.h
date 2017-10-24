/*
 * toggle
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>
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

#ifndef __GST_MYTOGGLE_H__
#define __GST_MYTOGGLE_H__


#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS


#define GST_TYPE_MYTOGGLE \
  (gst_mytoggle_get_type())
#define GST_MYTOGGLE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MYTOGGLE,GstMytoggle))
#define GST_MYTOGGLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MYTOGGLE,GstMytoggleClass))
#define GST_IS_MYTOGGLE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MYTOGGLE))
#define GST_IS_MYTOGGLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MYTOGGLE))

typedef struct _GstMytoggle GstMytoggle;
typedef struct _GstMytoggleClass GstMytoggleClass;

struct _GstMytoggle {
  GstBaseTransform 	 element;
  gboolean block_data;  
};

struct _GstMytoggleClass {
  GstBaseTransformClass parent_class;
};

GType gst_mytoggle_get_type(void);

G_END_DECLS

#endif /* __GST_MYTOGGLE_H__ */
