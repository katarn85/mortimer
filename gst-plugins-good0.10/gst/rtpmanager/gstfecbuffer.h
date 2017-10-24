#ifndef __GST_FECBUFFER_H__
#define __GST_FECBUFFER_H__

#include <gst/gst.h>
#include <gst/rtp/gstrtppayloads.h>

G_BEGIN_DECLS

void gst_fec_buffer_allocate_data (GstBuffer * buffer, guint payload_len,guint8 pad_len, guint8 csrc_count);

gboolean	gst_fec_buffer_get_extension_flag (GstBuffer * buffer, guint8 fec_offset);
void		gst_fec_buffer_set_extension_flag (GstBuffer * buffer, guint8 extension_flag,guint8 fec_offset);

gboolean	gst_fec_buffer_get_long_mask (GstBuffer * buffer, guint8 fec_offset);
void		gst_fec_buffer_set_long_mask(GstBuffer * buffer, guint8 long_mask,guint8 fec_offset);

gboolean	gst_fec_buffer_get_padding (GstBuffer * buffer, guint8 fec_offset);
void		gst_fec_buffer_set_padding (GstBuffer * buffer, gboolean padding,guint8 fec_offset);

gboolean	gst_fec_buffer_get_extension (GstBuffer * buffer, guint8 fec_offset);
void		gst_fec_buffer_set_extension (GstBuffer * buffer, gboolean extension, guint8 fec_offset);

guint8		gst_fec_buffer_get_csrc_count (GstBuffer * buffer, guint8 fec_offset);
void		gst_fec_buffer_set_csrc_count (GstBuffer * buffer, guint16 csrc_count, guint8 fec_offset);

gboolean	gst_fec_buffer_get_marker (GstBuffer * buffer,guint8 fec_offset);
void		gst_fec_buffer_set_marker (GstBuffer * buffer, gboolean marker, guint8 fec_offset);

guint8		gst_fec_buffer_get_payload_type_recovery (GstBuffer * buffer, guint8 fec_offset);
void		gst_fec_buffer_set_payload_type_recovery (GstBuffer * buffer, guint8 payload_type_recovery, guint8 fec_offset);

guint16		gst_fec_buffer_get_snbase (GstBuffer * buffer, guint8 fec_offset);
void		gst_fec_buffer_set_snbase (GstBuffer * buffer, guint16 snbase, guint8 fec_offset);

guint32		gst_fec_buffer_get_timestamp_recovery (GstBuffer * buffer, guint8 fec_offset);
void		gst_fec_buffer_set_timestamp_recovery (GstBuffer * buffer, guint32 timestamp_recovery, guint8 fec_offset);

guint16		gst_fec_buffer_get_length_recovery (GstBuffer * buffer, guint8 fec_offset);
void		gst_fec_buffer_set_length_recovery (GstBuffer * buffer, guint16 length_recovery, guint8 fec_offset);


/*******************************************************FEC MAIN HEADER END********************************************/

/*******************************************************FEC LEVEL HEADER START*****************************************/

guint32		gst_fec_buffer_get_mask_continue (GstBuffer * buffer, guint8 fec_offset);
void		gst_fec_buffer_set_mask_continue (GstBuffer * buffer, guint32 mask_continue, guint8 fec_offset);

guint16		gst_fec_buffer_get_mask (GstBuffer * buffer, guint8 fec_offset);
void		gst_fec_buffer_set_mask (GstBuffer * buffer, guint16 mask, guint8 fec_offset);

guint16		gst_fec_buffer_get_protection_length (GstBuffer * buffer, guint8 fec_offset);
void		gst_fec_buffer_set_protection_length (GstBuffer * buffer, guint16 protection_length, guint8 fec_offset);

/*******************************************************FEC LEVEL HEADER END********************************************/

G_END_DECLS

#endif /* __GST_fecBUFFER_H__ */


