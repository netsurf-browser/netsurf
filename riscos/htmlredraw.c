/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Redraw of a CONTENT_HTML (RISC OS implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "oslib/colourtrans.h"
#include "oslib/draw.h"
#include "oslib/font.h"
#include "oslib/os.h"
#include "oslib/wimp.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/form.h"
#include "netsurf/render/html.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/image.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/toolbar.h"
#include "netsurf/riscos/ufont.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


static bool html_redraw_box(struct box *box,
		int x, int y,
		unsigned long current_background_color,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale);
static bool html_redraw_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1);
static bool html_redraw_rectangle(int x0, int y0, int width, int height,
		os_colour colour);
static bool html_redraw_fill(int x0, int y0, int width, int height,
		os_colour colour);
static bool html_redraw_circle(int x0, int y0, int radius,
		os_colour colour);
static bool html_redraw_border(colour color, int width, css_border_style style,
		int x0, int y0, int x1, int y1);
static bool html_redraw_checkbox(int x, int y, int width, int height,
		bool selected);
static bool html_redraw_radio(int x, int y, int width, int height,
		bool selected);
static bool html_redraw_file(int x, int y, int width, int height,
		struct box *box, float scale);
static bool html_redraw_background(int x, int y, int width, int height,
		struct box *box, float scale,
		unsigned long background_colour);

bool gui_redraw_debug = false;
static int ro_gui_redraw_box_depth;

static os_trfm trfm = { {
		{ 65536, 0 },
		{ 0, 65536 },
		{ 0, 0 } } };


/**
 * Draw a CONTENT_HTML to a RISC OS window.
 *
 * \param  c                 content of type CONTENT_HTML
 * \param  x                 coordinate for top-left of redraw
 * \param  y                 coordinate for top-left of redraw
 * \param  width             available width (not used for HTML redraw)
 * \param  height            available height (not used for HTML redraw)
 * \param  clip_x0           clip rectangle
 * \param  clip_y0           clip rectangle
 * \param  clip_x1           clip rectangle
 * \param  clip_y1           clip rectangle
 * \param  scale             scale for redraw
 * \param  background_colour the background colour
 * \return true if successful, false otherwise
 *
 * x, y, clip_[xy][01] are in RISC OS screen absolute OS-units.
 */

bool html_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	struct box *box;
	os_error *error;

	assert(c->data.html.layout != NULL);
	box = c->data.html.layout->children;
	assert(box);

	/* clear to background colour */
	if (c->data.html.background_colour != TRANSPARENT)
		background_colour = c->data.html.background_colour;
	error = xcolourtrans_set_gcol(background_colour << 8,
			colourtrans_SET_BG | colourtrans_USE_ECFS,
			os_ACTION_OVERWRITE, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	error = xos_clg();
	if (error) {
		LOG(("xos_clg: 0x%x: %s", error->errnum, error->errmess));
	}

	trfm.entries[0][0] = trfm.entries[1][1] = 65536 * scale;

	ro_gui_redraw_box_depth = 0;
	return html_redraw_box(box, x, y, background_colour,
			clip_x0, clip_y0, clip_x1, clip_y1, scale);
}


/**
 * Recursively draw a box to a RISC OS window.
 *
 * \param  box      box to draw
 * \param  x        coordinate of parent box
 * \param  y        coordinate of parent box
 * \param  current_background_color  background colour under this box
 * \param  clip_x0  clip rectangle
 * \param  clip_y0  clip rectangle
 * \param  clip_x1  clip rectangle
 * \param  clip_y1  clip rectangle
 * \param  scale    scale for redraw
 * \return true if successful, false otherwise
 *
 * x, y, clip_[xy][01] are in RISC OS screen absolute OS-units.
 */

