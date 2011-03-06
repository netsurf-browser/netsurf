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

#import "cocoa/BrowserViewController.h"

#import "desktop/selection.h"


static NSMutableString *cocoa_clipboard_string;

void gui_start_selection(struct gui_window *g)
{
	gui_empty_clipboard();
}

void gui_clear_selection(struct gui_window *g)
{
}


void gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{
	NSPasteboard *pb = [NSPasteboard generalPasteboard];
	NSString *string = [pb stringForType: NSStringPboardType];
	if (string) {
		const char *text = [string UTF8String];
		browser_window_paste_text( [(BrowserViewController *)g browser], text, strlen(text), true );
	}
}

bool gui_empty_clipboard(void)
{
	if (nil == cocoa_clipboard_string) {
		cocoa_clipboard_string = [[NSMutableString alloc] init];
	} else {
		[cocoa_clipboard_string setString: @""];
	}
	return true;
}

bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
	if (nil == cocoa_clipboard_string) return false;
	
	[cocoa_clipboard_string appendString: [[[NSString alloc] initWithBytes: text 
																	length: length 
																  encoding: NSUTF8StringEncoding] 
										   autorelease]];
	if (space) [cocoa_clipboard_string appendString: @" "];
	
	return true;
}

static bool cocoa_clipboard_copy_handler(const char *text, size_t length, struct box *box,
										 void *handle, const char *whitespace_text,
										 size_t whitespace_length)
{
	bool add_space = box != NULL ? box->space != 0 : false;

	if (whitespace_text && !gui_add_to_clipboard( whitespace_text, 
												 whitespace_length, false )) return false;
	return gui_add_to_clipboard( text, length, add_space );
}

bool gui_commit_clipboard(void)
{
	NSPasteboard *pb = [NSPasteboard generalPasteboard];
	[pb declareTypes: [NSArray arrayWithObject: NSStringPboardType] owner: nil];
	bool result = [pb setString: cocoa_clipboard_string forType: NSStringPboardType];
	if (result) gui_empty_clipboard();
	return result;
}

bool gui_copy_to_clipboard(struct selection *s)
{
	if (selection_defined( s ) && selection_traverse( s, cocoa_clipboard_copy_handler, NULL ))
		gui_commit_clipboard();
	return true;
}

