/*
 * Copyright 2011 John-Mark Bell <jmb@netsurf-browser.org>
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
 * Content factory (implementation)
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "content/content.h"
#include "content/content_factory.h"
#include "content/content_protected.h"
#include "content/llcache.h"

/**
 * Entry in list of content handlers
 */
typedef struct content_handler_entry {
	/** Next entry */
	struct content_handler_entry *next;

	/** MIME type handled by handler */
	lwc_string *mime_type;
	/** Content handler object */
	const content_handler *handler;
} content_handler_entry;

static content_handler_entry *content_handlers;

/**
 * Clean up after the content factory
 */
void content_factory_fini(void)
{
	content_handler_entry *victim;

	while (content_handlers != NULL) {
		victim = content_handlers;

		content_handlers = content_handlers->next;

		lwc_string_unref(victim->mime_type);

		free(victim);
	}
}

/**
 * Register a handler with the content factory
 *
 * \param mime_type  MIME type to handle
 * \param handler    Content handler for MIME type
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * \note Latest registration for a MIME type wins
 */
nserror content_factory_register_handler(lwc_string *mime_type,
		const content_handler *handler)
{
	content_handler_entry *entry;
	bool match;

	for (entry = content_handlers; entry != NULL; entry = entry->next) {
		if (lwc_string_caseless_isequal(mime_type, entry->mime_type,
				&match) == lwc_error_ok && match)
			break;
	}

	if (entry == NULL) {
		entry = malloc(sizeof(content_handler_entry));
		if (entry == NULL)
			return NSERROR_NOMEM;

		entry->next = content_handlers;
		content_handlers = entry;

		entry->mime_type = lwc_string_ref(mime_type);
	}

	entry->handler = handler;

	return NSERROR_OK;
}

/**
 * Find a handler for a MIME type.
 *
 * \param mime_type  MIME type to search for
 * \return Associated handler, or NULL if none
 */
static const content_handler *content_lookup(lwc_string *mime_type)
{
	content_handler_entry *entry;
	bool match;

	for (entry = content_handlers; entry != NULL; entry = entry->next) {
		if (lwc_string_caseless_isequal(mime_type, entry->mime_type,
				&match) == lwc_error_ok && match)
			break;
	}

	if (entry != NULL)
		return entry->handler;

	return NULL;
}

/**
 * Compute the generic content type for a MIME type
 *
 * \param mime_type  MIME type to consider
 * \return Generic content type
 */
content_type content_factory_type_from_mime_type(lwc_string *mime_type)
{
	const content_handler *handler;
	content_type type = CONTENT_NONE;

	handler = content_lookup(mime_type);
	if (handler != NULL) {
		type = handler->type(mime_type);
	}

	return type;
}

/**
 * Create a content object
 *
 * \param llcache           Underlying source data handle
 * \param fallback_charset  Character set to fall back to if none specified
 * \param quirks            Quirkiness of containing document
 * \return Pointer to content object, or NULL on failure
 */
struct content *content_factory_create_content(llcache_handle *llcache,
		const char *fallback_charset, bool quirks)
{
	struct content *c;
	const char *content_type_header;
	const content_handler *handler;
	http_content_type *ct;
	nserror error;

	content_type_header = 
			llcache_handle_get_header(llcache, "Content-Type");
	if (content_type_header == NULL)
		content_type_header = "text/plain";

	error = http_parse_content_type(content_type_header, &ct);
	if (error != NSERROR_OK)
		return NULL;

	handler = content_lookup(ct->media_type);
	if (handler == NULL) {
		http_content_type_destroy(ct);
		return NULL;
	}

	assert(handler->create != NULL);

	error = handler->create(handler, ct->media_type, ct->parameters, 
			llcache, fallback_charset, quirks, &c);
	if (error != NSERROR_OK) {
		http_content_type_destroy(ct);
		return NULL;
	}

	http_content_type_destroy(ct);

	return c;
}

