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

#include "desktop/gui.h"
#include "desktop/knockout.h"
#include "desktop/plotters.h"
#include "desktop/treeview.h"
#include "render/font.h"
#include "utils/log.h"

#define FIELD_FOLDER 0
#define FIELD_FIRST_ENTRY 1

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

struct treeview_text {
	const char *data;
	uint32_t len;
	int width;
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
	enum treeview_node_flags flags;
	enum treeview_node_type type;

	uint32_t height;

	struct treeview_node *parent;
	struct treeview_node *sibling_prev;
	struct treeview_node *sibling_next;
	struct treeview_node *children;

	void *client_data;

	struct treeview_field text;
};

struct treeview_node_entry {
	struct treeview_node *base;
	struct treeview_field fields[];
};

struct treeview {
	uint32_t view_height;
	uint32_t view_width;

	struct treeview_node *root;

	struct treeview_field *fields;
	int n_fields; /* fields[n_fields] is folder, lower are entry fields */
	int field_width;

	const struct treeview_callback_table *callbacks;
	const struct core_window_callback_table *cw_t; /**< Core window callback table */
	const struct core_window *cw_h; /**< Core window handle */
};


struct treeview_node_style {
	plot_style_t bg;		/**< Background */
	plot_font_style_t text;		/**< Text */

	plot_style_t sbg;		/**< Selected background */
	plot_font_style_t stext;	/**< Selected text */

	plot_style_t sabg;		/**< Selection area background */
	plot_font_style_t satext;	/**< Selection area text */
};

struct treeview_node_style plot_style_odd;
struct treeview_node_style plot_style_even;

struct treeview_resource {
	const char *url;
	struct hlcache_handle *c;
	int height;
	bool ready;
};
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
};



enum treeview_furniture_id {
	TREE_FURN_EXPAND = 0,
	TREE_FURN_CONTRACT,
	TREE_FURN_LAST
};
static struct treeview_text treeview_furn[TREE_FURN_LAST] = {
	{ "\xe2\x96\xb8", 3, 0 },
	{ "\xe2\x96\xbe", 3, 0 }
};


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
	case TREE_REL_CHILD:
		assert(b->type != TREE_NODE_ENTRY);
		a->parent = b;
		a->sibling_next = b->children;
		if (a->sibling_next)
			a->sibling_next->sibling_prev = a;
		b->children = a;
		break;

	case TREE_REL_SIBLING_NEXT:
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


nserror treeview_create_node_folder(struct treeview *tree,
		struct treeview_node **folder,
		struct treeview_node *relation,
		enum treeview_relationship rel,
		const struct treeview_field_data *field,
		void *data)
{
	struct treeview_node *n;

	assert(data != NULL);
	assert(tree != NULL);
	assert(tree->root != NULL);

	if (relation == NULL) {
		relation = tree->root;
		rel = TREE_REL_CHILD;
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

	return NSERROR_OK;
}



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

		e->fields[i].value.data = fields[i].value;
		e->fields[i].value.len = fields[i].value_len;

		if (entry->flags & TREE_NODE_EXPANDED) {
			/* Text will be seen, get its width */
			nsfont.font_width(&plot_style_odd.text,
					e->fields[i].value.data,
					e->fields[i].value.len,
					&(e->fields[i].value.width));
		} else {
			/* Invalidate the width, since it's not needed yet */
			entry->text.value.width = 0;
		}
	}

	return NSERROR_OK;
}


nserror treeview_create_node_entry(struct treeview *tree,
		struct treeview_node **entry,
		struct treeview_node *relation,
		enum treeview_relationship rel,
		const struct treeview_field_data fields[],
		void *data)
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
		rel = TREE_REL_CHILD;
	}

	e = malloc(sizeof(struct treeview_node_entry) +
			tree->n_fields * sizeof(struct treeview_field));
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

		e->fields[i].value.data = fields[i].value;
		e->fields[i].value.len = fields[i].value_len;
		e->fields[i].value.width = 0;
	}

	treeview_insert_node(n, relation, rel);

	*entry = n;

	return NSERROR_OK;
}


nserror treeview_delete_node(struct treeview_node *n)
{
	/* Destroy children first */
	while (n->children != NULL) {
		treeview_delete_node(n->children);
	}

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

	/* Handle any special treatment */
	switch (n->type) {
	case TREE_NODE_ENTRY:
		break;
	case TREE_NODE_FOLDER:
		break;
	case TREE_NODE_ROOT:
		break;
	default:
		return NSERROR_BAD_PARAMETER;
	}

	/* Free the node */
	free(n);

	return NSERROR_OK;
}


