/*
 * Copyright 2012 - 2013 Michael Drake <tlsa@netsurf-browser.org>
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


#include <stdlib.h>

#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/global_history.h"
#include "desktop/treeview.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/log.h"

#define N_FIELDS 5
#define N_DAYS 28
#define N_SEC_PER_DAY (60 * 60 * 24)

enum global_history_folders {
	GH_TODAY = 0,
	GH_YESTERDAY,
	GH_2_DAYS_AGO,
	GH_3_DAYS_AGO,
	GH_4_DAYS_AGO,
	GH_5_DAYS_AGO,
	GH_6_DAYS_AGO,
	GH_LAST_WEEK,
	GH_2_WEEKS_AGO,
	GH_3_WEEKS_AGO,
	GH_N_FOLDERS
};

struct global_history_folder {
	struct treeview_node *folder;
	struct treeview_field_data data;
};

struct global_history_ctx {
	struct treeview *tree;
	struct treeview_field_desc fields[N_FIELDS];
	struct global_history_folder folders[GH_N_FOLDERS];
	time_t today;
	int weekday;
};
struct global_history_ctx gh_ctx;

struct global_history_entry {
	int slot;
	nsurl *url;
	time_t t;
	struct treeview_node *entry;
	struct global_history_entry *next;
	struct global_history_entry *prev;

	struct treeview_field_data data[N_FIELDS - 1];
};
struct global_history_entry *gh_list[N_DAYS];


/**
 * Find an entry in the global history
 *
 * \param url The URL to find
 * \return Pointer to node, or NULL if not found
 */
static struct global_history_entry *global_history_find(nsurl *url)
{
	int i;
	struct global_history_entry *e;

	for (i = 0; i < N_DAYS; i++) {
		e = gh_list[i];

		while (e != NULL) {
			if (nsurl_compare(e->url, url,
					NSURL_COMPLETE) == true) {
				return e;
			}
			e = e->next;
		}

	}
	return NULL;
}

static inline nserror global_history_get_parent_treeview_node(
		struct treeview_node **parent, int slot)
{
	int folder_index;
	struct global_history_folder *f;

	if (slot < 7) {
		folder_index = slot;

	} else if (slot < 14) {
		folder_index = GH_LAST_WEEK;

	} else if (slot < 21) {
		folder_index = GH_2_WEEKS_AGO;

	} else if (slot < N_DAYS) {
		folder_index = GH_3_WEEKS_AGO;

	} else {
		/* Slot value is invalid */
		return NSERROR_BAD_PARAMETER;
	}

	/* Get the folder */
	f = &(gh_ctx.folders[folder_index]);

	/* Return the parent treeview folder */
	*parent = f->folder;
	return NSERROR_OK;
}


static nserror global_history_create_treeview_field_data(
		struct global_history_entry *e,
		const struct url_data *data)
{
	e->data[0].field = gh_ctx.fields[0].field;
	e->data[0].value = strdup(data->title);
	e->data[0].value_len = strlen(data->title);

	e->data[1].field = gh_ctx.fields[1].field;
	e->data[1].value = nsurl_access(e->url);
	e->data[1].value_len = nsurl_length(e->url);

	e->data[2].field = gh_ctx.fields[2].field;
	e->data[2].value = "Date time";
	e->data[2].value_len = SLEN("Date time");

	e->data[3].field = gh_ctx.fields[3].field;
	e->data[3].value = "Count";
	e->data[3].value_len = SLEN("Count");

	return NSERROR_OK;
}

/**
 * Add a global history entry to the treeview
 *
 * \param e	entry to add to treeview
 * \param slot  global history slot containing entry
 * \return NSERROR_OK on success, or appropriate error otherwise
 *
 * It is assumed that the entry is unique (for its URL) in the global
 * history table
 */
static nserror global_history_entry_insert(struct global_history_entry *e,
		int slot)
{
	nserror err;

	struct treeview_node *parent;
	err = global_history_get_parent_treeview_node(&parent, slot);
	if (err != NSERROR_OK) {
		return err;
	}

	err = treeview_create_node_entry(gh_ctx.tree, &(e->entry),
			parent, TREE_REL_CHILD, e->data, e);
	if (err != NSERROR_OK) {
		return err;
	}

	return NSERROR_OK;
}


