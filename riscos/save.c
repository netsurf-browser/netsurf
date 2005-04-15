/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Save dialog and drag and drop saving (implementation).
 */

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/dragasprite.h"
#include "oslib/osbyte.h"
#include "oslib/osfile.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "netsurf/desktop/save_text.h"
#include "netsurf/desktop/selection.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/save_complete.h"
#include "netsurf/riscos/save_draw.h"
#include "netsurf/riscos/thumbnail.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/config.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


static gui_save_type gui_save_current_type;
static struct content *gui_save_content = NULL;
static struct selection *gui_save_selection = NULL;
static int gui_save_filetype;

static bool using_dragasprite = true;
static wimp_w gui_save_dialogw = (wimp_w)-1;

typedef enum { LINK_ACORN, LINK_ANT, LINK_TEXT } link_format;

static bool ro_gui_save_complete(struct content *c, char *path);
static bool ro_gui_save_content(struct content *c, char *path);
static void ro_gui_save_object_native(struct content *c, char *path);
static bool ro_gui_save_link(struct content *c, link_format format, char *path);


/** An entry in gui_save_table. */
struct gui_save_table_entry {
	int filetype;
	const char *name;
};

/** Table of filetypes and default filenames. Must be in sync with
 * gui_save_type (riscos/gui.h). A filetype of 0 indicates the content should
 * be used. */
struct gui_save_table_entry gui_save_table[] = {
	/* GUI_SAVE_SOURCE,              */ {     0, "SaveSource" },
	/* GUI_SAVE_DRAW,                */ { 0xaff, "SaveDraw" },
	/* GUI_SAVE_TEXT,                */ { 0xfff, "SaveText" },
	/* GUI_SAVE_COMPLETE,            */ { 0xfaf, "SaveComplete" },
	/* GUI_SAVE_OBJECT_ORIG,         */ {     0, "SaveObject" },
	/* GUI_SAVE_OBJECT_NATIVE,       */ { 0xff9, "SaveObject" },
	/* GUI_SAVE_LINK_URI,            */ { 0xf91, "SaveLink" },
	/* GUI_SAVE_LINK_URL,            */ { 0xb28, "SaveLink" },
	/* GUI_SAVE_LINK_TEXT,           */ { 0xfff, "SaveLink" },
	/* GUI_SAVE_HOTLIST_EXPORT_HTML, */ { 0xfaf, "Hotlist" },
	/* GUI_SAVE_HISTORY_EXPORT_HTML, */ { 0xfaf, "History" },
	/* GUI_SAVE_TEXT_SELECTION,      */ { 0xfff, "SaveText" },
};


/**
 * Prepares the save box to reflect gui_save_type and a content, and
 * opens it.
 *
 * \param  save_type  type of save
 * \param  c          content to save
 * \param  sub_menu   open dialog as a sub menu, otherwise persistent
 * \param  x          x position, for sub_menu true only
 * \param  y          y position, for sub_menu true only
 * \param  parent     parent window for persistent box, for sub_menu false only
 */

void ro_gui_save_prepare(gui_save_type save_type, struct content *c)
{
	char icon_buf[20];
	const char *icon = icon_buf;
	const char *name = "";
	const char *nice;
	url_func_result res;

	assert((save_type == GUI_SAVE_HOTLIST_EXPORT_HTML) ||
			(save_type == GUI_SAVE_HISTORY_EXPORT_HTML) || c);

	gui_save_current_type = save_type;
	gui_save_content = c;
	gui_save_filetype = gui_save_table[save_type].filetype;
	if (!gui_save_filetype)
		gui_save_filetype = ro_content_filetype(c);

	/* icon */
	sprintf(icon_buf, "file_%.3x", gui_save_filetype);
	if (!ro_gui_wimp_sprite_exists(icon_buf))
		icon = "file_xxx";
	ro_gui_set_icon_string(dialog_saveas, ICON_SAVE_ICON, icon);

	/* filename */
	name = gui_save_table[save_type].name;
	if (c && (res = url_nice(c->url, (char **)&nice)) == URL_FUNC_OK)
		name = nice;
	else
		name = messages_get(name);

	ro_gui_set_icon_string(dialog_saveas, ICON_SAVE_PATH, name);
}

/**
 * Handle clicks in the save dialog.
 */

