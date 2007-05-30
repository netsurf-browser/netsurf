/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
 */

/** \file
 * Free text search (implementation)
 */

#include <ctype.h>
#include <string.h>

#include "oslib/hourglass.h"
#include "oslib/wimp.h"

#include "utils/config.h"
#include "content/content.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "desktop/selection.h"
#include "render/box.h"
#include "render/html.h"
#include "riscos/dialog.h"
#include "riscos/menus.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#ifdef WITH_SEARCH

#ifndef NOF_ELEMENTS
#define NOF_ELEMENTS(array) (sizeof(array)/sizeof(*(array)))
#endif

struct list_entry {
	unsigned start_idx;	/* start position of match */
	unsigned end_idx;	/* end of match */

	struct box *start_box;	/* used only for html contents */
	struct box *end_box;

	struct selection *sel;

	struct list_entry *prev;
	struct list_entry *next;
};

struct gui_window *search_current_window = 0;

static char *search_string = 0;
static struct list_entry search_head = { 0, 0, NULL, NULL, NULL, NULL, NULL };
static struct list_entry *search_found = &search_head;
static struct list_entry *search_current = 0;
static struct content *search_content = 0;
static bool search_prev_case_sens = false;

#define RECENT_SEARCHES 8
bool search_insert;
static char *recent_search[RECENT_SEARCHES];
static wimp_MENU(RECENT_SEARCHES) menu_recent;
wimp_menu *recent_search_menu = (wimp_menu *)&menu_recent;
#define DEFAULT_FLAGS (wimp_ICON_TEXT | wimp_ICON_FILLED | \
		(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | \
		(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT))


static void start_search(bool forwards);
static void do_search(char *string, int string_len, bool case_sens,
		bool forwards);
static const char *find_pattern(const char *string, int s_len,
		const char *pattern, int p_len, bool case_sens, int *m_len);
static bool find_occurrences_html(const char *pattern, int p_len,
		struct box *cur, bool case_sens);
static bool find_occurrences_text(const char *pattern, int p_len,
		struct content *c, bool case_sens);
static struct list_entry *add_entry(unsigned start_idx, unsigned end_idx);
static void free_matches(void);
static void show_all(bool all);
static void show_status(bool found);

static void ro_gui_search_end(wimp_w w);
static bool ro_gui_search_next(wimp_w w);
static bool ro_gui_search_click(wimp_pointer *pointer);
static bool ro_gui_search_keypress(wimp_key *key);
static void ro_gui_search_add_recent(char *search);

void ro_gui_search_init(void)
{
	dialog_search = ro_gui_dialog_create("search");
	ro_gui_wimp_event_register_keypress(dialog_search,
			ro_gui_search_keypress);
	ro_gui_wimp_event_register_close_window(dialog_search,
			ro_gui_search_end);
	ro_gui_wimp_event_register_menu_gright(dialog_search,
			ICON_SEARCH_TEXT, ICON_SEARCH_MENU,
			recent_search_menu);
	ro_gui_wimp_event_register_text_field(dialog_search,
			ICON_SEARCH_STATUS);
	ro_gui_wimp_event_register_checkbox(dialog_search,
			ICON_SEARCH_CASE_SENSITIVE);
	ro_gui_wimp_event_register_mouse_click(dialog_search,
			ro_gui_search_click);
	ro_gui_wimp_event_register_ok(dialog_search, ICON_SEARCH_FIND_NEXT,
			ro_gui_search_next);
	ro_gui_wimp_event_register_cancel(dialog_search, ICON_SEARCH_CANCEL);
	ro_gui_wimp_event_set_help_prefix(dialog_search, "HelpSearch");

	recent_search_menu->title_data.indirected_text.text =
			(char*)messages_get("Search");
	ro_gui_menu_init_structure(recent_search_menu, RECENT_SEARCHES);
}

/**
 * Wrapper for the pressing of an OK button for wimp_event.
 *
 * \return false, to indicate the window should not be closed
 */
bool ro_gui_search_next(wimp_w w)
{
	search_insert = true;
	start_search(true);
	return false;
}

bool ro_gui_search_click(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_SEARCH_FIND_PREV:
			search_insert = true;
			start_search(false);
			return true;
		case ICON_SEARCH_CASE_SENSITIVE:
			start_search(true);
			return true;
		case ICON_SEARCH_SHOW_ALL:
			show_all(ro_gui_get_icon_selected_state(pointer->w,
					pointer->i));
			return true;
	}
	return false;
}

