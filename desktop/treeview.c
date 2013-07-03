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
 * Treeview handling (implementation).
 */

#include "css/utils.h"
#include "desktop/gui.h"
#include "desktop/knockout.h"
#include "desktop/plotters.h"
#include "desktop/treeview.h"
#include "render/font.h"
#include "utils/log.h"

/* TODO: get rid of REDRAW_MAX -- need to be able to know window size */
#define REDRAW_MAX 8000

struct treeview_globals {
	int line_height;
	int furniture_width;
	int step_width;
	int window_padding;
	int icon_step;
} tree_g;

enum treeview_node_type {
	TREE_NODE_ROOT,
	TREE_NODE_FOLDER,
	TREE_NODE_ENTRY
};

enum treeview_node_section {
	TV_NODE_SECTION_TOGGLE,		/**< Expansion toggle */
	TV_NODE_SECTION_ON_NODE,	/**< Node content (text, icon) */
	TV_NODE_SECTION_NONE		/**< Empty area */
}; /**< Section type of a treeview at a point */

struct treeview_text {
	const char *data;	/**< Text string */
	uint32_t len;		/**< Lenfth of string in bytes */
	int width;		/**< Width of text in px */
};

struct treeview_field {
	enum treeview_field_flags flags;

	lwc_string *field;
	struct treeview_text value;
};

enum treeview_node_flags {
	TREE_NODE_NONE		= 0,		/**< No node flags set */
	TREE_NODE_EXPANDED	= (1 << 0),	/**< Whether node is expanded */
	TREE_NODE_SELECTED	= (1 << 1)	/**< Whether node is selected */

};

struct treeview_node {
	enum treeview_node_flags flags;	/**< Node flags */
	enum treeview_node_type type;	/**< Node type */

	int height;	/**< Includes height of any descendants (pixels) */
	int inset;	/**< Node's inset depending on tree depth (pixels) */

	struct treeview_node *parent;
	struct treeview_node *sibling_prev;
	struct treeview_node *sibling_next;
	struct treeview_node *children;

	void *client_data;  /**< Passed to client on node event msg callback */

	struct treeview_field text; /** Text to show for node (default field) */
}; /**< Treeview node */

struct treeview_node_entry {
	struct treeview_node base;
	struct treeview_field fields[];
}; /**< Entry class inherits node base class */

struct treeview_pos {
	int x;		/**< Mouse X coordinate */
	int y;		/**< Mouse Y coordinate */
	int node_y;	/**< Top of node at y */
	int node_h;	/**< Height of node at y */
}; /**< A mouse position wrt treeview */

struct treeview_drag {
	enum {
		TV_DRAG_NONE,
		TV_DRAG_SELECTION,
		TV_DRAG_MOVE,
		TV_DRAG_TEXTAREA
	} type;	/**< Drag type */
	struct treeview_node *start_node;	/**< Start node */
	bool selected;				/**< Start node is selected */
	enum treeview_node_section section;	/**< Node section at start */
	struct treeview_pos start;		/**< Start pos */
	struct treeview_pos prev;		/**< Previous pos */
}; /**< Drag state */

struct treeview {
	uint32_t view_width;		/** Viewport size */

	treeview_flags flags;		/** Treeview behaviour settings */

	struct treeview_node *root;	/**< Root node */

	struct treeview_field *fields;	/**< Array of fields */
	int n_fields; /**< fields[n_fields] is folder, lower are entry fields */
	int field_width; /**< Max width of shown field names */

	struct treeview_drag drag;			 /**< Drag state */

	const struct treeview_callback_table *callbacks; /**< For node events */

	const struct core_window_callback_table *cw_t; /**< Window cb table */
	struct core_window *cw_h; /**< Core window handle */
};


struct treeview_node_style {
	plot_style_t bg;		/**< Background */
	plot_font_style_t text;		/**< Text */
	plot_font_style_t itext;	/**< Entry field text */

	plot_style_t sbg;		/**< Selected background */
	plot_font_style_t stext;	/**< Selected text */
	plot_font_style_t sitext;	/**< Selected entry field text */
};

struct treeview_node_style plot_style_odd;	/**< Plot style for odd rows */
struct treeview_node_style plot_style_even;	/**< Plot style for even rows */

struct treeview_resource {
	const char *url;
	struct hlcache_handle *c;
	int height;
	bool ready;
}; /**< Treeview content resource data */
enum treeview_resource_id {
	TREE_RES_CONTENT = 0,
	TREE_RES_FOLDER,
	TREE_RES_SEARCH,
	TREE_RES_LAST
};
static struct treeview_resource treeview_res[TREE_RES_LAST] = {
	{ "resource:icons/content.png", NULL, 0, false },
	{ "resource:icons/directory.png", NULL, 0, false },
	{ "resource:icons/search.png", NULL, 0, false }
}; /**< Treeview content resources */



enum treeview_furniture_id {
	TREE_FURN_EXPAND = 0,
	TREE_FURN_CONTRACT,
	TREE_FURN_LAST
};
static struct treeview_text treeview_furn[TREE_FURN_LAST] = {
	{ "\xe2\x96\xb8", 3, 0 }, /* U+25B8: Right-pointing small triangle */
	{ "\xe2\x96\xbe", 3, 0 }  /* U+25BE: Down-pointing small triangle */
};


/**
 * Create treeview's root node
 *
 * \param root		Returns root node
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror treeview_create_node_root(struct treeview_node **root)
{
	struct treeview_node *n;

	n = malloc(sizeof(struct treeview_node));
	if (n == NULL) {
		return NSERROR_NOMEM;
	}

	n->flags = TREE_NODE_EXPANDED;
	n->type = TREE_NODE_ROOT;

	n->height = 0;
	n->inset = tree_g.window_padding - tree_g.step_width;

	n->text.flags = TREE_FLAG_NONE;
	n->text.field = NULL;
	n->text.value.data = NULL;
	n->text.value.len = 0;
	n->text.value.width = 0;

	n->parent = NULL;
	n->sibling_next = NULL;
	n->sibling_prev = NULL;
	n->children = NULL;

	n->client_data = NULL;

	*root = n;

	return NSERROR_OK;
}


/**
 * Insert a treeview node into a treeview
 *
 * \param a    parentless node to insert
 * \param b    tree node to insert a as a relation of
 * \param rel  a's relationship to b
 */
static inline void treeview_insert_node(struct treeview_node *a,
		struct treeview_node *b,
		enum treeview_relationship rel)
{
	assert(a != NULL);
	assert(a->parent == NULL);
	assert(b != NULL);

