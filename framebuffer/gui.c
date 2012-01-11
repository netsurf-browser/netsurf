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

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_event.h>

#include "desktop/gui.h"
#include "desktop/mouse.h"
#include "desktop/plotters.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "utils/filepath.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/schedule.h"
#include "utils/types.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "desktop/textinput.h"
#include "render/form.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/framebuffer.h"
#include "framebuffer/schedule.h"
#include "framebuffer/findfile.h"
#include "framebuffer/image_data.h"
#include "framebuffer/font.h"
#include "framebuffer/options.h"

#include "content/urldb.h"
#include "desktop/history_core.h"
#include "content/fetch.h"

#define NSFB_TOOLBAR_DEFAULT_LAYOUT "blfsrut"

fbtk_widget_t *fbtk;

struct gui_window *input_window = NULL;
struct gui_window *search_current_window;
struct gui_window *window_list = NULL;

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

	bwidget->redraw_box.x0 = min(bwidget->redraw_box.x0, x0);
	bwidget->redraw_box.y0 = min(bwidget->redraw_box.y0, y0);
	bwidget->redraw_box.x1 = max(bwidget->redraw_box.x1, x1);
	bwidget->redraw_box.y1 = max(bwidget->redraw_box.y1, y1);

	if (fbtk_clip_to_widget(widget, &bwidget->redraw_box)) {
		bwidget->redraw_required = true;
		fbtk_request_redraw(widget);
	} else {
		bwidget->redraw_box.y0 = bwidget->redraw_box.x0 = INT_MAX;
		bwidget->redraw_box.y1 = bwidget->redraw_box.x1 = -(INT_MAX);
		bwidget->redraw_required = false;
	}
}

/* queue a window scroll */
static void
widget_scroll_y(struct gui_window *gw, int y, bool abs)
{
	struct browser_widget_s *bwidget = fbtk_get_userpw(gw->browser);
	int content_height;
	int height;
	float scale = gw->bw->scale;

	LOG(("window scroll"));
	if (abs) {
		bwidget->pany = y - bwidget->scrolly;
	} else {
		bwidget->pany += y;
	}
	bwidget->pan_required = true;

	content_height = content_get_height(gw->bw->current_content) * scale;

	height = fbtk_get_height(gw->browser);

	/* dont pan off the top */
	if ((bwidget->scrolly + bwidget->pany) < 0)
		bwidget->pany = -bwidget->scrolly;

	/* do not pan off the bottom of the content */
	if ((bwidget->scrolly + bwidget->pany) > (content_height - height))
		bwidget->pany = (content_height - height) - bwidget->scrolly;

	fbtk_request_redraw(gw->browser);

	fbtk_set_scroll_position(gw->vscroll, bwidget->scrolly + bwidget->pany);
}

/* queue a window scroll */
static void
widget_scroll_x(struct gui_window *gw, int x, bool abs)
{
	struct browser_widget_s *bwidget = fbtk_get_userpw(gw->browser);
	int content_width;
	int width;
	float scale = gw->bw->scale;

	if (abs) {
		bwidget->panx = x - bwidget->scrollx;
	} else {
		bwidget->panx += x;
	}
	bwidget->pan_required = true;

	content_width = content_get_width(gw->bw->current_content) * scale;

	width = fbtk_get_width(gw->browser);


	/* dont pan off the left */
	if ((bwidget->scrollx + bwidget->panx) < 0)
		bwidget->panx = - bwidget->scrollx;

	/* do not pan off the right of the content */
	if ((bwidget->scrollx + bwidget->panx) > (content_width - width))
		bwidget->panx = (content_width - width) - bwidget->scrollx;

	fbtk_request_redraw(gw->browser);

	fbtk_set_scroll_position(gw->hscroll, bwidget->scrollx + bwidget->panx);
}

static void
fb_pan(fbtk_widget_t *widget,
       struct browser_widget_s *bwidget,
       struct browser_window *bw)
{
	int x;
	int y;
	int width;
	int height;
	nsfb_bbox_t srcbox;
	nsfb_bbox_t dstbox;

	nsfb_t *nsfb = fbtk_get_nsfb(widget);

	height = fbtk_get_height(widget);
	width = fbtk_get_width(widget);

	LOG(("panning %d, %d", bwidget->panx, bwidget->pany));

	x = fbtk_get_absx(widget);
	y = fbtk_get_absy(widget);

	/* if the pan exceeds the viewport size just redraw the whole area */
	if (bwidget->pany > height || bwidget->pany < -height ||
	    bwidget->panx > width || bwidget->panx < -width) {

		bwidget->scrolly += bwidget->pany;
		bwidget->scrollx += bwidget->panx;
		fb_queue_redraw(widget, 0, 0, width, height);

		/* ensure we don't try to scroll again */
		bwidget->panx = 0;
		bwidget->pany = 0;
	}

