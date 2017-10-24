/* 
 * gstmms.h: header file for gst-mms plugin
 */

#ifndef __GST_MMS_H__
#define __GST_MMS_H__

#include <gst/gst.h>
#include <libmms/mmsx.h>
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

/* #define's don't like whitespacey bits */
#define GST_TYPE_MMS \
  (gst_mms_get_type())
#define GST_MMS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MMS,GstMMS))
#define GST_MMS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MMS,GstMMSClass))
#define GST_IS_MMS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MMS))
#define GST_IS_MMS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MMS))
#define GST_MMSSRC_CAST(obj) \
  ((GstMMS *)(obj))
//#define GST_BASE_SRC(obj)               (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BASE_SRC,GstBaseSrc))

typedef struct _GstMMS      GstMMS;
typedef struct _GstMMSClass GstMMSClass;

#define GST_MMS_STREAM_GET_LOCK(mms)   (GST_MMSSRC_CAST((mms))->stream_rec_lock)
#define GST_MMS_STREAM_LOCK(mms)       (g_static_rec_mutex_lock (GST_MMS_STREAM_GET_LOCK((mms))))
#define GST_MMS_STREAM_UNLOCK(mms)     (g_static_rec_mutex_unlock (GST_MMS_STREAM_GET_LOCK((mms))))

enum _GstMMSState
{
    GST_MMS_STATE_INIT,
    GST_MMS_STATE_CONNECTING,
    GST_MMS_STATE_CONNECTED,
    GST_MMS_STATE_FLUSHING
};

struct _GstMMS
{
  GstPushSrc parent;

  gchar  *uri_name;
  gchar  *current_connection_uri_name;
  guint  connection_speed;
  
  mmsx_t *connection;
  mms_io_t *io;
  guint connection_timeout;

  GstPollFD fdp;
  GstPoll *poll;
  
  GstTask *task;
  GStaticRecMutex *stream_rec_lock;
  gint loop_cmd;
  
  enum _GstMMSState state;
};

struct _GstMMSClass 
{
  GstPushSrcClass parent_class;
};

GType gst_mms_get_type (void);

G_END_DECLS

#endif /* __GST_MMS_H__ */
