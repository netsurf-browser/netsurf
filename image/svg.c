/*
 * Copyright 2007 James Bursa <bursa@users.sourceforge.net>
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
 * Content for image/svg (implementation).
 */

#define _GNU_SOURCE  /* for strndup */
#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils/config.h"
#ifdef WITH_NS_SVG
#include <libxml/parser.h>
#include <libxml/debugXML.h>
#include "content/content.h"
#include "css/css.h"
#include "desktop/plotters.h"
#include "desktop/options.h"
#include "image/svg.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"


struct svg_redraw_state {
	/* screen origin */
	int origin_x;
	int origin_y;

	float viewport_width;
	float viewport_height;

	/* current transformation matrix */
	struct {
		float a, b, c, d, e, f;
	} ctm;

	struct css_style style;

	/* paint attributes */
	colour fill;
	colour stroke;
	int stroke_width;
};


static bool svg_redraw_svg(xmlNode *svg, struct svg_redraw_state state);
static bool svg_redraw_path(xmlNode *path, struct svg_redraw_state state);
static bool svg_redraw_rect(xmlNode *rect, struct svg_redraw_state state);
static bool svg_redraw_circle(xmlNode *circle, struct svg_redraw_state state);
static bool svg_redraw_line(xmlNode *line, struct svg_redraw_state state);
static bool svg_redraw_poly(xmlNode *poly, struct svg_redraw_state state,
		bool polygon);
static bool svg_redraw_text(xmlNode *text, struct svg_redraw_state state);
static void svg_parse_position_attributes(const xmlNode *node,
		const struct svg_redraw_state state,
		float *x, float *y, float *width, float *height);
static float svg_parse_length(const xmlChar *s, int viewport_size,
		const struct svg_redraw_state state);
static void svg_parse_paint_attributes(const xmlNode *node,
		struct svg_redraw_state *state);
static void svg_parse_color(const char *s, colour *c);
static void svg_parse_font_attributes(const xmlNode *node,
		struct svg_redraw_state *state);
static void svg_parse_transform_attributes(xmlNode *node,
		struct svg_redraw_state *state);


/**
 * Create a CONTENT_SVG.
 */

bool svg_create(struct content *c, const char *params[])
{
	c->data.svg.doc = 0;
	c->data.svg.svg = 0;

	return true;
}


/**
 * Convert a CONTENT_SVG for display.
 */

bool svg_convert(struct content *c, int w, int h)
{
	xmlDoc *document;
	xmlNode *svg;
	union content_msg_data msg_data;

	/* parse XML to tree */
	document = xmlReadMemory(c->source_data, c->source_size,
			c->url, 0, XML_PARSE_NONET | XML_PARSE_COMPACT);
	if (!document) {
		LOG(("xmlReadMemory failed"));
		msg_data.error = messages_get("ParsingFail");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	c->data.svg.doc = document;

	/* xmlDebugDumpDocument(stderr, document); */

	/* find root <svg> element */
	for (svg = document->children;
			svg && svg->type != XML_ELEMENT_NODE;
			svg = svg->next)
		continue;
	if (!svg) {
		LOG(("no element in svg"));
		msg_data.error = "no element in svg";
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}
	if (strcmp(svg->name, "svg") != 0) {
		LOG(("root element is not svg"));
		msg_data.error = "root element is not svg";
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
        }
	c->data.svg.svg = svg;

	/* get graphic dimensions */
	struct svg_redraw_state state;
	state.viewport_width = w;
	state.viewport_height = h;
	state.style = css_base_style;
	float x, y, width, height;
	svg_parse_position_attributes(svg, state, &x, &y, &width, &height);
	c->width = width;
	c->height = height;

	/*c->title = malloc(100);
	if (c->title)
		snprintf(c->title, 100, messages_get("svgTitle"),
				width, height, c->source_size);*/
	//c->size += ?;
	c->status = CONTENT_STATUS_DONE;
	return true;
}


/**
 * Redraw a CONTENT_SVG.
 */

bool svg_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	assert(c->data.svg.svg);

	struct svg_redraw_state state;

	state.origin_x = x;
	state.origin_y = y;
	state.viewport_width = width;
	state.viewport_height = height;
	state.ctm.a = scale;
	state.ctm.b = 0;
	state.ctm.c = 0;
	state.ctm.d = scale;
	state.ctm.e = 0;
	state.ctm.f = 0;
	state.style = css_base_style;
	state.style.font_size.value.length.value = option_font_size * 0.1;
	state.fill = 0x000000;
	state.stroke = TRANSPARENT;
	state.stroke_width = 1;

	plot.clg(0xffffff);

	return svg_redraw_svg(c->data.svg.svg, state);
}


