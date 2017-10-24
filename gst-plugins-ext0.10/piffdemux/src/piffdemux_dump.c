/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2009 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) <2009> STEricsson <benjamin.gaignard@stericsson.com>
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

#include "piffatomparser.h"

#include <string.h>

#define GET_UINT8(data)   gst_byte_reader_get_uint8_unchecked(data)
#define GET_UINT16(data)  gst_byte_reader_get_uint16_be_unchecked(data)
#define GET_UINT32(data)  gst_byte_reader_get_uint32_be_unchecked(data)
#define GET_UINT64(data)  gst_byte_reader_get_uint64_be_unchecked(data)
#define GET_FP32(data)   (gst_byte_reader_get_uint32_be_unchecked(data)/65536.0)
#define GET_FP16(data)   (gst_byte_reader_get_uint16_be_unchecked(data)/256.0)
#define GET_FOURCC(data)  piff_atom_parser_get_fourcc_unchecked(data)

gboolean
piffdemux_dump_vmhd (GstPiffDemux * piffdemux, GstByteReader * data, int depth)
{
  if (!piff_atom_parser_has_remaining (data, 4 + 4))
    return FALSE;

  GST_LOG ("%*s  version/flags: %08x", depth, "", GET_UINT32 (data));
  GST_LOG ("%*s  mode/color:    %08x", depth, "", GET_UINT32 (data));
  return TRUE;
}


gboolean
piffdemux_dump_mfro (GstPiffDemux * piffdemux, GstByteReader * data, int depth)
{
  if (!piff_atom_parser_has_remaining (data, 4))
    return FALSE;

  GST_LOG ("%*s  size: %d", depth, "", GET_UINT32 (data));
  return TRUE;
}

gboolean
piffdemux_dump_tfra (GstPiffDemux * piffdemux, GstByteReader * data, int depth)
{
  guint64 time = 0, moof_offset = 0;
  guint32 len = 0, num_entries = 0, ver_flags = 0, track_id = 0, i;
  guint value_size, traf_size, trun_size, sample_size;

  if (!gst_byte_reader_get_uint32_be (data, &ver_flags))
    return FALSE;

  GST_LOG ("%*s  version/flags: %08x", depth, "", ver_flags);

  if (!gst_byte_reader_get_uint32_be (data, &track_id) ||
      gst_byte_reader_get_uint32_be (data, &len) ||
      gst_byte_reader_get_uint32_be (data, &num_entries))
    return FALSE;

  GST_LOG ("%*s  track ID:      %u", depth, "", track_id);
  GST_LOG ("%*s  length:        0x%x", depth, "", len);
  GST_LOG ("%*s  n entries:     %u", depth, "", num_entries);

  value_size = ((ver_flags >> 24) == 1) ? sizeof (guint64) : sizeof (guint32);
  sample_size = (len & 3) + 1;
  trun_size = ((len & 12) >> 2) + 1;
  traf_size = ((len & 48) >> 4) + 1;

  if (!piff_atom_parser_has_chunks (data, num_entries,
          value_size + value_size + traf_size + trun_size + sample_size))
    return FALSE;

  for (i = 0; i < num_entries; i++) {
    piff_atom_parser_get_offset (data, value_size, &time);
    piff_atom_parser_get_offset (data, value_size, &moof_offset);
    GST_LOG ("%*s    time:          %" G_GUINT64_FORMAT, depth, "", time);
    GST_LOG ("%*s    moof_offset:   %" G_GUINT64_FORMAT,
        depth, "", moof_offset);
    GST_LOG ("%*s    traf_number:   %u", depth, "",
        piff_atom_parser_get_uint_with_size_unchecked (data, traf_size));
    GST_LOG ("%*s    trun_number:   %u", depth, "",
        piff_atom_parser_get_uint_with_size_unchecked (data, trun_size));
    GST_LOG ("%*s    sample_number: %u", depth, "",
        piff_atom_parser_get_uint_with_size_unchecked (data, sample_size));
  }

  return TRUE;
}