	switch (rel) {
	case TREE_REL_FIRST_CHILD:
		assert(b->type != TREE_NODE_ENTRY);
		a->parent = b;
		a->sibling_next = b->children;
		if (a->sibling_next)
			a->sibling_next->sibling_prev = a;
		b->children = a;
		break;

	case TREE_REL_NEXT_SIBLING:
		assert(b->type != TREE_NODE_ROOT);
		a->sibling_prev = b;
		a->sibling_next = b->sibling_next;
		a->parent = b->parent;
		b->sibling_next = a;
		if (a->sibling_next)
			a->sibling_next->sibling_prev = a;
		break;

	default:
		assert(0);
		break;
	}

	assert(a->parent != NULL);

	a->inset = a->parent->inset + tree_g.step_width;

	if (a->parent->flags & TREE_NODE_EXPANDED) {
		/* Parent is expanded, so inserted node will be visible and
		 * affect layout */
		b = a;
		do {
			b->parent->height += b->height;
			b = b->parent;
		} while (b->parent != NULL);

		if (a->text.value.width == 0) {
			nsfont.font_width(&plot_style_odd.text,
					a->text.value.data,
					a->text.value.len,
					&(a->text.value.width));
		}
	}
}


/* Exported interface, documented in treeview.h */
nserror treeview_create_node_folder(struct treeview *tree,
		struct treeview_node **folder,
		struct treeview_node *relation,
		enum treeview_relationship rel,
		const struct treeview_field_data *field,
		void *data, treeview_node_create_flags flags)
{
	struct treeview_node *n;

	assert(data != NULL);
	assert(tree != NULL);
	assert(tree->root != NULL);

	if (relation == NULL) {
		relation = tree->root;
		rel = TREE_REL_FIRST_CHILD;
	}

	n = malloc(sizeof(struct treeview_node));
	if (n == NULL) {
		return NSERROR_NOMEM;
	}

	n->flags = TREE_NODE_NONE;
	n->type = TREE_NODE_FOLDER;

	n->height = tree_g.line_height;

	n->text.value.data = field->value;
	n->text.value.len = field->value_len;
	n->text.value.width = 0;

	n->parent = NULL;
	n->sibling_next = NULL;
	n->sibling_prev = NULL;
	n->children = NULL;

	n->client_data = data;

	treeview_insert_node(n, relation, rel);

	*folder = n;

	/* Inform front end of change in dimensions */
	if (!(flags & TREE_CREATE_SUPPRESS_RESIZE))
		tree->cw_t->update_size(tree->cw_h, -1, tree->root->height);

	return NSERROR_OK;
}


/* Exported interface, documented in treeview.h */
nserror treeview_update_node_entry(struct treeview *tree,
		struct treeview_node *entry,
		const struct treeview_field_data fields[],
		void *data)
{
	bool match;
	struct treeview_node_entry *e = (struct treeview_node_entry *)entry;
	int i;

	assert(data != NULL);
	assert(tree != NULL);
	assert(entry != NULL);
	assert(data == entry->client_data);
	assert(entry->parent != NULL);

	assert(fields != NULL);
	assert(fields[0].field != NULL);
	assert(lwc_string_isequal(tree->fields[0].field,
			fields[0].field, &match) == lwc_error_ok &&
			match == true);
	entry->text.value.data = fields[0].value;
	entry->text.value.len = fields[0].value_len;
	entry->text.value.width = 0;

	if (entry->parent->flags & TREE_NODE_EXPANDED) {
		/* Text will be seen, get its width */
		nsfont.font_width(&plot_style_odd.text,
				entry->text.value.data,
				entry->text.value.len,
				&(entry->text.value.width));
	} else {
		/* Just invalidate the width, since it's not needed now */
		entry->text.value.width = 0;
	}

	for (i = 1; i < tree->n_fields; i++) {
		assert(fields[i].field != NULL);
		assert(lwc_string_isequal(tree->fields[i].field,
				fields[i].field, &match) == lwc_error_ok &&
				match == true);

		e->fields[i - 1].value.data = fields[i].value;
		e->fields[i - 1].value.len = fields[i].value_len;

		if (entry->flags & TREE_NODE_EXPANDED) {
			/* Text will be seen, get its width */
			nsfont.font_width(&plot_style_odd.text,
					e->fields[i - 1].value.data,
					e->fields[i - 1].value.len,
					&(e->fields[i - 1].value.width));
		} else {
			/* Invalidate the width, since it's not needed yet */
			e->fields[i - 1].value.width = 0;
		}
	}

	return NSERROR_OK;
}


/* Exported interface, documented in treeview.h */
nserror treeview_create_node_entry(struct treeview *tree,
		struct treeview_node **entry,
		struct treeview_node *relation,
		enum treeview_relationship rel,
		const struct treeview_field_data fields[],
		void *data, treeview_node_create_flags flags)
{
	bool match;
	struct treeview_node_entry *e;
	struct treeview_node *n;
	int i;

	assert(data != NULL);
	assert(tree != NULL);
	assert(tree->root != NULL);

	if (relation == NULL) {
		relation = tree->root;
		rel = TREE_REL_FIRST_CHILD;
	}

	e = malloc(sizeof(struct treeview_node_entry) +
			(tree->n_fields - 1) * sizeof(struct treeview_field));
	if (e == NULL) {
		return NSERROR_NOMEM;
	}


	n = (struct treeview_node *) e;

	n->flags = TREE_NODE_NONE;
	n->type = TREE_NODE_ENTRY;

	n->height = tree_g.line_height;

	assert(fields != NULL);
	assert(fields[0].field != NULL);
	assert(lwc_string_isequal(tree->fields[0].field,
			fields[0].field, &match) == lwc_error_ok &&
			match == true);
	n->text.value.data = fields[0].value;
	n->text.value.len = fields[0].value_len;
	n->text.value.width = 0;

	n->parent = NULL;
	n->sibling_next = NULL;
	n->sibling_prev = NULL;
	n->children = NULL;

	n->client_data = data;

	for (i = 1; i < tree->n_fields; i++) {
		assert(fields[i].field != NULL);
		assert(lwc_string_isequal(tree->fields[i].field,
				fields[i].field, &match) == lwc_error_ok &&
				match == true);

		e->fields[i - 1].value.data = fields[i].value;
		e->fields[i - 1].value.len = fields[i].value_len;
		e->fields[i - 1].value.width = 0;
	}

	treeview_insert_node(n, relation, rel);

	*entry = n;

	/* Inform front end of change in dimensions */
	if (!(flags & TREE_CREATE_SUPPRESS_RESIZE))
		tree->cw_t->update_size(tree->cw_h, -1, tree->root->height);

	return NSERROR_OK;
}


/**
 * Delete a treeview node
 *
 * \param tree		Treeview object to delete node from
 * \param n		Node to delete
 * \param interaction	Delete is result of user interaction with treeview
 * \return NSERROR_OK on success, appropriate error otherwise
 *
 * Will emit folder or entry deletion msg callback.
 */
static nserror treeview_delete_node_internal(struct treeview *tree,
		struct treeview_node *n, bool interaction)
{
	struct treeview_node_msg msg;
	msg.msg = TREE_MSG_NODE_DELETE;
	struct treeview_node *p;
	static int nest_depth = 0;

