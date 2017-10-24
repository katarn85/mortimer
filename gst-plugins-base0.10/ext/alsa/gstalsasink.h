/* GStreamer
 * Copyright (C)  2005 Wim Taymans <wim@fluendo.com>
 *
 * gstalsasink.h: 
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


#ifndef __GST_ALSASINK_H__
#define __GST_ALSASINK_H__

#include <gst/gst.h>
#include <gst/audio/gstaudiosink.h>
#include <alsa/asoundlib.h>

G_BEGIN_DECLS

#define GST_TYPE_ALSA_SINK            (gst_alsasink_get_type())
#define GST_ALSA_SINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ALSA_SINK,GstAlsaSink))
#define GST_ALSA_SINK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ALSA_SINK,GstAlsaSinkClass))
#define GST_IS_ALSA_SINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ALSA_SINK))
#define GST_IS_ALSA_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ALSA_SINK))
#define GST_ALSA_SINK_CAST(obj)       ((GstAlsaSink *) (obj))

typedef struct _GstAlsaSink GstAlsaSink;
typedef struct _GstAlsaSinkClass GstAlsaSinkClass;

#define GST_ALSA_SINK_GET_LOCK(obj)	(GST_ALSA_SINK_CAST (obj)->alsa_lock)
#define GST_ALSA_SINK_LOCK(obj)	        (g_mutex_lock (GST_ALSA_SINK_GET_LOCK (obj)))
#define GST_ALSA_SINK_UNLOCK(obj)	(g_mutex_unlock (GST_ALSA_SINK_GET_LOCK (obj)))

/**
 * GstAlsaSink:
 *
 * Opaque data structure
 */
struct _GstAlsaSink {
  GstAudioSink    sink;

  gchar                 *device;

  snd_pcm_t             *handle;
  snd_pcm_hw_params_t   *hwparams;
  snd_pcm_sw_params_t   *swparams;

  snd_pcm_access_t access;
  snd_pcm_format_t format;
  guint rate;
  guint channels;
  gint bytes_per_sample;
  gboolean iec958;
  gboolean need_swap;
  gboolean first_frame_processed;
  gboolean mute;
  gboolean amixer_pp_onoff;
  gboolean hd_audio_mode;
  gboolean hd_audio_avoc_set;

  guint buffer_time;
  guint period_time;
  snd_pcm_uframes_t buffer_size;
  snd_pcm_uframes_t period_size;

  snd_pcm_uframes_t start_threshold;
  guint64 sum_of_written_frames;
  gboolean need_update_render_delay;

  GstCaps *cached_caps;

  GMutex *alsa_lock;
  gint device_id;
  gboolean delayset;
  gint prev_delay;
  gboolean game_render;
  void* debug_fd;
  gint focused_zone;
  gboolean reset_called;
  guint64 render_start_time;//will not be updated if need_update_render_delay is FALSE.
  guint delayed_unmute;
  guint mute_flag;
};

struct _GstAlsaSinkClass {
  GstAudioSinkClass parent_class;
};

GType gst_alsasink_get_type(void);

G_END_DECLS

#endif /* __GST_ALSASINK_H__ */
