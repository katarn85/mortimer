/*
 * Copyright (C) 2007-2009 Nokia Corporation.
 *
 * Author: Felipe Contreras <felipe.contreras@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "gstomx_base_videodec.h"
#include "gstomx.h"
#include <math.h>
#include <stdio.h>
#include <dlfcn.h>

#include <audio-session-manager.h>
#ifdef TVS_USERDATA_PARSER
#include <tvs-userdata-parser/IUDProcessor.h>
#endif
#include <linux/videodev2.h>


// TODO: kiho,song
typedef enum
{
	CD_USER_DATA_TYPE_DIGITAL,	//< UserData Digital
	CD_USER_DATA_TYPE_ANALOG,	//< UserData Analog
}CDUserDataType_k;

#define LIB_NAME "libtvs-userdata-parser.so"

static guint SEIMETADATA_SIZE;
static guint SEI_MDCV_SIZE;
static guint COLOURDESCRIPTION_SIZE;

static void *dl_handle;
static void* (*_CreateUDMInstance) (CDUserDataType_k udmType, int isRVU);
static void (*_ResetUDMInstance) (void* udManagerInst);
static int (*_ProcessUserData) (void* udManagerInst,  struct v4l2_sdp_vbi_format *pCcData);
static unsigned short (*_GetRating) (void* udManagerInst);
#ifdef TVS_USERDATA_PARSER
static TTUDM (*_GetClosedCaption) (void* udManagerInst);
#endif

__attribute__((constructor)) void open_tvs_userdata_parser_lib()
{
  dl_handle = dlopen (LIB_NAME, RTLD_LAZY);
  if (!dl_handle) {
    return;
  }
#ifdef TVS_USERDATA_PARSER  
  _CreateUDMInstance = dlsym (dl_handle, "CreateUDMInstance");
  _ResetUDMInstance = dlsym (dl_handle, "ResetUDMInstance");
  _ProcessUserData = dlsym (dl_handle, "ProcessUserData");
  _GetRating = dlsym (dl_handle, "GetRating");
  _GetClosedCaption = dlsym (dl_handle, "GetClosedCaption");

  if (!_CreateUDMInstance || !_ResetUDMInstance || !_ProcessUserData || !_GetRating || !_GetClosedCaption) {
    dlclose (dl_handle);
    dl_handle = NULL;
  }
#endif
}

__attribute__((destructor)) void close_tvs_userdata_parser_lib()
{
  if (dl_handle) {
    dl_handle = dlclose (dl_handle);
    dl_handle = NULL;
  }
}

static GstStaticPadTemplate dtv_cc_srctempl = GST_STATIC_PAD_TEMPLATE ("dtv_cc_src",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("text/dtv-closed-caption")
    );

enum TYPE_3D
{
	TYPE_3D_NONE = 0,
	TYPE_3D_MVC_BASE,
	TYPE_3D_MVC_DEPENDENT,
	TYPE_3D_SVAF_2ES_LEFT,
	TYPE_3D_SVAF_2ES_RIGHT,
};

typedef enum
{
  LSB2MSB = 0,
  MSB2LSB = 1,
} ReadOrder;

typedef struct _FPAParser
{
  gint pos_bit;
  gint length_byte;
  unsigned char* data_;
  ReadOrder readOrder_;
  gint frame_packing_arrangement_id_;
  gint frame_packing_arrangement_cancel_flag_;
  gint frame_packing_arrangement_type_;
  gint content_interpretation_type_;
  GByteArray* bits;
} FPAParser;

GSTOMX_BOILERPLATE (GstOmxBaseVideoDec, gst_omx_base_videodec, GstOmxBaseFilter,
    GST_OMX_BASE_FILTER_TYPE);

GST_DEBUG_CATEGORY_EXTERN(GST_CAT_VDEC);

/*
 * declaration of private functions to handle FPA data
 */
static void read_n_bits_from_lsb_to_msb(FPAParser* parser, gint n);
static void read_n_bits_from_msb_to_lsb(FPAParser* parser, gint n);
static int make_binary_number_(FPAParser* parser);
int read_bit(FPAParser* parser, int n);
static void FPAParser_Init(FPAParser* parser);
static void FPAParser_Deinit(FPAParser* parser);
static void calculate_frame_packing_arrangement_id_(FPAParser* parser);
static gboolean FPAParser_Analyze(FPAParser* parser);

/*
 * declaration of private functions to handle data from omx
 */
static gboolean dtv_cc_src_event (GstPad * pad, GstEvent * event);
static gboolean acquire_dtv_cc_src_pad(GstOmxBaseFilter *omx_base);
static void push_dtv_cc_data(GstOmxBaseFilter *omx_base, OMX_VIDEO_PARAM_VBITYPE *cc_data);
static void push_org_userdata(GstOmxBaseFilter *omx_base, OMX_VIDEO_PARAM_VBITYPE *cc_data);
static void userdata_cb(GOmxCore * core, gconstpointer data);
static void seimetadata_cb(GOmxCore * core, OMX_EVENTTYPE event, OMX_U32 data_1, OMX_U32 data_2, OMX_PTR event_data);
static void colourdescription_cb(GOmxCore * core, gconstpointer data);

/*
* This is for MVC / SVAF2ES.
* Our decoders have a restriction that they can't accept different timestamp of each first packet.
* We can use static variable because MVC / SVAF2ES playback is not possible on device
*/
static GstClockTime left_3d_timestamp = GST_CLOCK_TIME_NONE;
static GstClockTime right_3d_timestamp = GST_CLOCK_TIME_NONE;
static gboolean left_3d_compared_done = FALSE;
static gboolean right_3d_compared_done = FALSE;


static GstClockTime out_left_3d_timestamp = GST_CLOCK_TIME_NONE;
static GstClockTime out_right_3d_timestamp = GST_CLOCK_TIME_NONE;
static gboolean out_left_3d_compared_done = FALSE;
static gboolean out_right_3d_compared_done = FALSE;

static GMutex right_done_mutex;
static GMutex left_done_mutex;
static GCond right_done_cond;
static GCond left_done_cond;

static GMutex right_sync_mutex;
static GMutex left_sync_mutex;
static GCond right_sync_cond;
static GCond left_sync_cond;

static GMutex right_in_done_mutex;
static GMutex left_in_done_mutex;
static GCond right_in_done_cond;
static GCond left_in_done_cond;

static GMutex right_in_sync_mutex;
static GMutex left_in_sync_mutex;
static GCond right_in_sync_cond;
static GCond left_in_sync_cond;

static gboolean *left_sync_got_eos = NULL;
static gboolean *right_sync_got_eos = NULL;

static gboolean left_thread_inited = FALSE;
static gboolean right_thread_inited = FALSE;

static GstClockTime timestamp_3d = GST_CLOCK_TIME_NONE;


