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


#import "cocoa/SearchWindowController.h"
#import "cocoa/BrowserViewController.h"

#import "desktop/browser.h"
#import "desktop/search.h"

static void cocoa_search_set_back( bool active, void *p );
static void cocoa_search_set_forward( bool active, void *p );

static struct search_callbacks cocoa_search_callbacks = {
	.forward_state = cocoa_search_set_forward,
	.back_state = cocoa_search_set_back,
	.status = NULL,
	.hourglass = NULL,
	.add_recent = NULL
};

@implementation SearchWindowController

@synthesize caseSensitive;
@synthesize selectAll;
@synthesize canGoBack;
@synthesize canGoForward;
@synthesize searchString;
@synthesize browser;

- init;
{
	if ((self = [super initWithWindowNibName: @"SearchWindow"]) == nil) return nil;
	
	[self bind: @"browser" toObject: NSApp withKeyPath: @"frontTab" options: nil];
	canGoBack = canGoForward = YES;
	
	return self;
}

- (void) dealloc;
{
	[self unbind: @"browser"];
	[super dealloc];
}

- (IBAction) searchNext: (id) sender;
{
	[self search: SearchForward];
}

- (IBAction) searchPrevious: (id) sender;
{
	[self search: SearchBackward];
}

- (void) search: (SearchDirection)direction;
{
	search_flags_t flags = (direction == SearchForward) ? SEARCH_FLAG_FORWARDS : 0;
	if (caseSensitive) flags |= SEARCH_FLAG_CASE_SENSITIVE;
	if (selectAll) flags |= SEARCH_FLAG_SHOWALL;

	struct browser_window *bw = [browser browser];
	if (bw != NULL && browser_window_search_verify_new( bw, &cocoa_search_callbacks, self )) {
		browser_window_search_step( bw, flags, [searchString UTF8String] );
	}
}

- (IBAction) searchStringDidChange: (id) sender;
{
	struct browser_window *bw = [browser browser];
	browser_window_search_destroy_context( bw );
	
	[self setCanGoBack: YES];
	[self setCanGoForward: YES];
}

- (void) setCaseSensitive: (BOOL) newValue;
{
	if (caseSensitive != newValue) {
		caseSensitive = newValue;
		[self setCanGoBack: YES];
		[self setCanGoForward: YES];
	}
}

- (void) setSelectAll: (BOOL) newValue;
{
	if (selectAll != newValue) {
		selectAll = newValue;
		[self setCanGoBack: YES];
		[self setCanGoForward: YES];
	}
}

static void cocoa_search_set_back( bool active, void *p )
{
	[(SearchWindowController *)p setCanGoBack: active];
}

static void cocoa_search_set_forward( bool active, void *p )
{
	[(SearchWindowController *)p setCanGoForward: active];
}

@end
