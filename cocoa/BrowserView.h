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


@interface BrowserView : NSView {
	struct browser_window *browser;
	BOOL spinning;
	NSString *status;
	
	NSPoint caretPoint;
	CGFloat caretHeight;
	BOOL caretVisible;
	BOOL hasCaret;
	NSTimer *caretTimer;
	
	BOOL isDragging;
	NSPoint dragStart;
}

@property (readwrite, assign, nonatomic) struct browser_window *browser;
@property (readwrite, assign, nonatomic) BOOL spinning;
@property (readwrite, copy, nonatomic) NSString *status;
@property (readwrite, retain, nonatomic) NSTimer *caretTimer;

- (void) removeCaret;
- (void) addCaretAt: (NSPoint) point height: (CGFloat) height;

- (IBAction) goBack: (id) sender;
- (IBAction) goForward: (id) sender;
- (IBAction) showHistory: (id) sender;
- (IBAction) reloadPage: (id) sender;
- (IBAction) stopLoading: (id) sender;

- (IBAction) zoomIn: (id) sender;
- (IBAction) zoomOut: (id) sender;
- (IBAction) zoomOriginal: (id) sender;

@end
