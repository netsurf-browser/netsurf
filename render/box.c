/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

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
	struct form* current_form;
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
#ifdef WITH_SPRITE
	CONTENT_SPRITE,
#endif
#ifdef WITH_DRAW
	CONTENT_DRAW,
#endif
	CONTENT_UNKNOWN };

static struct box * convert_xml_to_box(xmlNode * n, struct content *content,
		struct css_style * parent_style,
		struct box * parent, struct box *inline_container,
		struct box_status status);
static struct css_style * box_get_style(struct content *c,
                struct content ** stylesheet,
		unsigned int stylesheet_count, struct css_style * parent_style,
		xmlNode * n);
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
static void add_option(xmlNode* n, struct form_control* current_select, const char *text);
static void box_normalise_block(struct box *block, pool box_pool);
static void box_normalise_table(struct box *table, pool box_pool);
void box_normalise_table_row_group(struct box *row_group,
		unsigned int **row_span, unsigned int *table_columns,
		pool box_pool);
void box_normalise_table_row(struct box *row,
		unsigned int **row_span, unsigned int *table_columns,
		pool box_pool);
static void box_normalise_inline_container(struct box *cont, pool box_pool);
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
 */

struct box * box_create(struct css_style * style,
		char *href, char *title, pool box_pool)
{
	unsigned int i;
	struct box *box = pool_alloc(box_pool, sizeof (struct box));
	assert(box);
	box->type = BOX_INLINE;
	box->style = style;
	box->width = UNKNOWN_WIDTH;
	box->max_width = UNKNOWN_MAX_WIDTH;
	box->href = href ? xstrdup(href) : 0;
	box->title = title ? xstrdup(title) : 0;
	box->columns = 1;
	box->rows = 1;
	box->text = 0;
	box->space = 0;
	box->clone = 0;
	box->style_clone = 0;
	box->length = 0;
	box->start_column = 0;
	box->next = 0;
	box->prev = 0;
	box->children = 0;
	box->last = 0;
	box->parent = 0;
	box->float_children = 0;
	box->next_float = 0;
	box->col = 0;
	box->font = 0;
	box->gadget = 0;
	box->usemap = 0;
	box->background = 0;
	box->object = 0;
	box->object_params = 0;
	box->object_state = 0;
	box->x = box->y = 0;
	box->height = 0;
	for (i = 0; i != 4; i++)
		box->margin[i] = box->padding[i] = box->border[i] = 0;
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
 * make a box tree with style data from an xml tree
 */

void xml_to_box(xmlNode *n, struct content *c)
{
	struct box_status status = {c, 0, 0, 0};

	LOG(("node %p", n));
	assert(c->type == CONTENT_HTML);

	c->data.html.layout = box_create(0, 0, 0, c->data.html.box_pool);
	c->data.html.layout->type = BOX_BLOCK;

	c->data.html.style = xcalloc(1, sizeof(struct css_style));
	memcpy(c->data.html.style, &css_base_style, sizeof(struct css_style));
	c->data.html.style->font_size.value.length.value = option_font_size * 0.1;
	c->data.html.fonts = nsfont_new_set();

	c->data.html.object_count = 0;
	c->data.html.object = xcalloc(0, sizeof(*c->data.html.object));

	convert_xml_to_box(n, c, c->data.html.style,
			c->data.html.layout, 0, status);
	LOG(("normalising"));
	box_normalise_block(c->data.html.layout->children,
			c->data.html.box_pool);
}


/**
 * make a box tree with style data from an xml tree
 *
 * arguments:
 * 	n		xml tree
 * 	content		content structure
 * 	parent_style	style at this point in xml tree
 * 	parent		parent in box tree
 * 	inline_container	current inline container box, or 0
 * 	status		status for forms etc.
 *
 * returns:
 * 	updated current inline container
 */

/* mapping from CSS display to box type
 * this table must be in sync with css/css_enums */
static box_type box_map[] = {
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

struct box * convert_xml_to_box(xmlNode * n, struct content *content,
		struct css_style * parent_style,
		struct box * parent, struct box *inline_container,
		struct box_status status)
{
	struct box * box = 0;
	struct box * inline_container_c;
	struct css_style * style = 0;
	xmlNode * c;
	char * s;
	xmlChar * title0;
	char * title = 0;
	bool convert_children = true;
	char *href_in = status.href;

	assert(n != 0 && parent_style != 0 && parent != 0);
	LOG(("node %p, node type %i", n, n->type));

	if (n->type == XML_ELEMENT_NODE) {
		struct element_entry *element;

		gui_multitask();

		style = box_get_style(content, content->data.html.stylesheet_content,
				content->data.html.stylesheet_count, parent_style, n);
		LOG(("display: %s", css_display_name[style->display]));
		if (style->display == CSS_DISPLAY_NONE) {
			free(style);
			LOG(("node %p, node type %i END", n, n->type));
			goto end;
		}
		/* floats are treated as blocks */
		if (style->float_ == CSS_FLOAT_LEFT || style->float_ == CSS_FLOAT_RIGHT)
			if (style->display == CSS_DISPLAY_INLINE)
				style->display = CSS_DISPLAY_BLOCK;

		/* extract title attribute, if present */
		if ((title0 = xmlGetProp(n, (const xmlChar *) "title"))) {
			status.title = title = squash_whitespace(title0);
			xmlFree(title0);
		}

		/* special elements */
		element = bsearch((const char *) n->name, element_table,
				ELEMENT_TABLE_COUNT, sizeof(element_table[0]),
				(int (*)(const void *, const void *)) strcmp);
		if (element != 0) {
			/* a special convert function exists for this element */
			struct box_result res = element->convert(n, &status, style);
			box = res.box;
			convert_children = res.convert_children;
			if (res.memory_error) {
				/** \todo  handle memory exhaustion */
			}
			if (box == 0) {
				/* no box for this element */
				assert(convert_children == 0);
				free(style);
				LOG(("node %p, node type %i END", n, n->type));
				goto end;
			}
		} else {
			/* general element */
			box = box_create(style, status.href, title,
					content->data.html.box_pool);
		}
		/* set box type from style if it has not been set already */
		if (box->type == BOX_INLINE)
			box->type = box_map[style->display];

	} else if (n->type == XML_TEXT_NODE) {
		/* text node: added to inline container below */

	} else {
		/* not an element or text node: ignore it (eg. comment) */
		LOG(("node %p, node type %i END", n, n->type));
		goto end;
	}

	content->size += sizeof(struct box) + sizeof(struct css_style);

	if (n->type == XML_TEXT_NODE &&
			(parent_style->white_space == CSS_WHITE_SPACE_NORMAL ||
			 parent_style->white_space == CSS_WHITE_SPACE_NOWRAP)) {
		char *text = squash_whitespace(n->content);

		/* if the text is just a space, combine it with the preceding
		 * text node, if any */
		if (text[0] == ' ' && text[1] == 0) {
			if (inline_container != 0) {
				assert(inline_container->last != 0);
				inline_container->last->space = 1;
			}
			free(text);
			goto end;
		}

		if (inline_container == 0) {
			/* this is the first inline node: make a container */
			inline_container = box_create(0, 0, 0,
					content->data.html.box_pool);
			inline_container->type = BOX_INLINE_CONTAINER;
			box_add_child(parent, inline_container);
		}

		box = box_create(parent_style, status.href, title,
				content->data.html.box_pool);
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
				/* no body */;
			if (i != box->length) {
				/* there is a space in text block and we
				 * want all spaces to be converted to NBSP
				 */
				char *org_text = box->text;
				org_text[box->length] = '\0';
				box->text = cnv_space2nbsp(org_text);
				free(org_text);
				box->length = strlen(box->text);
			}
		}
		box->font = nsfont_open(content->data.html.fonts, box->style);

		box_add_child(inline_container, box);
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
		assert(parent_style->white_space == CSS_WHITE_SPACE_PRE);
		if (parent_style->text_transform != CSS_TEXT_TRANSFORM_NONE)
			box_text_transform(text, strlen(text),
					parent_style->text_transform);
		current = text;
		do {
			size_t len = strcspn(current, "\r\n");
			char old = current[len];
			current[len] = 0;
			if (!inline_container) {
				inline_container = box_create(0, 0, 0,
						content->data.html.box_pool);
				inline_container->type = BOX_INLINE_CONTAINER;
				box_add_child(parent, inline_container);
			}
			box = box_create(parent_style, status.href, title,
					content->data.html.box_pool);
			box->type = BOX_INLINE;
			box->style_clone = 1;
			box->text = xstrdup(current);
			box->length = strlen(box->text);
			box->font = nsfont_open(content->data.html.fonts, box->style);
			box_add_child(inline_container, box);
			current[len] = old;
			current += len;
			if (current[0] == '\r' && current[1] == '\n') {
				current += 2;
				inline_container = 0;
			} else if (current[0] != 0) {
				current++;
				inline_container = 0;
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
		if (inline_container == 0) {
			/* this is the first inline node: make a container */
			inline_container = box_create(0, 0, 0,
					content->data.html.box_pool);
			inline_container->type = BOX_INLINE_CONTAINER;
			box_add_child(parent, inline_container);
		}

		if (box->type == BOX_INLINE || box->type == BOX_BR) {
			/* inline box: add to tree and recurse */
			box_add_child(inline_container, box);
			if (convert_children) {
				for (c = n->children; c != 0; c = c->next)
					inline_container = convert_xml_to_box(c, content, style,
							parent, inline_container,
							status);
			}
			LOG(("node %p, node type %i END", n, n->type));
			goto end;
		} else if (box->type == BOX_INLINE_BLOCK) {
			/* inline block box: add to tree and recurse */
			box_add_child(inline_container, box);
			if (convert_children) {
				inline_container_c = 0;
				for (c = n->children; c != 0; c = c->next)
					inline_container_c = convert_xml_to_box(c, content, style,
							box, inline_container_c,
							status);
			}
			LOG(("node %p, node type %i END", n, n->type));
			goto end;
		} else {
			/* float: insert a float box between the parent and current node */
			assert(style->float_ == CSS_FLOAT_LEFT || style->float_ == CSS_FLOAT_RIGHT);
			LOG(("float"));
			parent = box_create(0, status.href, title,
					content->data.html.box_pool);
			if (style->float_ == CSS_FLOAT_LEFT)
				parent->type = BOX_FLOAT_LEFT;
			else
				parent->type = BOX_FLOAT_RIGHT;
			box_add_child(inline_container, parent);
			if (box->type == BOX_INLINE || box->type == BOX_INLINE_BLOCK)
				box->type = BOX_BLOCK;
		}
	}

	assert(n->type == XML_ELEMENT_NODE);

	/* non-inline box: add to tree and recurse */
	box_add_child(parent, box);
	if (convert_children) {
		inline_container_c = 0;
		for (c = n->children; c != 0; c = c->next)
			inline_container_c = convert_xml_to_box(c, content, style,
					box, inline_container_c,
					status);
	}
	if (style->float_ == CSS_FLOAT_NONE)
		/* new inline container unless this is a float */
		inline_container = 0;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "colspan"))) {
		int colspan = atoi(s);
		if (1 <= colspan && colspan <= 100)
			box->columns = colspan;
		xmlFree(s);
	}
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "rowspan"))) {
		if ((box->rows = strtol(s, 0, 10)) == 0)
			box->rows = 1;
		xmlFree(s);
	}

