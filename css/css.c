/**
 * $Id: css.c,v 1.4 2003/04/05 21:38:06 bursa Exp $
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define CSS_INTERNALS
#define NDEBUG
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/css/parser.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

/**
 * internal structures
 */

struct decl {
	unsigned long score;
	struct rule * rule;
};

void css_dump_style(const struct css_style * const style);


	
const struct css_style css_base_style = {
	0xffffff,
	CSS_CLEAR_NONE,
	0x000000,
	CSS_DISPLAY_BLOCK,
	CSS_FLOAT_NONE,
	{ CSS_FONT_SIZE_LENGTH, { { 12, CSS_UNIT_PT } } },
	CSS_FONT_WEIGHT_NORMAL,
	CSS_FONT_STYLE_NORMAL,
	{ CSS_HEIGHT_AUTO, { 1, CSS_UNIT_EM } },
	{ CSS_LINE_HEIGHT_ABSOLUTE, { 1.2 } },
	CSS_TEXT_ALIGN_LEFT,
	{ CSS_WIDTH_AUTO, { { 1, CSS_UNIT_EM } } }
};

const struct css_style css_empty_style = {
	CSS_COLOR_INHERIT,
	CSS_CLEAR_INHERIT,
	CSS_COLOR_INHERIT,
	CSS_DISPLAY_INHERIT,
	CSS_FLOAT_INHERIT,
	{ CSS_FONT_SIZE_INHERIT, { { 1, CSS_UNIT_EM } } },
	CSS_FONT_WEIGHT_INHERIT,
	CSS_FONT_STYLE_INHERIT,
	{ CSS_HEIGHT_AUTO, { 1, CSS_UNIT_EM } },
	{ CSS_LINE_HEIGHT_INHERIT, { 1.2 } },
	CSS_TEXT_ALIGN_INHERIT,
	{ CSS_WIDTH_INHERIT, { { 1, CSS_UNIT_EM } } }
};

const struct css_style css_blank_style = {
	TRANSPARENT,
	CSS_CLEAR_NONE,
	CSS_COLOR_INHERIT,
	CSS_DISPLAY_INLINE,
	CSS_FLOAT_NONE,
	{ CSS_FONT_SIZE_INHERIT, { { 1, CSS_UNIT_EM } } },
	CSS_FONT_WEIGHT_INHERIT,
	CSS_FONT_STYLE_INHERIT,
	{ CSS_HEIGHT_AUTO, { 1, CSS_UNIT_EM } },
	{ CSS_LINE_HEIGHT_INHERIT, { 1.2 } },
	CSS_TEXT_ALIGN_INHERIT,
	{ CSS_WIDTH_AUTO, { { 1, CSS_UNIT_EM } } }
};



void css_create(struct content *c)
{
	unsigned int i;
	LOG(("content %p", c));
	c->data.css = xcalloc(1, sizeof(*c->data.css));
	css_lex_init(&c->data.css->lexer);
	c->data.css->parser = css_parser_Alloc(malloc);
	for (i = 0; i != HASH_SIZE; i++)
		c->data.css->rule[i] = 0;
}


void css_process_data(struct content *c, char *data, unsigned long size)
{
	int token;
	YY_BUFFER_STATE buffer;
	struct parse_params param = {0, c->data.css, 0};

	LOG(("content %p, size %lu", c, size));

	buffer = css__scan_bytes(data, size, c->data.css->lexer);
	while ((token = css_lex(c->data.css->lexer))) {
		css_parser_(c->data.css->parser, token,
				strdup(css_get_text(c->data.css->lexer)),
				&param);
	}
	css__delete_buffer(buffer, c->data.css->lexer);
}


int css_convert(struct content *c, unsigned int width, unsigned int height)
{
	struct parse_params param = {0, c->data.css, 0};

	LOG(("content %p", c));

	css_parser_(c->data.css->parser, 0, 0, &param);

	css_parser_Free(c->data.css->parser, free);
	css_lex_destroy(c->data.css->lexer);

	return 0;
}


void css_revive(struct content *c, unsigned int width, unsigned int height)
{
}


