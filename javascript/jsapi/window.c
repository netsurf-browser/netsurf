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
#include "utils/log.h"

/* IDL

[NamedPropertiesObject]
interface Window : EventTarget {
  // the current browsing context
  [Unforgeable] readonly attribute WindowProxy window;
  [Replaceable] readonly attribute WindowProxy self;
  [Unforgeable] readonly attribute Document document;
	   attribute DOMString name;
  [PutForwards=href, Unforgeable] readonly attribute Location location;
  readonly attribute History history;

  boolean find(optional DOMString aString, optional boolean aCaseSensitive, optional boolean aBackwards, optional boolean aWrapAround, optional boolean aWholeWord, optional boolean aSearchInFrames, optional boolean aShowDialog);

  [Replaceable] readonly attribute BarProp locationbar;
  [Replaceable] readonly attribute BarProp menubar;
  [Replaceable] readonly attribute BarProp personalbar;
  [Replaceable] readonly attribute BarProp scrollbars;
  [Replaceable] readonly attribute BarProp statusbar;
  [Replaceable] readonly attribute BarProp toolbar;
	   attribute DOMString status;
  void close();
  void stop();
  void focus();
  void blur();

  // other browsing contexts
  [Replaceable] readonly attribute WindowProxy frames;
  [Replaceable] readonly attribute unsigned long length;
  [Unforgeable] readonly attribute WindowProxy top;
	   attribute WindowProxy? opener;
  readonly attribute WindowProxy parent;
  readonly attribute Element? frameElement;
  WindowProxy open(optional DOMString url, optional DOMString target, optional DOMString features, optional boolean replace);
  getter WindowProxy (unsigned long index);
  getter object (DOMString name);

  // the user agent
  readonly attribute Navigator navigator;
  readonly attribute External external;
  readonly attribute ApplicationCache applicationCache;

  // user prompts
  void alert(DOMString message);
  boolean confirm(DOMString message);
  DOMString? prompt(DOMString message, optional DOMString default);
  void print();
  any showModalDialog(DOMString url, optional any argument);

  // cross-document messaging
  void postMessage(any message, DOMString targetOrigin, optional sequence<Transferable> transfer);

  // event handler IDL attributes
  [TreatNonCallableAsNull] attribute Function? onabort;
  [TreatNonCallableAsNull] attribute Function? onafterprint;
  [TreatNonCallableAsNull] attribute Function? onbeforeprint;
  [TreatNonCallableAsNull] attribute Function? onbeforeunload;
  [TreatNonCallableAsNull] attribute Function? onblur;
  [TreatNonCallableAsNull] attribute Function? oncancel;
  [TreatNonCallableAsNull] attribute Function? oncanplay;
  [TreatNonCallableAsNull] attribute Function? oncanplaythrough;
  [TreatNonCallableAsNull] attribute Function? onchange;
  [TreatNonCallableAsNull] attribute Function? onclick;
  [TreatNonCallableAsNull] attribute Function? onclose;
  [TreatNonCallableAsNull] attribute Function? oncontextmenu;
  [TreatNonCallableAsNull] attribute Function? oncuechange;
  [TreatNonCallableAsNull] attribute Function? ondblclick;
  [TreatNonCallableAsNull] attribute Function? ondrag;
  [TreatNonCallableAsNull] attribute Function? ondragend;
  [TreatNonCallableAsNull] attribute Function? ondragenter;
  [TreatNonCallableAsNull] attribute Function? ondragleave;
  [TreatNonCallableAsNull] attribute Function? ondragover;
  [TreatNonCallableAsNull] attribute Function? ondragstart;
  [TreatNonCallableAsNull] attribute Function? ondrop;
  [TreatNonCallableAsNull] attribute Function? ondurationchange;
  [TreatNonCallableAsNull] attribute Function? onemptied;
  [TreatNonCallableAsNull] attribute Function? onended;
  [TreatNonCallableAsNull] attribute Function? onerror;
  [TreatNonCallableAsNull] attribute Function? onfocus;
  [TreatNonCallableAsNull] attribute Function? onhashchange;
  [TreatNonCallableAsNull] attribute Function? oninput;
  [TreatNonCallableAsNull] attribute Function? oninvalid;
  [TreatNonCallableAsNull] attribute Function? onkeydown;
  [TreatNonCallableAsNull] attribute Function? onkeypress;
  [TreatNonCallableAsNull] attribute Function? onkeyup;
  [TreatNonCallableAsNull] attribute Function? onload;
  [TreatNonCallableAsNull] attribute Function? onloadeddata;
  [TreatNonCallableAsNull] attribute Function? onloadedmetadata;
  [TreatNonCallableAsNull] attribute Function? onloadstart;
  [TreatNonCallableAsNull] attribute Function? onmessage;
  [TreatNonCallableAsNull] attribute Function? onmousedown;
  [TreatNonCallableAsNull] attribute Function? onmousemove;
  [TreatNonCallableAsNull] attribute Function? onmouseout;
  [TreatNonCallableAsNull] attribute Function? onmouseover;
  [TreatNonCallableAsNull] attribute Function? onmouseup;
  [TreatNonCallableAsNull] attribute Function? onmousewheel;
  [TreatNonCallableAsNull] attribute Function? onoffline;
  [TreatNonCallableAsNull] attribute Function? ononline;
  [TreatNonCallableAsNull] attribute Function? onpause;
  [TreatNonCallableAsNull] attribute Function? onplay;
  [TreatNonCallableAsNull] attribute Function? onplaying;
  [TreatNonCallableAsNull] attribute Function? onpagehide;
  [TreatNonCallableAsNull] attribute Function? onpageshow;
  [TreatNonCallableAsNull] attribute Function? onpopstate;
  [TreatNonCallableAsNull] attribute Function? onprogress;
  [TreatNonCallableAsNull] attribute Function? onratechange;
  [TreatNonCallableAsNull] attribute Function? onreset;
  [TreatNonCallableAsNull] attribute Function? onresize;
  [TreatNonCallableAsNull] attribute Function? onscroll;
  [TreatNonCallableAsNull] attribute Function? onseeked;
  [TreatNonCallableAsNull] attribute Function? onseeking;
  [TreatNonCallableAsNull] attribute Function? onselect;
  [TreatNonCallableAsNull] attribute Function? onshow;
  [TreatNonCallableAsNull] attribute Function? onstalled;
  [TreatNonCallableAsNull] attribute Function? onstorage;
  [TreatNonCallableAsNull] attribute Function? onsubmit;
  [TreatNonCallableAsNull] attribute Function? onsuspend;
  [TreatNonCallableAsNull] attribute Function? ontimeupdate;
  [TreatNonCallableAsNull] attribute Function? onunload;
  [TreatNonCallableAsNull] attribute Function? onvolumechange;
  [TreatNonCallableAsNull] attribute Function? onwaiting;
};

*/


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

static JSFunctionSpec jsfunctions_window[] =
{
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

static JSBool JSAPI_PROPERTYSET(window, JSContext *cx, JSObject *obj, jsval *vp)
{
	return JS_FALSE;
}

static JSBool JSAPI_PROPERTYGET(self, JSContext *cx, JSObject *obj, jsval *vp)
{
	JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
	return JS_TRUE;
}

static JSBool JSAPI_PROPERTYSET(self, JSContext *cx, JSObject *obj, jsval *vp)
{
	return JS_FALSE;
}


static JSPropertySpec jsproperties_window[] =
{
	JSAPI_PS(window, 0, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED),
	JSAPI_PS(self, 0, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED),
	JSAPI_PS_END
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

	/* private pointer to browsing context */
	if (!JS_SetPrivate(cx, window, win_priv))
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
