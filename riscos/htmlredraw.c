/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "oslib/colourtrans.h"
#include "oslib/draw.h"
#include "oslib/font.h"
#include "netsurf/css/css.h"
#include "netsurf/content/content.h"
#include "netsurf/render/form.h"
#include "netsurf/render/html.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


static void html_redraw_box(struct content *content, struct box * box,
		signed long x, signed long y,
		unsigned long current_background_color,
		signed long gadget_subtract_x, signed long gadget_subtract_y,
		bool *select_on,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		float scale);
static void html_redraw_clip(long clip_x0, long clip_y0,
		long clip_x1, long clip_y1);
static void html_redraw_rectangle(int x0, int y0, int width, int height,
		os_colour colour);
static void html_redraw_border(colour color, int width, css_border_style style,
		int x0, int y0, int x1, int y1);

bool gui_redraw_debug = false;

static os_trfm trfm = { {
		{ 65536, 0 },
		{ 0, 65536 },
		{ 0, 0 } } };


void html_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		float scale)
{
	bool select_on = false;
	unsigned long background_colour = 0xffffff;
	struct box *box;

	assert(c->data.html.layout != NULL);
	box = c->data.html.layout->children;
	assert(box);

	/* clear to background colour */
	if (c->data.html.background_colour != TRANSPARENT) {
		colourtrans_set_gcol(c->data.html.background_colour << 8,
				colourtrans_SET_BG | colourtrans_USE_ECFS,
				os_ACTION_OVERWRITE, 0);
		os_clg();
		background_colour = c->data.html.background_colour;
	}

	trfm.entries[0][0] = trfm.entries[1][1] = 65536 * scale;

	html_redraw_box(c, box, x, y, background_colour, x, y,
			&select_on, clip_x0, clip_y0, clip_x1, clip_y1, scale);
}


/* validation strings can't be const */
static char validation_checkbox_selected[] = "Sopton";
static char validation_checkbox_unselected[] = "Soptoff";

static char empty_text[] = "";

