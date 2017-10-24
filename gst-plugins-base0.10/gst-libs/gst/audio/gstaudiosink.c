/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2005 Wim Taymans <wim@fluendo.com>
 *
 * gstaudiosink.c: simple audio sink base class
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

/**
 * SECTION:gstaudiosink
 * @short_description: Simple base class for audio sinks
 * @see_also: #GstBaseAudioSink, #GstRingBuffer, #GstAudioSink.
 *
 * This is the most simple base class for audio sinks that only requires
 * subclasses to implement a set of simple functions:
 *
 * <variablelist>
 *   <varlistentry>
 *     <term>open()</term>
 *     <listitem><para>Open the device.</para></listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>prepare()</term>
 *     <listitem><para>Configure the device with the specified format.</para></listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>write()</term>
 *     <listitem><para>Write samples to the device.</para></listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>reset()</term>
 *     <listitem><para>Unblock writes and flush the device.</para></listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>delay()</term>
 *     <listitem><para>Get the number of samples written but not yet played 
 *     by the device.</para></listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>unprepare()</term>
 *     <listitem><para>Undo operations done by prepare.</para></listitem>
 *   </varlistentry>
 *   <varlistentry>
 *     <term>close()</term>
 *     <listitem><para>Close the device.</para></listitem>
 *   </varlistentry>
 * </variablelist>
 *
 * All scheduling of samples and timestamps is done in this base class
 * together with #GstBaseAudioSink using a default implementation of a
 * #GstRingBuffer that uses threads.
 *
 * Last reviewed on 2006-09-27 (0.10.12)
 */

#include <string.h>

#include "gstaudiosink.h"

#include "gst/glib-compat-private.h"

#ifndef GST_TIME_FORMAT5
#define GST_TIME_FORMAT5 "u:%02u:%02u.%03u"
#endif

#ifndef GST_TIME_ARGS5
#define GST_TIME_ARGS5(t) \
        GST_CLOCK_TIME_IS_VALID (t) ? \
        (guint) (((GstClockTime)(t)) / (GST_SECOND * 60 * 60)) : 99, \
        GST_CLOCK_TIME_IS_VALID (t) ? \
        (guint) ((((GstClockTime)(t)) / (GST_SECOND * 60)) % 60) : 99, \
        GST_CLOCK_TIME_IS_VALID (t) ? \
        (guint) ((((GstClockTime)(t)) / GST_SECOND) % 60) : 99, \
        GST_CLOCK_TIME_IS_VALID (t) ? \
        (guint) (((((GstClockTime)(t)) % GST_SECOND))/GST_MSECOND) : 999
#endif

GST_DEBUG_CATEGORY_STATIC (gst_audio_sink_debug);
#define GST_CAT_DEFAULT gst_audio_sink_debug

#define GST_TYPE_AUDIORING_BUFFER        \
        (gst_audioringbuffer_get_type())
#define GST_AUDIORING_BUFFER(obj)        \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AUDIORING_BUFFER,GstAudioRingBuffer))
#define GST_AUDIORING_BUFFER_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AUDIORING_BUFFER,GstAudioRingBufferClass))
#define GST_AUDIORING_BUFFER_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_AUDIORING_BUFFER, GstAudioRingBufferClass))
#define GST_AUDIORING_BUFFER_CAST(obj)        \
        ((GstAudioRingBuffer *)obj)
#define GST_IS_AUDIORING_BUFFER(obj)     \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AUDIORING_BUFFER))
#define GST_IS_AUDIORING_BUFFER_CLASS(klass)\
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AUDIORING_BUFFER))

typedef struct _GstAudioRingBuffer GstAudioRingBuffer;
typedef struct _GstAudioRingBufferClass GstAudioRingBufferClass;

#define GST_AUDIORING_BUFFER_GET_COND(buf) (((GstAudioRingBuffer *)buf)->cond)
#define GST_AUDIORING_BUFFER_WAIT(buf)     (g_cond_wait (GST_AUDIORING_BUFFER_GET_COND (buf), GST_OBJECT_GET_LOCK (buf)))
#define GST_AUDIORING_BUFFER_SIGNAL(buf)   (g_cond_signal (GST_AUDIORING_BUFFER_GET_COND (buf)))
#define GST_AUDIORING_BUFFER_BROADCAST(buf)(g_cond_broadcast (GST_AUDIORING_BUFFER_GET_COND (buf)))

