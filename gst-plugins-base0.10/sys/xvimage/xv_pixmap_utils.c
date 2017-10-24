/**************************************************************************

Copyright 2010 - 2013 Samsung Electronics co., Ltd. All Rights Reserved.

Contact: Boram Park <boram1288.park@samsung.com>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/file.h>

#include <stdio.h>
#include <malloc.h>
#include <dlfcn.h>

#include "xv_pixmap_utils.h"

// This is to enable Color Conversion code to Render SW Decoded Output on to Grapghics Plane : 0 : to Dsiable  1: to Enable
#define NEG_PADDING_SIZE (1024)
#define ALPHA (0xff000000)

static int *crop_shift = NULL;
static int colorconv_ref = 0;

/* 5 tables used for color conversion from YCbCr->rgb

T1: 1634*(Cr-128) + 512
T2: -401*(Cb-128)
T3: -832*(Cr-128) + 512
T4: 2066*(Cb-128) + 512
T5: (Y - 16)*1192

*/
static int T3T1_V_Tables[256] = {
	  6881076,    6815541,    6750007,    6750009,    6684474,    6618940,    6553405,    6487871,    6487873,    6422338,
	  6356804,    6291269,    6225735,    6160200,    6160202,    6094668,    6029133,    5963599,    5898064,    5898066,
	  5832532,    5766997,    5701463,    5635928,    5635930,    5570396,    5504861,    5439327,    5373792,    5308258,
	  5308260,    5242725,    5177191,    5111656,    5046122,    5046124,    4980589,    4915055,    4849520,    4783986,
	  4783988,    4718453,    4652919,    4587384,    4521850,    4456316,    4456317,    4390783,    4325248,    4259714,
	  4194180,    4194181,    4128647,    4063112,    3997578,    3932044,    3932045,    3866511,    3800976,    3735442,
	  3669907,    3604373,    3604375,    3538840,    3473306,    3407771,    3342237,    3342239,    3276704,    3211170,
	  3145635,    3080101,    3080103,    3014568,    2949034,    2883499,    2817965,    2752431,    2752432,    2686898,
	  2621363,    2555829,    2490295,    2490296,    2424762,    2359227,    2293693,    2228159,    2228160,    2162626,
	  2097091,    2031557,    1966023,    1900488,    1900490,    1834955,    1769421,    1703887,    1638352,    1638354,
	  1572819,    1507285,    1441751,    1376216,    1376218,    1310683,    1245149,    1179614,    1114080,    1048546,
	  1048547,     983013,     917478,     851944,     	 786410,      786411,      720877,     655342,      589808,      524274,
	   524275,      458741,      393206,     327672,     262138,     196603,     196605,        131070,      0,               -65534,
	  -131069,    -131067,    -196602,    -262136,    -327670,    -393205,    -393203,    -458738,    -524272,    -589806,
	  -655341,    -720875,    -720874,    -786408,    -851942,    -917477,    -983011,    -983010,   -1048544,   -1114078,
	 -1179613,   -1245147,   -1245146,   -1310680,   -1376215,   -1441749,   -1507283,   -1572818,   -1572816,   -1638351,
	 -1703885,   -1769419,   -1834954,   -1834952,   -1900487,   -1966021,   -2031555,   -2097090,   -2097088,   -2162623,
	 -2228157,   -2293691,   -2359226,   -2424760,   -2424759,   -2490293,   -2555827,   -2621362,   -2686896,   -2686895,
	 -2752429,   -2817963,   -2883498,   -2949032,   -2949031,   -3014565,   -3080099,   -3145634,   -3211168,   -3276703,
	 -3276701,   -3342235,   -3407770,   -3473304,   -3538839,   -3538837,   -3604371,   -3669906,   -3735440,   -3800975,
	 -3800973,   -3866508,   -3932042,   -3997576,   -4063111,   -4128645,   -4128644,   -4194178,   -4259712,   -4325247,
	 -4390781,   -4390780,   -4456314,   -4521848,   -4587383,   -4652917,   -4652916,   -4718450,   -4783984,   -4849519,
	 -4915053,   -4980588,   -4980586,   -5046120,   -5111655,   -5177189,   -5242724,   -5242722,   -5308256,   -5373791,
	 -5439325,   -5504860,   -5504858,   -5570392,   -5635927,   -5701461,   -5766996,   -5832530,   -5832528,   -5898063,
	 -5963597,   -6029132,   -6094666,   -6094664,   -6160199,   -6225733,   -6291268,   -6356802,   -6356801,   -6422335,
	 -6487869,   -6553404,   -6618938,   -6684473,   -6684471,   -6750005
};