gboolean
piffdemux_dump_tfhd (GstPiffDemux * piffdemux, GstByteReader * data, int depth)
{
  guint32 flags = 0, n = 0, track_id = 0;
  guint64 base_data_offset = 0;

  if (!gst_byte_reader_skip (data, 1) ||
      !gst_byte_reader_get_uint24_be (data, &flags))
    return FALSE;
  GST_LOG ("%*s  flags: %08x", depth, "", flags);

  if (!gst_byte_reader_get_uint32_be (data, &track_id))
    return FALSE;
  GST_LOG ("%*s  track_id: %u", depth, "", track_id);

  if (flags & TF_BASE_DATA_OFFSET) {
    if (!gst_byte_reader_get_uint64_be (data, &base_data_offset))
      return FALSE;
    GST_LOG ("%*s    base-data-offset: %" G_GUINT64_FORMAT,
        depth, "", base_data_offset);
  }

  if (flags & TF_SAMPLE_DESCRIPTION_INDEX) {
    if (!gst_byte_reader_get_uint32_be (data, &n))
      return FALSE;
    GST_LOG ("%*s    sample-description-index: %u", depth, "", n);
  }

  if (flags & TF_DEFAULT_SAMPLE_DURATION) {
    if (!gst_byte_reader_get_uint32_be (data, &n))
      return FALSE;
    GST_LOG ("%*s    default-sample-duration:  %u", depth, "", n);
  }

  if (flags & TF_DEFAULT_SAMPLE_SIZE) {
    if (!gst_byte_reader_get_uint32_be (data, &n))
      return FALSE;
    GST_LOG ("%*s    default-sample-size:  %u", depth, "", n);
  }

  if (flags & TF_DEFAULT_SAMPLE_FLAGS) {
    if (!gst_byte_reader_get_uint32_be (data, &n))
      return FALSE;
    GST_LOG ("%*s    default-sample-flags:  %u", depth, "", n);
  }

  GST_LOG ("%*s    duration-is-empty:     %s", depth, "",
      flags & TF_DURATION_IS_EMPTY ? "yes" : "no");

  return TRUE;
}

gboolean
piffdemux_dump_trun (GstPiffDemux * piffdemux, GstByteReader * data, int depth)
{
  guint32 flags = 0, samples_count = 0, data_offset = 0, first_sample_flags = 0;
  guint32 sample_duration = 0, sample_size = 0, sample_flags =
      0, composition_time_offsets = 0;
  int i = 0;

  if (!gst_byte_reader_skip (data, 1) ||
      !gst_byte_reader_get_uint24_be (data, &flags))
    return FALSE;

  GST_LOG ("%*s  flags: %08x", depth, "", flags);

  if (!gst_byte_reader_get_uint32_be (data, &samples_count))
    return FALSE;
  GST_LOG ("%*s  samples_count: %u", depth, "", samples_count);

  if (flags & TR_DATA_OFFSET) {
    if (!gst_byte_reader_get_uint32_be (data, &data_offset))
      return FALSE;
    GST_LOG ("%*s    data-offset: %u", depth, "", data_offset);
  }

  if (flags & TR_FIRST_SAMPLE_FLAGS) {
    if (!gst_byte_reader_get_uint32_be (data, &first_sample_flags))
      return FALSE;
    GST_LOG ("%*s    first-sample-flags: %u", depth, "", first_sample_flags);
  }

  for (i = 0; i < samples_count; i++) {
    if (flags & TR_SAMPLE_DURATION) {
      if (!gst_byte_reader_get_uint32_be (data, &sample_duration))
        return FALSE;
      GST_LOG ("%*s    sample-duration:  %u", depth, "", sample_duration);
    }

    if (flags & TR_SAMPLE_SIZE) {
      if (!gst_byte_reader_get_uint32_be (data, &sample_size))
        return FALSE;
      GST_LOG ("%*s    sample-size:  %u", depth, "", sample_size);
    }

    if (flags & TR_SAMPLE_FLAGS) {
      if (!gst_byte_reader_get_uint32_be (data, &sample_flags))
        return FALSE;
      GST_LOG ("%*s    sample-flags:  %u", depth, "", sample_flags);
    }

    if (flags & TR_COMPOSITION_TIME_OFFSETS) {
      if (!gst_byte_reader_get_uint32_be (data, &composition_time_offsets))
        return FALSE;
      GST_LOG ("%*s    composition_time_offsets:  %u", depth, "",
          composition_time_offsets);
    }
  }

  return TRUE;
}

