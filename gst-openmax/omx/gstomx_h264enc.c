/*
 * Copyright (C) 2007-2009 Nokia Corporation.
 *
 * Author: Felipe Contreras <felipe.contreras@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "gstomx_h264enc.h"
#include "gstomx.h"

enum
{
  ARG_0,
  ARG_APPEND_DCI,
  ARG_BYTE_STREAM,
  ARG_SLICE_MODE,
  ARG_SLICE_SIZE,
};

GSTOMX_BOILERPLATE (GstOmxH264Enc, gst_omx_h264enc, GstOmxBaseVideoEnc,
    GST_OMX_BASE_VIDEOENC_TYPE);

/*
 *  description : convert byte-stream format to packetized frame
 *  params      : @self : GstOmxH264Enc, @buf: byte-stream buf, @sync: notify this buf is sync frame
 *  return      : none
 *  comments    :
 */
static void
convert_to_packetized_frame (GstOmxH264Enc *self, GstBuffer **buf)
{
  unsigned char *data = GST_BUFFER_DATA (*buf);
  unsigned int size = GST_BUFFER_SIZE(*buf);
  int idx = 0;
  gint start_idx = -1;
  unsigned char *nalu_start = NULL;
  GstOmxBaseFilter *omx_base = GST_OMX_BASE_FILTER(self);

  GST_LOG_OBJECT (self, "convert_to_packtized format. size=%d sliceMode=%d",
      GST_BUFFER_SIZE(*buf), self->slice_fmo.eSliceMode);

  if (omx_base->gomx->component_vendor == GOMX_VENDOR_SLSI &&
    self->slice_fmo.eSliceMode == OMX_VIDEO_SLICEMODE_AVCDefault) { /* 1 slice per frame */
    GST_LOG_OBJECT (self, " handle single NALU per buffer");
    while (idx < size - GSTOMX_H264_NAL_START_LEN) {
      if (((data[idx] == 0x00) && (data[idx+1] == 0x00) && (data[idx+2] == 0x00)&& (data[idx+3] == 0x01)) ||
        ((data[idx] == 0x00) && (data[idx+1] == 0x00) && (data[idx+2] == 0x01))) {
        if (data[idx+2] == 0x01) {
          GST_ERROR_OBJECT (self, "ERROR : NALU header is 3-bytes, yet to support !!");
        } else {
          GSTOMX_H264_WB32(data + idx, size - idx - GSTOMX_H264_NAL_START_LEN);
          return;
        }
      }
      idx++;
    } /* while */
  } else { /* handle multiple NALUs in one buffer */
    GST_LOG_OBJECT (self, " handle multiple NALUs per buffer");
    while (idx < size - GSTOMX_H264_NAL_START_LEN) {
      if (((data[idx] == 0x00) && (data[idx+1] == 0x00) && (data[idx+2] == 0x00)&& (data[idx+3] == 0x01)) ||
        ((data[idx] == 0x00) && (data[idx+1] == 0x00) && (data[idx+2] == 0x01))) {
        if (data[idx+2] == 0x01) {
          GST_ERROR_OBJECT (self, "ERROR : NALU header is 3-bytes, yet to support !!");
        } else {
          if (start_idx >= 0) {
            if (idx <= start_idx) {
              GST_ERROR_OBJECT (self, "ERROR : idx <= start_idx !!");
              return;
            }
            GST_LOG_OBJECT (self, "size of current nal unit = %d", idx-start_idx);
            GSTOMX_H264_WB32(nalu_start, idx - start_idx - GSTOMX_H264_NAL_START_LEN);
          }
          start_idx = idx;
          nalu_start = data + idx;
          idx++;
        }
      }
      idx++;
    } /* while */
    idx += GSTOMX_H264_NAL_START_LEN;

    /* converting last nal unit */
    if (start_idx >= 0) {
      GST_LOG_OBJECT (self, "size of current nal unit = %d", idx-start_idx);
      GSTOMX_H264_WB32(nalu_start, idx - start_idx - GSTOMX_H264_NAL_START_LEN);
    }
  }
}

/*
 *  description : convert byte-stream codec data to packtized codec_data
 *  params      : @self : GstOmxH264Enc, @inbuf: byte-stream codec data (omx buf), @outbuf: packetized codec_data
 *  return      : true on successes / false on failure
 *  comments    :
 */
