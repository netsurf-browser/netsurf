/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <string.h>

#include "oslib/font.h"
#include "oslib/hourglass.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/pdriver.h"
#include "oslib/wimp.h"

#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/render/font.h"
#include "netsurf/render/html.h"
#include "netsurf/render/layout.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/print.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

/** \todo position images correctly (seem to be offset
 *        to the right and upwards by half the print margin width)
 *  \todo fix images when printing with the PostScript driver
 *        (redraws appear not to be intercepted)
 *  \todo landscape format pages
 *  \todo be somewhat more intelligent and try not to crop pages
 *        half way up a line of text
 *  \todo make use of print stylesheets
 */

#ifdef WITH_PRINT

/* 1 millipoint == 1/400 OS unit = 1/800 browser units */

struct gui_window *print_current_window = 0;
static bool print_in_background = false;
static int print_num_copies = 1;
static bool print_bg_images = true;
static int print_max_sheets = -1;

/* array of fonts in document - max 255 */
struct print_font {
	font_f handle;
	void *fontName;
};

static void print_update_sheets_shaded_state(bool on);
static void print_send_printsave(struct content *c);
static bool print_send_printtypeknown(wimp_message *m);
static void print_document(struct gui_window *g, const char *filename);
static bool print_find_fonts(struct box *box, struct print_font **print_fonts, int *numFonts);

/**
 * Open the print dialog
 *
 * \param g parent window
 * \param x leftmost edge of dialog (only if sub_menu == true)
 * \param y topmost edge of dialog (as above)
 * \param sub_menu open window as a submenu or as a persistent dialog
 * \param keypress whether we were opened by a keypress
 */
void ro_gui_print_open(struct gui_window *g, int x, int y, bool sub_menu, bool keypress)
{
	char *pdName;
	bool printers_exists = true;
	os_error *e;

	assert(g != NULL);

	print_current_window = g;

	/* Read Printer Driver name */
	e = xpdriver_info(0, 0, 0, 0, &pdName, 0, 0, 0);
	if (e) {
		LOG(("%s", e->errmess));
		printers_exists = false;
	}

	print_bg_images = g->option.background_images;

	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_TO_BOTTOM, true);

	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_SHEETS, false);
	ro_gui_set_icon_integer(dialog_print, ICON_PRINT_SHEETS_VALUE, 1);
	ro_gui_set_icon_string(dialog_print, ICON_PRINT_SHEETS_TEXT, "sheet is filled");
	print_update_sheets_shaded_state(true);

	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_FG_IMAGES, true);
	ro_gui_set_icon_shaded_state(dialog_print, ICON_PRINT_FG_IMAGES, true);

	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_BG_IMAGES, print_bg_images);

	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_IN_BACKGROUND, false);

	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_UPRIGHT, true);
	ro_gui_set_icon_selected_state(dialog_print, ICON_PRINT_SIDEWAYS, false);

	ro_gui_set_icon_integer(dialog_print, ICON_PRINT_COPIES, 1);

	if (!printers_exists) {
		ro_gui_set_icon_shaded_state(dialog_print, ICON_PRINT_PRINT, true);
	}
	else {
		ro_gui_set_window_title(dialog_print, pdName);
	}

	if (sub_menu) {
		e = xwimp_create_sub_menu((wimp_menu *) dialog_print, x, y);
		if (e) {
			LOG(("xwimp_create_sub_menu: 0x%x: %s",
					e->errnum, e->errmess));
			warn_user("MenuError", e->errmess);
		}
	}
	else {
		ro_gui_dialog_open_persistant(g->window, dialog_print, !keypress);
	}
}

/**
 * Handle mouse clicks in print dialog
 *
 * \param pointer wimp_pointer block
 */
