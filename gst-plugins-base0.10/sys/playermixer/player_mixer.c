
#include "player_mixer.h"
#include "player_mixer_log.h"
#include "tbm_utils.h"

#include <glib.h>
#include <stdint.h>

#include <linux/videodev2.h>
#include <libdrm/drm.h>
#include <gst/app/gstappsrc.h>
#include <gst/interfaces/xoverlay.h>  
#include <gst/gstconfig.h>
#include <gst/gststructure.h>
#include <gst/glib-compat.h>

#define  NUM_MIXER_WINDOW 5
#define MIXER_WIDTH 1920	//For FHD Target
#define MIXER_HEIGHT 1080

typedef struct  _position
{
	unsigned int x;
	unsigned int y;
	unsigned int w;
	unsigned int h;
}position_s;

position_s mixerwindow[NUM_MIXER_WINDOW];
position_s beforemixerwindow[NUM_MIXER_WINDOW];

typedef struct _mixer_s
{
	GstElement *pipeline;
	GstElement *appsrc;
	GstPad *srcpad;
	int idx;
   	tbm_bo tbo_AP_YVU420_Interlace;
   	tbm_bo_handle tbo_hnd_AP_YVU420_Interlace;
	int drm_fd;
	tbm_bufmgr bufmgr_AP;
	GstElement *sink;
	GMainLoop *loop;
	guint sourceid;
	GTimer *timer;
	uint8_t *data[4];
	int linesize[4];       ///< number of bytes per line
	gint decwidth;
	gint decheight;
	Display* disp;
	position_s playerpostion;
	GMutex* lock;
	int scaler_id;
}mixer_s;


/*
declarations of private functions
*/
static int player_mixer_initialize(mixer_h mixer);
static int player_mixer_makepipeline(mixer_h mixer);
static int player_mixer_maketbm(mixer_h mixer);
static int player_mixer_setdisplay(mixer_h mixer, void* display_handle);
static gboolean bus_message (GstBus * bus, GstMessage * message, mixer_s * mixer);

/*
implementation of public functions
*/

GstStaticPadTemplate app_src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv")
    );

mixer_s Tempmixer = {0,};
mixer_s* g_mixh = &Tempmixer;

int 
player_mixer_create(mixer_h* mixer, void* display_handle)
{
	mixer_s** handle = (mixer_s**)mixer;
	mixer_h temp=(mixer_s*)malloc(sizeof(mixer_s));
	memset(temp, 0 , sizeof(mixer_s));
	*handle=temp;
	MIXER_DBG("MIXER INSTANCE IS CREATED[%p]\n",(*handle)->sink);
	player_mixer_setdisplay(*handle,display_handle);
	player_mixer_initialize(*handle);
	return PLAYER_MIXER_ERROR_NONE;
	/*panelsize  
	   
	Orsay CVideoMixer CMixerBufffer 
	*/
}

int 
player_mixer_destory(mixer_h* mixer)
{
	MIXER_DBG("player_mixer_destroy entered\n");
	mixer_s* handle = *mixer;
	if(!gst_pad_unlink (gst_element_get_static_pad(handle->appsrc,"src"), gst_element_get_static_pad(handle->sink,"sink")))	
	{
			MIXER_DBG("fail to unlink between mixer and xvimagesink \n");
	}
	gst_element_set_state(handle->sink, GST_STATE_NULL);
	gst_element_set_state(handle->appsrc, GST_STATE_NULL);
	gst_element_set_state(handle->pipeline, GST_STATE_NULL);
	handle->sink = NULL;
	handle->appsrc = NULL;
	handle->pipeline = NULL;
	if(handle->tbo_AP_YVU420_Interlace)
	{
		tbm_bo_unmap(handle->tbo_AP_YVU420_Interlace);
		tbm_bo_unref(handle->tbo_AP_YVU420_Interlace);
		handle->tbo_AP_YVU420_Interlace = NULL;
	}
	if(handle->bufmgr_AP)
	deinit_tbm_bufmgr(&handle->drm_fd, &handle->bufmgr_AP);
	free(handle);
	memset(g_mixh,0,sizeof(mixer_s));
	unsigned int i =0;
	for(i=0;i<NUM_MIXER_WINDOW;i++)
	{
		memset(&beforemixerwindow[i],0,sizeof(position_s));
		memset(&mixerwindow[i],0,sizeof(position_s));
	}
	MIXER_DBG("mixer instance was free\n");
	return PLAYER_MIXER_ERROR_NONE;
}

