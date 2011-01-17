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

#import "NetSurfAppDelegate.h"

#import "desktop/browser.h"

@interface NetSurfAppDelegate ()

- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent;

@end


@implementation NetSurfAppDelegate

NSString * const kHomepageURL = @"HomepageURL";

@synthesize historyWindow;

+ (void) initialize;
{
	[[NSUserDefaults standardUserDefaults] registerDefaults: [NSDictionary dictionaryWithObjectsAndKeys: 
															  @"http://netsurf-browser.org/welcome/", kHomepageURL,
															  nil]];
}

- (void) newDocument: (id) sender;
{
	NSString *homepageURL = [[NSUserDefaults standardUserDefaults] objectForKey: kHomepageURL];
	browser_window_create( [homepageURL UTF8String], NULL, NULL, true, false );
}

- (void) openDocument: (id) sender;
{
	NSOpenPanel *openPanel = [NSOpenPanel openPanel];
	[openPanel setAllowsMultipleSelection: YES];
	if ([openPanel runModalForTypes: nil] == NSOKButton) {
		for (NSURL *url in [openPanel URLs]) {
			browser_window_create( [[url absoluteString] UTF8String], NULL, NULL, true, false );
		}
	}
}

- (void)handleGetURLEvent:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    NSString *urlAsString = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];
	browser_window_create( [urlAsString UTF8String], NULL, NULL, true, false );
}

- (void) awakeFromNib;
{
	[historyWindow setExcludedFromWindowsMenu: YES];
}

// Application delegate methods

- (BOOL) applicationOpenUntitledFile: (NSApplication *)sender;
{
	[self newDocument: self];
	return YES;
}

-(void)applicationWillFinishLaunching:(NSNotification *)aNotification 
{
    NSAppleEventManager *appleEventManager = [NSAppleEventManager sharedAppleEventManager];
    [appleEventManager setEventHandler:self 
                           andSelector:@selector(handleGetURLEvent:withReplyEvent:)
                         forEventClass:kInternetEventClass andEventID:kAEGetURL];
}


@end