	if (interaction && (tree->flags & TREEVIEW_NO_DELETES)) {
		return NSERROR_OK;
	}

	/* Destroy children first */
	nest_depth++;
	while (n->children != NULL) {
		treeview_delete_node_internal(tree, n->children, interaction);
	}
	nest_depth--;

	/* Unlink node from tree */
	if (n->parent != NULL && n->parent->children == n) {
		/* Node is a first child */
		n->parent->children = n->sibling_next;

	} else if (n->sibling_prev != NULL) {
		/* Node is not first child */
		n->sibling_prev->sibling_next = n->sibling_next;
	}

	if (n->sibling_next != NULL) {
		/* Always need to do this */
		n->sibling_next->sibling_prev = n->sibling_prev;
	}

	/* Reduce ancestor heights */
	p = n->parent;
	while (p != NULL && p->flags & TREE_NODE_EXPANDED) {
		p->height -= n->height;
		p = p->parent;
	}

	/* Handle any special treatment */
	switch (n->type) {
	case TREE_NODE_ENTRY:
		tree->callbacks->entry(msg, n->client_data);
		break;
	case TREE_NODE_FOLDER:
		tree->callbacks->folder(msg, n->client_data);
		break;
	case TREE_NODE_ROOT:
		break;
	default:
		return NSERROR_BAD_PARAMETER;
	}

	if (nest_depth == 0) {
		/* This is the node we were originally asked to delete */

		if (tree->flags & TREEVIEW_DEL_EMPTY_DIRS &&
				n->parent != NULL &&
				n->parent->type != TREE_NODE_ROOT &&
				n->parent->children == NULL) {
			/* Delete empty parent */
			nest_depth++;
			treeview_delete_node_internal(tree, n->parent,
					interaction);
			nest_depth--;
		}

		/* Inform front end of change in dimensions */
		tree->cw_t->update_size(tree->cw_h, -1, tree->root->height);
	}

	/* Free the node */
	free(n);

	return NSERROR_OK;
}


/* Exported interface, documented in treeview.h */
nserror treeview_delete_node(struct treeview *tree, struct treeview_node *n)
{
	return treeview_delete_node_internal(tree, n, false);
}


/* Exported interface, documented in treeview.h */
nserror treeview_create(struct treeview **tree,
		const struct treeview_callback_table *callbacks,
		int n_fields, struct treeview_field_desc fields[],
		const struct core_window_callback_table *cw_t,
		struct core_window *cw, treeview_flags flags)
{
	nserror error;
	int i;

	assert(cw_t != NULL);
	assert(cw != NULL);
	assert(callbacks != NULL);

	assert(fields != NULL);
	assert(fields[0].flags & TREE_FLAG_DEFAULT);
	assert(fields[n_fields - 1].flags & TREE_FLAG_DEFAULT);
	assert(n_fields >= 2);

	*tree = malloc(sizeof(struct treeview));
	if (*tree == NULL) {
		return NSERROR_NOMEM;
	}

	(*tree)->fields = malloc(sizeof(struct treeview_field) * n_fields);
	if ((*tree)->fields == NULL) {
		free(tree);
		return NSERROR_NOMEM;
	}

	error = treeview_create_node_root(&((*tree)->root));
	if (error != NSERROR_OK) {
		free((*tree)->fields);
		free(*tree);
		return error;
	}

	(*tree)->field_width = 0;
	for (i = 0; i < n_fields; i++) {
		struct treeview_field *f = &((*tree)->fields[i]);

		f->flags = fields[i].flags;
		f->field = lwc_string_ref(fields[i].field);
		f->value.data = lwc_string_data(fields[i].field);
		f->value.len = lwc_string_length(fields[i].field);

		nsfont.font_width(&plot_style_odd.text, f->value.data,
				f->value.len, &(f->value.width));

		if (f->flags & TREE_FLAG_SHOW_NAME)
			if ((*tree)->field_width < f->value.width)
				(*tree)->field_width = f->value.width;
	}

	(*tree)->field_width += tree_g.step_width;

	(*tree)->callbacks = callbacks;
	(*tree)->n_fields = n_fields - 1;

	(*tree)->drag.type = TV_DRAG_NONE;
	(*tree)->drag.start_node = NULL;
	(*tree)->drag.start.x = 0;
	(*tree)->drag.start.y = 0;
	(*tree)->drag.start.node_y = 0;
	(*tree)->drag.start.node_h = 0;
	(*tree)->drag.prev.x = 0;
	(*tree)->drag.prev.y = 0;
	(*tree)->drag.prev.node_y = 0;
	(*tree)->drag.prev.node_h = 0;

	(*tree)->flags = flags;

	(*tree)->cw_t = cw_t;
	(*tree)->cw_h = cw;

	return NSERROR_OK;
}


/* Exported interface, documented in treeview.h */
nserror treeview_destroy(struct treeview *tree)
{
	int f;

	assert(tree != NULL);

	/* Destroy nodes */
	treeview_delete_node_internal(tree, tree->root, false);

	/* Destroy feilds */
	for (f = 0; f <= tree->n_fields; f++) {
		lwc_string_unref(tree->fields[f].field);
	}
	free(tree->fields);

	/* Free treeview */
	free(tree);

	return NSERROR_OK;
}


/* Walk a treeview subtree, calling a callback at each node (depth first)
 *
 * \param root		Root to walk tree from (doesn't get a callback call)
 * \param full		Iff true, visit children of collapsed nodes
 * \param callback_bwd	Function to call on each node in backwards order
 * \param callback_fwd	Function to call on each node in forwards order
 * \param ctx		Context to pass to callback
 * \return true iff callback caused premature abort
 */
static bool treeview_walk_internal(struct treeview_node *root, bool full,
		bool (*callback_bwd)(struct treeview_node *node, void *ctx),
		bool (*callback_fwd)(struct treeview_node *node, void *ctx),
		void *ctx)
{
	struct treeview_node *node, *next;

	node = root;

	while (node != NULL) {
		next = (full || (node->flags & TREE_NODE_EXPANDED)) ?
				node->children : NULL;

		if (next != NULL) {
			/* Down to children */
			node = next;
		} else {
			/* No children.  As long as we're not at the root,
			 * go to next sibling if present, or nearest ancestor
			 * with a next sibling. */

			while (node != root &&
					node->sibling_next == NULL) {
				if (callback_bwd != NULL &&
						callback_bwd(node, ctx)) {
					/* callback caused early termination */
					return true;
				}
				node = node->parent;
			}

			if (node == root)
				break;

			if (callback_bwd != NULL && callback_bwd(node, ctx)) {
				/* callback caused early termination */
				return true;
			}
			node = node->sibling_next;
		}

		assert(node != NULL);
		assert(node != root);

		if (callback_fwd != NULL && callback_fwd(node, ctx)) {
			/* callback caused early termination */
			return true;
		}

	}
	return false;
}


