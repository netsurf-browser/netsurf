/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * spidermonkey jsapi bindings and compatability glue.
 */

#ifndef _NETSURF_JAVASCRIPT_JSAPI_H_
#define _NETSURF_JAVASCRIPT_JSAPI_H_

#ifdef WITH_MOZJS
#include "js/jsapi.h"
#else
#include "mozjs/jsapi.h"
#endif

#include "render/html_internal.h"

#if JS_VERSION <= 180

#include <string.h>

/* *CAUTION* these macros introduce and use jsthis and jsrval
 * parameters, native function code should not conflict with these
 */

/* five parameter jsapi native call */
#define JSAPI_NATIVE(name, cx, argc, vp) \
	jsapi_native_##name(cx, JSObject *jsthis, argc, vp, jsval *jsrval)

/* five parameter function descriptor */
#define JSAPI_FS(name, nargs, flags) \
	JS_FS(#name, jsapi_native_##name, nargs, flags, 0)

/* function descriptor end */
#define JSAPI_FS_END JS_FS_END

/* return value */
#define JSAPI_RVAL(cx, vp) JS_RVAL(cx, jsrval)

/* return value setter */
#define JSAPI_SET_RVAL(cx, vp, v) JS_SET_RVAL(cx, jsrval, v)

/* arguments */
#define JSAPI_ARGV(cx, vp) (vp)

/* proprty native calls */
#define JSAPI_PROPERTYGET(name, cx, obj, vp) \
	jsapi_property_##name##_get(cx, obj, jsval id, vp)
#define JSAPI_PROPERTYSET(name, cx, obj, vp) \
	jsapi_property_##name##_set(cx, obj, jsval id, vp)

/* property specifier */
#define JSAPI_PS(name, tinyid, flags) \
	{ #name , tinyid , flags , jsapi_property_##name##_get , jsapi_property_##name##_set }

#define JSAPI_PS_RO(name, tinyid, flags) \
	{ #name , tinyid , flags | JSPROP_READONLY, jsapi_property_##name##_get , NULL }

#define JSAPI_PS_END { NULL, 0, 0, NULL, NULL }

static inline JSObject *
JS_NewCompartmentAndGlobalObject(JSContext *cx,
				 JSClass *jsclass,
				 JSPrincipals *principals)
{
	JSObject *global;
	global = JS_NewObject(cx, jsclass, NULL, NULL);
	if (global == NULL) {
		return NULL;
	}
	return global;
}

#define JS_StrictPropertyStub JS_PropertyStub

#define JSString_to_char(injsstring, outchar, outlen)	\
	outchar = JS_GetStringBytes(injsstring);		\
	outlen = strlen(outchar)

#else /* #if JS_VERSION <= 180 */

/* three parameter jsapi native call */
#define JSAPI_NATIVE(name, cx, argc, vp) jsapi_native_##name(cx, argc, vp)

/* three parameter function descriptor */
#define JSAPI_FS(name, nargs, flags) \
	JS_FS(#name, jsapi_native_##name, nargs, flags)

/* function descriptor end */
#define JSAPI_FS_END JS_FS_END

/* return value */
#define JSAPI_RVAL JS_RVAL

/* return value setter */
#define JSAPI_SET_RVAL JS_SET_RVAL

/* arguments */
#define JSAPI_ARGV(cx, vp) JS_ARGV(cx,vp)

/* proprty native calls */
#define JSAPI_PROPERTYGET(name, cx, obj, vp) \
	jsapi_property_##name##_get(cx, obj, jsid id, vp)
#define JSAPI_PROPERTYSET(name, cx, obj, vp) \
	jsapi_property_##name##_set(cx, obj, jsid id, JSBool strict, vp)

/* property specifier */
#define JSAPI_PS(name, tinyid, flags) {				\
		#name ,						\
		tinyid ,					\
		flags ,						\
		jsapi_property_##name##_get ,			\
		jsapi_property_##name##_set			\
	}

#define JSAPI_PS_RO(name, tinyid, flags) {			\
		#name ,						\
		tinyid ,					\
		flags | JSPROP_READONLY,			\
		jsapi_property_##name##_get ,			\
		NULL						\
	}

#define JSAPI_PS_END { NULL, 0, 0, NULL, NULL }


#define JSString_to_char(injsstring, outchar, outlen)		\
	outlen = JS_GetStringLength(injsstring);		\
	outchar = alloca(sizeof(char)*(outlen+1));		\
	JS_EncodeStringToBuffer(injsstring, outchar, outlen);	\
	outchar[outlen] = '\0'


#endif

/** Create a new javascript window object
 *
 * @param cx The javascript context.
 * @param parent The parent object or NULL for new global
 * @param win_priv The private context to set on the object
 * @return new javascript object or NULL on error
 */
JSObject *jsapi_new_window(JSContext *cx, JSObject *parent, void *win_priv);

/** Create a new javascript document object
 *
 * @param cx The javascript context.
 * @param parent The parent object, usually a global window object
 * @param doc_priv The private context to set on the object
 * @return new javascript object or NULL on error
 */
JSObject *jsapi_new_document(JSContext *cx, JSObject *parent, struct html_content *htmlc);

/** Create a new javascript console object
 *
 * @param cx The javascript context.
 * @param parent The parent object, usually a global window object
 * @return new javascript object or NULL on error
 */
JSObject *jsapi_new_console(JSContext *cx, JSObject *parent);

/** Create a new javascript navigator object
 *
 * @param cx The javascript context.
 * @param parent The parent object, usually a global window object
 * @return new javascript object or NULL on error
 */
JSObject *jsapi_new_navigator(JSContext *cx, JSObject *parent);

/** Create a new javascript element object
 *
 * @param cx The javascript context.
 * @param parent The parent object, usually a global window object
 * @param doc_priv The private context to set on the object
 * @return new javascript object or NULL on error
 */
JSObject *jsapi_new_element(JSContext *cx, JSObject *parent, struct html_content *htmlc, struct dom_element *domelement);

#endif
