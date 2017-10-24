/**
 * @file	:fecxorscheme.c
 * @brief	:This file is used to implement the Parity FEC Source functions.
 *
 * Copyright 2013 by Samsung Electronics, Inc.,
 *
 * This software is the confidential and proprietary information
 * of Samsung Electronics, Inc. ("Confidential Information").  You
 * shall not disclose such Confidential Information and shall use
 * it only in accordance with the terms of the license agreement
 * you entered into with Samsung.
 */
#include <fecxorscheme.h>
#include <gst/gst.h>
#include <string.h>
#define GST_FEC_HEADER_LEN 10
#define ADD_TWO_BYTE 2
#define COPY_FOUR_BYTE 4
#define COPY_EIGHT_BYTE 8

#define MAX_RECONSTRUCTED_RTP_PACKET_SIZE 2000
#define MAX_ERROR_CORRECTION 1
#define MASK_FIRST_BIT 0x0001
#define SHIFT_BY_ONE 1
#define PRIV_PKT 1

#define MASK_1ST_BYTE 0xff
#define MASK_2ND_BYTE 0xff00
#define MASK_3RD_BYTE 0xff0000
#define MASK_4TH_BYTE 0xff000000

#define MASK_SEQUENCE_NUM   0xffff
#define MASK_1ST_2_RTP_BYTE   0x3fff0000
#define MASK_RTP_VERSION_2  0x80000000

#define ZERO 0
#define ONE 1


#include <gst/rtp/gstrtpbuffer.h>
#include <gstfecbuffer.h>

#include "gstrtpjitterbuffer.h"
#include "rtpjitterbuffer.h"
#include "rtpstats.h"

/**
* @fn    :gst_xor_rtp_reconstruct_packet()
* @brief :This Function is used to Detect and  Reconstruct the lost RTP Packet .
*
* @param	:[IN]rtppacketlist Pointer to RTPPacket List
* @param	:[IN]fecpacketlist Pointer to RTPPacket List
* @param	:[IN]jbuf Pointer to RTP jitter buffer
* @param	:[IN]clock_rate cloak rate of RTP packet
* @return	:GstBuffer* returns GstBuffer pointer of reconstructed packet on success otherwise returns NULL.
*/

