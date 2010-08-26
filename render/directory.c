/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2010 Michael Drake <tlsa@netsurf-browser.org>
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
#include "content/dirlist.h"
#include "content/fetch.h"
#include "render/directory.h"
#include "render/html.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"

#define MAX_LENGTH 2048

bool directory_create(struct content *c, const struct http_parameter *params) {
	if (!html_create(c, params))
		/* html_create() must have broadcast MSG_ERROR already, so we
		* don't need to. */
		return false;

	binding_parse_chunk(c->data.html.parser_binding,
			(uint8_t *) dirlist_generate_top(),
			strlen(dirlist_generate_top()));

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
	bool compare, directory;
	char *up;
	struct stat filestat;
	char *filepath, *mimetype = NULL;
	int filepath_size;
	char *urlpath;
	int urlpath_size;
	char moddate[100];
	char modtime[100];
	long long filesize;
	bool extendedinfo, evenrow = false;
	char *title;
	int title_length;

	/* Get directory path from URL */
	path = url_to_path(content__get_url(c));
	if (!path) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	/* Convert path for display */
	nice_path = malloc(strlen(path) * 4 + 1);
	if (!nice_path) {
		msg_data.error = messages_get("MiscErr");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	/* Escape special HTML characters */
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

	/* Set which columns to suppress */
	dirlist_generate_hide_columns(0, buffer, MAX_LENGTH);

	binding_parse_chunk(c->data.html.parser_binding,
			(uint8_t *) buffer, strlen(buffer));

	/* Construct a localised title string */
	title_length = strlen(nice_path) + strlen(messages_get("FileIndex"));
	title = malloc(title_length);

	if(title == NULL) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	/* Set title to localised "Index of <nice_path>" */
	snprintf(title, title_length, messages_get("FileIndex"), nice_path);

	/* Print document title and heading */
	dirlist_generate_title(title, buffer, MAX_LENGTH);
	free(nice_path);
	free(title);

	binding_parse_chunk(c->data.html.parser_binding,
			(uint8_t *) buffer, strlen(buffer));

	/* Print parent directory link */
	res = url_parent(content__get_url(c), &up);
	if (res == URL_FUNC_OK) {
		res = url_compare(content__get_url(c), up, false, &compare);
		if ((res == URL_FUNC_OK) && !compare) {
			dirlist_generate_parent_link(up, buffer, MAX_LENGTH);

			binding_parse_chunk(c->data.html.parser_binding,
					(uint8_t *) buffer, strlen(buffer));
		}
		free(up);
	}

	/* Print directory contents table column headings */
	dirlist_generate_headings(buffer, MAX_LENGTH);

	binding_parse_chunk(c->data.html.parser_binding,
			(uint8_t *) buffer, strlen(buffer));

	if ((parent = opendir(path)) == NULL) {
		msg_data.error = messages_get("EmptyErr");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	/* Print a row for each item in the directory */
	while ((entry = readdir(parent)) != NULL) {
		if (!strcmp(entry->d_name, ".") ||
				!strcmp(entry->d_name, ".."))
			/* Skip . and .. entries */
			continue;

		filepath_size = strlen(path) + strlen(entry->d_name) + 2;
		filepath = malloc(filepath_size);
		if (filepath != NULL) {
			strcpy(filepath, path);
			if (path_add_part(filepath, filepath_size,
					entry->d_name) == false) {
				msg_data.error = messages_get("MiscErr");
				content_broadcast(c, CONTENT_MSG_ERROR,
						msg_data);
				return false;
			}
			if (stat(filepath, &filestat) == 0)
				extendedinfo = true;
			else
				extendedinfo = false;
		} else {
			msg_data.error = messages_get("MiscErr");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		}

		if (S_ISDIR(filestat.st_mode))
			directory = true;
		else
			directory = false;

		urlpath_size = strlen(content__get_url(c)) +
				strlen(entry->d_name) + 2;
		urlpath = malloc(urlpath_size);
		if (urlpath != NULL) {
			strcpy(urlpath, content__get_url(c));
			if(urlpath[strlen(urlpath) - 1] != '/')
				strncat(urlpath, "/", urlpath_size);
			strncat(urlpath, entry->d_name, urlpath_size);

			if (extendedinfo == true) {
				/* Get date in output format */
				if (strftime((char *)&moddate, sizeof moddate,
						"%a %d %b %Y",
						localtime(
						&filestat.st_mtime)) == 0)
					strncpy(moddate, "-", sizeof moddate);
				/* Get time in output format */
				if (strftime((char *)&modtime, sizeof modtime,
						"%H:%M",
						localtime(
						&filestat.st_mtime)) == 0)
					strncpy(modtime, "-", sizeof modtime);

				if (directory) {
					mimetype = strdup((char*)messages_get(
							"FileDirectory"));
					filesize = -1;
				} else {
					mimetype = fetch_mimetype(filepath);
					filesize = (long long)
							filestat.st_size;
				}
			} else {
				strncpy(moddate, "", sizeof moddate);
				strncpy(modtime, "", sizeof modtime);
				filesize = -1;
			}
			/* Print row */
			dirlist_generate_row(evenrow, directory,
					urlpath, entry->d_name,
					mimetype ? mimetype : (char*)"",
					filesize,
					moddate, modtime,
					buffer, MAX_LENGTH);

			binding_parse_chunk(c->data.html.parser_binding,
					(uint8_t *) buffer, strlen(buffer));

			free(urlpath);
		}

		if (evenrow == false)
			evenrow = true;
		else
			evenrow = false;

		if (mimetype != NULL) {
			free(mimetype);
			mimetype = NULL;
		}
		free(filepath);
	}
	closedir(parent);

	binding_parse_chunk(c->data.html.parser_binding,
			(uint8_t *) dirlist_generate_bottom(),
			strlen(dirlist_generate_bottom()));

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