void 
player_mixer_set_position(void* player)
{
	MIXER_DBG("player [%p]\n",player);
}

#define GEN_MASK(x) ((1<<(x))-1)
#define ROUND_UP_X(v,x) (((v) + GEN_MASK(x)) & ~GEN_MASK(x))
#define ROUND_UP_4(x) ROUND_UP_X (x, 2)
#define DIV_ROUND_UP_X(v,x) (((v) + GEN_MASK(x)) >> (x))

static
int create_mix_buffer(mixer_s* mixer,int* ysize,int* cbsize)
{
//Size calc
	int size1, w2, h2, size2;
	int stride, stride2;
	stride = ROUND_UP_4 (MIXER_WIDTH);
	h2 = ROUND_UP_X (MIXER_HEIGHT, 1/*y_chroma_shift*/);
	size1 = stride * h2;
	w2 = DIV_ROUND_UP_X (MIXER_WIDTH, 1/*x_chroma_shift*/);
	stride2 = ROUND_UP_4 (w2);
	h2 = DIV_ROUND_UP_X (MIXER_HEIGHT, 1/*y_chroma_shift*/);
	size2 = stride2 * h2;
	gint fsize = size1 + 2 * size2;
	*ysize = size1;
	*cbsize = size2;
	uint8_t * ptr = NULL;
// Allocate the AP memory 
	if(!mixer->tbo_hnd_AP_YVU420_Interlace.ptr)
	{
		mixer->tbo_AP_YVU420_Interlace =  tbm_bo_alloc(mixer->bufmgr_AP, fsize, TBM_BO_SCANOUT);
		if(!mixer->tbo_AP_YVU420_Interlace)
		{
			MIXER_ERR ("failed in tbm_bo_alloc");
			return PLAYER_MIXER_ERROR;
		}
		mixer->tbo_hnd_AP_YVU420_Interlace =  tbm_bo_map(mixer->tbo_AP_YVU420_Interlace, TBM_DEVICE_CPU, TBM_OPTION_WRITE);
		if(!mixer->tbo_hnd_AP_YVU420_Interlace.ptr)
		{
			MIXER_ERR ("failed in tbm_bo_map");
			return PLAYER_MIXER_ERROR;
		}
	}
    return PLAYER_MIXER_ERROR_NONE;
}

void player_mixer_set_property(int scaler_id)
{
	MIXER_DBG ("scaler_id[%d]",scaler_id);
	g_object_set(g_mixh->sink,"device-scaler",scaler_id,NULL);
}
void player_mixer_change_state(GstState state)
{
		gst_element_set_state (g_mixh->pipeline, state);		//MIXING START
}

