/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer windowing toolkit
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

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_plot_util.h>
#include <libnsfb_event.h>
#include <libnsfb_cursor.h>

#include "utils/log.h"
#include "css/css.h"
#include "desktop/browser.h"
#include "desktop/plotters.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/bitmap.h"
#include "framebuffer/image_data.h"

static struct css_style root_style;

enum fbtk_widgettype_e {
        FB_WIDGET_TYPE_ROOT = 0,
        FB_WIDGET_TYPE_WINDOW,
        FB_WIDGET_TYPE_BITMAP,
        FB_WIDGET_TYPE_FILL,
        FB_WIDGET_TYPE_TEXT,
        FB_WIDGET_TYPE_HSCROLL,
        FB_WIDGET_TYPE_VSCROLL,
        FB_WIDGET_TYPE_USER,
};

typedef struct fbtk_widget_list_s fbtk_widget_list_t;

/* wrapper struct for all widget types */
struct fbtk_widget_s {
        /* Generic properties */
        int x;
        int y;
        int width;
        int height;
        colour bg;
        colour fg;

        /* handlers */
        fbtk_mouseclick_t click;
        void *clickpw; /* private data for callback */

        fbtk_input_t input;
        void *inputpw; /* private data for callback */

        fbtk_move_t move;
        void *movepw; /* private data for callback */

        fbtk_redraw_t redraw;
        void *redrawpw; /* private data for callback */

        bool redraw_required;

        fbtk_widget_t *parent; /* parent widget */

        /* Widget specific */
        enum fbtk_widgettype_e type;

        union {
                /* toolkit base handle */
                struct {
                        nsfb_t *fb;
                        fbtk_widget_t *rootw;
                        fbtk_widget_t *input;
                } root;

                /* window */
                struct {
                        /* widgets associated with this window */
                        fbtk_widget_list_t *widgets; /* begining of list */
                        fbtk_widget_list_t *widgets_end; /* end of list */
                } window;

                /* bitmap */
                struct {
                        struct bitmap *bitmap;
                } bitmap;

                /* text */
                struct {
                        char* text;
                        bool outline;
                        fbtk_enter_t enter;
                        void *pw;
                        int idx;
                } text;

                /* application driven widget */
                struct {
                        void *pw; /* private data for user widget */
                } user;

                struct {
                        int pos;
                        int pct;
                } scroll;

        } u;
};

/* widget list */
struct fbtk_widget_list_s {
        struct fbtk_widget_list_s *next;
        struct fbtk_widget_list_s *prev;
        fbtk_widget_t *widget;
} ;

enum {
        POINT_LEFTOF_REGION = 1,
        POINT_RIGHTOF_REGION = 2,
        POINT_ABOVE_REGION = 4,
        POINT_BELOW_REGION = 8,
};

#define REGION(x,y,cx1,cx2,cy1,cy2) \
        (( (y) > (cy2) ? POINT_BELOW_REGION : 0) |                      \
         ( (y) < (cy1) ? POINT_ABOVE_REGION : 0) |                      \
         ( (x) > (cx2) ? POINT_RIGHTOF_REGION : 0) |                    \
         ( (x) < (cx1) ? POINT_LEFTOF_REGION : 0) )

#define SWAP(a, b) do { int t; t=(a); (a)=(b); (b)=t;  } while(0)

/* clip a rectangle to another rectangle */
bool fbtk_clip_rect(const bbox_t * restrict clip, bbox_t * restrict box)
{
        uint8_t region1;
        uint8_t region2;

	if (box->x1 < box->x0) SWAP(box->x0, box->x1);
	if (box->y1 < box->y0) SWAP(box->y0, box->y1);

	region1 = REGION(box->x0, box->y0, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);
	region2 = REGION(box->x1, box->y1, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);

        /* area lies entirely outside the clipping rectangle */
        if ((region1 | region2) && (region1 & region2))
                return false;

        if (box->x0 < clip->x0)
                box->x0 = clip->x0;
        if (box->x0 > clip->x1)
                box->x0 = clip->x1;

        if (box->x1 < clip->x0)
                box->x1 = clip->x0;
        if (box->x1 > clip->x1)
                box->x1 = clip->x1;

        if (box->y0 < clip->y0)
                box->y0 = clip->y0;
        if (box->y0 > clip->y1)
                box->y0 = clip->y1;

        if (box->y1 < clip->y0)
                box->y1 = clip->y0;
        if (box->y1 > clip->y1)
                box->y1 = clip->y1;

        return true;
}


