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

//#define EGL_TEST
Evas_GL_API *__gl_api;
void (*real_glGetVertexAttribfv)(GLuint index, GLenum pname, GLfloat *params);
void (*real_glGetIntegerv)(GLenum pname, GLint * params);
void (*real_glGetProgramiv)(GLuint program, GLenum pname, GLint *params);
void (*real_glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
void (*real_glGetActiveAttrib)(GLuint program, GLuint index, GLsizei bufSize,
			       GLsizei *length, GLint *size, GLenum *type,
			       char *name);
void (*real_glGetActiveUniform)(GLuint program, GLuint index, GLsizei bufSize,
				GLsizei *length, GLint *size, GLenum *type,
				char *name);
void (*real_glGetShaderSource)(GLuint shader, GLsizei bufSize, GLsizei *length,
			       char *source);
void (*real_glGetBufferParameteriv)(GLenum target, GLenum value, GLint *data);

void __gl_dummy_function()
{
	PRINTERR("call of dummy gl function!!!");
}

void set_real_func(const char *func_name, void **func_pointer,
		   ORIGINAL_LIBRARY id)
{
	void *faddr;
	void *_id;

	if (lib_handle[id] == NULL)
		lib_handle[id] = dlopen(lib_string[id], RTLD_LAZY);

	_id = lib_handle[id];

	if (_id == NULL)
		probe_terminate_with_err("dlopen failed", func_name, id);

	faddr = dlsym(_id, func_name);
	if (faddr == NULL || dlerror() != NULL) {
		PRINTWRN("[set_real_function] function <%s> not found in lib <%s>; dummy function will be seted",
			 func_name, lib_string[id]);
		faddr = __gl_dummy_function;
	}

	memcpy(func_pointer, &faddr, sizeof(faddr));
}

#define INIT_REAL_GL(func)\
	do {									\
		 set_real_func(#func, (void **)&real##_##func, LIBGLES20);	\
	} while (0)

int __init_gl_functions__(void)
{
	/* init gl API */
	__init_gl_api__();

	/* init real call functions */
	INIT_REAL_GL(glGetVertexAttribfv);
	INIT_REAL_GL(glGetProgramiv);
	INIT_REAL_GL(glGetShaderiv);
	INIT_REAL_GL(glGetActiveAttrib);
	INIT_REAL_GL(glGetActiveUniform);
	INIT_REAL_GL(glGetShaderSource);
	INIT_REAL_GL(glGetIntegerv);
	INIT_REAL_GL(glGetBufferParameteriv);

	return 0;
}

void probe_terminate_with_err(const char *msg, const char *func_name,
			      ORIGINAL_LIBRARY id)
{
	char error_msg[1024];
	const char *lib_name = "unknown";

	if (id != LIB_NO && id < NUM_ORIGINAL_LIBRARY)
		lib_name = lib_string[id];
	snprintf(error_msg, sizeof(error_msg), "%s : [%s], %s\n", msg,
		 func_name, lib_name);
	perror(error_msg);
	PRINTERR(error_msg);
	//wait for flush
	sleep(1);
	exit(0);
}

////////////////////////////////////////////////////////////////////////////
//egl init probe function
//  params:
//    func_name    - original function name for dlsym search
//    func_pointer - original function pointer (return)
//
//  info
//    search original function by name
//    function have no return becouse on error it terminates main application
#ifdef EGL_TEST

void dummy()
{
	return;
}

void init_probe_egl(__attribute__ ((unused))const char *func_name, void **func_pointer,
		    __attribute__ ((unused))ORIGINAL_LIBRARY id)
{
	PRINTMSG(func_name);
	*func_pointer = (void *)dummy;
}
#else
void init_probe_egl(const char *func_name, void **func_pointer,
		    ORIGINAL_LIBRARY id)
{
	void *faddr = 0;

	(gProbeBlockCount++);
	if (lib_handle[id] == ((void *)0)) {
		lib_handle[id] = dlopen(lib_string[id],
					RTLD_LAZY | RTLD_GLOBAL);

		if (lib_handle[id] == ((void *)0))
			probe_terminate_with_err("dlopen failed", func_name, id);
	};
	faddr = dlsym(lib_handle[id], func_name);
	if (faddr == NULL || dlerror() != NULL)
		probe_terminate_with_err("dlsym failed", func_name, id);
	memcpy(func_pointer, &faddr, sizeof(faddr));
	(gProbeBlockCount--);
}
#endif

void init_probe_gl(const char *func_name, void **func_pointer,
		   ORIGINAL_LIBRARY id, int blockresult, int32_t vAPI_ID)
{
	void *faddr;
	probeInfo_t tempProbeInfo;

	probeBlockStart();
	if (lib_handle[id] == ((void *)0)) {
		lib_handle[id] = dlopen(lib_string[id], RTLD_LAZY);
		if (lib_handle[id] == ((void *)0))
			probe_terminate_with_err("dlopen failed", func_name,
						 id);

		setProbePoint(&tempProbeInfo);
		if (blockresult != 0) {
			/* get max value */
			char maxValString[64];
			GLint maxVal[2];
			real_glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVal[0]);
			real_glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
					   &maxVal[1]);
			snprintf(maxValString, sizeof(maxValString), "%d,%d", maxVal[0], maxVal[1]);
			PREPARE_LOCAL_BUF();
			PACK_COMMON_BEGIN(MSG_PROBE_GL, vAPI_ID, "", 0);
			PACK_COMMON_END('p', 1, 0, 0);
			PACK_GL_ADD(APITYPE_INIT, 0, maxValString);
			FLUSH_LOCAL_BUF();
		}
	}

	faddr = dlsym(lib_handle[id], func_name);
	if (faddr == NULL || dlerror() != NULL)
		probe_terminate_with_err("function not found in lib", func_name,
					 id);

	memcpy(func_pointer, &faddr, sizeof(faddr));
	probeBlockEnd();
}
