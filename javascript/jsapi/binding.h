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
 * spidermonkey jsapi class bindings
 */

#ifndef _NETSURF_JAVASCRIPT_JSAPI_BINDING_H_
#define _NETSURF_JAVASCRIPT_JSAPI_BINDING_H_


#include "render/html_internal.h"

JSObject *jsapi_InitClass_Window(JSContext *cx, JSObject *parent);

/** Create a new javascript window object
 *
 * @param cx The javascript context.
 * @param parent The parent object or NULL for new global
 * @param win_priv The private context to set on the object
 * @return new javascript object or NULL on error
 */
JSObject *jsapi_new_Window(JSContext *cx, 
			    JSObject *window,
			    JSObject *parent, 
			    struct browser_window *bw, 
			   html_content *htmlc);


JSObject *jsapi_InitClass_Document(JSContext *cx, JSObject *parent);

/** Create a new javascript document object
 *
 * @param cx The javascript context.
 * @param parent The parent object, usually a global window object
 * @param doc_priv The private context to set on the object
 * @return new javascript object or NULL on error
 */
JSObject *jsapi_new_Document(JSContext *cx,
		JSObject *proto,
		JSObject *parent,
		dom_document *node,
			     struct html_content *htmlc);

/** Create a new javascript console object
 *
 * @param cx The javascript context.
 * @param parent The parent object, usually a global window object
 * @return new javascript object or NULL on error
 */
JSObject *jsapi_new_Console(JSContext *cx, JSObject *parent);


JSObject *jsapi_InitClass_Navigator(JSContext *cx, JSObject *parent);
/** Create a new javascript navigator object
 *
 * @param cx The javascript context.
 * @param parent The parent object, usually a global window object
 * @return new javascript object or NULL on error
 */
JSObject *jsapi_new_Navigator(JSContext *cx, JSObject *proto, JSObject *parent);

/** Create a new javascript element object
 *
 * @param cx The javascript context.
 * @param parent The parent object, usually a global window object
 * @param doc_priv The private context to set on the object
 * @return new javascript object or NULL on error
 */
JSObject *jsapi_new_element(JSContext *cx, JSObject *parent, struct html_content *htmlc, struct dom_element *domelement);

#endif