/* creates a new widget of a given type */
static fbtk_widget_t *
new_widget(enum fbtk_widgettype_e type)
{
        fbtk_widget_t *neww;
        neww = calloc(1, sizeof(fbtk_widget_t));
        neww->type = type;
        return neww;
}


/* find the root widget from any widget in the toolkits hierarchy */
static fbtk_widget_t *
get_root_widget(fbtk_widget_t *widget)
{
        while (widget->parent != NULL)
                widget = widget->parent;

        /* check root widget was found */
        if (widget->type != FB_WIDGET_TYPE_ROOT) {
                LOG(("Widget with null parent that is not the root widget!"));
                return NULL;
        }

        return widget;
}


/* set widget to be redrawn */
void
fbtk_request_redraw(fbtk_widget_t *widget)
{
        widget->redraw_required = 1;

        if (widget->type == FB_WIDGET_TYPE_WINDOW) {
                fbtk_widget_list_t *lent = widget->u.window.widgets;

                while (lent != NULL) {
                        lent->widget->redraw_required = 1;
                        lent = lent->next;
        }
        }

        while (widget->parent != NULL) {
                widget = widget->parent;
                widget->redraw_required = 1;
        }
}

static fbtk_widget_t *
add_widget_to_window(fbtk_widget_t *window, fbtk_widget_t *widget)
{
        fbtk_widget_list_t *newent;
        fbtk_widget_list_t **nextent;
        fbtk_widget_list_t *prevent; /* previous entry pointer */

        if (window->type == FB_WIDGET_TYPE_WINDOW) {
                /* caller attached widget to a window */

                nextent = &window->u.window.widgets;
                prevent = NULL;
                while (*nextent != NULL) {
                        prevent = (*nextent);
                        nextent = &(prevent->next);
                }

                newent = calloc(1, sizeof(struct fbtk_widget_list_s));

                newent->widget = widget;
                newent->next = NULL;
                newent->prev = prevent;

                *nextent = newent;

                window->u.window.widgets_end = newent;
        }
        widget->parent = window;

        fbtk_request_redraw(widget);

        return widget;
}

static void
remove_widget_from_window(fbtk_widget_t *window, fbtk_widget_t *widget)
{
        fbtk_widget_list_t *lent = window->u.window.widgets;

        while ((lent != NULL) && (lent->widget != widget)) {
                lent = lent->next;
        }

        if (lent != NULL) {
                if (lent->prev == NULL) {
                        window->u.window.widgets = lent->next;
                } else {
                        lent->prev->next = lent->next;
                }
                if (lent->next == NULL) {
                        window->u.window.widgets_end = lent->prev;
                } else {
                        lent->next->prev = lent->prev;
                }
                free(lent);
        }
}

static void
fbtk_redraw_widget(fbtk_widget_t *root, fbtk_widget_t *widget)
{
        nsfb_bbox_t saved_plot_ctx;
        nsfb_bbox_t plot_ctx;

        //LOG(("widget %p type %d", widget, widget->type));
        if (widget->redraw_required == false)
                return;

        widget->redraw_required = false;

        /* ensure there is a redraw handler */
        if (widget->redraw == NULL)
                return;

        /* get the current clipping rectangle */
        nsfb_plot_get_clip(root->u.root.fb, &saved_plot_ctx);

        plot_ctx.x0 = fbtk_get_x(widget);
        plot_ctx.y0 = fbtk_get_y(widget);
        plot_ctx.x1 = plot_ctx.x0 + widget->width;
        plot_ctx.y1 = plot_ctx.y0 + widget->height;

        /* clip widget to the current area and redraw if its exposed */
        if (nsfb_plot_clip(&saved_plot_ctx, &plot_ctx )) {

                nsfb_plot_set_clip(root->u.root.fb, &plot_ctx);

                /* do our drawing according to type */
                widget->redraw(root, widget, widget->redrawpw);

                /* restore clipping rectangle */
                nsfb_plot_set_clip(root->u.root.fb, &saved_plot_ctx);

                //LOG(("OS redrawing %d,%d %d,%d", fb_plot_ctx.x0, fb_plot_ctx.y0, fb_plot_ctx.x1, fb_plot_ctx.y1));
        }

}

