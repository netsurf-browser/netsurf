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

#import "desktop/browser.h"
#import "desktop/history_core.h"
#import "desktop/plotters.h"

@implementation HistoryView

@synthesize browser;

- (void) setBrowser: (struct browser_window *) bw;
{
	browser = bw;
	[self updateHistory];
}

- (NSSize) size;
{
	const CGFloat padding = 10;
	
	int width, height;
	history_size( browser->history, &width, &height );
	
	return NSMakeSize( cocoa_px_to_pt( width ) + padding, cocoa_px_to_pt( height ) + padding );
}

- (void) updateHistory;
{
	[self setFrameSize: [self size]];
	[self setNeedsDisplay: YES];
}

- (void) drawRect: (NSRect)rect;
{
	cocoa_set_font_scale_factor( 1.0 );
	cocoa_set_clip( rect );
	
	history_redraw( browser->history );
}

- (void) mouseUp: (NSEvent *)theEvent;
{
	const NSPoint location = [self convertPoint: [theEvent locationInWindow] fromView: nil];
	const bool newWindow = [theEvent modifierFlags] & NSCommandKeyMask;
	history_click( browser, browser->history, 
				   cocoa_pt_to_px( location.x ), cocoa_pt_to_px( location.y ), 
				   newWindow );
}

- (BOOL) isFlipped;
{
	return YES;
}

@end
