/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * GUI URL auto-completion (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/wimp.h"
#include "netsurf/content/url_store.h"
#include "netsurf/utils/log.h"
#include "netsurf/riscos/global_history.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/url_complete.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/utils.h"

#define MAXIMUM_VISIBLE_LINES 7

static char **url_complete_matches = NULL;
static int url_complete_matches_allocated = 0;
static int url_complete_matches_available = 0;
static char *url_complete_matched_string = NULL;
static int url_complete_matches_selection = -1;
static int url_complete_keypress_selection = -1;
static wimp_w url_complete_parent = 0;
static bool url_complete_matches_reset = false;
static char *url_complete_original_url = NULL;

static char *url_complete_redraw[MAXIMUM_VISIBLE_LINES];
static char url_complete_icon_null[] = "\0";
static wimp_icon url_complete_icon;
static int mouse_x;
static int mouse_y;

/**
 * Should be called when the caret is placed into a URL completion icon.
 *
 * \param g    the gui_window to initialise URL completion for
 */
void ro_gui_url_complete_start(struct gui_window *g) {
  	char *url;
  	
	if ((!g->toolbar) || (!g->toolbar->display_url) ||
			(g->window == url_complete_parent))
		return;

	ro_gui_url_complete_close(NULL, 0);
	url = ro_gui_get_icon_string(g->toolbar->toolbar_handle, ICON_TOOLBAR_URL);
	url_complete_matched_string = url_store_match_string(url);
	if (url_complete_matched_string)
		url_complete_parent = g->window;
}


/**
 * Handles a keypress for URL completion
 *
 * \param g    the gui_window to update
 * \param key  the key pressed
 */
bool ro_gui_url_complete_keypress(struct gui_window *g, int key) {
	wimp_window_state state;
	char **array_extend;
	struct url_data *reference = NULL;
	char *match_url;
	char *url;
	char *output;
	int i, lines;
	int old_selection;
	bool ignore_changes = false;
	int height;
	os_error *error;
	bool currently_open;

	/* we must have a toolbar/url bar */
	if ((!g->toolbar) || (!g->toolbar->display_url) || (!option_url_suggestion)) {
		ro_gui_url_complete_close(NULL, 0);
		return false;
	}

	/* if we are currently active elsewhere, remove the previous window */
	currently_open = ((g->window == url_complete_parent) &&
			(url_complete_matches_available > 0));
	if (g->window != url_complete_parent)
		ro_gui_url_complete_close(NULL, 0);

	/* forcibly open on down keys */
	if ((!currently_open) && (url_complete_matched_string)) {
		switch (key) {
			case wimp_KEY_DOWN:
			case wimp_KEY_PAGE_DOWN:
			case wimp_KEY_CONTROL | wimp_KEY_DOWN:
				free(url_complete_matched_string);
				url_complete_matched_string = NULL;
		}
	}
		

	/* get the text to match */
	url_complete_parent = g->window;
	url = ro_gui_get_icon_string(g->toolbar->toolbar_handle, ICON_TOOLBAR_URL);
	match_url = url_store_match_string(url);
	if (!match_url) {
		ro_gui_url_complete_close(NULL, 0);
		return false;
	}

	/* check if we should ignore text changes */
	if ((url_complete_keypress_selection >= 0) && (url_complete_matches))
		ignore_changes = !strcmp(url,
				url_complete_matches[url_complete_keypress_selection]);

	/* if the text to match has changed then update it */
	if (!ignore_changes && ((!url_complete_matched_string) ||
			(strcmp(match_url, url_complete_matched_string)))) {

		/* memorize the current matches */
		lines = MAXIMUM_VISIBLE_LINES;
		if (lines > url_complete_matches_available)
			lines = url_complete_matches_available;
		if (url_complete_matches)
			for (i = 0; i < MAXIMUM_VISIBLE_LINES; i++)
				url_complete_redraw[i] = url_complete_matches[i];

		/* our selection gets wiped */
		error = xwimp_force_redraw(dialog_url_complete,
				0, -(url_complete_matches_selection + 1) * 44,
				65536, -url_complete_matches_selection * 44);
		if (error) {
			LOG(("xwimp_force_redraw: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}

		/* clear our state */
		free(url_complete_original_url);
		free(url_complete_matched_string);
		url_complete_matched_string = match_url;
		url_complete_original_url = NULL;
		url_complete_matches_available = 0;
		url_complete_matches_selection = -1;
		url_complete_keypress_selection = -1;

		/* get some initial memory */
		if (!url_complete_matches) {
			url_complete_matches = malloc(64 * sizeof(char *));
			if (!url_complete_matches) {
				ro_gui_url_complete_close(NULL, 0);
				return false;
			}
			url_complete_matches_allocated = 64;
		}

		/* get all our matches */
		while ((output = url_store_match(match_url, &reference))) {
			url_complete_matches_available++;
			if (url_complete_matches_available >
					url_complete_matches_allocated) {

				array_extend = realloc(url_complete_matches,
						(url_complete_matches_allocated + 64) *
						sizeof(char *));
				if (!array_extend) {
					ro_gui_url_complete_close(NULL, 0);
					return false;
				}
				url_complete_matches = array_extend;
				url_complete_matches_allocated += 64;
			}
			url_complete_matches[url_complete_matches_available - 1] =
					output;
		}

		/* update the window */
		state.w = g->window;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}
		url_complete_matches_reset = true;
		ro_gui_url_complete_resize(g, (wimp_open *)&state);
		url_complete_matches_reset = false;

		/* redraw the relevant bits of the window */
		lines = MAXIMUM_VISIBLE_LINES;
		if (lines > url_complete_matches_available)
			lines = url_complete_matches_available;
		for (i = 0; i < lines; i++) {
			if (url_complete_redraw[i] != url_complete_matches[i]) {
				error = xwimp_force_redraw(dialog_url_complete,
					0, -(i + 1) * 44, 65536, -i * 44);
				if (error) {
					LOG(("xwimp_force_redraw: 0x%x: %s",
							error->errnum, error->errmess));
					warn_user("WimpError", error->errmess);
				}
			}
		}

	} else {
		free(match_url);
	}

	/* handle keypresses */
	if (!currently_open)
		return false;
	old_selection = url_complete_matches_selection;
	switch (key) {
		case wimp_KEY_UP:
			url_complete_matches_selection--;
			break;
		case wimp_KEY_DOWN:
			url_complete_matches_selection++;
			break;
		case wimp_KEY_PAGE_UP:
			url_complete_matches_selection -= MAXIMUM_VISIBLE_LINES;
			break;
		case wimp_KEY_PAGE_DOWN:
			url_complete_matches_selection += MAXIMUM_VISIBLE_LINES;
			break;
		case wimp_KEY_CONTROL | wimp_KEY_UP:
			url_complete_matches_selection = 0;
			break;
		case wimp_KEY_CONTROL | wimp_KEY_DOWN:
			url_complete_matches_selection = 65536;
			break;
	}
	if (url_complete_matches_selection > url_complete_matches_available - 1)
		url_complete_matches_selection = url_complete_matches_available - 1;
	else if (url_complete_matches_selection < -1)
		url_complete_matches_selection = -1;

	if (old_selection == url_complete_matches_selection)
		return false;

	error = xwimp_force_redraw(dialog_url_complete,
			0, -(old_selection + 1) * 44, 65536, -old_selection * 44);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	error = xwimp_force_redraw(dialog_url_complete,
			0, -(url_complete_matches_selection + 1) * 44,
			65536, -url_complete_matches_selection * 44);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	if (old_selection == -1) {
		free(url_complete_original_url);
		url_complete_original_url = malloc(strlen(url) + 1);
		if (!url_complete_original_url)
			return false;
		strcpy(url_complete_original_url, url);
	}
	if (url_complete_matches_selection == -1) {
		ro_gui_set_icon_string(g->toolbar->toolbar_handle,
				ICON_TOOLBAR_URL,
				url_complete_original_url);
	} else {
		ro_gui_set_icon_string(g->toolbar->toolbar_handle,
				ICON_TOOLBAR_URL,
				url_complete_matches[url_complete_matches_selection]);
	}
	url_complete_keypress_selection = url_complete_matches_selection;

	/* auto-scroll */
	state.w = dialog_url_complete;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return true;
	}
	if (state.yscroll < -(url_complete_matches_selection * 44))
		state.yscroll = -(url_complete_matches_selection * 44);
	height = state.visible.y1 - state.visible.y0;
	if (state.yscroll - height > -((url_complete_matches_selection + 1) * 44))
		state.yscroll = -((url_complete_matches_selection + 1) * 44) + height;
	error = xwimp_open_window((wimp_open *)(&state));
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return true;
	}

	return true;
}


/**
 * Move and resize the url completion window to match the toolbar.
 *
 * \param g	the gui_window to update
 * \param open  the wimp_open request (updated on exit)
 */
void ro_gui_url_complete_resize(struct gui_window *g, wimp_open *open) {
	os_box extent = { 0, 0, 0, 0 };
	wimp_icon_state url_state;
	wimp_window_state toolbar_state;
	wimp_window_state state;
	os_error *error;
	int lines;
	int scroll_v = 0;

	/* only react to our window */
	if (open->w != url_complete_parent)
		return;
	/* if there is no toolbar, or there is no URL bar shown, or there are
	 * no URL matches, close it */
	if ((!g->toolbar) || (!g->toolbar->display_url) ||
			(!url_complete_matches) ||
			(url_complete_matches_available == 0)) {
		ro_gui_url_complete_close(NULL, 0);
		return;
	}

	/* get our current auto-complete window state for the scroll values */
	state.w = dialog_url_complete;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if (url_complete_matches_reset)
		state.yscroll = 0;

	/* move the window to the correct position */
	toolbar_state.w = g->toolbar->toolbar_handle;
	error = xwimp_get_window_state(&toolbar_state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	url_state.w = g->toolbar->toolbar_handle;
	url_state.i = ICON_TOOLBAR_URL;
	error = xwimp_get_icon_state(&url_state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	lines = url_complete_matches_available;
	extent.y0 = -(lines * 44);
	extent.x1 = 65536;
	error = xwimp_set_extent(dialog_url_complete, &extent);
	if (error) {
		LOG(("xwimp_set_extent: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	state.next = open->next;
	state.flags &= ~wimp_WINDOW_VSCROLL;
	state.flags &= ~(4095 << 16); /* clear bits 16-27 */
	if (lines > MAXIMUM_VISIBLE_LINES) {
		lines = MAXIMUM_VISIBLE_LINES;
		scroll_v = ro_get_vscroll_width(NULL) - 2;
		state.flags |= wimp_WINDOW_VSCROLL;
	}
	state.visible.x0 = open->visible.x0 + 2 + url_state.icon.extent.x0;
	state.visible.x1 = open->visible.x0 - 2 + url_state.icon.extent.x1 - scroll_v;
	state.visible.y1 = open->visible.y1 - url_state.icon.extent.y1 + 2;
	state.visible.y0 = state.visible.y1 - (lines * 44);
	if (state.visible.x1 + scroll_v > toolbar_state.visible.x1)
		state.visible.x1 = toolbar_state.visible.x1 - scroll_v;
	if (state.visible.x1 - state.visible.x0 < 0) {
		error = xwimp_close_window(dialog_url_complete);
		if (error) {
			LOG(("xwimp_close_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	} else {
		error = xwimp_open_window_nested_with_flags(&state, (wimp_w)-1, 0);
		if (error) {
			LOG(("xwimp_open_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
		open->next = dialog_url_complete;
	}
}


/**
 * Try to close the current url completion window
 *
 * \param g  the gui_window the user clicked on (or NULL to forcibly close)
 * \param i  the icon the user clicked on to prompt the close
 * \return whether the window was closed
 */
bool ro_gui_url_complete_close(struct gui_window *g, wimp_i i) {
	os_error *error;
	bool currently_open;

	if ((g && (i == ICON_TOOLBAR_URL) && (g->window == url_complete_parent)))
		return false;

	currently_open = ((url_complete_parent) &&
			(url_complete_matches_available > 0));

	free(url_complete_matches);
	free(url_complete_matched_string);
	free(url_complete_original_url);
	url_complete_matches = NULL;
	url_complete_matched_string = NULL;
	url_complete_original_url = NULL;
	url_complete_matches_allocated = 0;
	url_complete_matches_available = 0;
	url_complete_keypress_selection = -1;
	url_complete_matches_selection = -1;
	url_complete_parent = 0;

	error = xwimp_close_window(dialog_url_complete);
	if (error) {
		LOG(("xwimp_close_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	return currently_open;
}


/**
 * Redraws a section of the URL completion window
 *
 * \param redraw  the area to redraw
 * \param tree	  the tree to redraw
 */
void ro_gui_url_complete_redraw(wimp_draw *redraw) {
	osbool more;
	os_error *error;
	int clip_y0, clip_y1, origin_y;
	int first_line, last_line, line;

	/* initialise our icon */
	url_complete_icon.flags = wimp_ICON_INDIRECTED | wimp_ICON_VCENTRED |
			wimp_ICON_TEXT | wimp_ICON_FILLED |
			(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
			(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT);
	url_complete_icon.extent.x0 = 0;
	url_complete_icon.extent.x1 = 16384;
	url_complete_icon.data.indirected_text.validation = url_complete_icon_null;

	/* no matches? no redraw */
	if (!url_complete_matches) {
	  	LOG(("Attempt to redraw with no matches made"));
		ro_gui_user_redraw(redraw, false, NULL);
		return;
	}
	
	/* redraw */
	more = wimp_redraw_window(redraw);
	while (more) {
		origin_y = redraw->box.y1 - redraw->yscroll;
		clip_y0 = redraw->clip.y0 - origin_y;
		clip_y1 = redraw->clip.y1 - origin_y;

		first_line = (-clip_y1) / 44;
		last_line = (-clip_y0 + 43) / 44;

		for (line = first_line; line < last_line; line++) {
			if (line == url_complete_matches_selection)
				url_complete_icon.flags |= wimp_ICON_SELECTED;
			else
				url_complete_icon.flags &= ~wimp_ICON_SELECTED;
			url_complete_icon.extent.y1 = -line * 44;
			url_complete_icon.extent.y0 = -(line + 1) * 44;
			url_complete_icon.data.indirected_text.text =
					url_complete_matches[line];
			url_complete_icon.data.indirected_text.size =
					strlen(url_complete_matches[line]);
			error = xwimp_plot_icon(&url_complete_icon);
			if (error) {
				LOG(("xwimp_plot_icon: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
			}
		}
		more = wimp_get_rectangle(redraw);
	}
}


/**
 * Handle mouse movements/clicks over the URL completion window.
 *
 * \param pointer  the pointer state
 * \param buttons  whether to react to mouse buttons
 */
void ro_gui_url_complete_mouse_at(wimp_pointer *pointer, bool buttons) {
	wimp_window_state state;
	os_error *error;
	int selection, old_selection;
	struct gui_window *g;
	char *url;

	if ((mouse_x == pointer->pos.x) && (mouse_y == pointer->pos.y) &&
			(pointer->buttons == 0))
		return;
	mouse_x = pointer->pos.x;
	mouse_y = pointer->pos.y;

	state.w = dialog_url_complete;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	selection = (state.visible.y1 - pointer->pos.y - state.yscroll) / 44;
	if (selection != url_complete_matches_selection) {
		if (url_complete_matches_selection == -1) {
			g = ro_gui_window_lookup(url_complete_parent);
			if (!g)
				return;
			url = ro_gui_get_icon_string(g->toolbar->toolbar_handle,
					ICON_TOOLBAR_URL);
			free(url_complete_original_url);
			url_complete_original_url = malloc(strlen(url) + 1);
			if (!url_complete_original_url)
				return;
			strcpy(url_complete_original_url, url);
		}
		old_selection = url_complete_matches_selection;
		url_complete_matches_selection = selection;
		error = xwimp_force_redraw(dialog_url_complete,
				0, -(old_selection + 1) * 44, 65536, -old_selection * 44);
		if (error) {
			LOG(("xwimp_force_redraw: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		error = xwimp_force_redraw(dialog_url_complete,
				0, -(url_complete_matches_selection + 1) * 44,
				65536, -url_complete_matches_selection * 44);
		if (error) {
			LOG(("xwimp_force_redraw: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
	
	/* Select sets the text and launches */
	if ((pointer->buttons == wimp_CLICK_SELECT) && (buttons)) {
		g = ro_gui_window_lookup(url_complete_parent);
		if (!g)
			return;
		ro_gui_set_icon_string(g->toolbar->toolbar_handle,
				ICON_TOOLBAR_URL,
				url_complete_matches[url_complete_matches_selection]);
		browser_window_go(g->bw,
				url_complete_matches[url_complete_matches_selection],
				0);
		global_history_add_recent(url_complete_matches[url_complete_matches_selection]);
		ro_gui_url_complete_close(NULL, 0);

	/* Adjust just sets the text */
	} else if ((pointer->buttons == wimp_CLICK_ADJUST) && (buttons)) {
		g = ro_gui_window_lookup(url_complete_parent);
		if (!g)
			return;
		ro_gui_set_icon_string(g->toolbar->toolbar_handle,
				ICON_TOOLBAR_URL,
				url_complete_matches[url_complete_matches_selection]);
		ro_gui_url_complete_keypress(g, 0);
	}
}


/**
 * Dumps all matching URLs to stderr.
 */
void url_complete_dump_matches(const char *url) {
	char *match_url;
	struct url_data *reference = NULL;
	char *output;

	match_url = url_store_match_string(url);
	if (!match_url)
		return;

	fprintf(stderr, "\nDumping matches for '%s' ('%s'):\n", url, match_url);
	while ((output = url_store_match(match_url, &reference))) {
		fprintf(stderr, " - ");
		fprintf(stderr, output);
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "\nEnd of matches.\n\n");
	free(match_url);
}