/*************** redraw widgets **************/

static int
fb_redraw_fill(fbtk_widget_t *root, fbtk_widget_t *widget, void *pw)
{
        nsfb_bbox_t bbox;
        fbtk_get_bbox(widget, &bbox);

        nsfb_claim(root->u.root.fb, &bbox);

        /* clear background */
        if ((widget->bg & 0xFF000000) != 0) {
                /* transparent polygon filling isnt working so fake it */
                nsfb_plot_rectangle_fill(root->u.root.fb, &bbox, widget->bg);
        }

        nsfb_release(root->u.root.fb, &bbox);
        return 0;
}

static int
fb_redraw_hscroll(fbtk_widget_t *root, fbtk_widget_t *widget, void *pw)
{
        int hscroll;
        int hpos;
        nsfb_bbox_t bbox;
        nsfb_bbox_t rect;

        fbtk_get_bbox(widget, &bbox);

        nsfb_claim(root->u.root.fb, &bbox);

        rect = bbox;

        nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);

        rect.x0 = bbox.x0 + 1;
        rect.y0 = bbox.y0 + 3;
        rect.x1 = bbox.x1 - 1;
        rect.y1 = bbox.y1 - 3;

        nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->fg);

        rect.x0 = bbox.x0;
        rect.y0 = bbox.y0 + 2;
        rect.x1 = bbox.x1 - 1;
        rect.y1 = bbox.y1 - 5;

        nsfb_plot_rectangle(root->u.root.fb, &rect, 1, 0xFF000000, false, false);

        hscroll = ((widget->width - 4) * widget->u.scroll.pct) / 100 ;
        hpos = ((widget->width - 4) * widget->u.scroll.pos) / 100 ;

        LOG(("hscroll %d",hscroll));

        rect.x0 = bbox.x0 + 3 + hpos;
        rect.y0 = bbox.y0 + 5;
        rect.x1 = bbox.x0 + hscroll + hpos;
        rect.y1 = bbox.y0 + widget->height - 5;

        nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);

        nsfb_release(root->u.root.fb, &bbox);

        return 0;
}

static int
fb_redraw_vscroll(fbtk_widget_t *root, fbtk_widget_t *widget, void *pw)
{
        int vscroll;
        int vpos;

        nsfb_bbox_t bbox;
        nsfb_bbox_t rect;

        fbtk_get_bbox(widget, &bbox);

        nsfb_claim(root->u.root.fb, &bbox);

        rect = bbox;

        nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);

        rect.x0 = bbox.x0 + 1;
        rect.y0 = bbox.y0 + 3;
        rect.x1 = bbox.x1 - 1;
        rect.y1 = bbox.y1 - 3;

        nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->fg);

        rect.x0 = bbox.x0;
        rect.y0 = bbox.y0 + 2;
        rect.x1 = bbox.x1 - 1;
        rect.y1 = bbox.y1 - 5;

        nsfb_plot_rectangle(root->u.root.fb, &rect, 1, 0xFF000000, false, false);

        vscroll = ((widget->height - 4) * widget->u.scroll.pct) / 100 ;
        vpos = ((widget->height - 4) * widget->u.scroll.pos) / 100 ;

        LOG(("scroll %d",vscroll));

        rect.x0 = bbox.x0 + 3 ;
        rect.y0 = bbox.y0 + 5 + vpos;
        rect.x1 = bbox.x0 + widget->width - 3;
        rect.y1 = bbox.y0 + vscroll + vpos - 5;

        nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);

        nsfb_release(root->u.root.fb, &bbox);

        return 0;
}

static int
fb_redraw_bitmap(fbtk_widget_t *root, fbtk_widget_t *widget, void *pw)
{
        nsfb_bbox_t bbox;
        nsfb_bbox_t rect;

        fbtk_get_bbox(widget, &bbox);

        rect = bbox;

        nsfb_claim(root->u.root.fb, &bbox);

        /* clear background */
        if ((widget->bg & 0xFF000000) != 0) {
                /* transparent polygon filling isnt working so fake it */
                nsfb_plot_rectangle_fill(root->u.root.fb, &bbox, widget->bg);
        }

        /* plot the image */
        nsfb_plot_bitmap(root->u.root.fb, &rect, (nsfb_colour_t *)widget->u.bitmap.bitmap->pixdata, widget->u.bitmap.bitmap->width, widget->u.bitmap.bitmap->height, widget->u.bitmap.bitmap->width, !widget->u.bitmap.bitmap->opaque);

        nsfb_release(root->u.root.fb, &bbox);

        return 0;
}

