/*
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
 * Copyright 2011 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * URL bars (implementation).
 */

#include <alloca.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "riscos/gui/url_bar.h"
#include "riscos/theme.h"
#include "riscos/url_suggest.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/utils.h"

#define URLBAR_HEIGHT 52
#define URLBAR_FAVICON_WIDTH 52
#define URLBAR_MIN_WIDTH 52
#define URLBAR_GRIGHT_GUTTER 8
#define URLBAR_FAVICON_NAME_LENGTH 12
#define URLBAR_INITIAL_LENGTH 256
#define URLBAR_EXTEND_LENGTH 128
#define URLBAR_FAVICON_SIZE 16

struct url_bar {
	/** The applied theme (or NULL to use the default) */
	struct theme_descriptor	*theme;

	/** The widget dimensions. */
	int			x_min, y_min;

	/** The window and icon details. */
	wimp_w			window;
	os_box			extent;

	wimp_i			container_icon;

	char			favicon_sprite[URLBAR_FAVICON_NAME_LENGTH];
	int			favicon_type;
	struct hlcache_handle	*favicon_content;
	os_box			favicon_extent;
	os_coord		favicon_offset;
	int			favicon_width, favicon_height;

	wimp_i			text_icon;
	char			*text_buffer;
	size_t			text_size;

	wimp_i			suggest_icon;
	int			suggest_x, suggest_y;

	bool			hidden;
	bool			display;
	bool			shaded;
};

static char text_validation[] = "Pptr_write;KN";
static char suggest_icon[] = "gright";
static char suggest_validation[] = "R5;Sgright,pgright";
static char null_text_string[] = "";


/*
 * Private function prototypes.
 */

static bool ro_gui_url_bar_icon_update(struct url_bar *url_bar);
static bool ro_gui_url_bar_icon_resize(struct url_bar *url_bar, bool full);

/* This is an exported interface documented in url_bar.h */

struct url_bar *ro_gui_url_bar_create(struct theme_descriptor *theme)
{
	struct url_bar		*url_bar;

	/* Allocate memory. */

	url_bar = malloc(sizeof(struct url_bar));
	if (url_bar == NULL) {
		LOG(("No memory for malloc()"));
		return NULL;
	}

	/* Set up default parameters. */

	url_bar->theme = theme;

	url_bar->display = false;
	url_bar->shaded = false;

	url_bar->x_min = URLBAR_FAVICON_WIDTH + URLBAR_MIN_WIDTH;
	url_bar->y_min = URLBAR_HEIGHT;

	url_bar->extent.x0 = 0;
	url_bar->extent.y0 = 0;
	url_bar->extent.x1 = 0;
	url_bar->extent.y1 = 0;

	url_bar->window = NULL;
	url_bar->container_icon = -1;
	url_bar->text_icon = -1;
	url_bar->suggest_icon = -1;

	url_bar->favicon_extent.x0 = 0;
	url_bar->favicon_extent.y0 = 0;
	url_bar->favicon_extent.x1 = 0;
	url_bar->favicon_extent.y1 = 0;
	url_bar->favicon_width = 0;
	url_bar->favicon_height = 0;
	url_bar->favicon_content = NULL;
	url_bar->favicon_type = 0;
	strncpy(url_bar->favicon_sprite, "Ssmall_xxx",
			URLBAR_FAVICON_NAME_LENGTH);

	url_bar->text_size = URLBAR_INITIAL_LENGTH;
	url_bar->text_buffer = malloc(url_bar->text_size);
	strncpy(url_bar->text_buffer, "", url_bar->text_size);

	url_bar->hidden = false;