struct _GstAudioRingBuffer
{
  GstRingBuffer object;

  gboolean running;
  gint queuedseg;

  GCond *cond;
};

struct _GstAudioRingBufferClass
{
  GstRingBufferClass parent_class;
};

static void gst_audioringbuffer_class_init (GstAudioRingBufferClass * klass);
static void gst_audioringbuffer_init (GstAudioRingBuffer * ringbuffer,
    GstAudioRingBufferClass * klass);
static void gst_audioringbuffer_dispose (GObject * object);
static void gst_audioringbuffer_finalize (GObject * object);

static GstRingBufferClass *ring_parent_class = NULL;

static gboolean gst_audioringbuffer_open_device (GstRingBuffer * buf);
static gboolean gst_audioringbuffer_close_device (GstRingBuffer * buf);
static gboolean gst_audioringbuffer_acquire (GstRingBuffer * buf,
    GstRingBufferSpec * spec);
static gboolean gst_audioringbuffer_release (GstRingBuffer * buf);
static gboolean gst_audioringbuffer_start (GstRingBuffer * buf);
static gboolean gst_audioringbuffer_pause (GstRingBuffer * buf);
static gboolean gst_audioringbuffer_stop (GstRingBuffer * buf);
static guint gst_audioringbuffer_delay (GstRingBuffer * buf);
static gboolean gst_audioringbuffer_activate (GstRingBuffer * buf,
    gboolean active);

static unsigned long long get_time()
{
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	return (ts.tv_sec * GST_SECOND + ts.tv_nsec * GST_NSECOND);
}

