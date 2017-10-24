/**
  * @file	spectrum.c
  * @brief	Implementation of spectrum analyzer

  * Copyright 2011 by Samsung Electronics, Inc.,
  *
  * This software is the confidential and proprietary information
  * of Samsung Electronics, Inc. ("Confidential Information").  You
  * shall not disclose such Confidential Information and shall use
  * it only in accordance with the terms of the license agreement
  * you entered into with Samsung.
  */

#include "spectrum.h"
#include "audioframe.h"
#include "kiss_fft.h"
#include "fixedpoint_audio.h"
#include "windowfft.h"
#include "audiotools.h"

#include <math.h>
#include <malloc.h>

#define BYTE_PER_SAMPLE (2)
#define LOWER_BOUND_DB (-1000)
#define PI (3.1415926536)
#define SIZE_FOR_FFT (4096)
#define SHIFT_FOR_BANDS_BORDER (2)
#define NUMBER_OF_BANDS (31)

typedef struct
{
	int* window;			/* pointer to window for FFT */
	int* buffer;			/* pointer to buffer for collecting data */
	kiss_fft_cpx* fin;		/* input buffer for fft */
	kiss_fft_cpx* fout;		/* output buffer for fft */
	kiss_fft_cfg cfgD;		/* direct fft transform struct */
	int currentDataSize;	/* size of samples in bytes (one sample packed in int) */
}StateAudioSpectrumAnalyzer;

audiospectrum_error_e audiospectrum_create(audiospectrum_h* analyser)
{
	*analyser = (audiospectrum_h)malloc(sizeof(audiospectrum_s));
	(*analyser)->priv = (void*)(StateAudioSpectrumAnalyzer*)malloc(sizeof(StateAudioSpectrumAnalyzer));
	StateAudioSpectrumAnalyzer* state = (StateAudioSpectrumAnalyzer*)(*analyser)->priv;
	
	if(state == 0)
	{
		return AUDIOSPECTRUM_ERROR;
	}

	state->currentDataSize = 0;

	state->fin = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * SIZE_FOR_FFT);
	if(state->fin == 0)
	{
		audiospectrum_destroy(state);
		return AUDIOSPECTRUM_ERROR;
	}

	state->fout = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * SIZE_FOR_FFT);
	if(state->fout == 0)
	{
		audiospectrum_destroy(state);
		return AUDIOSPECTRUM_ERROR;
	}

	state->cfgD = kiss_fft_alloc(SIZE_FOR_FFT, FALSE, 0, 0);
	if(state->cfgD == 0)
	{	
		audiospectrum_destroy(state);
		return AUDIOSPECTRUM_ERROR;
	}

	state->window = GetWindowFFTfx16(SIZE_FOR_FFT);
	if(state->window == 0)
	{
		audiospectrum_destroy(state);
		return AUDIOSPECTRUM_ERROR;
	}
	
	state->buffer = (int*)malloc(sizeof(int) * SIZE_FOR_FFT);
	if(state->buffer == 0)
	{
		audiospectrum_destroy(state);
		return AUDIOSPECTRUM_ERROR;
	}

	return AUDIOSPECTRUM_ERROR_NONE;
}

audiospectrum_error_e audiospectrum_destroy(audiospectrum_h analyser)
{
    if(!analyser)
	{
		return AUDIOSPECTRUM_ERROR_NONE;
	}
	StateAudioSpectrumAnalyzer* state = (StateAudioSpectrumAnalyzer*)analyser->priv;

	if(state == 0)
	{
		return AUDIOSPECTRUM_ERROR;
	}

	StateAudioSpectrumAnalyzer* stateInternal = (StateAudioSpectrumAnalyzer*)state;

	if(stateInternal->window)
	{
		free(stateInternal->window);
	}

	if(stateInternal->fin)
	{
		free(stateInternal->fin);
	}

	if(stateInternal->fout)
	{
		free(stateInternal->fout);
	}

	if(stateInternal->cfgD)
	{
		free(stateInternal->cfgD);
	}

	if(stateInternal->buffer)
	{
		free(stateInternal->buffer);
	}

	if(stateInternal)
	{
		free(stateInternal);
	}

	if(analyser)
	{
		free(analyser);
	}

	return AUDIOSPECTRUM_ERROR_NONE;
}


