/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Redraw of a CONTENT_HTML (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/form.h"
#include "netsurf/render/layout.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


static bool html_redraw_box(struct box *box,
		int x, int y,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour current_background_color);
static bool html_redraw_borders(struct box *box, int x, int y,
		int padding_width, int padding_height, float scale);
static colour html_redraw_darker(colour c);
static colour html_redraw_lighter(colour c);
static colour html_redraw_aa(colour c0, colour c1);
static bool html_redraw_checkbox(int x, int y, int width, int height,
		bool selected);
static bool html_redraw_radio(int x, int y, int width, int height,
		bool selected);
static bool html_redraw_file(int x, int y, int width, int height,
		struct box *box, float scale, colour background_colour);
static bool html_redraw_background(int x, int y,
		struct box *box, float scale, colour background_colour);
static bool html_redraw_scrollbars(struct box *box, float scale,
		int x, int y, int padding_width, int padding_height,
		colour background_colour);

bool html_redraw_debug = false;


/**
 * Draw a CONTENT_HTML using the current set of plotters (plot).
 *
 * \param  c		     content of type CONTENT_HTML
 * \param  x		     coordinate for top-left of redraw
 * \param  y		     coordinate for top-left of redraw
 * \param  width	     available width (not used for HTML redraw)
 * \param  height	     available height (not used for HTML redraw)
 * \param  clip_x0	     clip rectangle
 * \param  clip_y0	     clip rectangle
 * \param  clip_x1	     clip rectangle
 * \param  clip_y1	     clip rectangle
 * \param  scale	     scale for redraw
 * \param  background_colour the background colour
 * \return true if successful, false otherwise
 *
 * x, y, clip_[xy][01] are in target coordinates.
 */

bool html_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	struct box *box;

	box = c->data.html.layout;
	assert(box);

	/* clear to background colour */
	if (c->data.html.background_colour != TRANSPARENT)
		background_colour = c->data.html.background_colour;
	plot.clg(background_colour);

	return html_redraw_box(box, x, y,
			clip_x0, clip_y0, clip_x1, clip_y1,
			scale, background_colour);
}


/**
 * Recursively draw a box.
 *
 * \param  box	    box to draw
 * \param  x	    coordinate of parent box
 * \param  y	    coordinate of parent box
 * \param  clip_x0  clip rectangle
 * \param  clip_y0  clip rectangle
 * \param  clip_x1  clip rectangle
 * \param  clip_y1  clip rectangle
 * \param  scale    scale for redraw
 * \param  current_background_color  background colour under this box
 * \return true if successful, false otherwise
 *
 * x, y, clip_[xy][01] are in target coordinates.
 */

