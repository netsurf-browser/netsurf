/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2008 Michael Drake <tlsa@netsurf-browser.org>
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
 * Conversion of XML tree to box tree (implementation).
 */

#define _GNU_SOURCE  /* for strndup */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libxml/HTMLparser.h>
#include <libxml/parserInternals.h>
#include "utils/config.h"
#include "content/content.h"
#include "css/css.h"
#include "desktop/browser.h"
#include "desktop/options.h"
#include "render/box.h"
#include "render/form.h"
#include "render/html.h"
#ifdef riscos
#include "desktop/gui.h"
#endif
//#define NDEBUG
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/url.h"
#include "utils/utils.h"


static const content_type image_types[] = {
#ifdef WITH_JPEG
	CONTENT_JPEG,
#endif
#ifdef WITH_GIF
	CONTENT_GIF,
#endif
#ifdef WITH_BMP
	CONTENT_BMP,
#endif
#ifdef WITH_MNG
	CONTENT_PNG,
	CONTENT_JNG,
	CONTENT_MNG,
#endif
#ifdef WITH_SPRITE
	CONTENT_SPRITE,
#endif
#ifdef WITH_DRAW
	CONTENT_DRAW,
#endif
#ifdef WITH_ARTWORKS
	CONTENT_ARTWORKS,
#endif
	CONTENT_UNKNOWN };

#define MAX_SPAN (100)


/* the strings are not important, since we just compare the pointers */
const char *TARGET_SELF = "_self";
const char *TARGET_PARENT = "_parent";
const char *TARGET_TOP = "_top";
const char *TARGET_BLANK = "_blank";

/* keeps track of markup presentation */
struct markup_track {
	enum {
		ALIGN_NONE,
		ALIGN_LEFT,
		ALIGN_CENTER,
		ALIGN_RIGHT
	} align;
	bool cell_border;
	colour border_color;

	bool cell_padding;
	long padding_width;
};

static bool convert_xml_to_box(xmlNode *n, struct content *content,
		struct css_style *parent_style,
		struct box *parent, struct box **inline_container,
		char *href, const char *target, char *title,
		struct markup_track markup_track,
		struct css_importance *author);
bool box_construct_element(xmlNode *n, struct content *content,
		struct css_style *parent_style,
		struct box *parent, struct box **inline_container,
		char *href, const char *target, char *title,
		struct markup_track markup_track,
		struct css_importance *author);
bool box_construct_text(xmlNode *n, struct content *content,
		struct css_style *parent_style,
		struct box *parent, struct box **inline_container,
		char *href, const char *target, char *title);
static struct css_style * box_get_style(struct content *c,
		struct css_style *parent_style,
		xmlNode *n, struct markup_track *markup_track,
		struct css_importance *author);
static void box_solve_display(struct css_style *style, bool root);
static void box_text_transform(char *s, unsigned int len,
		css_text_transform tt);
#define BOX_SPECIAL_PARAMS xmlNode *n, struct content *content, \
		struct box *box, bool *convert_children, \
		struct markup_track markup_track, \
		struct css_importance *author
static bool box_a(BOX_SPECIAL_PARAMS);
static bool box_body(BOX_SPECIAL_PARAMS);
static bool box_br(BOX_SPECIAL_PARAMS);
static bool box_image(BOX_SPECIAL_PARAMS);
static bool box_form(BOX_SPECIAL_PARAMS);
static bool box_textarea(BOX_SPECIAL_PARAMS);
static bool box_select(BOX_SPECIAL_PARAMS);
static bool box_input(BOX_SPECIAL_PARAMS);
static bool box_input_text(BOX_SPECIAL_PARAMS, bool password);
static bool box_button(BOX_SPECIAL_PARAMS);
static bool box_frameset(BOX_SPECIAL_PARAMS);
static bool box_create_frameset(struct content_html_frames *f, xmlNode *n,
		struct content *content);
static bool box_select_add_option(struct form_control *control, xmlNode *n);
static bool box_object(BOX_SPECIAL_PARAMS);
static bool box_embed(BOX_SPECIAL_PARAMS);
static bool box_pre(BOX_SPECIAL_PARAMS);
/*static bool box_applet(BOX_SPECIAL_PARAMS);*/
static bool box_iframe(BOX_SPECIAL_PARAMS);
static bool box_get_attribute(xmlNode *n, const char *attribute,
		void *context, char **value);
static struct frame_dimension *box_parse_multi_lengths(const char *s,
		unsigned int *count);
static void parse_inline_colour(char *text, colour *variable);


/* element_table must be sorted by name */
struct element_entry {
	char name[10];	 /* element type */
	bool (*convert)(BOX_SPECIAL_PARAMS);
};
static const struct element_entry element_table[] = {
	{"a", box_a},
/*	{"applet", box_applet},*/
	{"body", box_body},
	{"br", box_br},
	{"button", box_button},
	{"embed", box_embed},
	{"form", box_form},
	{"frameset", box_frameset},
	{"iframe", box_iframe},
	{"image", box_image},
	{"img", box_image},
	{"input", box_input},
	{"object", box_object},
	{"pre", box_pre},
	{"select", box_select},
	{"textarea", box_textarea}
};
#define ELEMENT_TABLE_COUNT (sizeof(element_table) / sizeof(element_table[0]))

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
	struct box *inline_container = 0;
	struct css_importance author;
	struct markup_track markup_track;
	markup_track.cell_border = false;
	markup_track.cell_padding = false;
	markup_track.align = ALIGN_NONE;

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

	c->data.html.style = talloc_memdup(c, &css_base_style,
			sizeof css_base_style);
	if (!c->data.html.style)
		return false;
	c->data.html.style->font_size.value.length.value =
			option_font_size * 0.1;
	/* and get the default font family from the options */
	c->data.html.style->font_family = option_font_default;

	c->data.html.object_count = 0;
	c->data.html.object = 0;

	if (!convert_xml_to_box(n, c, c->data.html.style, &root,
			&inline_container, 0, 0, 0, markup_track, &author))
		return false;

	if (!box_normalise_block(&root, c))
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
 * \param  n		 fragment of xml tree
 * \param  content	 content of type CONTENT_HTML that is being processed
 * \param  parent_style  style at this point in xml tree
 * \param  parent	 parent in box tree
 * \param  inline_container  current inline container box, or 0, updated to
 *			 new current inline container on exit
 * \param  href		 current link URL, or 0 if not in a link
 * \param  target	 current link target, or 0 if none
 * \param  title	 current title, or 0 if none
 * \param  markup_track	 track presentation markup that affects descendents
 * \param  author	 denotes whether current style has author level
 *			 importance for certain properties
 * \return  true on success, false on memory exhaustion
 */

bool convert_xml_to_box(xmlNode *n, struct content *content,
		struct css_style *parent_style,
		struct box *parent, struct box **inline_container,
		char *href, const char *target, char *title,
		struct markup_track markup_track,
		struct css_importance *author)
{
	switch (n->type) {
	case XML_ELEMENT_NODE:
		return box_construct_element(n, content, parent_style, parent,
				inline_container,
				href, target, title, markup_track, author);
	case XML_TEXT_NODE:
		return box_construct_text(n, content, parent_style, parent,
				inline_container, href, target, title);
	default:
		/* not an element or text node: ignore it (eg. comment) */
		return true;
	}
}


/**
 * Construct the box tree for an XML element.
 *
 * \param  n		 XML node of type XML_ELEMENT_NODE
 * \param  content	 content of type CONTENT_HTML that is being processed
 * \param  parent_style  style at this point in xml tree
 * \param  parent	 parent in box tree
 * \param  inline_container  current inline container box, or 0, updated to
 *			 new current inline container on exit
 * \param  href		 current link URL, or 0 if not in a link
 * \param  target	 current link target, or 0 if none
 * \param  title	 current title, or 0 if none
 * \param  markup_track	 track presentation markup that affects descendents
 * \param  author	 denotes whether current style has author level
 *			 importance for certain properties
 * \return  true on success, false on memory exhaustion
 */

bool box_construct_element(xmlNode *n, struct content *content,
		struct css_style *parent_style,
		struct box *parent, struct box **inline_container,
		char *href, const char *target, char *title,
		struct markup_track markup_track,
		struct css_importance *author)
{
	bool convert_children = true;
	char *id = 0;
	char *s;
	struct box *box = 0;
	struct box *inline_container_c;
	struct box *inline_end;
	struct css_style *style = 0;
	struct element_entry *element;
	xmlChar *title0;
	xmlNode *c;

	assert(n);
	assert(n->type == XML_ELEMENT_NODE);
	assert(parent_style);
	assert(parent);
	assert(inline_container);

	gui_multitask();

	/* In case the parent is a pre block, we clear the
	 * strip_leading_newline flag since it is not used if we
	 * follow the pre with a tag
	 */
	parent->strip_leading_newline = 0;

	style = box_get_style(content, parent_style, n, &markup_track, author);
	if (!style)
		return false;

	/* extract title attribute, if present */
	if ((title0 = xmlGetProp(n, (const xmlChar *) "title"))) {
		char *title1 = squash_whitespace(title0);
		xmlFree(title0);
		if (!title1)
			return false;
		title = talloc_strdup(content, title1);
		free(title1);
		if (!title)
			return false;
	}

	/* extract id attribute, if present */
	if (!box_get_attribute(n, "id", content, &id))
		return false;

	/* create box for this element */
	box = box_create(style, href, target, title, id, content);
	if (!box)
		return false;
	/* set box type from style */
	box->type = box_map[style->display];

	/* special elements */
	element = bsearch((const char *) n->name, element_table,
			ELEMENT_TABLE_COUNT, sizeof(element_table[0]),
			(int (*)(const void *, const void *)) strcmp);
	if (element) {
		/* a special convert function exists for this element */
		if (!element->convert(n, content, box, &convert_children,
							markup_track, author))
			return false;
		href = box->href;
		target = box->target;
	}