void player_mixer_mixing(int displayhandle, GstBuffer* buf)
{	
	g_mutex_lock(g_mixh->lock);
	int linesize = 0;
	int outx,outy,outw, outh = 0;
	MIXER_DBG("dsplay handle [%d] x[%d]\n",displayhandle, mixerwindow[displayhandle].x);

	outx =mixerwindow[displayhandle].x;
	outy =mixerwindow[displayhandle].y; 
	outw = mixerwindow[displayhandle].w; 
	outh = mixerwindow[displayhandle].h;

	GstStructure *Decstruct = gst_caps_get_structure (GST_BUFFER_CAPS(buf), 0);
	gst_structure_get_int (Decstruct, "width", &(g_mixh->decwidth));
	gst_structure_get_int (Decstruct, "height", &(g_mixh->decheight));
	gst_structure_get_int (Decstruct, "linesize", &(linesize));

	linesize = 1024;
	MIXER_DBG("handle[%d] GET DATA!!!!!!!!!x[%d]y[%d]w[%d]h[%d] pointer[%p] input width[%d] input height[%d] input size[%d] linesize[%d] ",displayhandle,outx,outy,outw,outh,buf,g_mixh->decwidth,g_mixh->decheight,GST_BUFFER_SIZE(buf),linesize);
	GstFlowReturn ret = GST_FLOW_OK;
	GstBuffer  *outbuf = NULL;
	GstCaps *caps = NULL;
//TBM Buffer
	GstStructure *tbm_bo_s = NULL;
	const gchar * tbm_bo_name = "tbm_bo";
	int ysize, cbsize = 0;
	if(create_mix_buffer(g_mixh,&ysize,&cbsize) != PLAYER_MIXER_ERROR_NONE)
		return;
	outbuf = gst_buffer_new ();
	GST_BUFFER_DATA (outbuf) = g_mixh->tbo_hnd_AP_YVU420_Interlace.ptr;
	GST_BUFFER_SIZE (outbuf) = tbm_bo_size(g_mixh->tbo_AP_YVU420_Interlace);
	GST_BUFFER_FREE_FUNC (outbuf) = NULL;		//initialize
	
	guint boAP_key_input = 0;
	tbm_bo boAP_input = NULL;
	tbm_bo_handle boAP_hd_input = {0};
	GstStructure * struc1 = NULL;
	memset (&boAP_input, 0x0, sizeof(tbm_bo));
	memset (&boAP_hd_input, 0x0, sizeof(tbm_bo_handle));
	struc1 = gst_buffer_get_qdata(buf, g_quark_from_string("tbm_bo"));
	if (struc1 && gst_structure_get_uint(struc1, "tbm_bo_key", &boAP_key_input))
	{
		boAP_input = tbm_bo_import (g_mixh->bufmgr_AP, boAP_key_input);
		MIXER_DBG("getting tbm_bo_key is succeded\n");
	}
		
	if(beforemixerwindow[displayhandle].x != mixerwindow[displayhandle].x ||
	beforemixerwindow[displayhandle].y != mixerwindow[displayhandle].y ||
	beforemixerwindow[displayhandle].w != mixerwindow[displayhandle].w ||
	beforemixerwindow[displayhandle].h != mixerwindow[displayhandle].h )
	{
		memset(GST_BUFFER_DATA (outbuf),16,ysize*sizeof(guint8));
		int i =0;
		for(i=0;i<cbsize*2;i=i+2)
		{	
			*(GST_BUFFER_DATA (outbuf)+ysize+i )= 128;
			*(GST_BUFFER_DATA (outbuf)+ysize+i+1) = 128;	
		}
		beforemixerwindow[displayhandle].x = mixerwindow[displayhandle].x;
		beforemixerwindow[displayhandle].y = mixerwindow[displayhandle].y;
		beforemixerwindow[displayhandle].w = mixerwindow[displayhandle].w;
		beforemixerwindow[displayhandle].h = mixerwindow[displayhandle].h;
	}
	guint32 format; 
	gst_structure_get_fourcc(Decstruct, "format",&format);
    if (format && (format == GST_MAKE_FOURCC('S', 'T', 'V', '0')))
    {MIXER_DBG("HW Decoder Case\n");
		struct v4l2_drm *outinfo;               
		struct v4l2_drm_dec_info *decinfo;
		struct v4l2_private_frame_info *frminfo;
		outinfo = (struct v4l2_drm*)(GST_BUFFER_MALLOCDATA(buf));
		decinfo = &(outinfo->u.dec_info);
		frminfo = &(outinfo->u.dec_info.pFrame[0]);
		gint scaleret;
		tbm_ga_scale_wrap scale_wrap;
		memset(&scale_wrap, 0, sizeof(tbm_ga_scale_wrap));
		scale_wrap.bufmgr = g_mixh->bufmgr_AP;

		MIXER_DBG("HW input frminfo[%p] y_addr[%p] out_addr[%p] linesize[%d] linesize_v4l2[%d] u[%d] v[%d]\n",frminfo ,frminfo->y_phyaddr,GST_BUFFER_DATA (outbuf) ,linesize,frminfo->y_linesize,frminfo->u_linesize,frminfo->v_linesize);
	//scale Y
		scale_wrap.src_bo = NULL;
		scale_wrap.dst_bo = g_mixh->tbo_AP_YVU420_Interlace;
		scale_wrap.src_paddr = frminfo->y_phyaddr;
		scale_wrap.dst_paddr = NULL;
		scale_wrap.scale.color_mode = TBM_GA_FORMAT_8BPP;
		scale_wrap.scale.rop_mode = TBM_GA_ROP_COPY;
		scale_wrap.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
		scale_wrap.scale.src_hbyte_size = frminfo->y_linesize* TBM_BPP8;
		scale_wrap.scale.src_rect.x = 0;
		scale_wrap.scale.src_rect.y = 0; /* Y: Top of buffer (w*h) */
		scale_wrap.scale.src_rect.w =frminfo->width;
		scale_wrap.scale.src_rect.h = frminfo->height;
		scale_wrap.scale.dst_hbyte_size = MIXER_WIDTH * TBM_BPP8;  //   
		scale_wrap.scale.dst_rect.x = outx;
		scale_wrap.scale.dst_rect.y = outy;
		scale_wrap.scale.dst_rect.w = outw; /* the width and height of the target pixmap.*//*PM??*/
		scale_wrap.scale.dst_rect.h = outh;/*PM??*/
		scale_wrap.scale.rop_ca_value = 0;
		scale_wrap.scale.src_key = 0;
		scale_wrap.scale.rop_on_off = 0;

		scaleret = tbm_bo_ga_scale(&scale_wrap);
		if (!scaleret)
		{
			MIXER_ERR("Please check HW Decoder is used or not\n");
			MIXER_ERR("scaling Y failed! ret(%d)", scaleret);
			sleep(1);
			return scaleret;
		}
	//scale CbCr
		scale_wrap.src_bo = NULL;
		scale_wrap.dst_bo = g_mixh->tbo_AP_YVU420_Interlace;
		scale_wrap.src_paddr = frminfo->u_phyaddr;
		scale_wrap.dst_paddr = NULL;
		scale_wrap.scale.color_mode = TBM_GA_FORMAT_16BPP; /* Because of CbCr Interleaved */
		scale_wrap.scale.rop_mode = TBM_GA_ROP_COPY;
		scale_wrap.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
		scale_wrap.scale.src_hbyte_size =  frminfo->u_linesize* TBM_BPP8; 
		scale_wrap.scale.src_rect.x = 0;
		scale_wrap.scale.src_rect.y = 0; /* CbCr : Bottom of buffer (w*h/2)  HW   */
		scale_wrap.scale.src_rect.w = frminfo->width/2;  // 
		scale_wrap.scale.src_rect.h = frminfo->height/2;
		scale_wrap.scale.dst_hbyte_size = MIXER_WIDTH/*g_mixh->decwidth*/* TBM_BPP8;	//   
		scale_wrap.scale.dst_rect.x = outx/2; 
		scale_wrap.scale.dst_rect.y = (outy)/2+MIXER_HEIGHT;
		scale_wrap.scale.dst_rect.w = outw/2;  // 
		scale_wrap.scale.dst_rect.h = outh/2;/*PM??*/
		scale_wrap.scale.rop_ca_value = 0;
		scale_wrap.scale.src_key = 0;
		scale_wrap.scale.rop_on_off = 0;
		scaleret = tbm_bo_ga_scale(&scale_wrap);
		if (!scaleret) 
		{
			MIXER_ERR("scaling CbCr failed! ret(%d)", scaleret);
			return;
		}
    }
	else
	{
		MIXER_DBG("SW Decoder Case\n");
		gint scaleret;
		tbm_ga_scale_wrap scale_wrap;
		memset(&scale_wrap, 0, sizeof(tbm_ga_scale_wrap));
		scale_wrap.bufmgr = g_mixh->bufmgr_AP;
	//scale Y
		scale_wrap.src_bo = boAP_input;
		scale_wrap.dst_bo = g_mixh->tbo_AP_YVU420_Interlace;
		scale_wrap.src_paddr = NULL/*frminfo->y_phyaddr*/;
		scale_wrap.dst_paddr = NULL;
		scale_wrap.scale.color_mode = TBM_GA_FORMAT_8BPP;
		scale_wrap.scale.rop_mode = TBM_GA_ROP_COPY;
		scale_wrap.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
		scale_wrap.scale.src_hbyte_size = g_mixh->decwidth * TBM_BPP8; /* Y line size */
		scale_wrap.scale.src_rect.x = 0;
		scale_wrap.scale.src_rect.y = 0; /* Y: Top of buffer (w*h) */
		scale_wrap.scale.src_rect.w =g_mixh->decwidth;
		scale_wrap.scale.src_rect.h = g_mixh->decheight;
		scale_wrap.scale.dst_hbyte_size = MIXER_WIDTH * TBM_BPP8;  //   
		scale_wrap.scale.dst_rect.x = outx;
		scale_wrap.scale.dst_rect.y = outy;
		scale_wrap.scale.dst_rect.w = outw; /* the width and height of the target pixmap.*//*PM??*/
		scale_wrap.scale.dst_rect.h = outh;/*PM??*/
		scale_wrap.scale.rop_ca_value = 0;
		scale_wrap.scale.src_key = 0;
		scale_wrap.scale.rop_on_off = 0;
		scaleret = tbm_bo_ga_scale(&scale_wrap);
		if (!scaleret)
		{
			MIXER_ERR("Please check HW Decoder is used or not\n");
			MIXER_ERR("scaling Y failed! ret(%d)", scaleret);
			return;
		}

		scale_wrap.src_bo = boAP_input;
		scale_wrap.dst_bo = g_mixh->tbo_AP_YVU420_Interlace;
		scale_wrap.src_paddr = NULL/*frminfo->u_phyaddr*/;
		scale_wrap.dst_paddr = NULL;
		scale_wrap.scale.color_mode = TBM_GA_FORMAT_16BPP; /* Because of CbCr Interleaved */
		scale_wrap.scale.rop_mode = TBM_GA_ROP_COPY;
		scale_wrap.scale.pre_mul_alpha = TBM_GA_PREMULTIPY_ALPHA_OFF_SHADOW;
		scale_wrap.scale.src_hbyte_size =  g_mixh->decwidth/*xvimagesink->hw_frame_uv_linesize*/ * TBM_BPP8; /* for YUV420 interleaved case*/
		scale_wrap.scale.src_rect.x = 0;
		scale_wrap.scale.src_rect.y = g_mixh->decheight; /* CbCr : Bottom of buffer (w*h/2) */
		scale_wrap.scale.src_rect.w = g_mixh->decwidth/2;  // 
		scale_wrap.scale.src_rect.h = g_mixh->decheight/2;
		scale_wrap.scale.dst_hbyte_size = MIXER_WIDTH/*g_mixh->decwidth*/* TBM_BPP8;	//   
		scale_wrap.scale.dst_rect.x = outx/2; 
		scale_wrap.scale.dst_rect.y = (outy)/2+MIXER_HEIGHT;
		scale_wrap.scale.dst_rect.w = outw/2;  // 
		scale_wrap.scale.dst_rect.h = outh/2;/*PM??*/
		scale_wrap.scale.rop_ca_value = 0;
		scale_wrap.scale.src_key = 0;
		scale_wrap.scale.rop_on_off = 0;
		scaleret = tbm_bo_ga_scale(&scale_wrap);
		if (!scaleret) 
		{
			MIXER_ERR("scaling CbCr failed! ret(%d)", scaleret);
			return;
		}
	}	
	unsigned int boAP_key = tbm_bo_export(g_mixh->tbo_AP_YVU420_Interlace);
	tbm_bo_s = gst_structure_new(tbm_bo_name, "tbm_bo_key", G_TYPE_UINT, boAP_key, "tbm_bo_hnd", G_TYPE_POINTER, g_mixh->tbo_hnd_AP_YVU420_Interlace, 	NULL);
	if(tbm_bo_s)
	{
		gst_buffer_set_qdata(outbuf, g_quark_from_static_string(tbm_bo_name), tbm_bo_s);
		gst_buffer_set_caps (outbuf, GST_PAD_CAPS (g_mixh->srcpad));
	}
	g_signal_emit_by_name (g_mixh->appsrc, "push-buffer",outbuf, &ret);
	gst_buffer_unref (outbuf);
      if (ret != GST_FLOW_OK) 
	{
		MIXER_ERR ("push-buffer error\n");
		return;
	}
	MIXER_DBG("mixing is end\n");
	g_mutex_unlock(g_mixh->lock);
}