	if (bwidget->pany < 0) {
		/* pan up by less then viewport height */
		srcbox.x0 = x;
		srcbox.y0 = y;
		srcbox.x1 = srcbox.x0 + width;
		srcbox.y1 = srcbox.y0 + height + bwidget->pany;

		dstbox.x0 = x;
		dstbox.y0 = y - bwidget->pany;
		dstbox.x1 = dstbox.x0 + width;
		dstbox.y1 = dstbox.y0 + height + bwidget->pany;

		/* move part that remains visible up */
		nsfb_plot_copy(nsfb, &srcbox, nsfb, &dstbox);

		/* redraw newly exposed area */
		bwidget->scrolly += bwidget->pany;
		fb_queue_redraw(widget, 0, 0, width, - bwidget->pany);
	}

	if (bwidget->pany > 0) {
		/* pan down by less then viewport height */
		srcbox.x0 = x;
		srcbox.y0 = y + bwidget->pany;
		srcbox.x1 = srcbox.x0 + width;
		srcbox.y1 = srcbox.y0 + height - bwidget->pany;

		dstbox.x0 = x;
		dstbox.y0 = y;
		dstbox.x1 = dstbox.x0 + width;
		dstbox.y1 = dstbox.y0 + height - bwidget->pany;

		/* move part that remains visible down */
		nsfb_plot_copy(nsfb, &srcbox, nsfb, &dstbox);

		/* redraw newly exposed area */
		bwidget->scrolly += bwidget->pany;
		fb_queue_redraw(widget, 0, height - bwidget->pany, width, height);
	}

	if (bwidget->panx < 0) {
		/* pan left by less then viewport width */
		srcbox.x0 = x;
		srcbox.y0 = y;
		srcbox.x1 = srcbox.x0 + width + bwidget->panx;
		srcbox.y1 = srcbox.y0 + height;

		dstbox.x0 = x - bwidget->panx;
		dstbox.y0 = y;
		dstbox.x1 = dstbox.x0 + width + bwidget->panx;
		dstbox.y1 = dstbox.y0 + height;

		/* move part that remains visible left */
		nsfb_plot_copy(nsfb, &srcbox, nsfb, &dstbox);

		/* redraw newly exposed area */
		bwidget->scrollx += bwidget->panx;
		fb_queue_redraw(widget, 0, 0, -bwidget->panx, height);
	}

	if (bwidget->panx > 0) {
		/* pan right by less then viewport width */
		srcbox.x0 = x + bwidget->panx;
		srcbox.y0 = y;
		srcbox.x1 = srcbox.x0 + width - bwidget->panx;
		srcbox.y1 = srcbox.y0 + height;

		dstbox.x0 = x;
		dstbox.y0 = y;
		dstbox.x1 = dstbox.x0 + width - bwidget->panx;
		dstbox.y1 = dstbox.y0 + height;

		/* move part that remains visible right */
		nsfb_plot_copy(nsfb, &srcbox, nsfb, &dstbox);

		/* redraw newly exposed area */
		bwidget->scrollx += bwidget->panx;
		fb_queue_redraw(widget, width - bwidget->panx, 0, width, height);
	}


	bwidget->pan_required = false;
	bwidget->panx = 0;
	bwidget->pany = 0;
}

static void
fb_redraw(fbtk_widget_t *widget,
	  struct browser_widget_s *bwidget,
	  struct browser_window *bw)
{
	int x;
	int y;
	struct rect clip;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &fb_plotters
	};

	LOG(("%d,%d to %d,%d",
	     bwidget->redraw_box.x0,
	     bwidget->redraw_box.y0,
	     bwidget->redraw_box.x1,
	     bwidget->redraw_box.y1));

	x = fbtk_get_absx(widget);
	y = fbtk_get_absy(widget);

	/* adjust clipping co-ordinates according to window location */
	bwidget->redraw_box.y0 += y;
	bwidget->redraw_box.y1 += y;
	bwidget->redraw_box.x0 += x;
	bwidget->redraw_box.x1 += x;

	nsfb_claim(fbtk_get_nsfb(widget), &bwidget->redraw_box);

	/* redraw bounding box is relative to window */
	clip.x0 = bwidget->redraw_box.x0;
	clip.y0 = bwidget->redraw_box.y0;
	clip.x1 = bwidget->redraw_box.x1;
	clip.y1 = bwidget->redraw_box.y1;

	browser_window_redraw(bw,
			(x - bwidget->scrollx) / bw->scale,
			(y - bwidget->scrolly) / bw->scale,
			&clip, &ctx);

	nsfb_update(fbtk_get_nsfb(widget), &bwidget->redraw_box);

	bwidget->redraw_box.y0 = bwidget->redraw_box.x0 = INT_MAX;
	bwidget->redraw_box.y1 = bwidget->redraw_box.x1 = INT_MIN;
	bwidget->redraw_required = false;
}

