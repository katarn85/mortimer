/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 *
 * Sanghyun Lee <sanghyunnim.lee@samsung.com>
 * Juyoung Kim <j0.kim@samsung.com>
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
 * - Samsung RnD Institute Russia
 *
 */

#include "da_gles20.h"
#include "daprobe.h"
#include "binproto.h"
#include "real_functions.h"
#include "common_probe_init.h"

#ifndef REAL_NAME
	#define REAL_NAME(func) func
#endif

#ifndef CALL_ORIG
	#define CALL_ORIG(func, ...) func##p(__VA_ARGS__)
#endif

#ifndef BEFORE
	#define BEFORE BEFORE_GL_ORIG
#endif

#ifndef TYPEDEF
	#define TYPEDEF(type) typedef type
#endif

static char contextValue[MAX_GL_CONTEXT_VALUE_SIZE]; /* maybe it should not be gobal static variable */
static enum DaOptions _sopt = OPT_GLES;
static __thread GLenum gl_error_external = GL_NO_ERROR;

static void __ui_array_to_str(char *to, GLuint *arr ,int count, size_t bufsize)
{
	int i = 0, len = 0;

	for (i = 0; i < count; i++) {
		if (bufsize < sizeof(GLuint) * 4) {
			PRINTERR("too small buffer.");
			break;
		}

		len = snprintf(to, bufsize,"%u, ", *arr++);

		to += len;
		bufsize -= len;
	}

	if (count != 0) {
		to -= 2;
		*to = '\0';
	}
}

GLenum REAL_NAME(glGetError)(void)
{
	TYPEDEF(GLenum (*methodType)(void));
	BEFORE(glGetError);
	GLenum ret = CALL_ORIG(glGetError,);

	if (gl_error_external == GL_NO_ERROR)
		gl_error_external = ret;

	if (blockresult) {
		//external call
		ret = gl_error_external;
		gl_error_external = GL_NO_ERROR;
	}

	AFTER_NO_PARAM('d', ret, APITYPE_CONTEXT, "");

	return ret;
}

// ==================================================================
// A 2
// ==================================================================

void REAL_NAME(glActiveTexture)(GLenum texture)
{
	TYPEDEF(void (*methodType)(GLenum));
	BEFORE(glActiveTexture);
	CALL_ORIG(glActiveTexture, texture);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x",
	      (uint64_t)(texture));
}

void REAL_NAME(glAttachShader)(GLuint program, GLuint shader)
{
	TYPEDEF(void (*methodType)(GLuint, GLuint));
	BEFORE(glAttachShader);
	CALL_ORIG(glAttachShader, program, shader);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dd", program, shader);
}

// ==================================================================
// B 12
// ==================================================================
void __get_context_buf_data(GLenum target, char *buf, int buf_size)
{
	GLint n_buffer_size, n_buffer_usage_size;
	int print_size;

	if (buf == NULL)
		return;

	real_glGetBufferParameteriv(target, GL_BUFFER_SIZE,
				    &n_buffer_size);
	real_glGetBufferParameteriv(target, GL_BUFFER_USAGE,
				    &n_buffer_usage_size);

	print_size = snprintf(buf, buf_size, "%u,%u,%u",
			      target, n_buffer_size, n_buffer_usage_size);

	if (print_size >= buf_size) {
		/* data was truncated. so data is invalid */
		buf[0]='\0';
	}

}

void REAL_NAME(glBindAttribLocation)(GLuint program, GLuint index, const char *name)
{
	TYPEDEF(void (*methodType)(GLuint, GLuint, const char *));
	BEFORE(glBindAttribLocation);
	CALL_ORIG(glBindAttribLocation, program, index, name);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dds",
	      program, index, name);
}

void REAL_NAME(glBindBuffer)(GLenum target, GLuint buffer)
{
	char context_value[MAX_GL_CONTEXT_VALUE_SIZE] = {0,};

	TYPEDEF(void (*methodType)(GLenum, GLuint));
	BEFORE(glBindBuffer);
	CALL_ORIG(glBindBuffer, target, buffer);

	if (blockresult && buffer)
		__get_context_buf_data(target, &context_value[0],
				       sizeof(context_value));

	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, context_value, "xd",
	      (uint64_t)(target), buffer);
}

void REAL_NAME(glBindFramebuffer)(GLenum target, GLuint framebuffer)
{
	TYPEDEF(void (*methodType)(GLenum, GLuint));
	BEFORE(glBindFramebuffer);
	CALL_ORIG(glBindFramebuffer, target, framebuffer);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xd",
	      (uint64_t)(target), framebuffer);
}

void REAL_NAME(glBindRenderbuffer)(GLenum target, GLuint renderbuffer)
{
	TYPEDEF(void (*methodType)(GLenum, GLuint));
	BEFORE(glBindRenderbuffer);
	CALL_ORIG(glBindRenderbuffer, target, renderbuffer);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xd",
	      (uint64_t)(target), renderbuffer);
}

void REAL_NAME(glBindTexture)(GLenum target, GLuint texture)
{
	TYPEDEF(void (*methodType)(GLenum, GLuint));
	BEFORE(glBindTexture);
	CALL_ORIG(glBindTexture, target, texture);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xd",
	      (uint64_t)(target), texture);
}

void REAL_NAME(glBlendColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	TYPEDEF(void (*methodType)(GLclampf, GLclampf, GLclampf, GLclampf));
	BEFORE(glBlendColor);
	CALL_ORIG(glBlendColor, red, green, blue, alpha);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ffff",
	      red, green, blue, alpha);
}

void REAL_NAME(glBlendEquation)(GLenum mode)
{
	TYPEDEF(void (*methodType)(GLenum));
	BEFORE(glBlendEquation);
	CALL_ORIG(glBlendEquation, mode);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x",
	      (uint64_t)(mode));
}

void REAL_NAME(glBlendEquationSeparate)(GLenum modeRGB, GLenum modeAlpha)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum));
	BEFORE(glBlendEquationSeparate);
	CALL_ORIG(glBlendEquationSeparate, modeRGB, modeAlpha);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xx",
	      (uint64_t)(modeRGB), (uint64_t)(modeAlpha));
}

void REAL_NAME(glBlendFunc)(GLenum sfactor, GLenum dfactor)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum));
	BEFORE(glBlendFunc);
	CALL_ORIG(glBlendFunc, sfactor, dfactor);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xx",
	      (uint64_t)(sfactor), (uint64_t)(dfactor));
}

void REAL_NAME(glBlendFuncSeparate)(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha,
			 GLenum dstAlpha)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLenum, GLenum));
	BEFORE(glBlendFuncSeparate);
	CALL_ORIG(glBlendFuncSeparate, srcRGB, dstRGB, srcAlpha, dstAlpha);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xxxx",
	      (uint64_t)(srcRGB), (uint64_t)(dstRGB),
	      (uint64_t)(srcAlpha), (uint64_t)(dstAlpha));
}

void REAL_NAME(glBufferData)(GLenum target, GLsizeiptr size, const GLvoid * data,
		  GLenum usage)
{
	TYPEDEF(void (*methodType)(GLenum, GLsizeiptr, const GLvoid *, GLenum));
	BEFORE(glBufferData);
	CALL_ORIG(glBufferData, target, size, data, usage);

	char context_value[MAX_GL_CONTEXT_VALUE_SIZE];
	if (blockresult)
		__get_context_buf_data(target, &context_value[0],
				       sizeof(context_value));
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, context_value, "xxpx",
	      (uint64_t)(target), (uint64_t)(size),
	      voidp_to_uint64(data), (uint64_t)(usage));
}

