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
#include "content/fetch.h"
#include "render/directory.h"
#include "render/html.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"

#define MAX_LENGTH 2048

#define NO_NAME_COLUMN 1
#define NO_TYPE_COLUMN 1 << 1
#define NO_SIZE_COLUMN 1 << 2
#define NO_DATE_COLUMN 1 << 3
#define NO_TIME_COLUMN 1 << 4

static const char footer[] = "</div>\n</body>\n</html>\n";

static const char* dirlist_generate_top(void);
static bool dirlist_generate_hide_columns(int flags, char *buffer,
		int buffer_length);
static bool dirlist_generate_title(char *title, char *buffer,
		int buffer_length);
static bool dirlist_generate_parent_link(char *parent, char *buffer,
		int buffer_length);
static bool dirlist_generate_headings(char *buffer, int buffer_length);
static bool dirlist_generate_row(bool even, bool directory, char *url,
		char *name, char *type, long long size, char *date,
		char *time, char *buffer, int buffer_length);
static const char* dirlist_generate_bottom(void);

static int dirlist_filesize_calculate(unsigned long *bytesize);
static int dirlist_filesize_value(unsigned long bytesize);
static char* dirlist_filesize_unit(unsigned long bytesize);


/**
 * Generates the top part of an HTML directroy listing page
 *
 * \return  Top of directory listing HTML
 *
 * This is part of a series of functions.  To generate a complete page,
 * call the following functions in order:
 *
 *     dirlist_generate_top()
 *     dirlist_generate_hide_columns()  -- optional
 *     dirlist_generate_title()
 *     dirlist_generate_parent_link()   -- optional
 *     dirlist_generate_headings()
 *     dirlist_generate_row()           -- call 'n' times for 'n' rows
 *     dirlist_generate_bottom()
 */

const char* dirlist_generate_top(void)
{
	return	"<html>\n"
		"<head>\n"
		"<style>\n"
		"html, body { margin: 0; padding: 0; }\n"
		"body { background-color: #abf; }\n"
		"h1 { padding: 5mm; margin: 0; "
				"border-bottom: 2px solid #bcf; }\n"
		"p { padding: 2px 5mm; margin: 0; }\n"
		"div { display: table; width: 94%; margin: 5mm auto 0 auto; "
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
				"padding-right: 0; }\n";
}


/**
 * Generates the part of an HTML directory listing page that can suppress
 * particular columns
 *
 * \param  flags	  flags for which cols to suppress. 0 to suppress none
 * \param  buffer	  buffer to fill with generated HTML
 * \param  buffer_length  maximum size of buffer
 * \return  true iff buffer filled without error
 *
 * This is part of a series of functions.  To generate a complete page,
 * call the following functions in order:
 *
 *     dirlist_generate_top()
 *     dirlist_generate_hide_columns()  -- optional
 *     dirlist_generate_title()
 *     dirlist_generate_parent_link()   -- optional
 *     dirlist_generate_headings()
 *     dirlist_generate_row()           -- call 'n' times for 'n' rows
 *     dirlist_generate_bottom()
 */

bool dirlist_generate_hide_columns(int flags, char *buffer, int buffer_length)
{
	int error = snprintf(buffer, buffer_length,
			"%s\n%s\n%s\n%s\n%s\n",
			(flags & NO_NAME_COLUMN) ?
					"span.name { display: none; }\n" : "",
			(flags & NO_TYPE_COLUMN) ?
					"span.type { display: none; }\n" : "",
			(flags & NO_SIZE_COLUMN) ?
					"span.size { display: none; }\n" : "",
			(flags & NO_DATE_COLUMN) ?
					"span.date { display: none; }\n" : "",
			(flags & NO_TIME_COLUMN) ?
					"span.time { display: none; }\n" : "");
	if (error < 0 || error >= buffer_length)
		/* Error or buffer too small */
		return false;
	else
		/* OK */
		return true;
}


