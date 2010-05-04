/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer windowing toolkit scrollbar widgets
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

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_event.h>
#include <libnsfb_cursor.h>

#include "utils/log.h"
#include "desktop/browser.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/fbtk_widget.h"
#include "framebuffer/bitmap.h"
#include "framebuffer/image_data.h"

/** Vertical scroll widget */

static int
vscroll_redraw(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
        int vscroll;
        int vpos;

        nsfb_bbox_t bbox;
        nsfb_bbox_t rect;
	fbtk_widget_t *root = get_root_widget(widget);

        fbtk_get_bbox(widget, &bbox);

        nsfb_claim(root->u.root.fb, &bbox);

        rect = bbox;

	/* background */
        nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);

        rect.x0 = bbox.x0 + 2;
        rect.y0 = bbox.y0 + 1;
        rect.x1 = bbox.x1 - 3;
        rect.y1 = bbox.y1 - 2;
        nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->fg);

	/* scroll well */
        nsfb_plot_rectangle(root->u.root.fb, &rect, 1, 0xFF999999, false, false);

	/* scroll well outline */
        vscroll = ((widget->height - 4) * widget->u.scroll.pct) / 100 ;
        vpos = ((widget->height - 4) * widget->u.scroll.pos) / 100 ;

        LOG(("scroll %d",vscroll));

        rect.x0 = bbox.x0 + 5;
        rect.y0 = bbox.y0 + 3 + vpos;
        rect.x1 = bbox.x0 + widget->width - 5;
        rect.y1 = bbox.y0 + vscroll + vpos;

        nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);

        nsfb_update(root->u.root.fb, &bbox);

        return 0;
}

static int
vscrollu_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int ret = 0;
        if (cbi->event->type == NSFB_EVENT_KEY_DOWN)
		ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLY, -1);
	return ret;
}

static int
vscrolld_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int ret = 0;
        if (cbi->event->type == NSFB_EVENT_KEY_DOWN)
		ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLY, 1);
	return ret;
}

static int
vscrollarea_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
        int vscroll;
        int vpos;
	int ret = 0;

        if (cbi->event->type != NSFB_EVENT_KEY_DOWN)
        	return 0;

        vscroll = ((widget->height - 4) * widget->u.scroll.pct) / 100 ;
        vpos = ((widget->height - 4) * widget->u.scroll.pos) / 100 ;

	if (cbi->y < vpos) {
                /* above bar */
		ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLY, -1);
	} else if (cbi->y > (vpos + vscroll)) {
                /* below bar */
		ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLY, 1);
	}
	return ret;
}


fbtk_widget_t *
fbtk_create_vscroll(fbtk_widget_t *window, 
		    int x, int y, 
		    int width, int height, 
		    colour fg, 
		    colour bg,
		    fbtk_callback callback,
		    void *context)
{
        fbtk_widget_t *neww = new_widget(FB_WIDGET_TYPE_VSCROLL);

        neww->x = x;
        neww->y = y + scrollu.height;
        neww->width = width;
        neww->height = height - scrollu.height - scrolld.height;
        neww->fg = fg;
        neww->bg = bg;

	fbtk_set_handler(neww, FBTK_CBT_REDRAW, vscroll_redraw, NULL);

	fbtk_set_handler(neww, FBTK_CBT_CLICK, vscrollarea_click, neww);

	fbtk_set_handler(neww, FBTK_CBT_SCROLLY, callback, context);

	neww->u.scroll.btnul = fbtk_create_button(window, x + (width - scrollu.width) / 2, y, fg, &scrollu, vscrollu_click, neww);

	neww->u.scroll.btndr = fbtk_create_button(window, x + (width - scrolld.width) / 2, y + height - scrolld.height, fg, &scrolld, vscrolld_click, neww);


        return add_widget_to_window(window, neww);
}

/* Horizontal scroll widget */

