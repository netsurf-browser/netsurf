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

#include "utils/log.h"
#include "css/css.h"
#include "desktop/browser.h"
#include "desktop/plotters.h"

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_tk.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_bitmap.h"
#include "framebuffer/fb_cursor.h"
#include "framebuffer/fb_image_data.h"
#include "framebuffer/fb_frontend.h"

static struct css_style root_style;

enum fbtk_widgettype_e {
        FB_WIDGET_TYPE_ROOT = 0,
        FB_WIDGET_TYPE_WINDOW,
        FB_WIDGET_TYPE_BITMAP,
        FB_WIDGET_TYPE_FILL,
        FB_WIDGET_TYPE_TEXT,
        FB_WIDGET_TYPE_HSCROLL,
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
                        framebuffer_t *fb;
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
fbtk_redraw_widget(fbtk_widget_t *widget)
{
        bbox_t saved_plot_ctx;

        //LOG(("widget %p type %d", widget, widget->type));

        /* set the clipping rectangle to the widget area */
        saved_plot_ctx = fb_plot_ctx;

        fb_plot_ctx.x0 = widget->x;
        fb_plot_ctx.y0 = widget->y;
        fb_plot_ctx.x1 = widget->x + widget->width;
        fb_plot_ctx.y1 = widget->y + widget->height;

        /* do our drawing according to type */
        widget->redraw(widget, widget->redrawpw);

        widget->redraw_required = false;
        //LOG(("OS redrawing %d,%d %d,%d", fb_plot_ctx.x0, fb_plot_ctx.y0, fb_plot_ctx.x1, fb_plot_ctx.y1));
        fb_os_redraw(&fb_plot_ctx);

        /* restore clipping rectangle */
        fb_plot_ctx = saved_plot_ctx;
        //LOG(("Redraw Complete"));
}

/*************** redraw widgets **************/

static int
fb_redraw_fill(fbtk_widget_t *widget, void *pw)
{
        /* clear background */
        if ((widget->bg & 0xFF000000) != 0) {
                /* transparent polygon filling isnt working so fake it */
                plot.fill(fb_plot_ctx.x0, fb_plot_ctx.y0, 
                          fb_plot_ctx.x1, fb_plot_ctx.y1,
                          widget->bg);
        }
        return 0;
}

static int
fb_redraw_hscroll(fbtk_widget_t *widget, void *pw)
{
        int hscroll;
        int hpos;

        plot.fill(fb_plot_ctx.x0, fb_plot_ctx.y0, 
                  fb_plot_ctx.x1, fb_plot_ctx.y1,
                  widget->bg);

        plot.rectangle(fb_plot_ctx.x0, 
                       fb_plot_ctx.y0 + 2,
                       fb_plot_ctx.x1 - fb_plot_ctx.x0 - 1,
                       fb_plot_ctx.y1 - fb_plot_ctx.y0 - 5,
                       1, 0x00000000, false, false);

        hscroll = ((widget->width - 4) * widget->u.scroll.pct) / 100 ;
        hpos = ((widget->width - 4) * widget->u.scroll.pos) / 100 ;

        LOG(("hscroll %d",hscroll));

        plot.fill(fb_plot_ctx.x0 + 3 + hpos, 
                  fb_plot_ctx.y0 + 5, 
                  fb_plot_ctx.x0 + hscroll + hpos, 
                  fb_plot_ctx.y0 + widget->height - 5,
                  widget->fg);

        return 0;
}

static int
fb_redraw_bitmap(fbtk_widget_t *widget, void *pw)
{
        /* clear background */
        if ((widget->bg & 0xFF000000) != 0) {
                /* transparent polygon filling isnt working so fake it */
                plot.fill(fb_plot_ctx.x0, fb_plot_ctx.y0, 
                          fb_plot_ctx.x1, fb_plot_ctx.y1,
                          widget->bg);
        }

        /* plot the image */
        plot.bitmap(widget->x, widget->y, widget->width, widget->height,
                    widget->u.bitmap.bitmap, 0, NULL);
        return 0;
}

static int
fbtk_window_default_redraw(fbtk_widget_t *window, void *pw)
{
        fbtk_widget_list_t *lent;
        fbtk_widget_t *widget;
        int res = 0;

        if (!window->redraw)
                return res;

        /* get the list of widgets */
        lent = window->u.window.widgets;

        while (lent != NULL) {
                widget = lent->widget;

                if ((widget->redraw != NULL) && 
                    (widget->redraw_required)) {
                        fbtk_redraw_widget(widget);
                        
                }
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
fbtk_window_default_click(fbtk_widget_t *window, browser_mouse_state st, int x, int y, void *pw) 
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
                                                    st,
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
fb_redraw_text(fbtk_widget_t *widget, void *pw)
{
        /* clear background */
        if ((widget->bg & 0xFF000000) != 0) {
                /* transparent polygon filling isnt working so fake it */
                plot.fill(fb_plot_ctx.x0, fb_plot_ctx.y0, 
                          fb_plot_ctx.x1, fb_plot_ctx.y1,
                          widget->bg);
        }

        if (widget->u.text.outline) {
                plot.rectangle(fb_plot_ctx.x0, fb_plot_ctx.y0,
                               fb_plot_ctx.x1 - fb_plot_ctx.x0 - 1,
                               fb_plot_ctx.y1 - fb_plot_ctx.y0 - 1,
                               1, 0x00000000, false, false);
        }
        if (widget->u.text.text != NULL) {
                plot.text(fb_plot_ctx.x0 + 3,
                          fb_plot_ctx.y0 + 17,
                          &root_style,
                          widget->u.text.text,
                          strlen(widget->u.text.text),
                          widget->bg,
                          widget->fg);
        }
        return 0;
}




static int
text_input(fbtk_widget_t *widget, int value, void *pw)
{

        switch (value) {
        case -1:
                /* gain focus */
                if (widget->u.text.text == NULL)
                        widget->u.text.text = calloc(1,1);
                widget->u.text.idx = strlen(widget->u.text.text);
                break;

        case '\b':
                if (widget->u.text.idx <= 0)
                        break;
                widget->u.text.idx--;
                widget->u.text.text[widget->u.text.idx] = 0;
                break;
            
        case '\r':
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
        if ((widget == NULL) || (widget->type != FB_WIDGET_TYPE_HSCROLL)) 
                return;

        widget->u.scroll.pct = pct;

        fbtk_request_redraw(widget);
}

void
fbtk_set_scroll_pos(fbtk_widget_t *widget, int pos)
{
        if ((widget == NULL) || (widget->type != FB_WIDGET_TYPE_HSCROLL)) 
                return;

        widget->u.scroll.pos = pos;

        fbtk_request_redraw(widget);
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
fbtk_input(fbtk_widget_t *widget, uint32_t ucs4)
{
        fbtk_widget_t *input;                        

        widget = get_root_widget(widget);
        
        /* obtain widget with input focus */
        input = widget->u.root.input;
        if (input == NULL)
                return;

        if (input->input == NULL)
                return;

        input->input(input, ucs4, input->inputpw);
}

void
fbtk_click(fbtk_widget_t *widget, browser_mouse_state st)
{
        fbtk_widget_t *root;
        fbtk_widget_t *window;
        int x;
        int y;

        /* ensure we have the root widget */
        root = get_root_widget(widget);

        x = fb_cursor_x(root->u.root.fb);
        y = fb_cursor_y(root->u.root.fb);

        /* get the root window */
        window = root->u.root.rootw;

        if (window->click != NULL)
                window->click(window, st, x, y, window->clickpw);
}



void
fbtk_move_pointer(fbtk_widget_t *widget, int x, int y, bool relative)
{
        fbtk_widget_t *root;
        fbtk_widget_t *window;

        /* ensure we have the root widget */
        root = get_root_widget(widget);

        if (relative) {
                x += fb_cursor_x(root->u.root.fb);
                y += fb_cursor_y(root->u.root.fb);
        }

        root->redraw_required = true;

        fb_cursor_move(root->u.root.fb, x, y);

        /* get the root window */
        window = root->u.root.rootw;

        if (window->move != NULL)
                window->move(window, x, y,window->movepw);

}

int
fbtk_redraw(fbtk_widget_t *widget)
{
        fbtk_widget_t *root;
        fbtk_widget_t *window;

        /* ensure we have the root widget */
        root = get_root_widget(widget);

        if (!root->redraw_required)
                return 0;

        /* get the root window */
        window = root->u.root.rootw;

        fb_cursor_clear(root->u.root.fb);

        if (window->redraw != NULL) 
                fbtk_redraw_widget(window);

        root->redraw_required = false;

        fb_cursor_plot(root->u.root.fb);

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


/* Initialise toolkit for use */
fbtk_widget_t *
fbtk_init(framebuffer_t *fb)
{
        fbtk_widget_t *root = new_widget(FB_WIDGET_TYPE_ROOT);

        root->u.root.fb = fb;
        root->x = 0;
        root->y = 0;
        root->width = framebuffer->width;
        root->height = framebuffer->height;
        root->u.root.rootw = fbtk_create_window(root, 0, 0, 0, 0);

        root_style.font_size.value.length.unit = CSS_UNIT_PX;
        root_style.font_size.value.length.value = 14;

        return root;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
