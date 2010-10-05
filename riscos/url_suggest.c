/*
 * Copyright 2010 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * URL Suggestion Menu (implementation).
 */

#include <assert.h>
#include "oslib/wimp.h"
#include "content/content_type.h"
#include "content/urldb.h"
#include "riscos/menus.h"
#include "riscos/url_suggest.h"
#include "utils/messages.h"

struct url_suggest_item {
	const char		*url;	/*< The URL being stored.         */
	unsigned int		weight; /*< A weight assigned to the URL. */
	struct url_suggest_item	*next;  /*< The next URL in the list.     */
};

static bool url_suggest_callback(const char *url, const struct url_data *data);

static wimp_menu *suggest_menu;
static int suggest_entries;
static time_t suggest_time;
static struct url_suggest_item *suggest_list;

/**
 * Initialise the URL suggestion menu.  A menu block which must be set to
 * contain URL_SUGGEST_MAX_URLS entries is passed in.
 *
 * /param  *menu	The menu to use as the suggestion menu.
 * /return		true if initialisation was OK; else false.
 */

bool ro_gui_url_suggest_init(wimp_menu *menu)
{
	suggest_menu = menu;

	suggest_menu->title_data.indirected_text.text =
			(char *) messages_get("URLSuggest");
	ro_gui_menu_init_structure((wimp_menu *) suggest_menu,
			URL_SUGGEST_MAX_URLS);

	suggest_entries = 0;

	return true;
}


/**
 * Builds the URL suggestion menu. This is called by ro_gui_menu_create() when
 * it is asked to display the url_suggest_menu.
 *
 * /return		true if the menu has entries; else false.
 */

bool ro_gui_url_suggest_prepare_menu(void)
{
	int			i;
	struct url_suggest_item	*list, *next;

	/* Fetch the URLs we want to include from URLdb. */

	suggest_entries = 0;
	suggest_list = NULL;
	suggest_time = time(NULL);

	urldb_iterate_entries(url_suggest_callback);

	/* If any menu entries were found, put them into the menu.  The list
	 * is in reverse order, last to first, so the menu is filled backwards.
	 * Entries from the list are freed as we go.
	 */

	assert(suggest_entries <= URL_SUGGEST_MAX_URLS);

	if (suggest_entries > 0) {
		i = suggest_entries;

		list = suggest_list;
		suggest_list = NULL;

		while (list != NULL && i > 0) {
			i--;

			suggest_menu->entries[i].menu_flags = 0;
			suggest_menu->entries[i].data.indirected_text.text =
					(char *) list->url;
			suggest_menu->entries[i].data.indirected_text.size =
					strlen(list->url) + 1;

			next = list->next;
			free(list);
			list = next;
		}

		/* If i hasn't reached 0, then something went wrong -- get
		 * out now!
		 */

		if (i > 0)
			return false;

		suggest_menu->entries[0].menu_flags |=
				wimp_MENU_TITLE_INDIRECTED;
		suggest_menu->entries[suggest_entries - 1].menu_flags |=
				wimp_MENU_LAST;

		return true;
	}

	return false;
}


/**
 * Callback function for urldb_iterate_entries
 *
 * \param  url		URL which matches
 * \param  data		Data associated with URL
 * \return 		true to continue iteration, false otherwise
 */

bool url_suggest_callback(const char *url, const struct url_data *data)
{
	int			count;
	unsigned int		weight;
	struct url_suggest_item	**list, *new, *old;

	/* Ignore unvisited URLs, and those that don't apply to HTML or Text. */

	if (data->visits <= 0 ||(data->type != CONTENT_HTML &&
			data->type != CONTENT_TEXTPLAIN))
		return true;

	/* Calculate a weight for the URL. */

	weight = (suggest_time - data->last_visit) / data->visits;

	/* Hunt through those URLs already found to see if we want to add
	 * this one.  Smaller weights carry higher priority.
	 *
	 * The list is sorted into reverse order, so that lowest weight
	 * items are nearest the head.  Therefore, items are dropped from
	 * the head, making things simpler.
	 */

	list = &suggest_list;
	count = 0;

	while (*list != NULL && weight < (*list)->weight) {
		list = &((*list)->next);
		count++;
	}

	if (count > 0 || suggest_entries < URL_SUGGEST_MAX_URLS) {
		new = (struct url_suggest_item *)
				malloc(sizeof(struct url_suggest_item));

		if (new != NULL) {
			suggest_entries++;
			new->url = url;
			new->weight = weight;
			new->next = *list;

			*list = new;
		}
	}

	/* If adding the URL gave us too many menu items, drop the lowest
	 * priority ones until the list is the right length again.
	 */

	while (suggest_list != NULL && suggest_entries > URL_SUGGEST_MAX_URLS) {
		old = suggest_list;
		suggest_list = suggest_list->next;
		free(old);
		suggest_entries--;
	}

	return true;
}