nserror treeview_create(struct treeview **tree,
		const struct treeview_callback_table *callbacks,
		int n_fields, struct treeview_field_desc fields[],
		const struct core_window_callback_table *cw_t,
		const struct core_window *cw)
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

	(*tree)->cw_t = cw_t;
	(*tree)->cw_h = cw;

	return NSERROR_OK;
}

nserror treeview_destroy(struct treeview *tree)
{
	int f;

	assert(tree != NULL);

	/* Destroy nodes */
	treeview_delete_node(tree->root);

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
 * \param root     Root to walk tree from (doesn't get a callback call)
 * \param full     Iff true, visit children of collapsed nodes
 * \param callback Function to call on each node
 * \param ctx      Context to pass to callback
 * \return true iff callback caused premature abort
 */
static bool treeview_walk(struct treeview_node *root, bool full,
		bool (*callback)(struct treeview_node *node,
				int inset, void *ctx),
		void *ctx)
{
	struct treeview_node *node, *next;
	int inset = tree_g.window_padding - tree_g.step_width;

	node = root;

	while (node != NULL) {
		next = (full || (node->flags & TREE_NODE_EXPANDED)) ?
				node->children : NULL;

		if (next != NULL) {
			/* down to children */
			node = next;
			inset += tree_g.step_width;
		} else {
			/* no children */
			next = node->sibling_next;

			if (next != NULL) {
				/* on to next sibling */
				node = next;
			} else {
				/* no next sibling */
				while (node != root) {
					next = node->sibling_next;

					if (next != NULL) {
						break;
					}

					node = node->parent;
					inset -= tree_g.step_width;
				}

				if (node == root)
					break;

				node = node->sibling_next;
			}
		}

		assert(node != NULL);
		assert(node != root);

		if (callback(node, inset, ctx)) {
			/* callback caused early termination */
			return true;
		}

	}
	return false;
}


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
			/* Can't expand an empty node */
			return NSERROR_OK;
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

		for (i = 1; i < tree->n_fields; i++) {

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

	return NSERROR_OK;
}


static bool treeview_node_contract_cb(struct treeview_node *node, int inset,
		void *ctx)
{
	int height_reduction;

	assert(node != NULL);
	assert(node->type != TREE_NODE_ROOT);

	if ((node->flags & TREE_NODE_EXPANDED) == false) {
		/* Nothing to do. */
		return false;
	}

	node->flags |= ~TREE_NODE_EXPANDED;
	height_reduction = node->height - tree_g.line_height;

	assert(height_reduction >= 0);

	do {
		node->height -= height_reduction;
		node = node->parent;
	} while (node->parent != NULL);

	return false; /* Don't want to abort tree walk */
}
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
	treeview_walk(node, false, treeview_node_contract_cb, NULL);

	/* Contract node */
	treeview_node_contract_cb(node, 0, NULL);

	return NSERROR_OK;
}

/**
 * Redraws a treeview.
 *
 * \param tree		the tree to draw
 * \param x		X coordinate to draw the tree at (wrt plot origin)
 * \param y		Y coordinate to draw the tree at (wrt plot origin)
 * \param clip_x	clipping rectangle (wrt tree origin)
 * \param ctx		current redraw context
 */
void treeview_redraw(struct treeview *tree, int x, int y, struct rect *clip,
		const struct redraw_context *ctx)
{
	struct redraw_context new_ctx = *ctx;
	struct treeview_node *node, *root, *next;
	struct treeview_node_entry *entry;
	struct treeview_node_style *style = &plot_style_odd;
	struct content_redraw_data data;
	struct rect r;
	int inset = tree_g.window_padding - tree_g.step_width;
	uint32_t count = 0;
	int render_y = 0;
	int x0, y0, y1;
	int baseline = (tree_g.line_height * 3 + 2) / 4;
	enum treeview_resource_id res;
	plot_style_t *bg;
	plot_font_style_t *text;

	assert(tree != NULL);
	assert(tree->root != NULL);
	assert(tree->root->flags & TREE_NODE_EXPANDED);

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
			inset += tree_g.step_width;
		} else {
			/* no children */
			next = node->sibling_next;

			if (next != NULL) {
				/* on to next sibling */
				node = next;
			} else {
				/* no next sibling */
				while (node != root) {
					next = node->sibling_next;

					if (next != NULL) {
						break;
					}
					node = node->parent;
					inset -= tree_g.step_width;
				}

				if (node == root)
					break;

				node = node->sibling_next;
			}
		}

		assert(node != NULL);
		assert(node != root);

		count++;

		if (render_y + tree_g.line_height < clip->y0) {
			/* This node's line is above clip region */
			render_y += tree_g.line_height;
			continue;
		}

		style = (count & 0x1) ? &plot_style_odd : &plot_style_even;
		if (node->flags & TREE_NODE_SELECTED) {
			bg = &style->sbg;
			text = &style->stext;
		} else {
			bg = &style->bg;
			text = &style->text;
		}

		/* Render background */
		y0 = render_y;
		y1 = render_y + tree_g.line_height;
		new_ctx.plot->rectangle(r.x0, y0, r.x1, y1, bg);

		/* Render toggle */
		if (node->flags & TREE_NODE_EXPANDED) {
			new_ctx.plot->text(inset, render_y + baseline,
					treeview_furn[TREE_FURN_CONTRACT].data,
					treeview_furn[TREE_FURN_CONTRACT].len,
					text);
		} else {
			new_ctx.plot->text(inset, render_y + baseline,
					treeview_furn[TREE_FURN_EXPAND].data,
					treeview_furn[TREE_FURN_EXPAND].len,
					text);
		}

		/* Render icon */
		if (node->type == TREE_NODE_ENTRY)
			res = TREE_RES_CONTENT;
		else if (node->type == TREE_NODE_FOLDER)
			res = TREE_NODE_FOLDER;

		if (treeview_res[res].ready) {
			/* Icon resource is available */
			data.x = inset + tree_g.step_width;
			data.y = render_y + ((tree_g.line_height -
					treeview_res[res].height + 1) / 2);
			data.background_colour = style->bg.fill_colour;

			content_redraw(treeview_res[res].c,
					&data, &r, &new_ctx);
		}

		/* Render text */
		x0 = inset + tree_g.step_width + tree_g.icon_step;
		new_ctx.plot->text(x0, render_y + baseline,
				node->text.value.data, node->text.value.len,
				text);

		/* Rendered the node */
		render_y += tree_g.line_height;
		if (render_y > clip->y1) {
			/* Passed the bottom of what's in the clip region.
			 * Done. */
			break;
		}


		if (node->type != TREE_NODE_ENTRY ||
				!(node->flags & TREE_NODE_EXPANDED))
			/* Done everything for this node */
			continue;


		/* Reneder expanded entry background */
		y0 = render_y;
		y1 = render_y + tree_g.line_height * tree->n_fields;
		new_ctx.plot->rectangle(r.x0, y0, r.x1, y1, bg);

		/* Render expanded entry fields */
		entry = (struct treeview_node_entry *)node;
		for (i = 0; i < tree->n_fields; i++) {
			struct treeview_field *ef = &(tree->fields[i]);
			int max_width = tree->field_width;

			if (ef->flags & TREE_FLAG_SHOW_NAME) {
				new_ctx.plot->text(x0 + max_width -
							ef->value.width -
							tree_g.step_width,
						render_y + baseline,
						ef->value.data,
						ef->value.len,
						text);

				new_ctx.plot->text(x0 + max_width,
						render_y + baseline,
						entry->fields[i].value.data,
						entry->fields[i].value.len,
						text);
			} else {
				new_ctx.plot->text(x0, render_y + baseline,
						entry->fields[i].value.data,
						entry->fields[i].value.len,
						text);

			}

			/* Rendered the expanded entry field */
			render_y += tree_g.line_height;
		}

		/* Finshed rendering expanded entry */

		if (render_y > clip->y1) {
			/* Passed the bottom of what's in the clip region.
			 * Done. */
			break;
		}
	}

	if (render_y < clip->y1) {
		/* Fill the blank area at the bottom */
		y0 = render_y;
		new_ctx.plot->rectangle(r.x0, y0, r.x1, r.y1, bg);
		
	}

	/* Rendering complete */
	if (ctx->plot->option_knockout)
		knockout_plot_end();
}