/**
  * @brief	compute log2 in integer
  */
static int log2_(int N)
{
	int log = 0;

	while(N != 1)
	{
		N >>= 1;
		log++;
	}
	return log;
}

extern int limits[]; /** massive with limits of bands */
extern unsigned long long logTable[]; /** lut table for log function */

/**
  * @brief	compute log10 from integer
  */
static unsigned int log10Int(unsigned int n)
{
	int res = 0;
	while((n /= 10) > 0)
	{
		res++;
	}
	return res;
}

/**
  * @brief	compute log10 from long long
  */
static int log10Long(long long n)
{
	int res = 0;
	while((n /= 10) > 0)
	{
		res++;
	}
	return res;
}

/**
  * @brief	perform binary search in sorted array
  */
static int BinarySearch(unsigned long long a[], int low, int high, unsigned long long target) 
{
    while (low <= high) 
	{
        int middle = low + (high - low) / 2;
        if (target < a[middle])
		{
            high = middle - 1;
		}
		else if (target > a[middle])
		{
			low = middle + 1;
		}
        else
		{
            return middle;
		}
    }
    return high;
}

/**
  * @brief	Get log10 using LUT, returns log value multiplied by 40
  */
static int Log10Lut(unsigned long long val)
{
	return BinarySearch(logTable, 0, 760, val);
}

/**
  * @brief	compute new bands values
  */
static int PowersProcessBlock(audioframe_s *ptData, audiospectum_band_s *ptAudioGraph, StateAudioSpectrumAnalyzer* state)
{
	long long energy = 0;
	for(int i = 0; i < SIZE_FOR_FFT; ++i)
	{
		int tmp = FX16_MUL(state->buffer[i], state->window[i]);
		energy += tmp * tmp;
	
		state->fin[i].i = 0;
		state->fin[i].r = tmp * SIZE_FOR_FFT;
	}
	
	int M = log2_(SIZE_FOR_FFT); 
	kiss_fft(state->cfgD, state->fin, state->fout);
	int curBand = 0;

	long long bandEnergy[NUMBER_OF_BANDS];
	for(int i = 0; i < NUMBER_OF_BANDS; ++i)
	{
		bandEnergy[i] = 0;
	}

	for(int i = 0; i < SIZE_FOR_FFT / 2 && curBand <= NUMBER_OF_BANDS; ++i)
	{
		if(i * (1 << SHIFT_FOR_BANDS_BORDER) * (ptData->sample_freq) >= (unsigned int)limits[curBand] * SIZE_FOR_FFT) // compare samplerate/2*i/n with border freq
		{
			++curBand;
			if(curBand == NUMBER_OF_BANDS + 1)
			{
				break;
			}
		}
		if(curBand>0)
		{
			bandEnergy[curBand - 1] += ((long long)state->fout[i].r * state->fout[i].r + (long long)state->fout[i].i * state->fout[i].i);
		}
	}

	for(int i = 0; i < NUMBER_OF_BANDS; ++i)
	{
		int tmp;
		if(bandEnergy[i] == 0)
		{
			tmp = LOWER_BOUND_DB;
		}
		else
		{
			tmp = (Log10Lut(bandEnergy[i]) - Log10Lut(energy) - Log10Lut(SIZE_FOR_FFT)) / 2; // *20 /40
			if(tmp < LOWER_BOUND_DB)
			{
				tmp = LOWER_BOUND_DB;
			}
		}
		ptAudioGraph->bands[i] = tmp;
	}

	return 0;
}

