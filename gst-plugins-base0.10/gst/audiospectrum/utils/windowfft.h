/**
  * @file		windowfft.h
  * @brief	header file with definition of window function for FFT transform

  * Copyright 2011 by Samsung Electronics, Inc.,
  *
  * This software is the confidential and proprietary information
  * of Samsung Electronics, Inc. ("Confidential Information").  You
  * shall not disclose such Confidential Information and shall use
  * it only in accordance with the terms of the license agreement
  * you entered into with Samsung.
  */

#ifndef _WINDOWFFT_H_
#define _WINDOWFFT_H_

/**
  * @brief			the function calculates window for FFT transform, window coof is in fx16 format
  * @param			[in]size		size of window
  * @return							pointer to window in fx16 format, null on error 
  * @see							WindowFFT.cpp
  */
int* GetWindowFFTfx16(int size);

#endif /* _WINDOWFFT_H_ */