static int T4T2_U_Tables[256] = {
	285083954,  285215025,  285346097,  285477168,  285608240,  285739312,  285870383,  286001455,  286132526,  286263598,
	286394670,  286525741,  286656813,  286787885,  286918956,  287050028,  287181099,  287312171,  287443243,  287574314,
	287705386,  287836457,  287967529,  288098601,  288229672,  288360744,  288491815,  288622887,  288753959,  288885030,
	289016102,  289147173,  289278245,  289409317,  289540388,  289671460,  289802532,  289933603,  290064675,  290195746,
	290326818,  290457890,  290588961,  290785569,  290916640,  291047712,  291178784,  291309855,  291440927,  291571998,
	291703070,  291834142,  291965213,  292096285,  292227356,  292358428,  292489500,  292620571,  292751643,  292882715,
	293013786,  293144858,  293275929,  293407001,  293538073,  293669144,  293800216,  293931287,  294062359,  294193431,
	294324502,  294455574,  294586645,  294717717,  294848789,  294979860,  295110932,  295242003,  295373075,  295504147,
	295635218,  295766290,  295897362,  296028433,  296159505,  296290576,  296421648,  296552720,  296683791,  296814863,
	296945934,  297077006,  297208078,  297339149,  297470221,  297601292,  297732364,  297863436,  297994507,  298125579,
	298322186,  298453258,  298584330,  298715401,  298846473,  298977545,  299108616,  299239688,  299370759,  299501831,
	299632903,  299763974,  299895046,  300026117,  300157189,  300288261,  300419332,  300550404,  300681475,  300812547,
	300943619,  301074690,  301205762,  301336833,  301467905,  301598977,  301730048,  301861120,  301992192,  302123263,
	302254335,  302385406,  302516478,  302647550,  302778621,  302909693,  303040764,  303171836,  303302908,  303433979,
	303565051,  303696122,  303827194,  303958266,  304089337,  304220409,  304351480,  304482552,  304613624,  304744695,
	304875767,  305006838,  305137910,  305268982,  305400053,  305531125,  305662197,  305858804,  305989876,  306120947,
	306252019,  306383091,  306514162,  306645234,  306776305,  306907377,  307038449,  307169520,  307300592,  307431663,
	307562735,  307693807,  307824878,  307955950,  308087021,  308218093,  308349165,  308480236,  308611308,  308742380,
	308873451,  309004523,  309135594,  309266666,  309397738,  309528809,  309659881,  309790952,  309922024,  310053096,
	310184167,  310315239,  310446310,  310577382,  310708454,  310839525,  310970597,  311101668,  311232740,  311363812,
	311494883,  311625955,  311757027,  311888098,  312019170,  312150241,  312281313,  312412385,  312543456,  312674528,
	312805599,  312936671,  313067743,  313198814,  313395422,  313526493,  313657565,  313788637,  313919708,  314050780,
	314181851,  314312923,  314443995,  314575066,  314706138,  314837210,  314968281,  315099353,  315230424,  315361496,
	315492568,  315623639,  315754711,  315885782,  316016854,  316147926,  316278997,  316410069,  316541140,  316672212,
	316803284,  316934355,  317065427,  317196498,  317327570,  317458642,  317589713,  317720785,  317851857,  317982928,
	318114000,  318245071,  318376143,  318507215,  318638286,  318769358
};