	if (style->display == CSS_DISPLAY_NONE) {
		/* Free style and invalidate box's style pointer */
		talloc_free(style);
		box->style = NULL;

		/* If this box has an associated gadget, invalidate the
		 * gadget's box pointer and our pointer to the gadget. */
		if (box->gadget) {
			box->gadget->box = NULL;
			box->gadget = NULL;
		}

		/* We can't do this, as it will destroy any gadget
		 * associated with the box, thus making any form usage
		 * access freed memory. The box is in the talloc context,
		 * anyway, so will get cleaned up with the content. */
		/* box_free_box(box); */
		return true;
	}

	if (!*inline_container &&
			(box->type == BOX_INLINE ||
			box->type == BOX_BR ||
			box->type == BOX_INLINE_BLOCK ||
			style->float_ == CSS_FLOAT_LEFT ||
			style->float_ == CSS_FLOAT_RIGHT)) {
		/* this is the first inline in a block: make a container */
		*inline_container = box_create(0, 0, 0, 0, 0, content);
		if (!*inline_container)
			return false;
		(*inline_container)->type = BOX_INLINE_CONTAINER;
		box_add_child(parent, *inline_container);
	}

	if (box->type == BOX_INLINE || box->type == BOX_BR) {
		/* inline box: add to tree and recurse */
		box_add_child(*inline_container, box);
		if (convert_children && n->children) {
			for (c = n->children; c; c = c->next)
				if (!convert_xml_to_box(c, content, style,
						parent, inline_container,
						href, target, title,
						markup_track, author))
					return false;
			inline_end = box_create(style, href, target, title, id,
					content);
			if (!inline_end)
				return false;
			inline_end->type = BOX_INLINE_END;
			if (*inline_container)
				box_add_child(*inline_container, inline_end);
			else
				box_add_child(box->parent, inline_end);
			box->inline_end = inline_end;
			inline_end->inline_end = box;
		}
	} else if (box->type == BOX_INLINE_BLOCK) {
		/* inline block box: add to tree and recurse */
		box_add_child(*inline_container, box);
		inline_container_c = 0;
		for (c = n->children; convert_children && c; c = c->next)
			if (!convert_xml_to_box(c, content, style, box,
					&inline_container_c,
					href, target, title, markup_track,
					author))
				return false;
	} else {
		if (style->float_ == CSS_FLOAT_LEFT ||
				style->float_ == CSS_FLOAT_RIGHT) {
			/* float: insert a float box between the parent and
			 * current node */
			parent = box_create(0, href, target, title, 0, content);
			if (!parent)
				return false;
			if (style->float_ == CSS_FLOAT_LEFT)
				parent->type = BOX_FLOAT_LEFT;
			else
				parent->type = BOX_FLOAT_RIGHT;
			box_add_child(*inline_container, parent);
		}

		if (style->display == CSS_DISPLAY_LIST_ITEM) {
			struct box *marker;
			marker = box_create(style, 0, 0, title, 0, content);
			if (!marker)
				return false;
			marker->type = BOX_BLOCK;
			/** \todo marker content (list-style-type) */
			switch (style->list_style_type) {
			case CSS_LIST_STYLE_TYPE_DISC:
			default:
				/* 2022 BULLET */
				marker->text = "\342\200\242";
				marker->length = 3;
				break;
			case CSS_LIST_STYLE_TYPE_CIRCLE:
				/* 25CB WHITE CIRCLE */
				marker->text = "\342\227\213";
				marker->length = 3;
				break;
			case CSS_LIST_STYLE_TYPE_SQUARE:
				/* 25AA BLACK SMALL SQUARE */
				marker->text = "\342\226\252";
				marker->length = 3;
				break;
			case CSS_LIST_STYLE_TYPE_DECIMAL:
			case CSS_LIST_STYLE_TYPE_LOWER_ALPHA:
			case CSS_LIST_STYLE_TYPE_LOWER_ROMAN:
			case CSS_LIST_STYLE_TYPE_UPPER_ALPHA:
			case CSS_LIST_STYLE_TYPE_UPPER_ROMAN:
				if (parent->last && parent->last->list_marker)
					marker->rows = parent->last->
							list_marker->rows + 1;
				marker->text = talloc_array(content, char, 20);
				if (!marker->text)
					return false;
				snprintf(marker->text, 20, "%u.", marker->rows);
				marker->length = strlen(marker->text);
				break;
			case CSS_LIST_STYLE_TYPE_NONE:
				marker->text = 0;
				marker->length = 0;
				break;
			}
			if (style->list_style_image.type ==
					CSS_LIST_STYLE_IMAGE_URI) {
				if (!html_fetch_object(content,
						style->list_style_image.uri,
						marker,
						0, content->available_width,
						1000, false))
					return false;
			}
			box->list_marker = marker;
			marker->parent = box;
		}

		/* non-inline box: add to tree and recurse */
		box_add_child(parent, box);
		inline_container_c = 0;
		for (c = n->children; convert_children && c; c = c->next)
			if (!convert_xml_to_box(c, content, style, box,
					&inline_container_c,
					href, target, title, markup_track,
					author))
				return false;
		if (style->float_ == CSS_FLOAT_NONE)
			/* new inline container unless this is a float */
			*inline_container = 0;
	}

	/* misc. attributes that can't be handled in box_get_style() */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "colspan"))) {
	  	if (isdigit(s[0])) {
			box->columns = strtol(s, NULL, 10);
			if ((MAX_SPAN < box->columns) || (box->columns < 1))
				box->columns = 1;
		}
		xmlFree(s);
	}
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "rowspan"))) {
	  	if (isdigit(s[0])) {
			box->rows = strtol(s, NULL, 10);
			if ((MAX_SPAN < box->rows) || (box->rows < 1))
				box->rows = 1;
		}
		xmlFree(s);
	}

	/* transfer <tr height="n"> down to the <td> elements */
	/* \todo move this into box_get_style() */
	if (strcmp((const char *) n->name, "tr") == 0) {
		if ((s = (char *) xmlGetProp(n,
				(const xmlChar *) "height"))) {
			float value = atof(s);
			if (value < 0 || strlen(s) == 0) {
				/* ignore negative values and height="" */
			} else if (strrchr(s, '%')) {
				/* the specification doesn't make clear what
				 * percentage heights mean, so ignore them */
			} else {
				/* The tree is not normalized yet, so accept
				 * cells not in rows and rows not in row
				 * groups. */
				struct box *child;
				float current;
				for (child = box->children; child;
							child = child->next) {
					if (child->type == BOX_TABLE_CELL) {
						current = css_len2px(
							&child->style->height.
							length, child->style);
						value = (value > current) ?
							value : current;
						child->style->height.height =
							CSS_HEIGHT_LENGTH;
						child->style->height.length.
							unit = CSS_UNIT_PX;
						child->style->height.length.
							value = value;
					}
				}
			}
			xmlFree(s);
		}
	}

	/* fetch any background image for this box */
	if (style->background_image.type == CSS_BACKGROUND_IMAGE_URI) {
		if (!html_fetch_object(content, style->background_image.uri,
				box, image_types, content->available_width,
				1000, true))
			return false;
	}

	return true;
}


/**
 * Construct the box tree for an XML text node.
 *
 * \param  n		 XML node of type XML_TEXT_NODE
 * \param  content	 content of type CONTENT_HTML that is being processed
 * \param  parent_style  style at this point in xml tree
 * \param  parent	 parent in box tree
 * \param  inline_container  current inline container box, or 0, updated to
 *			 new current inline container on exit
 * \param  href		 current link URL, or 0 if not in a link
 * \param  target	 current link target, or 0 if none
 * \param  title	 current title, or 0 if none
 * \return  true on success, false on memory exhaustion
 */