void css_reformat(struct content *c, unsigned int width, unsigned int height)
{
}


void css_destroy(struct content *c)
{
	xfree(c->data.css);
}


/**
 * parser support functions
 */

struct node * css_new_node(node_type type, char *data,
		struct node *left, struct node *right)
{
	struct node *node = xcalloc(1, sizeof(*node));
	node->type = type;
	node->data = data;
	node->left = left;
	node->right = right;
	node->next = 0;
	node->comb = COMB_NONE;
	node->style = 0;
	return node;
}

void css_free_node(struct node *node)
{
	if (node == 0)
		return;
	if (node->left != 0)
		css_free_node(node->left);
	if (node->right != 0)
		css_free_node(node->right);
	if (node->next != 0)
		css_free_node(node->next);
	if (node->data != 0)
		free(node->data);
	free(node);
}



void css_get_style(struct css_stylesheet * stylesheet, struct css_selector * selector,
		unsigned int selectors, struct css_style * style)
{
	struct node *r, *n, *m;
	unsigned int hash, i, done_empty = 0;

	LOG(("stylesheet %p, selectors %u", stylesheet, selectors));

	hash = css_hash(selector[selectors - 1].element);
	for (r = stylesheet->rule[hash]; ; r = r->next) {
		if (r == 0 && !done_empty) {
			r = stylesheet->rule[0];
			done_empty = 1;
		}
		if (r == 0)
			return;
		i = selectors - 1;
		n = r;
		/* compare element */
		if (n->data != 0)
			if (strcasecmp(selector[i].element, n->data) != 0)
				goto not_matched;
		LOG(("top element '%s' matched", selector[i].element));
		while (1) {
			/* class and id */
			for (m = n->left; m != 0; m = m->next) {
				if (m->type == NODE_ID) {
					/* TODO: check if case sensitive */
					if (strcmp(selector[i].id, m->data) != 0)
						goto not_matched;
				} else if (m->type == NODE_CLASS) {
					/* TODO: check if case sensitive */
					LOG(("comparing class '%s' against '%s'", selector[i].class, m->data));
					if (strcmp(selector[i].class, m->data) != 0)
						goto not_matched;
				} else {
					goto not_matched;
				}
			}
			LOG(("class and id matched"));
			/* ancestors etc. */
			if (n->comb == COMB_NONE)
				goto matched; /* match successful */
			else if (n->comb == COMB_ANCESTOR) {
				/* search for ancestor */
				assert(n->right != 0);
				n = n->right;
				if (n->data == 0)
					goto not_matched;  /* TODO: handle this case */
				LOG(("searching for ancestor '%s'", n->data));
				while (i != 0 && strcasecmp(selector[i - 1].element, n->data) != 0)
					i--;
				if (i == 0)
					goto not_matched;
				i--;
				LOG(("found"));
			} else {
				/* TODO: COMB_PRECEDED, COMB_PARENT */
				goto not_matched;
			}
		}

matched:
		/* TODO: sort by specificity */
		LOG(("matched rule %p", r));
		css_merge(style, r->style);

not_matched:

	}
}


void css_parse_property_list(struct css_style * style, char * str)
{
	yyscan_t lexer;
	void *parser;
	YY_BUFFER_STATE buffer;
	int token;
	struct parse_params param = {1, 0, 0};

	css_lex_init(&lexer);
	parser = css_parser_Alloc(malloc);
	css_parser_(parser, LBRACE, strdup("{"), &param);

	buffer = css__scan_string(str, lexer);
	while ((token = css_lex(lexer))) {
		css_parser_(parser, token,
				strdup(css_get_text(lexer)),
				&param);
	}
	css__delete_buffer(buffer, lexer);
	css_parser_(parser, RBRACE, strdup("}"), &param);
	css_parser_(parser, 0, 0, &param);

	css_parser_Free(parser, free);
	css_lex_destroy(lexer);

	css_add_declarations(style, param.declaration);

	css_free_node(param.declaration);
}



/**
 * dump a style
 */

static void dump_length(const struct css_length * const length)
{
	fprintf(stderr, "%g%s", length->value,
	               css_unit_name[length->unit]);
}

