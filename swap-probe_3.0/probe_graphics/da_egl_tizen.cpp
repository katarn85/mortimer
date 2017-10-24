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
 * - Samsung RnD Institute Russia
 *
 */

#include <egl_macro.h>
#include <egl.h>
#include "da_gles20.h"
#include "daprobe.h"
#include "binproto.h"
#include "common_probe_init.h"

#ifndef BEFORE_EGL
#define BEFORE_EGL	BEFORE_EGL_TIZEN
#endif

static enum DaOptions _sopt = OPT_GLES;
static __thread EGLint egl_error_external = EGL_SUCCESS;

EGLint eglGetError(void)
{
	typedef EGLint (*methodType)(void);
	BEFORE_EGL(eglGetError);
	EGLint ret = eglGetErrorp();

	if (egl_error_external == EGL_SUCCESS)
		egl_error_external = ret;

	if (blockresult) {
		//external call
		ret = egl_error_external;
		egl_error_external = EGL_SUCCESS;
	}

	AFTER_NO_PARAM('d', ret, APITYPE_CONTEXT, "");

	return ret;
}

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id)
{
	typedef EGLDisplay (*methodType)(EGLNativeDisplayType display_id);
	/* probe prepare */
	BEFORE_EGL(eglGetDisplay);
	/* call original function */
	EGLDisplay ret = eglGetDisplayp(display_id);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('p', voidp_to_uint64(ret), APITYPE_CONTEXT, "", "p",
	      voidp_to_uint64(display_id));
	return ret;
}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLint *major,
					 EGLint *minor);
	/* probe prepare */
	BEFORE_EGL(eglInitialize);
	/* call original function */
	EGLBoolean ret = eglInitializep(dpy, major, minor);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "", "ppp", voidp_to_uint64(dpy),
	      voidp_to_uint64(major), voidp_to_uint64(minor));
	return ret;
}

EGLBoolean eglTerminate(EGLDisplay dpy)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy);
	/* probe prepare */
	BEFORE_EGL(eglTerminate);
	/* call original function */
	EGLBoolean ret = eglTerminatep(dpy);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "", "p", voidp_to_uint64(dpy));
	return ret;
}

const char *eglQueryString(EGLDisplay dpy, EGLint name)
{
	typedef const char *(*methodType)(EGLDisplay dpy, EGLint name);
	/* probe prepare */
	BEFORE_EGL(eglQueryString);
	/* call original function */
	const char *ret = eglQueryStringp(dpy, name);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('s', ret, APITYPE_CONTEXT, "", "pd", voidp_to_uint64(dpy), name);
	return ret;
}

EGLBoolean eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size,
			 EGLint *num_config)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLConfig *configs,
					 EGLint config_size,
					 EGLint *num_config);
	/* probe prepare */
	BEFORE_EGL(eglGetConfigs);
	/* call original function */
	EGLBoolean ret = eglGetConfigsp(dpy, configs, config_size, num_config);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "", "ppdp",
	      voidp_to_uint64(dpy), voidp_to_uint64(configs), config_size,
	      voidp_to_uint64(num_config));
	return ret;
}

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
			   EGLConfig *configs, EGLint config_size,
			   EGLint *num_config)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy,
					 const EGLint *attrib_list,
					 EGLConfig *configs, EGLint config_size,
					 EGLint *num_config);
	/* probe prepare */
	BEFORE_EGL(eglChooseConfig);
	/* call original function */
	EGLBoolean ret = eglChooseConfigp(dpy, attrib_list, configs,
					  config_size, num_config);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "pppdp", voidp_to_uint64(dpy), voidp_to_uint64(attrib_list),
	      voidp_to_uint64(configs), config_size,
	      voidp_to_uint64(num_config));
	return ret;
}

EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config,
			      EGLint attribute, EGLint *value)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLConfig config,
					 EGLint attribute, EGLint *value);
	/* probe prepare */
	BEFORE_EGL(eglGetConfigAttrib);
	/* call original function */
	EGLBoolean ret = eglGetConfigAttribp(dpy, config, attribute, value);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "ppdp", voidp_to_uint64(dpy), voidp_to_uint64(config), attribute,
	      voidp_to_uint64(value));
	return ret;
}

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
				  EGLNativeWindowType win,
				  const EGLint *attrib_list)
{
	typedef EGLSurface (*methodType)(EGLDisplay dpy, EGLConfig config,
					 EGLNativeWindowType win,
					 const EGLint *attrib_list);
	/* probe prepare */
	BEFORE_EGL(eglCreateWindowSurface);
	/* call original function */
	EGLSurface ret = eglCreateWindowSurfacep(dpy, config, win, attrib_list);

	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('p', voidp_to_uint64(ret), APITYPE_CONTEXT, "",
	      "pppp", voidp_to_uint64(dpy), voidp_to_uint64(config),
	      voidp_to_uint64(win), voidp_to_uint64(attrib_list));
	return ret;
}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
				   const EGLint *attrib_list)
{
	typedef EGLSurface (*methodType)(EGLDisplay dpy, EGLConfig config,
					 const EGLint *attrib_list);
	/* probe prepare */
	BEFORE_EGL(eglCreatePbufferSurface);
	/* call original function */
	EGLSurface ret = eglCreatePbufferSurfacep(dpy, config, attrib_list);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('p', voidp_to_uint64(ret), APITYPE_CONTEXT, "",
	      "ppp", voidp_to_uint64(dpy), voidp_to_uint64(config),
	      voidp_to_uint64(attrib_list));
	return ret;
}

EGLSurface eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config,
				  EGLNativePixmapType pixmap,
				  const EGLint *attrib_list)
{
	typedef EGLSurface (*methodType)(EGLDisplay dpy, EGLConfig config,
					 EGLNativePixmapType pixmap,
					 const EGLint *attrib_list);
	/* probe prepare */
	BEFORE_EGL(eglCreatePixmapSurface);
	/* call original function */
	EGLSurface ret = eglCreatePixmapSurfacep(dpy, config, pixmap,
						 attrib_list);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('p', voidp_to_uint64(ret), APITYPE_CONTEXT, "",
	      "pppp", voidp_to_uint64(dpy), voidp_to_uint64(config),
	      voidp_to_uint64(pixmap), voidp_to_uint64(attrib_list));
	return ret;
}

EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLSurface surface);
	/* probe prepare */
	BEFORE_EGL(eglDestroySurface);
	/* call original function */
	EGLBoolean ret = eglDestroySurfacep(dpy, surface);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "pp", voidp_to_uint64(dpy), voidp_to_uint64(surface));
	return ret;
}

EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute,
			   EGLint *value)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLSurface surface,
					 EGLint attribute, EGLint *value);
	/* probe prepare */
	BEFORE_EGL(eglQuerySurface);
	/* call original function */
	EGLBoolean ret = eglQuerySurfacep(dpy, surface, attribute, value);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "ppdp", voidp_to_uint64(dpy), voidp_to_uint64(surface),
	      attribute, voidp_to_uint64(value));
	return ret;
}

EGLBoolean eglBindAPI(EGLenum api)
{
	typedef EGLBoolean (*methodType)(EGLenum api);
	/* probe prepare */
	BEFORE_EGL(eglBindAPI);
	/* call original function */
	EGLBoolean ret = eglBindAPIp(api);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "d", api);
	return ret;
}

EGLenum eglQueryAPI(void)
{
	typedef EGLenum (*methodType)(void);
	/* probe prepare */
	BEFORE_EGL(eglQueryAPI);
	/* call original function */
	EGLenum ret = eglQueryAPIp();
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER_NO_PARAM('d', ret, APITYPE_CONTEXT, "");
	return ret;
}

EGLBoolean eglWaitClient(void)
{
	typedef EGLBoolean (*methodType)(void);
	/* probe prepare */
	BEFORE_EGL(eglWaitClient);
	/* call original function */
	EGLBoolean ret = eglWaitClientp();
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER_NO_PARAM('d', ret, APITYPE_CONTEXT, "");
	return ret;
}