static gboolean
convert_to_packetized_dci (GstOmxH264Enc *self, unsigned char *nalu_dci, unsigned nalu_dci_len,
    GstBuffer **packetized_dci, gint *out_sps_cnt, gint *out_pps_cnt)
{
  gint idx = 0;
  gint sps_cnt = 0;
  gint pps_cnt = 0;
  GQueue *sps_queue = 0;
  GQueue *pps_queue = 0;
  unsigned char *packet_dci = NULL;
  gint prev_nalu_start = 0;
  gint prev_nalu_type = GSTOMX_H264_NUT_UNKNOWN;
  gint sps_size = 0;
  gint pps_size = 0;
  GstBuffer *sps_data = NULL;
  GstBuffer *pps_data = NULL;
  GstBuffer *queue_data = NULL;
  gint nal_type = GSTOMX_H264_NUT_UNKNOWN;
  unsigned char profile = 0;
  unsigned char level = 0;
  unsigned char profile_comp = 0;
  gboolean bret = TRUE;
  gboolean single_sps_pps = FALSE; /* if there is only 1 sps, pps set, */

  sps_queue = g_queue_new ();
  pps_queue = g_queue_new ();

  /* find no.of SPS & PPS units */
  while (idx < nalu_dci_len) {
    if ((nalu_dci[idx] == 0x00) && (nalu_dci[idx+1] == 0x00) && (nalu_dci[idx+2] == 0x01)) {
      /* copy previous nal unit */
      if (prev_nalu_start && prev_nalu_type == GSTOMX_H264_NUT_SPS) {
        if (nalu_dci[idx -1] == 0x00) {
          sps_size = idx -1 - prev_nalu_start;
        } else {
          sps_size = idx - prev_nalu_start;
        }
        sps_data = gst_buffer_new_and_alloc (sps_size + GSTOMX_H264_C_DCI_LEN);
        if (!sps_data) {
          GST_ERROR_OBJECT (self, "failed to allocate memory..");
          bret = FALSE;
          goto exit;
        }
        GSTOMX_H264_WB16(GST_BUFFER_DATA(sps_data), sps_size);
        memcpy (GST_BUFFER_DATA(sps_data) + GSTOMX_H264_C_DCI_LEN, nalu_dci + prev_nalu_start, sps_size);
        g_queue_push_tail (sps_queue, sps_data);
      } else if (prev_nalu_start && prev_nalu_type == GSTOMX_H264_NUT_PPS) {
        if (nalu_dci[idx -1] == 0x00) {
          pps_size = idx -1 - prev_nalu_start;
        } else {
          pps_size = idx - prev_nalu_start;
        }
        pps_data = gst_buffer_new_and_alloc (pps_size + GSTOMX_H264_C_DCI_LEN);
        if (!pps_data) {
          GST_ERROR_OBJECT (self, "failed to allocate memory..");
          bret = FALSE;
          goto exit;
        }
        GSTOMX_H264_WB16(GST_BUFFER_DATA(pps_data), pps_size);
        memcpy (GST_BUFFER_DATA(pps_data) + GSTOMX_H264_C_DCI_LEN, nalu_dci + prev_nalu_start, pps_size);
        g_queue_push_tail (pps_queue, pps_data);
      }
      /* present nalu type */
      nal_type = nalu_dci[idx+3] & 0x1f;

      if (nal_type == GSTOMX_H264_NUT_SPS) {
        sps_cnt++;
        prev_nalu_start = idx + 3;
        prev_nalu_type =GSTOMX_H264_NUT_SPS;
        profile = nalu_dci[idx+4];
        level = nalu_dci[idx+6];
        GST_INFO_OBJECT (self, "Profile Number = %d and Level = %d...", nalu_dci[idx+4], level);
        GST_INFO_OBJECT (self, "Profile is %s", (profile == 66) ? "BaseLine Profile": (profile == 77)? "Main Profile": profile==88 ?
                  "Extended Profile": profile==100 ? "High Profile": "Not Supported codec");
      } else if ((nalu_dci[idx+3] & 0x1f) == GSTOMX_H264_NUT_PPS) {
        pps_cnt++;
        prev_nalu_start = idx + 3;
        prev_nalu_type = GSTOMX_H264_NUT_PPS;
      }
    }
    idx++;
  }

  /* copy previous nal unit */
  if (prev_nalu_start && prev_nalu_type == GSTOMX_H264_NUT_SPS) {
    sps_size = idx - prev_nalu_start;

    sps_data = gst_buffer_new_and_alloc (sps_size + GSTOMX_H264_C_DCI_LEN);
    if (!sps_data) {
      GST_ERROR_OBJECT (self, "failed to allocate memory..");
      bret = FALSE;
      goto exit;
    }

    GSTOMX_H264_WB16(GST_BUFFER_DATA(sps_data), sps_size);
    memcpy (GST_BUFFER_DATA(sps_data) + GSTOMX_H264_C_DCI_LEN, nalu_dci + prev_nalu_start, sps_size);
    g_queue_push_tail (sps_queue, sps_data);

  } else if (prev_nalu_start && prev_nalu_type == GSTOMX_H264_NUT_PPS) {
    pps_size = idx - prev_nalu_start;

    pps_data = gst_buffer_new_and_alloc (pps_size + GSTOMX_H264_C_DCI_LEN);
    if (!pps_data) {
      GST_ERROR_OBJECT (self, "failed to allocate memory..");
      bret = FALSE;
      goto exit;
    }

    GSTOMX_H264_WB16(GST_BUFFER_DATA(pps_data), pps_size);
    memcpy (GST_BUFFER_DATA(pps_data) + GSTOMX_H264_C_DCI_LEN, nalu_dci + prev_nalu_start, pps_size);
    g_queue_push_tail (pps_queue, pps_data);
  }

  /* make packetized codec data */
  if (sps_cnt == 1 && pps_cnt == 1) {
    single_sps_pps = TRUE;
    *packetized_dci = gst_buffer_new_and_alloc(GSTOMX_H264_MDATA + GSTOMX_H264_C_DCI_LEN + sps_size + GSTOMX_H264_CNT_LEN + GSTOMX_H264_C_DCI_LEN + pps_size);
    GST_LOG_OBJECT(self, "allocate packetized_dci in case of single sps, pps. (size=%d)",  GST_BUFFER_SIZE(*packetized_dci));
  } else {
    *packetized_dci = gst_buffer_new_and_alloc(GSTOMX_H264_MDATA);
    GST_LOG_OBJECT(self, "allocate packetized_dci in case of multi sps, pps");
  }

  if (*packetized_dci == NULL) {
    GST_ERROR_OBJECT (self, "Failed to allocate memory..");
    bret = FALSE;
    goto exit;
  }

  packet_dci = GST_BUFFER_DATA(*packetized_dci);
  packet_dci[0] = 0x01; /* configurationVersion */
  packet_dci[1] = profile; /* AVCProfileIndication */
  packet_dci[2] = profile_comp; /* profile_compatibility*/
  packet_dci[3] = level;  /* AVCLevelIndication */
  packet_dci[4] = 0xff; /* Reserver (6bits.111111) + LengthSizeMinusOne (2bits). lengthoflength = 4 for present */
  packet_dci[5] = 0xe0 | sps_cnt; /* Reserver (3bits. 111) + numOfSequenceParameterSets (5bits) */

  /* copy SPS sets */
  while (!g_queue_is_empty (sps_queue)) {
    sps_data = g_queue_pop_head (sps_queue);

    if (TRUE == single_sps_pps) {
      memcpy(packet_dci + GSTOMX_H264_MDATA, GST_BUFFER_DATA(sps_data), GST_BUFFER_SIZE(sps_data));
      gst_buffer_unref(sps_data);
      sps_data = NULL;
    } else {
      *packetized_dci = gst_buffer_join(*packetized_dci, sps_data);
    }
  }

  /* add number of PPS sets (1byte)*/
  if (TRUE == single_sps_pps) {
    packet_dci[GSTOMX_H264_MDATA + GSTOMX_H264_C_DCI_LEN + sps_size] = pps_cnt;
  } else {
    GstBuffer *next_data = gst_buffer_new_and_alloc(GSTOMX_H264_CNT_LEN);
    if (!next_data) {
      GST_ERROR_OBJECT (self, "Failed to allocate memory..");
      bret = FALSE;
      goto exit;
    }
    GST_BUFFER_DATA(next_data)[0] = pps_cnt;
    *packetized_dci = gst_buffer_join(*packetized_dci, next_data);
  }

  /* copy PPS sets */
  while (!g_queue_is_empty (pps_queue)) {
    pps_data = g_queue_pop_head (pps_queue);

    if (TRUE == single_sps_pps) {
      memcpy(packet_dci + GSTOMX_H264_MDATA + GSTOMX_H264_C_DCI_LEN + sps_size + GSTOMX_H264_CNT_LEN,
                      GST_BUFFER_DATA(pps_data), GST_BUFFER_SIZE(pps_data));
      gst_buffer_unref(pps_data);
      pps_data = NULL;
    } else {
      *packetized_dci = gst_buffer_join(*packetized_dci, pps_data);
    }
  }

exit:
  while (!g_queue_is_empty (sps_queue)) {
    queue_data = g_queue_pop_head (sps_queue);
    gst_buffer_unref (queue_data);
    queue_data = NULL;
  }
  g_queue_free (sps_queue);

  while (!g_queue_is_empty (pps_queue)) {
    queue_data = g_queue_pop_head (pps_queue);
    gst_buffer_unref (queue_data);
    queue_data = NULL;
  }
  g_queue_free (pps_queue);

  *out_sps_cnt = sps_cnt;
  *out_pps_cnt = sps_cnt;

  return bret;
}