GstBuffer* gst_xor_rtp_reconstruct_packet(GList* rtppacketlist,RTPJitterBufferItem* fecpacketlist)
{
	GstBuffer* pLocalRTPPacketbuff = NULL;
	GList*  bufRTPPKTvsSEQNUMlist;
	GList*  requiredrtppktlist=NULL;
	guint32  rtplostPktCount = ZERO;
	guint32  rtplostPktSeqNo = ZERO;
	guint fec_offset,fec_levelheader_offset;
	guint8 csrc_count;
	guint16 maskField;
	guint32 snBase;
	GstBuffer* p_fec_Buffer = GST_BUFFER_CAST(((RTPJitterBufferItem *)fecpacketlist)->data);

	/*for Fec offset calculation*/
	csrc_count=gst_rtp_buffer_get_csrc_count (p_fec_Buffer);
	fec_offset=gst_rtp_buffer_calc_header_len(csrc_count);
	fec_levelheader_offset= fec_offset + GST_FEC_HEADER_LEN;

	/* Find the required FEC Packet...first packet in this case */
	/* Find all the required RTP Packets as according to current FEC packet */
	/*Find all the RTP packets needed for reconstruction */
	maskField = gst_fec_buffer_get_mask(p_fec_Buffer,fec_levelheader_offset);
	snBase = gst_fec_buffer_get_snbase(p_fec_Buffer,fec_offset);

	maskField= GUINT16_SWAP_LE_BE(maskField);
	//Find no of Required RTP packet .
	for(guint32 count = ZERO; count < XOR_MASK_LENGTH; count++ )
	{
		if(maskField & MASK_FIRST_BIT)
		{
			requiredrtppktlist =g_list_append(requiredrtppktlist,(gpointer)(snBase + count));
		}
		else
		{
			; /* NULL */
		}
		maskField = maskField >> SHIFT_BY_ONE;
	}

	/*	Check the RTPPacket map, this will give the lostpacket sequence number...rtplostPktSeqNo */
	for(GList* ptempRequiredPktlist = g_list_first(requiredrtppktlist); ptempRequiredPktlist; ptempRequiredPktlist=g_list_next(ptempRequiredPktlist))
	{
		for(bufRTPPKTvsSEQNUMlist = g_list_first(rtppacketlist); bufRTPPKTvsSEQNUMlist; bufRTPPKTvsSEQNUMlist = g_list_next(bufRTPPKTvsSEQNUMlist))
		{
			guint32 seqNum = ((RTPJitterBufferItem *)bufRTPPKTvsSEQNUMlist)->seqnum;
			if(seqNum == (guint32)(ptempRequiredPktlist->data))
			{
				break;
			}
			else
			{
				; /* NULL */
			}
		}

		if(!(bufRTPPKTvsSEQNUMlist))
		{
			//Packet Lost
			rtplostPktSeqNo = 	(guint32)(ptempRequiredPktlist->data);  //TASK 2 Left
			rtplostPktCount++;
		}
		else
		{
			//Entry Found;
		}
	}

	/* Proceed further for Reconstrction */
	if(rtplostPktCount ==ZERO) /* No loss */
	{
		/* No need of Reconstruction*/
	}
	else if(rtplostPktCount > MAX_ERROR_CORRECTION) /* More than one loss occured */
	{
		/* Reconstruction cannot be achieved */
		GST_INFO_OBJECT (rtppacketlist, "More than one packet lost, rtplostPktCount : %d...No Reconstruction",rtplostPktCount);
	}
	else /* One packet loss occured */
	{
		/* Reconstruction can be achieved */

		pLocalRTPPacketbuff = gst_buffer_new_and_alloc(MAX_RECONSTRUCTED_RTP_PACKET_SIZE);

		for(bufRTPPKTvsSEQNUMlist = g_list_first(rtppacketlist); bufRTPPKTvsSEQNUMlist; bufRTPPKTvsSEQNUMlist = g_list_next(bufRTPPKTvsSEQNUMlist))
		{
			guint32 seqNum = ((RTPJitterBufferItem *)bufRTPPKTvsSEQNUMlist)->seqnum;
			if(seqNum == rtplostPktSeqNo -PRIV_PKT)
			{
				gst_buffer_copy_metadata (pLocalRTPPacketbuff,GST_BUFFER_CAST(((RTPJitterBufferItem *)bufRTPPKTvsSEQNUMlist)->data), GST_BUFFER_COPY_ALL);
				break;
			}
			else
			{
				; /* NULL */
			}
		}
		if(pLocalRTPPacketbuff)
		{
			/* Reconstruct the missing RTP Packet */
			if(gst_xor_reconstruct_rtp_data( pLocalRTPPacketbuff,rtppacketlist,p_fec_Buffer,rtplostPktSeqNo,requiredrtppktlist) == TRUE)
			{
				guint32 seqnum = ZERO;

				seqnum = gst_rtp_buffer_get_seq(pLocalRTPPacketbuff);
					/* Return the RTP Packet Pointer */
				GST_INFO_OBJECT (rtppacketlist,"One packet lost. with rtplostPktSeqNo : %d  reconstructed packet seqnum :%d \n",rtplostPktSeqNo,seqnum);
			}
			else
			{
				gst_buffer_unref (pLocalRTPPacketbuff);
				pLocalRTPPacketbuff = NULL;
			}
		}
		else
		{
			; /*NULL*/
		}
	}
	g_list_free(requiredrtppktlist);

	return pLocalRTPPacketbuff;
}
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

