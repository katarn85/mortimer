#include "gstfecbuffer.h"

#include <stdlib.h>
#include <string.h>

#define GST_FEC_HEADER_LEN 10

/* Note: we use bitfields here to make sure the compiler doesn't add padding
 * between fields on certain architectures; can't assume aligned access either
 */
typedef struct _GstFECHeader
{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  unsigned int csrc_count:4;    					/* CSRC count */
  unsigned int extension:1;             			/* header extension flag */
  unsigned int padding:1;               			/* padding flag */
  unsigned int long_mask:1;       					/* Long mask */
  unsigned int extension_flag:1;        			/* Extension Flag bit */
  unsigned int payload_type_recovery:7;  			/* payload type recovery */
  unsigned int marker:1;        					/* marker bit */
#elif G_BYTE_ORDER == G_BIG_ENDIAN

  unsigned int extension_flag:1;        			/* Extension Flag bit */
  unsigned int long_mask:1;       					/* Long mask */
  unsigned int padding:1;       					/* padding flag */
  unsigned int extension:1;     					/* header extension flag */
  unsigned int csrc_count:4;    					/* CSRC count */
  unsigned int marker:1;        					/* marker bit */
  unsigned int payload_type_recovery:7;  			/* payload type recovery*/
#else
#error "G_BYTE_ORDER should be big or little endian."
#endif
  unsigned int snbase:16;          					/* sequence number */
  unsigned int timestamp_recovery:32;  				/* timestamp  recovery*/
  unsigned int length_recovery:16;      			/* length Recovery */
} GstFECHeader;


typedef struct _GstFECLevelHeader
{
  unsigned int protection_length:16;          		/* protection length*/
  unsigned int mask:16;   							/* mask */
  unsigned int mask_continue:32;      				/* mask continue */
} GstFECLevelHeader;



//FEC main Header
#define GST_FEC_HEADER_EXTENSION_FLAG(data)      			(((GstFECHeader *)(data))->extension_flag)
#define GST_FEC_HEADER_LONG_MASK(data)      				(((GstFECHeader *)(data))->long_mask)
#define GST_FEC_HEADER_PADDING(data)      					(((GstFECHeader *)(data))->padding)
#define GST_FEC_HEADER_EXTENSION(data)    					(((GstFECHeader *)(data))->extension)
#define GST_FEC_HEADER_CSRC_COUNT(data)   					(((GstFECHeader *)(data))->csrc_count)
#define GST_FEC_HEADER_MARKER(data)       					(((GstFECHeader *)(data))->marker)
#define GST_FEC_HEADER_PAYLOAD_TYPE_RECOVERY(data) 			(((GstFECHeader *)(data))->payload_type_recovery)
#define GST_FEC_HEADER_SNBASE(data)          				(((GstFECHeader *)(data))->snbase)
#define GST_FEC_HEADER_TIMESTAMP_RECOVERY(data)    			(((GstFECHeader *)(data))->timestamp_recovery)
#define GST_FEC_HEADER_LENGTH_RECOVERY(data)         		(((GstFECHeader *)(data))->length_recovery)



//For FEC Level HEADER
#define GST_FEC_LEVEL_HEADER_PROTECTION_LENGTH(data)      	(((GstFECLevelHeader *)(data))->protection_length)
#define GST_FEC_LEVEL_HEADER_MASK(data)      				(((GstFECLevelHeader *)(data))->mask)
#define GST_FEC_LEVEL_HEADER_MASK_CONTINUE(data)      		(((GstFECLevelHeader *)(data))->mask_continue)



/**
 * gst_FEC_buffer_allocate_data:
 * @buffer: a #GstBuffer
 * @payload_len: the length of the payload
 * @pad_len: the amount of padding
 * @csrc_count: the number of CSRC entries
 *
 * Allocate enough data in @buffer to hold an FEC packet with @csrc_count CSRCs,
 * a payload length of @payload_len and padding of @pad_len.
 * MALLOCDATA of @buffer will be overwritten and will not be freed.
 * All other FEC header fields will be set to 0/FALSE.
 */
