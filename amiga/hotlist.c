/*
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
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

#include "desktop/browser.h"
#include "desktop/tree.h"
#include <proto/exec.h>
#include "content/urldb.h"

void hotlist_visited(struct content *content)
{
}

void ami_hotlist_init(struct tree **hotlist)
{
	struct tree *hotlist_tree;
	struct node *node;
	*hotlist = AllocVec(sizeof(struct tree),MEMF_CLEAR);
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
	node = tree_create_folder_node(hotlist_tree->root, "NetSurf");
	if (!node)
		node = hotlist_tree->root;

/*
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
*/
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
}
