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

#import "URLFieldCell.h"


@interface URLFieldCell ()

@property (readonly, retain, nonatomic) NSButtonCell *refreshCell;

- (NSRect) buttonFrame: (NSRect) cellFrame;

@end


@implementation URLFieldCell

#define BUTTON_SIZE 32
#define PADDING 2

- (void) drawInteriorWithFrame: (NSRect)cellFrame inView: (NSView *)controlView;
{
	[super drawInteriorWithFrame: cellFrame inView: controlView];
	const NSRect buttonRect = [self buttonFrame: cellFrame];
	[[self refreshCell] drawInteriorWithFrame: buttonRect inView: controlView];
}

- (void) selectWithFrame: (NSRect)aRect inView: (NSView *)controlView editor: (NSText *)textObj 
				delegate: (id)anObject start: (NSInteger)selStart length: (NSInteger)selLength;
{
	aRect.size.width -= BUTTON_SIZE + PADDING;
	[super selectWithFrame: aRect inView: controlView editor: textObj 
				  delegate: anObject start: selStart length: selLength];
}

- (void) editWithFrame: (NSRect)aRect inView: (NSView *)controlView editor: (NSText *)textObj 
			  delegate: (id)anObject event: (NSEvent *)theEvent;
{
	aRect.size.width -= BUTTON_SIZE + PADDING;
	[super editWithFrame: aRect inView: controlView editor: textObj 
				delegate: anObject event: theEvent];
}

- (BOOL) trackMouse: (NSEvent *)theEvent inRect: (NSRect)cellFrame ofView: (NSView *)controlView untilMouseUp: (BOOL)flag;
{
	const NSPoint point = [controlView convertPoint: [theEvent locationInWindow] fromView: nil];
	const NSRect buttonRect = [self buttonFrame: cellFrame];
	if (NSPointInRect( point, buttonRect )) {
		return [[self refreshCell] trackMouse: theEvent inRect: buttonRect 
									   ofView: controlView untilMouseUp: flag];
	} else {
		cellFrame.size.width -= BUTTON_SIZE + PADDING;
		return [super trackMouse: theEvent inRect: cellFrame ofView: controlView untilMouseUp: YES];
	}
}

- (void) dealloc;
{
	[refreshCell release];

	[super dealloc];
}

- (NSRect) buttonFrame: (NSRect) cellFrame;
{
	NSRect buttonRect = cellFrame;
	buttonRect.origin.x = NSMaxX( cellFrame ) - BUTTON_SIZE;
	buttonRect.size.width = BUTTON_SIZE;
	return buttonRect;
}

- (NSButtonCell *) refreshCell;
{
	if (nil == refreshCell) {
		refreshCell = [[NSButtonCell alloc] initImageCell: [NSImage imageNamed: NSImageNameRefreshTemplate]];
		[refreshCell setButtonType: NSMomentaryPushInButton];
		[refreshCell setBordered: NO];
	}
	return refreshCell;
}

- (void) setRefreshTarget: (id) newTarget;
{
	[[self refreshCell] setTarget: newTarget];
}

- (id) refreshTarget;
{
	return [[self refreshCell] target];
}

- (void) setRefreshAction: (SEL) newAction;
{
	[[self refreshCell] setAction: newAction];
}

- (SEL) refreshAction;
{
	return [[self refreshCell] action];
}

@end
