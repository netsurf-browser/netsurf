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
#include "desktop/textinput.h"

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_tk.h"
#include "framebuffer/fb_bitmap.h"
#include "framebuffer/fb_frontend.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_schedule.h"
#include "framebuffer/fb_cursor.h"
#include "framebuffer/fb_findfile.h"
#include "framebuffer/fb_image_data.h"
#include "framebuffer/fb_font.h"

#include "content/urldb.h"
#include "desktop/history_core.h"
#include "content/fetch.h"

char *default_stylesheet_url;
char *adblock_stylesheet_url;
char *options_file_location;

fbtk_widget_t *fbtk;

struct gui_window *input_window = NULL;
struct gui_window *search_current_window;
struct gui_window *window_list = NULL;

bool redraws_pending = false;

framebuffer_t *framebuffer;

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

/* private data for browser user widget */ 
struct browser_widget_s {
	struct browser_window *bw; /**< The browser window connected to this gui window */
	int scrollx, scrolly; /**< scroll offsets. */

	/* Pending window redraw state. */
	bool redraw_required; /**< flag indicating the foreground loop
			       * needs to redraw the browser widget.
			       */
	bbox_t redraw_box; /**< Area requiring redraw. */
	bool pan_required; /**< flag indicating the foreground loop
			       * needs to pan the window.
			       */
	int panx, pany; /**< Panning required. */
};


/* queue a redraw operation, co-ordinates are relative to the window */
static void
fb_queue_redraw(struct fbtk_widget_s *widget, int x0, int y0, int x1, int y1)
{
        struct browser_widget_s *bwidget = fbtk_get_userpw(widget);

        bwidget->redraw_box.x0 = MIN(bwidget->redraw_box.x0, x0);
        bwidget->redraw_box.y0 = MIN(bwidget->redraw_box.y0, y0);
        bwidget->redraw_box.x1 = MAX(bwidget->redraw_box.x1, x1);
        bwidget->redraw_box.y1 = MAX(bwidget->redraw_box.y1, y1);

        bwidget->redraw_required = true;

        fbtk_request_redraw(widget);
}

static void fb_pan(fbtk_widget_t *widget, 
                   struct browser_widget_s *bwidget, 
                   struct browser_window *bw)
{
	struct content *c;
        int x;
        int y;
        int width;
        int height;

	c = bw->current_content;

	if ((!c) || (c->locked)) 
                return;

        height = fbtk_get_height(widget);
        width = fbtk_get_width(widget);
        x = fbtk_get_x(widget);
        y = fbtk_get_y(widget);

	/* dont pan off the top */
	if ((bwidget->scrolly + bwidget->pany) < 0)
		bwidget->pany = - bwidget->scrolly;

        /* do not pan off the bottom of the content */
	if ((bwidget->scrolly + bwidget->pany) > (c->height - height))
		bwidget->pany = (c->height - height) - bwidget->scrolly;

	/* dont pan off the left */
	if ((bwidget->scrollx + bwidget->panx) < 0)
		bwidget->panx = - bwidget->scrollx;

        /* do not pan off the right of the content */
	if ((bwidget->scrollx + bwidget->panx) > (c->width - width))
		bwidget->panx = (c->width - width) - bwidget->scrollx;

	LOG(("panning %d, %d",bwidget->panx, bwidget->pany));

	/* pump up the volume. dance, dance! lets do it */
	if (bwidget->pany < 0) {
		/* we cannot pan more than a window height at a time */
		if (bwidget->pany < -height)
			bwidget->pany = -height;

		LOG(("panning up %d", bwidget->pany));

		fb_plotters_move_block(x, y, 
                                       width, height + bwidget->pany,
                                       x, y - bwidget->pany);
		bwidget->scrolly += bwidget->pany;
		fb_queue_redraw(widget, 0, 0, width, - bwidget->pany);
	}

	if (bwidget->pany > 0) {
		/* we cannot pan more than a window height at a time */
		if (bwidget->pany > height)
			bwidget->pany = height;

		LOG(("panning down %d", bwidget->pany));

		fb_plotters_move_block(x, y + bwidget->pany,
                                       width, height - bwidget->pany,
                                       x, y);
		bwidget->scrolly += bwidget->pany;
		fb_queue_redraw(widget, 0, height - bwidget->pany, width, height);
	}