static int
fbtk_window_default_redraw(fbtk_widget_t *root, fbtk_widget_t *window, void *pw)
{
        fbtk_widget_list_t *lent;
        int res = 0;

        if (!window->redraw)
                return res;

        /* get the list of widgets */
        lent = window->u.window.widgets;

        while (lent != NULL) {
                fbtk_redraw_widget(root, lent->widget);
                lent = lent->next;
        }
        return res;
}

static int
fbtk_window_default_move(fbtk_widget_t *window, int x, int y, void *pw)
{
        fbtk_widget_list_t *lent;
        fbtk_widget_t *widget;
        int res = 0;

        /* get the list of widgets */
        lent = window->u.window.widgets_end;

        while (lent != NULL) {
                widget = lent->widget;

                if ((x > widget->x) &&
                    (y > widget->y) &&
                    (x < widget->x + widget->width) &&
                    (y < widget->y + widget->height)) {
                        if (widget->move != NULL) {
                                res = widget->move(widget,
                                             x - widget->x,
                                             y - widget->y,
                                             widget->movepw);
                        }
                        break;
                }
                lent = lent->prev;
        }
        return res;
}

static int
fbtk_window_default_click(fbtk_widget_t *window, nsfb_event_t *event, int x, int y, void *pw)
{
        fbtk_widget_list_t *lent;
        fbtk_widget_t *widget;
        int res = 0;

        /* get the list of widgets */
        lent = window->u.window.widgets;

        while (lent != NULL) {
                widget = lent->widget;

                if ((x > widget->x) &&
                    (y > widget->y) &&
                    (x < widget->x + widget->width) &&
                    (y < widget->y + widget->height)) {
                        if (widget->input != NULL) {
                                fbtk_widget_t *root = get_root_widget(widget);
                                root->u.root.input = widget;
                        }

                        if (widget->click != NULL) {
                                res = widget->click(widget,
                                                    event,
                                                    x - widget->x,
                                                    y - widget->y,
                                                    widget->clickpw);
                                break;
                        }



                }
                lent = lent->next;
        }
        return res;
}

static int
fb_redraw_text(fbtk_widget_t *root, fbtk_widget_t *widget, void *pw)
{
        nsfb_bbox_t bbox;
        nsfb_bbox_t rect;

        fbtk_get_bbox(widget, &bbox);

        rect = bbox;

        nsfb_claim(root->u.root.fb, &bbox);

        /* clear background */
        if ((widget->bg & 0xFF000000) != 0) {
                /* transparent polygon filling isnt working so fake it */
                nsfb_plot_rectangle_fill(root->u.root.fb, &bbox, widget->bg);
        }


        if (widget->u.text.outline) {
                rect.x1--;
                rect.y1--;
                nsfb_plot_rectangle(root->u.root.fb, &rect, 1, 0x00000000, false, false);
        }

        if (widget->u.text.text != NULL) {
                plot.text(bbox.x0 + 3,
                          bbox.y0 + 17,
                          &root_style,
                          widget->u.text.text,
                          strlen(widget->u.text.text),
                          widget->bg,
                          widget->fg);
        }

        nsfb_release(root->u.root.fb, &bbox);

        return 0;
}




static int
text_input(fbtk_widget_t *widget, nsfb_event_t *event, void *pw)
{
        int value;
        if (event == NULL) {
                /* gain focus */
                if (widget->u.text.text == NULL)
                        widget->u.text.text = calloc(1,1);
                widget->u.text.idx = strlen(widget->u.text.text);

                fbtk_request_redraw(widget);

                return 0;
                
        }

        if (event->type != NSFB_EVENT_KEY_DOWN)
                return 0;

        value = event->value.keycode;
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

        default:
                /* allow for new character and null */
                widget->u.text.text = realloc(widget->u.text.text, widget->u.text.idx + 2);
                widget->u.text.text[widget->u.text.idx] = value;
                widget->u.text.text[widget->u.text.idx + 1] = '\0';
                widget->u.text.idx++;
                break;
        }

        fbtk_request_redraw(widget);

        return 0;
}