/**
 * Redraw a <svg> or <g> element node.
 */

bool svg_redraw_svg(xmlNode *svg, struct svg_redraw_state state)
{
	float x, y;

	svg_parse_position_attributes(svg, state,
			&x, &y,
			&state.viewport_width, &state.viewport_height);
	svg_parse_paint_attributes(svg, &state);
	svg_parse_font_attributes(svg, &state);

	/* parse viewBox */
	xmlAttr *view_box = xmlHasProp(svg, (const xmlChar *) "viewBox");
	if (view_box) {
		const char *s = (const char *) view_box->children->content;
		float min_x, min_y, width, height;
		if (sscanf(s, "%f,%f,%f,%f",
				&min_x, &min_y, &width, &height) == 4 ||
				sscanf(s, "%f %f %f %f",
				&min_x, &min_y, &width, &height) == 4) {
			state.ctm.a *= state.viewport_width / width;
			state.ctm.d *= state.viewport_height / height;
			state.ctm.e = -min_x * state.ctm.a;
			state.ctm.f = -min_y * state.ctm.d;
		}
	}

	svg_parse_transform_attributes(svg, &state);

	for (xmlNode *child = svg->children; child; child = child->next) {
		bool ok = true;

		if (child->type == XML_ELEMENT_NODE) {
			if (strcmp(child->name, "svg") == 0)
				ok = svg_redraw_svg(child, state);
			else if (strcmp(child->name, "g") == 0)
				ok = svg_redraw_svg(child, state);
			else if (strcmp(child->name, "a") == 0)
				ok = svg_redraw_svg(child, state);
			else if (strcmp(child->name, "path") == 0)
				ok = svg_redraw_path(child, state);
			else if (strcmp(child->name, "rect") == 0)
				ok = svg_redraw_rect(child, state);
			else if (strcmp(child->name, "circle") == 0)
				ok = svg_redraw_circle(child, state);
			else if (strcmp(child->name, "line") == 0)
				ok = svg_redraw_line(child, state);
			else if (strcmp(child->name, "polyline") == 0)
				ok = svg_redraw_poly(child, state, false);
			else if (strcmp(child->name, "polygon") == 0)
				ok = svg_redraw_poly(child, state, true);
			else if (strcmp(child->name, "text") == 0)
				ok = svg_redraw_text(child, state);
		}

		if (!ok)
			return false;
	}

	return true;
}


/**
 * Redraw a <path> element node.
 *
 * http://www.w3.org/TR/SVG11/paths#PathElement
 */

