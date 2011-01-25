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

#import "TreeView.h"

#import "desktop/tree.h"
#import "desktop/plotters.h"
#import "desktop/history_global_core.h"


@implementation TreeView

static void tree_redraw_request( int x, int y, int w, int h, void *data );
static void tree_resized( struct tree *tree, int w, int h, void *data );
static void tree_scroll_visible( int y, int height, void *data );
static void tree_get_window_dimensions( int *width, int *height, void *data );

static const struct treeview_table cocoa_tree_callbacks = {
	.redraw_request = tree_redraw_request,
	.resized = tree_resized,
	.scroll_visible = tree_scroll_visible,
	.get_window_dimensions = tree_get_window_dimensions
};

- (id)initWithFrame:(NSRect)frame 
{
    if ((self = [super initWithFrame:frame]) == nil) return nil;

	treeHandle = tree_create( history_global_get_tree_flags(), &cocoa_tree_callbacks, self );
	if (NULL == treeHandle) {
		[self release];
		self = nil;
	}
	history_global_initialise( treeHandle, "" );
    return self;
}

- (void) dealloc;
{
	tree_delete( treeHandle );
	
	[super dealloc];
}

- (void)drawRect:(NSRect)dirtyRect 
{
	tree_set_redraw( treeHandle, true );
	tree_draw( treeHandle, 0, 0, NSMinX( dirtyRect ), NSMinY( dirtyRect ), NSWidth( dirtyRect ), NSHeight( dirtyRect ) );
}

- (BOOL) isFlipped;
{
	return YES;
}

- (void) mouseDown: (NSEvent *)theEvent;
{
	NSPoint point = [self convertPoint: [theEvent locationInWindow] fromView: nil];
	dragStart = point;
	
	tree_mouse_action( treeHandle, BROWSER_MOUSE_PRESS_1, point.x, point.y );
}

- (void) mouseUp: (NSEvent *)theEvent;
{
	NSPoint point = [self convertPoint: [theEvent locationInWindow] fromView: nil];
	if (isDragging) {
		tree_drag_end( treeHandle, BROWSER_MOUSE_DRAG_1, dragStart.x, dragStart.y, point.x, point.y );
		isDragging = NO;
	} else {
		tree_mouse_action( treeHandle, BROWSER_MOUSE_CLICK_1, point.x, point.y );
	}
}


#define squared(x) ((x)*(x))
#define MinDragDistance (5.0)

- (void) mouseDragged: (NSEvent *)theEvent;
{
	NSPoint point = [self convertPoint: [theEvent locationInWindow] fromView: nil];
	
	if (!isDragging) {
		const CGFloat distance = squared( dragStart.x - point.x ) + squared( dragStart.y - point.y );
		if (distance >= squared( MinDragDistance)) isDragging = YES;
	}
	
	if (isDragging) {
		tree_mouse_action( treeHandle, BROWSER_MOUSE_DRAG_1, point.x, point.y );
	}		
}

static void tree_redraw_request( int x, int y, int w, int h, void *data )
{
	[(TreeView *)data setNeedsDisplayInRect: NSMakeRect( x, y, w, h )];
}

static void tree_resized( struct tree *tree, int w, int h, void *data )
{
	[(TreeView *)data setMinimumSize: NSMakeSize( w, h )];
}

static void tree_scroll_visible( int y, int height, void *data )
{
	[(TreeView *)data scrollPoint: NSMakePoint( 0, y )];
}

static void tree_get_window_dimensions( int *width, int *height, void *data )
{
	NSSize size = [(TreeView *)data frame].size;
	*width = size.width;
	*height = size.height;
}

@end