/* Exported interface, documented in treeview.h */
nserror treeview_node_expand(struct treeview *tree,
		struct treeview_node *node)
{
	struct treeview_node *child;
	struct treeview_node_entry *e;
	int additional_height = 0;
	int i;

	assert(tree != NULL);
	assert(node != NULL);

	if (node->flags & TREE_NODE_EXPANDED) {
		/* What madness is this? */
		LOG(("Tried to expand an expanded node."));
		return NSERROR_OK;
	}

	switch (node->type) {
	case TREE_NODE_FOLDER:
		child = node->children;
		if (child == NULL) {
			/* Allow expansion of empty folders */
			break;
		}

		do {
			assert((child->flags & TREE_NODE_EXPANDED) == false);
			if (child->text.value.width == 0) {
				nsfont.font_width(&plot_style_odd.text,
						child->text.value.data,
						child->text.value.len,
						&(child->text.value.width));
			}

			additional_height += child->height;

			child = child->sibling_next;
		} while (child != NULL);

		break;

	case TREE_NODE_ENTRY:
		assert(node->children == NULL);

		e = (struct treeview_node_entry *)node;

		for (i = 0; i < tree->n_fields - 1; i++) {

			if (e->fields[i].value.width == 0) {
				nsfont.font_width(&plot_style_odd.text,
						e->fields[i].value.data,
						e->fields[i].value.len,
						&(e->fields[i].value.width));
			}

			/* Add height for field */
			additional_height += tree_g.line_height;
		}

		break;

	case TREE_NODE_ROOT:
		assert(node->type != TREE_NODE_ROOT);
		break;
	}

	/* Update the node */
	node->flags |= TREE_NODE_EXPANDED;

	/* And parent's heights */
	do {
		node->height += additional_height;
		node = node->parent;
	} while (node->parent != NULL);

	node->height += additional_height;

	/* Inform front end of change in dimensions */
	if (additional_height != 0)
		tree->cw_t->update_size(tree->cw_h, -1, tree->root->height);

	return NSERROR_OK;
}


/** Treewalk node callback for handling node contraction. */
static bool treeview_node_contract_cb(struct treeview_node *node, void *ctx)
{
	int height_reduction;

	assert(node != NULL);
	assert(node->type != TREE_NODE_ROOT);

	if ((node->flags & TREE_NODE_EXPANDED) == false) {
		/* Nothing to do. */
		return false;
	}

	node->flags ^= TREE_NODE_EXPANDED;
	height_reduction = node->height - tree_g.line_height;

	assert(height_reduction >= 0);

	do {
		node->height -= height_reduction;
		node = node->parent;
	} while (node != NULL);

	return false; /* Don't want to abort tree walk */
}
/* Exported interface, documented in treeview.h */
nserror treeview_node_contract(struct treeview *tree,
		struct treeview_node *node)
{
	assert(node != NULL);

	if ((node->flags & TREE_NODE_EXPANDED) == false) {
		/* What madness is this? */
		LOG(("Tried to contract a contracted node."));
		return NSERROR_OK;
	}

	/* Contract children. */
	treeview_walk_internal(node, false, NULL,
			treeview_node_contract_cb, NULL);

	/* Contract node */
	treeview_node_contract_cb(node, NULL);

	/* Inform front end of change in dimensions */
	tree->cw_t->update_size(tree->cw_h, -1, tree->root->height);

	return NSERROR_OK;
}


