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

#import "BrowserView.h"
#import "HistoryView.h"

#import "desktop/browser.h"
#import "desktop/history_core.h"
#import "desktop/plotters.h"
#import "desktop/textinput.h"
#import "desktop/options.h"
#import "desktop/selection.h"

#import "cocoa/font.h"


@implementation BrowserView

@synthesize browser;
@synthesize caretTimer;
@synthesize resizing = isResizing;

static const CGFloat CaretWidth = 1.0;
static const NSTimeInterval CaretBlinkTime = 0.8;

- (void) dealloc;
{
	[self setCaretTimer: nil];
	[history release];
	
	[super dealloc];
}

- (void) setCaretTimer: (NSTimer *)newTimer;
{
	if (newTimer != caretTimer) {
		[caretTimer invalidate];
		[caretTimer release];
		caretTimer = [newTimer retain];
	}
}

static inline NSRect cocoa_get_caret_rect( BrowserView *view )
{
	NSRect caretRect = {
		.origin = NSMakePoint( view->caretPoint.x * view->browser->scale, view->caretPoint.y * view->browser->scale ),
		.size = NSMakeSize( CaretWidth, view->caretHeight * view->browser->scale )
	};
	
	return caretRect;
}

- (void) removeCaret;
{
	hasCaret = NO;
	[self setNeedsDisplayInRect: cocoa_get_caret_rect( self )];

	[self setCaretTimer: nil];
}

- (void) addCaretAt: (NSPoint) point height: (CGFloat) height;
{
	if (hasCaret) {
		[self setNeedsDisplayInRect: cocoa_get_caret_rect( self )];
	}
	
	caretPoint = point;
	caretHeight = height;
	hasCaret = YES;
	caretVisible = YES;
	
	if (nil == caretTimer) {
		[self setCaretTimer: [NSTimer scheduledTimerWithTimeInterval: CaretBlinkTime target: self selector: @selector(caretBlink:) userInfo: nil repeats: YES]];
	} else {
		[caretTimer setFireDate: [NSDate dateWithTimeIntervalSinceNow: CaretBlinkTime]];
	}
	
	[self setNeedsDisplayInRect: cocoa_get_caret_rect( self )];
}
		 
		 
- (void) caretBlink: (NSTimer *)timer;
{
	if (hasCaret) {
		caretVisible = !caretVisible;
		[self setNeedsDisplayInRect: cocoa_get_caret_rect( self )];
	}
}

- (void)drawRect:(NSRect)dirtyRect; 
{
	if (NULL == browser->current_content) return;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	cocoa_set_font_scale_factor( browser->scale );
	
	NSRect frame = [self bounds];
	
	const NSRect *rects = NULL;
	NSInteger count = 0;
	[self getRectsBeingDrawn: &rects count: &count];
	
	for (NSInteger i = 0; i < count; i++) {
		plot.clip( NSMinX( rects[i] ), NSMinY( rects[i]), NSMaxX( rects[i] ), NSMaxY( rects[i] ) );

		content_redraw(browser->current_content,
					   0,
					   0,
					   NSWidth( frame ),
					   NSHeight( frame ),
					   NSMinX( rects[i] ),
					   NSMinY( rects[i] ),
					   NSMaxX( rects[i] ),
					   NSMaxY( rects[i] ),
					   browser->scale,
					   0xFFFFFF);
	}

	NSRect caretRect = cocoa_get_caret_rect( self );
	if (hasCaret && caretVisible && [self needsToDrawRect: caretRect]) {
		[[NSColor blackColor] set];
		[NSBezierPath fillRect: caretRect];
	}
	
	[pool release];
}

- (BOOL) isFlipped;
{
	return YES;
}

static browser_mouse_state cocoa_mouse_flags_for_event( NSEvent *evt )
{
	browser_mouse_state result = 0;
	
	NSUInteger flags = [evt modifierFlags];
	
	if (flags & NSShiftKeyMask) result |= BROWSER_MOUSE_MOD_1;
	if (flags & NSAlternateKeyMask) result |= BROWSER_MOUSE_MOD_2;
	
	return result;
}

- (NSPoint) convertMousePoint: (NSEvent *)event;
{
	NSPoint location = [self convertPoint: [event locationInWindow] fromView: nil];
	if (NULL != browser) {
		location.x /= browser->scale;
		location.y /= browser->scale;
	}
	return location;
}

- (void) mouseDown: (NSEvent *)theEvent;
{
	dragStart = [self convertMousePoint: theEvent];

	browser_window_mouse_click( browser, BROWSER_MOUSE_PRESS_1 | cocoa_mouse_flags_for_event( theEvent ), dragStart.x, dragStart.y );
}

