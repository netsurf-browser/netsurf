/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Rob Jackson <jacko@xms.ms>
 */

#include <assert.h>
#include <string.h>
#include "oslib/mimemap.h"
#include "oslib/osfile.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


static wimp_window *download_template;


static void ro_gui_download_leaf(const char *url, char *leaf);


/**
 * Load the download window template.
 */

void ro_gui_download_init(void)
{
	download_template = ro_gui_dialog_load_template("download");
}


/**
 * Create and open a download progress window.
 */

gui_window *gui_create_download_window(struct content *content)
{
	gui_window *g = xcalloc(1, sizeof(gui_window));
	os_error *e;

	assert(content->type == CONTENT_OTHER);

	g->type = GUI_DOWNLOAD_WINDOW;
	g->data.download.content = content;

	/* convert MIME type to RISC OS file type */
	e = xmimemaptranslate_mime_type_to_filetype(content->mime_type,
			&(g->data.download.file_type));
	if (e)
		g->data.download.file_type = 0xffd;

	/* fill in download window icons */
	download_template->icons[ICON_DOWNLOAD_URL].data.indirected_text.text =
		content->url;
	download_template->icons[ICON_DOWNLOAD_URL].data.indirected_text.size =
		strlen(content->url) + 1;
	strncpy(g->status, content->status_message, 256);
	download_template->icons[ICON_DOWNLOAD_STATUS].data.indirected_text.text =
		g->status;
	download_template->icons[ICON_DOWNLOAD_STATUS].data.indirected_text.size =
		256;
	sprintf(g->data.download.sprite_name, "file_%x",
			g->data.download.file_type);
	e = xwimpspriteop_select_sprite(g->data.download.sprite_name, 0);
	if (e)
		strcpy(g->data.download.sprite_name, "file_xxx");
	download_template->icons[ICON_DOWNLOAD_ICON].data.indirected_sprite.id =
			(osspriteop_id) g->data.download.sprite_name;
	ro_gui_download_leaf(content->url, g->data.download.path);
	download_template->icons[ICON_DOWNLOAD_PATH].data.indirected_text.text =
		g->data.download.path;
	download_template->icons[ICON_DOWNLOAD_PATH].data.indirected_text.size =
		256;

	/* create and open the download window */
	g->window = wimp_create_window(download_template);
	ro_gui_dialog_open(g->window);

	g->data.download.download_status = download_INCOMPLETE;

	g->next = window_list;
	window_list = g;
	return g;
}


/**
 * Find a friendly RISC OS leaf name for a URL.
 */

void ro_gui_download_leaf(const char *url, char *leaf)
{
	char *slash;
	size_t len;
	unsigned int i;

	/* take url from last / to first non-RISC OS character, eg. '.' */
	slash = strrchr(url, '/');
	if (!slash) {
		strcpy(leaf, "download");
		return;
	}
	/*len = strspn(slash + 1, "0123456789" "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz");*/  /* over-paranoid */
	len = strlen(slash+1);
	if (40 < len)
		len = 40;
	strncpy(leaf, slash + 1, len);
	leaf[len] = 0;
        /* swap "." for "/" */
	for (i=0; i!=len; i++) {
	        if (leaf[i] == '.') {
	                leaf[i] = '/';
	        }
	}
}


/**
 * Refresh the status icon in the download window.
 */

void gui_download_window_update_status(gui_window *g)
{
	strncpy(g->status, g->data.download.content->status_message, 256);
	wimp_set_icon_state(g->window,
			ICON_DOWNLOAD_STATUS, 0, 0);
}


/**
 * Handle failed downloads.
 */

void gui_download_window_error(gui_window *g, const char *error)
{
	g->data.download.content = 0;

	/* place error message in status icon in red */
	strncpy(g->status, error, 256);
	wimp_set_icon_state(g->window,
			ICON_DOWNLOAD_STATUS,
			wimp_COLOUR_RED << wimp_ICON_FG_COLOUR_SHIFT,
			wimp_ICON_FG_COLOUR);

	/* grey out file and pathname icons */
	wimp_set_icon_state(g->window,
			ICON_DOWNLOAD_ICON, wimp_ICON_SHADED, 0);
	wimp_set_icon_state(g->window,
			ICON_DOWNLOAD_PATH, wimp_ICON_SHADED, 0);

	g->data.download.download_status = download_ERROR;
}


