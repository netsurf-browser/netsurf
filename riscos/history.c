/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Browser history tree and window (implementation).
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <swis.h>
#include "oslib/colourtrans.h"
#include "oslib/font.h"
#include "oslib/wimp.h"
#include "netsurf/content/url_store.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/riscos/bitmap.h"
#include "netsurf/riscos/image.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/thumbnail.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

#define SIZE 10
#define WIDTH 200
#define HEIGHT 152
#define MARGIN 32
#define FULL_WIDTH (WIDTH + MARGIN + MARGIN)
#define FULL_HEIGHT (HEIGHT + MARGIN + MARGIN)
#define SPRITE_SIZE (16 + 44 + ((WIDTH / 2 + 3) & ~3) * HEIGHT / 2)

/** A node in the history tree. */
struct history_entry {
	char *url;    /**< Page URL. */
	char *frag_id; /** Fragment identifier */
	char *title;  /**< Page title. */
	struct history_entry *back;  /**< Parent. */
	struct history_entry *next;  /**< Next sibling. */
	struct history_entry *forward;  /**< First child. */
	struct history_entry *forward_pref;  /**< Child in direction of
	                                          current entry. */
	struct history_entry *forward_last;  /**< Last child. */
	unsigned int children;  /**< Number of children. */
	int x, y, width;
	struct bitmap *bitmap;	/**< Thumbnail bitmap, or 0. */
};

/** History tree for a window. */
struct history {
	/** First page in tree (page that window opened with). */
	struct history_entry *start;
	/** Current position in tree. */
	struct history_entry *current;
};

static struct browser_window *history_bw;
static struct history *history_current = 0;
/* Position of mouse in window */
static int mouse_x = 0, mouse_y = 0;
wimp_w history_window;
font_f history_font;

static void history_free_entry(struct history_entry *entry);
static void ro_gui_history_redraw_tree(struct history_entry *he,
		int x0, int y0);
static struct history_entry * ro_gui_history_click_find(
		struct history_entry *he,
		int x, int y);
static void history_go(struct browser_window *bw,
		struct history_entry *entry, bool new_window);


/**
 * Create a new history tree for a window.
 *
 * \return  pointer to an opaque history structure, 0 on failure.
 */

struct history *history_create(void)
{
	struct history *history;

	history = malloc(sizeof *history);
	if (!history) {
		warn_user("NoMemory", 0);
		return 0;
	}

	history->start = 0;
	history->current = 0;

	return history;
}


/**
 * Insert a url into the history tree.
 *
 * \param  history  opaque history structure, as returned by history_create()
 * \param  content  content to add to history
 * \param  frag_id  fragment identifier
 *
 * The page is added after the current entry and becomes current.
 */

void history_add(struct history *history, struct content *content, char *frag_id)
{
	url_func_result res;
	struct history_entry *entry;
	char *url;
	char *title;
	char *split;
	int width;
	struct bitmap *bitmap;

	if (!history)
		return;

	/* allocate space */
	entry = malloc(sizeof *entry);
	res = url_normalize(content->url, &url);
	if (res != URL_FUNC_OK) {
		warn_user("NoMemory", 0);
		return;
	}
	title = strdup(content->title ? content->title : url);
	if (!entry || !url || !title) {
		warn_user("NoMemory", 0);
		free(entry);
		free(url);
		free(title);
		return;
	}

	/* truncate title to available width */
	font_scan_string(history_font, title, font_GIVEN_FONT | font_KERN,
			WIDTH * 400, 0x7fffffff,
			0, 0, 0, &split, &width, 0, 0);
	if (title[split - title]) {
		title[split - title - 2] = 0x8c;  /* ellipsis */
		title[split - title - 1] = 0;
	}

	entry->url = url;
	entry->frag_id = frag_id ? strdup(frag_id) : 0;
	entry->title = title;
	entry->back = history->current;
	entry->next = 0;
	entry->forward = entry->forward_pref = entry->forward_last = 0;
	entry->children = 0;
	entry->width = width / 400;
	entry->bitmap = 0;
	if (history->current) {
		if (history->current->forward_last)
			history->current->forward_last->next = entry;
		else
			history->current->forward = entry;
		history->current->forward_pref = entry;
		history->current->forward_last = entry;
		history->current->children++;
	} else {
		history->start = entry;
	}
	history->current = entry;

	/* if we have a thumbnail, don't update until the page has finished
	 * loading */
	bitmap = url_store_get_thumbnail(url);
	bitmap = NULL;
	if (!bitmap) {
	 	bitmap = bitmap_create(WIDTH / 2, HEIGHT / 2, false);
  		if (!bitmap) {
			LOG(("Thumbnail initialisation failed."));
			return;
		}
		bitmap_set_opaque(bitmap, true);
		thumbnail_create(content, bitmap, url);
	}
	entry->bitmap = bitmap;
}


