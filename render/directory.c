/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <sys/types.h>
#include <dirent.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include "content/content_protected.h"
#include "content/fetch.h"
#include "render/directory.h"
#include "render/html.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"

#define MAX_LENGTH 2048

static const char header[] =
		"<html>\n"
		"<head>\n"
		"<style>\n"
		"html, body { margin: 0; padding: 0; }\n"
		"body { background-color: #abf; }\n"
		"h1 { padding: 5mm; margin: 0; "
				"border-bottom: 2px solid #bcf; }\n"
		"p { padding: 2px 5mm; margin: 0 0 1em 0; }\n"
		"div { display: table; width: 94%; margin: 0 auto; "
				"padding: 0; }\n"
		"a, strong { display: table-row; margin: 0; padding: 0; }\n"
		"a.odd { background-color: #bcf; }\n"
		"a.even { background-color: #b2c3ff; }\n"
		"span { display: table-cell; }\n"
		"em > span { padding-bottom: 1px; }\n"
		"a + a>span { border-top: 1px solid #9af; }\n"
		"span.name { padding-left: 22px; min-height: 19px;}\n"
		"a.dir > span.name { font-weight: bold; }\n"
		"a.dir > span.type { font-weight: bold; }\n"
		"span.size { text-align: right; padding-right: 0.3em; }\n"
		"span.size + span.size { text-align: left; "
				"padding-right: 0; }\n"
		"</style>\n"
		"<title>\n";
static const char footer[] = "</div>\n</body>\n</html>\n";

static char sizeunits[][7] = {"Bytes", "kBytes", "MBytes", "GBytes"};

static int filesize_value(unsigned long bytesize);
static char* filesize_unit(unsigned long bytesize);


bool directory_create(struct content *c, const struct http_parameter *params) {
	if (!html_create(c, params))
		/* html_create() must have broadcast MSG_ERROR already, so we
		* don't need to. */
		return false;

	binding_parse_chunk(c->data.html.parser_binding,
			(uint8_t *) header, sizeof(header) - 1);

	return true;
}

int filesize_value(unsigned long bytesize) {
	int i = 0;
	while (bytesize > 1024 * 4) {
		bytesize /= 1024;
		i++;
		if (i == 3)
			break;
	}

	return bytesize;
}

char* filesize_unit(unsigned long bytesize) {
	int i = 0;
	while (bytesize > 1024 * 4) {
		bytesize /= 1024;
		i++;
		if (i == 3)
			break;
	}

	return sizeunits[i];
}

