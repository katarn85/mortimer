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

#ifndef __GST_QTDEMUX_FOURCC_H__
#define __GST_QTDEMUX_FOURCC_H__

#include <gst/gst.h>

G_BEGIN_DECLS


#define FOURCC_vide     GST_MAKE_FOURCC('v','i','d','e')
#define FOURCC_soun     GST_MAKE_FOURCC('s','o','u','n')
#define FOURCC_subp     GST_MAKE_FOURCC('s','u','b','p')
#define FOURCC_hint     GST_MAKE_FOURCC('h','i','n','t')
#define FOURCC_mp4a     GST_MAKE_FOURCC('m','p','4','a')
#define FOURCC_mp4v     GST_MAKE_FOURCC('m','p','4','v')
#define FOURCC_MP4V     GST_MAKE_FOURCC('M','P','4','V')
#define FOURCC_fmp4     GST_MAKE_FOURCC('f','m','p','4')
#define FOURCC_FMP4     GST_MAKE_FOURCC('F','M','P','4')

#define FOURCC_meta     GST_MAKE_FOURCC('m','e','t','a')

#define FOURCC_____     GST_MAKE_FOURCC('-','-','-','-')

#define FOURCC_free     GST_MAKE_FOURCC('f','r','e','e')

#define FOURCC_drms     GST_MAKE_FOURCC('d','r','m','s')
#define FOURCC_drmi     GST_MAKE_FOURCC('d','r','m','i')
#define FOURCC_avc1     GST_MAKE_FOURCC('a','v','c','1')
#define FOURCC_avcC     GST_MAKE_FOURCC('a','v','c','C')

#define FOURCC_ulaw     GST_MAKE_FOURCC('u','l','a','w')
#define FOURCC_alaw     GST_MAKE_FOURCC('a','l','a','w')

#define FOURCC_raw_     GST_MAKE_FOURCC('r','a','w',' ')

#define FOURCC_alac     GST_MAKE_FOURCC('a','l','a','c')
#define FOURCC_samr     GST_MAKE_FOURCC('s','a','m','r')
#define FOURCC_sawb     GST_MAKE_FOURCC('s','a','w','b')
#define FOURCC_mdat     GST_MAKE_FOURCC('m','d','a','t')
#define FOURCC_in24     GST_MAKE_FOURCC('i','n','2','4')

#define FOURCC_text     GST_MAKE_FOURCC('t','e','x','t')
#define FOURCC_tx3g     GST_MAKE_FOURCC('t','x','3','g')
#define FOURCC_mp4s     GST_MAKE_FOURCC('m','p','4','s')
#define FOURCC_uuid     GST_MAKE_FOURCC('u','u','i','d')

/* Fragmented MP4 */

#define FOURCC_mfhd     GST_MAKE_FOURCC('m','f','h','d')
#define FOURCC_mfra     GST_MAKE_FOURCC('m','f','r','a')
#define FOURCC_mfro     GST_MAKE_FOURCC('m','f','r','o')
#define FOURCC_moof     GST_MAKE_FOURCC('m','o','o','f')
#define FOURCC_mvex     GST_MAKE_FOURCC('m','v','e','x')
#define FOURCC_sdtp     GST_MAKE_FOURCC('s','d','t','p')
#define FOURCC_tfhd     GST_MAKE_FOURCC('t','f','h','d')
#define FOURCC_tfxd     GST_MAKE_FOURCC('t','f','x','d')
#define FOURCC_tfra     GST_MAKE_FOURCC('t','f','r','a')
#define FOURCC_traf     GST_MAKE_FOURCC('t','r','a','f')
#define FOURCC_trex     GST_MAKE_FOURCC('t','r','e','x')
#define FOURCC_trun     GST_MAKE_FOURCC('t','r','u','n')
#define FOURCC_ovc1     GST_MAKE_FOURCC('o','v','c','1')
#define FOURCC_owma     GST_MAKE_FOURCC('o','w','m','a')
#define FOURCC_uuid     GST_MAKE_FOURCC('u','u','i','d')
#define FOURCC_tfrf    GST_MAKE_FOURCC('t','f','r','f')

G_END_DECLS

#endif /* __GST_QTDEMUX_FOURCC_H__ */
