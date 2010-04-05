/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
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
 * Error codes
 */

#ifndef NETSURF_UTILS_ERRORS_H_
#define NETSURF_UTILS_ERRORS_H_

/**
 * Enumeration of error codes
 */
typedef enum {
	NSERROR_OK,			/**< No error */

	NSERROR_NOMEM,			/**< Memory exhaustion */

	NSERROR_NO_FETCH_HANDLER,	/**< No fetch handler for URL scheme */

	NSERROR_NOT_FOUND,		/**< Requested item not found */

	NSERROR_SAVE_FAILED		/**< Failed to save data */
} nserror;

#endif

