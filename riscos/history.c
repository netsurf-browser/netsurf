/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include <stdbool.h>
#include <stdlib.h>
#include "oslib/colourtrans.h"
#include "oslib/font.h"
#include "oslib/wimp.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#define SIZE 10
#define WIDTH 300
#define HEIGHT 100
#define MARGIN 32

/** A node in the history tree. */
struct history_entry {
	char *url;		/**< Page URL. */
	char *title;		/**< Page title. */
	struct history_entry *back;
	struct history_entry *forward, *forward_pref, *forward_last;
	struct history_entry *prev, *next;
	unsigned int children;
	int x, y, width;
};

static struct browser_window *history_bw;
static struct history_entry *history_start;
static struct history_entry *history_current;
wimp_w history_window;
font_f history_font;

static void ro_gui_history_redraw_tree(struct history_entry *he,
		int x0, int y0);
static struct history_entry * ro_gui_history_click_find(struct history_entry *he,
		int x, int y);


/**
 * Insert a url into the history tree.
 *
 * Takes a copy of the url and title and inserts an entry into the tree forward
 * from current, returning the new entry. current may be 0 to start a new tree.
 */

struct history_entry * history_add(struct history_entry *current,
		char *url, char *title)
{
	struct history_entry *entry = xcalloc(1, sizeof(*entry));
	char *split;
	int width;

	font_scan_string(history_font, title, font_GIVEN_FONT | font_KERN,
			(WIDTH - MARGIN - MARGIN) * 400, 0x7fffffff,
			0, 0, 0, &split, &width, 0, 0);

	entry->url = xstrdup(url);
	entry->title = xstrdup(title);
	if (entry->title[split - title]) {
		entry->title[split - title - 2] = 0x8c;  /* ellipsis */
		entry->title[split - title - 1] = 0;
	}
	entry->back = current;
	entry->forward = entry->forward_pref = entry->forward_last = 0;
	entry->next = 0;
	entry->children = 0;
	entry->width = width / 400;
	if (current) {
		entry->prev = current->forward_last;
		if (current->forward_last)
			current->forward_last->next = entry;
		else
			current->forward = entry;
		current->forward_pref = current->forward_last = entry;
		current->children++;
	} else {
		entry->prev = 0;
	}
	return entry;
}


/**
 * Create history window.
 */

void ro_gui_history_init(void)
{
	history_window = ro_gui_dialog_create("history");
	history_font = font_find_font("Homerton.Medium", 128, 128, 0, 0, 0, 0);
}


/**
 * Free history window resources.
 */

void ro_gui_history_quit(void)
{
	font_lose_font(history_font);
}


/**
 * Open history window at a specified entry.
 */

void ro_gui_history_open(struct browser_window *bw,
		struct history_entry *entry, int wx, int wy)
{
	bool done = false;
	unsigned int i, j, max_y = 0;
	int x;
	struct history_entry *row[SIZE], *row2[SIZE];
	struct history_entry *he;
	os_box box = {0, 0, 0, 0};

	history_bw = bw;
	history_current = entry;
	for (history_start = entry;
			history_start->back;
			history_start = history_start->back)
		;

	/* calculate layout */
	for (i = 0; i != SIZE; i++)
		row[i] = row2[i] = 0;
	row[0] = history_start;
	history_start->x = 0;
	history_start->y = 0;
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

	box.x1 = WIDTH * (x - 1);
	box.y0 = -(HEIGHT * (max_y + 1));
	wimp_set_extent(history_window, &box);
	wimp_create_menu((wimp_menu *) history_window, wx, wy);
}


/**
 * Redraw history window.
 */

void ro_gui_history_redraw(wimp_draw *redraw)
{
	osbool more;

	more = wimp_redraw_window(redraw);
	colourtrans_set_gcol(os_COLOUR_WHITE, 0, os_ACTION_OVERWRITE, 0);

	while (more) {
		ro_gui_history_redraw_tree(history_start,
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

	os_plot(os_MOVE_TO, x0 + he->x * WIDTH + MARGIN,
			y0 - he->y * HEIGHT - MARGIN);
	os_plot(os_PLOT_RECTANGLE | os_PLOT_BY, WIDTH - MARGIN - MARGIN,
			-(HEIGHT - MARGIN - MARGIN));

	if (he == history_current)
		wimp_set_font_colours(wimp_COLOUR_WHITE, wimp_COLOUR_RED);
	else
		wimp_set_font_colours(wimp_COLOUR_WHITE, wimp_COLOUR_BLACK);

	font_paint(history_font, he->title,
			font_OS_UNITS | font_GIVEN_FONT | font_KERN,
			x0 + he->x * WIDTH + (WIDTH - he->width) / 2,
			y0 - he->y * HEIGHT - MARGIN - 24,
			0, 0, 0);

	for (c = he->forward; c; c = c->next) {
		if (c->x == -1)
			continue;
		os_plot(os_MOVE_TO, x0 + c->x * WIDTH - MARGIN,
				y0 - he->y * HEIGHT - HEIGHT / 2);
		os_plot(os_PLOT_SOLID | os_PLOT_TO,
				x0 + c->x * WIDTH + MARGIN,
				y0 - c->y * HEIGHT - HEIGHT / 2);
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

	x = (pointer->pos.x - (state.visible.x0 - state.xscroll)) / WIDTH;
	y = -(pointer->pos.y - (state.visible.y1 - state.yscroll)) / HEIGHT;
	LOG(("x = %i, y = %i", x, y));
	he = ro_gui_history_click_find(history_start, x, y);
	if (he) {
		history_bw->history_entry = he;
		wimp_create_menu(wimp_CLOSE_MENU, 0, 0);
		browser_window_open_location_historical(history_bw,
				he->url, 0, 0);
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