static int
hscroll_redraw(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
        int hscroll;
        int hpos;
        nsfb_bbox_t bbox;
        nsfb_bbox_t rect;
	fbtk_widget_t *root = get_root_widget(widget);

        fbtk_get_bbox(widget, &bbox);

        nsfb_claim(root->u.root.fb, &bbox);

        rect = bbox;

	/* background */
        nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);

	/* scroll well */
        rect.x0 = bbox.x0 + 1;
        rect.y0 = bbox.y0 + 2;
        rect.x1 = bbox.x1 - 2;
        rect.y1 = bbox.y1 - 3;
        nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->fg);

	/* scroll well outline */
        nsfb_plot_rectangle(root->u.root.fb, &rect, 1, 0xFF999999, false, false);

        hscroll = ((widget->width - 4) * widget->u.scroll.pct) / 100 ;
        hpos = ((widget->width - 4) * widget->u.scroll.pos) / 100 ;

        LOG(("hscroll %d",hscroll));

        rect.x0 = bbox.x0 + 3 + hpos;
        rect.y0 = bbox.y0 + 5;
        rect.x1 = bbox.x0 + hscroll + hpos;
        rect.y1 = bbox.y0 + widget->height - 5;

        nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);

        nsfb_update(root->u.root.fb, &bbox);

        return 0;
}

static int
hscrolll_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int ret = 0;
        if (cbi->event->type == NSFB_EVENT_KEY_DOWN)
		ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLX, -1);
	return ret;
}

static int
hscrollr_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int ret = 0;
        if (cbi->event->type == NSFB_EVENT_KEY_DOWN)
		ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLX, 1);
	return ret;
}

static int
hscrollarea_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
        int hscroll;
        int hpos;
	int ret;

        if (cbi->event->type != NSFB_EVENT_KEY_DOWN)
        	return 0;

        hscroll = ((widget->width - 4) * widget->u.scroll.pct) / 100 ;
        hpos = ((widget->width - 4) * widget->u.scroll.pos) / 100 ;

	if (cbi->x < hpos) {
                /* above bar */
		ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLX, -1);
	} else if (cbi->x > (hpos + hscroll)) {
                /* below bar */
		ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLX, 1);
	}
	return ret;
}

fbtk_widget_t *
fbtk_create_hscroll(fbtk_widget_t *window, 
		    int x, int y, 
		    int width, int height, 
		    colour fg, 
		    colour bg,
		    fbtk_callback callback,
		    void *context)
{
        fbtk_widget_t *neww = new_widget(FB_WIDGET_TYPE_HSCROLL);

        neww->x = x + scrolll.width;
        neww->y = y;
        neww->width = width - scrolll.width - scrollr.width;
        neww->height = height;
        neww->fg = fg;
        neww->bg = bg;

	fbtk_set_handler(neww, FBTK_CBT_REDRAW, hscroll_redraw, NULL);
	fbtk_set_handler(neww, FBTK_CBT_CLICK, hscrollarea_click, neww);
	fbtk_set_handler(neww, FBTK_CBT_SCROLLX, callback, context);

	neww->u.scroll.btnul = fbtk_create_button(window, x, y + ((height - scrolll.height) / 2), fg, &scrolll, hscrolll_click, neww);

	neww->u.scroll.btndr = fbtk_create_button(window, x + width - scrollr.width, y + ((height - scrolll.height) / 2), fg, &scrollr, hscrollr_click, neww);

        return add_widget_to_window(window, neww);
}


void
fbtk_set_scroll(fbtk_widget_t *widget, int pct)
{
        if (widget == NULL)
                return;

        if ((widget->type == FB_WIDGET_TYPE_HSCROLL) ||
            (widget->type == FB_WIDGET_TYPE_VSCROLL)) {

                widget->u.scroll.pct = pct;
                fbtk_request_redraw(widget);
        }
}

void
fbtk_set_scroll_pos(fbtk_widget_t *widget, int pos)
{
        if (widget == NULL)
                return;

        if ((widget->type == FB_WIDGET_TYPE_HSCROLL) ||
            (widget->type == FB_WIDGET_TYPE_VSCROLL)) {

        widget->u.scroll.pos = pos;

        fbtk_request_redraw(widget);
        }
}

