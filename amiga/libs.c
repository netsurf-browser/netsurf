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

#include "amiga/os3support.h"

#include "amiga/libs.h"
#include "amiga/misc.h"
#include "utils/utils.h"
#include "utils/log.h"

#include <proto/exec.h>
#include <proto/utility.h>

#ifdef __amigaos4__
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
#else
#define AMINS_LIB_OPEN(LIB, LIBVER, PREFIX, INTERFACE, INTVER, FAIL)	\
	LOG(("Opening %s v%d", LIB, LIBVER)); \
	if((PREFIX##Base = OpenLibrary(LIB, LIBVER))) {	\
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
	if(PREFIX##Base) CloseLibrary(PREFIX##Base);

#define AMINS_LIB_STRUCT(PREFIX)	\
	struct Library *PREFIX##Base;
#endif

#define GraphicsBase GfxBase /* graphics.library is a bit weird */

#ifdef __amigaos4__
AMINS_LIB_STRUCT(Application);
#else
struct UtilityBase *UtilityBase; /* AMINS_LIB_STRUCT(Utility) */
#endif
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
#ifdef __amigaos4__
	/* Libraries only needed on OS4 */
	AMINS_LIB_OPEN("application.library",  53, Application, "application", 2, false)
#else
	/* Libraries we get automatically on OS4 but not OS3 */
	AMINS_LIB_OPEN("utility.library",      37, Utility,     "main",        1, true)
#endif
	/* Standard libraries for both versions */
	AMINS_LIB_OPEN("asl.library",          37, Asl,         "main",        1, true)
	AMINS_LIB_OPEN("datatypes.library",    37, DataTypes,   "main",        1, true)
	AMINS_LIB_OPEN("diskfont.library",     40, Diskfont,    "main",        1, true)
	AMINS_LIB_OPEN("gadtools.library",     37, GadTools,    "main",        1, true)
	AMINS_LIB_OPEN("graphics.library",     40, Graphics,    "main",        1, true)
	AMINS_LIB_OPEN("icon.library",         44, Icon,        "main",        1, true)
	AMINS_LIB_OPEN("iffparse.library",     37, IFFParse,    "main",        1, true)
	AMINS_LIB_OPEN("intuition.library",    40, Intuition,   "main",        1, true)
	AMINS_LIB_OPEN("keymap.library",       37, Keymap,      "main",        1, true)
	AMINS_LIB_OPEN("layers.library",       37, Layers,      "main",        1, true)
	AMINS_LIB_OPEN("locale.library",       37, Locale,      "main",        1, true)
	AMINS_LIB_OPEN("Picasso96API.library",  0, P96,         "main",        1, true)
	AMINS_LIB_OPEN("workbench.library",    37, Workbench,   "main",        1, true)

	/* NB: timer.device is opened in schedule.c (ultimately by the scheduler process).
	 * The library base and interface are obtained there, rather than here, due to
	 * the additional complexities of opening devices, which aren't important here
	 * (as we only need the library interface), but are important for the scheduler
	 * (as it also uses the device interface).  We trust that the scheduler has
	 * initialised before any other code requires the timer's library interface
	 * (this is ensured by waiting for the scheduler to start up) and that it is
	 * OK to use a child process' timer interface, to avoid opening it twice.
	 */

	/* BOOPSI classes.
	 * \todo These should be opened using OpenClass(), however as
	 * the macros all use the deprecated _GetClass() functions,
	 * we may as well just open them normally for now.
	 */

	AMINS_LIB_OPEN("classes/arexx.class",          44, ARexx,         "main", 1, true)
	AMINS_LIB_OPEN("images/bevel.image",           44, Bevel,         "main", 1, true)
	AMINS_LIB_OPEN("images/bitmap.image",          44, BitMap,        "main", 1, true)
	AMINS_LIB_OPEN("gadgets/checkbox.gadget",      44, CheckBox,      "main", 1, true)
	AMINS_LIB_OPEN("gadgets/chooser.gadget",       44, Chooser,       "main", 1, true)
	AMINS_LIB_OPEN("gadgets/clicktab.gadget",      44, ClickTab,      "main", 1, true)
	AMINS_LIB_OPEN("gadgets/fuelgauge.gadget",     44, FuelGauge,     "main", 1, true)
	AMINS_LIB_OPEN("gadgets/getfile.gadget",       44, GetFile,       "main", 1, true)
	AMINS_LIB_OPEN("gadgets/getfont.gadget",       44, GetFont,       "main", 1, true)
	AMINS_LIB_OPEN("gadgets/getscreenmode.gadget", 44, GetScreenMode, "main", 1, true)
	AMINS_LIB_OPEN("gadgets/integer.gadget",       44, Integer,       "main", 1, true)
	AMINS_LIB_OPEN("images/label.image",           44, Label,         "main", 1, true)
	AMINS_LIB_OPEN("gadgets/layout.gadget",        44, Layout,        "main", 1, true)
	AMINS_LIB_OPEN("gadgets/listbrowser.gadget",   44, ListBrowser,   "main", 1, true)
	AMINS_LIB_OPEN("gadgets/radiobutton.gadget",   44, RadioButton,   "main", 1, true)
	AMINS_LIB_OPEN("gadgets/scroller.gadget",      44, Scroller,      "main", 1, true)
	AMINS_LIB_OPEN("gadgets/space.gadget",         44, Space,         "main", 1, true)
	AMINS_LIB_OPEN("gadgets/speedbar.gadget",      44, SpeedBar,      "main", 1, true)
	AMINS_LIB_OPEN("gadgets/string.gadget",        44, String,        "main", 1, true)
	AMINS_LIB_OPEN("classes/window.class",         44, Window,        "main", 1, true)

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
#ifdef __amigaos4__
	AMINS_LIB_CLOSE(Application)
#else
	AMINS_LIB_CLOSE(Utility)
#endif
}