bool svg_redraw_path(xmlNode *path, struct svg_redraw_state state)
{
	char *s, *path_d;

	svg_parse_paint_attributes(path, &state);
	svg_parse_transform_attributes(path, &state);

	/* read d attribute */
	s = path_d = (char *) xmlGetProp(path, (const xmlChar *) "d");
	if (!s) {
		LOG(("path missing d attribute"));
		return false;
	}

	/* allocate space for path: it will never have more elements than d */
	float *p = malloc(sizeof p[0] * strlen(s));
	if (!p) {
		LOG(("out of memory"));
		return false;
	}

	/* parse d and build path */
	for (unsigned int i = 0; s[i]; i++)
		if (s[i] == ',')
			s[i] = ' ';
	unsigned int i = 0;
	float last_x = 0, last_y = 0;
	float last_cubic_x = 0, last_cubic_y = 0;
	float last_quad_x = 0, last_quad_y = 0;
	while (*s) {
		char command[2];
		int plot_command;
		float x, y, x1, y1, x2, y2;
		int n;

		/* moveto (M, m), lineto (L, l) (2 arguments) */
		if (sscanf(s, " %1[MmLl] %f %f %n", command, &x, &y, &n) == 3) {
			/*LOG(("moveto or lineto"));*/
			if (*command == 'M' || *command == 'm')
				plot_command = PLOTTER_PATH_MOVE;
			else
				plot_command = PLOTTER_PATH_LINE;
			do {
				p[i++] = plot_command;
				if ('a' <= *command) {
					x += last_x;
					y += last_y;
				}
				p[i++] = last_cubic_x = last_quad_x = last_x
						= x;
				p[i++] = last_cubic_y = last_quad_y = last_y
						= y;
				s += n;
				plot_command = PLOTTER_PATH_LINE;
			} while (sscanf(s, "%f %f %n", &x, &y, &n) == 2);

		/* closepath (Z, z) (no arguments) */
		} else if (sscanf(s, " %1[Zz] %n", command, &n) == 1) {
			/*LOG(("closepath"));*/
			p[i++] = PLOTTER_PATH_CLOSE;
			s += n;

		/* horizontal lineto (H, h) (1 argument) */
		} else if (sscanf(s, " %1[Hh] %f %n", command, &x, &n) == 2) {
			/*LOG(("horizontal lineto"));*/
			do {
				p[i++] = PLOTTER_PATH_LINE;
				if (*command == 'h')
					x += last_x;
				p[i++] = last_cubic_x = last_quad_x = last_x
						= x;
				p[i++] = last_cubic_y = last_quad_y = last_y;
				s += n;
			} while (sscanf(s, "%f %n", &x, &n) == 1);

		/* vertical lineto (V, v) (1 argument) */
		} else if (sscanf(s, " %1[Vv] %f %n", command, &y, &n) == 2) {
			/*LOG(("vertical lineto"));*/
			do {
				p[i++] = PLOTTER_PATH_LINE;
				if (*command == 'v')
					y += last_y;
				p[i++] = last_cubic_x = last_quad_x = last_x;
				p[i++] = last_cubic_y = last_quad_y = last_y
						= y;
				s += n;
			} while (sscanf(s, "%f %n", &x, &n) == 1);

		/* curveto (C, c) (6 arguments) */
		} else if (sscanf(s, " %1[Cc] %f %f %f %f %f %f %n", command,
				&x1, &y1, &x2, &y2, &x, &y, &n) == 7) {
			/*LOG(("curveto"));*/
			do {
				p[i++] = PLOTTER_PATH_BEZIER;
				if (*command == 'c') {
					x1 += last_x;
					y1 += last_y;
					x2 += last_x;
					y2 += last_y;
					x += last_x;
					y += last_y;
				}
				p[i++] = x1;
				p[i++] = y1;
				p[i++] = last_cubic_x = x2;
				p[i++] = last_cubic_y = y2;
				p[i++] = last_quad_x = last_x = x;
				p[i++] = last_quad_y = last_y = y;
				s += n;
			} while (sscanf(s, "%f %f %f %f %f %f %n",
					&x1, &y1, &x2, &y2, &x, &y, &n) == 6);

		/* shorthand/smooth curveto (S, s) (4 arguments) */
		} else if (sscanf(s, " %1[Ss] %f %f %f %f %n", command,
				&x2, &y2, &x, &y, &n) == 5) {
			/*LOG(("shorthand/smooth curveto"));*/
			do {
				p[i++] = PLOTTER_PATH_BEZIER;
				x1 = last_x + (last_x - last_cubic_x);
				y1 = last_y + (last_y - last_cubic_y);
				if (*command == 's') {
					x2 += last_x;
					y2 += last_y;
					x += last_x;
					y += last_y;
				}
				p[i++] = x1;
				p[i++] = y1;
				p[i++] = last_cubic_x = x2;
				p[i++] = last_cubic_y = y2;
				p[i++] = last_quad_x = last_x = x;
				p[i++] = last_quad_y = last_y = y;
				s += n;
			} while (sscanf(s, "%f %f %f %f %n",
					&x2, &y2, &x, &y, &n) == 4);

		/* quadratic Bezier curveto (Q, q) (4 arguments) */
		} else if (sscanf(s, " %1[Qq] %f %f %f %f %n", command,
				&x1, &y1, &x, &y, &n) == 5) {
			/*LOG(("quadratic Bezier curveto"));*/
			do {
				p[i++] = PLOTTER_PATH_BEZIER;
				last_quad_x = x1;
				last_quad_y = y1;
				if (*command == 'q') {
					x1 += last_x;
					y1 += last_y;
					x += last_x;
					y += last_y;
				}
				p[i++] = 1./3 * last_x + 2./3 * x1;
				p[i++] = 1./3 * last_y + 2./3 * y1;
				p[i++] = 2./3 * x1 + 1./3 * x;
				p[i++] = 2./3 * y1 + 1./3 * y;
				p[i++] = last_cubic_x = last_x = x;
				p[i++] = last_cubic_y = last_y = y;
				s += n;
			} while (sscanf(s, "%f %f %f %f %n",
					&x1, &y1, &x, &y, &n) == 4);

		/* shorthand/smooth quadratic Bezier curveto (T, t)
		   (2 arguments) */
		} else if (sscanf(s, " %1[Tt] %f %f %n", command,
				&x, &y, &n) == 3) {
			/*LOG(("shorthand/smooth quadratic Bezier curveto"));*/
			do {
				p[i++] = PLOTTER_PATH_BEZIER;
				x1 = last_x + (last_x - last_quad_x);
				y1 = last_y + (last_y - last_quad_y);
				last_quad_x = x1;
				last_quad_y = y1;
				if (*command == 't') {
					x1 += last_x;
					y1 += last_y;
					x += last_x;
					y += last_y;
				}
				p[i++] = 1./3 * last_x + 2./3 * x1;
				p[i++] = 1./3 * last_y + 2./3 * y1;
				p[i++] = 2./3 * x1 + 1./3 * x;
				p[i++] = 2./3 * y1 + 1./3 * y;
				p[i++] = last_cubic_x = last_x = x;
				p[i++] = last_cubic_y = last_y = y;
				s += n;
			} while (sscanf(s, "%f %f %n",
					&x, &y, &n) == 2);

		} else {
			LOG(("parse failed at \"%s\"", s));
			break;
		}
	}

	xmlFree(path_d);

	/*LOG(("path:"));
	for (unsigned int j = 0; j != i; j++) {
		LOG(("    %f", p[j]));
	}*/

	bool ok = plot.path(p, i, state.fill, state.stroke_width, state.stroke,
			&state.ctm.a);

	free(p);

	return ok;
}


