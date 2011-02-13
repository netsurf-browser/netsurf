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
#include "content/content_protected.h"
#include "content/hlcache.h"
#include "desktop/shape.h"
#include "render/favicon.h"
#include "render/html.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/url.h"
#include "utils/utils.h"

static char *favicon_get_icon_ref(struct content *c, xmlNode *html);
static nserror favicon_callback(hlcache_handle *icon,
		const hlcache_event *event, void *pw);

/**
 * retrieve 1 url reference to 1 favicon
 * \param html xml node of html element
 * \return pointer to url; NULL for no icon; caller owns returned pointer
 */
char *favicon_get_icon_ref(struct content *c, xmlNode *html)
{
	xmlNode *node = html;
	char *rel, *href, *url, *url2 = NULL;
	url_func_result res;

	while (node) {
		if (node->children != NULL) {  /* children */
			node = node->children;
		} else if (node->next != NULL) {  /* siblings */
			node = node->next;
		} else {  /* ancestor siblings */
			while (node != NULL && node->next == NULL)
				node = node->parent;

			if (node == NULL)
				break;

			node = node->next;
		}

		assert(node != NULL);

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

			if (strcasecmp(rel, "apple-touch-icon") == 0) {
				xmlFree(rel);
				continue;
			}

			xmlFree(rel);

			if ((href = (char *) xmlGetProp(node,
					(const xmlChar *) "href")) == NULL)
				continue;

			res = url_join(href, c->data.html.base_url, &url);

			xmlFree(href);

			if (res != URL_FUNC_OK)
				continue;

			if (url2 != NULL) {
				free(url2);
				url2 = NULL;
			}

			res = url_normalize(url, &url2);

			free(url);

			if (res != URL_FUNC_OK) {
				url2 = NULL;

				if (res == URL_FUNC_NOMEM)
					return NULL;

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

	return url2;
}

/**
 * retrieve 1 favicon
 * \param c content structure
 * \param html xml node of html element
 * \return true for success, false for error
 */

bool favicon_get_icon(struct content *c, xmlNode *html)
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
	char *url;
	nserror error;

	url = favicon_get_icon_ref(c, html);
	if (url == NULL)
		return false;
	
	error = hlcache_handle_retrieve(url, LLCACHE_RETRIEVE_NO_ERROR_PAGES, 
			content__get_url(c), NULL, favicon_callback, c, NULL, 
			permitted_types, &c->data.html.favicon);	

	if (error == NSERROR_OK) {
		c->active += 1;
	}
	
	free(url);

	return error == NSERROR_OK;
}

/**
 * Callback for fetchcache() for linked favicon
 */

nserror favicon_callback(hlcache_handle *icon,
		const hlcache_event *event, void *pw)
{
	struct content *c = pw;
	bool consider_done = false, consider_redraw = false;

	switch (event->type) {
	case CONTENT_MSG_LOADING:
		/* check that the favicon is really a correct image type */
		if (content_get_type(icon) == CONTENT_UNKNOWN) {
			union content_msg_data msg_data;

			LOG(("%s is not a favicon", content_get_url(icon)));

			hlcache_handle_abort(icon);
			hlcache_handle_release(icon);
			c->data.html.favicon = NULL;
			c->active -= 1;
			consider_done = true;

			content_add_error(c, "NotFavIco", 0);

			msg_data.error = messages_get("NotFavIco");
			content_broadcast(c, CONTENT_MSG_STATUS, msg_data);
		}
		break;

	case CONTENT_MSG_READY:
		break;
	case CONTENT_MSG_DONE:
		c->active -= 1;
		consider_done = true;
		break;

	case CONTENT_MSG_ERROR:
		LOG(("favicon %s failed: %s", 
				content_get_url(icon), event->data.error));
		hlcache_handle_release(c->data.html.favicon);
		c->data.html.favicon = NULL;

		content_add_error(c, "?", 0);
		
		c->active -= 1;
		consider_done = true;
		break;

	case CONTENT_MSG_STATUS:
		content_broadcast(c, CONTENT_MSG_STATUS, event->data);
		break;

	case CONTENT_MSG_REDRAW:
		/* Fall through */
	case CONTENT_MSG_REFRESH:
		/* Fall through */
	case CONTENT_MSG_REFORMAT:
		consider_redraw = true;
		break;

	default:
		assert(0);
	}

#ifdef WITH_GIF
	if (consider_redraw && (c->data.html.favicon != NULL) &&
			(content_get_type(c->data.html.favicon) ==
			CONTENT_GIF)) {
		union content_msg_data msg_data;
		struct rect clip;
		/* This is needed in order to cause animated GIFs to update
		 * their bitmap */
		clip.x0 = clip.y0 = 0;
		clip.x1 = clip.y1 = 0;
		content_redraw(c->data.html.favicon, 0, 0, -1, -1, &clip,
				1.0, 0);
		content_broadcast(c, CONTENT_MSG_FAVICON_REFRESH, msg_data);
	}
#endif
	
	if (consider_done && (c->active == 0)) {
		/* all objects have arrived */
		content__reformat(c, c->available_width, c->height);
		html_set_status(c, "");
		content_set_done(c);
	}
	
	
	return NSERROR_OK;
}