static int T5_Y_Tables[256] = {
	-19,    -18,    -17,    -16,    -14,    -13,    -12,    -11,    -10,    -9,
	-7,     -6,     -5,     -4,     -3,     -2,     0,      1,      2,      3,
	4,      5,      6,      8,      9,      10,     11,     12,     13,     15,
	16,     17,     18,     19,     20,     22,     23,     24,     25,     26,
	27,     29,     30,     31,     32,     33,     34,     36,     37,     38,
	39,     40,     41,     43,     44,     45,     46,     47,     48,     50,
	51,     52,     53,     54,     55,     57,     58,     59,     60,     61,
	62,     64,     65,     66,     67,     68,     69,     71,     72,     73,
	74,     75,     76,     77,     79,     80,     81,     82,     83,     84,
	86,     87,     88,     89,     90,     91,     93,     94,     95,     96,
	97,     98,     100,    101,    102,    103,    104,    105,    107,    108,
	109,    110,    111,    112,    114,    115,    116,    117,    118,    119,
	121,    122,    123,    124,    125,    126,    128,    129,    130,    131,
	132,    133,    135,    136,    137,    138,    139,    140,    142,    143,
	144,    145,    146,    147,    149,    150,    151,    152,    153,    154,
	155,    157,    158,    159,    160,    161,    162,    164,    165,    166,
	167,    168,    169,    171,    172,    173,    174,    175,    176,    178,
	179,    180,    181,    182,    183,    185,    186,    187,    188,    189,
	190,    192,    193,    194,    195,    196,    197,    199,    200,    201,
	202,    203,    204,    206,    207,    208,    209,    210,    211,    213,
	214,    215,    216,    217,    218,    220,    221,    222,    223,    224,
	225,    226,    228,    229,    230,    231,    232,    233,    235,    236,
	237,    238,    239,    240,    242,    243,    244,    245,    246,    247,
	249,    250,    251,    252,    253,    254,    256,    257,    258,    259,
	260,    261,    263,    264,    265,    266,    267,    268,    270,    271,
	272,    273,    274,    275,    277,    278
};

void xv_colorconversion_init(void)
{
	int i;
	int old_ref = g_atomic_int_add (&colorconv_ref, 1);
	if (colorconv_ref == 1)
	{
		/* Crop Table generation is one time effort and shouldn't generated for every track
		     and Crop values are remains unchanged for all files. */
		if (crop_shift)
			return;
		crop_shift = (int*) malloc (sizeof(int) * (256 + 2*NEG_PADDING_SIZE) * 3);
		memset(crop_shift, 0, sizeof(int) * (256 + 2*NEG_PADDING_SIZE) * 3);
		for(i=0; i<NEG_PADDING_SIZE; i++)
		{
			crop_shift[i] = ALPHA | 0;
			crop_shift[2304 + i] = 0;
			crop_shift[4608 + i] = 0;
			crop_shift[i + NEG_PADDING_SIZE + 256] = ALPHA | (0xff << 16); 
			crop_shift[2304 +(i + NEG_PADDING_SIZE + 256)] = (0xff << 8);
			crop_shift[4608 + (i + NEG_PADDING_SIZE + 256)] = 0xff;
		}
		for(int i=0; i<256; i++)
		{
			crop_shift[i + NEG_PADDING_SIZE] = ALPHA | (i<<16);
			crop_shift[2304 + (i + NEG_PADDING_SIZE)] = (i<<8);
			crop_shift[4608 + (i + NEG_PADDING_SIZE)] = i;
		}
	}
}