end:
	free(title);
	if (!href_in)
		xmlFree(status.href);

	/* Now fetch any background image for this box */
	if (box && box->style && box->style->background_image.type ==
			CSS_BACKGROUND_IMAGE_URI) {
		char *url = strdup(box->style->background_image.uri);
		if (!url) {
			/** \todo  handle this */
			return inline_container;
		}
		/* start fetch */
		html_fetch_object(content, url, box, image_types,
				content->available_width, 1000, true);
	}

	LOG(("node %p, node type %i END", n, n->type));
	return inline_container;
}


/**
 * Get the style for an element
 *
 * The style is collected from three sources:
 *  1. any styles for this element in the document stylesheet(s)
 *  2. non-CSS HTML attributes
 *  3. the 'style' attribute
 */

struct css_style * box_get_style(struct content *c,
                struct content ** stylesheet,
		unsigned int stylesheet_count, struct css_style * parent_style,
		xmlNode * n)
{
	struct css_style * style = xcalloc(1, sizeof(struct css_style));
	struct css_style style_new;
	char * s;
	unsigned int i;

	memcpy(style, parent_style, sizeof(struct css_style));
	memcpy(&style_new, &css_blank_style, sizeof(struct css_style));
	for (i = 0; i != stylesheet_count; i++) {
		if (stylesheet[i] != 0) {
			assert(stylesheet[i]->type == CONTENT_CSS);
			css_get_style(stylesheet[i], n, &style_new);
		}
	}
	css_cascade(style, &style_new);

	/* This property only applies to the body element, if you believe
	   the spec. Many browsers seem to allow it on other elements too,
	   so let's be generic ;)
	 */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "background"))) {
	        style->background_image.type = CSS_BACKGROUND_IMAGE_URI;
	        /**\todo This will leak memory. */
                style->background_image.uri = url_join(s, c->data.html.base_url);
                if (!style->background_image.uri)
                        style->background_image.type = CSS_BACKGROUND_IMAGE_NONE;
                xmlFree(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "bgcolor"))) {
		unsigned int r, g, b;
		if (s[0] == '#' && sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
			style->background_color = (b << 16) | (g << 8) | r;
		else if (s[0] != '#')
			style->background_color = named_colour(s);
		xmlFree(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "color"))) {
		unsigned int r, g, b;
		if (s[0] == '#' && sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
			style->color = (b << 16) | (g << 8) | r;
		else if (s[0] != '#')
			style->color = named_colour(s);
		xmlFree(s);
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "height"))) {
		float value = atof(s);
		if (value < 0) {
			/* ignore negative values */
		} else if (strrchr(s, '%')) {
	                /*the specification doesn't make clear what
	                 * percentage heights mean, so ignore them */
	        } else {
			style->height.height = CSS_HEIGHT_LENGTH;
			style->height.length.unit = CSS_UNIT_PX;
			style->height.length.value = value;
		}
		xmlFree(s);
	}

	if (strcmp((const char *) n->name, "input") == 0) {
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "size"))) {
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
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "text"))) {
			unsigned int r, g, b;
			if (s[0] == '#' && sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
				style->color = (b << 16) | (g << 8) | r;
			else if (s[0] != '#')
				style->color = named_colour(s);
			xmlFree(s);
		}
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "width"))) {
		float value = atof(s);
		if (value < 0) {
			/* ignore negative values */
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
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "rows"))) {
			int value = atoi(s);
			if (0 < value) {
				style->height.height = CSS_HEIGHT_LENGTH;
				style->height.length.unit = CSS_UNIT_EM;
				style->height.length.value = value;
			}
			xmlFree(s);
		}
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "cols"))) {
			int value = atoi(s);
			if (0 < value) {
				style->width.width = CSS_WIDTH_LENGTH;
				style->width.value.length.unit = CSS_UNIT_EX;
				style->width.value.length.value = value;
			}
			xmlFree(s);
		}
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "style"))) {
		struct css_style astyle;
		memcpy(&astyle, &css_empty_style, sizeof(struct css_style));
		css_parse_property_list(c, &astyle, s);
		css_cascade(style, &astyle);
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
	char *s;
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "href")))
		status->href = s;
	box = box_create(style, status->href, status->title,
			status->content->data.html.box_pool);
	return (struct box_result) {box, true, false};
}