/* ringbuffer abstract base class */
static GType
gst_audioringbuffer_get_type (void)
{
  static GType ringbuffer_type = 0;

  if (!ringbuffer_type) {
    static const GTypeInfo ringbuffer_info = {
      sizeof (GstAudioRingBufferClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_audioringbuffer_class_init,
      NULL,
      NULL,
      sizeof (GstAudioRingBuffer),
      0,
      (GInstanceInitFunc) gst_audioringbuffer_init,
      NULL
    };

    ringbuffer_type =
        g_type_register_static (GST_TYPE_RING_BUFFER, "GstAudioSinkRingBuffer",
        &ringbuffer_info, 0);
  }
  return ringbuffer_type;
}

static void
gst_audioringbuffer_class_init (GstAudioRingBufferClass * klass)
{
  GObjectClass *gobject_class;
  GstRingBufferClass *gstringbuffer_class;

  gobject_class = (GObjectClass *) klass;
  gstringbuffer_class = (GstRingBufferClass *) klass;

  ring_parent_class = g_type_class_peek_parent (klass);

  gobject_class->dispose = gst_audioringbuffer_dispose;
  gobject_class->finalize = gst_audioringbuffer_finalize;

  gstringbuffer_class->open_device =
      GST_DEBUG_FUNCPTR (gst_audioringbuffer_open_device);
  gstringbuffer_class->close_device =
      GST_DEBUG_FUNCPTR (gst_audioringbuffer_close_device);
  gstringbuffer_class->acquire =
      GST_DEBUG_FUNCPTR (gst_audioringbuffer_acquire);
  gstringbuffer_class->release =
      GST_DEBUG_FUNCPTR (gst_audioringbuffer_release);
  gstringbuffer_class->start = GST_DEBUG_FUNCPTR (gst_audioringbuffer_start);
  gstringbuffer_class->pause = GST_DEBUG_FUNCPTR (gst_audioringbuffer_pause);
  gstringbuffer_class->resume = GST_DEBUG_FUNCPTR (gst_audioringbuffer_start);
  gstringbuffer_class->stop = GST_DEBUG_FUNCPTR (gst_audioringbuffer_stop);

  gstringbuffer_class->delay = GST_DEBUG_FUNCPTR (gst_audioringbuffer_delay);
  gstringbuffer_class->activate =
      GST_DEBUG_FUNCPTR (gst_audioringbuffer_activate);
}

typedef guint (*WriteFunc) (GstAudioSink * sink, gpointer data, guint length);

/* this internal thread does nothing else but write samples to the audio device.
 * It will write each segment in the ringbuffer and will update the play
 * pointer. 
 * The start/stop methods control the thread.
 */
static void
audioringbuffer_thread_func (GstRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;
  GstBaseSink *basesink;
  GstAudioRingBuffer *abuf = GST_AUDIORING_BUFFER_CAST (buf);
  WriteFunc writefunc;
  GstMessage *message;
  GValue val = { 0 };
  GstClockTime timestamp = 0;
  GstClockTime remain_data = 0;
  GstClockTime expected_remain_data = 0;
  GstClockTime written_data = 0;
  GstClockTime rendertime = 0;
  GstClockTime render_delay = 0;
  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);
  basesink = GST_BASE_SINK(sink);

  GST_DEBUG_OBJECT (sink, "enter thread");

  GST_OBJECT_LOCK (abuf);
  GST_DEBUG_OBJECT (sink, "signal wait");
  GST_AUDIORING_BUFFER_SIGNAL (buf);
  GST_OBJECT_UNLOCK (abuf);

  writefunc = csink->write;
  if (writefunc == NULL)
    goto no_function;

  g_value_init (&val, G_TYPE_POINTER);
  g_value_set_pointer (&val, sink->thread);
  message = gst_message_new_stream_status (GST_OBJECT_CAST (buf),
      GST_STREAM_STATUS_TYPE_ENTER, GST_ELEMENT_CAST (sink));
  gst_message_set_stream_status_object (message, &val);
  GST_DEBUG_OBJECT (sink, "posting ENTER stream status");
  gst_element_post_message (GST_ELEMENT_CAST (sink), message);

  while (TRUE) {
    gint left, len;
    guint8 *readptr;
    gint readseg;
    gint writeseg;
    timestamp = 0;
    remain_data = 0; // == delay
    expected_remain_data = 0; // remained pcm data (nsec)  =  accumulated pcm data - current time == delay
    written_data = 0; // accumulated pcm data (nsec)
    rendertime = 0; // current time (nsec)
    render_delay = 0;

    /* buffer must be started */
    if (gst_ring_buffer_prepare_read (buf, &readseg, &readptr, &len)) {
      gint written;

      left = len;
      guint64 sum = 0;

#if 1 // debug a/v sync
      if (basesink->debugCategory && GST_LEVEL_INFO <= gst_debug_category_get_threshold (basesink->debugCategory))
      {
        int i=0;
        gshort *ptr = readptr;
        for(i=0; i< left/2; i++)
        {
          if (ptr[i] != 0xffff && ptr[i] != 0x0000)
          sum += ABS(ptr[i]);
        }
        if (csink->delay)
          remain_data = gst_util_uint64_scale_int(csink->delay(sink), GST_SECOND, buf->spec.rate);
        rendertime = get_time();
        written_data = gst_util_uint64_scale_int((sink->sum_of_written_bytes/buf->spec.bytes_per_sample), GST_SECOND, buf->spec.rate);
        expected_remain_data = ((GST_CLOCK_TIME_IS_VALID(sink->render_started) && rendertime >= sink->render_started) ? rendertime - sink->render_started : 0);
        if (written_data >= expected_remain_data)
          expected_remain_data = written_data - expected_remain_data;
        else {
          GST_INFO_OBJECT(sink, "Underflow, written_data[ %"GST_TIME_FORMAT5" ],  expected_remain_data[ %"GST_TIME_FORMAT5" ]", GST_TIME_ARGS5(written_data), GST_TIME_ARGS5(expected_remain_data));
          written_data = expected_remain_data;
        }
      }
#endif

      if (buf->timestamp_per_segment && buf->num_of_timestamp > readseg) // For debugging
        timestamp = buf->timestamp_per_segment[readseg];
      writeseg = buf->debug_segwrite;

      do {
#if 1 // debug a/v sync
        if (basesink->debugCategory && timestamp) {
				render_delay = gst_base_sink_get_render_delay(basesink);
				if (GST_LEVEL_DEBUG == gst_debug_category_get_threshold (basesink->debugCategory)) {
	              GST_CAT_DEBUG_OBJECT(basesink->debugCategory, sink, "expected curr[%"GST_TIME_FORMAT5" ] ts[%"GST_TIME_FORMAT5" ] RealDelay[%lld ms] alsadelay[%lld ms] render_delay[%lld ms] WS[%d/%d], RS[%d/%d] size[%d/%d] - sum[ %lld, %s ]",
	                  GST_TIME_ARGS5(rendertime+expected_remain_data), GST_TIME_ARGS5(timestamp), expected_remain_data/GST_MSECOND, remain_data/GST_MSECOND, render_delay/GST_MSECOND, writeseg%buf->spec.segtotal, writeseg, readseg, buf->segdone, left, len, sum, sum>100000?"-sync-":"");					
				} else {
		          if (sum>100000) {
		              GST_CAT_INFO_OBJECT(basesink->debugCategory, sink, "expected curr[%"GST_TIME_FORMAT5" ] ts[%"GST_TIME_FORMAT5" ] RealDelay[%lld ms] alsadelay[%lld ms] render_delay[%lld ms] WS[%d/%d], RS[%d/%d] size[%d/%d] - sum[ %lld, %s ]",
		                  GST_TIME_ARGS5(rendertime+expected_remain_data), GST_TIME_ARGS5(timestamp), expected_remain_data/GST_MSECOND, remain_data/GST_MSECOND, render_delay/GST_MSECOND, writeseg%buf->spec.segtotal, writeseg, readseg, buf->segdone, left, len, sum, sum>100000?"-sync-":"");
		          }
				}
        }
#endif
        written = writefunc (sink, readptr, left);
        GST_INFO_OBJECT (sink, "ts[ %"GST_TIME_FORMAT5" ] WS[ %d / %d ], [%d/%d]byte RS[ %d / %d ]",
            GST_TIME_ARGS5(buf->timestamp), buf->debug_segwrite%buf->spec.segtotal, buf->debug_segwrite, written, left, readseg, buf->segdone);
        if (written < 0 || written > left) {
          /* might not be critical, it e.g. happens when aborting playback */
          GST_WARNING_OBJECT (sink,
              "error writing data in %s (reason: %s), skipping segment (left: %d, written: %d)",
              GST_DEBUG_FUNCPTR_NAME (writefunc),
              (errno > 1 ? g_strerror (errno) : "unknown"), left, written);
          break;
        }
        left -= written;
        readptr += written;
        sink->sum_of_written_bytes += written;
      } while (left > 0);

      /* clear written samples */
      gst_ring_buffer_clear (buf, readseg);
      buf->timestamp_per_segment[readseg] = 0;

      /* we wrote one segment */
      gst_ring_buffer_advance (buf, 1);
    } else {
      GST_OBJECT_LOCK (abuf);
      if (!abuf->running)
        goto stop_running;
      if (G_UNLIKELY (g_atomic_int_get (&buf->state) ==
              GST_RING_BUFFER_STATE_STARTED)) {
        GST_OBJECT_UNLOCK (abuf);
        continue;
      }
      GST_DEBUG_OBJECT (sink, "signal wait");
      GST_AUDIORING_BUFFER_SIGNAL (buf);
      GST_DEBUG_OBJECT (sink, "wait for action");
      GST_AUDIORING_BUFFER_WAIT (buf);
      GST_DEBUG_OBJECT (sink, "got signal");
      if (!abuf->running)
        goto stop_running;
      GST_DEBUG_OBJECT (sink, "continue running");
      GST_OBJECT_UNLOCK (abuf);
    }
  }

  /* Will never be reached */
  g_assert_not_reached ();
  return;

  /* ERROR */
no_function:
  {
    GST_DEBUG_OBJECT (sink, "no write function, exit thread");
    return;
  }
stop_running:
  {
    GST_OBJECT_UNLOCK (abuf);
    GST_DEBUG_OBJECT (sink, "stop running, exit thread");
    message = gst_message_new_stream_status (GST_OBJECT_CAST (buf),
        GST_STREAM_STATUS_TYPE_LEAVE, GST_ELEMENT_CAST (sink));
    gst_message_set_stream_status_object (message, &val);
    GST_DEBUG_OBJECT (sink, "posting LEAVE stream status");
    gst_element_post_message (GST_ELEMENT_CAST (sink), message);
    return;
  }
}

