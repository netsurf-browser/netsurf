/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Conversion of XML tree to box tree (implementation).
 */

#define _GNU_SOURCE  /* for strndup */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "libxml/HTMLparser.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/desktop/options.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/form.h"
#include "netsurf/render/html.h"
#ifdef riscos
#include "netsurf/desktop/gui.h"
#endif
#ifdef WITH_PLUGIN
#include "netsurf/riscos/plugin.h"
#endif
#define NDEBUG
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/pool.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


/** Status of box tree construction. */
struct box_status {
	struct content *content;
	char *href;
	char *title;
	struct form *current_form;
	char *id;
};

/** Return type for special case element functions. */
struct box_result {
	/** Box for element, if any, 0 otherwise. */
	struct box *box;
	/** Children of this element should be converted. */
	bool convert_children;
	/** Memory was exhausted when handling the element. */
	bool memory_error;
};

/** MultiLength, as defined by HTML 4.01. */
struct box_multi_length {
	enum { LENGTH_PX, LENGTH_PERCENT, LENGTH_RELATIVE } type;
	float value;
};

static const content_type image_types[] = {
#ifdef WITH_JPEG
	CONTENT_JPEG,
#endif
#ifdef WITH_GIF
	CONTENT_GIF,
#endif
#ifdef WITH_PNG
	CONTENT_PNG,
#endif
#ifdef WITH_MNG
	CONTENT_JNG,
	CONTENT_MNG,
#endif
#ifdef WITH_SPRITE
	CONTENT_SPRITE,
#endif
#ifdef WITH_DRAW
	CONTENT_DRAW,
#endif
	CONTENT_UNKNOWN };

#define MAX_SPAN	( 100 )

struct span_info {
	unsigned int row_span;
	bool auto_row;
	bool auto_column;
};

struct columns {
	unsigned int current_column;
	bool extra;
	/* Number of columns in main part of table 1..max columns */
	unsigned int num_columns;
	/* Information about columns in main table,
	  array 0 to num_columns - 1 */
	struct span_info *spans;
	/* Number of columns that have cells after a colspan 0 */
	unsigned int extra_columns;
	/* Number of rows in table */
	unsigned int num_rows;
};

static bool convert_xml_to_box(xmlNode *n, struct content *content,
		struct css_style *parent_style,
		struct box *parent, struct box **inline_container,
		struct box_status status);
struct css_style * box_get_style(struct content *c,
		struct css_style *parent_style,
		xmlNode *n);
static void box_text_transform(char *s, unsigned int len,
		css_text_transform tt);
static struct box_result box_a(xmlNode *n, struct box_status *status,
		struct css_style *style);
static struct box_result box_body(xmlNode *n, struct box_status *status,
		struct css_style *style);
static struct box_result box_br(xmlNode *n, struct box_status *status,
		struct css_style *style);
static struct box_result box_image(xmlNode *n, struct box_status *status,
		struct css_style *style);
static struct box_result box_form(xmlNode *n, struct box_status *status,
		struct css_style *style);
static struct box_result box_textarea(xmlNode *n, struct box_status *status,
		struct css_style *style);
static struct box_result box_select(xmlNode *n, struct box_status *status,
		struct css_style *style);
static struct box_result box_input(xmlNode *n, struct box_status *status,
		struct css_style *style);
static struct box *box_input_text(xmlNode *n, struct box_status *status,
		struct css_style *style, bool password);
static struct box_result box_button(xmlNode *n, struct box_status *status,
		struct css_style *style);
static struct box_result box_frameset(xmlNode *n, struct box_status *status,
		struct css_style *style);
static bool box_select_add_option(struct form_control *control, xmlNode *n);
static bool box_normalise_block(struct box *block, pool box_pool);
static bool box_normalise_table(struct box *table, pool box_pool);
static void box_normalise_table_spans( struct box *table );
static bool box_normalise_table_row_group(struct box *row_group,
		struct columns *col_info,
		pool box_pool);
static bool box_normalise_table_row(struct box *row,
		struct columns *col_info,
		pool box_pool);
static bool calculate_table_row(struct columns *col_info,
		unsigned int col_span, unsigned int row_span,
		unsigned int *start_column);
static bool box_normalise_inline_container(struct box *cont, pool box_pool);
static void box_free_box(struct box *box);
static struct box_result box_object(xmlNode *n, struct box_status *status,
		struct css_style *style);
static struct box_result box_embed(xmlNode *n, struct box_status *status,
		struct css_style *style);
static struct box_result box_applet(xmlNode *n, struct box_status *status,
		struct css_style *style);
static struct box_result box_iframe(xmlNode *n, struct box_status *status,
		struct css_style *style);
static bool plugin_decode(struct content* content, char* url, struct box* box,
		struct object_params* po);
static struct box_multi_length *box_parse_multi_lengths(const char *s,
		unsigned int *count);
static bool box_contains_point(struct box *box, int x, int y);

#define box_is_float(box) (box->type == BOX_FLOAT_LEFT || \
		box->type == BOX_FLOAT_RIGHT)

/* element_table must be sorted by name */
struct element_entry {
	char name[10];   /* element type */
	struct box_result (*convert)(xmlNode *n, struct box_status *status,
			struct css_style *style);
};
static const struct element_entry element_table[] = {
	{"a", box_a},
	{"applet", box_applet},
	{"body", box_body},
	{"br", box_br},
	{"button", box_button},
	{"embed", box_embed},
	{"form", box_form},
	{"frameset", box_frameset},
	{"iframe", box_iframe},
	{"img", box_image},
	{"input", box_input},
	{"object", box_object},
	{"select", box_select},
	{"textarea", box_textarea}
};
#define ELEMENT_TABLE_COUNT (sizeof(element_table) / sizeof(element_table[0]))


/**
 * Add a child to a box tree node.
 */

void box_add_child(struct box * parent, struct box * child)
{
	if (parent->children != 0) {	/* has children already */
		parent->last->next = child;
		child->prev = parent->last;
	} else {			/* this is the first child */
		parent->children = child;
		child->prev = 0;
	}

	parent->last = child;
	child->parent = parent;
}


/**
 * Create a box tree node.
 *
 * \param  style     style for the box (not copied)
 * \param  href      href for the box (copied), or 0
 * \param  title     title for the box (copied), or 0
 * \param  id        id for the box (copied), or 0
 * \param  box_pool  pool to allocate box from
 * \return  allocated and initialised box, or 0 on memory exhaustion
 */

struct box * box_create(struct css_style * style,
		const char *href, const char *title, const char *id,
		pool box_pool)
{
	unsigned int i;
	struct box *box;
	char *href1 = 0;
	char *title1 = 0;
	char *id1 = 0;

	if (href)
		href1 = strdup(href);
	if (title)
		title1 = strdup(title);
	if (id)
		id1 = strdup(id);
	if ((href && !href1) || (title && !title1) || (id && !id1)) {
		free(href1);
		free(title1);
		free(id1);
		return 0;
	}

	box = pool_alloc(box_pool, sizeof (struct box));
	if (!box) {
		free(href1);
		free(title1);
		free(id1);
		return 0;
	}

	box->type = BOX_INLINE;
	box->style = style;
	box->x = box->y = 0;
	box->width = UNKNOWN_WIDTH;
	box->height = 0;
	box->descendant_x0 = box->descendant_y0 = 0;
	box->descendant_x1 = box->descendant_y1 = 0;
	for (i = 0; i != 4; i++)
		box->margin[i] = box->padding[i] = box->border[i] = 0;
	box->scroll_x = box->scroll_y = 0;
	box->min_width = 0;
	box->max_width = UNKNOWN_MAX_WIDTH;
	box->text = NULL;
	box->length = 0;
	box->space = 0;
	box->clone = 0;
	box->style_clone = 0;
	box->href = href1;
	box->title = title1;
	box->columns = 1;
	box->rows = 1;
	box->start_column = 0;
	box->next = NULL;
	box->prev = NULL;
	box->children = NULL;
	box->last = NULL;
	box->parent = NULL;
	box->float_children = NULL;
	box->next_float = NULL;
	box->col = NULL;
	box->font = NULL;
	box->gadget = NULL;
	box->usemap = NULL;
	box->id = id1;
	box->background = NULL;
	box->object = NULL;
	box->object_params = NULL;

	return box;
}


/**
 * Insert a new box as a sibling to a box in a tree.
 */

void box_insert_sibling(struct box *box, struct box *new_box)
{
	new_box->parent = box->parent;
	new_box->prev = box;
	new_box->next = box->next;
	box->next = new_box;
	if (new_box->next)
		new_box->next->prev = new_box;
	else if (new_box->parent)
		new_box->parent->last = new_box;
}


/**
 * Construct a box tree from an xml tree and stylesheets.
 *
 * \param  n  xml tree
 * \param  c  content of type CONTENT_HTML to construct box tree in
 * \return  true on success, false on memory exhaustion
 */

bool xml_to_box(xmlNode *n, struct content *c)
{
	struct box root;
	struct box_status status = {c, 0, 0, 0, 0};
	struct box *inline_container = 0;

	assert(c->type == CONTENT_HTML);

	root.type = BOX_BLOCK;
	root.style = NULL;
	root.next = NULL;
	root.prev = NULL;
	root.children = NULL;
	root.last = NULL;
	root.parent = NULL;
	root.float_children = NULL;
	root.next_float = NULL;

	c->data.html.style = css_duplicate_style(&css_base_style);
	if (!c->data.html.style)
		return false;

	c->data.html.style->font_size.value.length.value =
			option_font_size * 0.1;
	c->data.html.fonts = nsfont_new_set();
	if (!c->data.html.fonts) {
		css_free_style(c->data.html.style);
		return false;
	}

	c->data.html.object_count = 0;
	c->data.html.object = 0;

	if (!convert_xml_to_box(n, c, c->data.html.style, &root,
			&inline_container, status))
		return false;
	if (!box_normalise_block(&root, c->data.html.box_pool))
		return false;

	c->data.html.layout = root.children;
	c->data.html.layout->parent = NULL;

	return true;
}


/* mapping from CSS display to box type
 * this table must be in sync with css/css_enums */
static const box_type box_map[] = {
	0, /*CSS_DISPLAY_INHERIT,*/
	BOX_INLINE, /*CSS_DISPLAY_INLINE,*/
	BOX_BLOCK, /*CSS_DISPLAY_BLOCK,*/
	BOX_BLOCK, /*CSS_DISPLAY_LIST_ITEM,*/
	BOX_INLINE, /*CSS_DISPLAY_RUN_IN,*/
	BOX_INLINE_BLOCK, /*CSS_DISPLAY_INLINE_BLOCK,*/
	BOX_TABLE, /*CSS_DISPLAY_TABLE,*/
	BOX_TABLE, /*CSS_DISPLAY_INLINE_TABLE,*/
	BOX_TABLE_ROW_GROUP, /*CSS_DISPLAY_TABLE_ROW_GROUP,*/
	BOX_TABLE_ROW_GROUP, /*CSS_DISPLAY_TABLE_HEADER_GROUP,*/
	BOX_TABLE_ROW_GROUP, /*CSS_DISPLAY_TABLE_FOOTER_GROUP,*/
	BOX_TABLE_ROW, /*CSS_DISPLAY_TABLE_ROW,*/
	BOX_INLINE, /*CSS_DISPLAY_TABLE_COLUMN_GROUP,*/
	BOX_INLINE, /*CSS_DISPLAY_TABLE_COLUMN,*/
	BOX_TABLE_CELL, /*CSS_DISPLAY_TABLE_CELL,*/
	BOX_INLINE /*CSS_DISPLAY_TABLE_CAPTION,*/
};


/**
 * Recursively construct a box tree from an xml tree and stylesheets.
 *
 * \param  n             fragment of xml tree
 * \param  content       content of type CONTENT_HTML that is being processed
 * \param  parent_style  style at this point in xml tree
 * \param  parent        parent in box tree
 * \param  inline_container  current inline container box, or 0, updated to
 *                       new current inline container on exit
 * \param  status        status for forms etc.
 * \return  true on success, false on memory exhaustion
 */

