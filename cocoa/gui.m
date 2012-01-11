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

#import "cocoa/gui.h"
#import "cocoa/plotter.h"
#import "cocoa/BrowserView.h"
#import "cocoa/BrowserViewController.h"
#import "cocoa/BrowserWindowController.h"
#import "cocoa/FormSelectMenu.h"

#import "desktop/gui.h"
#import "desktop/netsurf.h"
#import "desktop/browser.h"
#import "desktop/options.h"
#import "desktop/textinput.h"
#import "desktop/selection.h"
#import "desktop/401login.h"
#import "utils/utils.h"
#import "image/ico.h"
#import "content/fetchers/resource.h"

NSString * const kCookiesFileOption = @"CookiesFile";
NSString * const kURLsFileOption = @"URLsFile";
NSString * const kHotlistFileOption = @"Hotlist";
NSString * const kHomepageURLOption = @"HomepageURL";
NSString * const kOptionsFileOption = @"ClassicOptionsFile";
NSString * const kAlwaysCancelDownload = @"AlwaysCancelDownload";
NSString * const kAlwaysCloseMultipleTabs = @"AlwaysCloseMultipleTabs";

#define UNIMPL() NSLog( @"Function '%s' unimplemented", __func__ )

nsurl *gui_get_resource_url(const char *path)
{
	nsurl *url = NULL;
	NSString *nspath = [[NSBundle mainBundle] pathForResource: [NSString stringWithUTF8String: path] ofType: @""];
	if (nspath == nil) return NULL;
	nsurl_create([[[NSURL fileURLWithPath: nspath] absoluteString] UTF8String], &url);
	return url;
}

void gui_poll(bool active)
{
	cocoa_autorelease();
	
	NSEvent *event = [NSApp nextEventMatchingMask: NSAnyEventMask untilDate: active ? nil : [NSDate distantFuture]
										   inMode: NSDefaultRunLoopMode dequeue: YES];
	
	if (nil != event) {
		[NSApp sendEvent: event];
		[NSApp updateWindows];	
	}
}

void gui_quit(void)
{
	// nothing to do
}

struct browser_window;

struct gui_window *gui_create_browser_window(struct browser_window *bw,
											 struct browser_window *clone, bool new_tab)
{
	BrowserWindowController *window = nil;

	if (clone != NULL) {
		bw->scale = clone->scale;
		window = [(BrowserViewController *)(clone->window) windowController];
	} else {
		bw->scale = (float) option_scale / 100;	
	}

	BrowserViewController *result = [[BrowserViewController alloc] initWithBrowser: bw];

	if (!new_tab || nil == window) {
		window = [[[BrowserWindowController alloc] init] autorelease];
		[[window window] makeKeyAndOrderFront: nil];
	}
	[window addTab: result];
	
	return (struct gui_window *)result;
}

void gui_window_destroy(struct gui_window *g)
{
	BrowserViewController *vc = (BrowserViewController *)g;

	if ([vc browser]->parent != NULL) [[vc view] removeFromSuperview];
	[vc release];
}

void gui_window_set_title(struct gui_window *g, const char *title)
{
	[(BrowserViewController *)g setTitle: [NSString stringWithUTF8String: title]];
}

void gui_window_redraw_window(struct gui_window *g)
{
	[[(BrowserViewController *)g browserView] setNeedsDisplay: YES];
}

void gui_window_update_box(struct gui_window *g, const struct rect *rect)
{
	const NSRect nsrect = cocoa_scaled_rect_wh( [(BrowserViewController *)g browser]->scale, 
											 rect->x0, rect->y0, 
											 rect->x1 - rect->x0, rect->y1 - rect->y0 );
	[[(BrowserViewController *)g browserView] setNeedsDisplayInRect: nsrect];
}

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	NSCParameterAssert( g != NULL && sx != NULL && sy != NULL );
	
	NSRect visible = [[(BrowserViewController *)g browserView] visibleRect];
	*sx = cocoa_pt_to_px( NSMinX( visible ) );
	*sy = cocoa_pt_to_px( NSMinY( visible ) );
	return true;
}

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	[[(BrowserViewController *)g browserView] scrollPoint: cocoa_point( sx, sy )];
}

void gui_window_scroll_visible(struct gui_window *g, int x0, int y0,
							   int x1, int y1)
{
	gui_window_set_scroll( g, x0, y0 );
}

void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
							   bool scaled)
{
	NSCParameterAssert( width != NULL && height != NULL );
	
	NSRect frame = [[[(BrowserViewController *)g browserView] superview] frame];
	if (scaled) {
		const CGFloat scale = [(BrowserViewController *)g browser]->scale;
		frame.size.width /= scale;
		frame.size.height /= scale;
	}
	*width = cocoa_pt_to_px( NSWidth( frame ) );
	*height = cocoa_pt_to_px( NSHeight( frame ) );
}