static void
gst_audioringbuffer_init (GstAudioRingBuffer * ringbuffer,
    GstAudioRingBufferClass * g_class)
{
  ringbuffer->running = FALSE;
  ringbuffer->queuedseg = 0;

  ringbuffer->cond = g_cond_new ();
}

static void
gst_audioringbuffer_dispose (GObject * object)
{
  G_OBJECT_CLASS (ring_parent_class)->dispose (object);
}

static void
gst_audioringbuffer_finalize (GObject * object)
{
  GstAudioRingBuffer *ringbuffer = GST_AUDIORING_BUFFER_CAST (object);

  g_cond_free (ringbuffer->cond);

  G_OBJECT_CLASS (ring_parent_class)->finalize (object);
}

static gboolean
gst_audioringbuffer_open_device (GstRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;
  gboolean result = TRUE;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  if (csink->open)
    result = csink->open (sink);

  if (!result)
    goto could_not_open;

  return result;

could_not_open:
  {
    GST_DEBUG_OBJECT (sink, "could not open device");
    return FALSE;
  }
}

static gboolean
gst_audioringbuffer_close_device (GstRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;
  gboolean result = TRUE;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  if (csink->close)
    result = csink->close (sink);

  if (!result)
    goto could_not_close;

  return result;

could_not_close:
  {
    GST_DEBUG_OBJECT (sink, "could not close device");
    return FALSE;
  }
}

