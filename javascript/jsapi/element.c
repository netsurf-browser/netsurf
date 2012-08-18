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


/* IDL http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#interface-element


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

#include "jsclass.h"

#include "node.c"

static JSBool JSAPI_NATIVE(getAttribute, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_NULL);

	return JS_TRUE;
}

#define JSAPI_FS_ELEMENT \
	JSAPI_FS_NODE, \
	JSAPI_FS(getAttribute, 0, 0)

static JSBool JSAPI_PROPERTYGET(id, JSContext *cx, JSObject *obj, jsval *vp)
{
	JS_SET_RVAL(cx, vp, JSVAL_NULL);
	return JS_TRUE;
}

static JSBool JSAPI_PROPERTYSET(id, JSContext *cx, JSObject *obj, jsval *vp)
{
	return JS_FALSE;
}

#define JSAPI_PS_ELEMENT						\
	JSAPI_PS_NODE,							\
	JSAPI_PS(id, 0, JSPROP_ENUMERATE | JSPROP_SHARED)