bool convert_xml_to_box(xmlNode *n, struct content *content,
		struct css_style *parent_style,
		struct box *parent, struct box **inline_container,
		struct box_status status)
{
	struct box *box = 0;
	struct box *inline_container_c;
	struct css_style *style = 0;
	xmlNode *c;
	char *s;
	xmlChar *title0, *id0;
	char *title = 0, *id = 0;
	bool convert_children = true;
	char *href_in = status.href;

	assert(n);
	assert(parent_style);
	assert(parent);
	assert(inline_container);

	if (n->type == XML_ELEMENT_NODE) {
		struct element_entry *element;

		gui_multitask();

		style = box_get_style(content, parent_style, n);
		if (!style)
			goto no_memory;
		if (style->display == CSS_DISPLAY_NONE) {
			css_free_style(style);
			goto end;
		}
		/* floats are treated as blocks */
		if (style->float_ == CSS_FLOAT_LEFT ||
				style->float_ == CSS_FLOAT_RIGHT)
			if (style->display == CSS_DISPLAY_INLINE)
				style->display = CSS_DISPLAY_BLOCK;

		/* extract title attribute, if present */
		if ((title0 = xmlGetProp(n, (const xmlChar *) "title"))) {
			status.title = title = squash_whitespace(title0);
			xmlFree(title0);
			if (!title)
				goto no_memory;
		}

		/* extract id attribute, if present */
		if ((id0 = xmlGetProp(n, (const xmlChar *) "id"))) {
			status.id = id = squash_whitespace(id0);
			xmlFree(id0);
			if (!id)
				goto no_memory;
		}

		/* special elements */
		element = bsearch((const char *) n->name, element_table,
				ELEMENT_TABLE_COUNT, sizeof(element_table[0]),
				(int (*)(const void *, const void *)) strcmp);
		if (element) {
			/* a special convert function exists for this element */
			struct box_result res =
					element->convert(n, &status, style);
			box = res.box;
			convert_children = res.convert_children;
			if (res.memory_error)
				goto no_memory;
			if (!box) {
				/* no box for this element */
				assert(!convert_children);
				css_free_style(style);
				goto end;
			}
		} else {
			/* general element */
			box = box_create(style, status.href, title, id,
					content->data.html.box_pool);
			if (!box)
				goto no_memory;
		}
		/* set box type from style if it has not been set already */
		if (box->type == BOX_INLINE)
			box->type = box_map[style->display];

	} else if (n->type == XML_TEXT_NODE) {
		/* text node: added to inline container below */

	} else {
		/* not an element or text node: ignore it (eg. comment) */
		goto end;
	}

	content->size += sizeof(struct box) + sizeof(struct css_style);

	if (n->type == XML_TEXT_NODE &&
			(parent_style->white_space == CSS_WHITE_SPACE_NORMAL ||
			 parent_style->white_space == CSS_WHITE_SPACE_NOWRAP)) {
		char *text = squash_whitespace(n->content);
		if (!text)
			goto no_memory;

		/* if the text is just a space, combine it with the preceding
		 * text node, if any */
		if (text[0] == ' ' && text[1] == 0) {
			if (*inline_container) {
				assert((*inline_container)->last != 0);
				(*inline_container)->last->space = 1;
			}
			free(text);
			goto end;
		}

		if (!*inline_container) {
			/* this is the first inline node: make a container */
			*inline_container = box_create(0, 0, 0, 0,
					content->data.html.box_pool);
			if (!*inline_container)	{
				free(text);
				goto no_memory;
			}
			(*inline_container)->type = BOX_INLINE_CONTAINER;
			box_add_child(parent, *inline_container);
		}

		box = box_create(parent_style, status.href, title, id,
				content->data.html.box_pool);
		if (!box) {
			free(text);
			goto no_memory;
		}
		box->text = text;
		box->style_clone = 1;
		box->length = strlen(text);
		/* strip ending space char off */
		if (box->length > 1 && text[box->length - 1] == ' ') {
			box->space = 1;
			box->length--;
		}
		if (parent_style->text_transform != CSS_TEXT_TRANSFORM_NONE)
			box_text_transform(box->text, box->length,
					parent_style->text_transform);
		if (parent_style->white_space == CSS_WHITE_SPACE_NOWRAP) {
			unsigned int i;
			for (i = 0; i != box->length && text[i] != ' '; ++i)
				; /* no body */
			if (i != box->length) {
				/* there is a space in text block and we
				 * want all spaces to be converted to NBSP
				 */
				box->text = cnv_space2nbsp(text);
				if (!box->text) {
					free(text);
					goto no_memory;
				}
				box->length = strlen(box->text);
			}
		}
		box->font = nsfont_open(content->data.html.fonts, box->style);

		box_add_child(*inline_container, box);
		if (box->text[0] == ' ') {
			box->length--;
			memmove(box->text, &box->text[1], box->length);
			if (box->prev != NULL)
				box->prev->space = 1;
		}
		goto end;

	} else if (n->type == XML_TEXT_NODE) {
		/* white-space: pre */
		char *text = cnv_space2nbsp(n->content);
		char *current;
		/* note: pre-wrap/pre-line are unimplemented */
		assert(parent_style->white_space == CSS_WHITE_SPACE_PRE ||
			parent_style->white_space ==
						CSS_WHITE_SPACE_PRE_LINE ||
			parent_style->white_space ==
						CSS_WHITE_SPACE_PRE_WRAP);
		if (!text)
			goto no_memory;
		if (parent_style->text_transform != CSS_TEXT_TRANSFORM_NONE)
			box_text_transform(text, strlen(text),
					parent_style->text_transform);
		current = text;
		do {
			size_t len = strcspn(current, "\r\n");
			char old = current[len];
			current[len] = 0;
			if (!*inline_container) {
				*inline_container = box_create(0, 0, 0, 0,
						content->data.html.box_pool);
				if (!*inline_container) {
					free(text);
					goto no_memory;
				}
				(*inline_container)->type =
						BOX_INLINE_CONTAINER;
				box_add_child(parent, *inline_container);
			}
			box = box_create(parent_style, status.href, title,
					id, content->data.html.box_pool);
			if (!box) {
				free(text);
				goto no_memory;
			}
			box->type = BOX_INLINE;
			box->style_clone = 1;
			box->text = strdup(current);
			if (!box->text) {
				free(text);
				goto no_memory;
			}
			box->length = strlen(box->text);
			box->font = nsfont_open(content->data.html.fonts,
					box->style);
			box_add_child(*inline_container, box);
			current[len] = old;
			current += len;
			if (current[0] == '\r' && current[1] == '\n') {
				current += 2;
				*inline_container = 0;
			} else if (current[0] != 0) {
				current++;
				*inline_container = 0;
			}
		} while (*current);
		free(text);
		goto end;

	} else if (box->type == BOX_INLINE ||
			box->type == BOX_INLINE_BLOCK ||
			style->float_ == CSS_FLOAT_LEFT ||
			style->float_ == CSS_FLOAT_RIGHT ||
			box->type == BOX_BR) {
		/* this is an inline box */
		if (!*inline_container) {
			/* this is the first inline node: make a container */
			*inline_container = box_create(0, 0, 0, 0,
					content->data.html.box_pool);
			if (!*inline_container)
				goto no_memory;
			(*inline_container)->type = BOX_INLINE_CONTAINER;
			box_add_child(parent, *inline_container);
		}

		if (box->type == BOX_INLINE || box->type == BOX_BR) {
			/* inline box: add to tree and recurse */
			box_add_child(*inline_container, box);
			if (convert_children) {
				for (c = n->children; c != 0; c = c->next)
					if (!convert_xml_to_box(c, content,
							style, parent,
							inline_container,
							status))
						goto no_memory;
			}
			goto end;
		} else if (box->type == BOX_INLINE_BLOCK) {
			/* inline block box: add to tree and recurse */
			box_add_child(*inline_container, box);
			if (convert_children) {
				inline_container_c = 0;
				for (c = n->children; c != 0; c = c->next)
					if (!convert_xml_to_box(c, content,
							style, box,
							&inline_container_c,
							status))
						goto no_memory;
			}
			goto end;
		} else {
			/* float: insert a float box between the parent and
			 * current node */
			assert(style->float_ == CSS_FLOAT_LEFT ||
					style->float_ == CSS_FLOAT_RIGHT);
			parent = box_create(0, status.href, title, id,
					content->data.html.box_pool);
			if (!parent)
				goto no_memory;
			if (style->float_ == CSS_FLOAT_LEFT)
				parent->type = BOX_FLOAT_LEFT;
			else
				parent->type = BOX_FLOAT_RIGHT;
			box_add_child(*inline_container, parent);
			if (box->type == BOX_INLINE ||
					box->type == BOX_INLINE_BLOCK)
				box->type = BOX_BLOCK;
		}
	}

	assert(n->type == XML_ELEMENT_NODE);

	/* non-inline box: add to tree and recurse */
	box_add_child(parent, box);
	if (convert_children) {
		inline_container_c = 0;
		for (c = n->children; c != 0; c = c->next)
			if (!convert_xml_to_box(c, content, style,
					box, &inline_container_c, status))
				goto no_memory;
	}
	if (style->float_ == CSS_FLOAT_NONE)
		/* new inline container unless this is a float */
		*inline_container = 0;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "colspan")) != NULL) {
		box->columns = strtol(s, NULL, 10);
		if ( MAX_SPAN < box->columns ) {
			box->columns = 1;
		}
		xmlFree(s);
	}
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "rowspan")) != NULL) {
		box->rows = strtol(s, NULL, 10);
		if ( MAX_SPAN < box->rows ) {
			box->rows = 1;
		}
		xmlFree(s);
	}

end:
	free(title);
	free(id);
	if (!href_in)
		xmlFree(status.href);

	/* Now fetch any background image for this box */
	if (box && box->style && box->style->background_image.type ==
			CSS_BACKGROUND_IMAGE_URI) {
		char *url = strdup(box->style->background_image.uri);
		if (!url)
			return false;
		/* start fetch */
		if (!html_fetch_object(content, url, box, image_types,
				content->available_width, 1000, true))
			return false;
	}

	return true;

no_memory:
	free(title);
	free(id);
	if (!href_in)
		xmlFree(status.href);
	if (style && !box)
		css_free_style(style);

	return false;
}


/**
 * Get the style for an element.
 *
 * \param  c             content of type CONTENT_HTML that is being processed
 * \param  parent_style  style at this point in xml tree
 * \param  n             node in xml tree
 * \return  the new style, or 0 on memory exhaustion
 *
 * The style is collected from three sources:
 *  1. any styles for this element in the document stylesheet(s)
 *  2. non-CSS HTML attributes
 *  3. the 'style' attribute
 */