/**
 * Redraw a <rect> element node.
 *
 * http://www.w3.org/TR/SVG11/shapes#RectElement
 */

bool svg_redraw_rect(xmlNode *rect, struct svg_redraw_state state)
{
	float x, y, width, height;

	svg_parse_position_attributes(rect, state,
			&x, &y, &width, &height);
	svg_parse_paint_attributes(rect, &state);
	svg_parse_transform_attributes(rect, &state);

	float p[] = { PLOTTER_PATH_MOVE, x, y,
		PLOTTER_PATH_LINE, x + width, y,
		PLOTTER_PATH_LINE, x + width, y + height,
		PLOTTER_PATH_LINE, x,         y + height,
		PLOTTER_PATH_CLOSE };

	return plot.path(p, sizeof p / sizeof p[0], state.fill,
			state.stroke_width, state.stroke, &state.ctm.a);
}


/**
 * Redraw a <circle> element node.
 */

bool svg_redraw_circle(xmlNode *circle, struct svg_redraw_state state)
{
	float x = 0, y = 0, r = 0;

	for (xmlAttr *attr = circle->properties; attr; attr = attr->next) {
		if (strcmp(attr->name, "cx") == 0)
			x = svg_parse_length(attr->children->content,
					state.viewport_width, state);
		else if (strcmp(attr->name, "cy") == 0)
			y = svg_parse_length(attr->children->content,
					state.viewport_height, state);
		else if (strcmp(attr->name, "r") == 0)
			r = svg_parse_length(attr->children->content,
					state.viewport_width, state);
        }
	svg_parse_paint_attributes(circle, &state);
	svg_parse_transform_attributes(circle, &state);

	int px = state.origin_x + state.ctm.a * x +
			state.ctm.c * y + state.ctm.e;
	int py = state.origin_y + state.ctm.b * x +
			state.ctm.d * y + state.ctm.f;
	int pr = r * state.ctm.a;

	if (state.fill != TRANSPARENT)
		if (!plot.disc(px, py, pr, state.fill, true))
			return false;

	if (state.stroke != TRANSPARENT)
		if (!plot.disc(px, py, pr, state.stroke, false))
			return false;

	return true;
}


/**
 * Redraw a <line> element node.
 */

