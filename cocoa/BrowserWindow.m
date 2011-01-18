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

#import "BrowserWindow.h"
#import "BrowserView.h"

#import "desktop/browser.h"

@implementation BrowserWindow

@synthesize browser;
@synthesize url;
@synthesize view;

- initWithBrowser: (struct browser_window *) bw;
{
	if ((self = [super initWithWindowNibName: @"Browser"]) == nil) return nil;
	
	browser = bw;
	
	NSWindow *win = [self window];
	[win setAcceptsMouseMovedEvents: YES];
	
	if (browser->browser_window_type == BROWSER_WINDOW_NORMAL) {
		[win makeKeyAndOrderFront: self];
	}
	
	return self;
}

- (IBAction) navigate: (id) sender;
{
	browser_window_go( browser, [url UTF8String], NULL, true );
}

- (void) awakeFromNib;
{
	[view setBrowser: browser];
}

- (void)windowWillClose:(NSNotification *)notification;
{
	if (NULL != browser && browser->browser_window_type == BROWSER_WINDOW_NORMAL) {
		browser_window_destroy( browser );
	}
}

@end