bool html_redraw_box(struct box *box,
		int x, int y,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour current_background_color)
{
	struct box *c;
	int width, height;
	int padding_left, padding_top, padding_width, padding_height;
	int x0, y0, x1, y1;
	int colour;
	int x_scrolled, y_scrolled;

	x += box->x * scale;
	y += box->y * scale;
	width = box->width * scale;
	height = box->height * scale;
	padding_left = box->padding[LEFT] * scale;
	padding_top = box->padding[TOP] * scale;
	padding_width = (box->padding[LEFT] + box->width +
			box->padding[RIGHT]) * scale;
	padding_height = (box->padding[TOP] + box->height +
			box->padding[BOTTOM]) * scale;
	x_scrolled = x - box->scroll_x * scale;
	y_scrolled = y - box->scroll_y * scale;

	/* calculate clip rectangle for this box */
	if (box->style && box->style->overflow != CSS_OVERFLOW_VISIBLE) {
		x0 = x;
		y0 = y;
		x1 = x + padding_width;
		y1 = y + padding_height;
	} else {
		x0 = x + box->descendant_x0 * scale;
		y0 = y + box->descendant_y0 * scale;
		x1 = x + box->descendant_x1 * scale + 1;
		y1 = y + box->descendant_y1 * scale + 1;
	}

	/* if visibility is hidden render children only */
	if (box->style && box->style->visibility == CSS_VISIBILITY_HIDDEN) {
		if (!plot.group_start("hidden box"))
			return false;
		for (c = box->children; c; c = c->next)
			if (!html_redraw_box(c, x_scrolled, y_scrolled,
					x0, y0, x1, y1,
					scale, current_background_color))
				return false;
		return plot.group_end();
	}

	if (!plot.group_start("vis box"))
		return false;

	/* dotted debug outlines */
	if (html_redraw_debug) {
		if (!plot.rectangle(x, y, padding_width, padding_height,
				1, 0x0000ff, true, false))
			return false;
		if (!plot.rectangle(x + padding_left, y + padding_top,
				width, height, 1, 0xff0000, true, false))
			return false;
		if (!plot.rectangle(x - (box->border[LEFT] +
				box->margin[LEFT]) * scale,
				y - (box->border[TOP] +
				box->margin[TOP]) * scale,
				padding_width + (box->border[LEFT] +
				box->margin[LEFT] + box->border[RIGHT] +
				box->margin[RIGHT]) * scale,
				padding_height + (box->border[TOP] +
				box->margin[TOP] + box->border[BOTTOM] +
				box->margin[BOTTOM]) * scale,
				1, 0x00ffff, true, false))
			return false;
	}

	/* borders */
	if (box->style)
		if (!html_redraw_borders(box, x, y,
				padding_width, padding_height,
				scale))
			return false;

	/* return if the box is completely outside the clip rectangle */
	if (clip_y1 < y0 || y1 < clip_y0 || clip_x1 < x0 || x1 < clip_x0)
		return plot.group_end();

	if (box->type == BOX_BLOCK || box->type == BOX_INLINE_BLOCK ||
			box->type == BOX_TABLE_CELL || box->object) {
		/* find intersection of clip rectangle and box */
		if (x0 < clip_x0) x0 = clip_x0;
		if (y0 < clip_y0) y0 = clip_y0;
		if (clip_x1 < x1) x1 = clip_x1;
		if (clip_y1 < y1) y1 = clip_y1;
		/* no point trying to draw 0-width/height boxes */
		if (x0 == x1 || y0 == y1)
			/* not an error, so return true */
			return true;
		/* clip to it */
		if (!plot.clip(x0, y0, x1, y1))
			return false;
	} else {
		/* clip box unchanged */
		x0 = clip_x0;
		y0 = clip_y0;
		x1 = clip_x1;
		y1 = clip_y1;
	}

	/* background colour and image */
	if (box->style && box->type != BOX_BR && (box->type != BOX_INLINE ||
			box->style != box->parent->parent->style)) {
		/* find intersection of clip box and padding box */
		int px0 = x < x0 ? x0 : x;
		int py0 = y < y0 ? y0 : y;
		int px1 = x + padding_width < x1 ? x + padding_width : x1;
		int py1 = y + padding_height < y1 ? y + padding_height : y1;

		/* background colour */
		if (box->style->background_color != TRANSPARENT &&
				px0 < px1 && py0 < py1 &&
				/* don't redraw background if there is an
				 * image covering it */
				((box->style->background_repeat !=
				  CSS_BACKGROUND_REPEAT_REPEAT) ||
				(box->background == NULL) ||
				(box->background->bitmap == NULL) ||
				/*(ro_gui_current_redraw_gui == NULL) ||
				(!ro_gui_current_redraw_gui->
				  option.background_images) ||*/
				(!bitmap_get_opaque(box->background->
				  bitmap)))) {
			if (!plot.fill(px0, py0, px1, py1,
					box->style->background_color))
				return false;
			/* set current background color for font painting */
			current_background_color = box->style->background_color;
		}

		if ((box->background) && (px0 < px1) && (py0 < py1)) {
			/* clip to padding box for everything but the main window */
			if (box->parent) {
				if (!plot.clip(px0, py0, px1, py1))
					return false;
			} else {
				if (!plot.clip(clip_x0, clip_y0,
						clip_x1, clip_y1))
					return false;
			}

			/* plot background image */
			if (!html_redraw_background(x, y, box, scale,
					current_background_color))
				return false;

			/* restore previous graphics window */
			if (!plot.clip(x0, y0, x1, y1))
				return false;
		}
	}

	if (box->object) {
		if (!content_redraw(box->object,
				x_scrolled + padding_left,
				y_scrolled + padding_top,
				width, height, x0, y0, x1, y1, scale,
				current_background_color))
			return false;

	} else if (box->gadget && box->gadget->type == GADGET_CHECKBOX) {
		if (!html_redraw_checkbox(x + padding_left, y + padding_top,
				width, height,
				box->gadget->selected))
			return false;

	} else if (box->gadget && box->gadget->type == GADGET_RADIO) {
		if (!html_redraw_radio(x + padding_left, y + padding_top,
				width, height,
				box->gadget->selected))
			return false;

	} else if (box->gadget && box->gadget->type == GADGET_FILE) {
		if (!html_redraw_file(x + padding_left, y + padding_top,
				width, height, box, scale,
				current_background_color))
			return false;

	} else if (box->text) {
		/* antialias colour for under/overline */
		colour = html_redraw_aa(current_background_color,
				/*print_text_black ? 0 :*/ box->style->color);

		if (box->style->text_decoration &
				CSS_TEXT_DECORATION_UNDERLINE) {
			if (!plot.line(x,
					y + (int) (box->height * 0.9 * scale),
					x + box->width * scale,
					y + (int) (box->height * 0.9 * scale),
					0, colour, false, false))
				return false;
		}

		if (box->style->text_decoration &
				CSS_TEXT_DECORATION_OVERLINE) {
			if (!plot.line(x,
					y + (int) (box->height * 0.1 * scale),
					x + box->width * scale,
					y + (int) (box->height * 0.1 * scale),
					0, colour, false, false))
				return false;
		}

		if (box->style->text_decoration &
				CSS_TEXT_DECORATION_LINE_THROUGH) {
			if (!plot.line(x,
					y + (int) (box->height * 0.5 * scale),
					x + box->width * scale,
					y + (int) (box->height * 0.5 * scale),
					0, colour, false, false))
				return false;
		}

		if (!plot.text(x, y + (int) (box->height * 0.75 * scale),
				box->style, box->text, box->length,
				current_background_color,
				/*print_text_black ? 0 :*/ box->style->color))
			return false;

	} else {
		for (c = box->children; c != 0; c = c->next)
			if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
				if (!html_redraw_box(c, x_scrolled, y_scrolled,
						x0, y0, x1, y1, scale,
						current_background_color))
					return false;

		for (c = box->float_children; c != 0; c = c->next_float)
			if (!html_redraw_box(c, x_scrolled, y_scrolled,
					x0, y0, x1, y1, scale,
					current_background_color))
				return false;
	}

	/* scrollbars */
	if (box->style && box->type != BOX_BR && box->type != BOX_INLINE &&
			(box->style->overflow == CSS_OVERFLOW_SCROLL ||
			box->style->overflow == CSS_OVERFLOW_AUTO))
		if (!html_redraw_scrollbars(box, scale, x, y,
				padding_width, padding_height,
				current_background_color))
			return false;

	if (box->type == BOX_BLOCK || box->type == BOX_INLINE_BLOCK ||
			box->type == BOX_TABLE_CELL || box->object)
		if (!plot.clip(clip_x0, clip_y0, clip_x1, clip_y1))
			return false;

	return plot.group_end();
}


