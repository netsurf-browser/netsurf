/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_TREE_H
#define AMIGA_TREE_H

#include <exec/types.h>
#include <intuition/classusr.h>
#include "amiga/gui.h"

struct treeview_window {
	struct nsObject *node;
	struct Window *win;
	Object *objects[OID_LAST];
	struct Gadget *gadgets[GID_LAST];
	struct tree *tree;
	struct List *listbrowser_list;
};

enum
{
	AMI_TREE_HOTLIST,
	AMI_TREE_HISTORY,
	AMI_TREE_COOKIES,
	AMI_TREE_SSLCERT
};

enum
{
	AMI_MOVE_UP,
	AMI_MOVE_DOWN,
	AMI_MOVE_OUT
};

void ami_open_tree(struct tree *tree,int type);
void ami_tree_close(struct treeview_window *twin);
BOOL ami_tree_event(struct treeview_window *twin);
void ami_recreate_listbrowser(struct treeview_window *twin);
#endif
