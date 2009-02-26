/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <unistd.h>

#ifdef WITH_HUBBUB
#include <hubbub/hubbub.h>
#endif

#include "desktop/gui.h"
#include "desktop/plotters.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "desktop/history_core.h"

#include "image/bitmap.h"

#include "framebuffer/fb_bitmap.h"
#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_frontend.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_schedule.h"
#include "framebuffer/fb_cursor.h"
#include "framebuffer/fb_rootwindow.h"
#include "framebuffer/fb_image_data.h"

#define FB_FRAME_COLOUR 0xFFDDDDDD
#define FB_COLOUR_BLACK 0xFF000000
#define FB_COLOUR_WHITE 0xFFFFFFFF

enum fb_widget_type_e {
        FB_WIDGET_TYPE_NONE = 0,
        FB_WIDGET_TYPE_BUTTON,
        FB_WIDGET_TYPE_WINDOW,
        FB_WIDGET_TYPE_TEXT,
};

struct fb_widget {
        struct fb_widget *next;

        /* properties */
        enum fb_widget_type_e type;
        int x;
        int y;
        int width;
        int height;
        colour bg;
        colour fg;
        bool outline;

        /* handlers */
        fb_widget_mouseclick_t click;
        fb_widget_input_t input;

        /* data */
        struct bitmap *bitmap;
        struct gui_window *g;
        char* text;
};

static struct css_style root_style;

static struct fb_widget *widget_list;

/* widget for status */
static struct fb_widget *status_widget;

/* widget for url */
static struct fb_widget *url_widget;

/* widget with input focus */
static struct fb_widget *inputfocus_widget;
static int input_idx;

struct gui_window *rootwindow;

static void
fb_redraw_widget(struct fb_widget *widget)
{
       bbox_t saved_plot_ctx;

        /* set the clipping rectangle to the widget area */
        saved_plot_ctx = fb_plot_ctx;

        fb_plot_ctx.x0 = widget->x;
        fb_plot_ctx.y0 = widget->y;
        fb_plot_ctx.x1 = widget->x + widget->width;
        fb_plot_ctx.y1 = widget->y + widget->height;

        /* clear background */
        if ((widget->bg & 0xFF000000) != 0) {
                /* transparent polygon filling isnt working so fake it */
        plot.fill(fb_plot_ctx.x0, fb_plot_ctx.y0,
                  fb_plot_ctx.x1, fb_plot_ctx.y1,
                  widget->bg);
        }

        /* do our drawing according to type*/

        switch (widget->type) {

        case FB_WIDGET_TYPE_BUTTON:
                /* plot the image */
                plot.bitmap(widget->x,
                            widget->y,
                            widget->width,
                            widget->height,
                            widget->bitmap,
                            0, NULL);
                break;

        case FB_WIDGET_TYPE_WINDOW:
                break;

        case FB_WIDGET_TYPE_TEXT:
        	if (widget->outline) {
                	plot.rectangle(fb_plot_ctx.x0, fb_plot_ctx.y0,
                			fb_plot_ctx.x1 - fb_plot_ctx.x0 - 1,
                			fb_plot_ctx.y1 - fb_plot_ctx.y0 - 1,
                			1, 0x00000000, false, false);
                }
                if (widget->text != NULL) {
                        plot.text(fb_plot_ctx.x0 + 2,
                                  fb_plot_ctx.y0 + 15,
                                  &root_style,
                                  widget->text,
                                  strlen(widget->text),
                                  widget->bg,
                                  widget->fg);
                }
                break;

        default:
                break;
        }

        fb_os_redraw(&fb_plot_ctx);

        /* restore clipping rectangle */
        fb_plot_ctx = saved_plot_ctx;


}

/* inserts widget into head of list and issues a redraw request */
static void
fb_insert_widget(struct fb_widget *widget)
{
        widget->next = widget_list;
        widget_list = widget;

        fb_redraw_widget(widget);
}

/* generic input click focus handler */
static void
fb_change_input_focus(struct fb_widget *widget)
{
        LOG(("Changing input focus to %p", widget));

        if (inputfocus_widget == widget)
                return;

        /* new widget gainig focus */
        inputfocus_widget = widget;

        /* tell it so */
        widget->input(widget, NULL, - 1);
}

