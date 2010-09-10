/*
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
 * Generate HTML content for displaying directory listings (implementation).
 */

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "content/dirlist.h"
#include "utils/messages.h"

static const char footer[] = "</div>\n</body>\n</html>\n";

static int dirlist_filesize_calculate(unsigned long *bytesize);
static int dirlist_filesize_value(unsigned long bytesize);
static char* dirlist_filesize_unit(unsigned long bytesize);


/**
 * Generates the top part of an HTML directory listing page
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

bool dirlist_generate_top(char *buffer, int buffer_length)
{
	int error = snprintf(buffer, buffer_length,
		"<html>\n"
		"<head>\n"
		"<style>\n"
		"html, body { margin: 0; padding: 0; }\n"
		"body { background-color: #abf; padding-bottom: 2em; }\n"
		"h1 { padding: 5mm; margin: 0; "
				"border-bottom: 2px solid #bcf; }\n"
		"p { padding: 2px 5mm; margin: 0; }\n"
		"div { display: table; width: 94%%; margin: 5mm auto 2em auto; "
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
				"padding-right: 0; }\n");
	if (error < 0 || error >= buffer_length)
		/* Error or buffer too small */
		return false;
	else
		/* OK */
		return true;

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
			(flags & DIRLIST_NO_NAME_COLUMN) ?
					"span.name { display: none; }\n" : "",
			(flags & DIRLIST_NO_TYPE_COLUMN) ?
					"span.type { display: none; }\n" : "",
			(flags & DIRLIST_NO_SIZE_COLUMN) ?
					"span.size { display: none; }\n" : "",
			(flags & DIRLIST_NO_DATE_COLUMN) ?
					"span.date { display: none; }\n" : "",
			(flags & DIRLIST_NO_TIME_COLUMN) ?
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
 * \param  title	  title to use
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

bool dirlist_generate_title(const char *title, char *buffer, int buffer_length)
{
	int error;

	if (title == NULL)
		title = "";

	error = snprintf(buffer, buffer_length,
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
 * \param  mimetype	  MIME type of row entry
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
		const char *mimetype, long long size, char *date, char *time,
		char *buffer, int buffer_length)
{
	const char *unit;
	char size_string[100];
	int error;

	if (size < 0) {
		unit = "";
		strncpy(size_string, "", sizeof size_string);
	} else {
		unit = messages_get(dirlist_filesize_unit((unsigned long)size));
		snprintf(size_string, sizeof size_string, "%d",
				dirlist_filesize_value((unsigned long)size));
	}

	error = snprintf(buffer, buffer_length,
			"<a href=\"%s\" class=\"%s %s\">"
			"<span class=\"name\">%s</span> "
			"<span class=\"type\">%s</span> "
			"<span class=\"size\">%s</span>"
			"<span class=\"size\">%s</span> "
			"<span class=\"date\">%s</span> "
			"<span class=\"time\">%s</span></a>\n",
			url, even ? "even" : "odd",
			directory ? "dir" : "file",
			name, mimetype, size_string, unit, date, time);
	if (error < 0 || error >= buffer_length)
		/* Error or buffer too small */
		return false;
	else
		/* OK */
		return true;
}


/**
 * Generates the bottom part of an HTML directory listing page
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

bool dirlist_generate_bottom(char *buffer, int buffer_length)
{
	int error = snprintf(buffer, buffer_length,
			"</div>\n"
			"</body>\n"
			"</html>\n");
	if (error < 0 || error >= buffer_length)
		/* Error or buffer too small */
		return false;
	else
		/* OK */
		return true;
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
