/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
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

#include <assert.h>
#include <string.h>
#include "oslib/font.h"
#include "oslib/hourglass.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/pdriver.h"
#include "oslib/wimp.h"
#include "rufl.h"
#include "utils/config.h"
#include "content/content.h"
#include "desktop/plotters.h"
#include "render/box.h"
#include "render/font.h"
#include "render/html.h"
#include "render/layout.h"
#include "riscos/dialog.h"
#include "riscos/menus.h"
#include "riscos/print.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"


#define ICON_PRINT_TO_BOTTOM 1
#define ICON_PRINT_SHEETS 2
#define ICON_PRINT_SHEETS_VALUE 3
#define ICON_PRINT_SHEETS_DOWN 4
#define ICON_PRINT_SHEETS_UP 5
#define ICON_PRINT_SHEETS_TEXT 6
#define ICON_PRINT_FG_IMAGES 7
#define ICON_PRINT_BG_IMAGES 8
#define ICON_PRINT_IN_BACKGROUND 9
#define ICON_PRINT_UPRIGHT 10
#define ICON_PRINT_SIDEWAYS 11
#define ICON_PRINT_COPIES 12
#define ICON_PRINT_COPIES_DOWN 13
#define ICON_PRINT_COPIES_UP 14
#define ICON_PRINT_CANCEL 15
#define ICON_PRINT_PRINT 16
#define ICON_PRINT_TEXT_BLACK 20


/** \todo landscape format pages
 *  \todo be somewhat more intelligent and try not to crop pages
 *        half way up a line of text
 *  \todo make use of print stylesheets
 */

struct gui_window *print_current_window = 0;
bool print_text_black = false;
bool print_active = false;

#ifdef WITH_PRINT

/* 1 millipoint == 1/400 OS unit == 1/800 browser units */

static int print_prev_message = 0;
static bool print_in_background = false;
static float print_scale = 1.0;
static int print_num_copies = 1;
static bool print_bg_images = false;
static int print_max_sheets = -1;
/** List of fonts in current print. */
static char **print_fonts_list = 0;
/** Number of entries in print_fonts_list. */
static unsigned int print_fonts_count;
/** Error in print_fonts_plot_text() or print_fonts_callback(). */
static const char *print_fonts_error;


static bool ro_gui_print_click(wimp_pointer *pointer);
static bool ro_gui_print_apply(wimp_w w);
static void print_update_sheets_shaded_state(bool on);
static void print_send_printsave(struct content *c);
static bool print_send_printtypeknown(wimp_message *m);
static bool print_document(struct gui_window *g, const char *filename);
static const char *print_declare_fonts(struct content *content);
static bool print_fonts_plot_clg(colour c);
static bool print_fonts_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed);
static bool print_fonts_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed);
static bool print_fonts_plot_polygon(int *p, unsigned int n, colour fill);
static bool print_fonts_plot_fill(int x0, int y0, int x1, int y1, colour c);
static bool print_fonts_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1);
static bool print_fonts_plot_text(int x, int y, const struct css_style *style,
		const char *text, size_t length, colour bg, colour c);
static bool print_fonts_plot_disc(int x, int y, int radius, colour c,
                bool filled);
static bool print_fonts_plot_arc(int x, int y, int radius, int angle1, int angle2,
		colour c);
static bool print_fonts_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		struct content *content);
static bool print_fonts_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y, struct content *content);
static bool print_fonts_plot_group_start(const char *name);
static bool print_fonts_plot_group_end(void);
static void print_fonts_callback(void *context,
		const char *font_name, unsigned int font_size,
		const char *s8, unsigned short *s16, unsigned int n,
		int x, int y);


/** Plotter for print_declare_fonts(). All the functions do nothing except for
 * print_fonts_plot_text, which records the fonts used. */
static const struct plotter_table print_fonts_plotters = {
	print_fonts_plot_clg,
	print_fonts_plot_rectangle,
	print_fonts_plot_line,
	print_fonts_plot_polygon,
	print_fonts_plot_fill,
	print_fonts_plot_clip,
	print_fonts_plot_text,
	print_fonts_plot_disc,
	print_fonts_plot_arc,
	print_fonts_plot_bitmap,
	print_fonts_plot_bitmap_tile,
	print_fonts_plot_group_start,
	print_fonts_plot_group_end,
	NULL,
	NULL
};


/**
 * Initialise the print dialog.
 */