void css_dump_style(const struct css_style * const style)
{
	fprintf(stderr, "{ ");
	fprintf(stderr, "background-color: #%lx; ", style->background_color);
	fprintf(stderr, "clear: %s; ", css_clear_name[style->clear]);
	fprintf(stderr, "color: #%lx; ", style->color);
	fprintf(stderr, "display: %s; ", css_display_name[style->display]);
	fprintf(stderr, "float: %s; ", css_float_name[style->float_]);
	fprintf(stderr, "font-size: ");
	switch (style->font_size.size) {
		case CSS_FONT_SIZE_ABSOLUTE: fprintf(stderr, "[%g]", style->font_size.value.absolute); break;
		case CSS_FONT_SIZE_LENGTH:   dump_length(&style->font_size.value.length); break;
		case CSS_FONT_SIZE_PERCENT:  fprintf(stderr, "%g%%", style->font_size.value.percent); break;
		case CSS_FONT_SIZE_INHERIT:  fprintf(stderr, "inherit"); break;
		default:                     fprintf(stderr, "UNKNOWN"); break;
	}
	fprintf(stderr, "; ");
	fprintf(stderr, "height: ");
	switch (style->height.height) {
		case CSS_HEIGHT_AUTO:   fprintf(stderr, "auto"); break;
		case CSS_HEIGHT_LENGTH: dump_length(&style->height.length); break;
		default:                fprintf(stderr, "UNKNOWN"); break;
	}
	fprintf(stderr, "; ");
	fprintf(stderr, "line-height: ");
	switch (style->line_height.size) {
		case CSS_LINE_HEIGHT_ABSOLUTE: fprintf(stderr, "[%g]", style->line_height.value.absolute); break;
		case CSS_LINE_HEIGHT_LENGTH:   dump_length(&style->line_height.value.length); break;
		case CSS_LINE_HEIGHT_PERCENT:  fprintf(stderr, "%g%%", style->line_height.value.percent); break;
		case CSS_LINE_HEIGHT_INHERIT:  fprintf(stderr, "inherit"); break;
		default:                       fprintf(stderr, "UNKNOWN"); break;
	}
	fprintf(stderr, "; ");
	fprintf(stderr, "text-align: %s; ", css_text_align_name[style->text_align]);
	fprintf(stderr, "width: ");
	switch (style->width.width) {
		case CSS_WIDTH_AUTO:    fprintf(stderr, "auto"); break;
		case CSS_WIDTH_LENGTH:  dump_length(&style->width.value.length); break;
		case CSS_WIDTH_PERCENT: fprintf(stderr, "%g%%", style->width.value.percent); break;
		default:                fprintf(stderr, "UNKNOWN"); break;
	}
	fprintf(stderr, "; ");
	fprintf(stderr, "}");
}


void css_dump_stylesheet(const struct css_stylesheet * stylesheet)
{
	unsigned int i;
	struct node *r, *n, *m;
	for (i = 0; i != HASH_SIZE; i++) {
		fprintf(stderr, "hash %i:\n", i);
		for (r = stylesheet->rule[i]; r != 0; r = r->next) {
			for (n = r; n != 0; n = n->right) {
				if (n->data != 0)
					fprintf(stderr, "%s", n->data);
				for (m = n->left; m != 0; m = m->next) {
					switch (m->type) {
						case NODE_ID: fprintf(stderr, "%s", m->data); break;
						case NODE_CLASS: fprintf(stderr, ".%s", m->data); break;
						default: fprintf(stderr, "unexpected node");
					}
				}
				fprintf(stderr, " ");
			}
			css_dump_style(r->style);
			fprintf(stderr, "\n");
		}
	}
}


/**
 * cascade styles
 */

