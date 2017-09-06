/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 * Copyright 2015 Daniel Dilverstone <dsilvers@netsurf-browser.org>
 * Copyright 2016 Michael Drake <tlsa@netsurf-browser.org>
 * Copyright 2016 John-Mark Bell <jmb@netsurf-browser.org>
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

#include <stdint.h>
#include <nsutils/time.h>

#include "netsurf/inttypes.h"
#include "utils/utils.h"
#include "utils/nsoption.h"
#include "utils/log.h"
#include "utils/corestrings.h"
#include "content/content.h"

#include "javascript/js.h"
#include "javascript/content.h"

#include "duktape/binding.h"

#include "duktape.h"
#include "dukky.h"

#include <dom/dom.h>

#define EVENT_MAGIC MAGIC(EVENT_MAP)
#define HANDLER_LISTENER_MAGIC MAGIC(HANDLER_LISTENER_MAP)
#define HANDLER_MAGIC MAGIC(HANDLER_MAP)
#define EVENT_LISTENER_JS_MAGIC MAGIC(EVENT_LISTENER_JS_MAP)

static duk_ret_t dukky_populate_object(duk_context *ctx, void *udata)
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
		NSLOG(netsurf, INFO,
		      "RuhRoh, couldn't find a prototype, HTMLUnknownElement it is");
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
	NSLOG(netsurf, INFO, "Call the init function");
	duk_call(ctx, nargs + 1);
	return 1; /* The object */
}

