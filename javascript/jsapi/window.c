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

static JSBool jsalert(JSContext *cx, uintN argc, jsval *vp)
{
	JSString* u16_txt;
	char *txt;

	if (!JS_ConvertArguments(cx, argc, JS_ARGV(cx, vp), "S", &u16_txt))
		return JS_FALSE;


#if JS_VERSION <= 180
	txt = JS_GetStringBytes(u16_txt);
#else
	unsigned int length;
	length = JS_GetStringLength(u16_txt);
	txt = alloca(sizeof(char)*(length+1));
	JS_EncodeStringToBuffer(u16_txt, txt, length);
	txt[length] = '\0';
#endif

	warn_user(txt, NULL);

	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	
	return JS_TRUE;
}

static JSFunctionSpec jsfunctions_window[] =
{
	JS_FN("alert", jsalert, 1, 0),
	JS_FS_END
};

/* The class of the global object. */
static JSClass jsclass_window = {
	"window", 
	JSCLASS_HAS_PRIVATE | JSCLASS_GLOBAL_FLAGS,
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


JSObject * jsapi_new_window(JSContext *cx, JSObject *parent, void *win_priv)
{
	JSObject *window = NULL;
	
	if (parent == NULL) {
		window = JS_NewCompartmentAndGlobalObject(cx, &jsclass_window, NULL);
		if (window == NULL) {
			return NULL;
		}

		/* Populate the global object with the standard globals, like
		   Object and Array. */
		if (!JS_InitStandardClasses(cx, window)) {
			return NULL;
		}

	} else {
		/* @todo sort out windows that are not globals */
		assert(false);
	}

	if (!JS_DefineFunctions(cx, window, jsfunctions_window)) {
		return NULL;
	}

	/* private pointer to browsing context */
	if (!JS_SetPrivate(cx, window, win_priv))
		return NULL;

	LOG(("Created new window object %p", window));

	return window;
}
