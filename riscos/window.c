/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 */

/** \file
 * Browser window handling (implementation).
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include "oslib/colourtrans.h"
#include "oslib/osbyte.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/render/box.h"
#include "netsurf/render/form.h"
#include "netsurf/riscos/buffer.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/thumbnail.h"
#include "netsurf/riscos/treeview.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/messages.h"


/** List of all browser windows. */
static struct gui_window *window_list = 0;
/** GUI window which is being redrawn. Valid only during redraw. */
struct gui_window *ro_gui_current_redraw_gui;
/** GUI window which the current zoom window refers to. */
struct gui_window *ro_gui_current_zoom_gui;


static void ro_gui_window_clone_options(struct browser_window *new_bw,
		struct browser_window *old_bw);



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
	window.xmin = 100;
	window.ymin = 100;
	window.title_data.indirected_text.text = g->title;
	window.title_data.indirected_text.validation = (char *) -1;
	window.title_data.indirected_text.size = 255;
	window.icon_count = 0;
	if (open_centred) {
		scroll_width = ro_get_vscroll_width(g->window);
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
	if (g->toolbar && g->toolbar->display_url)
		error = xwimp_set_caret_position(
				g->toolbar->toolbar_handle,
				ICON_TOOLBAR_URL, -1, -1, -1, 0);
	else
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
	os_error *error;

	assert(g);
	assert(title);

	strncpy(g->title, title, sizeof g->title);
	error = xwimp_force_redraw_title(g->window);
	if (error) {
		LOG(("xwimp_force_redraw_title: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
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
	struct content *c = g->bw->current_content;
	int clip_x0, clip_y0, clip_x1, clip_y1;
	os_error *error;

	plot = ro_plotters;
	ro_plot_set_scale(g->option.scale);

	/*	Set the current redraw gui_window to get options from
	*/
	ro_gui_current_redraw_gui = g;

	/*	We should clear the background, except for HTML.
	*/
	if (!c || c->type != CONTENT_HTML)
		clear_background = true;

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	while (more) {
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
			os_clg();
		}

		if (c) {
			ro_plot_origin_x = redraw->box.x0 - redraw->xscroll;
			ro_plot_origin_y = redraw->box.y1 - redraw->yscroll;
			clip_x0 = (redraw->clip.x0 - ro_plot_origin_x) / 2;
			clip_y0 = (ro_plot_origin_y - redraw->clip.y1) / 2;
			clip_x1 = (redraw->clip.x1 - ro_plot_origin_x) / 2;
			clip_y1 = (ro_plot_origin_y - redraw->clip.y0) / 2;
			content_redraw(c, 0, 0,
					c->width, c->height,
					clip_x0, clip_y0, clip_x1, clip_y1,
					g->option.scale,
					0xFFFFFF);
		}
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
			return;
		}
	}

	/*	Reset the current redraw gui_window to prevent thumbnails from
		retaining options
	*/
	ro_gui_current_redraw_gui = NULL;
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
	bool use_buffer = g->option.buffer_everything;
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
/*	if (data->redraw.full_redraw) */
		use_buffer = use_buffer || g->option.buffer_animations;

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
			return;
		}
	}

	/*	Reset the current redraw gui_window to prevent thumbnails from
		retaining options
	*/
	ro_gui_current_redraw_gui = NULL;
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

	state.xscroll = sx * 2;
	state.yscroll = -sy * 2;
	if (g->toolbar)
		state.yscroll += g->toolbar->height;
	ro_gui_window_open(g, (wimp_open *) &state);
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
		toolbar_height = g->toolbar->height;

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
}

/**
 * Get the contents of a window's address bar.
 *
 * \param  g	gui_window to update
 * \return The url in the address bar or NULL
 */
