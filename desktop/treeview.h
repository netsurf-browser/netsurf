/*
 * Copyright 2012 - 2013 Michael Drake <tlsa@netsurf-browser.org>
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
 * Treeview handling (interface).
 */

#ifndef _NETSURF_DESKTOP_TREEVIEW_H_
#define _NETSURF_DESKTOP_TREEVIEW_H_

#include <stdbool.h>
#include <stdint.h>

#include "desktop/core_window.h"
#include "utils/types.h"

struct treeview;
struct treeview_node;

enum treeview_relationship {
	TREE_REL_FIRST_CHILD,
	TREE_REL_NEXT_SIBLING
};

enum treeview_msg {
	TREE_MSG_NODE_DELETE,
	TREE_MSG_NODE_EDIT,
	TREE_MSG_NODE_LAUNCH
};
struct treeview_node_msg {
	enum treeview_msg msg; /**< The message type */
	union {
		struct {
			lwc_string *feild; /* The field being edited */
			const char *text;  /* The proposed new value */
		} node_edit; /* Client may call treeview_update_node_* */
		struct {
			browser_mouse_state mouse; /* Button / modifier used */
		} node_launch;
	} data; /**< The message data. */
};

enum treeview_field_flags {
	TREE_FLAG_NONE		= 0,		/**< No flags set */
	TREE_FLAG_ALLOW_EDIT	= (1 << 0),	/**< Whether allow edit field */
	TREE_FLAG_DEFAULT	= (1 << 1),	/**< Whether field is default */
	TREE_FLAG_SHOW_NAME	= (1 << 2)	/**< Whether field name shown */

};
struct treeview_field_desc {
	lwc_string *field;
	enum treeview_field_flags flags;
};

struct treeview_field_data {
	lwc_string *field;
	const char *value;
	size_t value_len;
};


struct treeview_callback_table {
	nserror (*folder)(struct treeview_node_msg msg, void *data);
	nserror (*entry)(struct treeview_node_msg msg, void *data);
};

nserror treeview_init(void);
nserror treeview_fini(void);

nserror treeview_create(struct treeview **tree,
		const struct treeview_callback_table *callbacks,
		int n_fields, struct treeview_field_desc fields[],
		const struct core_window_callback_table *cw_t,
		struct core_window *cw);

nserror treeview_destroy(struct treeview *tree);

nserror treeview_create_node_folder(struct treeview *tree,
		struct treeview_node **folder,
		struct treeview_node *relation,
		enum treeview_relationship rel,
		const struct treeview_field_data *field,
		void *data);
nserror treeview_create_node_entry(struct treeview *tree,
		struct treeview_node **entry,
		struct treeview_node *relation, 
		enum treeview_relationship rel,
		const struct treeview_field_data fields[],
		void *data);

nserror treeview_update_node_entry(struct treeview *tree,
		struct treeview_node *entry,
		const struct treeview_field_data fields[],
		void *data);

nserror treeview_delete_node(struct treeview *tree, struct treeview_node *n);

nserror treeview_node_expand(struct treeview *tree,
		struct treeview_node *node);
nserror treeview_node_contract(struct treeview *tree,
		struct treeview_node *node);

void treeview_redraw(struct treeview *tree, int x, int y, struct rect *clip,
		const struct redraw_context *ctx);

/**
 * Handles all kinds of mouse action
 *
 * \param tree	Treeview
 * \param mouse	the mouse state at action moment
 * \param x	X coordinate
 * \param y	Y coordinate
 */
void treeview_mouse_action(struct treeview *tree,
		browser_mouse_state mouse, int x, int y);

struct treeview_node * treeview_get_root(struct treeview *tree);

bool treeview_has_selection(struct treeview *tree);

/**
 * Clear any selection in a treeview
 *
 * \param tree	treeview to clear selection in
 * \param rect	redraw rectangle (if redraw required)
 * \return true iff redraw required
 */
bool treeview_clear_selection(struct treeview *tree, struct rect *rect);

/**
 * Select all in a treeview
 *
 * \param tree	treeview to select all in
 * \param rect	redraw rectangle (if redraw required)
 * \return true iff redraw required
 */
bool treeview_select_all(struct treeview *tree, struct rect *rect);

#endif
