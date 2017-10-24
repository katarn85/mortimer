/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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

#include "piffdemux_types.h"
#include "piffdemux_dump.h"
#include "piffdemux_fourcc.h"

static const PiffNodeType piff_node_types[] = {
  {FOURCC_vide, "video media", 0},
  {FOURCC_hint, "hint", 0,},
  {FOURCC_mp4a, "mp4a", 0,},
  {FOURCC_mp4v, "mp4v", 0,},
  {FOURCC_alac, "alac", 0,},
  {FOURCC_meta, "meta", 0, piffdemux_dump_unknown},
  {FOURCC_____, "----", PIFF_FLAG_CONTAINER,},
  {FOURCC_free, "free", 0,},
  {FOURCC_mfra, "movie fragment random access",
      PIFF_FLAG_CONTAINER,},
  {FOURCC_tfra, "track fragment random access", 0,
      piffdemux_dump_tfra},
  {FOURCC_mfro, "movie fragment random access offset", 0,
      piffdemux_dump_mfro},
  {FOURCC_moof, "movie fragment", PIFF_FLAG_CONTAINER,},
  {FOURCC_mfhd, "movie fragment header", 0,},
  {FOURCC_traf, "track fragment", PIFF_FLAG_CONTAINER,},
  {FOURCC_tfhd, "track fragment header", 0,
      piffdemux_dump_tfhd},
  {FOURCC_sdtp, "independent and disposable samples", 0,
      piffdemux_dump_sdtp},
  {FOURCC_trun, "track fragment run", 0, piffdemux_dump_trun},
  {FOURCC_mdat, "moovie data", 0, piffdemux_dump_unknown},
  {FOURCC_trex, "moovie data", 0, piffdemux_dump_trex},
  {FOURCC_mvex, "mvex", PIFF_FLAG_CONTAINER,},
  {FOURCC_ovc1, "ovc1", 0},
  {FOURCC_owma, "owma", 0},
  {FOURCC_tfxd, "tfxd", 0},
  {FOURCC_tfrf, "tfrf", 0},
  {FOURCC_uuid, "uuid", 0},

  {0, "unknown", 0,},
};

static const int n_piff_node_types =
    sizeof (piff_node_types) / sizeof (piff_node_types[0]);

const PiffNodeType *
piffdemux_type_get (guint32 fourcc)
{
  int i;

  for (i = 0; i < n_piff_node_types; i++) {
    if (G_UNLIKELY (piff_node_types[i].fourcc == fourcc))
      return piff_node_types + i;
  }

  GST_WARNING ("unknown QuickTime node type %" GST_FOURCC_FORMAT,
      GST_FOURCC_ARGS (fourcc));

  return piff_node_types + n_piff_node_types - 1;
}
