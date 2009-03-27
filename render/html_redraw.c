/*
 * Copyright 2004-2008 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004-2007 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004-2007 Richard Wilson <info@tinct.net>
 * Copyright 2005-2006 Adrian Lees <adrianl@users.sourceforge.net>
 * Copyright 2006 Rob Kendrick <rjek@netsurf-browser.org>
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
 * Redraw of a CONTENT_HTML (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "utils/config.h"
#include "content/content.h"
#include "css/css.h"
#include "desktop/gui.h"
#include "desktop/plotters.h"
#include "desktop/knockout.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "desktop/options.h"
#include "desktop/print.h"
#include "image/bitmap.h"
#include "render/box.h"
#include "render/font.h"
#include "render/form.h"
#include "render/layout.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"


static bool html_redraw_box(struct box *box,
		int x, int y,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour current_background_color);
static bool html_redraw_box_children(struct box *box,
		int x_parent, int y_parent,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour current_background_color);
static bool html_redraw_text_box(struct box *box, int x, int y,
		int x0, int y0, int x1, int y1,
		float scale, colour current_background_color);
static bool html_redraw_caret(struct caret *caret,
		colour current_background_color, float scale);
static bool html_redraw_borders(struct box *box, int x_parent, int y_parent,
		int p_width, int p_height, float scale);
bool html_redraw_inline_borders(struct box *box, int x0, int y0, int x1, int y1,
		float scale, bool first, bool last);
static bool html_redraw_border_plot(int i, int *p, colour c,
		css_border_style style, int thickness);
static colour html_redraw_darker(colour c);
static colour html_redraw_lighter(colour c);
static colour html_redraw_blend(colour c0, colour c1);
static colour html_redraw_aa(colour c0, colour c1);
static bool html_redraw_checkbox(int x, int y, int width, int height,
		bool selected);
static bool html_redraw_radio(int x, int y, int width, int height,
		bool selected);
static bool html_redraw_file(int x, int y, int width, int height,
		struct box *box, float scale, colour background_colour);
static bool html_redraw_background(int x, int y, struct box *box, float scale,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		colour *background_colour, struct box *background);
static bool html_redraw_inline_background(int x, int y, struct box *box,
		float scale, int clip_x0, int clip_y0,
		int clip_x1, int clip_y1, int px0, int py0, int px1, int py1,
		bool first, bool last, colour *background_colour);
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

/** Overflow scrollbar colours
 *
 * Overflow scrollbar colours can be set by front end code to try to match
 * scrollbar colours used on the desktop.
 *
 * If a front end doesn't set scrollbar colours, these defaults are used.
 */