	if (bwidget->panx < 0) {
		/* we cannot pan more than a window width at a time */
		if (bwidget->panx < -width)
			bwidget->panx = -width;

		LOG(("panning left %d", bwidget->panx));

		fb_plotters_move_block(x, y, 
                                       width + bwidget->panx, height ,
                                       x - bwidget->panx, y );
		bwidget->scrollx += bwidget->panx;
		fb_queue_redraw(widget, 0, 0, -bwidget->panx, height);
	}

	if (bwidget->panx > 0) {
		/* we cannot pan more than a window width at a time */
		if (bwidget->panx > width)
			bwidget->panx = width;

		LOG(("panning right %d", bwidget->panx));

		fb_plotters_move_block(x + bwidget->panx, y,
                                       width - bwidget->panx, height,
                                       x, y);
		bwidget->scrollx += bwidget->panx;
		fb_queue_redraw(widget, width - bwidget->panx, 0, width, height);
	}


	bwidget->pan_required = false;
	bwidget->panx = 0;
	bwidget->pany = 0;
}

static void fb_redraw(fbtk_widget_t *widget, 
                      struct browser_widget_s *bwidget, 
                      struct browser_window *bw)
{
	struct content *c;
        int x;
        int y;
        int width;
        int height;

	c = bw->current_content;

	if ((!c) || (c->locked)) 
                return;

        height = fbtk_get_height(widget);
        width = fbtk_get_width(widget);
        x = fbtk_get_x(widget);
        y = fbtk_get_y(widget);

        /* adjust clipping co-ordinates according to window location */
        bwidget->redraw_box.y0 += y;
        bwidget->redraw_box.y1 += y;
        bwidget->redraw_box.x0 += x;
        bwidget->redraw_box.x1 += x;

        /* redraw bounding box is relative to window */
        content_redraw(c,
                       x - bwidget->scrollx, y - bwidget->scrolly,
                       width, height,
                       bwidget->redraw_box.x0, bwidget->redraw_box.y0,
                       bwidget->redraw_box.x1, bwidget->redraw_box.y1,
                       bw->scale, 0xFFFFFF);

        fb_os_redraw(&bwidget->redraw_box);

        bwidget->redraw_box.y0 = bwidget->redraw_box.x0 = INT_MAX;
        bwidget->redraw_box.y1 = bwidget->redraw_box.x1 = -(INT_MAX);
        bwidget->redraw_required = false;
}

static int
fb_browser_window_redraw(fbtk_widget_t *widget, void *pw)
{
        struct gui_window *gw = pw;
        struct browser_widget_s *bwidget;

        bwidget = fbtk_get_userpw(widget);

        if (bwidget->pan_required) {
                int pos;
                fb_pan(widget, bwidget, gw->bw);
                pos = (bwidget->scrollx * 100) / gw->bw->current_content->width;;
                fbtk_set_scroll_pos(gw->hscroll, pos);

        }

        if (bwidget->redraw_required) {
                fb_redraw(widget, bwidget, gw->bw);
        }
        return 0;
}

#ifdef WITH_HUBBUB
static void *myrealloc(void *ptr, size_t len, void *pw)
{
	return realloc(ptr, len);
}
#endif

void gui_init(int argc, char** argv)
{
	char buf[PATH_MAX];

        LOG(("argc %d, argv %p", argc, argv));

#ifdef WITH_HUBBUB
	fb_find_resource(buf, "Aliases", "./framebuffer/res/Aliases");
	LOG(("Using '%s' as Aliases file", buf));
	if (hubbub_initialise(buf, myrealloc, NULL) !=
			HUBBUB_OK)
		die("Unable to initialise HTML parsing library.\n");
#endif

        /* load browser messages */
	fb_find_resource(buf, "messages", "./framebuffer/res/messages");
	LOG(("Using '%s' as Messages file", buf));
	messages_load(buf);
        
        /* load browser options */
	fb_find_resource(buf, "Options", "~/.netsurf/Options");
	LOG(("Using '%s' as Preferences file", buf));
	options_file_location = strdup(buf);
	options_read(buf);

	/* set up stylesheet urls */
	fb_find_resource(buf, "default.css", "./framebuffer/res/default.css");
	default_stylesheet_url = path_to_url(buf);
	LOG(("Using '%s' as Default CSS URL", default_stylesheet_url));

        framebuffer = fb_os_init(argc, argv);

        fb_os_option_override();

        option_target_blank = false;

        switch (framebuffer->bpp) {
                /*        case 1:
                plot = framebuffer_1bpp_plot;
                break;
                */
        case 8:
                plot = framebuffer_8bpp_plot;
                break;

        case 16:
                plot = framebuffer_16bpp_plot;
                break;

        case 32:
                plot = framebuffer_32bpp_plot;
                break;

        default:
                LOG(("Unsupported bit depth (%d)", framebuffer->bpp));
                die("Unsupported bit depth");
        }

        framebuffer->cursor = fb_cursor_init(framebuffer, &pointer_image);

        if (fb_font_init() == false)
                die("Unable to initialise the font system");

        fbtk = fbtk_init(framebuffer);
}