void ro_gui_search_add_recent(char *search)
{
	char *tmp;
	int i;

	if ((search == NULL) || (search[0] == '\0'))
		return;

	if (!search_insert) {
		free(recent_search[0]);
		recent_search[0] = strdup(search);
		ro_gui_search_prepare_menu();
		return;
	}

	if ((recent_search[0] != NULL) &&
			(!strcmp(recent_search[0], search)))
		return;

	tmp = strdup(search);
	if (!tmp) {
		warn_user("NoMemory", 0);
		return;
	}
	free(recent_search[RECENT_SEARCHES - 1]);
	for (i = RECENT_SEARCHES - 1; i > 0; i--)
		recent_search[i] = recent_search[i - 1];
	recent_search[0] = tmp;
	search_insert = false;

	ro_gui_set_icon_shaded_state(dialog_search, ICON_SEARCH_MENU, false);
	ro_gui_search_prepare_menu();
}

bool ro_gui_search_prepare_menu(void)
{
	os_error *error;
	int i;
	int suggestions = 0;

	for (i = 0; i < RECENT_SEARCHES; i++)
		if (recent_search[i] != NULL)
			suggestions++;

	if (suggestions == 0)
		return false;

	for (i = 0; i < suggestions; i++) {
		recent_search_menu->entries[i].menu_flags &= ~wimp_MENU_LAST;
		recent_search_menu->entries[i].data.indirected_text.text =
				recent_search[i];
		recent_search_menu->entries[i].data.indirected_text.size =
				strlen(recent_search[i]) + 1;
	}
	recent_search_menu->entries[suggestions - 1].menu_flags |=
			wimp_MENU_LAST;

	if ((current_menu_open) && (current_menu == recent_search_menu)) {
		error = xwimp_create_menu(current_menu, 0, 0);
		if (error) {
			LOG(("xwimp_create_menu: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MenuError", error->errmess);
		}
	}
	return true;
}

/**
 * Open the search dialog
 *
 * \param g the gui window to search
 */
void ro_gui_search_prepare(struct gui_window *g)
{
	struct content *c;

	assert(g != NULL);

	c = g->bw->current_content;

	/* only handle html/textplain contents */
	if ((!c) || (c->type != CONTENT_HTML &&
			c->type != CONTENT_TEXTPLAIN))
		return;

	/* if the search dialogue is reopened over a new window, we still
	   need to cancel the previous search */
	ro_gui_search_end(dialog_search);

	search_current_window = g;

	ro_gui_set_icon_string(dialog_search, ICON_SEARCH_TEXT, "");
	ro_gui_set_icon_selected_state(dialog_search,
				ICON_SEARCH_CASE_SENSITIVE, false);
	ro_gui_set_icon_selected_state(dialog_search,
				ICON_SEARCH_SHOW_ALL, false);

	show_status(true);
	ro_gui_set_icon_shaded_state(dialog_search,
			ICON_SEARCH_FIND_PREV, true);
	ro_gui_set_icon_shaded_state(dialog_search,
			ICON_SEARCH_FIND_NEXT, true);

	ro_gui_wimp_event_memorise(dialog_search);
	search_insert = true;
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
		case 1: { /* ctrl a */
			bool sel = !ro_gui_get_icon_selected_state(key->w,
					ICON_SEARCH_SHOW_ALL);
			ro_gui_set_icon_selected_state(key->w,
					ICON_SEARCH_SHOW_ALL, sel);
			show_all(sel);
		}
		break;
		case 9: /* ctrl i */
			state = ro_gui_get_icon_selected_state(dialog_search,
					ICON_SEARCH_CASE_SENSITIVE);
			ro_gui_set_icon_selected_state(dialog_search,
					ICON_SEARCH_CASE_SENSITIVE, !state);
			start_search(true);
			return true;
		case IS_WIMP_KEY | wimp_KEY_UP:
			search_insert = true;
			start_search(false);
			return true;
		case IS_WIMP_KEY | wimp_KEY_DOWN:
			search_insert = true;
			start_search(true);
			return true;

		default:
			if (key->c == 21) {
				/* ctrl+u means the user's starting
				 * a new search */
				search_insert = true;
			}
			if (key->c == 8  || /* backspace */
			    key->c == 21 || /* ctrl u */
			    (key->c >= 0x20 && key->c <= 0x7f)) {
				start_search(true);
				return true;
			}
			break;
	}

	return false;
}