bool box_construct_text(xmlNode *n, struct content *content,
		struct css_style *parent_style,
		struct box *parent, struct box **inline_container,
		char *href, const char *target, char *title)
{
	struct box *box = 0;

	assert(n);
	assert(n->type == XML_TEXT_NODE);
	assert(parent_style);
	assert(parent);
	assert(inline_container);

	if (parent_style->white_space == CSS_WHITE_SPACE_NORMAL ||
			 parent_style->white_space == CSS_WHITE_SPACE_NOWRAP) {
		char *text = squash_whitespace(n->content);
		if (!text)
			return false;

		/* if the text is just a space, combine it with the preceding
		 * text node, if any */
		if (text[0] == ' ' && text[1] == 0) {
			if (*inline_container) {
				if ((*inline_container)->last == 0) {
					LOG(("empty inline_container %p",
							*inline_container));
					while (parent->parent &&
							parent->parent->parent)
						parent = parent->parent;
					box_dump(stderr, parent, 0);
				}
				assert((*inline_container)->last != 0);
				(*inline_container)->last->space = 1;
			}
			free(text);
			return true;
		}

		if (!*inline_container) {
			/* this is the first inline node: make a container */
			*inline_container = box_create(0, 0, 0, 0, 0, content);
			if (!*inline_container) {
				free(text);
				return false;
			}
			(*inline_container)->type = BOX_INLINE_CONTAINER;
			box_add_child(parent, *inline_container);
		}

		box = box_create(parent_style, href, target, title, 0, content);
		if (!box) {
			free(text);
			return false;
		}
		box->type = BOX_TEXT;
		box->text = talloc_strdup(content, text);
		free(text);
		if (!box->text)
			return false;
		box->length = strlen(box->text);
		/* strip ending space char off */
		if (box->length > 1 && box->text[box->length - 1] == ' ') {
			box->space = 1;
			box->length--;
		}
		if (parent_style->text_transform != CSS_TEXT_TRANSFORM_NONE)
			box_text_transform(box->text, box->length,
					parent_style->text_transform);
		if (parent_style->white_space == CSS_WHITE_SPACE_NOWRAP) {
			unsigned int i;
			for (i = 0; i != box->length &&
						box->text[i] != ' '; ++i)
				; /* no body */
			if (i != box->length) {
				/* there is a space in text block and we
				 * want all spaces to be converted to NBSP
				 */
				/*box->text = cnv_space2nbsp(text);
				if (!box->text) {
					free(text);
					goto no_memory;
				}
				box->length = strlen(box->text);*/
			}
		}

		box_add_child(*inline_container, box);
		if (box->text[0] == ' ') {
			box->length--;
			memmove(box->text, &box->text[1], box->length);
			if (box->prev != NULL)
				box->prev->space = 1;
		}

	} else {
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
			return false;
		if (parent_style->text_transform != CSS_TEXT_TRANSFORM_NONE)
			box_text_transform(text, strlen(text),
					parent_style->text_transform);
		current = text;

		/* swallow a single leading new line */
		if (parent->strip_leading_newline) {
			switch (*current) {
			case '\n':
				current++; break;
			case '\r':
				current++;
				if (*current == '\n') current++;
				break;
			}
			parent->strip_leading_newline = 0;
		}

		do {
			size_t len = strcspn(current, "\r\n");
			char old = current[len];
			current[len] = 0;
			if (!*inline_container) {
				*inline_container = box_create(0, 0, 0, 0, 0,
						content);
				if (!*inline_container) {
					free(text);
					return false;
				}
				(*inline_container)->type =
						BOX_INLINE_CONTAINER;
				box_add_child(parent, *inline_container);
			}
			box = box_create(parent_style, href, target, title, 0,
					content);
			if (!box) {
				free(text);
				return false;
			}
			box->type = BOX_TEXT;
			box->text = talloc_strdup(content, current);
			if (!box->text) {
				free(text);
				return false;
			}
			box->length = strlen(box->text);
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
	}

	return true;
}


/**
 * Get the style for an element.
 *
 * \param  c		 content of type CONTENT_HTML that is being processed
 * \param  parent_style  style at this point in xml tree
 * \param  n		 node in xml tree
 * \param  markup_track	 track presentation markup that affects descendents
 * \param  author	 denotes whether current style has author level
 *			 importance for certain properties
 * \return  the new style, or 0 on memory exhaustion
 *
 * The style is collected from three sources:
 *  1. any styles for this element in the document stylesheet(s)
 *  2. the 'style' attribute
 *  3. non-CSS HTML attributes (subject to importance of CSS style properties)
 */

struct css_style * box_get_style(struct content *c,
		struct css_style *parent_style,
		xmlNode *n, struct markup_track *markup_track,
		struct css_importance *author)
{
	char *s;
	struct css_style *style;
	struct css_style *style_new;
	char *url;
	url_func_result res;
	colour border_color = 0x888888; /* mid-grey default for tables */

	/* if not in a table, switch off cellpadding and cell borders */
	if (strcmp(n->name, "thead") != 0 &&
			strcmp(n->name, "tbody") != 0 &&
			strcmp(n->name, "tfoot") != 0 &&
			strcmp(n->name, "tr") != 0 &&
			strcmp(n->name, "td") != 0 &&
			strcmp(n->name, "th") != 0 &&
			strcmp(n->name, "col") != 0 &&
			strcmp(n->name, "colgroup") != 0) {
		markup_track->cell_border = false;
		markup_track->cell_padding = false;
	}

	style = talloc_memdup(c, parent_style, sizeof *style);
	if (!style)
		return 0;

	style_new = talloc_memdup(c, &css_blank_style, sizeof *style_new);
	if (!style_new)
		return 0;
	css_get_style(c->data.html.working_stylesheet, n, style_new, author);
	css_cascade(style, style_new, NULL);

	/* style_new isn't needed past this point */
	talloc_free(style_new);

	/* Handle style attribute. (style attribute values have high enough
	 * specificity to override existing style data.) */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "style"))) {
		struct css_style *astyle;
		astyle = css_duplicate_style(&css_empty_style);
		if (!astyle) {
			xmlFree(s);
			return 0;
		}
		css_parse_property_list(c, astyle, s);
		css_cascade(style, astyle, author);
		css_free_style(astyle);
		xmlFree(s);
	}

	/* Apply presentational HTML attributes to style
	 * (Only apply if style property does not have "author" level
	 * importance or higher.)
	 */

	/* This property only applies to the body element, if you believe
	 * the spec. Many browsers seem to allow it on other elements too,
	 * so let's be generic ;) */
	if (!author->background_image && (s = (char *) xmlGetProp(n,
					(const xmlChar *) "background"))) {
		res = url_join(s, c->data.html.base_url, &url);
		xmlFree(s);
		if (res == URL_FUNC_NOMEM) {
			return 0;
		} else if (res == URL_FUNC_OK) {
			/* if url is equivalent to the parent's url,
			 * we've got infinite inclusion: ignore */
			if (strcmp(url, c->data.html.base_url) == 0)
				free(url);
			else {
				style->background_image.type =
						CSS_BACKGROUND_IMAGE_URI;
				style->background_image.uri = talloc_strdup(
						c, url);
				free(url);
				if (!style->background_image.uri)
					return 0;
			}
		}
	}

	if (!author->background_color && (s = (char *) xmlGetProp(n,
						(const xmlChar *) "bgcolor"))) {
		parse_inline_colour(s, &style->background_color);
		xmlFree(s);
	}

	if (!author->color && (s = (char *) xmlGetProp(n,
						(const xmlChar *) "color"))) {
		parse_inline_colour(s, &style->color);
		xmlFree(s);
	}

	if (!author->height && (s = (char *) xmlGetProp(n,
						(const xmlChar *) "height"))) {
		float value = isdigit(s[0]) ? atof(s) : -1;
		if (value <= 0 || strlen(s) == 0) {
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

	if (!author->width && strcmp((const char *) n->name, "input") == 0) {
		int size = -1;
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "size"))) {
			size = isdigit(s[0]) ? atoi(s): -1;
			if (0 < size) {
				char *type = (char *) xmlGetProp(n,
						(const xmlChar *) "type");
				style->width.width = CSS_WIDTH_LENGTH;
				if (!type || strcasecmp(type, "text") == 0 ||
					strcasecmp(type, "password") == 0)
					/* in characters for text, password */
					style->width.value.length.unit =
							CSS_UNIT_EX;
				else if (strcasecmp(type, "file") != 0)
					/* in pixels otherwise; ignore width
					 * on file, because we do them
					 * differently to most browsers */
					style->width.value.length.unit =
							CSS_UNIT_PX;
				style->width.value.length.value = size;
				if (type)
					xmlFree(type);
			}
			xmlFree(s);
		}
		/* If valid maxlength value is provided, the size attribute is
		 * unset and maxlength is small, use it to reduce input width
		 * to sensible size */
		if ((s = (char *) xmlGetProp(n, (const xmlChar *)
								"maxlength"))) {
			int maxlength = isdigit(s[0]) ? atoi(s): -1;
			if (0 < maxlength && size == -1 && maxlength < 10) {
				/* Bump up really small widths */
				maxlength = maxlength < 5 ? maxlength + 1 :
								maxlength;
				char *type = (char *) xmlGetProp(n,
						(const xmlChar *) "type");
				style->width.width = CSS_WIDTH_LENGTH;
				if (!type || strcasecmp(type, "text") == 0 ||
					strcasecmp(type, "password") == 0)
					/* in characters for text, password */
					style->width.value.length.unit =
							CSS_UNIT_EX;
				style->width.value.length.value = maxlength;
				if (type)
					xmlFree(type);
			}
			xmlFree(s);
		}
	}

	if (!author->color && strcmp((const char *) n->name, "body") == 0) {
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "text"))) {
		  	parse_inline_colour(s, &style->color);
			xmlFree(s);
		}
	}

	if (!author->width && (s = (char *) xmlGetProp(n,
						(const xmlChar *) "width"))) {
		float value = isdigit(s[0]) ? atof(s) : -1;
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
		if (!author->height && (s = (char *) xmlGetProp(n,
						(const xmlChar *) "rows"))) {
			int value = isdigit(s[0]) ? atoi(s): -1;
			if (0 < value) {
				style->height.height = CSS_HEIGHT_LENGTH;
				style->height.length.unit = CSS_UNIT_EM;
				style->height.length.value = value;
			}
			xmlFree(s);
		}
		if (!author->width && (s = (char *) xmlGetProp(n,
						(const xmlChar *) "cols"))) {
			int value = isdigit(s[0]) ? atoi(s): -1;
			if (0 < value) {
				style->width.width = CSS_WIDTH_LENGTH;
				style->width.value.length.unit = CSS_UNIT_EX;
				style->width.value.length.value = value;
			}
			xmlFree(s);
		}
	}

	if (strcmp((const char *) n->name, "table") == 0) {
		if (!author->border_spacing && (s = (char *) xmlGetProp(n,
					(const xmlChar *) "cellspacing"))) {
			/* percentage cellspacing not implemented */
			if (!strrchr(s, '%')) {
				int value = isdigit(s[0]) ? atoi(s): -1;
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
			xmlFree(s);
		}

		if ((s = (char *) xmlGetProp(n,
					(const xmlChar *) "bordercolor"))) {
			parse_inline_colour(s, &border_color);
			xmlFree(s);
		}
		if ((s = (char *) xmlGetProp(n,
				(const xmlChar *) "border"))) {
			int border_width = atoi(s);
			/* precentage border width not implemented */
			if (!strrchr(s, '%') && 0 < border_width) {
				for (unsigned int i = 0; i != 4; i++) {
					if (!author->border_color[i])
						style->border[i].color =
							border_color;
					if (!author->border_width[i]) {
						style->border[i].width.width =
							CSS_BORDER_WIDTH_LENGTH;
						style->border[i].width.value.
							value = border_width;
						style->border[i].width.value.
							unit = CSS_UNIT_PX;
					}
					if (!author->border_style[i])
						style->border[i].style =
							CSS_BORDER_STYLE_OUTSET;
				}
			}
			xmlFree(s);
		}
	}

	if (strcmp((const char *) n->name, "td") == 0 ||
			strcmp((const char *) n->name, "th") == 0) {
		/* set any cellborders stipulated by associated table */
		if (markup_track->cell_border) {
			for (unsigned int i = 0; i != 4; i++) {
				if (!author->border_color[i])
					style->border[i].color = markup_track->
								border_color;
				if (!author->border_width[i]) {
					style->border[i].width.width =
							CSS_BORDER_WIDTH_LENGTH;
					style->border[i].width.value.value = 1;
					style->border[i].width.value.unit =
							CSS_UNIT_PX;
				}
				if (!author->border_style[i])
					style->border[i].style =
							CSS_BORDER_STYLE_INSET;
			}
		}
		/* set any cellpadding stipulated by associated table */
		if (markup_track->cell_padding) {
			for (unsigned int i = 0; i != 4; i++) {
				if (!author->padding[i]) {
					style->padding[i].padding =
						CSS_PADDING_LENGTH;
					style->padding[i].value.length.value =
						markup_track->padding_width;
					style->padding[i].value.length.unit =
						CSS_UNIT_PX;
				}
			}
		}
	}

	if ((strcmp((const char *) n->name, "img") == 0) ||
			(strcmp((const char *) n->name, "image") == 0) ||
			(strcmp((const char *) n->name, "applet") == 0)) {
		if ((s = (char *) xmlGetProp(n,
				(const xmlChar *) "hspace"))) {
			/* percentage hspace not implemented */
			if (!strrchr(s, '%')) {
				int value = isdigit(s[0]) ? atoi(s): -1;
				if (0 <= value && !author->margin[LEFT]) {
					style->margin[LEFT].margin =
							CSS_MARGIN_LENGTH;
					style->margin[LEFT].value.length.value =
							value;
					style->margin[LEFT].value.length.unit =
							CSS_UNIT_PX;
				}
				if (0 <= value && !author->margin[RIGHT]) {
					style->margin[RIGHT].margin =
							CSS_MARGIN_LENGTH;
					style->margin[RIGHT].value.length.
							value = value;
					style->margin[RIGHT].value.length.unit =
							CSS_UNIT_PX;
				}
			}
			xmlFree(s);
		}
		if ((s = (char *) xmlGetProp(n,
				(const xmlChar *) "vspace"))) {
			/* percentage vspace not implemented */
			if (!strrchr(s, '%')) {
				int value = isdigit(s[0]) ? atoi(s): -1;
				if (0 <= value && !author->margin[TOP]) {
					style->margin[TOP].margin =
							CSS_MARGIN_LENGTH;
					style->margin[TOP].value.length.value =
							value;
					style->margin[TOP].value.length.unit =
							CSS_UNIT_PX;
				}
				if (0 <= value && !author->margin[BOTTOM]) {
					style->margin[BOTTOM].margin =
							CSS_MARGIN_LENGTH;
					style->margin[BOTTOM].value.length.
							value = value;
					style->margin[BOTTOM].value.length.
							unit = CSS_UNIT_PX;
				}
			}
			xmlFree(s);
		}
	}

	/* Handle markup-originating alignment of block level elements.
	 * Adjust left and right margins. text-align property is handled in
	 * the default CSS file.
	 */
	if (markup_track->align != ALIGN_NONE &&
			(style->display == CSS_DISPLAY_BLOCK ||
			 style->display == CSS_DISPLAY_TABLE)) {
		if (!author->margin[LEFT]) {
			if (markup_track->align == ALIGN_LEFT) {
				/* left */
				style->margin[LEFT].margin = CSS_MARGIN_LENGTH;
				style->margin[LEFT].value.length.value = 0;
				style->margin[LEFT].value.length.unit =
								CSS_UNIT_PX;
			} else
				/* center or right */
				style->margin[LEFT].margin = CSS_MARGIN_AUTO;
		}

		if (!author->margin[RIGHT]) {
			if (markup_track->align == ALIGN_RIGHT) {
				/* right */
				style->margin[RIGHT].margin = CSS_MARGIN_LENGTH;
				style->margin[RIGHT].value.length.value= 0;
				style->margin[RIGHT].value.length.unit =
								CSS_UNIT_PX;
			} else
				/* left or center */
				style->margin[RIGHT].margin = CSS_MARGIN_AUTO;
		}
		if (author->margin[LEFT] || author->margin[RIGHT]) {
			/* author stylesheet sets a margin so stop markup
			 * alignment model propagation */
			markup_track->align = ALIGN_NONE;
		}
	}

	box_solve_display(style, !n->parent);

	/* Update markup_track with attributes which affect children of
	 * current box. */

	/* Handle html block level element alignment model.
	 * Note that only margins of block level children are considered,
	 * text-align for the current block can be handled in the default
	 * CSS file.
	 */
	if (strcmp(n->name, "center") == 0)
		markup_track->align = ALIGN_CENTER;
	else if (strcmp(n->name, "div") == 0 ||
			strcmp(n->name, "col") == 0 ||
			strcmp(n->name, "colgroup") == 0 ||
			strcmp(n->name, "tbody") == 0 ||
			strcmp(n->name, "td") == 0 ||
			strcmp(n->name, "tfoot") == 0 ||
			strcmp(n->name, "th") == 0 ||
			strcmp(n->name, "thead") == 0 ||
			strcmp(n->name, "tr") == 0) {

		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "align"))) {
			if (strcasecmp(s, "center") == 0)
				markup_track->align = ALIGN_CENTER;
			else if (strcasecmp(s, "right") == 0)
				markup_track->align = ALIGN_RIGHT;
			else if (strcasecmp(s, "left") == 0)
				markup_track->align = ALIGN_LEFT;
			xmlFree(s);
		}
	}
	/* Table cells without an align value have a default implied
	 * alignment */
	if (strcmp(n->name, "td") == 0) {
		if (!(s = (char *) xmlGetProp(n, (const xmlChar *) "align")))
			markup_track->align = ALIGN_LEFT;
		else
			xmlFree(s);
	}
	if (strcmp(n->name, "th") == 0) {
		if (!(s = (char *) xmlGetProp(n, (const xmlChar *) "align")))
			markup_track->align = ALIGN_CENTER;
		else
			xmlFree(s);
	}

	/* Some of TABLE's attributes apply to the table cells contained
	 * within the table. Those details are stored so they may be applied
	 * to the cells when we get to them. */
	if (strcmp((const char *) n->name, "table") == 0) {
		if ((s = (char *) xmlGetProp(n,
				(const xmlChar *) "cellpadding"))) {
			char *endp;
			long value = strtol(s, &endp, 10);
			/* precentage padding width not implemented */
			if (*endp == 0 && 0 <= value && value < 1000) {
				markup_track->padding_width = value;
				markup_track->cell_padding = true;
			}
			xmlFree(s);
		}
		if ((s = (char *) xmlGetProp(n,
				(const xmlChar *) "border"))) {
			markup_track->border_color = border_color;
			int border_width = atoi(s);
			/* percentage border width not implemented */
			if (!strrchr(s, '%') && 0 < border_width) {
				markup_track->cell_border = true;
			}
			xmlFree(s);
		}
	}

	return style;
}