audiospectrum_error_e audiospectrum_analyse(
	audiospectrum_h analyser, audioframe_s* audioframe, audiospectum_band_s* spectrum)
{
	StateAudioSpectrumAnalyzer* state = (StateAudioSpectrumAnalyzer*)analyser->priv;
	audioframe_s* ptData = audioframe;
	audiospectum_band_s* ptAudioGraph = spectrum;

	if(state == 0 || ptAudioGraph == 0)
	{
		return AUDIOSPECTRUM_ERROR;
	}

	if(ptData == 0 || ptData->num_ch <= 0 || ptData->size <= 0 || ptData->data == 0 || (ptData->sample_size != 2 && ptData->sample_size != 4) || (ptData->sample_freq != 44100 && ptData->sample_freq != 48000))
	{
		return AUDIOSPECTRUM_ERROR;
	}

	int sizeSamples = ptData->size / ptData->sample_size;
	int sizeFrames = sizeSamples / ptData->num_ch;

	int* pData = TransformFrameToInternalFormat(ptData);

	if(pData == 0)
	{
		return AUDIOSPECTRUM_ERROR;
	}
	//TransformInternalFormatToFrame(ptData, pData); //TODO: check this string

	//Spectrum Analyser works with 16-16 fixed point data, so if we have 24 bit data placed in int we devide it on 256
	if( ptData->sample_size == 4)
	{
		for(int i = 0; i < sizeSamples; ++i)
		{
			pData[i] >>= 8; 
		}
	}

	int diff = ((int)state->currentDataSize + sizeFrames) - SIZE_FOR_FFT; //difference between samples we have and samples we need, another words how many extra(not need) data we have
	
	//if diff >= then then we have enough data to process
	if(diff >= 0)
	{
		if(diff >= state->currentDataSize)//if true our new part of data is bigger or equal then need, so we put in out bufer last part of new data and process (sizeFrames >= state->typicalSize)
		{
			int indStart = sizeFrames -  SIZE_FOR_FFT;
			for(int i = indStart, j = 0; j < SIZE_FOR_FFT; ++i, ++j)
			{
				int sum = 0; //we exept that data is 24 bit or 16bit so no overflow
				for(unsigned int ch = 0; ch < ptData->num_ch; ++ch)
				{
					sum += pData[i * ptData->num_ch + ch];
				}

				sum /= (int)ptData->num_ch;
				state->buffer[j] = sum;
			}
			PowersProcessBlock(ptData, ptAudioGraph, state);
			state->currentDataSize = 0;
		}
		else //if false our new part of data is smaller than need, so we need to shift begin of old data and add new part of data to the end and process(sizeFrames < state->typicalSize)
		{
			memmove(&state->buffer[0], &state->buffer[diff], (state->currentDataSize - diff) * 4);
			int indStart = state->currentDataSize - diff;
			for(int i = indStart, j = 0; i < SIZE_FOR_FFT; ++i, ++j)
			{
				int sum = 0; //we exept that data is 24 bit or 16bit so no overflow
				for(unsigned int ch = 0; ch < ptData->num_ch; ++ch)
				{
					sum += pData[j * ptData->num_ch + ch];
				}
				sum /= (int)ptData->num_ch;

				state->buffer[i] = sum;
			}
			
			PowersProcessBlock(ptData, ptAudioGraph, state);
			state->currentDataSize = 0;
		}
		
		free(pData);
		return 0;
	}
	else //we have no data to process so we just collect
	{
		for(int i = state->currentDataSize, j = 0; j < sizeFrames; ++i, ++j)
		{
			int sum = 0; //we exept that data is 24 bit or 16bit so no overflow
			for(unsigned int ch = 0; ch < ptData->num_ch; ++ch)
			{
				sum += pData[j * ptData->num_ch + ch];
			}
			sum /= (int)ptData->num_ch;

			state->buffer[i] = sum;
		}
		state->currentDataSize += sizeFrames;
		
		free(pData);
		return AUDIOSPECTRUM_ERROR;
	}

	return AUDIOSPECTRUM_ERROR_NONE;
}