void xv_colorconversion_deinit(void)
{
	int old_ref = g_atomic_int_add (&colorconv_ref, -1);
	if (colorconv_ref <= 0)
	{
		if (crop_shift)
			free(crop_shift);
		crop_shift = NULL;
	}
}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define ARGB888_VALUE (crop[Y + r_add] | crop[Y + g_add] | crop[Y + b_add])
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#define ARGB888_VALUE ( ((crop[Y + r_add] & 0x00ff0000) >> 8) | (crop[Y + g_add] << 8) | (crop[Y + b_add] << 24) | 0xff)
#endif
void convert_yuv420_interleaved_to_argb(unsigned char *y, unsigned char *CbCr,
	int yLineSize, int uvLineSize, int width, int height, unsigned char *rgbaOut)
{
	int Y,U,V;	
	int16_t r_add, g_add,b_add;
	uint8_t  *uPtr,*vPtr = NULL; 
	uint8_t *y0,*y1;
	uint32_t *rgb0, *rgb1;
	uint32_t h,w, stride, yLine, uvLine,new_width,new_height;
	uint8_t y01,y10,y02,y12;

	int *crop = crop_shift + NEG_PADDING_SIZE;

	yLine = yLineSize;
	uvLine = uvLineSize;
	new_width = width>>1;
	new_height = height>>1;

	y0 = y;
	y1 = y + yLine;

	uPtr = CbCr;

	rgb0 = (uint32_t *)((void *)rgbaOut);
	rgb1 = rgb0 + width;

	stride = width;
	for (h=new_height; h>0; h--)
	{
		for(w=new_width; w>0; w--)
		{
			U = *uPtr++;
			V = *uPtr++;
			r_add = (int16_t)(T3T1_V_Tables[V]);
			g_add = (int16_t) ((int16_t)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (int16_t)(T4T2_U_Tables[U]>>16);

			y01 = *y0++; y02 = *y0++;
			y10 = *y1++; y12 = *y1++;
			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y02];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y10];
			*rgb1++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y12];
			*rgb1++ =  ARGB888_VALUE;
		}

		// if width is ODD then process the remaining pixel (Ex : width = 235, new_width = 117, 117*2 = 234, remaining pixels = 1
		if (width & 0x1)
		{
			U = *uPtr++;
			V = *uPtr++;
			r_add = (int16_t)(T3T1_V_Tables[V]);
			g_add = (int16_t) ((int16_t)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (int16_t)(T4T2_U_Tables[U]>>16);

			y01 = *y0++;
			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
			y10 = *y1++;
			Y = T5_Y_Tables[y10];
			*rgb1++ =  ARGB888_VALUE;
		}

		rgb0 += width;
		rgb1 += width;
		y0 += (yLine+(yLine - width));
		y1 += (yLine+(yLine - width));	
		if (width & 0x1)  // width is ODD
		{
			uPtr += uvLine -(width+1);
		}
		else //// width is EVEN
		{
			uPtr += uvLine -width;
		}
	}

#if 0	// Disable odd height line handle, just ignore it here.
	// if Height is ODD then process the remaining
	if (height & 0x1)
	{
		for(w=new_width; w > 0; w--)
		{
			U = *uPtr++;
			V = *uPtr++;
			r_add = (int16_t)(T3T1_V_Tables[V]);
			g_add = (int16_t) ((int16_t)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (int16_t)(T4T2_U_Tables[U]>>16);

			y01 = *y0++; y02 = *y0++;
			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y02];
			*rgb1++ =  ARGB888_VALUE;
		}

		if (width & 0x1)   
		{
			U = *uPtr++;
			V = *uPtr++;
			r_add = (int16_t)(T3T1_V_Tables[V]);
			g_add = (int16_t) ((int16_t)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (int16_t)(T4T2_U_Tables[U]>>16);

			y01 = *y0++;
			Y = T5_Y_Tables[y01];	
			*rgb0++ =  ARGB888_VALUE;
		}
	}
#endif	
}

