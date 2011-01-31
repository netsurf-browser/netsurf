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
#import "cocoa/plotter.h"

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

static NSString *cocoa_get_user_path( NSString *fileName ) ;

@implementation NetSurfApp

@synthesize frontTab;

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

	urldb_load( [[defaults objectForKey: kURLsFileOption] UTF8String] );
	urldb_load_cookies( option_cookie_file );
	
	cocoa_update_scale_factor();
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

#pragma mark -

static char *cocoa_get_resource_url( NSString *name, NSString *type )
{
	NSString *path = [[NSBundle mainBundle] pathForResource: name ofType: type];
	return strdup( [[[NSURL fileURLWithPath: path] absoluteString] UTF8String] );
}

static NSString *cocoa_get_preferences_path( void )
{
	NSArray *paths = NSSearchPathForDirectoriesInDomains( NSApplicationSupportDirectory, NSUserDomainMask, YES );
	NSCAssert( [paths count] >= 1, @"Where is the application support directory?" );
	
	NSString *netsurfPath = [[paths objectAtIndex: 0] stringByAppendingPathComponent: @"NetSurf"];
	
	NSFileManager *fm = [NSFileManager defaultManager];
	BOOL isDirectory = NO;
	BOOL exists = [fm fileExistsAtPath: netsurfPath isDirectory: &isDirectory];
	
	if (!exists) {
		exists = [fm createDirectoryAtPath: netsurfPath attributes: nil];
		isDirectory = YES;
	}
	if (!(exists && isDirectory)) {
		die( "Cannot create netsurf preferences directory" );
	}
	
	return netsurfPath;
}

static NSString *cocoa_get_user_path( NSString *fileName ) 
{
	return [cocoa_get_preferences_path() stringByAppendingPathComponent: fileName];
}

static const char *cocoa_get_options_file( void )
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	[defaults registerDefaults: [NSDictionary dictionaryWithObjectsAndKeys: 
								 cocoa_get_user_path( @"Options" ), kOptionsFileOption,
								 nil]];
	
	return [[defaults objectForKey: kOptionsFileOption] UTF8String];
}

static NSApplication *cocoa_prepare_app( void )
{
	if (NSApp != nil) return NSApp;
	
	NSDictionary *infoDictionary = [[NSBundle mainBundle] infoDictionary];
	Class principalClass =  NSClassFromString([infoDictionary objectForKey:@"NSPrincipalClass"]);
	NSCAssert([principalClass respondsToSelector:@selector(sharedApplication)], @"Principal class must implement sharedApplication.");
	[principalClass sharedApplication];
	
	NSString *mainNibName = [infoDictionary objectForKey:@"NSMainNibFile"];
	NSNib *mainNib = [[NSNib alloc] initWithNibNamed:mainNibName bundle:[NSBundle mainBundle]];
	[mainNib instantiateNibWithOwner:NSApp topLevelObjects:nil];
	[mainNib release];
	
	return NSApp;
}

void cocoa_autorelease( void )
{
	static NSAutoreleasePool *pool = nil;
	[pool release];
	pool = [[NSAutoreleasePool alloc] init];
}

int main( int argc, char **argv )
{
	cocoa_autorelease();
	
	default_stylesheet_url = cocoa_get_resource_url( @"default", @"css" );
	quirks_stylesheet_url = cocoa_get_resource_url( @"quirks", @"css" );
	adblock_stylesheet_url = cocoa_get_resource_url( @"adblock", @"css" );
	
	const char * const messages = [[[NSBundle mainBundle] pathForResource: @"Messages" ofType: @""] UTF8String];
	const char * const options = cocoa_get_options_file();
	
	netsurf_init(&argc, &argv, options, messages);
	
    [cocoa_prepare_app() run];
	
	netsurf_exit();
	
	return 0;
}
