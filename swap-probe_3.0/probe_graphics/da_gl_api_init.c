/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 *
 * Vitaliy Cherepanov <v.cherepanov@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */

#include <unistd.h>
#include "da_gles20.h"
#include "binproto.h"
#include "common_probe_init.h"
#include "real_functions.h"

/* GL __local_* functions prototypes */
#define X(func) extern void __local_##func(void);
#include "da_gl_api_func_list.h"
	GL_ALL_FUNCTIONS;
#undef X
/* --------------------------------- */


static bool __gl_api_initialized = 0;
Evas_GL_API *__gl_api = NULL;

void __init_gl_api__(void)
{
	if (__gl_api == NULL)
		__gl_api = real_malloc(sizeof(*__gl_api));
	memset(__gl_api, 0, sizeof(*__gl_api));
}

void save_orig_gl_api_list(Evas_GL_API *api)
{
	/* TODO make this check more pretty */
	if (__gl_api_initialized == 0) {
		memcpy(__gl_api, api, sizeof(*api));
		__gl_api_initialized = 1;
	}
}

/* IMPORTANT this code must be right before change_gl_api_list function!
 *
 * next define is GL api replacing by probe functions
 *
 */
#define X(func) \
	do {							\
		api->func = (typeof(api->func)) __local_##func;	\
		if (api->func == NULL)				\
			PRINTWRN("api->%s not setted", #func);	\
	} while(0);
#include "da_gl_api_func_list.h"
/* --------------------------------------------------------------------- */

void change_gl_api_list(Evas_GL_API *api)
{
	/* change links */
	GL_ALL_FUNCTIONS;
}

#undef X