/**
 * Calculate 'display' based on 'display', 'position', and 'float', as given
 * by CSS 2.1 9.7.
 *
 * \param  style  style to update
 * \param  root	  this is the root element
 */

void box_solve_display(struct css_style *style, bool root)
{
	if (style->display == CSS_DISPLAY_NONE)			/* 1. */
		return;
	else if (style->position == CSS_POSITION_ABSOLUTE ||
			style->position == CSS_POSITION_FIXED)	/* 2. */
		style->float_ = CSS_FLOAT_NONE;
	else if (style->float_ != CSS_FLOAT_NONE)		/* 3. */
		;
	else if (root)						/* 4. */
		;
	else							/* 5. */
		return;

	/* Special case for absolute positioning: make absolute inlines into
	 * inline block so that the boxes are constructed in an inline container
	 * as if they were not absolutely positioned. Layout expects and
	 * handles this. */
	if ((style->position == CSS_POSITION_ABSOLUTE ||
			style->position == CSS_POSITION_FIXED) &&
			(style->display == CSS_DISPLAY_INLINE ||
			 style->display == CSS_DISPLAY_INLINE_BLOCK ||
			 style->display == CSS_DISPLAY_INLINE_TABLE)) {
		style->display = CSS_DISPLAY_INLINE_BLOCK;
		return;
	}

	/* map specified value to computed value using table given in 9.7 */
	if (style->display == CSS_DISPLAY_INLINE_TABLE)
		style->display = CSS_DISPLAY_TABLE;
	else if (style->display == CSS_DISPLAY_LIST_ITEM ||
			style->display == CSS_DISPLAY_TABLE)
		; /* same as specified */
	else
		style->display = CSS_DISPLAY_BLOCK;
}


/**
 * Apply the CSS text-transform property to given text for its ASCII chars.
 *
 * \param  s	string to transform
 * \param  len  length of s
 * \param  tt	transform type
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


/**
 * \name  Special case element handlers
 *
 * These functions are called by box_construct_element() when an element is
 * being converted, according to the entries in element_table.
 *
 * The parameters are the xmlNode, the content for the document, and a partly
 * filled in box structure for the element.
 *
 * Return true on success, false on memory exhaustion. Set *convert_children
 * to false if children of this element in the XML tree should be skipped (for
 * example, if they have been processed in some special way already).
 *
 * Elements ordered as in the HTML 4.01 specification. Section numbers in
 * brackets [] refer to the spec.
 *
 * \{
 */

/**
 * Document body [7.5.1].
 */

bool box_body(BOX_SPECIAL_PARAMS)
{
	content->data.html.background_colour = box->style->background_color;
	return true;
}


/**
 * Forced line break [9.3.2].
 */

bool box_br(BOX_SPECIAL_PARAMS)
{
	box->type = BOX_BR;
	return true;
}

/**
 * Preformatted text [9.3.4].
 */

bool box_pre(BOX_SPECIAL_PARAMS)
{
	box->strip_leading_newline = 1;
	return true;
}

/**
 * Anchor [12.2].
 */

