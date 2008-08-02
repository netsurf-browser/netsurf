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

#include "desktop/tree.h"

void tree_initialise_redraw(struct tree *tree)
{
}

void tree_redraw_area(struct tree *tree, int x, int y, int width, int height)
{
}

void tree_draw_line(int x, int y, int width, int height)
{
}

void tree_draw_node_element(struct tree *tree, struct node_element *element)
{
}

void tree_draw_node_expansion(struct tree *tree, struct node *node)
{
}

void tree_recalculate_node_element(struct node_element *element)
{
}

void tree_update_URL_node(struct node *node, const char *url,
	const struct url_data *data)
{
}

void tree_resized(struct tree *tree)
{
}

void tree_set_node_sprite_folder(struct node *node)
{
}

void tree_set_node_sprite(struct node *node, const char *sprite,
	const char *expanded)
{
}