static int
fb_browser_window_redraw(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;
	struct browser_widget_s *bwidget;

	bwidget = fbtk_get_userpw(widget);
	if (bwidget == NULL) {
		LOG(("browser widget from widget %p was null", widget));
		return -1;
	}

	if (bwidget->pan_required) {
		fb_pan(widget, bwidget, gw->bw);
	}

	if (bwidget->redraw_required) {
		fb_redraw(widget, bwidget, gw->bw);
	} else {
		bwidget->redraw_box.x0 = 0;
		bwidget->redraw_box.y0 = 0;
		bwidget->redraw_box.x1 = fbtk_get_width(widget);
		bwidget->redraw_box.y1 = fbtk_get_height(widget);
		fb_redraw(widget, bwidget, gw->bw);
	}
	return 0;
}


static const char *fename;
static int febpp;
static int fewidth;
static int feheight;
static const char *feurl;

static bool
process_cmdline(int argc, char** argv)
{
	int opt;

	LOG(("argc %d, argv %p", argc, argv));

	fename = "sdl";
	febpp = 32;

	if ((option_window_width != 0) && (option_window_height != 0)) {
		fewidth = option_window_width;
		feheight = option_window_height;
	} else {
		fewidth = 800;
		feheight = 600;
	}

	if (option_homepage_url != NULL && option_homepage_url[0] != '\0')
		feurl = option_homepage_url;
	else
		feurl = NETSURF_HOMEPAGE;


	while((opt = getopt(argc, argv, "f:b:w:h:")) != -1) {
		switch (opt) {
		case 'f':
			fename = optarg;
			break;

		case 'b':
			febpp = atoi(optarg);
			break;

		case 'w':
			fewidth = atoi(optarg);
			break;

		case 'h':
			feheight = atoi(optarg);
			break;

		default:
			fprintf(stderr,
				"Usage: %s [-f frontend] [-b bpp] url\n",
				argv[0]);
			return false;
		}
	}

	if (optind < argc) {
		feurl = argv[optind];
	}

	return true;
}

static void
gui_init(int argc, char** argv)
{
	nsfb_t *nsfb;

	option_core_select_menu = true;

	if (option_cookie_file == NULL) {
		option_cookie_file = strdup("~/.netsurf/Cookies");
		LOG(("Using '%s' as Cookies file", option_cookie_file));
	}

	if (option_cookie_jar == NULL) {
		option_cookie_jar = strdup("~/.netsurf/Cookies");
		LOG(("Using '%s' as Cookie Jar file", option_cookie_jar));
	}

	if (option_cookie_file == NULL || option_cookie_jar == NULL)
		die("Failed initialising cookie options");

	if (process_cmdline(argc,argv) != true)
		die("unable to process command line.\n");

	nsfb = framebuffer_initialise(fename, fewidth, feheight, febpp);
	if (nsfb == NULL)
		die("Unable to initialise framebuffer");

	framebuffer_set_cursor(&pointer_image);

	if (fb_font_init() == false)
		die("Unable to initialise the font system");

	fbtk = fbtk_init(nsfb);

	fbtk_enable_oskb(fbtk);

	urldb_load_cookies(option_cookie_file);
}

/** Entry point from OS.
 *
 * /param argc The number of arguments in the string vector.
 * /param argv The argument string vector.
 * /return The return code to the OS
 */
int
main(int argc, char** argv)
{
	struct browser_window *bw;
	char *options;
	char *messages;

	setbuf(stderr, NULL);

	respaths = fb_init_resource("${HOME}/.netsurf/:${NETSURFRES}:"NETSURF_FB_RESPATH":./framebuffer/res:"NETSURF_FB_FONTPATH);

	options = filepath_find(respaths, "Choices");
	messages = filepath_find(respaths, "messages");

	netsurf_init(&argc, &argv, options, messages);

	free(messages);
	free(options);

	gui_init(argc, argv);

	LOG(("calling browser_window_create"));
	bw = browser_window_create(feurl, 0, 0, true, false);

	netsurf_main_loop();

	browser_window_destroy(bw);

	netsurf_exit();

	return 0;
}


