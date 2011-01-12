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

struct browser_window;

@class BrowserView;

@interface BrowserWindow : NSWindowController {
	struct browser_window *browser;
	NSString *url;
	BrowserView *view;
}

@property (readwrite, assign, nonatomic) struct browser_window *browser;
@property (readwrite, copy, nonatomic) NSString *url;
@property (readwrite, retain, nonatomic) IBOutlet BrowserView *view;

- initWithBrowser: (struct browser_window *) bw;

- (IBAction) navigate: (id) sender;

@end
