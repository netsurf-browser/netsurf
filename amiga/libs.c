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
#include "utils/utils.h"

#include <proto/exec.h>

#define AMINS_LIB_OPEN(LIB, LIBVER, PREFIX, INTERFACE, INTVER)	\
	if((PREFIX##Base = OpenLibrary(LIB, LIBVER))) {	\
		I##PREFIX = (struct PREFIX##IFace *)GetInterface(PREFIX##Base, INTERFACE, INTVER, NULL);	\
	} else {	\
		warn_user("CompError", LIB);	\
	}

#define AMINS_LIB_CLOSE(PREFIX)	\
	if(I##PREFIX) DropInterface((struct Interface *)I##PREFIX);	\
	if(PREFIX##Base) CloseLibrary(PREFIX##Base);

#define AMINS_LIB_STRUCT(PREFIX)	\
	struct Library *PREFIX##Base;	\
	struct PREFIX##IFace *I##PREFIX;

AMINS_LIB_STRUCT(Application);
AMINS_LIB_STRUCT(Asl);
AMINS_LIB_STRUCT(Diskfont);
AMINS_LIB_STRUCT(Graphics);
AMINS_LIB_STRUCT(Intuition);
AMINS_LIB_STRUCT(Keymap);
AMINS_LIB_STRUCT(P96);

void ami_libs_open(void)
{
	AMINS_LIB_OPEN("application.library", 53, Application, "application", 2)
	AMINS_LIB_OPEN("asl.library", 37, Asl, "main", 1)
	AMINS_LIB_OPEN("diskfont.library", 50, Diskfont, "main", 1)
	AMINS_LIB_OPEN("graphics.library", 50, Graphics, "main", 1)
	AMINS_LIB_OPEN("intuition.library", 37, Intuition, "main", 1)
	AMINS_LIB_OPEN("keymap.library", 37, Keymap, "main", 1)
	AMINS_LIB_OPEN("Picasso96API.library", 0, P96, "main", 1)
}

void ami_libs_close(void)
{
	AMINS_LIB_CLOSE(Application)
	AMINS_LIB_CLOSE(Asl)
	AMINS_LIB_CLOSE(Diskfont)
	AMINS_LIB_CLOSE(Intuition)
	AMINS_LIB_CLOSE(Graphics)
	AMINS_LIB_CLOSE(Keymap)
	AMINS_LIB_CLOSE(P96)
}