/**
 * Draw borders for a box.
 *
 * \param  box		   box to draw
 * \param  x		   coordinate of left padding edge
 * \param  y		   coordinate of top padding edge
 * \param  padding_width   width of padding box
 * \param  padding_height  height of padding box
 * \param  scale	   scale for redraw
 * \return true if successful, false otherwise
 */

bool html_redraw_borders(struct box *box, int x, int y,
		int padding_width, int padding_height, float scale)
{
	unsigned int i;
	int top = box->border[TOP] * scale;
	int right = box->border[RIGHT] * scale;
	int bottom = box->border[BOTTOM] * scale;
	int left = box->border[LEFT] * scale;
	int p[20] = {
		x, y,
		x - left, y - top,
		x + padding_width + right, y - top,
		x + padding_width, y,
		x + padding_width, y + padding_height,
		x + padding_width + right, y + padding_height + bottom,
		x - left, y + padding_height + bottom,
		x, y + padding_height,
		x, y,
		x - left, y - top
	};
	int z[8];
	colour c;
	bool dotted;
	unsigned int light;

	assert(box->style);

	for (i = 0; i != 4; i++) {
		if (box->border[i] == 0)
			continue;

		c = box->style->border[i].color;
		dotted = false;
		light = i;

		switch (box->style->border[i].style) {
		case CSS_BORDER_STYLE_DOTTED:
			dotted = true;
		case CSS_BORDER_STYLE_DASHED:
			if (!plot.line((p[i * 4 + 0] + p[i * 4 + 2]) / 2,
					(p[i * 4 + 1] + p[i * 4 + 3]) / 2,
					(p[i * 4 + 4] + p[i * 4 + 6]) / 2,
					(p[i * 4 + 5] + p[i * 4 + 7]) / 2,
					box->border[i] * scale,
					c, dotted, !dotted))
				return false;
			continue;

		case CSS_BORDER_STYLE_SOLID:
			break;

		case CSS_BORDER_STYLE_DOUBLE:
			z[0] = p[i * 4 + 0];
			z[1] = p[i * 4 + 1];
			z[2] = (p[i * 4 + 0] * 2 + p[i * 4 + 2]) / 3;
			z[3] = (p[i * 4 + 1] * 2 + p[i * 4 + 3]) / 3;
			z[4] = (p[i * 4 + 6] * 2 + p[i * 4 + 4]) / 3;
			z[5] = (p[i * 4 + 7] * 2 + p[i * 4 + 5]) / 3;
			z[6] = p[i * 4 + 6];
			z[7] = p[i * 4 + 7];
			if (!plot.polygon(z, 4, c))
				return false;
			z[0] = p[i * 4 + 2];
			z[1] = p[i * 4 + 3];
			z[2] = (p[i * 4 + 2] * 2 + p[i * 4 + 0]) / 3;
			z[3] = (p[i * 4 + 3] * 2 + p[i * 4 + 1]) / 3;
			z[4] = (p[i * 4 + 4] * 2 + p[i * 4 + 6]) / 3;
			z[5] = (p[i * 4 + 5] * 2 + p[i * 4 + 7]) / 3;
			z[6] = p[i * 4 + 4];
			z[7] = p[i * 4 + 5];
			if (!plot.polygon(z, 4, c))
				return false;
			continue;

		case CSS_BORDER_STYLE_GROOVE:
			light = 3 - light;
		case CSS_BORDER_STYLE_RIDGE:
			z[0] = p[i * 4 + 0];
			z[1] = p[i * 4 + 1];
			z[2] = (p[i * 4 + 0] + p[i * 4 + 2]) / 2;
			z[3] = (p[i * 4 + 1] + p[i * 4 + 3]) / 2;
			z[4] = (p[i * 4 + 6] + p[i * 4 + 4]) / 2;
			z[5] = (p[i * 4 + 7] + p[i * 4 + 5]) / 2;
			z[6] = p[i * 4 + 6];
			z[7] = p[i * 4 + 7];
			if (!plot.polygon(z, 4,
					light == 0 || light == 1 ?
					html_redraw_darker(c) :
					html_redraw_lighter(c)))
				return false;
			z[0] = p[i * 4 + 2];
			z[1] = p[i * 4 + 3];
			z[6] = p[i * 4 + 4];
			z[7] = p[i * 4 + 5];
			if (!plot.polygon(z, 4,
					light == 0 || light == 1 ?
					html_redraw_lighter(c) :
					html_redraw_darker(c)))
				return false;;
			continue;

		case CSS_BORDER_STYLE_INSET:
			if (i == 0 || i == 3)
				c = html_redraw_darker(c);
			else
				c = html_redraw_lighter(c);
			break;
		case CSS_BORDER_STYLE_OUTSET:
			if (i == 0 || i == 3)
				c = html_redraw_lighter(c);
			else
				c = html_redraw_darker(c);
			break;

		default:
			break;
		}

		if (!plot.polygon(p + i * 4, 4, c))
			return false;
	}

	return true;
}