/**
 * Update the thumbnail for the current entry.
 *
 * \param  history  opaque history structure, as returned by history_create()
 * \param  content  content for current entry
 */

void history_update(struct history *history, struct content *content)
{
	if (!history || !history->current || !history->current->bitmap)
		return;

	thumbnail_create(content, history->current->bitmap, NULL);
}


/**
 * Free a history structure.
 *
 * \param  history  opaque history structure, as returned by history_create()
 */

void history_destroy(struct history *history)
{
	if (!history)
		return;
	if (history_current == history) {
		wimp_close_window(history_window);
		history_current = 0;
	}
	history_free_entry(history->start);
	free(history);
}


/**
 * Free an entry in the tree recursively.
 */

void history_free_entry(struct history_entry *entry)
{
	if (!entry)
		return;
	history_free_entry(entry->forward);
	history_free_entry(entry->next);
	free(entry->url);
	if (entry->frag_id)
		free(entry->frag_id);
	free(entry->title);
	free(entry);
}


/**
 * Create history window.
 */

void ro_gui_history_init(void)
{
	history_window = ro_gui_dialog_create("history");
	history_font = font_find_font("Homerton.Medium", 112, 128, 0, 0, 0, 0);
}


/**
 * Free history window resources.
 */

void ro_gui_history_quit(void)
{
	font_lose_font(history_font);
}


/**
 * Update resources folowing a mode change
 */
void ro_gui_history_mode_change(void)
{
	font_lose_font(history_font);
	history_font = font_find_font("Homerton.Medium", 112, 128, 0, 0, 0, 0);
}

/**
 * Open history window.
 *
 * \param pointer  whether to open the window at the pointer
 */

void ro_gui_history_open(struct browser_window *bw,
		struct history *history, bool pointer)
{
	bool done = false;
	unsigned int i, j, max_y = 0;
	int x;
	int width, height;
	struct history_entry *row[SIZE], *row2[SIZE];
	struct history_entry *he;
	os_box box = {0, 0, 0, 0};
	wimp_window_state state;

	if (!history || !history->start)
		return;

	history_bw = bw;
	history_current = history;

	/* calculate layout */
	for (i = 0; i != SIZE; i++)
		row[i] = row2[i] = 0;
	row[0] = history->start;
	history->start->x = 0;
	history->start->y = 0;
	for (x = 1; !done; x++) {
		for (i = 0; i != SIZE; i++) {
			if (row[i]) {
				for (j = i; j != SIZE && row2[j]; j++)
					;
				if (j == SIZE) {
					if (row[i]->forward)
						row[i]->forward->x = -1;
					break;
				}
				for (he = row[i]->forward; he; he = he->next) {
					row2[j++] = he;
					if (j == SIZE) {
						if (he->next)
							he->next->x = -1;
						break;
					}
				}
				if (j == SIZE)
					break;
			}
		}
		done = true;
		for (i = 0; i != SIZE; i++) {
			row[i] = row2[i];
			if (row[i]) {
				row[i]->x = x;
				row[i]->y = i;
				if (max_y < i)
					max_y = i;
				done = false;
			}
			row2[i] = 0;
		}
	}

	width = FULL_WIDTH * (x - 1);
	height = FULL_HEIGHT * (max_y + 1);

	box.x1 = width;
	box.y0 = -height;
	wimp_set_extent(history_window, &box);
	state.w = history_window;
	wimp_get_window_state(&state);
	state.visible.x0 = 0;
	state.visible.y0 = 0;
	state.visible.x1 = width;
	state.visible.y1 = height;
	state.next = wimp_HIDDEN;
	wimp_open_window((wimp_open *) &state);
	ro_gui_dialog_open_persistant(bw->window->window, history_window, pointer);
}


/**
 * Redraw history window.
 */

void ro_gui_history_redraw(wimp_draw *redraw)
{
	osbool more;

	more = wimp_redraw_window(redraw);

	while (more) {
		ro_gui_history_redraw_tree(history_current->start,
				redraw->box.x0 - redraw->xscroll,
				redraw->box.y1 - redraw->yscroll);
		more = wimp_get_rectangle(redraw);
	}
}