gboolean
piffdemux_dump_trex (GstPiffDemux * piffdemux, GstByteReader * data, int depth)
{
  if (!piff_atom_parser_has_remaining (data, 4 + 4 + 4 + 4 + 4 + 4))
    return FALSE;

  GST_LOG ("%*s  version/flags: %08x", depth, "", GET_UINT32 (data));
  GST_LOG ("%*s  track ID:      %08x", depth, "", GET_UINT32 (data));
  GST_LOG ("%*s  default sample desc. index: %08x", depth, "",
      GET_UINT32 (data));
  GST_LOG ("%*s  default sample duration:    %08x", depth, "",
      GET_UINT32 (data));
  GST_LOG ("%*s  default sample size:        %08x", depth, "",
      GET_UINT32 (data));
  GST_LOG ("%*s  default sample flags:       %08x", depth, "",
      GET_UINT32 (data));

  return TRUE;
}


gboolean
piffdemux_dump_sdtp (GstPiffDemux * piffdemux, GstByteReader * data, int depth)
{
  guint32 version;
  guint8 val;
  guint i = 1;

  version = GET_UINT32 (data);
  GST_LOG ("%*s  version/flags: %08x", depth, "", version);

  /* the sample_count is specified in the stsz or stz2 box.
   * the information for a sample is stored in a single byte,
   * so we read until there are no remaining bytes */
  while (piff_atom_parser_has_remaining (data, 1)) {
    val = GET_UINT8 (data);
    GST_LOG ("%*s     sample number: %d", depth, "", i);
    GST_LOG ("%*s     sample_depends_on: %d", depth, "",
        ((guint16) (val)) & 0x3);
    GST_LOG ("%*s     sample_is_depended_on: %d", depth, "",
        ((guint16) (val >> 2)) & 0x3);
    GST_LOG ("%*s     sample_has_redundancy: %d", depth, "",
        ((guint16) (val >> 4)) & 0x3);
    ++i;
  }
  return TRUE;
}

gboolean
piffdemux_dump_unknown (GstPiffDemux * piffdemux, GstByteReader * data, int depth)
{
  int len;

  len = gst_byte_reader_get_remaining (data);
  GST_LOG ("%*s  length: %d", depth, "", len);

  GST_MEMDUMP_OBJECT (piffdemux, "unknown atom data",
      gst_byte_reader_peek_data_unchecked (data), len);
  return TRUE;
}

static gboolean
piffdemux_node_dump_foreach (GNode * node, gpointer piffdemux)
{
  GstByteReader parser;
  guint8 *buffer = (guint8 *) node->data;       /* FIXME: move to byte reader */
  guint32 node_length;
  guint32 fourcc;
  const PiffNodeType *type;
  int depth;

  node_length = GST_READ_UINT32_BE (buffer);
  fourcc = GST_READ_UINT32_LE (buffer + 4);

  g_warn_if_fail (node_length >= 8);

  gst_byte_reader_init (&parser, buffer + 8, node_length - 8);

  type = piffdemux_type_get (fourcc);

  depth = (g_node_depth (node) - 1) * 2;
  GST_LOG ("%*s'%" GST_FOURCC_FORMAT "', [%d], %s",
      depth, "", GST_FOURCC_ARGS (fourcc), node_length, type->name);

  if (type->dump) {
    gboolean ret;

    ret = type->dump (GST_PIFFDEMUX_CAST (piffdemux), &parser, depth);

    if (!ret) {
      GST_WARNING ("%*s  not enough data parsing atom %" GST_FOURCC_FORMAT,
          depth, "", GST_FOURCC_ARGS (fourcc));
    }
  }

  return FALSE;
}

gboolean
piffdemux_node_dump (GstPiffDemux * piffdemux, GNode * node)
{
  if (__gst_debug_min < GST_LEVEL_LOG)
    return TRUE;

  g_node_traverse (node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
      piffdemux_node_dump_foreach, piffdemux);
  return TRUE;
}
