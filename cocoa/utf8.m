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
#define UNIMPL() NSLog( @"Function '%s' unimplemented", __func__ )

utf8_convert_ret utf8_to_local_encoding(const char *string, size_t len,
										char **result)
{
	UNIMPL();
	return -1;
}

utf8_convert_ret utf8_from_local_encoding(const char *string, size_t len,
										  char **result)
{
	UNIMPL();
	return -1;
}