/* sets the enter action on a writable icon */
void
fbtk_writable_text(fbtk_widget_t *widget, fbtk_enter_t enter, void *pw)
{
        widget->u.text.enter = enter;
        widget->u.text.pw = pw;

        widget->input = text_input;
        widget->inputpw = widget;
}


/********** acessors ***********/
int
fbtk_get_height(fbtk_widget_t *widget)
{
        return widget->height;
}

int
fbtk_get_width(fbtk_widget_t *widget)
{
        return widget->width;
}

int
fbtk_get_x(fbtk_widget_t *widget)
{
        int x = widget->x;

        while (widget->parent != NULL) {
                widget = widget->parent;
                x += widget->x;
        }

        return x;
}

int
fbtk_get_y(fbtk_widget_t *widget)
{
        int y = widget->y;

        while (widget->parent != NULL) {
                widget = widget->parent;
                y += widget->y;
        }

        return y;
}

/* get widgets bounding box in screen co-ordinates */
bool
fbtk_get_bbox(fbtk_widget_t *widget, nsfb_bbox_t *bbox)
{
        bbox->x0 = widget->x;
        bbox->y0 = widget->y;
        bbox->x1 = widget->x + widget->width;
        bbox->y1 = widget->y + widget->height;

        while (widget->parent != NULL) {
                widget = widget->parent;
                bbox->x0 += widget->x;
                bbox->y0 += widget->y;
                bbox->x1 += widget->x;
                bbox->y1 += widget->y;
        }

        return true;
}

void
fbtk_set_handler_click(fbtk_widget_t *widget, fbtk_mouseclick_t click, void *pw)
{
        widget->click = click;
        widget->clickpw = pw;
}

void
fbtk_set_handler_input(fbtk_widget_t *widget, fbtk_input_t input, void *pw)
{
        widget->input = input;
        widget->inputpw = pw;
}

void
fbtk_set_handler_redraw(fbtk_widget_t *widget, fbtk_redraw_t redraw, void *pw)
{
        widget->redraw = redraw;
        widget->redrawpw = pw;
}

void
fbtk_set_handler_move(fbtk_widget_t *widget, fbtk_move_t move, void *pw)
{
        widget->move = move;
        widget->movepw = pw;
}

void *
fbtk_get_userpw(fbtk_widget_t *widget)
{
        if ((widget == NULL) || (widget->type != FB_WIDGET_TYPE_USER))
                return NULL;

        return widget->u.user.pw;
}

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

void
fbtk_set_bitmap(fbtk_widget_t *widget, struct bitmap *image)
{
        if ((widget == NULL) || (widget->type != FB_WIDGET_TYPE_BITMAP))
                return;

        widget->u.bitmap.bitmap = image;

        fbtk_request_redraw(widget);
}

void
fbtk_set_pos_and_size(fbtk_widget_t *widget, int x, int y, int width, int height)
{
        if ((widget->x != x) ||
            (widget->y != y) ||
            (widget->width != width) ||
            (widget->height != height)) {
                widget->x = x;
                widget->y = y;
                widget->width = width;
                widget->height = height;
                fbtk_request_redraw(widget);
                LOG(("%d,%d %d,%d",x,y,width,height));
        }
}

int
fbtk_count_children(fbtk_widget_t *widget)
{
        int count = 0;
        fbtk_widget_list_t *lent;

        if (widget->type != FB_WIDGET_TYPE_WINDOW) {
                if (widget->type != FB_WIDGET_TYPE_ROOT)
                        return -1;
                widget = widget->u.root.rootw;
        }

        lent = widget->u.window.widgets;

        while (lent != NULL) {
                count++;
                lent = lent->next;
        }

        return count;
}


void
fbtk_input(fbtk_widget_t *root, nsfb_event_t *event)
{
        fbtk_widget_t *input;

        root = get_root_widget(root);

        /* obtain widget with input focus */
        input = root->u.root.input;
        if (input == NULL)
                return; /* no widget with input */

        if (input->input == NULL)
                return;

        /* call the widgets input method */
        input->input(input, event, input->inputpw);
}

void
fbtk_click(fbtk_widget_t *widget, nsfb_event_t *event)
{
        fbtk_widget_t *root;
        fbtk_widget_t *window;
        nsfb_bbox_t cloc;

        /* ensure we have the root widget */
        root = get_root_widget(widget);

        nsfb_cursor_loc_get(root->u.root.fb, &cloc);

        /* get the root window */
        window = root->u.root.rootw;
        LOG(("click %d, %d",cloc.x0,cloc.y0));
        if (window->click != NULL)
                window->click(window, event, cloc.x0, cloc.y0, window->clickpw);
}