void gui_init2(int argc, char** argv)
{
	struct browser_window *bw;
	const char *addr = NETSURF_HOMEPAGE;

        LOG(("argc %d, argv %p", argc, argv));

        if (option_homepage_url != NULL && option_homepage_url[0] != '\0')
                addr = option_homepage_url;

	if (argc > 1) addr = argv[1];

        LOG(("calling browser_window_create"));
	bw = browser_window_create(addr, 0, 0, true, false);
}


void gui_multitask(void)
{
    //    LOG(("gui_multitask"));
}


void gui_poll(bool active)
{
    //    LOG(("enter fetch_poll"));
    if (active)
        fetch_poll();

    active = schedule_run() | active | redraws_pending;

    fb_os_input(fbtk, active);

    fbtk_redraw(fbtk);

}

void gui_quit(void)
{
        LOG(("gui_quit"));
        fb_os_quit(framebuffer);
#ifdef WITH_HUBBUB
	/* We don't care if this fails as we're about to die, anyway */
	hubbub_finalise(myrealloc, NULL);
#endif
}

/* called back when click in browser window */
static int 
fb_browser_window_click(fbtk_widget_t *widget, 
                        browser_mouse_state st, 
                        int x, int y, 
                        void *pw)
{
        struct browser_window *bw = pw;
        struct browser_widget_s *bwidget = fbtk_get_userpw(widget);

        LOG(("browser window clicked at %d,%d",x,y));
        browser_window_mouse_click(bw,
                                   st,
                                   x + bwidget->scrollx, 
                                   y + bwidget->scrolly);        
        return 0;
}

/* called back when movement in browser window */
static int 
fb_browser_window_move(fbtk_widget_t *widget, 
                        int x, int y, 
                        void *pw)
{
        struct browser_window *bw = pw;
        struct browser_widget_s *bwidget = fbtk_get_userpw(widget);

        browser_window_mouse_track(bw, 
                                   0, 
                                   x + bwidget->scrollx, 
                                   y + bwidget->scrolly);

        return 0;
}

static int
fb_browser_window_input(fbtk_widget_t *widget, int value, void *pw)
{
        struct gui_window *gw = pw;
        int res = 0;
        LOG(("got value %d",value));
        switch (value) {

        case KEY_PAGE_UP:
                fb_window_scroll(gw, 0, -fbtk_get_height(gw->browser));
                break;

        case KEY_PAGE_DOWN:
                fb_window_scroll(gw, 0, fbtk_get_height(gw->browser));
                break;

        case KEY_RIGHT:
                fb_window_scroll(gw, 100, 0);
                break;

        case KEY_LEFT:
                fb_window_scroll(gw, -100, 0);
                break;

        case KEY_UP:
                fb_window_scroll(gw, 0, -100);
                break;

        case KEY_DOWN:
                fb_window_scroll(gw, 0, 100);
                break;

        default:
                res = browser_window_key_press(gw->bw, value);
                break;
        }

        return 0;
}

/* left icon click routine */
static int
fb_leftarrow_click(fbtk_widget_t *widget, 
                   browser_mouse_state st, 
                   int x, int y, void *pw)
{
        struct browser_window *bw = pw;

        if (st == BROWSER_MOUSE_CLICK_1) {
                if (history_back_available(bw->history))
                        history_back(bw, bw->history);
        }
        return 0;

}

/* right arrow icon click routine */
static int
fb_rightarrow_click(fbtk_widget_t *widget, browser_mouse_state st, int x, int y, void *pw)
{
        struct browser_window *bw = pw;

        if (st == BROWSER_MOUSE_CLICK_1) {
                if (history_forward_available(bw->history))
                        history_forward(bw, bw->history);
        }
        return 0;

}

/* reload icon click routine */
static int
fb_reload_click(fbtk_widget_t *widget, browser_mouse_state st, int x, int y, void *pw)
{
        struct browser_window *bw = pw;
        browser_window_reload(bw, true);
        return 0;
}