void REAL_NAME(glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size,
		     const GLvoid * data)
{
	TYPEDEF(void (*methodType)(GLenum, GLintptr, GLsizeiptr,
				   const GLvoid *));
	BEFORE(glBufferSubData);

	char context_value[MAX_GL_CONTEXT_VALUE_SIZE];
	if (blockresult)
		__get_context_buf_data(target, &context_value[0],
				       sizeof(context_value));
	CALL_ORIG(glBufferSubData, target, offset, size, data);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, context_value, "xxxp",
	      (uint64_t)(target), (uint64_t)(offset),
	      (uint64_t)(size), voidp_to_uint64(data));
}

// ==================================================================
// C 14
// ==================================================================

GLenum REAL_NAME(glCheckFramebufferStatus)(GLenum target)
{
	TYPEDEF(GLenum (*methodType)(GLenum));
	BEFORE(glCheckFramebufferStatus);
	GLenum ret = CALL_ORIG(glCheckFramebufferStatus, target);
	GL_GET_ERROR();
	AFTER('d', ret, APITYPE_CONTEXT, "", "x",
	      (uint64_t)(target));

	return ret;
}

void REAL_NAME(glClear)(GLbitfield mask)
{
	TYPEDEF(void (*methodType)(GLbitfield));
	BEFORE(glClear);
	CALL_ORIG(glClear, mask);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x",
	      (uint64_t)(mask));
}

void REAL_NAME(glClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	TYPEDEF(void (*methodType)(GLclampf, GLclampf, GLclampf, GLclampf));
	BEFORE(glClearColor);
	CALL_ORIG(glClearColor, red, green, blue, alpha);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ffff",
	      red, green, blue, alpha);
}

void REAL_NAME(glClearDepthf)(GLclampf depth)
{
	TYPEDEF(void (*methodType)(GLclampf));
	BEFORE(glClearDepthf);
	CALL_ORIG(glClearDepthf, depth);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "f", depth);
}

void REAL_NAME(glClearStencil)(GLint s)
{
	TYPEDEF(void (*methodType)(GLint));
	BEFORE(glClearStencil);
	CALL_ORIG(glClearStencil, s);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "d", s);
}

void REAL_NAME(glColorMask)(GLboolean red, GLboolean green, GLboolean blue,
		 GLboolean alpha)
{
	TYPEDEF(void (*methodType)(GLboolean, GLboolean, GLboolean, GLboolean));
	BEFORE(glColorMask);
	CALL_ORIG(glColorMask, red, green, blue, alpha);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dddd",
	      red, green, blue, alpha);
}

void REAL_NAME(glCompileShader)(GLuint shader)
{
	TYPEDEF(void (*methodType)(GLuint));
	BEFORE(glCompileShader);
	CALL_ORIG(glCompileShader, shader);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x",
	      (uint64_t)(shader));
}

void REAL_NAME(glCompressedTexImage2D)(GLenum target, GLint level, GLenum internalformat,
			    GLsizei width, GLsizei height, GLint border,
			    GLsizei imageSize, const GLvoid * data)
{
	TYPEDEF(void (*methodType)(GLenum, GLint, GLenum, GLsizei, GLsizei,
				   GLint, GLsizei, const GLvoid *));
	BEFORE(glCompressedTexImage2D);
	CALL_ORIG(glCompressedTexImage2D, target, level, internalformat, width,
		  height, border, imageSize, data);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xdxddddp",
	      (uint64_t)(target), level,
	      (uint64_t)(internalformat), width, height, border, imageSize,
	      voidp_to_uint64(data));
}

void REAL_NAME(glCompressedTexSubImage2D)(GLenum target, GLint level, GLint xoffset,
			       GLint yoffset, GLsizei width, GLsizei height,
			       GLenum format, GLsizei imageSize,
			       const GLvoid * data)
{
	TYPEDEF(void (*methodType)(GLenum, GLint, GLint, GLint, GLsizei,
				   GLsizei, GLenum, GLsizei, const GLvoid *));
	BEFORE(glCompressedTexSubImage2D);
	CALL_ORIG(glCompressedTexSubImage2D, target, level, xoffset, yoffset,
		  width, height, format, imageSize, data);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xdddddxdp",
	      (uint64_t)(target), level, xoffset, yoffset, width, height,
	      (uint64_t)(format), imageSize, voidp_to_uint64(data));
}

void REAL_NAME(glCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat,
		      GLint x, GLint y, GLsizei width, GLsizei height,
		      GLint border)
{
	TYPEDEF(void (*methodType)(GLenum, GLint, GLenum, GLint, GLint,
				   GLsizei, GLsizei, GLint));
	BEFORE(glCopyTexImage2D);
	CALL_ORIG(glCopyTexImage2D, target, level, internalformat, x, y, width,
		  height, border);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xdxddddd",
	      (uint64_t)(target), level,
	      (uint64_t)(internalformat), x, y, width, height, border);
}

void REAL_NAME(glCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset,
			 GLint yoffset, GLint x, GLint y, GLsizei width,
			 GLsizei height)
{
	TYPEDEF(void (*methodType)(GLenum, GLint, GLint, GLint, GLint, GLint,
				   GLsizei, GLsizei));
	BEFORE(glCopyTexSubImage2D);
	CALL_ORIG(glCopyTexSubImage2D, target, level, xoffset, yoffset, x, y,
		  width, height);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xddddddd",
	      (uint64_t)(target), level, xoffset, yoffset, x, y, width,
	      height);
}

GLuint REAL_NAME(glCreateProgram)(void)
{
	TYPEDEF(GLuint (*methodType)(void));
	BEFORE(glCreateProgram);
	GLuint ret = CALL_ORIG(glCreateProgram,);
	GL_GET_ERROR();
	AFTER_NO_PARAM('d', ret, APITYPE_CONTEXT, "");

	return ret;
}

GLuint REAL_NAME(glCreateShader)(GLenum shaderType)
{
	TYPEDEF(GLuint (*methodType)(GLenum));
	BEFORE(glCreateShader);
	GLuint ret = CALL_ORIG(glCreateShader, shaderType);
	GL_GET_ERROR();
	AFTER('d', ret, APITYPE_CONTEXT, "", "x", (uint64_t)(shaderType));

	return ret;
}

void REAL_NAME(glCullFace)(GLenum mode)
{
	TYPEDEF(void (*methodType)(GLenum));
	BEFORE(glCullFace);
	CALL_ORIG(glCullFace, mode);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x",
	      (uint64_t)(mode));
}

// ==================================================================
// D 14
// ==================================================================

void REAL_NAME(glDeleteBuffers)(GLsizei n, const GLuint * buffers)
{
	TYPEDEF(void (*methodType)(GLsizei, const GLuint *));
	BEFORE(glDeleteBuffers);

	char context_value[MAX_GL_CONTEXT_VALUE_SIZE];
	CALL_ORIG(glDeleteBuffers, n, buffers);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, context_value, "dp",
	      n, voidp_to_uint64(buffers));
}

void REAL_NAME(glDeleteFramebuffers)(GLsizei n, const GLuint * framebuffers)
{
	TYPEDEF(void (*methodType)(GLsizei, const GLuint *));
	BEFORE(glDeleteFramebuffers);
	CALL_ORIG(glDeleteFramebuffers, n, framebuffers);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dp",
	      n, voidp_to_uint64(framebuffers));
}

void REAL_NAME(glDeleteProgram)(GLuint program)
{
	TYPEDEF(void (*methodType)(GLuint));
	BEFORE(glDeleteProgram);
	CALL_ORIG(glDeleteProgram, program);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "d", program);
}

void REAL_NAME(glDeleteRenderbuffers)(GLsizei n, const GLuint * renderbuffers)
{
	TYPEDEF(void (*methodType)(GLsizei, const GLuint *));
	BEFORE(glDeleteRenderbuffers);
	CALL_ORIG(glDeleteRenderbuffers, n, renderbuffers);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dp",
	      n, voidp_to_uint64(renderbuffers));
}

