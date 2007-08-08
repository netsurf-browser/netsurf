/*
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
 * Generic tree handling (interface).
 */

#ifndef _NETSURF_DESKTOP_TREE_H_
#define _NETSURF_DESKTOP_TREE_H_

#include <stdbool.h>
#include <stdint.h>

struct url_data;
struct cookie_data;

typedef enum {
	TREE_ELEMENT_URL,
	TREE_ELEMENT_ADDED,
	TREE_ELEMENT_LAST_VISIT,
	TREE_ELEMENT_VISITS,
	TREE_ELEMENT_VISITED,
	TREE_ELEMENT_THUMBNAIL,
	TREE_ELEMENT_TITLE,
	TREE_ELEMENT_NAME,
	TREE_ELEMENT_VALUE,
	TREE_ELEMENT_COMMENT,
	TREE_ELEMENT_DOMAIN,
	TREE_ELEMENT_PATH,
	TREE_ELEMENT_EXPIRES,
	TREE_ELEMENT_LAST_USED,
	TREE_ELEMENT_SECURE,
	TREE_ELEMENT_VERSION,
	TREE_ELEMENT_PERSISTENT,
	TREE_ELEMENT_SSL
} node_element_data;

#define NODE_INSTEP 40

struct node_sprite;
struct toolbar;

typedef enum {
  	NODE_ELEMENT_TEXT,		/* <-- Text only */
  	NODE_ELEMENT_TEXT_PLUS_SPRITE,	/* <-- Text and sprite */
  	NODE_ELEMENT_THUMBNAIL,		/* <-- Bitmap only */
} node_element_type;


struct node_element_box {
	int x;				/* <-- X offset from origin */
	int y;				/* <-- Y offset from origin */
	int width;			/* <-- Element width */
	int height;			/* <-- Element height */
};


struct node_element {
  	struct node *parent;		/* <-- Parent node */
  	node_element_type type;		/* <-- Element type */
	struct node_element_box box;	/* <-- Element bounding box */
	char *text;			/* <-- Text for the element */
	struct node_sprite *sprite;	/* <-- Sprite for the element */
	struct node_element *next;	/* <-- Next node element */
	node_element_data data;		/* <-- Data being represented */
};


struct node {
  	bool selected;			/* <-- Whether the node is selected */
  	bool expanded;			/* <-- Whether the node is expanded */
  	bool folder;			/* <-- Whether the node is a folder */
  	bool editable;			/* <-- Whether the node is editable */
	bool retain_in_memory;		/* <-- Whether the node remains in memory after deletion */
	bool deleted;			/* <-- Whether the node is currently deleted */
	bool processing;		/* <-- Internal flag used when moving */
	struct node_element_box box;	/* <-- Bounding box of all elements */
	struct node_element data;	/* <-- Data to display */
	struct node *parent;		/* <-- Parent entry (NULL for root) */
	struct node *child;		/* <-- First child */
	struct node *last_child;	/* <-- Last child */
  	struct node *previous;		/* <-- Previous child of the parent */
	struct node *next;		/* <-- Next child of the parent */

};

struct tree {
	unsigned int handle;		/* <-- User assigned handle */
	int offset_x;			/* <-- User assigned tree x offset */
	int offset_y;			/* <-- User assigned tree y offset */
	struct node *root;		/* <-- Tree root element */
	int width;			/* <-- Tree width */
	int height;			/* <-- Tree height */
	int window_width;		/* <-- Tree window width */
	int window_height;		/* <-- Tree window height */
	bool no_drag;			/* <-- Tree items can't be dragged out */
	bool no_vscroll;		/* <-- Tree has a vertical scroll only when needed */
	bool no_furniture;		/* <-- Tree does not have connecting lines */
	bool single_selection;		/* <-- There can only be one item selected */
	int edit_handle;		/* <-- Handle for editing information */
	uintptr_t textarea_handle;	/* <-- Handle for UTF-8 textarea */
	bool movable;			/* <-- Whether nodes can be moved */
	struct node_element *editing;	/* <-- Node element being edited */
	struct node *temp_selection;	/* <-- Temporarily selected node */
	struct toolbar *toolbar;	/* <-- Tree toolbar */
};


/* Non-platform specific code */
void tree_initialise(struct tree *tree);
void tree_initialise_nodes(struct tree *tree, struct node *root);
void tree_handle_node_changed(struct tree *tree, struct node *node,
		bool recalculate_sizes, bool expansion);
void tree_handle_node_element_changed(struct tree *tree,
		struct node_element *element);
void tree_recalculate_node(struct tree *tree, struct node *node, bool recalculate_sizes);
void tree_recalculate_node_positions(struct tree *tree, struct node *root);
struct node *tree_get_node_at(struct node *root, int x, int y, bool *furniture);
struct node_element *tree_get_node_element_at(struct node *node, int x, int y,
		bool *furniture);
struct node_element *tree_find_element(struct node *node, node_element_data data);
void tree_move_selected_nodes(struct tree *tree, struct node *destination,
		bool before);
bool tree_has_selection(struct node *node);
void tree_draw(struct tree *tree, int clip_x, int clip_y, int clip_width,
		int clip_height);
void tree_link_node(struct node *link, struct node *node, bool before);
void tree_delink_node(struct node *node);
struct node *tree_create_folder_node(struct node *parent, const char *title);
struct node *tree_create_leaf_node(struct node *parent, const char *title);
struct node *tree_create_URL_node(struct node *parent,
		const char *url, const struct url_data *data,
		const char *title);
struct node *tree_create_URL_node_shared(struct node *parent,
		const char *url, const struct url_data *data);
struct node *tree_create_cookie_node(struct node *parent,
		const struct cookie_data *data);
void tree_set_node_sprite(struct node *node, const char *sprite,
		const char *expanded);
void tree_set_node_expanded(struct tree *tree, struct node *node, bool expanded);
void tree_set_node_selected(struct tree *tree, struct node *node,
		bool selected);
void tree_handle_selection_area(struct tree *tree, int x, int y, int width,
		int height, bool invert);
void tree_delete_selected_nodes(struct tree *tree, struct node *node);
void tree_delete_node(struct tree *tree, struct node *node, bool siblings);
void tree_recalculate_size(struct tree *tree);
bool tree_handle_expansion(struct tree *tree, struct node *node, bool expanded,
		bool folder, bool leaf);
struct node *tree_get_selected_node(struct node *node);
struct node *tree_get_link_details(struct tree *tree, int x, int y,
		bool *before);


/* Platform specific code */
void tree_initialise_redraw(struct tree *tree);
void tree_redraw_area(struct tree *tree, int x, int y, int width, int height);
void tree_draw_line(int x, int y, int width, int height);
void tree_draw_node_element(struct tree *tree, struct node_element *element);
void tree_draw_node_expansion(struct tree *tree, struct node *node);
void tree_recalculate_node_element(struct node_element *element);
void tree_update_URL_node(struct node *node, const char *url,
		const struct url_data *data);
void tree_resized(struct tree *tree);
void tree_set_node_sprite_folder(struct node *node);

#endif
