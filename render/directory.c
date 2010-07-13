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

static const char header[] = "<html>\n<head>\n<title>\n";
static const char footer[] = "</table>\n</tt>\n</body>\n</html>\n";


bool directory_create(struct content *c, const struct http_parameter *params) {
	if (!html_create(c, params))
		/* html_create() must have broadcast MSG_ERROR already, so we
		* don't need to. */
		return false;

	binding_parse_chunk(c->data.html.parser_binding,
			(uint8_t *) header, sizeof(header) - 1);

	return true;
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
	char modtime[100];
	bool extendedinfo, evenrow = false;
	char *bgcolour, *specialtag, *specialendtag;

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
			"<body>\n<h1>\nIndex of %s</h1>\n<hr><tt><table>",
			nice_path, nice_path);
	free(nice_path);

	binding_parse_chunk(c->data.html.parser_binding,
			(uint8_t *) buffer, strlen(buffer));

	snprintf(buffer, sizeof(buffer),
		"<tr><td><b>%s</b></td><td><b>%s</b></td>" \
		"<td><b>%s</b></td><td><b>%s</b></td></tr>\n",
		messages_get("FileName"), messages_get("FileSize"),
		messages_get("FileDate"), messages_get("FileType"));

	binding_parse_chunk(c->data.html.parser_binding,
			(uint8_t *) buffer, strlen(buffer));

	res = url_parent(content__get_url(c), &up);

	if (res == URL_FUNC_OK) {
		res = url_compare(content__get_url(c), up, false, &compare);
		if ((res == URL_FUNC_OK) && !compare) {
			if(up[strlen(up) - 1] == '/') up[strlen(up) - 1] = '\0';
			snprintf(buffer, sizeof(buffer),
				"<tr bgcolor=\"#CCCCFF\"><td><b><a href=\"%s\">%s</a></td>" \
				"<td>%s</td><td>&nbsp;</td><td>&nbsp;</td></tr>\n",
				up, messages_get("FileParent"), messages_get("FileDirectory"));

			binding_parse_chunk(c->data.html.parser_binding,
					(uint8_t *) buffer, strlen(buffer));

			evenrow = true;
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

		extendedinfo = false;

		if(filepath = malloc(strlen(path) + strlen(entry->d_name) + 2)) {
			strcpy(filepath, path);
			if(path_add_part(filepath,
				(strlen(path) + strlen(entry->d_name) + 2),
				entry->d_name)) {
				if(stat(filepath, &filestat) == 0) {
					mimetype = fetch_mimetype(filepath);
					extendedinfo = true;
				}
			}
			free(filepath);
		}

		if((extendedinfo == true) && (S_ISDIR(filestat.st_mode))) {
			specialtag = "<b>";
			specialendtag = "</b>";
		}
		else {
			specialtag = "";
			specialendtag = "";
		}

		if(evenrow == false) bgcolour = "CCCCFF";
			else bgcolour = "BBBBFF";
		if(evenrow == false) evenrow = true;
			else evenrow = false;

		snprintf(buffer, sizeof(buffer),
				"<tr bgcolor=\"#%s\"><td>%s<a href=\"%s/%s\">%s</a>%s</td>\n",
				bgcolour, specialtag,
				content__get_url(c), entry->d_name, entry->d_name,
				specialendtag);

		binding_parse_chunk(c->data.html.parser_binding,
				(uint8_t *) buffer, strlen(buffer));

		if(extendedinfo == true) {
			if(strftime((char *)&modtime, sizeof modtime,
				"%c", localtime(&filestat.st_mtime)) == 0)
				strncpy(modtime, "%nbsp;", sizeof modtime);

			if(S_ISDIR(filestat.st_mode)) {
				snprintf(buffer, sizeof(buffer),
						"<td>%s</td><td>%s</td><td>&nbsp;</td>\n",
						messages_get("FileDirectory"), modtime);
			}
			else {
				snprintf(buffer, sizeof(buffer),
						"<td>%d</td><td>%s</td><td>%s</td>\n",
						filestat.st_size, modtime, mimetype);
			}
		}
		else {
			snprintf(buffer, sizeof(buffer),
					"<td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td>\n");
		}

		binding_parse_chunk(c->data.html.parser_binding,
				(uint8_t *) buffer, strlen(buffer));

		strncpy(buffer, "</tr>\n", sizeof(buffer));
		binding_parse_chunk(c->data.html.parser_binding,
				(uint8_t *) buffer, strlen(buffer));

		if(mimetype) free(mimetype);
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

