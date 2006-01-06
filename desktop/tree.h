/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Generic tree handling (interface).
 */

#ifndef _NETSURF_DESKTOP_TREE_H_
#define _NETSURF_DESKTOP_TREE_H_

#include <stdbool.h>
#include "netsurf/content/url_store.h"

typedef enum {
	TREE_ELEMENT_URL,
	TREE_ELEMENT_ADDED,
	TREE_ELEMENT_LAST_VISIT,
	TREE_ELEMENT_VISITS,
	TREE_ELEMENT_VISITED,
	TREE_ELEMENT_THUMBNAIL,
	TREE_ELEMENT_TITLE
} node_element_data;

#define NODE_INSTEP 40

struct node_sprite;
struct toolbar;

typedef enum {
  	NODE_ELEMENT_TEXT,		/* <-- Text only */
  	NODE_ELEMENT_TEXT_PLUS_SPRITE,	/* <-- Text and sprite */
  	NODE_ELEMENT_SPRITE,		/* <-- Sprite only */
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
	int edit_handle;		/* <-- Handle for editing information */
	bool movable;			/* <-- Whether nodes can be moved */
	struct node_element *editing;	/* <-- Node element being edited */
	char edit_buffer[256];		/* <-- Editing buffer */
	struct node *temp_selection;	/* <-- Temporarily selected node */
	struct toolbar *toolbar;	/* <-- Tree toolbar */
};


/* Non-platform specific code */
void tree_initialise(struct tree *tree);
void tree_initialise_nodes(struct node *root);
void tree_handle_node_changed(struct tree *tree, struct node *node,
		bool recalculate_sizes, bool expansion);
void tree_handle_node_element_changed(struct tree *tree,
		struct node_element *element);
void tree_recalculate_node(struct node *node, bool recalculate_sizes);
void tree_recalculate_node_positions(struct node *root);
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
void tree_set_node_sprite(struct node *node, const char *sprite,
		const char *expanded);
struct node *tree_create_URL_node(struct node *parent, struct url_content *data,
		char *title);
struct node *tree_create_URL_node_shared(struct node *parent, struct url_content *data);
void tree_set_node_expanded(struct node *node, bool expanded);
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
void tree_update_URL_node(struct node *node, struct url_content *data);
void tree_resized(struct tree *tree);
void tree_set_node_sprite_folder(struct node *node);

#endif