duk_ret_t dukky_create_object(duk_context *ctx, const char *name, int args)
{
	duk_ret_t ret;
	NSLOG(netsurf, INFO, "name=%s nargs=%d", name + 2, args);
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
	if ((ret = duk_safe_call(ctx, dukky_populate_object, NULL, args + 3, 1))
	    != DUK_EXEC_SUCCESS)
		return ret;
	NSLOG(netsurf, INFO, "created");
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
		if (duk_safe_call(ctx, dukky_populate_object, NULL, 4, 1)
		    != DUK_EXEC_SUCCESS) {
			duk_set_top(ctx, top_at_fail);
			NSLOG(netsurf, INFO, "Boo and also hiss");
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

#define SET_HTML_CLASS(CLASS) \
		*html_class = PROTO_NAME(HTML##CLASS##ELEMENT); \
		*html_class_len = \
				SLEN(PROTO_NAME(HTML)) + \
				SLEN(#CLASS) + \
				SLEN("ELEMENT");

static void dukky_html_element_class_from_tag_type(dom_html_element_type type,
		const char **html_class, size_t *html_class_len)
{
	switch(type) {
	case DOM_HTML_ELEMENT_TYPE_HTML:
		SET_HTML_CLASS(HTML)
		break;
	case DOM_HTML_ELEMENT_TYPE_HEAD:
		SET_HTML_CLASS(HEAD)
		break;
	case DOM_HTML_ELEMENT_TYPE_META:
		SET_HTML_CLASS(META)
		break;
	case DOM_HTML_ELEMENT_TYPE_BASE:
		SET_HTML_CLASS(BASE)
		break;
	case DOM_HTML_ELEMENT_TYPE_TITLE:
		SET_HTML_CLASS(TITLE)
		break;
	case DOM_HTML_ELEMENT_TYPE_BODY:
		SET_HTML_CLASS(BODY)
		break;
	case DOM_HTML_ELEMENT_TYPE_DIV:
		SET_HTML_CLASS(DIV)
		break;
	case DOM_HTML_ELEMENT_TYPE_FORM:
		SET_HTML_CLASS(FORM)
		break;
	case DOM_HTML_ELEMENT_TYPE_LINK:
		SET_HTML_CLASS(LINK)
		break;
	case DOM_HTML_ELEMENT_TYPE_BUTTON:
		SET_HTML_CLASS(BUTTOM)
		break;
	case DOM_HTML_ELEMENT_TYPE_INPUT:
		SET_HTML_CLASS(INPUT)
		break;
	case DOM_HTML_ELEMENT_TYPE_TEXTAREA:
		SET_HTML_CLASS(TEXTAREA)
		break;
	case DOM_HTML_ELEMENT_TYPE_OPTGROUP:
		SET_HTML_CLASS(OPTGROUP)
		break;
	case DOM_HTML_ELEMENT_TYPE_OPTION:
		SET_HTML_CLASS(OPTION)
		break;
	case DOM_HTML_ELEMENT_TYPE_SELECT:
		SET_HTML_CLASS(SELECT)
		break;
	case DOM_HTML_ELEMENT_TYPE_HR:
		SET_HTML_CLASS(HR)
		break;
	case DOM_HTML_ELEMENT_TYPE_DL:
		SET_HTML_CLASS(DLIST)
		break;
	case DOM_HTML_ELEMENT_TYPE_DIR:
		SET_HTML_CLASS(DIRECTORY)
		break;
	case DOM_HTML_ELEMENT_TYPE_MENU:
		SET_HTML_CLASS(MENU)
		break;
	case DOM_HTML_ELEMENT_TYPE_FIELDSET:
		SET_HTML_CLASS(FIELDSET)
		break;
	case DOM_HTML_ELEMENT_TYPE_LEGEND:
		SET_HTML_CLASS(LEGEND)
		break;
	case DOM_HTML_ELEMENT_TYPE_P:
		SET_HTML_CLASS(PARAGRAPH)
		break;
	case DOM_HTML_ELEMENT_TYPE_H1:
	case DOM_HTML_ELEMENT_TYPE_H2:
	case DOM_HTML_ELEMENT_TYPE_H3:
	case DOM_HTML_ELEMENT_TYPE_H4:
	case DOM_HTML_ELEMENT_TYPE_H5:
	case DOM_HTML_ELEMENT_TYPE_H6:
		SET_HTML_CLASS(HEADING)
		break;
	case DOM_HTML_ELEMENT_TYPE_BLOCKQUOTE:
	case DOM_HTML_ELEMENT_TYPE_Q:
		SET_HTML_CLASS(QUOTE)
		break;
	case DOM_HTML_ELEMENT_TYPE_PRE:
		SET_HTML_CLASS(PRE)
		break;
	case DOM_HTML_ELEMENT_TYPE_BR:
		SET_HTML_CLASS(BR)
		break;
	case DOM_HTML_ELEMENT_TYPE_LABEL:
		SET_HTML_CLASS(LABEL)
		break;
	case DOM_HTML_ELEMENT_TYPE_UL:
		SET_HTML_CLASS(ULIST)
		break;
	case DOM_HTML_ELEMENT_TYPE_OL:
		SET_HTML_CLASS(OLIST)
		break;
	case DOM_HTML_ELEMENT_TYPE_LI:
		SET_HTML_CLASS(LI)
		break;
	case DOM_HTML_ELEMENT_TYPE_FONT:
		SET_HTML_CLASS(FONT)
		break;
	case DOM_HTML_ELEMENT_TYPE_DEL:
	case DOM_HTML_ELEMENT_TYPE_INS:
		SET_HTML_CLASS(MOD)
		break;
	case DOM_HTML_ELEMENT_TYPE_A:
		SET_HTML_CLASS(ANCHOR)
		break;
	case DOM_HTML_ELEMENT_TYPE_BASEFONT:
		SET_HTML_CLASS(BASEFONT)
		break;
	case DOM_HTML_ELEMENT_TYPE_IMG:
		SET_HTML_CLASS(IMAGE)
		break;
	case DOM_HTML_ELEMENT_TYPE_OBJECT:
		SET_HTML_CLASS(OBJECT)
		break;
	case DOM_HTML_ELEMENT_TYPE_PARAM:
		SET_HTML_CLASS(PARAM)
		break;
	case DOM_HTML_ELEMENT_TYPE_APPLET:
		SET_HTML_CLASS(APPLET)
		break;
	case DOM_HTML_ELEMENT_TYPE_MAP:
		SET_HTML_CLASS(MAP)
		break;
	case DOM_HTML_ELEMENT_TYPE_AREA:
		SET_HTML_CLASS(AREA)
		break;
	case DOM_HTML_ELEMENT_TYPE_SCRIPT:
		SET_HTML_CLASS(SCRIPT)
		break;
	case DOM_HTML_ELEMENT_TYPE_CAPTION:
		SET_HTML_CLASS(TABLECAPTION)
		break;
	case DOM_HTML_ELEMENT_TYPE_TD:
	case DOM_HTML_ELEMENT_TYPE_TH:
		SET_HTML_CLASS(TABLECELL)
		break;
	case DOM_HTML_ELEMENT_TYPE_COL:
	case DOM_HTML_ELEMENT_TYPE_COLGROUP:
		SET_HTML_CLASS(TABLECOL)
		break;
	case DOM_HTML_ELEMENT_TYPE_THEAD:
	case DOM_HTML_ELEMENT_TYPE_TBODY:
	case DOM_HTML_ELEMENT_TYPE_TFOOT:
		SET_HTML_CLASS(TABLESECTION)
		break;
	case DOM_HTML_ELEMENT_TYPE_TABLE:
		SET_HTML_CLASS(TABLE)
		break;
	case DOM_HTML_ELEMENT_TYPE_TR:
		SET_HTML_CLASS(TABLEROW)
		break;
	case DOM_HTML_ELEMENT_TYPE_STYLE:
		SET_HTML_CLASS(STYLE)
		break;
	case DOM_HTML_ELEMENT_TYPE_FRAMESET:
		SET_HTML_CLASS(FRAMESET)
		break;
	case DOM_HTML_ELEMENT_TYPE_FRAME:
		SET_HTML_CLASS(FRAME)
		break;
	case DOM_HTML_ELEMENT_TYPE_IFRAME:
		SET_HTML_CLASS(IFRAME)
		break;
	case DOM_HTML_ELEMENT_TYPE_ISINDEX:
		SET_HTML_CLASS(ISINDEX)
		break;
	case DOM_HTML_ELEMENT_TYPE__COUNT:
		assert(type != DOM_HTML_ELEMENT_TYPE__COUNT);
		/* fallthrough */
	case DOM_HTML_ELEMENT_TYPE__UNKNOWN:
		SET_HTML_CLASS(UNKNOWN)
		break;
	default:
		/* Known HTML element without a specialisation */
		*html_class = PROTO_NAME(HTMLELEMENT);
		*html_class_len =
				SLEN(PROTO_NAME(HTML)) +
				SLEN("ELEMENT");
		break;
	}
	return;
}

#undef SET_HTML_CLASS

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
		dom_string *namespace;
		dom_html_element_type type;
		const char *html_class;
		size_t html_class_len;
		err = dom_node_get_namespace(node, &namespace);
		if (err != DOM_NO_ERR) {
			/* Feck it, element */
			NSLOG(netsurf, INFO,
			      "dom_node_get_namespace() failed");
			duk_push_string(ctx, PROTO_NAME(ELEMENT));
			break;
		}
		if (namespace == NULL) {
			/* No namespace, -> element */
			NSLOG(netsurf, INFO, "no namespace");
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

		err = dom_html_element_get_tag_type(node, &type);
		if (err != DOM_NO_ERR) {
			type = DOM_HTML_ELEMENT_TYPE__UNKNOWN;
		}

		dukky_html_element_class_from_tag_type(type,
				&html_class, &html_class_len);

		duk_push_lstring(ctx, html_class, html_class_len);
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
	JS_LOG("Pushing node %p", node);
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
		JS_LOG("Found it memoised");
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
	return duk_error(ctx, DUK_ERR_ERROR, "Bad constructor");
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

/* Duktape heap utility functions */

/* We need to override the defaults because not all platforms are fully ANSI
 * compatible.  E.g. RISC OS gets upset if we malloc or realloc a zero byte
 * block, as do debugging tools such as Electric Fence by Bruce Perens.
 */

static void *dukky_alloc_function(void *udata, duk_size_t size)
{
	if (size == 0)
		return NULL;

	return malloc(size);
}

static void *dukky_realloc_function(void *udata, void *ptr, duk_size_t size)
{
	if (ptr == NULL && size == 0)
		return NULL;

	if (size == 0) {
		free(ptr);
		return NULL;
	}

	return realloc(ptr, size);
}

static void dukky_free_function(void *udata, void *ptr)
{
	if (ptr != NULL)
		free(ptr);
}


/**************************************** js.h ******************************/
struct jscontext {
	duk_context *ctx;
	duk_context *thread;
	uint64_t exec_start_time;
};

#define CTX (ctx->thread)

void js_initialise(void)
{
	/** TODO: Forces JS on for our testing, needs changing before a release
	 * lest we incur the wrath of others.
	 */
	/* Disabled force-on for forthcoming release */
	/* nsoption_set_bool(enable_javascript, true);
	 */
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
	NSLOG(netsurf, INFO, "Creating new duktape javascript context");
	if (ret == NULL) return NSERROR_NOMEM;
	ctx = ret->ctx = duk_create_heap(
		dukky_alloc_function,
		dukky_realloc_function,
		dukky_free_function,
		ret,
		NULL);
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
	NSLOG(netsurf, INFO, "Destroying duktape javascript context");
	duk_destroy_heap(ctx->ctx);
	free(ctx);
}

jsobject *js_newcompartment(jscontext *ctx, void *win_priv, void *doc_priv)
{
	assert(ctx != NULL);
	/* Pop any active thread off */
	NSLOG(netsurf, INFO,
	      "Yay, new compartment, win_priv=%p, doc_priv=%p", win_priv,
	      doc_priv);
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

static duk_ret_t eval_top_string(duk_context *ctx, void *udata)
{
	duk_eval(ctx);
	return 0;
}

duk_bool_t dukky_check_timeout(void *udata)
{
#define JS_EXEC_TIMEOUT_MS 10000 /* 10 seconds */
	jscontext *ctx = (jscontext *) udata;
	uint64_t now;

	(void) nsu_getmonotonic_ms(&now);

	/* This function may be called during duk heap construction,
	 * so only test for execution timeout if we've recorded a
	 * start time.
	 */
	return ctx->exec_start_time != 0 &&
			now > (ctx->exec_start_time + JS_EXEC_TIMEOUT_MS);
}

bool js_exec(jscontext *ctx, const char *txt, size_t txtlen)
{
	assert(ctx);
	if (txt == NULL || txtlen == 0) return false;
	duk_set_top(CTX, 0);
	duk_push_lstring(CTX, txt, txtlen);

	(void) nsu_getmonotonic_ms(&ctx->exec_start_time);
	if (duk_safe_call(CTX, eval_top_string, NULL, 1, 1) == DUK_EXEC_ERROR) {
		duk_get_prop_string(CTX, 0, "name");
		duk_get_prop_string(CTX, 0, "message");
		duk_get_prop_string(CTX, 0, "fileName");
		duk_get_prop_string(CTX, 0, "lineNumber");
		duk_get_prop_string(CTX, 0, "stack");
		NSLOG(netsurf, INFO, "Uncaught error in JS: %s: %s",
		      duk_safe_to_string(CTX, 1), duk_safe_to_string(CTX, 2));
		NSLOG(netsurf, INFO, "              was at: %s line %s",
		      duk_safe_to_string(CTX, 3), duk_safe_to_string(CTX, 4));
		NSLOG(netsurf, INFO, "         Stack trace: %s",
		      duk_safe_to_string(CTX, 5));
		return false;
	}
	if (duk_get_top(CTX) == 0) duk_push_boolean(CTX, false);
	NSLOG(netsurf, INFO, "Returning %s",
	      duk_get_boolean(CTX, 0) ? "true" : "false");
	return duk_get_boolean(CTX, 0);
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
		duk_push_pointer(ctx, evt);
		if (dukky_create_object(ctx, PROTO_NAME(EVENT), 1) != DUK_EXEC_SUCCESS) {
			/* ... events err */
			duk_pop(ctx);
			/* ... events */
			duk_push_object(ctx);
			/* ... events eobj[meh] */
		}
		/* ... events eobj */
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

static void dukky_push_handler_code_(duk_context *ctx, dom_string *name,
				     dom_event_target *et)
{
	dom_string *onname, *val;
	dom_element *ele = (dom_element *)et;
	dom_exception exc;
	dom_node_type ntype;

	/* Currently safe since libdom has no event targets which are not
	 * nodes.  Reconsider this as and when we work out how to have
	 * window do stuff
	 */
	exc = dom_node_get_node_type(et, &ntype);
	if (exc != DOM_NO_ERR) {
		duk_push_lstring(ctx, "", 0);
		return;
	}

	if (ntype != DOM_ELEMENT_NODE) {
		duk_push_lstring(ctx, "", 0);
		return;
	}

	exc = dom_string_concat(corestring_dom_on, name, &onname);
	if (exc != DOM_NO_ERR) {
		duk_push_lstring(ctx, "", 0);
		return;
	}

	exc = dom_element_get_attribute(ele, onname, &val);
	if ((exc != DOM_NO_ERR) || (val == NULL)) {
		dom_string_unref(onname);
		duk_push_lstring(ctx, "", 0);
		return;
	}

	dom_string_unref(onname);
	duk_push_lstring(ctx, dom_string_data(val), dom_string_length(val));
	dom_string_unref(val);
}

bool dukky_get_current_value_of_event_handler(duk_context *ctx,
					      dom_string *name,
					      dom_event_target *et)
{
	/* Must be entered as:
	 * ... node(et)
	 */
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
		dukky_push_handler_code_(ctx, name, et);
		/* ... node handlercode? */
		/* TODO: If this is null, clean up and propagate */
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
			NSLOG(netsurf, INFO,
			      "Unable to proceed with handler, could not compile");
			duk_pop_2(ctx);
			return false;
		}
		/* ... node handler */
		duk_insert(ctx, -2);
		/* ... handler node */
	} else {
		/* ... node handlers handler */
		duk_insert(ctx, -3);
		/* ... handler node handlers */
		duk_pop(ctx);
		/* ... handler node */
	}
	/* ... handler node */
	return true;
}

static void dukky_generic_event_handler(dom_event *evt, void *pw)
{
	duk_memory_functions funcs;
	duk_context *ctx = (duk_context *)pw;
	jscontext *jsctx;
	dom_string *name;
	dom_exception exc;
	dom_event_target *targ;
	dom_event_flow_phase phase;
	duk_uarridx_t idx;
	event_listener_flags flags;

	/* Retrieve the JS context from the Duktape context */
	duk_get_memory_functions(ctx, &funcs);
	jsctx = funcs.udata;

	NSLOG(netsurf, INFO, "WOOP WOOP, An event:");
	exc = dom_event_get_type(evt, &name);
	if (exc != DOM_NO_ERR) {
		NSLOG(netsurf, INFO, "Unable to find the event name");
		return;
	}
	NSLOG(netsurf, INFO, "Event's name is %*s", dom_string_length(name),
	      dom_string_data(name));
	exc = dom_event_get_event_phase(evt, &phase);
	if (exc != DOM_NO_ERR) {
		NSLOG(netsurf, INFO, "Unable to get event phase");
		return;
	}
	NSLOG(netsurf, INFO, "Event phase is: %s (%d)",
	      phase == DOM_CAPTURING_PHASE ? "capturing" : phase == DOM_AT_TARGET ? "at-target" : phase == DOM_BUBBLING_PHASE ? "bubbling" : "unknown",
	      (int)phase);

	exc = dom_event_get_current_target(evt, &targ);
	if (exc != DOM_NO_ERR) {
		dom_string_unref(name);
		NSLOG(netsurf, INFO, "Unable to find the event target");
		return;
	}

	/* If we're capturing right now, we skip the 'event handler'
	 * and go straight to the extras
	 */
	if (phase == DOM_CAPTURING_PHASE)
		goto handle_extras;

	/* ... */
	if (dukky_push_node(ctx, (dom_node *)targ) == false) {
		dom_string_unref(name);
		dom_node_unref(targ);
		NSLOG(netsurf, INFO,
		      "Unable to push JS node representation?!");
		return;
	}
	/* ... node */
	if (dukky_get_current_value_of_event_handler(
		    ctx, name, (dom_event_target *)targ) == false) {
		/* ... */
		goto handle_extras;
	}
	/* ... handler node */
	dukky_push_event(ctx, evt);
	/* ... handler node event */
	(void) nsu_getmonotonic_ms(&jsctx->exec_start_time);
	if (duk_pcall_method(ctx, 1) != 0) {
		/* Failed to run the method */
		/* ... err */
		NSLOG(netsurf, INFO,
		      "OH NOES! An error running a callback.  Meh.");
		exc = dom_event_stop_immediate_propagation(evt);
		if (exc != DOM_NO_ERR)
			NSLOG(netsurf, INFO,
			      "WORSE! could not stop propagation");
		duk_get_prop_string(ctx, -1, "name");
		duk_get_prop_string(ctx, -2, "message");
		duk_get_prop_string(ctx, -3, "fileName");
		duk_get_prop_string(ctx, -4, "lineNumber");
		duk_get_prop_string(ctx, -5, "stack");
		/* ... err name message fileName lineNumber stack */
		NSLOG(netsurf, INFO, "Uncaught error in JS: %s: %s",
		      duk_safe_to_string(ctx, -5),
		      duk_safe_to_string(ctx, -4));
		NSLOG(netsurf, INFO, "              was at: %s line %s",
		      duk_safe_to_string(ctx, -3),
		      duk_safe_to_string(ctx, -2));
		NSLOG(netsurf, INFO, "         Stack trace: %s",
		      duk_safe_to_string(ctx, -1));

		duk_pop_n(ctx, 6);
		/* ... */
		goto handle_extras;
	}
	/* ... result */
	if (duk_is_boolean(ctx, -1) &&
	    duk_to_boolean(ctx, -1) == 0) {
		dom_event_prevent_default(evt);
	}
	duk_pop(ctx);
handle_extras:
	/* ... */
	duk_push_lstring(ctx, dom_string_data(name), dom_string_length(name));
	dukky_push_node(ctx, (dom_node *)targ);
	/* ... type node */
	if (dukky_event_target_push_listeners(ctx, true)) {
		/* Nothing to do */
		duk_pop(ctx);
		goto out;
	}
	/* ... sublisteners */
	duk_push_array(ctx);
	/* ... sublisteners copy */
	idx = 0;
	while (duk_get_prop_index(ctx, -2, idx)) {
		/* ... sublisteners copy handler */
		duk_get_prop_index(ctx, -1, 1);
		/* ... sublisteners copy handler flags */
		if ((event_listener_flags)duk_to_int(ctx, -1) & ELF_ONCE) {
			duk_dup(ctx, -4);
			/* ... subl copy handler flags subl */
			dukky_shuffle_array(ctx, idx);
			duk_pop(ctx);
			/* ... subl copy handler flags */
		}
		duk_pop(ctx);
		/* ... sublisteners copy handler */
		duk_put_prop_index(ctx, -2, idx);
		/* ... sublisteners copy */
		idx++;
	}
	/* ... sublisteners copy undefined */
	duk_pop(ctx);
	/* ... sublisteners copy */
	duk_insert(ctx, -2);
	/* ... copy sublisteners */
	duk_pop(ctx);
	/* ... copy */
	idx = 0;
	while (duk_get_prop_index(ctx, -1, idx++)) {
		/* ... copy handler */
		if (duk_get_prop_index(ctx, -1, 2)) {
			/* ... copy handler meh */
			duk_pop_2(ctx);
			continue;
		}
		duk_pop(ctx);
		duk_get_prop_index(ctx, -1, 0);
		duk_get_prop_index(ctx, -2, 1);
		/* ... copy handler callback flags */
		flags = (event_listener_flags)duk_get_int(ctx, -1);
		duk_pop(ctx);
		/* ... copy handler callback */
		if (((phase == DOM_CAPTURING_PHASE) && !(flags & ELF_CAPTURE)) ||
		    ((phase != DOM_CAPTURING_PHASE) && (flags & ELF_CAPTURE))) {
			duk_pop_2(ctx);
			/* ... copy */
			continue;
		}
		/* ... copy handler callback */
		dukky_push_node(ctx, (dom_node *)targ);
		/* ... copy handler callback node */
		dukky_push_event(ctx, evt);
		/* ... copy handler callback node event */
		(void) nsu_getmonotonic_ms(&jsctx->exec_start_time);
		if (duk_pcall_method(ctx, 1) != 0) {
			/* Failed to run the method */
			/* ... copy handler err */
			NSLOG(netsurf, INFO,
			      "OH NOES! An error running a callback.  Meh.");
			exc = dom_event_stop_immediate_propagation(evt);
			if (exc != DOM_NO_ERR)
				NSLOG(netsurf, INFO,
				      "WORSE! could not stop propagation");
			duk_get_prop_string(ctx, -1, "name");
			duk_get_prop_string(ctx, -2, "message");
			duk_get_prop_string(ctx, -3, "fileName");
			duk_get_prop_string(ctx, -4, "lineNumber");
			duk_get_prop_string(ctx, -5, "stack");
			/* ... err name message fileName lineNumber stack */
			NSLOG(netsurf, INFO, "Uncaught error in JS: %s: %s",
			      duk_safe_to_string(ctx, -5),
			      duk_safe_to_string(ctx, -4));
			NSLOG(netsurf, INFO,
			      "              was at: %s line %s",
			      duk_safe_to_string(ctx, -3),
			      duk_safe_to_string(ctx, -2));
			NSLOG(netsurf, INFO, "         Stack trace: %s",
			      duk_safe_to_string(ctx, -1));

			duk_pop_n(ctx, 7);
			/* ... copy */
			continue;
		}
		/* ... copy handler result */
		if (duk_is_boolean(ctx, -1) &&
		    duk_to_boolean(ctx, -1) == 0) {
			dom_event_prevent_default(evt);
		}
		duk_pop_2(ctx);
		/* ... copy */
	}
	duk_pop_2(ctx);
out:
	/* ... */
	dom_node_unref(targ);
	dom_string_unref(name);
}

void dukky_register_event_listener_for(duk_context *ctx,
				       struct dom_element *ele,
				       dom_string *name,
				       bool capture)
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
		ele, name, listen, capture);
	if (exc != DOM_NO_ERR) {
		NSLOG(netsurf, INFO,
		      "Unable to register listener for %p.%*s", ele,
		      dom_string_length(name), dom_string_data(name));
	} else {
		NSLOG(netsurf, INFO, "have registered listener for %p.%*s",
		      ele, dom_string_length(name), dom_string_data(name));
	}
	dom_event_listener_unref(listen);
}

/* The sub-listeners are a list of {callback,flags} tuples */
/* We return true if we created a new sublistener table */
/* If we're told to not create, but we want to, we still return true */
bool dukky_event_target_push_listeners(duk_context *ctx, bool dont_create)
{
	bool ret = false;
	/* ... type this */
	duk_get_prop_string(ctx, -1, EVENT_LISTENER_JS_MAGIC);
	if (duk_is_undefined(ctx, -1)) {
		/* ... type this null */
		duk_pop(ctx);
		duk_push_object(ctx);
		duk_dup(ctx, -1);
		/* ... type this listeners listeners */
		duk_put_prop_string(ctx, -3, EVENT_LISTENER_JS_MAGIC);
		/* ... type this listeners */
	}
	/* ... type this listeners */
	duk_insert(ctx, -3);
	/* ... listeners type this */
	duk_pop(ctx);
	/* ... listeners type */
	duk_dup(ctx, -1);
	/* ... listeners type type */
	duk_get_prop(ctx, -3);
	/* ... listeners type ??? */
	if (duk_is_undefined(ctx, -1)) {
		/* ... listeners type ??? */
		if (dont_create == true) {
			duk_pop_3(ctx);
			duk_push_undefined(ctx);
			return true;
		}
		duk_pop(ctx);
		duk_push_array(ctx);
		duk_dup(ctx, -2);
		duk_dup(ctx, -2);
		/* ... listeners type sublisteners type sublisteners */
		duk_put_prop(ctx, -5);
		/* ... listeners type sublisteners */
		ret = true;
	}
	duk_insert(ctx, -3);
	/* ... sublisteners listeners type */
	duk_pop_2(ctx);
	/* ... sublisteners */
	return ret;
}

/* Shuffle a duktape array "down" one.  This involves iterating from
 * the index provided, shuffling elements down, until we reach an
 * undefined
 */
void dukky_shuffle_array(duk_context *ctx, duk_uarridx_t idx)
{
	/* ... somearr */
	while (duk_get_prop_index(ctx, -1, idx + 1)) {
		duk_put_prop_index(ctx, -2, idx);
		idx++;
	}
	/* ... somearr undefined */
	duk_del_prop_index(ctx, -2, idx + 1);
	duk_pop(ctx);
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
	dom_string *nodename;
	duk_bool_t is_body = false;

	exc = dom_node_get_node_name(node, &nodename);
	if (exc != DOM_NO_ERR) return;

	if (nodename == corestring_dom_BODY)
		is_body = true;

	dom_string_unref(nodename);

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
		if (is_body && (
			    key == corestring_dom_onblur ||
			    key == corestring_dom_onerror ||
			    key == corestring_dom_onfocus ||
			    key == corestring_dom_onload ||
			    key == corestring_dom_onresize ||
			    key == corestring_dom_onscroll)) {
			/* This is a forwarded event, it doesn't matter,
			 * we should skip registering for it and later
			 * we will register it for Window itself
			 */
			goto skip_register;
		}
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
						CTX, node, sub, false);
					dom_string_unref(sub);
				}
			}
		}
	skip_register:
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

