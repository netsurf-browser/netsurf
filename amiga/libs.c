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
#include "utils/utils.h"
#include "utils/log.h"

#include <proto/exec.h>
#include <proto/utility.h>

#define AMINS_LIB_OPEN(LIB, LIBVER, PREFIX, INTERFACE, INTVER, FAIL)	\
	LOG(("Opening %s v%d", LIB, LIBVER)); \
	if((PREFIX##Base = OpenLibrary(LIB, LIBVER))) {	\
		I##PREFIX = (struct PREFIX##IFace *)GetInterface(PREFIX##Base, INTERFACE, INTVER, NULL);	\
		if(I##PREFIX == NULL) {	\
			LOG(("Failed to get %s interface v%d of %s", INTERFACE, INTVER, LIB));	\
		}	\
	} else {	\
		LOG(("Failed to open %s v%d", LIB, LIBVER));	\
		if(FAIL == true) {	\
			STRPTR error = ASPrintf("Unable to open %s v%d", LIB, LIBVER);	\
			ami_misc_fatal_error(error);	\
			FreeVec(error);	\
			return false;	\
		}	\
	}

#define AMINS_LIB_CLOSE(PREFIX)	\
	if(I##PREFIX) DropInterface((struct Interface *)I##PREFIX);	\
	if(PREFIX##Base) CloseLibrary(PREFIX##Base);

#define AMINS_LIB_STRUCT(PREFIX)	\
	struct Library *PREFIX##Base;	\
	struct PREFIX##IFace *I##PREFIX;

#define GraphicsBase GfxBase /* graphics.library is a bit weird */

AMINS_LIB_STRUCT(Application);
AMINS_LIB_STRUCT(Asl);
AMINS_LIB_STRUCT(DataTypes);
AMINS_LIB_STRUCT(Diskfont);
AMINS_LIB_STRUCT(Graphics);
AMINS_LIB_STRUCT(GadTools);
AMINS_LIB_STRUCT(Icon);
AMINS_LIB_STRUCT(IFFParse);
AMINS_LIB_STRUCT(Intuition);
AMINS_LIB_STRUCT(Keymap);
AMINS_LIB_STRUCT(Layers);
AMINS_LIB_STRUCT(Locale);
AMINS_LIB_STRUCT(P96);
AMINS_LIB_STRUCT(Workbench);

AMINS_LIB_STRUCT(ARexx);
AMINS_LIB_STRUCT(Bevel);
AMINS_LIB_STRUCT(BitMap);
AMINS_LIB_STRUCT(Chooser);
AMINS_LIB_STRUCT(CheckBox);
AMINS_LIB_STRUCT(ClickTab);
AMINS_LIB_STRUCT(FuelGauge);
AMINS_LIB_STRUCT(GetFile);
AMINS_LIB_STRUCT(GetFont);
AMINS_LIB_STRUCT(GetScreenMode);
AMINS_LIB_STRUCT(Integer);
AMINS_LIB_STRUCT(Label);
AMINS_LIB_STRUCT(Layout);
AMINS_LIB_STRUCT(ListBrowser);
AMINS_LIB_STRUCT(RadioButton);
AMINS_LIB_STRUCT(Scroller);
AMINS_LIB_STRUCT(Space);
AMINS_LIB_STRUCT(SpeedBar);
AMINS_LIB_STRUCT(String);
AMINS_LIB_STRUCT(Window);


bool ami_libs_open(void)
{
	AMINS_LIB_OPEN("application.library",  53, Application, "application", 2, false)
	AMINS_LIB_OPEN("asl.library",          37, Asl,         "main",        1, true)
	AMINS_LIB_OPEN("datatypes.library",    37, DataTypes,   "main",        1, true)
	AMINS_LIB_OPEN("diskfont.library",     50, Diskfont,    "main",        1, true)
	AMINS_LIB_OPEN("gadtools.library",     37, GadTools,    "main",        1, true)
	AMINS_LIB_OPEN("graphics.library",     50, Graphics,    "main",        1, true)
	AMINS_LIB_OPEN("icon.library",         50, Icon,        "main",        1, true)
	AMINS_LIB_OPEN("iffparse.library",     37, IFFParse,    "main",        1, true)
	AMINS_LIB_OPEN("intuition.library",    37, Intuition,   "main",        1, true)
	AMINS_LIB_OPEN("keymap.library",       37, Keymap,      "main",        1, true)
	AMINS_LIB_OPEN("layers.library",       37, Layers,      "main",        1, true)
	AMINS_LIB_OPEN("locale.library",       37, Locale,      "main",        1, true)
	AMINS_LIB_OPEN("Picasso96API.library",  0, P96,         "main",        1, true)
	AMINS_LIB_OPEN("workbench.library",    37, Workbench,   "main",        1, true)

	/* BOOPSI classes.
	 * \todo These should be opened using OpenClass(), however as
	 * the macros all use the deprecated _GetClass() functions,
	 * we may as well just open them normally for now. */

	AMINS_LIB_OPEN("classes/arexx.class",          50, ARexx,         "main", 1, true)
	AMINS_LIB_OPEN("images/bevel.image",           50, Bevel,         "main", 1, true)
	AMINS_LIB_OPEN("images/bitmap.image",          50, BitMap,        "main", 1, true)
	AMINS_LIB_OPEN("gadgets/checkbox.gadget",      50, CheckBox,      "main", 1, true)
	AMINS_LIB_OPEN("gadgets/chooser.gadget",       50, Chooser,       "main", 1, true)
	AMINS_LIB_OPEN("gadgets/clicktab.gadget",      50, ClickTab,      "main", 1, true)
	AMINS_LIB_OPEN("gadgets/fuelgauge.gadget",     50, FuelGauge,     "main", 1, true)
	AMINS_LIB_OPEN("gadgets/getfile.gadget",       50, GetFile,       "main", 1, true)
	AMINS_LIB_OPEN("gadgets/getfont.gadget",       50, GetFont,       "main", 1, true)
	AMINS_LIB_OPEN("gadgets/getscreenmode.gadget", 50, GetScreenMode, "main", 1, true)
	AMINS_LIB_OPEN("gadgets/integer.gadget",       50, Integer,       "main", 1, true)
	AMINS_LIB_OPEN("images/label.image",           50, Label,         "main", 1, true)
	AMINS_LIB_OPEN("gadgets/layout.gadget",        50, Layout,        "main", 1, true)
	AMINS_LIB_OPEN("gadgets/listbrowser.gadget",   50, ListBrowser,   "main", 1, true)
	AMINS_LIB_OPEN("gadgets/radiobutton.gadget",   50, RadioButton,   "main", 1, true)
	AMINS_LIB_OPEN("gadgets/scroller.gadget",      50, Scroller,      "main", 1, true)
	AMINS_LIB_OPEN("gadgets/space.gadget",         50, Space,         "main", 1, true)
	AMINS_LIB_OPEN("gadgets/speedbar.gadget",      50, SpeedBar,      "main", 1, true)
	AMINS_LIB_OPEN("gadgets/string.gadget",        50, String,        "main", 1, true)
	AMINS_LIB_OPEN("classes/window.class",         50, Window,        "main", 1, true)

	return true;
}

void ami_libs_close(void)
{
	AMINS_LIB_CLOSE(ARexx)
	AMINS_LIB_CLOSE(Bevel)
	AMINS_LIB_CLOSE(BitMap)
	AMINS_LIB_CLOSE(CheckBox)
	AMINS_LIB_CLOSE(Chooser)
	AMINS_LIB_CLOSE(ClickTab)
	AMINS_LIB_CLOSE(FuelGauge)
	AMINS_LIB_CLOSE(GetFile)
	AMINS_LIB_CLOSE(GetFont)
	AMINS_LIB_CLOSE(GetScreenMode)
	AMINS_LIB_CLOSE(Integer)
	AMINS_LIB_CLOSE(Label)
	AMINS_LIB_CLOSE(Layout)
	AMINS_LIB_CLOSE(ListBrowser)
	AMINS_LIB_CLOSE(RadioButton)
	AMINS_LIB_CLOSE(Scroller)
	AMINS_LIB_CLOSE(Space)
	AMINS_LIB_CLOSE(SpeedBar)
	AMINS_LIB_CLOSE(String)
	AMINS_LIB_CLOSE(Window)

	AMINS_LIB_CLOSE(Application)
	AMINS_LIB_CLOSE(Asl)
	AMINS_LIB_CLOSE(DataTypes)
	AMINS_LIB_CLOSE(Diskfont)
	AMINS_LIB_CLOSE(GadTools)
	AMINS_LIB_CLOSE(Graphics)
	AMINS_LIB_CLOSE(Icon)
	AMINS_LIB_CLOSE(IFFParse)
	AMINS_LIB_CLOSE(Intuition)
	AMINS_LIB_CLOSE(Keymap)
	AMINS_LIB_CLOSE(Layers)
	AMINS_LIB_CLOSE(Locale)
	AMINS_LIB_CLOSE(P96)
	AMINS_LIB_CLOSE(Workbench)
}

