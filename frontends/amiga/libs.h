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

#ifndef AMIGA_LIBS_H
#define AMIGA_LIBS_H

#include <stdbool.h>
#include <intuition/classes.h>

/* BOOPSI classes */
#ifndef PROTO_AREXX_H
extern Class *ARexxClass;
#endif
#ifndef PROTO_BEVEL_H
extern Class *BevelClass;
#endif
#ifndef PROTO_BITMAP_H
extern Class *BitMapClass;
#endif
#ifndef PROTO_BUTTON_H
extern Class *ButtonClass;
#endif
#ifndef PROTO_CHECKBOX_H
extern Class *CheckBoxClass;
#endif
#ifndef PROTO_CHOOSER_H
extern Class *ChooserClass;
#endif
#ifndef PROTO_CLICKTAB_H
extern Class *ClickTabClass;
#endif
#ifndef PROTO_FUELGAUGE_H
extern Class *FuelGaugeClass;
#endif
#ifndef PROTO_GETFILE_H
extern Class *GetFileClass;
#endif
#ifndef PROTO_GETFONT_H
extern Class *GetFontClass;
#endif
#ifndef PROTO_GETSCREENMODE_H
extern Class *GetScreenModeClass;
#endif
#ifndef PROTO_INTEGER_H
extern Class *IntegerClass;
#endif
extern Class *LabelClass;
#ifndef PROTO_LAYOUT_H
extern Class *LayoutClass;
#endif
#ifndef PROTO_LISTBROWSER_H
extern Class *ListBrowserClass;
#endif
#ifndef __amigaos4__
/* OS4 uses a public class name instead */
extern Class *PageClass;
#endif
#ifndef PROTO_RADIOBUTTON_H
extern Class *RadioButtonClass;
#endif
#ifndef PROTO_SCROLLER_H
extern Class *ScrollerClass;
#endif
#ifndef PROTO_SPACE_H
extern Class *SpaceClass;
#endif
extern Class *SpeedBarClass;
#ifndef PROTO_STRING_H
extern Class *StringClass;
#endif
#ifndef PROTO_WINDOW_H
extern Class *WindowClass;
#endif

/* New improved ReAction macros! */
#define ARexxObj			NewObject(ARexxClass, NULL
#define BevelObj			NewObject(BevelClass, NULL
#define BitMapObj			NewObject(BitMapClass, NULL
#define ButtonObj			NewObject(ButtonClass, NULL
#define CheckBoxObj			NewObject(CheckBoxClass, NULL
#define ChooserObj			NewObject(ChooserClass, NULL
#define ClickTabObj			NewObject(ClickTabClass, NULL
#define FuelGaugeObj		NewObject(FuelGaugeClass, NULL
#define GetFileObj			NewObject(GetFileClass, NULL
#define GetFontObj			NewObject(GetFontClass, NULL
#define GetScreenModeObj	NewObject(GetScreenModeClass, NULL
#define IntegerObj			NewObject(IntegerClass, NULL
#define LabelObj			NewObject(LabelClass, NULL
#define LayoutHObj			NewObject(LayoutClass, NULL, LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ
#define LayoutVObj			NewObject(LayoutClass, NULL, LAYOUT_Orientation, LAYOUT_ORIENT_VERT
#define ListBrowserObj			NewObject(ListBrowserClass, NULL
#ifdef __amigaos4__
#define PageObj				NewObject(NULL, "page.gadget"
#else
#define PageObj				NewObject(PageClass, NULL
#endif
#define RadioButtonObj		NewObject(RadioButtonClass, NULL
#define ScrollerObj			NewObject(ScrollerClass, NULL
#define SpaceObj			NewObject(SpaceClass, NULL
#define SpeedBarObj			NewObject(SpeedBarClass, NULL
#define StringObj			NewObject(StringClass, NULL
#define WindowObj			NewObject(WindowClass, NULL

/* Functions */
bool ami_libs_open(void);
void ami_libs_close(void);
#endif