void
gst_fec_buffer_allocate_data (GstBuffer * buffer, guint payload_len,
    guint8 pad_len, guint8 csrc_count)
{
  guint len;
  guint8 *data;

  g_return_if_fail (csrc_count <= 15);
  g_return_if_fail (GST_IS_BUFFER (buffer));

  len = GST_FEC_HEADER_LEN + csrc_count * sizeof (guint32)
      + payload_len + pad_len;

  data = g_malloc (len);
  GST_BUFFER_MALLOCDATA (buffer) = data;
  GST_BUFFER_DATA (buffer) = data;
  GST_BUFFER_SIZE (buffer) = len;

  /* fill in defaults */
  GST_FEC_HEADER_EXTENSION_FLAG(data) = FALSE;
  GST_FEC_HEADER_LONG_MASK(data)  = FALSE;
  GST_FEC_HEADER_PADDING (data)   = FALSE;
  GST_FEC_HEADER_EXTENSION (data) = FALSE;
  GST_FEC_HEADER_CSRC_COUNT (data) = csrc_count;
  GST_FEC_HEADER_MARKER (data) = FALSE;
  GST_FEC_HEADER_PAYLOAD_TYPE_RECOVERY(data) = 0;
  GST_FEC_HEADER_SNBASE (data) = 0;
  GST_FEC_HEADER_TIMESTAMP_RECOVERY(data) = 0;
  GST_FEC_HEADER_LENGTH_RECOVERY(data) =0;
}



/**
 * gst_fec_buffer_get_extension_flag:
 * @buffer: the buffer
 *
 * Check if the extension flag bit is set on the FEC packet in @buffer.
 *
 * Returns: TRUE if @buffer has the extension flag bit set.
 */
inline gboolean
gst_fec_buffer_get_extension_flag (GstBuffer * buffer, guint8 fec_offset)
{
  return GST_FEC_HEADER_EXTENSION_FLAG(GST_BUFFER_DATA (buffer)+ fec_offset);
}

/**
 * gst_fec_buffer_set_extension_flag:
 * @buffer: the buffer
 * @extension_flag: the extension flag
 *
 * Set the extension flag bit on the FEC packet in @buffer to @extension_flag..
 */
inline void
gst_fec_buffer_set_extension_flag (GstBuffer * buffer, guint8 extension_flag,guint8 fec_offset)
{
   GST_FEC_HEADER_EXTENSION_FLAG (GST_BUFFER_DATA (buffer)+ fec_offset) = extension_flag;
}

/**
 * gst_fec_buffer_get_long_mask:
 * @buffer: the buffer
 *
 * Check if the long mask bit is set on the FEC packet in @buffer.
 *
 * Returns: TRUE if @buffer has the long mask bit set.
 */
inline gboolean
gst_fec_buffer_get_long_mask (GstBuffer * buffer, guint8 fec_offset)
{
  return GST_FEC_HEADER_LONG_MASK (GST_BUFFER_DATA (buffer)+ fec_offset);
}

/**
 * gst_fec_buffer_set_long_mask:
 * @buffer: the buffer
 * @long_mask: the long mask
 *
 * Set the long mask bit on the FEC packet in @buffer to @long_mask..
 */
inline void
gst_fec_buffer_set_long_mask(GstBuffer * buffer, guint8 long_mask,guint8 fec_offset)
{
   GST_FEC_HEADER_LONG_MASK (GST_BUFFER_DATA (buffer)+ fec_offset) = long_mask;
}

/**
 * gst_fec_buffer_get_padding:
 * @buffer: the buffer
 *
 * Check if the padding bit is set on the FEC packet in @buffer.
 *
 * Returns: TRUE if @buffer has the padding bit set.
 */
inline gboolean
gst_fec_buffer_get_padding (GstBuffer * buffer, guint8 fec_offset)
{
  return GST_FEC_HEADER_PADDING (GST_BUFFER_DATA (buffer)+ fec_offset);
}

/**
 * gst_fec_buffer_set_padding:
 * @buffer: the buffer
 * @padding: the new padding
 *
 * Set the padding bit on the FEC packet in @buffer to @padding.
 */
inline void
gst_fec_buffer_set_padding (GstBuffer * buffer, gboolean padding,guint8 fec_offset)
{
  GST_FEC_HEADER_PADDING (GST_BUFFER_DATA (buffer)+ fec_offset) = padding;
}


/**
 * gst_fec_buffer_get_extension:
 * @buffer: the buffer
 *
 * Check if the extension bit is set on the FEC packet in @buffer.
 *
 * Returns: TRUE if @buffer has the extension bit set.
 */
inline gboolean
gst_fec_buffer_get_extension (GstBuffer * buffer, guint8 fec_offset)
{
  return GST_FEC_HEADER_EXTENSION (GST_BUFFER_DATA (buffer)+ fec_offset);
}

/**
 * gst_fec_buffer_set_extension:
 * @buffer: the buffer
 * @extension: the new extension
 *
 * Set the extension bit on the FEC packet in @buffer to @extension.
 */
inline void
gst_fec_buffer_set_extension (GstBuffer * buffer, gboolean extension, guint8 fec_offset)
{
  GST_FEC_HEADER_EXTENSION (GST_BUFFER_DATA (buffer)+ fec_offset) = extension;
}

  /**
 * gst_fec_buffer_get_csrc_count:
 * @buffer: the buffer
 *
 * Get the CSRC count of the FEC packet in @buffer.
 *
 * Returns: the CSRC count of @buffer.
 */
