/*
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Author: Rob Clark <rob@ti.com>
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

#include "gstomx_base_audiodec.h"
#include "gstomx.h"

#include <audio-session-manager.h>

enum
{
  ARG_0,
  ARG_USE_STATETUNING, /* STATE_TUNING */
};

GSTOMX_BOILERPLATE (GstOmxBaseAudioDec, gst_omx_base_audiodec, GstOmxBaseFilter,
    GST_OMX_BASE_FILTER_TYPE);

static void
type_base_init (gpointer g_class)
{
}

/* MODIFICATION: add state tuning property */
static void
set_property (GObject * obj,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOmxBaseAudioDec *self;
  GstOmxBaseFilter *basefilter;

  self = GST_OMX_BASE_AUDIODEC (obj);
  basefilter = GST_OMX_BASE_FILTER (obj);

  switch (prop_id) {
    /* STATE_TUNING */
    case ARG_USE_STATETUNING:
      self->omx_base.use_state_tuning = g_value_get_boolean(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static void
get_property (GObject * obj, guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOmxBaseAudioDec *self;

  self = GST_OMX_BASE_AUDIODEC (obj);

  switch (prop_id) {
    /* STATE_TUNING */
    case ARG_USE_STATETUNING:
      g_value_set_boolean(value, self->omx_base.use_state_tuning);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

/*
 *  description : Parse the extradata and store into GstBuffer.qdata
 *  params      : @self : GstOmxBaseFilter, @buf: decoder input frame, @omx_buffer: omx_buffer
 *  return      : none
 *  comments    :
 */
static GstOmxReturn
process_audio_input_buf(GstOmxBaseFilter * omx_base, GstBuffer **buf, OMX_BUFFERHEADERTYPE *omx_buffer)
{
  GstOmxBaseAudioDec *self = NULL;
  GST_DEBUG_OBJECT(omx_base, "process_audio_input_buf");
  self = GST_OMX_BASE_AUDIODEC (omx_base);

  if(omx_base){
	omx_base->iNumberOfnotAudioFrameDone++;
	GST_DEBUG_OBJECT(omx_base, "OMX_A_OUTPUT, INPUT not output audio frame[%d] !!!", omx_base->iNumberOfnotAudioFrameDone);
	if(omx_base->iNumberOfnotAudioFrameDone > OMX_NO_OUTPUT_FRAME_DONE){
		GST_ERROR_OBJECT(omx_base, "OMX_A_OUTPUT, not output audio frame > OMX_NO_OUTPUT_FRAME_DONE !!!");
		return GSTOMX_RETURN_ERROR;
	}
  }

  return GSTOMX_RETURN_OK;
}

/*
 *  description : Parse the extradata and store into GstBuffer.qdata
 *  params      : @self : GstOmxBaseFilter, @buf: decoder output frame, @omx_buffer: omx_buffer
 *  return      : none
 *  comments    :
 */
static GstOmxReturn
process_audio_output_buf(GstOmxBaseFilter * omx_base, GstBuffer **buf, OMX_BUFFERHEADERTYPE *omx_buffer)
{
  GstOmxBaseAudioDec *self = NULL;
  OMX_OTHER_EXTRADATATYPE *pExtra = NULL; // extradata is aligned to 4byte.
  GstStructure *subDataList = NULL;
  
  if(omx_base != NULL)
  {
	omx_base->iNumberOfnotAudioFrameDone = 0;
	GST_DEBUG_OBJECT(omx_base, "OMX_A_OUTPUT, OUTPUT not output audio frame[%d] !!!", omx_base->iNumberOfnotAudioFrameDone);
  }
  else
  {
	GST_ERROR_OBJECT(omx_base, "omx_base is NULL");
  	return GSTOMX_RETURN_ERROR;
  }
  
  if ((buf == NULL) || (omx_buffer->pBuffer == NULL) ||
    (omx_buffer->nAllocLen < (omx_buffer->nOffset + omx_buffer->nFilledLen + sizeof(OMX_OTHER_EXTRADATATYPE))))
  {
    GST_DEBUG_OBJECT(omx_base, "buf[ %p ], allocLen[ %d ], offset[ %d ], filledLen[ %d ], sizeof[ %d ]",
    omx_buffer->pBuffer, omx_buffer->nAllocLen , omx_buffer->nOffset , omx_buffer->nFilledLen, sizeof(OMX_OTHER_EXTRADATATYPE));
    return GSTOMX_RETURN_ERROR; // No need to skip
  }

  OMX_U8 *pTmp = omx_buffer->pBuffer + omx_buffer->nOffset + omx_buffer->nFilledLen + 3; // +3 for 4byte alignment
  pExtra = (OMX_OTHER_EXTRADATATYPE *) (((OMX_U32) pTmp) & ~3);
  self = GST_OMX_BASE_AUDIODEC (omx_base);


/*  TODO : If lpcm decoder return big endian data it should be enabled. ( current HawkP decoder always return little endian data 2015.02.03)
  gint16* ptr = GST_BUFFER_DATA(*buf);
  if(self->is_tz_lpcm_be16)  //big to little for screen mirroring lpcm
  {
    guint i;
    GST_DEBUG_OBJECT (self, "swapping bytes");
    for (i = 0; i < omx_buffer->nFilledLen / 2; i++) {
      ptr[i] = GUINT16_SWAP_LE_BE (ptr[i]);
    }
  }
*/
  
  int remain = omx_buffer->nAllocLen - omx_buffer->nOffset + omx_buffer->nFilledLen;
  GST_DEBUG_OBJECT(omx_base, "buf[ %p ] nSize[ %d ], nVersion[ %d ], nPortIdx[ %d ], eType[ %d ], nDataSize[ %d ] data[ %p ]",
      buf, pExtra->nSize, pExtra->nVersion, pExtra->nPortIndex, pExtra->eType, pExtra->nDataSize, pExtra->data);
  while((pExtra->eType != OMX_ExtraDataNone) && pExtra->nSize > 0 && pExtra->nDataSize > 0 && remain > 0)
  {
    /* Detect extra data */
    GstCaps* caps = NULL;
    const char* fieldName = NULL;
    if (OMX_ExtraDataPostProcessedPCM == pExtra->eType)
    {
      caps = gst_caps_copy(GST_BUFFER_CAPS(*buf));
      gst_caps_set_simple(caps, "postprocessed", G_TYPE_BOOLEAN, TRUE, NULL);
      fieldName = "subpcm";
    }
    else if (OMX_ExtraDataMixedCompressedES == pExtra->eType)
    {
      caps = gst_caps_new_simple ("audio/x-spdif-es", NULL);
      fieldName = "spdif_es";
    }
    else
      GST_WARNING_OBJECT(omx_base, "Unknown eType[ %d ]", pExtra->eType);

    GST_LOG_OBJECT(omx_base, "caps[ %"GST_PTR_FORMAT" ] eType[ %d ] dataSize[ %d  ]   nSize[ %d / %d ] fieldName[ %s ]",
        caps, pExtra->eType, pExtra->nDataSize, pExtra->nSize, remain, fieldName);

    /* Create new GstBuffer and store it into parent the GstStructure */
    if (caps)
    {
      GstBuffer* subBuffer = gst_buffer_new_and_alloc (pExtra->nDataSize); // ref count 1
      if (G_LIKELY (subBuffer))
      {
        memcpy (GST_BUFFER_DATA (subBuffer), pExtra->data, pExtra->nDataSize);
        if (omx_base->use_timestamps)
          GST_BUFFER_TIMESTAMP (subBuffer) = gst_util_uint64_scale_int (omx_buffer->nTimeStamp, GST_SECOND, OMX_TICKS_PER_SECOND);
        gst_buffer_set_caps (subBuffer, caps);
        if (subDataList == NULL)
          subDataList = gst_structure_new("subdata", fieldName, GST_TYPE_BUFFER, subBuffer, NULL); // ref count 2, this will be unref by gst_structure free.
        else
          gst_structure_set(subDataList, fieldName, GST_TYPE_BUFFER, subBuffer, NULL); // ref count 2, this will be unref by gst_structure free.

        /* decrease ref count which increased by gst_buffer_new.
        *  If this buffer is extracted by audiodatasplitter, it will ref again and released by  final buffer owner (sink element) */
        gst_buffer_unref(subBuffer);
      }
      else
        GST_ERROR_OBJECT(omx_base, "buffer allocation failed");
      gst_caps_unref (caps);
    }

    if ( (pExtra->nSize % 4) != 0)
      GST_WARNING_OBJECT(omx_base, "pExtra->nSize[ %d ] is not aligned to 32bit", pExtra->nSize);

    if (pExtra->nSize <= remain)
      remain -= pExtra->nSize;
    else
    {
      GST_WARNING_OBJECT(omx_base, "last nSize[ %d ] <= remain[ %d ]", pExtra->nSize, remain);
      remain = 0;
    }

    /* Take the next extradata */
    pExtra = (OMX_OTHER_EXTRADATATYPE *) (((OMX_U8 *) pExtra) + pExtra->nSize); // next
  }
  GST_LOG_OBJECT(omx_base, "subDataList[ %p ] nSize[ %d ], nDataSize[ %d ], eType[ %d ], remain[ %d ]",
      subDataList, pExtra->nSize, pExtra->nDataSize, pExtra->eType, remain);

  /* Set gststructure to buf->qdata*/
  if (subDataList) {
    gst_buffer_set_qdata(*buf, g_quark_from_static_string("subdata"), subDataList);
  }
  return GSTOMX_RETURN_OK;
}

static gboolean
gstomx_base_audiodec_query (GstOmxBaseFilter * omx_base, GstQuery * query)
{
  gboolean res = FALSE;

  GST_LOG_OBJECT(omx_base, "query received[%d]", GST_QUERY_TYPE (query));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_RESOURCE:
      GST_INFO_OBJECT(omx_base, "RESOURCE QUERY - RESOURCE CATEGORY[ASM_RESOURCE_AUDIO_DECODER]");
      gst_query_add_resource(query, ASM_RESOURCE_AUDIO_DECODER);
      res = TRUE;
      break;
    default:
      res = GST_ELEMENT_GET_CLASS (omx_base)->query (omx_base, query);
      break;
  }

  return res;
}


static void
type_class_init (gpointer g_class, gpointer class_data)
{
  GObjectClass *gobject_class;
  GstOmxBaseFilterClass *basefilter_class;
  GstElementClass *base_class;
  gobject_class = G_OBJECT_CLASS (g_class);
  basefilter_class = GST_OMX_BASE_FILTER_CLASS (g_class);
  base_class = (GstElementClass*)basefilter_class;
  basefilter_class->process_input_buf = process_audio_input_buf;
  basefilter_class->process_output_buf = process_audio_output_buf;
  base_class->query = GST_DEBUG_FUNCPTR (gstomx_base_audiodec_query);

  /* Properties stuff */
  {
    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;

    /* STATE_TUNING */
    g_object_class_install_property (gobject_class, ARG_USE_STATETUNING,
        g_param_spec_boolean ("state-tuning", "start omx component in gst paused state",
        "Whether or not to use state-tuning feature",
        FALSE, G_PARAM_READWRITE));
  }
}

static void
settings_changed_cb (GOmxCore * core)
{
  GstOmxBaseFilter *omx_base;
  guint rate;
  guint channels;

  omx_base = core->object;

  GST_DEBUG_OBJECT (omx_base, "settings changed");

  {
    OMX_AUDIO_PARAM_PCMMODETYPE param;

    G_OMX_INIT_PARAM (param);

    param.nPortIndex = omx_base->out_port->port_index;
    OMX_GetParameter (omx_base->gomx->omx_handle, OMX_IndexParamAudioPcm,
        &param);

    rate = param.nSamplingRate;
    channels = param.nChannels;
    if (rate == 0) {
            /** @todo: this shouldn't happen. */
      GST_WARNING_OBJECT (omx_base, "Bad samplerate");
      rate = 44100;
    }
  }

  {
    GstCaps *new_caps;
	GstStructure* sinkcaps = gst_caps_get_structure(GST_PAD_CAPS(omx_base->sinkpad), 0);
	gchar* sink_mimetype = gst_structure_get_name(sinkcaps);

    new_caps = gst_caps_new_simple ("audio/x-raw-int",
        "prevmimetype", G_TYPE_STRING, sink_mimetype,
        "width", G_TYPE_INT, 16,
        "depth", G_TYPE_INT, 16,
        "rate", G_TYPE_INT, rate,
        "signed", G_TYPE_BOOLEAN, TRUE,
        "endianness", G_TYPE_INT, G_BYTE_ORDER,
        "channels", G_TYPE_INT, channels, NULL);

    GST_INFO_OBJECT (omx_base, "caps are: %" GST_PTR_FORMAT, new_caps);
    gst_pad_set_caps (omx_base->srcpad, new_caps);
  }
}

static gboolean
src_setcaps (GstPad * pad, GstCaps * caps)
{
    /* inform audio information to AVOC, only for omx hw decoder case */
    GstOmxBaseAudioDec *self = GST_OMX_BASE_AUDIODEC(GST_PAD_PARENT (pad));
    GstOmxBaseFilter *omx_base = GST_OMX_BASE_FILTER (GST_PAD_PARENT (pad));
    GstStructure* src_str = gst_caps_get_structure(caps, 0);
	
    if (src_str == NULL || self == NULL || omx_base == NULL) {
      GST_ERROR("src_str[ %p ], self[ %p ], omx_base[ %p ]", src_str ,self ,omx_base);
    }
	
  	return gst_pad_set_caps (pad, caps);
}

static void
omx_setup (GstOmxBaseFilter * omx_base)
{
	GstOmxBaseAudioDec *self;
	GOmxCore *gomx;
	if (!omx_base) 
	{
		GST_ERROR("omx_base is NULL");
		return;
	}
	self = GST_OMX_BASE_AUDIODEC (omx_base);
	gomx = (GOmxCore *) omx_base->gomx;
	GST_INFO_OBJECT (omx_base, "begin");
	if (omx_base->sinkpad)
	{
		GstCaps* caps = NULL;
		GstStructure *structure = NULL;
		OMX_PARAM_PORTDEFINITIONTYPE param;
		OMX_AUDIO_PARAM_DECODERINPUTTYPE decoderInputType;
		memset(&decoderInputType, 0, sizeof(OMX_AUDIO_PARAM_DECODERINPUTTYPE));
		G_OMX_INIT_PARAM (param);
		caps = GST_PAD_CAPS(omx_base->sinkpad);
		if (caps)
		{
			GST_LOG_OBJECT (self, "omx_setup (sink): %" GST_PTR_FORMAT, caps);
			structure = gst_caps_get_structure (caps, 0);
		if (structure)
		{
			decoderInputType.ePCMMode = OMX_AUDIO_PCMModeMax;
			decoderInputType.eNumData = OMX_NumericalDataSigned;
			decoderInputType.eEndian = OMX_EndianBig;
			decoderInputType.nUserInfo = 0;
			decoderInputType.bPresetMode = FALSE;

			const gchar* name = NULL;
			param.nPortIndex = omx_base->in_port->port_index;
			OMX_GetParameter (gomx->omx_handle, OMX_IndexParamPortDefinition, &param);

			name = gst_structure_get_name (structure);
			param.format.audio.eEncoding = OMX_AUDIO_CodingAutoDetect;

		if (g_str_has_prefix (name, "audio"))
		{
			if ((g_str_has_prefix (name, "audio/x-ac3")) || (g_str_has_prefix (name, "audio/ac3")))
			{
				param.format.audio.eEncoding = OMX_AUDIO_CodingAC3;
			}
			else if (g_str_has_prefix (name, "audio/x-eac3"))
			{
				param.format.audio.eEncoding = OMX_AUDIO_CodingEAC3;
			}
			else if (g_str_has_prefix (name, "audio/x-dts"))
			{
				gboolean LBR = FALSE;
				if (gst_structure_get_boolean(structure, "stream-format", &LBR))
				{
					if (LBR)
						param.format.audio.eEncoding = OMX_AUDIO_CodingDTSLBR;
					else
						param.format.audio.eEncoding = OMX_AUDIO_CodingDTS;
				}
				else
				{
					GST_WARNING_OBJECT(omx_base, "no stream-format information received in caps");		
				}
			}
			else if ((g_str_has_prefix (name, "audio/x-adpcm")) || (g_str_has_prefix (name, "audio/adpcm")))
			{
				const char* layout = NULL;
				param.format.audio.eEncoding = OMX_AUDIO_CodingADPCM;
				layout = gst_structure_get_string(structure, "layout");
				if (layout)
				{
					if (!g_strcmp0(layout, "microsoft"))
						decoderInputType.ePCMMode = OMX_AUDIO_PCMModeMs;
					else if (!g_strcmp0(layout, "quicktime"))
						decoderInputType.ePCMMode = OMX_AUDIO_PCMModeImaQT;
					else if (!g_strcmp0(layout, "dvi"))
						decoderInputType.ePCMMode = OMX_AUDIO_PCMModeImaWAV;
					else
						GST_WARNING_OBJECT(omx_base, "Unknown type, layout[ %s ]", layout);
				}
				else
					GST_DEBUG_OBJECT(omx_base, "No layout in audio caps");
			}
			else if (g_str_has_prefix (name, "audio/x-alaw"))
			{
				param.format.audio.eEncoding = OMX_AUDIO_CodingG711;
				decoderInputType.ePCMMode = OMX_AUDIO_PCMModeALaw;
			}
			else if (g_str_has_prefix (name, "audio/x-mulaw"))
			{
				param.format.audio.eEncoding = OMX_AUDIO_CodingG711;
				decoderInputType.ePCMMode = OMX_AUDIO_PCMModeMULaw;
			}
			else if ((g_str_has_prefix (name, "audio/x-aac")) || (g_str_has_prefix (name, "audio/mp4")))
			{
				param.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
			}
			else if (g_str_has_prefix (name, "audio/mpeg"))
			{
				gint version = 0;
				if (gst_structure_get_int(structure, "mpegversion", &version))
				{
					if (version == 1)
					{
						param.format.audio.eEncoding = OMX_AUDIO_CodingMP3;
					}
					else
					{
						const char* format=NULL;
						param.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
						format = gst_structure_get_string(structure, "stream-format");
						if (format && (!g_strcmp0(format, "loas")))
						{
							param.format.audio.eEncoding = OMX_AUDIO_CodingHEAAC;
						}
					}
				}
				else
					GST_ERROR_OBJECT(omx_base, "can not find audio mpeg version[ %s ]", name);
			}
			else if (g_str_has_prefix (name, "audio/x-pn-realaudio"))
			{
				gint flavor = 0;
				param.format.audio.eEncoding = OMX_AUDIO_CodingRA;
				if (gst_structure_get_int(structure, "flavor", &flavor))
				{
					decoderInputType.nUserInfo = flavor;
				}
			}
			else if (g_str_has_prefix (name, "audio/x-vorbis"))
			{
				param.format.audio.eEncoding = OMX_AUDIO_CodingVORBIS;
			}
			else if ((g_str_has_prefix (name, "audio/x-wma")) || (g_str_has_prefix (name, "audio/x-ms-wma")))
			{
				param.format.audio.eEncoding = OMX_AUDIO_CodingWMA;
			}
			else if (g_str_has_prefix (name, "audio/x-raw-int"))
			{
				param.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
			}
			else
			{
				// OMX_AUDIO_CodingHEAAC
				GST_ERROR_OBJECT(omx_base, "can not find audio codec info[ %s ]", name);
				return FALSE;
			}
		}
		else
		{
			GST_ERROR_OBJECT(omx_base, "can not find audio codec info[ %s ]", name);
			return FALSE;
		}  
		OMX_SetParameter (gomx->omx_handle, OMX_IndexParamPortDefinition, &param);
		gomx->media_type = 2;

		/* yh_46.kim */
		guint32 codectag = 0;
		gint32 value = 0;
		gboolean security_enabled = OMX_FALSE;
		gboolean preset_enabled = OMX_FALSE;

		decoderInputType.eEncoding = param.format.audio.eEncoding; /**< Type of data expected for this port (e.g. PCM, AMR, MP3, etc) */
		if (gst_structure_get_int(structure, "channels", &value))
			decoderInputType.nChannels = value;
		else
			decoderInputType.nChannels = 0;
		if (gst_structure_get_int(structure, "bpp", &value) && value != 0)
			decoderInputType.nBitsPerSample = value;
		else if (gst_structure_get_int(structure, "depth", &value) && value != 0)
			decoderInputType.nBitsPerSample = value;
		else if (gst_structure_get_int(structure, "samplesize", &value) && value != 0)
			decoderInputType.nBitsPerSample = value;
		else
			decoderInputType.nBitsPerSample = 0;

		if (gst_structure_get_int(structure, "rate", &value))
			decoderInputType.nSampleRate = value;
		else
			decoderInputType.nSampleRate = 0;

		if (gst_structure_get_int(structure, "block_align", &value))
			decoderInputType.nBlockAlign = value;
		else if (gst_structure_get_int(structure, "leaf_size", &value))
			decoderInputType.nBlockAlign = value;
		else
			decoderInputType.nBlockAlign = 0;

		if (gst_structure_get_int(structure, "bitrate", &value))
			decoderInputType.nBitRate = value;
		else
			decoderInputType.nBitRate = 0;

		if ((omx_base->codec_data) && (omx_base->codec_data->data) && (omx_base->codec_data->size>0))
		{
			decoderInputType.pCodecExtraData = omx_base->codec_data->data;
			decoderInputType.nCodecExtraDataSize = omx_base->codec_data->size;
		}
		else
		{
			decoderInputType.pCodecExtraData = NULL;
			decoderInputType.nCodecExtraDataSize = 0;
		}

		if (gst_structure_get_fourcc(structure, "format", &codectag))
		{
			decoderInputType.nAudioCodecTag = codectag; // 4cc
		}
		decoderInputType.nAudioDecodingType = 0; // normal, clip, seamless, pvr

		if(gst_structure_get_boolean(structure, "secure", &security_enabled))
		{
			GST_DEBUG_OBJECT(omx_base, "bSecureMode[ %x ]", security_enabled);
		}
		else
		{
			GST_WARNING_OBJECT(omx_base, "no security information received in caps");
		}
		decoderInputType.bSecureMode = security_enabled;

        if(gst_structure_get_boolean(structure, "preset", &preset_enabled))
        {
            GST_DEBUG_OBJECT(omx_base, "bPresetMode[ %x ]",preset_enabled);
        }
        decoderInputType.bPresetMode = preset_enabled;
		gint endianness;
		gboolean is_signed;
		gboolean is_bigendian;

		if(gst_structure_get_int (structure, "endianness", &endianness))
		{
			GST_DEBUG_OBJECT(omx_base, "endianness[ %d ]", endianness);			
		}
		is_bigendian = (endianness == 1234) ? FALSE : TRUE;
		decoderInputType.eEndian = is_bigendian ? OMX_EndianBig : OMX_EndianLittle;

		if(gst_structure_get_boolean (structure, "signed", &is_signed))
		{
			GST_DEBUG_OBJECT(omx_base, "signed[ %d ]", is_signed);			
		}
		decoderInputType.eNumData = is_signed ? OMX_NumericalDataSigned : OMX_NumericalDataUnsigned;

		if(decoderInputType.bSecureMode && param.format.audio.eEncoding == OMX_AUDIO_CodingPCM && is_bigendian && decoderInputType.nBitsPerSample == 16)
		{
			GST_DEBUG_OBJECT(omx_base, "SET LPCM TZ");
			self->is_tz_lpcm_be16 = TRUE;
		}

		GST_DEBUG_OBJECT(omx_base, "param[ eEncoding(0x%x), ePCMMode(0x%x), eNumData(0x%x), eEndian(0x%x), nChannels(%d), \nnBitsPerSample(%d), nSampleRate(%d), nBlockAlign(%d),\nBitRate(%d), pCodecExtraData(%p, s:%d),\nnAudioCodecTag(0x%x), nUserInfo(%d),  nAudioDecodingType(%d), bSecureMode(%d), bPresetMode(%d) ]",
		decoderInputType.eEncoding, decoderInputType.ePCMMode, decoderInputType.eNumData, decoderInputType.eEndian,
		decoderInputType.nChannels, decoderInputType.nBitsPerSample, decoderInputType.nSampleRate, decoderInputType.nBlockAlign,
		decoderInputType.nBitRate, decoderInputType.pCodecExtraData, decoderInputType.nCodecExtraDataSize, decoderInputType.nAudioCodecTag,
		decoderInputType.nUserInfo,decoderInputType.nAudioDecodingType,decoderInputType.bSecureMode,decoderInputType.bPresetMode);
		OMX_SetParameter(gomx->omx_handle, OMX_IndexAudioDecInputParam, &decoderInputType);
		}
	}
	}
}

static gboolean
sink_setcaps (GstPad * pad, GstCaps * caps)
{
  GstStructure *structure;
  GstOmxBaseAudioDec *self;
  GstOmxBaseFilter *omx_base;
  GstBuffer *buffer = NULL;
  GOmxCore *gomx;
  
  omx_base = GST_OMX_BASE_FILTER (GST_PAD_PARENT (pad));
  self = GST_OMX_BASE_AUDIODEC (omx_base);

  GST_INFO_OBJECT (omx_base, "setcaps (sink): %" GST_PTR_FORMAT, caps);
  gomx = (GOmxCore *) omx_base->gomx;

  structure = gst_caps_get_structure (caps, 0);

  {
    const GValue *codec_data;
    codec_data = gst_structure_get_value (structure, "codec_data");
    if (codec_data) {
      buffer = gst_value_get_buffer (codec_data);
      omx_base->codec_data = buffer;
      gst_buffer_ref (buffer);
    }
  }

  return gst_pad_set_caps (pad, caps);
}

static gboolean 
gstomx_base_audiodec_sinkpad_query (GstPad * pad, GstQuery * query)
{
  GstOmxBaseFilter *self = GST_OMX_BASE_FILTER (GST_OBJECT_PARENT(pad));
  GOmxCore *gomx = self->gomx;
  gboolean ret = FALSE;
  
  if (query == NULL)
  {
  	return ret;
  }
  
  GST_DEBUG_OBJECT(gomx, "query-check : %d", query->type);

  switch (query->type)
  {
	default:
    {
	  GST_DEBUG_OBJECT(gomx, "un-supported query type [%d], pass forward", query->type);
	  ret = gst_pad_query_default(pad, query);
    }
  }  
  return ret;
}

static gboolean 
gstomx_base_audiodec_srcpad_query (GstPad * pad, GstQuery * query)
{
  GstOmxBaseFilter *self = GST_OMX_BASE_FILTER (GST_OBJECT_PARENT(pad));
  GOmxCore *gomx = self->gomx;

  gboolean ret = FALSE;
  if (query == NULL)
  {
  	goto exit;
  }

  GST_DEBUG_OBJECT(gomx, "query-check : %d", query->type);

  switch (query->type)
  {
	case GST_QUERY_CUSTOM:
	{
	  GstStructure *structure = gst_query_get_structure (query);
	  ret = gst_structure_has_name(structure, "alsa-ask-sinkcaps");
	  if (ret)
	  {
	    GST_DEBUG_OBJECT(gomx, "query-reply : to alsa");

	    GstStructure* sinkcaps = gst_caps_get_structure(GST_PAD_CAPS(self->sinkpad), 0);

		gchar* pAudioCodec = gst_structure_get_name(sinkcaps);
		gst_structure_set(structure, "pAudioCodec", G_TYPE_STRING, pAudioCodec, NULL);
	    GST_DEBUG_OBJECT(gomx, "query-set codec name : %s", pAudioCodec);

	    int samplerate = -2;
	    if (gst_structure_get_int(sinkcaps, "rate", &samplerate))
	    {
		  gst_structure_set(structure, "samplerate", G_TYPE_INT, samplerate, NULL); 		
		  GST_DEBUG_OBJECT(gomx, "query-set sample rate: %d", samplerate);
	    }

		int mpegversion = -2;
	    if (gst_structure_has_field(sinkcaps, "mpegversion"))
	    {
	      gst_structure_get_int(sinkcaps, "mpegversion", &mpegversion);
		  gst_structure_set(structure, "mpegversion", G_TYPE_INT, mpegversion, NULL);		  
		  GST_DEBUG_OBJECT(gomx, "query-set mpegversion: %d", mpegversion);
	    }

		int mpeglayer = -2;
	    if (gst_structure_has_field(sinkcaps, "layer"))
	    {
	      gst_structure_get_int(sinkcaps, "layer", &mpeglayer);
		  gst_structure_set(structure, "mpeglayer", G_TYPE_INT, mpeglayer, NULL);		  
		  GST_DEBUG_OBJECT(gomx, "query-set mpeglayer : %d", mpeglayer);
	   	}
      }
	  else
	  {
		ret = gst_pad_query_default(pad, query);
	  } 
	  break;
    }
	default:
    {
	  GST_DEBUG_OBJECT(gomx, "un-supported query type [%d], pass forward", query->type);
	  ret = gst_pad_query_default(pad, query);
    }
  }

exit:  
  return ret;
}


GST_DEBUG_CATEGORY_EXTERN(GST_CAT_ADEC);

static void
type_instance_init (GTypeInstance * instance, gpointer g_class)
{
  GstOmxBaseFilter *omx_base;
  GstOmxBaseAudioDec *self;

  omx_base = GST_OMX_BASE_FILTER (instance);
  self = GST_OMX_BASE_AUDIODEC (omx_base);

  GST_DEBUG_OBJECT (omx_base, "start");
  omx_base->debugCategory = GST_CAT_ADEC;
  self->decoding_type = 0;
  self->is_tz_lpcm_be16 = FALSE;

  omx_base->omx_setup = omx_setup;
  omx_base->gomx->settings_changed_cb = settings_changed_cb;

  gst_pad_set_setcaps_function (omx_base->sinkpad, sink_setcaps);
  gst_pad_set_setcaps_function (omx_base->srcpad, src_setcaps);
  gst_pad_set_query_function (omx_base->sinkpad, gstomx_base_audiodec_sinkpad_query);
  gst_pad_set_query_function (omx_base->srcpad, gstomx_base_audiodec_srcpad_query);
  
}
