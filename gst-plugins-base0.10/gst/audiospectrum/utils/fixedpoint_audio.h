/**
  * @file	fixedpoint_audio.h
  * @brief	header file with some tools for work with fixed points

  * Copyright 2011 by Samsung Electronics, Inc.,
  *
  * This software is the confidential and proprietary information
  * of Samsung Electronics, Inc. ("Confidential Information").  You
  * shall not disclose such Confidential Information and shall use
  * it only in accordance with the terms of the license agreement
  * you entered into with Samsung.
  */

#ifndef _FIXEDPOINT_AUDIO_H_
#define _FIXEDPOINT_AUDIO_H_

#ifdef __cplusplus
extern "C"
{
#endif 

// Fixed point number with 22:10 bit distribution
typedef int FX10_TYPE;

#define INT_ROUND(val)       (int)( (val) + (((val) >= 0.0f) ? 0.5f : -0.5f))

#define FX10_ONE            (0x00000400)
#define FX10_HALF           (FX10_ONE >> 1)
#define INT2FX10(i)         ((i) << 10)
#define FX10_2INT(fx)       ((fx) >> 10)
#define FX10_2INT_ROUND(fx) ((((fx) >= 0) ? ((fx) + FX10_HALF) : ((fx) - FX10_HALF)) >> 10)
#define FX10_2INT_CEIL(fx)  ((((fx) >= 0) ? ((fx) + FX10_ONE - 1) : ((fx) - FX10_ONE + 1)) >> 10)
#define FLT2FX10(n)         INT_ROUND((n) * 1024.0f)
#define FX10_2FLT(f)        ((f) / 1024.0f)
#define FX10_MUL(l, r)      (((l) * (r)) >> 10)
#define FX10_DIV(l, r)      (((l) << 10) / (r))
#define FX10_FRAQ(f)        ((f) & (FX10_ONE - 1))

// Performs bi-linear inerpolation of four values; result is in the same format, as lt, rt, lb, rb
#define FX10_BLF(lt, rt, lb, rb, uf, vf)\
        (((((lt) * (FX10_ONE - (uf)) + (rt) * (uf)) >> 6) * (FX10_ONE - (vf)) + \
          (((lb) * (FX10_ONE - (uf)) + (rb) * (uf)) >> 6) * (vf)) >> 14)

// Fixed point number with 16:16 bit distribution
typedef int FX16_TYPE;

#define FX16_ONE            (0x00010000)
#define FX16_HALF           (FX16_ONE >> 1)

#define INT2FX16(i)         ((i) << 16)
#define FX16_2INT(fx)       ((fx) >> 16)
#define FX16_2INT_ROUND(fx) ((((fx) >= 0) ? ((fx) + FX16_HALF) : ((fx) - FX16_HALF)) >> 16)
#define FX16_2INT_CEIL(fx)  ((((fx) >= 0) ? ((fx) + FX16_ONE - 1) : ((fx) - FX16_ONE + 1)) >> 16)
#define FLT2FX16(n)         INT_ROUND((n) * 65536.0f)
#define FX16_2FLT(f)        ((f) / 65536.0f)
#define FX16_MUL(l, r)      (((l) * (r)) >> 16)
#define FX16_DIV(l, r)      (((l) << 12) / ((r) >> 4))
#define FX16_FRAQ(f)        ((f) & (FX16_ONE - 1))

// Performs bi-linear inerpolation of four values; result is in the same format, as lt, rt, lb, rb
#define FX16_BLF(lt, rt, lb, rb, uf, vf)\
        (((((lt) * (FX16_ONE - (uf)) + (rt) * (uf)) >> 12) * (FX16_ONE - (vf)) + \
          (((lb) * (FX16_ONE - (uf)) + (rb) * (uf)) >> 12) * (vf)) >> 20)

#ifdef __cplusplus
}
#endif 

#endif  /* _FIXEDPOINT_AUDIO_H_ */
