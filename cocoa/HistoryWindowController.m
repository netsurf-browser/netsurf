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

#import "cocoa/HistoryWindowController.h"
#import "cocoa/Tree.h"
#import "cocoa/TreeView.h"

#import "desktop/history_global_core.h"

@implementation HistoryWindowController

@synthesize view;

- init;
{
	if ((self = [super initWithWindowNibName: @"HistoryWindow"]) == nil) return nil;
	
	tree = [[Tree alloc] initWithFlags: history_global_get_tree_flags()];
	history_global_initialise( [tree tree], "directory.png" );
	
	return self;
}

- (void) dealloc;
{
	history_global_cleanup();
	[tree release];
	[self setView: nil];
	
	[super dealloc];
}

- (void)awakeFromNib;
{
	[view setTree: tree];
	[[self window] setExcludedFromWindowsMenu: YES];
}

@end