void REAL_NAME(glDeleteShader)(GLuint shader)
{
	TYPEDEF(void (*methodType)(GLuint));
	BEFORE(glDeleteShader);
	CALL_ORIG(glDeleteShader, shader);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "d", shader);
}

void REAL_NAME(glDeleteTextures)(GLsizei n, const GLuint *textures)
{
	TYPEDEF(void (*methodType)(GLsizei, const GLuint *));
	char buf[128] = "";

	BEFORE(glDeleteTextures);
	CALL_ORIG(glDeleteTextures, n, textures);
	GL_GET_ERROR();
	if (error == GL_NO_ERROR)
		__ui_array_to_str(buf, (GLuint *)textures, n, sizeof(buf));

	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, buf, "dp",
	      n, voidp_to_uint64(textures));
}

void REAL_NAME(glDepthFunc)(GLenum func)
{
	TYPEDEF(void (*methodType)(GLenum));
	BEFORE(glDepthFunc);
	CALL_ORIG(glDepthFunc, func);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x",
	      (uint64_t)(func));
}

void REAL_NAME(glDepthMask)(GLboolean flag)
{
	TYPEDEF(void (*methodType)(GLboolean));
	BEFORE(glDepthMask);
	CALL_ORIG(glDepthMask, flag);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x",
	      (uint64_t)(flag));
}

void REAL_NAME(glDepthRangef)(GLclampf nearVal, GLclampf farVal)
{
	TYPEDEF(void (*methodType)(GLclampf, GLclampf));
	BEFORE(glDepthRangef);
	CALL_ORIG(glDepthRangef, nearVal, farVal);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ff",
	      nearVal, farVal);
}

void REAL_NAME(glDetachShader)(GLuint program, GLuint shader)
{
	TYPEDEF(void (*methodType)(GLuint, GLuint));
	BEFORE(glDetachShader);
	CALL_ORIG(glDetachShader, program, shader);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dd",
	      program, shader);
}

void REAL_NAME(glDisable)(GLenum cap)
{
	TYPEDEF(void (*methodType)(GLenum));
	BEFORE(glDisable);
	CALL_ORIG(glDisable, cap);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x", (uint64_t)(cap));
}

void REAL_NAME(glDisableVertexAttribArray)(GLuint index)
{
	TYPEDEF(void (*methodType)(GLuint));
	BEFORE(glDisableVertexAttribArray);
	CALL_ORIG(glDisableVertexAttribArray, index);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "d", index);
}

void REAL_NAME(glDrawArrays)(GLenum mode, GLint first, GLsizei count)
{
	TYPEDEF(void (*methodType)(GLenum, GLint, GLsizei));
	BEFORE(glDrawArrays);
	CALL_ORIG(glDrawArrays, mode, first, count);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xdd",
	      (uint64_t)(mode), first, count);
}

void REAL_NAME(glDrawElements)(GLenum mode, GLsizei count, GLenum type,
		const GLvoid *indices)
{
	TYPEDEF(void (*methodType)(GLenum, GLsizei, GLenum, const GLvoid *));
	BEFORE(glDrawElements);
	CALL_ORIG(glDrawElements, mode, count, type, indices);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xdxp",
	      (uint64_t)(mode), count, (uint64_t)(type), indices);
}

// ==================================================================
// E 2
// ==================================================================

void REAL_NAME(glEnable)(GLenum cap)
{
	TYPEDEF(void (*methodType)(GLenum));
	BEFORE(glEnable);
	CALL_ORIG(glEnable, cap);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x", (uint64_t)(cap));
}

void REAL_NAME(glEnableVertexAttribArray)(GLuint index)
{
	TYPEDEF(void (*methodType)(GLuint));
	BEFORE(glEnableVertexAttribArray);
	CALL_ORIG(glEnableVertexAttribArray, index);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "d", index);
}

// ==================================================================
// F 5
// ==================================================================

void REAL_NAME(glFinish)(void)
{
	TYPEDEF(void (*methodType)(void));
	BEFORE(glFinish);
	CALL_ORIG(glFinish,);
	GL_GET_ERROR();
	AFTER_NO_PARAM('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "");
}

void REAL_NAME(glFlush)(void)
{
	TYPEDEF(void (*methodType)(void));
	BEFORE(glFlush);
	CALL_ORIG(glFlush,);
	GL_GET_ERROR();
	AFTER_NO_PARAM('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "");
}

void REAL_NAME(glFramebufferRenderbuffer)(GLenum target, GLenum attachment,
			       GLenum renderbuffertarget, GLuint renderbuffer)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLenum, GLuint));
	BEFORE(glFramebufferRenderbuffer);
	CALL_ORIG(glFramebufferRenderbuffer, target, attachment,
		  renderbuffertarget, renderbuffer);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xxxd",
	      (uint64_t)(target), (uint64_t)(attachment),
	      (uint64_t)(renderbuffertarget), renderbuffer);
}

void REAL_NAME(glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget,
			    GLuint texture, GLint level)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLenum, GLuint, GLint));
	BEFORE(glFramebufferTexture2D);
	CALL_ORIG(glFramebufferTexture2D, target, attachment, textarget,
		  texture, level);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xxxdd",
	      (uint64_t)(target), (uint64_t)(attachment),
	      (uint64_t)(textarget), texture, level);
}

void REAL_NAME(glFrontFace)(GLenum mode)
{
	TYPEDEF(void (*methodType)(GLenum));
	BEFORE(glFrontFace);
	CALL_ORIG(glFrontFace, mode);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x",
	      (uint64_t)(mode));
}

// ==================================================================
// G 31
// ==================================================================

void REAL_NAME(glGenBuffers)(GLsizei n, GLuint * buffers)
{
	TYPEDEF(void (*methodType)(GLsizei, GLuint *));
	char buf[128] = "";

	BEFORE(glGenBuffers);
	CALL_ORIG(glGenBuffers, n, buffers);
	GL_GET_ERROR();
	if (error == GL_NO_ERROR)
		__ui_array_to_str(buf, buffers, n, sizeof(buf));

	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, buf, "dp",
	      n, voidp_to_uint64(buffers));
}

void REAL_NAME(glGenFramebuffers)(GLsizei n, GLuint * framebuffers)
{
	TYPEDEF(void (*methodType)(GLsizei, GLuint *));
	BEFORE(glGenFramebuffers);
	CALL_ORIG(glGenFramebuffers, n, framebuffers);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dp",
	      n, voidp_to_uint64(framebuffers));
}

void REAL_NAME(glGenRenderbuffers)(GLsizei n, GLuint * renderbuffers)
{
	TYPEDEF(void (*methodType)(GLsizei, GLuint *));
	BEFORE(glGenRenderbuffers);
	CALL_ORIG(glGenRenderbuffers, n, renderbuffers);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dp",
	      n, voidp_to_uint64(renderbuffers));
}

void REAL_NAME(glGenTextures)(GLsizei n, GLuint * textures)
{
	TYPEDEF(void (*methodType)(GLsizei, GLuint *));
	char buf[128] = "";

	BEFORE(glGenTextures);
	CALL_ORIG(glGenTextures, n, textures);
	GL_GET_ERROR();
	if (error == GL_NO_ERROR)
		__ui_array_to_str(buf, textures, n, sizeof(buf));

	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, buf, "dp",
	      n, voidp_to_uint64(textures));
}

void REAL_NAME(glGenerateMipmap)(GLenum target)
{
	TYPEDEF(void (*methodType)(GLenum));
	BEFORE(glGenerateMipmap);
	CALL_ORIG(glGenerateMipmap, target);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x",
	      (uint64_t)(target));
}