void convert_yuv420_interleaved_to_argb2(unsigned char *yNonCached,
										 unsigned char *CbCrNonCached,
	int yLineSize, int uvLineSize, int width, int height, unsigned char *rgbaOut)
{
	int Y,U,V, i;
	int16_t r_add, g_add,b_add;
	uint8_t  *uPtr,*vPtr = NULL;
	uint8_t *y0,*y1;
	uint32_t *rgb0, *rgb1;
	uint32_t h,w, stride, yLine, uvLine,new_width,new_height;
	uint8_t y01,y10,y02,y12;
	unsigned char *yCached;
	unsigned char *CbCrCached;
	uint8_t *srcY, *srcCbCr, *dstY, *dstCbCr;
	unsigned int us;
	uint32_t r1, r2;

	int *crop = crop_shift + NEG_PADDING_SIZE;

	yLine = yLineSize;
	uvLine = uvLineSize;
	new_width = width>>1;
	new_height = height>>1;

	//fprintf(stderr, "Alloc: y: %d: uv: %d\n", yLineSize * height, uvLineSize * height / 2);
	yCached = (unsigned char *) malloc(yLine*2);
	CbCrCached = (unsigned char *) malloc(uvLine);

	struct timeval cts, cte;
	gettimeofday(&cts, NULL);
/*
	dstY = yCached;
	srcY = yNonCached;
	for (i = 0; i < yLineSize * height / 32; i++) {
		asm volatile (
					"vld1.8 {d0-d3}, [%[srcY]]\n\t"
					"vst1.8 {d0}, [%[dstY]]\n\t"
					: [dstY] "+r" (dstY)
					: [srcY]"r" (srcY)
					: "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7"
					);
		//dstY += 0;
		srcY += 32;
	}


	dstCbCr = CbCrCached;
	srcCbCr = CbCrNonCached;
	for (i = 0; i < uvLineSize * height / 64 / 2; i++) {
		asm volatile (
					"vld1.8 {d0-d3}, [%[srcCbCr]]!\n\t"
					"vst1.8 {d0}, [%[dstCbCr]]!\n\t"
					: [dstCbCr] "+r" (dstCbCr)
					: [srcCbCr]"r" (srcCbCr)
					: "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7"
					);
		dstCbCr += 0;
		srcCbCr += 32;
	}
*/

//	memcpy(yCached, yNonCached, yLineSize * height);
	//memcpy(CbCrCached, CbCrNonCached, uvLineSize * height/2);


	//y0 = yNonCached;
	//y1 = yNonCached + yLine;
	//y0 = y;
	//y1 = y + yLine;
	y0 = yCached;
	y1 = yCached + yLine;

	//uPtr = CbCr;
	//uPtr = CbCrNonCached;
	uPtr = CbCrCached;

	rgb0 = (uint32_t *)((void *)rgbaOut);
	rgb1 = rgb0 + width;

	stride = width;

	r1 = 0;
	register int tmp_v, tmp_u;
	for (h = new_height; h > 0; h--)
	{
		struct timeval ts, te;
		gettimeofday(&ts, NULL);
		memcpy(yCached, yNonCached + (new_height - h) * yLine*2, yLine*2);
		memcpy(CbCrCached, CbCrNonCached + (new_height - h) * uvLine, uvLine);
		gettimeofday(&te, NULL);
		us += (te.tv_sec - ts.tv_sec)*1e6 + (te.tv_usec - ts.tv_usec);


		y0 = yCached;
		y1 = yCached + yLine;
		uPtr = CbCrCached;

		for (w = new_width; w > 0; w--)
		{
			U = *uPtr++;
			V = *uPtr++;
			tmp_v = T3T1_V_Tables[V];
			tmp_u = T4T2_U_Tables[U];
			r_add = (int16_t) tmp_v;
			g_add = (int16_t) ((tmp_v >> 16) + (int16_t) tmp_u);
			b_add = (int16_t) (tmp_u >> 16);

			y01 = *y0++;
			y02 = *y0++;
			y10 = *y1++;
			y12 = *y1++;

			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
			//r1 += ARGB888_VALUE;

			Y = T5_Y_Tables[y02];
			*rgb0++ =  ARGB888_VALUE;
			//r1 += ARGB888_VALUE;

			Y = T5_Y_Tables[y10];
			*rgb1++ =  ARGB888_VALUE;
			//r1 += ARGB888_VALUE;

			Y = T5_Y_Tables[y12];
			*rgb1++ =  ARGB888_VALUE;
			//r1 += ARGB888_VALUE;
		}

		//#define ARGB888_VALUE (crop[Y + r_add] | crop[Y + g_add] | crop[Y + b_add])
		// if width is ODD then process the remaining pixel (Ex : width = 235, new_width = 117, 117*2 = 234, remaining pixels = 1
		/*
		if (width & 0x1)
		{
			U = *uPtr++;
			V = *uPtr++;
			r_add = (int16_t)(T3T1_V_Tables[V]);
			g_add = (int16_t) ((int16_t)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (int16_t)(T4T2_U_Tables[U]>>16);

			y01 = *y0++;
			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
			y10 = *y1++;
			Y = T5_Y_Tables[y10];
			*rgb1++ =  ARGB888_VALUE;
		}
		*/

		rgb0 += width;
		rgb1 += width;



		//y0 += (yLine+(yLine - width));
		//y1 += (yLine+(yLine - width));
		/*
		if (width & 0x1)  // width is ODD
		{
			uPtr += uvLine -(width+1);
		}
		else //// width is EVEN
		{
			uPtr += uvLine -width;
		}
		*/
	}

	free(yCached);
	free(CbCrCached);

	gettimeofday(&cte, NULL);
	FILE *out = fopen("/tmp/xv.log", "a");
	if (out) {
		fprintf(out, "memcpy2: %u ms\n", us/1000);
		fprintf(out, "convert2: %u ms, r = %u\n",
				(cte.tv_sec - cts.tv_sec) * 1000 +
				(cte.tv_usec - cts.tv_usec) / 1000 -
				us/1000,
				r1);
		fclose(out);
	}
}

