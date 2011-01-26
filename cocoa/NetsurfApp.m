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

#import "NetsurfApp.h"

#import "cocoa/gui.h"

#import "desktop/gui.h"
#include "content/urldb.h"
#include "content/fetch.h"
#include "css/utils.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/mouse.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "desktop/plotters.h"
#include "desktop/save_complete.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "render/html.h"
#include "utils/url.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#import "css/utils.h"

#ifndef NETSURF_HOMEPAGE
#define NETSURF_HOMEPAGE "http://www.netsurf-browser.org/welcome/"
#endif

@implementation NetSurfApp

- (void) loadOptions;
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	[defaults registerDefaults: [NSDictionary dictionaryWithObjectsAndKeys: 
								 cocoa_get_user_path( @"Cookies" ), kCookiesFileOption,
								 cocoa_get_user_path( @"URLs" ), kURLsFileOption,
								 cocoa_get_user_path( @"Hotlist" ), kHotlistFileOption,
								 [NSString stringWithUTF8String: NETSURF_HOMEPAGE], kHomepageURLOption,
								 nil]];
	
	
	if (NULL == option_cookie_file) {
		option_cookie_file = strdup( [[defaults objectForKey: kCookiesFileOption] UTF8String] );
	}
	
	if (NULL == option_cookie_jar) {
		option_cookie_jar = strdup( option_cookie_file );
	}
	
	if (NULL == option_homepage_url) {
		option_homepage_url = strdup( [[defaults objectForKey: kHomepageURLOption] UTF8String] );
	}

	nscss_screen_dpi = FLTTOFIX( 72.0 * [[NSScreen mainScreen] userSpaceScaleFactor] );

	urldb_load( [[defaults objectForKey: kURLsFileOption] UTF8String] );
	urldb_load_cookies( option_cookie_file );
}

- (void) saveOptions;
{
	urldb_save_cookies( option_cookie_file );
	urldb_save( [[[NSUserDefaults standardUserDefaults] objectForKey: kURLsFileOption] UTF8String] );
}

- (void) run;
{
	[self finishLaunching];
	[self loadOptions];
	netsurf_main_loop();
	[self saveOptions];
}

-(void) terminate: (id)sender;
{
	netsurf_quit = true;
	[self postEvent: [NSEvent otherEventWithType: NSApplicationDefined location: NSZeroPoint 
								   modifierFlags: 0 timestamp: 0 windowNumber: 0 context: NULL 
										 subtype: 0 data1: 0 data2: 0]  
			atStart: YES];
}

@end
