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

#ifndef NETSURF_FB_GUI_H
#define NETSURF_FB_GUI_H

typedef struct fb_cursor_s fb_cursor_t;

/* bounding box */
typedef struct bbox_s {
        int x0;
        int y0;
        int x1;
        int y1;
} bbox_t;

typedef struct framebuffer_s {
	int width;
	int height;
	uint8_t *ptr; /**< Base of video memory. */
	int linelen; /**< length of a video line. */
	int bpp;
	colour palette[256]; /* palette for index modes */
	fb_cursor_t *cursor;
} framebuffer_t;

struct gui_window {
        struct browser_window *bw;

        struct fbtk_widget_s *window;
        struct fbtk_widget_s *back;
        struct fbtk_widget_s *forward;
        struct fbtk_widget_s *url;
        struct fbtk_widget_s *status;
        struct fbtk_widget_s *throbber;
        struct fbtk_widget_s *hscroll;
        struct fbtk_widget_s *vscroll;
        struct fbtk_widget_s *browser;
        int throbber_index;
};


extern framebuffer_t *framebuffer;
extern struct gui_window *window_list;

/* scroll a window */
void fb_window_scroll(struct gui_window *g, int x, int y);

#endif /* NETSURF_FB_GUI_H */

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */


