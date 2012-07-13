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
 * Content for text/html scripts (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "utils/config.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "javascript/js.h"
#include "content/content_protected.h"
#include "content/fetch.h"
#include "render/html_internal.h"

typedef bool (script_handler_t)(struct jscontext *jscontext, const char *data, size_t size) ;


static script_handler_t *select_script_handler(content_type ctype)
{
	if (ctype == CONTENT_JS) {
		return js_exec;
	}
	return NULL;
}


/* attempt to progress script execution
 *
 * execute scripts using algorithm found in:
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/scripting-1.html#the-script-element
 *
 */
static bool html_scripts_exec(html_content *c)
{
	unsigned int i;
	struct html_script *s;
	script_handler_t *script_handler;

	if (c->jscontext == NULL)
		return false;

	for (i = 0, s = c->scripts; i != c->scripts_count; i++, s++) {
		if (s->already_started) {
			continue;
		}

		assert((s->type == HTML_SCRIPT_EXTERNAL) ||
		       (s->type == HTML_SCRIPT_INTERNAL));

		if (s->type == HTML_SCRIPT_EXTERNAL) {
			/* ensure script content is present */
			if (s->data.external == NULL)
				continue;

			/* ensure script content fetch status is not an error */
			if (content_get_status(s->data.external) == CONTENT_STATUS_ERROR)
				continue;

			/* ensure script handler for content type */
			script_handler = select_script_handler(content_get_type(s->data.external));
			if (script_handler == NULL)
				continue; /* unsupported type */

			if (content_get_status(s->data.external) == CONTENT_STATUS_DONE) {
				/* external script is now available */
				const char *data;
				unsigned long size;
				data = content_get_source_data(s->data.external, &size );
				script_handler(c->jscontext, data, size);

				s->already_started = true;

			} else {
				/* script not yet available */

				/* check if deferable or asynchronous */
				if (!s->defer && !s->async) {
					break;
				}
			}
		}
	}

	return true;
}

/* create new html script entry */
static struct html_script *
html_process_new_script(html_content *c, enum html_script_type type)
{
	struct html_script *nscript;
	/* add space for new script entry */
	nscript = realloc(c->scripts,
			  sizeof(struct html_script) * (c->scripts_count + 1));
	if (nscript == NULL) {
		return NULL;
	}

	c->scripts = nscript;

	/* increment script entry count */
	nscript = &c->scripts[c->scripts_count];
	c->scripts_count++;

	nscript->already_started = false;
	nscript->parser_inserted = false;
	nscript->force_async = true;
	nscript->ready_exec = false;
	nscript->async = false;
	nscript->defer = false;

	nscript->type = type;

	return nscript;
}

/**
 * Callback for fetchcache() for linked stylesheets.
 */

static nserror
html_convert_script_callback(hlcache_handle *script,
			  const hlcache_event *event,
			  void *pw)
{
	html_content *parent = pw;
	unsigned int i;
	struct html_script *s;

	/* Find script */
	for (i = 0, s = parent->scripts; i != parent->scripts_count; i++, s++) {
		if (s->type == HTML_SCRIPT_EXTERNAL &&
		    s->data.external == script)
			break;
	}

	assert(i != parent->scripts_count);

	switch (event->type) {
	case CONTENT_MSG_LOADING:
		break;

	case CONTENT_MSG_READY:
		break;

	case CONTENT_MSG_DONE:
		LOG(("script %d done '%s'", i,
				nsurl_access(hlcache_handle_get_url(script))));
		parent->base.active--;
		LOG(("%d fetches active", parent->base.active));

		/* script finished loading so try and continue execution */
		html_scripts_exec(parent);
		break;

	case CONTENT_MSG_ERROR:
		LOG(("script %s failed: %s",
				nsurl_access(hlcache_handle_get_url(script)),
				event->data.error));
		hlcache_handle_release(script);
		s->data.external = NULL;
		parent->base.active--;
		LOG(("%d fetches active", parent->base.active));
		content_add_error(&parent->base, "?", 0);

		/* script failed loading so try and continue execution */
		html_scripts_exec(parent);

		break;

	case CONTENT_MSG_STATUS:
		html_set_status(parent, content_get_status_message(script));
		content_broadcast(&parent->base, CONTENT_MSG_STATUS,
				event->data);
		break;

	default:
		assert(0);
	}

	if (parent->base.active == 0)
		html_finish_conversion(parent);

	return NSERROR_OK;
}

/** process script node
 *
 *
 */
dom_hubbub_error
html_process_script(void *ctx, dom_node *node)
{
	html_content *c = (html_content *)ctx;
	dom_exception exc; /* returned by libdom functions */
	dom_string *src, *script, *mimetype;
	struct html_script *nscript;
	union content_msg_data msg_data;

	/* ensure javascript context is available */
	if (c->jscontext == NULL) {
		union content_msg_data msg_data;

		msg_data.jscontext = &c->jscontext;
		content_broadcast(&c->base, CONTENT_MSG_GETCTX, msg_data);
		LOG(("javascript context %p ", c->jscontext));
		if (c->jscontext == NULL) {
			/* no context and it could not be created, abort */
			return DOM_HUBBUB_OK;
		}
	}

	LOG(("content %p parser %p node %p",c,c->parser_binding, node));

	exc = dom_element_get_attribute(node, html_dom_string_type, &mimetype);
	if (exc != DOM_NO_ERR || mimetype == NULL) {
		mimetype = dom_string_ref(html_dom_string_text_javascript);
	}

	exc = dom_element_get_attribute(node, html_dom_string_src, &src);
	if (exc != DOM_NO_ERR || src == NULL) {
		struct lwc_string_s *lwcmimetype;
		script_handler_t *script_handler;

		/* does not appear to be a src so script is inline content */
		exc = dom_node_get_text_content(node, &script);
		if ((exc != DOM_NO_ERR) || (script == NULL)) {
			dom_string_unref(mimetype);
			return DOM_HUBBUB_OK; /* no contents, skip */
		}

		nscript = html_process_new_script(c, HTML_STYLESHEET_INTERNAL);
		if (nscript == NULL) {
			dom_string_unref(mimetype);
			dom_string_unref(script);
			goto html_process_script_no_memory;
		}

		nscript->data.internal = script;
		nscript->mimetype = mimetype;
		nscript->already_started = true;

		/* charset (encoding) */

		/* ensure script handler for content type */
		dom_string_intern(mimetype, &lwcmimetype);
		script_handler = select_script_handler(content_factory_type_from_mime_type(lwcmimetype));
		lwc_string_unref(lwcmimetype);

		if (script_handler != NULL) {
			script_handler(c->jscontext,
				       dom_string_data(script),
				       dom_string_byte_length(script));
		}


	} else {
		/* script with a src tag */
		nserror ns_error;
		nsurl *joined;
		hlcache_child_context child;


		nscript = html_process_new_script(c, HTML_STYLESHEET_EXTERNAL);
		if (nscript == NULL) {
			dom_string_unref(src);
			dom_string_unref(mimetype);
			goto html_process_script_no_memory;
		}

		/* charset (encoding) */

		ns_error = nsurl_join(c->base_url, dom_string_data(src), &joined);
		dom_string_unref(src);
		if (ns_error != NSERROR_OK) {
			dom_string_unref(mimetype);
			goto html_process_script_no_memory;
		}

		nscript->mimetype = mimetype; /* keep reference to mimetype */

		LOG(("script %i '%s'", c->scripts_count, nsurl_access(joined)));

		child.charset = c->encoding;
		child.quirks = c->base.quirks;

		ns_error = hlcache_handle_retrieve(joined,
						   0,
						   content_get_url(&c->base),
						   NULL,
						   html_convert_script_callback,
						   c,
						   &child,
						   CONTENT_SCRIPT,
						   &nscript->data.external);

		nsurl_unref(joined);

		if (ns_error != NSERROR_OK) {
			goto html_process_script_no_memory;
		}

		c->base.active++; /* ensure base content knows the fetch is active */
		LOG(("%d fetches active", c->base.active));

	}
	html_scripts_exec(c);

	return DOM_HUBBUB_OK;

html_process_script_no_memory:
	msg_data.error = messages_get("NoMemory");
	content_broadcast(&c->base, CONTENT_MSG_ERROR, msg_data);
	return DOM_HUBBUB_NOMEM;
}
