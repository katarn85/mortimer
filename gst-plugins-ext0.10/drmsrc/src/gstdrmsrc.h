/*
 * drmsrc
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


#ifndef __GST_DRM_SRC_H__
#define __GST_DRM_SRC_H__

#include <sys/types.h>
#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include <drm_trusted_client_types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#ifndef S_ISREG
#define S_ISREG(mode) ((mode)&_S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode)&_S_IFDIR)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(x) (0)
#endif
#ifndef O_BINARY
#define O_BINARY (0)
#endif

G_BEGIN_DECLS

#define GST_TYPE_DRM_SRC (gst_drm_src_get_type())
#define GST_DRM_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DRM_SRC,GstDrmSrc))
#define GST_DRM_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DRM_SRC,GstDrmSrcClass))
#define GST_IS_DRM_SRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DRM_SRC))
#define GST_IS_DRM_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DRM_SRC))

typedef struct _GstDrmSrc GstDrmSrc;
typedef struct _GstDrmSrcClass GstDrmSrcClass;

struct _GstDrmSrc 
{
	GstBaseSrc element;
	gchar *filename;			
	gchar *uri;				 
	gint fd;				 
	guint64 read_position;	
	gboolean seekable;
	gboolean is_regular;    
	gboolean is_drm;          // flag indicating drm file
	DRM_DECRYPT_HANDLE hfile;
	gboolean is_playready;
	gboolean is_oma;
	gboolean event_posted;
	gboolean isopen;
#ifdef CONTROL_PAGECACHE
	guint64 accum;
#endif
};

struct _GstDrmSrcClass 
{
	GstBaseSrcClass parent_class;
};

GType gst_drm_src_get_type (void);

G_END_DECLS

#endif /* __GST_DRM_SRC_H__ */
