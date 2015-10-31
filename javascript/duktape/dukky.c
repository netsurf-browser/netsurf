/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 * Copyright 2015 All of us.
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
 * Duktapeish implementation of javascript engine functions.
 */

#include "content/content.h"

#include "utils/nsoption.h"
#include "utils/log.h"
#include "utils/corestrings.h"

#include "javascript/js.h"
#include "javascript/content.h"

#include "duktape/binding.h"

#include "duktape.h"
#include "dukky.h"

#include <dom/dom.h>

#define EVENT_MAGIC MAGIC(EVENT_MAP)
#define HANDLER_LISTENER_MAGIC MAGIC(HANDLER_LISTENER_MAP)
#define HANDLER_MAGIC MAGIC(HANDLER_MAP)

static duk_ret_t dukky_populate_object(duk_context *ctx)
{
	/* ... obj args protoname nargs */
	int nargs = duk_get_int(ctx, -1);
	duk_pop(ctx);
	/* ... obj args protoname */
	duk_get_global_string(ctx, PROTO_MAGIC);
	/* .. obj args protoname prototab */
	duk_insert(ctx, -2);
	/* ... obj args prototab protoname */
	duk_get_prop(ctx, -2);
	/* ... obj args prototab {proto/undefined} */
	if (duk_is_undefined(ctx, -1)) {
		LOG("RuhRoh, couldn't find a prototype, HTMLUnknownElement it is");
		duk_pop(ctx);
		duk_push_string(ctx, PROTO_NAME(HTMLUNKNOWNELEMENT));
		duk_get_prop(ctx, -2);
	}
	/* ... obj args prototab proto */
	duk_dup(ctx, -1);
	/* ... obj args prototab proto proto */
	duk_set_prototype(ctx, -(nargs+4));
	/* ... obj[proto] args prototab proto */
	duk_get_prop_string(ctx, -1, INIT_MAGIC);
	/* ... obj[proto] args prototab proto initfn */
	duk_insert(ctx, -(nargs+4));
	/* ... initfn obj[proto] args prototab proto */
	duk_pop_2(ctx);
	/* ... initfn obj[proto] args */
	LOG("Call the init function");
	duk_call(ctx, nargs + 1);
	return 1; /* The object */
}

duk_ret_t dukky_create_object(duk_context *ctx, const char *name, int args)
{
	duk_ret_t ret;
	LOG("name=%s nargs=%d", name+2, args);
	/* ... args */
	duk_push_object(ctx);
	/* ... args obj */
	duk_push_object(ctx);
	/* ... args obj handlers */
	duk_put_prop_string(ctx, -2, HANDLER_LISTENER_MAGIC);
	/* ... args obj */
	duk_push_object(ctx);
	/* ... args obj handlers */
	duk_put_prop_string(ctx, -2, HANDLER_MAGIC);
	/* ... args obj */
	duk_insert(ctx, -(args+1));
	/* ... obj args */
	duk_push_string(ctx, name);
	/* ... obj args name */
	duk_push_int(ctx, args);
	/* ... obj args name nargs */
	if ((ret = duk_safe_call(ctx, dukky_populate_object, args + 3, 1))
	    != DUK_EXEC_SUCCESS)
		return ret;
	LOG("created");
	return DUK_EXEC_SUCCESS;
}