static nserror global_history_add_entry_internal(nsurl *url, int slot,
		const struct url_data *data, bool got_treeview)
{
	nserror err;
	struct global_history_entry *e;

	/* Create new local history entry */
	e = malloc(sizeof(struct global_history_entry));
	if (e == NULL) {
		return false;
	}

	e->slot = slot;
	e->url = nsurl_ref(url);
	e->t = data->last_visit;
	e->entry = NULL;
	e->next = NULL;
	e->prev = NULL;

	err = global_history_create_treeview_field_data(e, data);
	if (err != NSERROR_OK) {
		return err;
	}
	
	if (gh_list[slot] == NULL) {
		/* list empty */
		gh_list[slot] = e;

	} else if (gh_list[slot]->t < e->t) {
		/* Insert at list head */
		e->next = gh_list[slot];
		gh_list[slot]->prev = e;
		gh_list[slot] = e;
	} else {
		struct global_history_entry *prev = gh_list[slot];
		struct global_history_entry *curr = prev->next;
		while (curr != NULL) {
			if (curr->t < e->t) {
				break;
			}
			prev = curr;
			curr = curr->next;
		}

		/* insert after prev */
		e->next = curr;
		e->prev = prev;
		prev->next = e;

		if (curr != NULL)
			curr->prev = e;
	}

	if (got_treeview) {
		err = global_history_entry_insert(e, slot);
		if (err != NSERROR_OK) {
			return err;
		}
	}

	return NSERROR_OK;
}

static void global_history_delete_entry_internal(
		struct global_history_entry *e)
{
	/* Unlink */
	if (gh_list[e->slot] == e) {
		/* e is first entry */
		gh_list[e->slot] = e->next;

		if (e->next != NULL)
			e->next->prev = NULL;

	} else if (e->next == NULL) {
		/* e is last entry */
		e->prev->next = NULL;

	} else {
		/* e has an entry before and after */
		e->prev->next = e->next;
		e->next->prev = e->prev;
	}

	/* Destroy */
	free((void *)e->data[0].value); /* Eww */
	nsurl_unref(e->url);
	free(e);
}

/**
 * Internal routine to actually perform global history addition
 *
 * \param url The URL to add
 * \param data URL data associated with URL
 * \return true (for urldb_iterate_entries)
 */
static bool global_history_add_entry(nsurl *url,
		const struct url_data *data)
{
	int slot;
	struct global_history_entry *e;
	time_t visit_date;
	time_t earliest_date = gh_ctx.today - (N_DAYS - 1) * N_SEC_PER_DAY;
	bool got_treeview = gh_ctx.tree != NULL;

	assert((url != NULL) && (data != NULL));

	visit_date = data->last_visit;

	/* Find day array slot for entry */
	if (visit_date >= gh_ctx.today) {
		slot = 0;
	} else if (visit_date >= earliest_date) {
		slot = (gh_ctx.today - visit_date) / N_SEC_PER_DAY + 1;
	} else {
		/* too old */
		return true;
	}

	if (got_treeview == true) {
		/* The treeview for global history already exists */

		/* See if there's already an entry for this URL */
		e = global_history_find(url);
		if (e != NULL) {
			/* Existing entry.  Delete it. */
			treeview_delete_node(gh_ctx.tree, e->entry);
			return true;
		}
	}

	if (global_history_add_entry_internal(url, slot, data,
			got_treeview) != NSERROR_OK) {
		return false;
	}

	return true;
}

/**
 * Initialise the treeview entry feilds
 *
 * \return true on success, false on memory exhaustion
 */