void
fbtk_move_pointer(fbtk_widget_t *widget, int x, int y, bool relative)
{
        fbtk_widget_t *root;
        fbtk_widget_t *window;
        nsfb_bbox_t cloc;

        /* ensure we have the root widget */
        root = get_root_widget(widget);

        if (relative) {
                nsfb_cursor_loc_get(root->u.root.fb, &cloc);
                cloc.x0 += x;
                cloc.y0 += y;
        } else {
                cloc.x0 = x;
                cloc.y0 = y;
        }

        root->redraw_required = true;

        nsfb_cursor_loc_set(root->u.root.fb, &cloc);

        /* get the root window */
        window = root->u.root.rootw;

        if (window->move != NULL)
                window->move(window, cloc.x0, cloc.y0, window->movepw);

}

int
fbtk_redraw(fbtk_widget_t *widget)
{
        fbtk_widget_t *root;

        /* ensure we have the root widget */
        root = get_root_widget(widget);

        if (!root->redraw_required)
                return 0;

        fbtk_redraw_widget(root, root->u.root.rootw);

        return 1;
}

/****** widget destruction ********/
int fbtk_destroy_widget(fbtk_widget_t *widget)
{
        if (widget->type == FB_WIDGET_TYPE_WINDOW) {
                /* TODO: walk child widgets and destroy them */
        }

        remove_widget_from_window(widget->parent, widget);
        free(widget);

        return 0;
}


/************** Widget creation *************/
fbtk_widget_t *
fbtk_create_text(fbtk_widget_t *window,
                 int x, int y,
                 int width, int height,
                 colour bg, colour fg,
                 bool outline)
{
        fbtk_widget_t *newt = new_widget(FB_WIDGET_TYPE_TEXT);

        newt->x = x;
        newt->y = y;
        newt->width = width;
        newt->height = height;
        newt->u.text.outline = outline;

        newt->fg = fg;
        newt->bg = bg;

        newt->redraw = fb_redraw_text;

        return add_widget_to_window(window, newt);
}

fbtk_widget_t *
fbtk_create_bitmap(fbtk_widget_t *window, int x, int y, colour c, struct bitmap *image)
{
        fbtk_widget_t *newb = new_widget(FB_WIDGET_TYPE_BITMAP);

        newb->x = x;
        newb->y = y;
        newb->width = image->width;
        newb->height = image->height;
        newb->bg = c;

        newb->u.bitmap.bitmap = image;

        newb->redraw = fb_redraw_bitmap;

        return add_widget_to_window(window, newb);
}

static void
fbtk_width_height(fbtk_widget_t *parent, int x, int y, int *width, int *height)
{
        /* make widget fit inside parent */
        if (*width == 0) {
                *width = parent->width - x;
        } else if (*width < 0) {
                *width = parent->width + *width;
        }
        if ((*width + x) > parent->width) {
                *width = parent->width - x;
        }

        if (*height == 0) {
                *height = parent->height - y;
        } else if (*height < 0) {
                *height = parent->height + *height;
        }
        if ((*height + y) > parent->height) {
                *height = parent->height - y;
        }
}

fbtk_widget_t *
fbtk_create_fill(fbtk_widget_t *window, int x, int y, int width, int height, colour c)
{
        fbtk_widget_t *neww = new_widget(FB_WIDGET_TYPE_FILL);

        neww->x = x;
        neww->y = y;
        neww->width = width;
        neww->height = height;

        fbtk_width_height(window, x, y, &neww->width, &neww->height);

        neww->bg = c;

        neww->redraw = fb_redraw_fill;

        return add_widget_to_window(window, neww);
}

fbtk_widget_t *
fbtk_create_hscroll(fbtk_widget_t *window, int x, int y, int width, int height, colour fg, colour bg)
{
        fbtk_widget_t *neww = new_widget(FB_WIDGET_TYPE_HSCROLL);

        neww->x = x;
        neww->y = y;
        neww->width = width;
        neww->height = height;
        neww->fg = fg;
        neww->bg = bg;

        neww->redraw = fb_redraw_hscroll;

        return add_widget_to_window(window, neww);
}

