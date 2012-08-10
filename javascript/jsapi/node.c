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


/* IDL http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#interface-node 
interface Node : EventTarget {
  const unsigned short ELEMENT_NODE = 1;
  const unsigned short ATTRIBUTE_NODE = 2; // historical
  const unsigned short TEXT_NODE = 3;
  const unsigned short CDATA_SECTION_NODE = 4; // historical
  const unsigned short ENTITY_REFERENCE_NODE = 5; // historical
  const unsigned short ENTITY_NODE = 6; // historical
  const unsigned short PROCESSING_INSTRUCTION_NODE = 7;
  const unsigned short COMMENT_NODE = 8;
  const unsigned short DOCUMENT_NODE = 9;
  const unsigned short DOCUMENT_TYPE_NODE = 10;
  const unsigned short DOCUMENT_FRAGMENT_NODE = 11;
  const unsigned short NOTATION_NODE = 12; // historical
  readonly attribute unsigned short nodeType;
  readonly attribute DOMString nodeName;

  readonly attribute DOMString? baseURI;

  readonly attribute Document? ownerDocument;
  readonly attribute Node? parentNode;
  readonly attribute Element? parentElement;
  boolean hasChildNodes();
  readonly attribute NodeList childNodes;
  readonly attribute Node? firstChild;
  readonly attribute Node? lastChild;
  readonly attribute Node? previousSibling;
  readonly attribute Node? nextSibling;

  const unsigned short DOCUMENT_POSITION_DISCONNECTED = 0x01;
  const unsigned short DOCUMENT_POSITION_PRECEDING = 0x02;
  const unsigned short DOCUMENT_POSITION_FOLLOWING = 0x04;
  const unsigned short DOCUMENT_POSITION_CONTAINS = 0x08;
  const unsigned short DOCUMENT_POSITION_CONTAINED_BY = 0x10;
  const unsigned short DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC = 0x20; // historical
  unsigned short compareDocumentPosition(Node other);
  boolean contains(Node? other);

           attribute DOMString? nodeValue;
           attribute DOMString? textContent;
  Node insertBefore(Node node, Node? child);
  Node appendChild(Node node);
  Node replaceChild(Node node, Node child);
  Node removeChild(Node child);
  void normalize();

  
  Node cloneNode(optional boolean deep = true);
  boolean isEqualNode(Node? node);

  DOMString lookupPrefix(DOMString? namespace);
  DOMString lookupNamespaceURI(DOMString? prefix);
  boolean isDefaultNamespace(DOMString? namespace);
};
*/

#include "eventtarget.c"

#ifndef JSCLASS_NAME
#error "The class name must be defined"
#endif

#ifndef JSCLASS_TYPE
#define CLASS jsclass
#define PRIVATE priv
#define EXPAND(a,b) PASTE(a,b)
#define PASTE(x,y) x##_##y
#define JSCLASS_OBJECT EXPAND(CLASS,JSCLASS_NAME)
#define JSCLASS_TYPE EXPAND(JSCLASS_OBJECT,PRIVATE)
#endif

static JSBool JSAPI_NATIVE(hasChildNodes, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(compareDocumentPosition, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(contains, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(insertBefore, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(appendChild, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(replaceChild, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(removeChild, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(normalize, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(cloneNode, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(isEqualNode, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(lookupPrefix, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(lookupNamespaceURI, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(isDefaultNamespace, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}


#define JSAPI_FS_NODE \
	JSAPI_FS_EVENTTARGET, \
	JSAPI_FS(hasChildNodes, 0, 0), \
	JSAPI_FS(compareDocumentPosition, 0, 0), \
	JSAPI_FS(contains, 0, 0), \
	JSAPI_FS(insertBefore, 0, 0), \
	JSAPI_FS(appendChild, 0, 0), \
	JSAPI_FS(replaceChild, 0, 0), \
	JSAPI_FS(removeChild, 0, 0), \
	JSAPI_FS(normalize, 0, 0), \
	JSAPI_FS(cloneNode, 0, 0), \
	JSAPI_FS(isEqualNode, 0, 0), \
	JSAPI_FS(lookupPrefix, 0, 0), \
	JSAPI_FS(lookupNamespaceURI, 0, 0), \
	JSAPI_FS(isDefaultNamespace, 0, 0)
