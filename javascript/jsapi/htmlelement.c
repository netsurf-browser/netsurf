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
#include "render/html_internal.h"

#include "javascript/jsapi.h"

/* IDL http://www.whatwg.org/specs/web-apps/current-work/#elements-in-the-dom

CAUTION - innerHTML and outerHTML etc. are part of the DOM parsing
	  specification but more can be found in:
http://html5.org/specs/dom-parsing.html#extensions-to-the-element-interface

interface HTMLElement : Element {
  // metadata attributes
	   attribute DOMString title;
	   attribute DOMString lang;
	   attribute boolean translate;
	   attribute DOMString dir;
  readonly attribute DOMStringMap dataset;

  // microdata
	   attribute boolean itemScope;
  [PutForwards=value] readonly attribute DOMSettableTokenList itemType;
	   attribute DOMString itemId;
  [PutForwards=value] readonly attribute DOMSettableTokenList itemRef;
  [PutForwards=value] readonly attribute DOMSettableTokenList itemProp;
  readonly attribute HTMLPropertiesCollection properties;
	   attribute any itemValue;

  // user interaction
	   attribute boolean hidden;
  void click();
	   attribute long tabIndex;
  void focus();
  void blur();
	   attribute DOMString accessKey;
  readonly attribute DOMString accessKeyLabel;
	   attribute boolean draggable;
  [PutForwards=value] readonly attribute DOMSettableTokenList dropzone;
	   attribute DOMString contentEditable;
  readonly attribute boolean isContentEditable;
	   attribute HTMLMenuElement? contextMenu;
	   attribute boolean spellcheck;

  // command API
  readonly attribute DOMString? commandType;
  readonly attribute DOMString? commandLabel;
  readonly attribute DOMString? commandIcon;
  readonly attribute boolean? commandHidden;
  readonly attribute boolean? commandDisabled;
  readonly attribute boolean? commandChecked;

  // styling
  readonly attribute CSSStyleDeclaration style;

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
};

*/

static void jsfinalize_element(JSContext *cx, JSObject *obj);

struct jsclass_document_priv {
	struct html_content *htmlc;
	dom_element *node;
};

#define JSCLASS_NAME htmlelement

#include "jsclass.h"

static JSClass JSCLASS_OBJECT =
{
	"HTMLElement",
	JSCLASS_HAS_PRIVATE,
	JS_PropertyStub,
	JS_PropertyStub,
	JS_PropertyStub,
	JS_StrictPropertyStub,
	JS_EnumerateStub,
	JS_ResolveStub,
	JS_ConvertStub,
	jsfinalize_element,
	JSCLASS_NO_OPTIONAL_MEMBERS
};

#include "element.c"

static void jsfinalize_element(JSContext *cx, JSObject *obj)
{
	struct jsclass_document_priv *element;
	element = JS_GetInstancePrivate(cx, obj, &JSCLASS_OBJECT, NULL);
	if (element != NULL) {
		free(element);
	}
}



static JSFunctionSpec jsfunctions_element[] = {
	JSAPI_FS_ELEMENT,
	JSAPI_FS_END
};




static JSPropertySpec jsproperties_element[] =
{
	JSAPI_PS_ELEMENT,
	JSAPI_PS_END
};

JSObject *
jsapi_new_element(JSContext *cx,
		  JSObject *parent,
		  struct html_content *htmlc,
		  dom_element *domelement)
{
	/* create element object and return it */
	JSObject *jselement;
	struct jsclass_document_priv *element;

	element = malloc(sizeof(element));
	if (element == NULL) {
		return NULL;
	}
	element->htmlc = htmlc;
	element->node = domelement;

	jselement = JS_InitClass(cx,
			   parent,
			   NULL,
			   &JSCLASS_OBJECT,
			   NULL,
			   0,
			   jsproperties_element,
			   jsfunctions_element,
			   NULL,
			   NULL);
	if (jselement == NULL) {
		free(element);
		return NULL;
	}

	LOG(("setting private to %p", element));
	/* private pointer to browsing context */
	if (JS_SetPrivate(cx, jselement, element) != JS_TRUE) {
		LOG(("failed to set private"));
		free(element);
		return NULL;
	}

	return jselement;
}