int 
player_mixer_get_disp(Display** disp)
{
	if(g_mixh->disp)
	{
		*disp = g_mixh->disp;
		MIXER_DBG("getdisp [%p]",*disp);
		return PLAYER_MIXER_ERROR_NONE;		
	}
	else	
	{
		MIXER_DBG("g_mixh->disp is NULL");
		return PLAYER_MIXER_ERROR;
	}
}

void 
player_mixer_connect(int* handle)
{
	static int playerhandle = 0;
	*handle =  playerhandle;
	MIXER_DBG("handle[%d]\n",*handle);
	playerhandle++;
}
void player_mixer_set_display_area(mixer_h* mixer,int* display_handle,int x,int y, int w, int h)
{
	if(*display_handle == -1)
	{
		static int mixerwindownum = 0;
		memset(&beforemixerwindow[mixerwindownum],0,sizeof(position_s));
		mixerwindow[mixerwindownum].x = x;
		mixerwindow[mixerwindownum].y = y;
		mixerwindow[mixerwindownum].w = w;
		mixerwindow[mixerwindownum].h = h;
		*display_handle = mixerwindownum;
		MIXER_DBG("mixer_window is initialized, mixerwindow num[%d]",mixerwindownum);
		mixerwindownum++;
		if(mixerwindownum>=NUM_MIXER_WINDOW)
		mixerwindownum = 0;			//only 10 mixer is able to run at once. so the number of mixer instance(playerhandle) is over 10. then it will be initialized.
	}
	else
	{
		mixerwindow[*display_handle].x = x;
		mixerwindow[*display_handle].y = y;
		mixerwindow[*display_handle].w = w;
		mixerwindow[*display_handle].h = h;
		MIXER_DBG("mixer_window is not initialized, display_handle not [-1], APP want to change the window dinamically");
	}
}

