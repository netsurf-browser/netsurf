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
#include "oslib/osfile.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "netsurf/desktop/save_text.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/save_complete.h"
#include "netsurf/riscos/save_draw.h"
#include "netsurf/riscos/thumbnail.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

gui_save_type gui_current_save_type;

extern struct content *save_content;

typedef enum { LINK_ACORN, LINK_ANT, LINK_TEXT } link_format;

static bool ro_gui_save_complete(struct content *c, char *path);
static void ro_gui_save_object_native(struct content *c, char *path);
static bool ro_gui_save_link(struct content *c, link_format format, char *path);


/**
 * Handle clicks in the save dialog.
 */

void ro_gui_save_click(wimp_pointer *pointer)
{
	switch (pointer->i) {
	  	case ICON_SAVE_OK:
	  		/*	Todo: Try save, and report error NoPathError if needed */
	  		break; 
	  	case ICON_SAVE_CANCEL:
	  		if (pointer->buttons == wimp_CLICK_SELECT) {
	  		  	xwimp_close_window(pointer->w);
	  		  	xwimp_create_menu((wimp_menu *)-1, 0, 0);
	  		} else if (pointer->buttons == wimp_CLICK_ADJUST) {
	  			ro_gui_menu_prepare_save(save_content);
	  		}
	  		break;
		case ICON_SAVE_ICON:
			if (pointer->buttons == wimp_DRAG_SELECT) {
				gui_current_drag_type = GUI_DRAG_SAVE;
				ro_gui_drag_icon(pointer);
			}
			break;
	}
}


/**
 * Start drag of icon under the pointer.
 */

void ro_gui_drag_icon(wimp_pointer *pointer)
{
	char *sprite;
	os_box box = { pointer->pos.x - 34, pointer->pos.y - 34,
			pointer->pos.x + 34, pointer->pos.y + 34 };
	os_error *error;

	if (pointer->i == -1)
		return;

	sprite = ro_gui_get_icon_string(pointer->w, pointer->i);

	error = xdragasprite_start(dragasprite_HPOS_CENTRE |
			dragasprite_VPOS_CENTRE |
			dragasprite_BOUND_POINTER |
			dragasprite_DROP_SHADOW,
			(osspriteop_area *) 1, sprite, &box, 0);
	if (error) {
		LOG(("xdragasprite_start: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("DragError", error->errmess);
	}
}


/**
 * Handle User_Drag_Box event for a drag from the save dialog.
 */

void ro_gui_save_drag_end(wimp_dragged *drag)
{
	char *name;
	char *dot;
	wimp_pointer pointer;
	wimp_message message;

	wimp_get_pointer_info(&pointer);

	name = ro_gui_get_icon_string(dialog_saveas, ICON_SAVE_PATH);
	dot = strrchr(name, '.');
	if (dot)
		name = dot + 1;

	message.your_ref = 0;
	message.action = message_DATA_SAVE;
	message.data.data_xfer.w = pointer.w;
	message.data.data_xfer.i = pointer.i;
	message.data.data_xfer.pos.x = pointer.pos.x;
	message.data.data_xfer.pos.y = pointer.pos.y;
	message.data.data_xfer.est_size = 1000;
	message.data.data_xfer.file_type = 0xfaf;
	if (gui_current_save_type == GUI_SAVE_DRAW)
		message.data.data_xfer.file_type = 0xaff;
	if (gui_current_save_type == GUI_SAVE_COMPLETE) {
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
 * Handle Message_DataSaveAck for a drag from the save dialog.
 */

void ro_gui_save_datasave_ack(wimp_message *message)
{
	char *path = message->data.data_xfer.file_name;
	struct content *c = save_content;
	os_error *error;

	if (!save_content && gui_current_save_type != GUI_HOTLIST_EXPORT_HTML) {
		LOG(("unexpected DataSaveAck: save_content not set"));
		return;
	}

	ro_gui_set_icon_string(dialog_saveas, ICON_SAVE_PATH, path);

	switch (gui_current_save_type) {
		case GUI_SAVE_SOURCE:
			error = xosfile_save_stamped(path,
					ro_content_filetype(c),
					c->source_data,
					c->source_data + c->source_size);
			if (error) {
				LOG(("xosfile_save_stamped: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("SaveError", error->errmess);
				return;
			}
			break;

		case GUI_SAVE_COMPLETE:
			if (!ro_gui_save_complete(c, path))
				return;
			break;

		case GUI_SAVE_DRAW:
			if (!save_as_draw(c, path))
				return;
			break;

		case GUI_SAVE_TEXT:
			save_as_text(c, path);
			xosfile_set_type(path, 0xfff);
			break;

		case GUI_SAVE_OBJECT_ORIG:
			error = xosfile_save_stamped(path,
					ro_content_filetype(c),
					c->source_data,
					c->source_data + c->source_size);
			if (error) {
				LOG(("xosfile_save_stamped: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("SaveError", error->errmess);
				return;
			}
			break;

		case GUI_SAVE_OBJECT_NATIVE:
			ro_gui_save_object_native(c, path);
			break;

		case GUI_SAVE_LINK_URI:
			if (!ro_gui_save_link(c, LINK_ACORN, path))
				return;
			break;

		case GUI_SAVE_LINK_URL:
			if (!ro_gui_save_link(c, LINK_ANT, path))
				return;
			break;

		case GUI_SAVE_LINK_TEXT:
			if (!ro_gui_save_link(c, LINK_TEXT, path))
				return;
			break;
		case GUI_HOTLIST_EXPORT_HTML:
			ro_gui_hotlist_save_as(path);
			break;
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

	save_content = 0;
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



void ro_gui_save_object_native(struct content *c, char *path)
{
        os_error *error;
        osspriteop_area *temp;

        switch (c->type) {
                case CONTENT_JPEG:
                        error = xosspriteop_save_sprite_file(osspriteop_USER_AREA, c->data.jpeg.sprite_area, path);
                        break;
                case CONTENT_PNG:
                        error = xosspriteop_save_sprite_file(osspriteop_USER_AREA, c->data.png.sprite_area, path);
                        break;
                case CONTENT_GIF:
                        /* create sprite area */
                        temp = calloc(c->data.gif.gif->frame_image->size+16,
                                      sizeof(char));
                        temp->size = c->data.gif.gif->frame_image->size+16;
                        temp->sprite_count = 1;
                        temp->first = 16;
                        temp->used = c->data.gif.gif->frame_image->size+16;
                        memcpy((char*)temp+16,
                               (char*)c->data.gif.gif->frame_image,
                               c->data.gif.gif->frame_image->size);
                        /* ensure extra words for name are null */
                        memset((char*)temp+24, 0, 8);
                        error = xosspriteop_save_sprite_file(osspriteop_USER_AREA, temp, path);
                        free(temp);
                        break;
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
