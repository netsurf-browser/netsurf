/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
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

#include <assert.h>

#include <libwapcaplet/libwapcaplet.h>

#include "content/content_protected.h"
#include "content/fetch.h"
#include "content/hlcache.h"
#include "css/css.h"
#include "css/internal.h"
#include "desktop/gui.h"
#include "render/html.h"
#include "utils/http.h"
#include "utils/messages.h"

static nserror nscss_import(hlcache_handle *handle,
		const hlcache_event *event, void *pw);

/**
 * Allocation callback for libcss
 *
 * \param ptr   Pointer to reallocate, or NULL for new allocation
 * \param size  Number of bytes requires
 * \param pw    Allocation context
 * \return Pointer to allocated block, or NULL on failure
 */
static void *myrealloc(void *ptr, size_t size, void *pw)
{
	return realloc(ptr, size);
}

/**
 * Initialise a CSS content
 *
 * \param c       Content to initialise
 * \param params  Content-Type parameters
 * \return true on success, false on failure
 */
bool nscss_create(struct content *c, const http_parameter *params)
{
	const char *charset = NULL;
	union content_msg_data msg_data;
	nserror error;

	/** \todo what happens about the allocator? */
	/** \todo proper error reporting */

	/* Find charset specified on HTTP layer, if any */
	error = http_parameter_list_find_item(params, "charset", &charset);
	if (error != NSERROR_OK) {
		/* No charset specified, use fallback, if any */
		/** \todo libcss will take this as gospel, which is wrong */
		charset = c->fallback_charset;
	}

	if (nscss_create_css_data(&c->data.css, content__get_url(c),
			charset, c->quirks) != NSERROR_OK) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	return true;
}

/**
 * Create a struct content_css_data, creating a stylesheet object
 *
 * \param c        Struct to populate
 * \param url      URL of stylesheet
 * \param charset  Stylesheet charset
 * \param quirks   Stylesheet quirks mode
 * \return NSERROR_OK on success, NSERROR_NOMEM on memory exhaustion
 */