bool svg_redraw_line(xmlNode *line, struct svg_redraw_state state)
{
	float x1 = 0, y1 = 0, x2 = 0, y2 = 0;

	for (xmlAttr *attr = line->properties; attr; attr = attr->next) {
		if (strcmp(attr->name, "x1") == 0)
			x1 = svg_parse_length(attr->children->content,
					state.viewport_width, state);
		else if (strcmp(attr->name, "y1") == 0)
			y1 = svg_parse_length(attr->children->content,
					state.viewport_height, state);
		else if (strcmp(attr->name, "x2") == 0)
			x2 = svg_parse_length(attr->children->content,
					state.viewport_width, state);
		else if (strcmp(attr->name, "y2") == 0)
			y2 = svg_parse_length(attr->children->content,
					state.viewport_height, state);
        }
	svg_parse_paint_attributes(line, &state);
	svg_parse_transform_attributes(line, &state);

	int px1 = state.origin_x + state.ctm.a * x1 +
			state.ctm.c * y1 + state.ctm.e;
	int py1 = state.origin_y + state.ctm.b * x1 +
			state.ctm.d * y1 + state.ctm.f;
	int px2 = state.origin_x + state.ctm.a * x2 +
			state.ctm.c * y2 + state.ctm.e;
	int py2 = state.origin_y + state.ctm.b * x2 +
			state.ctm.d * y2 + state.ctm.f;

	return plot.line(px1, py1, px2, py2, state.stroke_width, state.stroke,
			false, false);
}


/**
 * Redraw a <polyline> or <polygon> element node.
 *
 * http://www.w3.org/TR/SVG11/shapes#PolylineElement
 * http://www.w3.org/TR/SVG11/shapes#PolygonElement
 */

bool svg_redraw_poly(xmlNode *poly, struct svg_redraw_state state,
		bool polygon)
{
	char *s, *points;

	svg_parse_paint_attributes(poly, &state);
	svg_parse_transform_attributes(poly, &state);

	/* read d attribute */
	s = points = (char *) xmlGetProp(poly, (const xmlChar *) "points");
	if (!s) {
		LOG(("poly missing d attribute"));
		return false;
	}

	/* allocate space for path: it will never have more elements than s */
	float *p = malloc(sizeof p[0] * strlen(s));
	if (!p) {
		LOG(("out of memory"));
		return false;
	}

	/* parse s and build path */
	for (unsigned int i = 0; s[i]; i++)
		if (s[i] == ',')
			s[i] = ' ';
	unsigned int i = 0;
	while (*s) {
		float x, y;
		int n;

		if (sscanf(s, "%f %f %n", &x, &y, &n) == 2) {
			if (i == 0)
				p[i++] = PLOTTER_PATH_MOVE;
			else
				p[i++] = PLOTTER_PATH_LINE;
			p[i++] = x;
			p[i++] = y;
			s += n;
                } else {
                	break;
                }
        }
        if (polygon)
		p[i++] = PLOTTER_PATH_CLOSE;

	xmlFree(points);

	bool ok = plot.path(p, i, state.fill, state.stroke_width, state.stroke,
			&state.ctm.a);

	free(p);

	return ok;
}


/**
 * Redraw a <text> or <tspan> element node.
 */

bool svg_redraw_text(xmlNode *text, struct svg_redraw_state state)
{
	float x, y, width, height;

	svg_parse_position_attributes(text, state,
			&x, &y, &width, &height);
	svg_parse_font_attributes(text, &state);
	svg_parse_transform_attributes(text, &state);

	int px = state.origin_x + state.ctm.a * x +
			state.ctm.c * y + state.ctm.e;
	int py = state.origin_y + state.ctm.b * x +
			state.ctm.d * y + state.ctm.f;
/* 	state.ctm.e = px - state.origin_x; */
/* 	state.ctm.f = py - state.origin_y; */

	struct css_style style = state.style;
	style.font_size.value.length.value *= state.ctm.a;

	for (xmlNode *child = text->children; child; child = child->next) {
		bool ok = true;

		if (child->type == XML_TEXT_NODE) {
			ok = plot.text(px, py,
					&css_base_style,
					child->content, strlen(child->content),
					0xffffff, 0x000000);
		} else if (child->type == XML_ELEMENT_NODE &&
				strcmp(child->name, "tspan") == 0) {
			ok = svg_redraw_text(child, state);
		}

		if (!ok)
			return false;
	}

	return true;
}


/**
 * Parse x, y, width, and height attributes, if present.
 */

