/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Free text search (implementation)
 */

#include <ctype.h>
#include <string.h>

#include "oslib/wimp.h"

#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/box.h"
#include "netsurf/render/html.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


#ifdef WITH_SEARCH

#ifndef NOF_ELEMENTS
#define NOF_ELEMENTS(array) (sizeof(array)/sizeof(*(array)))
#endif

struct list_entry {
	struct box *box;
	struct list_entry *prev;
	struct list_entry *next;
};

static struct gui_window *search_current_window = 0;

static char *search_string = 0;
static struct list_entry search_head = { 0, 0, 0 };
static struct list_entry *search_found = &search_head;
static struct list_entry *search_current = 0;
static struct content *search_content = 0;
static bool search_prev_from_top = false;
static bool search_prev_case_sens = false;

static void start_search(void);
static void end_search(void);
static void do_search(char *string, bool from_top, bool case_sens, bool forwards);
static const char *find_pattern(const char *string, int s_len, const char *pattern, int p_len, bool case_sens);
static bool find_occurrences(const char *pattern, int p_len, struct box *cur, bool case_sens);


/**
 * Open the search dialog
 *
 * \param g the gui window to search
 * \param x x position, for sub_menu true only
 * \param y y position, for sub_menu true only
 * \param sub_menu open as a sub_menu, otherwise persistent
 * \param keypress whether opened by a keypress or not
 */
void ro_gui_search_open(struct gui_window *g, int x, int y, bool sub_menu, bool keypress)
{
	os_error *e;

	assert(g != NULL);

	search_current_window = g;

	ro_gui_set_icon_string(dialog_search, ICON_SEARCH_TEXT, "");
	ro_gui_set_icon_selected_state(dialog_search, ICON_SEARCH_FORWARDS,
				true);
	ro_gui_set_icon_selected_state(dialog_search, ICON_SEARCH_BACKWARDS,
				false);
	ro_gui_set_icon_selected_state(dialog_search, ICON_SEARCH_START,
				false);
	ro_gui_set_icon_selected_state(dialog_search,
				ICON_SEARCH_CASE_SENSITIVE, false);

	if (sub_menu) {
		e = xwimp_create_sub_menu((wimp_menu *) dialog_search, x, y);
		if (e) {
			LOG(("xwimp_create_sub_menu: 0x%x: %s",
					e->errnum, e->errmess));
			warn_user("MenuError", e->errmess);
		}
	}
	else {
		ro_gui_dialog_open_persistant(g->window, dialog_search, !keypress);
	}
}

/**
 * Handle clicks in the search dialog
 *
 * \param pointer wimp_pointer block
 * \param parent The parent window of this persistent dialog
 */
void ro_gui_search_click(wimp_pointer *pointer)
{
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
			start_search();
			break;
		case ICON_SEARCH_CANCEL:
			end_search();
			break;
	}
}

/**
 * Handle keypresses in the search dialog
 *
 * \param key wimp_key block
 * \return true if keypress handled, false otherwise
 */
bool ro_gui_search_keypress(wimp_key *key)
{
	bool state;

	switch (key->c) {
		case 2: /* ctrl b */
			ro_gui_set_icon_selected_state(dialog_search, ICON_SEARCH_FORWARDS, false);
			ro_gui_set_icon_selected_state(dialog_search, ICON_SEARCH_BACKWARDS, true);
			return true;
		case 6: /* ctrl f */
			ro_gui_set_icon_selected_state(dialog_search, ICON_SEARCH_FORWARDS, true);
			ro_gui_set_icon_selected_state(dialog_search, ICON_SEARCH_BACKWARDS, false);
			return true;
		case 9: /* ctrl i */
			state = ro_gui_get_icon_selected_state(dialog_search, ICON_SEARCH_CASE_SENSITIVE);
			ro_gui_set_icon_selected_state(dialog_search, ICON_SEARCH_CASE_SENSITIVE, !state);
			return true;
		case 19: /* ctrl s */
			state = ro_gui_get_icon_selected_state(dialog_search, ICON_SEARCH_START);
			ro_gui_set_icon_selected_state(dialog_search, ICON_SEARCH_START, !state);

			return true;
		case wimp_KEY_RETURN:
			start_search();
			return true;
		case wimp_KEY_ESCAPE:
			end_search();
			return true;
	}

	return false;
}