/**
 * Redraw history tree recursively.
 */

void ro_gui_history_redraw_tree(struct history_entry *he,
		int x0, int y0)
{
	struct history_entry *c;

/*	os_plot(os_MOVE_TO, x0 + he->x * FULL_WIDTH + MARGIN,
			y0 - he->y * FULL_HEIGHT - MARGIN);
	os_plot(os_PLOT_RECTANGLE | os_PLOT_BY, WIDTH, -HEIGHT);*/

	if (he == history_current->current)
		colourtrans_set_gcol(os_COLOUR_RED, 0, os_ACTION_OVERWRITE, 0);
	else
		colourtrans_set_gcol(os_COLOUR_MID_DARK_GREY, 0,
				os_ACTION_OVERWRITE, 0);

	os_plot(os_MOVE_TO, x0 + he->x * FULL_WIDTH + MARGIN - 1,
			y0 - he->y * FULL_HEIGHT - MARGIN);
	os_plot(os_PLOT_SOLID | os_PLOT_BY, WIDTH + 1, 0);
	os_plot(os_PLOT_SOLID | os_PLOT_BY, 0, -HEIGHT - 1);
	os_plot(os_PLOT_SOLID | os_PLOT_BY, -WIDTH - 1, 0);
	os_plot(os_PLOT_SOLID | os_PLOT_BY, 0, HEIGHT + 1);

	if (he->bitmap) {
  		if (bitmap_get_buffer(he->bitmap)) {
			image_redraw(he->bitmap->sprite_area,
					x0 + he->x * FULL_WIDTH + MARGIN,
					y0 - he->y * FULL_HEIGHT - MARGIN,
					he->bitmap->width, he->bitmap->height,
					he->bitmap->width, he->bitmap->height,
					0xffffff,
					false, false, false,
					IMAGE_PLOT_TINCT_OPAQUE);
		} else {
		  	url_store_add_thumbnail(he->url, NULL);
		  	he->bitmap = NULL;
		  
		}
	}

	if (he == history_current->current)
		wimp_set_font_colours(wimp_COLOUR_WHITE, wimp_COLOUR_RED);
	else
		wimp_set_font_colours(wimp_COLOUR_WHITE, wimp_COLOUR_BLACK);

	xfont_paint(history_font, he->title,
			font_OS_UNITS | font_GIVEN_FONT | font_KERN,
			x0 + he->x * FULL_WIDTH + (FULL_WIDTH - he->width) / 2,
			y0 - he->y * FULL_HEIGHT - HEIGHT - MARGIN - 24,
			NULL, NULL, 0);

	colourtrans_set_gcol(os_COLOUR_MID_DARK_GREY, 0,
			os_ACTION_OVERWRITE, 0);

	for (c = he->forward; c; c = c->next) {
		if (c->x == -1)
			continue;
		os_plot(os_MOVE_TO, x0 + c->x * FULL_WIDTH - MARGIN,
				y0 - he->y * FULL_HEIGHT - FULL_HEIGHT / 2);
		os_plot(os_PLOT_SOLID | os_PLOT_TO,
				x0 + c->x * FULL_WIDTH + MARGIN,
				y0 - c->y * FULL_HEIGHT - FULL_HEIGHT / 2);
		ro_gui_history_redraw_tree(c, x0, y0);
	}
}

/**
 * Handle mouse movements over the history window.
 */
