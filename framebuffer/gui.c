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
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <hubbub/hubbub.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_event.h>

#include "desktop/gui.h"
#include "desktop/plotters.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "desktop/textinput.h"
#include "render/form.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/framebuffer.h"
#include "framebuffer/bitmap.h"
#include "framebuffer/schedule.h"
#include "framebuffer/findfile.h"
#include "framebuffer/image_data.h"
#include "framebuffer/font.h"

#include "content/urldb.h"
#include "desktop/history_core.h"
#include "content/fetch.h"

char *default_stylesheet_url;
char *quirks_stylesheet_url;
char *adblock_stylesheet_url;
char *options_file_location;


fbtk_widget_t *fbtk;

struct gui_window *input_window = NULL;
struct gui_window *search_current_window;
struct gui_window *window_list = NULL;

bool redraws_pending = false;


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

        if (fbtk_clip_to_widget(widget, &bwidget->redraw_box)) {
                bwidget->redraw_required = true;
                fbtk_request_redraw(widget);
        } else {
                bwidget->redraw_box.y0 = bwidget->redraw_box.x0 = INT_MAX;
                bwidget->redraw_box.y1 = bwidget->redraw_box.x1 = -(INT_MAX);
                bwidget->redraw_required = false;
        }
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
	nsfb_bbox_t redraw_box;

	c = bw->current_content;

	if ((!c) || (c->locked))
		return;

	nsfb_t *nsfb = fbtk_get_nsfb(widget);

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
	if (bwidget->pany > height || bwidget->pany < -height ||
			bwidget->panx > width || bwidget->panx < -width) {

		/* pan in any direction by more than viewport size */
		bwidget->scrolly += bwidget->pany;
		bwidget->scrollx += bwidget->panx;
		fb_queue_redraw(widget, 0, 0, width, height);

		/* ensure we don't try to scroll again */
		bwidget->panx = 0;
		bwidget->pany = 0;
	}

	if (bwidget->pany < 0) {
		/* pan up by less then viewport height */
		redraw_box.x0 = x;
		redraw_box.y0 = y - bwidget->pany;
		redraw_box.x1 = redraw_box.x0 + width;
		redraw_box.y1 = redraw_box.y0 + height + bwidget->pany;

		/* move part that remains visible up */
		nsfb_claim(nsfb, &redraw_box);
		nsfb_plot_copy(nsfb,
				x, y,
				width, height + bwidget->pany,
				x, y - bwidget->pany);
		nsfb_release(nsfb, &redraw_box);

		/* redraw newly exposed area */
		bwidget->scrolly += bwidget->pany;
		fb_queue_redraw(widget, 0, 0, width, - bwidget->pany);
	}

	if (bwidget->pany > 0) {
		/* pan down by less then viewport height */
		redraw_box.x0 = x;
		redraw_box.y0 = y;
		redraw_box.x1 = redraw_box.x0 + width;
		redraw_box.y1 = redraw_box.y0 + height - bwidget->pany;

		/* move part that remains visible down */
		nsfb_claim(nsfb, &redraw_box);
		nsfb_plot_copy(nsfb,
				x, y + bwidget->pany,
				width, height - bwidget->pany,
				x, y);
		nsfb_release(nsfb, &redraw_box);

		/* redraw newly exposed area */
		bwidget->scrolly += bwidget->pany;
		fb_queue_redraw(widget, 0, height - bwidget->pany, width,
				height);
	}

	if (bwidget->panx < 0) {
		/* pan left by less then viewport width */
		redraw_box.x0 = x - bwidget->panx;
		redraw_box.y0 = y;
		redraw_box.x1 = redraw_box.x0 + width + bwidget->panx;
		redraw_box.y1 = redraw_box.y0 + height;

		/* move part that remains visible left */
		nsfb_claim(nsfb, &redraw_box);
		nsfb_plot_copy(nsfb,
				x, y,
				width + bwidget->panx, height,
				x - bwidget->panx, y);
		nsfb_release(nsfb, &redraw_box);

		/* redraw newly exposed area */
		bwidget->scrollx += bwidget->panx;
		fb_queue_redraw(widget, 0, 0, -bwidget->panx, height);
	}

	if (bwidget->panx > 0) {
		/* pan right by less then viewport width */
		redraw_box.x0 = x;
		redraw_box.y0 = y;
		redraw_box.x1 = redraw_box.x0 + width - bwidget->panx;
		redraw_box.y1 = redraw_box.y0 + height;

		/* move part that remains visible right */
		nsfb_claim(nsfb, &redraw_box);
		nsfb_plot_copy(nsfb,
				x + bwidget->panx, y,
				width - bwidget->panx, height,
				x, y);
		nsfb_release(nsfb, &redraw_box);

		/* redraw newly exposed area */
		bwidget->scrollx += bwidget->panx;
		fb_queue_redraw(widget, width - bwidget->panx, 0, width,
				height);
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

        LOG(("redraw box %d,%d to %d,%d",bwidget->redraw_box.x0,bwidget->redraw_box.y0, bwidget->redraw_box.x1, bwidget->redraw_box.y1));

        height = fbtk_get_height(widget);
        width = fbtk_get_width(widget);
        x = fbtk_get_x(widget);
        y = fbtk_get_y(widget);

        /* adjust clipping co-ordinates according to window location */
        bwidget->redraw_box.y0 += y;
        bwidget->redraw_box.y1 += y;
        bwidget->redraw_box.x0 += x;
        bwidget->redraw_box.x1 += x;



        nsfb_claim(fbtk_get_nsfb(widget), &bwidget->redraw_box);

        /* redraw bounding box is relative to window */
	current_redraw_browser = bw;
        content_redraw(c,
                       x - bwidget->scrollx, y - bwidget->scrolly,
                       width, height,
                       bwidget->redraw_box.x0, bwidget->redraw_box.y0,
                       bwidget->redraw_box.x1, bwidget->redraw_box.y1,
                       bw->scale, 0xFFFFFF);
	current_redraw_browser = NULL;

        nsfb_release(fbtk_get_nsfb(widget), &bwidget->redraw_box);

        bwidget->redraw_box.y0 = bwidget->redraw_box.x0 = INT_MAX;
        bwidget->redraw_box.y1 = bwidget->redraw_box.x1 = -(INT_MAX);
        bwidget->redraw_required = false;
}