struct css_style * box_get_style(struct content *c,
		struct css_style *parent_style,
		xmlNode *n)
{
	char *s;
	unsigned int i;
	unsigned int stylesheet_count = c->data.html.stylesheet_count;
	struct content **stylesheet = c->data.html.stylesheet_content;
	struct css_style *style;
	struct css_style *style_new;
	char *url;
	url_func_result res;

	style = css_duplicate_style(parent_style);
	if (!style)
		return 0;

	style_new = css_duplicate_style(&css_blank_style);
	if (!style_new) {
		css_free_style(style);
		return 0;
	}

	for (i = 0; i != stylesheet_count; i++) {
		if (stylesheet[i]) {
			assert(stylesheet[i]->type == CONTENT_CSS);
			css_get_style(stylesheet[i], n, style_new);
		}
	}
	css_cascade(style, style_new);

	/* style_new isn't needed past this point */
	css_free_style(style_new);

	/* This property only applies to the body element, if you believe
	 * the spec. Many browsers seem to allow it on other elements too,
	 * so let's be generic ;) */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "background"))) {
		res = url_join(s, c->data.html.base_url, &url);
		if (res == URL_FUNC_NOMEM) {
			css_free_style(style);
			return 0;
		} else if (res == URL_FUNC_OK) {
			/* if url is equivalent to the parent's url,
			 * we've got infinite inclusion: ignore */
			if (strcmp(url, c->data.html.base_url) == 0)
				free(url);
			else {
				style->background_image.type =
						CSS_BACKGROUND_IMAGE_URI;
				style->background_image.uri = url;
			}
		}
		xmlFree(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "bgcolor")) != NULL) {
		unsigned int r, g, b;
		if (s[0] == '#' && sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
			style->background_color = (b << 16) | (g << 8) | r;
		else if (s[0] != '#')
			style->background_color = named_colour(s);
		xmlFree(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "color")) != NULL) {
		unsigned int r, g, b;
		if (s[0] == '#' && sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
			style->color = (b << 16) | (g << 8) | r;
		else if (s[0] != '#')
			style->color = named_colour(s);
		xmlFree(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "height")) != NULL) {
		float value = atof(s);
		if (value < 0 || strlen(s) == 0) {
			/* ignore negative values and height="" */
		} else if (strrchr(s, '%')) {
			/* the specification doesn't make clear what
			 * percentage heights mean, so ignore them */
		} else {
			style->height.height = CSS_HEIGHT_LENGTH;
			style->height.length.unit = CSS_UNIT_PX;
			style->height.length.value = value;
		}
		xmlFree(s);
	}

	if (strcmp((const char *) n->name, "input") == 0) {
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "size")) != NULL) {
			int size = atoi(s);
			if (0 < size) {
				char *type = (char *) xmlGetProp(n, (const xmlChar *) "type");
				style->width.width = CSS_WIDTH_LENGTH;
				if (!type || strcasecmp(type, "text") == 0 ||
						strcasecmp(type, "password") == 0)
					/* in characters for text, password, file */
					style->width.value.length.unit = CSS_UNIT_EX;
				else if (strcasecmp(type, "file") != 0)
					/* in pixels otherwise */
					style->width.value.length.unit = CSS_UNIT_PX;
				style->width.value.length.value = size;
				if (type)
					xmlFree(type);
			}
			xmlFree(s);
		}
	}

	if (strcmp((const char *) n->name, "body") == 0) {
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "text")) != NULL) {
			unsigned int r, g, b;
			if (s[0] == '#' && sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
				style->color = (b << 16) | (g << 8) | r;
			else if (s[0] != '#')
				style->color = named_colour(s);
			xmlFree(s);
		}
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "width")) != NULL) {
		float value = atof(s);
		if (value < 0 || strlen(s) == 0) {
			/* ignore negative values and width="" */
		} else if (strrchr(s, '%')) {
			style->width.width = CSS_WIDTH_PERCENT;
			style->width.value.percent = value;
		} else {
			style->width.width = CSS_WIDTH_LENGTH;
			style->width.value.length.unit = CSS_UNIT_PX;
			style->width.value.length.value = value;
		}
		xmlFree(s);
	}

	if (strcmp((const char *) n->name, "textarea") == 0) {
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "rows")) != NULL) {
			int value = atoi(s);
			if (0 < value) {
				style->height.height = CSS_HEIGHT_LENGTH;
				style->height.length.unit = CSS_UNIT_EM;
				style->height.length.value = value;
			}
			xmlFree(s);
		}
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "cols")) != NULL) {
			int value = atoi(s);
			if (0 < value) {
				style->width.width = CSS_WIDTH_LENGTH;
				style->width.value.length.unit = CSS_UNIT_EX;
				style->width.value.length.value = value;
			}
			xmlFree(s);
		}
	}

	if (strcmp((const char *) n->name, "table") == 0) {
		if ((s = (char *) xmlGetProp(n,
				(const xmlChar *) "cellspacing"))) {
			if (!strrchr(s, '%')) {		/* % not implemented */
				int value = atoi(s);
				if (0 <= value) {
					style->border_spacing.border_spacing =
						CSS_BORDER_SPACING_LENGTH;
					style->border_spacing.horz.unit =
					style->border_spacing.vert.unit =
							CSS_UNIT_PX;
					style->border_spacing.horz.value =
					style->border_spacing.vert.value =
							value;
				}
			}
		}
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "style")) != NULL) {
		struct css_style *astyle;
		astyle = css_duplicate_style(&css_empty_style);
		if (!astyle) {
			xmlFree(s);
			css_free_style(style);
			return 0;
		}
		css_parse_property_list(c, astyle, s);
		css_cascade(style, astyle);
		css_free_style(astyle);
		xmlFree(s);
	}

	return style;
}


/**
 * Apply the CSS text-transform property to given text for its ASCII chars.
 *
 * \param  s    string to transform
 * \param  len  length of s
 * \param  tt   transform type
 */

void box_text_transform(char *s, unsigned int len,
		css_text_transform tt)
{
	unsigned int i;
	if (len == 0)
		return;
	switch (tt) {
		case CSS_TEXT_TRANSFORM_UPPERCASE:
			for (i = 0; i < len; ++i)
				if (s[i] < 0x80)
					s[i] = toupper(s[i]);
			break;
		case CSS_TEXT_TRANSFORM_LOWERCASE:
			for (i = 0; i < len; ++i)
				if (s[i] < 0x80)
					s[i] = tolower(s[i]);
			break;
		case CSS_TEXT_TRANSFORM_CAPITALIZE:
			if (s[0] < 0x80)
				s[0] = toupper(s[0]);
			for (i = 1; i < len; ++i)
				if (s[i] < 0x80 && isspace(s[i - 1]))
					s[i] = toupper(s[i]);
			break;
		default:
			break;
	}
}


/*
 * Special case elements
 *
 * These functions are called by convert_xml_to_box when an element is being
 * converted, according to the entries in element_table (top of file).
 *
 * The parameters are the xmlNode, a status structure for the conversion, and
 * the style found for the element.
 *
 * If a box is created, it is returned in the result structure. The
 * convert_children field should be 1 if convert_xml_to_box should convert the
 * node's children recursively, 0 if it should ignore them (presumably they
 * have been processed in some way by the function). If box is 0, no box will
 * be created for that element, and convert_children must be 0.
 */
struct box_result box_a(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box *box;
	char *s, *s1;
	char *id = status->id;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "href")) != NULL)
		status->href = s;

	/* name and id share the same namespace */
	if ((s1 = (char *) xmlGetProp(n, (const xmlChar *) "name")) != NULL) {
		if (status->id && strcmp(status->id, s1) == 0) {
			/* both specified and they match => ok */
			id = status->id;
		}
		else if (!status->id) {
			/* only name specified */
			id = squash_whitespace(s1);
			if (!id) {
				xmlFree(s1);
				return (struct box_result) {0, false, true};
			}
		} else
			/* both specified but no match */
			id = 0;

		xmlFree(s1);
	}

	box = box_create(style, status->href, status->title, id,
			status->content->data.html.box_pool);

	if (id && id != status->id)
		free(id);

	if (!box)
		return (struct box_result) {0, false, true};

	return (struct box_result) {box, true, false};
}

struct box_result box_body(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box *box;
	status->content->data.html.background_colour = style->background_color;
	box = box_create(style, status->href, status->title, status->id,
			status->content->data.html.box_pool);
	if (!box)
		return (struct box_result) {0, false, true};
	return (struct box_result) {box, true, false};
}

struct box_result box_br(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box *box;
	box = box_create(style, status->href, status->title, status->id,
			status->content->data.html.box_pool);
	if (!box)
		return (struct box_result) {0, false, true};
	box->type = BOX_BR;
	return (struct box_result) {box, false, false};
}

struct box_result box_image(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box *box;
	char *s, *url, *s1, *map;
	xmlChar *s2;
	url_func_result res;

	box = box_create(style, status->href, status->title, status->id,
			status->content->data.html.box_pool);
	if (!box)
		return (struct box_result) {0, false, true};

	/* handle alt text */
	if ((s2 = xmlGetProp(n, (const xmlChar *) "alt")) != NULL) {
		box->text = squash_whitespace(s2);
		xmlFree(s2);
		if (!box->text)
			return (struct box_result) {0, false, true};
		box->length = strlen(box->text);
		box->font = nsfont_open(status->content->data.html.fonts, style);
	}

	/* imagemap associated with this image */
	if ((map = xmlGetProp(n, (const xmlChar *) "usemap")) != NULL) {
		if (map[0] == '#')
			box->usemap = strdup(map + 1);
		else
			box->usemap = strdup(map);
		xmlFree(map);
		if (!box->usemap) {
			free(box->text);
			return (struct box_result) {0, false, true};
		}
	}

	/* img without src is an error */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "src")) == NULL)
		return (struct box_result) {box, false, false};

	/* remove leading and trailing whitespace */
	s1 = strip(s);
	res = url_join(s1, status->content->data.html.base_url, &url);
	xmlFree(s);
	if (res == URL_FUNC_NOMEM) {
		free(box->text);
		return (struct box_result) {0, false, true};
	} else if (res == URL_FUNC_FAILED) {
		return (struct box_result) {box, false, false};
	}

	if (strcmp(url, status->content->data.html.base_url) == 0)
		/* if url is equivalent to the parent's url,
		 * we've got infinite inclusion: ignore */
		return (struct box_result) {box, false, false};

	/* start fetch */
	if (!html_fetch_object(status->content, url, box, image_types,
			status->content->available_width, 1000, false))
		return (struct box_result) {0, false, true};

	return (struct box_result) {box, false, false};
}

struct box_result box_form(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	char *action, *method, *enctype;
	form_method fmethod;
	struct box *box;
	struct form *form;

	box = box_create(style, status->href, status->title, status->id,
			status->content->data.html.box_pool);
	if (!box)
		return (struct box_result) {0, false, true};

	if (!(action = (char *) xmlGetProp(n, (const xmlChar *) "action"))) {
		/* the action attribute is required */
		return (struct box_result) {box, true, false};
	}

	fmethod = method_GET;
	if ((method = (char *) xmlGetProp(n, (const xmlChar *) "method"))) {
		if (strcasecmp(method, "post") == 0) {
			fmethod = method_POST_URLENC;
			if ((enctype = (char *) xmlGetProp(n,
					(const xmlChar *) "enctype"))) {
				if (strcasecmp(enctype,
						"multipart/form-data") == 0)
					fmethod = method_POST_MULTIPART;
				xmlFree(enctype);
			}
		}
		xmlFree(method);
	}

	status->current_form = form = form_new(action, fmethod);
	if (!form) {
		xmlFree(action);
		return (struct box_result) {0, false, true};
	}

	return (struct box_result) {box, true, false};
}

struct box_result box_textarea(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	/* A textarea is an INLINE_BLOCK containing a single INLINE_CONTAINER,
	 * which contains the text as runs of INLINE separated by BR. There is
	 * at least one INLINE. The first and last boxes are INLINE.
	 * Consecutive BR may not be present. These constraints are satisfied
	 * by using a 0-length INLINE for blank lines. */

	xmlChar *content, *current;
	struct box *box, *inline_container, *inline_box, *br_box;
	char *s;
	size_t len;

	box = box_create(style, NULL, 0, status->id,
			status->content->data.html.box_pool);
	if (!box)
		return (struct box_result) {0, false, true};
	box->type = BOX_INLINE_BLOCK;
	box->gadget = form_new_control(GADGET_TEXTAREA);
	if (!box->gadget)
		return (struct box_result) {0, false, true};
	box->gadget->box = box;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name")) != NULL) {
		box->gadget->name = strdup(s);
		xmlFree(s);
		if (!box->gadget->name)
			return (struct box_result) {0, false, true};
	}

	inline_container = box_create(0, 0, 0, 0,
			status->content->data.html.box_pool);
	if (!inline_container)
		return (struct box_result) {0, false, true};
	inline_container->type = BOX_INLINE_CONTAINER;
	box_add_child(box, inline_container);

	current = content = xmlNodeGetContent(n);
	while (1) {
		/* BOX_INLINE */
		len = strcspn(current, "\r\n");
		s = strndup(current, len);
		if (!s) {
			box_free(box);
			xmlFree(content);
			return (struct box_result) {NULL, false, false};
		}

		inline_box = box_create(style, 0, 0, 0,
				status->content->data.html.box_pool);
		if (!inline_box)
			return (struct box_result) {0, false, true};
		inline_box->type = BOX_INLINE;
		inline_box->style_clone = 1;
		inline_box->text = s;
		inline_box->length = len;
		inline_box->font = nsfont_open(status->content->data.html.fonts,
				style);
		box_add_child(inline_container, inline_box);

		current += len;
		if (current[0] == 0)
			/* finished */
			break;

		/* BOX_BR */
		br_box = box_create(style, 0, 0, 0,
				status->content->data.html.box_pool);
		if (!br_box)
			return (struct box_result) {0, false, true};
		br_box->type = BOX_BR;
		br_box->style_clone = 1;
		box_add_child(inline_container, br_box);

		if (current[0] == '\r' && current[1] == '\n')
			current += 2;
		else
			current++;
	}
	xmlFree(content);

	if (status->current_form)
		form_add_control(status->current_form, box->gadget);

	return (struct box_result) {box, false, false};
}

