/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
 */

#include <stdlib.h>

#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/query.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


/** Data for a query window */
struct gui_query_window
{
	struct gui_query_window *prev;	/** Previous query in list */
	struct gui_query_window *next;	/** Next query in list */

	query_id id;	/** unique ID number for this query */
	wimp_w window;	/** RISC OS window handle */

	const query_callback *cb;	/** Table of callback functions */
	void *pw;	/** Handle passed to callback functions */

	bool default_confirm;	/** Default action is to confirm */
};


/** Next unallocated query id */
static query_id next_id = (query_id)1;

/** List of all query windows. */
static struct gui_query_window *gui_query_window_list = 0;

/** Template for a query window. */
static struct wimp_window *query_template;

static void ro_gui_query_window_destroy(struct gui_query_window *qw);
static struct gui_query_window *ro_gui_query_window_lookup_id(query_id id);


void ro_gui_query_init(void)
{
	query_template = ro_gui_dialog_load_template("query");
}


/**
 * Lookup a query window using its RISC OS window handle
 *
 * \param  w  RISC OS window handle
 * \return pointer to query window or NULL
 */

struct gui_query_window *ro_gui_query_window_lookup(wimp_w w)
{
	struct gui_query_window *qw = gui_query_window_list;
	while (qw && qw->window != w)
		qw = qw->next;
	return qw;
}


/**
 * Lookup a query window using its ID number
 *
 * \param  id  id to search for
 * \return pointer to query window or NULL
 */

struct gui_query_window *ro_gui_query_window_lookup_id(query_id id)
{
	struct gui_query_window *qw = gui_query_window_list;
	while (qw && qw->id != id)
		qw = qw->next;
	return qw;
}


/**
 * Display a query to the user, requesting a response.
 *
 * \param  query   message token of query
 * \param  detail  parameter used in expanding tokenised message
 * \param  cb      table of callback functions to be called when user responds
 * \param  pw      handle to be passed to callback functions
 * \return id number of the query (or QUERY_INVALID if it failed)
 */

query_id query_user(const char *query, const char *detail, const query_callback *cb, void *pw)
{
	struct gui_query_window *qw;
	char query_buffer[300];
	os_error *error;

	qw = malloc(sizeof(struct gui_query_window));
	if (!qw) {
		warn_user("NoMemory", NULL);
		return QUERY_INVALID;
	}

	qw->cb = cb;
	qw->pw = pw;
	qw->id = next_id++;
	qw->default_confirm = false;

	if (next_id == QUERY_INVALID)
		next_id++;

	error = xwimp_create_window(query_template, &qw->window);
	if (error) {
		warn_user("WimpError", error->errmess);
	}

	snprintf(query_buffer, sizeof query_buffer, "%s %s",
			messages_get(query), detail ? detail : "");
	query_buffer[sizeof query_buffer - 1] = 0;

	ro_gui_set_icon_string(qw->window, ICON_QUERY_MESSAGE, query_buffer);

	xwimp_set_icon_state(qw->window, ICON_QUERY_HELP,
			wimp_ICON_DELETED, wimp_ICON_DELETED);

	ro_gui_dialog_open(qw->window);

	error = xwimp_set_caret_position(qw->window, (wimp_i)-1, 0, 0, 1 << 25, -1);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	/* put this query window at the head of our list */
	if (gui_query_window_list)
		gui_query_window_list->prev = qw;

	qw->prev = NULL;
	qw->next = gui_query_window_list;
	gui_query_window_list = qw;

	return qw->id;
}


/**
 * Close and destroy a query window, releasing all resources
 *
 * \param  qw  query window
 */

void ro_gui_query_window_destroy(struct gui_query_window *qw)
{
	os_error *error = xwimp_delete_window(qw->window);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	/* remove from linked-list of query windows and release memory */
	if (qw->prev)
		qw->prev->next = qw->next;
	else
		gui_query_window_list = qw->next;

	if (qw->next)
		qw->next->prev = qw->prev;

	free(qw);
}


/**
 * Close a query window without waiting for a response from the user.
 * (should normally only be called if the user has responded in some other
 *  way of which the query window in unaware.)
 *
 * \param  id  id of query window to close
 */

void query_close(query_id id)
{
	struct gui_query_window *qw = ro_gui_query_window_lookup_id(id);
	if (qw) ro_gui_query_window_destroy(qw);
}


void ro_gui_query_window_bring_to_front(query_id id)
{
	struct gui_query_window *qw = ro_gui_query_window_lookup_id(id);
	if (qw) {
		os_error *error;

		ro_gui_dialog_open(qw->window);

		error = xwimp_set_caret_position(qw->window, (wimp_i)-1, 0, 0, 1 << 25, -1);
		if (error) {
			LOG(("xwimp_get_caret_position: 0x%x : %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
}


/**
 * Handle mouse clicks in a query window.
 *
 * \param  qw   query window
 * \param  key  key press info from the Wimp
 */

void ro_gui_query_window_click(struct gui_query_window *qw, wimp_pointer *pointer)
{
	const query_callback *cb = qw->cb;
	switch (pointer->i) {
		case ICON_QUERY_YES:
			cb->confirm(qw->id, QUERY_YES, qw->pw);
			ro_gui_query_window_destroy(qw);
			break;

		case ICON_QUERY_NO:
			cb->cancel(qw->id, QUERY_NO, qw->pw);
			ro_gui_query_window_destroy(qw);
			break;

		case ICON_QUERY_HELP:
			/* \todo */
			break;
	}
}


/**
 * Handle keypresses in a query window.
 *
 * \param  qw       query window
 * \param  pointer  mouse pointer state from Wimp.
 * \return true iff the key press is the key press has been handled
 */

bool ro_gui_query_window_keypress(struct gui_query_window *qw, wimp_key *key)
{
	const query_callback *cb = qw->cb;
	switch (key->c) {

		case wimp_KEY_ESCAPE:
			cb->escape(qw->id, QUERY_ESCAPE, qw->pw);
			ro_gui_query_window_destroy(qw);
			return true;

		case wimp_KEY_RETURN:
			if (qw->default_confirm)
				cb->confirm(qw->id, QUERY_YES, qw->pw);
			else
				cb->cancel(qw->id, QUERY_NO, qw->pw);
			ro_gui_query_window_destroy(qw);
			return true;
	}

	return true;
}