void css_cascade(struct css_style * const style, const struct css_style * const apply)
{
	float f;

	if (apply->background_color != CSS_COLOR_INHERIT)
		style->background_color = apply->background_color;
	if (apply->clear != CSS_CLEAR_INHERIT)
		style->clear = apply->clear;
	if (apply->color != CSS_COLOR_INHERIT)
		style->color = apply->color;
	if (apply->display != CSS_DISPLAY_INHERIT)
		style->display = apply->display;
	if (apply->float_ != CSS_FLOAT_INHERIT)
		style->float_ = apply->float_;
	if (apply->height.height != CSS_HEIGHT_INHERIT)
		style->height = apply->height;
	if (apply->text_align != CSS_TEXT_ALIGN_INHERIT)
		style->text_align = apply->text_align;
	if (apply->width.width != CSS_WIDTH_INHERIT)
		style->width = apply->width;
	if (apply->font_weight != CSS_FONT_WEIGHT_INHERIT)
		style->font_weight = apply->font_weight;
	if (apply->font_style != CSS_FONT_STYLE_INHERIT)
		style->font_style = apply->font_style;

	/* font-size */
	f = apply->font_size.value.percent / 100;
	switch (apply->font_size.size) {
		case CSS_FONT_SIZE_ABSOLUTE:
			style->font_size = apply->font_size;
			break;
		case CSS_FONT_SIZE_LENGTH:
			switch (apply->font_size.value.length.unit) {
				case CSS_UNIT_EM:
					f = apply->font_size.value.length.value;
					break;
				case CSS_UNIT_EX:
					f = apply->font_size.value.length.value * 0.6 /*?*/;
					break;
				default:
					style->font_size = apply->font_size;
			}
			if ((apply->font_size.value.length.unit != CSS_UNIT_EM) &&
			    (apply->font_size.value.length.unit != CSS_UNIT_EX))
				break;
			/* drop through if EM or EX */
		case CSS_FONT_SIZE_PERCENT:
			switch (style->font_size.size) {
				case CSS_FONT_SIZE_ABSOLUTE:
					style->font_size.value.absolute *= f;
					break;
				case CSS_FONT_SIZE_LENGTH:
					style->font_size.value.length.value *= f;
					break;
				default:
					die("attempting percentage of unknown font-size");
			}
			break;
		case CSS_FONT_SIZE_INHERIT:
		default:                     /* leave unchanged */
			break;
	}
}


void css_merge(struct css_style * const style, const struct css_style * const apply)
{
	if (apply->background_color != CSS_COLOR_INHERIT)
		style->background_color = apply->background_color;
	if (apply->clear != CSS_CLEAR_INHERIT)
		style->clear = apply->clear;
	if (apply->color != CSS_COLOR_INHERIT)
		style->color = apply->color;
	if (apply->display != CSS_DISPLAY_INHERIT)
		style->display = apply->display;
	if (apply->float_ != CSS_FLOAT_INHERIT)
		style->float_ = apply->float_;
	if (apply->height.height != CSS_HEIGHT_INHERIT)
		style->height = apply->height;
	if (apply->text_align != CSS_TEXT_ALIGN_INHERIT)
		style->text_align = apply->text_align;
	if (apply->width.width != CSS_WIDTH_INHERIT)
		style->width = apply->width;
	if (apply->font_weight != CSS_FONT_WEIGHT_INHERIT)
		style->font_weight = apply->font_weight;
	if (apply->font_style != CSS_FONT_STYLE_INHERIT)
		style->font_style = apply->font_style;
	if (apply->font_size.size != CSS_FONT_SIZE_INHERIT)
		style->font_size = apply->font_size;
}



unsigned int css_hash(const char *s)
{
	unsigned int z = 0;
	if (s == 0)
		return 0;
	for (; *s != 0; s++)
		z += *s;
	return (z % (HASH_SIZE - 1)) + 1;
}



#ifdef DEBUG

int main()
{
	const char data[] = "h1 { blah: foo; display: block; }"
		"h1.c1 h2#id1 + h3, h4 h5.c2#id2 { size: 100mm; color: red }"
		"p { background-color: #123; clear: left; color: #ff0000; display: block;"
		"float: left; font-size: 150%; height: blah; line-height: 100;"
		"text-align: left right; width: 90%;}";
	struct content c;
	css_create(&c);
	css_process_data(&c, data, 24);
	css_process_data(&c, data + 24, sizeof(data) - 25);
	css_convert(&c, 100, 100);
	return 0;
}

#endif

