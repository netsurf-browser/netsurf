/*
 * Copyright 2011 Vincent Sanders <vince@netsurf-browser.org>
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

/** \file
 * System colour handling
 *
 */

#include "amiga/gui.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "desktop/gui.h"
#include "desktop/options.h"

#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/Picasso96API.h>
#include <intuition/gui.h>
#include <intuition/screens.h>

#define AMINS_SCROLLERPEN NUMDRIPENS

struct gui_system_colour_ctx {
	const char *name;
	int length;
	css_color colour;
	colour *option_colour;
	UWORD amiga_pen;
	lwc_string *lwcstr;
};

static struct gui_system_colour_ctx colour_list[] = { 
	{ 
		"ActiveBorder", 
		SLEN("ActiveBorder"), 
		0xff000000, 
		&option_sys_colour_ActiveBorder, 
		FILLPEN,
		NULL 
	}, { 
		"ActiveCaption", 
		SLEN("ActiveCaption"), 
		0xffdddddd, 
		&option_sys_colour_ActiveCaption, 
		FILLPEN,
		NULL 
	}, { 
		"AppWorkspace", 
		SLEN("AppWorkspace"), 
		0xffeeeeee, 
		&option_sys_colour_AppWorkspace, 
		BACKGROUNDPEN,
		NULL 
	}, { 
		"Background", 
		SLEN("Background"), 
		0xff0000aa, 
		&option_sys_colour_Background, 
		BACKGROUNDPEN,
		NULL 
	}, {
		"ButtonFace", 
		SLEN("ButtonFace"), 
		0xffaaaaaa, 
		&option_sys_colour_ButtonFace, 
		FOREGROUNDPEN,
		NULL 
	}, {
		"ButtonHighlight", 
		SLEN("ButtonHighlight"), 
		0xffdddddd, 
		&option_sys_colour_ButtonHighlight, 
		FORESHINEPEN,
		NULL
	}, {
		"ButtonShadow", 
		SLEN("ButtonShadow"), 
		0xffbbbbbb, 
		&option_sys_colour_ButtonShadow, 
		FORESHADOWPEN,
		NULL 
	}, {
		"ButtonText", 
		SLEN("ButtonText"), 
		0xff000000, 
		&option_sys_colour_ButtonText, 
		TEXTPEN,
		NULL 
	}, {
		"CaptionText", 
		SLEN("CaptionText"), 
		0xff000000, 
		&option_sys_colour_CaptionText, 
		FILLTEXTPEN,
		NULL 
	}, {
		"GrayText", 
		SLEN("GrayText"), 
		0xffcccccc, 
		&option_sys_colour_GrayText, 
		DISABLEDTEXTPEN,
		NULL 
	}, {
		"Highlight", 
		SLEN("Highlight"), 
		0xff0000ee, 
		&option_sys_colour_Highlight, 
		SELECTPEN,
		NULL 
	}, {
		"HighlightText", 
		SLEN("HighlightText"), 
		0xff000000, 
		&option_sys_colour_HighlightText, 
		SELECTTEXTPEN,
		NULL 
	}, {
		"InactiveBorder", 
		SLEN("InactiveBorder"), 
		0xffffffff, 
		&option_sys_colour_InactiveBorder, 
		INACTIVEFILLPEN,
		NULL 
	}, {
		"InactiveCaption", 
		SLEN("InactiveCaption"), 
		0xffffffff, 
		&option_sys_colour_InactiveCaption, 
		INACTIVEFILLPEN,
		NULL 
	}, {
		"InactiveCaptionText", 
		SLEN("InactiveCaptionText"), 
		0xffcccccc, 
		&option_sys_colour_InactiveCaptionText, 
		INACTIVEFILLTEXTPEN,
		NULL 
	}, {
		"InfoBackground", 
		SLEN("InfoBackground"), 
		0xffaaaaaa, 
		&option_sys_colour_InfoBackground, 
		BACKGROUNDPEN, /* This is wrong, HelpHint backgrounds are pale yellow but doesn't seem to be a DrawInfo pen defined for it. */
		NULL 
	}, {
		"InfoText", 
		SLEN("InfoText"), 
		0xff000000, 
		&option_sys_colour_InfoText, 
		TEXTPEN,
		NULL 
	}, {
		"Menu", 
		SLEN("Menu"), 
		0xffaaaaaa, 
		&option_sys_colour_Menu, 
		MENUBACKGROUNDPEN,
		NULL 
	}, {
		"MenuText", 
		SLEN("MenuText"), 
		0xff000000, 
		&option_sys_colour_MenuText, 
		MENUTEXTPEN,
		NULL 
	}, {
		"Scrollbar", 
		SLEN("Scrollbar"), 
		0xffaaaaaa, 
		&option_sys_colour_Scrollbar, 
		AMINS_SCROLLERPEN,
		NULL 
	}, {
		"ThreeDDarkShadow", 
		SLEN("ThreeDDarkShadow"), 
		0xff555555, 
		&option_sys_colour_ThreeDDarkShadow, 
		FORESHADOWPEN,
		NULL 
	}, {
		"ThreeDFace", 
		SLEN("ThreeDFace"), 
		0xffdddddd, 
		&option_sys_colour_ThreeDFace, 
		FOREGROUNDPEN,
		NULL 
	}, {
		"ThreeDHighlight", 
		SLEN("ThreeDHighlight"), 
		0xffaaaaaa, 
		&option_sys_colour_ThreeDHighlight, 
		FORESHINEPEN,
		NULL 
	}, {
		"ThreeDLightShadow", 
		SLEN("ThreeDLightShadow"), 
		0xff999999, 
		&option_sys_colour_ThreeDLightShadow, 
		HALFSHINEPEN,
		NULL 
	}, {
		"ThreeDShadow", 
		SLEN("ThreeDShadow"), 
		0xff777777, 
		&option_sys_colour_ThreeDShadow, 
		HALFSHADOWPEN,
		NULL 
	}, {
		"Window", 
		SLEN("Window"), 
		0xffaaaaaa, 
		&option_sys_colour_Window, 
		BACKGROUNDPEN,
		NULL 
	}, {
		"WindowFrame", 
		SLEN("WindowFrame"), 
		0xff000000, 
		&option_sys_colour_WindowFrame, 
		INACTIVEFILLPEN,
		NULL 
	}, {
		
		"WindowText", 
		SLEN("WindowText"), 
		0xff000000, 
		&option_sys_colour_WindowText, 
		INACTIVEFILLTEXTPEN,
		NULL 
	},

};

