/*
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
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

#include "desktop/browser.h"
#include "desktop/tree.h"
#include <proto/exec.h>
#include "content/urldb.h"
#include "amiga/hotlist.h"
#include "amiga/tree.h"

void ami_gui_hotlist_visited(struct content *content, struct tree *tree,
		struct node *node);

static const struct {
	const char *url;
	const char *msg_key;
} default_entries[] = {
	{ "http://www.netsurf-browser.org/", "HotlistHomepage" },
	{ "http://www.netsurf-browser.org/downloads/amiga/", "HotlistTestBuild" },
	{ "http://www.netsurf-browser.org/documentation/", "HotlistDocumentation" },
	{ "http://sourceforge.net/tracker/?atid=464312&group_id=51719",
			"HotlistBugTracker" },
	{ "http://sourceforge.net/tracker/?atid=464315&group_id=51719",
			"HotlistFeatureRequest" },
	{ "http://www.unsatisfactorysoftware.co.uk",
			"Unsatisfactory Software" }
};
#define ENTRIES_COUNT (sizeof(default_entries) / sizeof(default_entries[0]))

void hotlist_visited(struct content *content)
{
	if ((!content) || (!content->url) || (!hotlist))
		return;
	ami_gui_hotlist_visited(content, hotlist, hotlist->root);
}

/**
 * Informs the hotlist that some content has been visited
 *
 * \param content  the content visited
 * \param tree	   the tree to find the URL data from
 * \param node	   the node to update siblings and children of
 */
void ami_gui_hotlist_visited(struct content *content, struct tree *tree,
		struct node *node)
{
	struct node_element *element;

	for (; node; node = node->next) {
		if (!node->folder) {
			element = tree_find_element(node, TREE_ELEMENT_URL);
			if ((element) && (!strcmp(element->text,
					content->url))) {
				tree_update_URL_node(node, content->url, NULL);
				tree_handle_node_changed(tree, node, true,
						false);
			}
		}
		if (node->child)
			ami_gui_hotlist_visited(content, tree, node->child);
	}
}

void ami_hotlist_init(struct tree **hotlist)
{
	struct tree *hotlist_tree;
	struct node *node;
	int i;
	const struct url_data *data;

	*hotlist = AllocVec(sizeof(struct tree),MEMF_PRIVATE | MEMF_CLEAR);
	hotlist_tree = *hotlist;

	if (!hotlist_tree) {
		warn_user("NoMemory", 0);
		return;
	}

	hotlist_tree->root = tree_create_folder_node(NULL, "Root");
	if (!hotlist_tree->root) {
		warn_user("NoMemory", 0);
		FreeVec(hotlist_tree);
		hotlist_tree = NULL;
	}

	hotlist_tree->root->expanded = true;

	node = tree_create_folder_node(hotlist_tree->root, "Menu");
	if (!node)
		node = hotlist_tree->root;

	node = tree_create_folder_node(node, "NetSurf");
	if (!node)
		node = hotlist_tree->root;

	for (i = 0; i != ENTRIES_COUNT; i++) {
			data = urldb_get_url_data(default_entries[i].url);
			if (!data) {
				urldb_add_url(default_entries[i].url);
				urldb_set_url_persistence(
						default_entries[i].url,
						true);
				data = urldb_get_url_data(
						default_entries[i].url);
			}
			if (data) {
				tree_create_URL_node(node,
					default_entries[i].url, data,
					messages_get(default_entries[i].msg_key));
			}
		}

		tree_initialise(hotlist_tree);
}

void ami_hotlist_add(struct node *node,struct content *c)
{
	const struct url_data *data;

	data = urldb_get_url_data(c->url);
	if (!data)
	{
		urldb_add_url(c->url);
		urldb_set_url_persistence(c->url,true);
		data = urldb_get_url_data(c->url);
	}

	if (data)
	{
		tree_create_URL_node(node,c->url,data,c->title);
	}

	tree_handle_node_changed(hotlist,node,false,true);

	if(hotlist->handle)
		ami_recreate_listbrowser((struct treeview_window *)hotlist->handle);
}
