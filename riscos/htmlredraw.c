/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "swis.h"
#include "oslib/colourtrans.h"
#include "oslib/draw.h"
#include "oslib/font.h"
#include "oslib/os.h"
#include "oslib/wimp.h"
#include "netsurf/utils/config.h"
#include "netsurf/css/css.h"
#include "netsurf/content/content.h"
#include "netsurf/render/form.h"
#include "netsurf/render/html.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/ufont.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/riscos/toolbar.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


static void html_redraw_box(struct content *content, struct box * box,
		signed long x, signed long y,
		unsigned long current_background_color,
		bool *select_on,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		float scale);
static void html_redraw_clip(long clip_x0, long clip_y0,
		long clip_x1, long clip_y1);
static void html_redraw_rectangle(int x0, int y0, int width, int height,
		os_colour colour);
static void html_redraw_fill(int x0, int y0, int width, int height,
		os_colour colour);
static void html_redraw_circle(int x0, int y0, int radius,
		os_colour colour);
static void html_redraw_border(colour color, int width, css_border_style style,
		int x0, int y0, int x1, int y1);
static void html_redraw_checkbox(int x, int y, int width, int height,
		bool selected);
static void html_redraw_radio(int x, int y, int width, int height,
		bool selected);
static void html_redraw_file(int x, int y, int width, int height,
		struct box *box, float scale);
static void html_redraw_background(long x, long y, int width, int height,
		struct box *box, float scale);

bool gui_redraw_debug = false;

static os_trfm trfm = { {
		{ 65536, 0 },
		{ 0, 65536 },
		{ 0, 0 } } };


void html_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale)
{
	bool select_on = false;
	unsigned long background_colour = 0xffffff;
	struct box *box;

	assert(c->data.html.layout != NULL);
	box = c->data.html.layout->children;
	assert(box);

	/* clear to background colour */
	if (c->data.html.background_colour != TRANSPARENT)
		background_colour = c->data.html.background_colour;
	colourtrans_set_gcol(background_colour << 8,
			colourtrans_SET_BG | colourtrans_USE_ECFS,
			os_ACTION_OVERWRITE, 0);
	os_clg();

	trfm.entries[0][0] = trfm.entries[1][1] = 65536 * scale;

	html_redraw_box(c, box, x, y, background_colour,
			&select_on, clip_x0, clip_y0, clip_x1, clip_y1, scale);
}



