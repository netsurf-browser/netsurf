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

#import "utils/log.h"

const char *fetch_filetype(const char *unix_path)
{
	NSString *uti = [[NSWorkspace sharedWorkspace] typeOfFile: [NSString stringWithUTF8String: unix_path] 
														error: NULL];

	NSString *mimeType = nil;
	if (nil != uti) {
		mimeType = (NSString *)UTTypeCopyPreferredTagWithClass( (CFStringRef)uti, kUTTagClassMIMEType );
	}

	const char *result = "text/html";
	if (nil != mimeType) {
		result = [mimeType UTF8String];
		[mimeType release];
	}

	LOG(( "\tMIME type for '%s' is '%s'", unix_path, result ));
	
	return result;
}


char *fetch_mimetype(const char *ro_path)
{
	return strdup( fetch_filetype( ro_path ) );
}