//lsh_get
void REAL_NAME(glGetBooleanv)(GLenum pname, GLboolean * params)
{
	TYPEDEF(void (*methodType)(GLenum, GLboolean *));
	BEFORE(glGetBooleanv);
	CALL_ORIG(glGetBooleanv, pname, params);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x",
	      (uint64_t)(pname));
}

//lsh_get
void REAL_NAME(glGetFloatv)(GLenum pname, GLfloat * params)
{
	TYPEDEF(void (*methodType)(GLenum, GLfloat *));
	BEFORE(glGetFloatv);
	CALL_ORIG(glGetFloatv, pname, params);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x",
	      (uint64_t)(pname));
}

//lsh_get
void REAL_NAME(glGetIntegerv)(GLenum pname, GLint * params)
{
	TYPEDEF(void (*methodType)(GLenum, GLint *));
	BEFORE(glGetIntegerv);
	CALL_ORIG(glGetIntegerv, pname, params);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "x",
	      (uint64_t)(pname));
}

//lsh_get
void REAL_NAME(glGetActiveAttrib)(GLuint program, GLuint index, GLsizei bufSize,
		       GLsizei *length, GLint *size, GLenum *type, char *name)
{
	TYPEDEF(void (*methodType)(GLuint, GLuint, GLsizei, GLsizei *, GLint *,
				   GLenum *, char *));
	BEFORE(glGetActiveAttrib);
	CALL_ORIG(glGetActiveAttrib, program, index, bufSize, length, size,
		  type, name);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddd",
	      program, index, bufSize);
}

//lsh_get
void REAL_NAME(glGetActiveUniform)(GLuint program, GLuint index, GLsizei bufSize,
			GLsizei *length, GLint *size, GLenum *type, char *name)
{
	TYPEDEF(void (*methodType)(GLuint, GLuint, GLsizei, GLsizei *, GLint *,
				   GLenum *, char *));
	BEFORE(glGetActiveUniform);
	CALL_ORIG(glGetActiveUniform, program, index, bufSize, length, size,
		  type, name);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddd",
	      program, index, bufSize);
}

//lsh_get
void REAL_NAME(glGetAttachedShaders)(GLuint program, GLsizei maxCount, GLsizei *count,
			  GLuint *shaders)
{
	TYPEDEF(void (*methodType)(GLuint, GLsizei, GLsizei *, GLuint *));
	BEFORE(glGetAttachedShaders);
	CALL_ORIG(glGetAttachedShaders, program, maxCount, count, shaders);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dd",
	      program, maxCount);
}

//lsh_get
int REAL_NAME(glGetAttribLocation)(GLuint program, const char *name)
{
	TYPEDEF(int (*methodType)(GLuint, const char*));
	BEFORE(glGetAttribLocation);
	int ret = CALL_ORIG(glGetAttribLocation, program, name);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xs",
	      (uint64_t)(program), name);
	return ret;
}

//lsh_get
void REAL_NAME(glGetBufferParameteriv)(GLenum target, GLenum value, GLint * data)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLint *));
	BEFORE(glGetBufferParameteriv);
	CALL_ORIG(glGetBufferParameteriv, target, value, data);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xx",
	      (uint64_t)(target), (uint64_t)(value));
}

//lsh_get
void REAL_NAME(glGetFramebufferAttachmentParameteriv)(GLenum target, GLenum attachment,
					   GLenum pname, GLint * params)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLenum, GLint *));
	BEFORE(glGetFramebufferAttachmentParameteriv);
	CALL_ORIG(glGetFramebufferAttachmentParameteriv, target, attachment,
		  pname, params);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xxx",
	      (uint64_t)(target), (uint64_t)(attachment), (uint64_t)(pname));
}

//lsh_get
void REAL_NAME(glGetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei *length,
			 char *infoLog)
{
	TYPEDEF(void (*methodType)(GLuint, GLsizei, GLsizei *, char *));
	BEFORE(glGetProgramInfoLog);
	CALL_ORIG(glGetProgramInfoLog, program, maxLength, length, infoLog);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dd",
	      program, maxLength);
}

//lsh_get
void REAL_NAME(glGetProgramiv)(GLuint program, GLenum pname, GLint *params)
{
	TYPEDEF(void (*methodType)(GLuint, GLenum, GLint *));
	BEFORE(glGetProgramiv);
	CALL_ORIG(glGetProgramiv, program, pname, params);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dx",
	      program, (uint64_t)(pname));
}

//lsh_get
void REAL_NAME(glGetRenderbufferParameteriv)(GLenum target, GLenum pname, GLint *params)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLint *));
	BEFORE(glGetRenderbufferParameteriv);
	CALL_ORIG(glGetRenderbufferParameteriv, target, pname, params);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xx",
	      (uint64_t)(target), (uint64_t)(pname));
}

//lsh_get
void REAL_NAME(glGetShaderInfoLog)(GLuint shader, GLsizei maxLength, GLsizei *length,
			char *infoLog)
{
	TYPEDEF(void (*methodType)(GLuint, GLsizei, GLsizei *, char *));
	BEFORE(glGetShaderInfoLog);
	CALL_ORIG(glGetShaderInfoLog, shader, maxLength, length, infoLog);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dd",
	      shader, maxLength);
}

//lsh_get
void REAL_NAME(glGetShaderPrecisionFormat)(GLenum shaderType, GLenum precisionType,
				GLint *range, GLint *precision)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLint *, GLint *));
	BEFORE(glGetShaderPrecisionFormat);
	CALL_ORIG(glGetShaderPrecisionFormat, shaderType, precisionType, range,
		  precision);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xx",
	      (uint64_t)(shaderType), (uint64_t)(precisionType));
}

//lsh_get
void REAL_NAME(glGetShaderSource)(GLuint shader, GLsizei bufSize, GLsizei *length,
		       char *source)
{
	TYPEDEF(void (*methodType)(GLuint, GLsizei, GLsizei *, char *));
	BEFORE(glGetShaderSource);
	CALL_ORIG(glGetShaderSource, shader, bufSize, length, source);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dd",
	      shader, bufSize);
}

//lsh_get
void REAL_NAME(glGetShaderiv)(GLuint shader, GLenum pname, GLint *params)
{
	TYPEDEF(void (*methodType)(GLuint, GLenum, GLint *));
	BEFORE(glGetShaderiv);
	CALL_ORIG(glGetShaderiv, shader, pname, params);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dx",
	      shader, (uint64_t)(pname));
}

const GLubyte *REAL_NAME(glGetString)(GLenum name)
{
	TYPEDEF(const GLubyte *(*methodType)(GLenum));
	BEFORE(glGetString);
	const GLubyte *ret = CALL_ORIG(glGetString, name);
	GL_GET_ERROR();
	AFTER('p', ret, APITYPE_CONTEXT, "", "x", (uint64_t)(name));

	return ret;
}

//lsh_get
void REAL_NAME(glGetTexParameterfv)(GLenum target, GLenum pname, GLfloat * params)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLfloat *));
	BEFORE(glGetTexParameterfv);
	CALL_ORIG(glGetTexParameterfv, target, pname, params);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xx",
	      (uint64_t)(target), (uint64_t)(pname));
}

//lsh_get
void REAL_NAME(glGetTexParameteriv)(GLenum target, GLenum pname, GLint * params)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLint *));
	BEFORE(glGetTexParameteriv);
	CALL_ORIG(glGetTexParameteriv, target, pname, params);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xx",
	      (uint64_t)(target), (uint64_t)(pname));
}

//lsh_get
void REAL_NAME(glGetUniformfv)(GLuint program, GLint location, GLfloat *params)
{
	TYPEDEF(void (*methodType)(GLuint, GLuint, GLfloat *));
	BEFORE(glGetUniformfv);
	CALL_ORIG(glGetUniformfv, program, location, params);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dd",
	      program, location);
}