/**
 * Make a colour darker.
 *
 * \param  c  colour
 * \return  a darker shade of c
 */

colour html_redraw_darker(colour c)
{
	int b = c >> 16, g = (c >> 8) & 0xff, r = c & 0xff;
	b = b * 3 / 4;
	g = g * 3 / 4;
	r = r * 3 / 4;
	return (b << 16) | (g << 8) | r;
}


/**
 * Make a colour lighter.
 *
 * \param  c  colour
 * \return  a lighter shade of c
 */

colour html_redraw_lighter(colour c)
{
	int b = 0xff - (c >> 16), g = 0xff - ((c >> 8) & 0xff),
			r = 0xff - (c & 0xff);
	b = b * 3 / 4;
	g = g * 3 / 4;
	r = r * 3 / 4;
	return ((0xff - b) << 16) | ((0xff - g) << 8) | (0xff - r);
}


/**
 * Mix two colours to produce a colour suitable for anti-aliasing.
 *
 * \param  c0  first colour
 * \param  c1  second colour
 * \return  a colour half way between c0 and c1
 */

colour html_redraw_aa(colour c0, colour c1)
{
	return ((((c0 >> 16) + (c1 >> 16)) / 2) << 16) |
			(((((c0 >> 8) & 0xff) + ((c1 >> 8) & 0xff)) / 2) << 8) |
			((((c0 & 0xff) + (c1 & 0xff)) / 2) << 0);
}