/*
implementation of private functions
*/

static int 
player_mixer_setdisplay(mixer_h mixer, void* display_handle)
{
	mixer_s* handle = (mixer_s*)mixer;
	MIXER_DBG("mixer_set_display BEGIN\n");
	if(handle == NULL)
	{
		MIXER_ERR("app or app->src is null\n");
		return PLAYER_MIXER_ERROR;
	}
	handle->sink = gst_element_factory_make("xvimagesink","xvimagesink");
	gst_x_overlay_set_xwindow_id( GST_X_OVERLAY(handle->sink), display_handle );
	MIXER_DBG("mixer_set_display END[%p] \n",handle->sink);
	return PLAYER_MIXER_ERROR_NONE;
}

static int player_mixer_initialize(mixer_h mixer)
{
	mixer_s* handle = (mixer_s*)mixer;
	MIXER_DBG("mixer_connect BEGIN\n");
	player_mixer_makepipeline(handle);
	player_mixer_maketbm(handle);
	MIXER_DBG("mixer_connect END\n");
	return PLAYER_MIXER_ERROR_NONE;
}

static int player_mixer_makepipeline(mixer_h mixer)
{
	mixer_s* handle = (mixer_s*)mixer;
	char argv[100] = {0, };
	char *pargv = &argv;
	GstBus *bus = NULL;
	int argc = 0;
	gst_init(&argc,&pargv);
	handle->loop = g_main_loop_new (NULL, FALSE);
	handle->pipeline = gst_pipeline_new("MixerPipe");
	handle->appsrc = gst_element_factory_make("appsrc","mixer-app-src");
	if(!handle->pipeline ||!handle->appsrc ||!handle->sink)
	{
		MIXER_ERR("pipeline[%p] appsrc[%p] sink[%p]\n",handle->pipeline,handle->appsrc ,handle->sink);
		return PLAYER_MIXER_ERROR;
	}
	bus = gst_pipeline_get_bus (GST_PIPELINE (handle->pipeline));
	gst_bus_add_watch (bus, (GstBusFunc)bus_message, handle);
	g_object_unref(bus);
	gst_bin_add_many(GST_BIN(handle->pipeline),handle->appsrc,handle->sink,NULL);
	gst_element_link(handle->appsrc,handle->sink);
	GstCaps *caps = NULL;
	caps = gst_caps_new_simple("video/x-raw-yuv",
	              "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('S', 'T', 'V', '1'),
	              "width", G_TYPE_INT, MIXER_WIDTH,
	              "height", G_TYPE_INT, MIXER_HEIGHT,
	              "framerate", GST_TYPE_FRACTION, 1, 1,
	              NULL);
	gst_app_src_set_caps(GST_APP_SRC(handle->appsrc), caps);
	return PLAYER_MIXER_ERROR_NONE;
}