//lsh_get
void REAL_NAME(glGetUniformiv)(GLuint program, GLint location, GLint *params)
{
	TYPEDEF(void (*methodType)(GLuint, GLuint, GLint *));
	BEFORE(glGetUniformiv);
	CALL_ORIG(glGetUniformiv, program, location, params);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dd",
	      program, location);
}

//lsh_get
GLint REAL_NAME(glGetUniformLocation)(GLuint program, const char *name)
{
	TYPEDEF(GLint (*methodType)(GLuint, const char *));
	BEFORE(glGetUniformLocation);
	GLint ret = CALL_ORIG(glGetUniformLocation, program, name);
	GL_GET_ERROR();
	AFTER('d', ret, APITYPE_CONTEXT, "", "d", program);

	return ret;
}

//lsh_get
void REAL_NAME(glGetVertexAttribfv)(GLuint index, GLenum pname, GLfloat *params)
{
	TYPEDEF(void (*methodType)(GLuint, GLenum, GLfloat *));
	BEFORE(glGetVertexAttribfv);
	CALL_ORIG(glGetVertexAttribfv, index, pname, params);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dx",
	      index, (uint64_t)(pname));
}

//lsh_get
void REAL_NAME(glGetVertexAttribiv)(GLuint index, GLenum pname, GLint *params)
{
	TYPEDEF(void (*methodType)(GLuint, GLenum, GLint *));
	BEFORE(glGetVertexAttribiv);
	CALL_ORIG(glGetVertexAttribiv, index, pname, params);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dx",
	      index, (uint64_t)(pname));
}

//lsh_get
void REAL_NAME(glGetVertexAttribPointerv)(GLuint index, GLenum pname, GLvoid **pointer)
{
	TYPEDEF(void (*methodType)(GLuint, GLenum, GLvoid **));
	BEFORE(glGetVertexAttribPointerv);
	CALL_ORIG(glGetVertexAttribPointerv, index, pname, pointer);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dx",
	      index, (uint64_t)(pname));
}

// ==================================================================
// H 1
// ==================================================================

void REAL_NAME(glHint)(GLenum target, GLenum mode)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum));
	BEFORE(glHint);
	CALL_ORIG(glHint, target, mode);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xx",
	      (uint64_t)(target), (uint64_t)(mode));
}

// ==================================================================
// I 7
// ==================================================================

GLboolean REAL_NAME(glIsBuffer)(GLuint buffer)
{
	TYPEDEF(GLboolean (*methodType)(GLuint));
	BEFORE(glIsBuffer);
	GLboolean ret = CALL_ORIG(glIsBuffer, buffer);
	GL_GET_ERROR();
	AFTER('c', ret, APITYPE_CONTEXT, "", "d", buffer);

	return ret;
}

GLboolean REAL_NAME(glIsEnabled)(GLenum cap)
{
	TYPEDEF(GLboolean (*methodType)(GLenum));
	BEFORE(glIsEnabled);
	GLboolean ret = CALL_ORIG(glIsEnabled, cap);
	GL_GET_ERROR();
	AFTER('c', ret, APITYPE_CONTEXT, "", "x", (uint64_t)(cap));

	return ret;
}

GLboolean REAL_NAME(glIsFramebuffer)(GLuint framebuffer)
{
	TYPEDEF(GLboolean (*methodType)(GLuint));
	BEFORE(glIsFramebuffer);
	GLboolean ret = CALL_ORIG(glIsFramebuffer, framebuffer);
	GL_GET_ERROR();
	AFTER('c', ret, APITYPE_CONTEXT, "", "d", framebuffer);

	return ret;
}

GLboolean REAL_NAME(glIsProgram)(GLuint program)
{
	TYPEDEF(GLboolean (*methodType)(GLuint));
	BEFORE(glIsProgram);
	GLboolean ret = CALL_ORIG(glIsProgram, program);
	GL_GET_ERROR();
	AFTER('c', ret, APITYPE_CONTEXT, "", "d", program);

	return ret;
}

GLboolean REAL_NAME(glIsRenderbuffer)(GLuint renderbuffer)
{
	TYPEDEF(GLboolean (*methodType)(GLuint));
	BEFORE(glIsRenderbuffer);
	GLboolean ret = CALL_ORIG(glIsRenderbuffer, renderbuffer);
	GL_GET_ERROR();
	AFTER('c', ret, APITYPE_CONTEXT, "", "d", renderbuffer);

	return ret;
}

GLboolean REAL_NAME(glIsShader)(GLuint shader)
{
	TYPEDEF(GLboolean (*methodType)(GLuint));
	BEFORE(glIsShader);
	GLboolean ret = CALL_ORIG(glIsShader, shader);
	GL_GET_ERROR();
	AFTER('c', ret, APITYPE_CONTEXT, "", "d", shader);

	return ret;
}

GLboolean REAL_NAME(glIsTexture)(GLuint texture)
{
	TYPEDEF(GLboolean (*methodType)(GLuint));
	BEFORE(glIsTexture);
	GLboolean ret = CALL_ORIG(glIsTexture, texture);
	GL_GET_ERROR();
	AFTER('c', ret, APITYPE_CONTEXT, "", "d", texture);

	return ret;
}

// ==================================================================
// L 2
// ==================================================================

void REAL_NAME(glLineWidth)(GLfloat width)
{
	TYPEDEF(void (*methodType)(GLfloat));
	BEFORE(glLineWidth);
	CALL_ORIG(glLineWidth, width);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "f", width);
}

/* TODO refactor snprintf check*/
void REAL_NAME(glLinkProgram)(GLuint program)
{
	TYPEDEF(void (*methodType)(GLuint));
	BEFORE(glLinkProgram);
	CALL_ORIG(glLinkProgram, program);
	GL_GET_ERROR();
	char buf[512] = "";
	if (error == GL_NO_ERROR) {
		char *to = buf;
		int i, len;
		size_t avail;
		GLint activeNum[1];
		GLint maxLength[1];
		GLsizei length[1];
		GLint size[1];
		GLenum type[1];

		avail = sizeof(buf);

		real_glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, activeNum);
		real_glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
				    maxLength);

		char name1[maxLength[0]];
		len = snprintf(to, avail, "%d", activeNum[0]);
		to += len;
		avail -= len;
		for (i = 0; i < activeNum[0]; i++) {
			real_glGetActiveAttrib(program, i, maxLength[0], length,
					       size, type, name1);
			len = snprintf(to, avail, ",%d,%s,%d,%x", i, name1, size[0],
				       type[0]);

			if (avail <= (unsigned int)len) {
				PRINTERR("fatal. too small buf");
				break;
			}

			to += len;
			avail -= len;
		}

		real_glGetProgramiv(program, GL_ACTIVE_UNIFORMS, activeNum);
		real_glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH,
				    maxLength);

		char name2[maxLength[0]];
		len = snprintf(to, avail, ",%d", activeNum[0]);

		if (avail <= (unsigned int)len) {
			PRINTERR("fatal. too small buf");
		} else {
			to += len;
			avail -= len;
		}

		for (i = 0; i < activeNum[0]; i++) {
			real_glGetActiveUniform(program, i, maxLength[0],
						length, size, type, name2);
			len = snprintf(to, avail, ",%d,%s,%d,%x", i, name2,
				       size[0], type[0]);

			if (avail <= (unsigned int)len) {
				PRINTERR("fatal. too small buf");
				break;
			}

			to += len;
			avail -= len;

		}
	}
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, buf, "d",
	      program);
}

// ==================================================================
// P 2
// ==================================================================

