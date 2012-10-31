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

#include "utils/log.h"

#include "javascript/jsapi.h"
#include "javascript/jsapi/binding.h"


struct jsclass_private {
	struct browser_window *bw;
	struct html_content *htmlc;
	JSObject *document_obj;
	JSObject *navigator_obj;
	JSObject *console_obj;
};

static void jsclass_finalize(JSContext *cx, JSObject *obj);
static JSBool jsclass_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags, JSObject **objp);

JSClass JSClass_Window = {
	"Window",
	JSCLASS_NEW_RESOLVE | JSCLASS_HAS_PRIVATE | JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub,
	JS_PropertyStub,
	JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub,
	(JSResolveOp)jsclass_resolve,
	JS_ConvertStub,
	jsclass_finalize,
	JSCLASS_NO_OPTIONAL_MEMBERS
};


static JSBool JSAPI_NATIVE(alert, JSContext *cx, uintN argc, jsval *vp)
{
	JSString* u16_txt;
	char *txt;
	unsigned long length;

	if (!JS_ConvertArguments(cx, argc, JSAPI_ARGV(cx, vp), "S", &u16_txt))
		return JS_FALSE;

	JSString_to_char(u16_txt, txt, length);

	warn_user(txt, NULL);

	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(confirm, JSContext *cx, uintN argc, jsval *vp)
{
	JSString* u16_txt;
	char *txt;
	unsigned long length;
	JSBool result = JS_FALSE;

	if (!JS_ConvertArguments(cx, argc, JSAPI_ARGV(cx, vp), "S", &u16_txt))
		return JS_FALSE;

	JSString_to_char(u16_txt, txt, length);

	warn_user(txt, NULL);

	JSAPI_SET_RVAL(cx, vp, BOOLEAN_TO_JSVAL(result));

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(prompt, JSContext *cx, uintN argc, jsval *vp)
{
	JSString* u16_txt;
	char *txt;
	unsigned long length;

	if (!JS_ConvertArguments(cx, argc, JSAPI_ARGV(cx, vp), "S", &u16_txt))
		return JS_FALSE;

	JSString_to_char(u16_txt, txt, length);

	warn_user(txt, NULL);

	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(close, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(stop, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(focus, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(blur, JSContext *cx, uintN argc, jsval *vp)
{
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSFunctionSpec jsfunctions_window[] =
{
	JSAPI_FS(close, 0, 0),
	JSAPI_FS(stop, 0, 0),
	JSAPI_FS(focus, 0, 0),
	JSAPI_FS(blur, 0, 0),
	JSAPI_FS(alert, 1, 0),
	JSAPI_FS(confirm, 1, 0),
	JSAPI_FS(prompt, 1, 0),
	JSAPI_FS_END
};


static JSBool JSAPI_PROPERTYGET(window, JSContext *cx, JSObject *obj, jsval *vp)
{
	JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}

static JSBool JSAPI_PROPERTYGET(self, JSContext *cx, JSObject *obj, jsval *vp)
{
	JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}

static JSBool JSAPI_PROPERTYGET(document, JSContext *cx, JSObject *obj, jsval *vp)
{
	struct jsclass_private *private;

	private = JS_GetInstancePrivate(cx,
			obj,
			&JSClass_Window,
			NULL);
	if (private == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(private->document_obj));
	return JS_TRUE;
}

static JSPropertySpec jsproperties_window[] =
{
	JSAPI_PS_RO(document, 0, JSPROP_ENUMERATE | JSPROP_SHARED),
	JSAPI_PS_RO(window, 0, JSPROP_ENUMERATE | JSPROP_SHARED),
	JSAPI_PS_RO(self, 0, JSPROP_ENUMERATE | JSPROP_SHARED),
	JSAPI_PS_END
};

static JSBool jsclass_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags, JSObject **objp)
{
	*objp = NULL;
	return JS_TRUE;
}

static void jsclass_finalize(JSContext *cx, JSObject *obj)
{	struct jsclass_private *private;

	private = JS_GetInstancePrivate(cx, obj, &JSClass_Window, NULL);
	if (private != NULL) {
		free(private);
	}
}

JSObject *jsapi_InitClass_Window(JSContext *cx, JSObject *parent)
{
	JSObject *window = NULL;
	JSObject *proto;

	window = JS_NewCompartmentAndGlobalObject(cx, &JSClass_Window, NULL);
	if (window == NULL) {
		return NULL;
	}

	/** @todo reconsider global object handling. future
	 * editions of spidermonkey appear to be removing the
	 * idea of a global so we probably need to handle
	 * global object references internally
	 */

	/* set the contexts global */
	JS_SetGlobalObject(cx, window);

	/* Populate the global object with the standard globals, like
	 *  Object and Array.
	 */
	if (!JS_InitStandardClasses(cx, window)) {
		return NULL;
	}

	/* Initialises all the user javascript classes to make their
	 * prototypes available. 
	 */
	/** @todo should we be managing these prototype objects ourselves */
	proto = jsapi_InitClass_Document(cx, window);
	if (proto == NULL) {
		return NULL;
	}

	return window;
}

JSObject *jsapi_new_Window(JSContext *cx, 
			    JSObject *window,
			    JSObject *parent, 
			    struct browser_window *bw, 
			    html_content *htmlc)
{
	struct jsclass_private *private;

	/* @todo sort out windows that are not globals */
	assert(parent == NULL);

	/* create private data */
	private = malloc(sizeof(struct jsclass_private));
	if (private == NULL) {
		return NULL;
	}
	private->bw = bw;
	private->htmlc = htmlc;


	/* instantiate the subclasses off the window global */
	private->document_obj = jsapi_new_Document(cx,
						   NULL,
						   window, 
						   htmlc->document, 
						   htmlc);
	if (private->document_obj == NULL) { 
		free(private);
		return NULL;
	}

	private->navigator_obj = jsapi_new_Navigator(cx, window);
	if (private->navigator_obj == NULL) {
		free(private);
		return NULL;
	}

	/** @todo forms, history, location */

	private->console_obj = jsapi_new_Console(cx, window);
	if (private->console_obj == NULL) {
		free(private);
		return NULL;
	}

	/* private pointer to browsing context */
	if (!JS_SetPrivate(cx, window, private))
		return NULL;

	/* functions */
	if (!JS_DefineFunctions(cx, window, jsfunctions_window)) {
		return NULL;
	}

	/* properties */
	if (!JS_DefineProperties(cx, window, jsproperties_window))
		return NULL;


	LOG(("Created new window object %p", window));

	return window;
}