void ro_gui_save_click(wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_SAVE_OK:
			ro_gui_save_ok(pointer->w);
			break;
		case ICON_SAVE_CANCEL:
			if (pointer->buttons == wimp_CLICK_SELECT) {
				xwimp_create_menu((wimp_menu *)-1, 0, 0);
				ro_gui_dialog_close(pointer->w);
			} else if (pointer->buttons == wimp_CLICK_ADJUST) {
/* 	  			ro_gui_menu_prepare_save(gui_save_content); */
	  		}
	  		break;
		case ICON_SAVE_ICON:
			if (pointer->buttons == wimp_DRAG_SELECT) {
				const char *sprite = ro_gui_get_icon_string(pointer->w, pointer->i);
				gui_current_drag_type = GUI_DRAG_SAVE;
				gui_save_dialogw = pointer->w;
				ro_gui_drag_icon(pointer->pos.x, pointer->pos.y, sprite);
			}
			break;
	}
}


/**
 * Handle OK click/keypress in the save dialog.
 */

void ro_gui_save_ok(wimp_w w)
{
	char *name = ro_gui_get_icon_string(w, ICON_SAVE_PATH);
	if (!strrchr(name, '.'))
	{
		warn_user("NoPathError", NULL);
		return;
	}
	gui_save_dialogw = w;
	if (ro_gui_save_content(gui_save_content, name)) {
		xwimp_create_menu((wimp_menu *)-1, 0, 0);
		ro_gui_dialog_close(w);
	}
}


/**
 * Initiates drag saving of an object directly from a browser window
 *
 * \param  save_type  type of save
 * \param  c          content to save
 */

void gui_drag_save_object(gui_save_type save_type, struct content *c)
{
	wimp_pointer pointer;
	char icon_buf[20];
	const char *icon = icon_buf;
	os_error *error;

	/* Close the save window because otherwise we need two contexts
	*/
	if (gui_save_dialogw != (wimp_w)-1)
		ro_gui_dialog_close(gui_save_dialogw);

	gui_save_dialogw = (wimp_w)-1;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	gui_save_current_type = save_type;
	gui_save_content = c;
	gui_save_filetype = gui_save_table[save_type].filetype;
	if (!gui_save_filetype)
		gui_save_filetype = ro_content_filetype(c);

	/* sprite to use */
	sprintf(icon_buf, "file_%.3x", gui_save_filetype);
	if (!ro_gui_wimp_sprite_exists(icon_buf))
		icon = "file_xxx";

	gui_current_drag_type = GUI_DRAG_SAVE;

	ro_gui_drag_icon(pointer.pos.x, pointer.pos.y, icon);
}


void gui_drag_save_selection(struct selection *s)
{
	wimp_pointer pointer;
	char icon_buf[20];
	const char *icon = icon_buf;
	os_error *error;

	/* Close the save window because otherwise we need two contexts
	*/
	if (gui_save_dialogw != (wimp_w)-1)
		ro_gui_dialog_close(gui_save_dialogw);

	gui_save_dialogw = (wimp_w)-1;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	gui_save_current_type = GUI_SAVE_TEXT_SELECTION;
	gui_save_content = NULL;
	gui_save_selection = s;
	gui_save_filetype = gui_save_table[GUI_SAVE_TEXT_SELECTION].filetype;

	/* sprite to use */
	sprintf(icon_buf, "file_%.3x", gui_save_filetype);
	if (!ro_gui_wimp_sprite_exists(icon_buf))
		icon = "file_xxx";

	gui_current_drag_type = GUI_DRAG_SAVE;

	ro_gui_drag_icon(pointer.pos.x, pointer.pos.y, icon);
}


/**
 * Start drag of icon under the pointer.
 */