/**
 * Generates the part of an HTML directory listing page that contains the title
 *
 * \param  title	  title to use, gets prefixed by "Index of "
 * \param  buffer	  buffer to fill with generated HTML
 * \param  buffer_length  maximum size of buffer
 * \return  true iff buffer filled without error
 *
 * This is part of a series of functions.  To generate a complete page,
 * call the following functions in order:
 *
 *     dirlist_generate_top()
 *     dirlist_generate_hide_columns()  -- optional
 *     dirlist_generate_title()
 *     dirlist_generate_parent_link()   -- optional
 *     dirlist_generate_headings()
 *     dirlist_generate_row()           -- call 'n' times for 'n' rows
 *     dirlist_generate_bottom()
 */

bool dirlist_generate_title(char *title, char *buffer, int buffer_length)
{
	int error = snprintf(buffer, buffer_length,
			"</style>\n"
			"<title>%s</title>\n"
			"</head>\n"
			"<body>\n"
			"<h1>%s</h1>\n",
			title, title);
	if (error < 0 || error >= buffer_length)
		/* Error or buffer too small */
		return false;
	else
		/* OK */
		return true;
}


/**
 * Generates the part of an HTML directory listing page that links to the parent
 * directory
 *
 * \param  parent	  url of parent directory
 * \param  buffer	  buffer to fill with generated HTML
 * \param  buffer_length  maximum size of buffer
 * \return  true iff buffer filled without error
 *
 * This is part of a series of functions.  To generate a complete page,
 * call the following functions in order:
 *
 *     dirlist_generate_top()
 *     dirlist_generate_hide_columns()  -- optional
 *     dirlist_generate_title()
 *     dirlist_generate_parent_link()   -- optional
 *     dirlist_generate_headings()
 *     dirlist_generate_row()           -- call 'n' times for 'n' rows
 *     dirlist_generate_bottom()
 */

bool dirlist_generate_parent_link(char *parent, char *buffer, int buffer_length)
{
	int error = snprintf(buffer, buffer_length,
			"<p><a href=\"%s\">%s</a></p>",
			parent, messages_get("FileParent"));
	if (error < 0 || error >= buffer_length)
		/* Error or buffer too small */
		return false;
	else
		/* OK */
		return true;
}


/**
 * Generates the part of an HTML directory listing page that displays the column
 * headings
 *
 * \param  buffer	  buffer to fill with generated HTML
 * \param  buffer_length  maximum size of buffer
 * \return  true iff buffer filled without error
 *
 * This is part of a series of functions.  To generate a complete page,
 * call the following functions in order:
 *
 *     dirlist_generate_top()
 *     dirlist_generate_hide_columns()  -- optional
 *     dirlist_generate_title()
 *     dirlist_generate_parent_link()   -- optional
 *     dirlist_generate_headings()
 *     dirlist_generate_row()           -- call 'n' times for 'n' rows
 *     dirlist_generate_bottom()
 */

bool dirlist_generate_headings(char *buffer, int buffer_length)
{
	int error = snprintf(buffer, buffer_length,
			"<div>\n<strong>"
			"<span class=\"name\">%s</span> "
			"<span class=\"type\">%s</span> "
			"<span class=\"size\">%s</span>"
			"<span class=\"size\"></span> "
			"<span class=\"date\">%s</span> "
			"<span class=\"time\">%s</span></strong>\n",
			messages_get("FileName"), messages_get("FileType"),
			messages_get("FileSize"), messages_get("FileDate"),
			messages_get("FileTime"));
	if (error < 0 || error >= buffer_length)
		/* Error or buffer too small */
		return false;
	else
		/* OK */
		return true;
}


/**
 * Generates the part of an HTML directory listing page that displays a row
 * in the directory contents table
 *
 * \param  even		  evenness of row number, for alternate row colouring
 * \param  directory	  whether this row is for a directory (or a file)
 * \param  url		  url for row entry
 * \param  name		  name of row entry
 * \param  type		  MIME type of row entry
 * \param  size		  size of row entry.  If negative, size is left blank
 * \param  date		  date row entry was last modified
 * \param  time		  time row entry was last modified
 * \param  buffer	  buffer to fill with generated HTML
 * \param  buffer_length  maximum size of buffer
 * \return  true iff buffer filled without error
 *
 * This is part of a series of functions.  To generate a complete page,
 * call the following functions in order:
 *
 *     dirlist_generate_top()
 *     dirlist_generate_hide_columns()  -- optional
 *     dirlist_generate_title()
 *     dirlist_generate_parent_link()   -- optional
 *     dirlist_generate_headings()
 *     dirlist_generate_row()           -- call 'n' times for 'n' rows
 *     dirlist_generate_bottom()
 */