void ro_gui_print_init(void)
{
  	wimp_i radio_print_type[] = {ICON_PRINT_TO_BOTTOM, ICON_PRINT_SHEETS,
  			-1};
  	wimp_i radio_print_orientation[] = {ICON_PRINT_UPRIGHT,
  			ICON_PRINT_SIDEWAYS, -1};

	dialog_print = ro_gui_dialog_create("print");
	ro_gui_wimp_event_register_radio(dialog_print, radio_print_type);
	ro_gui_wimp_event_register_radio(dialog_print, radio_print_orientation);
	ro_gui_wimp_event_register_checkbox(dialog_print, ICON_PRINT_FG_IMAGES);
	ro_gui_wimp_event_register_checkbox(dialog_print, ICON_PRINT_BG_IMAGES);
	ro_gui_wimp_event_register_checkbox(dialog_print,
			ICON_PRINT_IN_BACKGROUND);
	ro_gui_wimp_event_register_checkbox(dialog_print,
			ICON_PRINT_TEXT_BLACK);
	ro_gui_wimp_event_register_text_field(dialog_print,
			ICON_PRINT_SHEETS_TEXT);
	ro_gui_wimp_event_register_numeric_field(dialog_print,
			ICON_PRINT_COPIES, ICON_PRINT_COPIES_UP,
			ICON_PRINT_COPIES_DOWN, 1, 99, 1, 0);
	ro_gui_wimp_event_register_numeric_field(dialog_print,
			ICON_PRINT_SHEETS_VALUE, ICON_PRINT_SHEETS_UP,
			ICON_PRINT_SHEETS_DOWN, 1, 99, 1, 0);
	ro_gui_wimp_event_register_cancel(dialog_print, ICON_PRINT_CANCEL);
	ro_gui_wimp_event_register_mouse_click(dialog_print,
			ro_gui_print_click);
	ro_gui_wimp_event_register_ok(dialog_print, ICON_PRINT_PRINT,
			ro_gui_print_apply);
	ro_gui_wimp_event_set_help_prefix(dialog_print, "HelpPrint");
}


/**
 * Prepares all aspects of the print dialog prior to opening.
 *
 * \param g parent window
 */

void ro_gui_print_prepare(struct gui_window *g)
{
	char *desc;
	bool printers_exists = true;
	os_error *error;

	assert(g);

	print_current_window = g;
	print_prev_message = 0;

	/* Read Printer Driver name */
	error = xpdriver_info(0, 0, 0, 0, &desc, 0, 0, 0);
	if (error) {
		LOG(("xpdriver_info: 0x%x: %s", error->errnum, error->errmess));
		printers_exists = false;
	}

	print_bg_images = g->option.background_images;

	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_TO_BOTTOM,
			true);

	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_SHEETS, false);
	ro_gui_set_icon_integer(dialog_print, ICON_PRINT_SHEETS_VALUE, 1);
	print_update_sheets_shaded_state(true);

	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_FG_IMAGES,
			true);
	ro_gui_set_icon_shaded_state(dialog_print, ICON_PRINT_FG_IMAGES, true);

	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_BG_IMAGES,
			print_bg_images);

	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_IN_BACKGROUND,
			false);

	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_UPRIGHT, true);
	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_SIDEWAYS,
			false);

	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_TEXT_BLACK,
			false);

	ro_gui_set_icon_integer(dialog_print, ICON_PRINT_COPIES, 1);

	ro_gui_set_icon_shaded_state(dialog_print, ICON_PRINT_PRINT,
			!printers_exists);
	if (printers_exists)
		ro_gui_set_window_title(dialog_print, desc);

	ro_gui_wimp_event_memorise(dialog_print);
}


/**
 * Handle mouse clicks in print dialog
 *
 * \param pointer wimp_pointer block
 */

bool ro_gui_print_click(wimp_pointer *pointer)
{
	if (pointer->buttons == wimp_CLICK_MENU)
		return true;

	switch (pointer->i) {
		case ICON_PRINT_TO_BOTTOM:
		case ICON_PRINT_SHEETS:
			print_update_sheets_shaded_state(pointer->i !=
					ICON_PRINT_SHEETS);
			break;
	}
	return false;
}


/**
 * Handle click on the Print button in the print dialog.
 */

