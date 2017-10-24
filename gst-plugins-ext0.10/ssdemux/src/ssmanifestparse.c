#include "ssmanifestparse.h"

static gboolean ssm_parse_root_node (GstSSMParse *parser, xmlNodePtr root_node);
GstCaps * ssm_prepare_video_caps (GstSSMParse *parser, GstSSMStreamNode *stream);
GstCaps * ssm_prepare_audio_caps (GstSSMParse *parser, GstSSMStreamNode *stream);
GstCaps *ssm_prepare_text_caps (GstSSMParse *parser, GstSSMStreamNode *stream);
static gboolean convert_NALUnitDCI_to_PacktizedDCI (unsigned char *nalu_dci, unsigned char **packetized_dci, unsigned int *packetized_dci_len);

#define MANIFEST_LOCK(parser) g_mutex_lock(parser->lock)
#define MANIFEST_UNLOCK(parser) g_mutex_unlock(parser->lock)
#define START_TS_MAX(a,b) ((a)>(b) ? (a) : (b))

const gchar *
ssm_parse_get_stream_name(SS_STREAM_TYPE type)
{
  if (type == SS_STREAM_VIDEO) return "video"; \
  else if (type == SS_STREAM_AUDIO) return "audio"; \
  else if (type == SS_STREAM_TEXT) return "text"; \
  else return "unknown";
}


static gboolean
int_from_string (gchar * ptr, gchar ** endptr, gint * val, gint base)
{
  gchar *end;
  g_return_val_if_fail (ptr != NULL, FALSE);
  g_return_val_if_fail (val != NULL, FALSE);

  errno = 0;
  *val = strtol (ptr, &end, base);
  if ((errno == ERANGE && (*val == LONG_MAX || *val == LONG_MIN)) || (errno != 0 && *val == 0)) {
    g_print ("Error in strtol : %s\n", strerror(errno));
    return FALSE;
  }

  if (endptr)
    *endptr = end;

  return end != ptr;
}

static gboolean
ssm_parse_get_xml_prop_boolean (GstSSMParse *parser, xmlNode * node,
    const gchar * property)
{
  xmlChar *prop_string;
  gboolean prop_bool = FALSE;

  prop_string = xmlGetProp (node, (const xmlChar *) property);
  if (prop_string) {
    if ((xmlStrcmp (prop_string, (xmlChar *) "false") == 0) ||
	(xmlStrcmp (prop_string, (xmlChar *) "FALSE") == 0)) {
      GST_LOG (" - %s: false", property);
    } else if ((xmlStrcmp (prop_string, (xmlChar *) "true") == 0) ||
       (xmlStrcmp (prop_string, (xmlChar *) "TRUE") == 0)) {
      GST_LOG(" - %s: true", property);
      prop_bool = TRUE;
    } else {
      GST_WARNING("failed to parse boolean property %s from xml string %s", property, prop_string);
    }
    xmlFree (prop_string);
  }

  return prop_bool;
}

static guint
ssm_parse_get_xml_prop_uint (GstSSMParse *parser,
    xmlNode * node, const gchar * property, guint default_val)
{
  xmlChar *prop_string;
  guint prop_uint = default_val;

  prop_string = xmlGetProp (node, (const xmlChar *) property);
  if (prop_string) {
    if (sscanf ((gchar *) prop_string, "%u", &prop_uint)) {
      GST_LOG (" - %s: %u", property, prop_uint);
    } else {
      GST_WARNING("failed to parse unsigned integer property %s from xml string %s",
          property, prop_string);
    }
    xmlFree (prop_string);
  }

  return prop_uint;
}

static guint64
ssm_parse_get_xml_prop_uint64 (GstSSMParse *parser,
    xmlNode * node, const gchar * property, guint64 default_val)
{
  xmlChar *prop_string;
  guint64 prop_uint64 = default_val;

  prop_string = xmlGetProp (node, (const xmlChar *) property);
  if (prop_string) {
    if (sscanf ((gchar *) prop_string, "%llu", &prop_uint64)) {
      GST_LOG (" - %s: %s[%"G_GUINT64_FORMAT"]", property, prop_string, prop_uint64);
    } else {
      GST_WARNING("failed to parse unsigned integer property %s from xml string %s",
          property, prop_string);
    }
    xmlFree (prop_string);
  }

  return prop_uint64;
}

static gint
ssm_parser_sort_qualitylevels_by_bitrate (gconstpointer a, gconstpointer b)
{
	return ((GstSSMQualityNode *) (a))->bitrate - ((GstSSMQualityNode *) (b))->bitrate; //sorting in ascending order
	//return ((GstSSMQualityNode *) (b))->bitrate - ((GstSSMQualityNode *) (a))->bitrate; // sorting in descending order
}


static void
gst_ssm_parse_free_quality_node (GstSSMQualityNode *qualitynode)
{
  if (qualitynode) {
    g_free (qualitynode->codec_data);
    g_free (qualitynode->fourcc);
    g_slice_free (GstSSMQualityNode, qualitynode);
  }
}

static void
gst_ssm_parse_free_fragment_node (GstSSMFragmentNode*fragnode)
{
  if (fragnode) {
    g_slice_free (GstSSMFragmentNode, fragnode);
  }
}


static void
gst_ssm_parse_free_stream_node (GstSSMStreamNode *streamnode)
{
  if (streamnode) {
    if (streamnode->quality_lists) {
      streamnode->quality_lists = g_list_first (streamnode->quality_lists);
      g_list_foreach (streamnode->quality_lists, (GFunc) gst_ssm_parse_free_quality_node, NULL);
      g_list_free (streamnode->quality_lists);
      streamnode->quality_lists = NULL;
    }
    if (streamnode->fragment_lists) {
      streamnode->fragment_lists = g_list_first (streamnode->fragment_lists);
      g_list_foreach (streamnode->fragment_lists, (GFunc) gst_ssm_parse_free_fragment_node, NULL);
      g_list_free (streamnode->fragment_lists);
      streamnode->fragment_lists = NULL;
    }
    if (streamnode->StreamType) {
      g_free(streamnode->StreamType);
      streamnode->StreamType = NULL;
    }
    if (streamnode->StreamUrl) {
      g_free(streamnode->StreamUrl);
      streamnode->StreamUrl = NULL;
    }
    if (streamnode->StreamSubType) {
      g_free(streamnode->StreamSubType);
      streamnode->StreamSubType = NULL;
    }
    if (streamnode->StreamName) {
      g_free(streamnode->StreamName);
      streamnode->StreamName = NULL;
    }
    if (streamnode->frag_cond) {
      g_cond_free (streamnode->frag_cond);
      streamnode->frag_cond = NULL;
    }
    if (streamnode->frag_lock) {
      g_mutex_free (streamnode->frag_lock);
      streamnode->frag_lock = NULL;
    }
    g_slice_free (GstSSMStreamNode, streamnode);
  }
}

GstSSMParse *
gst_ssm_parse_new (const gchar * uri)
{
  GstSSMParse *parser;
  gchar *tmp = NULL;

  g_return_val_if_fail (uri != NULL, NULL);

  parser = g_new0 (GstSSMParse, 1);

  parser->uri = g_strdup (uri);
  parser->lock = g_mutex_new();
  tmp = strrchr(uri, '/');
  tmp = tmp+1;
  parser->presentation_uri = (gchar *) malloc (tmp - uri + 1);
  if (NULL == parser->presentation_uri) {
    GST_ERROR ("Failed to allocate memory..\n");
    return NULL;
  }

  strncpy (parser->presentation_uri, uri, tmp - uri);
  parser->presentation_uri[tmp-uri] = '\0';
  parser->ns_start = GST_CLOCK_TIME_NONE;
  GST_INFO ("Presentation URI : %s", parser->presentation_uri);
  return parser;
}

void
gst_ssm_parse_free (GstSSMParse *parser)
{
  int i =0;

  g_return_if_fail (parser != NULL);

  g_mutex_free(parser->lock);

  // TODO: cleanup memory allocated.....
  if (parser->RootNode) {
    for (i = 0; i < SS_STREAM_NUM; i++) {
      if (parser->RootNode->streams[i]) {
        g_list_foreach (parser->RootNode->streams[i], (GFunc) gst_ssm_parse_free_stream_node, NULL);
        parser->RootNode->streams[i] = NULL;
      }
    }
    if (parser->RootNode->ProtectNode) {
      g_slice_free (GstSSMProtectionNode, parser->RootNode->ProtectNode);
      parser->RootNode->ProtectNode = NULL;
    }
    g_slice_free (GstSSMRootNode, parser->RootNode);
    parser->RootNode = NULL;
  }

  g_free (parser);
}


