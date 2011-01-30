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

#define __STDBOOL_H__	1
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>

#include <InterfaceDefs.h>
#include <Screen.h>

extern "C" {

#include "utils/utils.h"
#include "utils/log.h"
#include "desktop/gui.h"
#include "desktop/options.h"
#include "desktop/plot_style.h"

}

#include "beos/beos_gui.h"


#if !defined(__HAIKU__) && !defined(B_BEOS_VERSION_DANO)
/* more ui_colors, R5 only had a few defined... */
#define B_PANEL_TEXT_COLOR ((color_which)10)
#define B_DOCUMENT_BACKGROUND_COLOR ((color_which)11)
#define B_DOCUMENT_TEXT_COLOR ((color_which)12)
#define B_CONTROL_BACKGROUND_COLOR ((color_which)13)
#define B_CONTROL_TEXT_COLOR ((color_which)14)
#define B_CONTROL_BORDER_COLOR ((color_which)15)
#define B_CONTROL_HIGHLIGHT_COLOR ((color_which)16)
#define B_NAVIGATION_BASE_COLOR ((color_which)4)
#define B_NAVIGATION_PULSE_COLOR ((color_which)17)
#define B_SHINE_COLOR ((color_which)18)
#define B_SHADOW_COLOR ((color_which)19)
#define B_MENU_SELECTED_BORDER_COLOR ((color_which)9)
#define B_TOOLTIP_BACKGROUND_COLOR ((color_which)20)
#define B_TOOLTIP_TEXT_COLOR ((color_which)21)
#define B_SUCCESS_COLOR ((color_which)100)
#define B_FAILURE_COLOR ((color_which)101)
#define B_MENU_SELECTED_BACKGROUND_COLOR B_MENU_SELECTION_BACKGROUND_COLOR
#define B_RANDOM_COLOR ((color_which)0x80000000)
#define B_MICHELANGELO_FAVORITE_COLOR ((color_which)0x80000001)
#define B_DSANDLER_FAVORITE_SKY_COLOR ((color_which)0x80000002)
#define B_DSANDLER_FAVORITE_INK_COLOR ((color_which)0x80000003)
#define B_DSANDLER_FAVORITE_SHOES_COLOR ((color_which)0x80000004)
#define B_DAVE_BROWN_FAVORITE_COLOR ((color_which)0x80000005)
#endif
#define NOCOL ((color_which)0)


struct gui_system_colour_ctx {
	const char *name;
	int length;
	css_color css_colour;
	colour *option_colour;
	lwc_string *lwcstr;
	color_which ui;
};