bool ro_gui_print_apply(wimp_w w)
{
	int copies = atoi(ro_gui_get_icon_string(dialog_print,
						ICON_PRINT_COPIES));
	int sheets = atoi(ro_gui_get_icon_string(dialog_print,
						ICON_PRINT_SHEETS_VALUE));

	print_in_background = ro_gui_get_icon_selected_state(dialog_print,
			ICON_PRINT_IN_BACKGROUND);
	print_text_black = ro_gui_get_icon_selected_state(dialog_print,
			ICON_PRINT_TEXT_BLACK);
	print_num_copies = copies;
	if (ro_gui_get_icon_selected_state(dialog_print, ICON_PRINT_SHEETS))
		print_max_sheets = sheets;
	else
		print_max_sheets = -1;
	print_current_window->option.background_images =
			ro_gui_get_icon_selected_state(dialog_print,
					ICON_PRINT_BG_IMAGES);

	print_send_printsave(print_current_window->bw->current_content);

	return true;
}


/**
 * Set shaded state of sheets
 *
 * \param on whether to turn shading on or off
 */

void print_update_sheets_shaded_state(bool on)
{
	ro_gui_set_icon_shaded_state(dialog_print, ICON_PRINT_SHEETS_VALUE, on);
	ro_gui_set_icon_shaded_state(dialog_print, ICON_PRINT_SHEETS_DOWN, on);
	ro_gui_set_icon_shaded_state(dialog_print, ICON_PRINT_SHEETS_UP, on);
	ro_gui_set_icon_shaded_state(dialog_print, ICON_PRINT_SHEETS_TEXT, on);
	ro_gui_set_caret_first(dialog_print);
}


/**
 * Send a message_PRINT_SAVE
 *
 * \param c content to print
 */

void print_send_printsave(struct content *c)
{
	wimp_full_message_data_xfer m;
	os_error *e;
	int len;

	len = strlen(c->title) + 1;
	if (212 < len)
		len = 212;

	m.size = ((44+len+3) & ~3);
	m.your_ref = 0;
	m.action = message_PRINT_SAVE;
	m.w = (wimp_w)0;
	m.i = m.pos.x = m.pos.y = 0;
	m.est_size = 1024; /* arbitrary value - it really doesn't matter */
	m.file_type = ro_content_filetype(c);
	strncpy(m.file_name, c->title, 211);
	m.file_name[211] = 0;
	e = xwimp_send_message(wimp_USER_MESSAGE_RECORDED,
			(wimp_message *)&m, 0);
	if (e) {
		LOG(("xwimp_send_message: 0x%x: %s",
				e->errnum, e->errmess));
		warn_user("WimpError", e->errmess);
		print_cleanup();
	}
	print_prev_message = m.my_ref;
}


/**
 * Send a message_PRINT_TYPE_KNOWN
 *
 * \param m message to reply to
 * \return true on success, false otherwise
 */

bool print_send_printtypeknown(wimp_message *m)
{
	os_error *e;

	m->size = 20;
	m->your_ref = m->my_ref;
	m->action = message_PRINT_TYPE_KNOWN;
	e = xwimp_send_message(wimp_USER_MESSAGE, m, m->sender);
	if (e) {
		LOG(("xwimp_send_message: 0x%x: %s",
				e->errnum, e->errmess));
		warn_user("WimpError", e->errmess);
		return false;
	}

	return true;
}


/**
 * Handle a bounced message_PRINT_SAVE
 *
 * \param m the bounced message
 */

void print_save_bounce(wimp_message *m)
{
	if (m->my_ref == 0 || m->my_ref != print_prev_message)
		return;

	/* try to print anyway (we're graphics printing) */
	if (print_current_window) {
		print_document(print_current_window, "printer:");
	}
	print_cleanup();
}


/**
 * Handle message_PRINT_ERROR
 *
 * \param m the message containing the error
 */

void print_error(wimp_message *m)
{
	pdriver_message_print_error *p = (pdriver_message_print_error*)&m->data;
	if (m->your_ref == 0 || m->your_ref != print_prev_message)
		return;

	if (m->size == 20)
		warn_user("PrintErrorRO2", 0);
	else
		warn_user("PrintError", p->errmess);

	print_cleanup();
}


/**
 * Handle message_PRINT_TYPE_ODD
 *
 * \param m the message to handle
 */

void print_type_odd(wimp_message *m)
{
	if ((m->your_ref == 0 || m->your_ref == print_prev_message) &&
						!print_in_background) {
		/* reply to a previous message (ie printsave) */
		if (print_current_window && print_send_printtypeknown(m)) {
			print_document(print_current_window, "printer:");
		}
		print_cleanup();
	}
	else {
		/* broadcast message */
		/* no need to do anything */
	}

}