bool box_a(BOX_SPECIAL_PARAMS)
{
	bool ok;
	char *url;
	xmlChar *s;

	if ((s = xmlGetProp(n, (const xmlChar *) "href"))) {
		ok = box_extract_link((const char *) s,
				content->data.html.base_url, &url);
		xmlFree(s);
		if (!ok)
			return false;
		if (url) {
			box->href = talloc_strdup(content, url);
			free(url);
			if (!box->href)
				return false;
		}
	}

	/* name and id share the same namespace */
	if (!box_get_attribute(n, "name", content, &box->id))
		return false;

	/* target frame [16.3] */
	if ((s = xmlGetProp(n, (const xmlChar *) "target"))) {
		if (!strcasecmp(s, "_blank"))
			box->target = TARGET_BLANK;
		else if (!strcasecmp(s, "_top"))
			box->target = TARGET_TOP;
		else if (!strcasecmp(s, "_parent"))
			box->target = TARGET_PARENT;
		else if (!strcasecmp(s, "_self"))
			/* the default may have been overridden by a
			 * <base target=...>, so this is different to 0 */
			box->target = TARGET_SELF;
		else if (('a' <= s[0] && s[0] <= 'z') ||
				('A' <= s[0] && s[0] <= 'Z')) {  /* [6.16] */
			box->target = talloc_strdup(content, s);
			if (!box->target) {
				xmlFree(s);
				return false;
			}
		}
		xmlFree(s);
	}

	return true;
}


/**
 * Embedded image [13.2].
 */

bool box_image(BOX_SPECIAL_PARAMS)
{
	bool ok;
	char *s, *url;
	xmlChar *alt, *src;

	if (box->style && box->style->display == CSS_DISPLAY_NONE)
		return true;

	/* handle alt text */
	if ((alt = xmlGetProp(n, (const xmlChar *) "alt"))) {
		s = squash_whitespace(alt);
		xmlFree(alt);
		if (!s)
			return false;
		box->text = talloc_strdup(content, s);
		free(s);
		if (!box->text)
			return false;
		box->length = strlen(box->text);
	}

	/* imagemap associated with this image */
	if (!box_get_attribute(n, "usemap", content, &box->usemap))
		return false;
	if (box->usemap && box->usemap[0] == '#')
		box->usemap++;

	/* get image URL */
	if (!(src = xmlGetProp(n, (const xmlChar *) "src")))
		return true;
	if (!box_extract_link((char *) src, content->data.html.base_url, &url))
		return false;
	xmlFree(src);
	if (!url)
		return true;

	/* start fetch */
	ok = html_fetch_object(content, url, box, image_types,
			content->available_width, 1000, false);
	free(url);
	return ok;
}


/**
 * Generic embedded object [13.3].
 */

bool box_object(BOX_SPECIAL_PARAMS)
{
	struct object_params *params;
	struct object_param *param;
	xmlChar *codebase, *classid, *data;
	xmlNode *c;
	struct box *inline_container = 0;

	if (box->style && box->style->display == CSS_DISPLAY_NONE)
		return true;

	if (!box_get_attribute(n, "usemap", content, &box->usemap))
		return false;
	if (box->usemap && box->usemap[0] == '#')
		box->usemap++;

	params = talloc(content, struct object_params);
	if (!params)
		return false;
	params->data = 0;
	params->type = 0;
	params->codetype = 0;
	params->codebase = 0;
	params->classid = 0;
	params->params = 0;

	/* codebase, classid, and data are URLs
	 * (codebase is the base for the other two) */
	if ((codebase = xmlGetProp(n, (const xmlChar *) "codebase"))) {
		if (!box_extract_link((char *) codebase,
				content->data.html.base_url,
				&params->codebase))
			return false;
		xmlFree(codebase);
	}
	if (!params->codebase)
		params->codebase = content->data.html.base_url;

	if ((classid = xmlGetProp(n, (const xmlChar *) "classid"))) {
		if (!box_extract_link((char *) classid, params->codebase,
				&params->classid))
			return false;
		xmlFree(classid);
	}

	if ((data = xmlGetProp(n, (const xmlChar *) "data"))) {
		if (!box_extract_link((char *) data, params->codebase,
				&params->data))
			return false;
		xmlFree(data);
	}

	if (!params->classid && !params->data)
		/* nothing to embed; ignore */
		return true;

	/* Don't include ourself */
	if (params->classid &&
			strcmp(content->data.html.base_url,
				params->classid) == 0)
		return true;

	if (params->data &&
			strcmp(content->data.html.base_url,
				params->data) == 0)
		return true;

	/* codetype and type are MIME types */
	if (!box_get_attribute(n, "codetype", params, &params->codetype))
		return false;
	if (!box_get_attribute(n, "type", params, &params->type))
		return false;

	/* classid && !data => classid is used (consult codetype)
	 * (classid || !classid) && data => data is used (consult type)
	 * !classid && !data => invalid; ignored */

	if (params->classid && !params->data && params->codetype &&
			content_lookup(params->codetype) == CONTENT_OTHER)
		/* can't handle this MIME type */
		return true;

	if (params->data && params->type &&
			content_lookup(params->type) == CONTENT_OTHER)
		/* can't handle this MIME type */
		return true;

	/* add parameters to linked list */
	for (c = n->children; c; c = c->next) {
		if (c->type != XML_ELEMENT_NODE)
			continue;
		if (strcmp((const char *) c->name, "param") != 0)
			/* The first non-param child is the start of the alt
			 * html. Therefore, we should break out of this loop. */
			break;

		param = talloc(params, struct object_param);
		if (!param)
			return false;
		param->name = 0;
		param->value = 0;
		param->type = 0;
		param->valuetype = 0;
		param->next = 0;

		if (!box_get_attribute(c, "name", param, &param->name))
			return false;
		if (!box_get_attribute(c, "value", param, &param->value))
			return false;
		if (!box_get_attribute(c, "type", param, &param->type))
			return false;
		if (!box_get_attribute(c, "valuetype", param,
				&param->valuetype))
			return false;
		if (!param->valuetype) {
			param->valuetype = talloc_strdup(param, "data");
			if (!param->valuetype)
				return false;
		}

		param->next = params->params;
		params->params = param;
	}

	box->object_params = params;

	/* start fetch (MIME type is ok or not specified) */
	if (!html_fetch_object(content,
			params->data ? params->data : params->classid,
			box, 0, content->available_width, 1000, false))
		return false;

	/* convert children and place into fallback */
	for (c = n->children; c; c = c->next) {
		if (!convert_xml_to_box(c, content, box->style, box,
				&inline_container, 0, 0, 0, markup_track,
				author))
			return false;
	}
	box->fallback = box->children;
	box->children = box->last = 0;

	*convert_children = false;
	return true;
}


#if 0 /**
 * "Java applet" [13.4].
 *
 * \todo This needs reworking to be compliant to the spec
 * For now, we simply ignore all applet tags.
 */

struct box_result box_applet(xmlNode *n, struct box_status *status,
		struct css_style *style)
{
	struct box *box;
	struct object_params *po;
	struct object_param *pp = NULL;
	char *s;
	xmlNode *c;

	po = calloc(1, sizeof(struct object_params));
	if (!po)
		return (struct box_result) {0, false, true};

	box = box_create(style, status->href, 0, status->id,
			status->content->data.html.box_pool);
	if (!box) {
		free(po);
		return (struct box_result) {0, false, true};
	}

	/* archive */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "archive")) != NULL) {
		/** \todo tokenise this comma separated list */
		LOG(("archive '%s'", s));
		po->data = strdup(s);
		xmlFree(s);
		if (!po->data)
			goto no_memory;
	}
	/* code */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "code")) != NULL) {
		LOG(("applet '%s'", s));
		po->classid = strdup(s);
		xmlFree(s);
		if (!po->classid)
			goto no_memory;
	}

	/* object codebase */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "codebase")) != NULL) {
		po->codebase = strdup(s);
		LOG(("codebase: %s", s));
		xmlFree(s);
		if (!po->codebase)
			goto no_memory;
	}

	/* parameters
	 * parameter data is stored in a singly linked list.
	 * po->params points to the head of the list.
	 * new parameters are added to the head of the list.
	 */
	for (c = n->children; c != 0; c = c->next) {
		if (c->type != XML_ELEMENT_NODE)
			continue;

		if (strcmp((const char *) c->name, "param") == 0) {
			pp = calloc(1, sizeof(struct object_param));
			if (!pp)
				goto no_memory;

			if ((s = (char *) xmlGetProp(c,
					(const xmlChar *) "name")) != NULL) {
				pp->name = strdup(s);
				xmlFree(s);
				if (!pp->name)
					goto no_memory;
			}
			if ((s = (char *) xmlGetProp(c,
					(const xmlChar *) "value")) != NULL) {
				pp->value = strdup(s);
				xmlFree(s);
				if (!pp->value)
					goto no_memory;
			}
			if ((s = (char *) xmlGetProp(c,
					(const xmlChar *) "type")) != NULL) {
				pp->type = strdup(s);
				xmlFree(s);
				if (!pp->type)
					goto no_memory;
			}
			if ((s = (char *) xmlGetProp(c, (const xmlChar *)
							"valuetype")) != NULL) {
				pp->valuetype = strdup(s);
				xmlFree(s);
				if (!pp->valuetype)
					goto no_memory;
			} else {
				pp->valuetype = strdup("data");
				if (!pp->valuetype)
					goto no_memory;
			}

			pp->next = po->params;
			po->params = pp;
		} else {
				 /* The first non-param child is the start
				  * of the alt html. Therefore, we should
				  * break out of this loop.
				  */
				break;
		}
	}

	box->object_params = po;

	/* start fetch */
	if (plugin_decode(status->content, box))
		return (struct box_result) {box, false, false};

	return (struct box_result) {box, true, false};

no_memory:
	if (pp && pp != po->params) {
		/* ran out of memory creating parameter struct */
		free(pp->name);
		free(pp->value);
		free(pp->type);
		free(pp->valuetype);
		free(pp);
	}

	box_free_object_params(po);
	box_free_box(box);

	return (struct box_result) {0, false, true};
}
#endif