/* stop icon click routine */
static int
fb_stop_click(fbtk_widget_t *widget, browser_mouse_state st, int x, int y, void *pw)
{
        struct browser_window *bw = pw;
	browser_window_stop(bw);
        return 0;
}

/* left scroll icon click routine */
static int
fb_scrolll_click(fbtk_widget_t *widget, browser_mouse_state st, int x, int y, void *pw)
{
        fbtk_input(widget, KEY_LEFT);
        return 0;
}

static int
fb_scrollr_click(fbtk_widget_t *widget, browser_mouse_state st, int x, int y, void *pw)
{
        fbtk_input(widget, KEY_RIGHT);
        return 0;
}

static int
fb_url_enter(void *pw, char *text)
{
        struct browser_window *bw = pw;
        browser_window_go(bw, text, 0, true);
        return 0;
}

static int 
fb_url_move(fbtk_widget_t *widget, 
            int x, int y, 
            void *pw)
{
        fb_cursor_set(framebuffer->cursor, &caret_image);
        return 0;
}

static int 
set_ptr_default_move(fbtk_widget_t *widget, 
            int x, int y, 
            void *pw)
{
        fb_cursor_set(framebuffer->cursor, &pointer_image);
        return 0;
}

static int 
set_ptr_hand_move(fbtk_widget_t *widget, 
            int x, int y, 
            void *pw)
{
        fb_cursor_set(framebuffer->cursor, &hand_image);
        return 0;
}

struct gui_window *
gui_create_browser_window(struct browser_window *bw,
                          struct browser_window *clone,
                          bool new_tab)
{
        struct gui_window *gw;
        struct browser_widget_s *browser_widget;
        fbtk_widget_t *widget;
        int top = 0;
        int bot = 0;

        gw = calloc(1, sizeof(struct gui_window));

        if (gw == NULL)
                return NULL;

        /* seems we need to associate the gui window with the underlying
         * browser window 
         */
        gw->bw = bw;


        switch(bw->browser_window_type) {
	case BROWSER_WINDOW_NORMAL:
                gw->window = fbtk_create_window(fbtk, 0, 0, 0, 0); 

                top = 30;
                bot = -50;

                LOG(("Normal window"));

                /* fill toolbar background */
                widget = fbtk_create_fill(gw->window, 0, 0, 0, 30, FB_FRAME_COLOUR);
                fbtk_set_handler_move(widget, set_ptr_default_move, bw);

                /* back button */
                widget = fbtk_create_button(gw->window, 5, 2, FB_FRAME_COLOUR, &left_arrow, fb_leftarrow_click, bw);
                fbtk_set_handler_move(widget, set_ptr_hand_move, bw);

                /* forward button */
                widget = fbtk_create_button(gw->window, 35, 2, FB_FRAME_COLOUR, &right_arrow, fb_rightarrow_click, bw);
                fbtk_set_handler_move(widget, set_ptr_hand_move, bw);

                /* reload button */
                widget = fbtk_create_button(gw->window, 65, 2, FB_FRAME_COLOUR, &stop_image, fb_stop_click, bw);
                fbtk_set_handler_move(widget, set_ptr_hand_move, bw);

                /* reload button */
                widget = fbtk_create_button(gw->window, 95, 2, FB_FRAME_COLOUR, &reload, fb_reload_click, bw);
                fbtk_set_handler_move(widget, set_ptr_hand_move, bw);

                /* url widget */
                gw->url = fbtk_create_writable_text(gw->window, 
                                                    125 , 3, 
                                                    fbtk_get_width(gw->window) - 160, 24, 
                                                    FB_COLOUR_WHITE, 
                                                    FB_COLOUR_BLACK, 
                                                    true, 
                                                    fb_url_enter, 
                                                    bw);
                fbtk_set_handler_move(gw->url, fb_url_move, bw);

                gw->throbber = fbtk_create_bitmap(gw->window, 
                                                  130 + fbtk_get_width(gw->url),
                                                  3,
                                                  FB_FRAME_COLOUR,
                                                  &throbber0);



                /* add status area widget, width of framebuffer less some for
                 * scrollbar
                 */
                gw->status = fbtk_create_text(gw->window, 
                                              0 , fbtk_get_height(gw->window) - 20, 
                                              fbtk_get_width(gw->window) - 200, 20, 
                                              FB_FRAME_COLOUR, FB_COLOUR_BLACK, 
                                              false);

                fbtk_set_handler_move(gw->status, set_ptr_default_move, bw);


                fbtk_create_button(gw->window, fbtk_get_width(gw->window) - 200, fbtk_get_height(gw->window) - 20, FB_FRAME_COLOUR, &scrolll, fb_scrolll_click, bw);
                fbtk_create_button(gw->window, fbtk_get_width(gw->window) - 20, fbtk_get_height(gw->window) - 20, FB_FRAME_COLOUR, &scrollr, fb_scrollr_click, bw);

                gw->hscroll = fbtk_create_hscroll(gw->window, 
                                                  fbtk_get_width(gw->window) - 180, 
                                                  fbtk_get_height(gw->window) - 20, 
                                                  160, 
                                                  20, 
                                                  FB_COLOUR_BLACK, 
                                                  FB_FRAME_COLOUR);

                break;

        case BROWSER_WINDOW_FRAME:
                gw->window = fbtk_create_window(bw->parent->window->window, 0, 0, 0, 0); 
                LOG(("create frame"));
                break;

        default:
                gw->window = fbtk_create_window(bw->parent->window->window, 0, 0, 0, 0); 
                LOG(("unhandled type"));

        }

        browser_widget = calloc(1, sizeof(struct browser_widget_s));
        
        gw->browser = fbtk_create_user(gw->window, 0, top, 0, bot, browser_widget); 

        fbtk_set_handler_click(gw->browser, fb_browser_window_click, bw);
        fbtk_set_handler_input(gw->browser, fb_browser_window_input, gw);
        fbtk_set_handler_redraw(gw->browser, fb_browser_window_redraw, gw);
        fbtk_set_handler_move(gw->browser, fb_browser_window_move, bw);

        return gw;
}