void html_redraw_box(struct content *content, struct box * box,
		signed long x, signed long y,
		unsigned long current_background_color,
		signed long gadget_subtract_x, signed long gadget_subtract_y,
		bool *select_on,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		float scale)
{
	struct box *c;
	int width, height, x0, y0, x1, y1, colour;

	x += box->x * 2 * scale;
	y -= box->y * 2 * scale;
	width = (box->padding[LEFT] + box->width + box->padding[RIGHT]) * 2 * scale;
	height = (box->padding[TOP] + box->height + box->padding[BOTTOM]) * 2 * scale;

	x0 = x;
	y1 = y - 1;
	x1 = x0 + width - 1;
	y0 = y1 - height + 1;

	/* if visibility is hidden render children only */
	if (box->style->visibility == CSS_VISIBILITY_HIDDEN) {
		for (c = box->children; c; c = c->next)
			html_redraw_box(content, c, x, y, current_background_color,
					gadget_subtract_x, gadget_subtract_y, select_on,
					x0, y0, x1, y1, scale);
		return;
	}

	if (gui_redraw_debug) {
		html_redraw_rectangle(x, y, width, height, os_COLOUR_MAGENTA);
		html_redraw_rectangle(x + box->padding[LEFT] * 2, y - box->padding[TOP] * 2,
				box->width * 2 * scale, box->height * 2 * scale,
				os_COLOUR_CYAN);
		html_redraw_rectangle(x - (box->border[LEFT] + box->margin[LEFT]) * 2,
				y + (box->border[TOP] + box->margin[TOP]) * 2,
				width + (box->border[LEFT] + box->margin[LEFT] +
				         box->border[RIGHT] + box->margin[RIGHT]) * 2,
				height + (box->border[TOP] + box->margin[TOP] +
				         box->border[BOTTOM] + box->margin[BOTTOM]) * 2,
				os_COLOUR_YELLOW);
	}

	/* borders */
	if (box->style && box->border[TOP])
		html_redraw_border(box->style->border[TOP].color,
				box->border[TOP] * 2,
				box->style->border[TOP].style,
				x - box->border[LEFT] * 2,
				y + box->border[TOP],
				x + width + box->border[RIGHT] * 2,
				y + box->border[TOP]);
	if (box->style && box->border[RIGHT])
		html_redraw_border(box->style->border[RIGHT].color,
				box->border[RIGHT] * 2,
				box->style->border[RIGHT].style,
				x + width + box->border[RIGHT],
				y + box->border[TOP] * 2,
				x + width + box->border[RIGHT],
				y - height - box->border[BOTTOM] * 2);
	if (box->style && box->border[BOTTOM])
		html_redraw_border(box->style->border[BOTTOM].color,
				box->border[BOTTOM] * 2,
				box->style->border[BOTTOM].style,
				x - box->border[LEFT] * 2,
				y - height - box->border[BOTTOM],
				x + width + box->border[RIGHT] * 2,
				y - height - box->border[BOTTOM]);
	if (box->style && box->border[LEFT])
		html_redraw_border(box->style->border[LEFT].color,
				box->border[LEFT] * 2,
				box->style->border[LEFT].style,
				x - box->border[LEFT],
				y + box->border[TOP] * 2,
				x - box->border[LEFT],
				y - height - box->border[BOTTOM] * 2);

	/* return if the box is completely outside the clip rectangle, except
	 * for table rows which may contain cells spanning into other rows */
	if (box->type != BOX_TABLE_ROW &&
			(clip_y1 < y0 || y1 < clip_y0 ||
			 clip_x1 < x0 || x1 < clip_x0))
		return;

	if (box->type == BOX_BLOCK || box->type == BOX_INLINE_BLOCK ||
			box->type == BOX_TABLE_CELL || box->object) {
		/* find intersection of clip rectangle and box */
		if (x0 < clip_x0) x0 = clip_x0;
		if (y0 < clip_y0) y0 = clip_y0;
		if (clip_x1 < x1) x1 = clip_x1;
		if (clip_y1 < y1) y1 = clip_y1;
		/* clip to it */
		html_redraw_clip(x0, y0, x1, y1);
	} else {
		/* clip box unchanged */
		x0 = clip_x0;
		y0 = clip_y0;
		x1 = clip_x1;
		y1 = clip_y1;
	}

	/* background colour */
	if (box->style != 0 && box->style->background_color != TRANSPARENT) {
		colourtrans_set_gcol(box->style->background_color << 8,
				colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0);
		os_plot(os_MOVE_TO, x, y);
		os_plot(os_PLOT_RECTANGLE | os_PLOT_BY, width, -height);
		current_background_color = box->style->background_color;
	}

	if (box->object) {
		content_redraw(box->object, x, y, (unsigned int)width,
		               (unsigned int)height, x0, y0, x1, y1, scale);

	} else if (box->gadget &&
			(box->gadget->type == GADGET_CHECKBOX ||
			box->gadget->type == GADGET_RADIO)) {
		wimp_icon icon;

		icon.extent.x0 = -gadget_subtract_x + x;
		icon.extent.y0 = -gadget_subtract_y + y - height;
		icon.extent.x1 = -gadget_subtract_x + x + width;
		icon.extent.y1 = -gadget_subtract_y + y;

		switch (box->gadget->type) {
		case GADGET_CHECKBOX:
			icon.flags = wimp_ICON_TEXT | wimp_ICON_SPRITE |
			    wimp_ICON_VCENTRED | wimp_ICON_HCENTRED | wimp_ICON_INDIRECTED;
			icon.data.indirected_text_and_sprite.text = empty_text;
			if (box->gadget->data.checkbox.selected)
				icon.data.indirected_text_and_sprite.validation = validation_checkbox_selected;
			else
				icon.data.indirected_text_and_sprite.validation = validation_checkbox_unselected;
			icon.data.indirected_text_and_sprite.size = 1;
			LOG(("writing GADGET CHECKBOX"));
			wimp_plot_icon(&icon);
			break;

		case GADGET_RADIO:
			icon.flags = wimp_ICON_SPRITE | wimp_ICON_VCENTRED | wimp_ICON_HCENTRED;
			if (box->gadget->data.radio.selected)
				strcpy(icon.data.sprite, "radioon");
			else
				strcpy(icon.data.sprite, "radiooff");
			LOG(("writing GADGET RADIO"));
			wimp_plot_icon(&icon);
			break;
		case GADGET_HIDDEN:
		case GADGET_TEXTBOX:
		case GADGET_TEXTAREA:
		case GADGET_IMAGE:
		case GADGET_PASSWORD:
		case GADGET_SUBMIT:
		case GADGET_RESET:
		case GADGET_FILE:
		        break;
		}

	} else if (box->text && box->font) {

		if (content->data.html.text_selection.selected == 1) {
			struct box_position *start;
			struct box_position *end;

			start = &(content->data.html.text_selection.start);
			end = &(content->data.html.text_selection.end);

			if (start->box == box) {
				fprintf(stderr, "THE START OFFSET IS %d\n", start->pixel_offset * 2);
				if (end->box == box) {
					colourtrans_set_gcol(os_COLOUR_VERY_LIGHT_GREY, colourtrans_USE_ECFS, 0, 0);
					os_plot(os_MOVE_TO,
						x + start->pixel_offset * 2,
						y - height);
					os_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
						x + end->pixel_offset * 2 - 2,
						y - 2);
				} else {
					colourtrans_set_gcol(os_COLOUR_VERY_LIGHT_GREY, colourtrans_USE_ECFS, 0, 0);
					os_plot(os_MOVE_TO,
						x + start->pixel_offset * 2,
						y - height);
					os_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
						x + width - 2,
						y - 2);
					*select_on = true;
				}
			} else if (*select_on) {
				if (end->box != box) {
					colourtrans_set_gcol(os_COLOUR_VERY_LIGHT_GREY, colourtrans_USE_ECFS, 0, 0);
					os_plot(os_MOVE_TO, x,
						y - height);
					os_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
						x + width - 2,
						y - 2);
				} else {
					colourtrans_set_gcol(os_COLOUR_VERY_LIGHT_GREY, colourtrans_USE_ECFS, 0, 0);
					os_plot(os_MOVE_TO, x,
						y - height);
					os_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
						x + end->pixel_offset * 2 - 2,
						y - 2);
					*select_on = false;
				}
			}
		}

		colourtrans_set_font_colours(box->font->handle, current_background_color << 8,
					     box->style->color << 8, 14, 0, 0, 0);
		colour = box->style->color;
		colour = ((((colour >> 16) + (current_background_color >> 16)) / 2) << 16)
				| (((((colour >> 8) & 0xff) +
				     ((current_background_color >> 8) & 0xff)) / 2) << 8)
				| ((((colour & 0xff) +
				     (current_background_color & 0xff)) / 2) << 0);
		colourtrans_set_gcol((unsigned int)colour << 8, colourtrans_USE_ECFS,
				os_ACTION_OVERWRITE, 0);

		if (box->style->text_decoration & CSS_TEXT_DECORATION_UNDERLINE) {
			os_plot(os_MOVE_TO, x, y - (int) (box->height * 1.8 * scale));
			os_plot(os_PLOT_SOLID_EX_END | os_PLOT_BY, box->width * 2 * scale, 0);
		}
		if (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_UNDERLINE && box->parent->parent->type == BOX_BLOCK) {
		        colourtrans_set_gcol((unsigned int)box->parent->parent->style->color << 8, colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0);
		        os_plot(os_MOVE_TO, x, y - (int) (box->height * 1.8 * scale));
			os_plot(os_PLOT_SOLID_EX_END | os_PLOT_BY, box->width * 2 * scale, 0);
			colourtrans_set_gcol((unsigned int)box->style->color << 8, colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0);
		}
		if (box->style->text_decoration & CSS_TEXT_DECORATION_OVERLINE) {
			os_plot(os_MOVE_TO, x, y - (int) (box->height * 0.2 * scale));
			os_plot(os_PLOT_SOLID_EX_END | os_PLOT_BY, box->width * 2 * scale, 0);
		}
		if (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_OVERLINE && box->parent->parent->type == BOX_BLOCK) {
		        colourtrans_set_gcol((unsigned int)box->parent->parent->style->color << 8, colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0);
		        os_plot(os_MOVE_TO, x, y - (int) (box->height * 0.2 * scale));
			os_plot(os_PLOT_SOLID_EX_END | os_PLOT_BY, box->width * 2 * scale, 0);
			colourtrans_set_gcol((unsigned int)box->style->color << 8, colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0);
		}
		if (box->style->text_decoration & CSS_TEXT_DECORATION_LINE_THROUGH) {
			os_plot(os_MOVE_TO, x, y - (int) (box->height * 1.0 * scale));
			os_plot(os_PLOT_SOLID_EX_END | os_PLOT_BY, box->width * 2 * scale, 0);
		}
		if (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_LINE_THROUGH && box->parent->parent->type == BOX_BLOCK) {
		        colourtrans_set_gcol((unsigned int)box->parent->parent->style->color << 8, colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0);
		        os_plot(os_MOVE_TO, x, y - (int) (box->height * 1.0 * scale));
			os_plot(os_PLOT_SOLID_EX_END | os_PLOT_BY, box->width * 2 * scale, 0);
			colourtrans_set_gcol((unsigned int)box->style->color << 8, colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0);
		}

		if (scale == 1)
			font_paint(box->font->handle, box->text,
					font_OS_UNITS | font_GIVEN_FONT |
					font_KERN | font_GIVEN_LENGTH,
					x, y - (int) (box->height * 1.5),
					0, 0, (int) box->length);
		else
			font_paint(box->font->handle, box->text,
					font_OS_UNITS | font_GIVEN_FONT |
					font_KERN | font_GIVEN_LENGTH |
					font_GIVEN_TRFM,
					x, y - (int) (box->height * 1.5 * scale),
					0, &trfm, (int) box->length);


	} else {
		for (c = box->children; c != 0; c = c->next)
			if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
				html_redraw_box(content, c, x,
						y, current_background_color,
						gadget_subtract_x, gadget_subtract_y, select_on,
						x0, y0, x1, y1, scale);

		for (c = box->float_children; c != 0; c = c->next_float)
			html_redraw_box(content, c, x,
					y, current_background_color,
					gadget_subtract_x, gadget_subtract_y, select_on,
					x0, y0, x1, y1, scale);
	}

	if (box->type == BOX_BLOCK || box->type == BOX_INLINE_BLOCK ||
			box->type == BOX_TABLE_CELL || box->object)
		html_redraw_clip(clip_x0, clip_y0, clip_x1, clip_y1);