/**
 * Handle completed downloads.
 */

void gui_download_window_done(gui_window *g)
{
	snprintf(g->status, 256, messages_get("Downloaded"),
			human_friendly_bytesize(g->data.download.content->source_size));
	wimp_set_icon_state(g->window,
			ICON_DOWNLOAD_STATUS, 0, 0);

        // clear shaded path and icon icons
	wimp_set_icon_state(g->window,
			ICON_DOWNLOAD_ICON, 0, wimp_ICON_SHADED);
	wimp_set_icon_state(g->window,
			ICON_DOWNLOAD_PATH, 0, wimp_ICON_SHADED);

        g->data.download.download_status = download_COMPLETE;
}


/**
 * Handle clicks in a download window.
 */

void ro_download_window_click(struct gui_window *g, wimp_pointer *pointer)
{
	switch (pointer->i) {
		case ICON_DOWNLOAD_ABORT:
			if (g->data.download.download_status ==
					download_INCOMPLETE)
				fetch_abort(g->data.download.content->fetch);

			ro_download_window_close(g);
			break;

		case ICON_DOWNLOAD_ICON:
			if (g->data.download.download_status ==
					download_COMPLETE) {
				gui_current_drag_type = GUI_DRAG_DOWNLOAD_SAVE;
				current_gui = g;
				ro_gui_drag_icon(pointer);
			}
			break;
	}
}


/**
 * Handle User_Drag_Box event for a drag from a download window.
 */

void ro_download_drag_end(wimp_dragged *drag)
{
	wimp_pointer pointer;
	wimp_message message;

	wimp_get_pointer_info(&pointer);

	message.your_ref = 0;
	message.action = message_DATA_SAVE;
	message.data.data_xfer.w = pointer.w;
	message.data.data_xfer.i = pointer.i;
	message.data.data_xfer.pos.x = pointer.pos.x;
	message.data.data_xfer.pos.y = pointer.pos.y;
	message.data.data_xfer.est_size = (int)
			current_gui->data.download.content->source_size;
	message.data.data_xfer.file_type = current_gui->data.download.file_type;
	strncpy(message.data.data_xfer.file_name,
			current_gui->data.download.path, 212);
	message.size = 44 + ((strlen(message.data.data_xfer.file_name) + 4) &
			(~3u));

	wimp_send_message_to_window(wimp_USER_MESSAGE, &message,
			pointer.w, pointer.i);
}


/**
 * Handle Message_DataSaveAck for a drag from a download window.
 */

void ro_download_datasave_ack(wimp_message *message)
{
	char *data;
	char *data_end;
	os_error *error;

	assert(current_gui->data.download.download_status == download_COMPLETE);

	data = current_gui->data.download.content->source_data;
	data_end = data + current_gui->data.download.content->source_size;

	error = xosfile_save_stamped(message->data.data_xfer.file_name,
			current_gui->data.download.file_type,
			data, data_end);
	if (error) {
		LOG(("0x%x: %s\n", error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		return;
	}

	/* Ack successful save with message_DATA_LOAD */
	message->action = message_DATA_LOAD;
	message->your_ref = message->my_ref;
	wimp_send_message_to_window(wimp_USER_MESSAGE, message, message->data.data_xfer.w, message->data.data_xfer.i);

	ro_download_window_close(current_gui);
}


struct gui_window * ro_lookup_download_window_from_w(wimp_w window)
{
  gui_window* g;
  for (g = window_list; g != NULL; g = g->next)
  {
    if (g->type == GUI_DOWNLOAD_WINDOW)
    {
      if (g->window == window)
      {
        return g;
      }
    }
  }
  return NULL;
}

void ro_download_window_close(struct gui_window *g)
{
  // free contexts etc???

  wimp_close_window(g->window);
}
