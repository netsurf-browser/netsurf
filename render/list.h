/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * HTML lists (interface).
 */

#ifndef _NETSURF_RENDER_LIST_H_
#define _NETSURF_RENDER_LIST_H_

#include <stdbool.h>

struct list_counter {
  	char *name;				/** Counter name */
  	struct list_counter_state *first;	/** First counter state */
  	struct list_counter_state *state;	/** Current counter state */
	struct list_counter *next;		/** Next counter */
};

struct list_counter_state {
	int count;				/** Current count */
	struct list_counter_state *parent;	/** Parent counter, or NULL */
	struct list_counter_state *next;	/** Next counter, or NULL */
};


void render_list_destroy_counters(void);
bool render_list_counter_reset(const char *name, int value);
bool render_list_counter_increment(const char *name, int value);
bool render_list_counter_end_scope(const char *name);
char *render_list_counter(struct css_counter *css_counter);

void render_list_test(void);

#endif
