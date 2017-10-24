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
 * - Samsung RnD Institute Russia
 *
 */

#include "da_gles20.h"
#include "daprobe.h"
#include "binproto.h"
#include "common_probe_init.h"
#include "real_functions.h"

static enum DaOptions _sopt = OPT_GLES;

/* Evas open gl API functions */

void evas_gl_free(Evas_GL *evas_gl)
{
	typedef void (*methodType)(Evas_GL *evas_gl);
	BEFORE_EVAS_GL(evas_gl_free);
	evas_gl_freep(evas_gl);
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "p",
	      voidp_to_uint64(evas_gl));
}

void evas_gl_config_free(Evas_GL_Config *cfg)
{
	typedef void (*methodType)(Evas_GL_Config *cfg);
	BEFORE_EVAS_GL(evas_gl_config_free);
	evas_gl_config_freep(cfg);
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "p",
	      voidp_to_uint64(cfg));
}

void evas_gl_surface_destroy(Evas_GL *evas_gl, Evas_GL_Surface *surf)
{
	typedef void (*methodType)(Evas_GL *evas_gl, Evas_GL_Surface *surf);
	BEFORE_EVAS_GL(evas_gl_surface_destroy);
	evas_gl_surface_destroyp(evas_gl, surf);
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "pp",
	      voidp_to_uint64(evas_gl), voidp_to_uint64(surf));
}

void evas_gl_context_destroy(Evas_GL *evas_gl, Evas_GL_Context *ctx)
{
	typedef void (*methodType)(Evas_GL *evas_gl, Evas_GL_Context *ctx);
	BEFORE_EVAS_GL(evas_gl_context_destroy);
	evas_gl_context_destroyp(evas_gl, ctx);
	AFTER('v', NO_RETURN_VALUE, APITYPE_CONTEXT, "", "pp",
	      voidp_to_uint64(evas_gl), voidp_to_uint64(ctx));
}

Evas_GL *evas_gl_new(Evas *e)
{
	typedef Evas_GL *(*methodType)(Evas *e);
	BEFORE_EVAS_GL(evas_gl_new);
	Evas_GL *res = evas_gl_newp(e);
	AFTER('p', voidp_to_uint64(res), APITYPE_CONTEXT, "", "p",
	      voidp_to_uint64(e));
	return res;
}

Evas_GL_Config *evas_gl_config_new(void)
{
	typedef Evas_GL_Config *(*methodType)(void);
	BEFORE_EVAS_GL(evas_gl_config_new);
	Evas_GL_Config *res = evas_gl_config_newp();
	AFTER_NO_PARAM('p', voidp_to_uint64(res), APITYPE_CONTEXT, "");
	return res;
}

Evas_GL_Surface *evas_gl_surface_create(Evas_GL *evas_gl, Evas_GL_Config *cfg,
					int w, int h)
{
	typedef Evas_GL_Surface *(*methodType)(Evas_GL *evas_gl,
					       Evas_GL_Config *cfg,
					       int w, int h);
	BEFORE_EVAS_GL(evas_gl_surface_create);
	Evas_GL_Surface *res = evas_gl_surface_createp(evas_gl, cfg, w, h);
	AFTER('p', voidp_to_uint64(res), APITYPE_CONTEXT, "", "ppdd",
	      voidp_to_uint64(evas_gl), voidp_to_uint64(cfg), w, h);
	return res;
}

Evas_GL_Context *evas_gl_context_create(Evas_GL *evas_gl,
					Evas_GL_Context *share_ctx)
{
	typedef Evas_GL_Context *(*methodType)(Evas_GL *evas_gl,
					       Evas_GL_Context *share_ctx);
	BEFORE_EVAS_GL(evas_gl_context_create);
	Evas_GL_Context *res = evas_gl_context_createp(evas_gl, share_ctx);
	AFTER('p', voidp_to_uint64(res), APITYPE_CONTEXT, "", "pp",
	      voidp_to_uint64(evas_gl), voidp_to_uint64(share_ctx));
	return res;
}

