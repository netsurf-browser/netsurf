/*
 * Copyright 2010 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer windowing toolkit scrollbar widgets.
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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_plot_util.h>
#include <libnsfb_event.h>

#include "utils/log.h"
#include "desktop/browser.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/framebuffer.h"
#include "framebuffer/image_data.h"

#include "widget.h"

//#define TEXT_WIDGET_BORDER 3 /**< The pixel border round a text widget. */

/* Lighten a colour by taking seven eights of each channel's intensity
 * and adding a full eighth
 */
#define brighten_colour(c1)					\
	(((((7 * ((c1 >> 16) & 0xff)) >> 3) + 32) << 16) |	\
	 ((((7 * ((c1 >> 8) & 0xff)) >> 3) + 32) << 8) |	\
	 ((((7 * (c1 & 0xff)) >> 3) + 32) << 0))

/* Convert pixels to points, assuming a DPI of 90 */
#define px_to_pt(x) (((x) * 72) / FBTK_DPI)

/** Text redraw callback.
 *
 * Called when a text widget requires redrawing.
 *
 * @param widget The widget to be redrawn.
 * @param cbi The callback parameters.
 * @return The callback result.
 */
static int
fb_redraw_text(fbtk_widget_t *widget, fbtk_callback_info *cbi )
{
	nsfb_bbox_t bbox;
	nsfb_bbox_t rect;
	fbtk_widget_t *root;
	plot_font_style_t font_style;
	int fh;
	int padding;

	padding = (widget->height * FBTK_WIDGET_PADDING) / 200;

	root = fbtk_get_root_widget(widget);

	fbtk_get_bbox(widget, &bbox);

	rect = bbox;

	nsfb_claim(root->u.root.fb, &bbox);

	/* clear background */
	if ((widget->bg & 0xFF000000) != 0) {
		/* transparent polygon filling isnt working so fake it */
		nsfb_plot_rectangle_fill(root->u.root.fb, &bbox, widget->bg);
	}

	/* widget can have a single pixel outline border */
	if (widget->u.text.outline) {
		rect.x1--;
		rect.y1--;
		nsfb_plot_rectangle(root->u.root.fb, &rect, 1, 0x00000000, false, false);
		padding++;
	}

	if (widget->u.text.text != NULL) {
		fh = widget->height - padding - padding;
		font_style.family = PLOT_FONT_FAMILY_SANS_SERIF;
		font_style.size = px_to_pt(fh) * FONT_SIZE_SCALE;
		font_style.weight = 400;
		font_style.flags = FONTF_NONE;
		font_style.background = widget->bg;
		font_style.foreground = widget->fg;

		FBTK_LOG(("plotting %p at %d,%d %d,%d w/h %d,%d font h %d padding %d",
		     widget, bbox.x0, bbox.y0, bbox.x1, bbox.y1,
		     widget->width, widget->height, fh, padding));
		/* Call the fb text plotting, baseline is 3/4 down the
		 * font, somewhere along the way the co-ordinate
		 * system for the baseline is to the "higher value
		 * pixel co-ordinate" due to this the + 1 is neccessary.
		 */
		fb_plotters.text(bbox.x0 + padding,
			  bbox.y0 + (((fh * 3) + 3)/4) + padding + 1,
			  widget->u.text.text,
			  strlen(widget->u.text.text),
			  &font_style);
	}

	nsfb_update(root->u.root.fb, &bbox);

	return 0;
}

/** Text button redraw callback.
 *
 * Called when a text widget requires redrawing.
 *
 * @param widget The widget to be redrawn.
 * @param cbi The callback parameters.
 * @return The callback result.
 */