duk_bool_t
dukky_push_node_stacked(duk_context *ctx)
{
	int top_at_fail = duk_get_top(ctx) - 2;
	/* ... nodeptr klass */
	duk_get_global_string(ctx, NODE_MAGIC);
	/* ... nodeptr klass nodes */
	duk_dup(ctx, -3);
	/* ... nodeptr klass nodes nodeptr */
	duk_get_prop(ctx, -2);
	/* ... nodeptr klass nodes node/undefined */
	if (duk_is_undefined(ctx, -1)) {
		/* ... nodeptr klass nodes undefined */
		duk_pop(ctx);
		/* ... nodeptr klass nodes */
		duk_push_object(ctx);
		/* ... nodeptr klass nodes obj */
		duk_push_object(ctx);
		/* ... nodeptr klass nodes obj handlers */
		duk_put_prop_string(ctx, -2, HANDLER_LISTENER_MAGIC);
		/* ... nodeptr klass nodes obj */
		duk_push_object(ctx);
		/* ... nodeptr klass nodes obj handlers */
		duk_put_prop_string(ctx, -2, HANDLER_MAGIC);
		/* ... nodeptr klass nodes obj */
		duk_dup(ctx, -4);
		/* ... nodeptr klass nodes obj nodeptr */
		duk_dup(ctx, -4);
		/* ... nodeptr klass nodes obj nodeptr klass */
		duk_push_int(ctx, 1);
		/* ... nodeptr klass nodes obj nodeptr klass 1 */
		if (duk_safe_call(ctx, dukky_populate_object, 4, 1)
		    != DUK_EXEC_SUCCESS) {
			duk_set_top(ctx, top_at_fail);
			LOG("Boo and also hiss");
			return false;
		}
		/* ... nodeptr klass nodes node */
		duk_dup(ctx, -4);
		/* ... nodeptr klass nodes node nodeptr */
		duk_dup(ctx, -2);
		/* ... nodeptr klass nodes node nodeptr node */
		duk_put_prop(ctx, -4);
		/* ... nodeptr klass nodes node */
	}
	/* ... nodeptr klass nodes node */
	duk_insert(ctx, -4);
	/* ... node nodeptr klass nodes */
	duk_pop_3(ctx);
	/* ... node */
	return true;
}

static void
dukky_push_node_klass(duk_context *ctx, struct dom_node *node)
{
	dom_node_type nodetype;
	dom_exception err;

	err = dom_node_get_node_type(node, &nodetype);
	if (err != DOM_NO_ERR) {
		/* Oh bum, just node then */
		duk_push_string(ctx, PROTO_NAME(NODE));
		return;
	}

	switch(nodetype) {
	case DOM_ELEMENT_NODE: {
		dom_string *namespace, *tag;
		err = dom_node_get_namespace(node, &namespace);
		if (err != DOM_NO_ERR) {
			/* Feck it, element */
			LOG("dom_node_get_namespace() failed");
			duk_push_string(ctx, PROTO_NAME(ELEMENT));
			break;
		}
		if (namespace == NULL) {
			/* No namespace, -> element */
			LOG("no namespace");
			duk_push_string(ctx, PROTO_NAME(ELEMENT));
			break;
		}

		if (dom_string_isequal(namespace, corestring_dom_html_namespace) == false) {
			/* definitely not an HTML element of some kind */
			duk_push_string(ctx, PROTO_NAME(ELEMENT));
			dom_string_unref(namespace);
			break;
		}
		dom_string_unref(namespace);

		err = dom_node_get_node_name(node, &tag);
		if (err != DOM_NO_ERR) {
			duk_push_string(ctx, PROTO_NAME(HTMLUNKNOWNELEMENT));
			break;
		}

		duk_push_string(ctx, PROTO_NAME(HTML));
		duk_push_lstring(ctx, dom_string_data(tag), dom_string_length(tag));
		dom_string_unref(tag);
		duk_push_string(ctx, "ELEMENT");
		duk_concat(ctx, 3);

		break;
	}
	case DOM_TEXT_NODE:
		duk_push_string(ctx, PROTO_NAME(TEXT));
		break;
	case DOM_COMMENT_NODE:
		duk_push_string(ctx, PROTO_NAME(COMMENT));
		break;
	case DOM_DOCUMENT_NODE:
		duk_push_string(ctx, PROTO_NAME(DOCUMENT));
		break;
	case DOM_ATTRIBUTE_NODE:
	case DOM_PROCESSING_INSTRUCTION_NODE:
	case DOM_DOCUMENT_TYPE_NODE:
	case DOM_DOCUMENT_FRAGMENT_NODE:
	case DOM_NOTATION_NODE:
	case DOM_ENTITY_REFERENCE_NODE:
	case DOM_ENTITY_NODE:
	case DOM_CDATA_SECTION_NODE:
	default:
		/* Oh bum, just node then */
		duk_push_string(ctx, PROTO_NAME(NODE));
	}
}