static gboolean
gst_ssm_parse_confirm_audiotag(gchar ** fourcc, gint audio_tag)
{
  switch(audio_tag){
    case 255:
      *fourcc = (gchar *)strdup("AACL");
      break;
    case 353:
      *fourcc = (gchar *)strdup("WMAP");
      break;
    default:
      return FALSE;
  }
  return TRUE;
}

static gboolean
ssm_parse_quality_node (GstSSMParse *parser, GstSSMStreamNode *stream, xmlNodePtr quality_node)
{
  GstSSMQualityNode *quality_level = NULL;

  g_return_val_if_fail (parser != NULL, FALSE);
  g_return_val_if_fail (quality_node != NULL, FALSE);
  g_return_val_if_fail (stream != NULL, FALSE);

  quality_level = g_slice_new0 (GstSSMQualityNode);
  if (quality_level == NULL) {
    GST_ERROR ("Allocation of quality_level node failed!");
    return FALSE;
  }

  quality_level->codec_data = NULL;
  quality_level->fourcc = NULL;

  quality_level->index = ssm_parse_get_xml_prop_uint (parser, quality_node, "Index", -1);
  GST_DEBUG("Quality Index = %d", quality_level->index);
  if (SS_STREAM_VIDEO == stream->type && (quality_level->index == -1)) {
    GST_ERROR ("Index attribute is not present for VIDEO stream...");
    return FALSE;
  }

  /* MANDATORY : parsing Bitrate attribute */
  quality_level->bitrate = ssm_parse_get_xml_prop_uint (parser, quality_node, "Bitrate", -1);
  GST_DEBUG("Bitrate = %d", quality_level->bitrate);
  if (quality_level->bitrate == -1) {
    GST_ERROR ("bitrate attribute is not present...");
    return FALSE;
  }

  /* MANDATORY for video: parsing MaxWidth attribute */
  quality_level->max_width = ssm_parse_get_xml_prop_uint (parser, quality_node, "MaxWidth", -1);
  GST_DEBUG("MaxWidth = %d", quality_level->max_width);
  if (SS_STREAM_VIDEO == stream->type && (quality_level->max_width == -1)) {
    GST_ERROR ("max_width attribute is not present in VIDEO...");
    return FALSE;
  }

  /* MANDATORY for video: parsing MaxHeight attribute */
  quality_level->max_height = ssm_parse_get_xml_prop_uint (parser, quality_node, "MaxHeight", -1);
  GST_DEBUG("MaxWidth = %d", quality_level->max_height);
  if (SS_STREAM_VIDEO == stream->type && (quality_level->max_height == -1)) {
    GST_ERROR ("max_height attribute is not present in VIDEO...");
    return FALSE;
  }

  /* MANDATORY for audio: parsing SamplingRate attribute */
  quality_level->samplingrate = ssm_parse_get_xml_prop_uint (parser, quality_node, "SamplingRate", -1);
  GST_DEBUG("SamplingRate = %d", quality_level->samplingrate);
  if (SS_STREAM_AUDIO == stream->type && (quality_level->samplingrate == -1)) {
    GST_ERROR ("SamplingRate attribute is not present for AUDIO...");
    return FALSE;
  }

  /* MANDATORY for audio: parsing Channels attribute */
  quality_level->channels = ssm_parse_get_xml_prop_uint (parser, quality_node, "Channels", -1);
  GST_DEBUG("SamplingRate = %d", quality_level->channels);
  if (SS_STREAM_AUDIO == stream->type && (quality_level->channels == -1)) {
    GST_ERROR ("Channels attribute is not present for AUDIO...");
    return FALSE;
  }

  /* MANDATORY for audio: parsing BitsPerSample attribute */
  quality_level->bps = ssm_parse_get_xml_prop_uint (parser, quality_node, "BitsPerSample", -1);
  GST_DEBUG("BitsPerSample = %d", quality_level->bps);
  if (SS_STREAM_AUDIO == stream->type && (quality_level->bps == -1)) {
    GST_ERROR ("BitsPerSample attribute is not present for AUDIO...");
    return FALSE;
  }

  /* MANDATORY for audio: parsing PacketSize attribute */
  quality_level->packet_size = ssm_parse_get_xml_prop_uint (parser, quality_node, "PacketSize", -1);
  GST_DEBUG("PacketSize = %d", quality_level->packet_size);
  if (SS_STREAM_AUDIO == stream->type && (quality_level->packet_size == -1)) {
    GST_ERROR ("PacketSize attribute is not present for AUDIO...");
    return FALSE;
  }


  /* MANDATORY for audio: parsing AudioTag attribute */
  quality_level->audio_tag = ssm_parse_get_xml_prop_uint (parser, quality_node, "AudioTag", -1);
  GST_DEBUG("AudioTag = %d", quality_level->audio_tag);
  if (SS_STREAM_AUDIO == stream->type && (quality_level->audio_tag == -1)) {
    GST_ERROR ("AudioTag attribute is not present for AUDIO...");
    return FALSE;
  }

  /* MANDATORY for audio & video: parsing FourCC attribute */
  quality_level->fourcc = (gchar *)xmlGetProp(quality_node, (const xmlChar *) "FourCC");
  if(!quality_level->fourcc && SS_STREAM_AUDIO == stream->type){
    if(!gst_ssm_parse_confirm_audiotag(&quality_level->fourcc, quality_level->audio_tag)){
      GST_ERROR ("failed to set fourcc from audio tag %d",quality_level->audio_tag);
      return FALSE;
    }
  }

  if (!quality_level->fourcc && ((SS_STREAM_AUDIO == stream->type) || (SS_STREAM_VIDEO == stream->type))) {
    GST_ERROR ("failed to parse fourcc from quality node");
    return FALSE;
  }

  if (quality_level->fourcc && !((!strncmp ((char *)quality_level->fourcc, "AACL", 4)) || !strncmp ((char *)quality_level->fourcc, "WMAP", 4) ||
  (!strncmp ((char *)quality_level->fourcc, "H264", 4)) || !strncmp ((char *)quality_level->fourcc, "WVC1", 4) ||
  (!strncmp ((char *)quality_level->fourcc, "TTML", 4)))) {
    GST_INFO ("Not a proper Fourcc Code...If possible take from SubType\n\n\n");
    free (quality_level->fourcc);

    GST_DEBUG ("Subtype = %s\n\n",stream->StreamSubType);
    quality_level->fourcc = g_strdup (stream->StreamSubType);
    if (!((!strncmp ((char *)quality_level->fourcc, "AACL", 4)) || !strncmp ((char *)quality_level->fourcc, "WMAP", 4) ||
    (!strncmp ((char *)quality_level->fourcc, "H264", 4)) || !strncmp ((char *)quality_level->fourcc, "WVC1", 4))) {
      GST_ERROR ("Subtype also does not contain valid fourcc code...ERROR\n");
      return FALSE;
    }
  }

  GST_DEBUG("FourCC = %s", quality_level->fourcc);

  // TODO: need to check whether it is required for all video & audio

  /* MANDATORY for audio & video: parsing CodecPrivateData attribute */
  quality_level->codec_data = (gchar *)xmlGetProp(quality_node, (const xmlChar *) "CodecPrivateData");
  if (!quality_level->codec_data && ((SS_STREAM_AUDIO == stream->type) || (SS_STREAM_VIDEO == stream->type))) {
    GST_ERROR ("failed to get codec data from quality node");
    return FALSE;
  }

  /* Optional for VIDEO H264: parsing NALUnitLengthField attribute */
  quality_level->NALULengthofLength = ssm_parse_get_xml_prop_uint (parser, quality_node, "NALUnitLengthField", 4);
  GST_DEBUG("NALUnitLengthField = %d", quality_level->NALULengthofLength);

  stream->quality_lists = g_list_append (stream->quality_lists, quality_level);

  GST_LOG ("Appened quality level to stream...");

  return TRUE;

}

