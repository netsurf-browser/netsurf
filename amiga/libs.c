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
#include "amiga/misc.h"

#include <proto/exec.h>

#define AMINS_OPEN_LIB(LIB, LIBVER, SUFFIX, INTERFACE, INTVER)	\
	if((SUFFIX##Base = OpenLibrary(LIB, LIBVER))) {	\
		I##SUFFIX = (struct SUFFIX##IFace *)GetInterface(SUFFIX##Base, INTERFACE, INTVER, NULL);	\
	} else {	\
		warn_user("CompError", LIB);	\
	}

#define AMINS_CLOSE_LIB(SUFFIX)	\
	if(I##SUFFIX) DropInterface((struct Interface *)I##SUFFIX);	\
	if(SUFFIX##Base) CloseLibrary(SUFFIX##Base);

void ami_libs_open(void)
{
	AMINS_OPEN_LIB("keymap.library", 37, Keymap, "main", 1)
	AMINS_OPEN_LIB("application.library", 53, Application, "application", 2)
}

void ami_libs_close(void)
{
	AMINS_CLOSE_LIB(Application)
	AMINS_CLOSE_LIB(Keymap)
}