inline guint8
gst_fec_buffer_get_csrc_count (GstBuffer * buffer, guint8 fec_offset)
{
  return GST_FEC_HEADER_CSRC_COUNT (GST_BUFFER_DATA (buffer)+ fec_offset);
}
  /**
 * gst_fec_buffer_set_csrc_count:
 * @buffer: the buffer
 * @csrc_count: the new csrc count
 *
 * Set the csrc count of the FEC packet in @buffer to @csrc_count.
 */
inline void
gst_fec_buffer_set_csrc_count (GstBuffer * buffer, guint16 csrc_count, guint8 fec_offset)
{
  GST_FEC_HEADER_CSRC_COUNT (GST_BUFFER_DATA (buffer)+ fec_offset) = csrc_count;
}


  /**
 * gst_fec_buffer_get_marker:
 * @buffer: the buffer
 *
 * Check if the marker bit is set on the FEC packet in @buffer.
 *
 * Returns: TRUE if @buffer has the marker bit set.
 */
inline gboolean
gst_fec_buffer_get_marker (GstBuffer * buffer,guint8 fec_offset)
{
  return GST_FEC_HEADER_MARKER (GST_BUFFER_DATA (buffer)+fec_offset);
}

/**
 * gst_fec_buffer_set_marker:
 * @buffer: the buffer
 * @marker: the new marker
 *
 * Set the marker bit on the FEC packet in @buffer to @marker.
 */
inline void
gst_fec_buffer_set_marker (GstBuffer * buffer, gboolean marker, guint8 fec_offset)
{
  GST_FEC_HEADER_MARKER (GST_BUFFER_DATA (buffer)+ fec_offset) = marker;
}

/**
 * gst_fec_buffer_get_payload_type_recovery:
 * @buffer: the buffer
 *
 * Get the payload type of the FEC packet in @buffer.
 *
 * Returns: The payload type.
 */
inline guint8
gst_fec_buffer_get_payload_type_recovery (GstBuffer * buffer, guint8 fec_offset)
{
   return GST_FEC_HEADER_PAYLOAD_TYPE_RECOVERY (GST_BUFFER_DATA (buffer)+fec_offset);
}

/**
 * gst_fec_buffer_set_payload_type_recovery:
 * @buffer: the buffer
 * @payload_type_recovery: the new type
 *
 * Set the payload type of the FEC packet in @buffer to @payload_type_recovery.
 */
inline void
gst_fec_buffer_set_payload_type_recovery (GstBuffer * buffer, guint8 payload_type_recovery, guint8 fec_offset)
{
  g_return_if_fail (payload_type_recovery < 0x80);

  GST_FEC_HEADER_PAYLOAD_TYPE_RECOVERY (GST_BUFFER_DATA (buffer)+ fec_offset) = payload_type_recovery;
}
/**
 * gst_fec_buffer_get_snbase:
 * @buffer: the buffer
 *
 * Get the sequence number of the FEC packet in @buffer.
 *
 * Returns: The sequence number in host order.
 */
inline guint16
gst_fec_buffer_get_snbase (GstBuffer * buffer, guint8 fec_offset)
{
  return g_htons (GST_FEC_HEADER_SNBASE (GST_BUFFER_DATA (buffer)+ fec_offset));
}

/**
 * gst_fec_buffer_set_snbase:
 * @buffer: the buffer
 * @snbase: the new sequence number
 *
 * Set the sequence number of the FEC packet in @buffer to @snbase.
 */
inline void
gst_fec_buffer_set_snbase (GstBuffer * buffer, guint16 snbase, guint8 fec_offset)
{
  GST_FEC_HEADER_SNBASE (GST_BUFFER_DATA (buffer)+ fec_offset) = g_htons (snbase);
}


/**
 * gst_fec_buffer_get_timestamp_recovery:
 * @buffer: the buffer
 *
 * Get the timestamp_recovery of the FEC packet in @buffer.
 *
 * Returns: The timestamp_recovery in host order.
 */
inline guint32
gst_fec_buffer_get_timestamp_recovery (GstBuffer * buffer, guint8 fec_offset)
{
  return g_htonl (GST_FEC_HEADER_TIMESTAMP_RECOVERY (GST_BUFFER_DATA (buffer)+ fec_offset));
}


/**
 * gst_fec_buffer_set_timestamp_recovery:
 * @buffer: the buffer
 * @timestamp_recovery: the new timestamp recovery
 *
 * Set the timestamp_recovery of the FEC packet in @buffer to @timestamp_recovery.
 */
