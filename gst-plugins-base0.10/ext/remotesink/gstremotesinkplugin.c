/*  GStreamer remote sink plugin
 *  Copyright (C) <2014> Liu Yang(yang010.liu@samsung.com)
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

/* FIXME 0.11: turn this into a proper base class */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gst/gst-i18n-plugin.h>
#include "gstremoteaudiosink.h"

GST_DEBUG_CATEGORY (remote_sink_debug);

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "remoteaudiosink", GST_RANK_PRIMARY,
          GST_TYPE_REMOTE_AUDIO_SINK))
    return FALSE;

  GST_DEBUG_CATEGORY_INIT (remote_sink_debug, "remote sink", 0, "remote sink plugins");

#ifdef ENABLE_NLS
  GST_DEBUG ("binding text domain %s to locale dir %s", GETTEXT_PACKAGE,
      LOCALEDIR);
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "remote sink",
    "remote audio/video/subtitle sink plugin library",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
