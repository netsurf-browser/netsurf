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

/* IDL from http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core.html


interface Document : Node {
  // Modified in DOM Level 3:
  readonly attribute DocumentType    doctype;
  readonly attribute DOMImplementation implementation;
  readonly attribute Element         documentElement;
  Element            createElement(in DOMString tagName)
                                        raises(DOMException);
  DocumentFragment   createDocumentFragment();
  Text               createTextNode(in DOMString data);
  Comment            createComment(in DOMString data);
  CDATASection       createCDATASection(in DOMString data)
                                        raises(DOMException);
  ProcessingInstruction createProcessingInstruction(in DOMString target, 
                                                    in DOMString data)
                                        raises(DOMException);
  Attr               createAttribute(in DOMString name)
                                        raises(DOMException);
  EntityReference    createEntityReference(in DOMString name)
                                        raises(DOMException);
  NodeList           getElementsByTagName(in DOMString tagname);
  // Introduced in DOM Level 2:
  Node               importNode(in Node importedNode, 
                                in boolean deep)
                                        raises(DOMException);
  // Introduced in DOM Level 2:
  Element            createElementNS(in DOMString namespaceURI, 
                                     in DOMString qualifiedName)
                                        raises(DOMException);
  // Introduced in DOM Level 2:
  Attr               createAttributeNS(in DOMString namespaceURI, 
                                       in DOMString qualifiedName)
                                        raises(DOMException);
  // Introduced in DOM Level 2:
  NodeList           getElementsByTagNameNS(in DOMString namespaceURI, 
                                            in DOMString localName);
  // Introduced in DOM Level 2:
  Element            getElementById(in DOMString elementId);
  // Introduced in DOM Level 3:
  readonly attribute DOMString       inputEncoding;
  // Introduced in DOM Level 3:
  readonly attribute DOMString       xmlEncoding;
  // Introduced in DOM Level 3:
           attribute boolean         xmlStandalone;
                                        // raises(DOMException) on setting

  // Introduced in DOM Level 3:
           attribute DOMString       xmlVersion;
                                        // raises(DOMException) on setting

  // Introduced in DOM Level 3:
           attribute boolean         strictErrorChecking;
  // Introduced in DOM Level 3:
           attribute DOMString       documentURI;
  // Introduced in DOM Level 3:
  Node               adoptNode(in Node source)
                                        raises(DOMException);
  // Introduced in DOM Level 3:
  readonly attribute DOMConfiguration domConfig;
  // Introduced in DOM Level 3:
  void               normalizeDocument();
  // Introduced in DOM Level 3:
  Node               renameNode(in Node n, 
                                in DOMString namespaceURI, 
                                in DOMString qualifiedName)
                                        raises(DOMException);
};


 */


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
	JS_FinalizeStub, 
	JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool JSAPI_NATIVE(getElementById, JSContext *cx, uintN argc, jsval *vp)
{
	JSString* u16_txt;
	char *txt;
	unsigned long txtlen;
	struct html_content *htmlc;
	dom_string *idstr;
	dom_element *idelement;

	htmlc = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &jsclass_document, NULL);
	if (htmlc == NULL)
		return JS_FALSE;

	if (htmlc->document == NULL) {
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

	dom_document_get_element_by_id(htmlc->document, idstr, &idelement);

	if (idelement==NULL) {
	JSAPI_SET_RVAL(cx, vp, JSVAL_NULL);
	} else {
		/* create element object and return it*/
	}

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(write, JSContext *cx, uintN argc, jsval *vp)
{
	JSString* u16_txt;
	char *txt;
	unsigned long length;
	struct html_content *htmlc;

	htmlc = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &jsclass_document, NULL);
	if (htmlc == NULL)
		return JS_FALSE;

	if (!JS_ConvertArguments(cx, argc, JSAPI_ARGV(cx, vp), "S", &u16_txt))
		return JS_FALSE;

	JSString_to_char(u16_txt, txt, length);

	LOG(("content %p parser %p writing %s",htmlc, htmlc->parser, txt));
	if (htmlc->parser != NULL) {
		dom_hubbub_parser_insert_chunk(htmlc->parser, (uint8_t *)txt, length);
	}
	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSFunctionSpec jsfunctions_document[] = {
	JSAPI_FS(write, 1, 0),
	JSAPI_FS(getElementById, 1, 0),
	JSAPI_FS_END
};




JSObject *jsapi_new_document(JSContext *cx, JSObject *parent, void *doc_priv)
{
	JSObject *doc;
	doc = JS_InitClass(cx, 
		     parent, 
		     NULL, 
		     &jsclass_document, 
		     NULL, 
		     0, 
		     NULL, 
		     jsfunctions_document, 
		     NULL, 
		     NULL);
	if (doc == NULL) {
		return NULL;
	}

	LOG(("setting content to %p",doc_priv));
	/* private pointer to browsing context */
	if (JS_SetPrivate(cx, doc, doc_priv) != JS_TRUE) {
		LOG(("failed to set content"));
		return NULL;
	}
	
	return doc;
}
