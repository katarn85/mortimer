

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
//#include "gst/gst-i18n-plugin.h"
#include "piffdemux.h"
#include <gst/pbutils/pbutils.h>

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "piffdemux", GST_RANK_PRIMARY, GST_TYPE_PIFFDEMUX))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "piffdemux",
    "ISO base media file format support (PIFF)",
    plugin_init, VERSION, "LGPL",  "Samsung Electronics Co", "http://www.samsung.com");