/*
 *  description : resotre packtized dci (codec_data)
 *  params      : @self : GstOmxH264Enc, @inbuf: codec data, @outbuf: h264enc->dci
 *  return      : none
 *  comments    :
 *    from original packetized codec_data: METADATA(6) + SPS_CNT(0) + {SPS_SIZE(2)+SPS_DATA(sps_size)}*n +
                                                             PPS_CNT(1) + {PPS_SIZE(2)+PPS_DATA(pps_size)}*n
 *    to estore packetized dci: {SPS_SIZE(4)+SPS_DATA(sps_size)}*n + {PPS_SIZE(4)+PPS_DATA(pps_size)}*n
 */
static gboolean
restore_packetized_dci (GstOmxH264Enc *self, GstBuffer *inbuf, GstBuffer **outbuf, gint sps_cnt, gint pps_cnt)
{
  unsigned char *indata = GST_BUFFER_DATA(inbuf);
  guint sps_size =0;
  guint pps_size =0;
  gint i = 0;
  GstBuffer *next_data = NULL;

  GST_INFO_OBJECT (self, "restore_packetized_dci. sps_cnt=%d, pps_cnt=%d", sps_cnt, pps_cnt);

  if (sps_cnt == 1 && pps_cnt == 1) {
    sps_size = GSTOMX_H264_RB16(indata + GSTOMX_H264_MDATA);
    pps_size = GSTOMX_H264_RB16(indata + GSTOMX_H264_MDATA + GSTOMX_H264_C_DCI_LEN + sps_size + GSTOMX_H264_CNT_LEN);

    *outbuf = gst_buffer_new_and_alloc (GSTOMX_H264_A_DCI_LEN + sps_size + GSTOMX_H264_A_DCI_LEN + pps_size);
    if (!*outbuf) {
      GST_ERROR_OBJECT (self, "Failed to allocate memory in gst_buffer_new_and_alloc.");
      goto error_exit;
    }

    GSTOMX_H264_WB32(GST_BUFFER_DATA(*outbuf), sps_size);
    indata += GSTOMX_H264_MDATA + GSTOMX_H264_C_DCI_LEN;
    memcpy (GST_BUFFER_DATA(*outbuf) + GSTOMX_H264_A_DCI_LEN, indata, sps_size);
    indata += sps_size;

    GSTOMX_H264_WB32(GST_BUFFER_DATA(*outbuf) + GSTOMX_H264_A_DCI_LEN + sps_size, pps_size);
    indata += GSTOMX_H264_CNT_LEN + GSTOMX_H264_C_DCI_LEN;
    memcpy (GST_BUFFER_DATA(*outbuf) + GSTOMX_H264_A_DCI_LEN + sps_size + GSTOMX_H264_A_DCI_LEN, indata, pps_size);
    indata += pps_size;
  } else {
    /* in this case, dci has multi nalu. ex) sps + sps + sps + pps + pps */
    indata += GSTOMX_H264_MDATA;
    *outbuf = gst_buffer_new ();

    for (i = 0; i < sps_cnt; i++) {
      sps_size = GSTOMX_H264_RB16(indata); /* metadata(6) */

      next_data = gst_buffer_new_and_alloc(GSTOMX_H264_A_DCI_LEN + sps_size);
      if (!next_data) {
        GST_ERROR_OBJECT (self, "Failed to allocate memory in gst_buffer_new_and_alloc.");
        goto error_exit;
      }
      GSTOMX_H264_WB32(GST_BUFFER_DATA(next_data), sps_size);
      indata += GSTOMX_H264_C_DCI_LEN; /* sps size field (2 byte) */
      memcpy (GST_BUFFER_DATA(next_data) + GSTOMX_H264_A_DCI_LEN, indata, sps_size);

      *outbuf = gst_buffer_join(*outbuf, next_data);
      indata += sps_size;
    }
    indata += GSTOMX_H264_CNT_LEN; /* pps cnt field (1 byte) */

    for (i = 0; i < pps_cnt; i++) {
      pps_size = GSTOMX_H264_RB16(indata);

      next_data = gst_buffer_new_and_alloc(GSTOMX_H264_A_DCI_LEN + pps_size);
      if (!next_data) {
        GST_ERROR_OBJECT (self, "Failed to allocate memory in gst_buffer_new_and_alloc.");
        goto error_exit;
      }
      GSTOMX_H264_WB32(GST_BUFFER_DATA(next_data), pps_size);
      indata += GSTOMX_H264_C_DCI_LEN;
      memcpy (GST_BUFFER_DATA(next_data) + GSTOMX_H264_A_DCI_LEN, indata, pps_size);

      *outbuf = gst_buffer_join(*outbuf, next_data);
      indata += pps_size;
    }
  }

  return TRUE;

error_exit:
  if (*outbuf) {
    gst_buffer_unref(*outbuf);
    *outbuf = NULL;
  }
  return FALSE;
}