char *gui_window_get_url(struct gui_window *g)
{
	if (!g->toolbar)
		return NULL;

	return ro_gui_get_icon_string(g->toolbar->toolbar_handle,
			ICON_TOOLBAR_URL);
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
			height = g->toolbar->height;
			if (!ro_gui_theme_update_toolbar(NULL, g->toolbar)) {
				ro_gui_theme_destroy_toolbar(g->toolbar);
				g->toolbar = NULL;
				if (height != 0)
					ro_gui_window_update_dimensions(g, height);
			} else {
				if (height != g->toolbar->height)
					ro_gui_window_update_dimensions(g, height -
						g->toolbar->height);
			}
			ro_gui_prepare_navigate(g);
		}
	}
	if (hotlist_toolbar) {
		if (!ro_gui_theme_update_toolbar(NULL, hotlist_toolbar)) {
			ro_gui_theme_destroy_toolbar(hotlist_toolbar);
			hotlist_toolbar = NULL;
		}
		if (hotlist_tree) {
			ro_gui_theme_attach_toolbar(hotlist_toolbar,
					(wimp_w)hotlist_tree->handle);
			hotlist_tree->offset_y = hotlist_toolbar->height;
			xwimp_force_redraw((wimp_w)hotlist_tree->handle,
					0, -16384, 16384, 16384);
		}
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

	content = g->bw->current_content;

	/* check for toggle to full size - NOW FEATURING "TEMPORARY HACK" */
	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if ((state.flags & wimp_WINDOW_TOGGLED) &&
			(state.flags & wimp_WINDOW_BOUNDED_ONCE) &&
			!(state.flags & wimp_WINDOW_FULL_SIZE)) {
		/*	Check if we need to perform our hack
		*/
		ro_gui_screen_size(&screen_width, &screen_height);
		if ((content->height * 2 * g->option.scale) < screen_height) {
			open->visible.y0 = 0;
			open->visible.y1 = 0x1000;
			height = 0x1000;
			toggle_hack = true;
		}
	}

	/* account for toolbar height, if present */
	if (g->toolbar)
		toolbar_height = g->toolbar->height;
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

	if (g->toolbar)
		ro_gui_theme_process_toolbar(g->toolbar, -1);
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
	for (g = window_list; g; g = g->next)
		if (g->toolbar && g->toolbar->toolbar_handle == window)
			return g;
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

	state.w = pointer->w;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	x = window_x_units(pointer->pos.x, &state) / 2 / g->option.scale;
	y = -window_y_units(pointer->pos.y, &state) / 2 / g->option.scale;

	if (pointer->buttons)
		browser_window_mouse_click(g->bw, BROWSER_MOUSE_DRAG, x, y);
	else
		browser_window_mouse_click(g->bw, BROWSER_MOUSE_HOVER, x, y);
}


/**
 * Process Mouse_Click events in a toolbar.
 */

