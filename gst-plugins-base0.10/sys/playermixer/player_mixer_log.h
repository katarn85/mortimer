#ifndef __MIXER_LOG_H
#define __MIXER_LOG_H

#include "dlog.h"

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************
******************define, struct ,typedef, union, enum, global val *************************************
***********************************************************************/
#undef LOG_TAG
#define LOG_TAG "MIXER"
        
#ifndef _DBG
#define _DBG(fmt, args...) printf("[%s:%d] "fmt"\n", __func__, __LINE__, ##args)
#endif

#define MIXER_DBG(fmt, arg...)  LOGD("[%s:%d] "fmt ,__func__, __LINE__, ##arg);
#define MIXER_INFO(fmt, arg...) LOGI(FONT_COLOR_GREEN"[%s:%d] "fmt FONT_COLOR_RESET,__func__, __LINE__, ##arg);
#define MIXER_WARN(fmt, arg...) LOGW(FONT_COLOR_YELLOW"[%s:%d] "fmt FONT_COLOR_RESET,__func__, __LINE__, ##arg);
#define MIXER_ERR(fmt,arg...)   LOGE(FONT_COLOR_RED"[%s:%d] "fmt FONT_COLOR_RESET, __func__,  __LINE__, ##arg);

/* anci c color type */
#define FONT_COLOR_RESET    "\033[0m"
#define FONT_COLOR_RED      "\033[31m"
#define FONT_COLOR_GREEN    "\033[32m"
#define FONT_COLOR_YELLOW   "\033[33m"
#define FONT_COLOR_BLUE     "\033[34m"
#define FONT_COLOR_PURPLE   "\033[35m"
#define FONT_COLOR_CYAN     "\033[36m"
#define FONT_COLOR_GRAY     "\033[37m"

#define MIXER_DBG_INFO_RED(fmt, arg...) MIXER_DBG_INFO(FONT_COLOR_RED fmt FONT_COLOR_RESET, ##arg)
#define MIXER_DBG_INFO_GREEN(fmt, arg...) MIXER_DBG_INFO(FONT_COLOR_GREEN fmt FONT_COLOR_RESET, ##arg)
#define MIXER_DBG_INFO_YELLOW(fmt, arg...) MIXER_DBG_INFO(FONT_COLOR_YELLOW fmt FONT_COLOR_RESET, ##arg)
#define MIXER_DBG_INFO_BLUE(fmt, arg...) MIXER_DBG_INFO(FONT_COLOR_BLUE fmt FONT_COLOR_RESET, ##arg)
#define MIXER_DBG_INFO_PURPLE(fmt, arg...) MIXER_DBG_INFO(FONT_COLOR_PURPLE fmt FONT_COLOR_RESET, ##arg)
#define MIXER_DBG_INFO_CYAN(fmt, arg...) MIXER_DBG_INFO(FONT_COLOR_CYAN fmt FONT_COLOR_RESET, ##arg)
#define MIXER_DBG_INFO_GRAY(fmt, arg...) MIXER_DBG_INFO(FONT_COLOR_GRAY fmt FONT_COLOR_RESET, ##arg)
#define MIXER_DBG_INFO_WITH_COLOR(color, fmt, arg...) MIXER_DBG_INFO(color fmt FONT_COLOR_RESET, ##arg)

#ifdef __cplusplus
}
#endif    

#endif	//__MIXER_LOG_H