static int
player_mixer_maketbm(mixer_h mixer)
{
	MIXER_DBG( "+++Enter ");
	mixer_s* handle = (mixer_s*)mixer;
	GstStructure *structure = gst_structure_new("tbm_auth","TBMauthentication", G_TYPE_POINTER, 0, NULL);
	GstQuery     *query     = gst_query_new_application (GST_QUERY_CUSTOM, structure);
	handle->srcpad = gst_element_get_static_pad (handle->appsrc, "src");
	GstStructure * struc = NULL;
	if(gst_pad_peer_query(handle->srcpad, query))
	{
		struc = gst_query_get_structure (query);
		gst_structure_get(struc,"TBMauthentication",G_TYPE_POINTER,&handle->disp,NULL);
	}
	if (query)
	{
		gst_query_unref(query);
	}
	(handle)->drm_fd = -1;	
	if (init_tbm_bufmgr(&handle->drm_fd, &handle->bufmgr_AP, handle->disp) == FALSE)
	{
		MIXER_DBG( "drm_fd[ %d ], err[ %d ] ", handle->drm_fd, errno);
		return PLAYER_MIXER_ERROR;
	}
	handle->lock = g_mutex_new ();
	memset (&handle->tbo_AP_YVU420_Interlace, 0x0, sizeof(tbm_bo));
	memset (&handle->tbo_hnd_AP_YVU420_Interlace, 0x0, sizeof(tbm_bo_handle));
	memcpy(g_mixh,handle,sizeof(mixer_s));
	MIXER_DBG( "+++g_mixh ");
	return PLAYER_MIXER_ERROR_NONE;
}


static gboolean
bus_message (GstBus * bus, GstMessage * message, mixer_s * mixer)
{
   MIXER_DBG ("got message %s",gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
	switch (GST_MESSAGE_TYPE (message))
	{
		case GST_MESSAGE_ERROR: 
		{
			GError *err = NULL;
			gchar *dbg_info = NULL;
			gst_message_parse_error (message, &err, &dbg_info);
			MIXER_ERR ("ERROR from element %s: %s\n",
			GST_OBJECT_NAME (message->src), err->message);
			MIXER_DBG ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
			g_error_free (err);
			g_free (dbg_info);
			if(mixer)
			g_main_loop_quit (mixer->loop);
			break;
		}
		case GST_MESSAGE_EOS:
		{
			if(mixer)
			g_main_loop_quit (mixer->loop);
			break;
		}
		default:
		break;
	}
  return TRUE;
}


