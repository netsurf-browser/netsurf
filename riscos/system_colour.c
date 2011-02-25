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

#include "oslib/os.h"
#include "oslib/wimp.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "desktop/gui.h"
#include "desktop/options.h"
#include "riscos/system_colour.h"

struct gui_system_colour_ctx {
	const char *name;
	int length;
	css_color colour;
	wimp_colour system_colour;
	colour *option_colour;
	lwc_string *lwcstr;
};

/* \TODO -- The wimp_COLOUR_... values in the table below map the colour
 *          definitions to parts of the RISC OS desktop palette.  In places
 *          this is fairly arbitrary, and could probably do with re-checking.
 */

static struct gui_system_colour_ctx colour_list[] = {
	{
		"ActiveBorder",
		SLEN("ActiveBorder"),
		0xff000000,
		wimp_COLOUR_BLACK,
		&option_sys_colour_ActiveBorder,
		NULL
	}, {
		"ActiveCaption",
		SLEN("ActiveCaption"),
		0xffdddddd,
		wimp_COLOUR_CREAM,
		&option_sys_colour_ActiveCaption,
		NULL
	}, {
		"AppWorkspace",
		SLEN("AppWorkspace"),
		0xffeeeeee,
		wimp_COLOUR_VERY_LIGHT_GREY,
		&option_sys_colour_AppWorkspace,
		NULL
	}, {
		"Background",
		SLEN("Background"),
		0xff0000aa,
		wimp_COLOUR_VERY_LIGHT_GREY, /* \TODO -- Check */
		&option_sys_colour_Background,
		NULL
	}, {
		"ButtonFace",
		SLEN("ButtonFace"),
		0xffaaaaaa,
		wimp_COLOUR_VERY_LIGHT_GREY,
		&option_sys_colour_ButtonFace,
		NULL
	}, {
		"ButtonHighlight",
		SLEN("ButtonHighlight"),
		0xffdddddd,
		wimp_COLOUR_DARK_GREY, /* \TODO -- Check */
		&option_sys_colour_ButtonHighlight,
		NULL
	}, {
		"ButtonShadow",
		SLEN("ButtonShadow"),
		0xffbbbbbb,
		wimp_COLOUR_MID_DARK_GREY,
		&option_sys_colour_ButtonShadow,
		NULL
	}, {
		"ButtonText",
		SLEN("ButtonText"),
		0xff000000,
		wimp_COLOUR_BLACK,
		&option_sys_colour_ButtonText,
		NULL
	}, {
		"CaptionText",
		SLEN("CaptionText"),
		0xff000000,
		wimp_COLOUR_BLACK,
		&option_sys_colour_CaptionText,
		NULL
	}, {
		"GrayText",
		SLEN("GrayText"),
		0xffcccccc,
		wimp_COLOUR_MID_LIGHT_GREY, /* \TODO -- Check */
		&option_sys_colour_GrayText,
		NULL
	}, {
		"Highlight",
		SLEN("Highlight"),
		0xff0000ee,
		wimp_COLOUR_BLACK,
		&option_sys_colour_Highlight,
		NULL
	}, {
		"HighlightText",
		SLEN("HighlightText"),
		0xff000000,
		wimp_COLOUR_WHITE,
		&option_sys_colour_HighlightText,
		NULL
	}, {
		"InactiveBorder",
		SLEN("InactiveBorder"),
		0xffffffff,
		wimp_COLOUR_BLACK,
		&option_sys_colour_InactiveBorder,
		NULL
	}, {
		"InactiveCaption",
		SLEN("InactiveCaption"),
		0xffffffff,
		wimp_COLOUR_LIGHT_GREY,
		&option_sys_colour_InactiveCaption,
		NULL
	}, {
		"InactiveCaptionText",
		SLEN("InactiveCaptionText"),
		0xffcccccc,
		wimp_COLOUR_BLACK,
		&option_sys_colour_InactiveCaptionText,
		NULL
	}, {
		"InfoBackground",
		SLEN("InfoBackground"),
		0xffaaaaaa,
		wimp_COLOUR_CREAM,
		&option_sys_colour_InfoBackground,
		NULL
	}, {
		"InfoText",
		SLEN("InfoText"),
		0xff000000,
		wimp_COLOUR_BLACK,
		&option_sys_colour_InfoText,
		NULL
	}, {
		"Menu",
		SLEN("Menu"),
		0xffaaaaaa,
		wimp_COLOUR_WHITE,
		&option_sys_colour_Menu,
		NULL
	}, {
		"MenuText",
		SLEN("MenuText"),
		0xff000000,
		wimp_COLOUR_BLACK,
		&option_sys_colour_MenuText,
		NULL
	}, {
		"Scrollbar",
		SLEN("Scrollbar"),
		0xffaaaaaa,
		wimp_COLOUR_LIGHT_GREY, /* \TODO -- Check */
		&option_sys_colour_Scrollbar,
		NULL
	}, {
		"ThreeDDarkShadow",
		SLEN("ThreeDDarkShadow"),
		0xff555555,
		wimp_COLOUR_MID_DARK_GREY,
		&option_sys_colour_ThreeDDarkShadow,
		NULL
	}, {
		"ThreeDFace",
		SLEN("ThreeDFace"),
		0xffdddddd,
		wimp_COLOUR_VERY_LIGHT_GREY,
		&option_sys_colour_ThreeDFace,
		NULL
	}, {
		"ThreeDHighlight",
		SLEN("ThreeDHighlight"),
		0xffaaaaaa,
		wimp_COLOUR_WHITE,
		&option_sys_colour_ThreeDHighlight,
		NULL
	}, {
		"ThreeDLightShadow",
		SLEN("ThreeDLightShadow"),
		0xff999999,
		wimp_COLOUR_WHITE,
		&option_sys_colour_ThreeDLightShadow,
		NULL
	}, {
		"ThreeDShadow",
		SLEN("ThreeDShadow"),
		0xff777777,
		wimp_COLOUR_MID_DARK_GREY,
		&option_sys_colour_ThreeDShadow,
		NULL
	}, {
		"Window",
		SLEN("Window"),
		0xffaaaaaa,
		wimp_COLOUR_VERY_LIGHT_GREY,
		&option_sys_colour_Window,
		NULL
	}, {
		"WindowFrame",
		SLEN("WindowFrame"),
		0xff000000,
		wimp_COLOUR_BLACK,
		&option_sys_colour_WindowFrame,
		NULL
	}, {
		"WindowText",
		SLEN("WindowText"),
		0xff000000,
		wimp_COLOUR_BLACK,
		&option_sys_colour_WindowText,
		NULL
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
			colour_list[ccount].colour =
					*(colour_list[ccount].option_colour);
		}
	}

	ro_gui_system_colour_update();

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


#define ro_gui_system_colour_convert_os_to_css(os) (0xff000000 | 	\
		(((os) & 0x0000ff00) << 8) |				\
		(((os) & 0x00ff0000) >> 8) |				\
		(((os) & 0xff000000) >> 24))


/* This is a exported interface for the RISC OS frontend,
 * documented in riscos/system_colour.h
 */

void ro_gui_system_colour_update(void)
{
	os_error	*error;
	os_PALETTE(20)	palette;
	unsigned int	ccount;

	error = xwimp_read_palette((os_palette *) &palette);
	if (error != NULL) {
		LOG(("xwimp_read_palette: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}

	for (ccount = 0; ccount < colour_list_len; ccount++) {
		if (*(colour_list[ccount].option_colour) == 0) {
			colour_list[ccount].colour =
					ro_gui_system_colour_convert_os_to_css(
					palette.entries[colour_list[ccount].
						system_colour]);
		}
	}
}
