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


/* IDL http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#eventtarget

interface EventTarget {
  void addEventListener(DOMString type, EventListener? callback, optional boolean capture = false);
  void removeEventListener(DOMString type, EventListener? callback, optional boolean capture = false);
  boolean dispatchEvent(Event event);
};
*/

#include "jsclass.h"

static JSBool JSAPI_NATIVE(addEventListener, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(removeEventListener, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

static JSBool JSAPI_NATIVE(dispatchEvent, JSContext *cx, uintN argc, jsval *vp)
{
	struct JSCLASS_TYPE *priv;

	priv = JS_GetInstancePrivate(cx, JS_THIS_OBJECT(cx,vp), &JSCLASS_OBJECT, NULL);
	if (priv == NULL)
		return JS_FALSE;


	JSAPI_SET_RVAL(cx, vp, JSVAL_VOID);

	return JS_TRUE;
}

#define JSAPI_FS_EVENTTARGET \
	JSAPI_FS(addEventListener, 0, 0), \
	JSAPI_FS(removeEventListener, 0, 0), \
	JSAPI_FS(dispatchEvent, 0, 0)
