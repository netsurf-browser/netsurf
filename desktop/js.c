#include "mozjs/jsapi.h"

#include "desktop/js.h"
#include "utils/log.h"

static JSRuntime *rt; /* global runtime */

void js_initialise(void)
{
	/* Create a JS runtime. */
	rt = JS_NewRuntime(8L * 1024L * 1024L);
	LOG(("New runtime handle %p", rt));
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
	LOG(("%s:%u:%s\n",
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
    "global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};


jsobject *js_newcompartment(jscontext *ctx)
{
	JSContext *cx = (JSContext *)ctx;
	JSObject *global;
	
	if (cx == NULL) {
		return NULL;
	}
#ifdef HAVE_JS_NEWCOMPARTMENTANDGLOBALOBJECT
	global = JS_NewCompartmentAndGlobalObject(cx, &global_class, NULL);
	if (global == NULL) {
		return NULL;
	}
#else
	global = JS_NewObject(cx, &global_class, NULL, NULL);
	if (global == NULL) {
		return NULL;
	}
	JS_SetGlobalObject(cx, global);
#endif

	/* Populate the global object with the standard globals,
	   like Object and Array. */
	if (!JS_InitStandardClasses(cx, global)) {
		return NULL;
	}

	LOG(("Creating new global object %p", global));

	return (jsobject *)global;
}