struct box_result box_select(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box *box;
	struct box *inline_container;
	struct box *inline_box;
	struct form_control *gadget;
	char* s;
	xmlNode *c, *c2;

	gadget = form_new_control(GADGET_SELECT);
	if (!gadget)
		return (struct box_result) {0, false, true};

	gadget->data.select.multiple = false;
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "multiple")) != NULL) {
		gadget->data.select.multiple = true;
		xmlFree(s);
	}

	gadget->data.select.items = NULL;
	gadget->data.select.last_item = NULL;
	gadget->data.select.num_items = 0;
	gadget->data.select.num_selected = 0;

	for (c = n->children; c; c = c->next) {
		if (strcmp((const char *) c->name, "option") == 0) {
			if (!box_select_add_option(gadget, c))
				goto no_memory;
		} else if (strcmp((const char *) c->name, "optgroup") == 0) {
			for (c2 = c->children; c2; c2 = c2->next) {
				if (strcmp((const char *) c2->name,
						"option") == 0) {
					if (!box_select_add_option(gadget, c2))
						goto no_memory;
				}
			}
		}
	}

	if (gadget->data.select.num_items == 0) {
		/* no options: ignore entire select */
		form_free_control(gadget);
		return (struct box_result) {0, false, false};
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name")) != NULL) {
		gadget->name = strdup(s);
		xmlFree(s);
		if (!gadget->name)
			goto no_memory;
	}

	box = box_create(style, NULL, 0, status->id,
			status->content->data.html.box_pool);
	if (!box)
		goto no_memory;
	box->type = BOX_INLINE_BLOCK;
	box->gadget = gadget;
	gadget->box = box;

	inline_container = box_create(0, 0, 0, 0,
			status->content->data.html.box_pool);
	if (!inline_container)
		goto no_memory;
	inline_container->type = BOX_INLINE_CONTAINER;
	inline_box = box_create(style, 0, 0, 0,
			status->content->data.html.box_pool);
	if (!inline_box)
		goto no_memory;
	inline_box->type = BOX_INLINE;
	inline_box->style_clone = 1;
	box_add_child(inline_container, inline_box);
	box_add_child(box, inline_container);

	if (!gadget->data.select.multiple &&
			gadget->data.select.num_selected == 0) {
		gadget->data.select.current = gadget->data.select.items;
		gadget->data.select.current->initial_selected =
			gadget->data.select.current->selected = true;
		gadget->data.select.num_selected = 1;
	}

	if (gadget->data.select.num_selected == 0)
		inline_box->text = strdup(messages_get("Form_None"));
	else if (gadget->data.select.num_selected == 1)
		inline_box->text = strdup(gadget->data.select.current->text);
	else
		inline_box->text = strdup(messages_get("Form_Many"));
	if (!inline_box->text)
		goto no_memory;

	inline_box->length = strlen(inline_box->text);
	inline_box->font = nsfont_open(status->content->data.html.fonts, style);

	if (status->current_form)
		form_add_control(status->current_form, gadget);

	return (struct box_result) {box, false, false};

no_memory:
	form_free_control(gadget);
	return (struct box_result) {0, false, true};
}


/**
 * Add an option to a form select control.
 *
 * \param  control  select containing the option
 * \param  n        xml element node for <option>
 * \return  true on success, false on memory exhaustion
 */

bool box_select_add_option(struct form_control *control, xmlNode *n)
{
	char *value = 0;
	char *text = 0;
	bool selected;
	xmlChar *content;
	xmlChar *s;

	content = xmlNodeGetContent(n);
	if (!content)
		goto no_memory;
	text = squash_whitespace(content);
	xmlFree(content);
	if (!text)
		goto no_memory;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
		value = strdup(s);
		xmlFree(s);
	} else
		value = strdup(text);
	if (!value)
		goto no_memory;

	selected = xmlHasProp(n, (const xmlChar *) "selected");

	if (!form_add_option(control, value, text, selected))
		goto no_memory;

	return true;

no_memory:
	free(value);
	free(text);
	return false;
}


struct box_result box_input(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box* box = NULL;
	struct form_control *gadget = NULL;
	char *s, *type, *url;
	url_func_result res;

	type = (char *) xmlGetProp(n, (const xmlChar *) "type");

	if (type && strcasecmp(type, "password") == 0) {
		box = box_input_text(n, status, style, true);
		if (!box)
			goto no_memory;
		gadget = box->gadget;
		gadget->box = box;

	} else if (type && strcasecmp(type, "file") == 0) {
		box = box_create(style, NULL, 0, status->id,
				status->content->data.html.box_pool);
		if (!box)
			goto no_memory;
		box->type = BOX_INLINE_BLOCK;
		box->gadget = gadget = form_new_control(GADGET_FILE);
		if (!gadget)
			goto no_memory;
		gadget->box = box;
		box->font = nsfont_open(status->content->data.html.fonts, style);

	} else if (type && strcasecmp(type, "hidden") == 0) {
		/* no box for hidden inputs */
		gadget = form_new_control(GADGET_HIDDEN);
		if (!gadget)
			goto no_memory;

		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
			gadget->value = strdup(s);
			xmlFree(s);
			if (!gadget->value)
				goto no_memory;
			gadget->length = strlen(gadget->value);
		}

	} else if (type && (strcasecmp(type, "checkbox") == 0 ||
			strcasecmp(type, "radio") == 0)) {
		box = box_create(style, NULL, 0, status->id,
				status->content->data.html.box_pool);
		if (!box)
			goto no_memory;
		box->gadget = gadget = form_new_control(type[0] == 'c' ||
				type[0] == 'C' ? GADGET_CHECKBOX :
				GADGET_RADIO);
		if (!gadget)
			goto no_memory;
		gadget->box = box;

		gadget->selected = xmlHasProp(n, (const xmlChar *) "checked");

		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value")) != NULL) {
			gadget->value = strdup(s);
			xmlFree(s);
			if (!gadget->value)
				goto no_memory;
			gadget->length = strlen(gadget->value);
		}

	} else if (type && (strcasecmp(type, "submit") == 0 ||
			strcasecmp(type, "reset") == 0)) {
		struct box_result result = box_button(n, status, style);
		struct box *inline_container, *inline_box;
		if (result.memory_error)
			goto no_memory;
		box = result.box;
		inline_container = box_create(0, 0, 0, 0,
				status->content->data.html.box_pool);
		if (!inline_container)
			goto no_memory;
		inline_container->type = BOX_INLINE_CONTAINER;
		inline_box = box_create(style, 0, 0, 0,
				status->content->data.html.box_pool);
		if (!inline_box)
			goto no_memory;
		inline_box->type = BOX_INLINE;
		inline_box->style_clone = 1;
		if (box->gadget->value != NULL)
			inline_box->text = strdup(box->gadget->value);
		else if (box->gadget->type == GADGET_SUBMIT)
			inline_box->text = strdup(messages_get("Form_Submit"));
		else
			inline_box->text = strdup(messages_get("Form_Reset"));
		if (!inline_box->text)
			goto no_memory;
		inline_box->length = strlen(inline_box->text);
		inline_box->font = nsfont_open(status->content->data.html.fonts, style);
		box_add_child(inline_container, inline_box);
		box_add_child(box, inline_container);

	} else if (type && strcasecmp(type, "button") == 0) {
		struct box_result result = box_button(n, status, style);
		struct box *inline_container, *inline_box;
		if (result.memory_error)
			goto no_memory;
		box = result.box;
		inline_container = box_create(0, 0, 0, 0,
				status->content->data.html.box_pool);
		if (!inline_container)
			goto no_memory;
		inline_container->type = BOX_INLINE_CONTAINER;
		inline_box = box_create(style, 0, 0, 0,
				status->content->data.html.box_pool);
		if (!inline_box)
			goto no_memory;
		inline_box->type = BOX_INLINE;
		inline_box->style_clone = 1;
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value")))
			inline_box->text = strdup(s);
		else
			inline_box->text = strdup("Button");
		if (!inline_box->text)
			goto no_memory;
		inline_box->length = strlen(inline_box->text);
		inline_box->font = nsfont_open(status->content->data.html.fonts, style);
		box_add_child(inline_container, inline_box);
		box_add_child(box, inline_container);

	} else if (type && strcasecmp(type, "image") == 0) {
		box = box_create(style, NULL, 0, status->id,
				status->content->data.html.box_pool);
		if (!box)
			goto no_memory;
		box->gadget = gadget = form_new_control(GADGET_IMAGE);
		if (!gadget)
			goto no_memory;
		gadget->box = box;
		gadget->type = GADGET_IMAGE;
		if ((s = (char *) xmlGetProp(n, (const xmlChar*) "src")) != NULL) {
			res = url_join(s, status->content->data.html.base_url, &url);
			/* if url is equivalent to the parent's url,
			 * we've got infinite inclusion. stop it here.
			 * also bail if url_join failed.
			 */
			if (res == URL_FUNC_OK &&
					strcasecmp(url, status->content->data.html.base_url) != 0)
				if (!html_fetch_object(status->content, url, box,
						image_types,
						status->content->available_width,
						1000, false))
					goto no_memory;
			xmlFree(s);
		}

	} else {
		/* the default type is "text" */
		box = box_input_text(n, status, style, false);
		if (!box)
			goto no_memory;
		gadget = box->gadget;
		gadget->box = box;
	}

	if (type != 0)
		xmlFree(type);

	if (gadget != 0) {
		if (status->current_form)
			form_add_control(status->current_form, gadget);
		else
			gadget->form = 0;
		s = (char *) xmlGetProp(n, (const xmlChar *) "name");
		if (s) {
			gadget->name = strdup(s);
			xmlFree(s);
			if (!gadget->name)
				goto no_memory;
		}
	}

	return (struct box_result) {box, false, false};

no_memory:
	if (type)
		xmlFree(type);
	if (gadget)
		form_free_control(gadget);

	return (struct box_result) {0, false, true};
}

struct box *box_input_text(xmlNode *n, struct box_status *status,
		struct css_style *style, bool password)
{
	char *s;
	struct box *box = box_create(style, 0, 0, status->id,
			status->content->data.html.box_pool);
	struct box *inline_container, *inline_box;

	if (!box)
		return 0;

	box->type = BOX_INLINE_BLOCK;

	box->gadget = form_new_control((password) ? GADGET_PASSWORD : GADGET_TEXTBOX);
	if (!box->gadget)
		return 0;
	box->gadget->box = box;

	box->gadget->maxlength = 100;
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "maxlength")) != NULL) {
		box->gadget->maxlength = atoi(s);
		xmlFree(s);
	}

	s = (char *) xmlGetProp(n, (const xmlChar *) "value");
	box->gadget->value = strdup((s != NULL) ? s : "");
	box->gadget->initial_value = strdup((box->gadget->value != NULL) ? box->gadget->value : "");
	if (s)
		xmlFree(s);
	if (box->gadget->value == NULL || box->gadget->initial_value == NULL) {
		box_free(box);
		return NULL;
	}
	box->gadget->length = strlen(box->gadget->value);

	inline_container = box_create(0, 0, 0, 0,
			status->content->data.html.box_pool);
	if (!inline_container)
		return 0;
	inline_container->type = BOX_INLINE_CONTAINER;
	inline_box = box_create(style, 0, 0, 0,
			status->content->data.html.box_pool);
	if (!inline_box)
		return 0;
	inline_box->type = BOX_INLINE;
	inline_box->style_clone = 1;
	if (password) {
		inline_box->length = strlen(box->gadget->value);
		inline_box->text = malloc(inline_box->length + 1);
		if (!inline_box->text)
			return 0;
		memset(inline_box->text, '*', inline_box->length);
		inline_box->text[inline_box->length] = '\0';
	} else {
		/* replace spaces/TABs with hard spaces to prevent line wrapping */
		inline_box->text = cnv_space2nbsp(box->gadget->value);
		if (!inline_box->text)
			return 0;
		inline_box->length = strlen(inline_box->text);
	}
	inline_box->font = nsfont_open(status->content->data.html.fonts, style);
	box_add_child(inline_container, inline_box);
	box_add_child(box, inline_container);

	return box;
}