/**
 * Window subdivision [16.2.1].
 */

bool box_frameset(BOX_SPECIAL_PARAMS)
{
	bool ok;

	if (content->data.html.frameset) {
		LOG(("Error: multiple framesets in document."));
		/* Don't convert children */
		if (convert_children)
			*convert_children = false;
		/* And ignore this spurious frameset */
		box->style->display = CSS_DISPLAY_NONE;
		return true;
	}

	content->data.html.frameset = talloc_zero(content,
						struct content_html_frames);
	if (!content->data.html.frameset)
		return false;

	ok = box_create_frameset(content->data.html.frameset, n, content);
	if (ok)
		box->style->display = CSS_DISPLAY_NONE;

	if (convert_children)
		*convert_children = false;
	return ok;
}

bool box_create_frameset(struct content_html_frames *f, xmlNode *n,
		struct content *content) {
	unsigned int row, col, index, i;
	unsigned int rows = 1, cols = 1;
	char *s, *url;
	struct frame_dimension *row_height = 0, *col_width = 0;
	xmlNode *c;
	struct content_html_frames *frame;
	bool default_border = true;
	colour default_border_colour = 0x000000;

	/* parse rows and columns */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "rows"))) {
		row_height = box_parse_multi_lengths(s, &rows);
		xmlFree(s);
		if (!row_height)
			return false;
	} else {
		row_height = calloc(1, sizeof(struct frame_dimension));
		if (!row_height)
			return false;
		row_height->value = 100;
		row_height->unit = FRAME_DIMENSION_PERCENT;
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "cols"))) {
		col_width = box_parse_multi_lengths(s, &cols);
		xmlFree(s);
		if (!col_width)
			return false;
	} else {
		col_width = calloc(1, sizeof(struct frame_dimension));
		if (!col_width)
			return false;
		col_width->value = 100;
		col_width->unit = FRAME_DIMENSION_PERCENT;
	}

	/* common extension: border="0|1" to control all children */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "border"))) {
		if ((s[0] == '0') && (s[1] == '\0'))
			default_border = false;
		xmlFree(s);
	}
	/* common extension: frameborder="yes|no" to control all children */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "frameborder"))) {
	  	if (!strcasecmp(s, "no"))
	  		default_border = false;
		xmlFree(s);
	}
	/* common extension: bordercolor="#RRGGBB|<named colour>" to control
	 *all children */
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "bordercolor"))) {
	  	parse_inline_colour(s, &default_border_colour);
		xmlFree(s);
	}

	/* update frameset and create default children */
	f->cols = cols;
	f->rows = rows;
	f->scrolling = SCROLLING_NO;
	f->children = talloc_array(content, struct content_html_frames,
								(rows * cols));
	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			index = (row * cols) + col;
			frame = &f->children[index];
			frame->cols = 0;
			frame->rows = 0;
			frame->width = col_width[col];
			frame->height = row_height[row];
			frame->margin_width = 0;
			frame->margin_height = 0;
			frame->name = NULL;
			frame->url = NULL;
			frame->no_resize = false;
			frame->scrolling = SCROLLING_AUTO;
			frame->border = default_border;
			frame->border_colour = default_border_colour;
			frame->children = NULL;
		}
	}
	free(col_width);
	free(row_height);

	/* create the frameset windows */
	c = n->children;
	for (row = 0; c && row < rows; row++) {
		for (col = 0; c && col < cols; col++) {
			while (c && !(c->type == XML_ELEMENT_NODE && (
				strcmp((const char *) c->name, "frame") == 0 ||
				strcmp((const char *) c->name, "frameset") == 0
					)))
				c = c->next;
			if (!c)
				break;

			/* get current frame */
			index = (row * cols) + col;
			frame = &f->children[index];

			/* nest framesets */
			if (strcmp((const char *) c->name, "frameset") == 0) {
				frame->border = 0;
				if (!box_create_frameset(frame, c, content))
					return false;
				c = c->next;
				continue;
			}

			/* get frame URL (not required) */
			url = NULL;
			if ((s = (char *) xmlGetProp(c,
					(const xmlChar *) "src"))) {
				box_extract_link(s, content->data.html.base_url,
									&url);
				xmlFree(s);
			}

			/* copy url */
			if (url) {
			  	/* no self-references */
			  	if (strcmp(content->data.html.base_url, url))
					frame->url = talloc_strdup(content,
									url);
				free(url);
				url = NULL;
			}

			/* fill in specified values */
			if ((s = (char *) xmlGetProp(c,
					(const xmlChar *) "name"))) {
				frame->name = talloc_strdup(content, s);
				xmlFree(s);
			}
			frame->no_resize = xmlHasProp(c,
					(const xmlChar *) "noresize");
			if ((s = (char *) xmlGetProp(c,
					(const xmlChar *) "frameborder"))) {
				i = atoi(s);
				frame->border = (i != 0);
				xmlFree(s);
			}
			if ((s = (char *) xmlGetProp(c,
					(const xmlChar *) "scrolling"))) {
				if (!strcasecmp(s, "yes"))
					frame->scrolling = SCROLLING_YES;
				else if (!strcasecmp(s, "no"))
					frame->scrolling = SCROLLING_NO;
				xmlFree(s);
			}
			if ((s = (char *) xmlGetProp(c,
					(const xmlChar *) "marginwidth"))) {
				frame->margin_width = atoi(s);
				xmlFree(s);
			}
			if ((s = (char *) xmlGetProp(c,
					(const xmlChar *) "marginheight"))) {
				frame->margin_height = atoi(s);
				xmlFree(s);
			}
			if ((s = (char *) xmlGetProp(c, (const xmlChar *)
							"bordercolor"))) {
				parse_inline_colour(s, &frame->border_colour);
				xmlFree(s);
			}

			/* advance */
			c = c->next;
		}
	}

	return true;
}


/**
 * Inline subwindow [16.5].
 */

bool box_iframe(BOX_SPECIAL_PARAMS)
{
	char *url, *s;
	struct content_html_iframe *iframe;
	int i;

	/* get frame URL */
	if (!(s = (char *) xmlGetProp(n,
			(const xmlChar *) "src")))
		return true;
	if (!box_extract_link(s, content->data.html.base_url, &url)) {
		xmlFree(s);
		return false;
	}
	xmlFree(s);
	if (!url)
		return true;

	/* don't include ourself */
	if (strcmp(content->data.html.base_url, url) == 0) {
		free(url);
		return true;
	}

	/* create a new iframe */
	iframe = talloc(content, struct content_html_iframe);
	if (!iframe) {
		free(url);
		return false;
	}
	iframe->box = box;
	iframe->margin_width = 0;
	iframe->margin_height = 0;
	iframe->name = NULL;
	iframe->url = talloc_strdup(content, url);
	iframe->scrolling = SCROLLING_AUTO;
	iframe->border = true;
	iframe->next = content->data.html.iframe;
	content->data.html.iframe = iframe;

	/* fill in specified values */
	if ((s = (char *) xmlGetProp(n,
			(const xmlChar *) "name"))) {
		iframe->name = talloc_strdup(content, s);
		xmlFree(s);
	}
	if ((s = (char *) xmlGetProp(n,
			(const xmlChar *) "frameborder"))) {
		i = atoi(s);
		iframe->border = (i != 0);
		xmlFree(s);
	}
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "bordercolor"))) {
	  	parse_inline_colour(s, &iframe->border_colour);
		xmlFree(s);
	}
	if ((s = (char *) xmlGetProp(n,
			(const xmlChar *) "scrolling"))) {
		if (!strcasecmp(s, "yes"))
			iframe->scrolling = SCROLLING_YES;
		else if (!strcasecmp(s, "no"))
			iframe->scrolling = SCROLLING_NO;
		xmlFree(s);
	}
	if ((s = (char *) xmlGetProp(n,
			(const xmlChar *) "marginwidth"))) {
		iframe->margin_width = atoi(s);
		xmlFree(s);
	}
	if ((s = (char *) xmlGetProp(n,
			(const xmlChar *) "marginheight"))) {
		iframe->margin_height = atoi(s);
		xmlFree(s);
	}

	/* release temporary memory */
	free(url);

	/* box */
	box->type = BOX_INLINE_BLOCK;
	assert(box->style);

	if (convert_children)
		*convert_children = false;
	return true;
}


/**
 * Interactive form [17.3].
 */

bool box_form(BOX_SPECIAL_PARAMS)
{
	char *xmlaction, *action, *faction, *method, *enctype, *charset, *target;
	form_method fmethod;
	struct form *form;
	url_func_result result;

	if (!(xmlaction = (char *)
			xmlGetProp(n, (const xmlChar *) "action"))) {
		/* the action attribute is required, but many forms fail to
		 * specify it. In the case where it is _not_ specified,
		 * follow other browsers and make the form action the
		 * URI of the page the form is contained in. */
		action = strdup("");
	} else {
		action = strdup(xmlaction);
		xmlFree(xmlaction);
	}

	if (!action)
		return false;

	result = url_join(action, content->data.html.base_url, &faction);
	if (result != URL_FUNC_OK) {
		free(action);
		return false;
	}

	/* No longer needed */
	free(action);

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

	/* acceptable encoding(s) for form data */
	charset = (char *) xmlGetProp(n, (const xmlChar *) "accept-charset");

	/* target for form data */
	target = (char *) xmlGetProp(n, (const xmlChar *) "target");

	form = form_new(faction, target, fmethod, charset,
				content->data.html.encoding);
	if (!form) {
		free(faction);
		xmlFree(target);
		xmlFree(charset);
		return false;
	}
	form->prev = content->data.html.forms;
	content->data.html.forms = form;

	return true;
}


/**
 * Form control [17.4].
 */

