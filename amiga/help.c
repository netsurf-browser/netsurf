/*
 * Copyright 2013 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/help.h"

/* AmigaGuide class */
#include "amiga/agclass/amigaguide_class.h"

Class *AmigaGuideClass = NULL;
Object *AmigaGuideObject = NULL;

/* This array needs to match the enum in help.h */
STRPTR context_nodes[] = {
	"Main",
	"GUI",
	"Prefs",
	NULL
};

void ami_help_init(struct Screen *screen)
{
	AmigaGuideClass = initAGClass();

	AmigaGuideObject = NewObject(AmigaGuideClass, NULL,
		AMIGAGUIDE_Name, "PROGDIR:NetSurf.guide",
		AMIGAGUIDE_BaseName, "NetSurf",
		AMIGAGUIDE_Screen, screen,
		AMIGAGUIDE_ContextArray, context_nodes,
		AMIGAGUIDE_ContextID, AMI_HELP_MAIN,
		TAG_DONE);
}

void ami_help_open(ULONG node)
{
	SetAttrs(AmigaGuideObject, AMIGAGUIDE_ContextID, node, TAG_DONE);
	IDoMethod(AmigaGuideObject, AGM_OPEN, NULL);
}

void ami_help_free(void)
{
	if (AmigaGuideObject) DisposeObject(AmigaGuideObject);
	if (AmigaGuideClass) freeAGClass(AmigaGuideClass);
	
	AmigaGuideObject = NULL;
	AmigaGuideClass = NULL;
}

void ami_help_new_screen(struct Screen *screen)
{
	SetAttrs(AmigaGuideObject, AMIGAGUIDE_Screen, screen, TAG_DONE);
}