EGLBoolean eglReleaseThread(void)
{
	typedef EGLBoolean (*methodType)(void);
	/* probe prepare */
	BEFORE_EGL(eglReleaseThread);
	/* call original function */
	EGLBoolean ret = eglReleaseThreadp();
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER_NO_PARAM('d', ret, APITYPE_CONTEXT, "");
	return ret;
}

EGLSurface eglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype,
					    EGLClientBuffer buffer,
					    EGLConfig config,
					    const EGLint *attrib_list)
{
	typedef EGLSurface (*methodType)(EGLDisplay dpy, EGLenum buftype,
					 EGLClientBuffer buffer,
					 EGLConfig config,
					 const EGLint *attrib_list);
	/* probe prepare */
	BEFORE_EGL(eglCreatePbufferFromClientBuffer);
	/* call original function */
	EGLSurface ret = eglCreatePbufferFromClientBufferp(dpy, buftype, buffer,
							   config, attrib_list);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('p', voidp_to_uint64(ret), APITYPE_CONTEXT, "",
	      "pdppp", voidp_to_uint64(dpy), buftype, voidp_to_uint64(buffer),
	      voidp_to_uint64(config), voidp_to_uint64(attrib_list));
	return ret;
}

EGLBoolean eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface,
			    EGLint attribute, EGLint value)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLSurface surface,
					 EGLint attribute, EGLint value);
	/* probe prepare */
	BEFORE_EGL(eglSurfaceAttrib);
	/* call original function */
	EGLBoolean ret = eglSurfaceAttribp(dpy, surface, attribute, value);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "ppdd", voidp_to_uint64(dpy), voidp_to_uint64(surface), attribute,
	      value);
	return ret;
}

EGLBoolean eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLSurface surface,
					 EGLint buffer);
	/* probe prepare */
	BEFORE_EGL(eglBindTexImage);
	/* call original function */
	EGLBoolean ret = eglBindTexImagep(dpy, surface, buffer);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "ppd", voidp_to_uint64(dpy), voidp_to_uint64(surface), buffer);
	return ret;
}

EGLBoolean eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLSurface surface,
					 EGLint buffer);
	/* probe prepare */
	BEFORE_EGL(eglReleaseTexImage);
	/* call original function */
	EGLBoolean ret = eglReleaseTexImagep(dpy, surface, buffer);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "ppd", voidp_to_uint64(dpy), voidp_to_uint64(surface), buffer);
	return ret;
}

EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLint interval);
	/* probe prepare */
	BEFORE_EGL(eglSwapInterval);
	/* call original function */
	EGLBoolean ret = eglSwapIntervalp(dpy, interval);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "pd", voidp_to_uint64(dpy), interval);
	return ret;
}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config,
			    EGLContext share_context, const EGLint *attrib_list)
{
	typedef EGLContext (*methodType)(EGLDisplay dpy, EGLConfig config,
					 EGLContext share_context,
					 const EGLint *attrib_list);
	/* probe prepare */
	BEFORE_EGL(eglCreateContext);
	/* call original function */
	EGLContext ret = eglCreateContextp(dpy, config, share_context,
					   attrib_list);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */

	AFTER('p', voidp_to_uint64(ret), APITYPE_CONTEXT, "",
	      "pppp", voidp_to_uint64(dpy), voidp_to_uint64(config),
	      voidp_to_uint64(share_context), voidp_to_uint64(attrib_list));
	return ret;
}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLContext ctx);
	/* probe prepare */
	BEFORE_EGL(eglDestroyContext);
	/* call original function */
	EGLBoolean ret = eglDestroyContextp(dpy, ctx);
	/* get error code */
	EGL_GET_ERROR()

	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "pp", voidp_to_uint64(dpy), voidp_to_uint64(ctx));
	return ret;
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read,
			  EGLContext ctx)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLSurface draw,
					 EGLSurface read, EGLContext ctx);
	/* probe prepare */
	BEFORE_EGL(eglMakeCurrent);
	/* call original function */
	EGLBoolean ret = eglMakeCurrentp(dpy, draw, read, ctx);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "pppp", voidp_to_uint64(dpy), voidp_to_uint64(draw),
	      voidp_to_uint64(read), voidp_to_uint64(ctx));
	return ret;
}