struct box_result box_body(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box *box;
	status->content->data.html.background_colour = style->background_color;
        box = box_create(style, status->href, status->title,
			status->content->data.html.box_pool);

	return (struct box_result) {box, true, false};
}

struct box_result box_br(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box *box;
	box = box_create(style, status->href, status->title,
			status->content->data.html.box_pool);
	box->type = BOX_BR;
	return (struct box_result) {box, false, false};
}

struct box_result box_image(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box *box;
	char *s, *url, *s1, *map;
	xmlChar *s2;

	box = box_create(style, status->href, status->title,
			status->content->data.html.box_pool);

	/* handle alt text */
	if ((s2 = xmlGetProp(n, (const xmlChar *) "alt"))) {
		box->text = squash_whitespace(s2);
		box->length = strlen(box->text);
		box->font = nsfont_open(status->content->data.html.fonts, style);
		xmlFree(s2);
	}

	/* img without src is an error */
	if (!(s = (char *) xmlGetProp(n, (const xmlChar *) "src")))
		return (struct box_result) {box, false, false};

	/* imagemap associated with this image */
	if ((map = xmlGetProp(n, (const xmlChar *) "usemap"))) {
	        if (map[0] == '#') {
	                box->usemap = xstrdup(map+1);
	        }
	        else {
	                box->usemap = xstrdup(map);
	        }
	        xmlFree(map);
	}

	/* remove leading and trailing whitespace */
	s1 = strip(s);
	url = url_join(s1, status->content->data.html.base_url);
	if (!url) {
		xmlFree(s);
		return (struct box_result) {box, false, false};
	}

	LOG(("image '%s'", url));
	xmlFree(s);

	/* start fetch */
	html_fetch_object(status->content, url, box, image_types,
			status->content->available_width, 1000, false);

	return (struct box_result) {box, false, false};
}

struct box_result box_form(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	char *s, *s2;
	struct box *box;
	struct form *form;

	box = box_create(style, status->href, status->title,
			status->content->data.html.box_pool);

	s = (char *) xmlGetProp(n, (const xmlChar *) "action");
	if (!s) {
		/* the action attribute is required */
		return (struct box_result) {box, true, false};
	}

	status->current_form = form = xcalloc(1, sizeof(*form));
	form->action = s;

	form->method = method_GET;
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "method"))) {
		if (strcasecmp(s, "post") == 0) {
			form->method = method_POST_URLENC;
			if ((s2 = (char *) xmlGetProp(n, (const xmlChar *) "enctype"))) {
				if (strcasecmp(s2, "multipart/form-data") == 0)
					form->method = method_POST_MULTIPART;
				xmlFree(s2);
			}
		}
		xmlFree(s);
	}

	form->controls = form->last_control = 0;

	return (struct box_result) {box, true, false};
}