static gboolean do_sync_timestamp_right(GstOmxBaseFilter * basefilter, GstClockTime* me, GstClockTime* you)
{
	static guint debug_count = 0;
	
	while(!GST_CLOCK_TIME_IS_VALID(*you))
	{
		if (basefilter->in_port->flush_started)
		{
			GST_INFO_OBJECT(basefilter, "flush start [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
			return FALSE;
		}
		
		if (basefilter->is_push_deactivate == TRUE)
		{	
			GST_ERROR_OBJECT(basefilter, "outloop push error, we should not do output timestamp sync");
			return FALSE;
		}
		if (basefilter->in_port->got_eos || (left_sync_got_eos && *left_sync_got_eos))
		{
			basefilter->timeout_count++;
			// To avoid the blocking of playback for the contents which has unmatched last data, start timeout.
			if (basefilter->timeout_count > 500)
			{
				GST_WARNING_OBJECT(basefilter, "End by timeout [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
				return FALSE;
			}
		}
		
		g_usleep(2000);
	}

	g_mutex_lock(&left_in_sync_mutex);
	while((*me != *you) && GST_CLOCK_TIME_IS_VALID(*you))
	{
		if (basefilter->in_port->flush_started)
		{
			g_mutex_unlock(&left_in_sync_mutex);
			GST_INFO_OBJECT(basefilter, "flush start [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
			return FALSE;
		}
		
		if (basefilter->is_push_deactivate == TRUE)
		{	
			g_mutex_unlock(&left_in_sync_mutex);
			GST_ERROR_OBJECT(basefilter, "outloop push error, we should not do output timestamp sync");
			return FALSE;
		}

		if ((*me < *you) && GST_CLOCK_TIME_IS_VALID(*you)) // must get next packet
		{
			g_mutex_unlock(&left_in_sync_mutex);
			return FALSE;
		}
		GST_DEBUG_OBJECT(basefilter,"dependent wait compare input dts sync Start");
		g_cond_wait(&left_in_sync_cond,&left_in_sync_mutex);
		GST_DEBUG_OBJECT(basefilter,"dependent wait compare input dts sync Stop");
	}
	if(!GST_CLOCK_TIME_IS_VALID(*you)){
		GST_INFO_OBJECT(basefilter, "flush stop but have not get the first valid dts [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
		g_mutex_unlock(&left_in_sync_mutex);
		return FALSE;
	}
	g_mutex_unlock(&left_in_sync_mutex);
	return TRUE;
}

static gboolean do_sync_timestamp_left(GstOmxBaseFilter * basefilter, GstClockTime* me, GstClockTime* you)
{
	static guint debug_count = 0;
	
	while(!GST_CLOCK_TIME_IS_VALID(*you))
	{
		if (basefilter->in_port->flush_started)
		{
			GST_INFO_OBJECT(basefilter, "flush start [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
			return FALSE;
		}
		
		if (basefilter->is_push_deactivate == TRUE)
		{	
			GST_ERROR_OBJECT(basefilter, "outloop push error, we should not do output timestamp sync");
			return FALSE;
		}
		if (basefilter->in_port->got_eos || (right_sync_got_eos && *right_sync_got_eos))
		{
			basefilter->timeout_count++;
			// To avoid the blocking of playback for the contents which has unmatched last data, start timeout.
			if (basefilter->timeout_count > 500)
			{
				GST_WARNING_OBJECT(basefilter, "End by timeout [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
				return FALSE;
			}
		}
		
		g_usleep(2000);
	}

	g_mutex_lock(&right_in_sync_mutex);
	while((*me != *you) && GST_CLOCK_TIME_IS_VALID(*you))
	{
		if (basefilter->in_port->flush_started)
		{
			g_mutex_unlock(&right_in_sync_mutex);
			GST_INFO_OBJECT(basefilter, "flush start [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
			return FALSE;
		}
		
		if (basefilter->is_push_deactivate == TRUE)
		{	
			g_mutex_unlock(&right_in_sync_mutex);
			GST_ERROR_OBJECT(basefilter, "outloop push error, we should not do output timestamp sync");
			return FALSE;
		}

		if ((*me < *you) && GST_CLOCK_TIME_IS_VALID(*you)) // must get next packet
		{
			g_mutex_unlock(&right_in_sync_mutex);
			return FALSE;
		}
		GST_DEBUG_OBJECT(basefilter,"main wait compare input dts sync Start");
		g_cond_wait(&right_in_sync_cond,&right_in_sync_mutex);			  
		GST_DEBUG_OBJECT(basefilter,"main wait compare input dts sync Stop");
	}

	if(!GST_CLOCK_TIME_IS_VALID(*you)){
		GST_INFO_OBJECT(basefilter, "flush stop but have not get the first valid dts [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
		g_mutex_unlock(&right_in_sync_mutex);
		return FALSE;
	}
	g_mutex_unlock(&right_in_sync_mutex);
	return TRUE;
}


static gboolean do_sync_timestamp(GstOmxBaseFilter * basefilter, GstClockTime* me, GstClockTime* you)
{
	static guint debug_count = 0;
	
	while(1)
	{
		if (basefilter->in_port->flush_started)
		{
			GST_INFO_OBJECT(basefilter, "flush start [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
			return FALSE;
		}

		if (basefilter->is_push_deactivate == TRUE)
		{	
			GST_ERROR_OBJECT(basefilter, "outloop push error, we should not do timestamp sync");
			return FALSE;
		}
			
		if ((debug_count++ % 500) == 0)
			GST_WARNING_OBJECT(basefilter, "Wait for the another decoder's preparation [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
		
		if (!GST_CLOCK_TIME_IS_VALID(*you)) // wait for the next packet of the another decoder.
		{
			g_usleep(2000);
			if (basefilter->in_port->got_eos)
			{
				basefilter->timeout_count++;
				// To avoid the blocking of playback for the contents which has unmatched last data, start timeout.
				if (basefilter->timeout_count > 500)
				{
					GST_WARNING_OBJECT(basefilter, "End by timeout [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
					return FALSE;
				}
			}
			continue;
		}

		if (*me < *you) // must get next packet
			return FALSE;
		else if (*me > *you) // wait for the next packet of the another decoder.
			g_usleep(2000);
		else if (*me == *you)
			return TRUE;
	}
}

static gboolean do_sync_output_timestamp_right(GstOmxBaseFilter * basefilter, GstClockTime* me, GstClockTime* you)
{	
	while(!GST_CLOCK_TIME_IS_VALID(*you))
	{
		if (basefilter->in_port->flush_started)
		{
			GST_INFO_OBJECT(basefilter, "flush start [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
			return FALSE;
		}
		
		if (basefilter->is_push_deactivate == TRUE)
		{	
			GST_ERROR_OBJECT(basefilter, "outloop push error, we should not do output timestamp sync");
			return FALSE;
		}
		
		if (basefilter->in_port->got_eos || (left_sync_got_eos && *left_sync_got_eos))
		{
			basefilter->timeout_count_output++;
			// To avoid the blocking of playback for the contents which has unmatched last data, start timeout.
			if (basefilter->timeout_count_output > 500)
			{
				GST_WARNING_OBJECT(basefilter, "do_sync_output_timestamp End by timeout [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
				return FALSE;
			}
		}
		g_usleep(2000);
	}

	g_mutex_lock(&left_sync_mutex);
	while((*me != *you) && GST_CLOCK_TIME_IS_VALID(*you))
	{
		if (basefilter->in_port->flush_started)
		{
			g_mutex_unlock(&left_sync_mutex);
			GST_INFO_OBJECT(basefilter, "flush start [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
			return FALSE;
		}
		
		if (basefilter->is_push_deactivate == TRUE)
		{	
			g_mutex_unlock(&left_sync_mutex);
			GST_ERROR_OBJECT(basefilter, "outloop push error, we should not do output timestamp sync");
			return FALSE;
		}

		if ((*me < *you)&& GST_CLOCK_TIME_IS_VALID(*you)) // must get next packet
		{
			g_mutex_unlock(&left_sync_mutex);
			return FALSE;
		}
		GST_DEBUG_OBJECT(basefilter,"dependent wait compare output pts sync Start");
		g_cond_wait(&left_sync_cond,&left_sync_mutex);		 
		GST_DEBUG_OBJECT(basefilter,"dependent wait compare output pts sync Start");
	}

	if(!GST_CLOCK_TIME_IS_VALID(*you)){
		GST_INFO_OBJECT(basefilter, "flush stop but have not get the first valid pts [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
		g_mutex_unlock(&left_sync_mutex);
		return FALSE;
	}
	
	g_mutex_unlock(&left_sync_mutex);
	return TRUE;
}

static gboolean do_sync_output_timestamp_left(GstOmxBaseFilter * basefilter, GstClockTime* me, GstClockTime* you)
{	
	while(!GST_CLOCK_TIME_IS_VALID(*you))
	{
		if (basefilter->in_port->flush_started)
		{
			GST_INFO_OBJECT(basefilter, "flush start [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
			return FALSE;
		}
		
		if (basefilter->is_push_deactivate == TRUE)
		{	
			GST_ERROR_OBJECT(basefilter, "outloop push error, we should not do output timestamp sync");
			return FALSE;
		}
		
		if (basefilter->in_port->got_eos || (right_sync_got_eos && *right_sync_got_eos))
		{
			basefilter->timeout_count_output++;
			// To avoid the blocking of playback for the contents which has unmatched last data, start timeout.
			if (basefilter->timeout_count_output > 500)
			{
				GST_WARNING_OBJECT(basefilter, "do_sync_output_timestamp End by timeout [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
				return FALSE;
			}
		}
		g_usleep(2000);
	}

	g_mutex_lock(&right_sync_mutex);
	while((*me != *you) && GST_CLOCK_TIME_IS_VALID(*you))
	{
		if (basefilter->in_port->flush_started)
		{
			g_mutex_unlock(&right_sync_mutex);
			GST_INFO_OBJECT(basefilter, "flush start [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
			return FALSE;
		}
		
		if (basefilter->is_push_deactivate == TRUE)
		{	
			g_mutex_unlock(&right_sync_mutex);
			GST_ERROR_OBJECT(basefilter, "outloop push error, we should not do output timestamp sync");
			return FALSE;
		}

		if ((*me < *you) && GST_CLOCK_TIME_IS_VALID(*you)) // must get next packet
		{
			g_mutex_unlock(&right_sync_mutex);
			return FALSE;
		}
		GST_DEBUG_OBJECT(basefilter,"main wait compare output pts sync Start");
		g_cond_wait(&right_sync_cond,&right_sync_mutex);		   
		GST_DEBUG_OBJECT(basefilter,"main wait compare output pts sync Stop");
	}
	if(!GST_CLOCK_TIME_IS_VALID(*you)){
		GST_INFO_OBJECT(basefilter, "flush stop but have not get the first valid pts [ me : %lld ms][ you : %lld ms]", GST_TIME_AS_MSECONDS(*me), GST_TIME_AS_MSECONDS(*you));
		g_mutex_unlock(&right_sync_mutex);
		return FALSE;
	}
	
	g_mutex_unlock(&right_sync_mutex);
	return TRUE;
}


static GstOmxReturn
process_video_input_buf (GstOmxBaseFilter * omx_base_filter, GstBuffer **buf)
{
  /* Now, this is used only for 3D 2ES */
  GstOmxBaseVideoDec *self = GST_OMX_BASE_VIDEODEC(omx_base_filter);
  int time_out_counter = 0;
  gint64 end_time = 0;
  GST_LOG_OBJECT (self, "TP_COMPARE[ %d ][ L: %lld ][ R: %lld ]  [ %lld ]", omx_base_filter->is_empty_inport, left_3d_timestamp, right_3d_timestamp, GST_BUFFER_TIMESTAMP(*buf) );

  omx_base_filter->iNumberOfnotVideoFrameDone++;
  GST_DEBUG_OBJECT(self, "OMX_V_OUTPUT, INPUT not output video frame[%d] !!!", omx_base_filter->iNumberOfnotVideoFrameDone);
  if(omx_base_filter->iNumberOfnotVideoFrameDone > OMX_NO_OUTPUT_FRAME_DONE){
	GST_ERROR_OBJECT(self, "OMX_V_OUTPUT, not output video frame > OMX_NO_OUTPUT_FRAME_DONE !!!");
	return GSTOMX_RETURN_ERROR;
  }
 
  GstStructure* dts_structure = gst_buffer_get_qdata(*buf, g_quark_from_string("packet_dts"));
  if (dts_structure)
  {
	  omx_base_filter->is_ffmpegdemux_pkg = TRUE;
	  // Is first input after flush ?
#if 0  // We can not skip any input video packets which may cause no decoded video frame output
	  if (omx_base_filter->is_empty_inport == FALSE && omx_base_filter->is_first_frame == TRUE)
	  {
	  	GST_LOG_OBJECT(self,"return GSTOMX_RETURN_OK ");
	  	return GSTOMX_RETURN_OK;
	  }
#endif
	 // Is 3D 2ES ?
	  if (self->type_3d == TYPE_3D_MVC_BASE || self->type_3d == TYPE_3D_SVAF_2ES_LEFT)
	  {
			GST_LOG_OBJECT(self,"get structure from buffer, it is ffdemux case");
			g_mutex_lock(&left_in_sync_mutex);
			if (gst_structure_get_clock_time(dts_structure, "packet_dts", &left_3d_timestamp))
			{
				GST_LOG_OBJECT(self,"get dts from qdata %lld",left_3d_timestamp);
			}
			else
			{
				GST_LOG_OBJECT(self,"can not get dts from qdata");
			}
			g_cond_signal (&left_in_sync_cond);
			g_mutex_unlock(&left_in_sync_mutex);
			
			if (do_sync_timestamp_left(omx_base_filter, &left_3d_timestamp, &right_3d_timestamp) == FALSE)
		    {	
				return GSTOMX_RETURN_SKIP;
			}
			else
			{
				g_mutex_lock(&left_in_done_mutex);
				timestamp_3d = GST_BUFFER_TIMESTAMP (*buf);
				GST_DEBUG_OBJECT(self,"Set timestamp_3d %lld", timestamp_3d);
				left_3d_compared_done = TRUE;	
				g_cond_signal(&left_in_done_cond);
				g_mutex_unlock(&left_in_done_mutex);
		
				g_mutex_lock(&right_in_done_mutex); 
				end_time = g_get_monotonic_time () + 1 * G_TIME_SPAN_SECOND;
				while (right_3d_compared_done != TRUE)    	
				{			
					if(!g_cond_wait_until(&right_in_done_cond,&right_in_done_mutex, end_time))			
					{				
						// timeout has passed.				
						g_mutex_unlock(&right_in_done_mutex);				
						GST_ERROR_OBJECT(self,"main wait compare done time out, break");
						return GSTOMX_RETURN_SKIP;;			
					}    
				}		
				right_3d_compared_done = FALSE;		
				g_mutex_unlock(&right_in_done_mutex);
			}
	  }
	  else if (self->type_3d == TYPE_3D_MVC_DEPENDENT || self->type_3d == TYPE_3D_SVAF_2ES_RIGHT)
	  {
			GST_LOG_OBJECT(self,"get structure from buffer, it is ffdemux case");
			g_mutex_lock(&right_in_sync_mutex);
			if (gst_structure_get_clock_time(dts_structure, "packet_dts", &right_3d_timestamp))
			{
				GST_LOG_OBJECT(self,"get dts from qdata %lld",right_3d_timestamp);
			}
			else
			{
				GST_LOG_OBJECT(self,"can not get dts from qdata");
			}
			g_cond_signal (&right_in_sync_cond);
			g_mutex_unlock(&right_in_sync_mutex);
	
			if (do_sync_timestamp_right(omx_base_filter, &right_3d_timestamp, &left_3d_timestamp) == FALSE)
		    {	
		      	return GSTOMX_RETURN_SKIP;
			}
			else
			{
				g_mutex_lock(&right_in_done_mutex);
				right_3d_compared_done = TRUE;	
				g_cond_signal(&right_in_done_cond);
				g_mutex_unlock(&right_in_done_mutex);
		
				g_mutex_lock(&left_in_done_mutex); 
				end_time = g_get_monotonic_time () + 1 * G_TIME_SPAN_SECOND;
				while (left_3d_compared_done != TRUE)    	
				{			
					if(!g_cond_wait_until(&left_in_done_cond,&left_in_done_mutex, end_time))			
					{				
						// timeout has passed.				
						g_mutex_unlock(&left_in_done_mutex);				
						GST_ERROR_OBJECT(self,"dependent wait compare done time out, break");
						return GSTOMX_RETURN_SKIP;;			
					}    
				}		
				GST_BUFFER_TIMESTAMP (*buf) = timestamp_3d;
				GST_DEBUG_OBJECT(self,"Get timestamp_3d %lld", timestamp_3d);
				left_3d_compared_done = FALSE;		
				g_mutex_unlock(&left_in_done_mutex);
			}
	  }
  }
  else
  {
	 omx_base_filter->is_ffmpegdemux_pkg = FALSE;
	  // Is first input after flush ?
	  if (omx_base_filter->is_empty_inport == FALSE)
	    return GSTOMX_RETURN_OK;
	  // Is 3D 2ES ?
	  if (self->type_3d == TYPE_3D_MVC_BASE || self->type_3d == TYPE_3D_SVAF_2ES_LEFT)
	  {
 	    GST_LOG_OBJECT(self,"can not get structure from buffer, it is netflix case");
	    left_3d_timestamp = GST_BUFFER_TIMESTAMP(*buf);
	    if (do_sync_timestamp(omx_base_filter, &left_3d_timestamp, &right_3d_timestamp) == FALSE)
	      return GSTOMX_RETURN_SKIP;
	  }
	  else if (self->type_3d == TYPE_3D_MVC_DEPENDENT || self->type_3d == TYPE_3D_SVAF_2ES_RIGHT)
	  {
	    GST_LOG_OBJECT(self,"can not get structure from buffer, it is netflix case");
	    right_3d_timestamp = GST_BUFFER_TIMESTAMP(*buf);
	    if (do_sync_timestamp(omx_base_filter, &right_3d_timestamp, &left_3d_timestamp) == FALSE)
	      return GSTOMX_RETURN_SKIP;
	  }
  }
  return GSTOMX_RETURN_OK;
}

inline static void avsync_debug_logging(GstOmxBaseFilter* omx_base_filter, GstBuffer **buf, OMX_BUFFERHEADERTYPE *omx_buffer, struct v4l2_drm *frame)
{
  if (frame && buf && omx_buffer->pBuffer && omx_base_filter->debugCategory
		&& GST_LEVEL_INFO <= gst_debug_category_get_threshold (omx_base_filter->debugCategory)
		&& GST_LEVEL_DEBUG >= gst_debug_category_get_threshold (omx_base_filter->debugCategory))
  {
	guint y_addr = (guint)frame->u.dec_info.pFrame[0].y_viraddr;
	guint y_linesize = (guint)frame->u.dec_info.pFrame[0].y_linesize;
	guint height = (guint)frame->u.dec_info.pFrame[0].height;
	guint width = (guint)frame->u.dec_info.pFrame[0].width;
	
	guint ptr = y_addr + ( y_linesize * height/2) + (width /2);
	int debug_info = ptr ? *(gchar*)ptr : 0;
	
	if (debug_info>0x50 && GST_LEVEL_INFO == gst_debug_category_get_threshold (omx_base_filter->debugCategory))
		  GST_CAT_INFO_OBJECT(omx_base_filter->debugCategory, omx_base_filter, "timestamp[ %"GST_TIME_FORMAT" ], size[ %d ], pixel[ %02x, %s]",
				GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(*buf)), GST_BUFFER_SIZE(*buf), debug_info, debug_info>0x50?"-sync-":" "); 
	else
		  GST_CAT_DEBUG_OBJECT(omx_base_filter->debugCategory, omx_base_filter, "timestamp[ %"GST_TIME_FORMAT" ], size[ %d ], pixel[ %02x, %s]",
				GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(*buf)), GST_BUFFER_SIZE(*buf), debug_info, debug_info>0x50?"-sync-":" ");	
  }
}


static GstOmxReturn
process_video_output_buf (GstOmxBaseFilter * omx_base_filter, GstBuffer **buf, OMX_BUFFERHEADERTYPE *omx_buffer)
{
  GST_LOG_OBJECT (omx_base_filter, "process_video_output_buf");

  //save qdata if there is changed value
  GstOmxBaseVideoDec *self = GST_OMX_BASE_VIDEODEC (omx_base_filter);
  int time_out_counter = 0;
  gint64 end_time = 0;

  if(omx_base_filter != NULL)
  {
  	omx_base_filter->iNumberOfnotVideoFrameDone = 0;
	GST_DEBUG_OBJECT(self, "OMX_V_OUTPUT, OUTPUT not output video frame[%d] !!!", omx_base_filter->iNumberOfnotVideoFrameDone);
  }
  else
  {
  	GST_ERROR_OBJECT(self, "omx_base_filter is NULL");
  	return GSTOMX_RETURN_ERROR;
  }

  GST_LOG_OBJECT (self, "TP_COMPARE_OUT [ L: %lld ][ R: %lld ]  [ %lld ]", out_left_3d_timestamp, out_right_3d_timestamp, GST_BUFFER_TIMESTAMP(*buf) );
  if (self->type_3d == TYPE_3D_MVC_BASE || self->type_3d == TYPE_3D_SVAF_2ES_LEFT)
  {
  	
	g_mutex_lock(&left_sync_mutex);
	out_left_3d_timestamp = GST_BUFFER_TIMESTAMP (*buf);
	g_cond_signal (&left_sync_cond);
	g_mutex_unlock(&left_sync_mutex);
	GST_LOG_OBJECT(self,"get left pts from buf %lld",out_left_3d_timestamp);

	if (do_sync_output_timestamp_left(omx_base_filter, &out_left_3d_timestamp, &out_right_3d_timestamp) == FALSE)
	{	
		return GSTOMX_RETURN_SKIP;
	}
	else
	{
		g_mutex_lock(&left_done_mutex);
		out_left_3d_compared_done = TRUE;	
		g_cond_signal(&left_done_cond);
		g_mutex_unlock(&left_done_mutex);
		
		g_mutex_lock(&right_done_mutex); 
		end_time = g_get_monotonic_time () + 1 * G_TIME_SPAN_SECOND;
		while (out_right_3d_compared_done != TRUE)    	
		{			
			if(!g_cond_wait_until(&right_done_cond,&right_done_mutex, end_time))			
			{				
				// timeout has passed.				
				g_mutex_unlock(&right_done_mutex);				
				GST_ERROR_OBJECT(self,"main wait compare output pts done time out, break");				
				return GSTOMX_RETURN_SKIP;;			
			}    
		}		
		out_right_3d_compared_done = FALSE;		
		g_mutex_unlock(&right_done_mutex);
	}
  }
  else if (self->type_3d == TYPE_3D_MVC_DEPENDENT || self->type_3d == TYPE_3D_SVAF_2ES_RIGHT)
  {
	g_mutex_lock(&right_sync_mutex);
	out_right_3d_timestamp = GST_BUFFER_TIMESTAMP (*buf);
	g_cond_signal(&right_sync_cond);
	g_mutex_unlock(&right_sync_mutex);
	GST_LOG_OBJECT(self,"get right pts from buf %lld",out_right_3d_timestamp);

	if (do_sync_output_timestamp_right(omx_base_filter, &out_right_3d_timestamp, &out_left_3d_timestamp) == FALSE)
	{	
		return GSTOMX_RETURN_SKIP;
	}
	else
	{
		g_mutex_lock(&right_done_mutex);
		out_right_3d_compared_done = TRUE;	
		g_cond_signal(&right_done_cond);
		g_mutex_unlock(&right_done_mutex);
		
		g_mutex_lock(&left_done_mutex); 
		end_time = g_get_monotonic_time () + 1 * G_TIME_SPAN_SECOND;
		while (out_left_3d_compared_done != TRUE)    	
		{			
			if(!g_cond_wait_until(&left_done_cond,&left_done_mutex,end_time))			
			{				
				// timeout has passed.				
				g_mutex_unlock(&left_done_mutex);				
				GST_ERROR_OBJECT(self,"dependent wait compare output pts done time out, break");				
				return GSTOMX_RETURN_SKIP;;			
			}    
		}		
		out_left_3d_compared_done = FALSE;	
		g_mutex_unlock(&left_done_mutex);
	}
  }
  //scantype
  GstStructure * scantype_structure = NULL;
  struct v4l2_drm *frame = NULL;
  frame = (struct v4l2_drm *)omx_buffer->pPlatformPrivate;
  
  guint scantype = frame->u.dec_info.scantype;
  if( scantype != NULL && (scantype != self->pre_scantype ))
  {
	  scantype_structure = gst_structure_new("scantype","scantype_value", G_TYPE_INT,  scantype ,NULL);
	  gst_structure_set (scantype_structure, "scantype", G_TYPE_INT, scantype, NULL);
	  gst_buffer_set_qdata(*buf, g_quark_from_static_string("scantype"), scantype_structure);
	  self->pre_scantype = scantype;
	  GST_LOG_OBJECT (omx_base_filter, "process_video_output_buf scantype = %d", scantype);
  }
  
  //realtime_bitrate
  GstStructure * realtime_bitrate_structure = NULL;
  OMX_PARAM_PORTDEFINITIONTYPE param;
  param.nPortIndex = omx_base_filter->in_port->port_index;
  OMX_GetParameter (omx_base_filter->gomx->omx_handle, OMX_IndexParamPortDefinition, &param);

  gint bitrate = param.format.video.nBitrate;

  if( bitrate != NULL && (bitrate != self->pre_bitrate ))
  {
	  realtime_bitrate_structure = gst_structure_new("realtime_bitrate","realtime_bitrate_value", G_TYPE_INT,  bitrate,NULL);
	  gst_structure_set (realtime_bitrate_structure, "realtime_bitrate", G_TYPE_INT, bitrate,NULL);
	  gst_buffer_set_qdata(*buf, g_quark_from_static_string("realtime_bitrate"), realtime_bitrate_structure);
	  self->pre_bitrate = bitrate;
	  GST_LOG_OBJECT (omx_base_filter, "process_video_output_buf realtime_bitrate = %d", param.format.video.nBitrate);
  }
  
  //seimetadata-size
  //seimetadata
	if(self->seimetadata_filled == TRUE)
	{
		guint64 pts = (((guint64)(self->seimetadata.pts)) << 6);  // driver use like this, usec >> 6
		if (GST_BUFFER_TIMESTAMP(*buf)/1000 > pts) // compare them based on usec.
		{
			GstStructure * seimetadata_size_structure = NULL;
			GstStructure * seimetadata_structure = NULL;
			
			seimetadata_size_structure = gst_structure_new("seimetadata-size","seimetadata-size_value", G_TYPE_INT,  self->seimetadata.size ,NULL);
			seimetadata_structure = gst_structure_new("seimetadata","seimetadata_value", G_TYPE_POINTER,  self->seimetadata.payload ,NULL);

			gst_structure_set (seimetadata_size_structure, "seimetadata-size", G_TYPE_INT, self->seimetadata.size, NULL);
			gst_structure_set (seimetadata_structure, "seimetadata", G_TYPE_POINTER, self->seimetadata.payload,NULL);

			gst_buffer_set_qdata(*buf, g_quark_from_static_string("seimetadata-size"), seimetadata_size_structure);	
			gst_buffer_set_qdata(*buf, g_quark_from_static_string("seimetadata"), seimetadata_structure);

			self->seimetadata_filled = FALSE;

			GST_WARNING_OBJECT (omx_base_filter, "send sei pts[ %lld us >= %lld us ]", GST_BUFFER_TIMESTAMP(*buf)/1000, pts);
		}			
	}

	if (self->sei_mdcv_data_filled == TRUE)
	{
		GstStructure * sei_mdcv_data_structure = gst_structure_new("sei-mdcv",
							"display_primaries_x0", G_TYPE_UINT, self->sei_mdcv_data.display_primaries_x[0],
							"display_primaries_x1", G_TYPE_UINT, self->sei_mdcv_data.display_primaries_x[1],
							"display_primaries_x2", G_TYPE_UINT, self->sei_mdcv_data.display_primaries_x[2],
							"display_primaries_y0", G_TYPE_UINT, self->sei_mdcv_data.display_primaries_y[0],
							"display_primaries_y1", G_TYPE_UINT, self->sei_mdcv_data.display_primaries_y[1],
							"display_primaries_y2", G_TYPE_UINT, self->sei_mdcv_data.display_primaries_y[2],
							"white_point_x", G_TYPE_UINT, self->sei_mdcv_data.white_point_x,
							"white_point_y", G_TYPE_UINT, self->sei_mdcv_data.white_point_y,
							"max_display_mastering_luminance", G_TYPE_UINT, self->sei_mdcv_data.max_display_mastering_luminance,
							"min_display_mastering_luminance", G_TYPE_UINT, self->sei_mdcv_data.min_display_mastering_luminance, NULL);
		gst_buffer_set_qdata(*buf, g_quark_from_static_string("sei-mdcv"), sei_mdcv_data_structure);
		GST_WARNING_OBJECT(omx_base_filter, "primaries_x[ %d %d %d ], y[ %d %d %d ], white_point[ %d, %d ], lum[ %d, %d ]", 
				self->sei_mdcv_data.display_primaries_x[0], self->sei_mdcv_data.display_primaries_x[1], self->sei_mdcv_data.display_primaries_x[2],
				self->sei_mdcv_data.display_primaries_y[0], self->sei_mdcv_data.display_primaries_y[1], self->sei_mdcv_data.display_primaries_y[2],
				self->sei_mdcv_data.white_point_x, self->sei_mdcv_data.white_point_y, self->sei_mdcv_data.max_display_mastering_luminance,self->sei_mdcv_data.min_display_mastering_luminance);
		self->sei_mdcv_data_filled = FALSE;
	}

  avsync_debug_logging(omx_base_filter, buf, omx_buffer, frame);
  return GSTOMX_RETURN_OK;
}

static gboolean
pad_event (GstPad * pad, GstEvent * event)
{
  GstOmxBaseVideoDec *self = GST_OMX_BASE_VIDEODEC (GST_OBJECT_PARENT (pad));
  GstOmxBaseFilter *omx_base = GST_OMX_BASE_FILTER (self);
  GST_INFO_OBJECT (self, "event: %s", GST_EVENT_TYPE_NAME (event));
  gboolean ret = TRUE;

  switch (GST_EVENT_TYPE (event))
  {
    case GST_EVENT_FLUSH_STOP: /* Now, this is used only for 3D 2ES, and can not be ignored by child */
      {
        if (self->type_3d == TYPE_3D_MVC_BASE || self->type_3d == TYPE_3D_SVAF_2ES_LEFT)
        {
          left_3d_timestamp = GST_CLOCK_TIME_NONE;
		  out_left_3d_timestamp = GST_CLOCK_TIME_NONE;
        }
        else if (self->type_3d == TYPE_3D_MVC_DEPENDENT || self->type_3d == TYPE_3D_SVAF_2ES_RIGHT)
        {
          right_3d_timestamp = GST_CLOCK_TIME_NONE;
		  out_right_3d_timestamp = GST_CLOCK_TIME_NONE;
        }
      }
      break;
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
      if (gst_event_has_name(event, "clipmode") == TRUE && omx_base->in_port)
      {        
        gboolean onoff = FALSE;
        if (gst_structure_get_boolean(gst_event_get_structure(event), "onoff", &onoff))
        {
          OMX_VIDEO_CONFIG_DECODING_TYPE config;
          G_OMX_INIT_PARAM (config);
          config.nVideoDecodingType = (onoff ? CLIP_MODE:0);
          OMX_ERRORTYPE ret = OMX_SetConfig (omx_base->gomx->omx_handle, OMX_IndexConfigVideoDecodingType, &config);
          if (ret != OMX_ErrorNone)
            GST_ERROR_OBJECT(self, "OMX_SetConfig return[ %d ]", ret);
          GST_ERROR_OBJECT(self, "clipmode[ %s ]", onoff ? "ON":"OFF");
        }
        else
          GST_ERROR_OBJECT(self, "can not find 'onoff' field");
        ret = FALSE;
        gst_event_unref(event);
      }
      else
        GST_ERROR_OBJECT(self, "GST_EVENT_CUSTOM_DOWNSTREAM_OOB, but no clipmode event or in_port[ %x ]", omx_base->in_port);
      break;
    case GST_EVENT_EOS:
      if (!(CLIP_MODE & self->decoding_type)
          && (omx_base->in_port->num_of_frames <= 1 && omx_base->out_port->num_of_frames == 0))
      {
        GST_ERROR_OBJECT(self, "No available input data[ in : %d , out : %d ], so, use forced EOS", omx_base->in_port->num_of_frames, omx_base->out_port->num_of_frames);
        omx_base->forced_eos = TRUE;
      }
      ret = TRUE;
      break;
    default:
      break;
  }

  if (ret == TRUE && self->pad_event)
  {
    // If the child class want to intercept this event, it will return FALSE. then base_filter will ignore next steps
    return self->pad_event(pad, event);
  }

  return ret;
}

static void
type_base_init (gpointer g_class)
{
//  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  //gst_element_class_add_static_pad_template (element_class, &dtv_cc_srctempl);
}

/* MODIFICATION: add state tuning property */
static void
set_property (GObject * obj,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOmxBaseVideoDec *self;
  GstOmxBaseFilter *basefilter;
  self = GST_OMX_BASE_VIDEODEC (obj);
  basefilter = GST_OMX_BASE_FILTER (obj);

  switch (prop_id) {
    /* STATE_TUNING */
    case ARG_USE_STATETUNING:
      self->omx_base.use_state_tuning = g_value_get_boolean(value);
      break;
	/* Decoding Type */
    case ARG_DECODING_TYPE:
      g_mutex_lock (basefilter->ready_lock); // Because the omx_state will be changed after omx_setup(). and decoding_type will be applied inside this omx_setup().
      if (basefilter->gomx !=NULL)
      {
           if (basefilter->gomx
	  	&& (basefilter->gomx->omx_state == OMX_StateLoaded || basefilter->gomx->omx_state == OMX_StateInvalid))
           {
             self->decoding_type = g_value_get_int(value);
             if ((CLIP_MODE & self->decoding_type) && (SEAMLESS_MODE & self->decoding_type))
             GST_WARNING_OBJECT(self, "CLIP & SEAMLESS cannot be choosed together, In this case, only CLIP will be applied");
           }
          else
             GST_ERROR_OBJECT(self, "Cannot set decoding type, gomx[ %p ]  omx_state[ 0x%x ]  last decoding_type[ 0x%x ]", basefilter->gomx, basefilter->gomx ? (-1):basefilter->gomx->omx_state, self->decoding_type);
      }
      else
        GST_ERROR_OBJECT(self,"basefilter->gomx is NULL");
      g_mutex_unlock (basefilter->ready_lock);
      break;
    case ARG_ERROR_CONCEALMENT:
      g_mutex_lock (basefilter->ready_lock); // Because the omx_state will be changed after omx_setup(). and decoding_type will be applied inside this omx_setup().
      if (basefilter->gomx !=NULL)
      {
           if (basefilter->gomx
                && (basefilter->gomx->omx_state == OMX_StateLoaded || basefilter->gomx->omx_state == OMX_StateInvalid))
           {
             self->error_concealment = g_value_get_boolean(value);
           }
           else
             GST_ERROR_OBJECT(self, "Cannot set error concealment, gomx[ %p ]  omx_state[ 0x%x ]", basefilter->gomx, basefilter->gomx ? (-1):basefilter->gomx->omx_state);
      }
      else
        GST_ERROR_OBJECT(self,"basefilter->gomx is NULL");
      g_mutex_unlock (basefilter->ready_lock);
      break;
    case ARG_USERDATA_MODE:
      self->userdata_extraction_mode = g_value_get_int(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static void
get_property (GObject * obj, guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOmxBaseVideoDec *self;

  self = GST_OMX_BASE_VIDEODEC (obj);

  switch (prop_id) {
    /* STATE_TUNING */
    case ARG_USE_STATETUNING:
      g_value_set_boolean(value, self->omx_base.use_state_tuning);
      break;
    /* Decoding Type */
    case ARG_DECODING_TYPE:
      g_value_set_int(value, self->decoding_type);
      break;
    case ARG_ERROR_CONCEALMENT:
      g_value_set_boolean(value, self->error_concealment);
      break;
    case ARG_USERDATA_MODE:
      g_value_set_int(value, self->userdata_extraction_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
  }
}

static gboolean
gstomx_base_videodec_query (GstOmxBaseFilter * omx_base, GstQuery * query)
{
  gboolean res = FALSE;

  GST_LOG_OBJECT(omx_base, "query received[%d]", GST_QUERY_TYPE (query));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_RESOURCE:
      if (g_str_equal("omx_videodec_1", GST_OBJECT_NAME(gst_element_get_factory(GST_ELEMENT_CAST(omx_base)))))
      {
        GST_INFO_OBJECT(omx_base, "RESOURCE QUERY - RESOURCE CATEGORY[ASM_RESOURCE_VIDEO_DECODER_SUB]");
        gst_query_add_resource(query, ASM_RESOURCE_VIDEO_DECODER_SUB);
      }
      else if (g_str_equal("omx_mpeg4dec_1", GST_OBJECT_NAME(gst_element_get_factory(GST_ELEMENT_CAST(omx_base)))))
      {
        GST_INFO_OBJECT(omx_base, "RESOURCE QUERY - RESOURCE CATEGORY[ASM_RESOURCE_VIDEO_DECODER_SUB]");
        gst_query_add_resource(query, ASM_RESOURCE_VIDEO_DECODER_SUB);
      }
      else if (g_str_equal("omx_mjpegdec", GST_OBJECT_NAME(gst_element_get_factory(GST_ELEMENT_CAST(omx_base))))
		|| g_str_equal("omx_uhd_mjpegdec", GST_OBJECT_NAME(gst_element_get_factory(GST_ELEMENT_CAST(omx_base)))))
	{
	  GST_INFO_OBJECT(omx_base, "RESOURCE QUERY - RESOURCE CATEGORY[ASM_RESOURCE_MJPEG_DECODER]");
	  gst_query_add_resource(query, ASM_RESOURCE_MJPEG_DECODER);
	}
      else 
      {
        GST_INFO_OBJECT(omx_base, "RESOURCE QUERY - RESOURCE CATEGORY[ASM_RESOURCE_VIDEO_DECODER]");
        gst_query_add_resource(query, ASM_RESOURCE_VIDEO_DECODER);
      }
      res = TRUE;
      break;
    default:
      res = GST_ELEMENT_GET_CLASS (omx_base)->query (GST_ELEMENT_CAST(omx_base), query);
      break;
  }
  return res;
}

static void finalize(GObject *object)
{
  GstOmxBaseVideoDec *self = GST_OMX_BASE_VIDEODEC(object);
  GstOmxBaseFilter *omx_base = GST_OMX_BASE_FILTER (self);
  if (self->type_3d == TYPE_3D_MVC_BASE || self->type_3d == TYPE_3D_SVAF_2ES_LEFT)
  {
    left_3d_timestamp = GST_CLOCK_TIME_NONE;
	out_left_3d_timestamp = GST_CLOCK_TIME_NONE;
	if(left_thread_inited == TRUE){
		left_thread_inited = FALSE;
		left_sync_got_eos = NULL;
		g_mutex_clear(&left_done_mutex);
		g_cond_clear(&left_done_cond);

		g_mutex_clear(&left_sync_mutex);
		g_cond_clear(&left_sync_cond);

		g_mutex_clear(&left_in_done_mutex);
		g_cond_clear(&left_in_done_cond);

		g_mutex_clear(&left_in_sync_mutex);
		g_cond_clear(&left_in_sync_cond);

		GST_DEBUG_OBJECT(self, "SET left_thread_inited = FALSE in finalize");
	}
  }
  else if (self->type_3d == TYPE_3D_MVC_DEPENDENT || self->type_3d == TYPE_3D_SVAF_2ES_RIGHT)
  {
    right_3d_timestamp = GST_CLOCK_TIME_NONE;
	out_right_3d_timestamp = GST_CLOCK_TIME_NONE;
	if(right_thread_inited == TRUE){
		right_thread_inited = FALSE;
		right_sync_got_eos = NULL;
		g_mutex_clear(&right_done_mutex);
		g_cond_clear(&right_done_cond);

		g_mutex_clear(&right_sync_mutex);
		g_cond_clear(&right_sync_cond);

		g_mutex_clear(&right_in_done_mutex);
		g_cond_clear(&right_in_done_cond);

		g_mutex_clear(&right_in_sync_mutex);
		g_cond_clear(&right_in_sync_cond);
		
		GST_DEBUG_OBJECT(self, "SET right_thread_inited = FALSE in finalize");
	}
  }
  omx_base->sync_mutex = NULL;
  omx_base->sync_cond = NULL;

  omx_base->in_sync_mutex = NULL;
  omx_base->in_sync_cond = NULL;
  
  omx_base->sync_is_init = NULL;
  GST_DEBUG_OBJECT(self, "SET omx_base->sync_is_init = NULL in finalize");
}

static void
type_class_init (gpointer g_class, gpointer class_data)
{
  GObjectClass *gobject_class;
  GstOmxBaseFilterClass *basefilter_class;
  GstElementClass *base_class;

  gobject_class = G_OBJECT_CLASS (g_class);
  basefilter_class = GST_OMX_BASE_FILTER_CLASS (g_class);
  base_class = (GstElementClass*)basefilter_class;
  base_class->query = GST_DEBUG_FUNCPTR (gstomx_base_videodec_query);

  /* Properties stuff */
  {
    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;

    /* STATE_TUNING */
    g_object_class_install_property (gobject_class, ARG_USE_STATETUNING,
        g_param_spec_boolean ("state-tuning", "start omx component in gst paused state",
        "Whether or not to use state-tuning feature",
        FALSE, G_PARAM_READWRITE));
    /* DECODING_TYPE, to be selected only during NULL/ready state */
    g_object_class_install_property (gobject_class, ARG_DECODING_TYPE,
        g_param_spec_int ("decoding-type", "Decoding Type",
        "To select decoding type.\n\
                        Available only on NULL,READY STATE.\n\
                        0x01 : Clip(only for I,P frame)\n\
                        0x02 : Seamless", 0, 2, 0,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    /* ERROR_CONCEALMENT */
    g_object_class_install_property (gobject_class, ARG_ERROR_CONCEALMENT,
        g_param_spec_boolean ("error-concealment", "error concealment",
        "Error concealment for screen mirroring UDP",
        FALSE, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, ARG_USERDATA_MODE,
        g_param_spec_int ("userdata-mode", "set userdata extraction mode",
        "0: pure userdata(v4l2_sdp_vbi_format), 1:temporary usecase", 0, 2, 1,
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  }
  basefilter_class->process_input_buf = process_video_input_buf;
  basefilter_class->process_output_buf = process_video_output_buf;
  basefilter_class->finalize = finalize;
}

static void
settings_changed_cb (GOmxCore * core)
{
  GstOmxBaseFilter *omx_base;
  GstOmxBaseVideoDec *self;
  guint width;
  guint height;
  guint32 format = 0;

  omx_base = core->object;
  self = GST_OMX_BASE_VIDEODEC (omx_base);

  OMX_PARAM_PORTDEFINITIONTYPE param;
  G_OMX_INIT_PARAM (param);
  param.nPortIndex = omx_base->out_port->port_index;
  OMX_GetParameter (omx_base->gomx->omx_handle, OMX_IndexParamPortDefinition,
      &param);
  
  width = param.format.video.nFrameWidth;
  height = param.format.video.nFrameHeight;
  
  GST_LOG_OBJECT (omx_base, "settings changed: fourcc =0x%x, w[ %d ] h[ %d ] fps[ %d (Q16), %d ]", (guint)param.format.video.eColorFormat,
      param.format.video.nFrameWidth, param.format.video.nFrameHeight, param.format.video.xFramerate, param.format.video.xFramerate/(1<<16) );
  switch ((guint)param.format.video.eColorFormat) {
    case OMX_COLOR_FormatYUV420Planar:
    case OMX_COLOR_FormatYUV420PackedPlanar:
      format = GST_MAKE_FOURCC ('I', '4', '2', '0');
      break;
    case OMX_COLOR_FormatYCbYCr:
      format = GST_MAKE_FOURCC ('Y', 'U', 'Y', '2');
      break;
    case OMX_COLOR_FormatCbYCrY:
      format = GST_MAKE_FOURCC ('U', 'Y', 'V', 'Y');
      break;
    /* MODIFICATION: Add extended_color_format */
    case OMX_EXT_COLOR_FormatNV12TFdValue:
    case OMX_EXT_COLOR_FormatNV12TPhysicalAddress:
      format = GST_MAKE_FOURCC ('S', 'T', '1', '2');
      break;
    case OMX_EXT_COLOR_FormatNV12LPhysicalAddress:
      format = GST_MAKE_FOURCC ('S', 'N', '1', '2');
      break;
    case OMX_COLOR_FormatYUV420SemiPlanar:
      format = GST_MAKE_FOURCC ('N', 'V', '1', '2');
      break;
    default:
      break;
  }

  /*Enable Video Frame HW format Setting by SRC-Nanjing zlong.wang,  2014-01-28*/
  {
    GstCaps *src_peer_caps = NULL;
    gchar *src_peer_caps_str = NULL;

    if(format == GST_MAKE_FOURCC('I', '4', '2', '0')) {
      if ((src_peer_caps = gst_pad_peer_get_caps(omx_base->srcpad))
        && (src_peer_caps_str = gst_caps_to_string(src_peer_caps))
        && (g_strrstr(src_peer_caps_str, "STV0")) || omx_base->playscene) {
        GST_LOG_OBJECT (omx_base, "downstream[ %s:%s ] has caps 'STV0', so we assert it is 'xvimagesink'!", GST_DEBUG_PAD_NAME(GST_PAD_PEER(omx_base->srcpad)));
        format = GST_MAKE_FOURCC('S', 'T', 'V', '0');
      } else {
        GST_LOG_OBJECT (omx_base, "downstream[ %s:%s ] has no caps 'STV0', so we assert it is not 'xvimagesink'!", GST_DEBUG_PAD_NAME(GST_PAD_PEER(omx_base->srcpad)));
        format = GST_MAKE_FOURCC('N', 'V', '1', '2');
      }
      GST_LOG_OBJECT (omx_base, "downstream srcpad: %" GST_PTR_FORMAT, src_peer_caps);
      if (src_peer_caps)
        gst_caps_unref (src_peer_caps);
      if (src_peer_caps_str)
        g_free (src_peer_caps_str);
    }
    if(src_peer_caps){
    	 omx_base->pre_format = format;
	 GST_LOG_OBJECT (omx_base, "Set the setting format : %u ", omx_base->pre_format);
    }else {
	format = omx_base->pre_format;
	GST_LOG_OBJECT (omx_base, "The downstream element already unlinked, Set the setting format : %u ", format);
    }
  }
  
  /* UPDATE CAPS OF SRC PAD*/
  {
    GstCaps *new_src_caps = NULL;
    GstCaps *sinkcaps = NULL;
    GstStructure *new_structure = NULL;
    GstStructure *sink_structure = NULL;
    gint ARdegree = 0;

    new_structure = gst_structure_new ("video/x-raw-yuv",
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height, "format", GST_TYPE_FOURCC, format, NULL);

    // TODO: Update FrameRate (xFrameRate from omx dec)

    if (param.format.video.xFramerate > 0) // USE fps from hw decoder
    {
      gst_structure_set (new_structure, "framerate", GST_TYPE_FRACTION, param.format.video.xFramerate, (1<<16), NULL);
    }
    else if (self->framerate_denom != 0) // use fps from demuxer
    {
      gst_structure_set (new_structure, "framerate", GST_TYPE_FRACTION, self->framerate_num, self->framerate_denom, NULL);
    }
    else
      /* FIXME this is a workaround for xvimagesink */
      gst_structure_set (new_structure, "framerate", GST_TYPE_FRACTION, 0, 1, NULL);

    /* SET 3D information (check sink caps first) */
    sinkcaps = GST_PAD_CAPS( omx_base->sinkpad );
    if (!sinkcaps)
    {
      GST_ERROR_OBJECT(omx_base, "No Sinkcaps, why? not linked? ");
      goto FINISH;
    }

    GST_DEBUG_OBJECT (omx_base, "sinkcaps are: %" GST_PTR_FORMAT, sinkcaps);
    // Try to set auto detected 3d info if exists.
    if (self->need_set_fpa_3dtype == 3/*SideBySide, SVAF*/)
    {
      gst_structure_set(new_structure, "3Dformat", G_TYPE_STRING, "SVAF", "3Dmode", G_TYPE_STRING, "SideBySide", NULL);
    }
    else if (self->need_set_fpa_3dtype == 4/*TopBottom, SVAF*/)
    {
      gst_structure_set(new_structure, "3Dformat", G_TYPE_STRING, "SVAF", "3Dmode", G_TYPE_STRING, "TopAndBottom", NULL);
    }

    sink_structure = gst_caps_get_structure( sinkcaps, 0 );
    if (!sink_structure)
    {
      GST_ERROR_OBJECT(omx_base, "No structure in sinkcaps?? %"GST_PTR_FORMAT,  sinkcaps);
      goto FINISH;
    }

    // Overwrite if there is given informaition (on sinkpad).
    const gchar* str3Dformat = gst_structure_get_string(sink_structure, "3Dformat");
    const gchar* str3Dmode = gst_structure_get_string(sink_structure, "3Dmode");
    if (str3Dformat && str3Dmode)
    {
      gst_structure_set (new_structure, "3Dformat", G_TYPE_STRING, str3Dformat, "3Dmode", G_TYPE_STRING, str3Dmode, NULL);
    }

	if(self->color_description_filled == TRUE)
	{
		gst_structure_set(new_structure,
			"iColorPrimaries", G_TYPE_INT, self->color_description.colour_primaries, 
			"iTransferCharacteristics", G_TYPE_INT, self->color_description.transfer_characteristics,
			"iMatrixCoefficients", G_TYPE_INT,self->color_description.matrix_coeffs, NULL);

		self->color_description_filled = FALSE;
	}

    /* For auto change degree from mov container, 2014-06-13 */
    if (gst_structure_get_int (sink_structure, "ARdegree", &ARdegree))
      GST_LOG_OBJECT(omx_base, "Get MOV ARdegree  %d", ARdegree);
    gst_structure_set (new_structure, "ARdegree", G_TYPE_INT, ARdegree, NULL);
    GST_LOG_OBJECT(omx_base, "Set MOV ARdegree  %d", ARdegree);
    /* For judgment video codec format rotation,
    CANRotate 0: not support, 1: support, 2014-09-02 */
    const gchar* sink_mimetype = gst_structure_get_name(sink_structure);
    if (g_strrstr(sink_mimetype, "video/x-jpeg")){
      gst_structure_set (new_structure, "CANRotate", G_TYPE_INT, 0, NULL);
      GST_LOG_OBJECT(omx_base, "Set CANRotate  %d", 0);
    }else {
      gst_structure_set (new_structure, "CANRotate", G_TYPE_INT, 1, NULL);
      GST_LOG_OBJECT(omx_base, "Set CANRotate  %d", 1);
    }

    /* Set mimetype of input data to srccaps for sink element , sink element will use this value for videoquality setting */
    gst_structure_set (new_structure, "prevmimetype", G_TYPE_STRING, sink_mimetype, NULL);
    gint version = 0;
    if (sink_mimetype && g_str_equal(sink_mimetype, "video/mpeg"))
    {
      gst_structure_get_int(sink_structure, "mpegversion", &version);
      gst_structure_set (new_structure, "codec_version", G_TYPE_INT, version, NULL);
    }
    else if (sink_mimetype && g_str_equal(sink_mimetype, "video/wmv"))
    {
      gst_structure_get_int(sink_structure, "wmvversion", &version);
      gst_structure_set (new_structure, "codec_version", G_TYPE_INT, version, NULL);
    }

    /* Set max w/h for  seamless resolution-change scenario.
   *   In this case, we don't use maxW/H which from ffdemuxer. because, sometimes  ffdemuxer can't know the real maxw/h,
   *   So, this omx element will set its own maxW/H to srccaps */
    GstPadTemplate *tempPad = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(omx_base), "sink");
    GstCaps* caps = gst_pad_template_get_caps(tempPad);
    const GValue* width_range = gst_structure_get_value(gst_caps_get_structure( caps, 0 ), "maxwidth");
    const GValue* height_range = gst_structure_get_value(gst_caps_get_structure( caps, 0 ), "maxheight");
    gint maxwidth = gst_value_get_int_range_max(width_range);
    gint maxheight = gst_value_get_int_range_max(height_range);

    if (maxheight == 1088) maxheight = 1080; // This is for output size.  1088 is max size of input on FHD model.
    if (maxwidth == 4096) maxwidth = 3840; // This is for output size.  4096 is max size of input on UHD model.
    
    gst_structure_set (new_structure, "maxwidth", G_TYPE_INT, maxwidth, "maxheight", G_TYPE_INT, maxheight, NULL);

FINISH:

    new_src_caps = gst_caps_new_empty ();
    gst_caps_append_structure (new_src_caps, new_structure);
    GST_INFO_OBJECT (omx_base, "new_src_caps are: %" GST_PTR_FORMAT, new_src_caps);
    gst_pad_set_caps (omx_base->srcpad, new_src_caps);
    gst_caps_unref (new_src_caps); /* Modification: unref caps */
  }
}

static gboolean
sink_setcaps (GstPad * pad, GstCaps * caps)
{
  GstStructure *structure;
  GstOmxBaseVideoDec *self;
  GstOmxBaseFilter *omx_base;
  OMX_PARAM_PORTDEFINITIONTYPE param;


  self = GST_OMX_BASE_VIDEODEC (GST_PAD_PARENT (pad));
  omx_base = GST_OMX_BASE_FILTER (self);

  GST_INFO_OBJECT (self, "%" GST_PTR_FORMAT, caps);

  g_return_val_if_fail (gst_caps_get_size (caps) == 1, FALSE);

  structure = gst_caps_get_structure (caps, 0);

  {
    const GValue *framerate = NULL;
    framerate = gst_structure_get_value (structure, "r_framerate");
    if (!framerate) {
      framerate = gst_structure_get_value (structure, "framerate");
    }

    if (framerate) {
      self->framerate_num = gst_value_get_fraction_numerator (framerate);
      self->framerate_denom = gst_value_get_fraction_denominator (framerate);
    }
    else {
      GST_ERROR_OBJECT(self, "NO FRAMERATE!!");
    }
  }

  //judge the video format, only support YUV420 (DF140614-00422), 2014-06-18.
  {
	guint32 fourcc = 0;
	if(structure){
		if(gst_structure_get_fourcc (structure, "fourccformat", &fourcc)){
			if(fourcc != GST_MAKE_FOURCC ('I', '4', '2', '0')){
				GST_WARNING_OBJECT(self, "This video format is fourcc %d , I420 %d ", fourcc, GST_MAKE_FOURCC ('I', '4', '2', '0'));
				return FALSE;
			}
		}
	}
  }

  G_OMX_INIT_PARAM (param);

  {
    const GValue *codec_data = NULL;
    GstBuffer *buffer = NULL;
    codec_data = gst_structure_get_value (structure, "codec_data");
    if (codec_data) {
      buffer = gst_value_get_buffer (codec_data);
      omx_base->codec_data = buffer;
      gst_buffer_ref (buffer);
    }
  }

  return gst_pad_set_caps (pad, caps);
}

static void auto_detect_codingtype(GstStructure* structure, OMX_VIDEO_CODINGTYPE* compression_format)
{
	const gchar* name = gst_structure_get_name(structure);
	g_return_if_fail(name != NULL);
	g_return_if_fail(compression_format != NULL);
	if (g_str_equal(name, "video/x-vp8") || g_str_equal(name, "video/x-vp8_tz"))
		*compression_format = OMX_VIDEO_CodingVP8;
	else if (g_str_equal(name, "video/x-vp9") || g_str_equal(name, "video/x-vp9_tz"))
		*compression_format = OMX_VIDEO_CodingVP9;
	else if (g_str_equal(name, "video/mpeg") || g_str_equal(name,"video/mpeg_tz"))
	{
		gint mpegversion = 0;
		if (gst_structure_get_int(structure, "mpegversion", &mpegversion))
		{
			if (mpegversion == 1 || mpegversion == 2)
				*compression_format = OMX_VIDEO_CodingMPEG2;
			else if (mpegversion == 4)
				*compression_format = OMX_VIDEO_CodingMPEG4;
		}
	}
	else if (g_str_equal(name, "video/x-h264") || g_str_equal(name, "video/x-h264_tz"))
		*compression_format = OMX_VIDEO_CodingAVC;
	else if (g_str_equal(name, "video/x-h265") || g_str_equal(name, "video/x-h265_tz"))
		*compression_format = OMX_VIDEO_CodingHEVC;
	else if (g_str_equal(name, "video/x-h263") || g_str_equal(name, "video/x-h263_tz"))
		*compression_format = OMX_VIDEO_CodingH263;
	else if (g_str_equal(name, "video/x-wmv") || g_str_equal(name, "video/x-wmv_tz"))
		*compression_format = OMX_VIDEO_CodingWMV;
	else if (g_str_equal(name, "video/x-pn-realvideo") || g_str_equal(name, "video/x-pn-realvideo_tz"))
		*compression_format = OMX_VIDEO_CodingRV;
	else if (g_str_equal(name, "video/x-avs") || g_str_equal(name, "video/x-avs_tz"))
		*compression_format = OMX_VIDEO_CodingAVS;
	else if (g_str_equal(name, "video/x-avs+") || g_str_equal(name,"video/x-avs+_tz"))
		*compression_format = OMX_VIDEO_CodingAVSP;
	else if (g_str_equal(name, "video/x-jpeg") || g_str_equal(name,"video/x-jpeg_tz"))
		*compression_format = OMX_VIDEO_CodingMJPEG;
	else if (g_str_equal(name, "video/x-h265") || g_str_equal(name, "video/x-h265_tz"))
		*compression_format = OMX_VIDEO_CodingHEVC;
	else if (g_str_equal(name, "video/x-msmpeg") || g_str_equal(name, "video/x-divx")
		|| g_str_equal(name, "video/x-xvid") || g_str_equal(name, "video/x-3ivx")
		|| g_str_equal(name, "video/x-msmpeg_tz") || g_str_equal(name, "video/x-divx_tz")
		|| g_str_equal(name, "video/x-xvid_tz") || g_str_equal(name, "video/x-3ivx_tz"))
		*compression_format = OMX_VIDEO_CodingMPEG4;
	else
		GST_ERROR("name[ %s ]", name);
	return;
}

static void
omx_setup (GstOmxBaseFilter * omx_base)
{
  GstOmxBaseVideoDec *self;
  GOmxCore *gomx;
  gint width = 0;
  gint height = 0;
  if (!omx_base) {
    GST_ERROR("omx_base is NULL");
    return;
  }

  self = GST_OMX_BASE_VIDEODEC (omx_base);
  gomx = (GOmxCore *) omx_base->gomx;

  GST_INFO_OBJECT (omx_base, "begin");
  if (omx_base->sinkpad)
  {
    GstCaps* caps = NULL;
    GstStructure *structure = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE param;
    G_OMX_INIT_PARAM (param);
    caps = GST_PAD_CAPS(omx_base->sinkpad);
    if (caps)
    {
      GST_LOG_OBJECT (self, "omx_setup (sink): %" GST_PTR_FORMAT, caps);
      structure = gst_caps_get_structure (caps, 0);
      if (structure)
      {
        guint32 codec_videotag = 0;
        const gchar* str3Dformat = NULL;
        gboolean security_enabled = OMX_FALSE;
        gboolean preset_enabled = OMX_FALSE;
        OMX_VIDEO_PARAM_DECODERINPUTTYPE decoderInputType;
        memset(&decoderInputType, 0, sizeof(OMX_VIDEO_PARAM_DECODERINPUTTYPE));

        if (self->compression_format == OMX_VIDEO_CodingAutoDetect)
        {
          auto_detect_codingtype(structure, &self->compression_format);
        }

        /*playscene for widevine-classic*/
        if(gst_structure_get_int(structure,"PlayScene",&omx_base->playscene))
        {
            GST_DEBUG_OBJECT(self,"omx_base->playscene %d",omx_base->playscene);
        }

        /* extradata (codec_data) */
        if ((omx_base->codec_data) && (omx_base->codec_data->data) && (omx_base->codec_data->size>0))
        {
          GST_DEBUG_OBJECT (self, "codec_extradata[%p, size:%d]", omx_base->codec_data->data, omx_base->codec_data->size);
          decoderInputType.pCodecExtraData = (OMX_U8*)omx_base->codec_data->data;
          decoderInputType.nCodecExtraDataSize = (OMX_U32)omx_base->codec_data->size;
        }
        else
        {
          GST_WARNING_OBJECT(self, "omx_base->codec_data[ %p ]", omx_base->codec_data);
        }

        /* format (codec_tag) */ 
        if (gst_structure_get_fourcc(structure, "format", &codec_videotag))
        {
          decoderInputType.videoCodecTag = (OMX_U32)codec_videotag;
          GST_DEBUG_OBJECT(omx_base, "video codec tag[ %x, %x ]", codec_videotag, decoderInputType.videoCodecTag);
        }
        else
        {
          GST_WARNING_OBJECT(omx_base, "video codec tag[ %x, %x ]", codec_videotag, decoderInputType.videoCodecTag);
        }

        if ((str3Dformat = gst_structure_get_string(structure, "3Dformat")) != NULL)
        {
          /* This is only for 2ES 3D format */
          if (g_str_has_prefix(str3Dformat, "MVC_"))
          {
            self->compression_format = OMX_VIDEO_CodingMVC;
            if (g_str_has_suffix(str3Dformat, "_base"))
            {
              left_3d_timestamp = GST_CLOCK_TIME_NONE;
			  out_left_3d_timestamp = GST_CLOCK_TIME_NONE;

			  left_sync_got_eos = &(omx_base->in_port->got_eos);
			  	
			  g_mutex_init(&left_done_mutex);
			  g_cond_init (&left_done_cond);
			  
			  g_mutex_init(&left_sync_mutex);
			  g_cond_init (&left_sync_cond);

			  g_mutex_init(&left_in_done_mutex);
			  g_cond_init (&left_in_done_cond);
			  
			  g_mutex_init(&left_in_sync_mutex);
			  g_cond_init (&left_in_sync_cond);

			  left_thread_inited = TRUE;

			  omx_base->sync_mutex = &right_sync_mutex;
			  omx_base->sync_cond = &right_sync_cond;

			  omx_base->in_sync_mutex = &right_in_sync_mutex;
			  omx_base->in_sync_cond = &right_in_sync_cond;
			  
			  omx_base->sync_is_init = &right_thread_inited;

			  GST_DEBUG_OBJECT(self, "SET left_thread_inited = TRUE in omx_setup");
              self->type_3d = TYPE_3D_MVC_BASE;
              decoderInputType.nStreoScopicType = OMX_Video_StereoScopic3D_MVC_B;
            }
            else
            {
              right_3d_timestamp = GST_CLOCK_TIME_NONE;
			  out_right_3d_timestamp = GST_CLOCK_TIME_NONE;

			  right_sync_got_eos = &(omx_base->in_port->got_eos);
			  
			  g_mutex_init(&right_done_mutex);
			  g_cond_init (&right_done_cond);

			  g_mutex_init(&right_sync_mutex);
			  g_cond_init (&right_sync_cond);

			  g_mutex_init(&right_in_done_mutex);
			  g_cond_init (&right_in_done_cond);

			  g_mutex_init(&right_in_sync_mutex);
			  g_cond_init (&right_in_sync_cond);

			  right_thread_inited = TRUE;
			  
			  omx_base->sync_mutex = &left_sync_mutex;
			  omx_base->sync_cond = &left_sync_cond;
			  omx_base->in_sync_mutex = &left_in_sync_mutex;
			  omx_base->in_sync_cond = &left_in_sync_cond;
			  omx_base->sync_is_init = &left_thread_inited;

			  GST_DEBUG_OBJECT(self, "SET right_thread_inited = TRUE in omx_setup");
              self->type_3d = TYPE_3D_MVC_DEPENDENT;
              decoderInputType.nStreoScopicType = OMX_Video_StereoScopic3D_MVC_D;
            }
          }
          else if (g_str_has_prefix(str3Dformat, "SVAF_2ES_"))
          {
            if (g_str_has_suffix(str3Dformat, "2ES_Left"))
            {
              left_3d_timestamp = GST_CLOCK_TIME_NONE;
			  out_left_3d_timestamp = GST_CLOCK_TIME_NONE;

			  left_sync_got_eos = &(omx_base->in_port->got_eos);
			  
			  g_mutex_init(&left_done_mutex);
			  g_cond_init (&left_done_cond);
			  
			  g_mutex_init(&left_sync_mutex);
			  g_cond_init (&left_sync_cond);

			  g_mutex_init(&left_in_done_mutex);
			  g_cond_init (&left_in_done_cond);
			  
			  g_mutex_init(&left_in_sync_mutex);
			  g_cond_init (&left_in_sync_cond);

			  left_thread_inited = TRUE;

			  omx_base->sync_mutex = &right_sync_mutex;
			  omx_base->sync_cond = &right_sync_cond;

			  omx_base->in_sync_mutex = &right_in_sync_mutex;
			  omx_base->in_sync_cond = &right_in_sync_cond;
			  
			  omx_base->sync_is_init = &right_thread_inited;

			  GST_DEBUG_OBJECT(self, "SET left_thread_inited = TRUE in omx_setup");
              self->type_3d = TYPE_3D_SVAF_2ES_LEFT;
              decoderInputType.nStreoScopicType = OMX_Video_StereoScopic3D_SVAF_L;
            }
            else
            {
              right_3d_timestamp = GST_CLOCK_TIME_NONE;
			  out_right_3d_timestamp = GST_CLOCK_TIME_NONE;

			  right_sync_got_eos = &(omx_base->in_port->got_eos);
			  
			  g_mutex_init(&right_done_mutex);
			  g_cond_init (&right_done_cond);

			  g_mutex_init(&right_sync_mutex);
			  g_cond_init (&right_sync_cond);

			  g_mutex_init(&right_in_done_mutex);
			  g_cond_init (&right_in_done_cond);

			  g_mutex_init(&right_in_sync_mutex);
			  g_cond_init (&right_in_sync_cond);

			  right_thread_inited = TRUE;
			  
			  omx_base->sync_mutex = &left_sync_mutex;
			  omx_base->sync_cond = &left_sync_cond;
			  omx_base->in_sync_mutex = &left_in_sync_mutex;
			  omx_base->in_sync_cond = &left_in_sync_cond;
			  omx_base->sync_is_init = &left_thread_inited;

			  GST_DEBUG_OBJECT(self, "SET right_thread_inited = TRUE in omx_setup");
              self->type_3d = TYPE_3D_SVAF_2ES_RIGHT;
              decoderInputType.nStreoScopicType = OMX_Video_StereoScopic3D_SVAF_R;
            }
          }
          GST_DEBUG_OBJECT(omx_base, "str3Dformat[ %s ] nStreoScopicType[ %d ]", str3Dformat, decoderInputType.nStreoScopicType);
        }

        /* H264 : Support multiple selection by OR operation for decoding_type. Refer to gstomx_h264::type_class_init()   (ex, CLIP_MODE | MULTI_DECODING_MODE)
        * OTHER CODECS : Only a mode can be selected (Clip or Seamless) */
        if (CLIP_MODE & self->decoding_type)  // 0x01:clip  0x02:seamless  -> exclusive
        {
          decoderInputType.nVideoDecodingType = 1;
          GST_ERROR_OBJECT(omx_base, "THIS IS CLIP MODE !!");
        }
        else if (SEAMLESS_MODE & self->decoding_type)
        {
          decoderInputType.nVideoDecodingType = 2;
          GST_ERROR_OBJECT(omx_base, "THIS IS SEAMLESS MODE !!");
        }

        if (MULTI_DECODING_MODE & self->decoding_type)  // 0x04:multidecoding
        {
          decoderInputType.bMultiDecoding = OMX_TRUE;
          GST_ERROR_OBJECT(omx_base, "THIS IS N-DECODING MODE !!");
        }
        else
          decoderInputType.bMultiDecoding = OMX_FALSE;

        if(gst_structure_get_boolean(structure, "secure", &security_enabled))
        {
          GST_DEBUG_OBJECT(omx_base, "bSecureMode[ %x ]", security_enabled);
        }

        if(gst_structure_get_boolean(structure, "preset",&preset_enabled))
        {
          GST_DEBUG_OBJECT(omx_base, "bPresetMode[ %x ]",preset_enabled);
        }

        if (self->error_concealment)
        {
          decoderInputType.bErrorConcealment = OMX_TRUE;
          GST_DEBUG_OBJECT(omx_base, "ERROR CONCEALMENT ON !!");
        }
	else
          decoderInputType.bErrorConcealment = OMX_FALSE;
	

        decoderInputType.bSecureMode = security_enabled;
        decoderInputType.bPresetMode = preset_enabled;
        GST_DEBUG_OBJECT(omx_base,"param[ bSecureMode(%d),bPresetMode(%d) ]",decoderInputType.bSecureMode,decoderInputType.bPresetMode);
        OMX_SetParameter(gomx->omx_handle, OMX_IndexVideoDecInputParam, &decoderInputType);
	  gomx->media_type = 1;

        param.nPortIndex = omx_base->in_port->port_index;
        OMX_GetParameter (gomx->omx_handle, OMX_IndexParamPortDefinition, &param);

        gst_structure_get_int (structure, "width", &width);
        gst_structure_get_int (structure, "height", &height);
        param.format.video.nFrameWidth = width;
        param.format.video.nFrameHeight = height;
        if (self->framerate_denom)
          param.format.video.xFramerate = (OMX_U32)((((OMX_U64)(self->framerate_num)) << 16) / self->framerate_denom); // xFramerate is Q16 format. U64 converting is just to prevent the overflow.
        else
        {
          param.format.video.xFramerate = (OMX_U32)(30 << 16);
          GST_ERROR_OBJECT(omx_base, "FRAME RATE ERROR!! [ num:%d / den:%d ], SO, WE SET 30FPS BY CONSTRAINT", self->framerate_num, self->framerate_denom);
        }
        param.format.video.eCompressionFormat = self->compression_format;
        GST_DEBUG_OBJECT(omx_base, "w&h[ %d x %d ], fmt[ 0x%x ], FrameRate[ %d Q16 = (%f fps) = %d/%d ]",
            width, height, param.format.video.eCompressionFormat, param.format.video.xFramerate, (float)(param.format.video.xFramerate)/(1<<16), self->framerate_num, self->framerate_denom);
        OMX_SetParameter (gomx->omx_handle, OMX_IndexParamPortDefinition, &param);
      }
      else
      {
        GST_ERROR_OBJECT(omx_base, "NO SRC CAPS STRUCTURE");
      }
    }
    else
    {
      GST_ERROR_OBJECT(omx_base, "NO SRC CAPS");
    }
  }
  GST_INFO_OBJECT (omx_base, "end");
}

#if 0
static void
dtv_cc_output_loop (gpointer data)
{
  GstPad *dtv_cc_srcpad;
  GOmxCore *gomx;
  GOmxPort *out_port;
  GstOmxBaseFilter *omx_base;
  GstFlowReturn ret = GST_FLOW_OK;

  dtv_cc_srcpad = data;
  omx_base = GST_OMX_BASE_FILTER (gst_pad_get_parent (dtv_cc_srcpad));
  gomx = omx_base->gomx;

  GST_LOG_OBJECT (omx_base, "begin");
}
#endif

static gboolean srcpad_event (GstPad * pad, GstEvent *event)
{
  g_return_val_if_fail(pad, FALSE);
  g_return_val_if_fail(event, FALSE);
  GstOmxBaseFilter *self = GST_OMX_BASE_FILTER (GST_OBJECT_PARENT (pad));
  GOmxCore *gomx = self->gomx;
  switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:
    {
      GstStructure* structure = gst_event_get_structure (event);
      gboolean onoff = FALSE;
      if(gst_structure_has_name(structure, "cache_control") && gst_structure_get_boolean(structure, "onoff", &onoff))
      {
        OMX_VIDEO_CONFIG_BUFFER_INVALIDATE config;
        G_OMX_INIT_PARAM (config);
        config.bBufferInvalidate = (onoff ? OMX_TRUE:OMX_FALSE);
        OMX_ERRORTYPE ret = OMX_SetConfig (self->gomx->omx_handle, OMX_IndexConfigVideoBufferInvalidate, &config);
        if (ret != OMX_ErrorNone)
          GST_ERROR_OBJECT(self, "OMX_SetConfig return[ %d ]", ret);
        GST_ERROR_OBJECT(self, "Rotation Cache invalidate[ %s ]", onoff ? "ON":"OFF");
      }
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, event);
}


static void
type_instance_init (GTypeInstance * instance, gpointer g_class)
{
  GstOmxBaseFilter *omx_base;
  GstOmxBaseVideoDec *self;

  omx_base = GST_OMX_BASE_FILTER (instance);
  self = GST_OMX_BASE_VIDEODEC (omx_base);

  self->decoding_type = SEAMLESS_MODE; // Default
  self->compression_format = OMX_VIDEO_CodingAutoDetect;

  omx_base->debugCategory = GST_CAT_VDEC;

  omx_base->omx_setup = omx_setup;

  omx_base->playscene = 0;
  omx_base->pre_format = GST_MAKE_FOURCC('I', '4', '2', '0');  //set default format.
  omx_base->gomx->settings_changed_cb = settings_changed_cb;
#ifdef TVS_USERDATA_PARSER
  omx_base->gomx->userdata_cb = userdata_cb;
#else
  omx_base->gomx->userdata_cb = NULL;
#endif
  omx_base->gomx->userdata_etc_cb = seimetadata_cb;
  omx_base->gomx->colour_description_cb = colourdescription_cb;
  gst_pad_set_setcaps_function (omx_base->sinkpad, sink_setcaps);
  gst_pad_set_event_function(omx_base->srcpad, srcpad_event);

  self->dtv_cc_srcpad = NULL;
  self->udManager = NULL;
  self->source_changed = TRUE;
  self->rating = 0;
  self->got_rating = FALSE;
  self->need_set_fpa_3dtype = 0;
  self->seimetadata_filled = FALSE;
  self->color_description_filled = FALSE;
  SEIMETADATA_SIZE = sizeof(OMX_VIDEO_USERDATA_ETC);
  memset(&self->seimetadata, 0x0, SEIMETADATA_SIZE);
  COLOURDESCRIPTION_SIZE = sizeof(OMX_VIDEO_COLOUR_DESCRIPTION);
  memset(&self->color_description, 0x0, COLOURDESCRIPTION_SIZE);
  SEI_MDCV_SIZE = sizeof(OMX_VIDEO_MASTERING_DISPLAY_COLOUR_VOLUME);
  memset(&self->sei_mdcv_data, 0x0, SEI_MDCV_SIZE);

  self->pad_event = NULL;
  omx_base->pad_event = pad_event;
  self->type_3d = TYPE_3D_NONE;
  self->pre_scantype = 0;
  self->pre_bitrate = 0;
  omx_base->sync_mutex = NULL;
  omx_base->sync_cond = NULL;
  omx_base->in_sync_mutex = NULL;
  omx_base->in_sync_cond = NULL;
  omx_base->sync_is_init = NULL;
  self->userdata_extraction_mode = 1;
}

/*
 * implementation of functions to handle FPA data
 */
static void read_n_bits_from_lsb_to_msb(FPAParser* parser, gint n)
{
  gint mask = 0;
  gint index = 0;
  gint internalPos_bit = 0;
  const gint EightBit = 8;
  guint8 tmp = 0;

  if (!parser || (parser && (!parser->bits)))
    return;
  
  while(n --)
  {
    index = parser->pos_bit / EightBit;
    if(index >= parser->length_byte)
      break;

    internalPos_bit = (parser->pos_bit - (EightBit * index));
    mask = 1 << internalPos_bit;
    tmp = ((parser->data_[index] & mask) >> internalPos_bit);
    parser->bits = g_byte_array_prepend(parser->bits, &tmp, 1);
    ++ (parser->pos_bit);
  }
}

static void read_n_bits_from_msb_to_lsb(FPAParser* parser, gint n)
{
  gint mask = 0;
  gint index = 0;
  gint internalPos_bit = 0;
  const gint EightBit = 8;
  guint8 tmp = 0;

  if (!parser || (parser && (!parser->bits)))
    return;

  while(n --)
  {
    index = parser->pos_bit / EightBit;
    if(index >= parser->length_byte)
      break;

    internalPos_bit = 7 - (parser->pos_bit - (EightBit * index));
    mask = 1 << internalPos_bit;
    tmp = ((parser->data_[index] & mask) >> internalPos_bit);
    parser->bits = g_byte_array_append(parser->bits, &tmp, 1);
    ++ (parser->pos_bit);
  }
}

static int make_binary_number_(FPAParser* parser)
{
  int retval = 0;
  if (!parser || (parser && (!parser->bits)))
    return 0;
  
  while(parser->bits->len > 0)
  {
    retval |= parser->bits->data[0] << (parser->bits->len - 1);
    parser->bits = g_byte_array_remove_index(parser->bits, 0);
  }

  return retval;
}

int read_bit(FPAParser* parser, int n)
{
  if(parser == NULL || n <= 0 || (parser && !parser->bits))
    return 0;

  if(parser->readOrder_ == LSB2MSB)
    read_n_bits_from_lsb_to_msb(parser, n);
  else
    read_n_bits_from_msb_to_lsb(parser, n);
  return make_binary_number_(parser);
}

static void FPAParser_Init(FPAParser* parser)
{
  if (parser == NULL)
  {
    GST_ERROR("FPAParser ius NULL");
    return;
  }
  memset(parser, 0, sizeof(FPAParser));
  parser->bits = g_byte_array_new();
  if (!parser->bits)
  {
    GST_ERROR("g_byte_array_new() Failed");
  }
  return;
}

static void FPAParser_Deinit(FPAParser* parser)
{
  if (parser && parser->bits)
  {
    g_byte_array_unref(parser->bits);
  }
}

static void calculate_frame_packing_arrangement_id_(FPAParser* parser)
{
  gint b=0;
  gint leadingZeroBits = -1;
  gint firstHalf = 0;
  gint secondHalf = 0;
  
  if (!parser)
    return;
  
  for(b=0; !b; leadingZeroBits ++)
  {
    b = read_bit(parser, 1);
  }

  firstHalf = (gint)(pow((double)2, (double)leadingZeroBits) - 1);
  secondHalf = read_bit(parser, leadingZeroBits);

  parser->frame_packing_arrangement_id_ =  firstHalf + secondHalf;
}

static gboolean FPAParser_Analyze(FPAParser* parser)
{
  if (!parser)
    return FALSE;

  calculate_frame_packing_arrangement_id_(parser);
  parser->frame_packing_arrangement_cancel_flag_ = read_bit(parser, 1);

  GST_DEBUG("CodeNum[%d]", parser->frame_packing_arrangement_id_);
  GST_DEBUG("FPA_Cancel_Flag[%d]", parser->frame_packing_arrangement_cancel_flag_);

  if(!parser->frame_packing_arrangement_cancel_flag_)
  {
    parser->frame_packing_arrangement_type_ = read_bit(parser, 7);
    read_bit(parser, 1);
    parser->content_interpretation_type_ = read_bit(parser, 6);
    GST_DEBUG("FPA_Type[%d] Content_Type[%d]", parser->frame_packing_arrangement_type_, parser->content_interpretation_type_);
    return TRUE;
  }

  return FALSE;
}

/*
 * implementation of functions to handle data from omx
 */
static gboolean
dtv_cc_src_event (GstPad * pad, GstEvent * event)
{
  GstOmxBaseVideoDec *dec;
  gboolean res = FALSE;

  dec = GST_OMX_BASE_VIDEODEC (gst_pad_get_parent (pad));
  if (G_UNLIKELY (dec == NULL)) {
    gst_event_unref (event);
    return FALSE;
  }

  GST_DEBUG_OBJECT (dec, "received event %d, %s", GST_EVENT_TYPE (event),
      GST_EVENT_TYPE_NAME (event));
  /*dtv closed caption srcpad do not handle or passby any event to upstream elements.
	*the purpose of this function is to stop the default behaviour.*/

  gst_event_unref (event);
  gst_object_unref (dec);

  return res;
}

static gboolean
acquire_dtv_cc_src_pad(GstOmxBaseFilter *omx_base)
{
  GstOmxBaseVideoDec *self = GST_OMX_BASE_VIDEODEC (omx_base);

  if (self->dtv_cc_srcpad) {
    GST_DEBUG_OBJECT(omx_base, "dtc_cc_srcpad has already created!");
    return TRUE;
  }

  if (!dl_handle) {
    GST_DEBUG_OBJECT(omx_base, "load tvs userdata parser lib failed!");
    return FALSE;
  }

  GST_DEBUG_OBJECT(omx_base, "now create the dtc_cc_srcpad!");
  self->dtv_cc_srcpad = gst_pad_new_from_static_template (&dtv_cc_srctempl, "dtv_cc_src");
  gst_pad_set_event_function (self->dtv_cc_srcpad,
    GST_DEBUG_FUNCPTR (dtv_cc_src_event));
  gst_pad_use_fixed_caps (self->dtv_cc_srcpad);
  gst_pad_set_caps (self->dtv_cc_srcpad,
      gst_static_pad_template_get_caps (&dtv_cc_srctempl));

  gst_pad_set_active (self->dtv_cc_srcpad, TRUE);
  gst_element_add_pad (GST_ELEMENT (self), self->dtv_cc_srcpad);
  GST_DEBUG_OBJECT (self, "dtv_cc_srcpad is created!");

  //gst_pad_start_task (self->dtv_cc_srcpad, dtv_cc_output_loop, self->dtv_cc_srcpad);
	
  return TRUE;
}

#ifdef TVS_USERDATA_PARSER
static void
push_org_userdata(GstOmxBaseFilter *omx_base, OMX_VIDEO_PARAM_VBITYPE *cc_data)
{
  GstFlowReturn ret;
  GstBuffer *outbuf = NULL;
  GstOmxBaseVideoDec *self = GST_OMX_BASE_VIDEODEC (omx_base);
  outbuf = gst_buffer_new_and_alloc (sizeof(struct v4l2_sdp_vbi_format));
  if (!outbuf){
    GST_ERROR_OBJECT (omx_base, "error: alloc outbuf failed!");
    return;
  }
#if 0  
  	gst_buffer_init(outbuf);
#else
	/*gst_buffer_init is gstreamer internal static function, so we need to initial it ourself*/
	GST_BUFFER_TIMESTAMP (outbuf) = GST_CLOCK_TIME_NONE;
	GST_BUFFER_DURATION (outbuf) = GST_CLOCK_TIME_NONE;
	GST_BUFFER_OFFSET (outbuf) = GST_BUFFER_OFFSET_NONE;
	GST_BUFFER_OFFSET_END (outbuf) = GST_BUFFER_OFFSET_NONE;
	GST_BUFFER_FREE_FUNC (outbuf) = g_free;
#endif
  memcpy(GST_BUFFER_DATA (outbuf),(void*)cc_data, sizeof(struct v4l2_sdp_vbi_format));
  GST_BUFFER_SIZE (outbuf) = sizeof(struct v4l2_sdp_vbi_format);
  gst_buffer_set_caps (outbuf, GST_PAD_CAPS(self->dtv_cc_srcpad));
  GST_DEBUG_OBJECT (omx_base, "The data is:[ %s ], size[ %d ]", GST_BUFFER_DATA (outbuf)? GST_BUFFER_DATA (outbuf): "NULL", GST_BUFFER_SIZE (outbuf));
  ret = gst_pad_push (self->dtv_cc_srcpad, outbuf);
  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (omx_base, "Push of dtv closed caption line returned flow %s", gst_flow_get_name (ret));
  }
  return;
}

static void
push_dtv_cc_data(GstOmxBaseFilter *omx_base, OMX_VIDEO_PARAM_VBITYPE *cc_data)
{
  gint udType;
  gboolean got_caption = FALSE;
  GstFlowReturn ret;
  TTUDM caption_data;
  GstBuffer *outbuf = NULL;
  GstOmxBaseVideoDec *self = GST_OMX_BASE_VIDEODEC (omx_base);	
  if (!_CreateUDMInstance || !_ResetUDMInstance || !_ProcessUserData || !_GetRating || !_GetClosedCaption) {
		GST_ERROR_OBJECT(omx_base, "_CreateUDMInstance[ %p ] _ResetUDMInstance[ %p ] _ProcessUserData[ %p ] _GetRating[ %p ] _GetClosedCaption[ %p ]",
			_CreateUDMInstance, _ResetUDMInstance, _ProcessUserData, _GetRating, _GetClosedCaption);
		return;
  }

  if (!self->udManager) {
    GST_DEBUG_OBJECT(omx_base, "udManager is not created yet, so create it!");
    self->udManager = _CreateUDMInstance(CD_USER_DATA_TYPE_DIGITAL, 0);
  }
  if (self->source_changed) {
    GST_DEBUG_OBJECT(omx_base, "source changed, ResetUDMInstance!");
    _ResetUDMInstance (self->udManager);
    self->source_changed = FALSE;
  }
  GST_DEBUG_OBJECT(omx_base, "The orignal data is [%s]", cc_data->payload);
  GST_DEBUG_OBJECT(omx_base, "The data size is [%d]", cc_data->payload_size);  
  

  GST_DEBUG_OBJECT(omx_base, "now process the dtv closed caption data!");
  udType = _ProcessUserData (self->udManager, (struct v4l2_sdp_vbi_format *)cc_data);
  switch(udType)
  {
    case CD_PROCESS_RESULT_CAPTION:
	  got_caption = TRUE;
      break;
    case CD_PROCESS_RESULT_BOTH:
	  got_caption = TRUE;
    case CD_PROCESS_RESULT_RATING:
      self->rating = _GetRating (self->udManager);
      self->got_rating = TRUE;
	  break;
    case CD_PROCESS_RESULT_DISCARD:
    default:
	  break;
  }

  if (!got_caption) {
    GST_DEBUG_OBJECT(omx_base, "no closed caption data got!");
    return;
  }

  caption_data = _GetClosedCaption (self->udManager);
  
  outbuf = gst_buffer_new_and_alloc (caption_data.m_size);
  if (!outbuf){
    GST_ERROR_OBJECT (omx_base, "error: alloc outbuf failed!");
    return;
  }

  memcpy(GST_BUFFER_DATA (outbuf),caption_data.m_data,caption_data.m_size);
  GST_BUFFER_SIZE (outbuf) = caption_data.m_size;
  GST_BUFFER_TIMESTAMP (outbuf) = caption_data.m_udInfo.pts;

  //caption_data = (TTUDM *)GST_BUFFER_DATA (outbuf);
  //*caption_data = _GetClosedCaption (self->udManager);
 
  if (self->got_rating) {
    GstStructure *dtv_cc_rating = NULL;
    dtv_cc_rating = gst_structure_new ("dtv-cc-info", "rating", G_TYPE_UINT, self->rating, NULL);
    if (dtv_cc_rating) {
      GST_LOG_OBJECT (self, "attach dtv cc rating(%d)!", self->rating);
      gst_buffer_set_qdata(outbuf, gst_structure_get_name_id(dtv_cc_rating), dtv_cc_rating);
      self->got_rating = FALSE;
    }
  }
  gst_buffer_set_caps (outbuf, GST_PAD_CAPS(self->dtv_cc_srcpad));
  GST_DEBUG_OBJECT (omx_base, "pushing a dtv closed caption line with size(%d), time(%" GST_TIME_FORMAT "), caps(%" GST_PTR_FORMAT ")",
    GST_BUFFER_SIZE (outbuf), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (outbuf)), GST_PAD_CAPS(self->dtv_cc_srcpad));
  GST_DEBUG_OBJECT (omx_base, "The data is:[%s]", GST_BUFFER_DATA (outbuf)? GST_BUFFER_DATA (outbuf): "NULL");
  ret = gst_pad_push (self->dtv_cc_srcpad, outbuf);

  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (omx_base, "Push of dtv closed caption line returned flow %s", gst_flow_get_name (ret));
  }
}

static void
userdata_cb(GOmxCore * core, gconstpointer data)
{
  GstOmxBaseFilter *omx_base;
  OMX_VIDEO_PARAM_VBITYPE* tmp = (OMX_VIDEO_PARAM_VBITYPE*)data;
  omx_base = core->object;
  GstOmxBaseVideoDec *self = GST_OMX_BASE_VIDEODEC (omx_base);
  g_return_if_fail(omx_base->srcpad);
  
  if (!tmp) {
    GST_ERROR_OBJECT(omx_base, "data is NULL");
    return;
  }

  GST_DEBUG_OBJECT (omx_base , "d_type[ %d ] PicStr[ %d ] TF[ %d ], tempRef[ %d ], pts[ %d ], vbi[ %d ], valid[ %d ], payS[ %d ] pay[ %x ] rsv[ %d ]",
      tmp->data_type, tmp->picture_structure, tmp->top_field_first, tmp->temporal_reference, tmp->pts, tmp->vbi_line,
      tmp->valid, tmp->payload_size, tmp->payload[0], tmp->reserved[1]);

  if (tmp->data_type & 0x04) // include DTV_FPA
  {
    // Parse the payload, If it is real FPA and we knew the type, then signal to player.
    guint i=0;
    gchar str[128*5] = {0,};
    gchar* strtmp = NULL;
    guint maxSize = (tmp->payload_size > 128 ? 128 : tmp->payload_size);
    FPAParser parser;
    FPAParser_Init(&parser);
    for(i=0; i<maxSize; i++)
    {
      strtmp = str + (i*2);
      snprintf(strtmp, 4, "%02x", tmp->payload[i]);
    }
    GST_DEBUG_OBJECT (omx_base, "PAYLOAD[ %d ] : 0x%s", tmp->payload_size, str);
    parser.pos_bit = 0;
    parser.readOrder_ = MSB2LSB;
    parser.data_ = tmp->payload;
    parser.length_byte = tmp->payload_size;
    if (FPAParser_Analyze(&parser))
    {
      GstStructure* structure = gst_caps_get_structure (GST_PAD_CAPS(omx_base->srcpad), 0);
      GST_DEBUG_OBJECT(omx_base, "Before : %" GST_PTR_FORMAT, structure);

      if (structure)
      {
        if (parser.frame_packing_arrangement_type_ == 3/*SideBySide, SVAF*/)
        {
          gst_structure_set(structure, "3Dformat", G_TYPE_STRING, "SVAF", "3Dmode", G_TYPE_STRING, "SideBySide", NULL);
        }
        else if (parser.frame_packing_arrangement_type_ == 4/*TopBottom, SVAF*/)
        {
          gst_structure_set(structure, "3Dformat", G_TYPE_STRING, "SVAF", "3Dmode", G_TYPE_STRING, "TopAndBottom", NULL);
        }
      }
      else
        self->need_set_fpa_3dtype = parser.frame_packing_arrangement_type_; // Somtimes this userdata_cb can be called before set srccaps by setting_changed_cb.

      GST_INFO_OBJECT(omx_base, "SVAF[ %d ], After :%" GST_PTR_FORMAT, parser.frame_packing_arrangement_type_, GST_PAD_CAPS(omx_base->srcpad));
    }
    FPAParser_Deinit(&parser);
  }

  if (tmp->data_type & 0x01) // include DTV_CC
  {
    // Parse CC
    // add a new pad to send a ccdata
    GST_DEBUG_OBJECT(omx_base, "DTV_CC!!! PAYLOAD[ %d ]", tmp->payload_size);
    if (!acquire_dtv_cc_src_pad(omx_base)) {
      GST_DEBUG_OBJECT(omx_base, "error: acquire dtv cc srcpad failded!");
      return;
    };


    if (self->userdata_extraction_mode == 0)
      push_org_userdata(omx_base, tmp); /* Push original userdata to the next element */
    else
      push_dtv_cc_data(omx_base, tmp);
  }
}
#endif

static void
seimetadata_cb(GOmxCore * core, OMX_EVENTTYPE event, OMX_U32 data_1, OMX_U32 data_2, OMX_PTR event_data)
{
	GstOmxBaseFilter *omx_base = core->object;
	GstOmxBaseVideoDec *self = GST_OMX_BASE_VIDEODEC (omx_base);
	GST_WARNING_OBJECT(omx_base, "seimetadata[ 0x%x ] aquired", event);
	g_return_if_fail(event_data);
	
	if (event == OMX_EventSdpUserdataEtc)
	{
		OMX_VIDEO_USERDATA_ETC* tmp = (OMX_VIDEO_USERDATA_ETC*)event_data;
		memset(&self->seimetadata, 0x0, SEIMETADATA_SIZE);
		memcpy(&self->seimetadata, tmp, SEIMETADATA_SIZE);
		GST_WARNING_OBJECT(omx_base, "seimetadata aquired[ pts( %lld us, %d),  size: %d ]", (((guint64)(self->seimetadata.pts)) << 6), self->seimetadata.pts, self->seimetadata.size);
		self->seimetadata_filled = TRUE;
	}
	else if (event == OMX_EventSdpMasteringDisplayColourVolume)
	{
		OMX_VIDEO_MASTERING_DISPLAY_COLOUR_VOLUME* tmp = (OMX_VIDEO_MASTERING_DISPLAY_COLOUR_VOLUME*)event_data;
		memcpy(&self->sei_mdcv_data, tmp, SEI_MDCV_SIZE);
		GST_WARNING_OBJECT(omx_base, "primaries_x[ %d %d %d ], y[ %d %d %d ], white_point[ %d, %d ], lum[ %d, %d ]", 
				tmp->display_primaries_x[0], tmp->display_primaries_x[1], tmp->display_primaries_x[2],
				tmp->display_primaries_y[0], tmp->display_primaries_y[1], tmp->display_primaries_y[2],
				tmp->white_point_x, tmp->white_point_y, tmp->max_display_mastering_luminance,tmp->min_display_mastering_luminance);
		self->sei_mdcv_data_filled = TRUE;
	}
}

static void
colourdescription_cb(GOmxCore * core, gconstpointer data)
{
  GstOmxBaseFilter *omx_base = core->object;
  GST_WARNING_OBJECT(omx_base, "colourdescription_cb aquired");
  
  OMX_VIDEO_COLOUR_DESCRIPTION* tmp = (OMX_VIDEO_COLOUR_DESCRIPTION*)data;
  GstOmxBaseVideoDec *self = GST_OMX_BASE_VIDEODEC (omx_base);
  g_return_if_fail(omx_base->srcpad);

  if (!tmp) {
    GST_ERROR_OBJECT(omx_base, "data is NULL");
    return;
  }

	memset(&self->color_description, 0x0, COLOURDESCRIPTION_SIZE);
	memcpy(&self->color_description, tmp, COLOURDESCRIPTION_SIZE);

	GstCaps* new_caps = gst_caps_copy(gst_pad_get_negotiated_caps (omx_base->srcpad));
	GstStructure *structure = gst_caps_get_structure(new_caps, 0 );

	if(structure)
	{	
	  GST_WARNING_OBJECT(omx_base, "colourdescription_cb set!!");
		gst_structure_set(structure,
			"iColorPrimaries", G_TYPE_INT, self->color_description.colour_primaries, 
			"iTransferCharacteristics", G_TYPE_INT, self->color_description.transfer_characteristics,
			"iMatrixCoefficients", G_TYPE_INT,self->color_description.matrix_coeffs, NULL);

		GST_INFO_OBJECT(omx_base, "colourdescription : %p" GST_PTR_FORMAT, structure);

		gst_pad_set_caps(GST_PAD_CAPS(omx_base->srcpad), new_caps);
	}
	else
	{
		self->color_description_filled = TRUE;
		GST_INFO_OBJECT(omx_base, "colourdescription aquired");
	}
	gst_caps_unref(new_caps);
}


