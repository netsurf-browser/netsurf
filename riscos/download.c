/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include <string.h>
#include "oslib/mimemap.h"
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
	char name[] = "download";
	int context, window_size, data_size;
	char *data;

	/* find required buffer sizes */
	context = wimp_load_template(wimp_GET_SIZE, 0, 0, wimp_NO_FONTS,
			name, 0, &window_size, &data_size);
	assert(context != 0);

	download_template = xcalloc((unsigned int) window_size, 1);
	data = xcalloc((unsigned int) data_size, 1);

	/* load */
	wimp_load_template(download_template, data, data + data_size,
			wimp_NO_FONTS, name, 0, 0, 0);
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
	sprintf(g->data.download.sprite_name, "Sfile_%x",
			g->data.download.file_type);
	e = xwimpspriteop_select_sprite(g->data.download.sprite_name + 1, 0);
	if (e)
		strcpy(g->data.download.sprite_name, "Sfile_xxx");
	download_template->icons[ICON_DOWNLOAD_ICON].data.indirected_text.validation =
		g->data.download.sprite_name;
	ro_gui_download_leaf(content->url, g->data.download.path);
	download_template->icons[ICON_DOWNLOAD_PATH].data.indirected_text.text =
		g->data.download.path;
	download_template->icons[ICON_DOWNLOAD_PATH].data.indirected_text.size =
		256;

	/* create and open the download window */
	g->data.download.window = wimp_create_window(download_template);
	ro_gui_dialog_open(g->data.download.window);

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

	/* take url from last / to first non-RISC OS character, eg. '.' */
	slash = strrchr(url, '/');
	if (!slash) {
		strcpy(leaf, "download");
		return;
	}
	len = strspn(slash + 1, "0123456789" "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz");  /* over-paranoid */
	if (40 < len)
		len = 40;
	strncpy(leaf, slash + 1, len);
	leaf[len] = 0;
}


/**
 * Refresh the status icon in the download window.
 */

void gui_download_window_update_status(gui_window *g)
{
	strncpy(g->status, g->data.download.content->status_message, 256);
	wimp_set_icon_state(g->data.download.window,
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
	wimp_set_icon_state(g->data.download.window,
			ICON_DOWNLOAD_STATUS,
			wimp_COLOUR_RED << wimp_ICON_FG_COLOUR_SHIFT,
			wimp_ICON_FG_COLOUR);

	/* grey out file and pathname icons */
	wimp_set_icon_state(g->data.download.window,
			ICON_DOWNLOAD_ICON, wimp_ICON_SHADED, 0);
	wimp_set_icon_state(g->data.download.window,
			ICON_DOWNLOAD_PATH, wimp_ICON_SHADED, 0);
}


/**
 * Handle completed downloads.
 */

void gui_download_window_done(gui_window *g)
{
	snprintf(g->status, 256, messages_get("Downloaded"),
			g->data.download.content->data.other.length);
	wimp_set_icon_state(g->data.download.window,
			ICON_DOWNLOAD_STATUS, 0, 0);
}