void REAL_NAME(glPixelStorei)(GLenum pname, GLint param)
{
	TYPEDEF(void (*methodType)(GLenum, GLint));
	BEFORE(glPixelStorei);
	CALL_ORIG(glPixelStorei, pname, param);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xd",
	      (uint64_t)(pname), param);
}

void REAL_NAME(glPolygonOffset)(GLfloat factor, GLfloat units)
{
	TYPEDEF(void (*methodType)(GLfloat, GLfloat));
	BEFORE(glPolygonOffset);
	CALL_ORIG(glPolygonOffset, factor, units);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ff",
	      factor, units);
}

// ==================================================================
// R 3
// ==================================================================

//lsh_get
void REAL_NAME(glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height,
		  GLenum format, GLenum type, GLvoid * data)
{
	TYPEDEF(void (*methodType)(GLint, GLint, GLsizei, GLsizei, GLenum,
				   GLenum, GLvoid *));
	BEFORE(glReadPixels);
	CALL_ORIG(glReadPixels, x, y, width, height, format, type, data);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "",
	      "ddddxx", x, y, width, height,
	      (uint64_t)(format), (uint64_t)(type));
}

void REAL_NAME(glReleaseShaderCompiler)(void)
{
	TYPEDEF(void (*methodType)(void));
	BEFORE(glReleaseShaderCompiler);
	CALL_ORIG(glReleaseShaderCompiler,);
	GL_GET_ERROR();
	AFTER_NO_PARAM('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "");
}

void REAL_NAME(glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width,
			   GLsizei height)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLsizei, GLsizei));
	BEFORE(glRenderbufferStorage);
	CALL_ORIG(glRenderbufferStorage, target, internalformat, width, height);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xxdd",
	      (uint64_t)(target), (uint64_t)(internalformat), width, height);
}

// ==================================================================
// S 10
// ==================================================================

void REAL_NAME(glSampleCoverage)(GLclampf value, GLboolean invert)
{
	TYPEDEF(void (*methodType)(GLclampf, GLboolean));
	BEFORE(glSampleCoverage);
	CALL_ORIG(glSampleCoverage, value, invert);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "fx",
	      value, (uint64_t)(invert));
}

void REAL_NAME(glScissor)(GLint x, GLint y, GLsizei width, GLsizei height)
{
	TYPEDEF(void (*methodType)(GLint, GLint, GLsizei, GLsizei));
	BEFORE(glScissor);
	CALL_ORIG(glScissor, x, y, width, height);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dddd",
	      x, y, width, height);
}

//lsh_param
void REAL_NAME(glShaderBinary)(GLsizei n, const GLuint *shaders, GLenum binaryformat,
		    const void *binary, GLsizei length)
{
	TYPEDEF(void (*methodType)(GLsizei, const GLuint *, GLenum,
				   const void *, GLsizei));
	BEFORE(glShaderBinary);
	CALL_ORIG(glShaderBinary, n, shaders, binaryformat, binary, length);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "",
	      "dpxpd", n, voidp_to_uint64(shaders),
	      (uint64_t)(binaryformat), voidp_to_uint64(binary), length);
}

//lsh_param
void REAL_NAME(glShaderSource)(GLuint shader, GLsizei count, const char **string,
		    const GLint *length)
{
	TYPEDEF(void (*methodType)(GLuint, GLsizei, const char **,
				   const GLint*));
	BEFORE(glShaderSource);
	CALL_ORIG(glShaderSource, shader, count, string, length);
	GL_GET_ERROR();
	if (error == GL_NO_ERROR) {
		GLint length[1];
		real_glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, length);
		char buf[length[0]];
		real_glGetShaderSource(shader, length[0], NULL, buf);
		AFTER_SHADER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, buf,
			     length[0], "ddpp", shader, count,
			     voidp_to_uint64(string), voidp_to_uint64(length));
	} else {
		AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "",
		      "ddpp", shader, count,
		      voidp_to_uint64(string), voidp_to_uint64(length));
	}
}

void REAL_NAME(glStencilFunc)(GLenum func, GLint ref, GLuint mask)
{
	TYPEDEF(void (*methodType)(GLenum, GLint, GLint));
	BEFORE(glStencilFunc);
	CALL_ORIG(glStencilFunc, func, ref, mask);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xdd",
	      (uint64_t)(func), ref, mask);
}

void REAL_NAME(glStencilFuncSeparate)(GLenum face, GLenum func, GLint ref, GLuint mask)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLint, GLuint));
	BEFORE(glStencilFuncSeparate);
	CALL_ORIG(glStencilFuncSeparate, face, func, ref, mask);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xxdd",
	      (uint64_t)(face), (uint64_t)(func), ref, mask);
}

void REAL_NAME(glStencilMask)(GLuint mask)
{
	TYPEDEF(void (*methodType)(GLuint));
	BEFORE(glStencilMask);
	CALL_ORIG(glStencilMask, mask);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "d", mask);
}

void REAL_NAME(glStencilMaskSeparate)(GLenum face, GLuint mask)
{
	TYPEDEF(void (*methodType)(GLenum, GLuint));
	BEFORE(glStencilMaskSeparate);
	CALL_ORIG(glStencilMaskSeparate, face, mask);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xd",
	      (uint64_t)(face), mask);
}

void REAL_NAME(glStencilOp)(GLenum sfail, GLenum dpfail, GLenum dppass)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLenum));
	BEFORE(glStencilOp);
	CALL_ORIG(glStencilOp, sfail, dpfail, dppass);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xxx",
	      (uint64_t)(sfail), (uint64_t)(dpfail),
	      (uint64_t)(dppass));
}

void REAL_NAME(glStencilOpSeparate)(GLenum face, GLenum sfail, GLenum dpfail,
			 GLenum dppass)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLenum, GLenum));
	BEFORE(glStencilOpSeparate);
	CALL_ORIG(glStencilOpSeparate, face, sfail, dpfail, dppass);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xxxx",
	      (uint64_t)(face), (uint64_t)(sfail), (uint64_t)(dpfail),
	      (uint64_t)(dppass));
}

// ==================================================================
// T 6
// ==================================================================

void REAL_NAME(glTexImage2D)(GLenum target, GLint level, GLenum internalformat,
		  GLsizei width, GLsizei height, GLint border, GLenum format,
		  GLenum type, const GLvoid *data)
{
	TYPEDEF(void (*methodType)(GLenum, GLint, GLint, GLsizei, GLsizei,
				   GLint, GLenum, GLenum, const GLvoid *));
	BEFORE(glTexImage2D);
	CALL_ORIG(glTexImage2D, target, level, internalformat, width,
		  height, border, format, type, data);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xdddddxxp",
	      (uint64_t)(target), level, internalformat, width, height,
	      border, (uint64_t)(format), (uint64_t)(type),
	      voidp_to_uint64(data));
}

void REAL_NAME(glTexParameterf)(GLenum target, GLenum pname, GLfloat param)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLfloat));
	BEFORE(glTexParameterf);
	CALL_ORIG(glTexParameterf, target, pname, param);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xxf",
	      (uint64_t)(target), (uint64_t)(pname), param);
}

void REAL_NAME(glTexParameterfv)(GLenum target, GLenum pname, const GLfloat * params)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, const GLfloat *));
	BEFORE(glTexParameterfv);
	CALL_ORIG(glTexParameterfv, target, pname, params);
	GL_GET_ERROR();
	if (error == GL_NO_ERROR) {
		char param0[sizeof(GLenum) * 4];
		snprintf(param0, sizeof(param0), "%x", (GLenum)params[0]);
		AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, param0, "xxp",
		      (uint64_t)(target), (uint64_t)(pname),
		      voidp_to_uint64(params));
	} else {
		AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xxp",
		      (uint64_t)(target), (uint64_t)(pname),
		      voidp_to_uint64(params));
	}
}

