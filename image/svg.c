/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2007 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Content for image/svg (implementation).
 */

#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/debugXML.h>
#include "utils/config.h"
#include "content/content.h"
#include "desktop/plotters.h"
#include "image/svg.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"


static bool svg_redraw_svg(xmlNode *svg, int x, int y);
static bool svg_redraw_rect(xmlNode *rect, int x, int y);
static bool svg_redraw_text(xmlNode *text, int x, int y);


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

	xmlDebugDumpDocument(stderr, document);

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
	xmlChar *width = xmlGetProp(svg, "width");
	if (width) {
		c->width = atoi(width);
		xmlFree(width);
	} else {
		c->width = 100;
	}

	xmlChar *height = xmlGetProp(svg, "height");
	if (height) {
		c->height = atoi(height);
		xmlFree(height);
	} else {
		c->height = 100;
	}

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

	return svg_redraw_svg(c->data.svg.svg, x, y);
}


/**
 * Redraw a <svg> element node.
 */

bool svg_redraw_svg(xmlNode *svg, int x, int y)
{
	for (xmlNode *child = svg->children; child; child = child->next) {
		bool ok = true;

		if (child->type == XML_ELEMENT_NODE) {
			if (strcmp(child->name, "svg") == 0)
				ok = svg_redraw_svg(child, x, y);
			else if (strcmp(child->name, "g") == 0)
				ok = svg_redraw_svg(child, x, y);
			else if (strcmp(child->name, "rect") == 0)
				ok = svg_redraw_rect(child, x, y);
			else if (strcmp(child->name, "text") == 0)
				ok = svg_redraw_text(child, x, y);
		}

		if (!ok)
			return false;
	}

	return true;
}


/**
 * Redraw a <rect> element node.
 */

bool svg_redraw_rect(xmlNode *rect, int x, int y)
{
	int width = 0, height = 0;

	for (xmlAttr *attr = rect->properties; attr; attr = attr->next) {
		if (strcmp(attr->name, "x") == 0)
			x += atoi(attr->children->content);
		else if (strcmp(attr->name, "y") == 0)
			y += atoi(attr->children->content);
		else if (strcmp(attr->name, "width") == 0)
			width = atoi(attr->children->content);
		else if (strcmp(attr->name, "height") == 0)
			height = atoi(attr->children->content);
	}

	return plot.rectangle(x, y, width, height, 5, 0x000000, false, false);
}


/**
 * Redraw a <text> or <tspan> element node.
 */

bool svg_redraw_text(xmlNode *text, int x, int y)
{
	for (xmlAttr *attr = text->properties; attr; attr = attr->next) {
		if (strcmp(attr->name, "x") == 0)
			x += atoi(attr->children->content);
		else if (strcmp(attr->name, "y") == 0)
			y += atoi(attr->children->content);
	}

	for (xmlNode *child = text->children; child; child = child->next) {
		bool ok = true;

		if (child->type == XML_TEXT_NODE) {
			ok = plot.text(x, y, &css_base_style,
					child->content, strlen(child->content),
					0xffffff, 0x000000);
		} else if (child->type == XML_ELEMENT_NODE &&
				strcmp(child->name, "tspan") == 0) {
			ok = svg_redraw_text(child, x, y);
		}

		if (!ok)
			return false;
	}

	return true;
}


/**
 * Destroy a CONTENT_SVG and free all resources it owns.
 */

void svg_destroy(struct content *c)
{
	if (c->data.svg.doc)
		xmlFreeDoc(c->data.svg.doc);
}
