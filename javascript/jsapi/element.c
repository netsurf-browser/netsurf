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


#include "javascript/jsapi.h"
#include "utils/config.h"
#include "render/html_internal.h"
#include "utils/log.h"

/* IDL http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#interface-element

CAUTION - innerHTML etc. are not part of the DOM they come from:
http://html5.org/specs/dom-parsing.html#extensions-to-the-element-interface

interface Element : Node {
  readonly attribute DOMString? namespaceURI;
  readonly attribute DOMString? prefix;
  readonly attribute DOMString localName;
  readonly attribute DOMString tagName;

           attribute DOMString id;
           attribute DOMString className;
  readonly attribute DOMTokenList classList;

  readonly attribute Attr[] attributes;
  DOMString? getAttribute(DOMString name);
  DOMString? getAttributeNS(DOMString? namespace, DOMString localName);
  void setAttribute(DOMString name, DOMString value);
  void setAttributeNS(DOMString? namespace, DOMString name, DOMString value);
  void removeAttribute(DOMString name);
  void removeAttributeNS(DOMString? namespace, DOMString localName);
  boolean hasAttribute(DOMString name);
  boolean hasAttributeNS(DOMString? namespace, DOMString localName);

  HTMLCollection getElementsByTagName(DOMString localName);
  HTMLCollection getElementsByTagNameNS(DOMString? namespace, DOMString localName);
  HTMLCollection getElementsByClassName(DOMString classNames);

  readonly attribute HTMLCollection children;
  readonly attribute Element? firstElementChild;
  readonly attribute Element? lastElementChild;
  readonly attribute Element? previousElementSibling;
  readonly attribute Element? nextElementSibling;
  readonly attribute unsigned long childElementCount;

  // NEW
  void prepend((Node or DOMString)... nodes);
  void append((Node or DOMString)... nodes);
  void before((Node or DOMString)... nodes);
  void after((Node or DOMString)... nodes);
  void replace((Node or DOMString)... nodes);
  void remove();
};
*/

static void jsfinalize_element(JSContext *cx, JSObject *obj);

struct jsclass_document_priv {
	struct html_content *htmlc;
	dom_element *node;
};

static JSClass jsclass_element =
{
        "Element", 
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

#define JSCLASS_NAME element

#include "node.c"

static void jsfinalize_element(JSContext *cx, JSObject *obj)
{
	struct jsclass_document_priv *element;
	element = JS_GetInstancePrivate(cx, obj, &jsclass_element, NULL);
	if (element != NULL) {
		free(element);
	}
}



static JSFunctionSpec jsfunctions_element[] = {
	JSAPI_FS_NODE,
	JSAPI_FS_END
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
			   &jsclass_element, 
			   NULL, 
			   0, 
			   NULL, 
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