/* Code taken from VLC, check license */
struct yuv_planes
{
	void *y, *u, *v;
	size_t pitch;
};

/* Packed picture buffer. Pitch is in bytes (_not_ pixels). */
struct yuv_pack
{
	void *yuv;
	size_t pitch;
};

void NV12_RGBA(unsigned char *y,
				unsigned char *CbCr,
				int yLineSize,
				int width,
				int height,
				unsigned char *rgbaOut)
{
	struct yuv_pack out = { rgbaOut, yLineSize*4 };
	struct yuv_planes in = { y, CbCr, CbCr, yLineSize };

	nv12_rgb_neon(&out, &in, width, height);
}

void NV21_RGBA(unsigned char *y,
				unsigned char *CbCr,
				int yLineSize,
				int width,
				int height,
				unsigned char *rgbaOut)
{
	struct yuv_pack out = { rgbaOut, yLineSize*4 };
	struct yuv_planes in = { y, CbCr, CbCr, yLineSize };

	nv21_rgb_neon(&out, &in, width, height);
}

void convert_yuv420_interleaved_to_argb_factor4(unsigned char *y, unsigned char *CbCr,
	int yLineSize, int uvLineSize, int width, int height, unsigned char *rgbaOut)
{
	int Y,U,V;	
	int16_t r_add, g_add,b_add;
	uint8_t  *uPtr,*vPtr; 
	uint8_t *y0,*y1;
	uint32_t *rgb0, *rgb1;
	uint32_t h,w, yLine, uvLine,new_width,new_height;
	uint8_t y01,y10,y02,y12;

	int *crop = crop_shift + NEG_PADDING_SIZE;

	yLine = yLineSize;
	uvLine = uvLineSize;
	new_width = width>>1;
	new_height = height>>1;

	y0 = y;
	y1 = y + yLine;

	uPtr = CbCr;

	rgb0 = (uint32_t *)((void *)rgbaOut);
	rgb1 = rgb0 + width;

	for (h=new_height; h>0; h--)
	{
		for(w=new_width; w>0; w-=4)
		{
			U = *uPtr++;
			V = *uPtr++;
			r_add = (int16_t)(T3T1_V_Tables[V]);
			g_add = (int16_t) ((int16_t)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (int16_t)(T4T2_U_Tables[U]>>16);

			y01 = *y0++; y02 = *y0++;
			y10 = *y1++; y12 = *y1++;
			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y02];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y10];
			*rgb1++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y12];
			*rgb1++ =  ARGB888_VALUE;

			U = *uPtr++;
			V = *uPtr++;
			r_add = (int16_t)(T3T1_V_Tables[V]);
			g_add = (int16_t) ((int16_t)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (int16_t)(T4T2_U_Tables[U]>>16);

			y01 = *y0++; y02 = *y0++;
			y10 = *y1++; y12 = *y1++;
			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y02];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y10];
			*rgb1++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y12];
			*rgb1++ =  ARGB888_VALUE;

			U = *uPtr++;
			V = *uPtr++;
			r_add = (int16_t)(T3T1_V_Tables[V]);
			g_add = (int16_t) ((int16_t)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (int16_t)(T4T2_U_Tables[U]>>16);

			y01 = *y0++; y02 = *y0++;
			y10 = *y1++; y12 = *y1++;
			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y02];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y10];
			*rgb1++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y12];
			*rgb1++ =  ARGB888_VALUE;

			U = *uPtr++;
			V = *uPtr++;
			r_add = (int16_t)(T3T1_V_Tables[V]);
			g_add = (int16_t) ((int16_t)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (int16_t)(T4T2_U_Tables[U]>>16);

			y01 = *y0++; y02 = *y0++;
			y10 = *y1++; y12 = *y1++;
			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y02];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y10];
			*rgb1++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y12];
			*rgb1++ =  ARGB888_VALUE;
		}

		rgb0 += width;
		rgb1 += width;
		y0 += (yLine+(yLine - width));
		y1 += (yLine+(yLine - width));
		uPtr += uvLine -width;
	}

	// if Height is ODD then process the remaining
	if (height & 0x1)
	{
		for(w=new_width; w > 0; w--)
		{
			U = *uPtr++;
			V = *uPtr++;
			r_add = (int16_t)(T3T1_V_Tables[V]);
			g_add = (int16_t) ((int16_t)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (int16_t)(T4T2_U_Tables[U]>>16);

			y01 = *y0++; y02 = *y0++;
			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y02];
			*rgb1++ =  ARGB888_VALUE;
		}
	}
}

