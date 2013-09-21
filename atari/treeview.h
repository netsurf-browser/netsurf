/*
 * Copyright 2013 Ole Loots <ole@monochrom.net>
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

#ifndef NSATARI_TREEVIEW_H
#define NSATARI_TREEVIEW_H

#include "desktop/core_window.h"
#include "atari/gui.h"
#include "atari/gemtk/gemtk.h"

#define ATARI_TREEVIEW_WIDGETS (CLOSER | MOVER | SIZER| NAME | FULLER | \
								SMALLER | VSLIDE | HSLIDE | UPARROW | DNARROW \
								| LFARROW | RTARROW)

enum treeview_area_e {
	TREEVIEW_AREA_WORK = 0,
	TREEVIEW_AREA_TOOLBAR,
	TREEVIEW_AREA_CONTENT
};

struct core_window;
struct atari_treeview_window;

typedef struct atari_treeview_window *ATARI_TREEVIEW_PTR;

// TODO: add drag_status callback!!
typedef nserror (*atari_treeview_init2_callback)(struct core_window *cw,
				struct core_window_callback_table * default_callbacks);
typedef void (*atari_treeview_finish_callback)(struct core_window *cw);
typedef void (*atari_treeview_keypress_callback)(struct core_window *cw,
												uint32_t ucs4);
typedef void (*atari_treeview_mouse_action_callback)(struct core_window *cw,
												browser_mouse_state mouse,
												int x, int y);
typedef void (*atari_treeview_draw_callback)(struct core_window *cw, int x,
											int y, struct rect *clip,
											const struct redraw_context *ctx);

struct atari_treeview_callbacks {
	atari_treeview_init2_callback init_phase2;
	atari_treeview_finish_callback finish;
	atari_treeview_draw_callback draw;
	atari_treeview_keypress_callback keypress;
	atari_treeview_mouse_action_callback mouse_action;
	gemtk_wm_event_handler_f gemtk_user_func;
};

struct atari_treeview_window *
atari_treeview_create(GUIWIN *win, struct atari_treeview_callbacks * callbacks,
					uint32_t flags);
void atari_treeview_delete(struct atari_treeview_window * cw);
void atari_treeview_open(struct atari_treeview_window * cw, GRECT *pos);
bool atari_treeview_is_open(struct atari_treeview_window *cw);
void atari_treeview_close(struct atari_treeview_window * cw);
GUIWIN * atari_treeview_get_gemtk_window(struct atari_treeview_window *tv);
void atari_treeview_get_grect(ATARI_TREEVIEW_PTR tptr, enum treeview_area_e mode,
									GRECT *dest);
void atari_treeview_redraw(struct atari_treeview_window *tv);
#endif //NSATARI_TREEVIEW_H