	return url_bar;
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_rebuild(struct url_bar *url_bar,
		struct theme_descriptor *theme, theme_style style,
		wimp_w window, bool display, bool shaded)
{
	if (url_bar == NULL)
		return false;

	url_bar->theme = theme;
	url_bar->window = window;

	url_bar->display = display;
	url_bar->shaded = shaded;

	url_bar->container_icon = -1;
	url_bar->text_icon = -1;
	url_bar->suggest_icon = -1;

	ro_gui_wimp_get_sprite_dimensions((osspriteop_area *) -1,
		suggest_icon, &url_bar->suggest_x, &url_bar->suggest_y);

	url_bar->x_min = URLBAR_FAVICON_WIDTH + URLBAR_MIN_WIDTH +
			URLBAR_GRIGHT_GUTTER + url_bar->suggest_x;
	url_bar->y_min = (url_bar->suggest_y > URLBAR_HEIGHT) ?
			url_bar->suggest_y : URLBAR_HEIGHT;

	return ro_gui_url_bar_icon_update(url_bar);
}


/* This is an exported interface documented in url_bar.h */

void ro_gui_url_bar_destroy(struct url_bar *url_bar)
{
	if (url_bar == NULL)
		return;

	free(url_bar);
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_get_dims(struct url_bar *url_bar,
		int *width, int *height)
{
	if (url_bar == NULL)
		return false;

	if (url_bar->x_min != -1 && url_bar->y_min != -1) {
		if (width != NULL)
			*width = url_bar->x_min;
		if (height != NULL)
			*height = url_bar->y_min;

		return true;
	}

	return false;
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_set_extent(struct url_bar *url_bar,
		int x0, int y0, int x1, int y1)
{
	bool		stretch;

	if (url_bar == NULL)
		return false;

	if ((x1 - x0) < url_bar->x_min || (y1 - y0) < url_bar->y_min)
		return false;

	if (url_bar->extent.x0 == x0 && url_bar->extent.y0 == y0 &&
			url_bar->extent.x1 == x1 &&
			url_bar->extent.y1 == y1)
		return true;

	/* If it's only the length that changes, less needs to be updated. */

	stretch = (url_bar->extent.x0 == x0 && url_bar->extent.y0 == y0 &&
			url_bar->extent.y1 == y1) ? true : false;

	/* Redraw the relevant bits of the toolbar. */

	if (url_bar->window != NULL && !url_bar->hidden) {
		if (stretch) {
			xwimp_force_redraw(url_bar->window,
					x0 + URLBAR_FAVICON_WIDTH, y0,
					(x1 > url_bar->extent.x1) ?
					x1 : url_bar->extent.x1, y1);
		} else {
			xwimp_force_redraw(url_bar->window,
				url_bar->extent.x0, url_bar->extent.y0,
				url_bar->extent.x1, url_bar->extent.y1);
			xwimp_force_redraw(url_bar->window, x0, y0, x1, y1);
		}
	}

	/* Reposition the URL bar icons. */

	url_bar->extent.x0 = x0;
	url_bar->extent.y0 = y0;
	url_bar->extent.x1 = x1;
	url_bar->extent.y1 = y1;

	return ro_gui_url_bar_icon_resize(url_bar, !stretch);
}


/**
 * Create or delete a URL bar's icons if required to bring it into sync with
 * the current hidden setting.
 *
 * \param *url_bar		The URL bar to update.
 * \return			true if successful; else false.
 */

bool ro_gui_url_bar_icon_update(struct url_bar *url_bar)
{
	wimp_icon_create	icon;
	os_error		*error;
	bool			resize;

	if (url_bar == NULL || url_bar->window == NULL)
		return false;

	icon.w = url_bar->window;
	icon.icon.extent.x0 = 0;
	icon.icon.extent.y0 = 0;
	icon.icon.extent.x1 = 0;
	icon.icon.extent.y1 = 0;

	resize = false;

	/* Create or delete the container icon. */

	if (!url_bar->hidden && url_bar->container_icon == -1) {
		icon.icon.flags = wimp_ICON_BORDER |
				(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT);
		error = xwimp_create_icon(&icon, &url_bar->container_icon);
		if (error != NULL) {
			LOG(("xwimp_create_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			url_bar->container_icon = -1;
			return false;
		}

		resize = true;
	} else  if (url_bar->hidden && url_bar->container_icon != -1){
		error = xwimp_delete_icon(url_bar->window,
				url_bar->container_icon);
		if (error != NULL) {
			LOG(("xwimp_delete_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

		url_bar->container_icon = -1;
	}

	/* Create or delete the text icon. */

	if (!url_bar->hidden && url_bar->text_icon == -1) {
		icon.icon.data.indirected_text.text = url_bar->text_buffer;
		icon.icon.data.indirected_text.validation = text_validation;
		icon.icon.data.indirected_text.size = url_bar->text_size;
		icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
				wimp_ICON_VCENTRED | wimp_ICON_FILLED |
				(wimp_COLOUR_BLACK <<
					wimp_ICON_FG_COLOUR_SHIFT);
		if (url_bar->display)
			icon.icon.flags |= (wimp_BUTTON_NEVER <<
					wimp_ICON_BUTTON_TYPE_SHIFT);
		else
			icon.icon.flags |= (wimp_BUTTON_WRITE_CLICK_DRAG <<
					wimp_ICON_BUTTON_TYPE_SHIFT);
		error = xwimp_create_icon(&icon, &url_bar->text_icon);
		if (error) {
			LOG(("xwimp_create_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			url_bar->text_icon = -1;
			return false;
		}

		resize = true;
	} else if (url_bar->hidden && url_bar->text_icon != -1) {
		error = xwimp_delete_icon(url_bar->window,
				url_bar->text_icon);
		if (error != NULL) {
			LOG(("xwimp_delete_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

		url_bar->text_icon = -1;
	}

	/* Create or delete the suggest icon. */

	if (!url_bar->hidden && url_bar->suggest_icon == -1) {
		icon.icon.data.indirected_text.text = null_text_string;
		icon.icon.data.indirected_text.size = 1;
		icon.icon.data.indirected_text.validation = suggest_validation;
		icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_SPRITE |
				wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED |
				wimp_ICON_VCENTRED | (wimp_BUTTON_CLICK <<
				wimp_ICON_BUTTON_TYPE_SHIFT);
		error = xwimp_create_icon(&icon, &url_bar->suggest_icon);
		if (error) {
			LOG(("xwimp_create_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

		if (!url_bar->display)
			ro_gui_wimp_event_register_menu_gright(url_bar->window,
					wimp_ICON_WINDOW, url_bar->suggest_icon,
					ro_gui_url_suggest_menu);

		if (!ro_gui_url_bar_update_urlsuggest(url_bar))
			return false;

		resize = true;
	} else if (url_bar->hidden && url_bar->suggest_icon != -1) {
		ro_gui_wimp_event_deregister(url_bar->window,
				url_bar->suggest_icon);
		error = xwimp_delete_icon(url_bar->window,
				url_bar->suggest_icon);
		if (error != NULL) {
			LOG(("xwimp_delete_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

		url_bar->suggest_icon = -1;
	}

	/* If any icons were created, resize the bar. */

	if (resize && !ro_gui_url_bar_icon_resize(url_bar, true))
		return false;

	/* If there are any icons, apply shading as necessary. */

	if (url_bar->container_icon != -1)
		ro_gui_set_icon_shaded_state(url_bar->window,
				url_bar->container_icon, url_bar->shaded);

	if (url_bar->text_icon != -1)
		ro_gui_set_icon_shaded_state(url_bar->window,
				url_bar->text_icon, url_bar->shaded);

	if (url_bar->suggest_icon != -1)
		ro_gui_set_icon_shaded_state(url_bar->window,
				url_bar->suggest_icon, url_bar->shaded);

	return true;
}


/**
 * Position the icons in the URL bar to take account of the currently
 * configured extent.
 *
 * \param *url_bar		The URL bar to update.
 * \param full			true to resize everything; false to move only
 *				the right-hand end of the bar.
 * \return			true if successful; else false.
 */

bool ro_gui_url_bar_icon_resize(struct url_bar *url_bar, bool full)
{
	int		x0, y0, x1, y1;
	int		centre;
	os_error	*error;
	os_coord	eig = {1, 1};
	wimp_caret	caret;

	if (url_bar == NULL || url_bar->window == NULL)
		return false;

	/* calculate 1px in OS units */
	ro_convert_pixels_to_os_units(&eig, (os_mode) -1);

	/* The vertical centre line of the widget's extent. */

	centre = url_bar->extent.y0 +
			(url_bar->extent.y1 - url_bar->extent.y0) / 2;

	/* Position the container icon. */

	if (url_bar->container_icon != -1) {
		x0 = url_bar->extent.x0;
		x1 = url_bar->extent.x1 -
				url_bar->suggest_x - URLBAR_GRIGHT_GUTTER;

		y0 = centre - (URLBAR_HEIGHT / 2);
		y1 = y0 + URLBAR_HEIGHT;

		error = xwimp_resize_icon(url_bar->window,
				url_bar->container_icon,
				x0, y0, x1, y1);
		if (error != NULL) {
			LOG(("xwimp_resize_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			url_bar->container_icon = -1;
			return false;
		}
	}

	/* Position the URL Suggest icon. */

	if (url_bar->suggest_icon != -1) {
		x0 = url_bar->extent.x1 - url_bar->suggest_x;
		x1 = url_bar->extent.x1;

		y0 = centre - (url_bar->suggest_y / 2);
		y1 = y0 + url_bar->suggest_y;

		error = xwimp_resize_icon(url_bar->window,
				url_bar->suggest_icon,
				x0, y0, x1, y1);
		if (error != NULL) {
			LOG(("xwimp_resize_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			url_bar->suggest_icon = -1;
			return false;
		}
	}

	/* Position the Text icon. */

	if (url_bar->text_icon != -1) {
		x0 = url_bar->extent.x0 + URLBAR_FAVICON_WIDTH;
		x1 = url_bar->extent.x1 - eig.x -
				url_bar->suggest_x - URLBAR_GRIGHT_GUTTER;

		y0 = centre - (URLBAR_HEIGHT / 2) + eig.y;
		y1 = y0 + URLBAR_HEIGHT - 2 * eig.y;

		error = xwimp_resize_icon(url_bar->window,
				url_bar->text_icon,
				x0, y0, x1, y1);
		if (error != NULL) {
			LOG(("xwimp_resize_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			url_bar->text_icon = -1;
			return false;
		}

		if (xwimp_get_caret_position(&caret) == NULL) {
			if ((caret.w == url_bar->window) &&
					(caret.i == url_bar->text_icon)) {
				xwimp_set_caret_position(url_bar->window,
						url_bar->text_icon, caret.pos.x,
						caret.pos.y, -1, caret.index);
			}
		}
	}

	/* Position the Favicon icon. */

	url_bar->favicon_extent.x0 = url_bar->extent.x0 + eig.x;
	url_bar->favicon_extent.x1 = url_bar->extent.x0 + URLBAR_FAVICON_WIDTH;
	url_bar->favicon_extent.y0 = centre - (URLBAR_HEIGHT / 2) + eig.y;
	url_bar->favicon_extent.y1 = url_bar->favicon_extent.y0 + URLBAR_HEIGHT
			- 2 * eig.y;

	return true;
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_hide(struct url_bar *url_bar, bool hide)
{
	if (url_bar == NULL || url_bar->hidden == hide)
		return (url_bar == NULL) ? false : true;

	url_bar->hidden = hide;

	return ro_gui_url_bar_icon_update(url_bar);
}


/* This is an exported interface documented in url_bar.h */

void ro_gui_url_bar_redraw(struct url_bar *url_bar, wimp_draw *redraw)
{
	wimp_icon	icon;
	struct rect	clip;

	/* Test for a valid URL bar, and then check that the redraw box
	 * coincides with the bar's favicon extent.
	 */

	if (url_bar == NULL || url_bar->hidden ||
			(redraw->clip.x0 - (redraw->box.x0 - redraw->xscroll))
					> (url_bar->favicon_extent.x1) ||
			(redraw->clip.y0 - (redraw->box.y1 - redraw->yscroll))
					> url_bar->favicon_extent.y1 ||
			(redraw->clip.x1 - (redraw->box.x0 - redraw->xscroll))
					< url_bar->favicon_extent.x0 ||
			(redraw->clip.y1 - (redraw->box.y1 - redraw->yscroll))
					< url_bar->favicon_extent.y0)
		return;

	if (url_bar->favicon_content == NULL) {
		icon.data.indirected_text.text = null_text_string;
		icon.data.indirected_text.validation = url_bar->favicon_sprite;
		icon.data.indirected_text.size = 1;
		icon.flags = wimp_ICON_TEXT | wimp_ICON_SPRITE |
				wimp_ICON_INDIRECTED | wimp_ICON_FILLED |
				wimp_ICON_HCENTRED | wimp_ICON_VCENTRED;
		icon.extent.x0 = url_bar->favicon_extent.x0;
		icon.extent.x1 = url_bar->favicon_extent.x1;
		icon.extent.y0 = url_bar->favicon_extent.y0;
		icon.extent.y1 = url_bar->favicon_extent.y1;

		xwimp_plot_icon(&icon);
	} else {
		struct content_redraw_data data;
		struct redraw_context ctx = {
			.interactive = true,
			.background_images = true,
			.plot = &ro_plotters
		};

		xwimp_set_colour(wimp_COLOUR_WHITE);
		xos_plot(os_MOVE_TO,
				(redraw->box.x0 - redraw->xscroll) +
					url_bar->favicon_extent.x0,
				(redraw->box.y1 - redraw->yscroll) +
					url_bar->favicon_extent.y0);
		xos_plot(os_PLOT_TO | os_PLOT_RECTANGLE,
				(redraw->box.x0 - redraw->xscroll) +
					url_bar->favicon_extent.x1,
				(redraw->box.y1 - redraw->yscroll) +
					url_bar->favicon_extent.y1);

		clip.x0 = (redraw->clip.x0 - ro_plot_origin_x) / 2;
		clip.y0 = (ro_plot_origin_y - redraw->clip.y0) / 2;
		clip.x1 = (redraw->clip.x1 - ro_plot_origin_x) / 2;
		clip.y1 = (ro_plot_origin_y - redraw->clip.y1) / 2;

		data.x = (url_bar->favicon_extent.x0 +
				url_bar->favicon_offset.x) / 2;
		data.y = (url_bar->favicon_offset.y -
					url_bar->favicon_extent.y1) / 2;
		data.width = url_bar->favicon_width;
		data.height = url_bar->favicon_height;
		data.background_colour = 0xFFFFFF;
		data.scale = 1;
		data.repeat_x = false;
		data.repeat_y = false;

		content_redraw(url_bar->favicon_content, &data, &clip, &ctx);
	}
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_click(struct url_bar *url_bar,
		wimp_pointer *pointer, wimp_window_state *state,
		url_bar_action *action)
{
	os_coord			pos;

	if (url_bar == NULL || url_bar->hidden ||
			url_bar->display || url_bar->shaded)
		return false;

	/* Check that the click was within our part of the window. */

	pos.x = pointer->pos.x - state->visible.x0 + state->xscroll;
	pos.y = pointer->pos.y - state->visible.y1 + state->yscroll;

	if (pos.x < url_bar->extent.x0 || pos.x > url_bar->extent.x1 ||
			pos.y < url_bar->extent.y0 ||
			pos.y > url_bar->extent.y1)
		return false;

	/* If we find a Select or Adjust drag, check if it originated on the
	 * URL bar or over the favicon.  If either, then return an event.
	 */

	if (pointer->buttons == wimp_DRAG_SELECT ||
			pointer->buttons == wimp_DRAG_ADJUST) {
		if (pointer->i == url_bar->text_icon) {
			if (action != NULL)
				*action = TOOLBAR_URL_DRAG_URL;
			return true;
		}

		if (pos.x >= url_bar->favicon_extent.x0 &&
				pos.x <= url_bar->favicon_extent.x1 &&
				pos.y >= url_bar->favicon_extent.y0 &&
				pos.y <=url_bar->favicon_extent.y1) {
			if (action != NULL)
				*action = TOOLBAR_URL_DRAG_FAVICON;
			return true;
		}
	}

	return false;
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_menu_prepare(struct url_bar *url_bar, wimp_i i,
		wimp_menu *menu, wimp_pointer *pointer)
{
	if (url_bar == NULL || url_bar->suggest_icon != i ||
			menu != ro_gui_url_suggest_menu)
		return false;

	if (pointer != NULL)
		return ro_gui_url_suggest_prepare_menu();

	return true;
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_menu_select(struct url_bar *url_bar, wimp_i i,
		wimp_menu *menu, wimp_selection *selection, menu_action action)
{
	const char		*url;
	struct gui_window	*g;

	if (url_bar == NULL || url_bar->suggest_icon != i ||
			menu != ro_gui_url_suggest_menu)
		return false;

	url = ro_gui_url_suggest_get_selection(selection);
	g = ro_gui_toolbar_lookup(url_bar->window);

	if (url != NULL && g != NULL && g->bw != NULL) {
		gui_window_set_url(g, url);
		browser_window_go(g->bw, url, 0, true);
	}

	return true;
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_help_suffix(struct url_bar *url_bar, wimp_i i,
		os_coord *mouse, wimp_window_state *state,
		wimp_mouse_state buttons, const char **suffix)
{
	os_coord			pos;

	if (url_bar == NULL || url_bar->hidden)
		return false;

	/* Check that the click was within our part of the window. */

	pos.x = mouse->x - state->visible.x0 + state->xscroll;
	pos.y = mouse->y - state->visible.y1 + state->yscroll;

	if (pos.x < url_bar->extent.x0 || pos.x > url_bar->extent.x1 ||
			pos.y < url_bar->extent.y0 ||
			pos.y > url_bar->extent.y1)
		return false;

	/* Return hard-coded icon numbers that match the ones that were
	 * always allocated to the URL bar in a previous implementation.
	 * If Messages can be updated, this could be changed.
	 */

	if (i == url_bar->text_icon)
		*suffix = "14";
	else if (i == url_bar->suggest_icon)
		*suffix = "15";
	else
		*suffix = "";

	return true;
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_take_caret(struct url_bar *url_bar)
{
	os_error	*error;

	if (url_bar == NULL || url_bar->hidden)
		return false;

	error = xwimp_set_caret_position(url_bar->window, url_bar->text_icon,
			-1, -1, -1, 0);
	if (error) {
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);

		return false;
	}

	return true;
}


/* This is an exported interface documented in url_bar.h */

void ro_gui_url_bar_set_url(struct url_bar *url_bar, const char *url,
		bool is_utf8, bool set_caret)
{
	wimp_caret	caret;
	os_error	*error;
	const char	*set_url;

	if (url_bar == NULL || url_bar->text_buffer == NULL)
		return;

	if (url_bar->text_icon == -1) {
		strncpy(url_bar->text_buffer, url, url_bar->text_size);
		return;
	}

	ro_gui_set_icon_string(url_bar->window, url_bar->text_icon,
			url, is_utf8);

	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (set_caret || (caret.w == url_bar->window &&
			caret.i == url_bar->text_icon)) {
		set_url = ro_gui_get_icon_string(url_bar->window,
				url_bar->text_icon);

		error = xwimp_set_caret_position(url_bar->window,
				url_bar->text_icon, 0, 0, -1, strlen(set_url));
		if (error) {
			LOG(("xwimp_set_caret_position: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
}


/* This is an exported interface documented in url_bar.h */

const char *ro_gui_url_bar_get_url(struct url_bar *url_bar)
{
	if (url_bar == NULL)
		return NULL;

	return (const char *) url_bar->text_buffer;
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_get_url_extent(struct url_bar *url_bar, os_box *extent)
{
	wimp_icon_state		state;
	os_error		*error;

	if (url_bar == NULL || url_bar->hidden)
		return false;

	if (extent == NULL)
		return true;

	state.w = url_bar->window;
	state.i = url_bar->container_icon;
	error = xwimp_get_icon_state(&state);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	extent->x0 = state.icon.extent.x0;
	extent->y0 = state.icon.extent.y0;
	extent->x1 = state.icon.extent.x1;
	extent->y1 = state.icon.extent.y1;

	return true;
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_test_for_text_field_click(struct url_bar *url_bar,
		wimp_pointer *pointer)
{
	if (url_bar == NULL || url_bar->hidden || pointer == NULL)
		return false;

	return (pointer->w == url_bar->window &&
			pointer->i == url_bar->text_icon) ? true : false;
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_test_for_text_field_keypress(struct url_bar *url_bar,
		wimp_key *key)
{
	if (url_bar == NULL || url_bar->hidden || key == NULL)
		return false;

	return (key->w == url_bar->window &&
			key->i == url_bar->text_icon) ? true : false;
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_set_site_favicon(struct url_bar *url_bar,
		struct hlcache_handle *h)
{
	content_type		type = CONTENT_NONE;

	if (url_bar == NULL)
		return false;

	if (h != NULL)
		type = content_get_type(h);

	// \TODO -- Maybe test for CONTENT_ICO ???

	if (type == CONTENT_IMAGE) {
		url_bar->favicon_content = h;
		url_bar->favicon_width = content_get_width(h);
		url_bar->favicon_height = content_get_height(h);

		if (url_bar->favicon_width > URLBAR_FAVICON_SIZE)
			url_bar->favicon_width = URLBAR_FAVICON_SIZE;

		if (url_bar->favicon_height > URLBAR_FAVICON_SIZE)
			url_bar->favicon_height = URLBAR_FAVICON_SIZE;

		url_bar->favicon_offset.x = ((url_bar->favicon_extent.x1 -
				url_bar->favicon_extent.x0) -
				(url_bar->favicon_width * 2)) / 2;
		url_bar->favicon_offset.y = ((url_bar->favicon_extent.y1 -
				url_bar->favicon_extent.y0) -
				(url_bar->favicon_height * 2)) / 2;
	} else {
		url_bar->favicon_content = NULL;

		if (url_bar->favicon_type != 0)
			snprintf(url_bar->favicon_sprite,
					URLBAR_FAVICON_NAME_LENGTH,
					"Ssmall_%.3x", url_bar->favicon_type);
		else
			snprintf(url_bar->favicon_sprite,
					URLBAR_FAVICON_NAME_LENGTH,
					"Ssmall_xxx");
	}

		if (!url_bar->hidden)
			xwimp_force_redraw(url_bar->window,
				url_bar->favicon_extent.x0,
				url_bar->favicon_extent.y0,
				url_bar->favicon_extent.x1,
				url_bar->favicon_extent.y1);

	return true;
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_set_content_favicon(struct url_bar *url_bar,
		struct hlcache_handle *h)
{
	int	type = 0;
	char	sprite[URLBAR_FAVICON_NAME_LENGTH];

	if (url_bar == NULL)
		return false;

	if (h != NULL)
		type = ro_content_filetype(h);

	if (type != 0) {
		snprintf(sprite, URLBAR_FAVICON_NAME_LENGTH,
				"small_%.3x", type);

		if (!ro_gui_wimp_sprite_exists(sprite))
			type = 0;
	}

	url_bar->favicon_type = type;

	if (url_bar->favicon_content == NULL) {
		if (type == 0)
			snprintf(url_bar->favicon_sprite,
				URLBAR_FAVICON_NAME_LENGTH, "Ssmall_xxx");
		else
			snprintf(url_bar->favicon_sprite,
				URLBAR_FAVICON_NAME_LENGTH, "S%s", sprite);

		if (!url_bar->hidden)
			xwimp_force_redraw(url_bar->window,
					url_bar->favicon_extent.x0,
					url_bar->favicon_extent.y0,
					url_bar->favicon_extent.x1,
					url_bar->favicon_extent.y1);
	}

	return true;
}


/* This is an exported interface documented in url_bar.h */

bool ro_gui_url_bar_update_urlsuggest(struct url_bar *url_bar)
{
	if (url_bar == NULL || url_bar->hidden)
		return (url_bar == NULL) ? false : true;

	if (url_bar->window != NULL && url_bar->suggest_icon != -1)
		ro_gui_set_icon_shaded_state(url_bar->window,
				url_bar->suggest_icon,
				!ro_gui_url_suggest_get_menu_available());

	return true;
}

