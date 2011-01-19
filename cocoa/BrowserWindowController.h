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

@class PSMTabBarControl;
@class BrowserViewController;

@interface BrowserWindowController : NSWindowController {
	PSMTabBarControl *tabBar;
	NSTabView *tabView;
	BrowserViewController *activeBrowser;
}

@property (readwrite, retain, nonatomic) IBOutlet PSMTabBarControl *tabBar;
@property (readwrite, retain, nonatomic) IBOutlet NSTabView *tabView;
@property (readwrite, assign, nonatomic) BrowserViewController *activeBrowser;

- (IBAction) newTab: (id) sender;

- (void) addTab: (BrowserViewController *)browser;
- (void) removeTab: (BrowserViewController *)browser;

@end
