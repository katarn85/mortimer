/*
 * This library is licensed under 2 different licenses and you
 * can choose to use it under the terms of either one of them. The
 * two licenses are the MPL 1.1 and the LGPL.
 *
 * MPL:
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * LGPL:
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstwfdrtspsrc.h"
#include "gstwfdrtprequester.h"
#include "wfdrtpbuffer/gstwfdrtpbuffer.h"

static gboolean
plugin_init (GstPlugin * plugin)
{

  if (!gst_element_register (plugin, "wfdrtspsrc", GST_RANK_NONE, GST_TYPE_WFDRTSPSRC))
    return FALSE;
  if (!gst_element_register (plugin, "wfdrtprequester", GST_RANK_NONE, GST_TYPE_WFD_RTP_REQUESTER))
    return FALSE;
  if (!gst_element_register (plugin, "wfdrtpbuffer", GST_RANK_NONE, GST_TYPE_WFD_RTP_BUFFER))
    return FALSE;

  return TRUE;
}


GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "wfdmanager",
    "Wi-Fi Display management plugin library",
    plugin_init,
    VERSION,
    "LGPL",
    "Samsung Electronics Co",
    "http://www.samsung.com")