void REAL_NAME(glTexParameteri)(GLenum target, GLenum pname, GLint param)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, GLint));
	BEFORE(glTexParameteri);
	CALL_ORIG(glTexParameteri, target, pname, param);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xxx",
	      (uint64_t)(target), (uint64_t)(pname),
	      (uint64_t)(param));
}

void REAL_NAME(glTexParameteriv)(GLenum target, GLenum pname, const GLint * params)
{
	TYPEDEF(void (*methodType)(GLenum, GLenum, const GLint *));
	BEFORE(glTexParameteriv);
	CALL_ORIG(glTexParameteriv, target, pname, params);
	GL_GET_ERROR();
	if (error == GL_NO_ERROR) {
		char param0[sizeof(GLenum) * 4];
		snprintf(param0, sizeof(param0), "%x", (GLenum)params[0]);
		AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, param0, "xxp",
		      (uint64_t)(target), (uint64_t)(pname),
		      voidp_to_uint64(params));
	} else {
		AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "xxp",
		      (uint64_t)(target),
		      (uint64_t)(pname),
		      voidp_to_uint64(params));
	}
}

void REAL_NAME(glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset,
		     GLsizei width, GLsizei height, GLenum format, GLenum type,
		     const GLvoid * data)
{
	TYPEDEF(void (*methodType)(GLenum, GLint, GLint, GLint, GLsizei,
				   GLsizei, GLenum, GLenum, const GLvoid *));
	BEFORE(glTexSubImage2D);
	CALL_ORIG(glTexSubImage2D, target, level, xoffset, yoffset, width,
		  height, format, type, data);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "",
	      "xdddddxxp",
	      (uint64_t)(target), level, xoffset, yoffset, width, height,
	      (uint64_t)(format), (uint64_t)(type), voidp_to_uint64(data));
}

// ==================================================================
// U 20
// ==================================================================

void REAL_NAME(glUniform1f)(GLint location, GLfloat v0)
{
	TYPEDEF(void (*methodType)(GLint, GLfloat));
	BEFORE(glUniform1f);
	CALL_ORIG(glUniform1f, location, v0);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "df",
	      location, v0);
}

void REAL_NAME(glUniform2f)(GLint location, GLfloat v0, GLfloat v1)
{
	TYPEDEF(void (*methodType)(GLint, GLfloat, GLfloat));
	BEFORE(glUniform2f);
	CALL_ORIG(glUniform2f, location, v0, v1);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dff",
	      location, v0, v1);
}

void REAL_NAME(glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
	TYPEDEF(void (*methodType)(GLint, GLfloat, GLfloat, GLfloat));
	BEFORE(glUniform3f);
	CALL_ORIG(glUniform3f, location, v0, v1, v2);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dfff",
	      location, v0, v1, v2);
}

void REAL_NAME(glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
	TYPEDEF(void (*methodType)(GLint, GLfloat, GLfloat, GLfloat, GLfloat));
	BEFORE(glUniform4f);
	CALL_ORIG(glUniform4f, location, v0, v1, v2, v3);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dffff",
	      location, v0, v1, v2, v3);
}

void REAL_NAME(glUniform1fv)(GLint location, GLsizei count, const GLfloat *value)
{
	TYPEDEF(void (*methodType)(GLint, GLsizei, const GLfloat *));
	BEFORE(glUniform1fv);
	CALL_ORIG(glUniform1fv, location, count, value);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddF",
	      location, count, count * 1, voidp_to_uint64(value));
}

void REAL_NAME(glUniform2fv)(GLint location, GLsizei count, const GLfloat *value)
{
	TYPEDEF(void (*methodType)(GLint, GLsizei, const GLfloat *));
	BEFORE(glUniform2fv);
	CALL_ORIG(glUniform2fv, location, count, value);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddF",
	      location, count, count * 2, voidp_to_uint64(value));
}

void REAL_NAME(glUniform3fv)(GLint location, GLsizei count, const GLfloat *value)
{
	TYPEDEF(void (*methodType)(GLint, GLsizei, const GLfloat *));
	BEFORE(glUniform3fv);
	CALL_ORIG(glUniform3fv, location, count, value);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddF",
	      location, count, count * 3, voidp_to_uint64(value));
}

void REAL_NAME(glUniform4fv)(GLint location, GLsizei count, const GLfloat *value)
{
	TYPEDEF(void (*methodType)(GLint, GLsizei, const GLfloat *));
	BEFORE(glUniform4fv);
	CALL_ORIG(glUniform4fv, location, count, value);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddF",
	      location, count, count * 4, voidp_to_uint64(value));
}

void REAL_NAME(glUniform1i)(GLint location, GLint v0)
{
	TYPEDEF(void (*methodType)(GLint, GLint));
	BEFORE(glUniform1i);
	CALL_ORIG(glUniform1i, location, v0);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dd",
	      location, v0);
}

void REAL_NAME(glUniform2i)(GLint location, GLint v0, GLint v1)
{
	TYPEDEF(void (*methodType)(GLint, GLint, GLint));
	BEFORE(glUniform2i);
	CALL_ORIG(glUniform2i, location, v0, v1);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddd",
	      location, v0, v1);
}

void REAL_NAME(glUniform3i)(GLint location, GLint v0, GLint v1, GLint v2)
{
	TYPEDEF(void (*methodType)(GLint, GLint, GLint, GLint));
	BEFORE(glUniform3i);
	CALL_ORIG(glUniform3i, location, v0, v1, v2);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dddd",
	      location, v0, v1, v2);
}

void REAL_NAME(glUniform4i)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
	TYPEDEF(void (*methodType)(GLint, GLint, GLint, GLint, GLint));
	BEFORE(glUniform4i);
	CALL_ORIG(glUniform4i, location, v0, v1, v2, v3);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddddd",
	      location, v0, v1, v2, v3);
}

void REAL_NAME(glUniform1iv)(GLint location, GLsizei count, const GLint *value)
{
	TYPEDEF(void (*methodType)(GLint, GLsizei, const GLint *));
	BEFORE(glUniform1iv);
	CALL_ORIG(glUniform1iv, location, count, value);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddD",
	      location, count, count * 1, voidp_to_uint64(value));
}

void REAL_NAME(glUniform2iv)(GLint location, GLsizei count, const GLint *value)
{
	TYPEDEF(void (*methodType)(GLint, GLsizei, const GLint *));
	BEFORE(glUniform2iv);
	CALL_ORIG(glUniform2iv, location, count, value);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddD",
	      location, count, count * 2, voidp_to_uint64(value));
}

void REAL_NAME(glUniform3iv)(GLint location, GLsizei count, const GLint *value)
{
	TYPEDEF(void (*methodType)(GLint, GLsizei, const GLint *));
	BEFORE(glUniform3iv);
	CALL_ORIG(glUniform3iv, location, count, value);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddD",
	      location, count, count * 3, voidp_to_uint64(value));
}

void REAL_NAME(glUniform4iv)(GLint location, GLsizei count, const GLint *value)
{
	TYPEDEF(void (*methodType)(GLint, GLsizei, const GLint *));
	BEFORE(glUniform4iv);
	CALL_ORIG(glUniform4iv, location, count, value);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddD",
	      location, count, count * 4, voidp_to_uint64(value));
}

void REAL_NAME(glUniformMatrix2fv)(GLint location, GLsizei count, GLboolean transpose,
			const GLfloat *value)
{
	TYPEDEF(void (*methodType)(GLint, GLsizei, GLboolean, const GLfloat *));
	BEFORE(glUniformMatrix2fv);
	CALL_ORIG(glUniformMatrix2fv, location, count, transpose, value);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddxF",
	      location, count, (uint64_t)(transpose), count * 2 * 2,
	      voidp_to_uint64(value));
}