duk_bool_t
dukky_push_node(duk_context *ctx, struct dom_node *node)
{
	LOG("Pushing node %p", node);
	/* First check if we can find the node */
	/* ... */
	duk_get_global_string(ctx, NODE_MAGIC);
	/* ... nodes */
	duk_push_pointer(ctx, node);
	/* ... nodes nodeptr */
	duk_get_prop(ctx, -2);
	/* ... nodes node/undefined */
	if (!duk_is_undefined(ctx, -1)) {
		/* ... nodes node */
		duk_insert(ctx, -2);
		/* ... node nodes */
		duk_pop(ctx);
		/* ... node */
		LOG("Found it memoised");
		return true;
	}
	/* ... nodes undefined */
	duk_pop_2(ctx);
	/* ... */
	/* We couldn't, so now we determine the node type and then
	 * we ask for it to be created
	 */
	duk_push_pointer(ctx, node);
	/* ... nodeptr */
	dukky_push_node_klass(ctx, node);
	/* ... nodeptr klass */
	return dukky_push_node_stacked(ctx);
}

static duk_ret_t
dukky_bad_constructor(duk_context *ctx)
{
	duk_error(ctx, DUK_ERR_ERROR, "Bad constructor");
	return 0;
}

void
dukky_inject_not_ctr(duk_context *ctx, int idx, const char *name)
{
	/* ... p[idx] ... proto */
	duk_push_c_function(ctx, dukky_bad_constructor, 0);
	/* ... p[idx] ... proto cons */
	duk_insert(ctx, -2);
	/* ... p[idx] ... cons proto */
	duk_put_prop_string(ctx, -2, "prototype");
	/* ... p[idx] ... cons[proto] */
	duk_put_prop_string(ctx, idx, name);
	/* ... p ... */
	return;
}

/**************************************** js.h ******************************/
struct jscontext {
	duk_context *ctx;
	duk_context *thread;
};

#define CTX (ctx->thread)

void js_initialise(void)
{
	/** TODO: Forces JS on for our testing, needs changing before a release
	 * lest we incur the wrath of others.
	 */
	nsoption_set_bool(enable_javascript, true);
	javascript_init();
}

void js_finalise(void)
{
	/* NADA for now */
}