Eina_Bool evas_gl_make_current(Evas_GL *evas_gl, Evas_GL_Surface *surf,
			       Evas_GL_Context *ctx)
{
	typedef Eina_Bool (*methodType)(Evas_GL *evas_gl, Evas_GL_Surface *surf,
					Evas_GL_Context *ctx);
	BEFORE_EVAS_GL(evas_gl_make_current);
	Eina_Bool res = evas_gl_make_currentp(evas_gl, surf, ctx);
	AFTER('b', res, APITYPE_CONTEXT, "", "ppp",
	      voidp_to_uint64(evas_gl), voidp_to_uint64(surf), voidp_to_uint64(ctx));
	return res;
}

const char *evas_gl_string_query(Evas_GL *evas_gl, int name)
{
	typedef const char *(*methodType)(Evas_GL *evas_gl, int name);
	BEFORE_EVAS_GL(evas_gl_string_query);
	const char *res = evas_gl_string_queryp(evas_gl, name);
	AFTER('s', res, APITYPE_CONTEXT, "", "pd",
	      voidp_to_uint64(evas_gl), name);
	return res;
}

Evas_GL_Func evas_gl_proc_address_get(Evas_GL *evas_gl, const char *name)
{
	typedef Evas_GL_Func(*methodType)(Evas_GL *evas_gl, const char *name);
	BEFORE_EVAS_GL(evas_gl_proc_address_get);
	Evas_GL_Func res = evas_gl_proc_address_getp(evas_gl, name);
	AFTER('p', voidp_to_uint64(res), APITYPE_CONTEXT, "", "ps",
	      voidp_to_uint64(evas_gl), name);
	return res;
}

Eina_Bool evas_gl_native_surface_get(Evas_GL *evas_gl, Evas_GL_Surface *surf,
				     Evas_Native_Surface *ns)
{
	typedef Eina_Bool(*methodType)(Evas_GL *evas_gl, Evas_GL_Surface *surf,
				   Evas_Native_Surface *ns);
	BEFORE_EVAS_GL(evas_gl_native_surface_get);
	Eina_Bool res = evas_gl_native_surface_getp(evas_gl, surf, ns);
	AFTER('b', res, APITYPE_CONTEXT, "", "ppp",
	      voidp_to_uint64(evas_gl), voidp_to_uint64(surf), voidp_to_uint64(ns));
	return res;
}

/* ----------------- api get functions -------------- */

Evas_GL_API *evas_gl_api_get(Evas_GL *evas_gl)
{
	typedef Evas_GL_API *(*methodType)(Evas_GL *evas_gl);
	BEFORE_EVAS_GL(evas_gl_api_get);
	Evas_GL_API *res = evas_gl_api_getp(evas_gl);

	/* save original api functions and rewrite it by probes */
	if (res != NULL) {
		save_orig_gl_api_list(res);
		change_gl_api_list(res);
	}

	AFTER('p', res, APITYPE_CONTEXT, "", "p",
	      voidp_to_uint64(evas_gl));

	return res;
}

Evas_GL_API* elm_glview_gl_api_get(const Evas_Object *obj)
{
	typedef Evas_GL_API *(*methodType)(const Evas_Object *obj);
	BEFORE_EVAS_GL(elm_glview_gl_api_get);
	Evas_GL_API *res = elm_glview_gl_api_getp(obj);

	/* save original api functions and rewrite it by probes */
	if (res != NULL) {
		save_orig_gl_api_list(res);
		change_gl_api_list(res);
	}

	/*clean error code*/
	__gl_api->glGetError();

	AFTER('p', res, APITYPE_CONTEXT, "", "p",
	      voidp_to_uint64(obj));

	return res;
}

Evas_GL_API *evas_gl_context_api_get(Evas_GL *evas_gl, Evas_GL_Context *ctx)
{
	typedef Evas_GL_API *(*methodType)(Evas_GL *evas_gl, Evas_GL_Context *ctx);
	static methodType evas_gl_context_api_getp = 0;

	GET_REAL_FUNC_RTLD_NEXT(evas_gl_context_api_get);

	Evas_GL_API *res = evas_gl_context_api_getp(evas_gl, ctx);

	/* save original api functions and rewrite it by probes */
	if (res != NULL) {
		save_orig_gl_api_list(res);
		change_gl_api_list(res);
	}

	return res;
}
