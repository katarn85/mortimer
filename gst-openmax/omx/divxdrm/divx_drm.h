/*
 * divx_drm
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef __DIVX_DRM_H__
#define __DIVX_DRM_H__
/*===========================================================================================
|                                                                                           |
|  INCLUDE FILES                                        |
|                                                                                           |
========================================================================================== */
#include "divx_drm_internal.h"
#ifdef __cplusplus
extern "C"
{
#endif
/*===========================================================================================
|                                                                                           |
|  GLOBAL DEFINITIONS AND DECLARATIONS                                        |
|                                                                                           |
========================================================================================== */
/**
 * DIVX_DRM_MAX_CAS_KEYS
 *
 * max size of DIVX DRM cas keys
 */
#define DIVX_DRM_MAX_CAS_KEYS 20
/**
 * DIVX_DRM_MAX_DRM_KEY_TABLE
 *
 * size of DIVX DRM key table
 */
#define DIVX_DRM_MAX_DRM_KEY_TABLE 2048
/**
 * DIVX_DRM_VIDEO_KEY_COUNT_MAX
 *
 * max size of DIVX DRM video key count
 */
#define DIVX_DRM_VIDEO_KEY_COUNT_MAX 128
/**
 * DIVX_DRM_VIDEO_KEY_SIZE_BYTES
 *
 * size of each DIVX DRM video key
 */
#define DIVX_DRM_VIDEO_KEY_SIZE_BYTES 16

/**
 * Enumerations of error code
 */
typedef enum
{
	DIVX_DRM_WARNING = 1,	/**< warning*/
	DIVX_DRM_ERROR_NONE = 0,	/**< Error none */
	DIVX_DRM_ERROR_UNKNOWN = -1	/**< Error unknown*/
}divx_drm_error_e;

/**
 * Enumerations of DRM type
 */
typedef enum
{
    DRM_TYPE_WMDRM = 0x0,
    DRM_TYPE_PLAYREADY = 0x1,
    DRM_TYPE_MARLIN = 0x2,
    DRM_TYPE_MARLINMS3 = 0x3,
    DRM_TYPE_MARLINBB = 0x4,
    DRM_TYPE_PLAYREADY2 = 0x5,
    DRM_TYPE_DVIX_VIDEO = 0x50,
    DRM_TYPE_DVIX_SAMSUNG_LEELEENAM = 0x51,
    DRM_TYPE_ICAS = 0x52,
    DRM_TYPE_IPLAYER = 0x53,
    DRM_TYPE_PVR_ECB = 0x54,
    DRM_TYPE_PVR_CBC = 0x55,
    DRM_TYPE_CORETRUST = 0x56,
    DRM_TYPE_ACTIVILLA = 0x57,
    DRM_TYPE_VERIMATRIX = 0x58,
    DRM_TYPE_HDCP = 0x59,
    DRM_TYPE_WIDI = 0x5A,
    DRM_TYPE_EXTERNAL_ICDI = 0x5B,
    DRM_TYPE_CORETRUST_LGU = 0x5C,
    DRM_TYPE_SCAS = 0x5D,
    DRM_TYPE_S4UD = 0x5E,
    DRM_TYPE_NONE = 0x7FFFFFFF
}drm_type_e;

/**
 * DIVX DRM cas info structure
 */
typedef struct cas_info
{
	unsigned int   pid;	/**< process id*/
	unsigned char key_even[16];	/**< even key*/
	unsigned char key_odd[16];	/**< odd key*/
	unsigned char iv[16];	/**< initialization vector*/
}cas_info_s;

/**
 * DIVX DRM out signal info structure
 */
typedef struct divx_drm_out_signal_info
{
	unsigned char cgmsa;	/**< cgmsa signal*/
	unsigned char acptb;	/**< acptb signal*/
	unsigned char hdcp;	/**< hdcp signal*/
	unsigned char ict;	/**< ict signal*/
}divx_drm_out_signal_info_s;

/**
 * DIVX DRM info structure
 */
typedef struct divx_drm_info
{
	drm_type_e drm_type;	/**< drm type*/

	unsigned int is_audio_proctected;	/**< audio protected flags*/
	unsigned int audio_decrypt_offset;	/**< audio encrypted data offset in the packet*/
	unsigned int audio_decrypt_size;	/**< audio encrypted data size*/
	unsigned char key_table[DIVX_DRM_MAX_DRM_KEY_TABLE];	/**< DRM key table*/

	unsigned int encrypted_type;	/**< encrypted type*/
	unsigned int encrypted_mode;	/**< encrypted mode*/

	unsigned int nb_keys;	/**< cas keys number*/
	cas_info_s keys[DIVX_DRM_MAX_CAS_KEYS];	/**< cas info keys*/

	unsigned char*  data;	/**< data packet pointer*/
	unsigned char 	drm_key[DIVX_DRM_VIDEO_KEY_SIZE_BYTES];	/**< DRM key*/
	int 	drm_key_index;	/**< index to get DRM key from DRM key table*/
	int 	drm_offset;	/**< encrypted data offset in the data packet*/
	int 	drm_length;	/**< encrypted data length in the data packet*/
	unsigned char* 	iv;	/**< initialization vector*/
	int	key_type;	/**< drm key type*/
	int 	left;	/**< left*/
	char * license_uri;	/**< license uri*/
}divx_drm_info_s;

/**
 * Pointer of divx_drm_manager structure
 */
typedef divx_drm_manager_s* divx_drm_manager_h;
/**
 * Pointer of divx_drm_out_signal_info structure
 */
typedef divx_drm_out_signal_info_s* divx_drm_out_signal_info_h;
/**
 * Pointer of divx_drm_info structure
 */
typedef divx_drm_info_s* divx_drm_info_h;

/*===========================================================================================
|                                                                                           |
|  GLOBAL FUNCTION PROTOTYPES                                        |
|                                                                                           |
========================================================================================== */

/**
 * This function can get handle of divx_drm_manager.
 *
 * @param	manager		[out]	Pointer of divx_drm_manager_h
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @pre
 * @post
 * @see
 * @remark	This method can be called to get handle of divx_drm_manager
 *
 * @par Example
 * @code
if (divx_drm_get_manager(&manager) != DIVX_DRM_ERROR_NONE)
{
	printf("failed to get divx drm manager\n");
}
 * @endcode
 */
int divx_drm_get_manager(divx_drm_manager_h* manager);

/**
 * This function can open the url and check whether it's a divx file, then initialize the DRM system.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 * @param	url			[in]	url of the divx drm file
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_open(manager,url) != DIVX_DRM_ERROR_NONE)
{
	printf("failed to open the file : %s \n",url);
}
 * @endcode
 */
int divx_drm_open(divx_drm_manager_h manager,const char* url);

/**
 * This function can close the divx drm file opened by divx_drm_open function and uninitialize the DRM system.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_close(manager) != DIVX_DRM_ERROR_NONE)
{
	printf("failed to close the divx drm file \n");
}
 * @endcode
 */
int divx_drm_close(divx_drm_manager_h manager);

/**
 * This function can get a registration code of the device.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 * @param	registration_code [out] registration code of the device
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_get_registration_code(manager,registration_code) != DIVX_DRM_ERROR_NONE)
{
	printf("failed to get registration code \n");
}
 * @endcode
 */
int divx_drm_get_registration_code(divx_drm_manager_h manager,char *registration_code);

/**
 * This function can parse the divx file and prepare for file decrypting.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 * @param	limit [out]	max playback count of rental file
 * @param	count [out] current playback count of rental file
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @pre	divx_drm_open
 * @post
 * @see	divx_drm_get_manager,divx_drm_open
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_decrypt_chunk(manager,&limit,&count) != DIVX_DRM_ERROR_NONE)
{
	printf("failed to decrypt chunk \n");
}
 * @endcode
 */
int divx_drm_decrypt_chunk( divx_drm_manager_h manager,int *limit, int *count );

/**
 * This function can deactivate the device.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 * @param	deactivation_code [out]	deactivation code of the device
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_deactivation(manager,deactivation_code) != DIVX_DRM_ERROR_NONE)
{
	printf("failed to deactivate the device \n");
}
 * @endcode
 */
int divx_drm_deactivation( divx_drm_manager_h manager,char *deactivation_code);

/**
 * This function can get frame key table.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 * @param	frame_keys [out]	frame key table
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_get_frame_keys(manager,frame_keys) != DIVX_DRM_ERROR_NONE)
{
	printf("failed to get frame keys\n");
}
 * @endcode
 */
int divx_drm_get_frame_keys(divx_drm_manager_h manager,unsigned char *frame_keys);

/**
 * This function can decrypt frame with SDK.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 * @param	keyhandle		[in]	frame key address
 * @param	buf [in]	encrypted buffer
 * @param	size		[in]	encrypted buffer size
 * @param	out_buf	[out]	decrypted buffer
 * @param	out_size		[out] decrypted buffer size


 *
 * @return	This function returns zero on success, or negative value with error code.
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_sw_decrypt_video(manager,**) != DIVX_DRM_ERROR_NONE)
{
	printf("failed to decrypt\n");
}
 * @endcode
 */

int divx_drm_sw_decrypt_video(divx_drm_manager_h manager,
						   unsigned char* keyhandle,
						   unsigned char* buf,
						   unsigned int size,
						   unsigned char* out_buf,
                           unsigned int out_size);


/**
 * This function can get decryption information.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 * @param	info		[out]	Handle of divx_drm_info
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_get_decryption_info(manager,info) != DIVX_DRM_ERROR_NONE)
{
	printf("failed to get decryption info\n");
}
 * @endcode
 */

int divx_drm_get_decryption_info(divx_drm_manager_h manager,divx_drm_info_h info);

/**
 * This function can get audio stream number.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 *
 * @return	This function returns audio stream number.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_get_audio_stream_num(manager) < 0)
{
	printf("failed to get audio stream num\n");
}
 * @endcode
 */
int divx_drm_get_audio_stream_num(divx_drm_manager_h manager);

/**
 * This function can get subtitle stream number.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 *
 * @return	This function returns subtitle stream number.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_get_subtitle_stream_num(manager) < 0)
{
	printf("failed to get subtitle stream num\n");
}
 * @endcode
 */
int divx_drm_get_subtitle_stream_num(divx_drm_manager_h manager);

/**
 * This function can get audio language trackID.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 * @param	track_ID	[in]	audio stream trackID
 *
 * @return	This function returns audio language trackID.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_get_audio_language(manager) < 0)
{
	printf("failed to get audio stream language trackID\n");
}
 * @endcode
 */
int divx_drm_get_audio_language(divx_drm_manager_h manager,int track_ID);

/**
 * This function can get subtitle language trackID.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 * @param	track_ID	[in]	subtitle stream trackID
 *
 * @return	This function returns subtitle language trackID.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_get_subtitle_language(manager) < 0)
{
	printf("failed to get subtitle stream language trackID\n");
}
 * @endcode
 */
int divx_drm_get_subtitle_language(divx_drm_manager_h manager,int track_ID);

/**
 * This function can get handle of audio stream.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 *
 * @return	This function returns handle of audio stream.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_get_audio_stream(manager) == NULL)
{
	printf("failed to get handle of audio stream\n");
}
 * @endcode
 */
int* divx_drm_get_audio_stream(divx_drm_manager_h manager);

/**
 * This function can get handle of subtitle stream.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 *
 * @return	This function returns handle of subtitle stream.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_get_subtitle_stream(manager) == NULL)
{
	printf("failed to get handle of subtitle stream\n");
}
 * @endcode
 */
int* divx_drm_get_subtitle_stream(divx_drm_manager_h manager);

/**
 * This function can check whether the device is activated.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 *
 * @return	This function returns whether the device is activated.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_is_activated(manager) != DIVX_DRM_ERROR_NONE)
{
	printf("the device is not activated\n");
}
 * @endcode
 */
int divx_drm_is_activated(divx_drm_manager_h manager);

/**
 * This function can check whether the device has activated history.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 *
 * @return	This function returns whether the device has activated history.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_is_having_activated_history(manager) != DIVX_DRM_ERROR_NONE)
{
	printf("the device has no activated history\n");
}
 * @endcode
 */
int divx_drm_is_having_activated_history(divx_drm_manager_h manager);

/**
 * This function can get out signal information.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 * @param	info		[out]	Handle of divx_drm_out_signal_info
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @pre
 * @post
 * @see	divx_drm_get_manager
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_get_out_signal_info(manager,info) != DIVX_DRM_ERROR_NONE)
{
	printf("failed to get out signal information\n");
}
 * @endcode
 */
int divx_drm_get_out_signal_info(divx_drm_manager_h manager,divx_drm_out_signal_info_h info);

/**
 * This function can decrypt video data.
 *
 * @param	manager		[in]	Handle of divx_drm_manager
 * @param	frame		[in,out]	in: handle of encrypted frame data ; out: handle of decrypted frame data
 * @param	frame_size	[in]	size of frame data
 *
 * @return	This function returns zero on success, or negative value with error code.
 * @pre	divx_drm_open,divx_drm_decrypt_chunk
 * @post
 * @see	divx_drm_get_manager,divx_drm_open,divx_drm_decrypt_chunk
 * @remark
 *
 * @par Example
 * @code
if (divx_drm_decrypt_video(manager,frame,frame_size) != DIVX_DRM_ERROR_NONE)
{
	printf("failed to decrypt video data\n");
}
 * @endcode
 */
int divx_drm_decrypt_video(divx_drm_manager_h manager,unsigned char *frame,unsigned int frame_size);
int divx_drm_get_activation_status(divx_drm_manager_h manager,unsigned char* user_ID,unsigned int* len);
int divx_drm_demux_add_extra_info(divx_drm_manager_h manager,unsigned char *dd_info,unsigned char **frame,unsigned int *frame_size);
int divx_drm_decoder_decrypt_video(divx_drm_manager_h manager,unsigned char *frame,unsigned int frame_size);
int divx_drm_decrypt_audio(divx_drm_manager_h manager,unsigned char *frame,unsigned int frame_size);
int divx_drm_decoder_commit(divx_drm_manager_h manager);
int divx_drm_debug_reset(divx_drm_manager_h manager);

#ifdef __cplusplus
}
#endif
#endif // __DIVX_DRM_H__

