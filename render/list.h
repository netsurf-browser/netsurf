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

void render_list_destroy_counters(void);
bool render_list_counter_reset(char *name, int value);
bool render_list_counter_increment(char *name, int value);
bool render_list_counter_end_scope(char *name);
char *render_list_counter(struct css_counter *css_counter);

void render_list_test(void);

#endif
