/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Generic tree handling (implementation).
 */


#include "netsurf/desktop/tree.h"


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
 * \param tree	 the tree to draw a line for
 * \param x	 the x co-ordinate
 * \param x	 the y co-ordinate
 * \param x	 the width of the line
 * \param x	 the height of the line
 */
void tree_draw_line(struct tree *tree, int x, int y, int width, int height) {
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
 * Updates the node details for a URL node.
 * The internal node dimensions are not updated.
 *
 * \param node  the node to update
 */
void tree_update_URL_node(struct node *node) {
}


/**
 * Updates the tree owner following a tree resize
 *
 * \param tree  the tree to update the owner of
 */
void tree_resized(struct tree *tree) {
}


/**
 * Sets a node element as having a folder sprite
 *
 * \param node  the node to update
 */
void tree_set_node_sprite_folder(struct node *node) {
}