unsigned long long logTable[] =
 {
                   1ULL,                    1ULL,                    1ULL,
                   1ULL,                    1ULL,                    1ULL,
                   1ULL,                    1ULL,                    1ULL,
                   1ULL,                    1ULL,                    1ULL,
                   1ULL,                    2ULL,                    2ULL,
                   2ULL,                    2ULL,                    2ULL,
                   2ULL,                    2ULL,                    3ULL,
                   3ULL,                    3ULL,                    3ULL,
                   3ULL,                    4ULL,                    4ULL,
                   4ULL,                    5ULL,                    5ULL,
                   5ULL,                    5ULL,                    6ULL,
                   6ULL,                    7ULL,                    7ULL,
                   7ULL,                    8ULL,                    8ULL,
                   9ULL,                   10ULL,                   10ULL,
                  11ULL,                   11ULL,                   12ULL,
                  13ULL,                   14ULL,                   14ULL,
                  15ULL,                   16ULL,                   17ULL,
                  18ULL,                   19ULL,                   21ULL,
                  22ULL,                   23ULL,                   25ULL,
                  26ULL,                   28ULL,                   29ULL,
                  31ULL,                   33ULL,                   35ULL,
                  37ULL,                   39ULL,                   42ULL,
                  44ULL,                   47ULL,                   50ULL,
                  53ULL,                   56ULL,                   59ULL,
                  63ULL,                   66ULL,                   70ULL,
                  74ULL,                   79ULL,                   84ULL,
                  89ULL,                   94ULL,                   99ULL,
                 105ULL,                  112ULL,                  118ULL,
                 125ULL,                  133ULL,                  141ULL,
                 149ULL,                  158ULL,                  167ULL,
                 177ULL,                  188ULL,                  199ULL,
                 211ULL,                  223ULL,                  237ULL,
                 251ULL,                  266ULL,                  281ULL,
                 298ULL,                  316ULL,                  334ULL,
                 354ULL,                  375ULL,                  398ULL,
                 421ULL,                  446ULL,                  473ULL,
                 501ULL,				  530ULL,                  562ULL,
                 595ULL,			      630ULL,                  668ULL,
                 707ULL,				  749ULL,                  794ULL,
                 841ULL,		          891ULL,                  944ULL,
                 999ULL,          	     1059ULL,                 1122ULL,
                1188ULL,          	     1258ULL,                 1333ULL,
                1412ULL,          	     1496ULL,                 1584ULL,
                1678ULL,          	     1778ULL,                 1883ULL,
                1995ULL,          	     2113ULL,                 2238ULL,
                2371ULL,          	     2511ULL,                 2660ULL,
                2818ULL,          	     2985ULL,                 3162ULL,
                3349ULL,          	     3548ULL,                 3758ULL,
                3981ULL,          	     4216ULL,                 4466ULL,
                4731ULL,          	     5011ULL,                 5308ULL,
                5623ULL,          	     5956ULL,                 6309ULL,
                6683ULL,          	     7079ULL,                 7498ULL,
                7943ULL,          	     8413ULL,                 8912ULL,
                9440ULL,          	     9999ULL,                10592ULL,
               11220ULL,          	    11885ULL,                12589ULL,
               13335ULL,          	    14125ULL,                14962ULL,
               15848ULL,          	    16788ULL,                17782ULL,
               18836ULL,          	    19952ULL,                21134ULL,
               22387ULL,          	    23713ULL,                25118ULL,
               26607ULL,          	    28183ULL,                29853ULL,
               31622ULL,          	    33496ULL,                35481ULL,
               37583ULL,          	    39810ULL,                42169ULL,
               44668ULL,          	    47315ULL,                50118ULL,
               53088ULL,          	    56234ULL,                59566ULL,
               63095ULL,          	    66834ULL,                70794ULL,
               74989ULL,          	    79432ULL,                84139ULL,
               89125ULL,          	    94406ULL,               100000ULL,
              105925ULL,          	   112201ULL,               118850ULL,
              125892ULL,          	   133352ULL,               141253ULL,
              149623ULL,          	   158489ULL,               167880ULL,
              177827ULL,          	   188364ULL,               199526ULL,
              211348ULL,          	   223872ULL,               237137ULL,
              251188ULL,          	   266072ULL,               281838ULL,
              298538ULL,          	   316227ULL,               334965ULL,
              354813ULL,          	   375837ULL,               398107ULL,
              421696ULL,          	   446683ULL,               473151ULL,
              501187ULL,          	   530884ULL,               562341ULL,
              595662ULL,          	   630957ULL,               668343ULL,
              707945ULL,          	   749894ULL,               794328ULL,
              841395ULL,          	   891250ULL,               944060ULL,
             1000000ULL,          	  1059253ULL,              1122018ULL,
             1188502ULL,			  1258925ULL,              1333521ULL,
             1412537ULL,        	  1496235ULL,              1584893ULL,
             1678804ULL,        	  1778279ULL,              1883649ULL,
             1995262ULL,        	  2113489ULL,              2238721ULL,
             2371373ULL,        	  2511886ULL,              2660725ULL,
             2818382ULL,        	  2985382ULL,              3162277ULL,
             3349654ULL,        	  3548133ULL,              3758374ULL,
             3981071ULL,        	  4216965ULL,              4466835ULL,
             4731512ULL,        	  5011872ULL,              5308844ULL,
             5623413ULL,        	  5956621ULL,              6309573ULL,
             6683439ULL,        	  7079457ULL,              7498942ULL,
             7943282ULL,        	  8413951ULL,              8912509ULL,
             9440608ULL,        	 10000000ULL,             10592537ULL,
            11220184ULL,        	 11885022ULL,             12589254ULL,
            13335214ULL,        	 14125375ULL,             14962356ULL,
            15848931ULL,        	 16788040ULL,             17782794ULL,
            18836490ULL,        	 19952623ULL,             21134890ULL,
            22387211ULL,        	 23713737ULL,             25118864ULL,
            26607250ULL,        	 28183829ULL,             29853826ULL,
            31622776ULL,        	 33496543ULL,             35481338ULL,
            37583740ULL,        	 39810717ULL,             42169650ULL,
            44668359ULL,        	 47315125ULL,             50118723ULL,
            53088444ULL,        	 56234132ULL,             59566214ULL,
            63095734ULL,        	 66834391ULL,             70794578ULL,
            74989420ULL,        	 79432823ULL,             84139514ULL,
            89125093ULL,        	 94406087ULL,            100000000ULL,
           105925372ULL,      		112201845ULL,            118850222ULL,
           125892541ULL,      		133352143ULL,            141253754ULL,
           149623565ULL,      		158489319ULL,            167880401ULL,
           177827941ULL,      		188364908ULL,            199526231ULL,
           211348903ULL,      		223872113ULL,            237137370ULL,
           251188643ULL,      		266072505ULL,            281838293ULL,
           298538261ULL,      		316227766ULL,            334965439ULL,
           354813389ULL,			375837404ULL,            398107170ULL,
           421696503ULL,			446683592ULL,            473151258ULL,
           501187233ULL,			530884444ULL,            562341325ULL,
           595662143ULL,			630957344ULL,            668343917ULL,
           707945784ULL,            749894209ULL,            794328234ULL,
           841395141ULL,            891250938ULL,            944060876ULL,
          1000000000ULL,           1059253725ULL,           1122018454ULL,
          1188502227ULL,           1258925411ULL,           1333521432ULL,
          1412537544ULL,           1496235656ULL,           1584893192ULL,
          1678804018ULL,           1778279410ULL,           1883649089ULL,
          1995262314ULL,           2113489039ULL,           2238721138ULL,
          2371373705ULL,           2511886431ULL,           2660725059ULL,
          2818382931ULL,           2985382618ULL,           3162277660ULL,
          3349654391ULL,           3548133892ULL,           3758374042ULL,
          3981071705ULL,           4216965034ULL,           4466835921ULL,
          4731512589ULL,           5011872336ULL,           5308844442ULL,
          5623413251ULL,           5956621435ULL,           6309573444ULL,
          6683439175ULL,           7079457843ULL,           7498942093ULL,
          7943282347ULL,           8413951416ULL,           8912509381ULL,
          9440608762ULL,          10000000000ULL,          10592537251ULL,
         11220184543ULL,          11885022274ULL,          12589254117ULL,
         13335214321ULL,          14125375446ULL,          14962356560ULL,
         15848931924ULL,          16788040181ULL,          17782794100ULL,
         18836490894ULL,          19952623149ULL,          21134890398ULL,
         22387211385ULL,          23713737056ULL,          25118864315ULL,
         26607250597ULL,          28183829312ULL,          29853826189ULL,
         31622776601ULL,          33496543915ULL,          35481338923ULL,
         37583740428ULL,          39810717055ULL,          42169650342ULL,
         44668359215ULL,          47315125896ULL,          50118723362ULL,
         53088444423ULL,          56234132519ULL,          59566214352ULL,
         63095734448ULL,          66834391756ULL,          70794578438ULL,
         74989420933ULL,          79432823472ULL,          84139514164ULL,
         89125093813ULL,          94406087628ULL,         100000000000ULL,
        105925372517ULL,         112201845430ULL,         118850222743ULL,
        125892541179ULL,         133352143216ULL,         141253754462ULL,
        149623565609ULL,         158489319246ULL,         167880401812ULL,
        177827941003ULL,         188364908949ULL,         199526231496ULL,
        211348903983ULL,         223872113856ULL,         237137370566ULL,
        251188643151ULL,         266072505979ULL,         281838293126ULL,
        298538261891ULL,         316227766016ULL,         334965439157ULL,
        354813389233ULL,         375837404288ULL,         398107170553ULL,
        421696503428ULL,         446683592151ULL,         473151258961ULL,
        501187233627ULL,         530884444231ULL,         562341325190ULL,
        595662143529ULL,         630957344480ULL,         668343917568ULL,
        707945784384ULL,         749894209332ULL,         794328234724ULL,
        841395141645ULL,         891250938133ULL,         944060876286ULL,
       1000000000000ULL,        1059253725177ULL,        1122018454302ULL,
       1188502227437ULL,        1258925411794ULL,        1333521432163ULL,
       1412537544623ULL,        1496235656094ULL,        1584893192461ULL,
       1678804018122ULL,        1778279410039ULL,        1883649089490ULL,
       1995262314969ULL,        2113489039837ULL,        2238721138568ULL,
       2371373705662ULL,        2511886431510ULL,        2660725059799ULL,
       2818382931265ULL,        2985382618918ULL,        3162277660169ULL,
       3349654391579ULL,        3548133892336ULL,        3758374042885ULL,
       3981071705535ULL,        4216965034286ULL,        4466835921510ULL,
       4731512589616ULL,        5011872336274ULL,        5308844442311ULL,
       5623413251904ULL,        5956621435291ULL,        6309573444803ULL,
       6683439175687ULL,        7079457843843ULL,        7498942093326ULL,
       7943282347244ULL,        8413951416454ULL,        8912509381339ULL,
       9440608762861ULL,       10000000000002ULL,       10592537251775ULL,
      11220184543022ULL,       11885022274373ULL,       12589254117945ULL,
      13335214321636ULL,       14125375446231ULL,       14962356560948ULL,
      15848931924615ULL,       16788040181230ULL,       17782794100394ULL,
      18836490894903ULL,       19952623149694ULL,       21134890398372ULL,
      22387211385689ULL,       23713737056623ULL,       25118864315102ULL,
      26607250597995ULL,       28183829312652ULL,       29853826189188ULL,
      31622776601692ULL,       33496543915792ULL,       35481338923367ULL,
      37583740428855ULL,       39810717055361ULL,       42169650342870ULL,
      44668359215109ULL,       47315125896161ULL,       50118723362741ULL,
      53088444423114ULL,       56234132519051ULL,       59566214352918ULL,
      63095734448038ULL,       66834391756881ULL,       70794578438434ULL,
      74989420933267ULL,       79432823472451ULL,       84139514164544ULL,
      89125093813401ULL,       94406087628620ULL,      100000000000030ULL,
     105925372517761ULL,      112201845430230ULL,      118850222743738ULL,
     125892541179455ULL,      133352143216373ULL,      141253754462318ULL,
     149623565609489ULL,      158489319246160ULL,      167880401812308ULL,
     177827941003947ULL,      188364908949038ULL,      199526231496950ULL,
     211348903983730ULL,      223872113856904ULL,      237137370566240ULL,
     251188643151037ULL,      266072505979965ULL,      281838293126534ULL,
     298538261891890ULL,      316227766016938ULL,      334965439157934ULL,
     354813389233689ULL,      375837404288565ULL,      398107170553625ULL,
     421696503428718ULL,      446683592151107ULL,      473151258961634ULL,
     501187233627435ULL,      530884444231161ULL,      562341325190533ULL,
     595662143529205ULL,      630957344480400ULL,      668343917568835ULL,
     707945784384371ULL,      749894209332704ULL,      794328234724545ULL,
     841395141645475ULL,      891250938134043ULL,      944060876286239ULL,
    1000000000000335ULL,     1059253725177645ULL,     1122018454302341ULL,
    1188502227437420ULL,     1258925411794593ULL,     1333521432163776ULL,
    1412537544623235ULL,     1496235656094943ULL,     1584893192461655ULL,
    1678804018123135ULL,     1778279410039533ULL,     1883649089490449ULL,
    1995262314969568ULL,     2113489039837378ULL,     2238721138569116ULL,
    2371373705662479ULL,     2511886431510455ULL,     2660725059799739ULL,
    2818382931265440ULL,     2985382618919007ULL,     3162277660169491ULL,
    3349654391579457ULL,     3548133892337008ULL,     3758374042885773ULL,
    3981071705536386ULL,     4216965034287323ULL,     4466835921511224ULL,
    4731512589616496ULL,     5011872336274519ULL,     5308844442311790ULL,
    5623413251905515ULL,     5956621435292253ULL,     6309573444804214ULL,
    6683439175688568ULL,     7079457843843950ULL,     7498942093327288ULL,
    7943282347245713ULL,     8413951416455028ULL,     8912509381340722ULL,
    9440608762862702ULL,    10000000000003682ULL,    10592537251776754ULL,
   11220184543023692ULL,    11885022274374442ULL,    12589254117946142ULL,
   13335214321637932ULL,    14125375446232466ULL,    14962356560949500ULL,
   15848931924616554ULL,    16788040181231288ULL,    17782794100395192ULL,
   18836490894904260ULL,    19952623149695356ULL,    21134890398373348ULL,
   22387211385690612ULL,    23713737056624120ULL,    25118864315103732ULL,
   26607250597996412ULL,    28183829312653252ULL,    29853826189188728ULL,
   31622776601693364ULL,    33496543915792796ULL,    35481338923368052ULL,
   37583740428855424ULL,    39810717055361256ULL,    42169650342870296ULL,
   44668359215108952ULL,    47315125896161288ULL,    50118723362741088ULL,
   53088444423113344ULL,    56234132519050088ULL,    59566214352916928ULL,
   63095734448035944ULL,    66834391756878848ULL,    70794578438431976ULL,
   74989420933264592ULL,    79432823472448032ULL,    84139514164540288ULL,
   89125093813396272ULL,    94406087628615040ULL,   100000000000023728ULL,
  105925372517753664ULL,   112201845430222224ULL,   118850222743728880ULL,
  125892541179444944ULL,   133352143216361856ULL,   141253754462306160ULL,
  149623565609475424ULL,   158489319246144800ULL,   167880401812290912ULL,
  177827941003928640ULL,   188364908949017952ULL,   199526231496927456ULL,
  211348903983705824ULL,   223872113856876800ULL,   237137370566210144ULL,
  251188643151004448ULL,   266072505979929280ULL,   281838293126495648ULL,
  298538261891848192ULL,   316227766016892288ULL,   334965439157884096ULL,
  354813389233634112ULL,   375837404288505024ULL,   398107170553560448ULL,
  421696503428647808ULL,   446683592151031104ULL,   473151258961550912ULL,
  501187233627345280ULL,   530884444231063936ULL,   562341325190427264ULL,
  595662143529091328ULL,   630957344480276864ULL,   668343917568701056ULL,
  707945784384227072ULL,   749894209332547840ULL,   794328234724376320ULL,
  841395141645292800ULL,   891250938133846144ULL,   944060876286026880ULL,
 1000000000000106368ULL,  1059253725177398016ULL,  1122018454302075392ULL,
 1188502227437133056ULL,  1258925411794284544ULL,  1333521432163443968ULL,
 1412537544622876672ULL,  1496235656094558208ULL,  1584893192461240576ULL,
 1678804018122689280ULL,  1778279410039053824ULL,  1883649089489933056ULL,
 1995262314969013504ULL,  2113489039836781568ULL,  2238721138568475136ULL,
 2371373705661791232ULL,  2511886431509715968ULL,  2660725059798944768ULL,
 2818382931264587776ULL,  2985382618918091264ULL,  3162277660168508928ULL,
 3349654391578402816ULL,  3548133892335876608ULL,  3758374042884558336ULL,
 3981071705535083008ULL,  4216965034285925888ULL,  4466835921509726208ULL,
 4731512589614889984ULL,  5011872336272796672ULL,  5308844442309944320ULL,
 5623413251903537152ULL,  5956621435290133504ULL,  6309573444801942528ULL,
 6683439175686134784ULL,  7079457843841344512ULL,  7498942093324496896ULL,
 7943282347242724352ULL,  8413951416451826688ULL,  8912509381337294848ULL,
 9223372036854775808ULL,  9223372036854775808ULL			};	/** lut table for log */	  