void
gui_poll(bool active)
{
	nsfb_event_t event;
	int timeout; /* timeout in miliseconds */

	/* run the scheduler and discover how long to wait for the next event */
	timeout = schedule_run();

	/* if active do not wait for event, return immediately */
	if (active)
		timeout = 0;

	/* if redraws are pending do not wait for event, return immediately */
	if (fbtk_get_redraw_pending(fbtk))
		timeout = 0;

	if (fbtk_event(fbtk, &event, timeout)) {
		if ((event.type == NSFB_EVENT_CONTROL) &&
		    (event.value.controlcode ==  NSFB_CONTROL_QUIT))
			netsurf_quit = true;
	}

	fbtk_redraw(fbtk);

}

void
gui_quit(void)
{
	LOG(("gui_quit"));

	urldb_save_cookies(option_cookie_jar);

	framebuffer_finalise();
}

/* called back when click in browser window */
static int
fb_browser_window_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;
	struct browser_widget_s *bwidget = fbtk_get_userpw(widget);
	float scale;

	if (cbi->event->type != NSFB_EVENT_KEY_DOWN &&
	    cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	LOG(("browser window clicked at %d,%d", cbi->x, cbi->y));

	switch (cbi->event->type) {
	case NSFB_EVENT_KEY_DOWN:
		switch (cbi->event->value.keycode) {
		case NSFB_KEY_MOUSE_1:
			scale = gw->bw->scale;
			browser_window_mouse_click(gw->bw,
					BROWSER_MOUSE_PRESS_1,
					(cbi->x + bwidget->scrollx) / scale,
					(cbi->y + bwidget->scrolly) / scale);
			break;

		case NSFB_KEY_MOUSE_3:
			scale = gw->bw->scale;
			browser_window_mouse_click(gw->bw,
					BROWSER_MOUSE_PRESS_2,
					(cbi->x + bwidget->scrollx) / scale,
					(cbi->y + bwidget->scrolly) / scale);
			break;

		case NSFB_KEY_MOUSE_4:
			/* scroll up */
			scale = gw->bw->scale;
			if (browser_window_scroll_at_point(gw->bw,
					(cbi->x + bwidget->scrollx) / scale,
					(cbi->y + bwidget->scrolly) / scale,
					0, -100) == false)
				widget_scroll_y(gw, -100, false);
			break;

		case NSFB_KEY_MOUSE_5:
			/* scroll down */
			scale = gw->bw->scale;
			if (browser_window_scroll_at_point(gw->bw,
					(cbi->x + bwidget->scrollx) / scale,
					(cbi->y + bwidget->scrolly) / scale,
					0, 100) == false)
				widget_scroll_y(gw, 100, false);
			break;

		default:
			break;

		}

		break;
	case NSFB_EVENT_KEY_UP:
		switch (cbi->event->value.keycode) {
		case NSFB_KEY_MOUSE_1:
			scale = gw->bw->scale;
			browser_window_mouse_click(gw->bw,
					BROWSER_MOUSE_CLICK_1,
					(cbi->x + bwidget->scrollx) / scale,
					(cbi->y + bwidget->scrolly) / scale);
			break;

		case NSFB_KEY_MOUSE_3:
			scale = gw->bw->scale;
			browser_window_mouse_click(gw->bw,
					BROWSER_MOUSE_CLICK_2,
					(cbi->x + bwidget->scrollx) / scale,
					(cbi->y + bwidget->scrolly) / scale);
			break;

		default:
			break;

		}

		break;
	default:
		break;

	}
	return 1;
}

/* called back when movement in browser window */
static int
fb_browser_window_move(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;
	struct browser_widget_s *bwidget = fbtk_get_userpw(widget);

	browser_window_mouse_track(gw->bw, 0,
			(cbi->x + bwidget->scrollx) / gw->bw->scale,
			(cbi->y + bwidget->scrolly) / gw->bw->scale);

	return 0;
}


