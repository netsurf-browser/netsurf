/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
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


#include "desktop/save_complete.h"

/**
* conducts the filesystem save appropriate to the gui
* \param path save path
* \param filename name of file to save
* \param len data length
* \param sourcedata pointer to data to save, NULL when all data in c
* \param type content type
* \return true for success
*/

bool save_complete_gui_save(const char *path, const char *filename, size_t len,
		const char *sourcedata, content_type type)
{
	return false;
}

/**
* wrapper for lib function htmlSaveFileFormat; front sets path from 
* path + filename in a filesystem-specific way
*/

int save_complete_htmlSaveFileFormat(const char *path, const char *filename, 
		xmlDocPtr cur, const char *encoding, int format)
{
	return -1;
}