/**
 * Begins the search process
 */
void start_search(void)
{
	char *string;

	string = ro_gui_get_icon_string(dialog_search, ICON_SEARCH_TEXT);
	if (strlen(string) == 0)
		return;
	do_search(string,
		ro_gui_get_icon_selected_state(dialog_search,
						ICON_SEARCH_START),
		ro_gui_get_icon_selected_state(dialog_search,
						ICON_SEARCH_CASE_SENSITIVE),
		ro_gui_get_icon_selected_state(dialog_search,
						ICON_SEARCH_FORWARDS));
}

/**
 * Ends the search process, invalidating all global state and
 * freeing the list of found boxes
 */
void end_search(void)
{
	struct list_entry *a, *b;

	search_current_window = 0;

	if (search_string)
		free(search_string);
	search_string = 0;

	for (a = search_found->next; a; a = b) {
		b = a->next;
		free(a);
	}
	search_found->prev = 0;
	search_found->next = 0;

	search_current = 0;

	search_content = 0;

	search_prev_from_top = false;
	search_prev_case_sens = false;

	/* and close the window */
	xwimp_create_menu((wimp_menu *)-1, 0, 0);
	ro_gui_dialog_close(dialog_search);
}

/**
 * Search for a string in the box tree
 *
 * \param string the string to search for
 * \param from_top whether to display results from the top of the page, or
 *                 the current scroll position
 * \param case_sens whether to perform a case sensitive search
 * \param forwards direction to search in
 */
void do_search(char *string, bool from_top, bool case_sens, bool forwards)
{
	struct content *c;
	struct box *box;
	struct list_entry *a, *b;
	int x,y;
	bool new = false;

	if (!search_current_window)
		return;

	c = search_current_window->bw->current_content;

	if (!c)
		return;

	/* only handle html contents */
	if (c->type != CONTENT_HTML)
		return;

	box = c->data.html.layout;

	if (!box)
		return;

//	LOG(("'%s' - '%s' (%p, %p) %p (%d, %d) (%d, %d) %d", search_string, string, search_content, c, search_found->next, search_prev_from_top, from_top, search_prev_case_sens, case_sens, forwards));

	/* check if we need to start a new search or continue an old one */
	if (!search_string || c != search_content ||
	    !search_found->next || search_prev_from_top != from_top ||
	    search_prev_case_sens != case_sens ||
	    (case_sens && strcmp(string, search_string) != 0) ||
	    (!case_sens && strcasecmp(string, search_string) != 0)) {
		if (search_string)
			free(search_string);
		search_string = strdup(string);
		search_current = 0;
		for (a = search_found->next; a; a = b) {
			b = a->next;
			free(a);
		}
		search_found->prev = 0;
		search_found->next = 0;
		if (!find_occurrences(string, strlen(string), box, case_sens)) {
			for (a = search_found->next; a; a = b) {
				b = a->next;
				free(a);
			}
			search_found->prev = 0;
			search_found->next = 0;
			return;
		}
		new = true;
		search_content = c;
		search_prev_from_top = from_top;
		search_prev_case_sens = case_sens;
	}

//	LOG(("%d %p %p (%p, %p)", new, search_found->next, search_current, search_current->prev, search_current->next));

	if (!search_found->next)
		return;

	if (new && from_top) {
		/* new search, beginning at the top of the page */
		search_current = search_found->next;
	}
	else if (new) {
		/* new search, beginning from user's current scroll
		 * position */
		wimp_window_state state;
		os_error *error;

		state.w = search_current_window->window;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

		for (a = search_found->next; a; a = a->next) {
			box_coords(a->box, &x, &y);
			LOG(("%d, %d", y, state.yscroll / 2));
			if (forwards && -y <= state.yscroll / 2)
				break;
			if (!forwards && -y >= state.yscroll / 2)
				break;
		}

		if (a)
			search_current = a;
		else
			return;
	}
	else {
		/* continued search in the direction specified */
		if (forwards && search_current && search_current->next) {
			search_current = search_current->next;
		}
		else if (!forwards && search_current && search_current->prev) {
			search_current = search_current->prev;
		}
	}

	if (!search_current)
		return;

	/* get box position and jump to it */
	box_coords(search_current->box, &x, &y);
//	LOG(("%p (%d, %d)", search_current, x, y));
	gui_window_set_scroll(search_current_window, x, y);

}