/* Exported interface, documented in treeview.h */
void treeview_redraw(struct treeview *tree, int x, int y, struct rect *clip,
		const struct redraw_context *ctx)
{
	struct redraw_context new_ctx = *ctx;
	struct treeview_node *node, *root, *next;
	struct treeview_node_entry *entry;
	struct treeview_node_style *style = &plot_style_odd;
	struct content_redraw_data data;
	struct rect r;
	uint32_t count = 0;
	int render_y = y;
	int inset;
	int x0, y0, y1;
	int baseline = (tree_g.line_height * 3 + 2) / 4;
	enum treeview_resource_id res = TREE_RES_CONTENT;
	plot_style_t *bg_style;
	plot_font_style_t *text_style;
	plot_font_style_t *infotext_style;
	int height;
	int sel_min, sel_max;
	bool invert_selection;

	assert(tree != NULL);
	assert(tree->root != NULL);
	assert(tree->root->flags & TREE_NODE_EXPANDED);

	if (tree->drag.start.y > tree->drag.prev.y) {
		sel_min = tree->drag.prev.y;
		sel_max = tree->drag.start.y;
	} else {
		sel_min = tree->drag.start.y;
		sel_max = tree->drag.prev.y;
	}

	/* Start knockout rendering if it's available for this plotter */
	if (ctx->plot->option_knockout)
		knockout_plot_start(ctx, &new_ctx);

	/* Set up clip rectangle */
	r.x0 = clip->x0 + x;
	r.y0 = clip->y0 + y;
	r.x1 = clip->x1 + x;
	r.y1 = clip->y1 + y;
	new_ctx.plot->clip(&r);

	/* Draw the tree */
	node = root = tree->root;

	/* Setup common content redraw data */
	data.width = 17;
	data.height = 17;
	data.scale = 1;
	data.repeat_x = false;
	data.repeat_y = false;

	while (node != NULL) {
		int i;
		next = (node->flags & TREE_NODE_EXPANDED) ?
				node->children : NULL;

		if (next != NULL) {
			/* down to children */
			node = next;
		} else {
			/* No children.  As long as we're not at the root,
			 * go to next sibling if present, or nearest ancestor
			 * with a next sibling. */

			while (node != root &&
					node->sibling_next == NULL) {
				node = node->parent;
			}

			if (node == root)
				break;

			node = node->sibling_next;
		}

		assert(node != NULL);
		assert(node != root);
		assert(node->type == TREE_NODE_FOLDER ||
				node->type == TREE_NODE_ENTRY);

		count++;
		inset = node->inset;
		height = (node->type == TREE_NODE_ENTRY) ? node->height :
				tree_g.line_height;

		if ((render_y + height) < r.y0) {
			/* This node's line is above clip region */
			render_y += height;
			continue;
		}

		style = (count & 0x1) ? &plot_style_odd : &plot_style_even;
		if (tree->drag.type == TV_DRAG_SELECTION &&
				(render_y + height > sel_min &&
				render_y < sel_max)) {
			invert_selection = true;
		} else {
			invert_selection = false;
		}
		if ((node->flags & TREE_NODE_SELECTED && !invert_selection) ||
				(!(node->flags & TREE_NODE_SELECTED) &&
				invert_selection)) {
			bg_style = &style->sbg;
			text_style = &style->stext;
			infotext_style = &style->sitext;
		} else {
			bg_style = &style->bg;
			text_style = &style->text;
			infotext_style = &style->itext;
		}

		/* Render background */
		y0 = render_y;
		y1 = render_y + height;
		new_ctx.plot->rectangle(r.x0, y0, r.x1, y1, bg_style);

		/* Render toggle */
		if (node->flags & TREE_NODE_EXPANDED) {
			new_ctx.plot->text(inset, render_y + baseline,
					treeview_furn[TREE_FURN_CONTRACT].data,
					treeview_furn[TREE_FURN_CONTRACT].len,
					text_style);
		} else {
			new_ctx.plot->text(inset, render_y + baseline,
					treeview_furn[TREE_FURN_EXPAND].data,
					treeview_furn[TREE_FURN_EXPAND].len,
					text_style);
		}

		/* Render icon */
		if (node->type == TREE_NODE_ENTRY)
			res = TREE_RES_CONTENT;
		else if (node->type == TREE_NODE_FOLDER)
			res = TREE_RES_FOLDER;

		if (treeview_res[res].ready) {
			/* Icon resource is available */
			data.x = inset + tree_g.step_width;
			data.y = render_y + ((tree_g.line_height -
					treeview_res[res].height + 1) / 2);
			data.background_colour = bg_style->fill_colour;

			content_redraw(treeview_res[res].c,
					&data, &r, &new_ctx);
		}

		/* Render text */
		x0 = inset + tree_g.step_width + tree_g.icon_step;
		new_ctx.plot->text(x0, render_y + baseline,
				node->text.value.data, node->text.value.len,
				text_style);

		/* Rendered the node */
		render_y += tree_g.line_height;
		if (render_y > r.y1) {
			/* Passed the bottom of what's in the clip region.
			 * Done. */
			break;
		}


		if (node->type != TREE_NODE_ENTRY ||
				!(node->flags & TREE_NODE_EXPANDED))
			/* Done everything for this node */
			continue;

		/* Render expanded entry fields */
		entry = (struct treeview_node_entry *)node;
		for (i = 0; i < tree->n_fields - 1; i++) {
			struct treeview_field *ef = &(tree->fields[i + 1]);

			if (ef->flags & TREE_FLAG_SHOW_NAME) {
				int max_width = tree->field_width;

				new_ctx.plot->text(x0 + max_width -
							ef->value.width -
							tree_g.step_width,
						render_y + baseline,
						ef->value.data,
						ef->value.len,
						infotext_style);

				new_ctx.plot->text(x0 + max_width,
						render_y + baseline,
						entry->fields[i].value.data,
						entry->fields[i].value.len,
						infotext_style);
			} else {
				new_ctx.plot->text(x0, render_y + baseline,
						entry->fields[i].value.data,
						entry->fields[i].value.len,
						infotext_style);

			}

			/* Rendered the expanded entry field */
			render_y += tree_g.line_height;
		}

		/* Finshed rendering expanded entry */

		if (render_y > r.y1) {
			/* Passed the bottom of what's in the clip region.
			 * Done. */
			break;
		}
	}

	if (render_y < r.y1) {
		/* Fill the blank area at the bottom */
		y0 = render_y;
		new_ctx.plot->rectangle(r.x0, y0, r.x1, r.y1,
				&plot_style_even.bg);
		
	}

	/* Rendering complete */
	if (ctx->plot->option_knockout)
		knockout_plot_end();
}

struct treeview_selection_walk_data {
	enum {
		TREEVIEW_WALK_HAS_SELECTION,
		TREEVIEW_WALK_CLEAR_SELECTION,
		TREEVIEW_WALK_SELECT_ALL,
		TREEVIEW_WALK_COMMIT_SELECT_DRAG,
		TREEVIEW_WALK_DELETE_SELECTION
	} purpose;
	union {
		bool has_selection;
		struct {
			bool required;
			struct rect *rect;
		} redraw;
		struct {
			int sel_min;
			int sel_max;
		} drag;
	} data;
	int current_y;
	struct treeview *tree;
};
/** Treewalk node callback for handling selection related actions. */
static bool treeview_node_selection_walk_cb(struct treeview_node *node,
		void *ctx)
{
	struct treeview_selection_walk_data *sw = ctx;
	int height;
	bool changed = false;

	height = (node->type == TREE_NODE_ENTRY) ? node->height :
			tree_g.line_height;
	sw->current_y += height;

	switch (sw->purpose) {
	case TREEVIEW_WALK_HAS_SELECTION:
		if (node->flags & TREE_NODE_SELECTED) {
			sw->data.has_selection = true;
			return true; /* Can abort tree walk */
		}
		break;

	case TREEVIEW_WALK_DELETE_SELECTION:
		if (node->flags & TREE_NODE_SELECTED) {
			treeview_delete_node_internal(sw->tree, node, true);
			changed = true;
		}
		break;

	case TREEVIEW_WALK_CLEAR_SELECTION:
		if (node->flags & TREE_NODE_SELECTED) {
			node->flags ^= TREE_NODE_SELECTED;
			changed = true;
		}
		break;

	case TREEVIEW_WALK_SELECT_ALL:
		if (!(node->flags & TREE_NODE_SELECTED)) {
			node->flags ^= TREE_NODE_SELECTED;
			changed = true;
		}
		break;

	case TREEVIEW_WALK_COMMIT_SELECT_DRAG:
		if (sw->current_y > sw->data.drag.sel_min &&
				sw->current_y - height <
						sw->data.drag.sel_max) {
			node->flags ^= TREE_NODE_SELECTED;
		}
		return false; /* Don't stop walk */
	}

	if (changed) {
		if (sw->data.redraw.required == false) {
			sw->data.redraw.required = true;
			sw->data.redraw.rect->y0 = sw->current_y - height;
		}

		if (sw->current_y > sw->data.redraw.rect->y1) {
			sw->data.redraw.rect->y1 = sw->current_y;
		}
	}

	return false; /* Don't stop walk */
}


/* Exported interface, documented in treeview.h */
bool treeview_has_selection(struct treeview *tree)
{
	struct treeview_selection_walk_data sw;

	sw.purpose = TREEVIEW_WALK_HAS_SELECTION;
	sw.data.has_selection = false;

	treeview_walk_internal(tree->root, false, NULL,
			treeview_node_selection_walk_cb, &sw);

	return sw.data.has_selection;
}


/* Exported interface, documented in treeview.h */
bool treeview_clear_selection(struct treeview *tree, struct rect *rect)
{
	struct treeview_selection_walk_data sw;

	rect->x0 = 0;
	rect->y0 = 0;
	rect->x1 = REDRAW_MAX;
	rect->y1 = 0;

	sw.purpose = TREEVIEW_WALK_CLEAR_SELECTION;
	sw.data.redraw.required = false;
	sw.data.redraw.rect = rect;
	sw.current_y = 0;

	treeview_walk_internal(tree->root, false, NULL,
			treeview_node_selection_walk_cb, &sw);

	return sw.data.redraw.required;
}


