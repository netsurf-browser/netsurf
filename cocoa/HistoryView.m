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

#import "cocoa/HistoryView.h"
#import "cocoa/font.h"
#import "cocoa/coordinates.h"
#import "cocoa/plotter.h"
#import "cocoa/LocalHistoryController.h"
#import "cocoa/BrowserView.h"

#import "desktop/browser.h"
#import "desktop/history_core.h"
#import "desktop/plotters.h"

@implementation HistoryView

@synthesize browser = browserView;

- (void) setBrowser: (BrowserView *) bw;
{
	browserView = bw;
	browser = [bw browser];
	[self updateHistory];
}

- (NSSize) size;
{
	int width, height;
	history_size( browser->history, &width, &height );
	
	return cocoa_size( width, height );
}

- (void) updateHistory;
{
	[self setFrameSize: [self size]];
	[self setNeedsDisplay: YES];
}

- (void) drawRect: (NSRect)rect;
{
	[[NSColor clearColor] set];
	[NSBezierPath fillRect: rect];

	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &cocoa_plotters
	};
	
	cocoa_set_clip( rect );
	
	history_redraw( browser->history, &ctx );
}

- (void) mouseUp: (NSEvent *)theEvent;
{
	const NSPoint location = [self convertPoint: [theEvent locationInWindow] fromView: nil];
	const bool newWindow = [theEvent modifierFlags] & NSCommandKeyMask;
	if (history_click( browser, browser->history, 
					   cocoa_pt_to_px( location.x ), cocoa_pt_to_px( location.y ),
					   newWindow )) {
		[browserView setHistoryVisible: NO];
	}
}

- (BOOL) isFlipped;
{
	return YES;
}

- (void) mouseEntered: (NSEvent *) event;
{
	[[NSCursor pointingHandCursor] set];
}

- (void) mouseExited: (NSEvent *) event;
{
	[[NSCursor arrowCursor] set];
}

static bool cursor_rects_cb( const struct history *history, int x0, int y0, int x1, int y1, 
							const struct history_entry *page, void *user_data )
{
	HistoryView *view = user_data;
	
	NSRect rect = NSIntersectionRect( [view visibleRect], cocoa_rect( x0, y0, x1, y1 ) );
	if (!NSIsEmptyRect( rect )) {
		
		NSString *toolTip = [NSString stringWithFormat: @"%s\n%s", history_entry_get_title(page),
							  history_entry_get_url( page )];
		
		[view addToolTipRect: rect owner: toolTip userData: nil];
		NSTrackingArea *area = [[NSTrackingArea alloc] initWithRect: rect 
															options: NSTrackingMouseEnteredAndExited | NSTrackingActiveInActiveApp
															  owner: view userInfo: nil];
		[view addTrackingArea: area];
		[area release];
	}
	
	return true;
}

- (NSToolTipTag)addToolTipRect: (NSRect) rect owner: (id) owner userData: (void *) userData;
{
	if (toolTips == nil) toolTips = [[NSMutableArray alloc] init];
	[toolTips addObject: owner];
	
	return [super addToolTipRect: rect owner: owner userData: userData];
}

- (void) removeAllToolTips;
{
	[super removeAllToolTips];
	[toolTips removeAllObjects];
}

- (void) updateTrackingAreas;
{
	[self removeAllToolTips];

	for (NSTrackingArea *area in [self trackingAreas]) {
		[self removeTrackingArea: area];
	}
	
	history_enumerate( browser->history, cursor_rects_cb, self );
	
	[super updateTrackingAreas];
}

- (void) dealloc;
{
	[self removeAllToolTips];
	[super dealloc];
}

@end