bool js_fire_event(jscontext *ctx, const char *type, struct dom_document *doc, struct dom_node *target)
{
	dom_exception exc;
	dom_event *evt;
	dom_event_target *body;

	NSLOG(netsurf, INFO, "Event: %s (doc=%p, target=%p)", type, doc,
	      target);

	/** @todo Make this more generic, this only handles load and only
	 * targetting the window, so that we actually stand a chance of
	 * getting 3.4 out.
	 */

	if (target != NULL)
		/* Swallow non-Window-targetted events quietly */
		return true;

	if (strcmp(type, "load") != 0)
		/* Swallow non-load events quietly */
		return true;

	/* Okay, we're processing load, targetted at Window, do the single
	 * thing which gets us there, which is to find the appropriate event
	 * handler and call it.  If we have no event handler on Window then
	 * we divert to the body, and if there's no event handler there
	 * we swallow the event silently
	 */

	exc = dom_event_create(&evt);
	if (exc != DOM_NO_ERR) return true;
	exc = dom_event_init(evt, corestring_dom_load, false, false);
	if (exc != DOM_NO_ERR) {
		dom_event_unref(evt);
		return true;
	}
	/* ... */
	duk_get_global_string(CTX, HANDLER_MAGIC);
	/* ... handlers */
	duk_push_lstring(CTX, "load", 4);
	/* ... handlers "load" */
	duk_get_prop(CTX, -2);
	/* ... handlers handler? */
	if (duk_is_undefined(CTX, -1)) {
		/* No handler here, *try* and retrieve a handler from
		 * the body
		 */
		duk_pop(CTX);
		/* ... handlers */
		exc = dom_html_document_get_body(doc, &body);
		if (exc != DOM_NO_ERR) {
			dom_event_unref(evt);
			return true;
		}
		dukky_push_node(CTX, (struct dom_node *)body);
		/* ... handlers bodynode */
		if (dukky_get_current_value_of_event_handler(
			    CTX, corestring_dom_load, body) == false) {
			/* ... handlers */
			duk_pop(CTX);
			return true;
		}
		/* ... handlers handler bodynode */
		duk_pop(CTX);
	}
	/* ... handlers handler */
	duk_insert(CTX, -2);
	/* ... handler handlers */
	duk_pop(CTX);
	/* ... handler */
	duk_push_global_object(CTX);
	/* ... handler Window */
	dukky_push_event(CTX, evt);
	/* ... handler Window event */
	(void) nsu_getmonotonic_ms(&ctx->exec_start_time);
	if (duk_pcall_method(CTX, 1) != 0) {
		/* Failed to run the handler */
		/* ... err */
		NSLOG(netsurf, INFO,
		      "OH NOES! An error running a handler.  Meh.");
		duk_get_prop_string(CTX, -1, "name");
		duk_get_prop_string(CTX, -2, "message");
		duk_get_prop_string(CTX, -3, "fileName");
		duk_get_prop_string(CTX, -4, "lineNumber");
		duk_get_prop_string(CTX, -5, "stack");
		/* ... err name message fileName lineNumber stack */
		NSLOG(netsurf, INFO, "Uncaught error in JS: %s: %s",
		      duk_safe_to_string(CTX, -5),
		      duk_safe_to_string(CTX, -4));
		NSLOG(netsurf, INFO, "              was at: %s line %s",
		      duk_safe_to_string(CTX, -3),
		      duk_safe_to_string(CTX, -2));
		NSLOG(netsurf, INFO, "         Stack trace: %s",
		      duk_safe_to_string(CTX, -1));

		duk_pop_n(CTX, 6);
		/* ... */
		js_event_cleanup(ctx, evt);
		dom_event_unref(evt);
		return true;
	}
	/* ... result */
	duk_pop(CTX);
	/* ... */
	js_event_cleanup(ctx, evt);
	dom_event_unref(evt);
	return true;
}