/* Exported interface, documented in treeview.h */
bool treeview_select_all(struct treeview *tree, struct rect *rect)
{
	struct treeview_selection_walk_data sw;

	rect->x0 = 0;
	rect->y0 = 0;
	rect->x1 = REDRAW_MAX;
	rect->y1 = 0;

	sw.purpose = TREEVIEW_WALK_SELECT_ALL;
	sw.data.redraw.required = false;
	sw.data.redraw.rect = rect;
	sw.current_y = 0;

	treeview_walk_internal(tree->root, false, NULL,
			treeview_node_selection_walk_cb, &sw);

	return sw.data.redraw.required;
}


/**
 * Commit a current selection drag, modifying the node's selection state.
 */
static void treeview_commit_selection_drag(struct treeview *tree)
{
	struct treeview_selection_walk_data sw;

	sw.purpose = TREEVIEW_WALK_COMMIT_SELECT_DRAG;
	sw.current_y = 0;

	if (tree->drag.start.y > tree->drag.prev.y) {
		sw.data.drag.sel_min = tree->drag.prev.y;
		sw.data.drag.sel_max = tree->drag.start.y;
	} else {
		sw.data.drag.sel_min = tree->drag.start.y;
		sw.data.drag.sel_max = tree->drag.prev.y;
	}

	treeview_walk_internal(tree->root, false, NULL,
			treeview_node_selection_walk_cb, &sw);
}


/**
 * Commit a current selection drag, modifying the node's selection state.
 */
static bool treeview_delete_selection(struct treeview *tree, struct rect *rect)
{
	struct treeview_selection_walk_data sw;

	assert(tree != NULL);
	assert(tree->root != NULL);

	rect->x0 = 0;
	rect->y0 = 0;
	rect->x1 = REDRAW_MAX;
	rect->y1 = tree->root->height;

	sw.purpose = TREEVIEW_WALK_DELETE_SELECTION;
	sw.data.redraw.required = false;
	sw.data.redraw.rect = rect;
	sw.current_y = 0;
	sw.tree = tree;

	treeview_walk_internal(tree->root, false, NULL,
			treeview_node_selection_walk_cb, &sw);

	return sw.data.redraw.required;
}


/* Exported interface, documented in treeview.h */
bool treeview_keypress(struct treeview *tree, uint32_t key)
{
	struct rect r;	/**< Redraw rectangle */
	bool redraw = false;

	assert(tree != NULL);

	switch (key) {
		case KEY_SELECT_ALL:
			redraw = treeview_select_all(tree, &r);
			break;
		case KEY_COPY_SELECTION:
			/* TODO: Copy selection as text */
			break;
		case KEY_DELETE_LEFT:
		case KEY_DELETE_RIGHT:
			redraw = treeview_delete_selection(tree, &r);
			break;
		case KEY_CR:
		case KEY_NL:
			/* TODO: Launch selection */
			break;
		case KEY_ESCAPE:
		case KEY_CLEAR_SELECTION:
			redraw = treeview_clear_selection(tree, &r);
			break;
		/* TODO: Trivial keyboard navigation */
		case KEY_LEFT:
			break;
		case KEY_RIGHT:
			break;
		case KEY_UP:
			break;
		case KEY_DOWN:
			break;
		default:
			return false;
	}

	if (redraw) {
		tree->cw_t->redraw_request(tree->cw_h, r);
	}

	return true;
}

