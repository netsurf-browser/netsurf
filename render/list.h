/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
