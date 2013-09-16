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

struct atari_treeview_callbacks {
	nserror (*init)(struct core_window *cw,
				struct core_window_callback_table * default_callbacks);
	void (*fini)(struct core_window *cw);
	void (*draw)(struct core_window *cw);
	void (*keypress)(struct core_window *cw);
	void (*mouse)(struct core_window *cw);
	gemtk_wm_event_handler_f gemtk_user_func;
};

struct atari_treeview_callbacks;
struct atari_treeview_window;

struct atari_treeview_window *
atari_treeview_create(GUIWIN *win, struct atari_treeview_callbacks * callbacks,
					uint32_t flags);
void atari_treeview_delete(struct atari_treeview_window * cw);

#endif //NSATARI_TREEVIEW_H

