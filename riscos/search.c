/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <string.h>

#include "oslib/wimp.h"

#include "netsurf/utils/config.h"
#include "netsurf/content/content_type.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/box.h"
#include "netsurf/render/html.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


#ifdef WITH_SEARCH

struct list_entry {
	struct box *box;
	struct list_entry *prev;
	struct list_entry *next;
};

static char *search_string = 0;
static struct list_entry head = { 0, 0, 0 };
static struct list_entry *found = &head;
static struct list_entry *current = 0;
static struct gui_window *w = 0;
static bool prev_from_top = false;
static bool prev_case_sens = false;

static void do_search(struct gui_window *g, char *string, bool from_top, bool case_sens, bool forwards);
static bool find_occurrences(char *string, struct box *cur, bool case_sens);
static char * strcasestr(char *s1, char *s2);


/**
 * Handle clicks in the search dialog
 *
 * \param pointer wimp_pointer block
 * \param parent The parent window of this persistent dialog
 */
void ro_gui_search_click(wimp_pointer *pointer, wimp_w parent)
{
	struct gui_window *g;
	char *string;

	if (pointer->buttons == wimp_CLICK_MENU)
		return;

	switch(pointer->i) {
		case ICON_SEARCH_FORWARDS:
		case ICON_SEARCH_BACKWARDS:
			/* prevent deselection on adjust clicking */
			if (pointer->buttons == wimp_CLICK_ADJUST)
				ro_gui_set_icon_selected_state(dialog_search,
						pointer->i, true);
			break;
		case ICON_SEARCH_FIND:
			g = ro_gui_window_lookup(parent);
			string = ro_gui_get_icon_string(pointer->w,
						ICON_SEARCH_TEXT);
			if (!g || strlen(string) == 0)
				return;
			do_search(g, string,
				ro_gui_get_icon_selected_state(pointer->w,
						ICON_SEARCH_START),
				ro_gui_get_icon_selected_state(pointer->w,
						ICON_SEARCH_CASE_SENSITIVE),
				ro_gui_get_icon_selected_state(pointer->w,
						ICON_SEARCH_FORWARDS));
			break;
		case ICON_SEARCH_CANCEL:
			ro_gui_dialog_close(dialog_search);
			break;
	}
}

/**
 * Prepare the search dialog for display
 */
void ro_gui_search_prepare(void)
{
	ro_gui_set_icon_string(dialog_search, ICON_SEARCH_TEXT, "");
	ro_gui_set_icon_selected_state(dialog_search, ICON_SEARCH_FORWARDS,
				true);
	ro_gui_set_icon_selected_state(dialog_search, ICON_SEARCH_BACKWARDS,
				false);
	ro_gui_set_icon_selected_state(dialog_search, ICON_SEARCH_START,
				false);
	ro_gui_set_icon_selected_state(dialog_search,
				ICON_SEARCH_CASE_SENSITIVE, false);
}

/**
 * Search for a string in the box tree
 *
 * \param g gui_window which contains content to search
 * \param string the string to search for
 * \param from_top whether to display results from the top of the page, or
 *                 the current scroll position
 * \param case_sens whether to perform a case sensitive search
 * \param forwards direction to search in
 */
void do_search(struct gui_window *g, char *string, bool from_top, bool case_sens, bool forwards)
{
	struct content *c;
	struct box *box;
	struct list_entry *a, *b;
	int x,y;
	bool new = false;

	if (!g)
		return;

	c = g->bw->current_content;

	/* only handle html contents */
	if (c->type != CONTENT_HTML)
		return;

	box = c->data.html.layout->children;

	if (!box)
		return;

//	LOG(("'%s' - '%s' (%p, %p) %p (%d, %d) (%d, %d) %d", search_string, string, w, g, found->next, prev_from_top, from_top, prev_case_sens, case_sens, forwards));

	/* check if we need to start a new search or continue an old one */
	if (!search_string || !w || g != w || !found->next ||
	    prev_from_top != from_top || prev_case_sens != case_sens ||
	    (case_sens && strcmp(string, search_string) != 0) ||
	    (!case_sens && strcasecmp(string, search_string) != 0)) {
		if (search_string)
			free(search_string);
		search_string = strdup(string);
		current = 0;
		for (a = found->next; a; a = b) {
			b = a->next;
			free(a);
		}
		found->prev = 0;
		found->next = 0;
		if (!find_occurrences(string, box, case_sens)) {
			for (a = found->next; a; a = b) {
				b = a->next;
				free(a);
			}
			found->prev = 0;
			found->next = 0;
			return;
		}
		new = true;
		w = g;
		prev_from_top = from_top;
		prev_case_sens = case_sens;
	}

//	LOG(("%d %p %p (%p, %p)", new, found->next, current, current->prev, current->next));

	if (!found->next)
		return;

	if (new && from_top) {
		/* new search, beginning at the top of the page */
		current = found->next;
	}
	else if (new) {
		/* new search, beginning from user's current scroll
		 * position */
		wimp_window_state state;
		os_error *error;

		state.w = g->window;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

		for (a = found->next; a; a = a->next) {
			box_coords(a->box, &x, &y);
			LOG(("%d, %d", y, state.yscroll / 2));
			if (forwards && -y <= state.yscroll / 2)
				break;
			if (!forwards && -y >= state.yscroll / 2)
				break;
		}

		if (a)
			current = a;
		else
			return;
	}
	else {
		/* continued search in the direction specified */
		if (forwards && current && current->next) {
			current = current->next;
		}
		else if (!forwards && current && current->prev) {
			current = current->prev;
		}
	}

	if (!current)
		return;

	/* get box position and jump to it */
	box_coords(current->box, &x, &y);
//	LOG(("%p (%d, %d)", current, x, y));
	gui_window_set_scroll(g, x, y);

}

/**
 * Finds all occurrences of a given string in the box tree
 *
 * \param string the string to search for
 * \param cur pointer to the current box
 * \param case_sens whether to perform a case sensitive search
 * \return true on success, false on memory allocation failure
 */
bool find_occurrences(char *string, struct box *cur, bool case_sens)
{
	struct box *a;
	char *pos, *buf;
	struct list_entry *entry;

	/* ignore this box, if there's no visible text */
	if (!cur->object && cur->text) {
		buf = strndup(cur->text, cur->length);
		if (case_sens)
			pos = strstr(buf, string);
		else
			pos = strcasestr(buf, string);
		free(buf);
		if (pos) {
			/* found string in box => add to list */
			entry = calloc(1, sizeof(*entry));
			if (!entry) {
				warn_user("NoMemory", 0);
				return false;
			}
			entry->box = cur;
			entry->next = 0;
			entry->prev = found->prev;
			if (!found->prev)
				found->next = entry;
			else
				found->prev->next = entry;
			found->prev = entry;
		}
	}

	/* and recurse */
	for (a = cur->children; a; a = a->next) {
		if (a->type != BOX_FLOAT_LEFT && a->type != BOX_FLOAT_RIGHT)
			if (!find_occurrences(string, a, case_sens))
				return false;
	}

	for (a = cur->float_children; a; a = a->next_float) {
		if (a->type != BOX_FLOAT_LEFT && a->type != BOX_FLOAT_RIGHT)
			if (!find_occurrences(string, a, case_sens))
				return false;
	}

	return true;
}

/* case insensitive strstr */
char * strcasestr(char *s1, char *s2)
{
	int l1 = strlen(s1), l2 = strlen(s2);
	char *e1 = s1 + l1 - l2;

	while (s1 <= e1) {
		if (!strncasecmp(s1, s2, l2))
			return s1;
		s1++;
	}

	return 0;
}

#endif
