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
#ifndef __COMMON_PROBE_INIT_H__
#define __COMMON_PROBE_INIT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "dahelper.h"
#include "da_gles20.h"
////////////////////////////////////////////////////////////////////////////
//egl init probe function
//  params:
//    func_name    - original function name for dlsym search
//    func_pointer - original function pointer (return)
//
//  info
//    search original function by name
//    function have no return becouse on error it terminates main application
extern void init_probe_egl(const char *func_name, void **func_pointer,
			   ORIGINAL_LIBRARY id);

extern void init_probe_gl(const char *func_name, void **func_pointer,
			  ORIGINAL_LIBRARY id, int blockresult, int32_t vAPI_ID);

extern void probe_terminate_with_err(const char *msg, const char *func_name,
				     ORIGINAL_LIBRARY id);

void __init_gl_api__(void);
int __init_gl_functions__(void);

/* links to real functions to call in probes */
extern void (*real_glGetVertexAttribfv)(GLuint index, GLenum pname,
					GLfloat *params);
extern void (*real_glGetIntegerv)(GLenum pname, GLint * params);
extern void (*real_glGetProgramiv)(GLuint program, GLenum pname, GLint *params);
extern void (*real_glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
extern void (*real_glGetActiveAttrib)(GLuint program, GLuint index,
				      GLsizei bufSize, GLsizei *length,
				      GLint *size, GLenum *type, char *name);
extern void (*real_glGetActiveUniform)(GLuint program, GLuint index,
				       GLsizei bufSize, GLsizei *length,
				       GLint *size, GLenum *type, char *name);
extern void (*real_glGetShaderSource)(GLuint shader, GLsizei bufSize,
				      GLsizei *length, char *source);
extern void (*real_glGetBufferParameteriv)(GLenum target, GLenum value,
					   GLint *data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __COMMON_PROBE_INIT_H__ */