static gboolean
ssm_parse_fragment_node (GstSSMParse *parser, GstSSMStreamNode *stream, xmlNodePtr fragment_node)
{
  GstSSMFragmentNode *fragment = NULL;
  gboolean found_time = FALSE;
  gboolean found_duration = FALSE;

  g_return_val_if_fail (parser != NULL, FALSE);
  g_return_val_if_fail (fragment_node != NULL, FALSE);
  g_return_val_if_fail (stream != NULL, FALSE);

  fragment = g_slice_new0 (GstSSMFragmentNode);
  if (fragment == NULL) {
    GST_ERROR ("Allocation of fragment failed!");
    return FALSE;
  }

  /*parsing fragmentnumber attribute */
  fragment->num = ssm_parse_get_xml_prop_uint (parser, fragment_node, "n", -1);
  GST_DEBUG ("fragment number = %d", fragment->num);

  /*parsing duration attribute */
  fragment->dur = ssm_parse_get_xml_prop_uint64 (parser, fragment_node, "d", -1);
  if (fragment->dur == -1 && !fragment_node->next) {
    GST_ERROR ("Fragment Duration for last fragment is mandatory");
    return FALSE;
  } else if (fragment->dur != -1) {
    GST_DEBUG ("Fragment duration = %"GST_TIME_FORMAT, GST_TIME_ARGS (fragment->dur));
    found_duration = TRUE;
  }

  /*parsing time attribute */
  fragment->time = ssm_parse_get_xml_prop_uint64 (parser, fragment_node, "t", -1);
  if (fragment->time != -1) {
    GST_DEBUG ("Fragment time = %"GST_TIME_FORMAT, GST_TIME_ARGS(fragment->time));
    found_time = TRUE;
  }

  if (!found_time && !found_duration) {
    GST_ERROR ("Both time & duration attributes are NOT present.. ERROR");
    return FALSE;
  } else if (!found_time && found_duration) {
    GList *prev_fragment_list = NULL;
    GstSSMFragmentNode *prev_fragment = NULL;

    GST_DEBUG ("Only Duration attribute is present...");
    if (stream->fragment_lists) {
    prev_fragment_list = g_list_last(stream->fragment_lists);
    if (NULL == prev_fragment_list) {
      GST_ERROR ("Error last fragment lists are not present..\n");
      return FALSE;
    }
    prev_fragment = (GstSSMFragmentNode *)prev_fragment_list->data;
    fragment->time = prev_fragment->time +  prev_fragment->dur;
    GST_DEBUG ("Fragment time = %llu", fragment->time);
    } else {
      GST_INFO ("Frament list is empty, assuming it as first fragment...");
      fragment->time = 0;
    }
  } else if (found_time && !found_duration) {
    xmlNodePtr next_fragment_node = NULL;
    guint64 next_ts = 0;

    GST_DEBUG ("Only time attribute is present...");

    next_fragment_node = fragment_node->next;
    if (next_fragment_node) {
      next_ts = ssm_parse_get_xml_prop_uint64 (parser, fragment_node, "t", -1);
      if (next_ts == -1) {
        GST_ERROR ("Next fragment time not present to calculate duration of present fragment...");
        return FALSE;
      } else {
        fragment->dur = next_ts - fragment->time;
        GST_DEBUG ("Current fragment duration = %"GST_TIME_FORMAT, GST_TIME_ARGS(fragment->dur));
      }
    } else {
      GST_ERROR ("Next fragment not present to calculate duration of present fragment...");
      return FALSE;
    }
  }

#if 1
  /*error check for timestamps as specified in spec */
  {
    xmlNodePtr next_fragment_node = NULL;
    unsigned long long next_ts = 0;

    next_fragment_node = fragment_node->next;
    if (next_fragment_node) {
      next_ts = ssm_parse_get_xml_prop_uint64 (parser, fragment_node, "t", -1);
      if (next_ts != -1) {
        GST_DEBUG ("Next Fragment time = %llu and Current fragment time = %llu\n", next_ts,  fragment->time);

        if ( next_ts < fragment->time) {
          GST_ERROR ("Error in timestamp sequence...");
          return FALSE;
        }
      }
    }
  }
#endif

  stream->stream_duration += fragment->dur;

  stream->fragment_lists = g_list_append (stream->fragment_lists, fragment);
  GST_DEBUG ("Added fragment node to list...");
  // TODO: Need to invetigate on TrackFragmentIndex Attribute

  return TRUE;
}

static gboolean
ssm_parse_stream_index_node (GstSSMParse *parser, xmlNodePtr stream_node)
{
  GstSSMStreamNode *stream = NULL;
  xmlNodePtr stream_children = NULL;

  g_return_val_if_fail (parser != NULL, FALSE);
  g_return_val_if_fail (stream_node != NULL, FALSE);

  stream = g_slice_new0 (GstSSMStreamNode);
  if (stream == NULL) {
    GST_WARNING ("Allocation of stream node failed!");
    return FALSE;
  }

  stream->frag_lock = g_mutex_new ();
  stream->frag_cond = g_cond_new ();

  /* Type, Chunks, QualityLevels are MUST attributes */
  stream->StreamType = (gchar *)xmlGetProp(stream_node, (const xmlChar *) "Type");
  if (NULL == stream->StreamType) {
    GST_ERROR ("Type attribute is not present");
    return FALSE;
  }
  GST_DEBUG ("Type = %s", stream->StreamType);

  if (!strncmp ((char *)stream->StreamType, "video", 5))
    stream->type = SS_STREAM_VIDEO;
  else if (!strncmp ((char *)stream->StreamType, "audio", 5))
    stream->type = SS_STREAM_AUDIO;
  else if (!strncmp ((char *)stream->StreamType, "text", 4))
    stream->type = SS_STREAM_TEXT;
  else {
    GST_ERROR ("UnKnown Stream type...");
    return FALSE;
  }

  /* Optional SubType Attribute */
  stream->StreamSubType = (gchar *)xmlGetProp(stream_node, (const xmlChar *) "Subtype");
  if (stream->StreamSubType)
    GST_DEBUG ("StreamSubType = %s", stream->StreamSubType);

  stream->StreamTimeScale = ssm_parse_get_xml_prop_uint64 (parser, stream_node, "TimeScale", parser->RootNode->TimeScale);
  GST_LOG("StreamTimeScale = %"G_GUINT64_FORMAT, stream->StreamTimeScale);

  /* Optional StreamName Attribute */
  stream->StreamName = (gchar *)xmlGetProp(stream_node, (const xmlChar *) "Name");
  if (stream->StreamName)
    GST_DEBUG ("StreamName = %s", stream->StreamName);

  // TODO: need to understand more on this chunks whether mandatory or not in LIVE case
  stream->nChunks = ssm_parse_get_xml_prop_uint (parser, stream_node, "Chunks", 0);
  if (!stream->nChunks && !parser->RootNode->PresentationIsLive) {
    GST_ERROR ("nChunks is zero in VOD case...ERROR");
    return FALSE;
  }
  GST_DEBUG("nChunks = %d", stream->nChunks);

  stream->nQualityLevels = ssm_parse_get_xml_prop_uint (parser, stream_node, "QualityLevels", 0);
  GST_DEBUG("nQualityLevels = %d", stream->nQualityLevels);

  stream->StreamUrl = (gchar *)xmlGetProp(stream_node, (const xmlChar *) "Url");
  if (NULL == stream->StreamUrl) {
    GST_ERROR ("Url Pattern attribute is not present");
    return FALSE;
  }
  GST_DEBUG ("Url = %s", stream->StreamUrl);

  if (stream->type == SS_STREAM_VIDEO) {
    /* Video stream specific attributes */
    stream->MaxWidth = ssm_parse_get_xml_prop_uint (parser, stream_node, "MaxWidth", 0);
    GST_DEBUG("MaxWidth = %d", stream->MaxWidth);

    stream->MaxHeight = ssm_parse_get_xml_prop_uint (parser, stream_node, "MaxHeight", 0);
    GST_DEBUG("MaxHeight = %d", stream->MaxHeight);

    stream->DisplayWidth = ssm_parse_get_xml_prop_uint (parser, stream_node, "DisplayWidth", 0);
    GST_DEBUG("DisplayWidth = %d", stream->DisplayWidth);

    stream->DisplayHeight = ssm_parse_get_xml_prop_uint (parser, stream_node, "DisplayHeight", 0);
    GST_DEBUG("DisplayHeight = %d", stream->DisplayHeight);
  }

  /* get the children nodes */
  stream_children = stream_node->xmlChildrenNode;
  if (NULL == stream_children) {
    GST_ERROR ("No Children for StreamIndex Node...\n");
    return FALSE;
  }

  /* Parse StreamIndex Children Nodes i.e. QualityLevel */
  while (stream_children) {
    if ( xmlIsBlankNode (stream_children)) {/* skip blank nodes */
      stream_children = stream_children->next;
      continue;
    }
    if (!xmlStrcmp(stream_children->name, (const xmlChar *) "QualityLevel")) {
      if (!ssm_parse_quality_node (parser, stream, stream_children)) {
        GST_ERROR ("failed to parse quality node");
        return FALSE;
      }
    } else if (!xmlStrcmp(stream_children->name, (const xmlChar *) "c")) {
      if (!ssm_parse_fragment_node (parser, stream, stream_children)) {
        GST_ERROR ("failed to parse fragment node");
        return FALSE;
      }
    } else {
      GST_WARNING (" ==== >>>>>> Unknow ChildrenNode in StreamNode : %s", stream_children->name);
    }
    stream_children = stream_children->next;
  }

  /* sort the quality lists */
  if (stream->quality_lists && (g_list_length(stream->quality_lists) > 1)) {
    stream->quality_lists =  g_list_sort (stream->quality_lists,  (GCompareFunc) ssm_parser_sort_qualitylevels_by_bitrate);
  }

  parser->RootNode->streams[stream->type] = g_list_append (parser->RootNode->streams[stream->type], stream);

  return TRUE;

}