void gui_window_update_extent(struct gui_window *g)
{
	BrowserViewController * const window = (BrowserViewController *)g;

	struct browser_window *browser = [window browser];
	int width = content_get_width( browser->current_content );
	int height = content_get_height( browser->current_content );
	
	[[window browserView] setMinimumSize: cocoa_scaled_size( browser->scale, width, height )];
}

void gui_window_set_status(struct gui_window *g, const char *text)
{
	[(BrowserViewController *)g setStatus: [NSString stringWithUTF8String: text]];
}

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	switch (shape) {
		case GUI_POINTER_DEFAULT:
		case GUI_POINTER_WAIT:
		case GUI_POINTER_PROGRESS:
			[[NSCursor arrowCursor] set];
			break;
			
		case GUI_POINTER_CROSS:
			[[NSCursor crosshairCursor] set];
			break;
			
		case GUI_POINTER_POINT:
		case GUI_POINTER_MENU:
			[[NSCursor pointingHandCursor] set];
			break;
			
		case GUI_POINTER_CARET:
			[[NSCursor IBeamCursor] set];
			break;
			
		case GUI_POINTER_MOVE:
			[[NSCursor closedHandCursor] set];
			break;

		default:
			NSLog( @"Other cursor %d requested", shape );
			[[NSCursor arrowCursor] set];
			break;
	}
}

void gui_window_hide_pointer(struct gui_window *g)
{
}

void gui_window_set_url(struct gui_window *g, const char *url)
{
	[(BrowserViewController *)g setUrl: [NSString stringWithUTF8String: url]];
}

void gui_window_start_throbber(struct gui_window *g)
{
	[(BrowserViewController *)g setIsProcessing: YES];
	[(BrowserViewController *)g updateBackForward];
}

void gui_window_stop_throbber(struct gui_window *g)
{
	[(BrowserViewController *)g setIsProcessing: NO];
	[(BrowserViewController *)g updateBackForward];
}

void gui_window_set_icon(struct gui_window *g, hlcache_handle *icon)
{
	NSBitmapImageRep *bmp = icon != NULL ? (NSBitmapImageRep *)content_get_bitmap( icon ) : NULL;

	NSImage *image = nil;
	if (bmp != nil) {
		image = [[NSImage alloc] initWithSize: NSMakeSize( 32, 32 )];
		[image addRepresentation: bmp];
	} else {
		image = [[NSImage imageNamed: @"NetSurf"] copy];
	}
	[image setFlipped: YES];

	[(BrowserViewController *)g setFavicon: image];
	[image release];
}

void gui_window_set_search_ico(hlcache_handle *ico)
{
	UNIMPL();
}

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
	[[(BrowserViewController *)g browserView] addCaretAt: cocoa_point( x, y ) 
												  height: cocoa_px_to_pt( height )];
}

void gui_window_remove_caret(struct gui_window *g)
{
	[[(BrowserViewController *)g browserView] removeCaret];
}

void gui_window_new_content(struct gui_window *g)
{
	[(BrowserViewController *)g contentUpdated];
}

bool gui_window_scroll_start(struct gui_window *g)
{
	return true;
}

bool gui_window_drag_start(struct gui_window *g, gui_drag_type type,
		const struct rect *rect)
{
	return true;
}

void gui_window_save_link(struct gui_window *g, const char *url, 
						  const char *title)
{
	UNIMPL();
}

void gui_drag_save_object(gui_save_type type, hlcache_handle *c,
						  struct gui_window *g)
{
}

void gui_drag_save_selection(struct selection *s, struct gui_window *g)
{
}


void gui_create_form_select_menu(struct browser_window *bw,
								 struct form_control *control)
{
	FormSelectMenu  *menu = [[FormSelectMenu alloc] initWithControl: control forWindow: bw];
	[menu runInView: [(BrowserViewController *)bw->window browserView]];
	[menu release];
}

void gui_launch_url(const char *url)
{
	[[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString: [NSString stringWithUTF8String: url]]];
}

struct ssl_cert_info;

void gui_cert_verify(const char *url, const struct ssl_cert_info *certs, 
					 unsigned long num, nserror (*cb)(bool proceed, void *pw),
					 void *cbpw)
{
	cb( false, cbpw );
}


void gui_401login_open(const char *url, const char *realm,
					   nserror (*cb)(bool proceed, void *pw), void *cbpw)
{
	cb( false, cbpw );
}