/**
 * Plot a checkbox.
 *
 * \param  x	     left coordinate
 * \param  y	     top coordinate
 * \param  width     dimensions of checkbox
 * \param  height    dimensions of checkbox
 * \param  selected  the checkbox is selected
 * \return true if successful, false otherwise
 */

bool html_redraw_checkbox(int x, int y, int width, int height,
		bool selected)
{
	int z = width * 0.15;
	if (z == 0)
		z = 1;
	if (!plot.fill(x, y, x + width, y + height, 0x000000))
		return false;
	if (!plot.fill(x + z, y + z, x + width - z, y + height - z, 0xffffff))
		return false;
	if (selected)
		if (!plot.fill(x + z + z, y + z + z,
				x + width - z - z, y + height - z - z,
				0x0000ff))
			return false;

	return true;
}


/**
 * Plot a radio icon.
 *
 * \param  x	     left coordinate
 * \param  y	     top coordinate
 * \param  width     dimensions of radio icon
 * \param  height    dimensions of radio icon
 * \param  selected  the radio icon is selected
 * \return true if successful, false otherwise
 */

bool html_redraw_radio(int x, int y, int width, int height,
		bool selected)
{
	if (!plot.disc(x + width * 0.5, y + height * 0.5,
			width * 0.5 - 1, 0x000000))
		return false;
	if (!plot.disc(x + width * 0.5, y + height * 0.5,
			width * 0.4 - 1, 0xffffff))
		return false;
	if (selected)
		if (!plot.disc(x + width * 0.5, y + height * 0.5,
				width * 0.3 - 1, 0x0000ff))
			return false;

	return true;
}


/**
 * Plot a file upload input.
 *
 * \param  x	     left coordinate
 * \param  y	     top coordinate
 * \param  width     dimensions of input
 * \param  height    dimensions of input
 * \param  box	     box of input
 * \param  scale     scale for redraw
 * \param  background_colour  current background colour
 * \return true if successful, false otherwise
 */

bool html_redraw_file(int x, int y, int width, int height,
		struct box *box, float scale, colour background_colour)
{
	int text_width;
	const char *text;
	size_t length;

	if (box->gadget->value)
		text = box->gadget->value;
	else
		text = messages_get("Form_Drop");
	length = strlen(text);

	if (!nsfont_width(box->style, text, length, &text_width))
		return false;
	text_width *= scale;
	if (width < text_width + 8)
		x = x + width - text_width - 4;
	else
		x = x + 4;

	return plot.text(x, y + height * 0.75, box->style, text, length,
			background_colour,
			/*print_text_black ? 0 :*/ box->style->color);
}


/**
 * Plot background images.
 *
 * \param  x	  coordinate of box
 * \param  y	  coordinate of box
 * \param  box	  box to draw background image of
 * \param  scale  scale for redraw
 * \param  background_colour  current background colour
 * \return true if successful, false otherwise
 */

bool html_redraw_background(int x, int y,
		struct box *box, float scale, colour background_colour)
{
	int image_width, image_height;
	bool repeat_x = false;
	bool repeat_y = false;

	assert(box->background);

	/* only bitmaps handled currently */
	if (!box->background->bitmap)
		return true;

	/* exit if background images aren't wanted */
	/*if (ro_gui_current_redraw_gui)
		if (!ro_gui_current_redraw_gui->option.background_images)
			return true;
	else if (!option_background_images)
		return true;*/

	/* get the image dimensions for our positioning and scaling */
	image_width = box->background->width * scale;
	image_height = box->background->height * scale;