static int
fb_browser_window_input(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;
	static uint8_t modifier = 0;
	int ucs4 = -1;

	LOG(("got value %d", cbi->event->value.keycode));

	switch (cbi->event->type) {
	case NSFB_EVENT_KEY_DOWN:
		switch (cbi->event->value.keycode) {

		case NSFB_KEY_PAGEUP:
			if (browser_window_key_press(gw->bw, KEY_PAGE_UP) == false)
				widget_scroll_y(gw, -fbtk_get_height(gw->browser), false);
			break;

		case NSFB_KEY_PAGEDOWN:
			if (browser_window_key_press(gw->bw, KEY_PAGE_DOWN) == false)
				widget_scroll_y(gw, fbtk_get_height(gw->browser), false);
			break;

		case NSFB_KEY_RIGHT:
			if (browser_window_key_press(gw->bw, KEY_RIGHT) == false)
				widget_scroll_x(gw, 100, false);
			break;

		case NSFB_KEY_LEFT:
			if (browser_window_key_press(gw->bw, KEY_LEFT) == false)
				widget_scroll_x(gw, -100, false);
			break;

		case NSFB_KEY_UP:
			if (browser_window_key_press(gw->bw, KEY_UP) == false)
				widget_scroll_y(gw, -100, false);
			break;

		case NSFB_KEY_DOWN:
			if (browser_window_key_press(gw->bw, KEY_DOWN) == false)
				widget_scroll_y(gw, 100, false);
			break;

		case NSFB_KEY_RSHIFT:
			modifier |= 1;
			break;

		case NSFB_KEY_LSHIFT:
			modifier |= 1<<1;
			break;

		default:
			ucs4 = fbtk_keycode_to_ucs4(cbi->event->value.keycode,
						    modifier);
			if (ucs4 != -1)
				browser_window_key_press(gw->bw, ucs4);
			break;
		}
		break;

	case NSFB_EVENT_KEY_UP:
		switch (cbi->event->value.keycode) {
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
fb_leftarrow_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;
	struct browser_window *bw = gw->bw;

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	if (history_back_available(bw->history))
		history_back(bw, bw->history);

	fb_update_back_forward(gw);

	return 1;
}

/* right arrow icon click routine */
static int
fb_rightarrow_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;
	struct browser_window *bw = gw->bw;

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	if (history_forward_available(bw->history))
		history_forward(bw, bw->history);

	fb_update_back_forward(gw);
	return 1;

}

/* reload icon click routine */
static int
fb_reload_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct browser_window *bw = cbi->context;

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	browser_window_reload(bw, true);
	return 1;
}

/* stop icon click routine */
static int
fb_stop_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct browser_window *bw = cbi->context;

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	browser_window_stop(bw);
	return 0;
}

static int
fb_osk_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	map_osk();

	return 0;
}

/* close browser window icon click routine */
static int
fb_close_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	netsurf_quit = true;
	return 0;
}

