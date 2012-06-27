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

/** \file
 * spidermonkey jsapi bindings and compatability glue.
 */

#ifndef _NETSURF_JAVASCRIPT_JSAPI_H_
#define _NETSURF_JAVASCRIPT_JSAPI_H_

#ifdef WITH_MOZJS
#include "js/jsapi.h"
#else
#include "mozjs/jsapi.h"
#endif

#if JS_VERSION <= 180
inline JSObject *
JS_NewCompartmentAndGlobalObject(JSContext *cx, 
				 JSClass *jsclass, 
				 JSPrincipals *principals)
{
	JSObject *global;
	global = JS_NewObject(cx, jsclass, NULL, NULL);
	if (global == NULL) {
		return NULL;
	}
	JS_SetGlobalObject(cx, global);
	return global;
}

#define JS_StrictPropertyStub JS_PropertyStub

#define JSString_to_char(injsstring, outchar, outlen)	\
	txt = JS_GetStringBytes(u16_txt);		\
	outlen = strlen(txt)

#else

#define JSString_to_char(injsstring, outchar, outlen)		\
	outlen = JS_GetStringLength(injsstring);		\
	outchar = alloca(sizeof(char)*(outlen+1));		\
	JS_EncodeStringToBuffer(injsstring, outchar, outlen);	\
	outchar[outlen] = '\0'


#endif


JSObject *jsapi_new_window(JSContext *cx, JSObject *parent, void *win_priv);
JSObject *jsapi_new_document(JSContext *cx, JSObject *parent, void *doc_priv);
JSObject *jsapi_new_console(JSContext *cx, JSObject *parent);

#endif
