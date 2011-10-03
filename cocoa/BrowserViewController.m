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

#import "cocoa/BrowserViewController.h"
#import "cocoa/BrowserView.h"
#import "cocoa/BrowserWindowController.h"

#import "desktop/browser.h"
#import "desktop/history_core.h"
#import "desktop/textinput.h"
#import "desktop/options.h"
#import "desktop/selection.h"

#import "utils/filename.h"
#import "utils/url.h"


@implementation BrowserViewController

@synthesize browser;
@synthesize url;
@synthesize browserView;
@synthesize windowController;
@synthesize title;
@synthesize status;
@synthesize isProcessing;
@synthesize favicon;
@synthesize canGoBack;
@synthesize canGoForward;

- (void) dealloc;
{
	[self setUrl: nil];
	[self setBrowserView: nil];
	[self setWindowController: nil];
	[self setTitle: nil];
	[self setStatus: nil];
	[self setFavicon: nil];
	
	[super dealloc];
}

- initWithBrowser: (struct browser_window *) bw;
{
	if ((self = [super initWithNibName: @"Browser" bundle: nil]) == nil) return nil;
	
	browser = bw;
	
	return self;
}

- (IBAction) navigate: (id) sender;
{
	browser_window_go( browser, [url UTF8String], NULL, true );
}

- (void) awakeFromNib;
{
	[browserView setBrowser: browser];
}


- (IBAction) zoomIn: (id) sender;
{
	browser_window_set_scale( browser, browser->scale * 1.1, true );
}

- (IBAction) zoomOut: (id) sender;
{
	browser_window_set_scale( browser, browser->scale * 0.9, true );
}

- (IBAction) zoomOriginal: (id) sender;
{
	browser_window_set_scale( browser, (float)option_scale / 100.0, true );
}

- (IBAction) backForwardSelected: (id) sender;
{
	if ([sender selectedSegment] == 0) [self goBack: sender];
	else [self goForward: sender];
}

- (IBAction) goBack: (id) sender;
{
	if (browser && history_back_available( browser->history )) {
		history_back(browser, browser->history);
		[self updateBackForward];
	}
}

- (IBAction) goForward: (id) sender;
{
	if (browser && history_forward_available( browser->history )) {
		history_forward(browser, browser->history);
		[self updateBackForward];
	}
}

- (IBAction) goHome: (id) sender;
{
	browser_window_go( browser, option_homepage_url, NULL, true );
}

- (IBAction) reloadPage: (id) sender;
{
	browser_window_reload( browser, true );
}

- (IBAction) stopLoading: (id) sender;
{
	browser_window_stop( browser );
}

- (IBAction) viewSource: (id) sender;
{
	struct hlcache_handle *content;
	size_t size;
	const char *source;
	const char *path = NULL;

	if (browser == NULL)
		return;
	content = browser->current_content;
	if (content == NULL)
		return;
	source = content_get_source_data(content, &size);
	if (source == NULL)
		return;

	/* try to load local files directly. */
	char *scheme;
	if (url_scheme(nsurl_access(content_get_url(content)), &scheme) != URL_FUNC_OK)
		return;
	if (strcmp(scheme, "file") == 0)
		path = url_to_path(nsurl_access(content_get_url(content)));
	free(scheme);

	if (path == NULL) {
		/* We cannot release the requested filename until after it
		 * has finished being used. As we can't easily find out when
		 * this is, we simply don't bother releasing it and simply
		 * allow it to be re-used next time NetSurf is started. The
		 * memory overhead from doing this is under 1 byte per
		 * filename. */
		const char *filename = filename_request();
		const char *extension = "txt";
		fprintf(stderr, "filename '%p'\n", filename);
		if (filename == NULL)
			return;
		lwc_string *str = content_get_mime_type(content);
		if (str) {
			NSString *mime = [NSString stringWithUTF8String:lwc_string_data(str)];
			NSString *uti = (NSString *)UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, (CFStringRef)mime, NULL);
			NSString *ext = (NSString *)UTTypeCopyPreferredTagWithClass((CFStringRef)uti, kUTTagClassFilenameExtension);
			extension = [ext UTF8String];
			lwc_string_unref(str);
		}

		NSURL *dataUrl = [NSURL URLWithString:[NSString stringWithFormat:@"%s.%s", filename, extension]
			relativeToURL:[NSURL fileURLWithPath:@TEMP_FILENAME_PREFIX]];


		NSData *data = [NSData dataWithBytes:source length:size];
		[data writeToURL:dataUrl atomically:NO];
		path = [[dataUrl path] UTF8String];
	}

	if (path) {
		NSString * p = [NSString stringWithUTF8String: path];
		NSWorkspace * ws = [NSWorkspace sharedWorkspace];
		[ws openFile:p withApplication:@"Xcode"];	
	}
}

