/*
 * Copyright 2014 Vincent Sanders <vince@netsurf-browser.org>
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
 * Default operations table for files.
 */

#ifndef NETSURF_UTILS_FILE_H
#define NETSURF_UTILS_FILE_H

#include <stdarg.h>

/**
 * function table for file and filename operations.
 *
 * function table implementing GUI interface to file and filename
 * functionality appropriate for the OS.
 */
struct gui_file_table {
	/* Mandantory entries */

	/**
	 * Generate a path from one or more component elemnts.
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
	nserror (*mkpath)(char **str, size_t *size, size_t nemb, va_list ap);

	/**
	 * Get the basename of a file.
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
	nserror (*basename)(const char *path, char **str, size_t *size);
};

/** Default (posix) file operation table. */
struct gui_file_table *default_file_table;

/**
 * Generate a path from one or more component elemnts.
 *
 * If a string is allocated it must be freed by the caller.
 *
 * @warning If this is called before the gui operation tables are
 * initialised the behaviour defaults to posix paths. Ensure this is
 * the required behaviour.
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
nserror netsurf_mkpath(char **str, size_t *size, size_t nelm, ...);

#endif