nserror nscss_create_css_data(struct content_css_data *c,
		const char *url, const char *charset, bool quirks)
{
	css_error error;

	c->import_count = 0;
	c->imports = NULL;

	error = css_stylesheet_create(CSS_LEVEL_21, charset,
			url, NULL, quirks, false,
			myrealloc, NULL, 
			nscss_resolve_url, NULL,
			&c->sheet);
	if (error != CSS_OK) {
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}

/**
 * Process CSS source data
 *
 * \param c     Content structure
 * \param data  Data to process
 * \param size  Number of bytes to process
 * \return true on success, false on failure
 */
bool nscss_process_data(struct content *c, char *data, unsigned int size)
{
	union content_msg_data msg_data;
	css_error error;

	error = nscss_process_css_data(&c->data.css, data, size);
	if (error != CSS_OK && error != CSS_NEEDDATA) {
		msg_data.error = "?";
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
	}

	return (error == CSS_OK || error == CSS_NEEDDATA);
}

/**
 * Process CSS data
 *
 * \param c     CSS content object
 * \param data  Data to process
 * \param size  Number of bytes to process
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error nscss_process_css_data(struct content_css_data *c, char *data, 
		unsigned int size)
{
	return css_stylesheet_append_data(c->sheet, 
			(const uint8_t *) data, size);
}

/**
 * Convert a CSS content ready for use
 *
 * \param c  Content to convert
 * \param w  Width of area content will be displayed in
 * \param h  Height of area content will be displayed in
 * \return true on success, false on failure
 */
bool nscss_convert(struct content *c, int w, int h)
{
	union content_msg_data msg_data;
	uint32_t i;
	size_t size;
	css_error error;

	error = nscss_convert_css_data(&c->data.css, w, h);
	if (error != CSS_OK) {
		msg_data.error = "?";
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		c->status = CONTENT_STATUS_ERROR;
		return false;
	}

	/* Retrieve the size of this sheet */
	error = css_stylesheet_size(c->data.css.sheet, &size);
	if (error != CSS_OK) {
		msg_data.error = "?";
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		c->status = CONTENT_STATUS_ERROR;
		return false;
	}
	c->size += size;

	/* Add on the size of the imported sheets */
	for (i = 0; i < c->data.css.import_count; i++) {
		struct content *import = hlcache_handle_get_content(
				c->data.css.imports[i].c);

		if (import != NULL) {
			c->size += import->size;
		}
	}

	c->status = CONTENT_STATUS_DONE;

	return error == CSS_OK;
}

/**
 * Convert CSS data ready for use
 *
 * \param c     CSS data to convert
 * \param w     Width of area content will be displayed in
 * \param h     Height of area content will be displayed in
 * \return CSS error
 */
css_error nscss_convert_css_data(struct content_css_data *c, int w, int h)
{
	const char *referer;
	uint32_t i = 0;
	css_error error;
	nserror nerror;

	error = css_stylesheet_get_url(c->sheet, &referer);
	if (error != CSS_OK) {
		return error;
	}

	error = css_stylesheet_data_done(c->sheet);

	/* Process pending imports */
	while (error == CSS_IMPORTS_PENDING) {
		hlcache_child_context child;
		struct nscss_import *imports;
		lwc_string *uri;
		uint64_t media;
		css_stylesheet *sheet;
		
		error = css_stylesheet_next_pending_import(c->sheet,
				&uri, &media);
		if (error != CSS_OK && error != CSS_INVALID) {
			return error;
		}

		/* Give up if there are no more imports */
		if (error == CSS_INVALID) {
			error = CSS_OK;
			break;
		}

		/* Increase space in table */
		imports = realloc(c->imports, (c->import_count + 1) * 
				sizeof(struct nscss_import));
		if (imports == NULL) {
			return CSS_NOMEM;
		}
		c->imports = imports;

		/** \todo fallback charset */
		child.charset = NULL;
		error = css_stylesheet_quirks_allowed(c->sheet, &child.quirks);
		if (error != CSS_OK) {
			return error;
		}

		/* Create content */
		i = c->import_count;
		c->imports[c->import_count].media = media;
		nerror = hlcache_handle_retrieve(lwc_string_data(uri),
				0, referer, NULL, w, h, nscss_import, c,
				&child, &c->imports[c->import_count++].c);
		if (error != NSERROR_OK) {
			return CSS_NOMEM;
		}

		/* Wait for import to fetch + convert */
		/** \todo This blocking approach needs to die */
		while (c->imports[i].c != NULL && 
				content_get_status(c->imports[i].c) != 
				CONTENT_STATUS_DONE) {
			llcache_poll();
			gui_multitask();
		}

		if (c->imports[i].c != NULL) {
			struct content *s = hlcache_handle_get_content(
					c->imports[i].c);
			sheet = s->data.css.sheet;
		} else {
			error = css_stylesheet_create(CSS_LEVEL_DEFAULT,
					NULL, "", NULL, false, false,
					myrealloc, NULL, 
					nscss_resolve_url, NULL,
					&sheet);
			if (error != CSS_OK) {
				return error;
			}
		}

		error = css_stylesheet_register_import(c->sheet, sheet);
		if (error != CSS_OK) {
			return error;
		}

		error = CSS_IMPORTS_PENDING;
	}

	return error;
}

/**
 * Clean up a CSS content
 *
 * \param c  Content to clean up
 */
void nscss_destroy(struct content *c)
{
	nscss_destroy_css_data(&c->data.css);
}

/**
 * Clean up CSS data
 *
 * \param c  CSS data to clean up
 */
void nscss_destroy_css_data(struct content_css_data *c)
{
	uint32_t i;

	for (i = 0; i < c->import_count; i++) {
		if (c->imports[i].c != NULL) {
			hlcache_handle_release(c->imports[i].c);
		}
		c->imports[i].c = NULL;
	}

	free(c->imports);

	if (c->sheet != NULL) {
		css_stylesheet_destroy(c->sheet);
		c->sheet = NULL;
	}
}

/**
 * Retrieve imported stylesheets
 *
 * \param h  Stylesheet containing imports
 * \param n  Pointer to location to receive number of imports
 * \return Pointer to array of imported stylesheets
 */
struct nscss_import *nscss_get_imports(hlcache_handle *h, uint32_t *n)
{
	struct content *c = hlcache_handle_get_content(h);

	assert(c != NULL);
	assert(c->type == CONTENT_CSS);
	assert(n != NULL);

	*n = c->data.css.import_count;

	return c->data.css.imports;
}

/**
 * Handler for imported stylesheet events
 *
 * \param handle  Handle for stylesheet
 * \param event   Event object
 * \param pw    Callback context
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror nscss_import(hlcache_handle *handle,
		const hlcache_event *event, void *pw)
{
	struct content_css_data *parent = pw;
	uint32_t i = 0;

	switch (event->type) {
	case CONTENT_MSG_LOADING:
		if (content_get_type(handle) != CONTENT_CSS) {
			hlcache_handle_release(handle);

			for (i = 0; i < parent->import_count; i++) {
				if (parent->imports[i].c == handle) {
					parent->imports[i].c = NULL;
					break;
				}
			}
		}
		break;
	case CONTENT_MSG_READY:
		break;
	case CONTENT_MSG_DONE:
		break;
	case CONTENT_MSG_ERROR:
		hlcache_handle_release(handle);
		for (i = 0; i < parent->import_count; i++) {
			if (parent->imports[i].c == handle) {
				parent->imports[i].c = NULL;
				break;
			}
		}
		break;
	case CONTENT_MSG_STATUS:
		break;
	default:
		assert(0);
	}

	return NSERROR_OK;
}