void convert_yuv420_to_argb(guchar * y, guchar * u, guchar * v,
	guint yLineSize, guint uvLineSize, guint width, guint height, guchar * rgbaOut)
{
	gint Y,U,V;
	gint16 r_add, g_add,b_add;
	guint8  *uPtr,*vPtr; 
	guint8 *y0,*y1;
	guint *rgb0, *rgb1;
	guint h,w, yLine, uvLine,new_width,new_height;
	guint8 y01,y10,y02,y12;
	gint *crop = crop_shift + NEG_PADDING_SIZE;

	yLine = yLineSize;
	uvLine = uvLineSize;
	new_width = width>>1;
	new_height = height>>1;

	y0 = y;
	y1 = y + yLine;
	uPtr = u;
	vPtr = v;

	rgb0 = (guint *)(rgbaOut);
	rgb1 = rgb0 + width;

	for (h=new_height; h>0; h--)
	{
		for(w=new_width; w>0; w--)
		{
			U = *uPtr++;
			V = *vPtr++ ;
			r_add = (gint16)(T3T1_V_Tables[V]);
			g_add = (gint16) ((gint16)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (gint16)(T4T2_U_Tables[U]>>16);

			y01 = *y0++; y02 = *y0++;
			y10 = *y1++; y12 = *y1++;

			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y02];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y10];
			*rgb1++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y12];
			*rgb1++ =  ARGB888_VALUE;
		}

		// if width is ODD then process the remaining pixel (Ex : width = 235, new_width = 117, 117*2 = 234, remaining pixels = 1
		if (width & 0x1)
		{
			U = *uPtr++;
			V = *vPtr++ ;
			r_add = (int16_t)(T3T1_V_Tables[V]);
			g_add = (int16_t) ((int16_t)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (int16_t)(T4T2_U_Tables[U]>>16);

			y01 = *y0++;
			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
			y10 = *y1++;
			Y = T5_Y_Tables[y10];
			*rgb1++ =  ARGB888_VALUE;
		}

		rgb0 += width;
		rgb1 += width;
		y0 += (yLine+(yLine - width));
		y1 += (yLine+(yLine - width));	

		if (width & 0x1)  // width is ODD
		{
			uPtr += uvLine -(new_width+1);
			vPtr += uvLine -(new_width+1);
		}
		else //// width is EVEN
		{
			uPtr += uvLine -new_width;
			vPtr += uvLine -new_width;
		}
	}

	// if Height is ODD then process the remaining
	if (height & 0x1)
	{
		for(w=new_width; w > 0; w--)
		{
			U = *uPtr++;
			V = *vPtr++ ;
			r_add = (int16_t)(T3T1_V_Tables[V]);
			g_add = (int16_t) ((int16_t)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (int16_t)(T4T2_U_Tables[U]>>16);

			y01 = *y0++; y02 = *y0++;	
			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
			Y = T5_Y_Tables[y02];
			*rgb0++ =  ARGB888_VALUE;
		}

		if (width & 0x1)
		{
			U = *uPtr++;
			V = *vPtr++ ;
			r_add = (int16_t)(T3T1_V_Tables[V]);
			g_add = (int16_t) ((int16_t)T4T2_U_Tables[U] + (T3T1_V_Tables[V]>>16));
			b_add =  (int16_t)(T4T2_U_Tables[U]>>16);

			y01 = *y0++;
			Y = T5_Y_Tables[y01];
			*rgb0++ =  ARGB888_VALUE;
		}
	}
}

