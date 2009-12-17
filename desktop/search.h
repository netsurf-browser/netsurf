/*
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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

#ifndef _NETSURF_DESKTOP_SEARCH_H_
#define _NETSURF_DESKTOP_SEARCH_H_

#include <ctype.h>
#include <string.h>

struct search_context;

typedef enum {
	SEARCH_FLAG_CASE_SENSITIVE = (1 << 0),
	SEARCH_FLAG_FORWARDS = (1 << 1),
	SEARCH_FLAG_SHOWALL = (1 << 2)
} search_flags_t;
		
/**
 * called to clear the context; 'renews' the search too
 */
void search_destroy_context(struct search_context *context);

/**
 * Change the displayed search status.
 * \param found  search pattern matched in text
 * \param p the pointer sent to search_step() / search_create_context()
 */
typedef void (*search_status_callback)(bool found, void *p);

/**
 * display hourglass while searching
 * \param active start/stop indicator
 * \param p the pointer sent to search_step() / search_create_context()
 */
typedef void (*search_hourglass_callback)(bool active, void *p);

/**
 * add search string to recent searches list
 * front has full liberty how to implement the bare notification;
 * core gives no guarantee of the integrity of the const char *
 * \param string search pattern
 * \param p the pointer sent to search_step() / search_create_context()
 */
typedef void (*search_add_recent_callback)(const char *string, void *p);

/**
 * activate search forwards button in gui
 * \param active activate/inactivate
 * \param p the pointer sent to search_step() / search_create_context()
 */
typedef void (*search_forward_state_callback)(bool active, void *p);

/**
 * activate search back button in gui
 * \param active activate/inactivate
 * \param p the pointer sent to search_step() / search_create_context()
 */
typedef void (*search_back_state_callback)(bool active, void *p);

struct search_callbacks {
	search_forward_state_callback 	forward_state;
	search_back_state_callback 	back_state;
	search_status_callback 		status;
	search_hourglass_callback 	hourglass;
	search_add_recent_callback 	add_recent;
};

bool search_verify_new(struct browser_window *bw,
			struct search_callbacks *callbacks, void *p);
void search_step(struct search_context *context, search_flags_t flags,
		const char * string);
bool search_create_context(struct browser_window *bw, 
		struct search_callbacks *callbacks, void *p);
void search_show_all(bool all, struct search_context *context);

#endif