static gboolean
gst_audioringbuffer_acquire (GstRingBuffer * buf, GstRingBufferSpec * spec)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;
  gboolean result = FALSE;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  if (csink->prepare)
    result = csink->prepare (sink, spec);
  if (!result)
    goto could_not_prepare;

  /* set latency to one more segment as we need some headroom */
  spec->seglatency = spec->segtotal + 1;

  buf->data = gst_buffer_new_and_alloc (spec->segtotal * spec->segsize);
  memset (GST_BUFFER_DATA (buf->data), 0, GST_BUFFER_SIZE (buf->data));

  return TRUE;

  /* ERRORS */
could_not_prepare:
  {
    GST_DEBUG_OBJECT (sink, "could not prepare device");
    return FALSE;
  }
}

static gboolean
gst_audioringbuffer_activate (GstRingBuffer * buf, gboolean active)
{
  GstAudioSink *sink;
  GstAudioRingBuffer *abuf;
  GError *error = NULL;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  abuf = GST_AUDIORING_BUFFER_CAST (buf);

  if (active) {
    abuf->running = TRUE;

    GST_DEBUG_OBJECT (sink, "starting thread");

#if !GLIB_CHECK_VERSION (2, 31, 0)
    sink->thread =
        g_thread_create ((GThreadFunc) audioringbuffer_thread_func, buf, TRUE,
        &error);
#else
    sink->thread = g_thread_try_new ("audiosink-ringbuffer",
        (GThreadFunc) audioringbuffer_thread_func, buf, &error);
#endif

    if (!sink->thread || error != NULL)
      goto thread_failed;

    GST_DEBUG_OBJECT (sink, "waiting for thread");
    /* the object lock is taken */
    GST_AUDIORING_BUFFER_WAIT (buf);
    GST_DEBUG_OBJECT (sink, "thread is started");
  } else {
    abuf->running = FALSE;
    GST_DEBUG_OBJECT (sink, "signal wait");
    GST_AUDIORING_BUFFER_SIGNAL (buf);

    GST_OBJECT_UNLOCK (buf);

    /* join the thread */
    g_thread_join (sink->thread);

    GST_OBJECT_LOCK (buf);
  }
  return TRUE;

  /* ERRORS */
thread_failed:
  {
    if (error)
      GST_ERROR_OBJECT (sink, "could not create thread %s", error->message);
    else
      GST_ERROR_OBJECT (sink, "could not create thread for unknown reason");
    return FALSE;
  }
}