struct box_result box_button(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	xmlChar *s;
	char *type = (char *) xmlGetProp(n, (const xmlChar *) "type");
	struct box *box = box_create(style, 0, 0, status->id,
			status->content->data.html.box_pool);
	if (!box)
		return (struct box_result) {0, false, true};
	box->type = BOX_INLINE_BLOCK;

	if (!type || strcasecmp(type, "submit") == 0) {
		box->gadget = form_new_control(GADGET_SUBMIT);
	} else if (strcasecmp(type, "reset") == 0) {
		box->gadget = form_new_control(GADGET_RESET);
	} else {
		/* type="button" or unknown: just render the contents */
		xmlFree(type);
		return (struct box_result) {box, true, false};
	}

	if (type)
		xmlFree(type);

	if (!box->gadget) {
		box_free_box(box);
		return (struct box_result) {0, false, true};
	}

	if (status->current_form)
		form_add_control(status->current_form, box->gadget);
	else
		box->gadget->form = 0;
	box->gadget->box = box;
	if ((s = xmlGetProp(n, (const xmlChar *) "name")) != NULL) {
		box->gadget->name = strdup((char *) s);
		xmlFree(s);
		if (!box->gadget->name) {
			box_free_box(box);
			return (struct box_result) {0, false, true};
		}
	}
	if ((s = xmlGetProp(n, (const xmlChar *) "value")) != NULL) {
		box->gadget->value = strdup((char *) s);
		xmlFree(s);
		if (!box->gadget->value) {
			box_free_box(box);
			return (struct box_result) {0, false, true};
		}
	}

	return (struct box_result) {box, true, false};
}


/**
 * Print a box tree to stderr.
 */

void box_dump(struct box * box, unsigned int depth)
{
	unsigned int i;
	struct box * c;

	for (i = 0; i < depth; i++)
		fprintf(stderr, "  ");

	fprintf(stderr, "%p ", box);
	fprintf(stderr, "x%i y%i w%i h%i ", box->x, box->y, box->width, box->height);
	if ((unsigned long)box->max_width != UNKNOWN_MAX_WIDTH)
		fprintf(stderr, "min%i max%i ", box->min_width, box->max_width);
	fprintf(stderr, "(%i %i %i %i) ",
			box->descendant_x0, box->descendant_y0,
			box->descendant_x1, box->descendant_y1);

	switch (box->type) {
		case BOX_BLOCK:            fprintf(stderr, "BLOCK "); break;
		case BOX_INLINE_CONTAINER: fprintf(stderr, "INLINE_CONTAINER "); break;
		case BOX_INLINE:           fprintf(stderr, "INLINE "); break;
		case BOX_INLINE_BLOCK:     fprintf(stderr, "INLINE_BLOCK "); break;
		case BOX_TABLE:            fprintf(stderr, "TABLE [columns %i] ",
						   box->columns); break;
		case BOX_TABLE_ROW:        fprintf(stderr, "TABLE_ROW "); break;
		case BOX_TABLE_CELL:       fprintf(stderr, "TABLE_CELL [columns %i, "
						   "start %i, rows %i] ", box->columns,
						   box->start_column, box->rows); break;
		case BOX_TABLE_ROW_GROUP:  fprintf(stderr, "TABLE_ROW_GROUP "); break;
		case BOX_FLOAT_LEFT:       fprintf(stderr, "FLOAT_LEFT "); break;
		case BOX_FLOAT_RIGHT:      fprintf(stderr, "FLOAT_RIGHT "); break;
		case BOX_BR:               fprintf(stderr, "BR "); break;
		default:                   fprintf(stderr, "Unknown box type ");
	}
	if (box->text)
		fprintf(stderr, "'%.*s' ", (int) box->length, box->text);
	if (box->space)
		fprintf(stderr, "space ");
	if (box->object)
		fprintf(stderr, "(object '%s') ", box->object->url);
	if (box->style)
		css_dump_style(box->style);
	if (box->href != 0)
		fprintf(stderr, " -> '%s'", box->href);
	if (box->title != 0)
		fprintf(stderr, " [%s]", box->title);
	if (box->id != 0)
		fprintf(stderr, " <%s>", box->id);
	if (box->float_children)
		fprintf(stderr, " float_children %p", box->float_children);
	if (box->next_float)
		fprintf(stderr, " next_float %p", box->next_float);
	fprintf(stderr, "\n");

	for (c = box->children; c != 0; c = c->next)
		box_dump(c, depth + 1);
}


/**
 * ensure the box tree is correctly nested
 *
 * \return  true on success, false on memory exhaustion
 *
 * parent		permitted child nodes
 * BLOCK, INLINE_BLOCK	BLOCK, INLINE_CONTAINER, TABLE
 * INLINE_CONTAINER	INLINE, INLINE_BLOCK, FLOAT_LEFT, FLOAT_RIGHT, BR
 * INLINE		none
 * TABLE		at least 1 TABLE_ROW_GROUP
 * TABLE_ROW_GROUP	at least 1 TABLE_ROW
 * TABLE_ROW		at least 1 TABLE_CELL
 * TABLE_CELL		BLOCK, INLINE_CONTAINER, TABLE (same as BLOCK)
 * FLOAT_(LEFT|RIGHT)	exactly 1 BLOCK or TABLE
 */

bool box_normalise_block(struct box *block, pool box_pool)
{
	struct box *child;
	struct box *next_child;
	struct box *table;
	struct css_style *style;

	assert(block != 0);
	LOG(("block %p, block->type %u", block, block->type));
	assert(block->type == BOX_BLOCK || block->type == BOX_INLINE_BLOCK ||
			block->type == BOX_TABLE_CELL);
	gui_multitask();

	for (child = block->children; child != 0; child = next_child) {
		LOG(("child %p, child->type = %d", child, child->type));
		next_child = child->next;	/* child may be destroyed */
		switch (child->type) {
			case BOX_BLOCK:
				/* ok */
				if (!box_normalise_block(child, box_pool))
					return false;
				break;
			case BOX_INLINE_CONTAINER:
				if (!box_normalise_inline_container(child,
						box_pool))
					return false;
				break;
			case BOX_TABLE:
				if (!box_normalise_table(child, box_pool))
					return false;
				break;
			case BOX_INLINE:
			case BOX_INLINE_BLOCK:
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
			case BOX_BR:
				/* should have been wrapped in inline
				   container by convert_xml_to_box() */
				assert(0);
				break;
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_ROW:
			case BOX_TABLE_CELL:
				/* insert implied table */
				style = css_duplicate_style(block->style);
				if (!style)
					return false;
				css_cascade(style, &css_blank_style);
				table = box_create(style, block->href, 0, 0, box_pool);
				if (!table) {
					css_free_style(style);
					return false;
				}
				table->type = BOX_TABLE;
				if (child->prev == 0)
					block->children = table;
				else
					child->prev->next = table;
				table->prev = child->prev;
				while (child != 0 && (
						child->type == BOX_TABLE_ROW_GROUP ||
						child->type == BOX_TABLE_ROW ||
						child->type == BOX_TABLE_CELL)) {
					box_add_child(table, child);
					next_child = child->next;
					child->next = 0;
					child = next_child;
				}
				table->last->next = 0;
				table->next = next_child = child;
				if (table->next)
					table->next->prev = table;
				table->parent = block;
				if (!box_normalise_table(table, box_pool))
					return false;
				break;
			default:
				assert(0);
		}
	}

	return true;
}


void box_normalise_table_spans( struct box *table )
{
	struct box *table_row_group;
	struct box *table_row;
	struct box *table_cell;
	unsigned int last_column;
	unsigned int max_extra = 0;
	bool extra;
	bool force = false;
	unsigned int rows_left = table->rows;

	/* Scan table filling in table the width and height of table cells for
		cells with colspan = 0 or rowspan = 0. Ignore the colspan and
		rowspan of any cells that that follow an colspan = 0 */
	for (table_row_group = table->children; table_row_group != NULL;
				table_row_group = table_row_group->next) {
		for (table_row = table_row_group->children; NULL != table_row;
				table_row = table_row->next){
			last_column = 0;
			extra = false;
			for (table_cell = table_row->children; NULL != table_cell;
					table_cell = table_cell->next) {
				/* We hae reached the end of the row, and have passed
					a cell with colspan = 0 so ignore col and row spans */
				if ( force || extra || ( table_cell->start_column + 1 <=
											last_column )) {
					extra = true;
					table_cell->columns = 1;
					table_cell->rows = 1;
					if ( table_cell->start_column <= max_extra ) {
						max_extra = table_cell->start_column + 1;
					}
					table_cell->start_column += table->columns;
				} else {
					/* Fill out the number of columns or the number of rows
						if necessary */
					if ( 0 == table_cell->columns ) {
						table_cell->columns = table->columns -
								table_cell->start_column;
						if (( 0 == table_cell->start_column ) &&
								( 0 == table_cell->rows )) {
							force = true;
						}
					}
					assert( 0 != table_cell->columns );
					if ( 0 == table_cell->rows ) {
						table_cell->rows = rows_left;
					}
					assert( 0 != table_cell->rows );
					last_column = table_cell->start_column + 1;
				}
			}
			rows_left--;
		}
	}
	table->columns +=  max_extra;
}

bool box_normalise_table(struct box *table, pool box_pool)
{
	struct box *child;
	struct box *next_child;
	struct box *row_group;
	struct css_style *style;
	struct columns col_info;

	assert(table != 0);
	assert(table->type == BOX_TABLE);
	LOG(("table %p", table));
	col_info.num_columns = 1;
	col_info.current_column = 0;
	col_info.spans = malloc(2 * sizeof *col_info.spans);
	if (!col_info.spans)
		return false;
	col_info.spans[0].row_span = col_info.spans[1].row_span = 0;
	col_info.spans[0].auto_row = col_info.spans[0].auto_column =
		col_info.spans[1].auto_row = col_info.spans[1].auto_column = false;
	col_info.num_rows = col_info.extra_columns = 0;
	col_info.extra = false;

	for (child = table->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
			case BOX_TABLE_ROW_GROUP:
				/* ok */
				if (!box_normalise_table_row_group(child,
						&col_info, box_pool)) {
					free(col_info.spans);
					return false;
				}
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW:
			case BOX_TABLE_CELL:
				/* insert implied table row group */
				assert(table->style != NULL);
				style = css_duplicate_style(table->style);
				if (!style) {
					free(col_info.spans);
					return false;
				}
				css_cascade(style, &css_blank_style);
				row_group = box_create(style, table->href, 0,
						0, box_pool);
				if (!row_group) {
					free(col_info.spans);
					css_free_style(style);
					return false;
				}
				row_group->type = BOX_TABLE_ROW_GROUP;
				if (child->prev == 0)
					table->children = row_group;
				else
					child->prev->next = row_group;
				row_group->prev = child->prev;
				while (child != 0 && (
						child->type == BOX_BLOCK ||
						child->type == BOX_INLINE_CONTAINER ||
						child->type == BOX_TABLE ||
						child->type == BOX_TABLE_ROW ||
						child->type == BOX_TABLE_CELL)) {
					box_add_child(row_group, child);
					next_child = child->next;
					child->next = 0;
					child = next_child;
				}
				row_group->last->next = 0;
				row_group->next = next_child = child;
				if (row_group->next)
					row_group->next->prev = row_group;
				row_group->parent = table;
				if (!box_normalise_table_row_group(row_group,
						&col_info, box_pool)) {
					free(col_info.spans);
					return false;
				}
				break;
			case BOX_INLINE:
			case BOX_INLINE_BLOCK:
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
			case BOX_BR:
				/* should have been wrapped in inline
				   container by convert_xml_to_box() */
				assert(0);
				break;
			default:
				fprintf(stderr, "%i\n", child->type);
				assert(0);
		}
	}

	table->columns = col_info.num_columns;
	table->rows = col_info.num_rows;
	free(col_info.spans);

	box_normalise_table_spans( table );

	if (table->children == 0) {
		LOG(("table->children == 0, removing"));
		if (table->prev == 0)
			table->parent->children = table->next;
		else
			table->prev->next = table->next;
		if (table->next != 0)
			table->next->prev = table->prev;
		box_free(table);
	}

	LOG(("table %p done", table));

	return true;
}