/**
 * Find the first occurrence of 'match' in 'string' and return its index
 *
 * /param string   the string to be searched (unterminated)
 * /param s_len    length of the string to be searched
 * /param match    the pattern for which we are searching (unterminated)
 * /param m_len    length of pattern
 * /return         pointer to first match, NULL if none
 */

const char *find_pattern(const char *string, int s_len, const char *pattern, int p_len, bool case_sens)
{
	struct { const char *ss, *s, *p; } context[16];
	const char *ep = pattern + p_len;
	const char *es = string  + s_len;
	const char *p = pattern - 1;  /* a virtual '*' before the pattern */
	const char *ss = string;
	const char *s = string;
	int top = 0;

	while (p < ep) {
		bool matches;
		if (p < pattern || *p == '*') {
			char ch;

			/* skip any further asterisks; one is the same as many */
			do p++; while (p < ep && *p == '*');

			/* if we're at the end of the pattern, yes, it matches */
			if (p >= ep) return ss;

			/* anything matches a # so continue matching from
			   here, and stack a context that will try to match
			   the wildcard against the next character */
			
			ch = *p;
			if (ch != '#') {
				/* scan forwards until we find a match for this char */
				if (!case_sens) ch = toupper(ch);
				while (s < es) {
					if (case_sens) {
						if (*s == ch) break;
					} else if (toupper(*s) == ch)
						break;
					s++;
				}
			}
			
			if (s < es) {
				/* remember where we are in case the match fails; we can then resume */
				if (top < (int)NOF_ELEMENTS(context)) {
					context[top].ss = ss;
					context[top].s  = s + 1;
					context[top].p  = p - 1;    /* ptr to last asterisk */
					top++;
				}
				matches = true;
			}
			else
				matches = false;
		}
		else if (s < es) {
			char ch = *p;
			if (ch == '#')
				matches = true;
			else {
				if (case_sens)
					matches = (*s == ch);
				else
					matches = (toupper(*s) == toupper(ch));
			}
		}
		else
			matches = false;

		if (matches) {
			p++; s++;
		}
		else {
			/* doesn't match, resume with stacked context if we have one */
			if (--top < 0) return NULL;  /* no match, give up */
			
			ss = context[top].ss;
			s  = context[top].s;
			p  = context[top].p;
		}
	}

	/* end of pattern reached */
	return ss;
}


/**
 * Finds all occurrences of a given string in the box tree
 *
 * \param pattern   the string pattern to search for
 * \param p_len     pattern length
 * \param cur       pointer to the current box
 * \param case_sens whether to perform a case sensitive search
 * \return true on success, false on memory allocation failure
 */
bool find_occurrences(const char *pattern, int p_len, struct box *cur, bool case_sens)
{
	struct box *a;

	/* ignore this box, if there's no visible text */
	if (!cur->object && cur->text) {
		const char *pos = find_pattern(cur->text, cur->length, pattern, p_len, case_sens);
		if (pos) {
			/* found string in box => add to list */
			struct list_entry *entry = calloc(1, sizeof(*entry));
			if (!entry) {
				warn_user("NoMemory", 0);
				return false;
			}
			entry->box = cur;
			entry->next = 0;
			entry->prev = search_found->prev;
			if (!search_found->prev)
				search_found->next = entry;
			else
				search_found->prev->next = entry;
			search_found->prev = entry;
		}
	}

	/* and recurse */
	for (a = cur->children; a; a = a->next) {
		if (!find_occurrences(pattern, p_len, a, case_sens))
			return false;
	}

	return true;
}

#endif
