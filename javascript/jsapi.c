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

#include "content/content.h"
#include "javascript/content.h"
#include "javascript/js.h"

#include "utils/log.h"

static JSRuntime *rt; /* global runtime */

void js_initialise(void)
{
	/* Create a JS runtime. */

#if JS_VERSION >= 180
        JS_SetCStringsAreUTF8(); /* we prefer our runtime to be utf-8 */
#endif

	rt = JS_NewRuntime(8L * 1024L * 1024L);
	LOG(("New runtime handle %p", rt));

	/* register script content handler */
	javascript_init();
}

void js_finalise(void)
{
	if (rt != NULL) {
		LOG(("destroying runtime handle %p", rt));
		JS_DestroyRuntime(rt);
	}
	JS_ShutDown();
}

/* The error reporter callback. */
static void js_reportError(JSContext *cx, const char *message, JSErrorReport *report)
{
	LOG(("%s:%u:%s",
            report->filename ? report->filename : "<no filename>",
            (unsigned int) report->lineno,
	     message));
}

jscontext *js_newcontext(void)
{
	JSContext *cx;

	if (rt == NULL) {
		return NULL;
	}

	cx = JS_NewContext(rt, 8192);
	if (cx == NULL) {
		return NULL;
	}
	JS_SetOptions(cx, JSOPTION_VAROBJFIX | JSOPTION_JIT );
	JS_SetVersion(cx, JSVERSION_LATEST);
	JS_SetErrorReporter(cx, js_reportError);

	LOG(("New Context %p", cx));

	return (jscontext *)cx;
}

void js_destroycontext(jscontext *ctx)
{
	JSContext *cx = (JSContext *)ctx;
	if (cx != NULL) {
		LOG(("Destroying Context %p", cx));
		JS_DestroyContext(cx);
	}
}



jsobject *js_newcompartment(jscontext *ctx, void *win_priv, void *doc_priv)
{
	JSContext *cx = (JSContext *)ctx;
	JSObject *window_obj = NULL;
	JSObject *document_obj;
	JSObject *console_obj;

	if (cx == NULL)
		goto js_newcompartment_fail;

	/* create the window object as the global */
	window_obj = jsapi_new_window(cx, NULL, win_priv);
	if (window_obj == NULL) 
		goto js_newcompartment_fail;

	/* attach the subclasses off the window global */
	document_obj = jsapi_new_document(cx, window_obj, doc_priv);
	if (document_obj == NULL) 
		goto js_newcompartment_fail;

	/* @todo forms, history, location */

	console_obj = jsapi_new_console(cx, window_obj);
	if (console_obj == NULL) 
		goto js_newcompartment_fail;

	return (jsobject *)window_obj;

js_newcompartment_fail:

	return NULL;
}

bool js_exec(jscontext *ctx, const char *txt, size_t txtlen)
{
	JSContext *cx = (JSContext *)ctx;

	//LOG(("%p \"%s\"",cx ,txt));

	if (ctx == NULL) {
		return false;
	}

	if (txt == NULL) {
		return false;
	}

	if (txtlen == 0) {
		return false;
	}

	if (JS_EvaluateScript(cx, 
			      JS_GetGlobalObject(cx), 
			      txt, txtlen, 
			      "<head>", 0, NULL) == JS_TRUE) {
		return true;
	}

	return false;
}