void gui_window_destroy(struct gui_window *gw)
{
        fbtk_destroy_widget(gw->window);

        free(gw);


}

void gui_window_set_title(struct gui_window *g, const char *title)
{
        LOG(("%p, %s", g, title));
}



void fb_window_scroll(struct gui_window *g, int x, int y)
{
        struct browser_widget_s *bwidget = fbtk_get_userpw(g->browser);

        bwidget->panx += x;
        bwidget->pany += y;
        bwidget->pan_required = true;

        fbtk_request_redraw(g->browser);
}

void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
        fb_queue_redraw(g->browser, x0, y0, x1, y1);
}

void gui_window_redraw_window(struct gui_window *g)
{
        fb_queue_redraw(g->browser, 0, 0, fbtk_get_width(g->browser),fbtk_get_height(g->browser) );
}

void gui_window_update_box(struct gui_window *g,
		const union content_msg_data *data)
{
        fb_queue_redraw(g->browser, 
                        data->redraw.x, 
                        data->redraw.y,
                        data->redraw.x + data->redraw.width, 
                        data->redraw.y + data->redraw.height);
}

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
        struct browser_widget_s *bwidget = fbtk_get_userpw(g->browser);

        *sx = bwidget->scrollx;
        *sy = bwidget->scrolly;

        return true;
}

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
        struct browser_widget_s *bwidget = fbtk_get_userpw(g->browser);
        LOG(("scroll %d",sx));
        bwidget->panx = sx;
        bwidget->pany = sy;
        bwidget->pan_required = true;

        fbtk_request_redraw(g->browser);
}

void gui_window_scroll_visible(struct gui_window *g, int x0, int y0,
		int x1, int y1)
{
        LOG(("%s:(%p, %d, %d, %d, %d)", __func__, g, x0, y0, x1, y1));
}

void gui_window_position_frame(struct gui_window *g, int x0, int y0,
		int x1, int y1)
{
        struct gui_window *parent;
        int px, py;
        int w, h;
        LOG(("%s: %d, %d, %d, %d", g->bw->name, x0, y0, x1, y1));
        parent = g->bw->parent->window;

        if (parent->window == NULL)
                return; /* doesnt have an fbtk widget */

        px = fbtk_get_x(parent->browser) + x0;
        py = fbtk_get_y(parent->browser) + y0;
        w = x1 - x0;
        h = y1 - y0;
        if (w > (fbtk_get_width(parent->browser) - px)) 
            w = fbtk_get_width(parent->browser) - px;

        if (h > (fbtk_get_height(parent->browser) - py)) 
            h = fbtk_get_height(parent->browser) - py;

        fbtk_set_pos_and_size(g->window, px, py , w , h);

        fbtk_request_redraw(parent->browser);

}

