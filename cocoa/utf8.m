/*
 * Copyright 2011 Sven Weidauer <sven.weidauer@gmail.com>
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

#import <Cocoa/Cocoa.h>

#import "utils/utf8.h"

utf8_convert_ret utf8_to_local_encoding(const char *string, size_t len,
										char **result)
{
	NSCParameterAssert( NULL != result );

	char *newString = malloc( len + 1 );
	if (NULL == newString) return UTF8_CONVERT_NOMEM;
	memcpy( newString, string, len );
	newString[len] = 0;
	*result = newString;
	return UTF8_CONVERT_OK;
}

utf8_convert_ret utf8_from_local_encoding(const char *string, size_t len,
										  char **result)
{
	/* same function, local encoding = UTF-8 */
	return utf8_to_local_encoding( string, len, result );
}