#define DUKKY_NEW_PROTOTYPE(klass, uklass, klass_name)			\
	dukky_create_prototype(ctx, dukky_##klass##___proto, PROTO_NAME(uklass), klass_name)

nserror js_newcontext(int timeout, jscallback *cb, void *cbctx,
		jscontext **jsctx)
{
	duk_context *ctx;
	jscontext *ret = calloc(1, sizeof(*ret));
	*jsctx = NULL;
	LOG("Creating new duktape javascript context");
	if (ret == NULL) return NSERROR_NOMEM;
	ctx = ret->ctx = duk_create_heap_default();
	if (ret->ctx == NULL) { free(ret); return NSERROR_NOMEM; }
	/* Create the prototype stuffs */
	duk_push_global_object(ctx);
	duk_push_boolean(ctx, true);
	duk_put_prop_string(ctx, -2, "protos");
	duk_put_global_string(ctx, PROTO_MAGIC);
	/* Create prototypes here */
	dukky_create_prototypes(ctx);

	*jsctx = ret;
	return NSERROR_OK;
}

void js_destroycontext(jscontext *ctx)
{
	LOG("Destroying duktape javascript context");
	duk_destroy_heap(ctx->ctx);
	free(ctx);
}

jsobject *js_newcompartment(jscontext *ctx, void *win_priv, void *doc_priv)
{
	assert(ctx != NULL);
	/* Pop any active thread off */
	LOG("Yay, new compartment, win_priv=%p, doc_priv=%p", win_priv, doc_priv);
	duk_set_top(ctx->ctx, 0);
	duk_push_thread(ctx->ctx);
	ctx->thread = duk_require_context(ctx->ctx, -1);
	duk_push_int(CTX, 0);
	duk_push_int(CTX, 1);
	duk_push_int(CTX, 2);
	/* Manufacture a Window object */
	/* win_priv is a browser_window, doc_priv is an html content struct */
	duk_push_pointer(CTX, win_priv);
	duk_push_pointer(CTX, doc_priv);
	dukky_create_object(CTX, PROTO_NAME(WINDOW), 2);
	duk_push_global_object(CTX);
	duk_put_prop_string(CTX, -2, PROTO_MAGIC);
	duk_set_global_object(CTX);

	/* Now we need to prepare our node mapping table */
	duk_push_object(CTX);
	duk_push_pointer(CTX, NULL);
	duk_push_null(CTX);
	duk_put_prop(CTX, -3);
	duk_put_global_string(CTX, NODE_MAGIC);

	/* And now the event mapping table */
	duk_push_object(CTX);
	duk_put_global_string(CTX, EVENT_MAGIC);

	return (jsobject *)ctx;
}

static duk_ret_t eval_top_string(duk_context *ctx)
{
	duk_eval(ctx);
	return 0;
}

bool js_exec(jscontext *ctx, const char *txt, size_t txtlen)
{
	assert(ctx);
	if (txt == NULL || txtlen == 0) return false;
	duk_set_top(CTX, 0);
	duk_push_lstring(CTX, txt, txtlen);

	if (duk_safe_call(CTX, eval_top_string, 1, 1) == DUK_EXEC_ERROR) {
		duk_get_prop_string(CTX, 0, "name");
		duk_get_prop_string(CTX, 0, "message");
		duk_get_prop_string(CTX, 0, "fileName");
		duk_get_prop_string(CTX, 0, "lineNumber");
		duk_get_prop_string(CTX, 0, "stack");
		LOG("Uncaught error in JS: %s: %s", duk_safe_to_string(CTX, 1),
		    duk_safe_to_string(CTX, 2));
		LOG("              was at: %s line %s", duk_safe_to_string(CTX, 3),
		    duk_safe_to_string(CTX, 4));
		LOG("         Stack trace: %s", duk_safe_to_string(CTX, 5));
		return false;
	}
	if (duk_get_top(CTX) == 0) duk_push_boolean(CTX, false);
	LOG("Returning %s", duk_get_boolean(CTX, 0) ? "true" : "false");
	return duk_get_boolean(CTX, 0);
}

bool js_fire_event(jscontext *ctx, const char *type, struct dom_document *doc, struct dom_node *target)
{
	/* La La La */
	LOG("Oh dear, an event: %s", type);
	return true;
}

/*** New style event handling ***/

static void dukky_push_event(duk_context *ctx, dom_event *evt)
{
	/* ... */
	duk_get_global_string(ctx, EVENT_MAGIC);
	/* ... events */
	duk_push_pointer(ctx, evt);
	/* ... events eventptr */
	duk_get_prop(ctx, -2);
	/* ... events event? */
	if (duk_is_undefined(ctx, -1)) {
		/* ... events undefined */
		duk_pop(ctx);
		/* ... events */
		duk_push_object(ctx);
		/* ... events eobj */
		/** @todo fill out the event object */
		duk_push_pointer(ctx, evt);
		/* ... events eobj eventptr */
		duk_dup(ctx, -2);
		/* ... events eobj eventptr eobj */
		duk_put_prop(ctx, -4);
		/* ... events eobj */
	}
	/* ... events event */
	duk_replace(ctx, -2);
	/* ... event */
}

static void dukky_push_handler_code(duk_context *ctx, dom_event *evt)
{
	dom_string *name, *onname, *val;
	dom_element *ele;
	dom_exception exc;

	exc = dom_event_get_type(evt, &name);
	if (exc != DOM_NO_ERR) {
		duk_push_lstring(ctx, "", 0);
		return;
	}

	exc = dom_string_concat(corestring_dom_on, name, &onname);
	if (exc != DOM_NO_ERR) {
		dom_string_unref(name);
		duk_push_lstring(ctx, "", 0);
		return;
	}
	dom_string_unref(name);

	exc = dom_event_get_target(evt, &ele);
	if (exc != DOM_NO_ERR) {
		dom_string_unref(onname);
		duk_push_lstring(ctx, "", 0);
		return;
	}

	exc = dom_element_get_attribute(ele, onname, &val);
	if (exc != DOM_NO_ERR) {
		dom_string_unref(onname);
		dom_node_unref(ele);
		duk_push_lstring(ctx, "", 0);
		return;
	}
	dom_node_unref(ele);
	dom_string_unref(onname);
	duk_push_lstring(ctx, dom_string_data(val), dom_string_length(val));
	dom_string_unref(val);
}


static void dukky_generic_event_handler(dom_event *evt, void *pw)
{
	duk_context *ctx = (duk_context *)pw;
	dom_string *name;
	dom_exception exc;
	dom_event_target *targ;

	LOG("WOOP WOOP, An event:");
	exc = dom_event_get_type(evt, &name);
	if (exc != DOM_NO_ERR) {
		LOG("Unable to find the event name");
		return;
	}
	LOG("Event's name is %*s",
	    dom_string_length(name), dom_string_data(name));

	exc = dom_event_get_target(evt, &targ);
	if (exc != DOM_NO_ERR) {
		dom_string_unref(name);
		LOG("Unable to find the event target");
		return;
	}

	/* ... */
	if (dukky_push_node(ctx, (dom_node *)targ) == false) {
		dom_string_unref(name);
		dom_node_unref(targ);
		LOG("Unable to push JS node representation?!");
		return;
	}
	dom_node_unref(targ);
	/* ... node */
	duk_get_prop_string(ctx, -1, HANDLER_MAGIC);
	/* ... node handlers */
	duk_push_lstring(ctx, dom_string_data(name), dom_string_length(name));
	/* ... node handlers name */
	duk_get_prop(ctx, -2);
	/* ... node handlers handler? */
	if (duk_is_undefined(ctx, -1)) {
		/* ... node handlers undefined */
		duk_pop_2(ctx);
		/* ... node */
		dukky_push_handler_code(ctx, evt);
		/* ... node handlercode */
		/** @todo This is entirely wrong, but it's hard to get right */
		duk_push_string(ctx, "function (event) {");
		/* ... node handlercode prefix */
		duk_insert(ctx, -2);
		/* ... node prefix handlercode */
		duk_push_string(ctx, "}");
		/* ... node prefix handlercode suffix */
		duk_concat(ctx, 3);
		/* ... node fullhandlersrc */
		duk_push_string(ctx, "internal raw uncompiled handler");
		/* ... node fullhandlersrc filename */
		if (duk_pcompile(ctx, DUK_COMPILE_FUNCTION) != 0) {
			/* ... node err */
			LOG("Unable to proceed with handler, could not compile");
			dom_string_unref(name);
			duk_pop_2(ctx);
			return ;
		}
		/* ... node handler */
		duk_insert(ctx, -2);
		/* ... handler node */
	} else {
		/* ... node handlers handler */
		duk_insert(ctx, -2);
		/* ... handler node handlers */
		duk_pop(ctx);
		/* ... handler node */
	}
	/** @todo handle other kinds of event than the generic case */
	dom_string_unref(name);
	/* ... handler node */
	dukky_push_event(ctx, evt);
	/* ... handler node event */
	if (duk_pcall_method(ctx, 1) != 0) {
		/* Failed to run the method */
		/* ... err */
		LOG("OH NOES! An error running a callback.  Meh.");
		exc = dom_event_stop_immediate_propagation(evt);
		if (exc != DOM_NO_ERR)
			LOG("WORSE! could not stop propagation");
		duk_pop(ctx);
		/* ... */
		return;
	}
	/* ... result */
	if (duk_is_boolean(ctx, -1) &&
	    duk_to_boolean(ctx, -1) == 0) {
		dom_event_prevent_default(evt);
	}
	duk_pop(ctx);
	/* ... */
}

void dukky_register_event_listener_for(duk_context *ctx,
				       struct dom_element *ele,
				       dom_string *name)
{
	dom_event_listener *listen = NULL;
	dom_exception exc;

	/* ... */
	if (dukky_push_node(ctx, (struct dom_node *)ele) == false)
		return;
	/* ... node */
	duk_get_prop_string(ctx, -1, HANDLER_LISTENER_MAGIC);
	/* ... node handlers */
	duk_push_lstring(ctx, dom_string_data(name), dom_string_length(name));
	/* ... node handlers name */
	if (duk_has_prop(ctx, -2)) {
		/* ... node handlers */
		duk_pop_2(ctx);
		/* ... */
		return;
	}
	/* ... node handlers */
	duk_push_lstring(ctx, dom_string_data(name), dom_string_length(name));
	/* ... node handlers name */
	duk_push_boolean(ctx, true);
	/* ... node handlers name true */
	duk_put_prop(ctx, -3);
	/* ... node handlers */
	duk_pop_2(ctx);
	/* ... */
	exc = dom_event_listener_create(dukky_generic_event_handler, ctx,
					&listen);
	if (exc != DOM_NO_ERR) return;
	exc = dom_event_target_add_event_listener(
		ele, name, listen, false);
	if (exc != DOM_NO_ERR) {
		LOG("Unable to register listener for %p.%*s",
		    ele, dom_string_length(name), dom_string_data(name));
	} else {
		LOG("have registered listener for %p.%*s",
		    ele, dom_string_length(name), dom_string_data(name));
	}
	dom_event_listener_unref(listen);
}


void js_handle_new_element(jscontext *ctx, struct dom_element *node)
{
	assert(ctx);
	assert(node);
	dom_namednodemap *map;
	dom_exception exc;
	dom_ulong idx;
	dom_ulong siz;
	dom_attr *attr = NULL;
	dom_string *key = NULL;

	exc = dom_node_get_attributes(node, &map);
	if (exc != DOM_NO_ERR) return;
	if (map == NULL) return;

	exc = dom_namednodemap_get_length(map, &siz);
	if (exc != DOM_NO_ERR) goto out;

	for (idx = 0; idx < siz; idx++) {
		exc = dom_namednodemap_item(map, idx, &attr);
		if (exc != DOM_NO_ERR) goto out;
		exc = dom_attr_get_name(attr, &key);
		if (exc != DOM_NO_ERR) goto out;
		if (dom_string_length(key) > 2) {
			/* Can be on* */
			const uint8_t *data = (const uint8_t *)dom_string_data(key);
			if (data[0] == 'o' && data[1] == 'n') {
				dom_string *sub = NULL;
				exc = dom_string_substr(
					key, 2, dom_string_length(key),
					&sub);
				if (exc == DOM_NO_ERR) {
					dukky_register_event_listener_for(
						CTX, node, sub);
					dom_string_unref(sub);
				}
			}
		}
		dom_string_unref(key); key = NULL;
		dom_node_unref(attr); attr = NULL;
	}

out:
	if (key != NULL)
		dom_string_unref(key);

	if (attr != NULL)
		dom_node_unref(attr);

	dom_namednodemap_unref(map);
}

void js_event_cleanup(jscontext *ctx, struct dom_event *evt)
{
	assert(ctx);
	/* ... */
	duk_get_global_string(CTX, EVENT_MAGIC);
	/* ... EVENT_MAP */
	duk_push_pointer(CTX, evt);
	/* ... EVENT_MAP eventptr */
	duk_del_prop(CTX, -2);
	/* ... EVENT_MAP */
	duk_pop(CTX);
	/* ... */
}
