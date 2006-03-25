/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Browser history tree (interface).
 */

#ifndef _NETSURF_DESKTOP_HISTORY_H_
#define _NETSURF_DESKTOP_HISTORY_H_

#include <stdbool.h>

struct content;
struct history;
struct browser_window;

struct history *history_create(void);
void history_add(struct history *history, struct content *content,
		char *frag_id);
void history_update(struct history *history, struct content *content);
void history_destroy(struct history *history);
void history_back(struct browser_window *bw, struct history *history);
void history_forward(struct browser_window *bw, struct history *history);
bool history_back_available(struct history *history);
bool history_forward_available(struct history *history);
void history_size(struct history *history, int *width, int *height);
bool history_redraw(struct history *history);
bool history_click(struct browser_window *bw, struct history *history,
		int x, int y, bool new_window);
const char *history_position_url(struct history *history, int x, int y);

#endif