static int
fb_redraw_text_button(fbtk_widget_t *widget, fbtk_callback_info *cbi )
{
	nsfb_bbox_t bbox;
	nsfb_bbox_t rect;
	nsfb_bbox_t line;
	nsfb_plot_pen_t pen;
	plot_font_style_t font_style;
	int fh;
	int border;
	fbtk_widget_t *root = fbtk_get_root_widget(widget);

	if (widget->height < 20) {
		border = 0;
	} else {
		border = (widget->height * 10) / 90;
	}

	pen.stroke_type = NFSB_PLOT_OPTYPE_SOLID;
	pen.stroke_width = 1;
	pen.stroke_colour = brighten_colour(widget->bg);

	fbtk_get_bbox(widget, &bbox);

	rect = bbox;
	rect.x1--;
	rect.y1--;

	nsfb_claim(root->u.root.fb, &bbox);

	/* clear background */
	if ((widget->bg & 0xFF000000) != 0) {
		/* transparent polygon filling isnt working so fake it */
		nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);
	}

	if (widget->u.text.outline) {
		line.x0 = rect.x0;
		line.y0 = rect.y0;
		line.x1 = rect.x0;
		line.y1 = rect.y1;
		nsfb_plot_line(root->u.root.fb, &line, &pen);
		line.x0 = rect.x0;
		line.y0 = rect.y0;
		line.x1 = rect.x1;
		line.y1 = rect.y0;
		nsfb_plot_line(root->u.root.fb, &line, &pen);
		pen.stroke_colour = darken_colour(widget->bg);
		line.x0 = rect.x0;
		line.y0 = rect.y1;
		line.x1 = rect.x1;
		line.y1 = rect.y1;
		nsfb_plot_line(root->u.root.fb, &line, &pen);
		line.x0 = rect.x1;
		line.y0 = rect.y0;
		line.x1 = rect.x1;
		line.y1 = rect.y1;
		nsfb_plot_line(root->u.root.fb, &line, &pen);
		border++;
	}

	if (widget->u.text.text != NULL) {
		fh = widget->height - border - border;
		font_style.family = PLOT_FONT_FAMILY_SANS_SERIF;
		font_style.size = px_to_pt(fh) * FONT_SIZE_SCALE;
		font_style.weight = 400;
		font_style.flags = FONTF_NONE;
		font_style.background = widget->bg;
		font_style.foreground = widget->fg;

		LOG(("plotting %p at %d,%d %d,%d w/h %d,%d font h %d border %d",
		     widget, bbox.x0, bbox.y0, bbox.x1, bbox.y1,
		     widget->width, widget->height, fh, border));
		/* Call the fb text plotting, baseline is 3/4 down the
		 * font, somewhere along the way the co-ordinate
		 * system for the baseline is to the "higher value
		 * pixel co-ordinate" due to this the + 1 is neccessary.
		 */
		fb_plotters.text(bbox.x0 + border,
			  bbox.y0 + (((fh * 3) + 3)/4) + border + 1,
			  widget->u.text.text,
			  strlen(widget->u.text.text),
			  &font_style);
	}

	nsfb_update(root->u.root.fb, &bbox);

	return 0;
}

/** Routine called when text events occour in writeable widget.
 *
 * @param widget The widget reciving input events.
 * @param cbi The callback parameters.
 * @return The callback result.
 */
static int
text_input(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int value;
	static uint8_t modifier = 0;
	char *temp;

	if (cbi->event == NULL) {
		/* gain focus */
		if (widget->u.text.text == NULL)
			widget->u.text.text = calloc(1,1);
		widget->u.text.idx = strlen(widget->u.text.text);

		fbtk_request_redraw(widget);

		return 0;
	}

	value = cbi->event->value.keycode;

	if (cbi->event->type != NSFB_EVENT_KEY_DOWN) {
		switch (value) {
		case NSFB_KEY_RSHIFT:
			modifier &= ~1;
			break;

		case NSFB_KEY_LSHIFT:
			modifier &= ~(1<<1);
			break;

		default:
			break;
		}
		return 0;
	}

	switch (value) {
	case NSFB_KEY_BACKSPACE:
		if (widget->u.text.idx <= 0)
			break;
		widget->u.text.idx--;
		widget->u.text.text[widget->u.text.idx] = 0;
		break;

	case NSFB_KEY_RETURN:
		widget->u.text.enter(widget->u.text.pw, widget->u.text.text);
		break;

	case NSFB_KEY_PAGEUP:
	case NSFB_KEY_PAGEDOWN:
	case NSFB_KEY_RIGHT:
	case NSFB_KEY_LEFT:
	case NSFB_KEY_UP:
	case NSFB_KEY_DOWN:
		/* Not handling any of these correctly yet, but avoid putting
		 * charcters in the text widget when they're pressed. */
		break;

	case NSFB_KEY_RSHIFT:
		modifier |= 1;
		break;

	case NSFB_KEY_LSHIFT:
		modifier |= 1<<1;
		break;

	default:
		/* allow for new character and null */
		temp = realloc(widget->u.text.text, widget->u.text.idx + 2);
		if (temp != NULL) {
			widget->u.text.text = temp;
			widget->u.text.text[widget->u.text.idx] = fbtk_keycode_to_ucs4(value, modifier);
			widget->u.text.text[widget->u.text.idx + 1] = '\0';
			widget->u.text.idx++;
		}

		break;
	}

	fbtk_request_redraw(widget);

	return 0;
}