struct box_result box_textarea(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	xmlChar *content, *current;
	struct box *box, *inline_container, *inline_box;
	char* s;

	box = box_create(style, NULL, 0,
			status->content->data.html.box_pool);
	box->type = BOX_INLINE_BLOCK;
	box->gadget = form_new_control(GADGET_TEXTAREA);
	if (!box->gadget) {
		free(box);
		return (struct box_result) {0, false, true};
	}
	box->gadget->box = box;
	if (status->current_form)
		form_add_control(status->current_form, box->gadget);
	else
		box->gadget->form = 0;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name"))) {
		box->gadget->name = strdup(s);
		xmlFree(s);
		if (!box->gadget->name) {
			box_free(box);
			return (struct box_result) {0, false, true};
		}
	}

	/* split the content at newlines and make an inline container with an
	 * inline box for each line */
	current = content = xmlNodeGetContent(n);
	do {
		size_t len = strcspn(current, "\r\n");
		char old = current[len];
		current[len] = 0;
		inline_container = box_create(0, 0, 0,
				status->content->data.html.box_pool);
		inline_container->type = BOX_INLINE_CONTAINER;
		inline_box = box_create(style, 0, 0,
				status->content->data.html.box_pool);
		inline_box->type = BOX_INLINE;
		inline_box->style_clone = 1;
		if ((inline_box->text = strdup(current)) == NULL) {
				box_free(inline_box);
				box_free(inline_container);
				box_free(box);
				current[len] = old;
				xmlFree(content);
				return (struct box_result) {NULL, false, false};
		}
		inline_box->length = strlen(inline_box->text);
		inline_box->font = nsfont_open(status->content->data.html.fonts, style);
		box_add_child(inline_container, inline_box);
		box_add_child(box, inline_container);
		current[len] = old;
		current += len;
		if (current[0] == '\r' && current[1] == '\n')
			current += 2;
		else if (current[0] != 0)
			current++;
	} while (*current);
	xmlFree(content);

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

	if (status->current_form)
		form_add_control(status->current_form, gadget);
	else
		gadget->form = 0;

	gadget->data.select.multiple = false;
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "multiple"))) {
		gadget->data.select.multiple = true;
		xmlFree(s);
	}

	gadget->data.select.items = NULL;
	gadget->data.select.last_item = NULL;
	gadget->data.select.num_items = 0;
	gadget->data.select.num_selected = 0;

	for (c = n->children; c != 0; c = c->next) {
		if (strcmp((const char *) c->name, "option") == 0) {
			xmlChar *content = xmlNodeGetContent(c);
			add_option(c, gadget, content);
			xmlFree(content);
			gadget->data.select.num_items++;
		} else if (strcmp((const char *) c->name, "optgroup") == 0) {
			for (c2 = c->children; c2; c2 = c2->next) {
				if (strcmp((const char *) c2->name, "option") == 0) {
					xmlChar *content = xmlNodeGetContent(c2);
					add_option(c2, gadget, content);
					xmlFree(content);
					gadget->data.select.num_items++;
				}
			}
		}
	}

	if (gadget->data.select.num_items == 0) {
		/* no options: ignore entire select */
		form_free_control(gadget);
		return (struct box_result) {0, false, false};
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name"))) {
		gadget->name = strdup(s);
		xmlFree(s);
		if (!gadget->name) {
			form_free_control(gadget);
			return (struct box_result) {0, false, true};
		}
	}

	box = box_create(style, NULL, 0, status->content->data.html.box_pool);
	box->type = BOX_INLINE_BLOCK;
	box->gadget = gadget;
	gadget->box = box;

	inline_container = box_create(0, 0, 0,
			status->content->data.html.box_pool);
	inline_container->type = BOX_INLINE_CONTAINER;
	inline_box = box_create(style, 0, 0,
			status->content->data.html.box_pool);
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
		inline_box->text = xstrdup(messages_get("Form_None"));
	else if (gadget->data.select.num_selected == 1)
		inline_box->text = xstrdup(gadget->data.select.current->text);
	else
		inline_box->text = xstrdup(messages_get("Form_Many"));

	inline_box->length = strlen(inline_box->text);
	inline_box->font = nsfont_open(status->content->data.html.fonts, style);

	return (struct box_result) {box, false, false};
}

void add_option(xmlNode* n, struct form_control* current_select, const char *text)
{
	struct form_option *option = xcalloc(1, sizeof(struct form_option));
	const char *s;

	if ((text = squash_whitespace(text)) == NULL) {
		free(option);
		return;
	}

	assert(current_select != 0);

	if (current_select->data.select.items == 0)
		current_select->data.select.items = option;
	else
		current_select->data.select.last_item->next = option;
	current_select->data.select.last_item = option;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
		option->value = xstrdup(s);
		xmlFree(s);
	} else {
		option->value = xstrdup(text);
	}

	/* Convert all spaces into NBSP. */
	for (s = text; *s != '\0' && *s != ' '; ++s)
		/* no body */;
	if (*s == ' ') {
		const char *org_text = text;
		text = cnv_space2nbsp(org_text);
		free(org_text);
	}

	option->selected = option->initial_selected = false;
	option->text = text;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "selected"))) {
		xmlFree(s);
		if (current_select->data.select.num_selected == 0 ||
				current_select->data.select.multiple) {
			option->selected = option->initial_selected = true;
			current_select->data.select.num_selected++;
			current_select->data.select.current = option;
		}
	}
}

