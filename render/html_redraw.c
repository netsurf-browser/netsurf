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
#include "netsurf/desktop/gui.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/desktop/selection.h"
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
static bool html_redraw_text_box(struct box *box, int x, int y,
		int x0, int y0, int x1, int y1,
		float scale, colour current_background_color);
static bool html_redraw_borders(struct box *box, int x_parent, int y_parent,
		int padding_width, int padding_height, float scale);
static bool html_redraw_border_plot(int i, int *p, colour c,
		css_border_style style, int thickness);
static colour html_redraw_darker(colour c);
static colour html_redraw_lighter(colour c);
static colour html_redraw_aa(colour c0, colour c1);
static bool html_redraw_checkbox(int x, int y, int width, int height,
		bool selected);
static bool html_redraw_radio(int x, int y, int width, int height,
		bool selected);
static bool html_redraw_file(int x, int y, int width, int height,
		struct box *box, float scale, colour background_colour);
static bool html_redraw_background(int x, int y, struct box *box, float scale,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		colour *background_colour);
static bool html_redraw_text_decoration(struct box *box,
		int x_parent, int y_parent, float scale,
		colour background_colour);
static bool html_redraw_text_decoration_inline(struct box *box, int x, int y,
		float scale, colour colour, float ratio);
static bool html_redraw_text_decoration_block(struct box *box, int x, int y,
		float scale, colour colour, float ratio);
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
 * \param  x_parent coordinate of parent box
 * \param  y_parent coordinate of parent box
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
		int x_parent, int y_parent,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour current_background_color)
{
	struct box *c;
	int x, y;
	int width, height;
	int padding_left, padding_top, padding_width, padding_height;
	int x0, y0, x1, y1;
	int x_scrolled, y_scrolled;

	/* avoid trivial FP maths */
	if (scale == 1.0) {
		x = x_parent + box->x;
		y = y_parent + box->y;
		width = box->width;
		height = box->height;
		padding_left = box->padding[LEFT];
		padding_top = box->padding[TOP];
		padding_width = padding_left + box->width + box->padding[RIGHT];
		padding_height = padding_top + box->height +
				box->padding[BOTTOM];
	} else {
		x = (x_parent + box->x) * scale;
		y = (y_parent + box->y) * scale;
		width = box->width * scale;
		height = box->height * scale;
		/* left and top padding values are normally zero,
		 * so avoid trivial FP maths */
		padding_left = box->padding[LEFT] ? box->padding[LEFT] * scale
				: 0;
		padding_top = box->padding[TOP] ? box->padding[TOP] * scale
				: 0;
		padding_width = (box->padding[LEFT] + box->width +
				box->padding[RIGHT]) * scale;
		padding_height = (box->padding[TOP] + box->height +
				box->padding[BOTTOM]) * scale;
	}

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
			if (!html_redraw_box(c,
					x_parent + box->x - box->scroll_x,
					y_parent + box->y - box->scroll_y,
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
	if (box->style && box->type != BOX_TEXT && (box->border[TOP] ||
			box->border[RIGHT] || box->border[BOTTOM] ||
			box->border[LEFT]))
		if (!html_redraw_borders(box, x_parent, y_parent,
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
	if ((box->style && box->type != BOX_BR && (box->type != BOX_INLINE ||
			box->style != box->parent->parent->style)) &&
			((box->style->background_color != TRANSPARENT) ||
			(box->background))) {
		/* find intersection of clip box and padding box */
		int px0 = x < x0 ? x0 : x;
		int py0 = y < y0 ? y0 : y;
		int px1 = x + padding_width < x1 ? x + padding_width : x1;
		int py1 = y + padding_height < y1 ? y + padding_height : y1;

		/* valid clipping rectangles only */
		if ((px0 < px1) && (py0 < py1)) {
			/* plot background */
			if (!html_redraw_background(x, y, box, scale,
					px0, py0, px1, py1,
					&current_background_color))
				return false;
			/* restore previous graphics window */
			if (!plot.clip(x0, y0, x1, y1))
				return false;
		}
	}

	/* text decoration */
	if (box->type != BOX_TEXT && box->style &&
			box->style->text_decoration !=
			CSS_TEXT_DECORATION_NONE)
		if (!html_redraw_text_decoration(box, x_parent, y_parent,
				scale, current_background_color))
			return false;

	if (box->object) {
		x_scrolled = x - box->scroll_x * scale;
		y_scrolled = y - box->scroll_y * scale;
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
		if (!html_redraw_text_box(box, x, y, x0, y0, x1, y1,
				scale, current_background_color))
			return false;

	} else {
		for (c = box->children; c != 0; c = c->next)
			if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
				if (!html_redraw_box(c,
						x_parent + box->x - box->scroll_x,
						y_parent + box->y - box->scroll_y,
						x0, y0, x1, y1, scale,
						current_background_color))
					return false;

		for (c = box->float_children; c != 0; c = c->next_float)
			if (!html_redraw_box(c,
					x_parent + box->x - box->scroll_x,
					y_parent + box->y - box->scroll_y,
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
 * Redraw the text content of a box, possibly partially highlighted
 * because the text has been selected, or matches a search operation.
 *
 * \param  box      box with text content
 * \param  x        x co-ord of box
 * \param  y        y co-ord of box
 * \param  x0       current clip rectangle
 * \param  y0
 * \param  x1
 * \param  y1
 * \param  scale    current scale setting (1.0 = 100%)
 * \param  current_background_color
 * \return true iff successful and redraw should proceed
 */

bool html_redraw_text_box(struct box *box, int x, int y,
		int x0, int y0, int x1, int y1,
		float scale, colour current_background_color)
{
	bool highlighted = false;

	/* is this box part of a selection? */
	if (!box->object && current_redraw_browser) {
		unsigned start_idx;
		unsigned end_idx;

		/* first try the browser window's current selection */
		if (selection_defined(current_redraw_browser->sel) &&
			selection_highlighted(current_redraw_browser->sel,
				box, &start_idx, &end_idx)) {
			highlighted = true;
		}

		/* what about the current search operation, if any */
		if (!highlighted &&
			search_current_window == current_redraw_browser->window &&
			gui_search_term_highlighted(current_redraw_browser->window,
				box, &start_idx, &end_idx)) {
				highlighted = true;
		}

		/* \todo make search terms visible within selected text */
		if (highlighted) {
			unsigned endtxt_idx = end_idx;
			colour hfore_col, hback_col;
			bool clip_changed = false;
			bool text_visible = true;
			int startx, endx;

			if (end_idx > box->length) {
				/* adjust for trailing space, not present in box->text */
				assert(end_idx == box->length + 1);
				endtxt_idx = box->length;
			}

			if (!nsfont_width(box->style, box->text, start_idx, &startx))
				startx = 0;

			if (!nsfont_width(box->style, box->text, endtxt_idx, &endx))
				endx = 0;

			/* is there a trailing space that should be highlighted as well? */
			if (end_idx > box->length) {
				int spc_width;
				/* \todo is there a more elegant/efficient solution? */
				if (nsfont_width(box->style, " ", 1, &spc_width))
					endx += spc_width;
			}

			if (scale != 1.0) {
				startx *= scale;
				endx *= scale;
			}

			/* draw any text preceding highlighted portion */
			if (start_idx > 0 && 
				!plot.text(x, y + (int) (box->height * 0.75 * scale),
						box->style, box->text, start_idx,
						current_background_color,
						/*print_text_black ? 0 :*/ box->style->color))
				return false;

			/* decide whether highlighted portion is to be white-on-black or
			   black-on-white */
			if ((current_background_color & 0x808080) == 0x808080)
				hback_col = 0;
			else
				hback_col = 0xffffff;
			hfore_col = hback_col ^ 0xffffff;

			/* highlighted portion */
			if (!plot.fill(x + startx, y, x + endx, y + box->height * scale,
					hback_col))
				return false;

			if (start_idx > 0) {
				int px0 = max(x + startx, x0);
				int px1 = min(x + endx, x1);

				if (px0 < px1) {
					if (!plot.clip(px0, y0, px1, y1))
						return false;
					clip_changed = true;
				} else
					text_visible = false;
			}

			if (text_visible &&
				!plot.text(x, y + (int) (box->height * 0.75 * scale),
						box->style, box->text, endtxt_idx,
						hback_col, hfore_col))
				return false;

			/* draw any text succeeding highlighted portion */
			if (endtxt_idx < box->length) {
				int px0 = max(x + endx, x0);
				if (px0 < x1) {

					if (!plot.clip(px0, y0, x1, y1))
						return false;

					clip_changed = true;

					if (!plot.text(x, y + (int) (box->height * 0.75 * scale),
						box->style, box->text, box->length,
						current_background_color,
						/*print_text_black ? 0 :*/ box->style->color))
						return false;
				}
			}

			if (clip_changed && !plot.clip(x0, y0, x1, y1))
				return false;
		}
	}

	if (!highlighted) {
		if (!plot.text(x, y + (int) (box->height * 0.75 * scale),
				box->style, box->text, box->length,
				current_background_color,
				/*print_text_black ? 0 :*/ box->style->color))
			return false;
	}

	return true;
}


/**
 * Draw borders for a box.
 *
 * \param  box		   box to draw
 * \param  x_parent	   coordinate of left padding edge of parent of box
 * \param  y_parent	   coordinate of top padding edge of parent of box
 * \param  padding_width   width of padding box
 * \param  padding_height  height of padding box
 * \param  scale	   scale for redraw
 * \return true if successful, false otherwise
 */

bool html_redraw_borders(struct box *box, int x_parent, int y_parent,
		int padding_width, int padding_height, float scale)
{
	int top = box->border[TOP] * scale;
	int right = box->border[RIGHT] * scale;
	int bottom = box->border[BOTTOM] * scale;
	int left = box->border[LEFT] * scale;

	assert(box->style);

	if (box->type == BOX_INLINE && !box->object && !box->gadget &&
			!box->text) {
		int padding_height = (box->padding[TOP] + box->height +
				box->padding[BOTTOM]) * scale;
		for (struct box *c = box; c; c = c->next) {
			int x = (x_parent + c->x) * scale;
			int y = y_parent + c->y;
			int padding_width = c->width;
			if (c != box)
				y -= box->padding[TOP];
			if (c == box)
				padding_width += box->padding[LEFT];
			if (!box->inline_end || c == box->inline_end)
				padding_width += box->padding[RIGHT];
			if (scale != 1) {
				y *= scale;
				padding_width *= scale;
			}
			int p[20] = {
				x, y,
				x - left, y - top,
				x + padding_width + right, y - top,
				x + padding_width, y,
				x + padding_width, y + padding_height,
				x + padding_width + right,
				y + padding_height + bottom,
				x - left, y + padding_height + bottom,
				x, y + padding_height,
				x, y,
				x - left, y - top
			};
			if (box->border[LEFT] && c == box)
				html_redraw_border_plot(LEFT, p,
						box->style->border[LEFT].color,
						box->style->border[LEFT].style,
						box->border[LEFT] * scale);
			if (box->border[TOP])
				html_redraw_border_plot(TOP, p,
						box->style->border[TOP].color,
						box->style->border[TOP].style,
						box->border[TOP] * scale);
			if (box->border[BOTTOM])
				html_redraw_border_plot(BOTTOM, p,
						box->style->border[BOTTOM].
						color,
						box->style->border[BOTTOM].
						style,
						box->border[BOTTOM] * scale);
			if (box->border[RIGHT] && (!box->inline_end ||
					c == box->inline_end))
				html_redraw_border_plot(RIGHT, p,
						box->style->border[RIGHT].color,
						box->style->border[RIGHT].style,
						box->border[RIGHT] * scale);
			if (!box->inline_end || c == box->inline_end)
				break;
		}
	} else {
		int x = (x_parent + box->x) * scale;
		int y = (y_parent + box->y) * scale;
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
		for (unsigned int i = 0; i != 4; i++) {
			if (box->border[i] == 0)
				continue;
			if (!html_redraw_border_plot(i, p,
					box->style->border[i].color,
					box->style->border[i].style,
					box->border[i] * scale))
				return false;
		}
	}

	return true;
}


/**
 * Draw one border.
 *
 * \param  i          index of border (TOP, RIGHT, BOTTOM, LEFT)
 * \param  p          array of precomputed border vertices
 * \param  c          colour for border
 * \param  style      border line style
 * \param  thickness  border thickness
 * \return true if successful, false otherwise
 */

bool html_redraw_border_plot(int i, int *p, colour c,
		css_border_style style, int thickness)
{
	int z[8];
	bool dotted = false;
	unsigned int light = i;
	colour c_lit;

	switch (style) {
	case CSS_BORDER_STYLE_DOTTED:
		dotted = true;
	case CSS_BORDER_STYLE_DASHED:
		if (!plot.line((p[i * 4 + 0] + p[i * 4 + 2]) / 2,
				(p[i * 4 + 1] + p[i * 4 + 3]) / 2,
				(p[i * 4 + 4] + p[i * 4 + 6]) / 2,
				(p[i * 4 + 5] + p[i * 4 + 7]) / 2,
				thickness,
				c, dotted, !dotted))
			return false;
		return true;

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
		return true;

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
		if (!plot.polygon(z, 4, light <= 1 ?
				html_redraw_darker(c) :
				html_redraw_lighter(c)))
			return false;
		z[0] = p[i * 4 + 2];
		z[1] = p[i * 4 + 3];
		z[6] = p[i * 4 + 4];
		z[7] = p[i * 4 + 5];
		if (!plot.polygon(z, 4, light <= 1 ?
				html_redraw_lighter(c) :
				html_redraw_darker(c)))
			return false;
		return true;

	case CSS_BORDER_STYLE_INSET:
		light = (light + 2) % 4;
	case CSS_BORDER_STYLE_OUTSET:
		z[0] = p[i * 4 + 0];
		z[1] = p[i * 4 + 1];
		z[2] = (p[i * 4 + 0] + p[i * 4 + 2]) / 2;
		z[3] = (p[i * 4 + 1] + p[i * 4 + 3]) / 2;
		z[4] = (p[i * 4 + 6] + p[i * 4 + 4]) / 2;
		z[5] = (p[i * 4 + 7] + p[i * 4 + 5]) / 2;
		z[6] = p[i * 4 + 6];
		z[7] = p[i * 4 + 7];
		c_lit = c;
		switch (light) {
		case 3:
			c_lit = html_redraw_lighter(c_lit);
		case 0:
			c_lit = html_redraw_lighter(c_lit);
			break;
		case 1:
			c_lit = html_redraw_darker(c_lit);
		case 2:
			c_lit = html_redraw_darker(c_lit);
		}
		if (!plot.polygon(z, 4,	c_lit))
			return false;
		z[0] = p[i * 4 + 2];
		z[1] = p[i * 4 + 3];
		z[6] = p[i * 4 + 4];
		z[7] = p[i * 4 + 5];
		switch (light) {
		case 0:
			c = html_redraw_lighter(c);
		case 3:
			c = html_redraw_lighter(c);
			break;
		case 2:
			c = html_redraw_darker(c);
		case 1:
			c = html_redraw_darker(c);
		}
		if (!plot.polygon(z, 4, c))
			return false;
		return true;

	default:
		break;
	}

	if (!plot.polygon(p + i * 4, 4, c))
		return false;

	return true;
}


/**
 * Make a colour darker.
 *
 * \param  c  colour
 * \return  a darker shade of c
 */

#define mix_colour(c0, c1) ((((c0 >> 16) + 3 * (c1 >> 16)) >> 2) << 16) | \
		(((((c0 >> 8) & 0xff) + 3 * ((c1 >> 8) & 0xff)) >> 2) << 8) | \
		((((c0 & 0xff) + 3 * (c1 & 0xff)) >> 2) << 0);

colour html_redraw_darker(colour c)
{
	return mix_colour(0x000000, c)
}


/**
 * Make a colour lighter.
 *
 * \param  c  colour
 * \return  a lighter shade of c
 */

colour html_redraw_lighter(colour c)
{
	return mix_colour(0xffffff, c)
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

bool html_redraw_background(int x, int y, struct box *box, float scale,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		colour *background_colour)
{
	bool repeat_x = false;
	bool repeat_y = false;
	bool plot_colour = true;
	bool plot_bitmap;
	bool clip_to_children = false;
	struct box *clip_box = box;
	int px0 = clip_x0, py0 = clip_y0, px1 = clip_x1, py1 = clip_y1;
	int ox = x, oy = y;
	struct box *parent;

	plot_bitmap = (box->background && box->background->bitmap);
	if (plot_bitmap) {
		/* handle background-repeat */
		switch (box->style->background_repeat) {
			case CSS_BACKGROUND_REPEAT_REPEAT:
				repeat_x = repeat_y = true;
				/* optimisation: only plot the colour if bitmap is not opaque */
				plot_colour = !bitmap_get_opaque(box->background->bitmap);
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
						box->padding[RIGHT] -
						box->background->width) * scale *
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
						box->padding[BOTTOM] -
						box->background->height) * scale *
						box->style->background_position.vert.
						value.percent / 100;
				break;
			case CSS_BACKGROUND_POSITION_LENGTH:
				y += (int) (css_len2px(&box->style->background_position.
						vert.value.length, box->style) * scale);
				break;
			default:
				break;
		}
	}

	/* special case for table rows as their background needs to be clipped to
	 * all the cells */
	if (box->type == BOX_TABLE_ROW) {
		for (parent = box->parent; ((parent) && (parent->type != BOX_TABLE));
				parent = parent->parent);
		assert(parent && (parent->style));
		clip_to_children = (parent->style->border_spacing.horz.value > 0) ||
				(parent->style->border_spacing.vert.value > 0);
		if (clip_to_children)
			clip_box = box->children;
	}

	for (; clip_box; clip_box = clip_box->next) {
	  	/* clip to child boxes if needed */
	  	if (clip_to_children) {
	  	  	assert(clip_box->type == BOX_TABLE_CELL);

	  	  	/* update clip_* to the child cell */
	  	  	clip_x0 = ox + (clip_box->x * scale);
	  	  	clip_y0 = oy + (clip_box->y * scale);
	  	  	clip_x1 = clip_x0 + clip_box->padding[LEFT] +
	  	  			clip_box->width + clip_box->padding[RIGHT];
	  	  	clip_y1 = clip_y0 + clip_box->padding[TOP] +
	  	  			clip_box->height + clip_box->padding[BOTTOM];
			if (clip_x0 < px0) clip_x0 = px0;
			if (clip_y0 < py0) clip_y0 = py0;
			if (clip_x1 > px1) clip_x1 = px1;
			if (clip_y1 > py1) clip_y1 = py1;

			/* <td> attributes override <tr> */
			if ((clip_x0 >= clip_x1) || (clip_y0 >= clip_y1) ||
					(clip_box->style->background_color != TRANSPARENT) ||
					(clip_box->background &&
					 clip_box->background->bitmap &&
					 bitmap_get_opaque(clip_box->background->bitmap)))
				continue;
	  	}

		/* plot the background colour */
		if (box->style->background_color != TRANSPARENT) {
			*background_colour = box->style->background_color;
			if (plot_colour)
				if (!plot.fill(clip_x0, clip_y0, clip_x1, clip_y1,
						*background_colour))
				return false;
		}
		/* and plot the image */
		if (plot_bitmap) {
			if (!plot.clip(clip_x0, clip_y0, clip_x1, clip_y1))
					return false;
			if (!plot.bitmap_tile(x, y,
					ceilf(box->background->width * scale),
					ceilf(box->background->height * scale),
					box->background->bitmap,
					*background_colour,
					repeat_x, repeat_y))
				return false;
		}

		/* only <tr> rows being clipped to child boxes loop */
		if (!clip_to_children)
			return true;
	}
	return true;
}


/**
 * Plot text decoration for a box.
 *
 * \param  box       box to plot decorations for
 * \param  x_parent  x coordinate of parent of box
 * \param  y_parent  y coordinate of parent of box
 * \param  scale     scale for redraw
 * \param  background_colour  current background colour
 * \return true if successful, false otherwise
 */

bool html_redraw_text_decoration(struct box *box,
		int x_parent, int y_parent, float scale,
		colour background_colour)
{
	static const css_text_decoration decoration[] = {
		CSS_TEXT_DECORATION_UNDERLINE, CSS_TEXT_DECORATION_OVERLINE,
		CSS_TEXT_DECORATION_LINE_THROUGH };
	static const float line_ratio[] = { 0.9, 0.1, 0.5 };
	int colour;
	unsigned int i;

	/* antialias colour for under/overline */
	colour = html_redraw_aa(background_colour, box->style->color);

	if (box->type == BOX_INLINE) {
		for (i = 0; i != NOF_ELEMENTS(decoration); i++)
			if (box->style->text_decoration & decoration[i])
				if (!html_redraw_text_decoration_inline(box,
						x_parent, y_parent, scale,
						colour, line_ratio[i]))
					return false;
	} else {
		for (i = 0; i != NOF_ELEMENTS(decoration); i++)
			if (box->style->text_decoration & decoration[i])
				if (!html_redraw_text_decoration_block(box,
						x_parent + box->x,
						y_parent + box->y,
						scale,
						colour, line_ratio[i]))
					return false;
	}

	return true;
}


/**
 * Plot text decoration for an inline box.
 *
 * \param  box     box to plot decorations for, of type BOX_INLINE
 * \param  x       x coordinate of parent of box
 * \param  y       y coordinate of parent of box
 * \param  scale   scale for redraw
 * \param  colour  colour for decorations
 * \param  ratio   position of line as a ratio of line height
 * \return true if successful, false otherwise
 */

bool html_redraw_text_decoration_inline(struct box *box, int x, int y,
		float scale, colour colour, float ratio)
{
	for (struct box *c = box->next;
			c && c != box->inline_end;
			c = c->next) {
		if (!plot.line((x + c->x) * scale,
				(y + c->y + c->height * ratio) * scale,
				(x + c->x + c->width) * scale,
				(y + c->y + c->height * ratio) * scale,
				0, colour, false, false))
			return false;
	}
	return true;
}


/**
 * Plot text decoration for an non-inline box.
 *
 * \param  box     box to plot decorations for, of type other than BOX_INLINE
 * \param  x       x coordinate of box
 * \param  y       y coordinate of box
 * \param  scale   scale for redraw
 * \param  colour  colour for decorations
 * \param  ratio   position of line as a ratio of line height
 * \return true if successful, false otherwise
 */

bool html_redraw_text_decoration_block(struct box *box, int x, int y,
		float scale, colour colour, float ratio)
{
	/* draw through text descendants */
	for (struct box *c = box->children; c; c = c->next) {
		if (c->type == BOX_TEXT) {
			if (!plot.line((x + c->x) * scale,
					(y + c->y + c->height * ratio) * scale,
					(x + c->x + c->width) * scale,
					(y + c->y + c->height * ratio) * scale,
					0, colour, false, false))
				return false;
		} else if (c->type == BOX_INLINE_CONTAINER ||
				c->type == BOX_BLOCK) {
			if (!html_redraw_text_decoration_block(c,
					x + c->x, y + c->y,
					scale, colour, ratio))
				return false;
		}
	}
	return true;
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
 * \param  box		   scrolling box
 * \param  padding_width   scaled width of padding box
 * \param  padding_height  scaled height of padding box
 * \param  w		   scaled scrollbar width
 * \param  vscroll	   updated to vertical scrollbar present
 * \param  hscroll	   updated to horizontal scrollbar present
 * \param  well_height	   updated to vertical well height
 * \param  bar_top	   updated to top position of vertical scrollbar
 * \param  bar_height	   updated to height of vertical scrollbar
 * \param  well_width	   updated to horizontal well width
 * \param  bar_left	   updated to left position of horizontal scrollbar
 * \param  bar_width	   updated to width of horizontal scrollbar
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
