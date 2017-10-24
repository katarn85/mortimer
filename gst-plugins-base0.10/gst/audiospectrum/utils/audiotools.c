/**
  * @file	audiotools.c
  * @brief	Implementation of tools for transforming audio data

  * Copyright 2011 by Samsung Electronics, Inc.,
  *
  * This software is the confidential and proprietary information
  * of Samsung Electronics, Inc. ("Confidential Information").  You
  * shall not disclose such Confidential Information and shall use
  * it only in accordance with the terms of the license agreement
  * you entered into with Samsung.
  */

#include "audiotools.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

int SatToBytesSigned(int x, int bytes)
{
	if(bytes == 2)
	{
		return x > 32767 ? 32767 : (x < -32767 ? -32767 : x);
	}
	else if(bytes == 4)
	{
		return x > 8388607 ? 8388607 : (x < -8388607 ? -8388607 : x);
	}

	return x;
}

int* TransformFrameToInternalFormat(audioframe_s* frame)
{
	int numSamples = frame->size / frame->sample_size;
	if(numSamples == 0)
	{
		return 0;
	}

	if(frame->data == 0)
	{
		return 0;
	}

	int* data = (int*)malloc(numSamples * sizeof(int)); // in internal format each sample == int
	if(data == 0)
	{
		return 0;
	}
	//take input data to integers;
	memset(data, 0, numSamples * sizeof(int));

	if(frame->interleaved)
	{
		for(int i = 0; i < numSamples; ++i)
		{
			memcpy(&(((unsigned char*)data)[i * 4]), &(((unsigned char*)frame->data)[i * frame->sample_size]), frame->sample_size);	
		}
	}
	else
	{
		//transform to interleaved format
		for(int i = 0; i < numSamples / (int)frame->num_ch; ++i)
		{
			for(int j = 0; j < (int)frame->num_ch; ++j)
			{
				int indInterleaved = i * frame->num_ch + j;
				int indNotInterleaved = i + j * numSamples / frame->num_ch;
				memcpy(&((unsigned char*)data)[indInterleaved * 4], &((unsigned char*)frame->data)[indNotInterleaved * frame->sample_size], frame->sample_size);	
			}
		}
	}
	
	//we just place bytes of our number to integer, so negative numbers become positive. need to fix it 
	if(frame->sample_size == 4)
	{
		for(int i = 0; i < numSamples; ++i)
		{
			if((data[i] & 0x00800000) != 0)
			{
				data[i] = data[i] | 0xFF000000;
			}
			else
			{
				data[i] = data[i] & 0x00FFFFFF;
			}
		}
	}
	else if(frame->sample_size == 2)
	{
		for(int i = 0; i < numSamples; ++i)
		{
			if((data[i] & 0x00008000) != 0)
			{
				data[i] = data[i] | 0xFFFF0000;
			}
			else
			{
				data[i] = data[i] & 0x0000FFFF;
			}
		}
	}
				
	return data;
}

int TransformInternalFormatToFrame(audioframe_s* frame, int* internalData)
{
	int numSamples = frame->size / frame->sample_size;
	if(numSamples == 0)
	{
		return 1;
	}
	
	if(frame->data == 0)
	{
		return 1;
	}

	if(frame->interleaved)
	{
		for(int i = 0; i < numSamples; ++i)
		{
			memcpy(&((unsigned char*)frame->data)[i * frame->sample_size], &((unsigned char*)internalData)[i * 4], frame->sample_size);	
		}
	}
	else
	{
		//transform to interleaved format
		for(int i = 0; i < numSamples / (int)frame->num_ch; ++i) //frames
		{
			for(int j = 0; j < (int)frame->num_ch; ++j) //channels
			{
				int indInterleaved = i * frame->num_ch + j;
				int indNotInterleaved = i + j * numSamples / frame->num_ch;
				memcpy(&((unsigned char*)frame->data)[indNotInterleaved * frame->sample_size], &((unsigned char*)internalData)[indInterleaved * 4], frame->sample_size);	
			}
		}
	}

	return 0;
}