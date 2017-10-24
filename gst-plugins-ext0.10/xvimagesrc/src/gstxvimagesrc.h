/*
 * xvimagesrc
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: Hyunil Park <hyunil46.park@samsung.com>
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
#ifndef __GST_XV_IMAGE_SRC_H__
#define __GST_XV_IMAGE_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/shm.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/Xvproto.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xdamage.h>

#include <dri2.h> //libdri2-dev, libdrm-dev
#include <tbm_bufmgr.h>

#include "xv_types.h"

#define C(b,m)              (((b) >> (m)) & 0xFF)
#define B(c,s)              ((((unsigned int)(c)) & 0xff) << (s))
#define FOURCC(a,b,c,d)     (B(d,24) | B(c,16) | B(b,8) | B(a,0))
#define FOURCC_RGB32        FOURCC('R','G','B','4')
#define FOURCC_I420         FOURCC('I','4','2','0')
#define FOURCC_SN12         FOURCC('S','N','1','2')
#define FOURCC_NV12         FOURCC('N','V','1','2')
#define FOURCC_ST12         FOURCC('S','T','1','2')

G_BEGIN_DECLS

#define GST_TYPE_XV_IMAGE_SRC \
  (gst_xv_image_src_get_type())
#define GST_XV_IMAGE_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_XV_IMAGE_SRC,GstXVImageSrc))
#define GST_XV_IMAGE_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
      GST_TYPE_XV_IMAGE_SRC,GstXVImageSrcClass))
#define GST_IS_XV_IMAGE_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_XV_IMAGE_SRC))
#define GST_IS_XV_IMAGE_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_XV_IMAGE_SRC))

typedef struct _GstXVImageSrc GstXVImageSrc;
typedef struct _GstXVImageSrcClass GstXVImageSrcClass;

struct _GstXVImageSrc {
  GstPushSrc element;
  Display	*dpy;
  int p;
  Pixmap pixmap;
  GC gc;
  unsigned int width;
  unsigned int height;
  unsigned int framesize;
  guint32 format_id;
  Damage damage;
  int damage_base;
  unsigned int evt_base;
  tbm_bufmgr bufmgr;
  void *virtual;
  tbm_bo bo;
  DRI2Buffer* dri2_buffers;
  guint64 running_time;
  guint64 base_time;
  guint64 frame_duration;
  Atom atom_stream_off;
  gint rate_numerator;
  gint rate_denominator;
  GThread *updates_thread;
  gboolean thread_return;
  GQueue *queue;
  GMutex *queue_lock;
  GCond *queue_cond;
  GMutex *cond_lock;
  GCond *buffer_cond;
  gboolean pause_cond_var;
  GCond *pause_cond;
  GMutex *pause_cond_lock;
  GCond *pause_resp;
  GMutex *pause_resp_lock;
  GMutex *buffer_cond_lock;
  GMutex *dpy_lock;
  gint drm_fd;
  gboolean is_secured_mode;
  int current_data_type;
  int new_data_type;
  long get_image_overtime;
  int get_image_overtime_cnt;
  int gemname_cnt;
  int tz_enable;
  long sleep_base_time;
  long sleep_limit_time;
  gboolean switching_to_udp;
  guint64 initial_audio_latency;
  guint64 display_rotate;

  /* For display selection */
  Window win;
  Atom requestor;
  Atom selection;
  Atom target;
  Atom property;
};

struct _GstXVImageSrcClass {
  GstPushSrcClass parent_class;

  /* signals */
  void	(*ui_only)	(void *data);
  void	(*video_with_ui)	(void *data);
  void	(*video_only)	(void *data);
  void	(*selection_notify)	(void *data);
};

GType gst_xv_image_src_get_type (void);

G_END_DECLS

#endif /* __GST_XV_IMAGE_SRC_H__ */