colour css_scrollbar_fg_colour = 0x00d9d9d9; /* light grey */
colour css_scrollbar_bg_colour = 0x006b6b6b; /* mid grey */
colour css_scrollbar_arrow_colour = 0x00444444; /* dark grey */

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
		float scale, colour background_colour)
{
	struct box *box;
	bool result, want_knockout;

	box = c->data.html.layout;
	assert(box);

	want_knockout = plot.option_knockout;
	if (want_knockout)
		knockout_plot_start(&plot);

	/* clear to background colour */
	result = plot.clip(clip_x0, clip_y0, clip_x1, clip_y1);
	if (c->data.html.background_colour != TRANSPARENT)
		background_colour = c->data.html.background_colour;
	result &= plot.clg(background_colour);

	result &= html_redraw_box(box, x, y,
			clip_x0, clip_y0, clip_x1, clip_y1,
			scale, background_colour);

	if (want_knockout)
		knockout_plot_end();

	return result;

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
 * \param  inline_depth  depth of nested inlines inside an inline container
 * \return true if successful, false otherwise
 *
 * x, y, clip_[xy][01] are in target coordinates.
 */

bool html_redraw_box(struct box *box,
		int x_parent, int y_parent,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour current_background_color)
{
	int x, y;
	int width, height;
	int padding_left, padding_top, padding_width, padding_height;
	int border_left, border_top, border_right, border_bottom;
	int x0, y0, x1, y1;
	int x_scrolled, y_scrolled;
	struct box *bg_box = NULL;

	if (html_redraw_printing && box->printed)
		return true;

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
		border_left = box->border[LEFT];
		border_top = box->border[TOP];
		border_right = box->border[RIGHT];
		border_bottom = box->border[BOTTOM];
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
		border_left = box->border[LEFT] * scale;
		border_top = box->border[TOP] * scale;
		border_right = box->border[RIGHT] * scale;
		border_bottom = box->border[BOTTOM] * scale;
	}

	/* calculate rectangle covering this box and descendants */
	if (box->style && box->style->overflow != CSS_OVERFLOW_VISIBLE) {
		x0 = x - border_left;
		y0 = y - border_top;
		x1 = x + padding_width + border_right;
		y1 = y + padding_height + border_bottom;
	} else {
		x0 = x + box->descendant_x0 * scale;
		y0 = y + box->descendant_y0 * scale;
		x1 = x + box->descendant_x1 * scale + 1;
		y1 = y + box->descendant_y1 * scale + 1;
		if (!box->parent) {
			int margin_left, margin_right;
			int margin_top, margin_bottom;
			if (scale == 1.0) {
				margin_left = box->margin[LEFT];
				margin_top = box->margin[TOP];
				margin_right = box->margin[RIGHT];
				margin_bottom = box->margin[BOTTOM];
			} else {
				margin_left = box->margin[LEFT] * scale;
				margin_top = box->margin[TOP] * scale;
				margin_right = box->margin[RIGHT] * scale;
				margin_bottom = box->margin[BOTTOM] * scale;
			}
			x0 = x - border_left - margin_left < x0 ?
					x - border_left - margin_left : x0;
			y0 = y - border_top - margin_top < y0 ?
					y - border_top - margin_top : y0;
			x1 = x + padding_width + border_right +
					margin_right > x1 ?
					x + padding_width + border_right +
					margin_right : x1;
			y1 = y + padding_height + border_bottom +
					margin_bottom > y1 ?
					y + padding_height + border_bottom +
					margin_bottom : y1;
		}
	}

	/* return if the rectangle is completely outside the clip rectangle */
	if (clip_y1 < y0 || y1 < clip_y0 || clip_x1 < x0 || x1 < clip_x0)
		return true;

	/*if the rectangle is under the page bottom but it can fit in a page,
	don't print it now*/
	if (html_redraw_printing) {
		if (y1 > html_redraw_printing_border) {
			if (y1 - y0 <= html_redraw_printing_border &&
					(box->type == BOX_TEXT ||
					box->type == BOX_TABLE_CELL
					|| box->object || box->gadget)) {
				/*remember the highest of all points from the
				not printed elements*/
				if (y0 < html_redraw_printing_top_cropped)
					html_redraw_printing_top_cropped = y0;
				return true;
			}
		}
		else box->printed = true;/*it won't be printed anymore*/
	}

	/* if visibility is hidden render children only */
	if (box->style && box->style->visibility == CSS_VISIBILITY_HIDDEN) {
	  	if ((plot.group_start) && (!plot.group_start("hidden box")))
			return false;
		if (!html_redraw_box_children(box, x_parent, y_parent,
				x0, y0, x1, y1, scale,
				current_background_color))
			return false;
		return ((!plot.group_end) || (plot.group_end()));
	}

	if ((plot.group_start) && (!plot.group_start("vis box")))
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

	if (box->type == BOX_BLOCK || box->type == BOX_INLINE_BLOCK ||
			box->type == BOX_TABLE_CELL || box->object) {
		/* find intersection of clip rectangle and box */
		if (x0 < clip_x0) x0 = clip_x0;
		if (y0 < clip_y0) y0 = clip_y0;
		if (clip_x1 < x1) x1 = clip_x1;
		if (clip_y1 < y1) y1 = clip_y1;
		/* no point trying to draw 0-width/height boxes */
		if (x0 == x1 || y0 == y1)
			/* not an error */
			return ((!plot.group_end) || (plot.group_end()));
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

	/* background colour and image for block level content and replaced
	 * inlines */

	/* Thanks to backwards compatibility, CSS defines the following:
	 *
	 * + If the box is for the root element and it has a background,
	 *   use that (and then process the body box with no special case)
	 * + If the box is for the root element and it has no background,
	 *   then use the background (if any) from the body element as if
	 *   it were specified on the root. Then, when the box for the body
	 *   element is processed, ignore the background.
	 * + For any other box, just use its own styling.
	 */
	if (!box->parent) {
		/* Root box */
		if (box->style &&
				(box->style->background_color != TRANSPARENT ||
				box->background)) {
			/* With its own background */
			bg_box = box;
		} else if (!box->style ||
				(box->style->background_color == TRANSPARENT &&
				!box->background)) {
			/* Without its own background */
			if (box->children && box->children->style &&
					(box->children->style->
					background_color != TRANSPARENT ||
					box->children->background)) {
				/* But body has one, so use that */
				bg_box = box->children;
			}
		}
	} else if (box->parent && !box->parent->parent) {
		/* Body box */
		if (box->style &&
				(box->style->background_color != TRANSPARENT ||
				box->background)) {
			/* With a background */
			if (box->parent->style &&
				(box->parent->style->background_color !=
					TRANSPARENT ||
					box->parent->background)) {
				/* Root has own background; process normally */
				bg_box = box;
			}
		}
	} else {
		/* Any other box */
		bg_box = box;
	}

	/* bg_box == NULL implies that this box should not have
	* its background rendered. Otherwise filter out linebreaks,
	* optimize away non-differing inlines, only plot background
	* for BOX_TEXT it's in an inline and ensure the bg_box
	* has something worth rendering */
	if (bg_box && bg_box->style &&
			bg_box->type != BOX_BR &&
			bg_box->type != BOX_TEXT &&
			bg_box->type != BOX_INLINE_END &&
			(bg_box->type != BOX_INLINE || bg_box->object) &&
			((bg_box->style->background_color != TRANSPARENT) ||
			(bg_box->background))) {
		/* find intersection of clip box and border edge */
		int px0, py0, px1, py1;
		px0 = x - border_left < x0 ? x0 : x - border_left;
		py0 = y - border_top < y0 ? y0 : y - border_top;
		px1 = x + padding_width + border_right < x1 ?
				x + padding_width + border_right : x1;
		py1 = y + padding_height + border_bottom < y1 ?
				y + padding_height + border_bottom : y1;
		if (!box->parent) {
			/* Root element, special case:
			 * background covers margins too */
			int m_left, m_top, m_right, m_bottom;
			if (scale == 1.0) {
				m_left = box->margin[LEFT];
				m_top = box->margin[TOP];
				m_right = box->margin[RIGHT];
				m_bottom = box->margin[BOTTOM];
			} else {
				m_left = box->margin[LEFT] * scale;
				m_top = box->margin[TOP] * scale;
				m_right = box->margin[RIGHT] * scale;
				m_bottom = box->margin[BOTTOM] * scale;
			}
			px0 = px0 - m_left < x0 ? x0 : px0 - m_left;
			py0 = py0 - m_top < y0 ? y0 : py0 - m_top;
			px1 = px1 + m_right < x1 ? px1 + m_right : x1;
			py1 = py1 + m_bottom < y1 ? py1 + m_bottom : y1;
		}
		/* valid clipping rectangles only */
		if ((px0 < px1) && (py0 < py1)) {
			/* plot background */
			if (!html_redraw_background(x, y, box, scale,
					px0, py0, px1, py1,
					&current_background_color, bg_box))
				return false;
			/* restore previous graphics window */
			if (!plot.clip(x0, y0, x1, y1))
				return false;
		}
	}

	/* borders for block level content and replaced inlines */
	if (box->style && box->type != BOX_TEXT &&
			box->type != BOX_INLINE_END &&
			(box->type != BOX_INLINE || box->object) &&
			(border_top || border_right ||
			 border_bottom || border_left))
		if (!html_redraw_borders(box, x_parent, y_parent,
				padding_width, padding_height,
				scale))
			return false;

	/* backgrounds and borders for non-replaced inlines */
	if (box->style && box->type == BOX_INLINE && box->inline_end &&
			(box->style->background_color != TRANSPARENT ||
			box->background || border_top || border_right ||
			border_bottom || border_left)) {
		/* inline backgrounds and borders span other boxes and may
		 * wrap onto separate lines */
		struct box *ib;
		bool first = true;
		int ib_x;
		int ib_y = y;
		int ib_width;
		int ib_p_left, ib_p_width;
		int ib_b_left, ib_b_right;
		int xmin = x - border_left;
		int xmax = x + padding_width + border_right;
		int ymin = y - border_top;
		int ymax = y + padding_height + border_bottom;
		int px0 = xmin < x0 ? x0 : xmin;
		int px1 = xmax < x1 ? xmax : x1;
		int py0 = ymin < y0 ? y0 : ymin;
		int py1 = ymax < y1 ? ymax : y1;
		for (ib = box; ib; ib = ib->next) {
			/* to get extents of rectangle(s) associated with
			 * inline, cycle though all boxes in inline, skipping
			 * over floats */
			if (ib->type == BOX_FLOAT_LEFT ||
					ib->type == BOX_FLOAT_RIGHT)
				continue;
			if (scale == 1.0) {
				ib_x = x_parent + ib->x;
				ib_y = y_parent + ib->y;
				ib_width = ib->width;
				ib_p_left = ib->padding[LEFT];
				ib_p_width = ib_p_left + ib->width +
						ib->padding[RIGHT];
				ib_b_left = ib->border[LEFT];
				ib_b_right = ib->border[RIGHT];
			} else {
				ib_x = (x_parent + ib->x) * scale;
				ib_y = (y_parent + ib->y) * scale;
				ib_width = ib->width * scale;
				/* left and top padding values are normally
				 * zero, so avoid trivial FP maths */
				ib_p_left = ib->padding[LEFT] ?
						ib->padding[LEFT] * scale : 0;
				ib_p_width = (ib->padding[LEFT] + ib->width +
						ib->padding[RIGHT]) * scale;
				ib_b_left = ib->border[LEFT] * scale;
				ib_b_right = ib->border[RIGHT] * scale;
			}

			if (ib->inline_new_line && ib != box) {
				/* inline element has wrapped, plot background
				 * and borders */
				if (!html_redraw_inline_background(
						x, y, box, scale,
						px0, py0, px1, py1,
						xmin, ymin, xmax, ymax,
						first, false,
						&current_background_color))
					return false;
				/* restore previous graphics window */
				if (!plot.clip(x0, y0, x1, y1))
					return false;
				if (!html_redraw_inline_borders(box,
						xmin, ymin, xmax, ymax,
						scale, first, false))
					return false;
				/* reset coords */
				xmin = ib_x - ib_b_left;
				xmax = ib_x + ib_p_width + ib_b_right;
				ymin = ib_y - border_top - padding_top;
				ymax = ib_y + padding_height - padding_top +
						border_bottom;

				px0 = xmin < x0 ? x0 : xmin;
				px1 = xmax < x1 ? xmax : x1;
				py0 = ymin < y0 ? y0 : ymin;
				py1 = ymax < y1 ? ymax : y1;

				first = false;
			}

			/* increase width for current box */
			xmax = ib_x + ib_p_width + ib_b_right;
			px1 = xmax < x1 ? xmax : x1;

			if (ib == box->inline_end)
				/* reached end of BOX_INLINE span */
				break;
		}
		/* plot background and borders for last rectangle of
		 * the inline */
		if (!html_redraw_inline_background(x, ib_y, box, scale,
				px0, py0, px1, py1, xmin, ymin, xmax, ymax,
				first, true, &current_background_color))
			return false;
		/* restore previous graphics window */
		if (!plot.clip(x0, y0, x1, y1))
			return false;
		if (!html_redraw_inline_borders(box, xmin, ymin, xmax, ymax,
				scale, first, true))
			return false;

	}

	/* clip to the padding edge for boxes with overflow hidden or scroll */
	if (box->style && box->style->overflow != CSS_OVERFLOW_VISIBLE) {
		x0 = x;
		y0 = y;
		x1 = x + padding_width;
		y1 = y + padding_height;
		if (x0 < clip_x0) x0 = clip_x0;
		if (y0 < clip_y0) y0 = clip_y0;
		if (clip_x1 < x1) x1 = clip_x1;
		if (clip_y1 < y1) y1 = clip_y1;
		if (x1 <= x0 || y1 <= y0)
			return ((!plot.group_end) || (plot.group_end()));
		if (box->type == BOX_BLOCK || box->type == BOX_INLINE_BLOCK ||
				box->type == BOX_TABLE_CELL || box->object) {
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
		if (!html_redraw_box_children(box, x_parent, y_parent,
				x0, y0, x1, y1, scale,
				current_background_color))
			return false;
	}

	/* list marker */
	if (box->list_marker)
		if (!html_redraw_box(box->list_marker,
				x_parent + box->x - box->scroll_x,
				y_parent + box->y - box->scroll_y,
				clip_x0, clip_y0, clip_x1, clip_y1,
				scale, current_background_color))
			return false;

	/* scrollbars */
	if (box->style && box->type != BOX_BR && box->type != BOX_TABLE &&
			box->type != BOX_INLINE &&
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

	return ((!plot.group_end) || (plot.group_end()));
}


/**
 * Draw the various children of a box.
 *
 * \param  box	    box to draw children of
 * \param  x_parent coordinate of parent box
 * \param  y_parent coordinate of parent box
 * \param  clip_x0  clip rectangle
 * \param  clip_y0  clip rectangle
 * \param  clip_x1  clip rectangle
 * \param  clip_y1  clip rectangle
 * \param  scale    scale for redraw
 * \param  current_background_color  background colour under this box
 * \return true if successful, false otherwise
 */

bool html_redraw_box_children(struct box *box,
		int x_parent, int y_parent,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour current_background_color)
{
	struct box *c;

	for (c = box->children; c; c = c->next) {

		if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
			if (!html_redraw_box(c,
					x_parent + box->x - box->scroll_x,
					y_parent + box->y - box->scroll_y,
					clip_x0, clip_y0, clip_x1, clip_y1,
					scale, current_background_color))
				return false;
	}
	for (c = box->float_children; c; c = c->next_float)
		if (!html_redraw_box(c,
				x_parent + box->x - box->scroll_x,
				y_parent + box->y - box->scroll_y,
				clip_x0, clip_y0, clip_x1, clip_y1,
				scale, current_background_color))
			return false;

	return true;
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
	bool excluded = (box->object != NULL);
	struct rect clip;

	clip.x0 = x0;
	clip.y0 = y0;
	clip.x1 = x1;
	clip.y1 = y1;

	if (!text_redraw(box->text, box->length, box->byte_offset,
			box->space, box->style, x, y,
			&clip, box->height, scale,
			current_background_color, excluded))
		return false;

	/* does this textbox contain the ghost caret? */
	if (ghost_caret.defined && box == ghost_caret.text_box) {

		if (!html_redraw_caret(&ghost_caret, current_background_color,
				scale))
			return false;
	}
	return true;
}


/**
 * Redraw a short text string, complete with highlighting
 * (for selection/search) and ghost caret
 *
 * \param  utf8_text  pointer to UTF-8 text string
 * \param  utf8_len   length of string, in bytes
 * \param  offset     byte offset within textual representation
 * \param  space      indicates whether string is followed by a space
 * \param  style      text style to use
 * \param  x          x ordinate at which to plot text
 * \param  y          y ordinate at which to plot text
 * \param  clip       pointer to current clip rectangle
 * \param  height     height of text string
 * \param  scale      current display scale (1.0 = 100%)
 * \param  current_background_color
 * \param  excluded   exclude this text string from the selection
 * \return true iff successful and redraw should proceed
 */

bool text_redraw(const char *utf8_text, size_t utf8_len,
		size_t offset, bool space, struct css_style *style,
		int x, int y, struct rect *clip,
		int height,
		float scale, colour current_background_color,
		bool excluded)
{
	bool highlighted = false;

	/* is this box part of a selection? */
	if (!excluded && current_redraw_browser) {
		unsigned len = utf8_len + (space ? 1 : 0);
		unsigned start_idx;
		unsigned end_idx;

		/* first try the browser window's current selection */
		if (selection_defined(current_redraw_browser->sel) &&
			selection_highlighted(current_redraw_browser->sel,
				offset, offset + len, &start_idx, &end_idx)) {
			highlighted = true;
		}

		/* what about the current search operation, if any? */
		if (!highlighted && search_current_window ==
				current_redraw_browser->window &&
				gui_search_term_highlighted(
						current_redraw_browser->window,
						offset, offset + len,
						&start_idx, &end_idx)) {
			highlighted = true;
		}

		/* \todo make search terms visible within selected text */
		if (highlighted) {
			unsigned endtxt_idx = end_idx;
			colour hfore_col, hback_col;
			bool clip_changed = false;
			bool text_visible = true;
			int startx, endx;

			if (end_idx > utf8_len) {
				/* adjust for trailing space, not present in
				 * utf8_text */
				assert(end_idx == utf8_len + 1);
				endtxt_idx = utf8_len;
			}

			if (!nsfont.font_width(style, utf8_text, start_idx,
					&startx))
				startx = 0;

			if (!nsfont.font_width(style, utf8_text, endtxt_idx,
					&endx))
				endx = 0;

			/* is there a trailing space that should be highlighted
			 * as well? */
			if (end_idx > utf8_len) {
				int spc_width;
				/* \todo is there a more elegant/efficient
				 * solution? */
				if (nsfont.font_width(style, " ", 1,
						&spc_width))
					endx += spc_width;
			}

			if (scale != 1.0) {
				startx *= scale;
				endx *= scale;
			}

			/* draw any text preceding highlighted portion */
			if (start_idx > 0 &&
				!plot.text(x, y + (int) (height * 0.75 * scale),
						style, utf8_text, start_idx,
						current_background_color,
						/*print_text_black ? 0 :*/
						style->color))
				return false;

			/* decide whether highlighted portion is to be
			 * white-on-black or black-on-white */
			if ((current_background_color & 0x808080) == 0x808080)
				hback_col = 0;
			else
				hback_col = 0xffffff;
			hfore_col = hback_col ^ 0xffffff;

			/* highlighted portion */
			if (!plot.fill(x + startx, y, x + endx,
					y + height * scale, hback_col))
				return false;

			if (start_idx > 0) {
				int px0 = max(x + startx, clip->x0);
				int px1 = min(x + endx, clip->x1);

				if (px0 < px1) {
					if (!plot.clip(px0, clip->y0, px1,
							clip->y1))
						return false;
					clip_changed = true;
				} else {
					text_visible = false;
				}
			}

			if (text_visible &&
				!plot.text(x, y + (int) (height * 0.75 * scale),
						style, utf8_text, endtxt_idx,
						hback_col, hfore_col))
				return false;

			/* draw any text succeeding highlighted portion */
			if (endtxt_idx < utf8_len) {
				int px0 = max(x + endx, clip->x0);
				if (px0 < clip->x1) {

					if (!plot.clip(px0, clip->y0,
							clip->x1, clip->y1))
						return false;

					clip_changed = true;

					if (!plot.text(x, y + (int)
						(height * 0.75 * scale),
						style, utf8_text, utf8_len,
						current_background_color,
						/*print_text_black ? 0 :*/
						style->color))
						return false;
				}
			}

			if (clip_changed &&
				!plot.clip(clip->x0, clip->y0,
						clip->x1, clip->y1))
				return false;
		}
	}

	if (!highlighted) {
		if (!plot.text(x, y + (int) (height * 0.75 * scale),
				style, utf8_text, utf8_len,
				current_background_color,
				/*print_text_black ? 0 :*/ style->color))
			return false;
	}
	return true;
}


/**
 * Draw text caret.
 *
 * \param  c	  structure describing text caret
 * \param  current_background_color	background colour under the caret
 * \param  scale  current scale setting (1.0 = 100%)
 * \return true iff successful and redraw should proceed
 */

bool html_redraw_caret(struct caret *c, colour current_background_color,
		float scale)
{
	colour caret_color = 0x808080;  /* todo - choose a proper colour */
	int xc = c->x, y = c->y;
	int h = c->height - 1;
	int w = (h + 7) / 8;

	return (plot.line(xc * scale, y * scale,
				xc * scale, (y + h) * scale,
				0, caret_color, false, false) &&
			plot.line((xc - w) * scale, y * scale,
				(xc + w) * scale, y * scale,
				0, caret_color, false, false) &&
			plot.line((xc - w) * scale, (y + h) * scale,
				(xc + w) * scale, (y + h) * scale,
				0, caret_color, false, false));
}


/**
 * Draw borders for a box.
 *
 * \param  box		box to draw
 * \param  x_parent	coordinate of left padding edge of parent of box
 * \param  y_parent	coordinate of top padding edge of parent of box
 * \param  p_width	width of padding box
 * \param  p_height	height of padding box
 * \param  scale	scale for redraw
 * \return true if successful, false otherwise
 */

bool html_redraw_borders(struct box *box, int x_parent, int y_parent,
		int p_width, int p_height, float scale)
{
	int top = box->border[TOP];
	int right = box->border[RIGHT];
	int bottom = box->border[BOTTOM];
	int left = box->border[LEFT];
	int x, y;
	unsigned int i;
	int p[20];

	if (scale != 1.0) {
		top *= scale;
		right *= scale;
		bottom *= scale;
		left *= scale;
	}

	assert(box->style);

	x = (x_parent + box->x) * scale;
	y = (y_parent + box->y) * scale;

	/* calculate border vertices */
	p[0]  = x;			p[1]  = y;
	p[2]  = x - left;		p[3]  = y - top;
	p[4]  = x + p_width + right;	p[5]  = y - top;
	p[6]  = x + p_width;		p[7]  = y;
	p[8]  = x + p_width;		p[9]  = y + p_height;
	p[10] = x + p_width + right;	p[11] = y + p_height + bottom;
	p[12] = x - left;		p[13] = y + p_height + bottom;
	p[14] = x;			p[15] = y + p_height;
	p[16] = x;			p[17] = y;
	p[18] = x - left;		p[19] = y - top;

	for (i = 0; i != 4; i++) {
		if (box->border[i] == 0)
			continue;
		if (!html_redraw_border_plot(i, p,
				box->style->border[i].color,
				box->style->border[i].style,
				box->border[i] * scale))
			return false;
	}

	return true;
}


/**
 * Draw an inline's borders.
 *
 * \param  box	  BOX_INLINE which created the border
 * \param  x0	  coordinate of border edge rectangle
 * \param  y0	  coordinate of border edge rectangle
 * \param  x1	  coordinate of border edge rectangle
 * \param  y1	  coordinate of border edge rectangle
 * \param  scale  scale for redraw
 * \param  first  true if this is the first rectangle associated with the inline
 * \param  last	  true if this is the last rectangle associated with the inline
 * \return true if successful, false otherwise
 */

bool html_redraw_inline_borders(struct box *box, int x0, int y0, int x1, int y1,
		float scale, bool first, bool last)
{
	int top = box->border[TOP];
	int right = box->border[RIGHT];
	int bottom = box->border[BOTTOM];
	int left = box->border[LEFT];
	int p[20];

	if (scale != 1.0) {
		top *= scale;
		right *= scale;
		bottom *= scale;
		left *= scale;
	}

	/* calculate border vertices */
	p[0]  = x0 + left;	p[1]  = y0 + top;
	p[2]  = x0;		p[3]  = y0;
	p[4]  = x1;		p[5]  = y0;
	p[6]  = x1 - right;	p[7]  = y0 + top;
	p[8]  = x1 - right;	p[9]  = y1 - bottom;
	p[10] = x1;		p[11] = y1;
	p[12] = x0;		p[13] = y1;
	p[14] = x0 + left;	p[15] = y1 - bottom;
	p[16] = x0 + left;	p[17] = y0 + top;
	p[18] = x0;		p[19] = y0;

	assert(box->style);

	if (box->border[LEFT] && first)
		if (!html_redraw_border_plot(LEFT, p,
				box->style->border[LEFT].color,
				box->style->border[LEFT].style, left))
			return false;
	if (box->border[TOP])
		if (!html_redraw_border_plot(TOP, p,
				box->style->border[TOP].color,
				box->style->border[TOP].style, top))
			return false;
	if (box->border[BOTTOM])
		if (!html_redraw_border_plot(BOTTOM, p,
				box->style->border[BOTTOM].color,
				box->style->border[BOTTOM].style, bottom))
			return false;
	if (box->border[RIGHT] && last)
		if (!html_redraw_border_plot(RIGHT, p,
				box->style->border[RIGHT].color,
				box->style->border[RIGHT].style, right))
			return false;
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

	if (c == TRANSPARENT)
		return true;

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
		if (!plot.polygon(z, 4, c_lit))
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
 * Blend two colours.
 *
 * \param  c0  colour
 * \param  c1  colour
 * \return  a blended colour
 */

colour html_redraw_blend(colour c0, colour c1)
{
	return mix_colour(c0, c1)
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

#define WIDGET_BASEC 0xd9d9d9
#define WIDGET_BLOBC 0x000000
bool html_redraw_checkbox(int x, int y, int width, int height,
		bool selected)
{
	int dark = html_redraw_darker(html_redraw_darker(WIDGET_BASEC));
	int lite = html_redraw_lighter(html_redraw_lighter(WIDGET_BASEC));

	double z = width * 0.15;
	if (z == 0)
		z = 1;

	if (!(plot.fill(x, y, x + width, y + height, WIDGET_BASEC) &&
		plot.line(x, y, x + width, y, 1, dark, false, false) &&
		plot.line(x, y, x, y + height, 1, dark, false, false) &&
		plot.line(x + width, y, x + width, y + height, 1, lite,
		  false, false) &&
		plot.line(x, y + height, x + width, y + height, 1, lite,
		  false, false)))
		return false;

	if (selected) {
		if (width < 12 || height < 12) {
			/* render a solid box instead of a tick */
			if (!plot.fill(x + z + z, y + z + z,
				x + width - z, y + height - z,
				WIDGET_BLOBC))
				return false;
		} else {
			/* render a tick, as it'll fit comfortably */
			if (!(plot.line(x + width - z,
				y + z,
				x + (z * 3),
				y + height - z,
				2, WIDGET_BLOBC, false, false) &&

				plot.line(x + (z * 3),
				y + height - z,
				x + z + z,
				y + (height / 2),
				2, WIDGET_BLOBC, false, false)))
				return false;
		}
	}
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
	int dark = html_redraw_darker(html_redraw_darker(WIDGET_BASEC));
	int lite = html_redraw_lighter(html_redraw_lighter(WIDGET_BASEC));

	/* plot background of radio button */
	if (!plot.disc(x + width * 0.5, y + height * 0.5,
			width * 0.5 - 1, WIDGET_BASEC, true))
		return false;

	/* plot dark arc */
	if (!plot.arc(x + width * 0.5, y + height * 0.5,
			width * 0.5 - 1, 45, 225, dark))
		return false;

	/* plot light arc */
	if (!plot.arc(x + width * 0.5, y + height * 0.5,
			width * 0.5 - 1, 225, 45, lite))
		return false;

	if (selected) {
		/* plot selection blob */
		if (!plot.disc(x + width * 0.5, y + height * 0.5,
				width * 0.3 - 1, WIDGET_BLOBC, true))
			return false;
	}

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

	if (!nsfont.font_width(box->style, text, length, &text_width))
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
 * \param  background  box containing background details (usually ::box)
 * \return true if successful, false otherwise
 *
 * The reason for the presence of ::background is the backwards compatibility
 * mess that is backgrounds on <body>. The background will be drawn relative
 * to ::box, using the background information contained within ::background.
 */

bool html_redraw_background(int x, int y, struct box *box, float scale,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		colour *background_colour, struct box *background)
{
	bool repeat_x = false;
	bool repeat_y = false;
	bool plot_colour = true;
	bool plot_content;
	bool clip_to_children = false;
	struct box *clip_box = box;
	int px0 = clip_x0, py0 = clip_y0, px1 = clip_x1, py1 = clip_y1;
	int ox = x, oy = y;
	int width, height;
	struct box *parent;

	if (html_redraw_printing && option_remove_backgrounds)
		return true;

	plot_content = (background->background != NULL);

	if (plot_content) {
		if (!box->parent) {
			/* Root element, special case:
			 * background origin calc. is based on margin box */
			x -= box->margin[LEFT] * scale;
			y -= box->margin[TOP] * scale;
			width = box->margin[LEFT] + box->padding[LEFT] +
					box->width + box->padding[RIGHT] +
					box->margin[RIGHT];
			height = box->margin[TOP] + box->padding[TOP] +
					box->height + box->padding[BOTTOM] +
					box->margin[BOTTOM];
		} else {
			width = box->padding[LEFT] + box->width +
					box->padding[RIGHT];
			height = box->padding[TOP] + box->height +
					box->padding[BOTTOM];
		}
		/* handle background-repeat */
		switch (background->style->background_repeat) {
			case CSS_BACKGROUND_REPEAT_REPEAT:
				repeat_x = repeat_y = true;
				/* optimisation: only plot the colour if
				 * bitmap is not opaque */
				if (background->background->bitmap)
					plot_colour = !bitmap_get_opaque(
						background->background->bitmap);
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
		switch (background->style->background_position.horz.pos) {
			case CSS_BACKGROUND_POSITION_PERCENT:
				x += (width - background->background->width) *
					scale *
					background->style->background_position.
						horz.value.percent / 100;
				break;
			case CSS_BACKGROUND_POSITION_LENGTH:
				x += (int) (css_len2px(&background->style->
					background_position.horz.value.length,
					background->style) * scale);
				break;
			default:
				break;
		}

		switch (background->style->background_position.vert.pos) {
			case CSS_BACKGROUND_POSITION_PERCENT:
				y += (height - background->background->height) *
					scale *
					background->style->background_position.
						vert.value.percent / 100;
				break;
			case CSS_BACKGROUND_POSITION_LENGTH:
				y += (int) (css_len2px(&background->style->
					background_position.vert.value.length,
					background->style) * scale);
				break;
			default:
				break;
		}
	}

	/* special case for table rows as their background needs
	 * to be clipped to all the cells */
	if (box->type == BOX_TABLE_ROW) {
		for (parent = box->parent;
			((parent) && (parent->type != BOX_TABLE));
				parent = parent->parent);
		assert(parent && (parent->style));

		clip_to_children =
			(parent->style->border_spacing.horz.value > 0) ||
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
			clip_x1 = clip_x0 + (clip_box->padding[LEFT] +
					clip_box->width +
					clip_box->padding[RIGHT]) * scale;
			clip_y1 = clip_y0 + (clip_box->padding[TOP] +
					clip_box->height +
					clip_box->padding[BOTTOM]) * scale;

			if (clip_x0 < px0) clip_x0 = px0;
			if (clip_y0 < py0) clip_y0 = py0;
			if (clip_x1 > px1) clip_x1 = px1;
			if (clip_y1 > py1) clip_y1 = py1;

			/* <td> attributes override <tr> */
			if ((clip_x0 >= clip_x1) || (clip_y0 >= clip_y1) ||
					(clip_box->style->background_color !=
					TRANSPARENT) ||
					(clip_box->background &&
					 clip_box->background->bitmap &&
					 bitmap_get_opaque(
						 clip_box->background->bitmap)))
				continue;
		}

		/* plot the background colour */
		if (background->style->background_color != TRANSPARENT) {
			*background_colour =
				background->style->background_color;
			if (plot_colour)
				if (!plot.fill(clip_x0, clip_y0,
						clip_x1, clip_y1,
						*background_colour))
					return false;
		}
		/* and plot the image */
		if (plot_content) {
			if (!repeat_x) {
				if (clip_x0 < x)
					clip_x0 = x;
				if (clip_x1 > x +
						background->background->width *
						scale)
					clip_x1 = x + background->background->
							width * scale;
			}
			if (!repeat_y) {
				if (clip_y0 < y)
					clip_y0 = y;
				if (clip_y1 > y +
						background->background->height *
						scale)
					clip_y1 = y + background->background->
							height * scale;
			}
			/* valid clipping rectangles only */
			if ((clip_x0 < clip_x1) && (clip_y0 < clip_y1)) {
				if (!plot.clip(clip_x0, clip_y0,
						clip_x1, clip_y1))
					return false;
				if (!content_redraw_tiled(background->
						background, x, y,
						ceilf(background->background->
						width * scale),
						ceilf(background->background->
						height * scale),
						clip_x0, clip_y0,
						clip_x1, clip_y1,
						scale, *background_colour,
						repeat_x, repeat_y))
					return false;
			}
		}

		/* only <tr> rows being clipped to child boxes loop */
		if (!clip_to_children)
			return true;
	}
	return true;
}


/**
 * Plot an inline's background and/or background image.
 *
 * \param  x	  coordinate of box
 * \param  y	  coordinate of box
 * \param  box	  BOX_INLINE which created the background
 * \param  scale  scale for redraw
 * \param  clip_x0	coordinate of clip rectangle
 * \param  clip_y0	coordinate of clip rectangle
 * \param  clip_x1	coordinate of clip rectangle
 * \param  clip_y1	coordinate of clip rectangle
 * \param  px0	  coordinate of border edge rectangle
 * \param  py0	  coordinate of border edge rectangle
 * \param  px1	  coordinate of border edge rectangle
 * \param  py1	  coordinate of border edge rectangle
 * \param  first  true if this is the first rectangle associated with the inline
 * \param  last   true if this is the last rectangle associated with the inline
 * \param  background_colour  updated to current background colour if plotted
 * \return true if successful, false otherwise
 */

bool html_redraw_inline_background(int x, int y, struct box *box, float scale,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		int px0, int py0, int px1, int py1,
		bool first, bool last, colour *background_colour)
{
	bool repeat_x = false;
	bool repeat_y = false;
	bool plot_colour = true;
	bool plot_content;

	plot_content = (box->background != NULL);

	if (html_redraw_printing && option_remove_backgrounds)
		return true;

	if (plot_content) {
		/* handle background-repeat */
		switch (box->style->background_repeat) {
			case CSS_BACKGROUND_REPEAT_REPEAT:
				repeat_x = repeat_y = true;
				/* optimisation: only plot the colour if
				 * bitmap is not opaque */
				if (box->background->bitmap)
					plot_colour = !bitmap_get_opaque(
						box->background->bitmap);
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
				x += (px1 - px0 -
					box->background->width * scale) *
					box->style->background_position.
						horz.value.percent / 100;

				if (!repeat_x &&
						((box->style->
						background_position.
						horz.value.percent < 2 &&
						!first) ||
						(box->style->
						background_position.
						horz.value.percent > 98 &&
						!last))) {
					plot_content = false;
				}
				break;
			case CSS_BACKGROUND_POSITION_LENGTH:
				x += (int) (css_len2px(&box->style->
					background_position.horz.value.length,
					box->style) * scale);
				break;
			default:
				break;
		}

		switch (box->style->background_position.vert.pos) {
			case CSS_BACKGROUND_POSITION_PERCENT:
				y += (py1 - py0 -
					box->background->height * scale) *
					box->style->background_position.
						vert.value.percent / 100;
				break;
			case CSS_BACKGROUND_POSITION_LENGTH:
				y += (int) (css_len2px(&box->style->
					background_position.vert.value.length,
					box->style) * scale);
				break;
			default:
				break;
		}
	}

	/* plot the background colour */
	if (box->style->background_color != TRANSPARENT) {
		*background_colour =
			box->style->background_color;
		if (plot_colour)
			if (!plot.fill(clip_x0, clip_y0,
					clip_x1, clip_y1,
					*background_colour))
				return false;
	}
	/* and plot the image */
	if (plot_content) {
		if (!repeat_x) {
			if (clip_x0 < x)
				clip_x0 = x;
			if (clip_x1 > x +
					box->background->width *
					scale)
				clip_x1 = x + box->background->
						width * scale;
		}
		if (!repeat_y) {
			if (clip_y0 < y)
				clip_y0 = y;
			if (clip_y1 > y +
					box->background->height *
					scale)
				clip_y1 = y + box->background->
						height * scale;
		}
		/* valid clipping rectangles only */
		if ((clip_x0 < clip_x1) && (clip_y0 < clip_y1)) {
			if (!plot.clip(clip_x0, clip_y0,
					clip_x1, clip_y1))
				return false;
			if (!content_redraw_tiled(box->
					background, x, y,
					ceilf(box->background->
					width * scale),
					ceilf(box->background->
					height * scale),
					clip_x0, clip_y0,
					clip_x1, clip_y1,
					scale, *background_colour,
					repeat_x, repeat_y))
				return false;
		}
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
	if (html_redraw_printing)
		colour = box->style->color;
	else
		colour = html_redraw_aa(background_colour, box->style->color);

	if (box->type == BOX_INLINE) {
		if (!box->inline_end)
			return true;
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
	struct box *c;
	for (c = box->next;
			c && c != box->inline_end;
			c = c->next) {
		if (c->type != BOX_TEXT)
			continue;
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
	struct box *c;
	/* draw through text descendants */
	for (c = box->children; c; c = c->next) {
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
	colour c0, c1; /* highlight and shadow colours */
	int v[6]; /* array of triangle vertices */

	box_scrollbar_dimensions(box, padding_width, padding_height, w,
			&vscroll, &hscroll,
			&well_height, &bar_top, &bar_height,
			&well_width, &bar_left, &bar_width);

#define RECTANGLE(x0, y0, x1, y1, c, inset)				\
	c0 = inset ? html_redraw_darker(c) : html_redraw_lighter(c);	\
	c1 = inset ? html_redraw_lighter(c) : html_redraw_darker(c);	\
	if (!plot.line(x0, y0, x1, y0, 1, c0, false, false))		\
		return false;						\
	if (!plot.line(x1, y0, x1, y1 + 1, 1, c1, false, false))	\
		return false;						\
	if (!plot.line(x1, y0, x1, y0 + 1, 1,				\
			html_redraw_blend(c0, c1), false, false))	\
		return false;						\
	if (!plot.line(x1, y1, x0, y1, 1, c1, false, false))		\
		return false;						\
	if (!plot.line(x0, y1, x0, y0, 1, c0, false, false))		\
		return false;						\
	if (!plot.line(x0, y1, x0, y1 + 1, 1,				\
			html_redraw_blend(c0, c1), false, false))	\
		return false;

	/* horizontal scrollbar */
	if (hscroll) {
		/* scrollbar outline */
		RECTANGLE(x,
				y + padding_height - w,
				x + padding_width - 1,
				y + padding_height - 1,
				css_scrollbar_bg_colour, true);
		/* left arrow icon border */
		RECTANGLE(x + 1,
				y + padding_height - w + 1,
				x + w - 2,
				y + padding_height - 2,
				css_scrollbar_fg_colour, false);
		/* left arrow icon background */
		if (!plot.fill(x + 2,
				y + padding_height - w + 2,
				x + w - 2,
				y + padding_height - 2,
				css_scrollbar_fg_colour))
			return false;
		/* left arrow */
		v[0] = x + w / 4;
		v[1] = y + padding_height - w / 2;
		v[2] = x + w * 3 / 4;
		v[3] = y + padding_height - w * 3 / 4;
		v[4] = x + w * 3 / 4;
		v[5] = y + padding_height - w / 4;
		if (!plot.polygon(v, 3, css_scrollbar_arrow_colour))
			return false;
		/* scroll well background */
		if (!plot.fill(x + w - 1,
				y + padding_height - w + 1,
				x + w + well_width + (vscroll ? 2 : 1),
				y + padding_height - 1,
				css_scrollbar_bg_colour))
			return false;
		/* scroll position indicator bar */
		RECTANGLE(x + w + bar_left,
				y + padding_height - w + 1,
				x + w + bar_left + bar_width + (vscroll? 1 : 0),
				y + padding_height - 2,
				css_scrollbar_fg_colour, false);
		if (!plot.fill(x + w + bar_left + 1,
				y + padding_height - w + 2,
				x + w + bar_left + bar_width + (vscroll? 1 : 0),
				y + padding_height - 2,
				css_scrollbar_fg_colour))
			return false;
		/* right arrow icon border */
		RECTANGLE(x + w + well_width + 2,
				y + padding_height - w + 1,
				x + w + well_width + w - (vscroll ? 1 : 2),
				y + padding_height - 2,
				css_scrollbar_fg_colour, false);
		/* right arrow icon background */
		if (!plot.fill(x + w + well_width + 3,
				y + padding_height - w + 2,
				x + w + well_width + w - (vscroll ? 1 : 2),
				y + padding_height - 2,
				css_scrollbar_fg_colour))
			return false;
		/* right arrow */
		v[0] = x + w + well_width + w * 3 / 4 + (vscroll ? 1 : 0);
		v[1] = y + padding_height - w / 2;
		v[2] = x + w + well_width + w / 4 + (vscroll ? 1 : 0);
		v[3] = y + padding_height - w * 3 / 4;
		v[4] = x + w + well_width + w / 4 + (vscroll ? 1 : 0);
		v[5] = y + padding_height - w / 4;
		if (!plot.polygon(v, 3, css_scrollbar_arrow_colour))
			return false;
	}

	/* vertical scrollbar */
	if (vscroll) {
		/* outline */
		RECTANGLE(x + padding_width - w,
				y,
				x + padding_width - 1,
				y + padding_height - 1,
				css_scrollbar_bg_colour, true);
		/* top arrow background */
		RECTANGLE(x + padding_width - w + 1,
				y + 1,
				x + padding_width - 2,
				y + w - 2,
				css_scrollbar_fg_colour, false);
		if (!plot.fill(x + padding_width - w + 2,
				y + 2,
				x + padding_width - 2,
				y + w - 2,
				css_scrollbar_fg_colour))
			return false;
		/* up arrow */
		v[0] = x + padding_width - w / 2;
		v[1] = y + w / 4;
		v[2] = x + padding_width - w * 3 / 4;
		v[3] = y + w * 3 / 4;
		v[4] = x + padding_width - w / 4;
		v[5] = y + w * 3 / 4;
		if (!plot.polygon(v, 3, css_scrollbar_arrow_colour))
			return false;
		/* scroll well background */
		if (!plot.fill(x + padding_width - w + 1,
				y + w - 1,
				x + padding_width - 1,
				y + padding_height - w + 1,
				css_scrollbar_bg_colour))
			return false;
		/* scroll position indicator bar */
		RECTANGLE(x + padding_width - w + 1,
				y + w + bar_top,
				x + padding_width - 2,
				y + w + bar_top + bar_height,
				css_scrollbar_fg_colour, false);
		if (!plot.fill(x + padding_width - w + 2,
				y + w + bar_top + 1,
				x + padding_width - 2,
				y + w + bar_top + bar_height,
				css_scrollbar_fg_colour))
			return false;
		/* bottom arrow background */
		RECTANGLE(x + padding_width - w + 1,
				y + padding_height - w + 1,
				x + padding_width - 2,
				y + padding_height - 2,
				css_scrollbar_fg_colour, false);
		if (!plot.fill(x + padding_width - w + 2,
				y + padding_height - w + 2,
				x + padding_width - 2,
				y + padding_height - 2,
				css_scrollbar_fg_colour))
			return false;
		/* down arrow */
		v[0] = x + padding_width - w / 2;
		v[1] = y + w + well_height + w * 3 / 4;
		v[2] = x + padding_width - w * 3 / 4;
		v[3] = y + w + well_height + w / 4;
		v[4] = x + padding_width - w / 4;
		v[5] = y + w + well_height + w / 4;
		if (!plot.polygon(v, 3, css_scrollbar_arrow_colour))
			return false;
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