/**
 * Handle message_DATASAVE_ACK for the printing protocol.
 *
 * \param  m  the message to handle
 * \return true if message successfully handled, false otherwise
 *
 * We cheat here and, instead of giving Printers what it asked for (a copy of
 * the file so it can poke us later via a broadcast of PrintTypeOdd), we give
 * it a file that it can print itself without having to bother us further. For
 * PostScript printers (type 0) we give it a PostScript file. Otherwise, we give
 * it a PrintOut file.
 *
 * This method has a couple of advantages:
 * - we can reuse this code for background printing (we simply ignore the
 *   PrintTypeOdd reply)
 * - there's no need to ensure all components of a page queued to be printed
 *   still exist when it reaches the top of the queue. (which reduces complexity
 *   a fair bit)
 */

bool print_ack(wimp_message *m)
{
	pdriver_info_type info_type;
	pdriver_type type;
	os_error *error;

	if (m->your_ref == 0 || m->your_ref != print_prev_message ||
			!print_current_window)
		return false;

	/* read printer driver type */
	error = xpdriver_info(&info_type, 0, 0, 0, 0, 0, 0, 0);
	if (error) {
		LOG(("xpdriver_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("PrintError", error->errmess);
		print_cleanup();
		return true;
	}
	type = info_type >> 16;

	/* print to file */
	if (!print_document(print_current_window,
			m->data.data_xfer.file_name)) {
		print_cleanup();
		return true;
	}

	/* send dataload */
	m->your_ref = m->my_ref;
	m->action = message_DATA_LOAD;

	if (type == pdriver_TYPE_PS)
		m->data.data_xfer.file_type = osfile_TYPE_POSTSCRIPT;
	else
		m->data.data_xfer.file_type = osfile_TYPE_PRINTOUT;

	error = xwimp_send_message(wimp_USER_MESSAGE_RECORDED, m, m->sender);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		/* and delete temporary file */
		xosfile_delete(m->data.data_xfer.file_name,
				0, 0, 0, 0, 0);
	}
	print_prev_message = m->my_ref;

	print_cleanup();
	return true;
}


/**
 * Handle a bounced dataload message
 *
 * \param m the message to handle
 */

void print_dataload_bounce(wimp_message *m)
{
	if (m->your_ref == 0 || m->your_ref != print_prev_message)
		return;

	xosfile_delete(m->data.data_xfer.file_name, 0, 0, 0, 0, 0);
	print_cleanup();
}


/**
 * Cleanup after printing
 */

void print_cleanup(void)
{
	if (print_current_window)
		print_current_window->option.background_images =
							print_bg_images;
	print_current_window = 0;
	print_text_black = false;
	print_prev_message = 0;
	print_max_sheets = -1;
	ro_gui_menu_closed(true);
	ro_gui_dialog_close(dialog_print);
}


/**
 * Print a document.
 *
 * \param  g         gui_window containing the document to print
 * \param  filename  name of file to print to
 * \return true on success, false on error and error reported
 */

bool print_document(struct gui_window *g, const char *filename)
{
	int left, right, top, bottom, width, height;
	int saved_width, saved_height;
	int yscroll = 0, sheets = print_max_sheets;
	struct content *c = g->bw->current_content;
	const char *error_message;
	pdriver_features features;
	os_fw fhandle, old_job = 0;
	os_error *error;

	/* no point printing a blank page */
	if (!c) {
		warn_user("PrintError", "nothing to print");
		return false;
	}

	/* read printer driver features */
	error = xpdriver_info(0, 0, 0, &features, 0, 0, 0, 0);
	if (error) {
		LOG(("xpdriver_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("PrintError", error->errmess);
		return false;
	}

	/* read page size */
	error = xpdriver_page_size(0, 0, &left, &bottom, &right, &top);
	if (error) {
		LOG(("xpdriver_page_size: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("PrintError", error->errmess);
		return false;
	}

	width = (right - left) / 800;
	height = (top - bottom) / 800;

	/* layout the document to the correct width */
	saved_width = c->width;
	saved_height = c->height;
	if (c->type == CONTENT_HTML)
		layout_document(c, width, height);

	/* open printer file */
	error = xosfind_openoutw(osfind_NO_PATH | osfind_ERROR_IF_DIR |
			osfind_ERROR_IF_ABSENT, filename, 0, &fhandle);
	if (error) {
		LOG(("xosfind_openoutw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("PrintError", error->errmess);
		return false;
	}

	/* select print job */
	error = xpdriver_select_jobw(fhandle, "NetSurf", &old_job);
	if (error) {
		LOG(("xpdriver_select_jobw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("PrintError", error->errmess);
		xosfind_closew(fhandle);
		return false;
	}

	rufl_invalidate_cache();

	/* declare fonts, if necessary */
	if (features & pdriver_FEATURE_DECLARE_FONT) {
		if ((error_message = print_declare_fonts(c)))
			goto error;
	}

	plot = ro_plotters;
	ro_plot_set_scale(print_scale);
	ro_gui_current_redraw_gui = g;
	current_redraw_browser = NULL;  /* we don't want to print the
	                                  selection */

	/* print is now active */
	print_active = true;

	do {
		int clip_x0, clip_y0, clip_x1, clip_y1;
		os_box b = {left / 400 - 2, bottom / 400 - 2,
				right / 400 + 2, top / 400 + 2};
		os_hom_trfm t = { { {65536, 0}, {0, 65536} } };
		os_coord p = {left, bottom};
		osbool more;

		xhourglass_percentage((int) (yscroll * 100 / c->height));

		/* give page rectangle */
		error = xpdriver_give_rectangle(0, &b, &t, &p, os_COLOUR_WHITE);
		if (error) {
			LOG(("xpdriver_give_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			error_message = error->errmess;
			goto error;
		}

		LOG(("given rectangle: [(%d, %d), (%d, %d)]",
				b.x0, b.y0, b.x1, b.y1));

		/* and redraw the document */
		error = xpdriver_draw_page(print_num_copies, &b, 0, 0,
				&more, 0);
		if (error) {
			LOG(("xpdriver_draw_page: 0x%x: %s",
					error->errnum, error->errmess));
			error_message = error->errmess;
			goto error;
		}

		ro_plot_origin_x = left / 400;
		ro_plot_origin_y = top / 400 + yscroll * 2;

		while (more) {
			LOG(("redrawing area: [(%d, %d), (%d, %d)]",
					b.x0, b.y0, b.x1, b.y1));
			clip_x0 = (b.x0 - ro_plot_origin_x) / 2;
			clip_y0 = (ro_plot_origin_y - b.y1) / 2;
			clip_x1 = (b.x1 - ro_plot_origin_x) / 2;
			clip_y1 = (ro_plot_origin_y - b.y0) / 2;
			if (!content_redraw(c, 0, 0,
					c->width, c->height,
					clip_x0, clip_y0, clip_x1, clip_y1,
					print_scale,
					0xFFFFFF)) {
				error_message = "redraw error";
				goto error;
			}

			error = xpdriver_get_rectangle(&b, &more, 0);
			if (error) {
				LOG(("xpdriver_get_rectangle: 0x%x: %s",
						error->errnum, error->errmess));
				error_message = error->errmess;
				goto error;
			}
		}

		yscroll += height;
	} while (yscroll <= c->height && --sheets != 0);

	/* make print inactive */
	print_active = false;
	ro_gui_current_redraw_gui = 0;

	/* clean up */
	error = xpdriver_end_jobw(fhandle);
	if (error) {
		LOG(("xpdriver_end_jobw: 0x%x: %s",
				error->errnum, error->errmess));
		error_message = error->errmess;
		goto error;
	}

	error = xosfind_closew(fhandle);
	if (error) {
		LOG(("xosfind_closew: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("PrintError", error->errmess);
		return false;
	}

	if (old_job) {
		error = xpdriver_select_jobw(old_job, 0, 0);
		if (error) {
			LOG(("xpdriver_select_jobw: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("PrintError", error->errmess);
			/* the printing succeeded anyway */
			return true;
		}
	}

	rufl_invalidate_cache();

	/* restore document layout */
	if (c->type == CONTENT_HTML)
		layout_document(c, saved_width, saved_height);

	return true;

error:
	xpdriver_abort_job(fhandle);
	xosfind_closew(fhandle);
	if (old_job)
		xpdriver_select_jobw(old_job, 0, 0);
	print_active = false;
	ro_gui_current_redraw_gui = 0;

	warn_user("PrintError", error_message);

	rufl_invalidate_cache();

	/* restore document layout */
	if (c->type == CONTENT_HTML)
		layout_document(c, saved_width, saved_height);

	return false;
}


/**
 * Declare fonts to the printer driver.
 *
 * \param  c  content being printed
 * \return 0 on success, error message on error
 */

const char *print_declare_fonts(struct content *content)
{
	unsigned int i;
	const char *error_message = 0;
	os_error *error;

	free(print_fonts_list);
	print_fonts_list = 0;
	print_fonts_count = 0;
	print_fonts_error = 0;

	plot = print_fonts_plotters;
	if (!content_redraw(content, 0, 0, content->width, content->height,
			INT_MIN, INT_MIN, INT_MAX, INT_MAX, 1, 0xffffff)) {
		if (print_fonts_error)
			return print_fonts_error;
		return "Declaring fonts failed.";
	}

	for (i = 0; i != print_fonts_count; ++i) {
		LOG(("%u %s", i, print_fonts_list[i]));
		error = xpdriver_declare_font(0, print_fonts_list[i],
				pdriver_KERNED);
		if (error) {
			LOG(("xpdriver_declare_font: 0x%x: %s",
					error->errnum, error->errmess));
			error_message = error->errmess;
			goto end;
		}
	}
	error = xpdriver_declare_font(0, 0, 0);
	if (error) {
		LOG(("xpdriver_declare_font: 0x%x: %s",
				error->errnum, error->errmess));
		error_message = error->errmess;
		goto end;
	}

end:
	for (i = 0; i != print_fonts_count; i++)
		free(print_fonts_list[i]);
	free(print_fonts_list);
	print_fonts_list = 0;

	return error_message;
}


bool print_fonts_plot_clg(colour c)
{
	return true;
}
bool print_fonts_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed)
{
	return true;
}
bool print_fonts_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed)
{
	return true;
}
bool print_fonts_plot_polygon(int *p, unsigned int n, colour fill)
{
	return true;
}
bool print_fonts_plot_fill(int x0, int y0, int x1, int y1, colour c)
{
	return true;
}
bool print_fonts_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1)
{
	return true;
}
bool print_fonts_plot_disc(int x, int y, int radius, colour colour,
		bool filled)
{
	return true;
}
bool print_fonts_plot_arc(int x, int y, int radius, int angle1, int angle2,
		colour c)
{
	return true;
}
bool print_fonts_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg, struct content *content)
{
	return true;
}
bool print_fonts_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y, struct content *content)
{
	return true;
}
bool print_fonts_plot_group_start(const char *name)
{
	return true;
}
bool print_fonts_plot_group_end(void)
{
	return true;
}


/**
 * Plotter for text plotting during font listing.
 */

bool print_fonts_plot_text(int x, int y, const struct css_style *style,
		const char *text, size_t length, colour bg, colour c)
{
	const char *font_family;
	unsigned int font_size;
	rufl_style font_style;
	rufl_code code;

	nsfont_read_style(style, &font_family, &font_size, &font_style);

	code = rufl_paint_callback(font_family, font_style, font_size,
			text, length, 0, 0, print_fonts_callback, 0);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR) {
			LOG(("rufl_paint_callback: rufl_FONT_MANAGER_ERROR: "
					"0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
			print_fonts_error = rufl_fm_error->errmess;
		} else {
			LOG(("rufl_paint_callback: 0x%x", code));
		}
		return false;
	}
	if (print_fonts_error)
		return false;

	return true;
}


/**
 * Callback for print_fonts_plot_text().
 *
 * The font name is added to print_fonts_list.
 */

void print_fonts_callback(void *context,
		const char *font_name, unsigned int font_size,
		const char *s8, unsigned short *s16, unsigned int n,
		int x, int y)
{
	unsigned int i;
	char **fonts_list;

	(void) context;  /* unused */
	(void) font_size;  /* unused */
	(void) x;  /* unused */
	(void) y;  /* unused */

	assert(s8 || s16);

	/* check if the font name is new */
	for (i = 0; i != print_fonts_count &&
			strcmp(print_fonts_list[i], font_name) != 0; i++)
		;
	if (i != print_fonts_count)
		return;

	/* add to list of fonts */
	fonts_list = realloc(print_fonts_list,
			sizeof print_fonts_list[0] *
			(print_fonts_count + 1));
	if (!fonts_list) {
		print_fonts_error = messages_get("NoMemory");
		return;
	}
	fonts_list[print_fonts_count] = strdup(font_name);
	if (!fonts_list[print_fonts_count]) {
		print_fonts_error = messages_get("NoMemory");
		return;
	}
	print_fonts_list = fonts_list;
	print_fonts_count++;
}


#endif