/**
 * Begins/continues the search process
 * Note that this may be called many times for a single search.
 *
 * \param  forwards  search forwards from start/current position
 */

void start_search(bool forwards)
{
	int string_len;
	char *string;

	string = ro_gui_get_icon_string(dialog_search, ICON_SEARCH_TEXT);
	assert(string);

	ro_gui_search_add_recent(string);

	string_len = strlen(string);
	if (string_len <= 0) {
		free_matches();
		show_status(true);
		ro_gui_set_icon_shaded_state(dialog_search,
				ICON_SEARCH_FIND_PREV, true);
		ro_gui_set_icon_shaded_state(dialog_search,
				ICON_SEARCH_FIND_NEXT, true);
		gui_window_set_scroll(search_current_window, 0, 0);
		return;
	}

	do_search(string, string_len,
		ro_gui_get_icon_selected_state(dialog_search,
						ICON_SEARCH_CASE_SENSITIVE),
		forwards);
}

/**
 * Ends the search process, invalidating all global state and
 * freeing the list of found boxes
 *
 * \param w	the search window handle (not used)
 */
void ro_gui_search_end(wimp_w w)
{
	search_current_window = 0;

	if (search_string) {
		ro_gui_search_add_recent(search_string);
		free(search_string);
	}
	search_string = 0;

	free_matches();

	search_current = 0;

	search_content = 0;

	search_prev_case_sens = false;
}


/**
 * Release the memory used by the list of matches,
 * deleting selection objects too
 */

void free_matches(void)
{
	struct list_entry *a = search_found->next;
	struct list_entry *b;

	/* empty the list before clearing and deleting the
	   selections because the the clearing updates the
	   screen immediately, causing nested accesses to the list */

	search_found->prev = 0;
	search_found->next = 0;

	for (; a; a = b) {
		b = a->next;
		if (a->sel) {
			selection_clear(a->sel, true);
			selection_destroy(a->sel);
		}
		free(a);
	}
}


/**
 * Search for a string in the box tree
 *
 * \param string the string to search for
 * \param string_len length of search string
 * \param case_sens whether to perform a case sensitive search
 * \param forwards direction to search in
 */
void do_search(char *string, int string_len, bool case_sens, bool forwards)
{
	struct rect bounds;
	struct content *c;
	struct box *box;
	bool new = false;

	if (!search_current_window)
		return;

	c = search_current_window->bw->current_content;

	/* only handle html contents */
	if ((!c) || (c->type != CONTENT_HTML &&
			c->type != CONTENT_TEXTPLAIN))
		return;

	box = c->data.html.layout;

	if (!box)
		return;

//	LOG(("do_search '%s' - '%s' (%p, %p) %p (%d, %d) %d",
//		search_string, string, search_content, c, search_found->next,
//		search_prev_case_sens, case_sens, forwards));

	/* check if we need to start a new search or continue an old one */
	if (!search_string || c != search_content || !search_found->next ||
	    search_prev_case_sens != case_sens ||
	    (case_sens && strcmp(string, search_string) != 0) ||
	    (!case_sens && strcasecmp(string, search_string) != 0)) {
		bool res;

		if (search_string)
			free(search_string);
		search_current = 0;
		free_matches();

		search_string = malloc(string_len + 1);
		if (search_string) {
			memcpy(search_string, string, string_len);
			search_string[string_len] = '\0';
		}

		xhourglass_on();

		if (c->type == CONTENT_HTML)
			res = find_occurrences_html(string, string_len,
					box, case_sens);
		else {
			assert(c->type == CONTENT_TEXTPLAIN);
			res = find_occurrences_text(string, string_len,
					c, case_sens);
		}

		if (!res) {
			free_matches();
			xhourglass_off();
			return;
		}
		xhourglass_off();

		new = true;
		search_content = c;
		search_prev_case_sens = case_sens;
	}

//	LOG(("%d %p %p (%p, %p)", new, search_found->next, search_current, search_current->prev, search_current->next));

	if (new) {
		/* new search, beginning at the top of the page */
		search_current = search_found->next;
	}
	else if (search_current) {
		/* continued search in the direction specified */
		if (forwards) {
			if (search_current->next)
				search_current = search_current->next;
		}
		else {
			if (search_current->prev)
				search_current = search_current->prev;
		}
	}

	show_status(search_current != NULL);
	show_all(ro_gui_get_icon_selected_state(dialog_search,
			ICON_SEARCH_SHOW_ALL));

	ro_gui_set_icon_shaded_state(dialog_search, ICON_SEARCH_FIND_PREV,
		!search_current || !search_current->prev);
	ro_gui_set_icon_shaded_state(dialog_search, ICON_SEARCH_FIND_NEXT,
		!search_current || !search_current->next);

	if (!search_current)
		return;

	switch (c->type) {
		case CONTENT_HTML:
			/* get box position and jump to it */
			box_coords(search_current->start_box,
					&bounds.x0, &bounds.y0);
			/* \todo: move x0 in by correct idx */
			box_coords(search_current->end_box,
					&bounds.x1, &bounds.y1);
			/* \todo: move x1 in by correct idx */
			bounds.x1 += search_current->end_box->width;
			bounds.y1 += search_current->end_box->height;
			break;

		default:
			assert(c->type == CONTENT_TEXTPLAIN);
			textplain_coords_from_range(c,
					search_current->start_idx,
					search_current->end_idx, &bounds);
			break;
	}

	gui_window_scroll_visible(search_current_window,
			bounds.x0, bounds.y0, bounds.x1, bounds.y1);
}