static nserror global_history_initialise_entry_fields(void)
{
	int i;

	for (i = 0; i < N_FIELDS; i++)
		gh_ctx.fields[i].field = NULL;

	/* TODO: use messages */
	gh_ctx.fields[0].flags = TREE_FLAG_DEFAULT;
	if (lwc_intern_string("Title", SLEN("Title"),
			&gh_ctx.fields[0].field) !=
			lwc_error_ok) {
		goto error;
	}

	gh_ctx.fields[1].flags = TREE_FLAG_NONE;
	if (lwc_intern_string("URL", SLEN("URL"),
			&gh_ctx.fields[1].field) !=
			lwc_error_ok) {
		goto error;
	}

	gh_ctx.fields[2].flags = TREE_FLAG_SHOW_NAME;
	if (lwc_intern_string("Last visit", SLEN("Last visit"),
			&gh_ctx.fields[2].field) !=
			lwc_error_ok) {
		goto error;
	}

	gh_ctx.fields[3].flags = TREE_FLAG_SHOW_NAME;
	if (lwc_intern_string("Visits", SLEN("Visits"),
			&gh_ctx.fields[3].field) !=
			lwc_error_ok) {
		goto error;
	}

	gh_ctx.fields[4].flags = TREE_FLAG_DEFAULT;
	if (lwc_intern_string("Period", SLEN("Period"),
			&gh_ctx.fields[4].field) !=
			lwc_error_ok) {
		return false;
	}

	return NSERROR_OK;

error:
	for (i = 0; i < N_FIELDS; i++)
		if (gh_ctx.fields[i].field != NULL)
			lwc_string_unref(gh_ctx.fields[i].field);

	return NSERROR_UNKNOWN;
}


/**
 * Initialise the time
 *
 * \return true on success, false on memory exhaustion
 */
static nserror global_history_initialise_time(void)
{
	struct tm *full_time;
	time_t t;

	/* get the current time */
	t = time(NULL);
	if (t == -1) {
		LOG(("time info unaviable"));
		return NSERROR_UNKNOWN;
	}

	/* get the time at the start of today */
	full_time = localtime(&t);
	full_time->tm_sec = 0;
	full_time->tm_min = 0;
	full_time->tm_hour = 0;
	t = mktime(full_time);
	if (t == -1) {
		LOG(("mktime failed"));
		return NSERROR_UNKNOWN;
	}

	gh_ctx.today = t;
	gh_ctx.weekday = full_time->tm_wday;

	return NSERROR_OK;
}


/**
 * Initialise the treeview directories
 *
 * \return true on success, false on memory exhaustion
 */
static nserror global_history_init_dir(enum global_history_folders f,
		const char *label, int age)
{
	nserror err;
	time_t t = gh_ctx.today;
	struct treeview_node *relation = NULL;
	enum treeview_relationship rel = TREE_REL_CHILD;

	t -= age * N_SEC_PER_DAY;

	label = messages_get(label);

	if (f != GH_TODAY) {
		relation = gh_ctx.folders[f - 1].folder;
		rel = TREE_REL_SIBLING_NEXT;
	}

	gh_ctx.folders[f].data.field = gh_ctx.fields[N_FIELDS - 1].field;
	gh_ctx.folders[f].data.value = label;
	gh_ctx.folders[f].data.value_len = strlen(label);
	err = treeview_create_node_folder(gh_ctx.tree,
			&gh_ctx.folders[f].folder,
			relation, rel,
			&gh_ctx.folders[f].data,
			&gh_ctx.folders[f]);

	return err;
}


/**
 * Initialise the treeview directories
 *
 * \return true on success, false on memory exhaustion
 */
static nserror global_history_init_dirs(void)
{
	nserror err;

	err = global_history_init_dir(GH_TODAY, "DateToday", 0);
	if (err != NSERROR_OK) return err;

	err = global_history_init_dir(GH_YESTERDAY, "DateYesterday", 1);
	if (err != NSERROR_OK) return err;

	err = global_history_init_dir(GH_2_DAYS_AGO, "Date2Days", 2);
	if (err != NSERROR_OK) return err;

	err = global_history_init_dir(GH_3_DAYS_AGO, "Date3Days", 3);
	if (err != NSERROR_OK) return err;

	err = global_history_init_dir(GH_4_DAYS_AGO, "Date4Days", 4);
	if (err != NSERROR_OK) return err;

	err = global_history_init_dir(GH_5_DAYS_AGO, "Date5Days", 5);
	if (err != NSERROR_OK) return err;

	err = global_history_init_dir(GH_6_DAYS_AGO, "Date6Days", 6);
	if (err != NSERROR_OK) return err;

	err = global_history_init_dir(GH_LAST_WEEK, "Date1Week", 7);
	if (err != NSERROR_OK) return err;

	err = global_history_init_dir(GH_2_WEEKS_AGO, "Date2Week", 14);
	if (err != NSERROR_OK) return err;

	err = global_history_init_dir(GH_3_WEEKS_AGO, "Date3Week", 21);
	if (err != NSERROR_OK) return err;

	return NSERROR_OK;
}