bool dirlist_generate_row(bool even, bool directory, char *url, char *name,
		char *type, long long size, char *date, char *time,
		char *buffer, int buffer_length)
{
	const char *unit;
	char size_string[100];

	if (size < 0) {
		unit = "";
		strncpy(size_string, "", sizeof size_string);
	} else {
		unit = messages_get(dirlist_filesize_unit((unsigned long)size));
		snprintf(size_string, sizeof size_string, "%d",
				dirlist_filesize_value((unsigned long)size));
	}

	int error = snprintf(buffer, buffer_length,
			"<a href=\"%s\" class=\"%s %s\">"
			"<span class=\"name\">%s</span> "
			"<span class=\"type\">%s</span> "
			"<span class=\"size\">%s</span>"
			"<span class=\"size\">%s</span> "
			"<span class=\"date\">%s</span> "
			"<span class=\"time\">%s</span></a>\n",
			url, even ? "even" : "odd",
			directory ? "dir" : "file",
			name, type, size_string, unit, date, time);
	if (error < 0 || error >= buffer_length)
		/* Error or buffer too small */
		return false;
	else
		/* OK */
		return true;
}


/**
 * Generates the bottom part of an HTML directroy listing page
 *
 * \return  Bottom of directory listing HTML
 *
 * This is part of a series of functions.  To generate a complete page,
 * call the following functions in order:
 *
 *     dirlist_generate_top()
 *     dirlist_generate_hide_columns()  -- optional
 *     dirlist_generate_title()
 *     dirlist_generate_parent_link()   -- optional
 *     dirlist_generate_headings()
 *     dirlist_generate_row()           -- call 'n' times for 'n' rows
 *     dirlist_generate_bottom()
 */

const char* dirlist_generate_bottom(void)
{
	return	"</div>\n"
		"</body>\n"
		"</html>\n";
}


/**
 * Obtain display value and units for filesize after conversion to B/kB/MB/GB,
 * as appropriate.  
 *
 * \param  bytesize  file size in bytes, updated to filesize in output units
 * \return  number of times bytesize has been divided by 1024
 */

int dirlist_filesize_calculate(unsigned long *bytesize)
{
	int i = 0;
	while (*bytesize > 1024 * 4) {
		*bytesize /= 1024;
		i++;
		if (i == 3)
			break;
	}
	return i;
}


/**
 * Obtain display value for filesize after conversion to B/kB/MB/GB,
 * as appropriate
 *
 * \param  bytesize  file size in bytes
 * \return  Value to display for file size, in units given by filesize_unit()
 */

int dirlist_filesize_value(unsigned long bytesize)
{
	dirlist_filesize_calculate(&bytesize);
	return (int)bytesize;
}


/**
 * Obtain display units for filesize after conversion to B/kB/MB/GB,
 * as appropriate
 *
 * \param  bytesize  file size in bytes
 * \return  Units to display for file size, for value given by filesize_value()
 */

char* dirlist_filesize_unit(unsigned long bytesize)
{
	const char* units[] = { "Bytes", "kBytes", "MBytes", "GBytes" };
	return (char*)units[dirlist_filesize_calculate(&bytesize)];
}


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
	int error = 0;
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
	char *index_title;
	int index_title_length;

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
	index_title_length = strlen(nice_path) +
			strlen(messages_get("FileIndex"));
	index_title = malloc(index_title_length);

	if(index_title == NULL) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	snprintf(index_title, index_title_length,
				messages_get("FileIndex"),
				nice_path);
	if (error < 0 || error >= index_title_length) {
		/* Error or buffer too small */
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		free(index_title);
		return false;
	}

	/* Print document title and heading */
	dirlist_generate_title(index_title, buffer, MAX_LENGTH);
	free(nice_path);
	free(index_title);

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

