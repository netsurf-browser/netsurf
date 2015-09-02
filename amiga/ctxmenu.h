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

/**
 * Initialise context menus code
 */
void ami_ctxmenu_init(void);

/**
 * Cleanup context menus code
 */
void ami_ctxmenu_free(void);

/**
 * Get a Hook for WA_ContextMenuHook
 *
 * \returns pointer to a struct Hook
 */
struct Hook *ami_ctxmenu_get_hook(void);
#endif