void ro_gui_toolbar_click(struct gui_window *g, wimp_pointer *pointer)
{
	struct node *node;
	char url[80];

	/*	Store the toolbar
	*/
	current_toolbar = g->toolbar;

	/*	Handle Menu clicks
	*/
	if (pointer->buttons == wimp_CLICK_MENU) {
		ro_gui_create_menu(toolbar_menu, pointer->pos.x,
				pointer->pos.y, g);
		return;
	}

	/*	Handle the buttons appropriately
	*/
	switch (pointer->i) {
		case ICON_TOOLBAR_BACK:
			history_back(g->bw, g->bw->history);
			break;
		case ICON_TOOLBAR_FORWARD:
			history_forward(g->bw, g->bw->history);
			break;

		case ICON_TOOLBAR_STOP:
			browser_window_stop(g->bw);
			break;

		case ICON_TOOLBAR_RELOAD:
			if (pointer->buttons == wimp_CLICK_SELECT)
				browser_window_reload(g->bw, false);
			else if (pointer->buttons == wimp_CLICK_ADJUST)
				browser_window_reload(g->bw, true);
			break;

		case ICON_TOOLBAR_HISTORY:
			ro_gui_history_open(g->bw,
					g->bw->history,
					pointer->pos.x, pointer->pos.y);
			break;
		case ICON_TOOLBAR_HOME:
			if (option_homepage_url && option_homepage_url[0]) {
				if (pointer->buttons == wimp_CLICK_SELECT) {
					browser_window_go_post(g->bw, option_homepage_url,
							0, 0, true, 0, false);
				} else {
					browser_window_create(option_homepage_url, NULL, 0);
				}
			} else {
				snprintf(url, sizeof url,
						"file:/<NetSurf$Dir>/Docs/intro_%s",
						option_language);
				if (pointer->buttons == wimp_CLICK_SELECT) {
					browser_window_go_post(g->bw, url, 0, 0, true, 0, false);
				} else {
					browser_window_create(url, NULL, 0);
				}
			}
			break;
#ifdef WITH_SEARCH
		case ICON_TOOLBAR_SEARCH:
			ro_gui_search_open(g, 0, 0, false, true);
			break;
#endif
		case ICON_TOOLBAR_SCALE:
			current_gui = g;
			ro_gui_menu_prepare_scale();
			ro_gui_dialog_open_persistant(g->window, dialog_zoom, true);
			break;

		case ICON_TOOLBAR_BOOKMARK:
			if ((pointer->buttons == wimp_CLICK_ADJUST) && (hotlist_tree)) {
				node = tree_create_URL_node(hotlist_tree->root,
						messages_get(g->title),
						g->bw->current_content->url,
						ro_content_filetype(g->bw->current_content),
						time(NULL), -1, 0);
				tree_redraw_area(hotlist_tree, node->box.x - NODE_INSTEP, 0,
						NODE_INSTEP, 16384);
				tree_handle_node_changed(hotlist_tree, node, false, true);
				ro_gui_tree_scroll_visible(hotlist_tree, &node->data);
			} else if (hotlist_tree) {
				ro_gui_hotlist_show();
			}
			break;

		case ICON_TOOLBAR_SAVE:
			current_gui = g;
			if (g->bw->current_content)
				ro_gui_save_open(GUI_SAVE_SOURCE,
						g->bw->current_content,
						false, 0, 0, g->window, false);
			break;
		case ICON_TOOLBAR_PRINT:
			current_gui = g;
			ro_gui_print_open(g, 0, 0, false, false);
			break;
	}
}


/**
 * Handle Mouse_Click events in the status bar.
 *
 * \param  g	    browser window that owns the status bar
 * \param  pointer  details of mouse click
 */