EGLContext eglGetCurrentContext(void)
{
	typedef EGLContext (*methodType)(void);
	/* probe prepare */
	BEFORE_EGL(eglGetCurrentContext);
	/* call original function */
	EGLContext ret = eglGetCurrentContextp();

	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER_NO_PARAM('p', voidp_to_uint64(ret), APITYPE_CONTEXT, "");
	return ret;
}

EGLSurface eglGetCurrentSurface(EGLint readdraw)
{
	typedef EGLSurface (*methodType)(EGLint readdraw);
	/* probe prepare */
	BEFORE_EGL(eglGetCurrentSurface);
	/* call original function */
	EGLSurface ret = eglGetCurrentSurfacep(readdraw);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('p', voidp_to_uint64(ret), APITYPE_CONTEXT, "",
	      "d", readdraw);
	return ret;
}

EGLDisplay eglGetCurrentDisplay(void)
{
	typedef EGLDisplay (*methodType)(void);
	/* probe prepare */
	BEFORE_EGL(eglGetCurrentDisplay);
	/* call original function */
	EGLDisplay ret = eglGetCurrentDisplayp();
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER_NO_PARAM('p', voidp_to_uint64(ret), APITYPE_CONTEXT, "");
	return ret;
}

EGLBoolean eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute,
			   EGLint *value)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLContext ctx,
					 EGLint attribute, EGLint *value);
	/* probe prepare */
	BEFORE_EGL(eglQueryContext);
	/* call original function */
	EGLBoolean ret = eglQueryContextp(dpy, ctx, attribute, value);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "ppdp", voidp_to_uint64(dpy), voidp_to_uint64(ctx),
	      attribute, voidp_to_uint64(value));
	return ret;
}

EGLBoolean eglWaitGL(void)
{
	typedef EGLBoolean (*methodType)(void);
	/* probe prepare */
	BEFORE_EGL(eglWaitGL);
	/* call original function */
	EGLBoolean ret = eglWaitGLp();
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER_NO_PARAM('d', ret, APITYPE_CONTEXT, "");
	return ret;
}

EGLBoolean eglWaitNative(EGLint engine)
{
	typedef EGLBoolean (*methodType)(EGLint engine);
	/* probe prepare */
	BEFORE_EGL(eglWaitNative);
	/* call original function */
	EGLBoolean ret = eglWaitNativep(engine);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "d", engine);
	return ret;
}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLSurface surface);
	BEFORE_EGL(eglSwapBuffers);
	EGLBoolean ret = eglSwapBuffersp(dpy, surface);
	EGL_GET_ERROR();
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "pp", voidp_to_uint64(dpy), voidp_to_uint64(surface));
	return ret;
}

EGLBoolean eglCopyBuffers(EGLDisplay dpy, EGLSurface surface,
			  EGLNativePixmapType target)
{
	typedef EGLBoolean (*methodType)(EGLDisplay dpy, EGLSurface surface,
					 EGLNativePixmapType target);
	/* probe prepare */
	BEFORE_EGL(eglCopyBuffers);
	/* call original function */
	EGLBoolean ret = eglCopyBuffersp(dpy, surface, target);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('d', ret, APITYPE_CONTEXT, "",
	      "ppp", voidp_to_uint64(dpy), voidp_to_uint64(surface),
	      voidp_to_uint64(target));
	return ret;
}

#define __eglGetProcAddress_t __eglMustCastToProperFunctionPointerType
EGLAPI __eglGetProcAddress_t eglGetProcAddress(const char *procname)
{
	typedef EGLAPI __eglGetProcAddress_t(*methodType)(const char *procname);
	/* probe prepare */
	BEFORE_EGL(eglGetProcAddress);
	/* call original function */
	__eglGetProcAddress_t ret = eglGetProcAddressp(procname);
	/* get error code */
	EGL_GET_ERROR();
	/* pack and send */
	AFTER('p', voidp_to_uint64((void *)ret), APITYPE_CONTEXT, "",
	      "s", procname);
	return ret;
}

