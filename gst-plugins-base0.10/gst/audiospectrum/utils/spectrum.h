/**
  * @file	spectrum.h
  * @brief	header file with API of spectrum analyzer

  * Copyright 2011 by Samsung Electronics, Inc.,
  *
  * This software is the confidential and proprietary information
  * of Samsung Electronics, Inc. ("Confidential Information").  You
  * shall not disclose such Confidential Information and shall use
  * it only in accordance with the terms of the license agreement
  * you entered into with Samsung.
  */

#ifndef _SPECTRUM_H_
#define _SPECTRUM_H_

#include "audioframe.h"

typedef struct
{
	void* priv;
}audiospectrum_s;
typedef audiospectrum_s* audiospectrum_h;

typedef struct
{
	gint bands[31];
}audiospectum_band_s;

typedef enum
{
	AUDIOSPECTRUM_ERROR_NONE,
	AUDIOSPECTRUM_ERROR
}audiospectrum_error_e;

audiospectrum_error_e audiospectrum_create(audiospectrum_h* analyser);

audiospectrum_error_e audiospectrum_analyse(
	audiospectrum_h analyser, audioframe_s* audioframe, audiospectum_band_s* spectrum);

audiospectrum_error_e audiospectrum_destroy(audiospectrum_h analyser);

#endif /* _SPECTRUM_H_ */
