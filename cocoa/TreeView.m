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

#import "cocoa/TreeView.h"
#import "cocoa/Tree.h"

#import "desktop/plotters.h"

@interface TreeView () <TreeDelegate>
@end

@implementation TreeView

@synthesize tree;

- (void)drawRect:(NSRect)dirtyRect 
{
	[tree drawRect: dirtyRect inView: self];
}

- (BOOL) isFlipped;
{
	return YES;
}

- (void) dealloc;
{
	[self setTree: nil];
	[super dealloc];
}

- (void) setTree: (Tree *)newTree;
{
	if (tree != newTree) {
		[tree setRedrawing: NO];
		[tree setDelegate: nil];
		[tree release];
		
		tree = [newTree retain];
		[tree setDelegate: self];
		[tree setRedrawing: YES];
	}
}

//MARK: -
//MARK: Event handlers

- (void)mouseDown: (NSEvent *)event;
{
	isDragging = NO;
	dragStart = [self convertPoint: [event locationInWindow] fromView: nil];
	[tree mouseAction: BROWSER_MOUSE_PRESS_1 atPoint: dragStart];
}

#define squared(x) ((x)*(x))
#define MinDragDistance (5.0)

- (void) mouseDragged: (NSEvent *)event;
{
	const NSPoint point = [self convertPoint: [event locationInWindow] fromView: nil];
	
	if (!isDragging) {
		const CGFloat distance = squared( dragStart.x - point.x ) + squared( dragStart.y - point.y );
		if (distance >= squared( MinDragDistance)) isDragging = YES;
	}
}

- (void) mouseUp: (NSEvent *)event;
{
	const NSPoint point = [self convertPoint: [event locationInWindow] fromView: nil];

	browser_mouse_state modifierFlags = 0;
	
	if (isDragging) {
		isDragging = NO;
		[tree mouseDragEnd: modifierFlags fromPoint: dragStart toPoint: point];
	} else {
		modifierFlags |= BROWSER_MOUSE_CLICK_1;
		if ([event clickCount] == 2) modifierFlags |= BROWSER_MOUSE_DOUBLE_CLICK;
		[tree mouseAction: modifierFlags atPoint: point];
	}
}


//MARK: -
//MARK: Tree delegate methods

- (void) tree: (Tree *)t requestedRedrawInRect: (NSRect) rect;
{
	[self setNeedsDisplayInRect: rect];
}

- (void) tree: (Tree *)t resized: (NSSize) size;
{
	[self setMinimumSize: size];
}

- (void) tree: (Tree *)t scrollPoint: (NSPoint) point;
{
	[self scrollPoint: point];
}

- (NSSize) treeWindowSize: (Tree *)t;
{
	return [self frame].size;
}

@end
