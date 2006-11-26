/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

/** \file
 * Frame and frameset creation and manipulation (interface).
 */

#ifndef _NETSURF_DESKTOP_FRAMES_H_
#define _NETSURF_DESKTOP_FRAMES_H_

#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"


void browser_window_create_iframes(struct browser_window *bw,
		struct content_html_iframe *iframe);
void browser_window_recalculate_iframes(struct browser_window *bw);
void browser_window_create_frameset(struct browser_window *bw,
		struct content_html_frames *frameset);
void browser_window_recalculate_frameset(struct browser_window *bw);
bool browser_window_resize_frames(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y,
		gui_pointer_shape *pointer, const char **status, bool *action);
void browser_window_resize_frame(struct browser_window *bw, int x, int y);

#endif
