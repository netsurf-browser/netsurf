/*
 * Copyright 2008, 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/exec.h>
#include "amiga/hotlist.h"
#include "amiga/tree.h"
#include "desktop/hotlist_old.h"
#include "utils/messages.h"

bool ami_hotlist_find_dir(struct tree *tree, const char *dir_name)
{
	struct node *root = tree_node_get_child(tree_get_root(tree));
	struct node *node;
	struct node_element *element;

	for (node = root; node; node = tree_node_get_next(node))
	{
		element = tree_node_find_element(node, TREE_ELEMENT_TITLE, NULL);
		if(!element) element = tree_node_find_element(node, TREE_ELEMENT_TITLE, NULL);
		if(element && (strcmp(tree_node_element_get_text(element), dir_name) == 0))
		{
			return true;
		}
	}

	return false;
}

void ami_hotlist_add_default_dirs(struct tree *tree)
{
	if(ami_hotlist_find_dir(tree, messages_get("HotlistMenu")) == false) {
		tree_create_folder_node(tree, tree_get_root(tree),
			messages_get("HotlistMenu"), true, true, false);
	}

	if(ami_hotlist_find_dir(tree, messages_get("HotlistToolbar")) == false) {
		tree_create_folder_node(tree, tree_get_root(tree),
			messages_get("HotlistToolbar"), true, true, false);
	}
}

void ami_hotlist_initialise(const char *hotlist_file)
{
	tree_hotlist_path = hotlist_file;
	hotlist_window = ami_tree_create(TREE_HOTLIST, NULL);

	if(!hotlist_window) return;

	hotlist_old_initialise(ami_tree_get_tree(hotlist_window),
			   hotlist_file, NULL);
			   
   ami_hotlist_add_default_dirs(ami_tree_get_tree(hotlist_window));
}

void ami_hotlist_free(const char *hotlist_file)
{
	hotlist_old_cleanup(hotlist_file);
	ami_tree_destroy(hotlist_window);
	hotlist_window = NULL;
}
