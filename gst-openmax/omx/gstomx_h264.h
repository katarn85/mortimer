/*
 * Copyright (C) 2012 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Author: Sunghyun Eum <sunghyun.eum@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef GSTOMX_H264_H
#define GSTOMX_H264_H

G_BEGIN_DECLS

/* modification: byte-stream(nal) - packetized (3gpp) converting (h264dec, h264enc) */
#define GSTOMX_H264_MDATA 6 /* packtized meta data size. (ver, profile...) */
#define GSTOMX_H264_A_DCI_LEN 4 /* 4-byte sps, pps size field in append dci*/
#define GSTOMX_H264_C_DCI_LEN 2 /* 2-byte sps, pps size field in codec data*/
#define GSTOMX_H264_CNT_LEN 1 /* 1-byte sps, pps count field */

#define GSTOMX_H264_NAL_START_LEN 4
#define GSTOMX_H264_SPSPPS_LEN 2

typedef enum
{
    GSTOMX_H264_NUT_UNKNOWN = 0,
    GSTOMX_H264_NUT_SLICE = 1,
    GSTOMX_H264_NUT_DPA = 2,
    GSTOMX_H264_NUT_DPB = 3,
    GSTOMX_H264_NUT_DPC = 4,
    GSTOMX_H264_NUT_IDR = 5,
    GSTOMX_H264_NUT_SEI = 6,
    GSTOMX_H264_NUT_SPS = 7,
    GSTOMX_H264_NUT_PPS = 8,
    GSTOMX_H264_NUT_AUD = 9,
    GSTOMX_H264_NUT_EOSEQ = 10,
    GSTOMX_H264_NUT_EOSTREAM = 11,
    GSTOMX_H264_NUT_FILL = 12,
    GSTOMX_H264_NUT_MIXED = 24,
} GSTOMX_H264_NAL_UNIT_TYPE;

typedef enum
{
    GSTOMX_H264_FORMAT_UNKNOWN,
    GSTOMX_H264_FORMAT_PACKETIZED,
    GSTOMX_H264_FORMAT_BYTE_STREAM,
} GSTOMX_H264_STREAM_FORMAT;

#define GSTOMX_H264_WB32(p, d) \
    do { \
        ((unsigned char*)(p))[3] = (d); \
        ((unsigned char*)(p))[2] = (d)>>8; \
        ((unsigned char*)(p))[1] = (d)>>16; \
        ((unsigned char*)(p))[0] = (d)>>24; \
    } while(0);

#define GSTOMX_H264_WB16(p, d) \
    do { \
        ((unsigned char*)(p))[1] = (d); \
        ((unsigned char*)(p))[0] = (d)>>8; \
    } while(0);

#define GSTOMX_H264_RB16(x) ((((const unsigned char*)(x))[0] << 8) | ((const unsigned char*)(x))[1])

#define GSTOMX_H264_RB32(x) ((((const unsigned char*)(x))[0] << 24) | \
                                   (((const unsigned char*)(x))[1] << 16) | \
                                   (((const unsigned char*)(x))[2] <<  8) | \
                                   ((const unsigned char*)(x))[3])

G_END_DECLS
#endif /* GSTOMX_H264_H */