void REAL_NAME(glUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose,
			const GLfloat *value)
{
	TYPEDEF(void (*methodType)(GLint, GLsizei, GLboolean, const GLfloat *));
	BEFORE(glUniformMatrix3fv);
	CALL_ORIG(glUniformMatrix3fv, location, count, transpose, value);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddxF",
	      location, count, (uint64_t)(transpose), count * 3 * 3,
	      voidp_to_uint64(value));
}

void REAL_NAME(glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose,
			const GLfloat *value)
{
	TYPEDEF(void (*methodType)(GLint, GLsizei, GLboolean, const GLfloat *));
	BEFORE(glUniformMatrix4fv);
	CALL_ORIG(glUniformMatrix4fv, location, count, transpose, value);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddxF",
	      location, count, (uint64_t)(transpose), count * 4 * 4,
	      voidp_to_uint64(value));
}

void REAL_NAME(glUseProgram)(GLuint program)
{
	TYPEDEF(void (*methodType)(GLuint));
	BEFORE(glUseProgram);
	CALL_ORIG(glUseProgram, program);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "d",
	      program);
}

// ==================================================================
// V 7
// ==================================================================

void REAL_NAME(glValidateProgram)(GLuint program)
{
	TYPEDEF(void (*methodType)(GLuint));
	BEFORE(glValidateProgram);
	CALL_ORIG(glValidateProgram, program);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "d",
	      program);
}

void REAL_NAME(glVertexAttrib1f)(GLuint index, GLfloat v0)
{
	TYPEDEF(void (*methodType)(GLuint, GLfloat));
	BEFORE(glVertexAttrib1f);
	CALL_ORIG(glVertexAttrib1f, index, v0);

	GLfloat cv[4];
	real_glGetVertexAttribfv(index, GL_CURRENT_VERTEX_ATTRIB, cv);
	snprintf(contextValue, sizeof(contextValue), "%f,%f,%f,%f",
		 cv[0], cv[1], cv[2], cv[3]);

	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, contextValue, "df",
	      index, v0);
}

void REAL_NAME(glVertexAttrib2f)(GLuint index, GLfloat v0, GLfloat v1)
{
	TYPEDEF(void (*methodType)(GLuint, GLfloat, GLfloat));
	BEFORE(glVertexAttrib2f);
	CALL_ORIG(glVertexAttrib2f, index, v0, v1);

	GLfloat cv[4];
	real_glGetVertexAttribfv(index, GL_CURRENT_VERTEX_ATTRIB, cv);
	snprintf(contextValue, sizeof(contextValue), "%f,%f,%f,%f",
		 cv[0], cv[1], cv[2], cv[3]);

	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, contextValue, "dff",
	      index, v0, v1);
}

void REAL_NAME(glVertexAttrib3f)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2)
{
	TYPEDEF(void (*methodType)(GLuint, GLfloat, GLfloat, GLfloat));
	BEFORE(glVertexAttrib3f);
	CALL_ORIG(glVertexAttrib3f, index, v0, v1, v2);

	GLfloat cv[4];
	real_glGetVertexAttribfv(index, GL_CURRENT_VERTEX_ATTRIB, cv);
	snprintf(contextValue, sizeof(contextValue), "%f,%f,%f,%f",
		 cv[0], cv[1], cv[2], cv[3]);

	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, contextValue, "dfff",
	      index, v0, v1, v2);
}

void REAL_NAME(glVertexAttrib4f)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2,
		      GLfloat v3)
{
	TYPEDEF(void (*methodType)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat));
	BEFORE(glVertexAttrib4f);
	CALL_ORIG(glVertexAttrib4f, index, v0, v1, v2, v3);

	GLfloat cv[4];
	real_glGetVertexAttribfv(index, GL_CURRENT_VERTEX_ATTRIB, cv);
	snprintf(contextValue, sizeof(contextValue), "%f,%f,%f,%f",
		 cv[0], cv[1], cv[2], cv[3]);

	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, contextValue, "dffff",
	      index, v0, v1, v2, v3);
}

void REAL_NAME(glVertexAttrib1fv)(GLuint index, const GLfloat *v)
{
	TYPEDEF(void (*methodType)(GLuint, const GLfloat *));
	BEFORE(glVertexAttrib1fv);
	CALL_ORIG(glVertexAttrib1fv, index, v);

	GLfloat cv[4];
	real_glGetVertexAttribfv(index, GL_CURRENT_VERTEX_ATTRIB, cv);
	snprintf(contextValue, sizeof(contextValue), "%f,%f,%f,%f",
		 cv[0], cv[1], cv[2], cv[3]);

	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, contextValue,
	      "dp", index, voidp_to_uint64(v));
}

void REAL_NAME(glVertexAttrib2fv)(GLuint index, const GLfloat *v)
{
	TYPEDEF(void (*methodType)(GLuint, const GLfloat *));
	BEFORE(glVertexAttrib2fv);
	CALL_ORIG(glVertexAttrib2fv, index, v);

	GLfloat cv[4];
	real_glGetVertexAttribfv(index, GL_CURRENT_VERTEX_ATTRIB, cv);
	snprintf(contextValue, sizeof(contextValue), "%f,%f,%f,%f",
		 cv[0], cv[1], cv[2], cv[3]);

	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, contextValue,
	      "dp", index, voidp_to_uint64(v));
}

void REAL_NAME(glVertexAttrib3fv)(GLuint index, const GLfloat *v)
{
	TYPEDEF(void (*methodType)(GLuint, const GLfloat *));
	BEFORE(glVertexAttrib3fv);
	CALL_ORIG(glVertexAttrib3fv, index, v);

	GLfloat cv[4];
	real_glGetVertexAttribfv(index, GL_CURRENT_VERTEX_ATTRIB, cv);
	snprintf(contextValue, sizeof(contextValue), "%f,%f,%f,%f",
		 cv[0], cv[1], cv[2], cv[3]);

	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, contextValue,
	      "dp", index, voidp_to_uint64(v));
}

void REAL_NAME(glVertexAttrib4fv)(GLuint index, const GLfloat *v)
{
	TYPEDEF(void (*methodType)(GLuint, const GLfloat *));
	BEFORE(glVertexAttrib4fv);
	CALL_ORIG(glVertexAttrib4fv, index, v);
	GLfloat cv[4];
	real_glGetVertexAttribfv(index, GL_CURRENT_VERTEX_ATTRIB, cv);
	snprintf(contextValue, sizeof(contextValue), "%f,%f,%f,%f",
		 cv[0], cv[1], cv[2], cv[3]);

	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, contextValue,
	      "dp", index, voidp_to_uint64(v));
}

void REAL_NAME(glVertexAttribPointer)(GLuint index, GLint size, GLenum type,
			   GLboolean normalized, GLsizei stride,
			   const GLvoid *pointer)
{
	TYPEDEF(void (*methodType)(GLuint, GLint, GLenum, GLboolean, GLsizei,
				   const GLvoid *));
	BEFORE(glVertexAttribPointer);
	CALL_ORIG(glVertexAttribPointer, index, size, type, normalized, stride,
		  pointer);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "ddxxdp",
	      index, size, (uint64_t)(type), (uint64_t)(normalized),
	      stride, voidp_to_uint64(pointer));
}

void REAL_NAME(glViewport)(GLint x, GLint y, GLsizei width, GLsizei height)
{
	TYPEDEF(void (*methodType)(GLint, GLint, GLsizei, GLsizei));

	BEFORE(glViewport);

	CALL_ORIG(glViewport, x, y, width, height);
	GL_GET_ERROR();
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "dddd",
	      x, y, width, height);
}

#undef CALL_ORIG
#undef BEFORE