struct treeview_mouse_action {
	struct treeview *tree;
	browser_mouse_state mouse;
	int x;
	int y;
	int current_y;
};
static bool treeview_node_mouse_action_cb(struct treeview_node *node,
		int inset, void *ctx)
{
	struct treeview_mouse_action *ma = ctx;
	struct rect r;

	/* Skip line if we've not reached mouse y */
	if (ma->y > ma->current_y + tree_g.line_height) {
		ma->current_y += tree_g.line_height;
		return false; /* Don't want to abort tree walk */
	}

	if (ma->mouse & BROWSER_MOUSE_CLICK_1) {
		node->flags ^= TREE_NODE_SELECTED;

		r.x0 = 0;
		r.y0 = ma->current_y;
		r.x1 = INT_MAX;
		r.y1 = ma->current_y + tree_g.line_height;
		ma->tree->cw_t->redraw_request(ma->tree->cw_h, r);
	}

	return true; /* Reached line with click; stop walking tree */
}
void treeview_mouse_action(struct treeview *tree,
		browser_mouse_state mouse, int x, int y)
{
	struct treeview_mouse_action ma;

	ma.tree = tree;
	ma.mouse = mouse;
	ma.x = x;
	ma.y = y;
	ma.current_y = 0;

	treeview_walk(tree->root, false, treeview_node_mouse_action_cb, &ma);
}



