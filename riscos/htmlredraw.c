/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "oslib/colourtrans.h"
#include "oslib/font.h"
#include "netsurf/content/content.h"
#include "netsurf/render/html.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"


static void html_redraw_box(struct content *content, struct box * box,
		signed long x, signed long y,
		unsigned long current_background_color,
		signed long gadget_subtract_x, signed long gadget_subtract_y,
		bool *select_on,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1);
static inline void html_redraw_clip(long clip_x0, long clip_y0,
		long clip_x1, long clip_y1);


void html_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1)
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

	html_redraw_box(c, box, x, y, background_colour, x, y,
			&select_on, clip_x0, clip_y0, clip_x1, clip_y1);
}


/* validation strings can't be const */
static char validation_textarea[] = "R7;L";
static char validation_textbox[] = "";
static char validation_password[] = "D*";
static char validation_actionbutton[] = "R5";
static char validation_actionbutton_pressed[] = "R5,3";
static char validation_select[] = "R2";
static char validation_checkbox_selected[] = "Sopton";
static char validation_checkbox_unselected[] = "Soptoff";
static char validation_radio_selected[] = "Sradioon";
static char validation_radio_unselected[] = "Sradiooff";

static char select_text_multiple[] = "<Multiple>";  /* TODO: read from messages */
static char select_text_none[] = "<None>";

static char empty_text[] = "";

