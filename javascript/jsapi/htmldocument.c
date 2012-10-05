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

#include <dom/dom.h>

#include "utils/config.h"
#include "utils/log.h"

#include "javascript/jsapi.h"

/* IDL http://www.whatwg.org/specs/web-apps/current-work/#the-document-object

[OverrideBuiltins]
partial interface Document {
  // resource metadata management
  [PutForwards=href] readonly attribute Location? location;
           attribute DOMString domain;
  readonly attribute DOMString referrer;
           attribute DOMString cookie;
  readonly attribute DOMString lastModified;
  readonly attribute DOMString readyState;

  // DOM tree accessors
  getter object (DOMString name);
           attribute DOMString title;
           attribute DOMString dir;
           attribute HTMLElement? body;
  readonly attribute HTMLHeadElement? head;
  readonly attribute HTMLCollection images;
  readonly attribute HTMLCollection embeds;
  readonly attribute HTMLCollection plugins;
  readonly attribute HTMLCollection links;
  readonly attribute HTMLCollection forms;
  readonly attribute HTMLCollection scripts;
  NodeList getElementsByName(DOMString elementName);
  NodeList getItems(optional DOMString typeNames); // microdata 
  readonly attribute DOMElementMap cssElementMap;

  // dynamic markup insertion
  Document open(optional DOMString type, optional DOMString replace);
  WindowProxy open(DOMString url, DOMString name, DOMString features, optional boolean replace);
  void close();
  void write(DOMString... text);
  void writeln(DOMString... text);

  // user interaction
  readonly attribute WindowProxy? defaultView;
  readonly attribute Element? activeElement;
  boolean hasFocus();
           attribute DOMString designMode;
  boolean execCommand(DOMString commandId);
  boolean execCommand(DOMString commandId, boolean showUI);
  boolean execCommand(DOMString commandId, boolean showUI, DOMString value);
  boolean queryCommandEnabled(DOMString commandId);
  boolean queryCommandIndeterm(DOMString commandId);
  boolean queryCommandState(DOMString commandId);
  boolean queryCommandSupported(DOMString commandId);
  DOMString queryCommandValue(DOMString commandId);
  readonly attribute HTMLCollection commands;

  // event handler IDL attributes
           attribute EventHandler onabort;
           attribute EventHandler onblur;
           attribute EventHandler oncancel;
           attribute EventHandler oncanplay;
           attribute EventHandler oncanplaythrough;
           attribute EventHandler onchange;
           attribute EventHandler onclick;
           attribute EventHandler onclose;
           attribute EventHandler oncontextmenu;
           attribute EventHandler oncuechange;
           attribute EventHandler ondblclick;
           attribute EventHandler ondrag;
           attribute EventHandler ondragend;
           attribute EventHandler ondragenter;
           attribute EventHandler ondragleave;
           attribute EventHandler ondragover;
           attribute EventHandler ondragstart;
           attribute EventHandler ondrop;
           attribute EventHandler ondurationchange;
           attribute EventHandler onemptied;
           attribute EventHandler onended;
           attribute OnErrorEventHandler onerror;
           attribute EventHandler onfocus;
           attribute EventHandler oninput;
           attribute EventHandler oninvalid;
           attribute EventHandler onkeydown;
           attribute EventHandler onkeypress;
           attribute EventHandler onkeyup;
           attribute EventHandler onload;
           attribute EventHandler onloadeddata;
           attribute EventHandler onloadedmetadata;
           attribute EventHandler onloadstart;
           attribute EventHandler onmousedown;
           attribute EventHandler onmousemove;
           attribute EventHandler onmouseout;
           attribute EventHandler onmouseover;
           attribute EventHandler onmouseup;
           attribute EventHandler onmousewheel;
           attribute EventHandler onpause;
           attribute EventHandler onplay;
           attribute EventHandler onplaying;
           attribute EventHandler onprogress;
           attribute EventHandler onratechange;
           attribute EventHandler onreset;
           attribute EventHandler onscroll;
           attribute EventHandler onseeked;
           attribute EventHandler onseeking;
           attribute EventHandler onselect;
           attribute EventHandler onshow;
           attribute EventHandler onstalled;
           attribute EventHandler onsubmit;
           attribute EventHandler onsuspend;
           attribute EventHandler ontimeupdate;
           attribute EventHandler onvolumechange;
           attribute EventHandler onwaiting;

  // special event handler IDL attributes that only apply to Document objects
  [LenientThis] attribute EventHandler onreadystatechange;
};

 */