void html_redraw_box(struct content *content, struct box * box,
		signed long x, signed long y,
		unsigned long current_background_color,
		bool *select_on,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1,
		float scale)
{
	struct box *c;
	int width, height;
	int padding_left, padding_top;
	int padding_width, padding_height;
	int x0, y0, x1, y1;
	int colour;

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

	x0 = x;
	y1 = y - 1;
	x1 = x0 + padding_width - 1;
	y0 = y1 - padding_height + 1;

	/* if visibility is hidden render children only */
	if (box->style && box->style->visibility == CSS_VISIBILITY_HIDDEN) {
		for (c = box->children; c; c = c->next)
			html_redraw_box(content, c, x, y, current_background_color,
					select_on,
					x0, y0, x1, y1, scale);
		return;
	}

	if (gui_redraw_debug) {
		html_redraw_rectangle(x, y, padding_width, padding_height,
				os_COLOUR_MAGENTA);
		html_redraw_rectangle(x + padding_left, y - padding_top,
				width, height, os_COLOUR_CYAN);
		html_redraw_rectangle(x - (box->border[LEFT] +
					box->margin[LEFT]) * 2 * scale,
				y + (box->border[TOP] + box->margin[TOP]) *
					2 * scale,
				padding_width + (box->border[LEFT] +
					box->margin[LEFT] + box->border[RIGHT] +
					box->margin[RIGHT]) * 2 * scale,
				padding_height + (box->border[TOP] +
					box->margin[TOP] + box->border[BOTTOM] +
					box->margin[BOTTOM]) * 2 * scale,
				os_COLOUR_YELLOW);
	}

	/* borders */
	if (box->style && box->border[TOP])
		html_redraw_border(box->style->border[TOP].color,
				box->border[TOP] * 2 * scale,
				box->style->border[TOP].style,
				x - box->border[LEFT] * 2 * scale,
				y + box->border[TOP] * scale,
				x + padding_width + box->border[RIGHT] *
					2 * scale,
				y + box->border[TOP] * scale);
	if (box->style && box->border[RIGHT])
		html_redraw_border(box->style->border[RIGHT].color,
				box->border[RIGHT] * 2 * scale,
				box->style->border[RIGHT].style,
				x + padding_width + box->border[RIGHT] * scale,
				y + box->border[TOP] * 2 * scale,
				x + padding_width + box->border[RIGHT] * scale,
				y - padding_height - box->border[BOTTOM] *
					2 * scale);
	if (box->style && box->border[BOTTOM])
		html_redraw_border(box->style->border[BOTTOM].color,
				box->border[BOTTOM] * 2 * scale,
				box->style->border[BOTTOM].style,
				x - box->border[LEFT] * 2 * scale,
				y - padding_height - box->border[BOTTOM] *
					scale,
				x + padding_width + box->border[RIGHT] *
					2 * scale,
				y - padding_height - box->border[BOTTOM] *
					scale);
	if (box->style && box->border[LEFT])
		html_redraw_border(box->style->border[LEFT].color,
				box->border[LEFT] * 2 * scale,
				box->style->border[LEFT].style,
				x - box->border[LEFT] * scale,
				y + box->border[TOP] * 2 * scale,
				x - box->border[LEFT] * scale,
				y - padding_height - box->border[BOTTOM] *
					2 * scale);

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
			/* optimisation: skip if fully repeated bg image */
			if (!box->background ||
				box->style->background_repeat !=
				CSS_BACKGROUND_REPEAT_REPEAT) {

				colourtrans_set_gcol(
					box->style->background_color << 8,
					colourtrans_USE_ECFS,
					os_ACTION_OVERWRITE, 0);

				os_plot(os_MOVE_TO, px0, py0);

				if (px0 < px1 && py0 < py1)
					os_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
						px1, py1);
			}
			/* set current background color for font painting */
			current_background_color = box->style->background_color;
		}

		if (box->background) {
			/* clip to padding box */
			html_redraw_clip(px0, py0, px1, py1);

			/* plot background image */
			html_redraw_background(x, y, width, clip_y1 - clip_y0,
					box, scale);

			/* restore previous graphics window */
			html_redraw_clip(x0, y0, x1, y1);
		}
	}

	if (box->object) {
		content_redraw(box->object, x + padding_left, y - padding_top,
				width, height, x0, y0, x1, y1, scale);

	} else if (box->gadget && box->gadget->type == GADGET_CHECKBOX) {
		html_redraw_checkbox(x + padding_left, y - padding_top,
				width, height,
				box->gadget->selected);

	} else if (box->gadget && box->gadget->type == GADGET_RADIO) {
		html_redraw_radio(x + padding_left, y - padding_top,
				width, height,
				box->gadget->selected);

	} else if (box->gadget && box->gadget->type == GADGET_FILE) {
		colourtrans_set_font_colours(box->font->handle,
				current_background_color << 8,
				box->style->color << 8, 14, 0, 0, 0);
		html_redraw_file(x + padding_left, y - padding_top,
				width, height, box, scale);

	} else if (box->text && box->font) {

		if (content->data.html.text_selection.selected == 1) {
			struct box_position *start;
			struct box_position *end;

			start = &(content->data.html.text_selection.start);
			end = &(content->data.html.text_selection.end);

			if (start->box == box) {
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

		colourtrans_set_font_colours(box->font->handle,
				current_background_color << 8,
				box->style->color << 8, 14, 0, 0, 0);

		/* antialias colour for under/overline */
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
			nsfont_paint(box->font, box->text,
					x, y - (int) (box->height * 1.5),
					NULL, (int) box->length);
		else
			nsfont_paint(box->font, box->text,
					x, y - (int) (box->height * 1.5 * scale),
					&trfm, (int) box->length);


	} else {
		for (c = box->children; c != 0; c = c->next)
			if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
				html_redraw_box(content, c, x,
						y, current_background_color,
						select_on,
						x0, y0, x1, y1, scale);

		for (c = box->float_children; c != 0; c = c->next_float)
			html_redraw_box(content, c, x,
					y, current_background_color,
					select_on,
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


/**
 * Plot a dotted rectangle outline.
 */

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


/**
 * Fill a rectangle of colour.
 */

void html_redraw_fill(int x0, int y0, int width, int height,
		os_colour colour)
{
	colourtrans_set_gcol(colour, 0, os_ACTION_OVERWRITE, 0);
	os_plot(os_MOVE_TO, x0, y0 - height);
	os_plot(os_PLOT_RECTANGLE | os_PLOT_BY, width - 1, height - 1);
}


/**
 * Fill a circle of colour.
 */

void html_redraw_circle(int x0, int y0, int radius,
		os_colour colour)
{
	colourtrans_set_gcol(colour, 0, os_ACTION_OVERWRITE, 0);
	os_plot(os_MOVE_TO, x0, y0);
	os_plot(os_PLOT_CIRCLE | os_PLOT_BY, radius, 0);
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
	if (error)
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
	error = xdraw_stroke((draw_path *) path, 0, 0, 0, width * 256,
			&line_style, dash_pattern);
	if (error)
		LOG(("xdraw_stroke: 0x%x: %s",
				error->errnum, error->errmess));
}


/**
 * Plot a checkbox.
 */

void html_redraw_checkbox(int x, int y, int width, int height,
		bool selected)
{
	int z = width * 0.15;
	if (z == 0)
		z = 1;
	html_redraw_fill(x, y, width, height, os_COLOUR_BLACK);
	html_redraw_fill(x + z, y - z, width - z - z, height - z - z,
			os_COLOUR_WHITE);
	if (selected)
		html_redraw_fill(x + z + z, y - z - z,
				width - z - z - z - z, height - z - z - z - z,
				os_COLOUR_RED);
}


/**
 * Plot a radio icon.
 */

void html_redraw_radio(int x, int y, int width, int height,
		bool selected)
{
	html_redraw_circle(x + width * 0.5, y - height * 0.5,
			width * 0.5 - 1, os_COLOUR_BLACK);
	html_redraw_circle(x + width * 0.5, y - height * 0.5,
			width * 0.4 - 1, os_COLOUR_WHITE);
	if (selected)
		html_redraw_circle(x + width * 0.5, y - height * 0.5,
				width * 0.3 - 1, os_COLOUR_RED);
}


/**
 * Plot a file upload input.
 */

void html_redraw_file(int x, int y, int width, int height,
		struct box *box, float scale)
{
	int text_width;
	const char *text;
	const char *sprite;
	int length;

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

	nsfont_paint(box->font, text,
			x, y - height * 0.75, &trfm, length);

/*	xwimpspriteop_put_sprite_user_coords(sprite, x + 4, */
/*			y - height / 2 - 17, os_ACTION_OVERWRITE); */
}

void html_redraw_background(long xi, long yi, int width, int height,
	   struct box *box, float scale)
{
	unsigned int tinct_options = 0;
	long x = 0;
	long y = 0;
	unsigned int image_width, image_height;
	os_coord image_size;
	float multiplier;
	bool fixed = false;
	wimp_window_state state;

	if (box->background == 0) return;

	state.w = 0;

	if (ro_gui_current_redraw_gui) {
		/* read state of window we're drawing in */
		os_error *error;

		state.w = ro_gui_current_redraw_gui->window;
		error = xwimp_get_window_state(&state);
		if (error) {
			/* not fatal: fixed backgrounds will scroll. */
			LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
			/* invalidate state.w */
			state.w = 0;
		}

		/* Set the plot options */
		if (!ro_gui_current_redraw_gui->option_background_images) return;
		tinct_options = (ro_gui_current_redraw_gui->option_filter_sprites?tinct_BILINEAR_FILTER:0) |
				(ro_gui_current_redraw_gui->option_dither_sprites?tinct_DITHER:0);
	} else {
		if (!option_background_images) return;
		tinct_options = (option_filter_sprites?tinct_BILINEAR_FILTER:0) |
				(option_dither_sprites?tinct_DITHER:0);
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
			tinct_options |= tinct_FILL_HORIZONTALLY | tinct_FILL_VERTICALLY;
			break;
		case CSS_BACKGROUND_REPEAT_REPEAT_X:
			tinct_options |= tinct_FILL_HORIZONTALLY;
			break;
		case CSS_BACKGROUND_REPEAT_REPEAT_Y:
			tinct_options |= tinct_FILL_VERTICALLY;
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
			ro_gui_current_redraw_gui->data.browser.toolbar)
			toolbar_height = ro_gui_current_redraw_gui->data.browser.toolbar->height;

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
				x += 2 * len(&box->style->background_position.horz.value.length, box->style) * scale;
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
				y -= 2 * len(&box->style->background_position.vert.value.length, box->style) * scale;
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
				x += 2 * len(&box->style->background_position.horz.value.length, box->style) * scale;
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
				y -= 2 * len(&box->style->background_position.vert.value.length, box->style) * scale;
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
			_swix(Tinct_PlotScaledAlpha, _IN(2) | _IN(3) | _IN(4) | _IN(5) | _IN(6) | _IN(7),
			    ((char*) box->background->data.png.sprite_area + box->background->data.png.sprite_area->first),
			    x, y - image_height, image_width, image_height,
			    tinct_options);
			break;
#endif
#ifdef WITH_PNG
		case CONTENT_JNG:
		case CONTENT_MNG:
			_swix(Tinct_PlotScaledAlpha, _IN(2) | _IN(3) | _IN(4) | _IN(5) | _IN(6) | _IN(7),
			    ((char*) box->background->data.mng.sprite_area + box->background->data.mng.sprite_area->first),
			    x, y - image_height, image_width, image_height,
			    tinct_options);
			break;
#endif
#ifdef WITH_JPEG
		case CONTENT_JPEG:
			_swix(Tinct_PlotScaled, _IN(2) | _IN(3) | _IN(4) | _IN(5) | _IN(6) | _IN(7),
			    ((char*) box->background->data.jpeg.sprite_area + box->background->data.jpeg.sprite_area->first),
			    x, y - image_height, image_width, image_height,
			    tinct_options);
			break;
#endif
#ifdef WITH_GIF
		case CONTENT_GIF:
			_swix(Tinct_PlotScaledAlpha, _IN(2) | _IN(3) | _IN(4) | _IN(5) | _IN(6) | _IN(7),
			    (char*) box->background->data.gif.gif->frame_image,
			    x, y - image_height, image_width, image_height,
			    tinct_options);
			break;
#endif
	/**\todo Add draw/sprite background support? */
		default:
			break;
	}
}
