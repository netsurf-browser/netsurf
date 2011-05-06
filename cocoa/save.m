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

#import "desktop/save_complete.h"

#define UNIMPL() NSLog( @"Function '%s' unimplemented", __func__ )

bool save_complete_gui_save(const char *path, const char *filename,
							size_t len, const char *sourcedata, lwc_string *mime_type)
{
	UNIMPL();
	return false;
}

int save_complete_htmlSaveFileFormat(const char *path, const char *filename,
									 xmlDocPtr cur, const char *encoding, int format)
{
	UNIMPL();
	return 0;
}