/*
 *  description : set sync frame. if needed, convert output to packetized format, append dci every I frame.
 *  params      : @self : GstOmxBaseFilter, @buf: encoder output frame, @omx_buffer: omx_buffer
 *  return      : none
 *  comments    :
 */
static GstOmxReturn
process_output_buf(GstOmxBaseFilter * omx_base, GstBuffer **buf, OMX_BUFFERHEADERTYPE *omx_buffer)
{
  GstOmxH264Enc *self;
  self = GST_OMX_H264ENC (omx_base);

  if (!self->byte_stream) { /* Packtized Format */
    convert_to_packetized_frame (self, buf); /* convert byte stream to packetized stream */
    GST_LOG_OBJECT (self, "output buffer is converted to Packtized format.");
  } else {
    GST_LOG_OBJECT (self, "output buffer is Byte-stream format.");
  }

  if ((self->first_frame) ||(self->append_dci && omx_buffer->nFlags & OMX_BUFFERFLAG_SYNCFRAME)) {
    GstBuffer *newbuf = NULL;
    GST_LOG_OBJECT (self, "append dci at %s by gst-openmax.", (self->first_frame == TRUE) ? "first frame": "every I frame");

    if (self->dci == NULL) {
      GST_ERROR_OBJECT (self, "dci is null. can not append dci.");
      self->append_dci = FALSE;
    } else {
      newbuf = gst_buffer_merge (self->dci, *buf);
      if (newbuf == NULL) {
        GST_ERROR_OBJECT (self, "Failed to gst_buffer_merge.");
        return GSTOMX_RETURN_ERROR;
      }
      gst_buffer_copy_metadata(newbuf, *buf, GST_BUFFER_COPY_ALL);
      gst_buffer_unref(*buf);
      *buf = newbuf;
      }
    }

  if (self->first_frame == TRUE)
    self->first_frame = FALSE;

  /* Set sync frame info while encoding */
  if (omx_buffer->nFlags & OMX_BUFFERFLAG_SYNCFRAME) {
    GST_BUFFER_FLAG_UNSET(*buf, GST_BUFFER_FLAG_DELTA_UNIT);
  } else {
    GST_BUFFER_FLAG_SET(*buf, GST_BUFFER_FLAG_DELTA_UNIT);
  }

  return GSTOMX_RETURN_OK;
}