/**
 * Initialise the treeview entries
 *
 * \return true on success, false on memory exhaustion
 */
static nserror global_history_init_entries(void)
{
	int i;
	nserror err;

	/* Itterate over all global history data, inserting it into treeview */
	for (i = 0; i < N_DAYS; i++) {
		struct global_history_entry *e = gh_list[i];
		
		while (e != NULL) {
			err = global_history_entry_insert(e, i);
			if (err != NSERROR_OK) {
				return err;
			}
			e = e->next;
		}
	}

	return NSERROR_OK;
}


static nserror global_history_tree_node_folder_cb(
		struct treeview_node_msg msg, void *data)
{
	return NSERROR_OK;
}
static nserror global_history_tree_node_entry_cb(
		struct treeview_node_msg msg, void *data)
{
	struct global_history_entry *e = (struct global_history_entry *)data;

	switch (msg.msg) {
	case TREE_MSG_NODE_DELETE:
		global_history_delete_entry_internal(e);
		break;

	case TREE_MSG_FIELD_EDIT:
		break;
	}
	return NSERROR_OK;
}
struct treeview_callback_table tree_cb_t = {
	.folder = global_history_tree_node_folder_cb,
	.entry = global_history_tree_node_entry_cb
};

/**
 * Initialises the global history module.
 *
 * \param 
 * \param 
 * \return true on success, false on memory exhaustion
 */
nserror global_history_init(struct core_window_callback_table *cw_t,
		void *core_window_handle)
{
	nserror err;

	LOG(("Loading global history"));

	/* Init. global history treeview time */
	err = global_history_initialise_time();
	if (err != NSERROR_OK) {
		gh_ctx.tree = NULL;
		return err;
	}

	/* Init. global history treeview entry fields */
	err = global_history_initialise_entry_fields();
	if (err != NSERROR_OK) {
		gh_ctx.tree = NULL;
		return err;
	}

	/* Load the entries */
	urldb_iterate_entries(global_history_add_entry);

	/* Create the global history treeview */
	err = treeview_create(&gh_ctx.tree, &tree_cb_t,
			N_FIELDS, gh_ctx.fields,
			cw_t, core_window_handle);
	if (err != NSERROR_OK) {
		gh_ctx.tree = NULL;
		return err;
	}

	/* Add the folders to the treeview */
	err = global_history_init_dirs();
	if (err != NSERROR_OK) {
		return err;
	}

	LOG(("Building global history treeview"));

	/* Add the history to the treeview */
	err = global_history_init_entries();
	if (err != NSERROR_OK) {
		return err;
	}

	/* Expand the "Today" folder node */
	err = treeview_node_expand(gh_ctx.tree,
			gh_ctx.folders[GH_TODAY].folder);
	if (err != NSERROR_OK) {
		return err;
	}

	LOG(("Loaded global history"));

	return NSERROR_OK;
}

/**
 * Finalises the global history module.
 *
 * \param 
 * \param 
 * \return true on success, false on memory exhaustion
 */
nserror global_history_fini(struct core_window_callback_table *cw_t,
		void *core_window_handle)
{
	int i;
	nserror err;

	LOG(("Finalising global history"));

	/* Destroy the global history treeview */
	err = treeview_destroy(gh_ctx.tree);

	/* Free global history treeview entry fields */
	for (i = 0; i < N_FIELDS; i++)
		if (gh_ctx.fields[i].field != NULL)
			lwc_string_unref(gh_ctx.fields[i].field);

	LOG(("Finalised global history"));

	return NSERROR_OK;
}

void global_history_redraw(int x, int y, struct rect *clip,
		const struct redraw_context *ctx)
{
	treeview_redraw(gh_ctx.tree, x, y, clip, ctx);
}

void global_history_mouse_action(browser_mouse_state mouse, int x, int y)
{
	treeview_mouse_action(gh_ctx.tree, mouse, x, y);
}

