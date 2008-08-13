/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
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
 * Generic tree handling (implementation).
 */


#define __STDBOOL_H__	1
extern "C" {
#include "utils/config.h"
#include "desktop/tree.h"
}


/**
 * Sets the origin variables to the correct values for a specified tree
 *
 * \param tree  the tree to set the origin for
 */
void tree_initialise_redraw(struct tree *tree) {
}


/**
 * Informs the current window manager that an area requires updating.
 *
 * \param tree	  the tree that is requesting a redraw
 * \param x	  the x co-ordinate of the redraw area
 * \param y	  the y co-ordinate of the redraw area
 * \param width	  the width of the redraw area
 * \param height  the height of the redraw area
 */
void tree_redraw_area(struct tree *tree, int x, int y, int width, int height) {
}


/**
 * Draws a line.
 *
 * \param x	 the x co-ordinate
 * \param x	 the y co-ordinate
 * \param x	 the width of the line
 * \param x	 the height of the line
 */
void tree_draw_line(int x, int y, int width, int height) {
}


/**
 * Draws an element, including any expansion icons
 *
 * \param tree	   the tree to draw an element for
 * \param element  the element to draw
 */
void tree_draw_node_element(struct tree *tree, struct node_element *element) {
}


/**
 * Draws an elements expansion icon
 *
 * \param tree	   the tree to draw the expansion for
 * \param element  the element to draw the expansion for
 */
void tree_draw_node_expansion(struct tree *tree, struct node *node) {
}


/**
 * Recalculates the dimensions of a node element.
 *
 * \param element  the element to recalculate
 */
void tree_recalculate_node_element(struct node_element *element) {
}

/**
 * Sets a node element as having a specific sprite.
 *
 * \param node      the node to update
 * \param sprite    the sprite to use
 * \param selected  the expanded sprite name to use
 */
void tree_set_node_sprite(struct node *node, const char *sprite,
                const char *expanded) {

}

/**
 * Sets a node element as having a folder sprite
 *
 * \param node  the node to update
 */
void tree_set_node_sprite_folder(struct node *node) {

}

/**
 * Updates the node details for a URL node.
 * The internal node dimensions are not updated.
 *
 * \param node  the node to update
 */
void tree_update_URL_node(struct node *node, const char *url,
		const struct url_data *data) {
}


/**
 * Updates the tree owner following a tree resize
 *
 * \param tree  the tree to update the owner of
 */
void tree_resized(struct tree *tree) {
}