bool html_redraw_box(struct box *box,
		int x, int y,
		unsigned long current_background_color,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale)
{
	struct box *c;
	int width, height;
	int padding_left, padding_top, padding_width, padding_height;
	int x0, y0, x1, y1;
	int colour;
	os_error *error;

	ro_gui_redraw_box_depth++;

	x += box->x * 2 * scale;
	y -= box->y * 2 * scale;
	width = box->width * 2 * scale;
	height = box->height * 2 * scale;
	padding_left = box->padding[LEFT] * 2 * scale;
	padding_top = box->padding[TOP] * 2 * scale;
	padding_width = (box->padding[LEFT] + box->width +
			box->padding[RIGHT]) * 2 * scale;
	padding_height = (box->padding[TOP] + box->height +
			box->padding[BOTTOM]) * 2 * scale;

	/* calculate clip rectangle for this box */
	if (box->style && box->style->overflow != CSS_OVERFLOW_VISIBLE) {
		x0 = x;
		y0 = y - padding_height;
		x1 = x + padding_width - 1;
		y1 = y - 1;
        } else {
		x0 = x + box->descendant_x0 * 2 * scale;
		y0 = y - box->descendant_y1 * 2 * scale;
		x1 = x + box->descendant_x1 * 2 * scale + 1;
		y1 = y - box->descendant_y0 * 2 * scale + 1;
        }

	/* if visibility is hidden render children only */
	if (box->style && box->style->visibility == CSS_VISIBILITY_HIDDEN) {
		for (c = box->children; c; c = c->next)
			if (!html_redraw_box(c, x, y,
					current_background_color,
					x0, y0, x1, y1, scale))
				return false;
		return true;
	}

	if (gui_redraw_debug) {
		if (!html_redraw_rectangle(x, y, padding_width,
				padding_height, os_COLOUR_MAGENTA))
			return false;
		if (!html_redraw_rectangle(x + padding_left, y - padding_top,
				width, height, os_COLOUR_CYAN))
			return false;
		if (!html_redraw_rectangle(x - (box->border[LEFT] +
					box->margin[LEFT]) * 2 * scale,
				y + (box->border[TOP] + box->margin[TOP]) *
					2 * scale,
				padding_width + (box->border[LEFT] +
					box->margin[LEFT] + box->border[RIGHT] +
					box->margin[RIGHT]) * 2 * scale,
				padding_height + (box->border[TOP] +
					box->margin[TOP] + box->border[BOTTOM] +
					box->margin[BOTTOM]) * 2 * scale,
				os_COLOUR_YELLOW))
			return false;
	}

	/* borders */
	if (box->style && box->border[TOP])
		if (!html_redraw_border(box->style->border[TOP].color,
				box->border[TOP] * 2 * scale,
				box->style->border[TOP].style,
				x - box->border[LEFT] * 2 * scale,
				y + box->border[TOP] * scale,
				x + padding_width + box->border[RIGHT] *
					2 * scale,
				y + box->border[TOP] * scale))
			return false;
	if (box->style && box->border[RIGHT])
		if (!html_redraw_border(box->style->border[RIGHT].color,
				box->border[RIGHT] * 2 * scale,
				box->style->border[RIGHT].style,
				x + padding_width + box->border[RIGHT] * scale,
				y + box->border[TOP] * 2 * scale,
				x + padding_width + box->border[RIGHT] * scale,
				y - padding_height - box->border[BOTTOM] *
					2 * scale))
			return false;
	if (box->style && box->border[BOTTOM])
		if (!html_redraw_border(box->style->border[BOTTOM].color,
				box->border[BOTTOM] * 2 * scale,
				box->style->border[BOTTOM].style,
				x - box->border[LEFT] * 2 * scale,
				y - padding_height - box->border[BOTTOM] *
					scale,
				x + padding_width + box->border[RIGHT] *
					2 * scale,
				y - padding_height - box->border[BOTTOM] *
					scale))
			return false;
	if (box->style && box->border[LEFT])
		if (!html_redraw_border(box->style->border[LEFT].color,
				box->border[LEFT] * 2 * scale,
				box->style->border[LEFT].style,
				x - box->border[LEFT] * scale,
				y + box->border[TOP] * 2 * scale,
				x - box->border[LEFT] * scale,
				y - padding_height - box->border[BOTTOM] *
					2 * scale))
			return false;

	/* return if the box is completely outside the clip rectangle */
	if ((ro_gui_redraw_box_depth > 2) &&
			(clip_y1 < y0 || y1 < clip_y0 || clip_x1 < x0 || x1 < clip_x0))
		return true;

	if ((ro_gui_redraw_box_depth > 2) &&
		(box->type == BOX_BLOCK || box->type == BOX_INLINE_BLOCK ||
			box->type == BOX_TABLE_CELL || box->object)) {
		/* find intersection of clip rectangle and box */
		if (x0 < clip_x0) x0 = clip_x0;
		if (y0 < clip_y0) y0 = clip_y0;
		if (clip_x1 < x1) x1 = clip_x1;
		if (clip_y1 < y1) y1 = clip_y1;
		/* clip to it */
		if (!html_redraw_clip(x0, y0, x1, y1))
			return false;
	} else {
		/* clip box unchanged */
		x0 = clip_x0;
		y0 = clip_y0;
		x1 = clip_x1;
		y1 = clip_y1;
	}

	/* background colour and image */
	if (box->style && (box->type != BOX_INLINE ||
			box->style != box->parent->parent->style)) {
		/* find intersection of clip box and padding box */
		int px0 = x < x0 ? x0 : x;
		int py0 = y - padding_height < y0 ? y0 : y - padding_height;
		int px1 = x + padding_width < x1 ? x + padding_width : x1;
		int py1 = y < y1 ? y : y1;

		/* background colour */
		if (box->style->background_color != TRANSPARENT) {
			if (ro_gui_redraw_box_depth > 2) {

			/* optimisation removed - transparent images break */
			/* optimisation: skip if fully repeated bg image */
			if (!box->background ||
			    (ro_gui_current_redraw_gui &&
			     !ro_gui_current_redraw_gui->option.background_images)
			/* || box->style->background_repeat !=
					CSS_BACKGROUND_REPEAT_REPEAT*/) {

					error = xcolourtrans_set_gcol(
						box->style->background_color << 8,
						colourtrans_USE_ECFS,
						os_ACTION_OVERWRITE, 0, 0);
					if (error) {
						LOG(("xcolourtrans_set_gcol: 0x%x: %s", error->errnum, error->errmess));
						return false;
					}

					error = xos_plot(os_MOVE_TO, px0, py0);
					if (error) {
						LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
						return false;
					}

					if (px0 < px1 && py0 < py1) {
						error = xos_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
								px1 - 1, py1 - 1);
						if (error) {
							LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
							return false;
						}
					}
				}

			}
			/* set current background color for font painting */
			current_background_color = box->style->background_color;
		}

		if (box->background) {
			/* clip to padding box for everything but the main window */
			if (ro_gui_redraw_box_depth > 2) {
				if (!html_redraw_clip(px0, py0, px1, py1))
					return false;
			} else {
				if (!html_redraw_clip(clip_x0, clip_y0, clip_x1, clip_y1))
					return false;
			}

			/* plot background image */
			if (!html_redraw_background(x, y, width, clip_y1 - clip_y0,
					box, scale,
					current_background_color))
				return false;

			/* restore previous graphics window */
			if (!html_redraw_clip(x0, y0, x1, y1))
				return false;
		}
	}

	if (box->object) {
		if (!content_redraw(box->object, x + padding_left, y - padding_top,
				width, height, x0, y0, x1, y1, scale,
				current_background_color))
			return false;

	} else if (box->gadget && box->gadget->type == GADGET_CHECKBOX) {
		if (!html_redraw_checkbox(x + padding_left, y - padding_top,
				width, height,
				box->gadget->selected))
			return false;

	} else if (box->gadget && box->gadget->type == GADGET_RADIO) {
		if (!html_redraw_radio(x + padding_left, y - padding_top,
				width, height,
				box->gadget->selected))
			return false;

	} else if (box->gadget && box->gadget->type == GADGET_FILE) {
		error = xcolourtrans_set_font_colours(box->font->handle,
				current_background_color << 8,
				box->style->color << 8, 14, 0, 0, 0);
		if (error) {
			LOG(("xcolourtrans_set_font_colours: 0x%x: %s",
					error->errnum, error->errmess));
			return false;
		}
		if (!html_redraw_file(x + padding_left, y - padding_top,
				width, height, box, scale))
			return false;

	} else if (box->text && box->font) {

		error = xcolourtrans_set_font_colours(box->font->handle,
				current_background_color << 8,
				box->style->color << 8, 14, 0, 0, 0);
		if (error) {
			LOG(("xcolourtrans_set_font_colours: 0x%x: %s",
					error->errnum, error->errmess));
			return false;
		}

		/* antialias colour for under/overline */
		colour = box->style->color;
		colour = ((((colour >> 16) + (current_background_color >> 16)) / 2) << 16)
				| (((((colour >> 8) & 0xff) +
				     ((current_background_color >> 8) & 0xff)) / 2) << 8)
				| ((((colour & 0xff) +
				     (current_background_color & 0xff)) / 2) << 0);
		error = xcolourtrans_set_gcol((unsigned int)colour << 8, colourtrans_USE_ECFS,
				os_ACTION_OVERWRITE, 0, 0);
		if (error) {
			LOG(("xcolourtrans_set_gcol: 0x%x: %s",
					error->errnum, error->errmess));
			return false;
		}

		if (box->style->text_decoration & CSS_TEXT_DECORATION_UNDERLINE) {
			error = xos_plot(os_MOVE_TO, x, y - (int) (box->height * 1.8 * scale));
			if (error) {
				LOG(("xos_plot: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
			error = xos_plot(os_PLOT_SOLID_EX_END | os_PLOT_BY, box->width * 2 * scale, 0);
			if (error) {
				LOG(("xos_plot: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
		}
		if (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_UNDERLINE && box->parent->parent->type == BOX_BLOCK) {
			error = xcolourtrans_set_gcol((unsigned int)box->parent->parent->style->color << 8, colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0, 0);
			if (error) {
				LOG(("xcolourtrans_set_gcol: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
			error = xos_plot(os_MOVE_TO, x, y - (int) (box->height * 1.8 * scale));
			if (error) {
				LOG(("xos_plot: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
			error = xos_plot(os_PLOT_SOLID_EX_END | os_PLOT_BY, box->width * 2 * scale, 0);
			if (error) {
				LOG(("xos_plot: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
			error = xcolourtrans_set_gcol((unsigned int)box->style->color << 8, colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0, 0);
			if (error) {
				LOG(("xcolourtrans_set_gcol: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
		}
		if (box->style->text_decoration & CSS_TEXT_DECORATION_OVERLINE) {
			error = xos_plot(os_MOVE_TO, x, y - (int) (box->height * 0.2 * scale));
			if (error) {
				LOG(("xos_plot: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
			error = xos_plot(os_PLOT_SOLID_EX_END | os_PLOT_BY, box->width * 2 * scale, 0);
			if (error) {
				LOG(("xos_plot: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
		}
		if (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_OVERLINE && box->parent->parent->type == BOX_BLOCK) {
			error = xcolourtrans_set_gcol((unsigned int)box->parent->parent->style->color << 8, colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0, 0);
			if (error) {
				LOG(("xcolourtrans_set_gcol: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
			error = xos_plot(os_MOVE_TO, x, y - (int) (box->height * 0.2 * scale));
			if (error) {
				LOG(("xos_plot: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
			error = xos_plot(os_PLOT_SOLID_EX_END | os_PLOT_BY, box->width * 2 * scale, 0);
			if (error) {
				LOG(("xos_plot: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
			error = xcolourtrans_set_gcol((unsigned int)box->style->color << 8, colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0, 0);
			if (error) {
				LOG(("xcolourtrans_set_gcol: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
		}
		if (box->style->text_decoration & CSS_TEXT_DECORATION_LINE_THROUGH) {
			error = xos_plot(os_MOVE_TO, x, y - (int) (box->height * 1.0 * scale));
			if (error) {
				LOG(("xos_plot: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
			error = xos_plot(os_PLOT_SOLID_EX_END | os_PLOT_BY, box->width * 2 * scale, 0);
			if (error) {
				LOG(("xos_plot: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
		}
		if (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_LINE_THROUGH && box->parent->parent->type == BOX_BLOCK) {
			error = xcolourtrans_set_gcol((unsigned int)box->parent->parent->style->color << 8, colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0, 0);
			if (error) {
				LOG(("xcolourtrans_set_gcol: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
			error = xos_plot(os_MOVE_TO, x, y - (int) (box->height * 1.0 * scale));
			if (error) {
				LOG(("xos_plot: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
			error = xos_plot(os_PLOT_SOLID_EX_END | os_PLOT_BY, box->width * 2 * scale, 0);
			if (error) {
				LOG(("xos_plot: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
			error = xcolourtrans_set_gcol((unsigned int)box->style->color << 8, colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0, 0);
			if (error) {
				LOG(("xcolourtrans_set_gcol: 0x%x: %s", error->errnum,
						error->errmess));
				return false;
			}
		}

		if (scale == 1)
			nsfont_paint(box->font, box->text, box->length,
					x, y - (int) (box->height * 1.5),
					NULL);
		else
			nsfont_paint(box->font, box->text, box->length,
					x, y - (int) (box->height * 1.5 * scale),
					&trfm);


	} else {
		for (c = box->children; c != 0; c = c->next)
			if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
				if (!html_redraw_box(c, x,
						y, current_background_color,
						x0, y0, x1, y1, scale))
					return false;

		for (c = box->float_children; c != 0; c = c->next_float)
			if (!html_redraw_box(c, x,
					y, current_background_color,
					x0, y0, x1, y1, scale))
				return false;
	}

	if (box->type == BOX_BLOCK || box->type == BOX_INLINE_BLOCK ||
			box->type == BOX_TABLE_CELL || box->object)
		if (!html_redraw_clip(clip_x0, clip_y0, clip_x1, clip_y1))
			return false;

	return true;
}


/**
 * Set the clip rectangle.
 */

bool html_redraw_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1)
{
	os_error *error;

	error = xos_set_graphics_window();
	if (error) {
		LOG(("xos_set_graphics_window: 0x%x: %s", error->errnum,
				error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_x0 & 0xff));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_x0 >> 8));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_y0 & 0xff));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_y0 >> 8));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_x1 & 0xff));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_x1 >> 8));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_y1 & 0xff));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_y1 >> 8));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	return true;
}


/**
 * Plot a dotted rectangle outline.
 */

bool html_redraw_rectangle(int x0, int y0, int width, int height,
		os_colour colour)
{
	os_error *error;

	error = xcolourtrans_set_gcol(colour, 0, os_ACTION_OVERWRITE, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(os_MOVE_TO, x0, y0);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(os_PLOT_DOTTED | os_PLOT_BY, width, 0);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(os_PLOT_DOTTED | os_PLOT_BY, 0, -height);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(os_PLOT_DOTTED | os_PLOT_BY, -width, 0);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(os_PLOT_DOTTED | os_PLOT_BY, 0, height);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	return true;
}


/**
 * Fill a rectangle of colour.
 */

bool html_redraw_fill(int x0, int y0, int width, int height,
		os_colour colour)
{
	os_error *error;

	error = xcolourtrans_set_gcol(colour, 0, os_ACTION_OVERWRITE, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(os_MOVE_TO, x0, y0 - height);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(os_PLOT_RECTANGLE | os_PLOT_BY, width - 1, height - 1);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	return true;
}


/**
 * Fill a circle of colour.
 */

bool html_redraw_circle(int x0, int y0, int radius,
		os_colour colour)
{
	os_error *error;

	error = xcolourtrans_set_gcol(colour, 0, os_ACTION_OVERWRITE, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(os_MOVE_TO, x0, y0);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(os_PLOT_CIRCLE | os_PLOT_BY, radius, 0);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	return true;
}


static int path[] = { draw_MOVE_TO, 0, 0, draw_LINE_TO, 0, 0,
		draw_END_PATH, 0 };
static const draw_line_style line_style = { draw_JOIN_MITRED,
		draw_CAP_BUTT, draw_CAP_BUTT, 0, 0x7fffffff,
		0, 0, 0, 0 };
static const int dash_pattern_dotted[] = { 0, 1, 512 };
static const int dash_pattern_dashed[] = { 0, 1, 2048 };

/**
 * Draw a border.
 */

bool html_redraw_border(colour color, int width, css_border_style style,
		int x0, int y0, int x1, int y1)
{
	const draw_dash_pattern *dash_pattern;
	os_error *error;

	if (style == CSS_BORDER_STYLE_DOTTED)
		dash_pattern = (const draw_dash_pattern *) &dash_pattern_dotted;
	else if (style == CSS_BORDER_STYLE_DASHED)
		dash_pattern = (const draw_dash_pattern *) &dash_pattern_dashed;
	else
		dash_pattern = NULL;

	path[1] = x0 * 256;
	path[2] = y0 * 256;
	path[4] = x1 * 256;
	path[5] = y1 * 256;
	error = xcolourtrans_set_gcol(color << 8, 0, os_ACTION_OVERWRITE, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	error = xdraw_stroke((draw_path *) path, 0, 0, 0, width * 256,
			&line_style, dash_pattern);
	if (error) {
		LOG(("xdraw_stroke: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	return true;
}


/**
 * Plot a checkbox.
 */

bool html_redraw_checkbox(int x, int y, int width, int height,
		bool selected)
{
	int z = width * 0.15;
	if (z == 0)
		z = 1;
	if (!html_redraw_fill(x, y, width, height, os_COLOUR_BLACK))
		return false;
	if (!html_redraw_fill(x + z, y - z, width - z - z, height - z - z,
			os_COLOUR_WHITE))
		return false;
	if (selected)
		if (!html_redraw_fill(x + z + z, y - z - z,
				width - z - z - z - z, height - z - z - z - z,
				os_COLOUR_RED))
			return false;

	return true;
}


/**
 * Plot a radio icon.
 */

bool html_redraw_radio(int x, int y, int width, int height,
		bool selected)
{
	if (!html_redraw_circle(x + width * 0.5, y - height * 0.5,
			width * 0.5 - 1, os_COLOUR_BLACK))
		return false;
	if (!html_redraw_circle(x + width * 0.5, y - height * 0.5,
			width * 0.4 - 1, os_COLOUR_WHITE))
		return false;
	if (selected)
		if (!html_redraw_circle(x + width * 0.5, y - height * 0.5,
				width * 0.3 - 1, os_COLOUR_RED))
			return false;

	return true;
}


/**
 * Plot a file upload input.
 */

bool html_redraw_file(int x, int y, int width, int height,
		struct box *box, float scale)
{
	int text_width;
	const char *text;
	const char *sprite;
	size_t length;

	if (box->gadget->value) {
		text = box->gadget->value;
		sprite = "file_fff";
	} else {
		text = messages_get("Form_Drop");
		sprite = "drophere";
	}
	length = strlen(text);

	text_width = nsfont_width(box->font, text, length) * 2 * scale;
	if (width < text_width + 8)
		x = x + width - text_width - 4;
	else
		x = x + 4;

	nsfont_paint(box->font, text, length, x, y - height * 0.75, &trfm);

/*	xwimpspriteop_put_sprite_user_coords(sprite, x + 4, */
/*			y - height / 2 - 17, os_ACTION_OVERWRITE); */

	return true;
}

bool html_redraw_background(int xi, int yi, int width, int height,
	   struct box *box, float scale, unsigned long background_colour)
{
	int x = 0;
	int y = 0;
	unsigned int image_width, image_height;
	os_coord image_size;
	float multiplier;
	bool fixed = false;
	wimp_window_state state;
	os_error *error;
	bool repeat_x = false;
	bool repeat_y = false;

	if (box->background == 0) return true;

	state.w = 0;

	if (ro_gui_current_redraw_gui) {

		/* exit if background images aren't wanted */
		if (!ro_gui_current_redraw_gui->option.background_images)
			return true;

		/* read state of window we're drawing in */
		state.w = ro_gui_current_redraw_gui->window;
		error = xwimp_get_window_state(&state);
		if (error) {
			/* not fatal: fixed backgrounds will scroll. */
			LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
			/* invalidate state.w */
			state.w = 0;
		}
	}
	else {
		/* exit if background images aren't wanted */
		if (!option_background_images)
			return true;
	}

	/* Get the image dimensions for our positioning and scaling */
	image_size.x = box->background->width * scale;
	image_size.y = box->background->height * scale;

	/* handle background-attachment */
	switch (box->style->background_attachment) {
		case CSS_BACKGROUND_ATTACHMENT_FIXED:
			/* uncomment this to enable fixed backgrounds */
			/*if (state.w != 0)
				fixed = true;*/
			break;
		case CSS_BACKGROUND_ATTACHMENT_SCROLL:
			break;
		default:
			break;
	}

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

	/* fixed background */
	if (fixed) {
		int toolbar_height = 0;

		/* get toolbar height */
		if (ro_gui_current_redraw_gui &&
				ro_gui_current_redraw_gui->toolbar)
			toolbar_height = ro_gui_current_redraw_gui->
					toolbar->height;

		/* top left of viewport, taking account of toolbar height */
		x = state.visible.x0;
		y = state.visible.y1 - toolbar_height;

		/* handle background-position */
		switch (box->style->background_position.horz.pos) {
			case CSS_BACKGROUND_POSITION_PERCENT:
				multiplier = box->style->background_position.horz.value.percent / 100;
				x += ((state.visible.x1 - state.visible.x0) - (image_size.x * 2)) * multiplier;
				break;
			case CSS_BACKGROUND_POSITION_LENGTH:
				x += (int)(2. * css_len2px(&box->style->background_position.horz.value.length, box->style) * scale);
				break;
			default:
				break;
		}

		switch (box->style->background_position.vert.pos) {
			case CSS_BACKGROUND_POSITION_PERCENT:
				multiplier = box->style->background_position.vert.value.percent / 100;
				y -= ((state.visible.y1 - state.visible.y0 - toolbar_height) - (image_size.y * 2)) * multiplier;
				break;
			case CSS_BACKGROUND_POSITION_LENGTH:
				y -= (int)(2. * css_len2px(&box->style->background_position.vert.value.length, box->style) * scale);
				break;
			default:
				break;
		}
	}
	else {
		/* handle window offset */
		x = xi;
		y = yi;

		/* handle background-position */
		switch (box->style->background_position.horz.pos) {
			case CSS_BACKGROUND_POSITION_PERCENT:
				multiplier = box->style->background_position.horz.value.percent / 100;
				x += 2 * (box->width + box->padding[LEFT] + box->padding[RIGHT] - image_size.x) * multiplier;
				break;
			case CSS_BACKGROUND_POSITION_LENGTH:
				x += (int)(2. * css_len2px(&box->style->background_position.horz.value.length, box->style) * scale);
				break;
			default:
				break;
		}

		switch (box->style->background_position.vert.pos) {
			case CSS_BACKGROUND_POSITION_PERCENT:
				multiplier = box->style->background_position.vert.value.percent / 100;
				y -= 2 * (box->height + box->padding[TOP] + box->padding[BOTTOM] - image_size.y) * multiplier;
				break;
			case CSS_BACKGROUND_POSITION_LENGTH:
				y -= (int)(2. * css_len2px(&box->style->background_position.vert.value.length, box->style) * scale);
				break;
			default:
				break;
		}
	}

//	  LOG(("Body [%ld, %ld], Image: [%ld, %ld], Flags: %x", xi, yi, x, y, tinct_options));

	/* convert our sizes into OS units */
//	ro_convert_pixels_to_os_units(&image_size, (os_mode)-1);
	image_width = image_size.x * 2;
	image_height = image_size.y * 2;

	/* and plot the image */
	switch (box->background->type) {
#ifdef WITH_PNG
		case CONTENT_PNG:
			image_redraw(box->background->data.png.sprite_area,
					x, y, image_width, image_height,
					box->background->width * 2,
					box->background->height * 2,
					background_colour,
					repeat_x, repeat_y,
					IMAGE_PLOT_TINCT_ALPHA);
			break;
#endif
#ifdef WITH_MNG
		case CONTENT_JNG:
		case CONTENT_MNG:
			image_redraw(box->background->data.mng.sprite_area,
					x, y, image_width, image_height,
					box->background->width * 2,
					box->background->height * 2,
					background_colour,
					repeat_x, repeat_y,
					IMAGE_PLOT_TINCT_ALPHA);
			break;
#endif
#ifdef WITH_JPEG
		case CONTENT_JPEG:
			image_redraw(box->background->data.jpeg.sprite_area,
					x, y, image_width, image_height,
					box->background->width * 2,
					box->background->height * 2,
					background_colour,
					repeat_x, repeat_y,
					IMAGE_PLOT_TINCT_OPAQUE);
			break;
#endif
#ifdef WITH_GIF
		case CONTENT_GIF:
			image_redraw(box->background->data.gif.gif->frame_image,
					x, y, image_width, image_height,
					box->background->width * 2,
					box->background->height * 2,
					background_colour,
					repeat_x, repeat_y,
					IMAGE_PLOT_TINCT_ALPHA);
			break;
#endif
	/**\todo Add draw/sprite background support? */
		default:
			break;
	}

	return true;
}