static int
fb_browser_window_redraw(fbtk_widget_t *root, fbtk_widget_t *widget, void *pw)
{
        struct gui_window *gw = pw;
        struct browser_widget_s *bwidget;

        bwidget = fbtk_get_userpw(widget);
        if (bwidget == NULL) {
                LOG(("browser widget from widget %p was null", widget));
                return -1;
        }

        if (bwidget->pan_required) {
                int pos;
                fb_pan(widget, bwidget, gw->bw);
                pos = (bwidget->scrollx * 100) / gw->bw->current_content->width;
                fbtk_set_scroll_pos(gw->hscroll, pos);
                pos = (bwidget->scrolly * 100) / gw->bw->current_content->height;
                fbtk_set_scroll_pos(gw->vscroll, pos);

        }

        if (bwidget->redraw_required) {
                fb_redraw(widget, bwidget, gw->bw);
        }
        return 0;
}

static void *myrealloc(void *ptr, size_t len, void *pw)
{
	return realloc(ptr, len);
}

void gui_init(int argc, char** argv)
{
	char buf[PATH_MAX];
        nsfb_t *nsfb;

        LOG(("argc %d, argv %p", argc, argv));

	fb_find_resource(buf, "Aliases", "./framebuffer/res/Aliases");
	LOG(("Using '%s' as Aliases file", buf));
	if (hubbub_initialise(buf, myrealloc, NULL) != HUBBUB_OK)
		die("Unable to initialise HTML parsing library.\n");

        /* load browser messages */
	fb_find_resource(buf, "messages", "./framebuffer/res/messages");
	LOG(("Using '%s' as Messages file", buf));
	messages_load(buf);

	option_core_select_menu = true;
	
        /* load browser options */
	fb_find_resource(buf, "Choices-fb", "~/.netsurf/Choices-fb");
	LOG(("Using '%s' as Preferences file", buf));
	options_file_location = strdup(buf);
	options_read(buf);

	/* set up stylesheet urls */
	fb_find_resource(buf, "default.css", "./framebuffer/res/default.css");
	default_stylesheet_url = path_to_url(buf);
	LOG(("Using '%s' as Default CSS URL", default_stylesheet_url));

	fb_find_resource(buf, "quirks.css", "./framebuffer/res/quirks.css");
	quirks_stylesheet_url = path_to_url(buf);

        nsfb = framebuffer_initialise(argc, argv);
        if (nsfb == NULL)
                die("Unable to initialise framebuffer");

        framebuffer_set_cursor(&pointer_image);

        if (fb_font_init() == false)
                die("Unable to initialise the font system");

        fbtk = fbtk_init(nsfb);

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
        nsfb_event_t event;
        int timeout = 0;

        active |= schedule_run() | redraws_pending;

        if (!active)
                timeout = -1;

        fbtk_event(fbtk, &event, timeout);

        if ((event.type == NSFB_EVENT_CONTROL) &&
            (event.value.controlcode ==  NSFB_CONTROL_QUIT))
                netsurf_quit = true;

        fbtk_redraw(fbtk);

}