/**
 * Find the first occurrence of 'match' in 'string' and return its index
 *
 * /param  string     the string to be searched (unterminated)
 * /param  s_len      length of the string to be searched
 * /param  pattern    the pattern for which we are searching (unterminated)
 * /param  p_len      length of pattern
 * /param  case_sens  true iff case sensitive match required
 * /param  m_len      accepts length of match in bytes
 * /return pointer to first match, NULL if none
 */

const char *find_pattern(const char *string, int s_len, const char *pattern,
		int p_len, bool case_sens, int *m_len)
{
	struct { const char *ss, *s, *p; bool first; } context[16];
	const char *ep = pattern + p_len;
	const char *es = string  + s_len;
	const char *p = pattern - 1;  /* a virtual '*' before the pattern */
	const char *ss = string;
	const char *s = string;
	bool first = true;
	int top = 0;

	while (p < ep) {
		bool matches;
		if (p < pattern || *p == '*') {
			char ch;

			/* skip any further asterisks; one is the same as many */
			do p++; while (p < ep && *p == '*');

			/* if we're at the end of the pattern, yes, it matches */
			if (p >= ep) break;

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
				/* remember where we are in case the match fails;
					we can then resume */
				if (top < (int)NOF_ELEMENTS(context)) {
					context[top].ss = ss;
					context[top].s  = s + 1;
					context[top].p  = p - 1;    /* ptr to last asterisk */
					context[top].first = first;
					top++;
				}

				if (first) {
					ss = s;  /* remember first non-'*' char */
					first = false;
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
			if (matches && first) {
				ss = s;  /* remember first non-'*' char */
				first = false;
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
			first = context[top].first;
		}
	}

	/* end of pattern reached */
	*m_len = s - ss;
	return ss;
}


/**
 * Finds all occurrences of a given string in the html box tree
 *
 * \param pattern   the string pattern to search for
 * \param p_len     pattern length
 * \param cur       pointer to the current box
 * \param case_sens whether to perform a case sensitive search
 * \return true on success, false on memory allocation failure
 */
bool find_occurrences_html(const char *pattern, int p_len, struct box *cur,
		bool case_sens)
{
	struct box *a;

	/* ignore this box, if there's no visible text */
	if (!cur->object && cur->text) {
		const char *text = cur->text;
		unsigned length = cur->length;

		while (length > 0) {
			struct list_entry *entry;
			unsigned match_length;
			unsigned match_offset;
			const char *new_text;
			const char *pos = find_pattern(text, length,
					pattern, p_len, case_sens,
					&match_length);
			if (!pos) break;

			/* found string in box => add to list */
			match_offset = pos - cur->text;

			entry = add_entry(cur->byte_offset + match_offset,
						cur->byte_offset +
							match_offset +
							match_length);
			if (!entry)
				return false;

			entry->start_box = cur;
			entry->end_box = cur;

			new_text = pos + match_length;
			length -= (new_text - text);
			text = new_text;
		}
	}

	/* and recurse */
	for (a = cur->children; a; a = a->next) {
		if (!find_occurrences_html(pattern, p_len, a, case_sens))
			return false;
	}

	return true;
}


/**
 * Finds all occurrences of a given string in a textplain content
 *
 * \param pattern   the string pattern to search for
 * \param p_len     pattern length
 * \param c         the content to be searched
 * \param case_sens wheteher to perform a case sensitive search
 * \return true on success, false on memory allocation failure
 */

bool find_occurrences_text(const char *pattern, int p_len,
		struct content *c, bool case_sens)
{
	int nlines = textplain_line_count(c);
	int line;

	for(line = 0; line < nlines; line++) {
		size_t offset, length;
		const char *text = textplain_get_line(c, line,
				&offset, &length);
		if (text) {
			while (length > 0) {
				struct list_entry *entry;
				unsigned match_length;
				size_t start_idx;
				const char *new_text;
				const char *pos = find_pattern(text, length,
						pattern, p_len, case_sens,
						&match_length);
				if (!pos) break;

				/* found string in line => add to list */
				start_idx = offset + (pos - text);
				entry = add_entry(start_idx, start_idx +
						match_length);
				if (!entry)
					return false;

				new_text = pos + match_length;
				offset += (new_text - text);
				length -= (new_text - text);
				text = new_text;
			}
		}
	}

	return true;
}


/**
 * Add a new entry to the list of matches
 *
 * \param  start_idx  offset of match start within textual representation
 * \param  end_idx    offset of match end
 * \return pointer to added entry, NULL iff failed
 */

struct list_entry *add_entry(unsigned start_idx, unsigned end_idx)
{
	struct list_entry *entry;

	/* found string in box => add to list */
	entry = calloc(1, sizeof(*entry));
	if (!entry) {
		warn_user("NoMemory", 0);
		return NULL;
	}

	entry->start_idx = start_idx;
	entry->end_idx = end_idx;
	entry->sel = NULL;

	entry->next = 0;
	entry->prev = search_found->prev;
	if (!search_found->prev)
		search_found->next = entry;
	else
		search_found->prev->next = entry;
	search_found->prev = entry;

	return entry;
}


/**
 * Determines whether any portion of the given text box should be
 * selected because it matches the current search string.
 *
 * \param  g             gui window
 * \param  start_offset  byte offset within text of string to be checked
 * \param  end_offset    byte offset within text
 * \param  start_idx     byte offset within string of highlight start
 * \param  end_idx       byte offset of highlight end
 * \return true iff part of the box should be highlighted
 */

bool gui_search_term_highlighted(struct gui_window *g,
		unsigned start_offset, unsigned end_offset,
		unsigned *start_idx, unsigned *end_idx)
{
	if (g == search_current_window) {
		struct list_entry *a;
		for(a = search_found->next; a; a = a->next)
			if (a->sel && selection_defined(a->sel) &&
				selection_highlighted(a->sel,
					start_offset, end_offset,
					start_idx, end_idx))
				return true;
	}

	return false;
}


/**
 * Specifies whether all matches or just the current match should
 * be highlighted in the search text.
 */

void show_all(bool all)
{
	struct list_entry *a;

	for (a = search_found->next; a; a = a->next) {
		bool add = true;
		if (!all && a != search_current) {
			add = false;
			if (a->sel) {
				selection_clear(a->sel, true);
				selection_destroy(a->sel);
				a->sel = NULL;
			}
		}
		if (add && !a->sel) {
			a->sel = selection_create(search_current_window->bw);
			if (a->sel) {
				struct content *c = search_current_window->bw->current_content;
				switch (c->type) {
					case CONTENT_HTML:
						selection_init(a->sel,
							c->data.html.layout);
						break;
					default:
						assert(c->type ==
							CONTENT_TEXTPLAIN);
						selection_init(a->sel, NULL);
						break;
				}
				selection_set_start(a->sel, a->start_idx);
				selection_set_end(a->sel, a->end_idx);
			}
		}
	}
}


/**
 * Change the displayed search status.
 *
 * \param found  search pattern matched in text
 */

void show_status(bool found)
{
	ro_gui_set_icon_string(dialog_search, ICON_SEARCH_STATUS,
			found ? "" : messages_get("NotFound"));
}

#endif