guint32 gst_xor_reconstruct_rtp_data(GstBuffer* pLocalRTPPacketbuff,GList* rtppacketlist, GstBuffer* pfecbuffer, guint32 rtplostPktSeqNo,GList*  requiredrtppktlist)
{
	guint32 nRetVal = TRUE;
	GList* temprtplist;
	guint32 mSSRC;

	/* Initialize Recovery BitStream */
	guchar recoverybitstring[RECOVERY_BITSTRING];

	/*taking value from first RTPPacket in the rtppacketlist  */
	temprtplist=g_list_first(rtppacketlist);
	mSSRC = gst_rtp_buffer_get_ssrc(GST_BUFFER_CAST(((RTPJitterBufferItem *)temprtplist)->data)); //taking value from first RTPPacket..check later

	/* Get the Recovery BitStream */
	nRetVal = gst_xor_header_recovery_bitString(pfecbuffer, recoverybitstring, rtplostPktSeqNo, requiredrtppktlist,rtppacketlist);
	if(nRetVal != TRUE)
	{
		return  nRetVal;
	}
	else
	{
		; /* NULL */
	}
	/* Repair RTP Header */
	nRetVal = gst_xor_rapair_rtp_header(pLocalRTPPacketbuff, recoverybitstring, rtplostPktSeqNo, mSSRC);
	if(nRetVal != TRUE)
	{
		return  nRetVal;
	}
	else
	{
		; /* NULL */
	}
	/* Repair RTP Payload */
	nRetVal = gst_xor_rapair_rtp_payload(pfecbuffer, pLocalRTPPacketbuff, rtplostPktSeqNo, mSSRC,rtppacketlist,requiredrtppktlist);
	if(nRetVal != TRUE)
	{
		return  nRetVal;
	}
	else
	{
		; /* NULL */
	}
	return TRUE;
}
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
guint32 gst_xor_header_recovery_bitString(GstBuffer* pfecbuffer, guchar *recoverybitstream, guint32 rtplostPktSeqNo,GList*  requiredrtppktlist,GList* rtppacketlist)
{
	guchar bitstring1[RECOVERY_BITSTRING] = {ZERO};
	guchar bitstring[RECOVERY_BITSTRING] = {ZERO};
	guint32 firstTime = ONE;
	GstBuffer* pRTPPacketbuff = NULL;
	GList* bufRTPPKTvsSEQNUMlist;
	guint32 csrc_count;
	guint32 fec_offset;

	for(GList* pobReqPktVectorItr = g_list_first(requiredrtppktlist); pobReqPktVectorItr; pobReqPktVectorItr  = g_list_next(pobReqPktVectorItr))
	{
		/* Find from map */
		if((guint32)(pobReqPktVectorItr->data) == rtplostPktSeqNo)
		{
			continue;
		}
		else
		{
			; /* NULL */
		}
		for( bufRTPPKTvsSEQNUMlist=g_list_first(rtppacketlist); bufRTPPKTvsSEQNUMlist; bufRTPPKTvsSEQNUMlist=g_list_next(bufRTPPKTvsSEQNUMlist))
		{
			guint32 seqNum = gst_rtp_buffer_get_seq(GST_BUFFER_CAST(((RTPJitterBufferItem *)bufRTPPKTvsSEQNUMlist)->data));
			if( seqNum == (guint32)(pobReqPktVectorItr->data))
			{
				break;
			}
			else
			{
				; /* NULL */
			}
		}
		if(bufRTPPKTvsSEQNUMlist != NULL)
		{
			pRTPPacketbuff = (GST_BUFFER_CAST(((RTPJitterBufferItem *)bufRTPPKTvsSEQNUMlist)->data));
		}
		else
		{
			/* error */
			return FALSE;
		}

		/* Create ProtectedBitString for this RTP Packet */
		//guint32 seqNum = gst_rtp_buffer_get_seq(pRTPPacketbuff);
		gst_xor_create_header_protected_bitstring(pRTPPacketbuff, bitstring);

		/* Take XOR */
		if(firstTime)
		{
			memcpy(bitstring1, bitstring, RECOVERY_BITSTRING);
			firstTime = ZERO;
		}
		else
		{
			gst_xor_function(bitstring1, bitstring, RECOVERY_BITSTRING);
		}
	}
	csrc_count=gst_rtp_buffer_get_csrc_count (pfecbuffer);
	fec_offset=gst_rtp_buffer_calc_header_len(csrc_count);

	//FEC bit string = 80-bits FEC Header
	memcpy(recoverybitstream,(GST_BUFFER_DATA(pfecbuffer)+ fec_offset), FEC_BIT_STRING);

	/*Recovery bit string = the bitwise exclusive OR of the protected bit string generated from all the media
	packets in list and the FEC bit string generated from all the FEC packets in list*/
	gst_xor_function(recoverybitstream, bitstring1, RECOVERY_BITSTRING);

	return TRUE;
}