void svg_parse_position_attributes(const xmlNode *node,
		const struct svg_redraw_state state,
		float *x, float *y, float *width, float *height)
{
	*x = 0;
	*y = 0;
	*width = state.viewport_width;
	*height = state.viewport_height;

	for (xmlAttr *attr = node->properties; attr; attr = attr->next) {
		if (strcmp(attr->name, "x") == 0)
			*x = svg_parse_length(attr->children->content,
					state.viewport_width, state);
		else if (strcmp(attr->name, "y") == 0)
			*y = svg_parse_length(attr->children->content,
					state.viewport_height, state);
		else if (strcmp(attr->name, "width") == 0)
			*width = svg_parse_length(attr->children->content,
					state.viewport_width, state);
		else if (strcmp(attr->name, "height") == 0)
			*height = svg_parse_length(attr->children->content,
					state.viewport_height, state);
	}
}


/**
 * Parse a length as a number of pixels.
 */

float svg_parse_length(const xmlChar *s, int viewport_size,
		const struct svg_redraw_state state)
{
	int num_length = strspn(s, "0123456789+-.");
	const xmlChar *unit = s + num_length;
	float n = atof((const char *) s);
	float font_size = css_len2px(&state.style.font_size.value.length, 0);

	if (unit[0] == 0) {
		return n;
	} else if (unit[0] == '%') {
		return n / 100.0 * viewport_size;
	} else if (unit[0] == 'e' && unit[1] == 'm') {
		return n * font_size;
	} else if (unit[0] == 'e' && unit[1] == 'x') {
		return n / 2.0 * font_size;
	} else if (unit[0] == 'p' && unit[1] == 'x') {
		return n;
	} else if (unit[0] == 'p' && unit[1] == 't') {
		return n * 1.25;
	} else if (unit[0] == 'p' && unit[1] == 'c') {
		return n * 15.0;
	} else if (unit[0] == 'm' && unit[1] == 'm') {
		return n * 3.543307;
	} else if (unit[0] == 'c' && unit[1] == 'm') {
		return n * 35.43307;
	} else if (unit[0] == 'i' && unit[1] == 'n') {
		return n * 90;
	}

	return 0;
}


/**
 * Parse paint attributes, if present.
 */

void svg_parse_paint_attributes(const xmlNode *node,
		struct svg_redraw_state *state)
{
	for (const xmlAttr *attr = node->properties; attr; attr = attr->next) {
		if (strcmp(attr->name, "fill") == 0)
			svg_parse_color((const char *) attr->children->content,
					&state->fill);
		else if (strcmp(attr->name, "stroke") == 0)
			svg_parse_color((const char *) attr->children->content,
					&state->stroke);
		else if (strcmp(attr->name, "stroke-width") == 0)
			state->stroke_width = svg_parse_length(
					attr->children->content,
					state->viewport_width, *state);
		else if (strcmp(attr->name, "style") == 0) {
			const char *style = attr->children->content;
			const char *s;
			char *value;
			if ((s = strstr(style, "fill:"))) {
				s += 5;
				while (*s == ' ')
					s++;
				value = strndup(s, strcspn(s, "; "));
				svg_parse_color(value, &state->fill);
				free(value);
			}
			if ((s = strstr(style, "stroke:"))) {
				s += 7;
				while (*s == ' ')
					s++;
				value = strndup(s, strcspn(s, "; "));
				svg_parse_color(value, &state->stroke);
				free(value);
			}
			if ((s = strstr(style, "stroke-width:"))) {
				s += 13;
				while (*s == ' ')
					s++;
				state->stroke_width = svg_parse_length(s,
						state->viewport_width, *state);
			}
		}
	}
}


/**
 * Parse a colour.
 */

void svg_parse_color(const char *s, colour *c)
{
	unsigned int r, g, b;
	float rf, gf, bf;
	size_t len = strlen(s);

	if (len == 4 && s[0] == '#') {
		if (sscanf(s + 1, "%1x%1x%1x", &r, &g, &b) == 3)
			*c = (b << 20) | (b << 16) |
			     (g << 12) | (g << 8) |
			     (r << 4) | r;
	} else if (len == 7 && s[0] == '#') {
		if (sscanf(s + 1, "%2x%2x%2x", &r, &g, &b) == 3)
			*c = (b << 16) | (g << 8) | r;
	} else if (10 <= len && s[0] == 'r' && s[1] == 'g' && s[2] == 'b' &&
			s[3] == '(' && s[len - 1] == ')') {
		if (sscanf(s + 4, "%i,%i,%i", &r, &g, &b) == 3)
			*c = (b << 16) | (g << 8) | r;
		else if (sscanf(s + 4, "%f%%,%f%%,%f%%", &rf, &gf, &bf) == 3) {
			b = bf * 255 / 100;
			g = gf * 255 / 100;
			r = rf * 255 / 100;
			*c = (b << 16) | (g << 8) | r;
		}
	} else if (len == 4 && strcmp(s, "none") == 0) {
		*c = TRANSPARENT;
	} else {
		colour named = named_colour(s);
		if (named != CSS_COLOR_NONE)
			*c = named;
	}
}