int limits[32] = {	(int)(17.78 * (1<<SHIFT_FOR_BANDS_BORDER)),		(int)(22.39 * (1<<SHIFT_FOR_BANDS_BORDER)),		(int)(28.18 * (1<<SHIFT_FOR_BANDS_BORDER)),		(int)(35.48 * (1<<SHIFT_FOR_BANDS_BORDER)),		(int)(44.67 * (1<<SHIFT_FOR_BANDS_BORDER)),		(int)(56.23 * (1<<SHIFT_FOR_BANDS_BORDER)),		(int)(70.79 * (1<<SHIFT_FOR_BANDS_BORDER)),		(int)(89.13 * (1<<SHIFT_FOR_BANDS_BORDER)), 
					(int)(112.20 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(141.25 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(177.83 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(223.87 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(281.84 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(354.81 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(446.68 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(562.34 * (1<<SHIFT_FOR_BANDS_BORDER)), 
					(int)(707.95 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(891.25 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(1122.02 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(1412.54 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(1778.28 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(2238.72 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(2818.38 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(3548.13 * (1<<SHIFT_FOR_BANDS_BORDER)), 
					(int)(4466.84 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(5623.41 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(7079.46 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(8912.51 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(11220.18 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(14125.38 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(17782.79 * (1<<SHIFT_FOR_BANDS_BORDER)),	(int)(22387.21 * (1<<SHIFT_FOR_BANDS_BORDER)) }; /** limits for bands */

