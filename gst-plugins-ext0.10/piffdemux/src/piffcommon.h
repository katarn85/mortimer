
#ifndef __GST_PIFFCOMMON_H__
#define __GST_PIFFCOMMON_H__

G_BEGIN_DECLS

typedef struct _piff_fragment_longtime_info_t
{
  guint64 ts;
  guint64 duration;
}piff_fragment_longtime_info;

typedef struct _piff_fragment_time_info_t
{
  guint32 ts;
  guint32 duration;
}piff_fragment_time_info;

typedef struct _live_param_t
{
  gboolean is_eos; /* is live session ended */
  guint count; /*  fragment parameters count */
  gchar *media_type;
  piff_fragment_time_info *info;
  piff_fragment_longtime_info *long_info;
}piff_live_param_t;
G_END_DECLS

#endif /* __GST_PIFFPALETTE_H__ */