/* Mix two colours according to the proportion given by p.
 * Where 0 <= p <= 255
 * p=0   gives result=c0
 * p=255 gives result=c1
 */
#define mix_colour(c0, c1, p)						\
	((((((c1 & 0xff00ff) * (255 - p)) +				\
	    ((c0 & 0xff00ff) * (      p))   ) >> 8) & 0xff00ff) |	\
	 (((((c1 & 0x00ff00) * (255 - p)) +				\
	    ((c0 & 0x00ff00) * (      p))   ) >> 8) & 0x00ff00))


static void treeview_init_plot_styles(void)
{
	/* Background colour */
	plot_style_even.bg.stroke_type = PLOT_OP_TYPE_NONE;
	plot_style_even.bg.stroke_width = 0;
	plot_style_even.bg.stroke_colour = 0;
	plot_style_even.bg.fill_type = PLOT_OP_TYPE_SOLID;
	plot_style_even.bg.fill_colour = gui_system_colour_char("Window");

	/* Text colour */
	plot_style_even.text.family = PLOT_FONT_FAMILY_SANS_SERIF;
	plot_style_even.text.size = 11 * FONT_SIZE_SCALE;
	plot_style_even.text.weight = 400;
	plot_style_even.text.flags = FONTF_NONE;
	plot_style_even.text.foreground = gui_system_colour_char("WindowText");
	plot_style_even.text.background = gui_system_colour_char("Window");

	/* Selected background colour */
	plot_style_even.sbg = plot_style_even.bg;
	plot_style_even.sbg.fill_colour = gui_system_colour_char("Highlight");

	/* Selected text colour */
	plot_style_even.stext = plot_style_even.text;
	plot_style_even.stext.foreground =
			gui_system_colour_char("HighlightText");
	plot_style_even.stext.background = gui_system_colour_char("Highlight");

	/* Selection area background colour */
	plot_style_even.sabg = plot_style_even.bg;
	plot_style_even.sabg.fill_colour = mix_colour(
			plot_style_even.bg.fill_colour,
			plot_style_even.sbg.fill_colour, 255 * 3 / 4);

	/* Selection area text colour */
	plot_style_even.satext = plot_style_even.text;
	plot_style_even.satext.background = plot_style_even.sabg.fill_colour;


	/* Odd numbered node styles */
	plot_style_odd.bg = plot_style_even.bg;
	plot_style_odd.bg.fill_colour = mix_colour(
			plot_style_even.bg.fill_colour,
			plot_style_even.text.foreground, 255 * 15 / 16);
	plot_style_odd.text = plot_style_even.text;
	plot_style_odd.text.background = plot_style_odd.bg.fill_colour;

	plot_style_odd.sbg = plot_style_even.sbg;
	plot_style_odd.stext = plot_style_even.stext;

	plot_style_odd.sabg = plot_style_even.sabg;
	plot_style_odd.sabg.fill_colour = mix_colour(
			plot_style_even.sabg.fill_colour,
			plot_style_even.satext.foreground, 255 * 15 / 16);
	plot_style_odd.satext = plot_style_even.satext;
	plot_style_odd.satext.background = plot_style_odd.sabg.fill_colour;
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


nserror treeview_init(void)
{
	treeview_init_plot_styles();
	treeview_init_resources();
	treeview_init_furniture();

	tree_g.line_height = 20;
	tree_g.step_width = tree_g.furniture_width;
	tree_g.window_padding = 6;
	tree_g.icon_step = 23;

	return NSERROR_OK;
}


nserror treeview_fini(void)
{
	int i;

	for (i = 0; i < TREE_RES_LAST; i++) {
		hlcache_handle_release(treeview_res[i].c);
	}

	return NSERROR_OK;
}


struct treeview_node * treeview_get_root(struct treeview *tree)
{
	assert(tree != NULL);
	assert(tree->root != NULL);

	return tree->root;
}