bool box_input(BOX_SPECIAL_PARAMS)
{
	struct form_control *gadget = NULL;
	char *s, *type, *url;
	url_func_result res;

	type = (char *) xmlGetProp(n, (const xmlChar *) "type");

	if (type && strcasecmp(type, "password") == 0) {
		if (!box_input_text(n, content, box, 0, markup_track, author,
									 true))
			goto no_memory;
		gadget = box->gadget;
		gadget->box = box;

	} else if (type && strcasecmp(type, "file") == 0) {
		box->type = BOX_INLINE_BLOCK;
		box->gadget = gadget = form_new_control(GADGET_FILE);
		if (!gadget)
			goto no_memory;
		gadget->box = box;

	} else if (type && strcasecmp(type, "hidden") == 0) {
		/* no box for hidden inputs */
		box->style->display = CSS_DISPLAY_NONE;

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
		box->gadget = gadget = form_new_control(type[0] == 'c' ||
				type[0] == 'C' ? GADGET_CHECKBOX :
				GADGET_RADIO);
		if (!gadget)
			goto no_memory;
		gadget->box = box;

		gadget->selected = xmlHasProp(n, (const xmlChar *) "checked");

		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value"))) {
			gadget->value = strdup(s);
			xmlFree(s);
			if (!gadget->value)
				goto no_memory;
			gadget->length = strlen(gadget->value);
		}

	} else if (type && (strcasecmp(type, "submit") == 0 ||
			strcasecmp(type, "reset") == 0)) {
		struct box *inline_container, *inline_box;
		if (!box_button(n, content, box, 0, markup_track, author))
			goto no_memory;
		inline_container = box_create(0, 0, 0, 0, 0, content);
		if (!inline_container)
			goto no_memory;
		inline_container->type = BOX_INLINE_CONTAINER;
		inline_box = box_create(box->style, 0, 0, box->title, 0,
				content);
		if (!inline_box)
			goto no_memory;
		inline_box->type = BOX_TEXT;
		if (box->gadget->value != NULL)
			inline_box->text = talloc_strdup(content,
					box->gadget->value);
		else if (box->gadget->type == GADGET_SUBMIT)
			inline_box->text = talloc_strdup(content,
					messages_get("Form_Submit"));
		else
			inline_box->text = talloc_strdup(content,
					messages_get("Form_Reset"));
		if (!inline_box->text)
			goto no_memory;
		inline_box->length = strlen(inline_box->text);
		box_add_child(inline_container, inline_box);
		box_add_child(box, inline_container);

	} else if (type && strcasecmp(type, "button") == 0) {
		struct box *inline_container, *inline_box;
		if (!box_button(n, content, box, 0, markup_track, author))
			goto no_memory;
		inline_container = box_create(0, 0, 0, 0, 0, content);
		if (!inline_container)
			goto no_memory;
		inline_container->type = BOX_INLINE_CONTAINER;
		inline_box = box_create(box->style, 0, 0, box->title, 0,
				content);
		if (!inline_box)
			goto no_memory;
		inline_box->type = BOX_TEXT;
		if ((s = (char *) xmlGetProp(n, (const xmlChar *) "value")))
			inline_box->text = talloc_strdup(content, s);
		else
			inline_box->text = talloc_strdup(content, "Button");
		if (!inline_box->text)
			goto no_memory;
		inline_box->length = strlen(inline_box->text);
		box_add_child(inline_container, inline_box);
		box_add_child(box, inline_container);

	} else if (type && strcasecmp(type, "image") == 0) {
		box->gadget = gadget = form_new_control(GADGET_IMAGE);
		if (!gadget)
			goto no_memory;
		gadget->box = box;
		gadget->type = GADGET_IMAGE;

		if (box->style && box->style->display != CSS_DISPLAY_NONE) {
			if ((s = (char *) xmlGetProp(n,
					(const xmlChar*) "src"))) {
				res = url_join(s,
					content->data.html.base_url, &url);
				xmlFree(s);
				/* if url is equivalent to the parent's url,
				 * we've got infinite inclusion. stop it here
				 * also bail if url_join failed.
				 */
				if (res == URL_FUNC_OK &&
						strcasecmp(url,
						content->data.
						html.base_url) != 0) {
					if (!html_fetch_object(content, url,
							box, image_types,
							content->
							available_width,
							1000, false)) {
						free(url);
						goto no_memory;
					}
				}
				free(url);
			}
		}

	} else {
		/* the default type is "text" */
		if (!box_input_text(n, content, box, 0, markup_track, author,
									false))
			goto no_memory;
		gadget = box->gadget;
		gadget->box = box;
	}

	if (type)
		xmlFree(type);

	if (gadget) {
		if (content->data.html.forms)
			form_add_control(content->data.html.forms, gadget);
		s = (char *) xmlGetProp(n, (const xmlChar *) "name");
		if (s) {
			gadget->name = strdup(s);
			xmlFree(s);
			if (!gadget->name)
				goto no_memory;
		}
	}

	*convert_children = false;
	return true;

no_memory:
	if (type)
		xmlFree(type);
	if (gadget)
		form_free_control(gadget);

	return false;
}


/**
 * Helper function for box_input().
 */

bool box_input_text(BOX_SPECIAL_PARAMS, bool password)
{
	char *s;
	struct box *inline_container, *inline_box;

	box->type = BOX_INLINE_BLOCK;
	box->gadget = form_new_control((password) ? GADGET_PASSWORD :
			GADGET_TEXTBOX);
	if (!box->gadget)
		return 0;
	box->gadget->box = box;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "maxlength"))) {
		box->gadget->maxlength = atoi(s);
		xmlFree(s);
	}

	s = (char *) xmlGetProp(n, (const xmlChar *) "value");
	box->gadget->value = strdup((s != NULL) ? s : "");
	box->gadget->initial_value = strdup((box->gadget->value != NULL) ?
			box->gadget->value : "");
	if (s)
		xmlFree(s);
	if (box->gadget->value == NULL || box->gadget->initial_value == NULL) {
		box_free(box);
		return NULL;
	}
	box->gadget->length = strlen(box->gadget->value);

	inline_container = box_create(0, 0, 0, 0, 0, content);
	if (!inline_container)
		return 0;
	inline_container->type = BOX_INLINE_CONTAINER;
	inline_box = box_create(box->style, 0, 0, box->title, 0, content);
	if (!inline_box)
		return 0;
	inline_box->type = BOX_TEXT;
	if (password) {
		inline_box->length = strlen(box->gadget->value);
		inline_box->text = talloc_array(content, char,
				inline_box->length + 1);
		if (!inline_box->text)
			return 0;
		memset(inline_box->text, '*', inline_box->length);
		inline_box->text[inline_box->length] = '\0';
	} else {
		/* replace spaces/TABs with hard spaces to prevent line
		 * wrapping */
		char *text = cnv_space2nbsp(box->gadget->value);
		if (!text)
			return 0;
		inline_box->text = talloc_strdup(content, text);
		free(text);
		if (!inline_box->text)
			return 0;
		inline_box->length = strlen(inline_box->text);
	}
	box_add_child(inline_container, inline_box);
	box_add_child(box, inline_container);

	return box;
}


/**
 * Push button [17.5].
 */

bool box_button(BOX_SPECIAL_PARAMS)
{
	xmlChar *s;
	char *type = (char *) xmlGetProp(n, (const xmlChar *) "type");

	box->type = BOX_INLINE_BLOCK;

	if (!type || strcasecmp(type, "submit") == 0) {
		box->gadget = form_new_control(GADGET_SUBMIT);
	} else if (strcasecmp(type, "reset") == 0) {
		box->gadget = form_new_control(GADGET_RESET);
	} else {
		/* type="button" or unknown: just render the contents */
		xmlFree(type);
		return true;
	}

	if (type)
		xmlFree(type);

	if (!box->gadget)
		return false;

	if (content->data.html.forms)
		form_add_control(content->data.html.forms, box->gadget);
	box->gadget->box = box;
	if ((s = xmlGetProp(n, (const xmlChar *) "name")) != NULL) {
		box->gadget->name = strdup((char *) s);
		xmlFree(s);
		if (!box->gadget->name)
			return false;
	}
	if ((s = xmlGetProp(n, (const xmlChar *) "value")) != NULL) {
		box->gadget->value = strdup((char *) s);
		xmlFree(s);
		if (!box->gadget->value)
			return false;
	}

	return true;
}


/**
 * Option selector [17.6].
 */

bool box_select(BOX_SPECIAL_PARAMS)
{
	struct box *inline_container;
	struct box *inline_box;
	struct form_control *gadget;
	char* s;
	xmlNode *c, *c2;

	gadget = form_new_control(GADGET_SELECT);
	if (!gadget)
		return false;

	gadget->data.select.multiple = false;
	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "multiple"))) {
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
		return true;
	}

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name"))) {
		gadget->name = strdup(s);
		xmlFree(s);
		if (!gadget->name)
			goto no_memory;
	}

	box->type = BOX_INLINE_BLOCK;
	box->gadget = gadget;
	gadget->box = box;

	inline_container = box_create(0, 0, 0, 0, 0, content);
	if (!inline_container)
		goto no_memory;
	inline_container->type = BOX_INLINE_CONTAINER;
	inline_box = box_create(box->style, 0, 0, box->title, 0, content);
	if (!inline_box)
		goto no_memory;
	inline_box->type = BOX_TEXT;
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
		inline_box->text = talloc_strdup(content,
				messages_get("Form_None"));
	else if (gadget->data.select.num_selected == 1)
		inline_box->text = talloc_strdup(content,
				gadget->data.select.current->text);
	else
		inline_box->text = talloc_strdup(content,
				messages_get("Form_Many"));
	if (!inline_box->text)
		goto no_memory;

	inline_box->length = strlen(inline_box->text);

	if (content->data.html.forms)
		form_add_control(content->data.html.forms, box->gadget);

	*convert_children = false;
	return true;

no_memory:
	form_free_control(gadget);
	return false;
}


/**
 * Add an option to a form select control (helper function for box_select()).
 *
 * \param  control  select containing the option
 * \param  n	    xml element node for <option>
 * \return  true on success, false on memory exhaustion
 */

