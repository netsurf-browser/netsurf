/*
 * Copyright 2014 vincent Sanders <vince@netsurf-browser.org>
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
 * Table operations for files with posix compliant path separator.
 */

#include <stdarg.h>
#include <string.h>

#include "desktop/gui_factory.h"
#include "utils/utils.h"

#include "utils/file.h"

/**
 * Generate a posix path from one or more component elemnts.
 *
 * If a string is allocated it must be freed by the caller.
 *
 * @param[in,out] str pointer to string pointer if this is NULL enough
 *                    storage will be allocated for the complete path.
 * @param[in,out] size The size of the space available if \a str not
 *                     NULL on input and if not NULL set to the total
 *                     output length on output.
 * @param[in] nemb The number of elements.
 * @param[in] ... The elements of the path as string pointers.
 * @return NSERROR_OK and the complete path is written to str
 *         or error code on faliure.
 */
static nserror posix_vmkpath(char **str, size_t *size, size_t nelm, va_list ap)
{
	return vsnstrjoin(str, size, '/', nelm, ap);
}

/**
 * Get the basename of a file using posix path handling.
 *
 * This gets the last element of a path and returns it.
 *
 * @param[in] path The path to extract the name from.
 * @param[in,out] str Pointer to string pointer if this is NULL enough
 *                    storage will be allocated for the path element.
 * @param[in,out] size The size of the space available if \a
 *                     str not NULL on input and set to the total
 *                     output length on output.
 * @return NSERROR_OK and the complete path is written to str
 *         or error code on faliure.
 */
static nserror posix_basename(const char *path, char **str, size_t *size)
{
	const char *leafname;
	char *fname;

	if (path == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	leafname = strrchr(path, '/');
	if (!leafname) {
		leafname = path;
	} else {
		leafname += 1;
	}

	fname = strdup(leafname);
	if (fname == NULL) {
		return NSERROR_NOMEM;
	}

	*str = fname;
	if (size != NULL) {
		*size = strlen(fname);
	}
	return NSERROR_OK;
}

/* exported interface documented in utils/file.h */
nserror netsurf_mkpath(char **str, size_t *size, size_t nelm, ...)
{
	va_list ap;
	nserror ret;

	va_start(ap, nelm);
	ret = guit->file->mkpath(str, size, nelm, ap);
	va_end(ap);

	return ret;
}

/* default to using the posix file handling */
static struct gui_file_table file_table = {
	.mkpath = posix_vmkpath,
	.basename = posix_basename,
};

struct gui_file_table *default_file_table = &file_table;
