/*
 * Copyright 2015 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
 * Interface to Intuition-based context menu operations
 */

#ifndef AMIGA_CTXMENU_H
#define AMIGA_CTXMENU_H 1

struct Hook;
struct Menu;
struct gui_window_2;

enum {
	AMI_CTXMENU_HISTORY_BACK = 0,
	AMI_CTXMENU_HISTORY_FORWARD = 1
};

/**
 * Initialise context menus code (allocate label text, etc)
 * Must be called *after* NetSurf's screen pointer is obtained.
 */
void ami_ctxmenu_init(void);

/**
 * Cleanup context menus code
 */
void ami_ctxmenu_free(void);

/**
 * Get a Hook for WA_ContextMenuHook
 *
 * \param data ptr for the hook to use (struct gui_window_2 *)
 * \returns pointer to a struct Hook
 */
struct Hook *ami_ctxmenu_get_hook(APTR data);

/**
 * Release a Hook for WA_ContextMenuHook
 *
 * \param hook ptr to hook
 */
void ami_ctxmenu_release_hook(struct Hook *hook);

/**
 * Create history context menu.  First run sets up the menu, next creates entries.
 *
 * \param gwin struct gui_window_2 *
 * \returns pointer to menu
 */
struct Menu *ami_ctxmenu_history_create(int direction, struct gui_window_2 *gwin);


#else
inline void ami_ctxmenu_init(void) {}
inline void ami_ctxmenu_free(void) {}
inline struct Hook *ami_ctxmenu_get_hook(APTR data) {return NULL;}
inline void ami_ctxmenu_release_hook(struct Hook *hook) {}
#endif