struct box_result box_input(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box* box = 0;
	struct form_control *gadget = 0;
	char *s, *type, *url;

	type = (char *) xmlGetProp(n, (const xmlChar *) "type");

	if (type && strcasecmp(type, "password") == 0) {
		box = box_input_text(n, status, style, true);
		gadget = box->gadget;
		gadget->box = box;

	} else if (type && strcasecmp(type, "file") == 0) {
		box = box_create(style, NULL, 0,
				status->content->data.html.box_pool);
		box->type = BOX_INLINE_BLOCK;
		box->gadget = gadget = form_new_control(GADGET_FILE);
		if (!gadget) {
			box_free_box(box);
			xmlFree(type);
			return (struct box_result) {0, false, true};
		}
		gadget->box = box;
		box->font = nsfont_open(status->content->data.html.fonts, style);

	} else if (type && strcasecmp(type, "hidden") == 0) {
		/* no box for hidden inputs */
		gadget = form_new_control(GADGET_HIDDEN);
		if (!gadget) {
			xmlFree(type);
			return (struct box_result) {0, false, true};
		}

		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
			gadget->value = strdup(s);
			xmlFree(s);
			if (!gadget->value) {
				form_free_control(gadget);
				xmlFree(type);
				return (struct box_result) {0, false, true};
			}
		}

	} else if (type && (strcasecmp(type, "checkbox") == 0 ||
			strcasecmp(type, "radio") == 0)) {
		box = box_create(style, NULL, 0,
				status->content->data.html.box_pool);
		box->gadget = gadget = form_new_control(GADGET_RADIO);
		if (!gadget) {
			box_free_box(box);
			xmlFree(type);
			return (struct box_result) {0, false, true};
		}
		gadget->box = box;
		if (type[0] == 'c' || type[0] == 'C')
			gadget->type = GADGET_CHECKBOX;

		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "checked"))) {
			gadget->selected = true;
			xmlFree(s);
		}

		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
			gadget->value = strdup(s);
			xmlFree(s);
			if (!gadget->value) {
				box_free_box(box);
				xmlFree(type);
				return (struct box_result) {0, false, true};
			}
		}

	} else if (type && (strcasecmp(type, "submit") == 0 ||
			strcasecmp(type, "reset") == 0)) {
		struct box_result result = box_button(n, status, style);
		struct box *inline_container, *inline_box;
		box = result.box;
		inline_container = box_create(0, 0, 0,
				status->content->data.html.box_pool);
		inline_container->type = BOX_INLINE_CONTAINER;
		inline_box = box_create(style, 0, 0,
				status->content->data.html.box_pool);
		inline_box->type = BOX_INLINE;
		inline_box->style_clone = 1;
		if (box->gadget->value != NULL)
			inline_box->text = strdup(box->gadget->value);
		else if (box->gadget->type == GADGET_SUBMIT)
			inline_box->text = strdup(messages_get("Form_Submit"));
		else
			inline_box->text = strdup(messages_get("Form_Reset"));
		if (inline_box->text == NULL) {
			box_free(inline_box);
			box_free(inline_container);
			box_free(box);
			xmlFree(type);
			return (struct box_result) {NULL, false, false};
		}
		inline_box->length = strlen(inline_box->text);
		inline_box->font = nsfont_open(status->content->data.html.fonts, style);
		box_add_child(inline_container, inline_box);
		box_add_child(box, inline_container);

	} else if (type && strcasecmp(type, "button") == 0) {
	        struct box_result result = box_button(n, status, style);
		struct box *inline_container, *inline_box;
		box = result.box;
		inline_container = box_create(0, 0, 0,
				status->content->data.html.box_pool);
		inline_container->type = BOX_INLINE_CONTAINER;
		inline_box = box_create(style, 0, 0,
				status->content->data.html.box_pool);
		inline_box->type = BOX_INLINE;
		inline_box->style_clone = 1;
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
		        inline_box->text = s;
		}
		else {
		        inline_box->text = xstrdup("Button");
		}
		inline_box->length = strlen(inline_box->text);
		inline_box->font = nsfont_open(status->content->data.html.fonts, style);
		box_add_child(inline_container, inline_box);
		box_add_child(box, inline_container);

	} else if (type && strcasecmp(type, "image") == 0) {
	        box = box_create(style, NULL, 0,
				status->content->data.html.box_pool);
	        box->gadget = gadget = form_new_control(GADGET_IMAGE);
		if (!gadget) {
			box_free_box(box);
			xmlFree(type);
			return (struct box_result) {0, false, true};
		}
		gadget->box = box;
		gadget->type = GADGET_IMAGE;
		if ((s = (char *) xmlGetProp(n, (const xmlChar*) "src"))) {
			url = url_join(s, status->content->data.html.base_url);
			if (url)
				html_fetch_object(status->content, url, box,
						image_types,
						status->content->available_width,
						1000, false);
			xmlFree(s);
		}

	} else {
		/* the default type is "text" */
		box = box_input_text(n, status, style, false);
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
			if (!gadget->name) {
				box_free_box(box);
				return (struct box_result) {0, false, true};
			}
		}
	}

	return (struct box_result) {box, false, false};
}

struct box *box_input_text(xmlNode *n, struct box_status *status,
		struct css_style *style, bool password)
{
	char *s;
	struct box *box = box_create(style, 0, 0,
			status->content->data.html.box_pool);
	struct box *inline_container, *inline_box;
	box->type = BOX_INLINE_BLOCK;

	box->gadget = form_new_control(GADGET_TEXTBOX);
	box->gadget->box = box;