static int
fb_scroll_callback(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;

	switch (cbi->type) {
	case FBTK_CBT_SCROLLY:
		widget_scroll_y(gw, cbi->y, true);
		break;

	case FBTK_CBT_SCROLLX:
		widget_scroll_x(gw, cbi->x, true);
		break;

	default:
		break;
	}
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
fb_url_move(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	framebuffer_set_cursor(&caret_image);
	return 0;
}

static int
set_ptr_default_move(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	framebuffer_set_cursor(&pointer_image);
	return 0;
}

static int
fb_localhistory_btn_clik(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	fb_localhistory_map(gw->localhistory);

	return 0;
}


/** Create a toolbar window and populate it with buttons. 
 *
 * The toolbar layout uses a character to define buttons type and position:
 * b - back
 * l - local history
 * f - forward
 * s - stop 
 * r - refresh
 * u - url bar expands to fit remaining space
 * t - throbber/activity indicator
 * c - close the current window
 *
 * The default layout is "blfsrut" there should be no more than a
 * single url bar entry or behaviour will be undefined.
 *
 * @param gw Parent window 
 * @param toolbar_height The height in pixels of the toolbar
 * @param padding The padding in pixels round each element of the toolbar
 * @param frame_col Frame colour.
 * @param toolbar_layout A string defining which buttons and controls
 *                       should be added to the toolbar. May be empty
 *                       string to disable the bar..
 * 
 */
static fbtk_widget_t *
create_toolbar(struct gui_window *gw, 
	       int toolbar_height, 
	       int padding, 
	       colour frame_col,
	       const char *toolbar_layout)
{
	fbtk_widget_t *toolbar;
	fbtk_widget_t *widget;

	int xpos; /* The position of the next widget. */
	int xlhs = 0; /* extent of the left hand side widgets */
	int xdir = 1; /* the direction of movement + or - 1 */
	const char *itmtype; /* type of the next item */

	if (toolbar_layout == NULL) {
		toolbar_layout = NSFB_TOOLBAR_DEFAULT_LAYOUT;
	}

	LOG(("Using toolbar layout %s", toolbar_layout));

	itmtype = toolbar_layout;

	if (*itmtype == 0) {
		return NULL;
	}

	toolbar = fbtk_create_window(gw->window, 0, 0, 0, 
				     toolbar_height, 
				     frame_col);

	if (toolbar == NULL) {
		return NULL;
	}

	fbtk_set_handler(toolbar, 
			 FBTK_CBT_POINTERENTER, 
			 set_ptr_default_move, 
			 NULL);


	xpos = padding;

	/* loop proceeds creating widget on the left hand side until
	 * it runs out of layout or encounters a url bar declaration
	 * wherupon it works backwards from the end of the layout
	 * untill the space left is for the url bar
	 */
	while ((itmtype >= toolbar_layout) && 
	       (*itmtype != 0) && 
	       (xdir !=0)) {

		LOG(("toolbar adding %c", *itmtype));


		switch (*itmtype) {

		case 'b': /* back */
			widget = fbtk_create_button(toolbar, 
						    (xdir == 1) ? xpos : 
						     xpos - left_arrow.width, 
						    padding, 
						    left_arrow.width, 
						    -padding, 
						    frame_col, 
						    &left_arrow, 
						    fb_leftarrow_click, 
						    gw);
			gw->back = widget; /* keep reference */
			break;

		case 'l': /* local history */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1) ? xpos : 
						     xpos - history_image.width,
						    padding,
						    history_image.width,
						    -padding,
						    frame_col,
						    &history_image,
						    fb_localhistory_btn_clik,
						    gw);
			break;

		case 'f': /* forward */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - right_arrow.width,
						    padding,
						    right_arrow.width,
						    -padding,
						    frame_col,
						    &right_arrow,
						    fb_rightarrow_click,
						    gw);
			gw->forward = widget;
			break;

		case 'c': /* close the current window */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - stop_image_g.width,
						    padding,
						    stop_image_g.width,
						    -padding,
						    frame_col,
						    &stop_image_g,
						    fb_close_click,
						    gw->bw);
			break;

		case 's': /* stop  */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - stop_image.width,
						    padding,
						    stop_image.width,
						    -padding,
						    frame_col,
						    &stop_image,
						    fb_stop_click,
						    gw->bw);
			break;

		case 'r': /* reload */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - reload.width,
						    padding,
						    reload.width,
						    -padding,
						    frame_col,
						    &reload,
						    fb_reload_click,
						    gw->bw);
			break;

		case 't': /* throbber/activity indicator */
			widget = fbtk_create_bitmap(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - throbber0.width,
						    padding,
						    throbber0.width,
						    -padding,
						    frame_col, 
						    &throbber0);
			gw->throbber = widget;
			break;


		case 'u': /* url bar*/
			if (xdir == -1) {
				/* met the u going backwards add url
				 * now we know available extent 
				 */ 

				widget = fbtk_create_writable_text(toolbar,
						   xlhs,
						   padding,
						   xpos - xlhs,
						   -padding,
						   FB_COLOUR_WHITE,
						   FB_COLOUR_BLACK,
						   true,
						   fb_url_enter,
						   gw->bw);

				fbtk_set_handler(widget, 
						 FBTK_CBT_POINTERENTER, 
						 fb_url_move, gw->bw);

				gw->url = widget; /* keep reference */

				/* toolbar is complete */
				xdir = 0;
				break;
			}
			/* met url going forwards, note position and
			 * reverse direction 
			 */
			itmtype = toolbar_layout + strlen(toolbar_layout);
			xdir = -1;
			xlhs = xpos;
			xpos = (2 * fbtk_get_width(toolbar));
			widget = toolbar;
			break;

		default:
			widget = NULL;
			xdir = 0;
			LOG(("Unknown element %c in toolbar layout", *itmtype));
		        break;

		}

		if (widget != NULL) {
			xpos += (xdir * (fbtk_get_width(widget) + padding));
		}

		LOG(("xpos is %d",xpos));

		itmtype += xdir;
	}

	fbtk_set_mapping(toolbar, true);

	return toolbar;
}

static void
create_browser_widget(struct gui_window *gw, int toolbar_height, int furniture_width)
{
	struct browser_widget_s *browser_widget;
	browser_widget = calloc(1, sizeof(struct browser_widget_s));

	gw->browser = fbtk_create_user(gw->window,
				       0,
				       toolbar_height,
				       -furniture_width,
				       -furniture_width,
				       browser_widget);

	fbtk_set_handler(gw->browser, FBTK_CBT_REDRAW, fb_browser_window_redraw, gw);
	fbtk_set_handler(gw->browser, FBTK_CBT_INPUT, fb_browser_window_input, gw);
	fbtk_set_handler(gw->browser, FBTK_CBT_CLICK, fb_browser_window_click, gw);
	fbtk_set_handler(gw->browser, FBTK_CBT_POINTERMOVE, fb_browser_window_move, gw);
}