static gboolean
ssm_parse_protection_node (GstSSMParse *parser, xmlNodePtr protection_node)
{
  xmlChar *xml_string = NULL;

  g_return_val_if_fail (parser != NULL, FALSE);
  g_return_val_if_fail (protection_node != NULL, FALSE);

  parser->RootNode->ProtectNode = g_slice_new0 (GstSSMFragmentNode);
  if (NULL == parser->RootNode->ProtectNode) {
    GST_ERROR ("Failed to allocate memory...\n");
    return FALSE;
  }

  parser->RootNode->ProtectNode->SystemID = NULL;
  parser->RootNode->ProtectNode->Content = NULL;
  parser->RootNode->ProtectNode->ContentSize = 0;

  if (!xmlStrcmp(protection_node->name, (const xmlChar *) "ProtectionHeader")) {
    parser->RootNode->ProtectNode->SystemID = (unsigned char *) xmlGetProp(protection_node, (const xmlChar *) "SystemID");
    if (NULL == parser->RootNode->ProtectNode->SystemID) {
      GST_ERROR ("System ID is not present... need to decide ERROR or NOT... returning ERROR now\n");
      return FALSE;
    }

    GST_DEBUG ("system ID = %s\n", parser->RootNode->ProtectNode->SystemID);

    if (!strncasecmp ((char *)parser->RootNode->ProtectNode->SystemID,
           "9A04F079-9840-4286-AB92-E65BE0885F95",
           36)) {
      g_print ("======== >>>>>>>. Content is encrypted using PLAYREADY\n");
    } else {
      GST_ERROR ("\n\n ******** UN-supported encrypted content... *********\n\n");
      return FALSE;
    }

    xml_string = xmlNodeGetContent(protection_node);
    if (NULL == xml_string) {
      GST_ERROR ("Content is not present... need to decide ERROR or NOT\n");
      return FALSE;
    } else {
      gsize content_size = 0;

      g_print ("Content = %s\n", xml_string);

      parser->RootNode->ProtectNode->Content = g_base64_decode ((gchar*)xml_string, &content_size);
      if (NULL == parser->RootNode->ProtectNode->Content) {
        GST_ERROR ("Failed to do base64 decoding...\n");
        free (xml_string);
        xml_string = NULL;
        return FALSE;
      }

      parser->RootNode->ProtectNode->ContentSize = content_size;
      free (xml_string);
      xml_string = NULL;

      GST_DEBUG ("ProtectionNode content = %s and size = %d\n", parser->RootNode->ProtectNode->Content, content_size);
    }
  } else {
    GST_ERROR ("ProtectionHeader is NOT PRESENT in Protection node...ERROR\n");
    return FALSE;
  }

  GST_LOG ("successfully parsed protectionheader node...");
  return TRUE;
}

static gboolean
ssm_parse_root_node (GstSSMParse *parser, xmlNodePtr root_node)
{
  int i = 0;

  g_return_val_if_fail (parser != NULL, FALSE);
  g_return_val_if_fail (root_node != NULL, FALSE);

  parser->RootNode = g_slice_new0 (GstSSMRootNode);
  if (parser->RootNode == NULL) {
    GST_WARNING ("Allocation of root node failed!");
    return FALSE;
  }

  for (i = 0; i < SS_STREAM_NUM; i++) {
    parser->RootNode->streams[i] = NULL;
  }

  parser->RootNode->ProtectNode = NULL;

  /* MANDATORY : parsing MajorVersion attribute */
  parser->RootNode->MajorVersion = ssm_parse_get_xml_prop_uint (parser, root_node, "MajorVersion", -1);
  if (parser->RootNode->MajorVersion != 2) {
    GST_ERROR("Majorversion should be 2");
    return FALSE;
  }
  GST_LOG("SmoothStreamingMedia :: Majorversion = %d", parser->RootNode->MajorVersion);

  /* MANDATORY : parsing MinorVersion attribute */
  parser->RootNode->MinorVersion = ssm_parse_get_xml_prop_uint (parser, root_node, "MinorVersion", 0);
  GST_LOG("SmoothStreamingMedia :: MinorVersion = %d", parser->RootNode->MinorVersion);

  parser->RootNode->TimeScale = ssm_parse_get_xml_prop_uint64 (parser, root_node, "TimeScale", 10000000);
  GST_LOG("SmoothStreamingMedia :: TimeScale = %"G_GUINT64_FORMAT, parser->RootNode->TimeScale);

  parser->RootNode->Duration = ssm_parse_get_xml_prop_uint64 (parser, root_node, "Duration", -1);
  GST_LOG("SmoothStreamingMedia :: Duration = %"G_GUINT64_FORMAT, parser->RootNode->Duration);

  parser->RootNode->PresentationIsLive = ssm_parse_get_xml_prop_boolean (parser, root_node, "IsLive");
  GST_LOG("SmoothStreamingMedia :: IsLive = %d", parser->RootNode->PresentationIsLive);

  /* valid for live presentation only*/
  if (parser->RootNode->PresentationIsLive) {

    parser->RootNode->LookAheadCount = ssm_parse_get_xml_prop_uint (parser, root_node, "LookaheadCount", -1);
    GST_LOG("SmoothStreamingMedia :: LookaheadCount = %d", parser->RootNode->LookAheadCount);

    if (parser->RootNode->LookAheadCount == -1) {
     GST_INFO ("fallback case of lookaheadcount...");
     parser->RootNode->LookAheadCount = ssm_parse_get_xml_prop_uint (parser, root_node, "LookAheadFragmentCount", -1);
     GST_LOG("SmoothStreamingMedia :: LookAheadFragmentCount = %d", parser->RootNode->LookAheadCount);
    }

    parser->RootNode->DVRWindowLength = ssm_parse_get_xml_prop_uint64 (parser, root_node, "DVRWindowLength", 0);
    GST_LOG("SmoothStreamingMedia :: DVRWindowLength = %"G_GUINT64_FORMAT, parser->RootNode->DVRWindowLength);
    if (parser->RootNode->DVRWindowLength == 0)
      GST_INFO ("DVR Window Length is zero...means INFINITE");
  }

  return TRUE;
}


