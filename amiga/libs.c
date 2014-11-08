/*
 * Copyright 2014 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/libs.h"

#include <proto/exec.h>

void ami_libs_open(void)
{
	if((KeymapBase = OpenLibrary("keymap.library",37))) {
		IKeymap = (struct KeymapIFace *)GetInterface(KeymapBase, "main", 1, NULL);
	}

	if((ApplicationBase = OpenLibrary("application.library", 53))) {
		IApplication = (struct ApplicationIFace *)GetInterface(ApplicationBase, "application", 2, NULL);
	}
}

void ami_libs_close(void)
{

	if(IApplication) DropInterface((struct Interface *)IApplication);
	if(ApplicationBase) CloseLibrary(ApplicationBase);

	if(IKeymap) DropInterface((struct Interface *)IKeymap);
	if(KeymapBase) CloseLibrary(KeymapBase);
}

