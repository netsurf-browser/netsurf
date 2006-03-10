/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
 */

/** \file
 * Browser window handling (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include "oslib/colourtrans.h"
#include "oslib/osbyte.h"
#include "oslib/osfile.h"
#include "oslib/osspriteop.h"
#include "oslib/serviceinternational.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/content/url_store.h"
#include "netsurf/css/css.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/desktop/textinput.h"
#include "netsurf/render/box.h"
#include "netsurf/render/form.h"
#include "netsurf/riscos/buffer.h"
#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/global_history.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/thumbnail.h"
#include "netsurf/riscos/ucstables.h"
#include "netsurf/riscos/url_complete.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/talloc.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utf8.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/messages.h"

#ifndef wimp_KEY_END
#define wimp_KEY_END wimp_KEY_COPY
#endif

#define SCROLL_VISIBLE_PADDING 32

/** List of all browser windows. */
static struct gui_window *window_list = 0;
/** GUI window which is being redrawn. Valid only during redraw. */
struct gui_window *ro_gui_current_redraw_gui;

static float scale_snap_to[] = {0.10, 0.125, 0.25, 0.333, 0.5, 0.75,
				1.0,
				1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0};
#define SCALE_SNAP_TO_SIZE (sizeof scale_snap_to) / (sizeof(float))

static void ro_gui_window_launch_url(struct gui_window *g, const char *url);
static void ro_gui_window_clone_options(struct browser_window *new_bw,
		struct browser_window *old_bw);
static browser_mouse_state ro_gui_mouse_drag_state(wimp_mouse_state buttons);
static bool ro_gui_window_import_text(struct gui_window *g, const char *filename,
		bool toolbar);



/**
 * Create and open a new browser window.
 *
 * \param  bw	  browser_window structure to update
 * \param  clone  the browser window to clone options from, or NULL for default
 * \return  gui_window, or 0 on error and error reported
 */

struct gui_window *gui_create_browser_window(struct browser_window *bw,
		struct browser_window *clone)
{
	int screen_width, screen_height, win_width, win_height, scroll_width;
	static int window_count = 2;
	wimp_window window;
	wimp_window_state state;
	os_error *error;
	bool open_centred = true;
	struct gui_window *g;

	g = malloc(sizeof *g);
	if (!g) {
		warn_user("NoMemory", 0);
		return 0;
	}
	g->bw = bw;
	g->toolbar = 0;
	g->reformat_pending = false;
	g->old_width = 0;
	g->old_height = 0;
	strcpy(g->title, "NetSurf");
	g->throbber = 0;
	g->throbtime = 0;

	/*	Set the window position
	*/
	if (clone && clone->window && option_window_size_clone) {
		state.w = clone->window->window;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		window.visible.x0 = state.visible.x0;
		window.visible.x1 = state.visible.x1;
		window.visible.y0 = state.visible.y0 - 48;
		window.visible.y1 = state.visible.y1 - 48;
		open_centred = false;
	} else {
		ro_gui_screen_size(&screen_width, &screen_height);

		/*	Check if we have a preferred position
		*/
		if ((option_window_screen_width != 0) &&
				(option_window_screen_height != 0)) {
			win_width = (option_window_width * screen_width) /
					option_window_screen_width;
			win_height = (option_window_height * screen_height) /
					option_window_screen_height;
			window.visible.x0 = (option_window_x * screen_width) /
					option_window_screen_width;
			window.visible.y0 = (option_window_y * screen_height) /
					option_window_screen_height;
			if (option_window_stagger) {
				window.visible.y0 += 96 -
						(48 * (window_count % 5));
			}
			open_centred = false;
			if (win_width < 100)
				win_width = 100;
			if (win_height < 100)
				win_height = 100;
		} else {

		       /* Base how we define the window height/width
		       on the compile time options set */
#ifdef WITH_KIOSK_BROWSING
			/* We're going fullscreen, forget the iconbar! */
			win_width = screen_width;
			win_height = screen_height;
#else
			win_width = screen_width * 3 / 4;
			if (1600 < win_width)
				win_width = 1600;
			win_height = win_width * 3 / 4;
#endif

			window.visible.x0 = (screen_width - win_width) / 2;
			window.visible.y0 = ((screen_height - win_height) / 2) +
					96 - (48 * (window_count % 5));
		}
		window.visible.x1 = window.visible.x0 + win_width;
		window.visible.y1 = window.visible.y0 + win_height;
	}

	/*	Set the general window characteristics
	*/
	window.xscroll = 0;
	window.yscroll = 0;
	window.next = wimp_TOP;

	/* Base how we define the window characteristics
	 on the compile time options set */
#ifdef WITH_KIOSK_BROWSING
	window.flags = 	wimp_WINDOW_NEW_FORMAT |
			wimp_WINDOW_VSCROLL |
			wimp_WINDOW_HSCROLL |
			wimp_WINDOW_IGNORE_XEXTENT |
			wimp_WINDOW_IGNORE_YEXTENT |
			wimp_WINDOW_SCROLL_REPEAT;
#else
	window.flags =  wimp_WINDOW_MOVEABLE |
			wimp_WINDOW_NEW_FORMAT |
			wimp_WINDOW_BACK_ICON |
			wimp_WINDOW_CLOSE_ICON |
			wimp_WINDOW_TITLE_ICON |
			wimp_WINDOW_VSCROLL |
			wimp_WINDOW_HSCROLL |
			wimp_WINDOW_SIZE_ICON |
			wimp_WINDOW_TOGGLE_ICON |
			wimp_WINDOW_IGNORE_XEXTENT |
			wimp_WINDOW_IGNORE_YEXTENT |
			wimp_WINDOW_SCROLL_REPEAT;
#endif

	window.title_fg = wimp_COLOUR_BLACK;
	window.title_bg = wimp_COLOUR_LIGHT_GREY;
	window.work_fg = wimp_COLOUR_LIGHT_GREY;
	window.work_bg = wimp_COLOUR_TRANSPARENT;
	window.scroll_outer = wimp_COLOUR_DARK_GREY;
	window.scroll_inner = wimp_COLOUR_MID_LIGHT_GREY;
	window.highlight_bg = wimp_COLOUR_CREAM;
	window.extra_flags = 0;
	window.extent.x0 = 0;
	window.extent.y0 = window.visible.y1 - window.visible.y0;
	window.extent.x1 = window.visible.x1 - window.visible.x0;
	window.extent.y1 = 0;
	window.title_flags = wimp_ICON_TEXT |
			wimp_ICON_INDIRECTED |
			wimp_ICON_HCENTRED;
	window.work_flags = wimp_BUTTON_CLICK_DRAG <<
			wimp_ICON_BUTTON_TYPE_SHIFT;
	window.sprite_area = wimpspriteop_AREA;
	window.xmin = 1;
	window.ymin = 1;
	window.title_data.indirected_text.text = g->title;
	window.title_data.indirected_text.validation = (char *) -1;
	window.title_data.indirected_text.size = 255;
	window.icon_count = 0;
	if (open_centred) {
		scroll_width = ro_get_vscroll_width(NULL);
		window.visible.x0 -= scroll_width;
	}

	error = xwimp_create_window(&window, &g->window);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		free(g);
		return 0;
	}

	g->prev = 0;
	g->next = window_list;
	if (window_list)
		window_list->prev = g;
	window_list = g;
	window_count++;

	/*	Add in a toolbar
	*/
	g->toolbar = ro_gui_theme_create_toolbar(NULL, THEME_BROWSER_TOOLBAR);
	ro_gui_theme_attach_toolbar(g->toolbar, g->window);

	/*	Set the window options
	*/
	bw->window = g;
	ro_gui_window_clone_options(bw, clone);
	ro_gui_prepare_navigate(g);

	/*	Open the window
	*/
	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return g;
	}

	/*	Open the window at the top of the stack
	*/
	state.next = wimp_TOP;
	ro_gui_window_open(g, (wimp_open*)&state);


	/*	Set the caret position to the URL bar
	*/
	if (g->toolbar && g->toolbar->display_url) {
		error = xwimp_set_caret_position(
				g->toolbar->toolbar_handle,
				ICON_TOOLBAR_URL, -1, -1, -1, 0);
		ro_gui_url_complete_start(g);
	} else
		error = xwimp_set_caret_position(g->window,
				wimp_ICON_WINDOW, -100, -100, 32, -1);

	if (error) {
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return g;
	}

	return g;
}


/**
 * Close a browser window and free any related resources.
 *
 * \param  g  gui_window to destroy
 */

void gui_window_destroy(struct gui_window *g)
{
	os_error *error;

	assert(g);

	/* remove from list */
	if (g->prev)
		g->prev->next = g->next;
	else
		window_list = g->next;
	if (g->next)
		g->next->prev = g->prev;

	ro_gui_theme_destroy_toolbar(g->toolbar);

	/* delete window */
	error = xwimp_delete_window(g->window);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	free(g);
}


/**
 * Destroy all browser windows.
 */

void ro_gui_window_quit(void)
{
	while (window_list)
		browser_window_destroy(window_list->bw);
}


/**
 * Set the title of a browser window.
 *
 * \param  g	  gui_window to update
 * \param  title  new window title, copied
 */

void gui_window_set_title(struct gui_window *g, const char *title)
{
	int scale_disp;

	assert(g);
	assert(title);

	if (g->option.scale != 1.0) {
	  	scale_disp = g->option.scale * 100;
	  	if (ABS((float)scale_disp - g->option.scale * 100) >= 0.05)
			snprintf(g->title, sizeof g->title, "%s (%.1f%%)", title,
					g->option.scale * 100);
		else
			snprintf(g->title, sizeof g->title, "%s (%i%%)", title, scale_disp);
	} else {
		strncpy(g->title, title, sizeof g->title);
	}

	ro_gui_set_window_title(g->window, g->title);
}


/**
 * Force a redraw of part of the contents of a browser window.
 *
 * \param  g   gui_window to redraw
 * \param  x0  rectangle to redraw
 * \param  y0  rectangle to redraw
 * \param  x1  rectangle to redraw
 * \param  y1  rectangle to redraw
 */

