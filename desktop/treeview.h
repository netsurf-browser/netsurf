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
};					/**< Relationship between nodes */

enum treeview_msg {
	TREE_MSG_NODE_DELETE,		/**< Node to be deleted */
	TREE_MSG_NODE_EDIT,		/**< Node to be edited */
	TREE_MSG_NODE_LAUNCH		/**< Node to be launched */
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
	lwc_string *field;			/**< A treeview field name */
	enum treeview_field_flags flags;	/**< Flags for field */
}; /**< Treeview field description */

struct treeview_field_data {
	lwc_string *field;		/**< Field name */
	const char *value;		/**< Field value */
	size_t value_len;		/**< Field value length (bytes) */
};


struct treeview_callback_table {
	nserror (*folder)(struct treeview_node_msg msg, void *data);
	nserror (*entry)(struct treeview_node_msg msg, void *data);
}; /**< Client callbacks for events concerning nodes */

/**
 * Prepare treeview module for treeview usage
 *
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror treeview_init(void);

/**
 * Finalise the treeview module (all treeviews must have been destroyed first)
 *
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror treeview_fini(void);

/**
 * Create a treeview
 *
 * \param tree		Returns created treeview object
 * \param callbacks	Treeview client node event callbacks
 * \param n_fields	Number of treeview fields (see description)
 * \param fields	Array of treeview fields
 * \param cw_t		Callback table for core_window containing the treeview
 * \param cw		The core_window in which the treeview is shown
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * The fields array order is as follows (N = n_fields):
 *
 *    fields[0]			Main field for entries (shown when not expanded)
 *    fields[1]...fields[N-2]	Additional fields for entries
 *    fields[N-1]		Field for folder nodes
 *
 * So fields[0] and fields[N-1] have TREE_FLAG_DEFAULT set.
 */
nserror treeview_create(struct treeview **tree,
		const struct treeview_callback_table *callbacks,
		int n_fields, struct treeview_field_desc fields[],
		const struct core_window_callback_table *cw_t,
		struct core_window *cw);

/**
 * Destroy a treeview object
 *
 * \param tree		Treeview object to destroy
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * Will emit folder and entry deletion msg callbacks for all nodes in treeview.
 */
nserror treeview_destroy(struct treeview *tree);

/**
 * Create a folder node in given treeview
 *
 * \param tree		Treeview object in which to create folder
 * \param folder	Returns created folder node
 * \param relation	Existing node to insert as relation of, or NULL
 * \param rel		Folder's relationship to relation
 * \param field		Field data
 * \param data		Client data for node event callbacks
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * Field name must match name past in treeview_create fields[N-1].
 *
 * If relation is NULL, will insert as child of root node.
 */
nserror treeview_create_node_folder(struct treeview *tree,
		struct treeview_node **folder,
		struct treeview_node *relation,
		enum treeview_relationship rel,
		const struct treeview_field_data *field,
		void *data);

/**
 * Create an entry node in given treeview
 *
 * \param tree		Treeview object in which to create entry
 * \param entry		Returns created entry node
 * \param relation	Existing node to insert as relation of, or NULL
 * \param rel		Folder's relationship to relation
 * \param fields	Array of field data
 * \param data		Client data for node event callbacks
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * Fields array names must match names past in treeview_create fields[0...N-2].
 *
 * If relation is NULL, will insert as child of root node.
 */
nserror treeview_create_node_entry(struct treeview *tree,
		struct treeview_node **entry,
		struct treeview_node *relation, 
		enum treeview_relationship rel,
		const struct treeview_field_data fields[],
		void *data);

/**
 * Update an entry node in given treeview
 *
 * \param tree		Treeview object in which to create entry
 * \param entry		Entry node to update
 * \param fields	Array of new field data
 * \param data		Client data for node event callbacks
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * Fields array names must match names past in treeview_create fields[0...N-2].
 */
nserror treeview_update_node_entry(struct treeview *tree,
		struct treeview_node *entry,
		const struct treeview_field_data fields[],
		void *data);

/**
 * Delete a treeview node
 *
 * \param tree		Treeview object to delete node from
 * \param n		Node to delete
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * Will emit folder or entry deletion msg callback.
 */
nserror treeview_delete_node(struct treeview *tree, struct treeview_node *n);

/**
 * Expand a treeview node
 *
 * \param tree		Treeview object to expande node in
 * \param node		Node to expand
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror treeview_node_expand(struct treeview *tree,
		struct treeview_node *node);

/**
 * Contract a treeview node
 *
 * \param tree		Treeview object to contract node in
 * \param node		Node to contract
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror treeview_node_contract(struct treeview *tree,
		struct treeview_node *node);

/**
 * Redraw a treeview object
 *
 * \param tree		Treeview object to render
 * \param x		X coordinate to render treeview at
 * \param x		Y coordinate to render treeview at
 * \param clip		Current clip rectangle (wrt tree origin)
 * \param ctx		Current redraw context
 */
void treeview_redraw(struct treeview *tree, int x, int y, struct rect *clip,
		const struct redraw_context *ctx);

/**
 * Handles all kinds of mouse action
 *
 * \param tree		Treeview object
 * \param mouse		The current mouse state
 * \param x		X coordinate
 * \param y		Y coordinate
 */
void treeview_mouse_action(struct treeview *tree,
		browser_mouse_state mouse, int x, int y);

/**
 * Determine whether treeview has a selection
 *
 * \param tree		Treeview object to delete node from
 * \return true iff treeview has a selection
 */
bool treeview_has_selection(struct treeview *tree);

/**
 * Clear any selection in a treeview
 *
 * \param tree		Treeview object to clear selection in
 * \param rect		Redraw rectangle (if redraw required)
 * \return true iff redraw required
 */
bool treeview_clear_selection(struct treeview *tree, struct rect *rect);

/**
 * Select all in a treeview
 *
 * \param tree		Treeview object to select all in
 * \param rect		Redraw rectangle (if redraw required)
 * \return true iff redraw required
 */
bool treeview_select_all(struct treeview *tree, struct rect *rect);

#endif
