/*
 * Copyright 2020 Michael Drake <tlsa@netsurf-browser.org>
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

/**
 * \file
 * Pave info viewer window implementation
 */

#include <stdlib.h>
#include <string.h>

#include "css/utils.h"

#include "utils/nsurl.h"

#include "netsurf/mouse.h"
#include "netsurf/layout.h"
#include "netsurf/keypress.h"
#include "netsurf/plotters.h"
#include "netsurf/core_window.h"
#include "netsurf/browser_window.h"

#include "desktop/knockout.h"
#include "desktop/page-info.h"
#include "desktop/gui_internal.h"
#include "desktop/system_colour.h"

/**
 * Plot style for heading font.
 */
static plot_font_style_t pi__heading[PAGE_STATE__COUNT] = {
	[PAGE_STATE_UNKNOWN] = {
		.family = PLOT_FONT_FAMILY_SANS_SERIF,
		.size = 14 * PLOT_STYLE_SCALE,
		.flags = FONTF_NONE,
		.weight = 400,
	},
	[PAGE_STATE_INTERNAL] = {
		.family = PLOT_FONT_FAMILY_SANS_SERIF,
		.size = 14 * PLOT_STYLE_SCALE,
		.flags = FONTF_NONE,
		.weight = 400,
	},
	[PAGE_STATE_LOCAL] = {
		.family = PLOT_FONT_FAMILY_SANS_SERIF,
		.size = 14 * PLOT_STYLE_SCALE,
		.flags = FONTF_NONE,
		.weight = 400,
	},
	[PAGE_STATE_INSECURE] = {
		.family = PLOT_FONT_FAMILY_SANS_SERIF,
		.size = 14 * PLOT_STYLE_SCALE,
		.flags = FONTF_NONE,
		.weight = 400,
	},
	[PAGE_STATE_SECURE_OVERRIDE] = {
		.family = PLOT_FONT_FAMILY_SANS_SERIF,
		.size = 14 * PLOT_STYLE_SCALE,
		.flags = FONTF_NONE,
		.weight = 400,
	},
	[PAGE_STATE_SECURE_ISSUES] = {
		.family = PLOT_FONT_FAMILY_SANS_SERIF,
		.size = 14 * PLOT_STYLE_SCALE,
		.flags = FONTF_NONE,
		.weight = 400,
	},
	[PAGE_STATE_SECURE] = {
		.family = PLOT_FONT_FAMILY_SANS_SERIF,
		.size = 14 * PLOT_STYLE_SCALE,
		.flags = FONTF_NONE,
		.weight = 400,
	},
};

/**
 * Plot style for domain font.
 */
static plot_font_style_t pi__domain = {
	.family = PLOT_FONT_FAMILY_SANS_SERIF,
	.size = 8 * PLOT_STYLE_SCALE,
	.flags = FONTF_NONE,
	.weight = 700,
};

/**
 * Plot style for item font.
 */
static plot_font_style_t pi__item = {
	.family = PLOT_FONT_FAMILY_SANS_SERIF,
	.size = 11 * PLOT_STYLE_SCALE,
	.flags = FONTF_NONE,
	.weight = 400,
};

/**
 * Plot style for item detail font.
 */
static plot_font_style_t pi__item_detail = {
	.family = PLOT_FONT_FAMILY_SANS_SERIF,
	.size = 11 * PLOT_STYLE_SCALE,
	.flags = FONTF_NONE,
	.weight = 400,
};

/**
 * Plot style for window background.
 */
static plot_style_t pi__bg = {
	.fill_type = PLOT_OP_TYPE_SOLID,
};

/**
 * Plot style for hover background.
 */
static plot_style_t pi__hover = {
	.fill_type = PLOT_OP_TYPE_SOLID,
};

/**
 * An "text" type page info entry.
 */
struct page_info_text {
	const char *text;
	const plot_font_style_t *style;
	int width;
	int height;
	int padding_bottom;
};

/**
 * An "item" type page info entry.
 */
struct page_info_item {
	struct page_info_text item;
	struct page_info_text detail;
	const plot_style_t *hover_bg;
	int padding_bottom;
	int padding_top;
	bool hover;
};

/**
 * List of page info window entries.
 */
enum pi_entry {
	PI_ENTRY_HEADER,
	PI_ENTRY_DOMAIN,
	PI_ENTRY_CERT,
	PI_ENTRY_COOKIES,
	PI_ENTRY__COUNT,
};

