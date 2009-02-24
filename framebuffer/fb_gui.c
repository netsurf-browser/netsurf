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

#include "framebuffer/fb_bitmap.h"
#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_frontend.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_schedule.h"
#include "framebuffer/fb_cursor.h"
#include "framebuffer/fb_findfile.h"
#include "framebuffer/fb_rootwindow.h"
#include "framebuffer/fb_image_data.h"
#include "framebuffer/fb_font.h"

#include "content/urldb.h"
#include "desktop/history_core.h"
#include "content/fetch.h"

char *default_stylesheet_url;
char *adblock_stylesheet_url;
char *options_file_location;
struct gui_window *input_window = NULL;
struct gui_window *search_current_window;
struct gui_window *window_list = NULL;

bool redraws_pending = false;

framebuffer_t *framebuffer;

static void fb_queue_redraw(struct gui_window *g, int x0, int y0, int x1, int y1);

static void fb_pan(struct gui_window *g)
{
	struct content *c;

        if (!g)
                return;

	c = g->bw->current_content;

	if (!c) return;
	if (c->locked) return;

	LOG(("panning %d, %d from %d, %d in content %d,%d",
             g->panx, g->pany,g->scrollx,g->scrolly,c->width, c->height));
	/* dont pan off the top */
	if ((g->scrolly + g->pany) < 0)
		g->pany = - g->scrolly;

        /* do not pan off the bottom of the content */
	if ((g->scrolly + g->pany) > (c->height - g->height))
		g->pany = (c->height - g->height) - g->scrolly;

	LOG(("panning %d, %d",g->panx, g->pany));

	/* pump up the volume. dance, dance! lets do it */
	if (g->pany < 0) {
		/* we cannot pan more than a window height at a time */
		if (g->pany < -g->height)
			g->pany = -g->height;

		LOG(("panning up %d", g->pany));

		fb_plotters_move_block(g->x, g->y,
                                       g->width, g->height + g->pany,
                                       g->x, g->y - g->pany);
		g->scrolly += g->pany;
		fb_queue_redraw(g, 0, 0,
				g->width, - g->pany);
	}
	if (g->pany > 0) {
		/* we cannot pan more than a window height at a time */
		if (g->pany > g->height)
			g->pany = g->height;

		LOG(("panning down %d", g->pany));

		fb_plotters_move_block(g->x, g->y + g->pany,
                                       g->width, g->height - g->pany,
                                       g->x, g->y);
		g->scrolly += g->pany;
		fb_queue_redraw(g, 0, g->height - g->pany,
				g->width, g->height);
	}

	g->pan_required = false;
	g->panx = 0;
	g->pany = 0;
}

static void fb_redraw(struct gui_window *g)
{
	struct content *c;

        if (!g)
                return;

	c = g->bw->current_content;

	if (!c) return;
	if (c->locked) return;

        /* adjust clipping co-ordinates according to window location */
        g->redraw_box.y0 += g->y;
        g->redraw_box.y1 += g->y;
        g->redraw_box.x0 += g->x;
        g->redraw_box.x1 += g->x;

        /* redraw bounding box is relative to window */
        content_redraw(c,
                       g->x - g->scrollx,
                       g->y - g->scrolly ,
                       g->width,
                       g->height,
                       g->redraw_box.x0, g->redraw_box.y0,
                       g->redraw_box.x1, g->redraw_box.y1,
                       g->bw->scale, 0xFFFFFF);

        fb_os_redraw(&g->redraw_box);

	g->redraw_required = false;
        g->redraw_box.y0 = g->redraw_box.x0 = INT_MAX;
        g->redraw_box.y1 = g->redraw_box.x1 = -(INT_MAX);
	g->panx = 0;
	g->pany = 0;
        redraws_pending = false;
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
                exit(1);
        }

        framebuffer->cursor = fb_cursor_init(framebuffer, &pointer_image);

        fb_font_init();

}