/**
 * Parse font attributes, if present.
 */

void svg_parse_font_attributes(const xmlNode *node,
		struct svg_redraw_state *state)
{
	for (const xmlAttr *attr = node->properties; attr; attr = attr->next) {
		if (strcmp(attr->name, "font-size") == 0) {
			/*if (css_parse_length(
					(const char *) attr->children->content,
					&state->style.font_size.value.length,
					true, true)) {
				state->style.font_size.size =
						CSS_FONT_SIZE_LENGTH;
			}*/
		}
        }
}


/**
 * Parse transform attributes, if present.
 *
 * http://www.w3.org/TR/SVG11/coords#TransformAttribute
 */

void svg_parse_transform_attributes(xmlNode *node,
		struct svg_redraw_state *state)
{
	char *transform, *s;
	float a, b, c, d, e, f;
	float ctm_a, ctm_b, ctm_c, ctm_d, ctm_e, ctm_f;
	float angle, x, y;
	int n;

	/* parse transform */
	s = transform = (char *) xmlGetProp(node,
			(const xmlChar *) "transform");
	if (transform) {
		for (unsigned int i = 0; transform[i]; i++)
			if (transform[i] == ',')
				transform[i] = ' ';

		while (*s) {
			a = d = 1;
			b = c = 0;
			e = f = 0;
			if (sscanf(s, "matrix (%f %f %f %f %f %f) %n",
					&a, &b, &c, &d, &e, &f, &n) == 6)
				;
			else if (sscanf(s, "translate (%f %f) %n",
					&e, &f, &n) == 2)
				;
			else if (sscanf(s, "translate (%f) %n",
					&e, &n) == 1)
				;
			else if (sscanf(s, "scale (%f %f) %n",
					&a, &d, &n) == 2)
				;
			else if (sscanf(s, "scale (%f) %n",
					&a, &n) == 1)
				d = a;
			else if (sscanf(s, "rotate (%f %f %f) %n",
					&angle, &x, &y, &n) == 3) {
				angle = -angle / 180 * M_PI;
				a = cos(angle);
				b = sin(angle);
				c = -sin(angle);
				d = cos(angle);
				e = -x * cos(angle) + y * sin(angle) + x;
				f = -x * sin(angle) - y * cos(angle) + y;
	                } else if (sscanf(s, "rotate (%f) %n",
					&angle, &n) == 1) {
				angle = -angle / 180 * M_PI;
				a = cos(angle);
				b = sin(angle);
				c = -sin(angle);
				d = cos(angle);
	                } else if (sscanf(s, "skewX (%f) %n",
					&angle, &n) == 1) {
				angle = angle / 180 * M_PI;
				c = tan(angle);
	                } else if (sscanf(s, "skewY (%f) %n",
					&angle, &n) == 1) {
				angle = angle / 180 * M_PI;
				b = tan(angle);
	                } else
				break;
			ctm_a = state->ctm.a * a + state->ctm.c * b;
			ctm_b = state->ctm.b * a + state->ctm.d * b;
			ctm_c = state->ctm.a * c + state->ctm.c * d;
			ctm_d = state->ctm.b * c + state->ctm.d * d;
			ctm_e = state->ctm.a * e + state->ctm.c * f +
					state->ctm.e;
			ctm_f = state->ctm.b * e + state->ctm.d * f +
					state->ctm.f;
			state->ctm.a = ctm_a;
			state->ctm.b = ctm_b;
			state->ctm.c = ctm_c;
			state->ctm.d = ctm_d;
			state->ctm.e = ctm_e;
			state->ctm.f = ctm_f;
			s += n;
		}

		xmlFree(transform);
	}
}


/**
 * Destroy a CONTENT_SVG and free all resources it owns.
 */

void svg_destroy(struct content *c)
{
	if (c->data.svg.doc)
		xmlFreeDoc(c->data.svg.doc);
}

#endif /* WITH_NS_SVG */