bool box_normalise_table_row_group(struct box *row_group,
		struct columns *col_info,
		pool box_pool)
{
	struct box *child;
	struct box *next_child;
	struct box *row;
	struct css_style *style;

	assert(row_group != 0);
	assert(row_group->type == BOX_TABLE_ROW_GROUP);
	LOG(("row_group %p", row_group));

	for (child = row_group->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
			case BOX_TABLE_ROW:
				/* ok */
				if (!box_normalise_table_row(child, col_info,
						box_pool))
					return false;
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_CELL:
				/* insert implied table row */
				assert(row_group->style != NULL);
				style = css_duplicate_style(row_group->style);
				if (!style)
					return false;
				css_cascade(style, &css_blank_style);
				row = box_create(style, row_group->href, 0,
						0, box_pool);
				if (!row) {
					css_free_style(style);
					return false;
				}
				row->type = BOX_TABLE_ROW;
				if (child->prev == 0)
					row_group->children = row;
				else
					child->prev->next = row;
				row->prev = child->prev;
				while (child != 0 && (
						child->type == BOX_BLOCK ||
						child->type == BOX_INLINE_CONTAINER ||
						child->type == BOX_TABLE ||
						child->type == BOX_TABLE_ROW_GROUP ||
						child->type == BOX_TABLE_CELL)) {
					box_add_child(row, child);
					next_child = child->next;
					child->next = 0;
					child = next_child;
				}
				row->last->next = 0;
				row->next = next_child = child;
				if (row->next)
					row->next->prev = row;
				row->parent = row_group;
				if (!box_normalise_table_row(row, col_info,
						box_pool))
					return false;
				break;
			case BOX_INLINE:
			case BOX_INLINE_BLOCK:
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
			case BOX_BR:
				/* should have been wrapped in inline
				   container by convert_xml_to_box() */
				assert(0);
				break;
			default:
				assert(0);
		}
	}

	if (row_group->children == 0) {
		LOG(("row_group->children == 0, removing"));
		if (row_group->prev == 0)
			row_group->parent->children = row_group->next;
		else
			row_group->prev->next = row_group->next;
		if (row_group->next != 0)
			row_group->next->prev = row_group->prev;
		box_free(row_group);
	}

	LOG(("row_group %p done", row_group));

	return true;
}

/**
 * \return  true on success, false on memory exhaustion
 */

bool calculate_table_row(struct columns *col_info,
		unsigned int col_span, unsigned int row_span,
		unsigned int *start_column)
{
	unsigned int cell_start_col;
	unsigned int cell_end_col;
	unsigned int i;
	struct span_info *spans;

	if ( !col_info->extra ) {
		/* skip columns with cells spanning from above */
		while (( col_info->spans[col_info->current_column].row_span != 0 ) &&
		       ( !col_info->spans[col_info->current_column].auto_column )) {
			col_info->current_column++;
		}
		if ( col_info->spans[col_info->current_column].auto_column ) {
			col_info->extra = true;
			col_info->current_column = 0;
		}
	}

	cell_start_col = col_info->current_column;

	/* If the current table cell follows a cell with colspan=0,
	   ignore both colspan and rowspan just assume it is a standard
	   size cell */
	if ( col_info->extra ) {
		col_info->current_column++;
		col_info->extra_columns = col_info->current_column;
	} else {
		/* If span to end of table, assume spaning single column
			at the moment */
		cell_end_col = cell_start_col + (( 0 == col_span ) ? 1 : col_span );

		if ( col_info->num_columns < cell_end_col ) {
			spans = realloc(col_info->spans,
					sizeof *spans * (cell_end_col + 1));
			if (!spans)
				return false;
			col_info->spans = spans;
			col_info->num_columns = cell_end_col;

			/* Mark new final column as sentinal */
			col_info->spans[ cell_end_col ].row_span = 0;
			col_info->spans[ cell_end_col ].auto_row =
				col_info->spans[ cell_end_col ].auto_column =
				false;
		}

		if ( 0 == col_span ) {
			col_info->spans[ cell_start_col ].auto_column = true;
			col_info->spans[ cell_start_col ].row_span = row_span;
			col_info->spans[ cell_start_col ].auto_row = ( 0 == row_span );
		} else {
			for (i = cell_start_col; i < cell_end_col; i++) {
				col_info->spans[ i ].row_span = ( 0 == row_span )  ?
					1 : row_span;
				col_info->spans[ i ].auto_row = ( 0 == row_span );
				col_info->spans[ i ].auto_column = false;
			}
		}
		if ( 0 == col_span ) {
			col_info->spans[ cell_end_col ].auto_column = true;
		}
		col_info->current_column = cell_end_col;
	}

	*start_column = cell_start_col;
	return true;
}

bool box_normalise_table_row(struct box *row,
		struct columns *col_info,
		pool box_pool)
{
	struct box *child;
	struct box *next_child;
	struct box *cell;
	struct css_style *style;
	unsigned int i;

	assert(row != 0);
	assert(row->type == BOX_TABLE_ROW);
	LOG(("row %p", row));

	for (child = row->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
			case BOX_TABLE_CELL:
				/* ok */
				if (!box_normalise_block(child, box_pool))
					return false;
				cell = child;
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_ROW:
				/* insert implied table cell */
				assert(row->style != NULL);
				style = css_duplicate_style(row->style);
				if (!style)
					return false;
				css_cascade(style, &css_blank_style);
				cell = box_create(style, row->href, 0, 0,
						box_pool);
				if (!cell) {
					css_free_style(style);
					return false;
				}
				cell->type = BOX_TABLE_CELL;
				if (child->prev == 0)
					row->children = cell;
				else
					child->prev->next = cell;
				cell->prev = child->prev;
				while (child != 0 && (
						child->type == BOX_BLOCK ||
						child->type == BOX_INLINE_CONTAINER ||
						child->type == BOX_TABLE ||
						child->type == BOX_TABLE_ROW_GROUP ||
						child->type == BOX_TABLE_ROW)) {
					box_add_child(cell, child);
					next_child = child->next;
					child->next = 0;
					child = next_child;
				}
				cell->last->next = 0;
				cell->next = next_child = child;
				if (cell->next)
					cell->next->prev = cell;
				cell->parent = row;
				if (!box_normalise_block(cell, box_pool))
					return false;
				break;
			case BOX_INLINE:
			case BOX_INLINE_BLOCK:
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
			case BOX_BR:
				/* should have been wrapped in inline
				   container by convert_xml_to_box() */
				assert(0);
				break;
			default:
				assert(0);
		}

		if (!calculate_table_row(col_info, cell->columns, cell->rows,
				&cell->start_column))
			return false;
	}

	for ( i = 0; i < col_info->num_columns; i++ ) {
		if (( col_info->spans[i].row_span != 0 ) && ( !col_info->spans[i].auto_row )) {
			col_info->spans[i].row_span--;
			if (( col_info->spans[i].auto_column ) && ( 0 == col_info->spans[i].row_span )) {
				col_info->spans[i].auto_column = false;
			}
		}
	}
	col_info->current_column = 0;
	col_info->extra = false;

	if (row->children == 0) {
		LOG(("row->children == 0, removing"));
		if (row->prev == 0)
			row->parent->children = row->next;
		else
			row->prev->next = row->next;
		if (row->next != 0)
			row->next->prev = row->prev;
		box_free(row);
	} else {
		col_info->num_rows++;
	}

	LOG(("row %p done", row));

	return true;
}


bool box_normalise_inline_container(struct box *cont, pool box_pool)
{
	struct box *child;
	struct box *next_child;

	assert(cont != 0);
	assert(cont->type == BOX_INLINE_CONTAINER);
	LOG(("cont %p", cont));

	for (child = cont->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
			case BOX_INLINE:
			case BOX_BR:
				/* ok */
				break;
			case BOX_INLINE_BLOCK:
				/* ok */
				if (!box_normalise_block(child, box_pool))
					return false;
				break;
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				/* ok */
				assert(child->children != 0);
				switch (child->children->type) {
					case BOX_BLOCK:
						if (!box_normalise_block(
								child->children,
								box_pool))
							return false;
						break;
					case BOX_TABLE:
						if (!box_normalise_table(
								child->children,
								box_pool))
							return false;
						break;
					default:
						assert(0);
				}
				if (child->children == 0) {
					/* the child has destroyed itself: remove float */
					if (child->prev == 0)
						child->parent->children = child->next;
					else
						child->prev->next = child->next;
					if (child->next != 0)
						child->next->prev = child->prev;
					box_free(child);
				}
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_ROW:
			case BOX_TABLE_CELL:
			default:
				assert(0);
		}
	}
	LOG(("cont %p done", cont));

	return true;
}


/**
 * Free a box tree recursively.
 */

void box_free(struct box *box)
{
	struct box *child, *next;

	/* free children first */
	for (child = box->children; child; child = next) {
		next = child->next;
		box_free(child);
	}

	/* last this box */
	box_free_box(box);
}


/**
 * Free a single box structure.
 */

void box_free_box(struct box *box)
{
	if (!box->clone) {
		if (box->gadget)
			form_free_control(box->gadget);
		free(box->href);
		free(box->title);
		free(box->col);
		if (!box->style_clone && box->style)
			css_free_style(box->style);
	}

	free(box->usemap);
	free(box->text);
	free(box->id);
	/* TODO: free object_params */
}


/**
 * add an object to the box tree
 */
struct box_result box_object(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box *box;
	struct object_params *po;
	struct plugin_params* pp;
	char *s, *url = NULL, *map;
	xmlNode *c;
	url_func_result res;

	po = malloc(sizeof *po);
	if (!po)
		return (struct box_result) {0, false, true};

	box = box_create(style, status->href, 0, status->id,
			status->content->data.html.box_pool);
	if (!box)
		return (struct box_result) {0, false, true};

	/* initialise po struct */
	po->data = 0;
	po->type = 0;
	po->codetype = 0;
	po->codebase = 0;
	po->classid = 0;
	po->params = 0;

	/* object data */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "data")) != NULL) {
		res = url_join(s, status->content->data.html.base_url, &url);
		/* if url is equivalent to the parent's url,
		 * we've got infinite inclusion. stop it here.
		 * also bail if url_join failed.
		 */
		if (res != URL_FUNC_OK || strcasecmp(url, status->content->data.html.base_url) == 0) {
			free(po);
			xmlFree(s);
			return (struct box_result) {0, true, true};
		}
		po->data = strdup(s);
		LOG(("object '%s'", po->data));
		xmlFree(s);
	}

	/* imagemap associated with this object */
	if ((map = xmlGetProp(n, (const xmlChar *) "usemap")) != NULL) {
		box->usemap = (map[0] == '#') ? strdup(map+1) : strdup(map);
		xmlFree(map);
		if (!box->usemap)
			return (struct box_result) {0, false, true};
	}

	/* object type */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "type")) != NULL) {
		po->type = strdup(s);
		LOG(("type: %s", s));
		xmlFree(s);
	}

	/* object codetype */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "codetype")) != NULL) {
		po->codetype = strdup(s);
		LOG(("codetype: %s", s));
		xmlFree(s);
	}

	/* object codebase */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "codebase")) != NULL) {
		po->codebase = strdup(s);
		LOG(("codebase: %s", s));
		xmlFree(s);
	}

	/* object classid */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "classid")) != NULL) {
		po->classid = strdup(s);
		LOG(("classid: %s", s));
		xmlFree(s);
	}

	/* parameters
	 * parameter data is stored in a singly linked list.
	 * po->params points to the head of the list.
	 * new parameters are added to the head of the list.
	 */
	for (c = n->children; c != NULL; c = c->next) {
		if (strcmp((const char *) c->name, "param") == 0) {
			pp = malloc(sizeof *pp);
			if (!pp)
				return (struct box_result) {0, false, true};

			/* initialise pp struct */
			pp->name = 0;
			pp->value = 0;
			pp->valuetype = 0;
			pp->type = 0;
			pp->next = 0;

			if ((s = (char *) xmlGetProp(c, (const xmlChar *) "name")) != NULL) {
				pp->name = strdup(s);
				xmlFree(s);
			}
			if ((s = (char *) xmlGetProp(c, (const xmlChar *) "value")) != NULL) {
				pp->value = strdup(s);
				xmlFree(s);
			}
			if ((s = (char *) xmlGetProp(c, (const xmlChar *) "type")) != NULL) {
				pp->type = strdup(s);
				xmlFree(s);
			}
			if ((s = (char *) xmlGetProp(c, (const xmlChar *) "valuetype")) != NULL) {
				pp->valuetype = strdup(s);
				xmlFree(s);
			} else {
				pp->valuetype = strdup("data");
			}

			pp->next = po->params;
			po->params = pp;
		} else {
				/* The first non-param child is the start
				 * of the alt html. Therefore, we should
				 * break out of this loop.
				 */
				/** \todo: following statement is *not* breaking the loop ?! Is comment or code wrong here ? */
				continue;
		}
	}

	box->object_params = po;

	/* start fetch */
	if (plugin_decode(status->content, url, box, po))
		return (struct box_result) {box, false, false};

	return (struct box_result) {box, true, false};
}

