/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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
 * Content for directory listings (implementation).
 */

#include <dirent.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#ifdef WITH_HUBBUB
#include <hubbub/parser.h>
#endif
#include <libxml/HTMLparser.h>
#include "content/content.h"
#include "render/directory.h"
#include "render/html.h"
#include "utils/messages.h"
#include "utils/url.h"

#define MAX_LENGTH 2048

static const char header[] = "<html>\n<head>\n<title>\n";
static const char footer[] = "</pre>\n</body>\n</html>\n";


bool directory_create(struct content *c, const char *params[]) {
	if (!html_create(c, params))
		/* html_create() must have broadcast MSG_ERROR already, so we
		* don't need to. */
		return false;
#ifndef WITH_HUBBUB
	htmlParseChunk(c->data.html.parser, header, sizeof(header) - 1, 0);
#else
	hubbub_parser_parse_chunk(c->data.html.parser, 
				(uint8_t *) header, sizeof(header) - 1);
#endif
	return true;
}

bool directory_convert(struct content *c, int width, int height) {
	char *path;
	DIR *parent;
	struct dirent *entry;
	union content_msg_data msg_data;
	char buffer[MAX_LENGTH];
	char *nice_path, *cnv, *tmp;
	url_func_result res;
	bool compare;
	char *up;

	path = url_to_path(c->url);
	if (!path) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	nice_path = malloc(strlen(path) * 4 + 1);
	if (!nice_path) {
		msg_data.error = messages_get("MiscErr");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	for (cnv = nice_path, tmp = path; *tmp != '\0'; tmp++) {
		if (*tmp == '<') {
			*cnv++ = '&';
			*cnv++ = 'l';
			*cnv++ = 't';
			*cnv++ = ';';
		} else if (*tmp == '>') {
			*cnv++ = '&';
			*cnv++ = 'g';
			*cnv++ = 't';
			*cnv++ = ';';
		} else {
			*cnv++ = *tmp;
		}
	}
	*cnv++ = '\0';
	snprintf(buffer, sizeof(buffer), "Index of %s</title>\n</head>\n"
			"<body>\n<h1>\nIndex of %s</h1>\n<hr><pre>",
			nice_path, nice_path);
	free(nice_path);
#ifndef WITH_HUBBUB
	htmlParseChunk(c->data.html.parser, buffer, strlen(buffer), 0);
#else
	hubbub_parser_parse_chunk(c->data.html.parser, buffer, strlen(buffer));
#endif

	res = url_parent(c->url, &up);
	if (res == URL_FUNC_OK) {
		res = url_compare(c->url, up, false, &compare);
		if ((res == URL_FUNC_OK) && !compare) {
			snprintf(buffer, sizeof(buffer),
				"<a href=\"..\">[..]</a>\n");
#ifndef WITH_HUBBUB
			htmlParseChunk(c->data.html.parser, buffer,
					strlen(buffer), 0);
#else
			hubbub_parser_parse_chunk(c->data.html.parser, 
					buffer, strlen(buffer));
#endif
		}
		free(up);
	}

	if ((parent = opendir(path)) == NULL) {
		msg_data.error = messages_get("EmptyErr");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	while ((entry = readdir(parent)) != NULL) {
		if (!strcmp(entry->d_name, ".") ||
				!strcmp(entry->d_name, ".."))
			continue;

		snprintf(buffer, sizeof(buffer), "<a href=\"%s/%s\">%s</a>\n",
				c->url, entry->d_name, entry->d_name);
#ifndef WITH_HUBBUB
		htmlParseChunk(c->data.html.parser, buffer, strlen(buffer), 0);
#else
		hubbub_parser_parse_chunk(c->data.html.parser,
				buffer, strlen(buffer));
#endif
	}
	closedir(parent);

#ifndef WITH_HUBBUB
	htmlParseChunk(c->data.html.parser, footer, sizeof(footer) - 1, 0);
#else
	hubbub_parser_parse_chunk(c->data.html.parser, 
			(uint8_t *) footer, sizeof(footer) - 1);
#endif
	c->type = CONTENT_HTML;
	return html_convert(c, width, height);
}

void directory_destroy(struct content *c)
{
	/* This will only get called if the content is destroyed before
	 * content_convert() is called. Simply force the type to HTML and
	 * delegate the cleanup to html_destroy() */

	c->type = CONTENT_HTML;

	html_destroy(c);

	return;
}