inline void
gst_fec_buffer_set_timestamp_recovery (GstBuffer * buffer, guint32 timestamp_recovery, guint8 fec_offset)
{
  GST_FEC_HEADER_TIMESTAMP_RECOVERY (GST_BUFFER_DATA (buffer)+ fec_offset) = g_htonl (timestamp_recovery);
}

/**
 * gst_fec_buffer_get_length_recovery:
 * @buffer: the buffer
 *
 * Get the length_recovery of the FEC packet in @buffer.
 *
 * Returns: The length_recovery in host order.
 */
inline guint16
gst_fec_buffer_get_length_recovery (GstBuffer * buffer, guint8 fec_offset)
{
  return g_htons(GST_FEC_HEADER_LENGTH_RECOVERY (GST_BUFFER_DATA (buffer)+ fec_offset));
}


/**
 * gst_fec_buffer_set_length_recovery:
 * @buffer: the buffer
 * @length_recovery: the new length recovery
 *
 * Set the length_recovery of the FEC packet in @buffer to @length_recovery.
 */
inline void
gst_fec_buffer_set_length_recovery (GstBuffer * buffer, guint16 length_recovery, guint8 fec_offset)
{
  GST_FEC_HEADER_LENGTH_RECOVERY (GST_BUFFER_DATA (buffer)+ fec_offset) =  (length_recovery);
}

/*******************************************************FEC MAIN HEADER END********************************************/

/*******************************************************FEC LEVEL HEADER START*****************************************/
/**
 * gst_fec_buffer_get_mask_continue:
 * @buffer: the buffer
 *
 * Get the mask continue of the FEC packet in @buffer .
 *
 * Returns:  the mask continue .
 */
inline guint32
gst_fec_buffer_get_mask_continue (GstBuffer * buffer, guint8 fec_levelheader_offset)
{
  return g_htonl(GST_FEC_LEVEL_HEADER_MASK_CONTINUE(GST_BUFFER_DATA (buffer)+ fec_levelheader_offset));
}

/**
 * gst_fec_buffer_set_mask_continue:
 * @buffer: the buffer
 * @mask_continue: the mask continue
 *
 * Set the mask continue on the FEC packet in @buffer to @mask_continue..
 */
inline void
gst_fec_buffer_set_mask_continue (GstBuffer * buffer, guint32 mask_continue, guint8 fec_levelheader_offset)
{
   GST_FEC_LEVEL_HEADER_MASK_CONTINUE (GST_BUFFER_DATA (buffer)+ fec_levelheader_offset) = g_htonl(mask_continue);
}

/**
 * gst_fec_buffer_get_mask:
 * @buffer: the buffer
 *
 * Get the mask of the FEC packet in @buffer .
 *
 * Returns:  the mask .
 */
inline guint16
gst_fec_buffer_get_mask (GstBuffer * buffer, guint8 fec_levelheader_offset)
{
	return g_htons(GST_FEC_LEVEL_HEADER_MASK(GST_BUFFER_DATA (buffer)+ fec_levelheader_offset));

}

/**
 * gst_fec_buffer_set_mask:
 * @buffer: the buffer
 * @mask: the extension flag
 *
 * Set the mask on the FEC packet in @buffer to @mask..
 */
inline void
gst_fec_buffer_set_mask (GstBuffer * buffer, guint16 mask, guint8 fec_levelheader_offset)
{
   GST_FEC_LEVEL_HEADER_MASK (GST_BUFFER_DATA (buffer)+ fec_levelheader_offset) = g_htons(mask);
}
/**
 * gst_fec_buffer_get_protection_length:
 * @buffer: the buffer
 *
 * Get the protection length of the FEC packet in @buffer .
 *
 * Returns:  the protection length .
 */
inline guint16
gst_fec_buffer_get_protection_length (GstBuffer * buffer, guint8 fec_levelheader_offset)
{
  return g_htons(GST_FEC_LEVEL_HEADER_PROTECTION_LENGTH(GST_BUFFER_DATA (buffer)+ fec_levelheader_offset));
}

/**
 * gst_fec_buffer_set_protection_length:
 * @buffer: the buffer
 * @protection_length: the extension flag
 *
 * Set the protection length on the FEC packet in @buffer to @protection_length..
 */
inline void
gst_fec_buffer_set_protection_length (GstBuffer * buffer, guint16 protection_length, guint8 fec_levelheader_offset)
{
   GST_FEC_LEVEL_HEADER_PROTECTION_LENGTH (GST_BUFFER_DATA (buffer)+ fec_levelheader_offset) = g_htons(protection_length);
}
/******************************************************FEC Level HEADER END***************************/
