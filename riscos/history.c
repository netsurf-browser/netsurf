/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/thumbnail.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#define SIZE 10
#define WIDTH 200
#define HEIGHT 150
#define MARGIN 32
#define FULL_WIDTH (WIDTH + MARGIN + MARGIN)
#define FULL_HEIGHT (HEIGHT + MARGIN + MARGIN)
#define SPRITE_SIZE (16 + 44 + ((WIDTH / 2 + 3) & ~3) * HEIGHT / 2)

/** A node in the history tree. */
struct history_entry {
	char *url;    /**< Page URL. */
	char *title;  /**< Page title. */
	struct history_entry *back;  /**< Parent. */
	struct history_entry *next;  /**< Next sibling. */
	struct history_entry *forward;  /**< First child. */
	struct history_entry *forward_pref;  /**< Child in direction of
	                                          current entry. */
	struct history_entry *forward_last;  /**< Last child. */
	unsigned int children;  /**< Number of children. */
	int x, y, width;
	osspriteop_area *sprite_area;  /**< Thumbnail sprite area, or 0. */
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
wimp_w history_window;
font_f history_font;

static void history_free_entry(struct history_entry *entry);
static void ro_gui_history_redraw_tree(struct history_entry *he,
		int x0, int y0);
static struct history_entry * ro_gui_history_click_find(struct history_entry *he,
		int x, int y);


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
		warn_user("NoMemory");
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
 *
 * The page is added after the current entry and becomes current.
 */

void history_add(struct history *history, struct content *content)
{
	struct history_entry *entry;
	char *url;
	char *title;
	char *split;
	int width;
	osspriteop_area *area;
	os_error *error;

	if (!history)
		return;

	/* allocate space */
	entry = malloc(sizeof *entry);
	url = strdup(content->url);
	title = strdup(content->title ? content->title : url);
	if (!entry || !url || !title) {
		warn_user("NoMemory");
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
	entry->title = title;
	entry->back = history->current;
	entry->next = 0;
	entry->forward = entry->forward_pref = entry->forward_last = 0;
	entry->children = 0;
	entry->width = width / 400;
	entry->sprite_area = 0;
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

	area = malloc(SPRITE_SIZE);
	if (!area) {
		LOG(("malloc failed"));
		return;
	}

	area->size = SPRITE_SIZE;
	area->sprite_count = 0;
	area->first = 16;
	area->used = 16;

	error = xosspriteop_create_sprite(osspriteop_NAME,
			area, "thumbnail", false,
			WIDTH / 2, HEIGHT / 2, os_MODE8BPP90X90);
	if (error) {
		LOG(("0x%x: %s", error->errnum, error->errmess));
		return;
	}

	thumbnail_create(content, area,
			(osspriteop_header *) (area + 1),
			WIDTH / 2, HEIGHT / 2);
/*	xosspriteop_save_sprite_file(osspriteop_NAME,
			area, "thumbnail");*/

	entry->sprite_area = area;
}


/**
 * Update the thumbnail for the current entry.
 *
 * \param  history  opaque history structure, as returned by history_create()
 * \param  content  content for current entry
 */

void history_update(struct history *history, struct content *content)
{
	if (!history || !history->current->sprite_area)
		return;

	thumbnail_create(content, history->current->sprite_area,
			(osspriteop_header *)
				(history->current->sprite_area + 1),
			WIDTH / 2, HEIGHT / 2);
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
	free(entry->title);
	free(entry->sprite_area);
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
 * Open history window.
 */

void ro_gui_history_open(struct browser_window *bw,
		struct history *history, int wx, int wy)
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
	state.visible.x0 = wx - width / 2;
	state.visible.y0 = wy - height / 2;
	state.visible.x1 = wx + width / 2;
	state.visible.y1 = wy + height / 2;
	state.next = wimp_TOP;
	wimp_open_window((wimp_open *) &state);
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

	if (he->sprite_area) {
		osspriteop_area *area = he->sprite_area;
		osspriteop_header *header = (osspriteop_header *)(area + 1);
		osspriteop_trans_tab *table;

		/* 	Because we're supporting people with OS3.1 we need to check if the
			sprite we have is a legacy 256 colour one
		*/
		if (header->mode == (os_mode)0x301680b5) {
			
			/*	We plot with no mask and no scaling as any EIG factors are
				handled internally by Tinct
			*/
			_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
				(char *)(header),
				x0 + he->x * FULL_WIDTH + MARGIN,
				y0 - he->y * FULL_HEIGHT - FULL_HEIGHT + MARGIN,
				(option_filter_sprites?(1<<1):0) | (option_dither_sprites?(1<<2):0));
		} else {
		        unsigned int size;
			os_factors factors;

			xcolourtrans_generate_table_for_sprite(
					area, (osspriteop_id)header,
					colourtrans_CURRENT_MODE, colourtrans_CURRENT_PALETTE,
					0, colourtrans_GIVEN_SPRITE, 0, 0, &size);
			table = xcalloc(size, 1);
			xcolourtrans_generate_table_for_sprite(
					area, (osspriteop_id)header,
					colourtrans_CURRENT_MODE, colourtrans_CURRENT_PALETTE,
					table, colourtrans_GIVEN_SPRITE, 0, 0, 0);

			factors.xmul = 1;
			factors.ymul = 1;
			factors.xdiv = 1;
			factors.ydiv = 1;

			xosspriteop_put_sprite_scaled(osspriteop_PTR,
					area, (osspriteop_id)header,
					x0 + he->x * FULL_WIDTH + MARGIN,
					y0 - he->y * FULL_HEIGHT - FULL_HEIGHT + MARGIN,
					osspriteop_USE_MASK | osspriteop_USE_PALETTE, &factors, table);

			xfree(table);
		}
	}

/*
	if (he == history_current->current)
		wimp_set_font_colours(wimp_COLOUR_WHITE, wimp_COLOUR_RED);
	else
		wimp_set_font_colours(wimp_COLOUR_WHITE, wimp_COLOUR_BLACK);

	font_paint(history_font, he->title,
			font_OS_UNITS | font_GIVEN_FONT | font_KERN,
			x0 + he->x * FULL_WIDTH + (FULL_WIDTH - he->width) / 2,
			y0 - he->y * FULL_HEIGHT - MARGIN - 24,
			0, 0, 0);
*/

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
 * Handle mouse clicks in the history window.
 */

void ro_gui_history_click(wimp_pointer *pointer)
{
	int x, y;
	struct history_entry *he;
	wimp_window_state state;

	state.w = history_window;
	wimp_get_window_state(&state);

	x = (pointer->pos.x - (state.visible.x0 - state.xscroll)) / FULL_WIDTH;
	y = -(pointer->pos.y - (state.visible.y1 - state.yscroll)) / FULL_HEIGHT;
	LOG(("x = %i, y = %i", x, y));
	he = ro_gui_history_click_find(history_current->start, x, y);
	if (he) {
		history_current->current = he;
		wimp_close_window(history_window);
		history_current = 0;
		browser_window_go_post(history_bw, he->url, 0, 0, false);
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
	if (!history || !history->current->back)
		return;
	history->current = history->current->back;
	browser_window_go_post(bw, history->current->url, 0, 0, false);
}


/**
 * Go forward in the history.
 *
 * \param  bw       browser window
 * \param  history  history of the window
 */

void history_forward(struct browser_window *bw, struct history *history)
{
	if (!history || !history->current->forward_pref)
		return;
	history->current = history->current->forward_pref;
	browser_window_go_post(bw, history->current->url, 0, 0, false);
}