void ro_gui_history_mouse_at(wimp_pointer *pointer)
{
	int x, y;
	long width;
	struct history_entry *he;
	wimp_window_state state;
	wimp_icon_state ic;
	os_box box = {0, 0, 0, 0};

	/* If the mouse hasn't moved, or if we don't want tooltips, exit */
	if ((mouse_x == pointer->pos.x && mouse_y == pointer->pos.y) ||
			!option_history_tooltip)
		return;

	/* Update mouse position */
	mouse_x = pointer->pos.x;
	mouse_y = pointer->pos.y;

	/* Find history tree entry under mouse */
	state.w = history_window;
	wimp_get_window_state(&state);

	x = (pointer->pos.x - (state.visible.x0 - state.xscroll)) / FULL_WIDTH;
	y = -(pointer->pos.y - (state.visible.y1 - state.yscroll)) / FULL_HEIGHT;
	he = ro_gui_history_click_find(history_current->start, x, y);
	if (he) {
		/* get width of string */
		xwimptextop_string_width(he->url,
			strlen(he->url) > 256 ? 256 : strlen(he->url),
				(int*)&width);

		ro_gui_set_icon_string(dialog_tooltip, 0, he->url);

		/* resize icon appropriately */
		ic.w = dialog_tooltip;
		ic.i = 0;
		wimp_get_icon_state(&ic);
		wimp_resize_icon(dialog_tooltip, 0,
				ic.icon.extent.x0, ic.icon.extent.y0,
				width + 16, ic.icon.extent.y1);

		state.w = dialog_tooltip;
		wimp_get_window_state(&state);

		/* update window extent */
		box.x1 = width + 16;
		box.y0 = -36;
		xwimp_set_extent(dialog_tooltip, &box);

		/* set visible area */
		state.visible.x0 = pointer->pos.x + 24;
		state.visible.y0 = pointer->pos.y - 22 - 36;
		state.visible.x1 = pointer->pos.x + 24 + width + 16;
		state.visible.y1 = pointer->pos.y - 22;
		state.next = wimp_TOP;
		/* open window */
		wimp_open_window((wimp_open *) &state);
	}
	else {
	        /* not over a tree entry => close tooltip window. */
	        wimp_close_window(dialog_tooltip);
	}
}

/**
 * Handle mouse clicks in the history window.
 */

void ro_gui_history_click(wimp_pointer *pointer)
{
	int x, y;
	struct history_entry *he;
	wimp_window_state state;

	if (pointer->buttons != wimp_CLICK_SELECT &&
			pointer->buttons != wimp_CLICK_ADJUST)
		/* return if not select or adjust click */
		return;

	state.w = history_window;
	wimp_get_window_state(&state);

	x = (pointer->pos.x - (state.visible.x0 - state.xscroll)) / FULL_WIDTH;
	y = -(pointer->pos.y - (state.visible.y1 - state.yscroll)) / FULL_HEIGHT;
	he = ro_gui_history_click_find(history_current->start, x, y);
	if (he) {
		if (pointer->buttons == wimp_CLICK_SELECT)
			history_current->current = he;
		wimp_close_window(history_window);
		history_current = 0;
		history_go(history_bw, he,
				pointer->buttons == wimp_CLICK_ADJUST);
	}
}


/**
 * Search for an entry with the specified coordinates.
 */

struct history_entry * ro_gui_history_click_find(struct history_entry *he,
		int x, int y)
{
	struct history_entry *c, *d;
	if (he->x == x && he->y == y)
		return he;
	for (c = he->forward; c; c = c->next) {
		d = ro_gui_history_click_find(c, x, y);
		if (d)
			return d;
	}
	return 0;
}


/**
 * Go back in the history.
 *
 * \param  bw       browser window
 * \param  history  history of the window
 */

void history_back(struct browser_window *bw, struct history *history)
{
	if (!history || !history->current || !history->current->back)
		return;
	history->current = history->current->back;
	history_go(bw, history->current, false);
}


/**
 * Go forward in the history.
 *
 * \param  bw       browser window
 * \param  history  history of the window
 */

void history_forward(struct browser_window *bw, struct history *history)
{
	if (!history || !history->current || !history->current->forward_pref)
		return;
	history->current = history->current->forward_pref;
	history_go(bw, history->current, false);
}


/**
 * Check whether it is pssible to go back in the history.
 *
 * \param  history  history of the window
 * \return true if the history can go back, false otherwise
 */

bool history_back_available(struct history *history) {
	return (history && history->current && history->current->back);
}


/**
 * Check whether it is pssible to go forwards in the history.
 *
 * \param  history  history of the window
 * \return true if the history can go forwards, false otherwise
 */

bool history_forward_available(struct history *history) {
	return (history && history->current && history->current->forward_pref);
}

/**
 * Open a history entry in the specified browser window
 *
 * \param bw          browser window
 * \param entry       entry to open
 * \param new_window  open entry in new window
 */
void history_go(struct browser_window *bw, struct history_entry *entry,
		bool new_window)
{
	char *url;

	if (entry->frag_id) {
		url = calloc(strlen(entry->url) + strlen(entry->frag_id) + 5,
							sizeof(char));
		if (!url) {
			warn_user("NoMemory", 0);
			return;
		}
		sprintf(url, "%s#%s", entry->url, entry->frag_id);
	}
	else
		url = entry->url;

	if (new_window)
		browser_window_create(url, bw, 0);
	else
		browser_window_go_post(bw, url, 0, 0, false, 0, false);

	if (entry->frag_id)
		free(url);
}