#define colour_list_len (sizeof(colour_list) / sizeof(struct gui_system_colour_ctx))

static struct gui_system_colour_ctx *gui_system_colour_pw = NULL;

extern colour scrollbar_widget_fg_colour;
extern colour scrollbar_widget_bg_colour;
extern colour scrollbar_widget_arrow_colour;

css_color ami_css_colour_from_pen(struct Screen *screen, UWORD pen);

UWORD ami_system_colour_scrollbar_fgpen(struct DrawInfo *drinfo)
{
	LONG scrollerfillpen = FALSE;

	GetGUIAttrs(NULL, drinfo, GUIA_PropKnobColor, &scrollerfillpen, TAG_DONE);

	if(scrollerfillpen) return FILLPEN;
		else return FOREGROUNDPEN;
}

void ami_system_colour_scrollbar_widget(void)
{
	if(scrn == NULL) return;

	scrollbar_widget_fg_colour = p96EncodeColor(RGBFB_A8B8G8R8,
			ami_css_colour_from_pen(scrn, AMINS_SCROLLERPEN));
	scrollbar_widget_bg_colour = p96EncodeColor(RGBFB_A8B8G8R8,
			ami_css_colour_from_pen(scrn, FILLSHADOWPEN));
	scrollbar_widget_arrow_colour = p96EncodeColor(RGBFB_A8B8G8R8,
			ami_css_colour_from_pen(scrn, SHINEPEN));
}

bool gui_system_colour_init(void)
{
	unsigned int ccount;

	ami_system_colour_scrollbar_widget();

	if (gui_system_colour_pw != NULL) 
		return false;

	/* Intern colour strings */
	for (ccount = 0; ccount < colour_list_len; ccount++) {
		if (lwc_intern_string(colour_list[ccount].name, 
				      colour_list[ccount].length, 
				      &(colour_list[ccount].lwcstr)) != lwc_error_ok) {
			return false;
		}
	}

	/* pull in options if set (ie not transparent) */
	for (ccount = 0; ccount < colour_list_len; ccount++) {
		if (*(colour_list[ccount].option_colour) != 0) {
			colour_list[ccount].colour = *(colour_list[ccount].option_colour);
		}
		else if(scrn) {
			colour_list[ccount].colour =
				ami_css_colour_from_pen(scrn, colour_list[ccount].amiga_pen);
		}
	}

	gui_system_colour_pw = colour_list;
	
	return true;
}

void gui_system_colour_finalize(void)
{
	unsigned int ccount;

	for (ccount = 0; ccount < colour_list_len; ccount++) {
		lwc_string_unref(colour_list[ccount].lwcstr);
	}

	gui_system_colour_pw = NULL;
}

colour gui_system_colour_char(char *name)
{
	colour ret = 0xff00000;
	unsigned int ccount;

	for (ccount = 0; ccount < colour_list_len; ccount++) {
		if (strncasecmp(name, 
				colour_list[ccount].name, 
				colour_list[ccount].length) == 0) {
			ret = colour_list[ccount].colour;
			break;
		}
	}
	return ret;
}

css_error gui_system_colour(void *pw, lwc_string *name, css_color *colour)
{
	unsigned int ccount;
	bool match;

	for (ccount = 0; ccount < colour_list_len; ccount++) {
		if (lwc_string_caseless_isequal(name, 
				colour_list[ccount].lwcstr,
				&match) == lwc_error_ok && match) {
			*colour = colour_list[ccount].colour;
			return CSS_OK;
		}
	}	

	return CSS_INVALID;
}

css_color ami_css_colour_from_pen(struct Screen *screen, UWORD pen)
{
	css_color css_colour = 0x00000000;
	ULONG colour[3];
	struct DrawInfo *drinfo = GetScreenDrawInfo(screen);

	if(drinfo == NULL) return 0x00000000;

	if(pen == AMINS_SCROLLERPEN) pen = ami_system_colour_scrollbar_fgpen(drinfo);

	/* Get the colour of the pen being used for "pen" */
	GetRGB32(screen->ViewPort.ColorMap, drinfo->dri_Pens[pen], 1, (ULONG *)&colour);

	/* convert it to a css_color */
	css_colour = (0xff << 24) |
				((colour[0] & 0xff000000) >> 8) |
				((colour[1] & 0xff000000) >> 16) |
				((colour[2] & 0xff000000) >> 24);

	FreeScreenDrawInfo(screen, drinfo);
	return css_colour;
}
