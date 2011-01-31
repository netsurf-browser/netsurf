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

#import "utils/url.h"


char *url_to_path(const char *url)
{
	NSURL *nsurl = [NSURL URLWithString: [NSString stringWithUTF8String: url]];
	return strdup([[nsurl path] UTF8String]);
}

char *path_to_url(const char *path)
{
	return strdup( [[[NSURL fileURLWithPath: [NSString stringWithUTF8String: path]] 
					 absoluteString] UTF8String] );
}