/*	} else {
		if (content->data.html.text_selection.selected == 1) {
			struct box_position *start;
			struct box_position *end;

			start = &(content->data.html.text_selection.start);
			end = &(content->data.html.text_selection.end);

			if (start->box == box && end->box != box)
				*select_on = true;
			else if (*select_on && end->box == box)
				*select_on = false;
		}
	}*/
}


void html_redraw_clip(long clip_x0, long clip_y0,
		long clip_x1, long clip_y1)
{
	os_set_graphics_window();
	os_writec((char) (clip_x0 & 0xff)); os_writec((char) (clip_x0 >> 8));
	os_writec((char) (clip_y0 & 0xff)); os_writec((char) (clip_y0 >> 8));
	os_writec((char) (clip_x1 & 0xff)); os_writec((char) (clip_x1 >> 8));
	os_writec((char) (clip_y1 & 0xff)); os_writec((char) (clip_y1 >> 8));
}


void html_redraw_rectangle(int x0, int y0, int width, int height,
		os_colour colour)
{
	colourtrans_set_gcol(colour, 0, os_ACTION_OVERWRITE, 0);
	os_plot(os_MOVE_TO, x0, y0);
	os_plot(os_PLOT_DOTTED | os_PLOT_BY, width, 0);
	os_plot(os_PLOT_DOTTED | os_PLOT_BY, 0, -height);
	os_plot(os_PLOT_DOTTED | os_PLOT_BY, -width, 0);
	os_plot(os_PLOT_DOTTED | os_PLOT_BY, 0, height);
}