static void
create_normal_browser_window(struct gui_window *gw, int furniture_width)
{
	fbtk_widget_t *widget;
	fbtk_widget_t *toolbar;
	int statusbar_width = 0;
	int toolbar_height = option_fb_toolbar_size;

	LOG(("Normal window"));

	gw->window = fbtk_create_window(fbtk, 0, 0, 0, 0, 0);

	statusbar_width = option_toolbar_status_width *
		fbtk_get_width(gw->window) / 10000;

	/* toolbar */
	toolbar = create_toolbar(gw, 
				 toolbar_height, 
				 2, 
				 FB_FRAME_COLOUR, 
				 option_fb_toolbar_layout);

	/* set the actually created toolbar height */
	if (toolbar != NULL) {
		toolbar_height = fbtk_get_height(toolbar);
	} else {
		toolbar_height = 0;
	}

	/* status bar */
	gw->status = fbtk_create_text(gw->window,
				      0,
				      fbtk_get_height(gw->window) - furniture_width,
				      statusbar_width, furniture_width,
				      FB_FRAME_COLOUR, FB_COLOUR_BLACK,
				      false);
	fbtk_set_handler(gw->status, FBTK_CBT_POINTERENTER, set_ptr_default_move, NULL);

	LOG(("status bar %p at %d,%d", gw->status, fbtk_get_absx(gw->status), fbtk_get_absy(gw->status)));

	/* create horizontal scrollbar */
	gw->hscroll = fbtk_create_hscroll(gw->window,
					  statusbar_width,
					  fbtk_get_height(gw->window) - furniture_width,
					  fbtk_get_width(gw->window) - statusbar_width - furniture_width,
					  furniture_width,
					  FB_SCROLL_COLOUR,
					  FB_FRAME_COLOUR,
					  fb_scroll_callback,
					  gw);

	/* fill bottom right area */

	if (option_fb_osk == true) {
		widget = fbtk_create_text_button(gw->window,
						 fbtk_get_width(gw->window) - furniture_width,
						 fbtk_get_height(gw->window) - furniture_width,
						 furniture_width,
						 furniture_width,
						 FB_FRAME_COLOUR, FB_COLOUR_BLACK,
						 fb_osk_click,
						 NULL);
		fbtk_set_text(widget, "\xe2\x8c\xa8");
	} else {
		widget = fbtk_create_fill(gw->window,
					  fbtk_get_width(gw->window) - furniture_width,
					  fbtk_get_height(gw->window) - furniture_width,
					  furniture_width,
					  furniture_width,
					  FB_FRAME_COLOUR);

		fbtk_set_handler(widget, FBTK_CBT_POINTERENTER, set_ptr_default_move, NULL);
	}

	/* create vertical scrollbar */
	gw->vscroll = fbtk_create_vscroll(gw->window,
					  fbtk_get_width(gw->window) - furniture_width,
					  toolbar_height,
					  furniture_width,
					  fbtk_get_height(gw->window) - toolbar_height - furniture_width,
					  FB_SCROLL_COLOUR,
					  FB_FRAME_COLOUR,
					  fb_scroll_callback,
					  gw);

	/* browser widget */
	create_browser_widget(gw, toolbar_height, option_fb_furniture_size);

	/* Give browser_window's user widget input focus */
	fbtk_set_focus(gw->browser);
}


struct gui_window *
gui_create_browser_window(struct browser_window *bw,
			  struct browser_window *clone,
			  bool new_tab)
{
	struct gui_window *gw;

	gw = calloc(1, sizeof(struct gui_window));

	if (gw == NULL)
		return NULL;

	/* seems we need to associate the gui window with the underlying
	 * browser window
	 */
	gw->bw = bw;

	create_normal_browser_window(gw, option_fb_furniture_size);
	gw->localhistory = fb_create_localhistory(bw, fbtk, option_fb_furniture_size);

	/* map and request redraw of gui window */
	fbtk_set_mapping(gw->window, true);

	return gw;
}

void
gui_window_destroy(struct gui_window *gw)
{
	fbtk_destroy_widget(gw->window);

	free(gw);


}

void
gui_window_set_title(struct gui_window *g, const char *title)
{
	LOG(("%p, %s", g, title));
}

void
gui_window_redraw_window(struct gui_window *g)
{
	fb_queue_redraw(g->browser, 0, 0, fbtk_get_width(g->browser), fbtk_get_height(g->browser) );
}

void
gui_window_update_box(struct gui_window *g, const struct rect *rect)
{
	struct browser_widget_s *bwidget = fbtk_get_userpw(g->browser);
	fb_queue_redraw(g->browser,
			rect->x0 - bwidget->scrollx,
			rect->y0 - bwidget->scrolly,
			rect->x1 - bwidget->scrollx,
			rect->y1 - bwidget->scrolly);
}