static int
fb_widget_url_input(struct fb_widget *widget, struct gui_window *g, int value)
{

        if (url_widget == NULL)
                return 0;

        if (value == -1) {
                /* gain focus */
                if (widget->text == NULL)
                        widget->text = calloc(1,1);
                input_idx = strlen(widget->text);
        } else {
                if (value == '\b') {
                        if (input_idx <= 0)
                                return 0;
                        input_idx--;
                        widget->text[input_idx] = 0;
                } else if (value == '\r') {
                        browser_window_go(g->bw, widget->text, 0, true);
                } else {
                        widget->text = realloc(widget->text, input_idx + 2); /* allow for new character and null */
                        widget->text[input_idx] = value;
                        widget->text[input_idx + 1] = '\0';
                        input_idx++;
                }
                fb_redraw_widget(url_widget);
        }
        return 0;
}

static struct fb_widget *
fb_add_button_widget(int x,
              int y,
              struct bitmap *widget_image,
              fb_widget_mouseclick_t click_rtn)
{
        struct fb_widget *new_widget;
        new_widget = calloc(1, sizeof(struct fb_widget));
        if (new_widget == NULL)
                return NULL;

        new_widget->type = FB_WIDGET_TYPE_BUTTON;
        new_widget->x = x;
        new_widget->y = y;
        new_widget->width = widget_image->width;
        new_widget->height = widget_image->height;
        new_widget->outline = false;

        new_widget->click = click_rtn;

        new_widget->bitmap = widget_image;
        
        fb_insert_widget(new_widget);

        return new_widget;
}

static struct fb_widget *
fb_add_text_widget(int x, int y, int width, int height, colour bg, bool outline, fb_widget_input_t input_rtn)
{
        struct fb_widget *new_widget;
        new_widget = calloc(1, sizeof(struct fb_widget));
        if (new_widget == NULL)
                return NULL;

        new_widget->type = FB_WIDGET_TYPE_TEXT;
        new_widget->x = x;
        new_widget->y = y;
        new_widget->width = width;
        new_widget->height = height;
        new_widget->bg = bg;
        new_widget->fg = FB_COLOUR_BLACK;
        new_widget->outline = outline;

        new_widget->input = input_rtn;

        fb_insert_widget(new_widget);

        return new_widget;
}

struct fb_widget *
fb_add_window_widget(struct gui_window *g,
                     colour bg,
                     fb_widget_mouseclick_t click_rtn,
                     fb_widget_input_t input_rtn)
{
        struct fb_widget *new_widget;
        new_widget = calloc(1, sizeof(struct fb_widget));
        if (new_widget == NULL)
                return NULL;

        new_widget->type = FB_WIDGET_TYPE_WINDOW;
        new_widget->x = g->x;
        new_widget->y = g->y;
        new_widget->width = g->width;
        new_widget->height = g->height;
        new_widget->bg = bg;
        new_widget->outline = false;

        new_widget->click = click_rtn;
        new_widget->input = input_rtn;

        new_widget->g = g;

        fb_insert_widget(new_widget);

        return new_widget;
}


/* left icon click routine */
static int
fb_widget_leftarrow_click(struct gui_window *g, browser_mouse_state st, int x, int y)
{
        if (history_back_available(g->bw->history))
                history_back(g->bw, g->bw->history);
        return 0;

}

/* right arrow icon click routine */
static int
fb_widget_rightarrow_click(struct gui_window *g, browser_mouse_state st, int x, int y)
{
        if (history_forward_available(g->bw->history))
                history_forward(g->bw, g->bw->history);
        return 0;

}

/* reload icon click routine */
static int
fb_widget_reload_click(struct gui_window *g, browser_mouse_state st, int x, int y)
{
        browser_window_reload(g->bw, true);
        return 0;
}

/* stop icon click routine */
static int
fb_widget_stop_click(struct gui_window *g, browser_mouse_state st, int x, int y)
{
	browser_window_stop(g->bw);
        return 0;
}


