/**
 * @file	:fecxorscheme.h
 * @brief	:XORScheme contains all function prototype and macros that does Parity FEC handling
 *
 * Copyright 2013 by Samsung Electronics, Inc.,
 *
 * This software is the confidential and proprietary information
 * of Samsung Electronics, Inc. ("Confidential Information").  You
 * shall not disclose such Confidential Information and shall use
 * it only in accordance with the terms of the license agreement
 * you entered into with Samsung.
 */


#ifndef _XORSCHEME_H_
#define _XORSCHEME_H_
#include <gst/gst.h>

#include "gstrtpjitterbuffer.h"
#include "rtpjitterbuffer.h"
#include "rtpstats.h"

#define MAX_LEVEL_HEADER 10
#define RECOVERY_BITSTRING 16 /* Taken as multiple of 8, exact size is 10 */
#define FEC_BIT_STRING 10
#define FEC_HEADER 10
#define RTP_HEADER 12
#define FEC_LEVEL_HEADER 4
#define XOR_MASK_LENGTH 16

/**
* @fn    :gst_xor_rtp_reconstruct_packet()
* @brief :This Function is used to Detect and  Reconstruct the lost RTP Packet .
*
* @param	:[IN]rtppacketlist Pointer to RTPPacket List
* @param	:[IN]fecpacketlist Pointer to RTPPacket List
* @param	:[IN]jbuf Pointer to RTP jitter buffer
* @param	:[IN]clock_rate cloak rate of RTP packet
* @return	:GstBuffer* returns reconstructed GstBuffer pointer if success.
*/
GstBuffer* gst_xor_rtp_reconstruct_packet(GList* rtppacketlist,RTPJitterBufferItem *fecpacketlist);
/**
* @fn    :gst_xor_reconstruct_rtp_data()
* @brief :This Function is used to Reconstruct the lost RTP Packet Data.
*
* @param	:[IN]rtppacketlist Pointer to RTPPacket List
* @param	:[IN]pfecbuffer Pointer to FECPacket
* @param	:[IN]rtplostPktSeqNo lost Packet Sequence Number
* @param	:[IN]pLocalRTPPacketbuff Pointer to  RTPPacket Buffer
* @param	:[IN]requiredrtppktlist Pointer to Required RTPPacket List.
* @return	:guint32 return Error code or Success
*/
guint32 gst_xor_reconstruct_rtp_data(GstBuffer* pLocalRTPPacketbuff,GList* rtppacketlist, GstBuffer* pfecbuffer, guint32 rtplostPktSeqNo,GList*  requiredrtppktlist);

/**
* @fn    :gst_xor_header_recovery_bitString()
* @brief :This Function is used to Calculate the recovery bit string.
*
* @param	:[IN]pfecbuffer Pointer to XORFECPacket List
* @param	:[IN]recoverybitstream Pointer to recovery BitStream
* @param	:[IN]rtplostPktSeqNo Sequence Number of lost Packet
* @param	:[IN]requiredrtppktlist Pointer to Required RTPPacket List.
* @param	:[IN]rtppacketlist Pointer to RTPPacket List
* @return	:guint32 return Error code or Success
*/
guint32 gst_xor_header_recovery_bitString(GstBuffer *pfecbuffer, guchar *recoverybitstream, guint32 rtplostPktSeqNo,GList*  requiredrtppktlist,GList* rtppacketlist);

/**
* @fn    :gst_xor_create_header_protected_bitstring()
* @brief :This Function is used to Create 80-bits protected bit string
*
* @param	:[IN]pRTPPacketbuff Pointer to RTPPacket List
* @param	:[IN]bitString Pointer to protected bit string
* @return	:guint32 return Error code or Success
*/
guint32 gst_xor_create_header_protected_bitstring(GstBuffer* pLocalRTPPacketbuff, guchar* bitString);
/**
* @fn    :gst_xor_Function()
* @brief :This Function is used to compute the XOR result of two strings.
*
* @param	:[IN]string1 Pointer to string
* @param	:[IN]string2 Pointer to string
* @param	:[IN]length Length of strings
* @return	:guint32 return 0 if Success
*/
guint32 gst_xor_function( guint8 *string1, guint8 *string2, guint32 length );

/**
* @fn    :gst_xor_rapair_rtp_header()
* @brief :This Function is used to Rapair RTPHeader of Lost packet.
*
* @param	:[IN]pLocalRTPPacketbuff Pointer to RTPPacket structure
* @param	:[IN]recoverybitstream Pointer to recovery BitStream
* @param	:[IN]recoveredSeqNo Sequence Number to be recovered
* @param	:[IN]mSSRC SSRC Value
* @return	:guint32 return Error code or Success
*/
guint32 gst_xor_rapair_rtp_header(GstBuffer* pLocalRTPPacketbuff, guchar *recoveryBitStream, guint32 recoveredSeqNo, guint32 mSSRC);

 /**
* @fn    :gst_xor_rapair_rtp_payload()
* @brief :This Function is used to Rapair RTP Payload of Lost Packet.
*
* @param	:[IN]pfecbuffer Pointer to FECPacket List
* @param	:[IN]pLocalRTPPacketbuff Pointer to RTPPacket
* @param	:[IN]recoveredSeqNo Sequence Number to be recovered
* @param	:[IN]mSSRC SSRC value
* @param	:[IN]rtppacketlist Pointer to RTPPacket List
* @param	:[IN]requiredrtppktlist Pointer to RTPPacket List
* @return	:guint32 return Error code or Success
*/
guint32 gst_xor_rapair_rtp_payload(GstBuffer *pfecbuffer, GstBuffer* pLocalRTPPacketbuff,guint32 recoveredSeqNo, guint32 mSSRC,GList* rtppacketlist,GList* requiredrtppktlist);

  /**
* @fn    :gst_xor_payload_recovery_bitstring()
* @brief :This Function is used to Calculate the recovery bit string .
*
* @param	:[IN]recBitStream Pointer to recovery BitStream
* @param	:[IN]protectedLength protected Length
* @param	:[IN]recoveredSeqNo Sequence Number to be recovered
* @param	:[IN]xorlength xor Length
* @param	:[IN]rtppacketlist Pointer to RTPPacket List
* @param	:[IN]requiredrtppktlist Pointer to RTPPacket List
* @param	:[IN]lengthRecovery Length REcovery
* @return	:guint32 return Error code or Success
*/
guint32  gst_xor_payload_recovery_bitstring(guchar *recBitStream, guint32 protectedLength, guint32 recoveredSeqNo, guint32 xorLenght,GList* requiredrtppktlist,GList* bufRTPPKTvsSEQNUMlist,guint32 *lengthRecovery);

#endif