void gui_quit(void)
{
        LOG(("gui_quit"));
        framebuffer_finalise();

	/* We don't care if this fails as we're about to exit, anyway */
	hubbub_finalise(myrealloc, NULL);
}

/* called back when click in browser window */
static int
fb_browser_window_click(fbtk_widget_t *widget,
                        nsfb_event_t *event,
                        int x, int y,
                        void *pw)
{
        struct browser_window *bw = pw;
        struct browser_widget_s *bwidget = fbtk_get_userpw(widget);

        if (event->type != NSFB_EVENT_KEY_DOWN)
                return 0;

        LOG(("browser window clicked at %d,%d",x,y));

        switch (event->type) {
        case NSFB_EVENT_KEY_DOWN:
                switch (event->value.keycode) {
                case NSFB_KEY_MOUSE_1:
                        browser_window_mouse_click(bw,
                                                   BROWSER_MOUSE_PRESS_1,
                                                   x + bwidget->scrollx,
                                                   y + bwidget->scrolly);
                        break;

                case NSFB_KEY_MOUSE_3:
                        browser_window_mouse_click(bw,
                                                   BROWSER_MOUSE_PRESS_2,
                                                   x + bwidget->scrollx,
                                                   y + bwidget->scrolly);
                        break;

                case NSFB_KEY_MOUSE_4:
                        /* scroll up */
                        fb_window_scroll(widget, 0, -100);
                        break;

                case NSFB_KEY_MOUSE_5:
                        /* scroll down */
                        fb_window_scroll(widget, 0, 100);
                        break;

                default:
                        break;

                }

		break;
        case NSFB_EVENT_KEY_UP:
                switch (event->value.keycode) {
                case NSFB_KEY_MOUSE_1:
                        browser_window_mouse_click(bw,
                                                   BROWSER_MOUSE_CLICK_1,
                                                   x + bwidget->scrollx,
                                                   y + bwidget->scrolly);
                        break;

                case NSFB_KEY_MOUSE_3:
                        browser_window_mouse_click(bw,
                                                   BROWSER_MOUSE_CLICK_2,
                                                   x + bwidget->scrollx,
                                                   y + bwidget->scrolly);
                        break;

                default:
                        break;

                }

		break;
        default:
                break;

        }
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
fb_browser_window_input(fbtk_widget_t *widget,
                        nsfb_event_t *event,
                        void *pw)
{
        struct gui_window *gw = pw;
        int res = 0;
        static uint8_t modifier = 0;
        int ucs4 = -1;

        LOG(("got value %d", event->value.keycode));

        switch (event->type) {
        case NSFB_EVENT_KEY_DOWN:
                switch (event->value.keycode) {

                case NSFB_KEY_PAGEUP:
                        if (browser_window_key_press(gw->bw, KEY_PAGE_UP) == false)
                                fb_window_scroll(gw->browser, 0, -fbtk_get_height(gw->browser));
                        break;

                case NSFB_KEY_PAGEDOWN:
                        if (browser_window_key_press(gw->bw, KEY_PAGE_DOWN) == false)
                                fb_window_scroll(gw->browser, 0, fbtk_get_height(gw->browser));
                        break;

                case NSFB_KEY_RIGHT:
                        if (browser_window_key_press(gw->bw, KEY_RIGHT) == false)
                                fb_window_scroll(gw->browser, 100, 0);
                        break;

                case NSFB_KEY_LEFT:
                        if (browser_window_key_press(gw->bw, KEY_LEFT) == false)
                                fb_window_scroll(gw->browser, -100, 0);
                        break;

                case NSFB_KEY_UP:
                        if (browser_window_key_press(gw->bw, KEY_UP) == false)
                                fb_window_scroll(gw->browser, 0, -100);
                        break;

                case NSFB_KEY_DOWN:
                        if (browser_window_key_press(gw->bw, KEY_DOWN) == false)
                                fb_window_scroll(gw->browser, 0, 100);
                        break;

                case NSFB_KEY_RSHIFT:
                        modifier |= 1;
                        break;

                case NSFB_KEY_LSHIFT:
                        modifier |= 1<<1;
                        break;

                default:
                        ucs4 = fbtk_keycode_to_ucs4(event->value.keycode, modifier);
                        if (ucs4 != -1)
                                res = browser_window_key_press(gw->bw, ucs4);
                        break;
                }
                break;

        case NSFB_EVENT_KEY_UP:
                switch (event->value.keycode) {
                case NSFB_KEY_RSHIFT:
                        modifier &= ~1;
                        break;

                case NSFB_KEY_LSHIFT:
                        modifier &= ~(1<<1);
                        break;

                default:
                        break;
                }
                break;

        default:
                break;
        }

        return 0;
}

static void
fb_update_back_forward(struct gui_window *gw)
{
	struct browser_window *bw = gw->bw;

	fbtk_set_bitmap(gw->back,
			(browser_window_back_available(bw)) ?
			&left_arrow : &left_arrow_g);
	fbtk_set_bitmap(gw->forward,
			(browser_window_forward_available(bw)) ?
			&right_arrow : &right_arrow_g);
}

/* left icon click routine */
static int
fb_leftarrow_click(fbtk_widget_t *widget,
                   nsfb_event_t *event,
                   int x, int y, void *pw)
{
        struct gui_window *gw = pw;
        struct browser_window *bw = gw->bw;

        if (history_back_available(bw->history))
                history_back(bw, bw->history);

        fb_update_back_forward(gw);
        return 0;

}

/* right arrow icon click routine */
static int
fb_rightarrow_click(fbtk_widget_t *widget,
                    nsfb_event_t *event,
                    int x, int y,
                    void *pw)
{
        struct gui_window *gw =pw;
        struct browser_window *bw = gw->bw;

        if (history_forward_available(bw->history))
                history_forward(bw, bw->history);

        fb_update_back_forward(gw);
        return 0;

}

/* reload icon click routine */
static int
fb_reload_click(fbtk_widget_t *widget,
                nsfb_event_t *event,
                int x, int y,
                void *pw)
{
        struct browser_window *bw = pw;
        browser_window_reload(bw, true);
        return 0;
}

/* stop icon click routine */
static int
fb_stop_click(fbtk_widget_t *widget,
              nsfb_event_t *event,
              int x, int y,
              void *pw)
{
        struct browser_window *bw = pw;
	browser_window_stop(bw);
        return 0;
}

/* left scroll icon click routine */
static int
fb_scrolll_click(fbtk_widget_t *widget,
                 nsfb_event_t *event,
                 int x, int y, void *pw)
{
        struct gui_window *gw = pw;
        fb_window_scroll(gw->browser, -100, 0);
        return 0;
}

static int
fb_scrollr_click(fbtk_widget_t *widget,
                 nsfb_event_t *event,
                 int x, int y, void *pw)
{
        struct gui_window *gw = pw;
        fb_window_scroll(gw->browser, 100, 0);

        return 0;
}

static int
fb_scrollu_click(fbtk_widget_t *widget,
                 nsfb_event_t *event,
                 int x, int y, void *pw)
{
        struct gui_window *gw = pw;
        fb_window_scroll(gw->browser, 0, -100);

        return 0;
}

static int
fb_scrolld_click(fbtk_widget_t *widget,
                 nsfb_event_t *event,
                 int x, int y, void *pw)
{
        struct gui_window *gw = pw;
        fb_window_scroll(gw->browser, 0, 100);

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
        framebuffer_set_cursor(&caret_image);
        return 0;
}

static int
set_ptr_default_move(fbtk_widget_t *widget,
                     int x, int y,
                     void *pw)
{
        framebuffer_set_cursor(&pointer_image);
        return 0;
}

static int
set_ptr_hand_move(fbtk_widget_t *widget,
                  int x, int y,
                  void *pw)
{
        framebuffer_set_cursor(&hand_image);
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
        int right = 0;

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
                bot = 20;
                right = 18;
                LOG(("Normal window"));

                /* fill toolbar background */
                widget = fbtk_create_fill(gw->window,
                                          0, 0, 0, 30,
                                          FB_FRAME_COLOUR);
                fbtk_set_handler_move(widget, set_ptr_default_move, bw);

                /* back button */
                gw->back = fbtk_create_button(gw->window,
                                              5, 2,
                                              FB_FRAME_COLOUR,
                                              &left_arrow_g,
                                              fb_leftarrow_click,
                                              gw);
                fbtk_set_handler_move(gw->back, set_ptr_hand_move, bw);

                /* forward button */
                gw->forward = fbtk_create_button(gw->window,
                                                 35, 2,
                                                 FB_FRAME_COLOUR,
                                                 &right_arrow_g,
                                                 fb_rightarrow_click,
                                                 gw);
                fbtk_set_handler_move(gw->forward, set_ptr_hand_move, bw);

                /* reload button */
                widget = fbtk_create_button(gw->window,
                                            65, 2,
                                            FB_FRAME_COLOUR,
                                            &stop_image,
                                            fb_stop_click,
                                            bw);
                fbtk_set_handler_move(widget, set_ptr_hand_move, bw);

                /* reload button */
                widget = fbtk_create_button(gw->window,
                                            95, 2,
                                            FB_FRAME_COLOUR,
                                            &reload,
                                            fb_reload_click,
                                            bw);
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
                                              0 ,
                                              fbtk_get_height(gw->window) - bot,
                                              fbtk_get_width(gw->window) - 200 - right,
                                              bot,
                                              FB_FRAME_COLOUR, FB_COLOUR_BLACK,
                                              false);

                fbtk_set_handler_move(gw->status, set_ptr_default_move, bw);

                /* horizontal scrollbar */
                fbtk_create_button(gw->window,
                                   fbtk_get_width(gw->window) - 200 - right,
                                   fbtk_get_height(gw->window) - bot,
                                   FB_FRAME_COLOUR,
                                   &scrolll,
                                   fb_scrolll_click,
                                   gw);

                fbtk_create_button(gw->window,
                                   fbtk_get_width(gw->window) - 20 - right,
                                   fbtk_get_height(gw->window) - bot,
                                   FB_FRAME_COLOUR,
                                   &scrollr,
                                   fb_scrollr_click,
                                   gw);

                gw->hscroll = fbtk_create_hscroll(gw->window,
                                                  fbtk_get_width(gw->window) - 160 - 20 - right,
                                                  fbtk_get_height(gw->window) - bot,
                                                  160,
                                                  bot,
                                                  FB_SCROLL_COLOUR,
                                                  FB_FRAME_COLOUR);
                /* create vertical */
                fbtk_create_button(gw->window,
                                   fbtk_get_width(gw->window) - right,
                                   top,
                                   FB_FRAME_COLOUR,
                                   &scrollu,
                                   fb_scrollu_click,
                                   gw);

                fbtk_create_button(gw->window,
                                   fbtk_get_width(gw->window) - right,
                                   fbtk_get_height(gw->window) - bot - 20,
                                   FB_FRAME_COLOUR,
                                   &scrolld,
                                   fb_scrolld_click,
                                   gw);

                gw->vscroll = fbtk_create_vscroll(gw->window,
                                                  fbtk_get_width(gw->window) - right,
                                                  top + 20,
                                                  right,
                                                  fbtk_get_height(gw->window) - top - bot - 40 ,
                                                  FB_SCROLL_COLOUR,
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

        gw->browser = fbtk_create_user(gw->window, 0, top, -right, - (bot + top), browser_widget);

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



void fb_window_scroll(struct fbtk_widget_s *browser, int x, int y)
{
        struct browser_widget_s *bwidget = fbtk_get_userpw(browser);
        LOG(("window scroll"));
        bwidget->panx += x;
        bwidget->pany += y;
        bwidget->pan_required = true;

        fbtk_request_redraw(browser);
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
        struct browser_widget_s *bwidget = fbtk_get_userpw(g->browser);
        fb_queue_redraw(g->browser,
                        data->redraw.x - bwidget->scrollx,
                        data->redraw.y - bwidget->scrolly,
                        data->redraw.x - bwidget->scrollx +
                        data->redraw.width,
                        data->redraw.y - bwidget->scrolly +
                        data->redraw.height);
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

	assert(bwidget);

	bwidget->panx = sx - bwidget->scrollx;
	bwidget->pany = sy - bwidget->scrolly;

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

        pct = (fbtk_get_height(g->browser) * 100) / g->bw->current_content->height;
        fbtk_set_scroll(g->vscroll, pct);

}

void gui_window_set_status(struct gui_window *g, const char *text)
{
        fbtk_set_text(g->status, text);
}

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
        switch (shape) {
        case GUI_POINTER_POINT:
                framebuffer_set_cursor(&hand_image);
                break;

        case GUI_POINTER_CARET:
                framebuffer_set_cursor(&caret_image);
                break;

        case GUI_POINTER_MENU:
                framebuffer_set_cursor(&menu_image);
                break;

        case GUI_POINTER_PROGRESS:
                framebuffer_set_cursor(&progress_image);
                break;

        default:
                framebuffer_set_cursor(&pointer_image);
                break;
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

void gui_window_stop_throbber(struct gui_window *gw)
{
        gw->throbber_index = -1;
        fbtk_set_bitmap(gw->throbber, &throbber0);

        fb_update_back_forward(gw);

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