	box->gadget->maxlength = 100;
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "maxlength"))) {
		box->gadget->maxlength = atoi(s);
		xmlFree(s);
	}

	s = (char *) xmlGetProp(n, (const xmlChar *) "value");
	box->gadget->value = strdup((s != NULL) ? s : "");
	box->gadget->initial_value = strdup(box->gadget->value);
	if (s)
		xmlFree(s);
	if (box->gadget->value == NULL || box->gadget->initial_value == NULL) {
		box_free(box);
		return NULL;
	}

	inline_container = box_create(0, 0, 0,
			status->content->data.html.box_pool);
	inline_container->type = BOX_INLINE_CONTAINER;
	inline_box = box_create(style, 0, 0,
			status->content->data.html.box_pool);
	inline_box->type = BOX_INLINE;
	inline_box->style_clone = 1;
	if (password) {
		box->gadget->type = GADGET_PASSWORD;
		inline_box->length = strlen(box->gadget->value);
		inline_box->text = malloc(inline_box->length + 1);
		memset(inline_box->text, '*', inline_box->length);
		inline_box->text[inline_box->length] = '\0';
	} else {
		box->gadget->type = GADGET_TEXTBOX;
		/* replace spaces/TABs with hard spaces to prevent line wrapping */
		inline_box->text = cnv_space2nbsp(box->gadget->value);
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
	struct box *box = box_create(style, 0, 0,
			status->content->data.html.box_pool);
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
	if ((s = xmlGetProp(n, (const xmlChar *) "name"))) {
		box->gadget->name = strdup((char *) s);
		xmlFree(s);
		if (!box->gadget->name) {
			box_free_box(box);
			return (struct box_result) {0, false, true};
		}
	}
	if ((s = xmlGetProp(n, (const xmlChar *) "value"))) {
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
 * print a box tree to stderr
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

	switch (box->type) {
		case BOX_BLOCK:            fprintf(stderr, "BOX_BLOCK "); break;
		case BOX_INLINE_CONTAINER: fprintf(stderr, "BOX_INLINE_CONTAINER "); break;
		case BOX_INLINE:           fprintf(stderr, "BOX_INLINE "); break;
		case BOX_INLINE_BLOCK:     fprintf(stderr, "BOX_INLINE_BLOCK "); break;
		case BOX_TABLE:            fprintf(stderr, "BOX_TABLE [columns %i] ",
		                                   box->columns); break;
		case BOX_TABLE_ROW:        fprintf(stderr, "BOX_TABLE_ROW "); break;
		case BOX_TABLE_CELL:       fprintf(stderr, "BOX_TABLE_CELL [columns %i, "
		                                   "start %i, rows %i] ", box->columns,
		                                   box->start_column, box->rows); break;
		case BOX_TABLE_ROW_GROUP:  fprintf(stderr, "BOX_TABLE_ROW_GROUP "); break;
		case BOX_FLOAT_LEFT:       fprintf(stderr, "BOX_FLOAT_LEFT "); break;
		case BOX_FLOAT_RIGHT:      fprintf(stderr, "BOX_FLOAT_RIGHT "); break;
		case BOX_BR:               fprintf(stderr, "BOX_BR "); break;
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
		fprintf(stderr, " -> '%s' ", box->href);
	if (box->title != 0)
		fprintf(stderr, "[%s]", box->title);
	fprintf(stderr, "\n");

	for (c = box->children; c != 0; c = c->next)
		box_dump(c, depth + 1);
}


/**
 * ensure the box tree is correctly nested
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

void box_normalise_block(struct box *block, pool box_pool)
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
				box_normalise_block(child, box_pool);
				break;
			case BOX_INLINE_CONTAINER:
				box_normalise_inline_container(child, box_pool);
				break;
			case BOX_TABLE:
				box_normalise_table(child, box_pool);
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
				style = xcalloc(1, sizeof(struct css_style));
				assert(block->style != NULL);
				memcpy(style, block->style, sizeof(struct css_style));
				css_cascade(style, &css_blank_style);
				table = box_create(style, block->href, 0, box_pool);
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
				box_normalise_table(table, box_pool);
				break;
			default:
				assert(0);
		}
	}
	LOG(("block %p done", block));
}


void box_normalise_table(struct box *table, pool box_pool)
{
	struct box *child;
	struct box *next_child;
	struct box *row_group;
	struct css_style *style;
	unsigned int *row_span = xcalloc(2, sizeof(row_span[0]));
	unsigned int table_columns = 1;

	assert(table != 0);
	assert(table->type == BOX_TABLE);
	LOG(("table %p", table));
	row_span[0] = row_span[1] = 0;

	for (child = table->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
			case BOX_TABLE_ROW_GROUP:
				/* ok */
				box_normalise_table_row_group(child, &row_span,
						&table_columns, box_pool);
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW:
			case BOX_TABLE_CELL:
				/* insert implied table row group */
				style = xcalloc(1, sizeof(struct css_style));
				assert(table->style != NULL);
				memcpy(style, table->style, sizeof(struct css_style));
				css_cascade(style, &css_blank_style);
				row_group = box_create(style, table->href, 0,
						box_pool);
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
				box_normalise_table_row_group(row_group, &row_span,
						&table_columns, box_pool);
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

	table->columns = table_columns;
	free(row_span);

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
}


void box_normalise_table_row_group(struct box *row_group,
		unsigned int **row_span, unsigned int *table_columns,
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
				box_normalise_table_row(child, row_span,
						table_columns, box_pool);
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_CELL:
				/* insert implied table row */
				style = xcalloc(1, sizeof(struct css_style));
				assert(row_group->style != NULL);
				memcpy(style, row_group->style, sizeof(struct css_style));
				css_cascade(style, &css_blank_style);
				row = box_create(style, row_group->href, 0,
						box_pool);
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
				box_normalise_table_row(row, row_span,
						table_columns, box_pool);
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
}


void box_normalise_table_row(struct box *row,
		unsigned int **row_span, unsigned int *table_columns,
		pool box_pool)
{
	struct box *child;
	struct box *next_child;
	struct box *cell;
	struct css_style *style;
	unsigned int columns = 0, i;

	assert(row != 0);
	assert(row->type == BOX_TABLE_ROW);
	LOG(("row %p", row));

	for (child = row->children; child != 0; child = next_child) {
		next_child = child->next;
		switch (child->type) {
			case BOX_TABLE_CELL:
				/* ok */
				box_normalise_block(child, box_pool);
				cell = child;
				break;
			case BOX_BLOCK:
			case BOX_INLINE_CONTAINER:
			case BOX_TABLE:
			case BOX_TABLE_ROW_GROUP:
			case BOX_TABLE_ROW:
				/* insert implied table cell */
				style = xcalloc(1, sizeof(struct css_style));
				assert(row->style != NULL);
				memcpy(style, row->style, sizeof(struct css_style));
				css_cascade(style, &css_blank_style);
				cell = box_create(style, row->href, 0, box_pool);
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
				box_normalise_block(cell, box_pool);
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

		/* skip columns with cells spanning from above */
		while ((*row_span)[columns] != 0)
			columns++;
		cell->start_column = columns;
		if (*table_columns < columns + cell->columns) {
			*table_columns = columns + cell->columns;
			*row_span = xrealloc(*row_span,
					sizeof((*row_span)[0]) *
					(*table_columns + 1));
			(*row_span)[*table_columns] = 0;  /* sentinel */
		}
		for (i = 0; i != cell->columns; i++)
			(*row_span)[columns + i] = cell->rows;
		columns += cell->columns;
	}

	for (i = 0; i != *table_columns; i++)
		if ((*row_span)[i] != 0)
			(*row_span)[i]--;

	if (row->children == 0) {
		LOG(("row->children == 0, removing"));
		if (row->prev == 0)
			row->parent->children = row->next;
		else
			row->prev->next = row->next;
		if (row->next != 0)
			row->next->prev = row->prev;
		box_free(row);
	}

	LOG(("row %p done", row));
}


void box_normalise_inline_container(struct box *cont, pool box_pool)
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
				box_normalise_block(child, box_pool);
				break;
			case BOX_FLOAT_LEFT:
			case BOX_FLOAT_RIGHT:
				/* ok */
				assert(child->children != 0);
				switch (child->children->type) {
					case BOX_BLOCK:
						box_normalise_block(child->children,
								box_pool);
						break;
					case BOX_TABLE:
						box_normalise_table(child->children,
								box_pool);
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
		if (!box->style_clone)
			free(box->style);
	}

	free(box->usemap);
	free(box->text);
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

	box = box_create(style, status->href, 0,
			status->content->data.html.box_pool);

        po = xcalloc(1, sizeof(*po));

        /* initialise po struct */
        po->data = 0;
        po->type = 0;
        po->codetype = 0;
        po->codebase = 0;
        po->classid = 0;
        po->params = 0;

        /* object data */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "data"))) {
		url = url_join(s, status->content->data.html.base_url);
		if (!url) {
			free(po);
			xmlFree(s);
			return (struct box_result) {box, true, true};
		}
		po->data = strdup(s);
		LOG(("object '%s'", po->data));
		xmlFree(s);
	}

	/* imagemap associated with this object */
	if ((map = xmlGetProp(n, (const xmlChar *) "usemap"))) {
	        if (map[0] == '#') {
	                box->usemap = xstrdup(map+1);
	        }
	        else {
	                box->usemap = xstrdup(map);
	        }
	        xmlFree(map);
	}

        /* object type */
        if ((s = (char *) xmlGetProp(n, (const xmlChar *) "type"))) {

                po->type = strdup(s);
                LOG(("type: %s", s));
                xmlFree(s);
        }

        /* object codetype */
        if ((s = (char *) xmlGetProp(n, (const xmlChar *) "codetype"))) {

                po->codetype = strdup(s);
                LOG(("codetype: %s", s));
                xmlFree(s);
        }

        /* object codebase */
        if ((s = (char *) xmlGetProp(n, (const xmlChar *) "codebase"))) {

                po->codebase = strdup(s);
                LOG(("codebase: %s", s));
                xmlFree(s);
        }

        /* object classid */
        if ((s = (char *) xmlGetProp(n, (const xmlChar *) "classid"))) {

                po->classid = strdup(s);
                LOG(("classid: %s", s));
                xmlFree(s);
        }

        /* parameters
         * parameter data is stored in a singly linked list.
         * po->params points to the head of the list.
         * new parameters are added to the head of the list.
         */
        for (c = n->children; c != 0; c = c->next) {
	    if (strcmp((const char *) c->name, "param") == 0) {

               pp = xcalloc(1, sizeof(*pp));

               /* initialise pp struct */
               pp->name = 0;
               pp->value = 0;
               pp->valuetype = 0;
               pp->type = 0;
               pp->next = 0;

               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "name"))) {
                   pp->name = strdup(s);
                   xmlFree(s);
               }
               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "value"))) {
                   pp->value = strdup(s);
                   xmlFree(s);
               }
               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "type"))) {
                   pp->type = strdup(s);
                   xmlFree(s);
               }
               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "valuetype"))) {
                   pp->valuetype = strdup(s);
                   xmlFree(s);
               }
               else {

                   pp->valuetype = strdup("data");
               }

               pp->next = po->params;
               po->params = pp;
	    }
	    else {
	            /* The first non-param child is the start of the
	             * alt html. Therefore, we should break out of this loop.
	             */
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

	box = box_create(style, status->href, 0,
			status->content->data.html.box_pool);

	po = xcalloc(1, sizeof(*po));

	/* initialise po struct */
        po->data = 0;
        po->type = 0;
        po->codetype = 0;
        po->codebase = 0;
        po->classid = 0;
        po->params = 0;

	/* embed src */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "src"))) {
		url = url_join(s, status->content->data.html.base_url);
		if (!url) {
			free(po);
			xmlFree(s);
			return (struct box_result) {box, false, true};
		}
		LOG(("embed '%s'", url));
                po->data = strdup(s);
	        xmlFree(s);
        }

        /**
         * we munge all other attributes into a plugin_parameter structure
         */
         for(a=n->properties; a!=0; a=a->next) {

                pp = xcalloc(1, sizeof(*pp));

                /* initialise pp struct */
                pp->name = 0;
                pp->value = 0;
                pp->valuetype = 0;
                pp->type = 0;
                pp->next = 0;

                if(strcasecmp((const char*)a->name, "src") != 0) {
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

	box = box_create(style, status->href, 0,
			status->content->data.html.box_pool);

	po = xcalloc(1, sizeof(*po));

	/* initialise po struct */
        po->data = 0;
        po->type = 0;
        po->codetype = 0;
        po->codebase = 0;
        po->classid = 0;
        po->params = 0;

	/* code */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "code"))) {
		url = url_join(s, status->content->data.html.base_url);
		if (!url) {
			free(po);
			xmlFree(s);
			return (struct box_result) {box, true, false};
		}
		LOG(("applet '%s'", url));
		po->classid = strdup(s);
		xmlFree(s);
	}

        /* object codebase */
        if ((s = (char *) xmlGetProp(n, (const xmlChar *) "codebase"))) {

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

               pp = xcalloc(1, sizeof(*pp));

               /* initialise pp struct */
               pp->name = 0;
               pp->value = 0;
               pp->valuetype = 0;
               pp->type = 0;
               pp->next = 0;

               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "name"))) {
                   pp->name = strdup(s);
                   xmlFree(s);
               }
               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "value"))) {
                   pp->value = strdup(s);
                   xmlFree(s);
               }
               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "type"))) {
                   pp->type = strdup(s);
                   xmlFree(s);
               }
               if ((s = (char *) xmlGetProp(c, (const xmlChar *) "valuetype"))) {
                   pp->valuetype = strdup(s);
                   xmlFree(s);
               }
               else {

                   pp->valuetype = strdup("data");
               }

               pp->next = po->params;
               po->params = pp;
	    }
	    else {
	            /* The first non-param child is the start of the
	             * alt html. Therefore, we should break out of this loop.
	             */
	             continue;
	    }
	}

        box->object_params = po;

	/* start fetch */
	if(plugin_decode(status->content, url, box, po))
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

	box = box_create(style, status->href, 0,
			status->content->data.html.box_pool);

	po = xcalloc(1, sizeof(*po));

	/* initialise po struct */
        po->data = 0;
        po->type = 0;
        po->codetype = 0;
        po->codebase = 0;
        po->classid = 0;
        po->params = 0;

	/* iframe src */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "src"))) {
		url = url_join(s, status->content->data.html.base_url);
		if (!url) {
			free(po);
			xmlFree(s);
			return (struct box_result) {box, false, true};
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
 * TODO: reformat, plug failure leaks
 */
bool plugin_decode(struct content* content, char* url, struct box* box,
                  struct object_params* po)
{
  struct plugin_params * pp;

  /* Check if the codebase attribute is defined.
   * If it is not, set it to the codebase of the current document.
   */
   if(po->codebase == 0)
           po->codebase = url_join("./", content->data.html.base_url);
   else
           po->codebase = url_join(po->codebase, content->data.html.base_url);

  if (!po->codebase)
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
   if(po->data == 0 && po->classid == 0) {
           return false;
   }
   if(po->data == 0 && po->classid != 0) {
           if(strncasecmp(po->classid, "clsid:", 6) == 0) {
                   /* Flash */
                   if(strcasecmp(po->classid, "clsid:D27CDB6E-AE6D-11cf-96B8-444553540000") == 0) {
                           for(pp = po->params; pp != 0 &&
                               (strcasecmp(pp->name, "movie") != 0);
                               pp = pp->next);
                           if(pp == 0)
                           	   return false;
                           url = url_join(pp->value, po->basehref);
                           if (!url)
                           	   return false;
                           /* munge the codebase */
                           po->codebase = url_join("./",
					   content->data.html.base_url);
                           if (!po->codebase)
                           	   return false;
                   }
                   else {
                           LOG(("ActiveX object - n0"));
                           return false;
                   }
           }
           else {
                   url = url_join(po->classid, po->codebase);
                   if (!url)
                   	   return false;

                   /* The java plugin doesn't need the .class extension
                    * so we strip it.
                    */
                   if(strcasecmp((&po->classid[strlen(po->classid)-6]),
                                                            ".class") == 0)
                           po->classid[strlen(po->classid)-6] = 0;
           }
   }
   else {
           url = url_join(po->data, po->codebase);
           if (!url)
           	return false;
   }

   /* Check if the declared mime type is understandable.
    * Checks type and codetype attributes.
    */
    if(po->type != 0) {
           if (content_lookup(po->type) == CONTENT_OTHER)
                  return false;
    }
    if(po->codetype != 0) {
           if (content_lookup(po->codetype) == CONTENT_OTHER)
                  return false;
    }

  /* If we've got to here, the object declaration has provided us with
   * enough data to enable us to have a go at downloading and displaying it.
   *
   * We may still find that the object has a MIME type that we can't handle
   * when we fetch it (if the type was not specified or is different to that
   * given in the attributes).
   */
   html_fetch_object(content, url, box, 0, 1000, 1000, false);

   return true;
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
		if (box->type == BOX_FLOAT_LEFT ||
				box->type == BOX_FLOAT_RIGHT) {
			do {
				box = box->parent;
			} while (!box->float_children);
		} else
			box = box->parent;
		*x += box->x;
		*y += box->y;
	}
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
	struct box_result r;
	struct box_multi_length *row_height = 0, *col_width = 0;
	xmlNode *c;

	box = box_create(style, 0, status->title,
			status->content->data.html.box_pool);
	box->type = BOX_TABLE;

	/* parse rows and columns */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "rows"))) {
		row_height = box_parse_multi_lengths(s, &rows);
		xmlFree(s);
		if (!row_height) {
			box_free_box(box);
			return (struct box_result) {0, false, true};
		}
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "cols"))) {
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
		row_style = malloc(sizeof (struct css_style));
		if (!row_style) {
			free(row_height);
			free(col_width);
			return (struct box_result) {box, false, true};
		}
		memcpy(row_style, style, sizeof (struct css_style));
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
		row_box = box_create(row_style, 0, 0,
				status->content->data.html.box_pool);
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

			cell_box = box_create(style, 0, 0,
					status->content->data.html.box_pool);
			cell_box->type = BOX_TABLE_CELL;
			cell_box->style_clone = 1;
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

			object_box = box_create(style, 0, 0,
					status->content->data.html.box_pool);
			object_box->type = BOX_BLOCK;
			object_box->style_clone = 1;
			box_add_child(cell_box, object_box);

			if (!(s = (char *) xmlGetProp(c,
					(const xmlChar *) "src"))) {
				c = c->next;
				continue;
			}

			s1 = strip(s);
			url = url_join(s1, status->content->data.html.base_url);
			if (!url) {
				xmlFree(s);
				c = c->next;
				continue;
			}

			LOG(("frame, url '%s'", url));

			html_fetch_object(status->content, url, object_box, 0,
					object_width, object_height, false);
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
	unsigned int i, n = 1;
	struct box_multi_length *length;

	for (i = 0; s[i]; i++)
		if (s[i] == ',')
			n++;

	length = malloc(sizeof *length * n);
	if (!length)
		return 0;

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