- (void) mouseUp: (NSEvent *)theEvent;
{
	if (historyVisible) {
		[self setHistoryVisible: NO];
		return;
	}
	
	NSPoint location = [self convertMousePoint: theEvent];

	browser_mouse_state modifierFlags = cocoa_mouse_flags_for_event( theEvent );
	
	if (isDragging) {
		isDragging = NO;
		browser_window_mouse_drag_end( browser, modifierFlags, location.x, location.y );
	} else {
		modifierFlags |= BROWSER_MOUSE_CLICK_1;
		if ([theEvent clickCount] == 2) modifierFlags |= BROWSER_MOUSE_DOUBLE_CLICK;
		browser_window_mouse_click( browser, modifierFlags, location.x, location.y );
	}
}

#define squared(x) ((x)*(x))
#define MinDragDistance (5.0)

- (void) mouseDragged: (NSEvent *)theEvent;
{
	NSPoint location = [self convertMousePoint: theEvent];

	if (!isDragging) {
		const CGFloat distance = squared( dragStart.x - location.x ) + squared( dragStart.y - location.y );
		if (distance >= squared( MinDragDistance)) isDragging = YES;
	}
	
	if (isDragging) {
		browser_mouse_state modifierFlags = cocoa_mouse_flags_for_event( theEvent );
		browser_window_mouse_click( browser, BROWSER_MOUSE_DRAG_1 | modifierFlags, location.x, location.y );
		browser_window_mouse_track( browser, BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_ON | modifierFlags, location.x, location.y );
	}
}

- (void) mouseMoved: (NSEvent *)theEvent;
{
	NSPoint location = [self convertMousePoint: theEvent];

	browser_window_mouse_track( browser, cocoa_mouse_flags_for_event( theEvent ), location.x, location.y );
}

- (void) keyDown: (NSEvent *)theEvent;
{
	[self interpretKeyEvents: [NSArray arrayWithObject: theEvent]];
}

- (void) insertText: (id)string;
{
	for (NSUInteger i = 0, length = [string length]; i < length; i++) {
		unichar ch = [string characterAtIndex: i];
		browser_window_key_press( browser, ch );
	}
}

- (void) moveLeft: (id)sender;
{
	browser_window_key_press( browser, KEY_LEFT );
}

- (void) moveRight: (id)sender;
{
	browser_window_key_press( browser, KEY_RIGHT );
}

- (void) moveUp: (id)sender;
{
	browser_window_key_press( browser, KEY_UP );
}

- (void) moveDown: (id)sender;
{
	browser_window_key_press( browser, KEY_DOWN );
}

- (void) deleteBackward: (id)sender;
{
	browser_window_key_press( browser, KEY_DELETE_LEFT );
}

- (void) deleteForward: (id)sender;
{
	browser_window_key_press( browser, KEY_DELETE_RIGHT );
}

- (void) cancelOperation: (id)sender;
{
	browser_window_key_press( browser, KEY_ESCAPE );
}

- (void) scrollPageUp: (id)sender;
{
	browser_window_key_press( browser, KEY_PAGE_UP );
}

- (void) scrollPageDown: (id)sender;
{
	browser_window_key_press( browser, KEY_PAGE_DOWN );
}

- (void) insertTab: (id)sender;
{
	browser_window_key_press( browser, KEY_TAB );
}

- (void) insertBacktab: (id)sender;
{
	browser_window_key_press( browser, KEY_SHIFT_TAB );
}

- (void) moveToBeginningOfLine: (id)sender;
{
	browser_window_key_press( browser, KEY_LINE_START );
}

- (void) moveToEndOfLine: (id)sender;
{
	browser_window_key_press( browser, KEY_LINE_END );
}

- (void) moveToBeginningOfDocument: (id)sender;
{
	browser_window_key_press( browser, KEY_TEXT_START );
}

- (void) moveToEndOfDocument: (id)sender;
{
	browser_window_key_press( browser, KEY_TEXT_END );
}

- (void) insertNewline: (id)sender;
{
	browser_window_key_press( browser, KEY_NL );
}

- (void) selectAll: (id)sender;
{
	browser_window_key_press( browser, KEY_SELECT_ALL );
}

- (void) copy: (id) sender;
{
	browser_window_key_press( browser, KEY_COPY_SELECTION );
}

- (void) cut: (id) sender;
{
	browser_window_key_press( browser, KEY_CUT_SELECTION );
}

- (void) paste: (id) sender;
{
	browser_window_key_press( browser, KEY_PASTE );
}

- (BOOL) acceptsFirstResponder;
{
	return YES;
}

- (void) adjustFrame;
{
	if (!isResizing) {
		NSSize frameSize = [[self superview] frame].size;
		browser_window_reformat( browser, frameSize.width, frameSize.height );
	}
	
	[super adjustFrame];
}

- (BOOL) isHistoryVisible;
{
	return historyVisible;
}

- (void) setHistoryVisible: (BOOL) newVisible;
{
	if (newVisible == historyVisible) return;
	historyVisible = newVisible;
	
	if (historyVisible) {
		if (nil  == history) history = [[HistoryView alloc] initWithBrowser: browser];
		[history fadeIntoView: self];
	} else {
		[history fadeOut];
	}
}

@end
