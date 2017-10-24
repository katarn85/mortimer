#ifndef __TIZEN_PLYAER_MIXER_PRIVATE_H__
#define	__TIZEN_PLAYER_MIXER_PRIVATE_H__

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gst/gst.h> 

G_BEGIN_DECLS

typedef struct mixer_s* mixer_h;
typedef enum
{
	PLAYER_MIXER_ERROR_NONE,			
	PLAYER_MIXER_ERROR	
} player_mixer_error_e;
//For APPcreatec

int player_mixer_create(mixer_h* mixer, void* display_handle);
void player_mixer_set_position(void* player);
void player_mixer_set_display_area(mixer_h* mixer,int* display_handle, int x, int y, int w,int h);
int player_mixer_destory(mixer_h* mixer);

//For MixerSink
void player_mixer_mixing(int displayhandle, GstBuffer* buf);
int player_mixer_get_disp(Display** disp);
void player_mixer_connect(int* handle);
void player_mixer_change_state(GstState state);
void player_mixer_set_property(int scaler_id);

G_END_DECLS
#endif //__TIZEN_MEDIA_PLAYER_PRIVATE_H__