void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
		bool scaled)
{
        *width = fbtk_get_width(g->browser);
        *height = fbtk_get_height(g->browser);
}

void gui_window_update_extent(struct gui_window *g)
{
        int pct;

        pct = (fbtk_get_width(g->browser) * 100) / g->bw->current_content->width;
        fbtk_set_scroll(g->hscroll, pct);
}

void gui_window_set_status(struct gui_window *g, const char *text)
{
        fbtk_set_text(g->status, text);
}

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
        switch (shape) {
        case GUI_POINTER_POINT:
                fb_cursor_set(framebuffer->cursor, &hand_image);
                break;

        case GUI_POINTER_CARET:
                fb_cursor_set(framebuffer->cursor, &caret_image);
                break;

        default:
                fb_cursor_set(framebuffer->cursor, &pointer_image);
        }
}

void gui_window_hide_pointer(struct gui_window *g)
{        
}

void gui_window_set_url(struct gui_window *g, const char *url)
{
        fbtk_set_text(g->url, url);
}

static void
throbber_advance(void *pw)
{
        struct gui_window *g = pw;
        struct bitmap *image;

        switch (g->throbber_index) {
        case 0:
                image = &throbber1;
                g->throbber_index = 1;
                break;

        case 1:
                image = &throbber2;
                g->throbber_index = 2;
                break;

        case 2:
                image = &throbber3;
                g->throbber_index = 3;
                break;

        case 3:
                image = &throbber4;
                g->throbber_index = 4;
                break;

        case 4:
                image = &throbber5;
                g->throbber_index = 5;
                break;

        case 5:
                image = &throbber6;
                g->throbber_index = 6;
                break;

        case 6:
                image = &throbber7;
                g->throbber_index = 7;
                break;

        case 7:
                image = &throbber8;
                g->throbber_index = 8;
                break;

        case 8:
                image = &throbber0;
                g->throbber_index = 0;
                break;

	default:
		return;
        }

        if (g->throbber_index >= 0) {
                fbtk_set_bitmap(g->throbber, image);
                schedule(10, throbber_advance, g);
        }
}

void gui_window_start_throbber(struct gui_window *g)
{
        g->throbber_index = 0;
        schedule(10, throbber_advance, g);
}

void gui_window_stop_throbber(struct gui_window *g)
{
        g->throbber_index = -1;
        fbtk_set_bitmap(g->throbber, &throbber0);
}

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
}

void gui_window_remove_caret(struct gui_window *g)
{
}

void gui_window_new_content(struct gui_window *g)
{
}

bool gui_window_scroll_start(struct gui_window *g)
{
	return true;
}

bool gui_window_box_scroll_start(struct gui_window *g,
		int x0, int y0, int x1, int y1)
{
	return true;
}

bool gui_window_frame_resize_start(struct gui_window *g)
{
	LOG(("resize frame\n"));
	return true;
}

void gui_window_save_as_link(struct gui_window *g, struct content *c)
{
}

void gui_window_set_scale(struct gui_window *g, float scale)
{
	LOG(("set scale\n"));
}

struct gui_download_window *gui_download_window_create(const char *url,
		const char *mime_type, struct fetch *fetch,
		unsigned int total_size, struct gui_window *gui)
{
        return NULL;
}

void gui_download_window_data(struct gui_download_window *dw, const char *data,
		unsigned int size)
{
}

void gui_download_window_error(struct gui_download_window *dw,
		const char *error_msg)
{
}

void gui_download_window_done(struct gui_download_window *dw)
{
}

void gui_drag_save_object(gui_save_type type, struct content *c,
		struct gui_window *g)
{
}

void gui_drag_save_selection(struct selection *s, struct gui_window *g)
{
}

void gui_start_selection(struct gui_window *g)
{
}

void gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{
}

bool gui_empty_clipboard(void)
{
        return false;
}

bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
        return false;
}

bool gui_commit_clipboard(void)
{
        return false;
}

bool gui_copy_to_clipboard(struct selection *s)
{
        return false;
}

void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{
}

void gui_launch_url(const char *url)
{
}

bool gui_search_term_highlighted(struct gui_window *g,
		unsigned start_offset, unsigned end_offset,
		unsigned *start_idx, unsigned *end_idx)
{
        return false;
}



void gui_cert_verify(struct browser_window *bw, struct content *c,
		const struct ssl_cert_info *certs, unsigned long num)
{
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
