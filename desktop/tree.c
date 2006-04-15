/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Generic tree handling (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "netsurf/content/urldb.h"
#include "netsurf/desktop/tree.h"
#include "netsurf/desktop/options.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

static void tree_draw_node(struct tree *tree, struct node *node, int clip_x,
		int clip_y, int clip_width, int clip_height);
static struct node_element *tree_create_node_element(struct node *parent,
		node_element_data data);
static int tree_get_node_width(struct node *node);
static int tree_get_node_height(struct node *node);
static void tree_handle_selection_area_node(struct tree *tree,
		struct node *node, int x, int y, int width, int height,
		bool invert);
static void tree_selected_to_processing(struct node *node);
void tree_clear_processing(struct node *node);
struct node *tree_move_processing_node(struct node *node, struct node *link,
		bool before, bool first);

static int tree_initialising = 0;


/**
 * Initialises a user-created tree
 *
 * \param tree  the tree to initialise
 */
void tree_initialise(struct tree *tree) {

	assert(tree);

	tree_set_node_expanded(tree->root, true);
	tree_initialise_nodes(tree->root);
	tree_recalculate_node_positions(tree->root);
	tree_set_node_expanded(tree->root, false);
	tree->root->expanded = true;
	tree_recalculate_node_positions(tree->root);
	tree_recalculate_size(tree);
}


/**
 * Initialises a user-created node structure
 *
 * \param root  the root node to update from
 */
void tree_initialise_nodes(struct node *root) {
	struct node *node;

	assert(root);

	tree_initialising++;
	for (node = root; node; node = node->next) {
		tree_recalculate_node(node, true);
		if (node->child) {
			tree_initialise_nodes(node->child);
		}
	}
	tree_initialising--;

	if (tree_initialising == 0)
		tree_recalculate_node_positions(root);
}


/**
 * Recalculate the node data and redraw the relevant section of the tree.
 *
 * \param tree		     the tree to redraw
 * \param node		     the node to update
 * \param recalculate_sizes  whether the elements have changed
 * \param expansion	     the request is the result of a node expansion
 */
void tree_handle_node_changed(struct tree *tree, struct node *node,
		bool recalculate_sizes, bool expansion) {
	int width, height;

	assert(node);

	if ((expansion) && (node->expanded) && (node->child)) {
		tree_set_node_expanded(node->child, false);
		tree_set_node_selected(tree, node->child, false);
	}

	width = node->box.width;
	height = node->box.height;
	if ((recalculate_sizes) || (expansion))
		tree_recalculate_node(node, true);
	if ((node->box.height != height) || (expansion)) {
		tree_recalculate_node_positions(tree->root);
		tree_redraw_area(tree, 0, node->box.y, 16384, 16384);
	} else {
		width = (width > node->box.width) ? width : node->box.width;
		tree_redraw_area(tree, node->box.x, node->box.y, width, node->box.height);
	}
	if ((recalculate_sizes) || (expansion))
		tree_recalculate_size(tree);
}


/**
 * Recalculate the node element and redraw the relevant section of the tree.
 * The tree size is not updated.
 *
 * \param tree	   the tree to redraw
 * \param element  the node element to update
 */
void tree_handle_node_element_changed(struct tree *tree, struct node_element *element) {
	int width, height;

	assert(element);

	width = element->box.width;
	height = element->box.height;
	tree_recalculate_node_element(element);

	if (element->box.height != height) {
		tree_recalculate_node(element->parent, false);
		tree_redraw_area(tree, 0, element->box.y, 16384, 16384);
	} else {
		if (element->box.width != width)
			tree_recalculate_node(element->parent, false);
		width = (width > element->box.width) ? width :
				element->box.width;
		tree_redraw_area(tree, element->box.x, element->box.y, width, element->box.height);
	}
}


/**
 * Recalculates the size of a node.
 *
 * \param node		     the node to update
 * \param recalculate_sizes  whether the node elements have changed
 */
void tree_recalculate_node(struct node *node, bool recalculate_sizes) {
	struct node_element *element;
	int width, height;

	assert(node);

	width = node->box.width;
	height = node->box.height;
	node->box.width = 0;
	node->box.height = 0;
	if (node->expanded) {
		for (element = &node->data; element; element = element->next) {
			if (recalculate_sizes)
				tree_recalculate_node_element(element);
			node->box.width = (node->box.width >
				element->box.x + element->box.width - node->box.x) ?
				node->box.width :
				element->box.width + element->box.x - node->box.x;
			node->box.height += element->box.height;
		}
	} else {
		if (recalculate_sizes)
			for (element = &node->data; element; element = element->next)
				tree_recalculate_node_element(element);
		else
			tree_recalculate_node_element(&node->data);
		node->box.width = node->data.box.width;
		node->box.height = node->data.box.height;
	}

	if (height != node->box.height) {
		for (; node->parent; node = node->parent);
		if (tree_initialising == 0)
			tree_recalculate_node_positions(node);
	}
}


/**
 * Recalculates the position of a node, its siblings and children.
 *
 * \param root  the root node to update from
 */
void tree_recalculate_node_positions(struct node *root) {
	struct node *parent;
	struct node *node;
	struct node *child;
	struct node_element *element;
	int y;

	for (node = root; node; node = node->next) {
		if (node->previous) {
			node->box.x = node->previous->box.x;
			node->box.y = node->previous->box.y +
					tree_get_node_height(node->previous);
		} else if ((parent = node->parent)) {
			node->box.x = parent->box.x + NODE_INSTEP;
			node->box.y = parent->box.y +
					parent->box.height;
			for (child = parent->child; child != node;
					child = child->next)
				node->box.y += child->box.height;
		} else {
			node->box.x = 0;
			node->box.y = -40;
		}
		if (node->expanded) {
			if (node->folder) {
				node->data.box.x = node->box.x;
				node->data.box.y = node->box.y;
				tree_recalculate_node_positions(node->child);
			} else {
				y = node->box.y;
				for (element = &node->data; element;
						element = element->next) {
					if (element->type == NODE_ELEMENT_TEXT_PLUS_SPRITE) {
						element->box.x = node->box.x;
					} else {
						element->box.x = node->box.x + NODE_INSTEP;
					}
					element->box.y = y;
					y += element->box.height;
				}
			}
		} else {
			node->data.box.x = node->box.x;
			node->data.box.y = node->box.y;
		}
	}
}


/**
 * Calculates the width of a node including any children
 *
 * \param node  the node to calculate the height of
 * \return the total width of the node and children
 */
int tree_get_node_width(struct node *node) {
	int width = 0;
	int child_width;

	assert(node);

	for (; node; node = node->next) {
		if (width < (node->box.x + node->box.width))
			width = node->box.x + node->box.width;
		if ((node->child) && (node->expanded)) {
			child_width = tree_get_node_width(node->child);
			if (width < child_width)
				width = child_width;
		}
	}
	return width;
}


/**
 * Calculates the height of a node including any children
 *
 * \param node  the node to calculate the height of
 * \return the total height of the node and children
 */
int tree_get_node_height(struct node *node) {
	int y1;

	assert(node);

	if ((node->child) && (node->expanded)) {
		y1 = node->box.y;
		if (y1 < 0)
			y1 = 0;
		node = node->child;
		while ((node->next) || ((node->child) && (node->expanded))) {
			for (; node->next; node = node->next);
			if ((node->child) && (node->expanded))
				node = node->child;
		}
		return node->box.y + node->box.height - y1;
	} else {
		return node->box.height;
	}
}


/**
 * Updates all siblinds and descendants of a node to an expansion state.
 * No update is performed for the tree changes.
 *
 * \param node	    the node to set all siblings and descendants of
 * \param expanded  the expansion state to set
 */
void tree_set_node_expanded(struct node *node, bool expanded) {
	for (; node; node = node->next) {
		if (node->expanded != expanded) {
			node->expanded = expanded;
			tree_recalculate_node(node, false);
		}
		if ((node->child) && (node->expanded))
			tree_set_node_expanded(node->child, expanded);
	}
}


/**
 * Updates all siblinds and descendants of a node to an expansion state.
 *
 * \param tree	    the tree to update
 * \param node	    the node to set all siblings and descendants of
 * \param expanded  the expansion state to set
 * \param folder    whether to update folders
 * \param leaf	    whether to update leaves
 * \return whether any changes were made
 */
bool tree_handle_expansion(struct tree *tree, struct node *node, bool expanded, bool folder,
		bool leaf) {
	struct node *entry = node;
	bool redraw = false;

	for (; node; node = node->next) {
		if ((node->expanded != expanded) && (node != tree->root) &&
				((folder && (node->folder)) || (leaf && (!node->folder)))) {
			node->expanded = expanded;
			if (node->child)
				tree_set_node_expanded(node->child, false);
			if ((node->data.next) && (node->data.next->box.height == 0))
				tree_recalculate_node(node, true);
			else
				tree_recalculate_node(node, false);
			redraw = true;
		}
		if ((node->child) && (node->expanded))
			redraw |= tree_handle_expansion(tree, node->child, expanded, folder, leaf);
	}
	if ((entry == tree->root) && (redraw)) {
		tree_recalculate_node_positions(tree->root);
		tree_redraw_area(tree, 0, 0, 16384, 16384);
		tree_recalculate_size(tree);
	}
	return redraw;
}


/**
 * Updates all siblinds and descendants of a node to an selected state.
 * The required areas of the tree are redrawn.
 *
 * \param tree	    the tree to update nodes for
 * \param node	    the node to set all siblings and descendants of
 * \param selected  the selection state to set
 */
void tree_set_node_selected(struct tree *tree, struct node *node, bool selected) {
	for (; node; node = node->next) {
		if ((node->selected != selected) && (node != tree->root)) {
			node->selected = selected;
			tree_redraw_area(tree, node->box.x, node->box.y, node->box.width,
					node->data.box.height);
		}
		if ((node->child) && (node->expanded))
			tree_set_node_selected(tree, node->child, selected);
	}
}


/**
 * Finds a node at a specific location.
 *
 * \param root	     the root node to check from
 * \param x	     the x co-ordinate
 * \param y	     the y co-ordinate
 * \param furniture  whether the returned area was in an elements furniture
 * \return the node at the specified position, or NULL for none
 */
struct node *tree_get_node_at(struct node *root, int x, int y, bool *furniture) {
	struct node_element *result;

	if ((result = tree_get_node_element_at(root, x, y, furniture)))
		return result->parent;
	return NULL;
}


/**
 * Finds a node element at a specific location.
 *
 * \param node	     the root node to check from
 * \param x	     the x co-ordinate
 * \param y	     the y co-ordinate
 * \param furniture  whether the returned area was in an elements furniture
 * \return the node at the specified position, or NULL for none
 */
struct node_element *tree_get_node_element_at(struct node *node, int x, int y,
		bool *furniture) {
	struct node_element *element;

	*furniture = false;
	for (; node; node = node->next) {
		if (node->box.y > y) return NULL;
		if ((node->box.x - NODE_INSTEP < x) && (node->box.y < y) &&
				(node->box.x + node->box.width >= x) &&
				(node->box.y + node->box.height >= y)) {
			if (node->expanded) {
				for (element = &node->data; element;
						element = element->next) {
					if ((element->box.x < x) && (element->box.y < y) &&
							(element->box.x + element->box.width >= x) &&
							(element->box.y + element->box.height >= y))
						return element;
				}
			} else if ((node->data.box.x < x) &&
					(node->data.box.y < y) &&
					(node->data.box.x + node->data.box.width >= x) &&
					(node->data.box.y + node->data.box.height >= y))
				return &node->data;
			if (((node->child) || (node->data.next)) &&
					(node->data.box.x - NODE_INSTEP + 8 < x) &&
					(node->data.box.y + 8 < y) &&
					(node->data.box.x > x) &&
					(node->data.box.y + 32 > y)) {
				*furniture = true;
				return &node->data;
			}
		}

		if ((node->child) && (node->expanded) &&
				((element = tree_get_node_element_at(node->child, x, y,
				furniture))))
			return element;
	}
	return NULL;
}


/**
 * Finds a node element from a node with a specific user_type
 *
 * \param node	     the node to examine
 * \param user_type  the user_type to check for
 * \return the corresponding element
 */
struct node_element *tree_find_element(struct node *node, node_element_data data) {
	struct node_element *element;
	for (element = &node->data; element; element = element->next)
		if (element->data == data) return element;
	return NULL;
}


/**
 * Moves nodes within a tree.
 *
 * \param tree	  the tree to process
 * \param link	  the node to link before/as a child (folders) or before/after (link)
 * \param before  whether to link siblings before or after the supplied node
 */
void tree_move_selected_nodes(struct tree *tree, struct node *destination, bool before) {
	struct node *link;
	struct node *test;
	bool error;

	tree_clear_processing(tree->root);
	tree_selected_to_processing(tree->root);

	/* the destination node cannot be a child of any node with the processing flag set */
	error = destination->processing;
	for (test = destination; test; test = test->parent)
		error |= test->processing;
	if (error) {
		tree_clear_processing(tree->root);
		return;
	}
	if ((destination->folder) && (!destination->expanded) && (!before)) {
		destination->expanded = true;
		tree_handle_node_changed(tree, destination, false, true);
	}
	link = tree_move_processing_node(tree->root, destination, before, true);
	while (link)
		link = tree_move_processing_node(tree->root, link, false, false);

	tree_clear_processing(tree->root);
	tree_recalculate_node_positions(tree->root);
	tree_redraw_area(tree, 0, 0, 16384, 16384);
}


/**
 * Sets the processing flag to the selection state.
 *
 * \param node	  the node to process siblings and children of
 */
void tree_selected_to_processing(struct node *node) {
	for (; node; node = node->next) {
		node->processing = node->selected;
		if ((node->child) && (node->expanded))
			tree_selected_to_processing(node->child);
	}
}


/**
 * Clears the processing flag.
 *
 * \param node	  the node to process siblings and children of
 */
void tree_clear_processing(struct node *node) {
	for (; node; node = node->next) {
		node->processing = false;
		if (node->child)
			tree_clear_processing(node->child);
	}
}


/**
 * Moves the first node in a tree with the processing flag set.
 *
 * \param tree	  the node to move siblings/children of
 * \param link	  the node to link before/as a child (folders) or before/after (link)
 * \param before  whether to link siblings before or after the supplied node
 * \param first   whether to always link after the supplied node (ie not inside of folders)
 * \return the node moved
 */
struct node *tree_move_processing_node(struct node *node, struct node *link, bool before,
		bool first) {
	struct node *result;

	bool folder = link->folder;
	for (; node; node = node->next) {
	  	if (node->processing) {
	  		node->processing = false;
	  	  	tree_delink_node(node);
	  	  	if (!first)
	  	  		link->folder = false;
	  	  	tree_link_node(link, node, before);
	  	  	if (!first)
	  	  		link->folder = folder;
	  		return node;
	  	}
		if (node->child) {
		  	result = tree_move_processing_node(node->child, link, before, first);
		  	if (result)
		  		return result;
		}
	}
	return NULL;
}

/**
 * Checks whether a node, its siblings or any children are selected.
 *
 * \param node  the root node to check from
 */
bool tree_has_selection(struct node *node) {
	for (; node; node = node->next) {
		if (node->selected)
			return true;
		if ((node->child) && (node->expanded) &&
				(tree_has_selection(node->child)))
			return true;
	}
	return false;
}


/**
 * Updates the selected state for a region of nodes.
 *
 * \param tree	  the tree to update
 * \param x	  the minimum x of the selection rectangle
 * \param y	  the minimum y of the selection rectangle
 * \param width	  the width of the selection rectangle
 * \param height  the height of the selection rectangle
 * \param invert  whether to invert the selected state
 */
void tree_handle_selection_area(struct tree *tree, int x, int y, int width, int height,
		bool invert) {
	assert(tree);
	assert(tree->root);

	if (!tree->root->child) return;

	if (width < 0) {
		x += width;
		width =- width;
	}
	if (height < 0) {
		y += height;
		height =- height;
	}

	tree_handle_selection_area_node(tree, tree->root->child, x, y, width, height, invert);
}


/**
 * Updates the selected state for a region of nodes.
 *
 * \param tree	  the tree to update
 * \param node	  the node to update children and siblings of
 * \param x	  the minimum x of the selection rectangle
 * \param y	  the minimum y of the selection rectangle
 * \param width	  the width of the selection rectangle
 * \param height  the height of the selection rectangle
 * \param invert  whether to invert the selected state
 */
void tree_handle_selection_area_node(struct tree *tree, struct node *node, int x, int y,
		int width, int height, bool invert) {

	struct node_element *element;
	struct node *update;
	int x_max, y_max;

	assert(tree);
	assert(node);

	x_max = x + width;
	y_max = y + height;

	for (; node; node = node->next) {
		if (node->box.y > y_max) return;
		if ((node->box.x < x_max) && (node->box.y < y_max) &&
				(node->box.x + node->box.width + NODE_INSTEP >= x) &&
				(node->box.y + node->box.height >= y)) {
			update = NULL;
			if (node->expanded) {
				for (element = &node->data; element;
						element = element->next) {
					if ((element->box.x < x_max) && (element->box.y < y_max) &&
							(element->box.x + element->box.width >= x) &&
							(element->box.y + element->box.height >= y)) {
						update = element->parent;
						break;
					}
				}
			} else if ((node->data.box.x < x_max) &&
					(node->data.box.y < y_max) &&
					(node->data.box.x + node->data.box.width >= x) &&
					(node->data.box.y + node->data.box.height >= y))
				update = node->data.parent;
			if ((update) && (node != tree->root)) {
				if (invert) {
					node->selected = !node->selected;
					tree_handle_node_element_changed(tree, &node->data);
				} else if (!node->selected) {
					node->selected = true;
					tree_handle_node_element_changed(tree, &node->data);
				}
			}
		}
		if ((node->child) && (node->expanded))
			tree_handle_selection_area_node(tree, node->child, x, y, width, height,
					invert);
	}
}


/**
 * Redraws a tree.
 *
 * \param tree	       the tree to draw
 * \param clip_x       the minimum x of the clipping rectangle
 * \param clip_y       the minimum y of the clipping rectangle
 * \param clip_width   the width of the clipping rectangle
 * \param clip_height  the height of the clipping rectangle
 */
void tree_draw(struct tree *tree, int clip_x, int clip_y, int clip_width,
		int clip_height) {
	assert(tree);
	assert(tree->root);

	if (!tree->root->child) return;

	tree_initialise_redraw(tree);
	tree_draw_node(tree, tree->root->child, clip_x,
			clip_y, clip_width, clip_height);
}


/**
 * Redraws a node.
 *
 * \param tree	       the tree to draw
 * \param node	       the node to draw children and siblings of
 * \param clip_x       the minimum x of the clipping rectangle
 * \param clip_y       the minimum y of the clipping rectangle
 * \param clip_width   the width of the clipping rectangle
 * \param clip_height  the height of the clipping rectangle
 */
void tree_draw_node(struct tree *tree, struct node *node, int clip_x, int clip_y,
		int clip_width, int clip_height) {

	struct node_element *element;
	int x_max, y_max;

	assert(tree);
	assert(node);

	x_max = clip_x + clip_width + NODE_INSTEP;
	y_max = clip_y + clip_height;

	if ((node->parent->next) && (node->parent->next->box.y < clip_y))
		return;

	for (; node; node = node->next) {
		if (node->box.y > y_max) return;
		if (node->next)
			tree_draw_line(node->box.x - (NODE_INSTEP / 2),
					node->box.y + (40 / 2), 0,
					node->next->box.y - node->box.y);
		if ((node->box.x < x_max) && (node->box.y < y_max) &&
				(node->box.x + node->box.width + NODE_INSTEP >= clip_x) &&
				(node->box.y + node->box.height >= clip_y)) {
			if ((node->expanded) && (node->child))
				tree_draw_line(node->box.x + (NODE_INSTEP / 2),
						node->data.box.y + node->data.box.height, 0,
						(40 / 2));
			tree_draw_line(node->box.x - (NODE_INSTEP / 2),
					node->data.box.y +
					node->data.box.height - (40 / 2),
					(NODE_INSTEP / 2) - 4, 0);
			tree_draw_node_expansion(tree, node);
			if (node->expanded)
				for (element = &node->data; element;
						element = element->next)
					tree_draw_node_element(tree, element);
			else
				tree_draw_node_element(tree, &node->data);
		}
		if ((node->child) && (node->expanded))
			tree_draw_node(tree, node->child, clip_x, clip_y, clip_width,
					clip_height);
	}
}


/**
 * Gets link characteristics to insert a node at a specified position.
 *
 * \param tree	  the tree to find link information for
 * \param x	  the x co-ordinate
 * \param y	  the y co-ordinate
 * \param before  set to whether the node should be linked before on exit
 * \return the node to link with
 */
struct node *tree_get_link_details(struct tree *tree, int x, int y, bool *before) {
	struct node *node = NULL;
	bool furniture;

	assert(tree);
	assert(tree->root);

	*before = false;
	if (tree->root->child)
		node = tree_get_node_at(tree->root->child, x, y, &furniture);
	if ((!node) || (furniture))
		return tree->root;

	if (y < (node->box.y + (node->box.height / 2))) {
		*before = true;
	} else if ((node->folder) && (node->expanded) && (node->child)) {
		node = node->child;
		*before = true;
	}
	return node;
}


/**
 * Links a node into the tree.
 *
 * \param link	  the node to link before/as a child (folders) or before/after (link)
 * \param node	  the node to link
 * \param before  whether to link siblings before or after the supplied node
 */
void tree_link_node(struct node *link, struct node *node, bool before) {
	assert(link);
	assert(node);

	if ((!link->folder) || (before)) {
		node->parent = link->parent;
		if (before) {
			node->next = link;
			node->previous = link->previous;
			if (link->previous) link->previous->next = node;
			link->previous = node;
			if ((link->parent) && (link->parent->child == link))
				link->parent->child = node;
		} else {
			node->previous = link;
			node->next = link->next;
			if (link->next) link->next->previous = node;
			link->next = node;
		}
	} else {
		if (!link->child) {
			link->child = link->last_child = node;
			node->previous = NULL;
		} else {
			link->last_child->next = node;
			node->previous = link->last_child;
			link->last_child = node;
		}
		node->parent = link;
		node->next = NULL;
	}
	node->deleted = false;
}


/**
 * Delinks a node from the tree.
 *
 * \param node  the node to delink
 */
void tree_delink_node(struct node *node) {
	assert(node);

	if (node->parent) {
		if (node->parent->child == node)
			node->parent->child = node->next;
		if (node->parent->last_child == node)
			node->parent->last_child = node->previous;
		if (node->parent->child == NULL)
			node->parent->expanded = false;
		node->parent = NULL;
	}
	if (node->previous)
		node->previous->next = node->next;
	if (node->next)
		node->next->previous = node->previous;
	node->previous = NULL;
	node->next = NULL;
}


/**
 * Deletes all selected node from the tree.
 *
 * \param tree  the tree to delete from
 * \param node  the node to delete
 */
void tree_delete_selected_nodes(struct tree *tree, struct node *node) {
	struct node *next;

	while (node) {
		next = node->next;
		if ((node->selected) && (node != tree->root))
			tree_delete_node(tree, node, false);
		else if (node->child)
			tree_delete_selected_nodes(tree, node->child);
		node = next;
	}
}


/**
 * Deletes a node from the tree.
 *
 * \param tree	    the tree to delete from
 * \param node	    the node to delete
 * \param siblings  whether to delete all siblings
 */
void tree_delete_node(struct tree *tree, struct node *node, bool siblings) {
	struct node *next;
	struct node *parent;
	struct node_element *e, *f;

	assert(node);

	if (tree->temp_selection == node)
		tree->temp_selection = NULL;

	next = node->next;
	if (node->child)
		tree_delete_node(tree, node->child, true);
	node->child = NULL;
	parent = node->parent;
	tree_delink_node(node);

	if (!node->retain_in_memory) {
		for (e = &node->data; e; e = f) {
			f = e->next;

			if (e->text) {
				/* we don't free non-editable titles or URLs */
				if (node->editable)
					free(e->text);
				else {
					if (e->data == TREE_ELEMENT_URL) {
						/* reset URL characteristics */
						urldb_reset_url_visit_data(e->text);
					}

					if (e->data != TREE_ELEMENT_TITLE &&
						e->data != TREE_ELEMENT_URL)
						free(e->text);
				}
			}
			if (e->sprite)
				free(e->sprite);	/* \todo platform specific bits */

			if (e != &node->data)
				free(e);
		}
		free(node);
	} else {
		node->deleted = true;
	}
	if (siblings && next)
		tree_delete_node(tree, next, true);

	tree_recalculate_node_positions(tree->root);
	tree_redraw_area(tree, 0, 0, 16384, 16384);	/* \todo correct area */
	tree_recalculate_size(tree);
}


/**
 * Creates a folder node with the specified title, and links it into the tree.
 *
 * \param parent  the parent node, or NULL not to link
 * \param title	  the node title (copied)
 * \return the newly created node.
 */
struct node *tree_create_folder_node(struct node *parent, const char *title) {
	struct node *node;

	assert(title);

	node = calloc(sizeof(struct node), 1);
	if (!node) return NULL;
	node->editable = true;
	node->folder = true;
	node->data.parent = node;
	node->data.type = NODE_ELEMENT_TEXT;
	node->data.text = squash_whitespace(title);
	node->data.data = TREE_ELEMENT_TITLE;
	tree_set_node_sprite_folder(node);
	if (parent)
		tree_link_node(parent, node, false);
	tree_recalculate_node(node, true);
	return node;
}


/**
 * Creates a leaf node with the specified title, and links it into the tree.
 *
 * \param parent  the parent node, or NULL not to link
 * \param title	  the node title (copied)
 * \return the newly created node.
 */
struct node *tree_create_leaf_node(struct node *parent, const char *title) {
	struct node *node;

	assert(title);

	node = calloc(sizeof(struct node), 1);
	if (!node) return NULL;
	node->folder = false;
	node->data.parent = node;
	node->data.type = NODE_ELEMENT_TEXT;
	node->data.text = squash_whitespace(title);
	node->data.data = TREE_ELEMENT_TITLE;
	if (parent)
		tree_link_node(parent, node, false);
	return node;
}


/**
 * Creates a tree entry for a URL, and links it into the tree
 *
 *
 * \param parent     the node to link to
 * \param url        the URL (copied)
 * \param data	     the URL data to use
 * \param title	     the custom title to use
 * \return the node created, or NULL for failure
 */
struct node *tree_create_URL_node(struct node *parent,
		const char *url, const struct url_data *data,
		const char *title) {
	struct node *node;
	struct node_element *element;

	assert(data);

	if (!title) {
		if (data->title)
			title = strdup(data->title);
		else
			title = strdup(url);
		if (!title)
			return NULL;
	}
	node = tree_create_leaf_node(parent, title);
	if (!node)
		return NULL;
	node->editable = true;

	element = tree_create_node_element(node, TREE_ELEMENT_THUMBNAIL);
	if (element)
		element->type = NODE_ELEMENT_THUMBNAIL;
	tree_create_node_element(node, TREE_ELEMENT_VISITS);
	tree_create_node_element(node, TREE_ELEMENT_LAST_VISIT);
	element = tree_create_node_element(node, TREE_ELEMENT_URL);
	if (element)
		element->text = strdup(url);

	tree_update_URL_node(node, url, NULL);
	tree_recalculate_node(node, false);

	return node;
}


/**
 * Creates a tree entry for a URL, and links it into the tree.
 *
 * All information is used directly from the url_data, and as such cannot be
 * edited and should never be freed.
 *
 * \param parent      the node to link to
 * \param url         the URL
 * \param data	      the URL data to use
 * \return the node created, or NULL for failure
 */
struct node *tree_create_URL_node_shared(struct node *parent,
		const char *url, const struct url_data *data) {
	struct node *node;
	struct node_element *element;
	char *title;

	assert(url && data);

	if (data->title)
		title = data->title;
	else
		title = url;
	node = tree_create_leaf_node(parent, title);
	if (!node)
		return NULL;
	free(node->data.text);
	node->data.text = title;
	node->editable = false;

	element = tree_create_node_element(node, TREE_ELEMENT_THUMBNAIL);
	if (element)
		element->type = NODE_ELEMENT_THUMBNAIL;
	tree_create_node_element(node, TREE_ELEMENT_VISITS);
	tree_create_node_element(node, TREE_ELEMENT_LAST_VISIT);
	element = tree_create_node_element(node, TREE_ELEMENT_URL);
	if (element)
		element->text = url;

	tree_update_URL_node(node, url, data);
	tree_recalculate_node(node, false);

	return node;
}


/**
 * Creates an empty text node element and links it to a node.
 *
 * \param parent     the parent node
 * \param user_type  the required user_type
 * \return the newly created element.
 */
struct node_element *tree_create_node_element(struct node *parent, node_element_data data) {
	struct node_element *element;

	element = calloc(sizeof(struct node_element), 1);
	if (!element) return NULL;
	element->parent = parent;
	element->data = data;
	element->type = NODE_ELEMENT_TEXT;
	element->next = parent->data.next;
	parent->data.next = element;
	return element;
}


/**
 * Recalculates the size of a tree.
 *
 * \param tree  the tree to recalculate
 */
void tree_recalculate_size(struct tree *tree) {
	int width, height;

	assert(tree);

	if (!tree->handle)
		return;
	width = tree->width;
	height = tree->height;
	tree->width = tree_get_node_width(tree->root);
	tree->height = tree_get_node_height(tree->root);
	if ((width != tree->width) || (height != tree->height))
		tree_resized(tree);
}


/**
 * Returns the selected node, or NULL if multiple nodes are selected.
 *
 * \param node  the node to search sibling and children
 * \return the selected node, or NULL if multiple nodes are selected
 */
struct node *tree_get_selected_node(struct node *node) {
	struct node *result = NULL;
	struct node *temp;

	for (; node; node = node->next) {
		if (node->selected) {
			if (result)
				return NULL;
			result = node;
		}
		if ((node->child) && (node->expanded)) {
			temp = tree_get_selected_node(node->child);
			if (temp) {
				if (result)
					return NULL;
				else
					result = temp;
			}
		}
	}
	return result;
}