/**
* @fn    :gst_xor_create_header_protected_bitstring()
* @brief :This Function is used to Create 80-bits protected bit string
*
* @param	:[IN]pRTPPacketbuff Pointer to RTPPacket List
* @param	:[IN]bitString Pointer to protected bit string
* @return	:guint32 return Error code or Success
*/
guint32 gst_xor_create_header_protected_bitstring(GstBuffer* pRTPPacketbuff, guchar* bitString)
{
	guint16 nWord;
	guchar* localString = bitString;

	/* The first 64 bits of the RTP header */
	memcpy(bitString,(GST_BUFFER_DATA(pRTPPacketbuff)), COPY_EIGHT_BYTE);
	localString += COPY_EIGHT_BYTE;

	/* Remaining 16 bits */
	nWord = g_htons(GST_BUFFER_SIZE(pRTPPacketbuff) - RTP_HEADER);

	memcpy(&bitString[8],(guchar*)&nWord,ADD_TWO_BYTE);
	localString += ADD_TWO_BYTE;

	return TRUE;
}

/**
* @fn    :gst_xor_Function()
* @brief :This Function is used to compute the XOR result of two strings.
*
* @param	:[IN]string1 Pointer to string
* @param	:[IN]string2 Pointer to string
* @param	:[IN]length Length of strings
* @return	:guint32 return 0 if Success
*/
guint32 gst_xor_function( guchar *string1, guchar *string2, guint32 length )
{
	guint32 count1;
	/* Basic bit wise XOR operation  */
	for(count1 = ZERO; count1 < length; count1++)
	{
		string1[count1] ^= string2[count1];
	}
	return ZERO;
}

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
guint32 gst_xor_rapair_rtp_header(GstBuffer* pLocalRTPPacketbuff, guchar* recoverybitstream, guint32 recoveredSeqNo, guint32 mSSRC)
{
	guint32 rtpHdr;
	guint32 nWord;
	guint16 recoveredSN = ZERO;
	guchar* localRecoveryBS = recoverybitstream;

	//Conversion of endiness.
	nWord = ((recoverybitstream[ZERO] << 24) & MASK_4TH_BYTE) | ((recoverybitstream[ONE] << 16) & MASK_3RD_BYTE) |
		((recoverybitstream[2] << 8) & MASK_2ND_BYTE) | ((recoverybitstream[3]) & MASK_1ST_BYTE);

	/* Create standard 12-byte RTP header */
	rtpHdr = MASK_RTP_VERSION_2; // RTP version 2

	rtpHdr |= ((nWord & MASK_1ST_2_RTP_BYTE));
	localRecoveryBS += ADD_TWO_BYTE;

	//Set the Sequence Number
	/* Discarding any CycleCount, as it has already been taken care of, in RTPSession */
	recoveredSN = recoveredSeqNo;
	recoveredSN &= MASK_SEQUENCE_NUM;
	rtpHdr |= recoveredSN; // sequence number
	nWord =  g_ntohl(rtpHdr);
	memcpy((GST_BUFFER_DATA(pLocalRTPPacketbuff)),(guchar*)&nWord, COPY_FOUR_BYTE);


	/* Skipping TWO bytes */
	localRecoveryBS += ADD_TWO_BYTE;

	//Set Timestamp
	memcpy((GST_BUFFER_DATA(pLocalRTPPacketbuff)+ COPY_FOUR_BYTE), localRecoveryBS, COPY_FOUR_BYTE);
	localRecoveryBS += COPY_FOUR_BYTE;

	//Set SSRC
	nWord =  g_ntohl(mSSRC);
	memcpy((GST_BUFFER_DATA(pLocalRTPPacketbuff)+ COPY_EIGHT_BYTE),(guchar*)&nWord, COPY_FOUR_BYTE);

	return TRUE;
}

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
guint32 gst_xor_rapair_rtp_payload(GstBuffer* pfecbuffer, GstBuffer* pLocalRTPPacketbuff,guint32 recoveredSeqNo, guint32 mSSRC,GList* rtppacketlist,GList* requiredrtppktlist)
{
	guint32 protectedLength;
	guchar *recoveryBitStreamFEC= NULL;
	guchar* sourceData= NULL;
	guchar* recoveryBitStreamMedia= NULL;

	guint32 m_lengthRecovery=ZERO;
	guint32 extraLen = ZERO;
	guint32 xorlength = ZERO;
	guint32 recoveryLength = ZERO;

	guint fec_offset,fec_levelheader_offset;
	guint8 csrc_count;
	guint32 new_packet_length;

	csrc_count=gst_rtp_buffer_get_csrc_count (pfecbuffer);
	fec_offset=gst_rtp_buffer_calc_header_len(csrc_count);
	fec_levelheader_offset= fec_offset + GST_FEC_HEADER_LEN;

	/* Calculate protected Length */
	protectedLength = gst_fec_buffer_get_protection_length(pfecbuffer, (fec_levelheader_offset ));

	/* Calculate xor length */ //convert size to multiple of 8
	extraLen = COPY_EIGHT_BYTE - ((GST_BUFFER_SIZE(pfecbuffer) - (FEC_HEADER + FEC_LEVEL_HEADER)) % COPY_EIGHT_BYTE);
	xorlength = (GST_BUFFER_SIZE(pfecbuffer) - (FEC_HEADER + FEC_LEVEL_HEADER)) + extraLen;/* 10 for FEC Header, 4 for FEC level Header */
	recoveryLength = xorlength;

	/*Allocate Memory */
	recoveryBitStreamMedia = (guchar*)g_malloc( sizeof(guchar) * recoveryLength);

	if(recoveryBitStreamMedia)
	{
		//Success
	}
	else
	{
		return FALSE;
	}
	memset(recoveryBitStreamMedia, ZERO, recoveryLength);

	/*Allocate Memory */
	recoveryBitStreamFEC = (guchar*)g_malloc(sizeof(guchar) * recoveryLength);
	if(recoveryBitStreamFEC)
	{
		//Success
	}
	else
	{
		if(recoveryBitStreamMedia)
		{
			g_free(recoveryBitStreamMedia);
		}
		else
		{

		}
		return FALSE;
	}
	memset(recoveryBitStreamFEC, ZERO, recoveryLength);

	/* Function RTP payload recovery bitstring */
	gst_xor_payload_recovery_bitstring(recoveryBitStreamMedia, protectedLength, recoveredSeqNo, xorlength,requiredrtppktlist,rtppacketlist,&m_lengthRecovery);

	//FEC bit string = Level n payload
	sourceData = (GST_BUFFER_DATA(pfecbuffer) + (fec_levelheader_offset + FEC_LEVEL_HEADER));// when l=0 fec level header size is =4 and when l=1 fec level header size is 8 byte;
	memcpy(recoveryBitStreamFEC, sourceData, recoveryLength);

	/*Recovery bit string = the bitwise exclusive OR of the protected bit string generated from all the media
	packets in T and the FEC bit string generated from all the FEC	packets in T*/
	gst_xor_function(recoveryBitStreamFEC, recoveryBitStreamMedia, recoveryLength);

	//Append the recovered payload to pLocalRTPPacketbuff to get the fully recovered packet
	new_packet_length=(m_lengthRecovery + RTP_HEADER);
	gst_rtp_buffer_set_packet_len(pLocalRTPPacketbuff,new_packet_length);
	m_lengthRecovery = ZERO;

	if(recoveryLength >= ( new_packet_length - RTP_HEADER)) /* XOR Length may be more due to approximation done to make multiple of 8 */
	{
		memcpy((GST_BUFFER_DATA(pLocalRTPPacketbuff) + RTP_HEADER), recoveryBitStreamFEC, (new_packet_length - RTP_HEADER));
	}
	else
	{
		memcpy((GST_BUFFER_DATA(pLocalRTPPacketbuff) + RTP_HEADER), recoveryBitStreamFEC, recoveryLength/*pLocalRTPPacketbuff->data.nSize*/);
		//memset((GST_BUFFER_DATA(pLocalRTPPacketbuff) + RTP_HEADER + recoveryLength), 0, ((new_packet_length - RTP_HEADER) - recoveryLength));
	}

	if(recoveryBitStreamFEC)
	{
		g_free(recoveryBitStreamFEC);
	}
	else
	{

	}
	if(recoveryBitStreamMedia)
	{
		g_free(recoveryBitStreamMedia);
	}
	else
	{

	}
	return TRUE;
}

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