void ro_gui_status_click(struct gui_window *g, wimp_pointer *pointer)
{
	wimp_drag drag;
	os_error *error;
	switch (pointer->i) {
		case ICON_STATUS_RESIZE:
			gui_current_drag_type = GUI_DRAG_STATUS_RESIZE;
			drag.w = g->toolbar->status_handle;
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
}


/**
 * Handle Mouse_Click events in a browser window.
 *
 * \param  g	    browser window
 * \param  pointer  details of mouse click
 */

void ro_gui_window_click(struct gui_window *g, wimp_pointer *pointer)
{
	int x, y, shift;
	wimp_window_state state;
	os_error *error;

	assert(g);

	xosbyte1(osbyte_SCAN_KEYBOARD, 0 ^ 0x80, 0, &shift);

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
		ro_gui_create_menu(browser_menu, pointer->pos.x,
				pointer->pos.y, g);
	else if (pointer->buttons == wimp_CLICK_SELECT)
		browser_window_mouse_click(g->bw,
			(shift == 0xff) ? BROWSER_MOUSE_CLICK_1_MOD
					: BROWSER_MOUSE_CLICK_1, x, y);
	else if (pointer->buttons == wimp_CLICK_ADJUST)
		browser_window_mouse_click(g->bw,
			(shift == 0xff) ? BROWSER_MOUSE_CLICK_2_MOD
					: BROWSER_MOUSE_CLICK_2, x, y);
}


/**
 * Update the interface to reflect start of page loading.
 *
 * \param  g  window with start of load
 */

void gui_window_start_throbber(struct gui_window *g)
{
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
 * Remove/disown the caret.
 *
 * \param  g	   window with caret
 *
 * \todo: do we want to do a test if g really owns the caret ?
 */

void gui_window_remove_caret(struct gui_window *g)
{
	os_error *error;

	error = xwimp_set_caret_position((wimp_w)-1, (wimp_i)-1,
			0,
			0,
			0, -1);
	if (error) {
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Process Key_Pressed events in a browser window.
 */

bool ro_gui_window_keypress(struct gui_window *g, int key, bool toolbar)
{
	struct content *content = g->bw->current_content;
	wimp_window_state state;
	int y;
	char *url;
	char *toolbar_url;
	os_error *error;
	wimp_pointer pointer;
	url_func_result res;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s\n",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	/* First send the key to the browser window, eg. form fields. */
	if (!toolbar) {
		wchar_t c = (wchar_t)key;
		/* Munge cursor keys into unused control chars */
		/* We can't map on to any of: 3,8,10,13,17,18,21,22,23 or 24
		 * That leaves 1,2,4-7,11,12,14-16,18-20,25-31 and 129-159
		 */
		if (c == 394) c = 9;	       /* Tab */
		else if (c == 410) c = 11;     /* Shift+Tab */
		else if (c == 428) c = 26;     /* Ctrl+Left */
		else if (c == 429) c = 27;     /* Ctrl+Right*/
		else if (c == 396) c = 29;     /* Left */
		else if (c == 397) c = 28;     /* Right */
		else if (c == 398) c = 31;     /* Down */
		else if (c == 399) c = 30;     /* Up */
		if (c < 256)
			if (browser_window_key_press(g->bw, c))
				return true;
	}

	switch (key) {
		case wimp_KEY_F1:	/* Help. */
			ro_gui_open_help_page("docs");
			return true;

		case wimp_KEY_CONTROL + wimp_KEY_F1:
			current_gui = g;
			ro_gui_menu_prepare_pageinfo();
			ro_gui_dialog_open_persistant(g->window, dialog_pageinfo, false);
			return true;

		case wimp_KEY_F2:
			if (!g->toolbar)
				return false;
			ro_gui_set_icon_string(g->toolbar->toolbar_handle,
					ICON_TOOLBAR_URL, "www.");
			xwimp_set_caret_position(g->toolbar->toolbar_handle,
					ICON_TOOLBAR_URL, 0, 0, -1, 4);
			return true;

		case wimp_KEY_CONTROL + wimp_KEY_F2:	/* Close window. */
			browser_window_destroy(g->bw);
			return true;

		case wimp_KEY_F3:
			current_gui = g;
			ro_gui_save_open(GUI_SAVE_SOURCE, content,
					false, 0, 0, g->window, true);
			return true;

		case wimp_KEY_CONTROL + wimp_KEY_F3:
			current_gui = g;
			ro_gui_save_open(GUI_SAVE_TEXT, content,
					false, 0, 0, g->window, true);
			return true;

		case wimp_KEY_SHIFT + wimp_KEY_F3:
			current_gui = g;
			ro_gui_save_open(GUI_SAVE_COMPLETE, content,
					false, 0, 0, g->window, true);
			return true;

		case wimp_KEY_CONTROL + wimp_KEY_SHIFT + wimp_KEY_F3:
			current_gui = g;
			ro_gui_save_open(GUI_SAVE_DRAW, content,
					false, 0, 0, g->window, true);
			return true;

#ifdef WITH_SEARCH
		case wimp_KEY_F4:	/* Search */
			ro_gui_search_open(g, 0, 0, false, true);
			return true;
#endif

		case wimp_KEY_F5:	/* Refresh. */
			browser_window_reload(g->bw, false);
			return true;

		case wimp_KEY_F6:	/* Hotlist. */
			ro_gui_hotlist_show();
			return true;

		case wimp_KEY_F7:	/* Toggle fullscreen browsing. */



			return true;


		case wimp_KEY_F8:	/* View source. */
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

		case wimp_KEY_F11:	/* Zoom */
			current_gui = g;
			ro_gui_menu_prepare_scale();
			ro_gui_dialog_open_persistant(g->window, dialog_zoom, false);
			return true;

		case wimp_KEY_SHIFT + wimp_KEY_F11:	/* Toggle display of box outlines. */
			html_redraw_debug = !html_redraw_debug;
			gui_window_redraw_window(g);
			return true;

		case wimp_KEY_RETURN:
			if (!toolbar)
				break;
			toolbar_url = ro_gui_get_icon_string(g->toolbar->toolbar_handle,
					ICON_TOOLBAR_URL);
			res = url_normalize(toolbar_url, &url);
			if (res == URL_FUNC_OK) {
				gui_window_set_url(g, url);
				browser_window_go(g->bw, url, 0, false);
				free(url);
			}
			return true;

		case wimp_KEY_ESCAPE:
			browser_window_stop(g->bw);
			return true;

		case 14:	/* CTRL+N */
			current_gui = g;
			browser_window_create(current_gui->bw->current_content->url,
					current_gui->bw, 0);
			return true;
		case 18:	/* CTRL+R */
			browser_window_reload(g->bw, false);
			return true;

		case 17:       /* CTRL+Q (Zoom out) */
			current_gui = g;
			if (0.1 < current_gui->option.scale) {
				current_gui->option.scale -= 0.1;
				if (current_gui->option.scale < 0.1)
					current_gui->option.scale = 0.1;
				current_gui->reformat_pending = true;
				gui_reformat_pending = true;
			}
			return true;
		case 23:       /* CTRL+W (Zoom in) */
			current_gui = g;
			if (current_gui->option.scale < 10.0) {
				current_gui->option.scale += 0.1;
				if (10.0 < current_gui->option.scale)
					current_gui->option.scale = 10.0;
				current_gui->reformat_pending = true;
				gui_reformat_pending = true;
			}
			return true;

#ifdef WITH_PRINT
		case wimp_KEY_PRINT:
			ro_gui_print_open(g, 0, 0, false, true);
			return true;
#endif

		case wimp_KEY_UP:
		case wimp_KEY_DOWN:
		case wimp_KEY_PAGE_UP:
		case wimp_KEY_PAGE_DOWN:
		case wimp_KEY_CONTROL | wimp_KEY_UP:
		case wimp_KEY_CONTROL | wimp_KEY_DOWN:
			break;

		default:
			return false;
	}

	state.w = g->window;
	wimp_get_window_state(&state);
	y = state.visible.y1 - state.visible.y0 - 32;
	y -= g->toolbar->height;

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
	y -= g->toolbar->height;

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

	/* Search for a file input at the drop point. */
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

		if (box->gadget && box->gadget->type == GADGET_FILE)
			file_box = box;
	}

	if (!file_box)
		return false;

	/* Found: update form input. */
	free(file_box->gadget->value);
	file_box->gadget->value =
			strdup(message->data.data_xfer.file_name);

	/* Redraw box. */
	box_coords(file_box, &x, &y);
	gui_window_redraw(bw->window, x, y,
			x + file_box->width,
			y + file_box->height);

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
				1000);
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
		new_gui->option.background_blending = option_background_blending;
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
	option_background_blending = gui->option.background_blending;
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

void gui_window_set_pointer(gui_pointer_shape shape)
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
 * Called when the gui_window has new content.
 *
 * \param  g  the gui_window that has new content
 */

void gui_window_new_content(struct gui_window *g)
{
	ro_gui_prepare_navigate(g);
	ro_gui_dialog_close_persistant(g->window);
}
