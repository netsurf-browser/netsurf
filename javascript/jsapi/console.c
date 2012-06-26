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

#include "mozjs/jsapi.h"

#include "content/content.h"
#include "javascript/jsapi.h"
#include "utils/log.h"

static JSBool jsdebug(JSContext *cx, uintN argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool jsdir(JSContext *cx, uintN argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool jserror(JSContext *cx, uintN argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool jsgroup(JSContext *cx, uintN argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool jsgroupCollapsed(JSContext *cx, uintN argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool jsgroupEnd(JSContext *cx, uintN argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool jsinfo(JSContext *cx, uintN argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool jslog(JSContext *cx, uintN argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool jstime(JSContext *cx, uintN argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool jstimeEnd(JSContext *cx, uintN argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool jstrace(JSContext *cx, uintN argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool jswarn(JSContext *cx, uintN argc, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}

static JSFunctionSpec jsfunctions_console[] = {
	JS_FS("debug", jsdebug, 1, 0),
	JS_FS("dir", jsdir, 1, 0),
	JS_FS("error", jserror, 1, 0),
	JS_FS("group", jsgroup, 1, 0),
	JS_FS("groupCollapsed", jsgroupCollapsed, 1, 0),
	JS_FS("groupEnd", jsgroupEnd, 1, 0),
	JS_FS("info", jsinfo, 1, 0),
	JS_FS("log", jslog, 1, 0),
	JS_FS("time", jstime, 1, 0),
	JS_FS("timeEnd", jstimeEnd, 1, 0),
	JS_FS("trace", jstrace, 1, 0),
	JS_FS("warn", jswarn, 1, 0),
	JS_FS_END
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
