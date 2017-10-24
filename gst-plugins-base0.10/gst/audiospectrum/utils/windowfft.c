/**
  * @file		windowfft.c
  * @brief		Implementation of window function for FFT transform

  * Copyright 2011 by Samsung Electronics, Inc.,
  *
  * This software is the confidential and proprietary information
  * of Samsung Electronics, Inc. ("Confidential Information").  You
  * shall not disclose such Confidential Information and shall use
  * it only in accordance with the terms of the license agreement
  * you entered into with Samsung.
  */

#include "windowfft.h"
#include "fixedpoint_audio.h"

#include <math.h>
#include <malloc.h>

#define PI 3.1415926536

int* GetWindowFFTfx16(int size)
{
	float* tmp = (float*)malloc(size * sizeof(float));
	
	if(tmp == 0)
	{
		return 0;
	}

	for(int i = 0; i < size; ++i)
	{
		tmp[i] = (float)i/size;
	}
	
	float  a0 = 0.35875f, a1 = 0.48829f, a2 = 0.14128f, a3 = 0.01168f;
	for(int i = 0; i < size; ++i)
	{
		tmp[i] = (float)(a0 - a1 * cos(2 * PI * tmp[i]) + a2 * cos(2 * PI * 2 * tmp[i])- a3 * cos(2 * PI * 3 * tmp[i]));		
	}
	
	int* res = (int*)malloc(size * sizeof(int));
	
	if(res == 0)
	{
		free(tmp);
		return 0;
	}

	for(int i = 0; i < size; ++i)
	{
		res[i] = FLT2FX16(tmp[i]);
	}
	
	free(tmp);

	return res;
}