/* function is called with LOCK */
static gboolean
gst_audioringbuffer_release (GstRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;
  gboolean result = FALSE;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  /* free the buffer */
  gst_buffer_unref (buf->data);
  buf->data = NULL;

  if (csink->unprepare)
    result = csink->unprepare (sink);

  if (!result)
    goto could_not_unprepare;

  GST_DEBUG_OBJECT (sink, "unprepared");

  return result;

could_not_unprepare:
  {
    GST_DEBUG_OBJECT (sink, "could not unprepare device");
    return FALSE;
  }
}

static gboolean
gst_audioringbuffer_start (GstRingBuffer * buf)
{
  GstAudioSink *sink;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));

  /* Do not use gst_lock_get_time(), It is using internal lock.
  and if it is not a systemclock, it'll be locked because of trying to get the value from this ringbuffer. Is it a audio clock's bug? */
  sink->render_requested = get_time();
  gst_ring_buffer_clear_all (buf);
  
  GST_DEBUG_OBJECT (sink, "start, sending signal,  at %" GST_TIME_FORMAT5, GST_TIME_ARGS5(sink->render_requested));
  GST_AUDIORING_BUFFER_SIGNAL (buf);

  return TRUE;
}

static gboolean
gst_audioringbuffer_pause (GstRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  /* unblock any pending writes to the audio device */
  if (csink->reset) {
    GST_DEBUG_OBJECT (sink, "reset...");
    csink->reset (sink);
    GST_DEBUG_OBJECT (sink, "reset done");
  }

  sink->sum_of_written_bytes = 0;
  sink->render_started = GST_CLOCK_TIME_NONE;

  if (sink->render_delay_updated) {
    gst_base_sink_set_render_delay(GST_BASE_SINK(sink), 0);
	sink->render_delay_updated = FALSE;
  }

  return TRUE;
}

static gboolean
gst_audioringbuffer_stop (GstRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  /* unblock any pending writes to the audio device */
  if (csink->reset) {
    GST_DEBUG_OBJECT (sink, "reset...");
    csink->reset (sink);
    GST_DEBUG_OBJECT (sink, "reset done");
  }

  sink->sum_of_written_bytes = 0;
  sink->render_started = GST_CLOCK_TIME_NONE;
#if 0
  if (abuf->running) {
    GST_DEBUG_OBJECT (sink, "stop, waiting...");
    GST_AUDIORING_BUFFER_WAIT (buf);
    GST_DEBUG_OBJECT (sink, "stopped");
  }
#endif

  return TRUE;
}

static guint
gst_audioringbuffer_delay (GstRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;
  guint res = 0;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  if (csink->delay)
    res = csink->delay (sink);

  return res;
}