fbtk_widget_t *
fbtk_create_vscroll(fbtk_widget_t *window, int x, int y, int width, int height, colour fg, colour bg)
{
        fbtk_widget_t *neww = new_widget(FB_WIDGET_TYPE_VSCROLL);

        neww->x = x;
        neww->y = y;
        neww->width = width;
        neww->height = height;
        neww->fg = fg;
        neww->bg = bg;

        neww->redraw = fb_redraw_vscroll;

        return add_widget_to_window(window, neww);
}

fbtk_widget_t *
fbtk_create_button(fbtk_widget_t *window,
                   int x, int y,
                   colour c,
                   struct bitmap *image,
                   fbtk_mouseclick_t click,
                   void *pw)
{
        fbtk_widget_t *newb = fbtk_create_bitmap(window, x, y, c, image);

        newb->click = click;
        newb->clickpw = pw;

        return newb;
}

fbtk_widget_t *
fbtk_create_writable_text(fbtk_widget_t *window,
                          int x, int y,
                          int width, int height,
                          colour bg, colour fg,
                          bool outline,
                          fbtk_enter_t enter, void *pw)
{
        fbtk_widget_t *newt = fbtk_create_text(window, x, y, width, height, bg,fg,outline);
        newt->u.text.enter = enter;
        newt->u.text.pw = pw;

        newt->input = text_input;
        newt->inputpw = newt;
        return newt;
}

/* create user widget
 *
 * @param x coord relative to parent
 */
fbtk_widget_t *
fbtk_create_user(fbtk_widget_t *window,
                 int x, int y,
                 int width, int height,
                 void *pw)
{
        fbtk_widget_t *newu = new_widget(FB_WIDGET_TYPE_USER);


        /* make new window fit inside parent */
        if (width == 0) {
                width = window->width - x;
        } else if (width < 0) {
                width = window->width + width;
        }
        if ((width + x) > window->width) {
                width = window->width - x;
        }

        if (height == 0) {
                height = window->height - y;
        } else if (height < 0) {
                height = window->height + height;
        }
        if ((height + y) > window->height) {
                height = window->height - y;
        }

        newu->x = x;
        newu->y = y;
        newu->width = width;
        newu->height = height;

        newu->u.user.pw = pw;

        return add_widget_to_window(window, newu);
}


/* create new window
 *
 * @param x coord relative to parent
 */
fbtk_widget_t *
fbtk_create_window(fbtk_widget_t *parent,
                   int x, int y, int width, int height)
{
        fbtk_widget_t *newwin;

        LOG(("Creating window %p %d,%d %d,%d",parent,x,y,width,height));
        if (parent == NULL)
                return NULL;

        if ((parent->type == FB_WIDGET_TYPE_ROOT) &&
            (parent->u.root.rootw != NULL)) {
                LOG(("Using root window"));
            parent = parent->u.root.rootw;
        }

        newwin = new_widget(FB_WIDGET_TYPE_WINDOW);

        /* make new window fit inside parent */
        if (width == 0) {
                width = parent->width - x;
        } else if (width < 0) {
                width = parent->width + width;
        }
        if ((width + x) > parent->width) {
                width = parent->width - x;
        }

        if (height == 0) {
                height = parent->height - y;
        } else if (height < 0) {
                height = parent->height + height;
        }
        if ((height + y) > parent->height) {
                height = parent->height - y;
        }

        newwin->x = x;
        newwin->y = y;
        newwin->width = width;
        newwin->height = height;

        newwin->redraw = fbtk_window_default_redraw;
        newwin->move = fbtk_window_default_move;
        newwin->click = fbtk_window_default_click;

        LOG(("Created window %p %d,%d %d,%d",newwin,x,y,width,height));

        return add_widget_to_window(parent, newwin);
}