/* exported function documented in fbtk.h */
void
fbtk_writable_text(fbtk_widget_t *widget, fbtk_enter_t enter, void *pw)
{
	widget->u.text.enter = enter;
	widget->u.text.pw = pw;

	fbtk_set_handler(widget, FBTK_CBT_INPUT, text_input, widget);
}

/* exported function documented in fbtk.h */
void
fbtk_set_text(fbtk_widget_t *widget, const char *text)
{
	if ((widget == NULL) || (widget->type != FB_WIDGET_TYPE_TEXT))
		return;
	if (widget->u.text.text != NULL) {
		if (strcmp(widget->u.text.text, text) == 0)
			return; /* text is being set to the same thing */
		free(widget->u.text.text);
	}
	widget->u.text.text = strdup(text);
	widget->u.text.idx = strlen(text);

	fbtk_request_redraw(widget);
}

/* exported function documented in fbtk.h */
fbtk_widget_t *
fbtk_create_text(fbtk_widget_t *parent,
		 int x,
		 int y,
		 int width,
		 int height,
		 colour bg,
		 colour fg,
		 bool outline)
{
	fbtk_widget_t *neww;

	neww = fbtk_widget_new(parent, FB_WIDGET_TYPE_TEXT, x, y, width, height);
	neww->fg = fg;
	neww->bg = bg;
	neww->mapped = true;
	neww->u.text.outline = outline;

	fbtk_set_handler(neww, FBTK_CBT_REDRAW, fb_redraw_text, NULL);

	return neww;
}

/* exported function documented in fbtk.h */
fbtk_widget_t *
fbtk_create_writable_text(fbtk_widget_t *parent,
			  int x,
			  int y,
			  int width,
			  int height,
			  colour bg,
			  colour fg,
			  bool outline,
			  fbtk_enter_t enter,
			  void *pw)
{
	fbtk_widget_t *neww;

	neww = fbtk_widget_new(parent, FB_WIDGET_TYPE_TEXT, x, y, width, height);
	neww->fg = fg;
	neww->bg = bg;
	neww->mapped = true;

	neww->u.text.outline = outline;
	neww->u.text.enter = enter;
	neww->u.text.pw = pw;

	fbtk_set_handler(neww, FBTK_CBT_REDRAW, fb_redraw_text, NULL);
	fbtk_set_handler(neww, FBTK_CBT_INPUT, text_input, neww);

	return neww;
}

/* exported function documented in fbtk.h */
fbtk_widget_t *
fbtk_create_text_button(fbtk_widget_t *parent,
			int x,
			int y,
			int width,
			int height,
			colour bg,
			colour fg,
			fbtk_callback click,
			void *pw)
{
	fbtk_widget_t *neww;

	neww = fbtk_widget_new(parent, FB_WIDGET_TYPE_TEXT, x, y, width, height);
	neww->fg = fg;
	neww->bg = bg;
	neww->mapped = true;

	neww->u.text.outline = true;

	fbtk_set_handler(neww, FBTK_CBT_REDRAW, fb_redraw_text_button, NULL);
	fbtk_set_handler(neww, FBTK_CBT_CLICK, click, pw);
	fbtk_set_handler(neww, FBTK_CBT_POINTERENTER, fbtk_set_ptr, &hand_image);

	return neww;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