static struct gui_system_colour_ctx colour_list[] = { 
	{ 
		"ActiveBorder", 
		SLEN("ActiveBorder"), 
		0xff000000, 
		&option_sys_colour_ActiveBorder, 
		NULL, 
		NOCOL
	}, { 
		"ActiveCaption", 
		SLEN("ActiveCaption"), 
		0xffdddddd, 
		&option_sys_colour_ActiveCaption, 
		NULL, 
		B_WINDOW_TAB_COLOR
	}, { 
		"AppWorkspace", 
		SLEN("AppWorkspace"), 
		0xffeeeeee, 
		&option_sys_colour_AppWorkspace, 
		NULL, 
		B_PANEL_BACKGROUND_COLOR
	}, { 
		"Background", 
		SLEN("Background"), 
		0xff0000aa, 
		&option_sys_colour_Background, 
		NULL, 
		B_DESKTOP_COLOR
	}, {
		"ButtonFace", 
		SLEN("ButtonFace"), 
		0xffaaaaaa, 
		&option_sys_colour_ButtonFace, 
		NULL, 
		B_CONTROL_BACKGROUND_COLOR
	}, {
		"ButtonHighlight", 
		SLEN("ButtonHighlight"), 
		0xffdddddd, 
		&option_sys_colour_ButtonHighlight, 
		NULL, 
		B_CONTROL_HIGHLIGHT_COLOR
	}, {
		"ButtonShadow", 
		SLEN("ButtonShadow"), 
		0xffbbbbbb, 
		&option_sys_colour_ButtonShadow, 
		NULL, 
		NOCOL
	}, {
		"ButtonText", 
		SLEN("ButtonText"), 
		0xff000000, 
		&option_sys_colour_ButtonText, 
		NULL, 
		B_CONTROL_TEXT_COLOR
	}, {
		"CaptionText", 
		SLEN("CaptionText"), 
		0xff000000, 
		&option_sys_colour_CaptionText, 
		NULL, 
		NOCOL
	}, {
		"GrayText", 
		SLEN("GrayText"), 
		0xffcccccc, 
		&option_sys_colour_GrayText, 
		NULL, 
		NOCOL
	}, {
		"Highlight", 
		SLEN("Highlight"), 
		0xff0000ee, 
		&option_sys_colour_Highlight, 
		NULL, 
		NOCOL
	}, {
		"HighlightText", 
		SLEN("HighlightText"), 
		0xff000000, 
		&option_sys_colour_HighlightText, 
		NULL, 
		NOCOL
	}, {
		"InactiveBorder", 
		SLEN("InactiveBorder"), 
		0xffffffff, 
		&option_sys_colour_InactiveBorder, 
		NULL, 
		NOCOL
	}, {
		"InactiveCaption", 
		SLEN("InactiveCaption"), 
		0xffffffff, 
		&option_sys_colour_InactiveCaption, 
		NULL, 
		NOCOL
	}, {
		"InactiveCaptionText", 
		SLEN("InactiveCaptionText"), 
		0xffcccccc, 
		&option_sys_colour_InactiveCaptionText, 
		NULL, 
		NOCOL
	}, {
		"InfoBackground", 
		SLEN("InfoBackground"), 
		0xffaaaaaa, 
		&option_sys_colour_InfoBackground, 
		NULL, 
		B_TOOLTIP_BACKGROUND_COLOR
	}, {
		"InfoText", 
		SLEN("InfoText"), 
		0xff000000, 
		&option_sys_colour_InfoText, 
		NULL, 
		B_TOOLTIP_TEXT_COLOR
	}, {
		"Menu", 
		SLEN("Menu"), 
		0xffaaaaaa, 
		&option_sys_colour_Menu, 
		NULL, 
		B_MENU_BACKGROUND_COLOR
	}, {
		"MenuText", 
		SLEN("MenuText"), 
		0xff000000, 
		&option_sys_colour_MenuText, 
		NULL, 
		B_MENU_ITEM_TEXT_COLOR
	}, {
		"Scrollbar", 
		SLEN("Scrollbar"), 
		0xffaaaaaa, 
		&option_sys_colour_Scrollbar, 
		NULL, 
		NOCOL
	}, {
		"ThreeDDarkShadow", 
		SLEN("ThreeDDarkShadow"), 
		0xff555555, 
		&option_sys_colour_ThreeDDarkShadow, 
		NULL, 
		NOCOL
	}, {
		"ThreeDFace", 
		SLEN("ThreeDFace"), 
		0xffdddddd, 
		&option_sys_colour_ThreeDFace, 
		NULL, 
		NOCOL
	}, {
		"ThreeDHighlight", 
		SLEN("ThreeDHighlight"), 
		0xffaaaaaa, 
		&option_sys_colour_ThreeDHighlight, 
		NULL, 
		NOCOL
	}, {
		"ThreeDLightShadow", 
		SLEN("ThreeDLightShadow"), 
		0xff999999, 
		&option_sys_colour_ThreeDLightShadow, 
		NULL, 
		NOCOL
	}, {
		"ThreeDShadow", 
		SLEN("ThreeDShadow"), 
		0xff777777, 
		&option_sys_colour_ThreeDShadow, 
		NULL, 
		NOCOL
	}, {
		"Window", 
		SLEN("Window"), 
		0xffaaaaaa, 
		&option_sys_colour_Window, 
		NULL, 
		B_DOCUMENT_BACKGROUND_COLOR
	}, {
		"WindowFrame", 
		SLEN("WindowFrame"), 
		0xff000000, 
		&option_sys_colour_WindowFrame, 
		NULL, 
		NOCOL
	}, {
		
		"WindowText", 
		SLEN("WindowText"), 
		0xff000000, 
		&option_sys_colour_WindowText, 
		NULL, 
		B_DOCUMENT_TEXT_COLOR
	},

};

#define colour_list_len (sizeof(colour_list) / sizeof(struct gui_system_colour_ctx))

static struct gui_system_colour_ctx *gui_system_colour_pw = NULL;


bool gui_system_colour_init(void)
{
	unsigned int ccount;

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
			colour_list[ccount].css_colour = *(colour_list[ccount].option_colour);
		}
	}

	nsbeos_update_system_ui_colors();

	gui_system_colour_pw = colour_list;
	
	return true;
}

void gui_system_colour_finalize(void)
{
	unsigned int ccount;

	for (ccount = 0; ccount < colour_list_len; ccount++) {
		lwc_string_unref(colour_list[ccount].lwcstr);
	}
}

colour gui_system_colour_char(char *name)
{
	colour ret = 0xff00000;
	unsigned int ccount;

	for (ccount = 0; ccount < colour_list_len; ccount++) {
		if (strncasecmp(name, 
				colour_list[ccount].name, 
				colour_list[ccount].length) == 0) {
			ret = colour_list[ccount].css_colour;
			break;
		}
	}
	return ret;
}

css_error gui_system_colour(void *pw, lwc_string *name, css_color *css_colour)
{
	unsigned int ccount;
	bool match;

	for (ccount = 0; ccount < colour_list_len; ccount++) {
		if (lwc_string_caseless_isequal(name, 
				colour_list[ccount].lwcstr,
				&match) == lwc_error_ok && match) {
			*css_colour = colour_list[ccount].css_colour;
			return CSS_OK;
		}
	}	

	return CSS_INVALID;
}

void nsbeos_update_system_ui_colors(void)
{
	unsigned int ccount;

	for (ccount = 0; ccount < colour_list_len; ccount++) {
		if (colour_list[ccount].ui == NOCOL)
			continue;
		rgb_color c = ui_color(colour_list[ccount].ui);
		if (colour_list[ccount].ui == B_DESKTOP_COLOR) {
			BScreen s;
			c = s.DesktopColor();
		}

		//printf("uic[%d] = ui_color(%d) %02x %02x %02x %02x\n", ccount,
		//	colour_list[ccount].ui, c.red, c.green, c.blue, c.alpha);

		colour_list[ccount].css_colour = 0xff000000
			| ((((uint32_t)c.red << 16) & 0xff0000)
			| ((c.green << 8) & 0x00ff00)
			| ((c.blue) & 0x0000ff));
	}
}