void gui_window_redraw(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	os_error *error;

	assert(g);

	error = xwimp_force_redraw(g->window, x0 * 2, -y1 * 2, x1 * 2, -y0 * 2);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Redraws the content for all windows.
 */

void ro_gui_window_redraw_all(void)
{
	struct gui_window *g;
	for (g = window_list; g; g = g->next)
		gui_window_redraw_window(g);
}


/**
 * Force a redraw of the entire contents of a browser window.
 *
 * \param  g   gui_window to redraw
 */

void gui_window_redraw_window(struct gui_window *g)
{
	wimp_window_info info;
	os_error *error;

	assert(g);

	info.w = g->window;
	error = xwimp_get_window_info_header_only(&info);
	if (error) {
		LOG(("xwimp_get_window_info_header_only: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	error = xwimp_force_redraw(g->window, info.extent.x0, info.extent.y0,
			info.extent.x1, info.extent.y1);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Handle a Redraw_Window_Request for a browser window.
 */

void ro_gui_window_redraw(struct gui_window *g, wimp_draw *redraw)
{
	osbool more;
	bool clear_background = false;
	bool clear_partial = false;
	float scale = 1;
	struct content *c = g->bw->current_content;
	int clip_x0, clip_y0, clip_x1, clip_y1, clear_x1, clear_y1;
	int content_y1, content_x1;
	os_error *error;
	osspriteop_area *area;
	osspriteop_header *header;

	/*	Handle no content quickly
	*/
	if (!c) {
	  	ro_gui_user_redraw(redraw, true, os_COLOUR_WHITE);
	  	return;
	}

	plot = ro_plotters;
	ro_plot_set_scale(g->option.scale);
	ro_gui_current_redraw_gui = g;
	current_redraw_browser = g->bw;

	/*	We should clear the background, except for HTML.
	*/
	switch (c->type) {
		case CONTENT_HTML:
			break;
		case CONTENT_CSS:
		case CONTENT_TEXTPLAIN:
#ifdef WITH_JPEG
		case CONTENT_JPEG:
#endif
			clear_partial = true;
			clear_background = true;
			scale = g->option.scale;
			break;


#ifdef WITH_SPRITE
		case CONTENT_SPRITE:
			area = (osspriteop_area *)c->data.sprite.data;
			header = (osspriteop_header *)((char*)area + area->first);
			clear_partial = (header->image) == (header->mask);
#endif
#ifdef WITH_GIF
		case CONTENT_GIF:
#endif
#ifdef WITH_BMP
		case CONTENT_BMP:
		case CONTENT_ICO:
#endif
#ifdef WITH_MNG
		case CONTENT_JNG:
		case CONTENT_MNG:
		case CONTENT_PNG:
#endif
			if (c->bitmap)
				clear_partial = bitmap_get_opaque(c->bitmap);
		default:
			clear_background = true;
			scale = g->option.scale;
			break;
	}

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	while (more) {
		ro_plot_origin_x = redraw->box.x0 - redraw->xscroll;
		ro_plot_origin_y = redraw->box.y1 - redraw->yscroll;
		clip_x0 = (redraw->clip.x0 - ro_plot_origin_x) / 2;
		clip_y0 = (ro_plot_origin_y - redraw->clip.y1) / 2;
		clip_x1 = (redraw->clip.x1 - ro_plot_origin_x) / 2;
		clip_y1 = (ro_plot_origin_y - redraw->clip.y0) / 2;
		clear_x1 = redraw->clip.x1 - ro_plot_origin_x;
		clear_y1 = redraw->clip.y0 - ro_plot_origin_y;

		if (ro_gui_current_redraw_gui->option.buffer_everything)
			ro_gui_buffer_open(redraw);

		if (clear_background) {
			error = xcolourtrans_set_gcol(os_COLOUR_WHITE,
					colourtrans_SET_BG,
					os_ACTION_OVERWRITE, 0, 0);
			if (error) {
				LOG(("xcolourtrans_set_gcol: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MiscError", error->errmess);
			}
			if (clear_partial) {
			  	content_x1 = ro_plot_origin_x + c->width * 2 * scale;
			  	content_y1 = ro_plot_origin_y - c->height * 2 * scale;
			  	clear_x1 += ro_plot_origin_x;
			  	clear_y1 += ro_plot_origin_y;
			  	if (content_y1 > clear_y1) {
					xos_plot(os_MOVE_TO,
							ro_plot_origin_x,
							content_y1);
					xos_plot(os_PLOT_BG_TO | os_PLOT_RECTANGLE,
							clear_x1,
							clear_y1);
				}
			  	if (content_x1 < redraw->clip.x1) {
					xos_plot(os_MOVE_TO,
							content_x1,
							ro_plot_origin_y);
					xos_plot(os_PLOT_BG_TO | os_PLOT_RECTANGLE,
							clear_x1,
							content_y1);
				}
			} else {
				os_clg();
			}
		}

		content_redraw(c, 0, 0,
				c->width * scale, c->height * scale,
				clip_x0, clip_y0, clip_x1, clip_y1,
				g->option.scale,
				0xFFFFFF);

		if (ro_gui_current_redraw_gui->option.buffer_everything)
			ro_gui_buffer_close();

		error = xwimp_get_rectangle(redraw, &more);
		/* RISC OS 3.7 returns an error here if enough buffer was
		   claimed to cause a new dynamic area to be created. It
		   doesn't actually stop anything working, so we mask it out
		   for now until a better fix is found. This appears to be a
		   bug in RISC OS. */
		if (error && !(ro_gui_current_redraw_gui->
				option.buffer_everything &&
				error->errnum == error_WIMP_GET_RECT)) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			ro_gui_current_redraw_gui = NULL;
			current_redraw_browser = NULL;
			return;
		}
	}
	ro_gui_current_redraw_gui = NULL;
	current_redraw_browser = NULL;
}


/**
 * Redraw an area of a window.
 *
 * \param  g   gui_window
 * \param  data  content_msg_data union with filled in redraw data
 */

void gui_window_update_box(struct gui_window *g,
		const union content_msg_data *data)
{
	struct content *c = g->bw->current_content;
	osbool more;
	bool clear_background = false;
	bool use_buffer;
	wimp_draw update;
	int clip_x0, clip_y0, clip_x1, clip_y1;
	os_error *error;

	/* in some cases an update can be triggered before a content has fully
	 * loaded, so current_content is 0 */
	if (!c)
		return;

	update.w = g->window;
	update.box.x0 = floorf(data->redraw.x * 2 * g->option.scale);
	update.box.y0 = -ceilf((data->redraw.y + data->redraw.height) * 2 *
			g->option.scale);
	update.box.x1 = ceilf((data->redraw.x + data->redraw.width) * 2 *
			g->option.scale) + 1;
	update.box.y1 = -floorf(data->redraw.y * 2 * g->option.scale) + 1;

	error = xwimp_update_window(&update, &more);
	if (error) {
		LOG(("xwimp_update_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/*	Set the current redraw gui_window to get options from
	*/
	ro_gui_current_redraw_gui = g;
	current_redraw_browser = g->bw;
	use_buffer = (data->redraw.full_redraw) &&
			(g->option.buffer_everything || g->option.buffer_animations);

	plot = ro_plotters;
	ro_plot_origin_x = update.box.x0 - update.xscroll;
	ro_plot_origin_y = update.box.y1 - update.yscroll;
	ro_plot_set_scale(g->option.scale);

	/*	We should clear the background, except for HTML.
	*/
	if (c->type != CONTENT_HTML)
		clear_background = true;

	while (more) {
		clip_x0 = (update.clip.x0 - ro_plot_origin_x) / 2;
		clip_y0 = (ro_plot_origin_y - update.clip.y1) / 2;
		clip_x1 = (update.clip.x1 - ro_plot_origin_x) / 2;
		clip_y1 = (ro_plot_origin_y - update.clip.y0) / 2;

		if (use_buffer)
			ro_gui_buffer_open(&update);
		if (data->redraw.full_redraw) {
			if (clear_background) {
				error = xcolourtrans_set_gcol(os_COLOUR_WHITE,
						colourtrans_SET_BG,
						os_ACTION_OVERWRITE, 0, 0);
				if (error) {
					LOG(("xcolourtrans_set_gcol: 0x%x: %s",
							error->errnum,
							error->errmess));
					warn_user("MiscError", error->errmess);
				}
				os_clg();
			}

			content_redraw(c, 0, 0,
					c->width, c->height,
					clip_x0, clip_y0, clip_x1, clip_y1,
					g->option.scale,
					0xFFFFFF);
		} else {
			assert(data->redraw.object);
			content_redraw(data->redraw.object,
					floorf(data->redraw.object_x *
						g->option.scale),
					ceilf(data->redraw.object_y *
						g->option.scale),
					data->redraw.object_width *
						g->option.scale,
					data->redraw.object_height *
						g->option.scale,
					clip_x0, clip_y0, clip_x1, clip_y1,
					g->option.scale,
					0xFFFFFF);
		}

		if (use_buffer)
			ro_gui_buffer_close();
		error = xwimp_get_rectangle(&update, &more);
		/* RISC OS 3.7 returns an error here if enough buffer was
		   claimed to cause a new dynamic area to be created. It
		   doesn't actually stop anything working, so we mask it out
		   for now until a better fix is found. This appears to be a
		   bug in RISC OS. */
		if (error && !(use_buffer &&
				error->errnum == error_WIMP_GET_RECT)) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			ro_gui_current_redraw_gui = NULL;
			current_redraw_browser = NULL;
			return;
		}
	}

	/*	Reset the current redraw gui_window to prevent thumbnails from
		retaining options
	*/
	ro_gui_current_redraw_gui = NULL;
	current_redraw_browser = NULL;
}


/**
 * Get the scroll position of a browser window.
 *
 * \param  g   gui_window
 * \param  sx  receives x ordinate of point at top-left of window
 * \param  sy  receives y ordinate of point at top-left of window
 * \return true iff successful
 */

bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	wimp_window_state state;
	os_error *error;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	else {
		int toolbar_height = 0;

		if (g->toolbar)
			toolbar_height = ro_gui_theme_toolbar_full_height(g->toolbar);

		*sx = state.xscroll / (2 * g->option.scale);
		*sy = -(state.yscroll - toolbar_height) / (2 * g->option.scale);
		return true;
	}
}


/**
 * Set the scroll position of a browser window.
 *
 * \param  g   gui_window to scroll
 * \param  sx  point to place at top-left of window
 * \param  sy  point to place at top-left of window
 */

void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	wimp_window_state state;
	os_error *error;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	state.xscroll = sx * 2 * g->option.scale;
	state.yscroll = -sy * 2 * g->option.scale;
	if (g->toolbar)
		state.yscroll += ro_gui_theme_toolbar_full_height(g->toolbar);
	ro_gui_window_open(g, (wimp_open *) &state);
}


/**
 * Scrolls the specified area of a browser window into view.
 *
 * \param  g   gui_window to scroll
 * \param  x0  left point to ensure visible
 * \param  y0  top-left point to ensure visible
 * \param  x1  left point to ensure visible
 * \param  y1  top-left point to ensure visible
 */
void gui_window_scroll_visible(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	wimp_window_state state;
	os_error *error;
	int cx0, cy0, width, height;
	int padding_available;
	int toolbar_height = 0;
	int correction;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (g->toolbar)
	  	toolbar_height = ro_gui_theme_toolbar_full_height(g->toolbar);
	
	x0 = x0 * 2 * g->option.scale;
	y0 = y0 * 2 * g->option.scale;
	x1 = x1 * 2 * g->option.scale;
	y1 = y1 * 2 * g->option.scale;

	cx0 = state.xscroll;
	cy0 = -state.yscroll + toolbar_height;
	width = state.visible.x1 - state.visible.x0;
	height = state.visible.y1 - state.visible.y0 - toolbar_height;
	
	/* make sure we're visible */
	correction = (x1 - cx0 - width);
	if (correction > 0)
		cx0 += correction;
	correction = (y1 - cy0 - height);
	if (correction > 0)
		cy0 += correction;
	if (x0 < cx0)
		cx0 = x0;
	if (y0 < cy0)
		cy0 = y0;

	/* try to give a SCROLL_VISIBLE_PADDING border of space around us */
	padding_available = (width - x1 + x0) / 2;
	if (padding_available > 0) {
	  	if (padding_available > SCROLL_VISIBLE_PADDING)
	  		padding_available = SCROLL_VISIBLE_PADDING;
	  	correction = (cx0 + width - x1);
	  	if (correction < padding_available)
	  		cx0 += padding_available;
	  	correction = (x0 - cx0);
	  	if (correction < padding_available)
	  		cx0 -= padding_available;
	}
	padding_available = (height - y1 + y0) / 2;
	LOG(("Padding available: %i", padding_available));
	if (padding_available > 0) {
	  	if (padding_available > SCROLL_VISIBLE_PADDING)
	  		padding_available = SCROLL_VISIBLE_PADDING;
	  	correction = (cy0 + height - y1);
	  	if (correction < padding_available)
	  		cy0 += padding_available;
	  	correction = (y0 - cy0);
	  	if (correction < padding_available)
	  		cy0 -= padding_available;
	}

	state.xscroll = cx0;
	state.yscroll = -cy0 + toolbar_height;
	ro_gui_window_open(g, (wimp_open *)&state);
}


/**
 * Find the current width of a browser window.
 *
 * \param  g  gui_window to measure
 * \return  width of window
 */

int gui_window_get_width(struct gui_window *g)
{
	wimp_window_state state;
	os_error *error;

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return 800;
	}
	return (state.visible.x1 - state.visible.x0) / 2 / g->option.scale;
}


/**
 * Find the current height of a browser window.
 *
 * \param  g  gui_window to measure
 * \return  height of window
 */

int gui_window_get_height(struct gui_window *g)
{
	wimp_window_state state;
	os_error *error;

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return 800;
	}
	return (state.visible.y1 - state.visible.y0 - (g->toolbar ?
			ro_gui_theme_toolbar_full_height(g->toolbar) : 0)) /
			2 / g->option.scale;
}


/**
 * Set the extent of the inside of a browser window.
 *
 * \param  g	   gui_window to resize
 * \param  width   new extent
 * \param  height  new extent
 */

void gui_window_set_extent(struct gui_window *g, int width, int height)
{
	os_box extent = { 0, 0, 0, 0 };
	wimp_window_state state;
	int toolbar_height = 0;
	os_error *error;

	width *= 2 * g->option.scale;
	height *= 2 * g->option.scale;

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* account for toolbar height, if present */
	if (g->toolbar)
		toolbar_height = ro_gui_theme_toolbar_full_height(g->toolbar);

	if (width < state.visible.x1 - state.visible.x0)
		width = state.visible.x1 - state.visible.x0;
	if (height < state.visible.y1 - state.visible.y0 - toolbar_height)
		height = state.visible.y1 - state.visible.y0 - toolbar_height;

	extent.y0 = -height;
	extent.x1 = width;
	extent.y1 = toolbar_height;
	error = xwimp_set_extent(g->window, &extent);
	if (error) {
		LOG(("xwimp_set_extent: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	error = xwimp_open_window((wimp_open *) &state);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Set the status bar of a browser window.
 *
 * \param  g	 gui_window to update
 * \param  text  new status text
 */

void gui_window_set_status(struct gui_window *g, const char *text)
{
	if ((!g->toolbar) || (!g->toolbar->status_handle))
		return;

	ro_gui_set_icon_string(g->toolbar->status_handle,
				ICON_STATUS_TEXT, text);
}


/**
 * Set the contents of a window's address bar.
 *
 * \param  g	gui_window to update
 * \param  url  new url for address bar
 */

void gui_window_set_url(struct gui_window *g, const char *url)
{
	wimp_caret caret;
	os_error *error;
	char *toolbar_url;

	if (!g->toolbar)
		return;

	ro_gui_set_icon_string(g->toolbar->toolbar_handle,
			ICON_TOOLBAR_URL, url);
	ro_gui_force_redraw_icon(g->toolbar->toolbar_handle,
			ICON_TOOLBAR_FAVICON);

	/* if the caret is in the address bar, move it to the end */
	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (!(caret.w == g->toolbar->toolbar_handle &&
			caret.i == ICON_TOOLBAR_URL))
		return;

	toolbar_url = ro_gui_get_icon_string(g->toolbar->toolbar_handle,
			ICON_TOOLBAR_URL);
	error = xwimp_set_caret_position(g->toolbar->toolbar_handle,
			ICON_TOOLBAR_URL, 0, 0, -1, (int)strlen(toolbar_url));
	if (error) {
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	ro_gui_url_complete_start(g);
}


/**
 * Launch a new url in the given window.
 *
 * \param  g    gui_window to update
 * \param  url  url to be launched
 */

void ro_gui_window_launch_url(struct gui_window *g, const char *url)
{
	url_func_result res;
	char *url_norm;

	ro_gui_url_complete_close(NULL, 0);
	res = url_normalize(url, &url_norm);
	if (res == URL_FUNC_OK) {
		gui_window_set_url(g, url_norm);
		browser_window_go(g->bw, url_norm, 0);
		global_history_add_recent(url_norm);
		free(url_norm);
	}
}


/**
 * Forces all windows to be set to the current theme
 *
 * /param g	   the gui window to update
 */
void ro_gui_window_update_theme(void) {
	int height;
	struct gui_window *g;
	for (g = window_list; g; g = g->next) {
		if (g->toolbar) {
			height = ro_gui_theme_toolbar_height(g->toolbar);
			if (g->toolbar->editor)
				if (!ro_gui_theme_update_toolbar(NULL, g->toolbar->editor))
					g->toolbar->editor = NULL;
			if (!ro_gui_theme_update_toolbar(NULL, g->toolbar)) {
				ro_gui_theme_destroy_toolbar(g->toolbar);
				g->toolbar = NULL;
				if (height != 0)
					ro_gui_window_update_dimensions(g, height);
			} else {
				if (height != (ro_gui_theme_toolbar_height(g->toolbar)))
					ro_gui_window_update_dimensions(g, height -
						ro_gui_theme_toolbar_height(g->toolbar));
			}
			ro_gui_theme_toolbar_editor_sync(g->toolbar);
		}
	}
	if ((hotlist_tree) && (hotlist_tree->toolbar)) {
		if (hotlist_tree->toolbar->editor)
			if (!ro_gui_theme_update_toolbar(NULL, hotlist_tree->toolbar->editor))
				hotlist_tree->toolbar->editor = NULL;
		if (!ro_gui_theme_update_toolbar(NULL, hotlist_tree->toolbar)) {
			ro_gui_theme_destroy_toolbar(hotlist_tree->toolbar);
			hotlist_tree->toolbar = NULL;
		}
		ro_gui_theme_toolbar_editor_sync(hotlist_tree->toolbar);
		ro_gui_theme_attach_toolbar(hotlist_tree->toolbar,
				(wimp_w)hotlist_tree->handle);
		xwimp_force_redraw((wimp_w)hotlist_tree->handle,
				0, -16384, 16384, 16384);
	}
	if ((global_history_tree) && (global_history_tree->toolbar)) {
		if (global_history_tree->toolbar->editor)
			if (!ro_gui_theme_update_toolbar(NULL,
					global_history_tree->toolbar->editor))
				global_history_tree->toolbar->editor = NULL;
		if (!ro_gui_theme_update_toolbar(NULL, global_history_tree->toolbar)) {
			ro_gui_theme_destroy_toolbar(global_history_tree->toolbar);
			global_history_tree->toolbar = NULL;
		}
		ro_gui_theme_toolbar_editor_sync(global_history_tree->toolbar);
		ro_gui_theme_attach_toolbar(global_history_tree->toolbar,
				(wimp_w)global_history_tree->handle);
		xwimp_force_redraw((wimp_w)global_history_tree->handle,
				0, -16384, 16384, 16384);
	}
}


/**
 * Forces the windows extent to be updated
 *
 * /param g	   the gui window to update
 * /param yscroll  an amount to scroll the vertical scroll bar by
 */
void ro_gui_window_update_dimensions(struct gui_window *g, int yscroll) {
	os_error *error;
	wimp_window_state state;
	if (!g) return;
	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	state.yscroll -= yscroll;
	g->old_height = -1;
	ro_gui_window_open(g, (wimp_open *)&state);
/*	gui_window_redraw_window(g); */
}

/**
 * Open a window using the given wimp_open, handling toolbars and resizing.
 */

void ro_gui_window_open(struct gui_window *g, wimp_open *open)
{
	int width = open->visible.x1 - open->visible.x0;
	int height = open->visible.y1 - open->visible.y0;
	int toolbar_height = 0;
	struct content *content;
	wimp_window_state state;
	bool toggle_hack = false;
	int screen_height, screen_width;
	os_error *error;
	int key_down = 0;
	int inset = 0;

	content = g->bw->current_content;

	/* check for toggle to full size so we can force to full height for short contents  */
	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if ((state.flags & wimp_WINDOW_TOGGLED) && // bit 19
			(state.flags & wimp_WINDOW_BOUNDED_ONCE) && // bit 21
			!(state.flags & wimp_WINDOW_FULL_SIZE)) { // not bit 18
		ro_gui_screen_size(&screen_width, &screen_height);
		/* i can see no way of easily discovering if we were the result of a shift-
		 * toggle as wimp_WINDOW_PARTIAL_SIZE does not seem to be what we need. As
		 * such we do the really horrible thing of testing for Shift directly and
		 * decreasing the value accordingly. Yuck. */
		xosbyte1(osbyte_SCAN_KEYBOARD, 0 ^ 0x80, 0, &key_down);
		if (key_down != 0)
			inset = 160 + ro_get_hscroll_height(0);
		if ((content->height * 2 * g->option.scale) < screen_height - inset) {
			open->visible.y0 = inset;
			open->visible.y1 = screen_height;
			height = screen_height - inset;
			toggle_hack = true;
		}
	}

	/* account for toolbar height, if present */
	if (g->toolbar)
		toolbar_height = ro_gui_theme_toolbar_full_height(g->toolbar);
	height -= toolbar_height;

	/*	The height should be no less than the content height
	*/
	if (content && height < content->height * 2 * g->option.scale)
		height = content->height * 2 * g->option.scale;

	/* change extent if necessary */
	if (g->old_width != width || g->old_height != height) {
		if (content && g->old_width != width) {
			g->reformat_pending = true;
			gui_reformat_pending = true;
		}

		g->old_width = width;
		g->old_height = height;

		if (content && width < content->width * 2 * g->option.scale)
			width = content->width * 2 * g->option.scale;
		os_box extent = { 0, -height, width, toolbar_height };
		error = xwimp_set_extent(g->window, &extent);
		if (error) {
			LOG(("xwimp_set_extent: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

	}

	/* first resize stops any flickering by making the URL window on top */
	ro_gui_url_complete_resize(g, open);

	error = xwimp_open_window(open);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* update extent to actual size if toggled */
	if (toggle_hack) {
		width = open->visible.x1 - open->visible.x0;
		height = open->visible.y1 - open->visible.y0 - toolbar_height;
		if (content && height < content->height * 2 * g->option.scale)
			height = content->height * 2 * g->option.scale;
		{
			os_box extent = { 0, -height, width, toolbar_height };
			error = xwimp_set_extent(g->window, &extent);
			if (error) {
				LOG(("xwimp_set_extent: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
				return;
			}
		}
		g->old_width = width;
		g->old_height = height;
	}

	if (g->toolbar) {
		ro_gui_theme_process_toolbar(g->toolbar, -1);
		/* second resize updates to the new URL bar width */
		ro_gui_url_complete_resize(g, open);
	}
}


/**
 * Animate the "throbbers" of all browser windows.
 */

void ro_gui_throb(void)
{
	os_t t;
	struct gui_window *g;
	char throb_buf[12];

	xos_read_monotonic_time(&t);

	for (g = window_list; g; g = g->next) {
		if (!g->bw->throbbing || !g->toolbar || !g->toolbar->display_throbber ||
				!g->toolbar->descriptor ||!g->toolbar->descriptor->theme ||
				(t < g->throbtime + 10))
			continue;
		g->throbtime = t;
		g->throbber++;
		if (g->toolbar->descriptor->theme->throbber_frames < g->throbber)
			g->throbber = 1;
		sprintf(throb_buf, "throbber%i", g->throbber);
		ro_gui_set_icon_string(g->toolbar->toolbar_handle,
				ICON_TOOLBAR_THROBBER, throb_buf);
		if (g->toolbar->descriptor->throbber_redraw)
			ro_gui_force_redraw_icon(g->toolbar->toolbar_handle,
					ICON_TOOLBAR_THROBBER);
	}
}


/**
 * Convert a RISC OS window handle to a gui_window.
 *
 * \param  w  RISC OS window handle
 * \return  pointer to a structure if found, 0 otherwise
 */

struct gui_window *ro_gui_window_lookup(wimp_w window)
{
	struct gui_window *g;
	for (g = window_list; g; g = g->next)
		if (g->window == window)
			return g;
	return 0;
}


/**
 * Convert a toolbar RISC OS window handle to a gui_window.
 *
 * \param  w  RISC OS window handle of a toolbar
 * \return  pointer to a structure if found, 0 otherwise
 */

struct gui_window *ro_gui_toolbar_lookup(wimp_w window)
{
	struct gui_window *g;
	for (g = window_list; g; g = g->next) {
		if ((g->toolbar) && ((g->toolbar->toolbar_handle == window) ||
				((g->toolbar->editor) &&
				(g->toolbar->editor->toolbar_handle == window))))
			return g;
	}
	return 0;
}


/**
 * Convert a status bar RISC OS window handle to a gui_window.
 *
 * \param  w  RISC OS window handle of a status bar
 * \return  pointer to a structure if found, 0 otherwise
 */

struct gui_window *ro_gui_status_lookup(wimp_w window)
{
	struct gui_window *g;
	for (g = window_list; g; g = g->next)
		if (g->toolbar && g->toolbar->status_handle == window)
			return g;
	return 0;
}


/**
 * Handle pointer movements in a browser window.
 *
 * \param  g	    browser window that the pointer is in
 * \param  pointer  new mouse position
 */

void ro_gui_window_mouse_at(struct gui_window *g, wimp_pointer *pointer)
{
	int x, y;
	wimp_window_state state;
	os_error *error;

	assert(g);

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	x = window_x_units(pointer->pos.x, &state) / 2 / g->option.scale;
	y = -window_y_units(pointer->pos.y, &state) / 2 / g->option.scale;

	browser_window_mouse_track(g->bw, ro_gui_mouse_drag_state(pointer->buttons), x, y);
}


/**
 * Process Mouse_Click events in a toolbar.
 */

bool ro_gui_toolbar_click(wimp_pointer *pointer)
{
	struct gui_window *g = ro_gui_toolbar_lookup(pointer->w);

	/* toolbars in the options window have no gui_window */
	if (!g)
		return true;

	/* try to close url-completion */
	ro_gui_url_complete_close(g, pointer->i);

	/*	Handle Menu clicks
	*/
	if (pointer->buttons == wimp_CLICK_MENU) {
		ro_gui_menu_create(browser_toolbar_menu, pointer->pos.x,
				pointer->pos.y, g->window);
		return true;
	}

	/*	Handle toolbar edits
	*/
	if ((g->toolbar->editor) && (pointer->i < ICON_TOOLBAR_URL)) {
		ro_gui_theme_toolbar_editor_click(g->toolbar, pointer);
		return true;
	}

	/*	Handle the buttons appropriately
	*/
	switch (pointer->i) {
		case ICON_TOOLBAR_BACK:
		  	ro_gui_menu_handle_action(g->window,
		  			BROWSER_NAVIGATE_BACK, true);
			break;

		case ICON_TOOLBAR_FORWARD:
		  	ro_gui_menu_handle_action(g->window,
		  			BROWSER_NAVIGATE_FORWARD, true);
			break;

		case ICON_TOOLBAR_STOP:
		  	ro_gui_menu_handle_action(g->window,
		  			BROWSER_NAVIGATE_STOP, true);
			break;

		case ICON_TOOLBAR_RELOAD:
			if (pointer->buttons == wimp_CLICK_SELECT)
			  	ro_gui_menu_handle_action(g->window,
			  			BROWSER_NAVIGATE_RELOAD, true);
			else if (pointer->buttons == wimp_CLICK_ADJUST)
			  	ro_gui_menu_handle_action(g->window,
			  			BROWSER_NAVIGATE_RELOAD_ALL, true);
			break;

		case ICON_TOOLBAR_HISTORY:
			if (pointer->buttons == wimp_CLICK_SELECT)
			  	ro_gui_menu_handle_action(g->window,
			  			HISTORY_SHOW_LOCAL, true);
			else
			  	ro_gui_menu_handle_action(g->window,
			  			HISTORY_SHOW_GLOBAL, true);
			break;
		case ICON_TOOLBAR_HOME:
		  	ro_gui_menu_handle_action(g->window,
		  			BROWSER_NAVIGATE_HOME, true);
			break;
#ifdef WITH_SEARCH
		case ICON_TOOLBAR_SEARCH:
			ro_gui_menu_handle_action(g->window,
					BROWSER_FIND_TEXT, true);
			break;
#endif
		case ICON_TOOLBAR_SCALE:
			ro_gui_menu_handle_action(g->window,
					BROWSER_SCALE_VIEW, true);
			break;

		case ICON_TOOLBAR_BOOKMARK:
			if (pointer->buttons == wimp_CLICK_ADJUST)
				ro_gui_menu_handle_action(g->window,
						HOTLIST_ADD_URL, true);
			else
				ro_gui_menu_handle_action(g->window,
						HOTLIST_SHOW, true);
			break;

		case ICON_TOOLBAR_SAVE:
			if (pointer->buttons == wimp_CLICK_ADJUST)
				ro_gui_menu_handle_action(g->window,
						BROWSER_SAVE_COMPLETE, true);
			else
				ro_gui_menu_handle_action(g->window,
						BROWSER_SAVE, true);
			break;
		case ICON_TOOLBAR_PRINT:
		  	ro_gui_menu_handle_action(g->window,
		  			BROWSER_PRINT, true);
			break;
		case ICON_TOOLBAR_URL:
			if (pointer->buttons & (wimp_DRAG_SELECT | wimp_DRAG_ADJUST)) {
				if (g->bw->current_content) {
					gui_save_type save_type;

					if (ro_gui_shift_pressed())
						save_type = GUI_SAVE_LINK_URL;
					else
						save_type = GUI_SAVE_LINK_TEXT;

					gui_drag_save_object(save_type, g->bw->current_content, g);
				}
			}
			else
				ro_gui_url_complete_start(g);
			break;
		case ICON_TOOLBAR_SUGGEST:
			ro_gui_popup_menu(url_suggest_menu,
					g->toolbar->toolbar_handle,
					ICON_TOOLBAR_SUGGEST);
			break;
	}
	return true;
}


/**
 * Handle Mouse_Click events in the status bar.
 *
 * \param  g	    browser window that owns the status bar
 * \param  pointer  details of mouse click
 */

bool ro_gui_status_click(wimp_pointer *pointer)
{
  	struct gui_window *g = ro_gui_status_lookup(pointer->w);
	wimp_drag drag;
	os_error *error;

	assert(g);

	switch (pointer->i) {
		case ICON_STATUS_RESIZE:
			gui_current_drag_type = GUI_DRAG_STATUS_RESIZE;
			drag.w = pointer->w;
			drag.type = wimp_DRAG_SYSTEM_SIZE;
			drag.initial.x0 = pointer->pos.x;
			drag.initial.x1 = pointer->pos.x;
			drag.initial.y0 = pointer->pos.y;
			drag.initial.y1 = pointer->pos.y;
			error = xwimp_drag_box(&drag);
			if (error) {
				LOG(("xwimp_drag_box: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
			}
			break;
	}
	return true;
}


/**
 * Handle Mouse_Click events in a browser window.
 *
 * \param  g	    browser window
 * \param  pointer  details of mouse click
 */

void ro_gui_window_click(struct gui_window *g, wimp_pointer *pointer)
{
	wimp_window_state state;
	os_error *error;
	int x, y;

	assert(g);

	/* try to close url-completion */
	ro_gui_url_complete_close(g, pointer->i);

	state.w = pointer->w;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s", error->errnum,
							 error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	x = window_x_units(pointer->pos.x, &state) / 2 / g->option.scale;
	y = -window_y_units(pointer->pos.y, &state) / 2 / g->option.scale;

	/* set input focus */
	if (pointer->buttons == wimp_CLICK_SELECT ||
			pointer->buttons == wimp_CLICK_ADJUST) {
		error = xwimp_set_caret_position(state.w, -1,
				-100, -100, 32, -1);
		if (error) {
			LOG(("xwimp_set_caret_position: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
	}

	if (pointer->buttons == wimp_CLICK_MENU)
		ro_gui_menu_create(browser_menu, pointer->pos.x,
				pointer->pos.y, pointer->w);
	else
		browser_window_mouse_click(g->bw,
				ro_gui_mouse_click_state(pointer->buttons), x, y);
}


/**
 * Update the interface to reflect start of page loading.
 *
 * \param  g  window with start of load
 */

void gui_window_start_throbber(struct gui_window *g)
{
  	ro_gui_menu_objects_moved();
	ro_gui_prepare_navigate(g);
	xos_read_monotonic_time(&g->throbtime);
	g->throbber = 0;
}



/**
 * Update the interface to reflect page loading stopped.
 *
 * \param  g  window with start of load
 */

void gui_window_stop_throbber(struct gui_window *g)
{
	char throb_buf[12];
	ro_gui_prepare_navigate(g);
	g->throbber = 0;
	if (g->toolbar) {
		strcpy(throb_buf, "throbber0");
		ro_gui_set_icon_string(g->toolbar->toolbar_handle,
				ICON_TOOLBAR_THROBBER, throb_buf);
		if ((g->toolbar->descriptor) && (g->toolbar->descriptor->throbber_redraw))
			ro_gui_force_redraw_icon(g->toolbar->toolbar_handle,
					ICON_TOOLBAR_THROBBER);
	}
}


/**
 * Place the caret in a browser window.
 *
 * \param  g	   window with caret
 * \param  x	   coordinates of caret
 * \param  y	   coordinates of caret
 * \param  height  height of caret
 */

void gui_window_place_caret(struct gui_window *g, int x, int y, int height)
{
	os_error *error;

	error = xwimp_set_caret_position(g->window, -1,
			x * 2 * g->option.scale,
			-(y + height) * 2 * g->option.scale,
			height * 2 * g->option.scale, -1);
	if (error) {
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Remove the caret, if present.
 *
 * \param  g	   window with caret
 */

void gui_window_remove_caret(struct gui_window *g)
{
	wimp_caret caret;
	os_error *error;

	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (caret.w != g->window)
		/* we don't have the caret: do nothing */
		return;

	/* hide caret, but keep input focus */
	error = xwimp_set_caret_position(g->window, -1,
			-100, -100, 32, -1);
	if (error) {
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
}


/**
 * Process Key_Pressed events in a browser window.
 */

bool ro_gui_window_keypress(struct gui_window *g, int key, bool toolbar)
{
	struct content *content = g->bw->current_content;
	wimp_window_state state;
	int y, t_alphabet;
	char *toolbar_url;
	os_error *error;
	wimp_pointer pointer;
	float old_scale;
	static int *ucstable = NULL;
	static int alphabet = 0;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s\n",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	/* In order to make sensible use of the 0x80->0xFF ranges specified
	 * in the RISC OS 8bit alphabets, we must do the following:
	 *
	 * + Read the currently selected alphabet
	 * + Acquire a pointer to the UCS conversion table for this alphabet:
	 *     + Try using ServiceInternational 8 to get the table
	 *     + If that fails, use our internal table (see ucstables.c)
	 * + If the alphabet is not UTF8 and the conversion table exists:
	 *     + Lookup UCS code in the conversion table
	 *     + If code is -1 (i.e. undefined):
	 *         + Use codepoint 0xFFFD instead
	 * + If the alphabet is UTF8, we must buffer input, thus:
	 *     + If the keycode is < 0x80:
	 *         + Handle it directly
	 *     + If the keycode is a UTF8 sequence start:
	 *         + Initialise the buffer appropriately
	 *     + Otherwise:
	 *         + OR in relevant bits from keycode to buffer
	 *         + If we've received an entire UTF8 character:
	 *             + Handle UCS code
	 * + Otherwise:
	 *     + Simply handle the keycode directly, as there's no easy way
	 *       of performing the mapping from keycode -> UCS4 codepoint.
	 */
	error = xosbyte1(osbyte_ALPHABET_NUMBER, 127, 0, &t_alphabet);
	if (error) {
		LOG(("failed reading alphabet: 0x%x: %s",
				error->errnum, error->errmess));
		/* prevent any corruption of ucstable */
		t_alphabet = alphabet;
	}

	if (t_alphabet != alphabet) {
		osbool unclaimed;
		/* Alphabet has changed, so read UCS table location */
		alphabet = t_alphabet;

		error = xserviceinternational_get_ucs_conversion_table(
						alphabet, &unclaimed,
						(void**)&ucstable);
		if (error) {
			LOG(("failed reading UCS conversion table: 0x%x: %s",
					error->errnum, error->errmess));
			/* try using our own table instead */
			ucstable = ucstable_from_alphabet(alphabet);
		}
		if (unclaimed)
			/* Service wasn't claimed so use our own ucstable */
			ucstable = ucstable_from_alphabet(alphabet);
	}

	/* First send the key to the browser window, eg. form fields. */
	if (!toolbar) {
		wchar_t c = (wchar_t)key;
		static wchar_t wc = 0;	/* buffer for UTF8 alphabet */
		static int shift = 0;
		bool ctrl_key = true;

		/* Munge cursor keys into unused control chars */
		/* We can't map onto 1->26 (reserved for ctrl+<qwerty>
		   That leaves 27->31 and 128->159 */

		switch (key) {
			case wimp_KEY_TAB: c = 9; break;
			case wimp_KEY_SHIFT | wimp_KEY_TAB: c = 11; break;

			/* cursor movement keys */
			case wimp_KEY_HOME:
			case wimp_KEY_CONTROL | wimp_KEY_LEFT:
				c = KEY_LINE_START; break;
				break;
			case wimp_KEY_END:
				if (os_version >= RISCOS5)
					c = KEY_LINE_END;
				else
					c = KEY_DELETE_RIGHT;
				break;
			case wimp_KEY_CONTROL | wimp_KEY_RIGHT: c = KEY_LINE_END;   break;
			case wimp_KEY_CONTROL | wimp_KEY_UP:    c = KEY_TEXT_START; break;
			case wimp_KEY_CONTROL | wimp_KEY_DOWN:  c = KEY_TEXT_END;   break;
			case wimp_KEY_SHIFT | wimp_KEY_LEFT:    c = KEY_WORD_LEFT ; break;
			case wimp_KEY_SHIFT | wimp_KEY_RIGHT:   c = KEY_WORD_RIGHT; break;
			case wimp_KEY_SHIFT | wimp_KEY_UP:      c = KEY_PAGE_UP;    break;
			case wimp_KEY_SHIFT | wimp_KEY_DOWN:    c = KEY_PAGE_DOWN;  break;
			case wimp_KEY_LEFT:  c = KEY_LEFT; break;
			case wimp_KEY_RIGHT: c = KEY_RIGHT; break;
			case wimp_KEY_UP:    c = KEY_UP; break;
			case wimp_KEY_DOWN:  c = KEY_DOWN; break;

			/* editing */
			case wimp_KEY_CONTROL | wimp_KEY_END:
				c = KEY_DELETE_LINE_END;
				break;
			case wimp_KEY_DELETE:
				if (ro_gui_ctrl_pressed())
					c = KEY_DELETE_LINE_START;
				else if (os_version < RISCOS5)
					c = KEY_DELETE_LEFT;
				break;

			default:
				ctrl_key = false;
				break;
		}

		if (c < 256) {
			if (ctrl_key)
				/* do nothing, these are our control chars */;
			else if (alphabet != 111 /* UTF8 */ &&
							ucstable != NULL) {
				/* defined in this alphabet? */
				if (ucstable[c] == -1)
					return true;

				/* read UCS4 value out of table */
				c = ucstable[c];
			}
			else if (alphabet == 111 /* UTF8 */) {
				if ((c & 0x80) == 0x00 ||
						(c & 0xC0) == 0xC0) {
					/* UTF8 start sequence */
					if ((c & 0xE0) == 0xC0) {
						wc = ((c & 0x1F) << 6);
						shift = 1;
						return true;
					}
					else if ((c & 0xF0) == 0xE0) {
						wc = ((c & 0x0F) << 12);
						shift = 2;
						return true;
					}
					else if ((c & 0xF8) == 0xF0) {
						wc = ((c & 0x07) << 18);
						shift = 3;
						return true;
					}
					/* These next two have been removed
					 * from RFC3629, but there's no
					 * guarantee that RISC OS won't
					 * generate a UCS4 value outside the
					 * UTF16 plane, so we handle them
					 * anyway. */
					else if ((c & 0xFC) == 0xF8) {
						wc = ((c & 0x03) << 24);
						shift = 4;
					}
					else if ((c & 0xFE) == 0xFC) {
						wc = ((c & 0x01) << 30);
						shift = 5;
					}
					else if (c >= 0x80) {
						/* If this ever happens,
						 * RISC OS' UTF8 keyboard
						 * drivers are broken */
						LOG(("unexpected UTF8 start"
						     " byte %x (ignoring)",
						     c));
						return true;
					}
					/* Anything else is ASCII, so just
					 * handle it directly. */
				}
				else {
					if ((c & 0xC0) != 0x80) {
						/* If this ever happens,
						 * RISC OS' UTF8 keyboard
						 * drivers are broken */
						LOG(("unexpected keycode: "
						     "%x (ignoring)", c));
						return true;
					}

					/* Continuation of UTF8 character */
					wc |= ((c & 0x3F) << (6 * --shift));
					if (shift > 0)
						/* partial character */
						return true;
					else
						/* got entire character, so
						 * fetch from buffer and
						 * handle it */
						c = wc;
				}
			}
			if (browser_window_key_press(g->bw, c))
				return true;
		}
	}

	switch (key) {
		case wimp_KEY_F1:	/* Help. */
			return ro_gui_menu_handle_action(g->window,
					HELP_OPEN_CONTENTS, false);

		case wimp_KEY_CONTROL + wimp_KEY_F1:
			return ro_gui_menu_handle_action(g->window,
					BROWSER_PAGE_INFO, false);

		case wimp_KEY_F2:
			if (!g->toolbar)
				return false;
			ro_gui_url_complete_close(NULL, 0);
			ro_gui_set_icon_string(g->toolbar->toolbar_handle,
					ICON_TOOLBAR_URL, "www.");
			xwimp_set_caret_position(g->toolbar->toolbar_handle,
					ICON_TOOLBAR_URL, 0, 0, -1, 4);
			ro_gui_url_complete_start(g);
			ro_gui_url_complete_keypress(g, wimp_KEY_DOWN);
			return true;

		case wimp_KEY_CONTROL + wimp_KEY_F2:	/* Close window. */
			ro_gui_url_complete_close(NULL, 0);
			browser_window_destroy(g->bw);
			return true;

		case wimp_KEY_F3:
			return ro_gui_menu_handle_action(g->window,
					BROWSER_SAVE, false);

		case wimp_KEY_CONTROL + wimp_KEY_F3:
			return ro_gui_menu_handle_action(g->window,
					BROWSER_EXPORT_TEXT, false);

		case wimp_KEY_SHIFT + wimp_KEY_F3:
			return ro_gui_menu_handle_action(g->window,
					BROWSER_SAVE_COMPLETE, false);

		case wimp_KEY_CONTROL + wimp_KEY_SHIFT + wimp_KEY_F3:
			return ro_gui_menu_handle_action(g->window,
					BROWSER_EXPORT_DRAW, false);

#ifdef WITH_SEARCH
		case wimp_KEY_F4:	/* Search */
			return ro_gui_menu_handle_action(g->window,
					BROWSER_FIND_TEXT, false);
#endif

		case wimp_KEY_F5:	/* Reload */
			return ro_gui_menu_handle_action(g->window,
					BROWSER_NAVIGATE_RELOAD, false);

		case wimp_KEY_CONTROL + wimp_KEY_F5:	/* Full reload */
			return ro_gui_menu_handle_action(g->window,
					BROWSER_NAVIGATE_RELOAD_ALL, false);

		case wimp_KEY_F6:	/* Hotlist */
			return ro_gui_menu_handle_action(g->window,
					HOTLIST_SHOW, false);

		case wimp_KEY_F7:	/* Show local history */
			return ro_gui_menu_handle_action(g->window,
					HISTORY_SHOW_LOCAL, false);

		case wimp_KEY_CONTROL + wimp_KEY_F7:	/* Show global history */
			return ro_gui_menu_handle_action(g->window,
					HISTORY_SHOW_GLOBAL, false);

		case wimp_KEY_F8:	/* View source */
			ro_gui_view_source(content);
			return true;

		case wimp_KEY_F9:	/* Dump content for debugging. */
			switch (content->type) {
				case CONTENT_HTML:
					box_dump(content->data.html.layout, 0);
					break;
				case CONTENT_CSS:
					css_dump_stylesheet(content->data.css.css);
					break;
				default:
					break;
			}
			return true;

		case wimp_KEY_CONTROL + wimp_KEY_SHIFT + wimp_KEY_F9:
			talloc_report_full(0, stderr);
			return true;

		case wimp_KEY_F11:	/* Zoom */
			return ro_gui_menu_handle_action(g->window,
					BROWSER_SCALE_VIEW, false);

		case wimp_KEY_SHIFT + wimp_KEY_F11:	/* Toggle display of box outlines. */
			html_redraw_debug = !html_redraw_debug;
			gui_window_redraw_window(g);
			return true;

		case wimp_KEY_RETURN:
			if (!toolbar)
				break;
			toolbar_url = ro_gui_get_icon_string(g->toolbar->toolbar_handle,
					ICON_TOOLBAR_URL);
			ro_gui_window_launch_url(g, toolbar_url);
			return true;

		case wimp_KEY_ESCAPE:
			if (ro_gui_url_complete_close(NULL, 0)) {
			  	ro_gui_url_complete_start(g);
				return true;
			}
			return ro_gui_menu_handle_action(g->window,
					BROWSER_NAVIGATE_STOP, false);

		case 14:	/* CTRL+N */
			return ro_gui_menu_handle_action(g->window,
					BROWSER_NEW_WINDOW, false);

		case 17:       /* CTRL+Q (Zoom out) */
		case 23:       /* CTRL+W (Zoom in) */
			if (!content)
				break;
			old_scale = g->option.scale;
			if (key == 17) {
				for (int i = SCALE_SNAP_TO_SIZE - 1; i >= 0; i--)
					if (scale_snap_to[i] < old_scale) {
						g->option.scale = scale_snap_to[i];
						break;
					}
			} else {
				for (unsigned int i = 0; i < SCALE_SNAP_TO_SIZE; i++)
					if (scale_snap_to[i] > old_scale) {
						g->option.scale = scale_snap_to[i];
						break;
					}
                        }
			if (g->option.scale < scale_snap_to[0])
				g->option.scale = scale_snap_to[0];
			if (g->option.scale > scale_snap_to[SCALE_SNAP_TO_SIZE - 1])
				g->option.scale =
						scale_snap_to[SCALE_SNAP_TO_SIZE - 1];
			if (old_scale != g->option.scale) {
				g->reformat_pending = true;
				if ((content) && (content->type != CONTENT_HTML))
					browser_window_update(g->bw, false);
				gui_reformat_pending = true;
			}
			return true;

#ifdef WITH_PRINT
		case wimp_KEY_PRINT:
		  	return ro_gui_menu_handle_action(g->window,
		  			BROWSER_PRINT, false);
#endif

		case wimp_KEY_UP:
		case wimp_KEY_DOWN:
		case wimp_KEY_PAGE_UP:
		case wimp_KEY_PAGE_DOWN:
		case wimp_KEY_CONTROL | wimp_KEY_UP:
		case wimp_KEY_CONTROL | wimp_KEY_DOWN:
			if (toolbar)
				return ro_gui_url_complete_keypress(g, key);
			break;
		default:
			if (toolbar)
				return ro_gui_url_complete_keypress(g, key);
			return false;
	}

	state.w = g->window;
	wimp_get_window_state(&state);
	y = state.visible.y1 - state.visible.y0 - 32;
	if (g->toolbar)
		y -= ro_gui_theme_toolbar_full_height(g->toolbar);

	switch (key) {
		case wimp_KEY_UP:
			state.yscroll += 32;
			break;
		case wimp_KEY_DOWN:
			state.yscroll -= 32;
			break;
		case wimp_KEY_PAGE_UP:
			state.yscroll += y;
			break;
		case wimp_KEY_PAGE_DOWN:
			state.yscroll -= y;
			break;
		case wimp_KEY_CONTROL | wimp_KEY_UP:
			state.yscroll = 1000;
			break;
		case wimp_KEY_CONTROL | wimp_KEY_DOWN:
			state.yscroll = -0x10000000;
			break;
	}

		wimp_open_window((wimp_open *) &state);
	return true;
}


/**
 * Process Scroll_Request events.
 */
void ro_gui_scroll_request(wimp_scroll *scroll)
{
	int x, y;
	struct gui_window *g = ro_gui_window_lookup(scroll->w);

	if (!g)
		return;

	x = scroll->visible.x1 - scroll->visible.x0 - 32;
	y = scroll->visible.y1 - scroll->visible.y0 - 32;
	if (g->toolbar)
		y -= ro_gui_theme_toolbar_full_height(g->toolbar);

	switch (scroll->xmin) {
		case wimp_SCROLL_PAGE_LEFT:
			scroll->xscroll -= x;
			break;
		case wimp_SCROLL_COLUMN_LEFT:
			scroll->xscroll -= 32;
			break;
		case wimp_SCROLL_COLUMN_RIGHT:
			scroll->xscroll += 32;
			break;
		case wimp_SCROLL_PAGE_RIGHT:
			scroll->xscroll += x;
			break;
		default:
			break;
	}

	switch (scroll->ymin) {
		case wimp_SCROLL_PAGE_UP:
			scroll->yscroll += y;
			break;
		case wimp_SCROLL_LINE_UP:
			scroll->yscroll += 32;
			break;
		case wimp_SCROLL_LINE_DOWN:
			scroll->yscroll -= 32;
			break;
		case wimp_SCROLL_PAGE_DOWN:
			scroll->yscroll -= y;
			break;
		default:
			break;
	}

	wimp_open_window((wimp_open *) scroll);
}


/**
 * Convert x from screen to window coordinates.
 *
 * \param  x	  x coordinate / os units
 * \param  state  window state
 * \return  x coordinate in window / os units
 */
//#define window_x_units(x, state) (x - (state->visible.x0 - state->xscroll))
int window_x_units(int x, wimp_window_state *state) {
	return x - (state->visible.x0 - state->xscroll);
}


/**
 * Convert y from screen to window coordinates.
 *
 * \param  y	  y coordinate / os units
 * \param  state  window state
 * \return  y coordinate in window / os units
 */
//#define window_y_units(y, state) (y - (state->visible.y1 - state->yscroll))
int window_y_units(int y, wimp_window_state *state) {
	return y - (state->visible.y1 - state->yscroll);
}


/**
 * Convert x,y window co-ordinates into screen co-ordinates.
 *
 * \param  g    gui window
 * \param  x    x ordinate
 * \param  y    y ordinate
 * \param  pos  receives position in screen co-ordinatates
 * \return true iff conversion successful
 */

bool window_screen_pos(struct gui_window *g, int x, int y, os_coord *pos)
{
	wimp_window_state state;
	os_error *error;

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	pos->x = state.visible.x0 + ((x * 2 * g->option.scale)  - state.xscroll);
	pos->y = state.visible.y1 + ((-y * 2 * g->option.scale) - state.yscroll);
	return true;
}


/**
 * Handle Message_DataLoad (file dragged in) for a window.
 *
 * \param  g	    window
 * \param  message  Message_DataLoad block
 * \return  true if the load was processed
 *
 * If the file was dragged into a form file input, it is used as the value.
 */

bool ro_gui_window_dataload(struct gui_window *g, wimp_message *message)
{
	int box_x = 0, box_y = 0;
	int x, y;
	struct box *box;
	struct box *file_box = 0;
	struct box *text_box = 0;
	struct browser_window *bw = g->bw;
	struct content *content;
	wimp_window_state state;
	os_error *error;

	/* HTML content only. */
	if (!bw->current_content || bw->current_content->type != CONTENT_HTML)
		return false;

	/* Ignore directories etc. */
	if (0x1000 <= message->data.data_xfer.file_type)
		return false;

	/* Search for a file input or text area at the drop point. */
	state.w = message->data.data_xfer.w;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s\n",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	x = window_x_units(message->data.data_xfer.pos.x, &state) / 2 /
			g->option.scale;
	y = -window_y_units(message->data.data_xfer.pos.y, &state) / 2 /
			g->option.scale;

	content = bw->current_content;
	box = content->data.html.layout;
	while ((box = box_at_point(box, x, y, &box_x, &box_y, &content))) {
		if (box->style &&
				box->style->visibility == CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->gadget) {
			switch (box->gadget->type) {
				case GADGET_FILE:
					file_box = box;
				break;

				case GADGET_TEXTBOX:
				case GADGET_TEXTAREA:
				case GADGET_PASSWORD:
					text_box = box;
					break;

				default:	/* appease compiler */
					break;
			}
		}
	}

	if (!file_box && !text_box)
		return false;

	if (file_box) {
		utf8_convert_ret ret;
		char *utf8_fn;

		ret = utf8_from_local_encoding(
				message->data.data_xfer.file_name, 0,
				&utf8_fn);
		if (ret != UTF8_CONVERT_OK) {
			/* A bad encoding should never happen */
			assert(ret != UTF8_CONVERT_BADENC);
			LOG(("utf8_from_local_encoding failed"));
			/* Load was for us - just no memory */
			return true;
		}

		/* Found: update form input. */
		free(file_box->gadget->value);
		file_box->gadget->value = utf8_fn;

		/* Redraw box. */
		box_coords(file_box, &x, &y);
		gui_window_redraw(bw->window, x, y,
				x + file_box->width,
				y + file_box->height);
	}
	else if (message->data.data_xfer.file_type == osfile_TYPE_TEXT) {

		const char *filename = message->data.data_xfer.file_name;

		browser_window_mouse_click(g->bw, BROWSER_MOUSE_CLICK_1, x, y);

		if (!ro_gui_window_import_text(g, filename, false))
			return true;  /* it was for us, it just didn't work! */
	}
	else
		return false;	/* only text files allowed in textareas/input fields */

	/* send DataLoadAck */
	message->action = message_DATA_LOAD_ACK;
	message->your_ref = message->my_ref;
	error = xwimp_send_message(wimp_USER_MESSAGE, message, message->sender);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s\n",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	return true;
}


/**
 * Handle Message_DataLoad (file dragged in) for a toolbar
 *
 * \param  g        window
 * \param  message  Message_DataLoad block
 * \return true if the load was processed
 */

bool ro_gui_toolbar_dataload(struct gui_window *g, wimp_message *message)
{
	if (message->data.data_xfer.file_type == osfile_TYPE_TEXT &&
		ro_gui_window_import_text(g, message->data.data_xfer.file_name, true)) {

		os_error *error;

		/* send DataLoadAck */
		message->action = message_DATA_LOAD_ACK;
		message->your_ref = message->my_ref;
		error = xwimp_send_message(wimp_USER_MESSAGE, message, message->sender);
		if (error) {
			LOG(("xwimp_send_message: 0x%x: %s\n",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		return true;
	}
	return false;
}


/**
 * Process pending reformats
 */

void ro_gui_window_process_reformats(void)
{
	struct gui_window *g;

	for (g = window_list; g; g = g->next) {
		if (!g->reformat_pending)
			continue;
		content_reformat(g->bw->current_content,
				g->old_width / 2 / g->option.scale,
				gui_window_get_height(g));
		g->reformat_pending = false;
	}
	gui_reformat_pending = false;
}


/**
 * Clones a browser window's options.
 *
 * \param  new_bw  the new browser window
 * \param  old_bw  the browser window to clone from, or NULL for default
 */

void ro_gui_window_clone_options(struct browser_window *new_bw,
		struct browser_window *old_bw) {
	struct gui_window *old_gui = NULL;
	struct gui_window *new_gui;

	assert(new_bw);

	/*	Get our GUIs
	*/
	new_gui = new_bw->window;

	if (old_bw)
		old_gui = old_bw->window;

	/*	Clone the basic options
	*/
	if (!old_gui) {
		new_gui->option.scale = ((float)option_scale) / 100;
		new_gui->option.background_images = option_background_images;
		new_gui->option.buffer_animations = option_buffer_animations;
		new_gui->option.buffer_everything = option_buffer_everything;
	} else {
		new_gui->option = old_gui->option;
	}

	/*	Set up the toolbar
	*/
	if (new_gui->toolbar) {
		if ((old_gui) && (old_gui->toolbar)) {
			new_gui->toolbar->status_width = old_gui->toolbar->status_width;
			new_gui->toolbar->display_status = old_gui->toolbar->display_status;
			new_gui->toolbar->display_buttons = old_gui->toolbar->display_buttons;
			new_gui->toolbar->display_url = old_gui->toolbar->display_url;
			new_gui->toolbar->display_throbber = old_gui->toolbar->display_throbber;
		} else {
			new_gui->toolbar->status_width = option_toolbar_status_width;
			new_gui->toolbar->display_status = option_toolbar_show_status;
			new_gui->toolbar->display_buttons = option_toolbar_show_buttons;
			new_gui->toolbar->display_url = option_toolbar_show_address;
			new_gui->toolbar->display_throbber = option_toolbar_show_throbber;
		}
		new_gui->toolbar->reformat_buttons = true;
		ro_gui_theme_process_toolbar(new_gui->toolbar, -1);
	}
}


/**
 * Makes a browser window's options the default.
 *
 * \param  bw  the browser window to read options from
 */

void ro_gui_window_default_options(struct browser_window *bw) {
	struct gui_window *gui;

	assert(bw);

	/*	Get our GUI
	*/
	gui = bw->window;
	if (!gui) return;

	/*	Save the basic options
	*/
	option_scale = gui->option.scale * 100;
	option_buffer_animations = gui->option.buffer_animations;
	option_buffer_everything = gui->option.buffer_everything;

	/*	Set up the toolbar
	*/
	if (gui->toolbar) {
		option_toolbar_status_width = gui->toolbar->status_width;
		option_toolbar_show_status = gui->toolbar->display_status;
		option_toolbar_show_buttons = gui->toolbar->display_buttons;
		option_toolbar_show_address = gui->toolbar->display_url;
		option_toolbar_show_throbber = gui->toolbar->display_throbber;
	}
}


/** An entry in ro_gui_pointer_table. */
struct ro_gui_pointer_entry {
	bool wimp_area;  /** The pointer is in the Wimp's sprite area. */
	char sprite_name[12];
	int xactive;
	int yactive;
};

/** Map from gui_pointer_shape to pointer sprite data. Must be ordered as
 * enum gui_pointer_shape. */
struct ro_gui_pointer_entry ro_gui_pointer_table[] = {
	{ true, "ptr_default", 0, 0 },
	{ false, "ptr_point", 6, 0 },
	{ false, "ptr_caret", 4, 9 },
	{ false, "ptr_menu", 6, 4 },
	{ false, "ptr_ud", 6, 7 },
	{ false, "ptr_lr", 7, 6 },
	{ false, "ptr_ld", 7, 7 },
	{ false, "ptr_rd", 7, 7 },
	{ false, "ptr_cross", 7, 7 },
	{ false, "ptr_move", 8, 0 },
};

/**
 * Change mouse pointer shape
 */

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	static gui_pointer_shape curr_pointer = GUI_POINTER_DEFAULT;
	struct ro_gui_pointer_entry *entry;
	os_error *error;

	if (shape == curr_pointer)
		return;

	assert(shape < sizeof ro_gui_pointer_table /
			sizeof ro_gui_pointer_table[0]);

	entry = &ro_gui_pointer_table[shape];

	if (entry->wimp_area) {
		/* pointer in the Wimp's sprite area */
		error = xwimpspriteop_set_pointer_shape(entry->sprite_name,
				1, entry->xactive, entry->yactive, 0, 0);
		if (error) {
			LOG(("xwimpspriteop_set_pointer_shape: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	} else {
		/* pointer in our own sprite area */
		error = xosspriteop_set_pointer_shape(osspriteop_USER_AREA,
				gui_sprites,
				(osspriteop_id) entry->sprite_name,
				1, entry->xactive, entry->yactive, 0, 0);
		if (error) {
			LOG(("xosspriteop_set_pointer_shape: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}

	curr_pointer = shape;
}


/**
 * Remove the mouse pointer from the screen
 */

void gui_window_hide_pointer(gui_window *g)
{
	os_error *error;

	error = xwimpspriteop_set_pointer_shape(NULL, 0x30, 0, 0, 0, 0);
	if (error) {
		LOG(("xwimpspriteop_set_pointer_shape: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Called when the gui_window has new content.
 *
 * \param  g  the gui_window that has new content
 */

void gui_window_new_content(struct gui_window *g)
{
  	ro_gui_menu_objects_moved();
	ro_gui_prepare_navigate(g);
	ro_gui_dialog_close_persistent(g->window);
}


/**
 * Updates the navigation controls for all toolbars;
 *
 * \param  g    the gui_window to launch the URL into
 * \param  url  the URL to launch, or NULL to simply update the suggestion icon
 */
void ro_gui_window_prepare_navigate_all(void) {
  	struct gui_window *g;

	for (g = window_list; g; g = g->next)
		ro_gui_prepare_navigate(g);
}


/**
 * Returns the state of the mouse buttons and modifiers keys for a
 * click/release action, suitable for passing to the OS-independent
 * browser window code
 */

browser_mouse_state ro_gui_mouse_click_state(wimp_mouse_state buttons)
{
	browser_mouse_state state = 0;

	if (buttons & (wimp_CLICK_SELECT)) state |= BROWSER_MOUSE_CLICK_1;
	if (buttons & (wimp_CLICK_ADJUST)) state |= BROWSER_MOUSE_CLICK_2;

	if (buttons & (wimp_DRAG_SELECT)) state |= BROWSER_MOUSE_DRAG_1;
	if (buttons & (wimp_DRAG_ADJUST)) state |= BROWSER_MOUSE_DRAG_2;

	if (ro_gui_shift_pressed()) state |= BROWSER_MOUSE_MOD_1;
	if (ro_gui_ctrl_pressed())  state |= BROWSER_MOUSE_MOD_2;

	return state;
}


/**
 * Returns the state of the mouse buttons and modifiers keys whilst
 * dragging, for passing to the OS-independent browser window code
 */

browser_mouse_state ro_gui_mouse_drag_state(wimp_mouse_state buttons)
{
	browser_mouse_state state = 0;

	if (buttons & (wimp_CLICK_SELECT)) state |= BROWSER_MOUSE_HOLDING_1;
	if (buttons & (wimp_CLICK_ADJUST)) state |= BROWSER_MOUSE_HOLDING_2;

	if (ro_gui_shift_pressed()) state |= BROWSER_MOUSE_MOD_1;
	if (ro_gui_ctrl_pressed())  state |= BROWSER_MOUSE_MOD_2;

	return state;
}


/**
 * Returns true iff one or more Shift keys is held down
 */

bool ro_gui_shift_pressed(void)
{
	int shift = 0;
	xosbyte1(osbyte_SCAN_KEYBOARD, 0 ^ 0x80, 0, &shift);
	return (shift == 0xff);
}


/**
 * Returns true iff one or more Ctrl keys is held down
 */

bool ro_gui_ctrl_pressed(void)
{
	int ctrl = 0;
	xosbyte1(osbyte_SCAN_KEYBOARD, 1 ^ 0x80, 0, &ctrl);
	return (ctrl == 0xff);
}


/**
 * Platform-dependent part of starting a box scrolling operation,
 * for frames and textareas.
 *
 * \param  x0  minimum x ordinate of box relative to mouse pointer
 * \param  y0  minimum y ordinate
 * \param  x1  maximum x ordinate
 * \param  y1  maximum y ordinate
 * \return true iff succesful
 */

bool gui_window_box_scroll_start(struct gui_window *g, int x0, int y0, int x1, int y1)
{
	wimp_pointer pointer;
	os_error *error;
	wimp_drag drag;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	drag.type = wimp_DRAG_USER_POINT;
	drag.bbox.x0 = pointer.pos.x + (int)(x0 * 2 * g->option.scale);
	drag.bbox.y0 = pointer.pos.y + (int)(y0 * 2 * g->option.scale);
	drag.bbox.x1 = pointer.pos.x + (int)(x1 * 2 * g->option.scale);
	drag.bbox.y1 = pointer.pos.y + (int)(y1 * 2 * g->option.scale);

	error = xwimp_drag_box(&drag);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	gui_current_drag_type = GUI_DRAG_SCROLL;
	return true;
}


/**
 * Starts drag scrolling of a browser window
 *
 * \param gw  gui window
 */

bool gui_window_scroll_start(struct gui_window *g)
{
	wimp_window_info_base info;
	wimp_pointer pointer;
	os_error *error;
	wimp_drag drag;
	int height;
	int width;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	info.w = g->window;
	error = xwimp_get_window_info_header_only((wimp_window_info*)&info);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	width  = info.extent.x1 - info.extent.x0;
	height = info.extent.y1 - info.extent.y0;

	drag.type = wimp_DRAG_USER_POINT;
	drag.bbox.x1 = pointer.pos.x + info.xscroll;
	drag.bbox.y0 = pointer.pos.y + info.yscroll;
	drag.bbox.x0 = drag.bbox.x1 - (width  - (info.visible.x1 - info.visible.x0));
	drag.bbox.y1 = drag.bbox.y0 + (height - (info.visible.y1 - info.visible.y0));

	if (g->toolbar) {
		int tbar_height = ro_gui_theme_toolbar_full_height(g->toolbar);
		drag.bbox.y0 -= tbar_height;
		drag.bbox.y1 -= tbar_height;
	}

	error = xwimp_drag_box(&drag);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	gui_current_drag_type = GUI_DRAG_SCROLL;
	return true;
}


/**
 * Completes scrolling of a browser window
 *
 * \param g  gui window
 */

void ro_gui_window_scroll_end(struct gui_window *g, wimp_dragged *drag)
{
	wimp_window_state state;
	wimp_pointer pointer;
	os_error *error;
	int x, y;

	gui_current_drag_type = GUI_DRAG_NONE;

	error = xwimp_drag_box((wimp_drag*)-1);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	error = xwimpspriteop_set_pointer_shape(NULL, 0x31, 0, 0, 0, 0);
	if (error) {
		LOG(("xwimpspriteop_set_pointer_shape: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	x = window_x_units(drag->final.x0, &state) / 2 / g->option.scale;
	y = -window_y_units(drag->final.y0, &state) / 2 / g->option.scale;

	browser_window_mouse_drag_end(g->bw,
		ro_gui_mouse_click_state(pointer.buttons), x, y);
}


/**
 * Alter the scale setting of a window
 *
 * \param  g      gui window
 * \param  scale  scale value (1.0 == normal scale)
 */

void ro_gui_window_set_scale(struct gui_window *g, float scale)
{
	struct content *c;
	g->option.scale = scale;
	g->reformat_pending = true;
	c = g->bw->current_content;
	if ((c) && (c->type != CONTENT_HTML))
		browser_window_update(g->bw, false);
	gui_reformat_pending = true;
}


/**
 * Import text file into window or its toolbar
 *
 * \param  g         gui window containing textarea
 * \param  filename  pathname of file to be imported
 * \param  toolbar   true iff imported to toolbar rather than main window
 * \return true iff successful
 */

bool ro_gui_window_import_text(struct gui_window *g, const char *filename,
		bool toolbar)
{
	fileswitch_object_type obj_type;
	os_error *error;
	char *buf, *utf8_buf;
	int size;
	utf8_convert_ret ret;

	error = xosfile_read_stamped(filename, &obj_type, NULL, NULL,
			&size, NULL, NULL);
	if (error) {
		LOG(("xosfile_read_stamped: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("FileError", error->errmess);
		return true;  /* was for us, but it didn't work! */
	}

	buf = malloc(size);
	if (!buf) {
		warn_user("NoMemory", NULL);
		return true;
	}

	error = xosfile_load_stamped(filename, (byte*)buf,
			NULL, NULL, NULL, NULL, NULL);
	if (error) {
		LOG(("xosfile_load_stamped: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("LoadError", error->errmess);
		free(buf);
		return true;
	}

	ret = utf8_from_local_encoding(buf, size, &utf8_buf);
	if (ret != UTF8_CONVERT_OK) {
		/* bad encoding shouldn't happen */
		assert(ret != UTF8_CONVERT_BADENC);
		LOG(("utf8_from_local_encoding failed"));
		free(buf);
		warn_user("NoMemory", NULL);
		return true;
	}
	size = strlen(utf8_buf);

	if (toolbar) {
		const char *ep = utf8_buf + size;
		const char *sp;
		char *p = utf8_buf;

		/* skip leading whitespace */
		while (isspace(*p)) p++;

		sp = p;
		while (*p && *p != '\r' && *p != '\n')
			p += utf8_next(p, ep - p, 0);
		*p = '\0';

		if (p > sp)
			ro_gui_window_launch_url(g, sp);
	}
	else
		browser_window_paste_text(g->bw, utf8_buf, size, true);

	free(buf);
	free(utf8_buf);
	return true;
}
