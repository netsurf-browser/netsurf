/*
 * Copyright 2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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

#include <string.h>
#include "content/fetch.h"
#include "content/fetchcache.h"
#include "render/favicon.h"
#include "render/html.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/url.h"
#include "utils/utils.h"

static char *favicon_get_icon_ref(struct content *c, xmlNode *html);
static void favicon_callback(content_msg msg, struct content *icon,
		intptr_t p1, intptr_t p2, union content_msg_data data);

/**
 * retrieve 1 url reference to 1 favicon
 * \param html xml node of html element
 * \return pointer to url; NULL for no icon; caller owns returned pointer
 */
char *favicon_get_icon_ref(struct content *c, xmlNode *html)
{
	xmlNode *node;
	char *rel, *href, *url, *url2;
	url_func_result res;
	union content_msg_data msg_data;

	url2 = NULL;
	node = html;
	while (node) {
		if (node->children) {  /* children */
			node = node->children;
		} else if (node->next) {  /* siblings */
			node = node->next;
		} else {  /* ancestor siblings */
			while (node && !node->next)
				node = node->parent;
			if (!node)
				break;
			node = node->next;
		}
		assert(node);

		if (node->type != XML_ELEMENT_NODE)
			continue;
		if (strcmp((const char *) node->name, "link") == 0) {
			/* rel=<space separated list, including 'icon'> */
			if ((rel = (char *) xmlGetProp(node,
					(const xmlChar *) "rel")) == NULL)
				continue;
			if (strcasestr(rel, "icon") == 0) {
				xmlFree(rel);
				continue;
			}
			LOG(("icon node found"));
			if (strcasecmp(rel, "apple-touch-icon") == 0) {
				xmlFree(rel);
				continue;
			}
			xmlFree(rel);
			if (( href = (char *) xmlGetProp(node,
					(const xmlChar *) "href")) == NULL)
				continue;
			res = url_join(href, c->data.html.base_url, 
					&url);
			xmlFree(href);
			if (res != URL_FUNC_OK)
				continue;
			LOG(("most recent favicon '%s'", url));
			if (url2 != NULL) {
				free(url2);
				url2 = NULL;
			}
			res = url_normalize(url, &url2);
			free(url);
			if (res != URL_FUNC_OK) {
				url2 = NULL;
				if (res == URL_FUNC_NOMEM)
					goto no_memory;
				continue;
			}

		}
	}
	if (url2 == NULL) {
		char *scheme;

		/* There was no icon link defined in the HTML source data.
		 * If the HTML document's base URL uses either the HTTP or 
		 * HTTPS schemes, then try using "<scheme>://host/favicon.ico"
		 */
		if (url_scheme(c->data.html.base_url, &scheme) != URL_FUNC_OK)
			return NULL;

		if (strcasecmp(scheme, "http") != 0 && 
				strcasecmp(scheme, "https") != 0) {
			free(scheme);
			return NULL;
		}

		free(scheme);

		if (url_join("/favicon.ico", c->data.html.base_url, &url2)
				!= URL_FUNC_OK)
			return NULL;
	}
	LOG(("favicon %s", url2));
	return url2;
no_memory:
	msg_data.error = messages_get("NoMemory");
	/* content_broadcast(c, CONTENT_MSG_ERROR, msg_data); */
	return false;
}

/**
 * retrieve 1 favicon
 * \param c content structure
 * \param html xml node of html element
 * \return true for success, false for error
 */

bool favicon_get_icon(struct content *c, xmlNode *html)
{
	char *url = favicon_get_icon_ref(c, html);
	struct content *favcontent = NULL;
	if (url == NULL)
		return false;
	
	favcontent = fetchcache(url, favicon_callback, (intptr_t) c, 0,
			c->width, c->height, true, 0, 0, false, false);
	free(url);
	if (favcontent == NULL)
		return false;

	c->data.html.favicon = favcontent;
	
	fetchcache_go(favcontent, c->url, favicon_callback, (intptr_t) c, 0,
		       c->width, c->height, 0, 0, false, c);

	return true;
}

/**
 * Callback for fetchcache() for linked favicon
 */

void favicon_callback(content_msg msg, struct content *icon,
		intptr_t p1, intptr_t p2, union content_msg_data data)
{
	static const content_type permitted_types[] = {
#ifdef WITH_BMP
		CONTENT_ICO,
#endif
#if defined(WITH_MNG) || defined(WITH_PNG)
		CONTENT_PNG,
#endif
#ifdef WITH_GIF
		CONTENT_GIF,
#endif
		CONTENT_UNKNOWN
	};
	struct content *c = (struct content *) p1;
	unsigned int i = p2;
	const content_type *type;


	switch (msg) {
	case CONTENT_MSG_LOADING:
		/* check that the favicon is really a correct image type */
		for (type = permitted_types; *type != CONTENT_UNKNOWN; type++)
			if (icon->type == *type)
				break;

		if (*type == CONTENT_UNKNOWN) {
			c->data.html.favicon = 0;
			LOG(("%s is not a favicon", icon->url));
			content_add_error(c, "NotFavIco", 0);
			html_set_status(c, messages_get("NotFavIco"));
			content_broadcast(c, CONTENT_MSG_STATUS, data);
			content_remove_user(icon,
					favicon_callback,
					(intptr_t) c, i);
			if (!icon->user_list->next) {
				/* we were the only user and we don't want this
				 * content, so stop it fetching and mark it as
				 * having an error so it gets removed from the
				 * cache next time content_clean() gets called
				 */
				fetch_abort(icon->fetch);
				icon->fetch = 0;
				icon->status = CONTENT_STATUS_ERROR; 
			}
		}
		break;

	case CONTENT_MSG_READY:
		break;

	case CONTENT_MSG_DONE:
		LOG(("got favicon '%s'", icon->url));
		break;

	case CONTENT_MSG_LAUNCH:
		/* Fall through */
	case CONTENT_MSG_ERROR:
		LOG(("favicon %s failed: %s", icon->url, data.error));
		/* The favicon we were fetching may have been
		* redirected, in that case, the object pointers
		* will differ, so ensure that the object that's
		* in error is still in use by us before invalidating
		* the pointer */
		if (c->data.html.favicon == icon) {
			c->data.html.favicon = 0;
			content_add_error(c, "?", 0);
		}
		break;

	case CONTENT_MSG_STATUS:
		html_set_status(c, icon->status_message);
		content_broadcast(c, CONTENT_MSG_STATUS, data);
		break;

	case CONTENT_MSG_NEWPTR:
		c->data.html.favicon = icon;
		break;

	case CONTENT_MSG_AUTH:
		c->data.html.favicon = 0;
		content_add_error(c, "?", 0);
		break;

	case CONTENT_MSG_SSL:
		c->data.html.favicon = 0;
		content_add_error(c, "?", 0);
		break;
	case CONTENT_MSG_REDRAW:
		/* currently no support for favicon animations */
	case CONTENT_MSG_REFRESH:
		break;
	case CONTENT_MSG_REFORMAT:
		/* would be unusual :) */
		break;
	default:
		assert(0);
	}
}