bool
gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	struct browser_widget_s *bwidget = fbtk_get_userpw(g->browser);

	*sx = bwidget->scrollx / g->bw->scale;
	*sy = bwidget->scrolly / g->bw->scale;

	return true;
}

void
gui_window_set_scroll(struct gui_window *gw, int sx, int sy)
{
	struct browser_widget_s *bwidget = fbtk_get_userpw(gw->browser);

	assert(bwidget);

	widget_scroll_x(gw, sx * gw->bw->scale, true);
	widget_scroll_y(gw, sy * gw->bw->scale, true);
}

void
gui_window_scroll_visible(struct gui_window *g, int x0, int y0,
			  int x1, int y1)
{
	LOG(("%s:(%p, %d, %d, %d, %d)", __func__, g, x0, y0, x1, y1));
}

void
gui_window_get_dimensions(struct gui_window *g,
			  int *width,
			  int *height,
			  bool scaled)
{
	*width = fbtk_get_width(g->browser);
	*height = fbtk_get_height(g->browser);

	if (scaled) {
		*width /= g->bw->scale;
		*height /= g->bw->scale;
	}
}

void
gui_window_update_extent(struct gui_window *gw)
{
	float scale = gw->bw->scale;

	fbtk_set_scroll_parameters(gw->hscroll, 0,
			content_get_width(gw->bw->current_content) * scale,
			fbtk_get_width(gw->browser), 100);

	fbtk_set_scroll_parameters(gw->vscroll, 0,
			content_get_height(gw->bw->current_content) * scale,
			fbtk_get_height(gw->browser), 100);
}

void
gui_window_set_status(struct gui_window *g, const char *text)
{
	fbtk_set_text(g->status, text);
}

void
gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
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

void
gui_window_hide_pointer(struct gui_window *g)
{
}

void
gui_window_set_url(struct gui_window *g, const char *url)
{
	fbtk_set_text(g->url, url);
}

static void
throbber_advance(void *pw)
{
	struct gui_window *g = pw;
	struct fbtk_bitmap *image;

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

void
gui_window_start_throbber(struct gui_window *g)
{
	g->throbber_index = 0;
	schedule(10, throbber_advance, g);
}

void
gui_window_stop_throbber(struct gui_window *gw)
{
	gw->throbber_index = -1;
	fbtk_set_bitmap(gw->throbber, &throbber0);

	fb_update_back_forward(gw);

}

void
gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
}

void
gui_window_remove_caret(struct gui_window *g)
{
}

void
gui_window_new_content(struct gui_window *g)
{
}

bool
gui_window_scroll_start(struct gui_window *g)
{
	return true;
}

bool
gui_window_drag_start(struct gui_window *g, gui_drag_type type,
                      const struct rect *rect)
{
	return true;
}

void
gui_window_save_link(struct gui_window *g, const char *url, const char *title)
{
}

/**
 * set favicon
 */
void
gui_window_set_icon(struct gui_window *g, hlcache_handle *icon)
{
}

/**
 * set gui display of a retrieved favicon representing the search provider
 * \param ico may be NULL for local calls; then access current cache from
 * search_web_ico()
 */
void
gui_window_set_search_ico(hlcache_handle *ico)
{
}

struct gui_download_window *
gui_download_window_create(download_context *ctx, struct gui_window *parent)
{
	return NULL;
}

nserror
gui_download_window_data(struct gui_download_window *dw,
			 const char *data,
			 unsigned int size)
{
	return NSERROR_OK;
}

void
gui_download_window_error(struct gui_download_window *dw,
			  const char *error_msg)
{
}

void
gui_download_window_done(struct gui_download_window *dw)
{
}

void
gui_drag_save_object(gui_save_type type,
		     hlcache_handle *c,
		     struct gui_window *w)
{
}

void
gui_drag_save_selection(struct selection *s, struct gui_window *g)
{
}

void
gui_start_selection(struct gui_window *g)
{
}

void
gui_clear_selection(struct gui_window *g)
{
}

void
gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{
}

bool
gui_empty_clipboard(void)
{
	return false;
}

bool
gui_add_to_clipboard(const char *text, size_t length, bool space)
{
	return false;
}

bool
gui_commit_clipboard(void)
{
	return false;
}

bool
gui_copy_to_clipboard(struct selection *s)
{
	return false;
}

void
gui_create_form_select_menu(struct browser_window *bw,
			    struct form_control *control)
{
}

void
gui_launch_url(const char *url)
{
}

void
gui_cert_verify(const char *url,
		const struct ssl_cert_info *certs,
		unsigned long num,
		nserror (*cb)(bool proceed, void *pw),
		void *cbpw)
{
	cb(false, cbpw);
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
