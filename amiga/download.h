/*
 * Copyright 2008-9 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_DOWNLOAD_H
#define AMIGA_DOWNLOAD_H
#include "amiga/gui.h"

struct dlnode
{
	struct Node node;
	char *filename;
};

struct gui_download_window {
	struct Window *win;
	Object *objects[OID_LAST];
	struct Gadget *gadgets[GID_LAST];
	struct nsObject *node;
	ULONG pad[5];
	BPTR fh;
	uint32 size;
	uint32 downloaded;
	struct dlnode *dln;
	struct browser_window *bw;
	struct fetch *fetch;
	char *url;
	char fname[1024];
};

int drag_save;
void *drag_save_data;
struct gui_window *drag_save_gui;

void ami_download_window_abort(struct gui_download_window *dw);
void ami_drag_save(struct Window *win);
void ami_free_download_list(struct List *dllist);
void ami_superimpose_favicon(STRPTR path, struct content *icon, STRPTR type);
#endif