void ro_gui_print_click(wimp_pointer *pointer)
{
	int copies = atoi(ro_gui_get_icon_string(dialog_print,
						ICON_PRINT_COPIES));
	int sheets = atoi(ro_gui_get_icon_string(dialog_print,
						ICON_PRINT_SHEETS_VALUE));

	if (pointer->buttons == wimp_CLICK_MENU)
		return;

	switch (pointer->i) {
		case ICON_PRINT_SHEETS:
			/* retain selection state */
			ro_gui_set_icon_selected_state(dialog_print,
					pointer->i, true);
			print_update_sheets_shaded_state(false);
			break;
		case ICON_PRINT_TO_BOTTOM:
			print_update_sheets_shaded_state(true);
		case ICON_PRINT_UPRIGHT:
		case ICON_PRINT_SIDEWAYS:
			/* retain selection state */
			ro_gui_set_icon_selected_state(dialog_print,
					pointer->i, true);
			break;
		case ICON_PRINT_COPIES_UP:   copies += 1; break;
		case ICON_PRINT_COPIES_DOWN: copies -= 1; break;
		case ICON_PRINT_SHEETS_UP:   sheets += 1; break;
		case ICON_PRINT_SHEETS_DOWN: sheets -= 1; break;
		case ICON_PRINT_CANCEL:
			print_cleanup();
			break;
		case ICON_PRINT_PRINT:
			print_in_background = ro_gui_get_icon_selected_state(dialog_print, ICON_PRINT_IN_BACKGROUND);
			print_num_copies = copies;
			if (ro_gui_get_icon_selected_state(dialog_print, ICON_PRINT_SHEETS))
				print_max_sheets = sheets;
			else
				print_max_sheets = -1;
			print_current_window->option.background_images = ro_gui_get_icon_selected_state(dialog_print, ICON_PRINT_BG_IMAGES);
			print_send_printsave(print_current_window->bw->current_content);
			break;
	}

	if (copies < 1)
		copies = 1;
	else if (copies > 99)
		copies = 99;
	ro_gui_set_icon_integer(dialog_print, ICON_PRINT_COPIES, copies);

	if (sheets < 1)
		sheets = 1;
	else if (sheets > 99)
		sheets = 99;
	ro_gui_set_icon_integer(dialog_print, ICON_PRINT_SHEETS_VALUE, sheets);
	if (sheets > 1)
		ro_gui_set_icon_string(dialog_print, ICON_PRINT_SHEETS_TEXT, "sheets are filled");
	else
		ro_gui_set_icon_string(dialog_print, ICON_PRINT_SHEETS_TEXT, "sheet is filled");
}

/**
 * Handle keypresses in print dialog
 *
 * \param key wimp_key block
 * \return true if keypress dealt with, false otherwise.
 */
