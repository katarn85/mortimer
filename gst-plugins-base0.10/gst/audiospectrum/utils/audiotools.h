/**
  * @file	audiotools.h
  * @brief	header file with tools for transforming audio data

  * Copyright 2011 by Samsung Electronics, Inc.,
  *
  * This software is the confidential and proprietary information
  * of Samsung Electronics, Inc. ("Confidential Information").  You
  * shall not disclose such Confidential Information and shall use
  * it only in accordance with the terms of the license agreement
  * you entered into with Samsung.
  */

#ifndef _AUDIOTOOLS_H_
#define _AUDIOTOOLS_H_

#include "audioframe.h"

/**
  * @brief			saturate singal to signed bytes
  * @param			[in] x				value of signal sample
  * @param			[in] toBytes		number of bytes to which we want to saturate
  * @return								saturated value of signal
  * @see								AudioTools.cpp
  */
int SatToBytesSigned(int x, int toBytes); 


/**
  * @brief			transform data from decoder format to our internal format
  * @param			[in, out] frame		struct describing audio frame
  * @return								pointer to data in internal format, null on error
  * @remark								need to free returned pointer
  * @see								AudioTools.cpp
  */
int* TransformFrameToInternalFormat(audioframe_s* frame);


/**
  * @brief			transform data from our internal format to decoder format
  * @param			[in, out] frame		struct describing audio frame
  * @param			[in] internalData	pointer to data in internal format
  * @return		                        0 if successful, non-0 on error
  * @see								AudioTools.cpp
  */
int TransformInternalFormatToFrame(audioframe_s* frame, int* internalData);

#endif /* _AUDIOTOOLS_H_ */