/* AudioSink signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
};

#define _do_init(bla) \
    GST_DEBUG_CATEGORY_INIT (gst_audio_sink_debug, "audiosink", 0, "audiosink element");

GST_BOILERPLATE_FULL (GstAudioSink, gst_audio_sink, GstBaseAudioSink,
    GST_TYPE_BASE_AUDIO_SINK, _do_init);

static GstRingBuffer *gst_audio_sink_create_ringbuffer (GstBaseAudioSink *
    sink);

static gboolean
gst_audio_sink_started_callback (GstAudioSink * sink, GstClockTime render_start_time)
{
  sink->render_started = render_start_time;
  GST_INFO_OBJECT (sink, "systemclock[ %d ] [ %"GST_TIME_FORMAT5" ~ %"GST_TIME_FORMAT5" ~ %"GST_TIME_FORMAT5" ] ",
      GST_IS_SYSTEM_CLOCK(GST_ELEMENT_CLOCK (sink)), GST_TIME_ARGS5(GST_ELEMENT_CAST(sink)->base_time),
      GST_TIME_ARGS5(sink->render_requested), GST_TIME_ARGS5(sink->render_started));
  if (GST_CLOCK_TIME_IS_VALID(sink->render_started) && GST_CLOCK_TIME_IS_VALID(sink->render_requested))
  {
    if (GST_IS_SYSTEM_CLOCK(GST_ELEMENT_CLOCK (sink)))
    {
      if (sink->render_started >= sink->render_requested)
      {
	    /* If the difference between base_time and requsted_time is longer than 500ms,
	    *  it's invalid base_time, so just apply the interval of requested_time and started_time */
        GstClockTime start_time = gst_element_get_start_time (sink);
        if ((sink->render_requested >= (GST_ELEMENT_CAST(sink)->base_time + start_time))
				&& ((sink->render_requested - (GST_ELEMENT_CAST(sink)->base_time + start_time)) < (500*GST_MSECOND)))
        {
		// can believe base_time
		gst_base_sink_set_render_delay(GST_BASE_SINK(sink), (sink->render_started - (GST_ELEMENT_CAST(sink)->base_time + start_time)));
        }
        else
        {
		gst_base_sink_set_render_delay(GST_BASE_SINK(sink), (10*GST_MSECOND + sink->render_started - sink->render_requested));
        }

        GST_ERROR_OBJECT (sink, "render_delay = %"GST_TIME_FORMAT5"  [ %"GST_TIME_FORMAT5" ~ %"GST_TIME_FORMAT5" ~ %"GST_TIME_FORMAT5" ], base[ %"GST_TIME_FORMAT5" ], start[ %"GST_TIME_FORMAT5" ]",
            GST_TIME_ARGS5(sink->render_started-(GST_ELEMENT_CAST(sink)->base_time + start_time)), GST_TIME_ARGS5(GST_ELEMENT_CAST(sink)->base_time + start_time),
            GST_TIME_ARGS5(sink->render_requested), GST_TIME_ARGS5(sink->render_started), GST_TIME_ARGS5(GST_ELEMENT_CAST(sink)->base_time), GST_TIME_ARGS5(start_time));
      }
    }
    else
    {
      if (sink->render_started > sink->render_requested)
      {
        GST_ERROR_OBJECT (sink, "render_delay = %"GST_TIME_FORMAT5"  [ %"GST_TIME_FORMAT5" ~ %"GST_TIME_FORMAT5" ~ %"GST_TIME_FORMAT5" ] ",
            GST_TIME_ARGS5(sink->render_started-sink->render_requested), GST_TIME_ARGS5(GST_ELEMENT_CAST(sink)->base_time),
            GST_TIME_ARGS5(sink->render_requested), GST_TIME_ARGS5(sink->render_started));
        gst_base_sink_set_render_delay(GST_BASE_SINK(sink), (sink->render_started - sink->render_requested));
      }
    }
    sink->render_delay_updated = TRUE;
    sink->render_requested = GST_CLOCK_TIME_NONE;
  }
  return TRUE;
}

static void
gst_audio_sink_base_init (gpointer g_class)
{
}

static void
gst_audio_sink_class_init (GstAudioSinkClass * klass)
{
  GstBaseAudioSinkClass *gstbaseaudiosink_class;

  gstbaseaudiosink_class = (GstBaseAudioSinkClass *) klass;

  gstbaseaudiosink_class->create_ringbuffer =
      GST_DEBUG_FUNCPTR (gst_audio_sink_create_ringbuffer);

  g_type_class_ref (GST_TYPE_AUDIORING_BUFFER);
}

static void
gst_audio_sink_init (GstAudioSink * audiosink, GstAudioSinkClass * g_class)
{
  audiosink->render_requested = GST_CLOCK_TIME_NONE;
  audiosink->render_started = GST_CLOCK_TIME_NONE;
  audiosink->started_cb = gst_audio_sink_started_callback;
  audiosink->render_delay_updated = FALSE;
  audiosink->sum_of_written_bytes = 0;
}

static GstRingBuffer *
gst_audio_sink_create_ringbuffer (GstBaseAudioSink * sink)
{
  GstRingBuffer *buffer;

  GST_DEBUG_OBJECT (sink, "creating ringbuffer");
  buffer = g_object_new (GST_TYPE_AUDIORING_BUFFER, NULL);
  GST_DEBUG_OBJECT (sink, "created ringbuffer @%p", buffer);

  return buffer;
}