gboolean
gst_ssm_parse_manifest (GstSSMParse *parser, char *data, unsigned int size)
{
  xmlNodePtr root = NULL;
  xmlNodePtr curnode = NULL;
  xmlDocPtr doc;
  int i;

  g_return_val_if_fail (parser != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (size != 0, FALSE);

  /* parse manifest xml file */
  doc = xmlReadMemory ((gchar *) data, size, NULL, NULL, 0);
  if (doc == NULL ) {
    GST_ERROR("failed to parse manifest file...");
    goto error;
  }

  /* Get the root element */
  root = xmlDocGetRootElement(doc);
  if (root == NULL) {
    GST_ERROR("no ROOT node in manifest file...");
    goto error;
  }

  if (!xmlStrcmp(root->name, (const xmlChar *) "SmoothStreamingMedia"))  {
    /* parsing of ROOT Node SmoothStreamingMedia attributes */
    if (!ssm_parse_root_node (parser, root)) {
      GST_ERROR("failed to parse root node...");
      goto error;
    }
    GST_DEBUG ("Parsing ROOT element is successful");
  } else {
    GST_ERROR ("SmoothStreamingMedia ROOT element is not present...");
    goto error;
  }

  /* moving to children nodes */
  curnode = root->xmlChildrenNode;

  while (curnode) {
    if (xmlIsBlankNode (curnode)) {/* skip blank node */
      curnode = curnode->next;
      continue;
    }

    if (!xmlStrcmp(curnode->name, (const xmlChar *)"StreamIndex")) {
      /* Parsing Stream Node */
      if (!ssm_parse_stream_index_node (parser, curnode)) {
        GST_ERROR ("failed to parse stream index node...");
        return FALSE;
      }
    } else if (!xmlStrcmp(curnode->name, (const xmlChar *) "Protection")) {
      xmlNodePtr protect_node;

      /* get the children */
      protect_node = curnode->xmlChildrenNode;
      if (NULL == protect_node) {
        GST_ERROR ("No Children for Protection Node...\n");
        return FALSE;
      } else {
        if (!ssm_parse_protection_node (parser, protect_node)) {
          GST_ERROR ("failed to parse protectionheader node");
          return FALSE;
        }
      }
    }
    curnode = curnode->next;
  }

  /* Move to last fragment when presentation is LIVE */
  if (parser->RootNode->PresentationIsLive) {
    for (i = 0; i < SS_STREAM_NUM; i++) {
      if (parser->RootNode->streams[i]) {
        GST_INFO ("Live presentation, so moved to last node...");
        ((GstSSMStreamNode *)(parser->RootNode->streams[i]->data))->fragment_lists =
			g_list_last (((GstSSMStreamNode *)(parser->RootNode->streams[i]->data))->fragment_lists);
      }
    }
  }

  parser->ns_start = 0;

  /* get new segment start */
  for (i = 0; i < SS_STREAM_NUM; i++) {
    GstSSMStreamNode *stream = NULL;
    GList *frag_list = NULL;
    guint64 start_ts = GST_CLOCK_TIME_NONE;

    if (parser->RootNode->streams[i]) {
      /* Move to last fragment when presentation is LIVE */
      if (parser->RootNode->PresentationIsLive) {
        GST_INFO ("Live presentation, so moved to last node...");
        ((GstSSMStreamNode *)(parser->RootNode->streams[i]->data))->fragment_lists =
            g_list_last (((GstSSMStreamNode *)(parser->RootNode->streams[i]->data))->fragment_lists);
      }

      stream = (parser->RootNode->streams[i])->data;
      frag_list = stream->fragment_lists;
      start_ts = ((GstSSMFragmentNode *)frag_list->data)->time;

      GST_LOG ("ns_start = %"G_GUINT64_FORMAT" and start_ts[%s] = %"G_GUINT64_FORMAT,
          parser->ns_start, ssm_parse_get_stream_name(i), start_ts);

      /* take MAX of stream's start as new_segment start */
      parser->ns_start = START_TS_MAX (parser->ns_start, start_ts);
    }
  }

  GST_INFO ("ns_start = %"G_GUINT64_FORMAT, parser->ns_start);

  if (doc) {
    xmlFreeDoc(doc);
    doc = NULL;
  }

  GST_DEBUG ("successfully parsed manifest");

  return TRUE;

error:
  if (doc) {
    xmlFreeDoc(doc);
    doc = NULL;
  }

  return FALSE;
}

/* Only supporting url in 'QualityLevels({bitrate})/Fragments(xxxxxx={start time})'
  * FIXME : Add support for any */
gboolean
gst_ssm_parse_get_next_fragment_url (GstSSMParse *parser, SS_STREAM_TYPE stream_type, gchar **uri, guint64 *start_ts)
{
  GstSSMStreamNode *stream = NULL;
  GList *frag_list = NULL;
  GPtrArray *strs;
  guint64 time = 0;
  gchar **splitter1 = NULL;
  gchar **splitter2 = NULL;
  gchar **splitter3 = NULL;

  MANIFEST_LOCK(parser);
  stream = (parser->RootNode->streams[stream_type])->data; //get current video stream
  g_mutex_lock (stream->frag_lock);

  if (stream->fraglist_eos) {
    if (parser->RootNode->PresentationIsLive) {
      /* Live Presentation need to wait for next uri */
      g_print ("waiting for next URI in LIVE presentation...\n");
      g_cond_wait (stream->frag_cond, stream->frag_lock);
      g_print ("Received signal after appending new URI...move to next now\n");
     ((GstSSMStreamNode *)(parser->RootNode->streams[stream_type]->data))->fragment_lists =
	 g_list_next (((GstSSMStreamNode *)(parser->RootNode->streams[stream_type]->data))->fragment_lists);
    } else {
      /* VOD presentation reached EOS */
      GST_INFO("Fragment list is empty in VOD case...");
      g_mutex_unlock (stream->frag_lock);
      MANIFEST_UNLOCK(parser);
      return FALSE;
    }
  }

  stream = (parser->RootNode->streams[stream_type])->data; //get current video stream

  frag_list = stream->fragment_lists;

  strs = g_ptr_array_new ();

  /* adding presentation uri */
  g_ptr_array_add (strs, g_strdup(parser->presentation_uri));

  splitter1 = g_strsplit (stream->StreamUrl, "{", 3);

  /* add stream url till first '{' */
  g_ptr_array_add (strs, g_strdup(splitter1[0]));

  /* adding bitrate param */
  g_ptr_array_add (strs, g_strdup_printf ("%d", ((GstSSMQualityNode *)stream->quality_lists->data)->bitrate));

  /* tokenize based on '}' */
  splitter2 = g_strsplit (splitter1[1], "}", 2);

  g_ptr_array_add (strs, g_strdup(splitter2[1]));

  time = ((GstSSMFragmentNode *)frag_list->data)->time;

  /* adding fragment time */
  g_ptr_array_add (strs, g_strdup_printf ("%llu", time));

  *start_ts = gst_util_uint64_scale (time, GST_SECOND, parser->RootNode->TimeScale);

  /* tokenize based on '}' */
  splitter3 = g_strsplit (splitter1[2], "}", 2);

  g_ptr_array_add (strs, g_strdup(splitter3[1]));

  /* add a terminating NULL */
  g_ptr_array_add (strs, NULL);

  if (frag_list = g_list_next (((GstSSMStreamNode *)(parser->RootNode->streams[stream_type]->data))->fragment_lists)) {
    ((GstSSMStreamNode *)(parser->RootNode->streams[stream_type]->data))->fragment_lists = frag_list;
  } else {
    GST_INFO ("Reached end of fragment list...\n");
    ((GstSSMStreamNode *)(parser->RootNode->streams[stream_type]->data))->fraglist_eos = TRUE;
  }

  *uri = g_strjoinv (NULL, (gchar **) strs->pdata);
  g_strfreev ((gchar **) g_ptr_array_free (strs, FALSE));

  g_mutex_unlock (stream->frag_lock);

  MANIFEST_UNLOCK(parser);

  return TRUE;
}

GstCaps *
ssm_parse_get_stream_caps (GstSSMParse *parser, SS_STREAM_TYPE stream_type)
{
  GstSSMStreamNode *stream = NULL;
  MANIFEST_LOCK(parser);

  stream = parser->RootNode->streams[stream_type]->data;
  GstCaps *caps = NULL;
  if (stream_type == SS_STREAM_VIDEO) {
    caps =  ssm_prepare_video_caps (parser, stream);
  } else if (stream_type == SS_STREAM_AUDIO) {
    caps =  ssm_prepare_audio_caps (parser, stream);
  } else if (stream_type == SS_STREAM_TEXT) {
    caps =  ssm_prepare_text_caps (parser, stream);
  }
  MANIFEST_UNLOCK(parser);
  return caps;
}

GstCaps *
ssm_prepare_video_caps (GstSSMParse *parser, GstSSMStreamNode *stream)
{
  GstSSMQualityNode *cur_quality_node =  (GstSSMQualityNode *)(stream->quality_lists->data);
  GstBuffer *codec_data = NULL;
  GstCaps *caps = NULL;

  codec_data = gst_buffer_new ();
  if (!codec_data) {
    GST_ERROR ("failed to allocate buffer");
    return NULL;
  }

  if (!strncmp ((char *)cur_quality_node->fourcc, "H264", 4)) {
    /* converting NALU codec data to 3GPP codec data format */
    if (!convert_NALUnitDCI_to_PacktizedDCI (cur_quality_node->codec_data, &(GST_BUFFER_DATA(codec_data)), &GST_BUFFER_SIZE(codec_data))) {
      GST_ERROR ("Error in converting NALUDCI to Packetized DCI...\n");
      return NULL;
    }
    GST_BUFFER_MALLOCDATA(codec_data) = GST_BUFFER_DATA(codec_data);
  } else if (!strncmp ((char *)cur_quality_node->fourcc, "WVC1", 4)) {
    unsigned char *codec_data = cur_quality_node->codec_data;
    unsigned int DCI_len = strlen ((char *)cur_quality_node->codec_data);
    gchar tmp[3] = {0, };
    gint val = 0;
    gint codec_data_len = 0;
    gint idx = 0;

    codec_data_len = (DCI_len >>1);

    codec_data = gst_buffer_new_and_alloc (codec_data_len);
    if (!codec_data) {
      GST_ERROR ("Failed to allocate memory..\n");
      return FALSE;
    }

    /* copy codec data */
    while (DCI_len) {
      memset (tmp, 0x00, 3);
      strncpy ((char*)tmp, (char*)codec_data, 2);
      tmp[2] = '\0';
      if (!int_from_string ((char*)tmp, NULL, &val, 16)) {
        GST_ERROR ("Failed to int from string...");
        return NULL;
      }
      (GST_BUFFER_DATA(codec_data))[idx] = val;
      codec_data += 2;
      DCI_len = DCI_len - 2;
      idx++;
    }
  }

  if (!strncmp ((char *)cur_quality_node->fourcc, "H264", 4)) {
    /* prepare H264 caps */
    caps = gst_caps_new_simple ("video/x-h264",
                  "width", G_TYPE_INT, cur_quality_node->max_width,
                  "height", G_TYPE_INT, cur_quality_node->max_height,
                  "framerate", GST_TYPE_FRACTION, 30, 1,
                  "stream-format", G_TYPE_STRING, "avc",
                  "alignment", G_TYPE_STRING, "au",
                  "codec_data", GST_TYPE_BUFFER, codec_data,
                  NULL);
  } else if (!strncmp ((char *)cur_quality_node->fourcc, "WVC1", 4)) {
    /* prepare VC1 caps */
    caps = gst_caps_new_simple ("video/x-wmv",
                  "width", G_TYPE_INT, cur_quality_node->max_width,
                    "height", G_TYPE_INT, cur_quality_node->max_height,
                    "framerate", GST_TYPE_FRACTION, 30, 1,
                    "wmvversion", G_TYPE_INT, 3,
                    "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('W', 'V', 'C', '1'),
                    "codec_data", GST_TYPE_BUFFER, codec_data,
                    NULL);
  } else {
    char *s;
    /* prepare gst generic caps caps */
    GST_ERROR ("Wrong VIDEO fourcc...");
    s = g_strdup_printf ("video/x-gst-fourcc-%s", "unknown");
    caps = gst_caps_new_simple (s,
                     "width", G_TYPE_INT, cur_quality_node->max_width,
                     "height", G_TYPE_INT, cur_quality_node->max_height,
                     "framerate", GST_TYPE_FRACTION, 30, 1,
                      "codec_data", GST_TYPE_BUFFER, codec_data,
                     NULL);
  }

  gchar *caps_name = gst_caps_to_string(caps);
  GST_INFO ( "prepared video caps : %s\n", caps_name);
  g_free(caps_name);

  return caps;
}

GstCaps *
ssm_prepare_audio_caps (GstSSMParse *parser, GstSSMStreamNode *stream)
{
  GstSSMQualityNode *cur_quality_node =  (GstSSMQualityNode *)(stream->quality_lists->data);
  GstBuffer *codec_data = NULL;
  GstCaps *caps = NULL;

  if (cur_quality_node->codec_data &&
     ((!strncmp ((char *)cur_quality_node->fourcc, "AACL", 4)) || !strncmp ((char *)cur_quality_node->fourcc, "WMAP", 4))) {
    guint DCI_len = strlen ((char *)cur_quality_node->codec_data);
    gchar *dci = cur_quality_node->codec_data;
    gchar tmp[3] = {0, };
    gint val = 0;
    gint codec_data_len = (DCI_len >>1);
    gint idx = 0;

    codec_data = gst_buffer_new_and_alloc (codec_data_len);
    if (NULL == codec_data) {
      GST_ERROR ("Failed to allocate memory..\n");
      return NULL;
    }

    /* copy codec data */
    while (DCI_len) {
      memset (tmp, 0x00, 3);
      strncpy ((char*)tmp, (char*)dci, 2);
      tmp[2] = '\0';
      if (!int_from_string ((char*)tmp, NULL, &val , 16)) {
        GST_ERROR ("Failed to int from string...");
        return NULL;
      }
      (GST_BUFFER_DATA(codec_data))[idx] = val;
      dci += 2;
      DCI_len = DCI_len - 2;
      //g_print ("val = 0x%02x, codec_data_length = %d, idx = %d\n", val, DCI_len, idx);
      idx++;
    }
  } else {
    GST_ERROR ("\n\n\nUnsupported Audio Codec Fourcc...\n\n\n");
    return NULL;
  }

  if (!strncmp ((char *)cur_quality_node->fourcc, "AACL", 4)) {
    caps = gst_caps_new_simple ("audio/mpeg",
                          "mpegversion", G_TYPE_INT, 4,
                          "framed", G_TYPE_BOOLEAN, TRUE,
                          "stream-format", G_TYPE_STRING, "raw",
                          "rate", G_TYPE_INT, (int) cur_quality_node->samplingrate,
                          "channels", G_TYPE_INT, cur_quality_node->channels,
                          "codec_data", GST_TYPE_BUFFER, codec_data,
                          NULL);
  } else if (!strncmp ((char *)cur_quality_node->fourcc, "WMAP", 4)) {
    caps = gst_caps_new_simple ("audio/x-wma",
                        "rate", G_TYPE_INT, (int) cur_quality_node->samplingrate,
                         "channels", G_TYPE_INT, cur_quality_node->channels,
                        NULL);
  } else {
    char *s;

    GST_ERROR ("Wrong Audio fourcc...");
    s = g_strdup_printf ("audio/x-gst-fourcc-%s", "unknown");
    caps = gst_caps_new_simple (s,
                            "rate", G_TYPE_INT, (int) cur_quality_node->samplingrate,
                             "channels", G_TYPE_INT, cur_quality_node->channels,
                             NULL);
  }
  gchar *caps_name = gst_caps_to_string(caps);
  GST_INFO ( "prepared video caps : %s\n", caps_name);
  g_free(caps_name);

  return caps;
}

GstCaps *
ssm_prepare_text_caps (GstSSMParse *parser, GstSSMStreamNode *stream)
{
  // TODO: Yet to add support for text caps
  return NULL;
}

SS_BW_MODE
gst_ssm_parse_switch_qualitylevel (GstSSMParse *parser, guint drate)
{
  GstSSMStreamNode *stream =  parser->RootNode->streams[SS_STREAM_VIDEO]->data;
  guint bitrate = ((GstSSMQualityNode *)stream->quality_lists->data)->bitrate;
  SS_BW_MODE ret = SS_MODE_NO_SWITCH;

  /* check for upward transition */
  while (drate > (bitrate + BITRATE_SWITCH_UPPER_THRESHOLD * bitrate)) {
    if (g_list_next (stream->quality_lists)) {
      stream->quality_lists = g_list_next (stream->quality_lists);
      g_print ("Move to next quality level : drate = %d and bitrate = %d\n", drate, ((GstSSMQualityNode *)stream->quality_lists->data)->bitrate);
      ret = SS_MODE_AV;
      bitrate = ((GstSSMQualityNode *)stream->quality_lists->data)->bitrate;
    } else {
      g_print ("Already at MAX Bitrate possible...\n");
      ret = SS_MODE_NO_SWITCH;
      break;
    }
  }

  /* check for downward transition */
  while (drate < (bitrate + BITRATE_SWITCH_LOWER_THRESHOLD * bitrate)) {
    if (g_list_previous (stream->quality_lists)) {
      stream->quality_lists = g_list_previous (stream->quality_lists);
      g_print ("Move to previous quality level : drate = %d and bitrate = %d\n", drate, ((GstSSMQualityNode *)stream->quality_lists->data)->bitrate);
      bitrate = ((GstSSMQualityNode *)stream->quality_lists->data)->bitrate;
    } else {
      g_print ("Reached MIN video bitrate possible...\n");
      if (GST_SSM_PARSE_IS_LIVE_PRESENTATION(parser)) {
        g_print ("Going to audio-only because of LIVE...\n");
        ret = SS_MODE_AONLY;
      } else {
        ret = SS_MODE_AV;
      }
      break;
    }
  }

  return ret;
}

gboolean
gst_ssm_parse_append_next_fragment (GstSSMParse *parser, SS_STREAM_TYPE stream_type, guint64 timestamp, guint64 duration)
{
  GstSSMStreamNode *stream = NULL;
  GstSSMFragmentNode *new_fragment = NULL;
  GstSSMFragmentNode *last_fragment = NULL;
  GList *last_node = NULL;

  g_return_val_if_fail (parser != NULL, FALSE);

  /*get current stream based on stream_type */
  stream = (parser->RootNode->streams[stream_type])->data;

  g_return_val_if_fail (stream->fragment_lists, FALSE);

  g_mutex_lock (stream->frag_lock);

  last_node = g_list_last(stream->fragment_lists);

  last_fragment = (GstSSMFragmentNode *)(last_node->data);

  if (last_fragment->time < timestamp) {

    GST_LOG ("+++++ last_fragment time = %llu and current recd = %llu +++++\n", last_fragment->time, timestamp);

    /* allocate new fragment node */
    new_fragment = g_slice_new0 (GstSSMFragmentNode);
    if (new_fragment == NULL) {
      GST_ERROR ("Allocation of fragment failed!");
      return FALSE;
    }

    if (duration == GST_CLOCK_TIME_NONE) {
      /* useful when lookahead count is zero */
      // TODO: need to check how to handle, when there is discontinuity
      duration = timestamp - last_fragment->time;
    }

    // TODO: need to handle when either time or duration present OR if both are non proper values
    new_fragment->dur = duration;
    new_fragment->time = timestamp;
    new_fragment->num = 0;

    /* add the new fragment duration to total stream duration */
    stream->stream_duration += duration;

    /* append new fragment list to stream fragment list */
   ((GstSSMStreamNode *)(parser->RootNode->streams[stream_type]->data))->fragment_lists =
	 g_list_append (((GstSSMStreamNode *)(parser->RootNode->streams[stream_type]->data))->fragment_lists, new_fragment);

    GST_DEBUG ("+++++ Appened new '%s' URL and signaling the condition and duration = %llu ++++++\n",
        ssm_parse_get_stream_name(stream_type), stream->stream_duration);

    if (stream->fraglist_eos) {
      GST_INFO ("Need to move the list now to next.. as we received new one\n");
      ((GstSSMStreamNode *)(parser->RootNode->streams[stream_type]->data))->fragment_lists =
	 g_list_next (((GstSSMStreamNode *)(parser->RootNode->streams[stream_type]->data))->fragment_lists);
      stream->fraglist_eos = FALSE;
    }
    /* signal fragment wait */
    g_cond_signal (stream->frag_cond);

  } else {
    g_print ("--------- Received '%s' fragment is less than Last fragment in the list -----------\n", ssm_parse_get_stream_name(stream_type));
  }

  g_mutex_unlock (stream->frag_lock);

  return TRUE;
}

gboolean
gst_ssm_parse_seek_manifest (GstSSMParse *parser, guint64 seek_time)
{
  gint i = 0;
  GstSSMStreamNode *stream = NULL;
  guint64 stream_time = -1;
  guint64 start_ts = -1;

  parser->ns_start = 0;

  for (i = 0; i < SS_STREAM_NUM; i++) {
    if (parser->RootNode->streams[i]) {
      stream = parser->RootNode->streams[i]->data; // get current stream
      stream_time = ((GstSSMFragmentNode *)stream->fragment_lists->data)->time;
      stream_time = gst_util_uint64_scale (stream_time, GST_SECOND,
          GST_SSM_PARSE_GET_TIMESCALE(parser));

      if (seek_time > stream_time) {
        /* forward seek */
        while (seek_time > stream_time) {
          stream->fragment_lists = g_list_next (stream->fragment_lists);
          if(stream->fragment_lists && stream->fragment_lists->data) {
            stream_time = gst_util_uint64_scale (((GstSSMFragmentNode *)stream->fragment_lists->data)->time, GST_SECOND,
              GST_SSM_PARSE_GET_TIMESCALE(parser));
            GST_LOG ("seek time = %"GST_TIME_FORMAT", cur_time = %"GST_TIME_FORMAT,
              GST_TIME_ARGS(seek_time), GST_TIME_ARGS(stream_time));
          }
        }

        /* moving to fragment before our seeked time */
        stream->fragment_lists = g_list_previous (stream->fragment_lists);
        if(stream->fragment_lists && stream->fragment_lists->data)
          start_ts = ((GstSSMFragmentNode *)stream->fragment_lists->data)->time;
      } else if (seek_time < stream_time) {
        /* backward seek */
        while (seek_time < stream_time) {
          stream->fragment_lists = g_list_previous (stream->fragment_lists);
          if(stream->fragment_lists && stream->fragment_lists->data) {
            stream_time = gst_util_uint64_scale (((GstSSMFragmentNode *)stream->fragment_lists->data)->time, GST_SECOND,
                                              GST_SSM_PARSE_GET_TIMESCALE(parser));
            GST_LOG ("seek time = %"GST_TIME_FORMAT", cur_time = %"GST_TIME_FORMAT,
              GST_TIME_ARGS(seek_time), GST_TIME_ARGS(stream_time));
            start_ts = ((GstSSMFragmentNode *)stream->fragment_lists->data)->time;
          }
        }
      } else {
        /* rare case */
        start_ts = ((GstSSMFragmentNode *)stream->fragment_lists->data)->time;
      }

      if (stream->type == SS_STREAM_VIDEO) {
        /* move to least possible bitrate variant*/
        stream->quality_lists = g_list_first (stream->quality_lists);
      }

      GST_INFO ("SEEK : ns_start = %"G_GUINT64_FORMAT" and start_ts[%s] = %"G_GUINT64_FORMAT,
        parser->ns_start, ssm_parse_get_stream_name(i), start_ts);

      /* take max of stream's start as new_segment start */
      parser->ns_start = START_TS_MAX (parser->ns_start, start_ts);
    }
  }

  GST_INFO ("ns_start = %"G_GUINT64_FORMAT, parser->ns_start);

  return TRUE;
}

gboolean
gst_ssm_parse_get_protection_header (GstSSMParse *parser, unsigned char **protection_header, unsigned int *protection_header_len)
{
  if (!parser->RootNode->ProtectNode) {
    *protection_header = NULL;
    *protection_header_len = 0;
    return TRUE;
  }

  if (parser->RootNode->ProtectNode->ContentSize && parser->RootNode->ProtectNode->Content) {
    *protection_header = g_malloc0 (parser->RootNode->ProtectNode->ContentSize);
    if (*protection_header == NULL) {
      GST_ERROR ("failed to allocate memory...");
      return FALSE;
    }

    memcpy (*protection_header, parser->RootNode->ProtectNode->Content, parser->RootNode->ProtectNode->ContentSize);
    *protection_header_len = parser->RootNode->ProtectNode->ContentSize;
  }

  return TRUE;
}

static gboolean
convert_NALUnitDCI_to_PacktizedDCI (unsigned char *nalu_dci, unsigned char **packetized_dci, unsigned int *packetized_dci_len)
{
#ifndef CHECK_NALU_OUT
	gboolean bret = TRUE;
	unsigned char *pps_start = NULL;
	unsigned char *pps_end = NULL;
	unsigned int sps_len = 0;
	unsigned int pps_len = 0;
	unsigned char *sps_data = NULL;
	unsigned char *pps_data = NULL;
	unsigned char *tmp_sps_ptr = NULL;
	unsigned char *tmp_pps_ptr = NULL;
	unsigned char tmp[3] = {0, };
	int val;
	unsigned int idx = 0;

	// TODO: need to generalize this logic for finding multiple SPS and PPS sets and finding SPS/PPS by ANDing 0x1F

	pps_start = strstr ((char*)nalu_dci, "0000000168");
	pps_end = nalu_dci + strlen ((char *)nalu_dci);
	GST_DEBUG ("nalu_dci length= %d\n", strlen ((char *)nalu_dci));

	sps_len = pps_start - nalu_dci;

       GST_DEBUG ("sps length = %d\n", sps_len);

	sps_data = (unsigned char*) malloc (sps_len + 1);
	if (NULL == sps_data)
	{
		GST_ERROR ("Failed to allocate memory..\n");
		bret = FALSE;
		goto exit;
	}

	/* Copy SPS data */
	strncpy ((char*)sps_data, (char *)nalu_dci, sps_len);
	sps_data[sps_len] = '\0';

	tmp_sps_ptr = sps_data;

	GST_DEBUG ("SPS data = %s\n", sps_data);

	pps_len = pps_end - pps_start;
	GST_DEBUG ("pps length = %d\n", pps_len);

	pps_data = (unsigned char*)malloc (pps_len + 1);
	if (NULL == pps_data)
	{
		GST_ERROR ("Failed to allocate memory..\n");
		bret = FALSE;
		goto exit;
	}

	/* Copy PPS data */
	strncpy ((char*)pps_data, (char*)pps_start, pps_len);
	pps_data[pps_len] = '\0';
	tmp_pps_ptr = pps_data;

	GST_DEBUG ("PPS data = %s\n", pps_data);

	/* 6 bytes of metadata */
	*packetized_dci_len = 8;

	*packetized_dci = (unsigned char*) malloc (*packetized_dci_len);
	if (NULL == *packetized_dci)
	{
		GST_ERROR ("Failed to allocate memory..\n");
		bret = FALSE;
		goto exit;
	}

	/* configurationVersion */
	(*packetized_dci)[0] = 0x01;

	/* AVCProfileIndication */
	memset (tmp, 0x00, 3);

	strncpy ((char*)tmp, (char*)sps_data+10, 2);
	tmp[2] = '\0';

	bret = int_from_string ((char*)tmp, NULL, &val , 16);
	if (FALSE == bret)
	{
		GST_ERROR ("Failed to int from string...");
		goto exit;
	}

	(*packetized_dci)[1] = val;

	/* profile_compatibility*/
	memset (tmp, 0x00, 3);
	strncpy ((char*)tmp, (char*)sps_data+12, 2);
	tmp[2] = '\0';
	bret = int_from_string ((char*)tmp, NULL, &val , 16);
	if (FALSE == bret)
	{
		GST_ERROR ("Failed to int from string...");
		goto exit;
	}
	(*packetized_dci)[2] = val;

	/* AVCLevelIndication */
	memset (tmp, 0x00, 3);
	strncpy ((char*)tmp, (char*)sps_data+14, 2);
	tmp[2] = '\0';
	bret = int_from_string ((char*)tmp, NULL, &val , 16);
	if (FALSE == bret)
	{
		GST_ERROR ("Failed to int from string...");
		goto exit;
	}

	(*packetized_dci)[3] = val;

	/* Reserver (6) + LengthSizeMinusOne (2) */
	(*packetized_dci)[4] = 0xff;	 // hardcoding lengthoflength = 4 for present

	/* Reserver (3) + numOfSequenceParameterSets (5) */
	(*packetized_dci)[5] = 0xe1;	 // hardcoding numOfSequenceParameterSets = 1 for present

	/* avoiding NALU start code 0x00 00 00 01 */
	sps_len = sps_len -8;
	sps_data = sps_data + 8;

	(*packetized_dci)[6] = (sps_len >>1) >> 16;
	(*packetized_dci)[7] = (sps_len >>1);

	*packetized_dci_len += (sps_len/2);

	*packetized_dci = (unsigned char*) realloc (*packetized_dci, *packetized_dci_len);
	if (NULL == *packetized_dci)
	{
		GST_ERROR ("Failed to allocate memory..\n");
		bret = FALSE;
		goto exit;
	}

	idx = 8;

	/* convert SPS data and store*/
	while (sps_len)
	{
		memset (tmp, 0x00, 3);
		strncpy ((char*)tmp, (char*)sps_data, 2);
		tmp[2] = '\0';
		bret = int_from_string ((char*)tmp, NULL, &val , 16);
		if (FALSE == bret)
		{
			GST_ERROR ("Failed to int from string...");
			goto exit;
		}
		(*packetized_dci)[idx] = val;
		sps_data += 2;
		sps_len = sps_len - 2;
		//g_print ("val = 0x%02x, sps_len = %d, idx = %d\n", val, sps_len, idx);
		idx++;
	}

	/* avoiding NALU start code 0x00 00 00 01 */
	pps_len = pps_len -8;
	pps_data = pps_data + 8;

	/* no.of PPS sets (1 byte) + Lengthof PPS (2 bytes) + PPS length */
	*packetized_dci_len = *packetized_dci_len + 1 + 2 + (pps_len/2) ;
	*packetized_dci = (unsigned char*)realloc (*packetized_dci, *packetized_dci_len);
	if (NULL == *packetized_dci)
	{
		GST_ERROR ("Failed to allocate memory..\n");
		bret = FALSE;
		goto exit;
	}

	(*packetized_dci)[idx++] = 0x01; // Harding no.of PPS sets
	(*packetized_dci)[idx++] = (pps_len>>1) >> 16;
	(*packetized_dci)[idx++] = (pps_len >>1);

	/* convert PPS data and store */
	while (pps_len)
	{
		memset (tmp, 0x00, 3);
		strncpy ((char*)tmp, (char*)pps_data, 2);
		tmp[2] = '\0';
		bret = int_from_string ((char*)tmp, NULL, &val , 16);
		if (FALSE == bret)
		{
			GST_ERROR ("Failed to int from string...");
			goto exit;
		}

		(*packetized_dci)[idx] = val;
		pps_data += 2;
		pps_len = pps_len - 2;
		//g_print ("val = 0x%02x, pps_len = %d, idx = %d\n", val, pps_len, idx);
		idx++;
	}
	idx = 0;

#if 0
	g_print ("\n\n");

	g_print ("Complete VIDEO packetized_dci: 0x");
	while (idx < *packetized_dci_len)
	{
		g_print ("%02x", (*packetized_dci)[idx]);
		idx++;
	}

	g_print ("\n\n");
#endif

exit:
	if (tmp_sps_ptr)
	{
		free (tmp_sps_ptr);
		tmp_sps_ptr = NULL;
	}
	if (tmp_pps_ptr)
	{
		free (tmp_pps_ptr);
		tmp_pps_ptr = NULL;
	}
	if ((FALSE == bret) && *packetized_dci)
	{
		free (*packetized_dci);
		*packetized_dci = NULL;
	}

	return bret;

#else
       /* code for sending NALU DCI as it is */
       guint nalu_dci_len = strlen ((char *)nalu_dci);
       unsigned char *nalu = nalu_dci;
       guint idx = 0;
	unsigned char tmp[3] = {0, };
	gboolean bret = TRUE;
	int val = 0;

	*packetized_dci_len = (nalu_dci_len/2);

	*packetized_dci = (unsigned char*) malloc (*packetized_dci_len);
	if (NULL == *packetized_dci)
	{
		GST_ERROR ("Failed to allocate memory..\n");
		bret = FALSE;
		goto exit;
	}

	/* copy entire DCI*/
	while (nalu_dci_len)
	{
		memset (tmp, 0x00, 3);
		strncpy ((char*)tmp, (char*)nalu, 2);
		tmp[2] = '\0';
		bret = int_from_string ((char*)tmp, NULL, &val , 16);
		if (FALSE == bret)
		{
			GST_ERROR ("Failed to int from string...");
			goto exit;
		}
		(*packetized_dci)[idx] = val;
		nalu += 2;
		nalu_dci_len = nalu_dci_len - 2;
		//g_print ("val = 0x%02x, dci_len = %d, idx = %d\n", val, nalu_dci_len, idx);
		idx++;
	}

      idx = 0;

      g_print ("\n\n NEW DCI : 0x ");

	while (idx < *packetized_dci_len)
	{
	   g_print ("%02x", (*packetized_dci)[idx] );
	   idx++;
	}

       g_print ("\n\n\n");

exit:

	if ((FALSE == bret) && *packetized_dci)
	{
		free (*packetized_dci);
		*packetized_dci = NULL;
	}

	return bret;
#endif
}