/* update status widget */
void fb_rootwindow_status(const char* text)
{
        if (status_widget == NULL)
                return;

        if (status_widget->text != NULL)
                free(status_widget->text);

        status_widget->text = strdup(text);

        fb_redraw_widget(status_widget);
}

/* update url widget */
void fb_rootwindow_url(const char* text)
{
        if (url_widget == NULL)
                return;

        if (url_widget->text != NULL)
                free(url_widget->text);

        url_widget->text = strdup(text);
        input_idx = strlen(text);

        fb_redraw_widget(url_widget);
}


void fb_rootwindow_create(framebuffer_t *fb)
{
        struct fb_widget *newwidget;

        /* empty widget list */
        widget_list = NULL;

        /* no widget yet has input */
        inputfocus_widget = NULL;

        /* setup root css style (for text etc.) */
        root_style.font_size.value.length.unit = CSS_UNIT_PX;
        root_style.font_size.value.length.value = 14;

        /* underlying root window, cannot take input and lowest in stack */
        rootwindow = calloc(1, sizeof(struct gui_window));
        rootwindow->x = 0;
        rootwindow->y = 0;
        rootwindow->width = fb->width;
        rootwindow->height = fb->height;
        fb_add_window_widget(rootwindow, FB_FRAME_COLOUR, NULL, NULL);

        /* back button */
        newwidget = fb_add_button_widget(5, 2,
                                         &left_arrow,
                                         fb_widget_leftarrow_click);

        /* forward button */
        newwidget = fb_add_button_widget(newwidget->x + newwidget->width + 5,
                                         2,
                                         &right_arrow,
                                         fb_widget_rightarrow_click);

        /* reload button */
        newwidget = fb_add_button_widget(newwidget->x + newwidget->width + 5,
                                         2,
                                         &stop_image,
                                         fb_widget_stop_click);

        /* reload button */
        newwidget = fb_add_button_widget(newwidget->x + newwidget->width + 5,
                                         2,
                                         &reload,
                                         fb_widget_reload_click);

        /* url widget */
        url_widget = fb_add_text_widget(newwidget->x + newwidget->width + 5, 3,
                                        fb->width -
                                        (newwidget->x + newwidget->width + 5) -
                                        (25 + 10),
                                        22,
                                        FB_COLOUR_WHITE, true,
                                        fb_widget_url_input);


        /* add status area widget, width of framebuffer less some for
         * scrollbar
         */
        status_widget = fb_add_text_widget(0, fb->height - 20,
                                           fb->width - 200, 20,
                                           FB_FRAME_COLOUR, false,
                                           NULL);

}

void
fb_rootwindow_input(struct gui_window *g, int value)
{
        if ((inputfocus_widget != NULL) &&
            (inputfocus_widget->input != NULL)) {
                inputfocus_widget->input(inputfocus_widget, g, value);
        }
}

void
fb_rootwindow_click(struct gui_window *g, browser_mouse_state st, int x, int y)
{
        struct fb_widget *widget;

        widget = widget_list;
        while (widget != NULL) {
                if ((x > widget->x) &&
                    (y > widget->y) &&
                    (x < widget->x + widget->width) &&
                    (y < widget->y + widget->height)) {
                        if (widget->click != NULL) {
                                widget->click(g, st,
                                              x - widget->x, y - widget->y);
                        }

                        if (widget->input != NULL) {
                                fb_change_input_focus(widget);
                        }

                        break;
                }
                widget = widget->next;
        }
}


void
fb_rootwindow_move(framebuffer_t *fb,
                   struct gui_window *g,
                   int x,
                   int y,
                   bool relative)
{
        struct fb_widget *widget;

        if (relative) {
                x += fb_cursor_x(fb);
                y += fb_cursor_y(fb);
        }

        fb_cursor_move(fb, x, y);

        widget = widget_list;
        while (widget != NULL) {
                if ((x > widget->x) &&
                    (y > widget->y) &&
                    (x < widget->x + widget->width) &&
                    (y < widget->y + widget->height)) {

                        if (widget->g == g) {
                                browser_window_mouse_track(g->bw, 0, x - widget->x + g->scrollx, y - widget->y + g->scrolly);
                                break;
                        }
                }
                widget = widget->next;
        }

}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
