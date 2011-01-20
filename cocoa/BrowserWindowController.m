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

#import "BrowserWindowController.h"

#import "BrowserViewController.h"
#import "PSMTabBarControl.h"
#import "PSMRolloverButton.h"

#import "desktop/browser.h"

@implementation BrowserWindowController

@synthesize tabBar;
@synthesize tabView;
@synthesize activeBrowser;

- (id) init;
{
	if (nil == (self = [super initWithWindowNibName: @"BrowserWindow"])) return nil;
	
	return self;
}

- (void) dealloc;
{
	[self setTabBar: nil];
	[self setTabView: nil];
	
	[super dealloc];
}

- (void) awakeFromNib;
{
	[tabBar setShowAddTabButton: YES];
	[tabBar setTearOffStyle: PSMTabBarTearOffMiniwindow];
	[tabBar setCanCloseOnlyTab: YES];
	
	NSButton *b = [tabBar addTabButton];
	[b setTarget: self];
	[b setAction: @selector(newTab:)];
	
	[[self window] setAcceptsMouseMovedEvents: YES];
}

- (void) addTab: (BrowserViewController *)browser;
{
	NSTabViewItem *item = [[[NSTabViewItem alloc] initWithIdentifier: browser] autorelease];
	
	[item setView: [browser view]];
	[item bind: @"label" toObject: browser withKeyPath: @"title" options: nil];
	
	[tabView addTabViewItem: item];
	[browser setWindowController: self];
	
	[tabView selectTabViewItem: item];
}

- (void) removeTab: (BrowserViewController *)browser;
{
	NSUInteger itemIndex = [tabView indexOfTabViewItemWithIdentifier: browser];
	if (itemIndex != NSNotFound) {
		NSTabViewItem *item = [tabView tabViewItemAtIndex: itemIndex];
		[tabView removeTabViewItem: item];
		[browser setWindowController: nil];
	}
}

- (void) windowWillClose: (NSNotification *)notification;
{
	for (NSTabViewItem *tab in [tabView tabViewItems]) {
		[tabView removeTabViewItem: tab];
	}
}

extern NSString * const kHomepageURL;
- (IBAction) newTab: (id) sender;
{
	NSString *homepageURL = [[NSUserDefaults standardUserDefaults] objectForKey: kHomepageURL];
	struct browser_window *clone = [[[tabView selectedTabViewItem] identifier] browser];
	browser_window_create( [homepageURL UTF8String], clone, NULL, false, true );
}

- (void) setActiveBrowser: (BrowserViewController *)newBrowser;
{
	activeBrowser = newBrowser;
	[self setNextResponder: activeBrowser];
}

#pragma mark -
#pragma mark Tab bar delegate

- (void) tabView: (NSTabView *)tabView didSelectTabViewItem: (NSTabViewItem *)tabViewItem;
{
	[self setActiveBrowser: [tabViewItem identifier]];
}

- (BOOL)tabView:(NSTabView*)aTabView shouldDragTabViewItem:(NSTabViewItem *)tabViewItem fromTabBar:(PSMTabBarControl *)tabBarControl
{
    return YES;
}

- (BOOL)tabView:(NSTabView*)aTabView shouldDropTabViewItem:(NSTabViewItem *)tabViewItem inTabBar:(PSMTabBarControl *)tabBarControl
{
	[[tabViewItem identifier] setWindowController: self];
	return YES;
}

- (PSMTabBarControl *)tabView:(NSTabView *)aTabView newTabBarForDraggedTabViewItem:(NSTabViewItem *)tabViewItem atPoint:(NSPoint)point;
{
	BrowserWindowController *newWindow = [[[BrowserWindowController alloc] init] autorelease];
	[[tabViewItem identifier] setWindowController: newWindow];
	[[newWindow window] setFrameOrigin: point];
	return newWindow->tabBar;
}

- (void) tabView: (NSTabView *)aTabView didCloseTabViewItem: (NSTabViewItem *)tabViewItem;
{
	[tabViewItem unbind: @"label"];
	browser_window_destroy( [[tabViewItem identifier] browser] );
	[self setActiveBrowser: nil];
}

@end