static void jsfinalize_document(JSContext *cx, JSObject *obj);
static JSBool jsresove_node(JSContext *cx, JSObject *obj, jsval id, uintN flags, JSObject **objp);

struct jsclass_document_priv {
	struct html_content *htmlc;
	dom_document *node;
};


#define JSCLASS_NAME document

#include "jsclass.h"

static JSClass JSCLASS_OBJECT =
{
        "document", 
	JSCLASS_NEW_RESOLVE | JSCLASS_HAS_PRIVATE, 
	JS_PropertyStub, 
	JS_PropertyStub, 
	JS_PropertyStub, 
	JS_StrictPropertyStub,
        JS_EnumerateStub, 
	(JSResolveOp)jsresove_node, 
	JS_ConvertStub, 
	jsfinalize_document, 
	JSCLASS_NO_OPTIONAL_MEMBERS
};

#include "document.c"

static JSBool jsresove_node(JSContext *cx, JSObject *obj, jsval id, uintN flags, JSObject **objp)
{
	*objp = NULL;
	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(write, JSContext *cx, uintN argc, jsval *vp)
{
	JSString* u16_txt;
	char *txt;
	unsigned long length;
	struct jsclass_document_priv *document;

	document = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (document == NULL) {
		return JS_FALSE;
	}

	if (!JS_ConvertArguments(cx, argc, JSAPI_ARGV(cx, vp), "S", &u16_txt)) {
		return JS_FALSE;
	}

	JSString_to_char(u16_txt, txt, length);

	LOG(("content %p parser %p writing %s",
	     document->htmlc, document->htmlc->parser, txt));
	if (document->htmlc->parser != NULL) {
		dom_hubbub_parser_insert_chunk(document->htmlc->parser, (uint8_t *)txt, length);
	}
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSFunctionSpec jsfunctions_document[] = {
	JSAPI_FS_DOCUMENT,
	JSAPI_FS(write, 1, 0),
	JSAPI_FS_END
};

static JSPropertySpec jsproperties_document[] =
{
	JSAPI_PS_DOCUMENT,
	JSAPI_PS_END
};

static void jsfinalize_document(JSContext *cx, JSObject *obj)
{
	struct jsclass_document_priv *document;

	document = JS_GetInstancePrivate(cx, obj, &JSCLASS_OBJECT, NULL);
	if (document != NULL) {
		free(document);
	}
}

JSObject *jsapi_new_document(JSContext *cx, JSObject *parent, struct html_content *htmlc)
{
	/* create document object and return it */
	JSObject *jsdocument;
	struct jsclass_document_priv *document;

	document = malloc(sizeof(*document));
	if (document == NULL) {
		return NULL;
	}
	document->htmlc = htmlc;
	document->node = htmlc->document;

	jsdocument = JS_InitClass(cx, 
		     parent, 
		     NULL, 
		     &JSCLASS_OBJECT, 
		     NULL, 
		     0, 
		     jsproperties_document,
		     jsfunctions_document, 
		     NULL, 
		     NULL);
	if (jsdocument == NULL) {
		free(document);
		return NULL;
	}

	LOG(("setting document private to %p", document));
	/* private pointer to browsing context */
	if (JS_SetPrivate(cx, jsdocument, document) != JS_TRUE) {
		LOG(("failed to set document private"));
		free(document);
		return NULL;
	}
	
	return jsdocument;
}
