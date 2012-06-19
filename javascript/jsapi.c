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
#include "javascript/content.h"
#include "javascript/jsapi.h"
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



/* The class of the global object. */
static JSClass global_class = {
	"global", 
	JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub, 
	JS_PropertyStub, 
	JS_PropertyStub, 
#if JS_VERSION <= 180
	JS_PropertyStub,
#else
	JS_StrictPropertyStub,
#endif
	JS_EnumerateStub, 
	JS_ResolveStub, 
	JS_ConvertStub, 
	JS_FinalizeStub,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

jsobject *js_newcompartment(jscontext *ctx, struct content* c)
{
	JSContext *cx = (JSContext *)ctx;
	JSObject *global;
	
	if (cx == NULL) {
		return NULL;
	}

#if JS_VERSION <= 180
	global = JS_NewObject(cx, &global_class, NULL, NULL);
	if (global == NULL) {
		return NULL;
	}
	JS_SetGlobalObject(cx, global);
#else
	global = JS_NewCompartmentAndGlobalObject(cx, &global_class, NULL);
	if (global == NULL) {
		return NULL;
	}
#endif

	JS_SetContextPrivate(cx, c); /* private pointer to content */

	jsapi_new_globalfunc(cx, global);

	/* Populate the global object with the standard globals, like
	   Object and Array. */
	if (!JS_InitStandardClasses(cx, global)) {
		return NULL;
	}

	LOG(("Created new global object %p", global));

	return (jsobject *)global;
}

bool js_exec(jscontext *ctx, const char *txt, int txtlen)
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