static inline bool compare_float( float a, float b )
{
	const float epsilon = 0.00001;
	
	if (a == b) return true;
	
	return fabs( (a - b) / b ) <= epsilon;
}

- (BOOL) validateUserInterfaceItem: (id) item;
{
	SEL action = [item action];
	
	if (action == @selector(copy:)) {
		return browser_window_has_selection( browser );
	}
	
	if (action == @selector(cut:)) {
		return browser_window_has_selection( browser ) && browser->caret_callback != NULL;
	}
	
	if (action == @selector(paste:)) {
		return browser->paste_callback != NULL;
	}
	
	if (action == @selector( stopLoading: )) {
		return browser->loading_content != NULL;
	}
	
	if (action == @selector( zoomOriginal: )) {
		return !compare_float( browser->scale, (float)option_scale / 100.0 );
	}
	
	if (action == @selector( goBack: )) {
		return canGoBack;
	}
	
	if (action == @selector( goForward: )) {
		return canGoForward;
	}
	
	return YES;
}


- (void) updateBackForward;
{
	[browserView updateHistory];
	[self setCanGoBack: browser != NULL && history_back_available( browser->history )];
	[self setCanGoForward: browser != NULL && history_forward_available( browser->history )];
}

- (void) contentUpdated;
{
	[browserView updateHistory];
}

struct history_add_menu_item_data {
	NSInteger index;
	NSMenu *menu;
	id target;
};

static bool history_add_menu_item_cb( const struct history *history, int x0, int y0, int x1, int y1, 
									 const struct history_entry *page, void *user_data )
{
	struct history_add_menu_item_data *data = user_data; 
	
	NSMenuItem *item = nil;
	if (data->index < [data->menu numberOfItems]) {
		item = [data->menu itemAtIndex: data->index];
	} else {
		item = [[NSMenuItem alloc] initWithTitle: @"" 
										  action: @selector( historyItemSelected: ) 
								   keyEquivalent: @""];
		[data->menu addItem: item];
		[item release];
	}
	++data->index;
	
	[item setTarget: data->target];
	[item setTitle: [NSString stringWithUTF8String: history_entry_get_title( page )]];
	[item setRepresentedObject: [NSValue valueWithPointer: page]];
	
	return true;
}

- (IBAction) historyItemSelected: (id) sender;
{
	struct history_entry *entry = [[sender representedObject] pointerValue];
	history_go( browser, browser->history, entry, false );
	[self updateBackForward];
}

- (void) buildBackMenu: (NSMenu *)menu;
{
	struct history_add_menu_item_data data = {
		.index = 0,
		.menu = menu,
		.target = self
	};
	history_enumerate_back( browser->history, history_add_menu_item_cb, &data );
	while (data.index < [menu numberOfItems]) [menu removeItemAtIndex: data.index];
}

- (void) buildForwardMenu: (NSMenu *)menu;
{
	struct history_add_menu_item_data data = {
		.index = 0,
		.menu = menu,
		.target = self
	};
	history_enumerate_forward( browser->history, history_add_menu_item_cb, &data );
	while (data.index < [menu numberOfItems]) [menu removeItemAtIndex: data.index];
}

@end
