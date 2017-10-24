/*
 * pdpushsrc
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
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

#ifndef __GST_PD_PUSHSRC_H__
#define __GST_PD_PUSHSRC_H__

#include <sys/types.h>
#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>

G_BEGIN_DECLS

#define GST_TYPE_PD_PUSHSRC                (gst_pd_pushsrc_get_type())
#define GST_PD_PUSHSRC(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PD_PUSHSRC,GstPDPushSrc))
#define GST_PD_PUSHSRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PD_PUSHSRC,GstPDPushSrcClass))
#define GST_IS_PD_PUSHSRC(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PD_PUSHSRC))
#define GST_IS_PD_PUSHSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PD_PUSHSRC))
#define GST_PD_PUSHSRC_CAST(obj) ((GstPDPushSrc*) obj)

typedef struct _GstPDPushSrc GstPDPushSrc;
typedef struct _GstPDPushSrcClass GstPDPushSrcClass;

/**
 * GstFileSrc:
 *
 * Opaque #GstPDPushSrc structure.
 */
struct _GstPDPushSrc {
  GstBaseSrc element;
  GstPad *srcpad;

  gchar *filename;			/* filename */
  gchar *uri;				/* caching the URI */
  gint fd;				/* open file descriptor */
  guint64 read_position;		/* position of fd */

  gboolean seekable;                    /* whether the file is seekable */
  gboolean is_regular;                  /* whether it's a (symlink to a)                                          regular file */

  gboolean is_eos;

};

struct _GstPDPushSrcClass {
  GstBaseSrcClass parent_class;
};

GType gst_pd_pushsrc_get_type (void);

G_END_DECLS

#endif /* __GST_FILE_SRC_H__ */