	/* handle background-repeat */
	switch (box->style->background_repeat) {
		case CSS_BACKGROUND_REPEAT_REPEAT:
			repeat_x = repeat_y = true;
			break;
		case CSS_BACKGROUND_REPEAT_REPEAT_X:
			repeat_x = true;
			break;
		case CSS_BACKGROUND_REPEAT_REPEAT_Y:
			repeat_y = true;
			break;
		case CSS_BACKGROUND_REPEAT_NO_REPEAT:
			break;
		default:
			break;
	}

	/* handle background-position */
	switch (box->style->background_position.horz.pos) {
		case CSS_BACKGROUND_POSITION_PERCENT:
			x += (box->padding[LEFT] + box->width +
					box->padding[RIGHT] - image_width) *
					box->style->background_position.horz.
					value.percent / 100;
			break;
		case CSS_BACKGROUND_POSITION_LENGTH:
			x += (int) (css_len2px(&box->style->background_position.
					horz.value.length, box->style) * scale);
			break;
		default:
			break;
	}

	switch (box->style->background_position.vert.pos) {
		case CSS_BACKGROUND_POSITION_PERCENT:
			y += (box->padding[TOP] + box->height +
					box->padding[BOTTOM] - image_height) *
					box->style->background_position.vert.
					value.percent / 100;
			break;
		case CSS_BACKGROUND_POSITION_LENGTH:
			y -= (int) (css_len2px(&box->style->background_position.
					vert.value.length, box->style) * scale);
			break;
		default:
			break;
	}

	/* and plot the image */
	return plot.bitmap_tile(x, y, image_width, image_height,
			box->background->bitmap,
			background_colour,
			repeat_x, repeat_y);
}


/**
 * Plot scrollbars for a scrolling box.
 *
 * \param  box	  scrolling box
 * \param  scale  scale for redraw
 * \param  x	  coordinate of box
 * \param  y	  coordinate of box
 * \param  padding_width   width of padding box
 * \param  padding_height  height of padding box
 * \return true if successful, false otherwise
 */

bool html_redraw_scrollbars(struct box *box, float scale,
		int x, int y, int padding_width, int padding_height,
		colour background_colour)
{
	const int w = SCROLLBAR_WIDTH * scale;
	bool vscroll, hscroll;
	int well_height, bar_top, bar_height;
	int well_width, bar_left, bar_width;
	const colour vcolour = box->style->border[RIGHT].color;
	const colour hcolour = box->style->border[BOTTOM].color;

	box_scrollbar_dimensions(box, padding_width, padding_height, w,
			&vscroll, &hscroll,
			&well_height, &bar_top, &bar_height,
			&well_width, &bar_left, &bar_width);

#define TRIANGLE(x0, y0, x1, y1, x2, y2, c) \
	if (!plot.line(x0, y0, x1, y1, scale, c, false, false))	\
		return false;						\
	if (!plot.line(x0, y0, x2, y2, scale, c, false, false))	\
		return false;						\
	if (!plot.line(x1, y1, x2, y2, scale, c, false, false))	\
		return false;

	/* fill scrollbar well(s) with background colour */
	if (vscroll)
		if (!plot.fill(x + padding_width - w, y,
				x + padding_width, y + padding_height,
				background_colour))
			return false;
	if (hscroll)
		if (!plot.fill(x, y + padding_height - w,
				x + padding_width, y + padding_height,
				background_colour))
			return false;

	/* vertical scrollbar */
	if (vscroll) {
		/* left line */
		if (!plot.line(x + padding_width - w, y,
				x + padding_width - w, y + padding_height,
				scale, vcolour, false, false))
			return false;
		/* up arrow */
		TRIANGLE(x + padding_width - w / 2, y + w / 4,
				x + padding_width - w * 3 / 4, y + w * 3 / 4,
				x + padding_width - w / 4, y + w * 3 / 4,
				vcolour);
		/* separator */
		if (!plot.line(x + padding_width - w, y + w,
				x + padding_width, y + w,
				scale, vcolour, false, false))
			return false;
		/* bar */
		if (!plot.rectangle(x + padding_width - w * 3 / 4,
				y + w + bar_top + w / 4,
				w / 2, bar_height - w / 2,
				scale, vcolour, false, false))
			return false;
		/* separator */
		if (!plot.line(x + padding_width - w, y + w + well_height,
				x + padding_width, y + w + well_height,
				scale, vcolour, false, false))
			return false;
		/* down arrow */
		TRIANGLE(x + padding_width - w / 2,
				y + w + well_height + w * 3 / 4,
				x + padding_width - w * 3 / 4,
				y + w + well_height + w / 4,
				x + padding_width - w / 4,
				y + w + well_height + w / 4,
				vcolour);
	}

	/* horizontal scrollbar */
	if (hscroll) {
		/* top line */
		if (!plot.line(x, y + padding_height - w,
				x + well_width + w + w, y + padding_height - w,
				scale, hcolour, false, false))
			return false;
		/* left arrow */
		TRIANGLE(x + w / 4, y + padding_height - w / 2,
				x + w * 3 / 4, y + padding_height - w * 3 / 4,
				x + w * 3 / 4, y + padding_height - w / 4,
				hcolour);
		/* separator */
		if (!plot.line(x + w, y + padding_height - w,
				x + w, y + padding_height,
				scale, hcolour, false, false))
			return false;
		/* bar */
		if (!plot.rectangle(x + w + bar_left + w / 4,
				y + padding_height - w * 3 / 4,
				bar_width - w / 2, w / 2,
				scale, hcolour, false, false))
			return false;
		/* separator */
		if (!plot.line(x + w + well_width, y + padding_height - w,
				x + w + well_width, y + padding_height,
				scale, hcolour, false, false))
			return false;
		/* right arrow */
		TRIANGLE(x + w + well_width + w * 3 / 4,
				y + padding_height - w / 2,
				x + w + well_width + w / 4,
				y + padding_height - w * 3 / 4,
				x + w + well_width + w / 4,
				y + padding_height - w / 4,
				hcolour);
	}

	return true;
}


