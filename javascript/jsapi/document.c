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

/* IDL http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#interface-document

CAUTION - write, writeln are not part of the DOM they come from:
http://www.w3.org/TR/html5/apis-in-html-documents.html#document.write

interface Document : Node {
  readonly attribute DOMImplementation implementation;
  readonly attribute DOMString URL;
  readonly attribute DOMString documentURI;
  readonly attribute DOMString compatMode;
  readonly attribute DOMString characterSet;
  readonly attribute DOMString contentType;

  readonly attribute DocumentType? doctype;
  readonly attribute Element? documentElement;
  HTMLCollection getElementsByTagName(DOMString localName);
  HTMLCollection getElementsByTagNameNS(DOMString? namespace, DOMString localName);
  HTMLCollection getElementsByClassName(DOMString classNames);
  Element? getElementById(DOMString elementId);

  Element createElement(DOMString localName);
  Element createElementNS(DOMString? namespace, DOMString qualifiedName);
  DocumentFragment createDocumentFragment();
  Text createTextNode(DOMString data);
  Comment createComment(DOMString data);
  ProcessingInstruction createProcessingInstruction(DOMString target, DOMString data);

  Node importNode(Node node, optional boolean deep = true);
  Node adoptNode(Node node);

  Event createEvent(DOMString interface);

  Range createRange();

  // NodeFilter.SHOW_ALL = 0xFFFFFFFF
  NodeIterator createNodeIterator(Node root, optional unsigned long whatToShow = 0xFFFFFFFF, optional NodeFilter? filter = null);
  TreeWalker createTreeWalker(Node root, optional unsigned long whatToShow = 0xFFFFFFFF, optional NodeFilter? filter = null);

  // NEW
  void prepend((Node or DOMString)... nodes);
  void append((Node or DOMString)... nodes);
};


 */

static void jsfinalize_document(JSContext *cx, JSObject *obj);

struct jsclass_document_priv {
	struct html_content *htmlc;
	dom_document *node;
};

static JSClass jsclass_document =
{
        "document", 
	JSCLASS_HAS_PRIVATE, 
	JS_PropertyStub, 
	JS_PropertyStub, 
	JS_PropertyStub, 
	JS_StrictPropertyStub,
        JS_EnumerateStub, 
	JS_ResolveStub, 
	JS_ConvertStub, 
	jsfinalize_document, 
	JSCLASS_NO_OPTIONAL_MEMBERS
};

#define JSCLASS_NAME document

#include "node.c"

static JSBool JSAPI_NATIVE(getElementById, JSContext *cx, uintN argc, jsval *vp)
{
	JSString* u16_txt;
	char *txt;
	unsigned long txtlen;
	dom_string *idstr;
	dom_element *idelement;
	struct jsclass_document_priv *document;

	document = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &jsclass_document, NULL);
	if (document == NULL) {
		return JS_FALSE;
	}

	if (document->node == NULL) {
		/* no document available, this is obviously a problem
		 * for finding elements 
		 */
		JSAPI_SET_RVAL(cx, vp, JSVAL_NULL);

		return JS_TRUE;
	}

	if (!JS_ConvertArguments(cx, argc, JSAPI_ARGV(cx, vp), "S", &u16_txt))
		return JS_FALSE;

	JSString_to_char(u16_txt, txt, txtlen);

	dom_string_create((unsigned char*)txt, txtlen, &idstr);

	dom_document_get_element_by_id(document->node, idstr, &idelement);

	JSAPI_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(jsapi_new_element(cx, JS_GetGlobalObject(cx), document->htmlc, idelement)));

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(write, JSContext *cx, uintN argc, jsval *vp)
{
	JSString* u16_txt;
	char *txt;
	unsigned long length;
	struct jsclass_document_priv *document;

	document = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &jsclass_document, NULL);
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
	JSAPI_FS_NODE,
	JSAPI_FS(write, 1, 0),
	JSAPI_FS(getElementById, 1, 0),
	JSAPI_FS_END
};



static void jsfinalize_document(JSContext *cx, JSObject *obj)
{
	struct jsclass_document_priv *document;

	document = JS_GetInstancePrivate(cx, obj, &jsclass_document, NULL);
	if (document != NULL) {
		free(document);
	}
}

JSObject *jsapi_new_document(JSContext *cx, JSObject *parent, struct html_content *htmlc)
{
	/* create document object and return it */
	JSObject *jsdocument;
	struct jsclass_document_priv *document;

	document = malloc(sizeof(document));
	if (document == NULL) {
		return NULL;
	}
	document->htmlc = htmlc;
	document->node = htmlc->document;

	jsdocument = JS_InitClass(cx, 
		     parent, 
		     NULL, 
		     &jsclass_document, 
		     NULL, 
		     0, 
		     NULL, 
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
