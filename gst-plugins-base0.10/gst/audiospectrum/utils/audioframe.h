#ifndef _AUDIOFRAME_S_H_
#define _AUDIOFRAME_S_H_

#include <glib.h>

typedef struct
{
	guchar* data;		/** PCM data pointer */
	guint size;			/** Buffer size in bytes */

	guint num_ch;		/** number of channels */

	guint sample_freq;	/** sampling frequency */
	guint sample_size;	
	/** if sample size == 2, we consider that data sample is packed to short and it is 16 bit signed. 
			if sample size == 4, we consider that data sample is packed to int, but it is 24 bit signed NOT 32 bit integer */
			
	gboolean interleaved;			/** format of channel placement */
}audioframe_s;

#endif // _AUDIOFRAME_S_H_

