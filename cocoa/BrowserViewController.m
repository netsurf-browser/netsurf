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
		navigatedUsingBackForwards = YES;
		history_back(browser, browser->history);
		[self updateBackForward];
	}
}

- (IBAction) goForward: (id) sender;
{
	if (browser && history_forward_available( browser->history )) {
		navigatedUsingBackForwards = YES;
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
		return selection_defined( browser->sel );
	}
	
	if (action == @selector(cut:)) {
		return selection_defined( browser->sel ) && browser->caret_callback != NULL;
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
	[self setCanGoBack: browser != NULL && history_back_available( browser->history )];
	[self setCanGoForward: browser != NULL && history_forward_available( browser->history )];
}

- (void) contentUpdated;
{
	if (!navigatedUsingBackForwards) [browserView setHistoryVisible: NO];
	navigatedUsingBackForwards = NO;
}

@end