bool ro_gui_print_keypress(wimp_key *key)
{
	switch (key->c) {
		case wimp_KEY_ESCAPE:
			print_cleanup();
			return true;
		case wimp_KEY_RETURN:
			print_in_background = ro_gui_get_icon_selected_state(dialog_print, ICON_PRINT_IN_BACKGROUND);
			print_num_copies = atoi(ro_gui_get_icon_string(dialog_print, ICON_PRINT_COPIES));
			if (ro_gui_get_icon_selected_state(dialog_print, ICON_PRINT_SHEETS))
				print_max_sheets = atoi(ro_gui_get_icon_string(dialog_print, ICON_PRINT_SHEETS_VALUE));
			else
				print_max_sheets = -1;
			print_current_window->option.background_images = ro_gui_get_icon_selected_state(dialog_print, ICON_PRINT_BG_IMAGES);
			print_send_printsave(print_current_window->bw->current_content);
			return true;
	}

	return false;
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
	if (m->your_ref != 0 && !print_in_background) {
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
 * Handle message_DATASAVE_ACK for the printing protocol
 *
 * \param m the message to handle
 */
void print_ack(wimp_message *m)
{
	int type;
	os_error *e;

	/* Read Printer Driver Type */
	e = xpdriver_info(&type, 0, 0, 0, 0, 0, 0, 0);
	if (e) {
		LOG(("%s", e->errmess));
		print_cleanup();
		return;
	}

	type &= 0xFFFF0000; /* we don't care about the version no */

	if (print_current_window) {
		print_document(print_current_window,
				(const char*)m->data.data_xfer.file_name);

		/* send dataload */
		m->your_ref = m->my_ref;
		m->action = message_DATA_LOAD;

		/* We cheat here and, instead of giving Printers what
		 * it asked for (a copy of the file so it can poke us
		 * later via a broadcast of PrintTypeOdd), we give
		 * it a file that it can print itself without having
		 * to bother us further. For PostScript printers
		 * (type 0) we give it a PostScript file. Otherwise,
		 * we give it a PrintOut file.
		 *
		 * This method has a couple of advantages:
		 * - we can reuse this code for background printing
		 *   (we simply ignore the PrintTypeOdd reply)
		 * - there's no need to ensure all components of a
		 *   page queued to be printed still exist when it
		 *   reaches the top of the queue. (which reduces
		 *   complexity a fair bit)
		 */
		if (type == 0)
			/* postscript */
			m->data.data_xfer.file_type = 0xff5;
		else
			/* printout */
			m->data.data_xfer.file_type = 0xff4;

		e = xwimp_send_message(wimp_USER_MESSAGE_RECORDED, m,
				m->sender);
		if (e) {
			LOG(("xwimp_send_message: 0x%x: %s",
					e->errnum, e->errmess));
			warn_user("WimpError", e->errmess);
			/* and delete temporary file */
			xosfile_delete(m->data.data_xfer.file_name,
					0, 0, 0, 0, 0);
			print_cleanup();
		}
	}
}

/**
 * Handle a bounced dataload message
 *
 * \param m the message to handle
 */
void print_dataload_bounce(wimp_message *m)
{
	xosfile_delete(m->data.data_xfer.file_name, 0, 0, 0, 0, 0);
	print_cleanup();
}

/**
 * Cleanup after printing
 */
void print_cleanup(void)
{
	print_current_window->option.background_images = print_bg_images;
	print_current_window = 0;
	print_max_sheets = -1;
	xwimp_create_menu((wimp_menu *)-1, 0, 0);
	ro_gui_dialog_close(dialog_print);
}

/**
 * Print a document
 *
 * \param g gui_window containing the document to print
 * \param filename name of file to print to
 */
void print_document(struct gui_window *g, const char *filename)
{
	struct content *c = g->bw->current_content;
	struct box *box = NULL;
	int temp;
	os_error *e;
	pdriver_features features;
	int left, right, top, bottom, width, height;
	os_fw fhandle, old_job = 0;
	int yscroll = 0, sheets = print_max_sheets;

	/* no point printing a blank page */
	if (!c)
		return;

	LOG(("Printing page (%d)", print_max_sheets));

	if (c->type == CONTENT_HTML)
		box = c->data.html.layout->children;

	/* Read Printer Driver Features */
	e = xpdriver_info(0, 0, 0, &features, 0, 0, 0, 0);
	if (e) {
		LOG(("%s", e->errmess));
		return;
	}

	LOG(("read features"));

	/* Acquire page size */
	e = xpdriver_page_size(&width, &height, &left, &bottom, &right, &top);
	if (e) {
		LOG(("%s", e->errmess));
		return;
	}

	LOG(("page size: %d x %d", width/800, height/800));

	width = (right - left) / 800;
	height = (top - bottom) / 800;

	LOG(("printable area: [(%d, %d), (%d, %d)] = %d x %d",
				left, bottom, right, top, width, height));


	temp = c->width;

	/* layout the document to the correct width */
	if (c->type == CONTENT_HTML)
		layout_document(box, width, c->data.html.box_pool);

	/* open printer */
	e = xosfind_openoutw(0xf, filename, 0, &fhandle);
	if (e) {
		LOG(("%s", e->errmess));
		return;
	}

	LOG(("opened %s", filename));

	/* select print job */
	e = xpdriver_select_jobw(fhandle, "NetSurf", &old_job);
	if (e) {
		LOG(("%s", e->errmess));
		xosfind_closew(fhandle);
		return;
	}

	LOG(("selected job - polling the wimp now does bad things(TM)"));

	/* declare fonts, if necessary */
	if (features & pdriver_FEATURE_DECLARE_FONT &&
					c->type == CONTENT_HTML) {
		struct print_font *print_fonts =
					calloc(255, sizeof(*print_fonts));
		int numFonts = 0;
		int i;

		if (!print_fonts) {
			LOG(("malloc failed"));
			goto error;
		}

		if(!print_find_fonts(box, &print_fonts, &numFonts)) {
			LOG(("print_find_fonts_failed"));
			for (i = 0; i != numFonts; ++i) {
					free((print_fonts[i]).fontName);
				}
			free(print_fonts);
			goto error;
		}
		LOG(("%d", numFonts));

		for (i = 0; i != numFonts; ++i) {
			LOG(("0x%x: %s", (print_fonts[i]).handle, (char*)(print_fonts[i]).fontName));
			e = xpdriver_declare_font((font_f)(print_fonts[i]).handle, (print_fonts[i]).fontName, 0);
			if (e) {
				for (i = 0; i != numFonts; ++i) {
					free((print_fonts[i]).fontName);
				}
				free(print_fonts);
				LOG(("%s", e->errmess));
				goto error;
			}
		}

		for (i = 0; i != numFonts; ++i) {
			free((print_fonts[i]).fontName);
		}
		free(print_fonts);

		e = xpdriver_declare_font(0, 0, 0);
		if (e) {
			LOG(("%s", e->errmess));
			goto error;
		}

		LOG(("declared fonts"));
	}

	do {
		os_box b = {left/400 - 2, bottom/400 - 2,
				right/400 + 2, top/400 + 2};
		os_hom_trfm t = { { { 65536, 0}, {0, 65536} } };
		os_coord p = {left, bottom};

		e = xhourglass_percentage((int)(yscroll*100/c->height));
		if (e) {
			LOG(("%s", e->errmess));
			/* the hourglass failing to be updated
			 * shouldn't stop the printjob
			 */
		}

		/* Give page rectangle */
		e = xpdriver_give_rectangle(0, &b, &t, &p, os_COLOUR_WHITE);
		if (e) {
			LOG(("%s", e->errmess));
			goto error;
		}

		LOG(("given rectangle: [(%d, %d), (%d, %d)]", b.x0, b.y0, b.x1, b.y1));

		/* and redraw the document */
		osbool more;
		e = xpdriver_draw_page(print_num_copies, &b, 0, 0, &more, 0);
		if (e) {
			LOG(("%s", e->errmess));
			goto error;
		}

		LOG(("done draw_page"));

		ro_gui_current_redraw_gui = g;

		while (more) {
			LOG(("redrawing area: [(%d, %d), (%d, %d)]", b.x0, b.y0, b.x1, b.y1));
			if (c) {
				content_redraw(c, b.x0, b.y1+(yscroll*2),
						c->width * 2, c->height * 2,
						b.x0, b.y0,
						b.x1-1, b.y1-1,
						1.0 /* scale == 100% */);
			}
			e = xpdriver_get_rectangle(&b, &more, 0);
			if (e) {
				LOG(("%s", e->errmess));
				ro_gui_current_redraw_gui = NULL;
				goto error;
			}
		}

		yscroll += height;
	} while (yscroll <= c->height && --sheets != 0);

	ro_gui_current_redraw_gui = NULL;
	LOG(("finished redraw"));

	/* clean up */
	e = xpdriver_end_jobw(fhandle);
	if (e) {
		LOG(("%s", e->errmess));
		goto error;
	}
	xosfind_close(fhandle);
	if (old_job) xpdriver_select_jobw(old_job, 0, 0);

	LOG(("done job"));

	/* restore document layout */
	if (c->type == CONTENT_HTML)
		layout_document(box, temp, c->data.html.box_pool);

	return;

error:
	xpdriver_abort_job(fhandle);
	xosfind_close(fhandle);
	if (old_job) xpdriver_select_jobw(old_job, 0, 0);

	/* restore document layout */
	if (c->type == CONTENT_HTML)
		layout_document(box, temp, c->data.html.box_pool);
}

/**
 * Find all fonts in a document
 *
 * \param box Root of box tree
 * \param print_fonts pointer to array of fonts in document
 * \paran numFonts number of fonts declared
 * \return true on success, false otherwise
 */
bool print_find_fonts(struct box *box, struct print_font **print_fonts, int *numFonts)
{
	struct box *a;
	const char *txt;
	int txt_len;
	unsigned int width, rolength, consumed;
	const char *rofontname, *rotext;
	int i;

	assert(box);

	if (box->text && box->font && box->length > 0) {
		txt = box->text;
		txt_len = box->length;

		if (box->font->ftype == FONTTYPE_UFONT) {
			/** \todo handle ufont */
			LOG(("ufont"));
			return false;
		}

		nsfont_txtenum(box->font, txt, txt_len,
				&width, &rofontname,
				&rotext, &rolength,
				&consumed);

		if (rotext == NULL) {
			LOG(("rotext = null (%d)", txt_len));
			return false;
		}

		for (i = 0; i != *numFonts; ++i) {
			if (!strcmp(((*print_fonts)[i]).fontName, rofontname))
				break;
		}

		if (i == *numFonts) {
			/* max 255 fonts (as per draw) */
			if (*numFonts == 255)
				return false;
			if ((((*print_fonts)[*numFonts]).fontName = strdup(rofontname)) == NULL) {
				LOG(("failed to strdup (%s)", rofontname));
				return false;
			}
			((*print_fonts)[(*numFonts)++]).handle = (font_f)box->font->handle;
		}

		free((void*)rotext);
	}

	for (a = box->children; a; a = a->next) {
		if (!print_find_fonts(a, print_fonts, numFonts))
			return false;
	}

	return true;
}
#endif