bool directory_convert(struct content *c) {
	char *path;
	DIR *parent;
	struct dirent *entry;
	union content_msg_data msg_data;
	char buffer[MAX_LENGTH];
	char *nice_path, *cnv, *tmp;
	url_func_result res;
	bool compare;
	char *up;
	struct stat filestat;
	char *filepath, *mimetype;
	char moddate[100];
	char modtime[100];
	bool extendedinfo, evenrow = false;

	path = url_to_path(content__get_url(c));
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
	*cnv = '\0';
	snprintf(buffer, sizeof(buffer), "Index of %s</title>\n</head>\n"
			"<body>\n<h1>Index of %s</h1>\n",
			nice_path, nice_path);
	free(nice_path);

	binding_parse_chunk(c->data.html.parser_binding,
			(uint8_t *) buffer, strlen(buffer));

	res = url_parent(content__get_url(c), &up);

	if (res == URL_FUNC_OK) {
		res = url_compare(content__get_url(c), up, false, &compare);
		if ((res == URL_FUNC_OK) && !compare) {
			if (up[strlen(up) - 1] == '/')
				up[strlen(up) - 1] = '\0';
			snprintf(buffer, sizeof(buffer),
				"<p><a href=\"%s\">%s</a></p>",
				up, messages_get("FileParent"));

			binding_parse_chunk(c->data.html.parser_binding,
					(uint8_t *) buffer, strlen(buffer));
		}
		free(up);
	}

	snprintf(buffer, sizeof(buffer),
		"<div>\n<strong>"
		"<span class=\"name\">%s</span> "
		"<span class=\"type\">%s</span> "
		"<span class=\"size\">%s</span><span class=\"size\"></span> "
		"<span class=\"date\">%s</span> "
		"<span class=\"time\">%s</span></strong>\n",
		messages_get("FileName"), messages_get("FileType"),
		messages_get("FileSize"), messages_get("FileDate"),
		messages_get("FileTime"));

	binding_parse_chunk(c->data.html.parser_binding,
			(uint8_t *) buffer, strlen(buffer));

	if ((parent = opendir(path)) == NULL) {
		msg_data.error = messages_get("EmptyErr");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	while ((entry = readdir(parent)) != NULL) {
		if (!strcmp(entry->d_name, ".") ||
				!strcmp(entry->d_name, ".."))
			continue;

		extendedinfo = false;

		filepath = malloc(strlen(path) + strlen(entry->d_name) + 2);
		if (filepath != NULL) {
			strcpy(filepath, path);
			if (path_add_part(filepath,
					(strlen(path) +
					strlen(entry->d_name) + 2),
					entry->d_name)) {
				if (stat(filepath, &filestat) == 0) {
					mimetype = fetch_mimetype(filepath);
					extendedinfo = true;
				}
			}
			free(filepath);
		}

		snprintf(buffer, sizeof(buffer),
				"<a href=\"%s/%s\" class=\"%s %s\">"
				"<span class=\"name\">%s</span> ",
				content__get_url(c), entry->d_name,
				evenrow ? "even" : "odd",
				S_ISDIR(filestat.st_mode) ? "dir" : "file",
				entry->d_name);

		binding_parse_chunk(c->data.html.parser_binding,
				(uint8_t *) buffer, strlen(buffer));

		if (extendedinfo == true) {
			if (strftime((char *)&moddate, sizeof moddate,
					"%a %d %b %Y",
					localtime(&filestat.st_mtime)) == 0)
				strncpy(moddate, "-", sizeof moddate);
			if (strftime((char *)&modtime, sizeof modtime,
					"%H:%M",
					localtime(&filestat.st_mtime)) == 0)
				strncpy(modtime, "-", sizeof modtime);

			if (S_ISDIR(filestat.st_mode)) {
				snprintf(buffer, sizeof(buffer),
					"<span class=\"type\">%s</span> "
					"<span class=\"size\"></span>"
					"<span class=\"size\"></span> "
					"<span class=\"date\">%s</span> "
					"<span class=\"time\">%s</span></a>\n",
					messages_get("FileDirectory"),
					moddate, modtime);
			} else {
				snprintf(buffer, sizeof(buffer),
					"<span class=\"type\">%s</span> "
					"<span class=\"size\">%d</span>"
					"<span class=\"size\">%s</span> "
					"<span class=\"date\">%s</span> "
					"<span class=\"time\">%s</span></a>\n",
					mimetype,
					filesize_value(
					(unsigned long)filestat.st_size),
					messages_get(filesize_unit(
					(unsigned long)filestat.st_size)),
					moddate, modtime);
			}
		} else {
			snprintf(buffer, sizeof(buffer),
					"<span class=\"type\"></span> "
					"<span class=\"size\"></span>"
					"<span class=\"size\"></span> "
					"<span class=\"date\"></span> "
					"<span class=\"time\"></span></a>\n");
		}

		binding_parse_chunk(c->data.html.parser_binding,
				(uint8_t *) buffer, strlen(buffer));

		if (evenrow == false)
			evenrow = true;
		else
			evenrow = false;

		if (mimetype)
			free(mimetype);
	}
	closedir(parent);

	binding_parse_chunk(c->data.html.parser_binding,
			(uint8_t *) footer, sizeof(footer) - 1);

	c->type = CONTENT_HTML;
	return html_convert(c);
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

bool directory_clone(const struct content *old, struct content *new_content)
{
	/* This will only get called if the content is cloned before
	 * content_convert() is called. Simply replay creation. */
	if (directory_create(new_content, NULL) == false)
		return false;

	return true;
}