/*
 *  description : if needed, convert byte-stream codec_data to packetized format, save dci.
 *  params      : @self : GstOmxBaseFilter, @omx_buffer: omx_buffer
 *  return      : none
 *  comments    :
 */
static void
process_output_caps(GstOmxBaseFilter * base, OMX_BUFFERHEADERTYPE *omx_buffer)
{
  GstOmxH264Enc *h264enc;
  gboolean ret = FALSE;

  h264enc = GST_OMX_H264ENC (base);

  if (!h264enc->byte_stream) { /* Packtized Format */
    GstCaps *caps = NULL;
    GstStructure *structure;
    GstBuffer *codec_data = NULL;
    gint sps_cnt = 0;
    gint pps_cnt = 0;
    GValue value = { 0, {{0}
        }
    };
    g_value_init (&value, GST_TYPE_BUFFER);

    GST_INFO_OBJECT (h264enc, "Packtized Format: set src caps with codec-data");

    /* convert codec_data to packtized format.. (metadata(cnt) + sps_size + sps + cnt + pps_size + pps) */
    ret = convert_to_packetized_dci (h264enc, omx_buffer->pBuffer + omx_buffer->nOffset, omx_buffer->nFilledLen,
        &codec_data, &sps_cnt, &pps_cnt);

    if (FALSE == ret) {
      GST_ERROR_OBJECT (h264enc, "ERROR: convert to packetized dci");
      if (codec_data) {
        gst_buffer_unref(codec_data);
        codec_data = NULL;
      }
      return;
    }

    /* restore packtized format sps, pps */
    ret = restore_packetized_dci (h264enc, codec_data, &h264enc->dci, sps_cnt, pps_cnt);
    if (ret == FALSE) {
      GST_ERROR_OBJECT (h264enc, "ERROR: restore packetized dci");
      return;
    }

    GST_DEBUG_OBJECT (h264enc, "adding codec_data in caps");
    caps = gst_pad_get_negotiated_caps (base->srcpad);
    caps = gst_caps_make_writable (caps);
    structure = gst_caps_get_structure (caps, 0);

    /* packetized format, set codec_data field in caps */
    gst_value_set_buffer (&value,codec_data);
    gst_buffer_unref (codec_data);
    codec_data = NULL;
    gst_structure_set_value (structure, "codec_data", &value);
    g_value_unset (&value);

    gst_pad_set_caps (base->srcpad, caps);
    gst_caps_unref (caps);
  } else {
    /* byte-stream format */
    GST_INFO_OBJECT (h264enc, "Byte-stream Format");
    h264enc->dci = gst_buffer_new_and_alloc (omx_buffer->nFilledLen);
    if (!h264enc->dci) {
      GST_ERROR_OBJECT (h264enc, "failed to allocate memory...");
      return;
    }
    memcpy (GST_BUFFER_DATA (h264enc->dci),
        omx_buffer->pBuffer + omx_buffer->nOffset, omx_buffer->nFilledLen);
  }
}