struct treeview_mouse_action {
	struct treeview *tree;
	browser_mouse_state mouse;
	int x;
	int y;
	int current_y;	/* Y coordinate value of top of current node */
};
/** Treewalk node callback for handling mouse action. */
static bool treeview_node_mouse_action_cb(struct treeview_node *node, void *ctx)
{
	struct treeview_mouse_action *ma = ctx;
	struct rect r;
	bool redraw = false;
	bool click;
	int height;
	enum {
		TV_NODE_ACTION_NONE		= 0,
		TV_NODE_ACTION_SELECTION	= (1 << 0)
	} action = TV_NODE_ACTION_NONE;
	enum treeview_node_section section = TV_NODE_SECTION_NONE;
	nserror err;

	r.x0 = 0;
	r.x1 = REDRAW_MAX;

	height = (node->type == TREE_NODE_ENTRY) ? node->height :
			tree_g.line_height;

	/* Skip line if we've not reached mouse y */
	if (ma->y > ma->current_y + height) {
		ma->current_y += height;
		return false; /* Don't want to abort tree walk */
	}

	/* Find where the mouse is */
	if (ma->y <= ma->current_y + tree_g.line_height) {
		if (ma->x >= node->inset - 1 &&
				ma->x < node->inset + tree_g.step_width) {
			/* Over expansion toggle */
			section = TV_NODE_SECTION_TOGGLE;

		} else if (ma->x >= node->inset + tree_g.step_width &&
				ma->x < node->inset + tree_g.step_width +
				tree_g.icon_step + node->text.value.width) {
			/* On node */
			section = TV_NODE_SECTION_ON_NODE;
		}
	} else if (node->type == TREE_NODE_ENTRY &&
			height > tree_g.line_height) {
		/* Expanded entries */
		int x = node->inset + tree_g.step_width + tree_g.icon_step;
		int y = ma->current_y + tree_g.line_height;
		int i;
		struct treeview_node_entry *entry =
				(struct treeview_node_entry *)node;
		for (i = 0; i < ma->tree->n_fields - 1; i++) {
			struct treeview_field *ef = &(ma->tree->fields[i + 1]);

			if (ma->y > y + tree_g.line_height) {
				y += tree_g.line_height;
				continue;
			}

			if (ef->flags & TREE_FLAG_SHOW_NAME) {
				int max_width = ma->tree->field_width;

				if (ma->x >= x + max_width - ef->value.width -
						tree_g.step_width &&
						ma->x < x + max_width -
						tree_g.step_width) {
					/* On a field name */
					section = TV_NODE_SECTION_ON_NODE;

				} else if (ma->x >= x + max_width &&
						ma->x < x + max_width +
						entry->fields[i].value.width) {
					/* On a field value */
					section = TV_NODE_SECTION_ON_NODE;
				}
			} else {
				if (ma->x >= x && ma->x < x +
						entry->fields[i].value.width) {
					/* On a field value */
					section = TV_NODE_SECTION_ON_NODE;
				}
			}

			break;
		}
	}

	/* Record what position / section a drag started on */
	if (ma->mouse & (BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_PRESS_2) &&
			ma->tree->drag.type == TV_DRAG_NONE) {
		ma->tree->drag.selected = node->flags & TREE_NODE_SELECTED;
		ma->tree->drag.start_node = node;
		ma->tree->drag.section = section;
		ma->tree->drag.start.x = ma->x;
		ma->tree->drag.start.y = ma->y;
		ma->tree->drag.start.node_y = ma->current_y;
		ma->tree->drag.start.node_h = height;

		ma->tree->drag.prev.x = ma->x;
		ma->tree->drag.prev.y = ma->y;
		ma->tree->drag.prev.node_y = ma->current_y;
		ma->tree->drag.prev.node_h = height;
	}

	/* Handle drag start */
	if (ma->tree->drag.type == TV_DRAG_NONE) {
		if (ma->mouse & BROWSER_MOUSE_DRAG_1 &&
				ma->tree->drag.selected == false &&
				ma->tree->drag.section ==
						TV_NODE_SECTION_NONE) {
			ma->tree->drag.type = TV_DRAG_SELECTION;
			ma->tree->cw_t->drag_status(ma->tree->cw_h,
					CORE_WINDOW_DRAG_SELECTION);

		} else if (ma->mouse & BROWSER_MOUSE_DRAG_2) {
			ma->tree->drag.type = TV_DRAG_SELECTION;
			ma->tree->cw_t->drag_status(ma->tree->cw_h,
					CORE_WINDOW_DRAG_SELECTION);
		}

		if (ma->tree->drag.start_node != NULL &&
				ma->tree->drag.type == TV_DRAG_SELECTION) {
			ma->tree->drag.start_node->flags ^= TREE_NODE_SELECTED;
		}
	}

	/* Handle selection drags */
	if (ma->tree->drag.type == TV_DRAG_SELECTION) {
		int curr_y1 = ma->current_y + height;
		int prev_y1 = ma->tree->drag.prev.node_y +
				ma->tree->drag.prev.node_h;

		r.y0 = (ma->current_y < ma->tree->drag.prev.node_y) ?
				ma->current_y : ma->tree->drag.prev.node_y;
		r.y1 = (curr_y1 > prev_y1) ? curr_y1 : prev_y1;

		redraw = true;

		ma->tree->drag.prev.x = ma->x;
		ma->tree->drag.prev.y = ma->y;
		ma->tree->drag.prev.node_y = ma->current_y;
		ma->tree->drag.prev.node_h = height;
	}

	click = ma->mouse & (BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_CLICK_2);

	if (((node->type == TREE_NODE_FOLDER) &&
			(ma->mouse & BROWSER_MOUSE_DOUBLE_CLICK) && click) ||
			(section == TV_NODE_SECTION_TOGGLE && click)) {
		/* Clear any existing selection */
		redraw |= treeview_clear_selection(ma->tree, &r);

		/* Toggle node expansion */
		if (node->flags & TREE_NODE_EXPANDED) {
			err = treeview_node_contract(ma->tree, node);
		} else {
			err = treeview_node_expand(ma->tree, node);
		}

		/* Set up redraw */
		if (!redraw || r.y0 > ma->current_y)
			r.y0 = ma->current_y;
		r.y1 = REDRAW_MAX;
		redraw = true;

	} else if ((node->type == TREE_NODE_ENTRY) &&
			(ma->mouse & BROWSER_MOUSE_DOUBLE_CLICK) && click) {
		struct treeview_node_msg msg;
		msg.msg = TREE_MSG_NODE_LAUNCH;
		msg.data.node_launch.mouse = ma->mouse;

		/* Clear any existing selection */
		redraw |= treeview_clear_selection(ma->tree, &r);

		/* Tell client an entry was launched */
		ma->tree->callbacks->entry(msg, node->client_data);

	} else if (ma->mouse & BROWSER_MOUSE_PRESS_1 &&
			!(node->flags & TREE_NODE_SELECTED) &&
			section != TV_NODE_SECTION_TOGGLE) {
		/* Clear any existing selection */
		redraw |= treeview_clear_selection(ma->tree, &r);

		/* Select node */
		action |= TV_NODE_ACTION_SELECTION;

	} else if (ma->mouse & BROWSER_MOUSE_PRESS_2 ||
			(ma->mouse & BROWSER_MOUSE_PRESS_1 &&
			 ma->mouse & BROWSER_MOUSE_MOD_2)) {
		/* Toggle selection of node */
		action |= TV_NODE_ACTION_SELECTION;
	}

	if (action & TV_NODE_ACTION_SELECTION) {
		/* Handle change in selection */
		node->flags ^= TREE_NODE_SELECTED;

		/* Redraw */
		if (!redraw) {
			r.y0 = ma->current_y;
			r.y1 = ma->current_y + height;
			redraw = true;
		} else {
			if (r.y0 > ma->current_y)
				r.y0 = ma->current_y;
			if (r.y1 < ma->current_y + height)
				r.y1 = ma->current_y + height;
		}
	}

	if (redraw) {
		ma->tree->cw_t->redraw_request(ma->tree->cw_h, r);
	}

	return true; /* Reached line with click; stop walking tree */
}
/* Exported interface, documented in treeview.h */
void treeview_mouse_action(struct treeview *tree,
		browser_mouse_state mouse, int x, int y)
{
	bool redraw = false;

	assert(tree != NULL);
	assert(tree->root != NULL);

	if (mouse == BROWSER_MOUSE_HOVER &&
			tree->drag.type == TV_DRAG_SELECTION) {
		treeview_commit_selection_drag(tree);
		tree->drag.type = TV_DRAG_NONE;
		tree->drag.start_node = NULL;

		tree->cw_t->drag_status(tree->cw_h, CORE_WINDOW_DRAG_NONE);
		return;
	}

	if (y > tree->root->height) {
		/* Below tree */
		struct rect r;

		r.x0 = 0;
		r.x1 = REDRAW_MAX;

		/* Record what position / section a drag started on */
		if (mouse & (BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_PRESS_2) &&
				tree->drag.type == TV_DRAG_NONE) {
			tree->drag.selected = false;
			tree->drag.start_node = NULL;
			tree->drag.section = TV_NODE_SECTION_NONE;
			tree->drag.start.x = x;
			tree->drag.start.y = y;
			tree->drag.start.node_y = tree->root->height;
			tree->drag.start.node_h = 0;

			tree->drag.prev.x = x;
			tree->drag.prev.y = y;
			tree->drag.prev.node_y = tree->root->height;
			tree->drag.prev.node_h = 0;
		}

		/* Handle drag start */
		if (tree->drag.type == TV_DRAG_NONE) {
			if (mouse & BROWSER_MOUSE_DRAG_1 &&
					tree->drag.selected == false &&
					tree->drag.section ==
							TV_NODE_SECTION_NONE) {
				tree->drag.type = TV_DRAG_SELECTION;
			} else if (mouse & BROWSER_MOUSE_DRAG_2) {
				tree->drag.type = TV_DRAG_SELECTION;
			}

			if (tree->drag.start_node != NULL &&
					tree->drag.type == TV_DRAG_SELECTION) {
				tree->drag.start_node->flags ^=
						TREE_NODE_SELECTED;
			}
		}

		/* Handle selection drags */
		if (tree->drag.type == TV_DRAG_SELECTION) {
			int curr_y1 = tree->root->height;
			int prev_y1 = tree->drag.prev.node_y +
					tree->drag.prev.node_h;

			r.y0 = tree->drag.prev.node_y;
			r.y1 = (curr_y1 > prev_y1) ? curr_y1 : prev_y1;

			redraw = true;

			tree->drag.prev.x = x;
			tree->drag.prev.y = y;
			tree->drag.prev.node_y = curr_y1;
			tree->drag.prev.node_h = 0;
		}

		if (mouse & BROWSER_MOUSE_PRESS_1) {
			/* Clear any existing selection */
			redraw |= treeview_clear_selection(tree, &r);
		}

		if (redraw) {
			tree->cw_t->redraw_request(tree->cw_h, r);
		}

	} else {
		/* On tree */
		struct treeview_mouse_action ma;

		ma.tree = tree;
		ma.mouse = mouse;
		ma.x = x;
		ma.y = y;
		ma.current_y = 0;

		treeview_walk_internal(tree->root, false, NULL,
				treeview_node_mouse_action_cb, &ma);
	}
}



