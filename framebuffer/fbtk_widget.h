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

#ifndef NETSURF_FB_FBTK_WIDGET_H
#define NETSURF_FB_FBTK_WIDGET_H

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

	fbtk_callback callback; /* event callback */
	void *callback_context;

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
			struct fbtk_widget_s *btnul; /* scroll button up/left */
			struct fbtk_widget_s *btndr; /* scroll button down/right*/
		} scroll;

	} u;
};


/* widget manipulation functions */

fbtk_widget_t *new_widget(enum fbtk_widgettype_e type);

fbtk_widget_t *add_widget_to_window(fbtk_widget_t *window, fbtk_widget_t *widget);

#endif
