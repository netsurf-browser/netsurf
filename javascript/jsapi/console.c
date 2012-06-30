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

#include "javascript/jsapi.h"

//#include "content/content.h"
#include "utils/log.h"

static JSBool JSAPI_NATIVE(debug, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(dir, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(error, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(group, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(groupCollapsed, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(groupEnd, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(info, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(log, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(time, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(timeEnd, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(trace, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(warn, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSFunctionSpec jsfunctions_console[] = {
	JSAPI_FS(debug, 1, 0),
	JSAPI_FS(dir, 1, 0),
	JSAPI_FS(error, 1, 0),
	JSAPI_FS(group, 1, 0),
	JSAPI_FS(groupCollapsed, 1, 0),
	JSAPI_FS(groupEnd, 1, 0),
	JSAPI_FS(info, 1, 0),
	JSAPI_FS(log, 1, 0),
	JSAPI_FS(time, 1, 0),
	JSAPI_FS(timeEnd, 1, 0),
	JSAPI_FS(trace, 1, 0),
	JSAPI_FS(warn, 1, 0),
	JSAPI_FS_END
};

static JSClass jsclass_console =
{
        "console", 
	JSCLASS_HAS_PRIVATE, 
	JS_PropertyStub, 
	JS_PropertyStub, 
	JS_PropertyStub, 
	JS_StrictPropertyStub,
        JS_EnumerateStub, 
	JS_ResolveStub, 
	JS_ConvertStub, 
	JS_FinalizeStub, 
	JSCLASS_NO_OPTIONAL_MEMBERS
};


JSObject *jsapi_new_console(JSContext *cx, JSObject *parent)
{
	return JS_InitClass(cx, 
		     parent, 
		     NULL, 
		     &jsclass_console, 
		     NULL, 
		     0, 
		     NULL, 
		     jsfunctions_console, 
		     NULL, 
		     NULL);
}