/**
 * An entry on a page info window.
 */
struct page_info_entry {
	/**
	 * List of page info entry types.
	 */
	enum page_info_entry_type {
		PAGE_INFO_ENTRY_TYPE_TEXT,
		PAGE_INFO_ENTRY_TYPE_ITEM,
	} type;
	/**
	 * Type-specific page info entry data.
	 */
	union {
		struct page_info_text text;
		struct page_info_item item;
	};
};

/**
 * The default page info window data.
 */
struct page_info_entry pi__entries[PI_ENTRY__COUNT] = {
	[PI_ENTRY_HEADER] = {
		.type = PAGE_INFO_ENTRY_TYPE_TEXT,
	},
	[PI_ENTRY_DOMAIN] = {
		.type = PAGE_INFO_ENTRY_TYPE_TEXT,
		.text = {
			.style = &pi__domain,
		},
	},
	[PI_ENTRY_CERT] = {
		.type = PAGE_INFO_ENTRY_TYPE_ITEM,
		.item = {
			.item = {
				.style = &pi__item,
			},
			.detail = {
				.style = &pi__item_detail,
			},
			.hover_bg = &pi__hover,
		},
	},
	[PI_ENTRY_COOKIES] = {
		.type = PAGE_INFO_ENTRY_TYPE_ITEM,
		.item = {
			.item = {
				.style = &pi__item,
			},
			.detail = {
				.style = &pi__item_detail,
			},
			.hover_bg = &pi__hover,
		},
	},
};

/**
 * The page info window structure.
 */
struct page_info {
	const struct core_window_callback_table *cw_t;
	struct core_window *cw_h;

	const struct browser_window *bw;
	lwc_string *domain;

	browser_window_page_info_state state;
	unsigned cookies;

	char cookie_text[64];
	struct page_info_entry entries[PI_ENTRY__COUNT];

	int width;
	int height;

	int window_padding;
};

/* Exported interface documented in desktop/page_info.h */
nserror page_info_init(void)
{
	bool dark_on_light;
	nserror err;
	colour good;
	colour bad;
	colour bg;
	colour fg;

	err = ns_system_colour_char("Window", &bg);
	if (err != NSERROR_OK) {
		return err;
	}

	err = ns_system_colour_char("WindowText", &fg);
	if (err != NSERROR_OK) {
		return err;
	}

	dark_on_light = colour_lightness(bg) > colour_lightness(fg);

	pi__bg.fill_colour = bg;
	pi__hover.fill_colour = dark_on_light?
			darken_colour(bg) :
			lighten_colour(bg);

	pi__domain.background = bg;
	pi__domain.foreground = fg;

	pi__item.background = bg;
	pi__item.foreground = fg;

	pi__item_detail.background = bg;
	pi__item_detail.foreground = blend_colour(bg, fg);

	good = colour_engorge_component(fg,
			dark_on_light, PLOT_COLOUR_COMPONENT_GREEN);
	bad = colour_engorge_component(fg,
			dark_on_light, PLOT_COLOUR_COMPONENT_RED);

	pi__heading[PAGE_STATE_UNKNOWN].background = bg;
	pi__heading[PAGE_STATE_UNKNOWN].foreground = bad;
	pi__heading[PAGE_STATE_INTERNAL].background = bg;
	pi__heading[PAGE_STATE_INTERNAL].foreground = fg;
	pi__heading[PAGE_STATE_LOCAL].background = bg;
	pi__heading[PAGE_STATE_LOCAL].foreground = fg;
	pi__heading[PAGE_STATE_INSECURE].background = bg;
	pi__heading[PAGE_STATE_INSECURE].foreground = bad;
	pi__heading[PAGE_STATE_SECURE_OVERRIDE].background = bg;
	pi__heading[PAGE_STATE_SECURE_OVERRIDE].foreground = bad;
	pi__heading[PAGE_STATE_SECURE_ISSUES].background = bg;
	pi__heading[PAGE_STATE_SECURE_ISSUES].foreground = bad;
	pi__heading[PAGE_STATE_SECURE].background = bg;
	pi__heading[PAGE_STATE_SECURE].foreground = good;

	return NSERROR_OK;
}

/* Exported interface documented in desktop/page_info.h */
nserror page_info_fini(void)
{
	return NSERROR_OK;
}
