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
 * Interface to javascript engine functions.
 */

#ifndef _NETSURF_JAVASCRIPT_JS_H_
#define _NETSURF_JAVASCRIPT_JS_H_

#include "utils/errors.h"


typedef struct jscontext jscontext;
typedef struct jsobject jsobject;

typedef bool(jscallback)(void *ctx);

struct dom_event;
struct dom_document;
struct dom_node;
struct dom_element;
struct dom_string;

/** Initialise javascript interpreter */
void js_initialise(void);

/** finalise javascript interpreter */
void js_finalise(void);

/** Create a new javascript context.
 *
 * There is usually one context per browser context
 *
 * \param timeout elapsed wallclock time (in seconds)  before \a callback is called
 * \param cb the callback when the runtime exceeds the timeout
 * \param cbctx The context to pass to the callback
 * \param jsctx Updated to the created JS context
 * \return NSERROR_OK on success, appropriate error otherwise.
 */
nserror js_newcontext(int timeout, jscallback *cb, void *cbctx,
		jscontext **jsctx);

/** Destroy a previously created context */
void js_destroycontext(jscontext *ctx);

/** Create a new javascript compartment
 *
 * This is called once for a page with javascript script tags on
 * it. It constructs a fresh global window object.
 */
jsobject *js_newcompartment(jscontext *ctx, void *win_priv, void *doc_priv);

/* execute some javascript in a context */
bool js_exec(jscontext *ctx, const char *txt, size_t txtlen);


/* fire an event at a dom node */
bool js_fire_event(jscontext *ctx, const char *type, struct dom_document *doc, struct dom_node *target);

bool
js_dom_event_add_listener(jscontext *ctx,
			  struct dom_document *document,
			  struct dom_node *node,
			  struct dom_string *event_type_dom,
			  void *js_funcval);

/*** New Events ***/

/** Handle a new element being created.
 *
 * This is called once an element is inserted into the DOM document handled
 * by the context provided.  The JS implementation must then scan the element
 * for on* attributes and register appropriate listeners for those handlers.
 */
void js_handle_new_element(jscontext *ctx, struct dom_element *node);

/** Handle an event propagation finished callback.
 *
 * This is called once an event finishes propagating, no matter how it
 * finishes.  The intent here is that the JS context can perform any cleanups
 * it may need to perform before the DOM finishes and the event may end up
 * freed.
 */
void js_event_cleanup(jscontext *ctx, struct dom_event *evt);

#endif /* _NETSURF_JAVASCRIPT_JS_H_ */