void ro_gui_drag_icon(int x, int y, const char *sprite)
{
	os_error *error;
	wimp_drag drag;
	int r2;

	drag.initial.x0 = x - 34;
	drag.initial.y0 = y - 34;
	drag.initial.x1 = x + 34;
	drag.initial.y1 = y + 34;

	if (sprite && (xosbyte2(osbyte_READ_CMOS, 28, 0, &r2) || (r2 & 2))) {
		error = xdragasprite_start(dragasprite_HPOS_CENTRE |
				dragasprite_VPOS_CENTRE |
				dragasprite_BOUND_POINTER |
				dragasprite_DROP_SHADOW,
				(osspriteop_area *) 1, sprite, &drag.initial, 0);

		if (!error) {
			using_dragasprite = true;
			return;
		}

		LOG(("xdragasprite_start: 0x%x: %s",
				error->errnum, error->errmess));
	}

	drag.type = wimp_DRAG_USER_FIXED;
	drag.bbox.x0 = -0x8000;
	drag.bbox.y0 = -0x8000;
	drag.bbox.x1 = 0x7fff;
	drag.bbox.y1 = 0x7fff;

	using_dragasprite = false;
	error = xwimp_drag_box(&drag);

	if (error) {
		LOG(("xwimp_drag_box: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("DragError", error->errmess);
	}
}


/**
 * Handle User_Drag_Box event for a drag from the save dialog or browser window.
 */

void ro_gui_save_drag_end(wimp_dragged *drag)
{
	const char *name;
	wimp_pointer pointer;
	wimp_message message;

	wimp_get_pointer_info(&pointer);

	if (gui_save_dialogw == (wimp_w)-1) {
		/* saving directly from browser window, choose a name based upon the URL */
		struct content *c = gui_save_content;
		const char *nice;
		name = gui_save_table[gui_save_current_type].name;
		if (c) {
			url_func_result res;
			if ((res = url_nice(c->url, (char **)&nice)) == URL_FUNC_OK)
				name = nice;
		}
	}
	else {
		/* saving from dialog, grab leafname from icon */
		char *dot;
		name = ro_gui_get_icon_string(gui_save_dialogw, ICON_SAVE_PATH);
		dot = strrchr(name, '.');
		if (dot)
			name = dot + 1;
	}

	message.your_ref = 0;
	message.action = message_DATA_SAVE;
	message.data.data_xfer.w = pointer.w;
	message.data.data_xfer.i = pointer.i;
	message.data.data_xfer.pos.x = pointer.pos.x;
	message.data.data_xfer.pos.y = pointer.pos.y;
	message.data.data_xfer.est_size = 1000;
	message.data.data_xfer.file_type = gui_save_filetype;
	if (gui_save_current_type == GUI_SAVE_COMPLETE) {
		message.data.data_xfer.file_type = 0x2000;
		if (name[0] != '!') {
			message.data.data_xfer.file_name[0] = '!';
			strncpy(message.data.data_xfer.file_name + 1, name,
					211);
		} else {
			strncpy(message.data.data_xfer.file_name, name, 212);
		}
	} else
		strncpy(message.data.data_xfer.file_name, name, 212);
	message.data.data_xfer.file_name[211] = 0;
	message.size = 44 + ((strlen(message.data.data_xfer.file_name) + 4) &
			(~3u));

	wimp_send_message_to_window(wimp_USER_MESSAGE, &message,
			pointer.w, pointer.i);
}



/**
 * Send DataSave message on behalf of clipboard code and remember that it's the
 * clipboard contents we're being asked for when the DataSaveAck reply arrives
 */

void ro_gui_send_datasave(gui_save_type save_type, const wimp_full_message_data_xfer *message, wimp_t to)
{
	os_error *error;

	/* Close the save window because otherwise we need two contexts
	*/
	if (gui_save_dialogw != (wimp_w)-1)
		ro_gui_dialog_close(gui_save_dialogw);

	error = xwimp_send_message(wimp_USER_MESSAGE, (wimp_message*)message, to);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s", error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	else {
		gui_save_current_type = save_type;
		gui_save_dialogw = (wimp_w)-1;
		gui_current_drag_type = GUI_DRAG_SAVE;
	}
}


/**
 * Handle Message_DataSaveAck for a drag from the save dialog or browser window.
 */

void ro_gui_save_datasave_ack(wimp_message *message)
{
	char *path = message->data.data_xfer.file_name;
	struct content *c = gui_save_content;

	switch (gui_save_current_type) {
		case GUI_SAVE_HOTLIST_EXPORT_HTML:
		case GUI_SAVE_HISTORY_EXPORT_HTML:
		case GUI_SAVE_TEXT_SELECTION:
		case GUI_SAVE_CLIPBOARD_CONTENTS:
			break;

		default:
			if (!gui_save_content) {
				LOG(("unexpected DataSaveAck: gui_save_content not set"));
				return;
			}
			break;
	}

	if (gui_save_dialogw != (wimp_w)-1)
		ro_gui_set_icon_string(gui_save_dialogw, ICON_SAVE_PATH, path);

	if (ro_gui_save_content(c, path)) {
		os_error *error;

		if (gui_save_dialogw != (wimp_w)-1) {
			/*	Close the save window
			*/
			ro_gui_dialog_close(gui_save_dialogw);
		}

		/* Ack successful save with message_DATA_LOAD */
		message->action = message_DATA_LOAD;
		message->your_ref = message->my_ref;
		error = xwimp_send_message_to_window(wimp_USER_MESSAGE, message,
				message->data.data_xfer.w, message->data.data_xfer.i, 0);
		if (error) {
			LOG(("xwimp_send_message_to_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}

		error = xwimp_create_menu(wimp_CLOSE_MENU, 0, 0);
		if (error) {
			LOG(("xwimp_create_menu: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MenuError", error->errmess);
		}

		gui_save_content = 0;
	}
}



/**
 * Does the actual saving
 *
 * \param  c     content to save (or 0 for other)
 * \param  path  path to save as
 * \return  true on success, false on error and error reported
 */

bool ro_gui_save_content(struct content *c, char *path)
{
	os_error *error;

	switch (gui_save_current_type) {
#ifdef WITH_DRAW_EXPORT
		case GUI_SAVE_DRAW:
			return save_as_draw(c, path);
#endif
#ifdef WITH_TEXT_EXPORT
		case GUI_SAVE_TEXT:
			save_as_text(c, path);
			xosfile_set_type(path, 0xfff);
			break;
#endif
#ifdef WITH_SAVE_COMPLETE
		case GUI_SAVE_COMPLETE:
			assert(c);
			if (c->type == CONTENT_HTML) {
				if (strcmp(path, "<Wimp$Scrap>"))
					return ro_gui_save_complete(c, path);

				/* we can't send a whole directory to another application,
				 * so just send the HTML source */
				gui_save_current_type = GUI_SAVE_SOURCE;
			}
			else
				gui_save_current_type = GUI_SAVE_OBJECT_ORIG;	/* \todo do this earlier? */
			/* no break */
#endif
		case GUI_SAVE_SOURCE:
		case GUI_SAVE_OBJECT_ORIG:
			error = xosfile_save_stamped(path,
					ro_content_filetype(c),
					c->source_data,
					c->source_data + c->source_size);
			if (error) {
				LOG(("xosfile_save_stamped: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("SaveError", error->errmess);
				return false;
			}
			break;

		case GUI_SAVE_OBJECT_NATIVE:
			ro_gui_save_object_native(c, path);
			break;

		case GUI_SAVE_LINK_URI:
			return ro_gui_save_link(c, LINK_ACORN, path);

		case GUI_SAVE_LINK_URL:
			return ro_gui_save_link(c, LINK_ANT, path);

		case GUI_SAVE_LINK_TEXT:
			return ro_gui_save_link(c, LINK_TEXT, path);

		case GUI_SAVE_HOTLIST_EXPORT_HTML:
			if (!options_save_tree(hotlist_tree, path, "NetSurf hotlist"))
				return false;
			error = xosfile_set_type(path, 0xfaf);
			if (error)
				LOG(("xosfile_set_type: 0x%x: %s",
						error->errnum, error->errmess));
			break;
		case GUI_SAVE_HISTORY_EXPORT_HTML:
			if (!options_save_tree(global_history_tree, path, "NetSurf history"))
				return false;
			error = xosfile_set_type(path, 0xfaf);
			if (error)
				LOG(("xosfile_set_type: 0x%x: %s",
						error->errnum, error->errmess));
			break;

		case GUI_SAVE_TEXT_SELECTION:
			selection_save_text(gui_save_selection, path);
			xosfile_set_type(path, 0xfff);
			break;

		case GUI_SAVE_CLIPBOARD_CONTENTS:
			return ro_gui_save_clipboard(path);

		default:
			LOG(("Unexpected content type: %d, path %s", gui_save_current_type, path));
			return false;
	}
	return true;
}


/**
 * Prepare an application directory and save_complete() to it.
 *
 * \param  c     content of type CONTENT_HTML to save
 * \param  path  path to save as
 * \return  true on success, false on error and error reported
 */

#define WIDTH 64
#define HEIGHT 64
#define SPRITE_SIZE (16 + 44 + ((WIDTH / 2 + 3) & ~3) * HEIGHT / 2)

#ifdef WITH_SAVE_COMPLETE

bool ro_gui_save_complete(struct content *c, char *path)
{
	char buf[256];
	FILE *fp;
	os_error *error;
	osspriteop_area *area;
	osspriteop_header *sprite_header;
	char *appname;
	unsigned int index;

        /* Create dir */
	error = xosfile_create_dir(path, 0);
	if (error) {
		LOG(("xosfile_create_dir: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		return false;
	}

        /* Save !Run file */
	snprintf(buf, sizeof buf, "%s.!Run", path);
	fp = fopen(buf, "w");
	if (!fp) {
		LOG(("fopen(): errno = %i", errno));
		warn_user("SaveError", strerror(errno));
		return false;
	}
	fprintf(fp, "Filer_Run <Obey$Dir>.index\n");
	fclose(fp);
	error = xosfile_set_type(buf, 0xfeb);
	if (error) {
		LOG(("xosfile_set_type: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		return false;
	}

	/* Create !Sprites */
	snprintf(buf, sizeof buf, "%s.!Sprites", path);
	appname = strrchr(path, '.');
	if (!appname)
		appname = path;

  	area = thumbnail_initialise(34, 34, os_MODE8BPP90X90);
  	if (!area) {
		warn_user("NoMemory", 0);
		return false;
	}
	sprite_header = (osspriteop_header *)(area + 1);
	strncpy(sprite_header->name, appname + 1, 12);

	/* Paint gets confused with uppercase characters */
	for (index = 0; index < 12; index++)
		sprite_header->name[index] = tolower(sprite_header->name[index]);
	thumbnail_create(c, area,
			(osspriteop_header *) ((char *) area + 16),
			34, 34);
	error = xosspriteop_save_sprite_file(osspriteop_NAME, area, buf);
	free(area);
	if (error) {
		LOG(("xosspriteop_save_sprite_file: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
	        return false;
	}

	return save_complete(c, path);
}

#endif

void ro_gui_save_object_native(struct content *c, char *path)
{

	switch (c->type) {
#ifdef WITH_JPEG
		case CONTENT_JPEG:
			bitmap_save(c->bitmap, path);
			break;
#endif
#ifdef WITH_PNG
		case CONTENT_PNG:
/*			error = xosspriteop_save_sprite_file(osspriteop_USER_AREA, c->data.png.sprite_area, path);
			break;*/
#endif
#ifdef WITH_MNG
		case CONTENT_JNG:
		case CONTENT_MNG:
			bitmap_save(c->bitmap, path);
			break;
#endif
#ifdef WITH_GIF
		case CONTENT_GIF:
			bitmap_save(c->bitmap, path);
			break;
#endif
		default:
			break;
	}
}


/**
 * Save a link file.
 *
 * \param  c       content to save link to
 * \param  format  format of link file
 * \param  path    pathname for link file
 * \return  true on success, false on failure and reports the error
 */

bool ro_gui_save_link(struct content *c, link_format format, char *path)
{
	FILE *fp = fopen(path, "w");

	if (!fp) {
		warn_user("SaveError", strerror(errno));
		return false;
	}

	switch (format) {
		case LINK_ACORN: /* URI */
			fprintf(fp, "%s\t%s\n", "URI", "100");
			fprintf(fp, "\t# NetSurf %s\n\n", netsurf_version);
			fprintf(fp, "\t%s\n", c->url);
			if (c->title)
				fprintf(fp, "\t%s\n", c->title);
			else
				fprintf(fp, "\t*\n");
			break;
		case LINK_ANT: /* URL */
		case LINK_TEXT: /* Text */
			fprintf(fp, "%s\n", c->url);
			break;
	}

	fclose(fp);

	switch (format) {
		case LINK_ACORN: /* URI */
			xosfile_set_type(path, 0xf91);
			break;
		case LINK_ANT: /* URL */
			xosfile_set_type(path, 0xb28);
			break;
		case LINK_TEXT: /* Text */
			xosfile_set_type(path, 0xfff);
			break;
	}

	return true;
}