void gui_init2(int argc, char** argv)
{
	struct browser_window *bw;
	const char *addr = NETSURF_HOMEPAGE;

        LOG(("argc %d, argv %p", argc, argv));

        if (option_homepage_url != NULL && option_homepage_url[0] != '\0')
                addr = option_homepage_url;

	if (argc > 1) addr = argv[1];

        fb_rootwindow_create(framebuffer);

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

    //LOG(("enter schedule run"));
    active = schedule_run() | active | redraws_pending;

    fb_os_input(input_window, active);

    if (redraws_pending == true) {
            struct gui_window *g;

            fb_cursor_move(framebuffer, fb_cursor_x(framebuffer), fb_cursor_y(framebuffer));

            redraws_pending = false;

            for (g = window_list; g != NULL; g = g->next) {
                    if (g->pan_required == true) {
                            fb_pan(g);
                    }
                    if (g->redraw_required == true) {
                            fb_redraw(g);
                    }
            }
    }

    fb_cursor_plot(framebuffer);
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
fb_browser_window_click(struct gui_window *g, browser_mouse_state st, int x, int y)
{
        LOG(("browser window clicked at %d,%d",x,y));
        browser_window_mouse_click(g->bw,
                                   st,
                                   x + g->scrollx, 
                                   y + g->scrolly);        
        return 0;
}

static int
fb_browser_window_input(fb_widget_t *widget, struct gui_window *g, int value)
{
        LOG(("got value %d",value));
        if (value >= 0)
                return browser_window_key_press(g->bw, value);
        return 0;
}

struct gui_window *gui_create_browser_window(struct browser_window *bw,
                                             struct browser_window *clone,
                                             bool new_tab)
{
        struct gui_window *g, *p;
        LOG(("bw %p, clone %p", bw, clone));

        g = calloc(1, sizeof(struct gui_window));
        if (g == NULL)
                return NULL;

        g->x = 0;
        g->y = 30;
        g->width = framebuffer->width;
        g->height = framebuffer->height - 50;
        g->bw = bw;

        if (window_list == NULL) {
                window_list = input_window = g;
                fb_add_window_widget(g, 0, fb_browser_window_click, fb_browser_window_input);
        } else {
                for (p = window_list; p->next != NULL; p = p->next);
                p->next = g;
                g->prev = p;
        }

        return g;
}

void gui_window_destroy(struct gui_window *g)
{
        LOG(("g %p", g));

        if (g->prev == NULL) {
                window_list = input_window = g->next;
        } else {
                g->prev->next = g->next;
        }
        if (g->next != NULL)
                g->next->prev = g->prev;

        free(g);

        if (window_list == NULL)
                netsurf_quit = true;

}

void gui_window_set_title(struct gui_window *g, const char *title)
{
        LOG(("%s(%p, %s)", __func__, g, title));
}

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

/* queue a redraw operation, co-ordinates are relative to the window */
static void
fb_queue_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	if (!g) return;

        LOG(("%p, %d, %d, %d, %d", g, x0 , y0, x1, y1));

        g->redraw_box.x0 = MIN(g->redraw_box.x0, x0);
        g->redraw_box.y0 = MIN(g->redraw_box.y0, y0);
        g->redraw_box.x1 = MAX(g->redraw_box.x1, x1);
        g->redraw_box.y1 = MAX(g->redraw_box.y1, y1);

        redraws_pending = true;
	g->redraw_required = true;
}

static void fb_queue_pan(struct gui_window *g, int x, int y)
{
	if (!g) return;

        LOG(("%p, x %d, y %d", g, x , y));

        g->panx +=x;
        g->pany +=y;

        redraws_pending = true;
	g->pan_required = true;
}

void fb_window_scroll(struct gui_window *g, int x, int y)
{
	fb_queue_pan(g, x, y);
}

void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	if (!g) return;

        fb_queue_redraw(g, x0, y0, x1, y1);
}

void gui_window_redraw_window(struct gui_window *g)
{
	if (!g) return;

        fb_queue_redraw(g, 0, 0, g->width, g->height);
}

void gui_window_update_box(struct gui_window *g,
		const union content_msg_data *data)
{
	struct content *c;

	if (!g) return;

        c = g->bw->current_content;

	if (c == NULL) return;

        gui_window_redraw(g, data->redraw.x, data->redraw.y, data->redraw.x + data->redraw.width, data->redraw.y + data->redraw.height);
}

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
        LOG(("g %p, sx %d, sy%d", g, *sx, *sy));
        *sx=0;
        *sy=g->scrolly;

        return true;
}

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
        LOG(("%s:(%p, %d, %d)", __func__, g, sx, sy));
        g->scrolly = sy;
}

void gui_window_scroll_visible(struct gui_window *g, int x0, int y0,
		int x1, int y1)
{
        LOG(("%s:(%p, %d, %d, %d, %d)", __func__, g, x0, y0, x1, y1));
}

void gui_window_position_frame(struct gui_window *g, int x0, int y0,
		int x1, int y1)
{
        LOG(("%p, %d, %d, %d, %d", g, x0, y0, x1, y1));
}

void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
		bool scaled)
{
        LOG(("%p, %d, %d, %d", g, *width, *height, scaled));
        *width = g->width;
        *height = g->height;

}

void gui_window_update_extent(struct gui_window *g)
{
        LOG(("g %p", g));
}

void gui_window_set_status(struct gui_window *g, const char *text)
{
        static char *cur_text = NULL;

        if (cur_text != NULL) {
                if (strcmp(cur_text, text) == 0)
                        return;

                free(cur_text);
        }
        cur_text = strdup(text);

        fb_rootwindow_status(text);
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
        fb_rootwindow_url(url);
}

void gui_window_start_throbber(struct gui_window *g)
{
}

void gui_window_stop_throbber(struct gui_window *g)
{
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