/**
 * add an embed to the box tree
 */

struct box_result box_embed(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box *box;
	struct object_params *po;
	struct plugin_params *pp;
	char *s, *url = NULL;
	xmlAttr *a;
	url_func_result res;

	po = malloc(sizeof *po);
	if (!po)
		return (struct box_result) {0, false, true};

	box = box_create(style, status->href, 0, status->id,
			status->content->data.html.box_pool);
	if (!box)
		return (struct box_result) {0, false, true};

	/* initialise po struct */
	po->data = 0;
	po->type = 0;
	po->codetype = 0;
	po->codebase = 0;
	po->classid = 0;
	po->params = 0;

	/* embed src */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "src")) != NULL) {
		res = url_join(s, status->content->data.html.base_url, &url);
		/* if url is equivalent to the parent's url,
		 * we've got infinite inclusion. stop it here.
		 * also bail if url_join failed.
		 */
		if (res != URL_FUNC_OK || strcasecmp(url, status->content->data.html.base_url) == 0) {
			free(po);
			xmlFree(s);
			return (struct box_result) {0, false, true};
		}
		LOG(("embed '%s'", url));
		po->data = strdup(s);
		xmlFree(s);
	}

	/**
	 * we munge all other attributes into a plugin_parameter structure
	 */
	for (a=n->properties; a != NULL; a=a->next) {
		pp = malloc(sizeof *pp);
		if (!pp)
			return (struct box_result) {0, false, true};

		/* initialise pp struct */
		pp->name = 0;
		pp->value = 0;
		pp->valuetype = 0;
		pp->type = 0;
		pp->next = 0;

		if (strcasecmp((const char*)a->name, "src") != 0) {
			pp->name = strdup((const char*)a->name);
			pp->value = strdup((char*)a->children->content);
			pp->valuetype = strdup("data");
			pp->next = po->params;
			po->params = pp;
		}
	 }

	box->object_params = po;

	/* start fetch */
	plugin_decode(status->content, url, box, po);

	return (struct box_result) {box, false, false};
}

/**
 * add an applet to the box tree
 */

struct box_result box_applet(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box *box;
	struct object_params *po;
	struct plugin_params *pp;
	char *s, *url = NULL;
	xmlNode *c;
	url_func_result res;

	po = malloc(sizeof *po);
	if (!po)
		return (struct box_result) {0, false, true};

	box = box_create(style, status->href, 0, status->id,
			status->content->data.html.box_pool);
	if (!box)
		return (struct box_result) {0, false, true};

	/* initialise po struct */
	po->data = 0;
	po->type = 0;
	po->codetype = 0;
	po->codebase = 0;
	po->classid = 0;
	po->params = 0;

	/* code */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "code")) != NULL) {
		res = url_join(s, status->content->data.html.base_url, &url);
		/* if url is equivalent to the parent's url,
		 * we've got infinite inclusion. stop it here.
		 * also bail if url_join failed.
		 */
		if (res != URL_FUNC_OK || strcasecmp(url, status->content->data.html.base_url) == 0) {
			free(po);
			xmlFree(s);
			return (struct box_result) {box, true, false};
		}
		LOG(("applet '%s'", url));
		po->classid = strdup(s);
		xmlFree(s);
	}

	/* object codebase */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "codebase")) != NULL) {
		po->codebase = strdup(s);
		LOG(("codebase: %s", s));
		xmlFree(s);
	}

	/* parameters
	 * parameter data is stored in a singly linked list.
	 * po->params points to the head of the list.
	 * new parameters are added to the head of the list.
	 */
	for (c = n->children; c != 0; c = c->next) {
		if (strcmp((const char *) c->name, "param") == 0) {
			pp = malloc(sizeof *pp);
			if (!pp)
				return (struct box_result) {0, false, true};

			/* initialise pp struct */
			pp->name = 0;
			pp->value = 0;
			pp->valuetype = 0;
			pp->type = 0;
			pp->next = 0;

			if ((s = (char *) xmlGetProp(c, (const xmlChar *) "name")) != NULL) {
				pp->name = strdup(s);
				xmlFree(s);
			}
			if ((s = (char *) xmlGetProp(c, (const xmlChar *) "value")) != NULL) {
				pp->value = strdup(s);
				xmlFree(s);
			}
			if ((s = (char *) xmlGetProp(c, (const xmlChar *) "type")) != NULL) {
				pp->type = strdup(s);
				xmlFree(s);
			}
			if ((s = (char *) xmlGetProp(c, (const xmlChar *) "valuetype")) != NULL) {
				pp->valuetype = strdup(s);
				xmlFree(s);
			} else {
				pp->valuetype = strdup("data");
			}

			pp->next = po->params;
			po->params = pp;
		} else {
				 /* The first non-param child is the start
				  * of the alt html. Therefore, we should
				  * break out of this loop.
				  */
				/** \todo: following statement is *not* breaking the loop ?! Is comment or code wrong here ? */
				continue;
		}
	}

	box->object_params = po;

	/* start fetch */
	if (plugin_decode(status->content, url, box, po))
		return (struct box_result) {box, false, false};

	return (struct box_result) {box, true, false};
}

/**
 * box_iframe
 * add an iframe to the box tree
 * TODO - implement GUI nested wimp stuff 'cos this looks naff atm. (16_5)
 */
struct box_result box_iframe(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box *box;
	struct object_params *po;
	char *s, *url = NULL;
	url_func_result res;

	po = malloc(sizeof *po);
	if (!po)
		return (struct box_result) {0, false, true};

	box = box_create(style, status->href, 0, status->id,
			status->content->data.html.box_pool);
	if (!box)
		return (struct box_result) {0, false, true};

	/* initialise po struct */
	po->data = 0;
	po->type = 0;
	po->codetype = 0;
	po->codebase = 0;
	po->classid = 0;
	po->params = 0;

	/* iframe src */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "src")) != NULL) {
		res = url_join(s, status->content->data.html.base_url, &url);
		/* if url is equivalent to the parent's url,
		 * we've got infinite inclusion. stop it here.
		 * also bail if url_join failed.
		 */
		if (res != URL_FUNC_OK || strcasecmp(url, status->content->data.html.base_url) == 0) {
			free(po);
			xmlFree(s);
			return (struct box_result) {0, false, true};
		}
		LOG(("embed '%s'", url));
		po->data = strdup(s);
		xmlFree(s);
	}

	box->object_params = po;

	/* start fetch */
	plugin_decode(status->content, url, box, po);

	return (struct box_result) {box, false, false};
}

/**
 * plugin_decode
 * This function checks that the contents of the plugin_object struct
 * are valid. If they are, it initiates the fetch process. If they are
 * not, it exits, leaving the box structure as it was on entry. This is
 * necessary as there are multiple ways of declaring an object's attributes.
 *
 * Returns false if the object could not be handled.
 *
 * TODO: plug failure leaks
 */
bool plugin_decode(struct content* content, char* url, struct box* box,
		struct object_params* po)
{
	struct plugin_params * pp;
	url_func_result res;

	/* Check if the codebase attribute is defined.
	 * If it is not, set it to the codebase of the current document.
	 */
	if (po->codebase == 0)
		res = url_join("./", content->data.html.base_url, &po->codebase);
	else
		res = url_join(po->codebase, content->data.html.base_url, &po->codebase);

	if (res != URL_FUNC_OK)
		return false;

	/* Set basehref */
	po->basehref = strdup(content->data.html.base_url);

	/* Check that we have some data specified.
	 * First, check the data attribute.
	 * Second, check the classid attribute.
	 * The data attribute takes precedence.
	 * If neither are specified or if classid begins "clsid:",
	 * we can't handle this object.
	 */
	if (po->data == 0 && po->classid == 0)
		return false;

	if (po->data == 0 && po->classid != 0) {
		if (strncasecmp(po->classid, "clsid:", 6) == 0) {
			/* Flash */
			if (strcasecmp(po->classid, "clsid:D27CDB6E-AE6D-11cf-96B8-444553540000") == 0) {
				for (pp = po->params;
					pp != 0 && strcasecmp(pp->name, "movie") != 0;
					pp = pp->next)
					/* no body */;
				if (pp == 0)
						return false;
				res = url_join(pp->value, po->basehref, &url);
				if (res != URL_FUNC_OK)
						return false;
				/* munge the codebase */
				res = url_join("./",
						content->data.html.base_url,
						&po->codebase);
				if (res != URL_FUNC_OK)
						return false;
			}
			else {
				LOG(("ActiveX object - n0"));
				return false;
			}
		} else {
			res = url_join(po->classid, po->codebase, &url);
			if (res != URL_FUNC_OK)
				return false;

			/* The java plugin doesn't need the .class extension
			 * so we strip it.
			 */
			if (strcasecmp(&po->classid[strlen(po->classid)-6],
					".class") == 0)
				po->classid[strlen(po->classid)-6] = 0;
		}
	} else {
		res = url_join(po->data, po->codebase, &url);
		if (res != URL_FUNC_OK)
			return false;
	}

	/* Check if the declared mime type is understandable.
	 * Checks type and codetype attributes.
	 */
	if (po->type != 0 && content_lookup(po->type) == CONTENT_OTHER)
		return false;
	if (po->codetype != 0 && content_lookup(po->codetype) == CONTENT_OTHER)
		return false;

	/* If we've got to here, the object declaration has provided us with
	 * enough data to enable us to have a go at downloading and
	 * displaying it.
	 *
	 * We may still find that the object has a MIME type that we can't
	 * handle when we fetch it (if the type was not specified or is
	 * different to that given in the attributes).
	 */
	if (!html_fetch_object(content, url, box, 0, 1000, 1000, false))
		return false;

	return true;
}