static void
finalize (GObject * obj)
{
  GstOmxH264Enc *h264enc;
  h264enc = GST_OMX_H264ENC (obj);

  GST_LOG_OBJECT (h264enc, "h264enc finalize enter");

  if (h264enc->dci) {
    gst_buffer_unref(h264enc->dci);
    h264enc->dci = NULL;
  }
  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
type_base_init (gpointer g_class)
{
  GstElementClass *element_class;

  element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (element_class,
      "OpenMAX IL H.264/AVC video encoder",
      "Codec/Encoder/Video",
      "Encodes video in H.264/AVC format with OpenMAX IL", "Felipe Contreras");

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gstomx_template_caps (G_TYPE_FROM_CLASS (g_class), "sink")));

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gstomx_template_caps (G_TYPE_FROM_CLASS (g_class), "src")));
}

static void
set_property (GObject * obj,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOmxH264Enc *self;
  GstOmxBaseFilter *omx_base;

  self = GST_OMX_H264ENC (obj);
  omx_base = GST_OMX_BASE_FILTER (obj);

  switch (prop_id) {
    case ARG_APPEND_DCI:
      self->append_dci = g_value_get_boolean (value);
      GST_INFO_OBJECT (self, "set Appending dci = %d", self->append_dci);
      break;

    case ARG_BYTE_STREAM:
      self->byte_stream = g_value_get_boolean (value);
      GST_INFO_OBJECT (self, "set Byte stream = %d", self->byte_stream);
      break;

    case ARG_SLICE_MODE:
      {
        OMX_VIDEO_PARAM_AVCSLICEFMO param;
        OMX_ERRORTYPE ret;

        self->slice_fmo.eSliceMode = g_value_get_uint (value);

        G_OMX_INIT_PARAM (param);
        param.nPortIndex = omx_base->out_port->port_index;
        OMX_GetParameter (omx_base->gomx->omx_handle, OMX_IndexParamVideoSliceFMO, &param);

        param.eSliceMode = self->slice_fmo.eSliceMode;
        param.nNumSliceGroups = 1;
        param.nSliceGroupMapType = 1;
        ret = OMX_SetParameter (omx_base->gomx->omx_handle, OMX_IndexParamVideoSliceFMO, &param);
        if (ret != OMX_ErrorNone)
          GST_ERROR_OBJECT (self, "failed to set eSliceMode = %d", self->slice_fmo.eSliceMode);
        else
          GST_INFO_OBJECT (self, "set eSliceMode = %d", self->slice_fmo.eSliceMode);
      }
      break;

    case ARG_SLICE_SIZE:
      {
        OMX_VIDEO_PARAM_AVCTYPE param;
        OMX_ERRORTYPE ret;

        self->h264type.nSliceHeaderSpacing = g_value_get_uint (value);

        G_OMX_INIT_PARAM (param);
        param.nPortIndex = omx_base->out_port->port_index;
        OMX_GetParameter (omx_base->gomx->omx_handle, OMX_IndexParamVideoAvc, &param);

        param.nSliceHeaderSpacing = self->h264type.nSliceHeaderSpacing;
        ret= OMX_SetParameter (omx_base->gomx->omx_handle, OMX_IndexParamVideoAvc, &param);
        if (ret != OMX_ErrorNone)
          GST_ERROR_OBJECT (self, "failed to set nSliceHeaderSpacing = %d", self->h264type.nSliceHeaderSpacing);
        else
          GST_INFO_OBJECT (self, "nSliceHeaderSpacing = %d", self->h264type.nSliceHeaderSpacing);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static void
get_property (GObject * obj, guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOmxH264Enc *self;

  self = GST_OMX_H264ENC (obj);

  switch (prop_id) {
    case ARG_APPEND_DCI:
      g_value_set_boolean (value, self->append_dci);
      break;
    case ARG_BYTE_STREAM:
      g_value_set_boolean (value, self->byte_stream);
      break;
    case ARG_SLICE_MODE:
      g_value_set_int (value, self->slice_fmo.eSliceMode);
      break;
    case ARG_SLICE_SIZE:
      g_value_set_uint (value, self->h264type.nSliceHeaderSpacing);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static void
type_class_init (gpointer g_class, gpointer class_data)
{
  GObjectClass *gobject_class;
  GstOmxBaseFilterClass *basefilter_class;

  gobject_class = G_OBJECT_CLASS (g_class);
  basefilter_class = GST_OMX_BASE_FILTER_CLASS (g_class);

  {
    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;

    g_object_class_install_property (gobject_class, ARG_APPEND_DCI,
        g_param_spec_boolean ("append-dci", "append codec_data with every key frame for byte-stream format",
            "append codec_data with every key frame for byte-stream format",
            FALSE,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, ARG_BYTE_STREAM,
        g_param_spec_boolean ("byte-stream", "Output stream format",
            "output stream format whether byte-stream format (TRUE) or packetized (FALSE)",
            FALSE,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, ARG_SLICE_MODE,
        g_param_spec_int ("slice-mode", "H264 encoder slice mode",
            "slice mode: 0-Disable, 1-Fixed MB num slice, 2-Fixed bit num slice",
            0, 4, 0,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (gobject_class, ARG_SLICE_SIZE,
        g_param_spec_uint ("slice-size", "H264 encoder slice size",
            "MB or bit num: MB number:1 ~ (MBCnt-1), Bit number: 1900 (bit) ~",
            0, G_MAXUINT, 0,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }

  basefilter_class->process_output_buf = process_output_buf;
  basefilter_class->process_output_caps = process_output_caps;

  gobject_class->finalize = finalize;

}

static void
settings_changed_cb (GOmxCore * core)
{
  GstOmxBaseVideoEnc *omx_base;
  GstOmxBaseFilter *omx_base_filter;
  GstOmxH264Enc *self;

  guint width;
  guint height;

  omx_base_filter = core->object;
  omx_base = GST_OMX_BASE_VIDEOENC (omx_base_filter);
  self = GST_OMX_H264ENC (omx_base_filter);

  GST_DEBUG_OBJECT (omx_base, "settings changed");

  {
    OMX_PARAM_PORTDEFINITIONTYPE param;

    G_OMX_INIT_PARAM (param);

    param.nPortIndex = omx_base_filter->out_port->port_index;
    OMX_GetParameter (core->omx_handle, OMX_IndexParamPortDefinition, &param);

    width = param.format.video.nFrameWidth;
    height = param.format.video.nFrameHeight;
  }

  {
    GstCaps *new_caps, *peer_caps;
    gchar *format ;
    gchar *src_caps_to_str = NULL;
    int i =0;

    new_caps = gst_caps_new_simple ("video/x-h264",
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        "framerate", GST_TYPE_FRACTION,
        omx_base->framerate_num, omx_base->framerate_denom, NULL);

    /* get peer pad caps */
    peer_caps = gst_pad_peer_get_caps(omx_base_filter->srcpad);
    if (peer_caps) {
	src_caps_to_str = gst_caps_to_string(peer_caps);
       GST_LOG_OBJECT (omx_base, "peer caps : %s\n", src_caps_to_str);
	 if (src_caps_to_str)
        	g_free (src_caps_to_str);

      for (i = 0; i < peer_caps->structs->len; i++) {
        GstStructure *s;
        s = gst_caps_get_structure (peer_caps, i);

        if (g_strrstr (gst_structure_get_name (s), "video/x-h264")) {
          if (!gst_structure_get (s, "stream-format", G_TYPE_STRING, &format, NULL)) {
            GST_WARNING_OBJECT (self, "Failed to get stream-format from peer caps...");
          } else {
            GST_INFO_OBJECT (self, "format : %s", format);
            gst_caps_set_simple (new_caps, "stream-format", G_TYPE_STRING, format, NULL);
            if (g_strrstr(format, "byte-stream")) {
              GST_INFO_OBJECT (self, "Mandatory BYTE_STREAM");
              self->byte_stream = TRUE;
            } else if (g_strrstr(format, "avc")){
              GST_INFO_OBJECT (self, "Mandatory PACKETIZED");
              self->byte_stream = FALSE;
            } else {
              GST_INFO_OBJECT (self, "Nothing mentioned about stream-format... use byte_stream property to decide");
              if (self->byte_stream) {
                GST_INFO_OBJECT (self, "Is in BYTE_STREAM");
              } else {
                GST_INFO_OBJECT (self, "Is in PACKETIZED");
              }
            }
          }
        }
      }
    }

    gst_caps_unref(peer_caps);
    GST_INFO_OBJECT (omx_base, "caps are: %" GST_PTR_FORMAT, new_caps);
    gst_pad_set_caps (omx_base_filter->srcpad, new_caps);
  }
}

static void
type_instance_init (GTypeInstance * instance, gpointer g_class)
{
  GstOmxBaseFilter *omx_base_filter;
  GstOmxBaseVideoEnc *omx_base;
  GstOmxH264Enc *self;

  omx_base_filter = GST_OMX_BASE_FILTER (instance);
  omx_base = GST_OMX_BASE_VIDEOENC (instance);
  self = GST_OMX_H264ENC (instance);

  self->byte_stream = FALSE;
  self->append_dci = FALSE;
  self->first_frame = TRUE;
  self->dci = NULL;
  self->slice_fmo.eSliceMode = OMX_VIDEO_SLICEMODE_AVCLevelMax;

  omx_base->compression_format = OMX_VIDEO_CodingAVC;

  omx_base_filter->gomx->settings_changed_cb = settings_changed_cb;
}