/**
 * Determine if a box has a vertical scrollbar.
 *
 * \param  box  scrolling box
 * \return the box has a vertical scrollbar
 */

bool box_vscrollbar_present(const struct box * const box)
{
	return box->descendant_y0 < -box->border[TOP] ||
			box->padding[TOP] + box->height + box->padding[BOTTOM] +
			box->border[BOTTOM] < box->descendant_y1;
}


/**
 * Determine if a box has a horizontal scrollbar.
 *
 * \param  box  scrolling box
 * \return the box has a horizontal scrollbar
 */

bool box_hscrollbar_present(const struct box * const box)
{
	return box->descendant_x0 < -box->border[LEFT] ||
			box->padding[LEFT] + box->width + box->padding[RIGHT] +
			box->border[RIGHT] < box->descendant_x1;
}


/**
 * Calculate scrollbar dimensions and positions for a box.
 *
 * \param  box             scrolling box
 * \param  padding_width   scaled width of padding box
 * \param  padding_height  scaled height of padding box
 * \param  w               scaled scrollbar width
 * \param  vscroll         updated to vertical scrollbar present
 * \param  hscroll         updated to horizontal scrollbar present
 * \param  well_height     updated to vertical well height
 * \param  bar_top         updated to top position of vertical scrollbar
 * \param  bar_height      updated to height of vertical scrollbar
 * \param  well_width      updated to horizontal well width
 * \param  bar_left        updated to left position of horizontal scrollbar
 * \param  bar_width       updated to width of horizontal scrollbar
 */

void box_scrollbar_dimensions(const struct box * const box,
		const int padding_width, const int padding_height, const int w,
		bool * const vscroll, bool * const hscroll,
		int * const well_height,
		int * const bar_top, int * const bar_height,
		int * const well_width,
		int * const bar_left, int * const bar_width)
{
	*vscroll = box_vscrollbar_present(box);
	*hscroll = box_hscrollbar_present(box);
	*well_height = padding_height - w - w;
	*bar_top = 0;
	*bar_height = *well_height;
	if (box->descendant_y1 - box->descendant_y0 != 0) {
		*bar_top = (float) *well_height * (float) box->scroll_y /
				(float) (box->descendant_y1 -
				box->descendant_y0);
		*bar_height = (float) *well_height * (float) box->height /
				(float) (box->descendant_y1 -
				box->descendant_y0);
	}
	*well_width = padding_width - w - w - (*vscroll ? w : 0);
	*bar_left = 0;
	*bar_width = *well_width;
	if (box->descendant_x1 - box->descendant_x0 != 0) {
		*bar_left = (float) *well_width * (float) box->scroll_x /
				(float) (box->descendant_x1 -
				box->descendant_x0);
		*bar_width = (float) *well_width * (float) box->width /
				(float) (box->descendant_x1 -
				box->descendant_x0);
	}
}