void html_redraw_box(struct content *content, struct box * box,
		signed long x, signed long y,
		unsigned long current_background_color,
		signed long gadget_subtract_x, signed long gadget_subtract_y,
		bool *select_on,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1)
{
	struct box *c;
	char *select_text;
	struct formoption *opt;
	int width, height, x0, y0, x1, y1;

	x += box->x * 2;
	y -= box->y * 2;
	width = box->width * 2;
	height = box->height * 2;

	x0 = x;
	y1 = y - 1;
	x1 = x0 + width - 1;
	y0 = y1 - height + 1;

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
		colourtrans_set_gcol(box->style->background_color << 8, colourtrans_USE_ECFS, os_ACTION_OVERWRITE, 0);
		os_plot(os_MOVE_TO, x, y);
		os_plot(os_PLOT_RECTANGLE | os_PLOT_BY, width, -height);
		current_background_color = box->style->background_color;
	}

	if (box->object) {
		content_redraw(box->object, x, y, width, height, x0, y0, x1, y1);

	} else if (box->gadget && box->gadget->type != GADGET_TEXTAREA) {
		wimp_icon icon;
		LOG(("writing GADGET"));

		icon.extent.x0 = -gadget_subtract_x + x;
		icon.extent.y0 = -gadget_subtract_y + y - height;
		icon.extent.x1 = -gadget_subtract_x + x + width;
		icon.extent.y1 = -gadget_subtract_y + y;

		switch (box->gadget->type) {
		case GADGET_TEXTBOX:
			colourtrans_set_font_colours(box->font->handle, current_background_color << 8,
					     box->style->color << 8, 14, 0, 0, 0);
			font_paint(box->font->handle, box->gadget->data.textbox.text,
					font_OS_UNITS | font_GIVEN_FONT | font_KERN,
					x, y - (int) (box->height * 1.5),
					NULL, NULL, 0);
			break;

                case GADGET_PASSWORD:
			icon.flags = wimp_ICON_TEXT | wimp_ICON_BORDER |
				wimp_ICON_VCENTRED | wimp_ICON_FILLED |
				wimp_ICON_INDIRECTED |
				(wimp_COLOUR_DARK_GREY << wimp_ICON_FG_COLOUR_SHIFT) |
				(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT);
			icon.data.indirected_text.text = box->gadget->data.password.text;
			icon.data.indirected_text.size = box->gadget->data.password.maxlength + 1;
			icon.data.indirected_text.validation = validation_password;
			LOG(("writing GADGET PASSWORD"));
			wimp_plot_icon(&icon);
      			break;

		case GADGET_ACTIONBUTTON:
			icon.flags = wimp_ICON_TEXT | wimp_ICON_BORDER |
			    wimp_ICON_VCENTRED | wimp_ICON_FILLED |
			    wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED |
			    (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT);
			icon.data.indirected_text.text = box->gadget->data.actionbutt.label;
			icon.data.indirected_text.size = strlen(box->gadget->data.actionbutt.label);
			if (box->gadget->data.actionbutt.pressed) {
				icon.data.indirected_text.validation = validation_actionbutton_pressed;
				icon.flags |=
				    (wimp_COLOUR_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT) | wimp_ICON_SELECTED;
			} else {
				icon.data.indirected_text.validation = validation_actionbutton;
				icon.flags |= (wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT);
			}
			LOG(("writing GADGET ACTION"));
			wimp_plot_icon(&icon);
			break;

		case GADGET_SELECT:
			icon.flags = wimp_ICON_TEXT | wimp_ICON_BORDER |
			    wimp_ICON_VCENTRED | wimp_ICON_FILLED |
			    wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED |
			    (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
			    (wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT);
			select_text = 0;
			opt = box->gadget->data.select.items;
//			if (box->gadget->data.select.size == 1) {
			        while (opt != NULL)
			        {
				        if (opt->selected)
				        {
					        if (select_text == 0)
						        select_text = opt->text;
					        else
						        select_text = select_text_multiple;
				        }
				        opt = opt->next;
			        }
			        if (select_text == 0)
				        select_text = select_text_none;
/*			}
			else {
			        while (opt != NULL)
			        {
				        if (opt->selected)
				        {
					        select_text = opt->text;
					        opt = opt->next;
					        break;
				        }
				        opt = opt->next;
			        }
			        if (select_text == 0) {
			        // display the first n options
			                opt = box->gadget->data.select.items;
			                select_text = opt->text;
			                opt = opt->next;
			                for(i = box->gadget->data.select.size-1;
                                            i != 0; i--, opt=opt->next) {
                                                strcat(select_text, "\n");
                                                strcat(select_text, opt->text);
                                        }
			        }
			        else {
                                        for(i = box->gadget->data.select.size-1;
                                            i != 0; i--, opt=opt->next) {
                                                strcat(select_text, "\n");
                                                strcat(select_text, opt->text);
                                        }
			        }
			}
*/			icon.data.indirected_text.text = select_text;
			icon.data.indirected_text.size = strlen(icon.data.indirected_text.text);
			icon.data.indirected_text.validation = validation_select;
			LOG(("writing GADGET ACTION"));
			wimp_plot_icon(&icon);
			break;

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
		case GADGET_IMAGE:
			break;
		}
		LOG(("gadgets finished"));
	
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

		font_paint(box->font->handle, box->text,
			   font_OS_UNITS | font_GIVEN_FONT | font_KERN | font_GIVEN_LENGTH,
			   x, y - (int) (box->height * 1.5),
			   NULL, NULL, (int) box->length);

	} else {
		for (c = box->children; c != 0; c = c->next)
			if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
				html_redraw_box(content, c, x,
						y, current_background_color,
						gadget_subtract_x, gadget_subtract_y, select_on,
						x0, y0, x1, y1);

		for (c = box->float_children; c != 0; c = c->next_float)
			html_redraw_box(content, c, x,
					y, current_background_color,
					gadget_subtract_x, gadget_subtract_y, select_on,
					x0, y0, x1, y1);
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


inline void html_redraw_clip(long clip_x0, long clip_y0,
		long clip_x1, long clip_y1)
{
	os_set_graphics_window();
	os_writec((char) (clip_x0 & 0xff)); os_writec((char) (clip_x0 >> 8));
	os_writec((char) (clip_y0 & 0xff)); os_writec((char) (clip_y0 >> 8));
	os_writec((char) (clip_x1 & 0xff)); os_writec((char) (clip_x1 >> 8));
	os_writec((char) (clip_y1 & 0xff)); os_writec((char) (clip_y1 >> 8));
}