guint32  gst_xor_payload_recovery_bitstring(guchar *recBitStream, guint32 protectedLength, guint32 recoveredSeqNo, guint32 xorlength,GList* requiredrtppktlist,GList* rtppacketlist,guint32 *lengthRecovery)
{
	guchar* bitstring = NULL;
	GstBuffer* pRTPPacketbuff = NULL;
	guint32 firstTime = ONE;
	guint32 reqPktSeqNum = ZERO;
	GList* tempRTPPKTvsSEQNUMlist;
	guint32 length = xorlength;
	guint32 rtp_offset;
	guint8 csrc_count;
	guint32 rtplength;

	/*Allocate Memory */ /* For all media packets in the list */
	bitstring = (guchar*) g_malloc ( sizeof(guchar) * length);
	if(bitstring)
	{
		//Success
	}
	else
	{
		//bitstring = NULL;
		return FALSE;
	}

	for(GList* pobReqPktVectorItr = g_list_first(requiredrtppktlist); pobReqPktVectorItr; pobReqPktVectorItr = g_list_next(pobReqPktVectorItr))
	{
		memset(bitstring, ZERO, length);
		reqPktSeqNum = ((guint32)pobReqPktVectorItr->data);
		if(reqPktSeqNum == recoveredSeqNo)
		{
			continue;
		}
		for(tempRTPPKTvsSEQNUMlist = g_list_first(rtppacketlist); tempRTPPKTvsSEQNUMlist; tempRTPPKTvsSEQNUMlist = g_list_next(tempRTPPKTvsSEQNUMlist))
		{
			guint32 seqNum = gst_rtp_buffer_get_seq(GST_BUFFER_CAST(((RTPJitterBufferItem *)tempRTPPKTvsSEQNUMlist)->data));
			if(seqNum == reqPktSeqNum)
			{
				break;
			}
			else
			{

			}
		}
		if(tempRTPPKTvsSEQNUMlist != NULL)
		{
			pRTPPacketbuff = (GST_BUFFER_CAST(((RTPJitterBufferItem *)tempRTPPKTvsSEQNUMlist)->data));

			/* Calculate Length Recovery Value */
			csrc_count=gst_rtp_buffer_get_csrc_count (pRTPPacketbuff);
			rtp_offset=gst_rtp_buffer_calc_header_len(csrc_count);
			*lengthRecovery ^=  (guint32 )((GST_BUFFER_SIZE(pRTPPacketbuff)) - rtp_offset);//how

			/* Copy the protected bit string */
			rtplength = GST_BUFFER_SIZE(pRTPPacketbuff) - (rtp_offset) ;
			memcpy(bitstring, ((GST_BUFFER_DATA(pRTPPacketbuff)) + rtp_offset), rtplength);

			/* Take XOR */
			if(firstTime)
			{
				memcpy(recBitStream, bitstring, length);
				firstTime = ZERO;
			}
			else
			{
				gst_xor_function(recBitStream, bitstring, length);
			}
		}
		else
		{
			; /* NULL */
		}
	}

	/* Calculate Length Recovery Value */
	*lengthRecovery ^=  protectedLength;
	if(bitstring)
	{
		g_free(bitstring);
	}
	else
	{

	}
	return TRUE;
}
/*****************************************************************************END OF FILE***********************************************************/
