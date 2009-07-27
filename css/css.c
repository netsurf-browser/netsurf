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

#include "content/content.h"
#include "content/fetch.h"
#include "content/fetchcache.h"
#include "css/css.h"
#include "css/internal.h"
#include "desktop/gui.h"
#include "render/html.h"
#include "utils/messages.h"

static void nscss_import(content_msg msg, struct content *c,
		intptr_t p1, intptr_t p2, union content_msg_data data);

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
 * \param parent  Parent content, or NULL if top-level
 * \param params  Content-Type parameters
 * \return true on success, false on failure
 */
bool nscss_create(struct content *c, struct content *parent, 
		const char *params[])
{
	css_origin origin = CSS_ORIGIN_AUTHOR;
	uint64_t media = CSS_MEDIA_ALL;
	lwc_context *dict = NULL;
	bool quirks = true;
	uint32_t i;
	union content_msg_data msg_data;
	css_error error;

	/** \todo extract charset from params */
	/** \todo what happens about the allocator? */
	/** \todo proper error reporting */

	if (parent != NULL) {
		assert(parent->type == CONTENT_HTML || 
				parent->type == CONTENT_CSS);

		if (parent->type == CONTENT_HTML) {
			assert(parent->data.html.dict != NULL);

			if (c == parent->data.html.
					stylesheets[STYLESHEET_BASE].c ||
					c == parent->data.html.
					stylesheets[STYLESHEET_QUIRKS].c ||
					c == parent->data.html.
					stylesheets[STYLESHEET_ADBLOCK].c)
				origin = CSS_ORIGIN_UA;

			quirks = (parent->data.html.quirks != 
					BINDING_QUIRKS_MODE_NONE);

			for (i = 0; i < parent->data.html.stylesheet_count; 
					i++) {
				if (parent->data.html.stylesheets[i].c == c)
					media = parent->data.html.
							stylesheets[i].media;
			}

			dict = parent->data.html.dict;
		} else {
			assert(parent->data.css.sheet != NULL);
			assert(parent->data.css.dict != NULL);

			error = css_stylesheet_get_origin(
					parent->data.css.sheet, &origin);
			if (error != CSS_OK) {
				msg_data.error = "?";
				content_broadcast(c, CONTENT_MSG_ERROR, 
						msg_data);
				return false;
			}

			error = css_stylesheet_quirks_allowed(
					parent->data.css.sheet, &quirks);
			if (error != CSS_OK) {
				msg_data.error = "?";
				content_broadcast(c, CONTENT_MSG_ERROR, 
						msg_data);
				return false;
			}

			for (i = 0; i < parent->data.css.import_count; i++) {
				if (parent->data.css.imports[i].c == c)
					media = parent->data.css.
							imports[i].media;
			}

			dict = parent->data.css.dict;
		}
	}

	if (dict == NULL) {
		lwc_error lerror = lwc_create_context(myrealloc, NULL, &dict);

		if (lerror != lwc_error_ok) {
			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		}
	}

	c->data.css.dict = lwc_context_ref(dict);
	c->data.css.import_count = 0;
	c->data.css.imports = NULL;

	error = css_stylesheet_create(CSS_LEVEL_21, NULL,
			c->url, NULL, origin, media, quirks, false,
			c->data.css.dict, 
			myrealloc, NULL, 
			nscss_resolve_url, NULL,
			&c->data.css.sheet);
	if (error != CSS_OK) {
		lwc_context_unref(c->data.css.dict);
		c->data.css.dict = NULL;
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	return true;
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

	error = css_stylesheet_append_data(c->data.css.sheet, 
			(const uint8_t *) data, size);

	if (error != CSS_OK && error != CSS_NEEDDATA) {
		msg_data.error = "?";
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
	}

	return (error == CSS_OK || error == CSS_NEEDDATA);
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
	css_error error;

	error = css_stylesheet_data_done(c->data.css.sheet);

	/* Process pending imports */
	while (error == CSS_IMPORTS_PENDING) {
		struct nscss_import *imports;
		uint32_t i;
		lwc_string *uri;
		uint64_t media;
		css_stylesheet *sheet;
		
		error = css_stylesheet_next_pending_import(c->data.css.sheet,
				&uri, &media);
		if (error != CSS_OK && error != CSS_INVALID) {
			msg_data.error = "?";
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			c->status = CONTENT_STATUS_ERROR;
			return false;
		}

		/* Give up if there are no more imports */
		if (error == CSS_INVALID) {
			error = CSS_OK;
			break;
		}

		/* Increase space in table */
		imports = realloc(c->data.css.imports, 
				(c->data.css.import_count + 1) * 
				sizeof(struct nscss_import));
		if (imports == NULL) {
			msg_data.error = "?";
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			c->status = CONTENT_STATUS_ERROR;
			return false;
		}
		c->data.css.imports = imports;

		/* Create content */
		i = c->data.css.import_count;
		c->data.css.imports[c->data.css.import_count].media = media;
		c->data.css.imports[c->data.css.import_count++].c =
				fetchcache(lwc_string_data(uri), 
				nscss_import, (intptr_t) c, i, 
				c->width, c->height, true, NULL, NULL, 
				false, false);
		if (c->data.css.imports[i].c == NULL) {
			msg_data.error = "?";
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			c->status = CONTENT_STATUS_ERROR;
			return false;
		}

		/* Fetch content */
		c->active++;
		fetchcache_go(c->data.css.imports[i].c, c->url, 
				nscss_import, (intptr_t) c, i,
				c->width, c->height, NULL, NULL, false, c);

		/* Wait for import to fetch + convert */
		while (c->active > 0) {
			fetch_poll();
			gui_multitask();
		}

		if (c->data.css.imports[i].c != NULL) {
			sheet = c->data.css.imports[i].c->data.css.sheet;
			c->data.css.imports[i].c->data.css.sheet = NULL;
		} else {
			error = css_stylesheet_create(CSS_LEVEL_DEFAULT,
					NULL, "", NULL, CSS_ORIGIN_AUTHOR,
					media, false, false, c->data.css.dict, 
					myrealloc, NULL, 
					nscss_resolve_url, NULL,
					&sheet);
			if (error != CSS_OK) {
				msg_data.error = messages_get("NoMemory");
				content_broadcast(c, CONTENT_MSG_ERROR, 
						msg_data);
				c->status = CONTENT_STATUS_ERROR;
				return false;
			}
		}

		error = css_stylesheet_register_import(
				c->data.css.sheet, sheet);
		if (error != CSS_OK) {
			msg_data.error = "?";
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			c->status = CONTENT_STATUS_ERROR;
			return false;
		}

		error = CSS_IMPORTS_PENDING;
	}

	c->status = CONTENT_STATUS_DONE;

	/* Filthy hack to stop this content being reused 
	 * when whatever is using it has finished with it. */
	c->fresh = false;

	return error == CSS_OK;
}

/**
 * Clean up a CSS content
 *
 * \param c  Content to clean up
 */
void nscss_destroy(struct content *c)
{
	uint32_t i;

	for (i = 0; i < c->data.css.import_count; i++) {
		if (c->data.css.imports[i].c != NULL) {
			content_remove_user(c->data.css.imports[i].c,
					nscss_import, (uintptr_t) c, i);
		}
		c->data.css.imports[i].c = NULL;
	}

	free(c->data.css.imports);

	if (c->data.css.sheet != NULL) {
		css_stylesheet_destroy(c->data.css.sheet);
		c->data.css.sheet = NULL;
	}

	if (c->data.css.dict != NULL) {
		lwc_context_unref(c->data.css.dict);
		c->data.css.dict = NULL;
	}
}

/**
 * Fetchcache handler for imported stylesheets
 *
 * \param msg   Message type
 * \param c     Content being fetched
 * \param p1    Parent content
 * \param p2    Index into parent's imported stylesheet array
 * \param data  Message data
 */
void nscss_import(content_msg msg, struct content *c,
		intptr_t p1, intptr_t p2, union content_msg_data data)
{
	struct content *parent = (struct content *) p1;
	uint32_t i = (uint32_t) p2;

	switch (msg) {
	case CONTENT_MSG_LOADING:
		if (c->type != CONTENT_CSS) {
			content_remove_user(c, nscss_import, p1, p2);
			if (c->user_list->next == NULL) {
				fetch_abort(c->fetch);
				c->fetch = NULL;
				c->status = CONTENT_STATUS_ERROR;
			}

			parent->data.css.imports[i].c = NULL;
			parent->active--;
			content_add_error(parent, "NotCSS", 0);
		}
		break;
	case CONTENT_MSG_READY:
		break;
	case CONTENT_MSG_DONE:
		parent->active--;
		break;
	case CONTENT_MSG_AUTH:
	case CONTENT_MSG_SSL:
	case CONTENT_MSG_LAUNCH:
	case CONTENT_MSG_ERROR:
		if (parent->data.css.imports[i].c == c) {
			parent->data.css.imports[i].c = NULL;
			parent->active--;
		}
		break;
	case CONTENT_MSG_STATUS:
		break;
	case CONTENT_MSG_NEWPTR:
		parent->data.css.imports[i].c = c;
		break;
	default:
		assert(0);
	}
}