bool box_select_add_option(struct form_control *control, xmlNode *n)
{
	char *value = 0;
	char *text = 0;
	char *text_nowrap = 0;
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

	/* replace spaces/TABs with hard spaces to prevent line wrapping */
	text_nowrap = cnv_space2nbsp(text);
	if (!text_nowrap)
		goto no_memory;

	if (!form_add_option(control, value, text_nowrap, selected))
		goto no_memory;

	free(text);

	return true;

no_memory:
	free(value);
	free(text);
	free(text_nowrap);
	return false;
}


/**
 * Multi-line text field [17.7].
 */

bool box_textarea(BOX_SPECIAL_PARAMS)
{
	/* A textarea is an INLINE_BLOCK containing a single INLINE_CONTAINER,
	 * which contains the text as runs of TEXT separated by BR. There is
	 * at least one TEXT. The first and last boxes are TEXT.
	 * Consecutive BR may not be present. These constraints are satisfied
	 * by using a 0-length TEXT for blank lines. */

	xmlChar *current, *string;
	xmlNode *n2;
	xmlBufferPtr buf;
	xmlParserCtxtPtr ctxt;
	struct box *inline_container, *inline_box, *br_box;
	char *s;
	size_t len;

	box->type = BOX_INLINE_BLOCK;
	box->gadget = form_new_control(GADGET_TEXTAREA);
	if (!box->gadget)
		return false;
	box->gadget->box = box;

	if ((s = (char *) xmlGetProp(n, (const xmlChar *) "name"))) {
		box->gadget->name = strdup(s);
		xmlFree(s);
		if (!box->gadget->name)
			return false;
	}

	inline_container = box_create(0, 0, 0, box->title, 0, content);
	if (!inline_container)
		return false;
	inline_container->type = BOX_INLINE_CONTAINER;
	box_add_child(box, inline_container);

	n2 = n->children;
	buf = xmlBufferCreate();
	while(n2) {
		int ret = xmlNodeDump(buf, n2->doc, n2, 0, 0);
		if (ret == -1) {
			xmlBufferFree(buf);
			return false;
		}
		n2 = n2->next;
	}

	ctxt = xmlCreateDocParserCtxt(buf->content);
	string = current = NULL;
	if (ctxt) {
		string = current = xmlStringDecodeEntities(ctxt,
				buf->content,
				XML_SUBSTITUTE_REF,
				0, 0, 0);
		xmlFreeParserCtxt(ctxt);
	}

	if (!string) {
		/* If we get here, either the parser context failed to be
		 * created or we were unable to decode the entities in the
		 * buffer. Therefore, try to create a blank string in order
		 * to recover. */
		string = current = xmlStrdup((const xmlChar *) "");
		if (!string) {
			xmlBufferFree(buf);
			return false;
		}
	}

	while (1) {
		/* BOX_TEXT */
		len = strcspn(current, "\r\n");
		s = talloc_strndup(content, current, len);
		if (!s) {
			xmlFree(string);
			xmlBufferFree(buf);
			return false;
		}

		inline_box = box_create(box->style, 0, 0, box->title, 0,
				content);
		if (!inline_box) {
			xmlFree(string);
			xmlBufferFree(buf);
			return false;
		}
		inline_box->type = BOX_TEXT;
		inline_box->text = s;
		inline_box->length = len;
		box_add_child(inline_container, inline_box);

		current += len;
		if (current[0] == 0)
			/* finished */
			break;

		/* BOX_BR */
		br_box = box_create(box->style, 0, 0, box->title, 0, content);
		if (!br_box) {
			xmlFree(string);
			xmlBufferFree(buf);
			return false;
		}
		br_box->type = BOX_BR;
		box_add_child(inline_container, br_box);

		if (current[0] == '\r' && current[1] == '\n')
			current += 2;
		else
			current++;
	}

	xmlFree(string);
	xmlBufferFree(buf);

	if (content->data.html.forms)
		form_add_control(content->data.html.forms, box->gadget);

	*convert_children = false;
	return true;
}


/**
 * Embedded object (not in any HTML specification:
 * see http://wp.netscape.com/assist/net_sites/new_html3_prop.html )
 */

bool box_embed(BOX_SPECIAL_PARAMS)
{
	struct object_params *params;
	struct object_param *param;
	xmlChar *src;
	xmlAttr *a;

	if (box->style && box->style->display == CSS_DISPLAY_NONE)
		return true;

	params = talloc(content, struct object_params);
	if (!params)
		return false;
	params->data = 0;
	params->type = 0;
	params->codetype = 0;
	params->codebase = 0;
	params->classid = 0;
	params->params = 0;

	/* src is a URL */
	if (!(src = xmlGetProp(n, (const xmlChar *) "src")))
		return true;
	if (!box_extract_link((char *) src, content->data.html.base_url,
			&params->data))
		return false;
	xmlFree(src);
	if (!params->data)
		return true;

	/* Don't include ourself */
	if (strcmp(content->data.html.base_url, params->data) == 0)
		return true;

	/* add attributes as parameters to linked list */
	for (a = n->properties; a; a = a->next) {
		if (strcasecmp((const char *) a->name, "src") == 0)
			continue;
		if (!a->children || !a->children->content)
			continue;

		param = talloc(content, struct object_param);
		if (!param)
			return false;
		param->name = talloc_strdup(content, (const char *) a->name);
		param->value = talloc_strdup(content,
				(char *) a->children->content);
		param->type = 0;
		param->valuetype = talloc_strdup(content, "data");
		param->next = 0;

		if (!param->name || !param->value || !param->valuetype)
			return false;

		param->next = params->params;
		params->params = param;
	}

	box->object_params = params;

	/* start fetch */
	return html_fetch_object(content, params->data, box, 0,
			content->available_width, 1000, false);
}

/**
 * \}
 */


/**
 * Get the value of an XML element's attribute.
 *
 * \param  n	      xmlNode, of type XML_ELEMENT_NODE
 * \param  attribute  name of attribute
 * \param  context    talloc context for result buffer
 * \param  value      updated to value, if the attribute is present
 * \return  true on success, false if attribute present but memory exhausted
 *
 * Note that returning true does not imply that the attribute was found. If the
 * attribute was not found, *value will be unchanged.
 */

bool box_get_attribute(xmlNode *n, const char *attribute,
		void *context, char **value)
{
	xmlChar *s = xmlGetProp(n, (const xmlChar *) attribute);
	if (!s)
		return true;
	*value = talloc_strdup(context, s);
	xmlFree(s);
	if (!*value)
		return false;
	return true;
}


/**
 * Extract a URL from a relative link, handling junk like whitespace and
 * attempting to read a real URL from "javascript:" links.
 *
 * \param  rel	   relative URL taken from page
 * \param  base	   base for relative URLs
 * \param  result  updated to target URL on heap, unchanged if extract failed
 * \return  true on success, false on memory exhaustion
 */

bool box_extract_link(const char *rel, const char *base, char **result)
{
	char *s, *s1, *apos0 = 0, *apos1 = 0, *quot0 = 0, *quot1 = 0;
	unsigned int i, j, end;
	url_func_result res;

	s1 = s = malloc(3 * strlen(rel) + 1);
	if (!s)
		return false;

	/* copy to s, removing white space and control characters */
	for (i = 0; rel[i] && isspace(rel[i]); i++)
		;
	for (end = strlen(rel); end != i && isspace(rel[end - 1]); end--)
		;
	for (j = 0; i != end; i++) {
		if ((unsigned char) rel[i] < 0x20) {
			; /* skip control characters */
		} else if (rel[i] == ' ') {
			s[j++] = '%';
			s[j++] = '2';
			s[j++] = '0';
		} else {
			s[j++] = rel[i];
		}
	}
	s[j] = 0;

	/* extract first quoted string out of "javascript:" link */
	if (strncmp(s, "javascript:", 11) == 0) {
		apos0 = strchr(s, '\'');
		if (apos0)
			apos1 = strchr(apos0 + 1, '\'');
		quot0 = strchr(s, '"');
		if (quot0)
			quot1 = strchr(quot0 + 1, '"');
		if (apos0 && apos1 && (!quot0 || !quot1 || apos0 < quot0)) {
			*apos1 = 0;
			s1 = apos0 + 1;
		} else if (quot0 && quot1) {
			*quot1 = 0;
			s1 = quot0 + 1;
		}
	}

	/* construct absolute URL */
	res = url_join(s1, base, result);
	free(s);
	if (res == URL_FUNC_NOMEM)
		return false;
	else if (res == URL_FUNC_FAILED)
		return true;

	return true;
}


/**
 * Parse a multi-length-list, as defined by HTML 4.01.
 *
 * \param  s	    string to parse
 * \param  count    updated to number of entries
 * \return  array of struct box_multi_length, or 0 on memory exhaustion
 */

struct frame_dimension *box_parse_multi_lengths(const char *s,
		unsigned int *count)
{
	char *end;
	unsigned int i, n;
	struct frame_dimension *length;

	for (i = 0, n = 1; s[i]; i++)
		if (s[i] == ',')
			n++;

	length = calloc(n, sizeof(struct frame_dimension));
	if (!length)
		return NULL;

	for (i = 0; i != n; i++) {
		while (isspace(*s))
			s++;
		length[i].value = strtof(s, &end);
		if (length[i].value <= 0)
			length[i].value = 1;
		s = end;
		switch (*s) {
			case '%':
				length[i].unit = FRAME_DIMENSION_PERCENT;
				break;
			case '*':
				length[i].unit = FRAME_DIMENSION_RELATIVE;
				break;
			default:
				length[i].unit = FRAME_DIMENSION_PIXELS;
				break;
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
 * Parse an inline colour string
 */
static void parse_inline_colour(char *s, colour *variable) {
	colour new_colour = CSS_COLOR_NONE;
	if (s[0] == '#') {
	  	if (strlen(s) == 7)
	  		new_colour = hex_colour(s + 1, 6);
	} else {
	  	new_colour = named_colour(s);
	}
	if (new_colour != CSS_COLOR_NONE)
		 *variable = new_colour;
}