struct box_result box_frameset(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	unsigned int row, col;
	unsigned int rows = 1, cols = 1;
	int object_width, object_height;
	char *s, *s1, *url;
	struct box *box;
	struct box *row_box;
	struct box *cell_box;
	struct box *object_box;
	struct css_style *row_style;
	struct css_style *cell_style;
	struct css_style *object_style;
	struct box_result r;
	struct box_multi_length *row_height = 0, *col_width = 0;
	xmlNode *c;
	url_func_result res;

	box = box_create(style, 0, status->title, status->id,
			status->content->data.html.box_pool);
	if (!box)
		return (struct box_result) {0, false, true};
	box->type = BOX_TABLE;

	/* parse rows and columns */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "rows")) != NULL) {
		row_height = box_parse_multi_lengths(s, &rows);
		xmlFree(s);
		if (!row_height) {
			box_free_box(box);
			return (struct box_result) {0, false, true};
		}
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "cols")) != NULL) {
		col_width = box_parse_multi_lengths(s, &cols);
		xmlFree(s);
		if (!col_width) {
			free(row_height);
			box_free_box(box);
			return (struct box_result) {0, false, true};
		}
	}

	LOG(("rows %u, cols %u", rows, cols));

	box->min_width = 1;
	box->max_width = 10000;
	box->col = malloc(sizeof box->col[0] * cols);
	if (!box->col) {
		free(row_height);
		free(col_width);
		box_free_box(box);
		return (struct box_result) {0, false, true};
	}

	if (col_width) {
		for (col = 0; col != cols; col++) {
			if (col_width[col].type == LENGTH_PX) {
				box->col[col].type = COLUMN_WIDTH_FIXED;
				box->col[col].width = col_width[col].value;
			} else if (col_width[col].type == LENGTH_PERCENT) {
				box->col[col].type = COLUMN_WIDTH_PERCENT;
				box->col[col].width = col_width[col].value;
			} else {
				box->col[col].type = COLUMN_WIDTH_RELATIVE;
				box->col[col].width = col_width[col].value;
			}
			box->col[col].min = 1;
			box->col[col].max = 10000;
		}
	} else {
		box->col[0].type = COLUMN_WIDTH_RELATIVE;
		box->col[0].width = 1;
		box->col[0].min = 1;
		box->col[0].max = 10000;
	}

	/* create the frameset table */
	c = n->children;
	for (row = 0; c && row != rows; row++) {
		row_style = css_duplicate_style(style);
		if (!row_style) {
			box_free(box);
			free(row_height);
			free(col_width);
			return (struct box_result) {0, false, true};
		}
		object_height = 1000;  /** \todo  get available height */
	/*	if (row_height) {
			row_style->height.height = CSS_HEIGHT_LENGTH;
			row_style->height.length.unit = CSS_UNIT_PX;
			if (row_height[row].type == LENGTH_PERCENT)
				row_style->height.length.value = 1000 *
						row_height[row].value / 100;
			else if (row_height[row].type == LENGTH_RELATIVE)
				row_style->height.length.value = 100 *
						row_height[row].value;
			else
				row_style->height.length.value =
						row_height[row].value;
			object_height = row_style->height.length.value;
		}*/
		row_box = box_create(row_style, 0, 0, 0,
				status->content->data.html.box_pool);
		if (!row_box)
			return (struct box_result) {0, false, true};

		row_box->type = BOX_TABLE_ROW;
		box_add_child(box, row_box);

		for (col = 0; c && col != cols; col++) {
			while (c && !(c->type == XML_ELEMENT_NODE && (
				strcmp((const char *) c->name, "frame") == 0 ||
				strcmp((const char *) c->name, "frameset") == 0
					)))
				c = c->next;
			if (!c)
				break;

			/* estimate frame width */
			object_width = status->content->available_width;
			if (col_width && col_width[col].type == LENGTH_PX)
				object_width = col_width[col].value;

			cell_style = css_duplicate_style(style);
			if (!cell_style) {
				box_free(box);
				free(row_height);
				free(col_width);
				return (struct box_result) {0, false, true};
			}
			css_cascade(cell_style, &css_blank_style);
			cell_style->overflow = CSS_OVERFLOW_AUTO;

			cell_box = box_create(cell_style, 0, 0, 0,
					status->content->data.html.box_pool);
			if (!cell_box)
				return (struct box_result) {0, false, true};
			cell_box->type = BOX_TABLE_CELL;
			box_add_child(row_box, cell_box);

			if (strcmp((const char *) c->name, "frameset") == 0) {
				LOG(("frameset"));
				r = box_frameset(c, status, style);
				if (r.memory_error) {
					box_free(box);
					free(row_height);
					free(col_width);
					return (struct box_result) {0, false,
							true};
				}
				r.box->style_clone = 1;
				box_add_child(cell_box, r.box);

				c = c->next;
				continue;
			}

			object_style = css_duplicate_style(style);
			if (!object_style) {
				box_free(box);
				free(row_height);
				free(col_width);
				return (struct box_result) {0, false, true};
			}
			if (col_width && col_width[col].type == LENGTH_PX) {
				object_style->width.width = CSS_WIDTH_LENGTH;
				object_style->width.value.length.unit =
						CSS_UNIT_PX;
				object_style->width.value.length.value =
						object_width;
			}

			object_box = box_create(object_style, 0, 0, 0,
					status->content->data.html.box_pool);
			if (!object_box)
				return (struct box_result) {0, false, true};
			object_box->type = BOX_BLOCK;
			box_add_child(cell_box, object_box);

			if ((s = (char *) xmlGetProp(c, (const xmlChar *) "src")) == NULL) {
				c = c->next;
				continue;
			}

			s1 = strip(s);
			res = url_join(s1, status->content->data.html.base_url, &url);
			/* if url is equivalent to the parent's url,
			 * we've got infinite inclusion. stop it here.
			 * also bail if url_join failed.
			 */
			if (res != URL_FUNC_OK || strcasecmp(url, status->content->data.html.base_url) == 0) {
				xmlFree(s);
				c = c->next;
				continue;
			}

			LOG(("frame, url '%s'", url));

			if (!html_fetch_object(status->content, url,
					object_box, 0,
					object_width, object_height, false))
				return (struct box_result) {0, false, true};
			xmlFree(s);

			c = c->next;
		}
	}

	free(row_height);
	free(col_width);

	style->width.width = CSS_WIDTH_PERCENT;
	style->width.value.percent = 100;

	return (struct box_result) {box, false, false};
}


/**
 * Parse a multi-length-list, as defined by HTML 4.01.
 *
 * \param  s    string to parse
 * \param  count  updated to number of entries
 * \return  array of struct box_multi_length, or 0 on memory exhaustion
 */

struct box_multi_length *box_parse_multi_lengths(const char *s,
		unsigned int *count)
{
	char *end;
	unsigned int i, n;
	struct box_multi_length *length;

	for (i = 0, n = 1; s[i]; i++)
		if (s[i] == ',')
			n++;

	if ((length = malloc(sizeof *length * n)) == NULL)
		return NULL;

	for (i = 0; i != n; i++) {
		while (isspace(*s))
			s++;
		length[i].value = strtof(s, &end);
		if (length[i].value <= 0)
			length[i].value = 1;
		s = end;
		switch (*s) {
			case '%': length[i].type = LENGTH_PERCENT; break;
			case '*': length[i].type = LENGTH_RELATIVE; break;
			default:  length[i].type = LENGTH_PX; break;
		}
		while (*s && *s != ',')
			s++;
		if (*s == ',')
			s++;
	}

	*count = n;
	return length;
}


/**
 * Find the absolute coordinates of a box.
 *
 * \param  box  the box to calculate coordinates of
 * \param  x    updated to x coordinate
 * \param  y    updated to y coordinate
 */

void box_coords(struct box *box, int *x, int *y)
{
	*x = box->x;
	*y = box->y;
	while (box->parent) {
		if (box_is_float(box)) {
			do {
				box = box->parent;
			} while (!box->float_children);
		} else
			box = box->parent;
		*x += box->x - box->scroll_x;
		*y += box->y - box->scroll_y;
	}
}


/**
 * Find the boxes at a point.
 *
 * \param  box      box to search children of
 * \param  x        point to find, in global document coordinates
 * \param  y        point to find, in global document coordinates
 * \param  box_x    position of box, in global document coordinates, updated
 *                  to position of returned box, if any
 * \param  box_y    position of box, in global document coordinates, updated
 *                  to position of returned box, if any
 * \param  content  updated to content of object that returned box is in, if any
 * \return  box at given point, or 0 if none found
 *
 * To find all the boxes in the heirarchy at a certain point, use code like
 * this:
 * \code
 *	struct box *box = top_of_document_to_search;
 *	int box_x = 0, box_y = 0;
 *	struct content *content = document_to_search;
 *
 *	while ((box = box_at_point(box, x, y, &box_x, &box_y, &content))) {
 *		// process box
 *	}
 * \endcode
 */

struct box *box_at_point(struct box *box, int x, int y,
		int *box_x, int *box_y,
		struct content **content)
{
	int bx = *box_x, by = *box_y;
	struct box *child, *sibling;

	assert(box);

	/* drill into HTML objects */
	if (box->object) {
		if (box->object->type == CONTENT_HTML &&
				box->object->data.html.layout) {
			*content = box->object;
			box = box->object->data.html.layout;
		} else {
			goto siblings;
		}
	}

	/* consider floats first, since they will often overlap other boxes */
	for (child = box->float_children; child; child = child->next_float) {
		if (box_contains_point(child, x - bx, y - by)) {
			*box_x += child->x - child->scroll_x;
			*box_y += child->y - child->scroll_y;
			return child;
		}
	}

	/* non-float children */
	for (child = box->children; child; child = child->next) {
		if (box_is_float(child))
			continue;
		if (box_contains_point(child, x - bx, y - by)) {
			*box_x += child->x - child->scroll_x;
			*box_y += child->y - child->scroll_y;
			return child;
		}
	}

siblings:
	/* siblings and siblings of ancestors */
	while (box) {
		if (!box_is_float(box)) {
			bx -= box->x - box->scroll_x;
			by -= box->y - box->scroll_y;
			for (sibling = box->next; sibling;
					sibling = sibling->next) {
				if (box_is_float(sibling))
					continue;
				if (box_contains_point(sibling,
						x - bx, y - by)) {
					*box_x = bx + sibling->x -
							sibling->scroll_x;
					*box_y = by + sibling->y -
							sibling->scroll_y;
					return sibling;
				}
			}
			box = box->parent;
		} else {
			bx -= box->x - box->scroll_x;
			by -= box->y - box->scroll_y;
			for (sibling = box->next_float; sibling;
					sibling = sibling->next_float) {
				if (box_contains_point(sibling,
						x - bx, y - by)) {
					*box_x = bx + sibling->x -
							sibling->scroll_x;
					*box_y = by + sibling->y -
							sibling->scroll_y;
					return sibling;
				}
			}
			do {
				box = box->parent;
			} while (!box->float_children);
		}
	}

	return 0;
}


/**
 * Determine if a point lies within a box.
 *
 * \param  box  box to consider
 * \param  x    coordinate relative to box parent
 * \param  y    coordinate relative to box parent
 * \return  true if the point is within the box or a descendant box
 *
 * This is a helper function for box_at_point().
 */

bool box_contains_point(struct box *box, int x, int y)
{
	if (box->style && box->style->overflow != CSS_OVERFLOW_VISIBLE) {
		if (box->x <= x &&
				x < box->x + box->padding[LEFT] + box->width +
				box->padding[RIGHT] &&
				box->y <= y &&
				y < box->y + box->padding[TOP] + box->height +
				box->padding[BOTTOM])
			return true;
	} else {
		if (box->x + box->descendant_x0 <= x &&
				x < box->x + box->descendant_x1 &&
				box->y + box->descendant_y0 <= y &&
				y < box->y + box->descendant_y1)
			return true;
	}
	return false;
}


/**
 * Find the box containing an object at the given coordinates, if any.
 *
 * \param  c  content to search, must have type CONTENT_HTML
 * \param  x  coordinates in document units
 * \param  y  coordinates in document units
 */

struct box *box_object_at_point(struct content *c, int x, int y)
{
	struct box *box = c->data.html.layout;
	int box_x = 0, box_y = 0;
	struct content *content = c;
	struct box *object_box = 0;

	assert(c->type == CONTENT_HTML);

	while ((box = box_at_point(box, x, y, &box_x, &box_y, &content)) != NULL) {
		if (box->style &&
				box->style->visibility == CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->object)
			object_box = box;
	}

	return object_box;
}


/**
 * Find a box based upon its id attribute.
 *
 * \param box box to search
 * \param id  id to look for
 * \return the box or 0 if not found
 */
struct box *box_find_by_id(struct box *box, const char *id)
{
	struct box *a, *b;

	if (box->id != NULL && strcmp(id, box->id) == 0)
		return box;

	for (a = box->children; a; a = a->next) {
		if ((b = box_find_by_id(a, id)) != NULL)
			return b;
	}

	return NULL;
}