bool fbtk_event(fbtk_widget_t *root, nsfb_event_t *event, int timeout)
{
        bool unused = false; /* is the event available */

        /* ensure we have the root widget */
        root = get_root_widget(root);

        //LOG(("Reading event with timeout %d",timeout));

        if (nsfb_event(root->u.root.fb, event, timeout) == false)
                return false;

        switch (event->type) {
        case NSFB_EVENT_KEY_DOWN:
        case NSFB_EVENT_KEY_UP:
                if ((event->value.controlcode >= NSFB_KEY_MOUSE_1) &&
                    (event->value.controlcode <= NSFB_KEY_MOUSE_5)) {
                        fbtk_click(root, event);
                } else {
                        fbtk_input(root, event);
                }
                break;

        case NSFB_EVENT_CONTROL:
                unused = true;
                break;

        case NSFB_EVENT_MOVE_RELATIVE:
                fbtk_move_pointer(root, event->value.vector.x, event->value.vector.y, true);
                break;

        case NSFB_EVENT_MOVE_ABSOLUTE:
                fbtk_move_pointer(root, event->value.vector.x, event->value.vector.y, false);
                break;
                
        default:
                break;

        }
        return unused;
}


nsfb_t *
fbtk_get_nsfb(fbtk_widget_t *widget)
{
        fbtk_widget_t *root;

        /* ensure we have the root widget */
        root = get_root_widget(widget);

        return root->u.root.fb;
}

/* Initialise toolkit for use */
fbtk_widget_t *
fbtk_init(nsfb_t *fb)
{
        fbtk_widget_t *root = new_widget(FB_WIDGET_TYPE_ROOT);

        nsfb_get_geometry(fb, &root->width, &root->height, NULL);

        LOG(("width %d height %d",root->width, root->height));
        root->u.root.fb = fb;
        root->x = 0;
        root->y = 0;
        root->u.root.rootw = fbtk_create_window(root, 0, 0, 0, 0);

        root_style.font_size.value.length.unit = CSS_UNIT_PX;
        root_style.font_size.value.length.value = 14;

        return root;
}

static int keymap[] = {
       /* 0    1    2    3    4    5    6    7    8    9               */
         -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,   8,   9, /*   0 -   9 */
         -1,  -1,  -1,  13,  -1,  -1,  -1,  -1,  -1,  -1, /*  10 -  19 */
         -1,  -1,  -1,  -1,  -1,  -1,  -1,  27,  -1,  -1, /*  20 -  29 */
         -1,  -1, ' ', '!', '"', '#', '$',  -1, '&','\'', /*  30 -  39 */
        '(', ')', '*', '+', ',', '-', '.', '/', '0', '1', /*  40 -  49 */
        '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', /*  50 -  59 */
        '<', '=', '>', '?', '@',  -1,  -1,  -1,  -1,  -1, /*  60 -  69 */
         -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, /*  70 -  79 */
         -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, /*  80 -  89 */
         -1, '[','\\', ']', '~', '_', '`', 'a', 'b', 'c', /*  90 -  99 */
        'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', /* 100 - 109 */
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', /* 110 - 119 */
        'x', 'y', 'z',  -1,  -1,  -1,  -1,  -1,  -1,  -1, /* 120 - 129 */
};

static int sh_keymap[] = {
       /* 0    1    2    3    4    5    6    7    8    9               */
         -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,   8,   9, /*   0 -   9 */
         -1,  -1,  -1,  13,  -1,  -1,  -1,  -1,  -1,  -1, /*  10 -  19 */
         -1,  -1,  -1,  -1,  -1,  -1,  -1,  27,  -1,  -1, /*  20 -  29 */
         -1,  -1, ' ', '!', '"', '~', '$',  -1, '&', '@', /*  30 -  39 */
        '(', ')', '*', '+', '<', '_', '>', '?', ')', '!', /*  40 -  49 */
        '"', 243, '&', '*', '(', ';', ':', /*  50 -  59 */
        '<', '+', '>', '?', '@',  -1,  -1,  -1,  -1,  -1, /*  60 -  69 */
         -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, /*  70 -  79 */
         -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, /*  80 -  89 */
         -1, '{', '|', '}', '~', '_', 254, 'A', 'B', 'C', /*  90 -  99 */
        'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', /* 100 - 109 */
        'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', /* 110 - 119 */
        'X', 'Y', 'Z',  -1,  -1,  -1,  -1,  -1,  -1,  -1, /* 120 - 129 */
};


/* performs character mapping */
int fbtk_keycode_to_ucs4(int code, uint8_t mods)
{
        int ucs4 = -1;

        if (mods) {
                if ((code >= 0) && (code < sizeof(sh_keymap)))
                        ucs4 = sh_keymap[code];
        } else {
                if ((code >= 0) && (code < sizeof(keymap)))
                        ucs4 = keymap[code];
        }
        return ucs4;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