/* Mix two colours according to the proportion given by p.
 * Where 0 <= p <= 255
 * p=0   gives result ==> c1
 * p=255 gives result ==> c0
 */
#define mix_colour(c0, c1, p)						\
	((((((c1 & 0xff00ff) * (255 - p)) +				\
	    ((c0 & 0xff00ff) * (      p))   ) >> 8) & 0xff00ff) |	\
	 (((((c1 & 0x00ff00) * (255 - p)) +				\
	    ((c0 & 0x00ff00) * (      p))   ) >> 8) & 0x00ff00))


/**
 * Initialise the plot styles from CSS system colour values.
 */
static void treeview_init_plot_styles(int font_pt_size)
{
	/* Background colour */
	plot_style_even.bg.stroke_type = PLOT_OP_TYPE_NONE;
	plot_style_even.bg.stroke_width = 0;
	plot_style_even.bg.stroke_colour = 0;
	plot_style_even.bg.fill_type = PLOT_OP_TYPE_SOLID;
	plot_style_even.bg.fill_colour = gui_system_colour_char("Window");

	/* Text colour */
	plot_style_even.text.family = PLOT_FONT_FAMILY_SANS_SERIF;
	plot_style_even.text.size = font_pt_size * FONT_SIZE_SCALE;
	plot_style_even.text.weight = 400;
	plot_style_even.text.flags = FONTF_NONE;
	plot_style_even.text.foreground = gui_system_colour_char("WindowText");
	plot_style_even.text.background = gui_system_colour_char("Window");

	/* Entry field text colour */
	plot_style_even.itext = plot_style_even.text;
	plot_style_even.itext.foreground = mix_colour(
			plot_style_even.text.foreground,
			plot_style_even.text.background, 255 * 10 / 16);

	/* Selected background colour */
	plot_style_even.sbg = plot_style_even.bg;
	plot_style_even.sbg.fill_colour = gui_system_colour_char("Highlight");

	/* Selected text colour */
	plot_style_even.stext = plot_style_even.text;
	plot_style_even.stext.foreground =
			gui_system_colour_char("HighlightText");
	plot_style_even.stext.background = gui_system_colour_char("Highlight");

	/* Selected entry field text colour */
	plot_style_even.sitext = plot_style_even.stext;
	plot_style_even.sitext.foreground = mix_colour(
			plot_style_even.stext.foreground,
			plot_style_even.stext.background, 255 * 25 / 32);


	/* Odd numbered node styles */
	plot_style_odd.bg = plot_style_even.bg;
	plot_style_odd.bg.fill_colour = mix_colour(
			plot_style_even.bg.fill_colour,
			plot_style_even.text.foreground, 255 * 15 / 16);
	plot_style_odd.text = plot_style_even.text;
	plot_style_odd.text.background = plot_style_odd.bg.fill_colour;
	plot_style_odd.itext = plot_style_odd.text;
	plot_style_odd.itext.foreground = mix_colour(
			plot_style_odd.text.foreground,
			plot_style_odd.text.background, 255 * 10 / 16);

	plot_style_odd.sbg = plot_style_even.sbg;
	plot_style_odd.stext = plot_style_even.stext;
	plot_style_odd.sitext = plot_style_even.sitext;
}


/**
 * Callback for hlcache.
 */
static nserror treeview_res_cb(hlcache_handle *handle,
		const hlcache_event *event, void *pw)
{
	struct treeview_resource *r = pw;

	switch (event->type) {
	case CONTENT_MSG_READY:
	case CONTENT_MSG_DONE:
		r->ready = true;
		r->height = content_get_height(handle);
		break;

	default:
		break;
	}

	return NSERROR_OK;
}


/**
 * Fetch content resources used by treeview.
 */
static void treeview_init_resources(void)
{
	int i;

	for (i = 0; i < TREE_RES_LAST; i++) {
		nsurl *url;
		if (nsurl_create(treeview_res[i].url, &url) == NSERROR_OK) {
			hlcache_handle_retrieve(url, 0, NULL, NULL,
					treeview_res_cb,
					&(treeview_res[i]), NULL,
					CONTENT_IMAGE, &(treeview_res[i].c));
			nsurl_unref(url);
		}
	}
}


/**
 * Measures width of characters used to represent treeview furniture.
 */
static void treeview_init_furniture(void)
{
	int i;
	tree_g.furniture_width = 0;

	for (i = 0; i < TREE_FURN_LAST; i++) {
		nsfont.font_width(&plot_style_odd.text,
				treeview_furn[i].data,
				treeview_furn[i].len,
				&(treeview_furn[i].width));

		if (treeview_furn[i].width > tree_g.furniture_width)
			tree_g.furniture_width = treeview_furn[i].width;
	}

	tree_g.furniture_width += 5;
}


/* Exported interface, documented in treeview.h */
nserror treeview_init(void)
{
	int font_px_size;
	int font_pt_size = 11;

	treeview_init_plot_styles(font_pt_size);
	treeview_init_resources();
	treeview_init_furniture();

	font_px_size = (font_pt_size * FIXTOINT(nscss_screen_dpi) + 36) / 72;

	tree_g.line_height = (font_px_size * 8 + 3) / 6;
	tree_g.step_width = tree_g.furniture_width;
	tree_g.window_padding = 6;
	tree_g.icon_step = 23;

	return NSERROR_OK;
}


/* Exported interface, documented in treeview.h */
nserror treeview_fini(void)
{
	int i;

	for (i = 0; i < TREE_RES_LAST; i++) {
		hlcache_handle_release(treeview_res[i].c);
	}

	return NSERROR_OK;
}