static int path[] = { draw_MOVE_TO, 0, 0, draw_LINE_TO, 0, 0,
		draw_END_PATH, 0 };
static const draw_line_style line_style = { draw_JOIN_MITRED,
		draw_CAP_BUTT, draw_CAP_BUTT, 0, 0x7fffffff,
		0, 0, 0, 0 };
static const int dash_pattern_dotted[] = { 0, 1, 512 };
static const int dash_pattern_dashed[] = { 0, 1, 2048 };

void html_redraw_border(colour color, int width, css_border_style style,
		int x0, int y0, int x1, int y1)
{
	draw_dash_pattern *dash_pattern = 0;
	os_error *error;

	if (style == CSS_BORDER_STYLE_DOTTED)
		dash_pattern = (draw_dash_pattern *) &dash_pattern_dotted;
	else if (style == CSS_BORDER_STYLE_DASHED)
		dash_pattern = (draw_dash_pattern *) &dash_pattern_dashed;

	path[1] = x0 * 256;
	path[2] = y0 * 256;
	path[4] = x1 * 256;
	path[5] = y1 * 256;
	error = xcolourtrans_set_gcol(color << 8, 0, os_ACTION_OVERWRITE, 0, 0);
	if (error)
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
	error = xdraw_stroke((draw_path *) path, 0, 0, 0, width * 256,
			&line_style, dash_pattern);
	if (error)
		LOG(("xdraw_stroke: 0x%x: %s",
				error->errnum, error->errmess));
}
