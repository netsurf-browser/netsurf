/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Rob Jackson <jacko@xms.ms>
 */

/** \file
 * Download windows (RISC OS implementation).
 *
 * This file implements the interface given by desktop/gui.h for download
 * windows. Each download window has an associated fetch. Downloads start by
 * writing received data to a temporary file. At some point the user chooses
 * a destination (by drag & drop), and the temporary file is then moved to the
 * destination and the download continues until complete.
 */

#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "oslib/mimemap.h"
#include "oslib/osargs.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osfscontrol.h"
#include "oslib/osgbpb.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/content/fetch.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"


/** Data for a download window. */
struct gui_download_window {
	/** Associated fetch, or 0 if the fetch has completed or aborted. */
	struct fetch *fetch;
	unsigned int received;	/**< Amount of data received so far. */
	unsigned int total_size; /**< Size of resource, or 0 if unknown. */

	wimp_w window;		/**< RISC OS window handle. */
	bits file_type;		/**< RISC OS file type. */

	char url[256];		/**< Buffer for URL icon. */
	char sprite_name[20];	/**< Buffer for sprite icon. */
	char path[256];		/**< Buffer for pathname icon. */
	char status[256];	/**< Buffer for status icon. */

	/** User has chosen the destination, and it is being written. */
	bool saved;
	bool error;		/**< Error occurred, aborted. */
	/** RISC OS file handle, of temporary file when !saved, and of
	 * destination when saved. */
	os_fw file;

	struct timeval start_time;	/**< Time download started. */
	struct timeval last_time;	/**< Time status was last updated. */
	unsigned int last_received;	/**< Value of received at last_time. */

	struct gui_download_window *prev;	/**< Previous in linked list. */
	struct gui_download_window *next;	/**< Next in linked list. */
};


/** List of all download windows. */
static struct gui_download_window *download_window_list = 0;
/** Download window with current save operation. */
static struct gui_download_window *download_window_current = 0;

/** Template for a download window. */
static wimp_window *download_template;

/** Width of progress bar at 100%. */
static int download_progress_width;
/** Coordinates of progress bar. */
static int download_progress_x0;
static int download_progress_y0;
static int download_progress_y1;


static void ro_gui_download_update_status(struct gui_download_window *dw);
static void ro_gui_download_update_status_wrapper(void *p);
static void ro_gui_download_window_destroy_wrapper(void *p);


/**
 * Load the download window template.
 */

void ro_gui_download_init(void)
{
	download_template = ro_gui_dialog_load_template("download");
	download_progress_width =
		download_template->icons[ICON_DOWNLOAD_STATUS].extent.x1 -
		download_template->icons[ICON_DOWNLOAD_STATUS].extent.x0;
	download_progress_x0 =
		download_template->icons[ICON_DOWNLOAD_PROGRESS].extent.x0;
	download_progress_y0 =
		download_template->icons[ICON_DOWNLOAD_PROGRESS].extent.y0;
	download_progress_y1 =
		download_template->icons[ICON_DOWNLOAD_PROGRESS].extent.y1;
}


/**
 * Create and open a download progress window.
 *
 * \param  url         URL of download
 * \param  mime_type   MIME type sent by server
 * \param  fetch       fetch structure
 * \param  total_size  size of resource, or 0 if unknown
 * \return  a new gui_download_window structure, or 0 on error and error
 *          reported
 */

struct gui_download_window *gui_download_window_create(const char *url,
		const char *mime_type, struct fetch *fetch,
		unsigned int total_size)
{
	char *nice;
	char temp_name[40];
	struct gui_download_window *dw;
	os_error *error;

	dw = malloc(sizeof *dw);
	if (!dw) {
		warn_user("NoMemory", 0);
		return 0;
	}

	dw->fetch = fetch;
	dw->saved = false;
	dw->error = false;
	dw->received = 0;
	dw->total_size = total_size;
	strncpy(dw->url, url, sizeof dw->url);
	dw->url[sizeof dw->url - 1] = 0;
	dw->status[0] = 0;
	gettimeofday(&dw->start_time, 0);
	dw->last_time = dw->start_time;
	dw->last_received = 0;

	/* convert MIME type to RISC OS file type */
	error = xmimemaptranslate_mime_type_to_filetype(mime_type,
			&(dw->file_type));
	if (error) {
		LOG(("xmimemaptranslate_mime_type_to_filetype: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		dw->file_type = 0xffd;
	}

	/* open temporary output file */
	snprintf(temp_name, sizeof temp_name, "<Wimp$ScrapDir>.ns%x",
			(unsigned int) dw);
	error = xosfind_openoutw(osfind_NO_PATH | osfind_ERROR_IF_DIR,
			temp_name, 0, &dw->file);
	if (error) {
		LOG(("xosfind_openoutw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		free(dw);
		return 0;
	}

	/* fill in download window icons */
	download_template->icons[ICON_DOWNLOAD_URL].data.indirected_text.text =
			dw->url;
	download_template->icons[ICON_DOWNLOAD_URL].data.indirected_text.size =
			sizeof dw->url;

	download_template->icons[ICON_DOWNLOAD_STATUS].data.indirected_text.
			text = dw->status;
	download_template->icons[ICON_DOWNLOAD_STATUS].data.indirected_text.
			size = sizeof dw->status;

	sprintf(dw->sprite_name, "file_%.3x", dw->file_type);
	error = xwimpspriteop_select_sprite(dw->sprite_name, 0);
	if (error) {
		if (error->errnum != error_SPRITE_OP_DOESNT_EXIST) {
			LOG(("xwimpspriteop_select_sprite: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
		}
		strcpy(dw->sprite_name, "file_xxx");
	}
	download_template->icons[ICON_DOWNLOAD_ICON].data.indirected_sprite.id =
			(osspriteop_id) dw->sprite_name;

	strcpy(dw->path, messages_get("SaveObject"));
	if ((nice = url_nice(url))) {
		strcpy(dw->path, nice);
		free(nice);
	}
	download_template->icons[ICON_DOWNLOAD_PATH].data.indirected_text.text =
			dw->path;
	download_template->icons[ICON_DOWNLOAD_PATH].data.indirected_text.size =
			sizeof dw->path;

	download_template->icons[ICON_DOWNLOAD_DESTINATION].data.
			indirected_text.text = dw->path;
	download_template->icons[ICON_DOWNLOAD_DESTINATION].data.
			indirected_text.size = sizeof dw->path;

	download_template->icons[ICON_DOWNLOAD_DESTINATION].flags |=
			wimp_ICON_DELETED;

	/* create and open the download window */
	error = xwimp_create_window(download_template, &dw->window);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		free(dw);
		return 0;
	}

	dw->prev = 0;
	dw->next = download_window_list;
	download_window_list = dw;

	ro_gui_download_update_status(dw);

	ro_gui_dialog_open(dw->window);

	return dw;
}


/**
 * Handle received download data.
 *
 * \param  dw    download window
 * \param  data  pointer to block of data received
 * \param  size  size of data
 */

void gui_download_window_data(struct gui_download_window *dw, const char *data,
		unsigned int size)
{
	int unwritten;
	os_error *error;

	error = xosgbpb_writew(dw->file, data, size, &unwritten);
	if (error) {
		LOG(("xosgbpb_writew: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		fetch_abort(dw->fetch);
		gui_download_window_error(dw, error->errmess);
		return;
	}
	if (unwritten) {
		LOG(("xosgbpb_writew: unwritten %i", unwritten));
		warn_user("SaveError", messages_get("Unwritten"));
		fetch_abort(dw->fetch);
		gui_download_window_error(dw, messages_get("Unwritten"));
		return;
	}

	dw->received += size;
}


/**
 * Update the status text and progress bar.
 *
 * \param  dw  download window
 */

void ro_gui_download_update_status(struct gui_download_window *dw)
{
	char *received;
	char *total_size;
	char *speed;
	char time[20] = "?";
	float f = 0;
	struct timeval t;
	float dt;
	unsigned int left;
	float rate;
	os_error *error;

	gettimeofday(&t, 0);
	dt = (t.tv_sec + 0.000001 * t.tv_usec) - (dw->last_time.tv_sec +
			0.000001 * dw->last_time.tv_usec);
	if (dt == 0)
		dt = 0.001;

	total_size = human_friendly_bytesize(dw->total_size);

	if (dw->fetch) {
		rate = (dw->received - dw->last_received) / dt;
		received = human_friendly_bytesize(dw->received);
		speed = human_friendly_bytesize(rate);
		if (dw->total_size) {
			if (rate) {
				left = (dw->total_size - dw->received) / rate;
				sprintf(time, "%u:%.2u", left / 60, left % 60);
			}
			snprintf(dw->status, sizeof dw->status,
					messages_get("Download"),
					received, total_size, speed, time);
		} else {
			left = t.tv_sec - dw->start_time.tv_sec;
			sprintf(time, "%u:%.2u", left / 60, left % 60);
			snprintf(dw->status, sizeof dw->status,
					messages_get("DownloadU"),
					received, speed, time);
		}
	} else {
		left = dw->last_time.tv_sec - dw->start_time.tv_sec;
		if (left == 0)
			left = 1;
		rate = (float) dw->received / (float) left;
		sprintf(time, "%u:%.2u", left / 60, left % 60);
		speed = human_friendly_bytesize(rate);
		snprintf(dw->status, sizeof dw->status,
				messages_get("Downloaded"),
				total_size, speed, time);
	}

	dw->last_time = t;
	dw->last_received = dw->received;

	if (dw->total_size)
		f = (float) dw->received /
				(float) dw->total_size;
	error = xwimp_resize_icon(dw->window, ICON_DOWNLOAD_PROGRESS,
			download_progress_x0,
			download_progress_y0,
			download_progress_x0 + download_progress_width * f,
			download_progress_y1);
	if (error) {
		LOG(("xwimp_resize_icon: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	error = xwimp_set_icon_state(dw->window, ICON_DOWNLOAD_STATUS, 0, 0);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	if (dw->fetch)
		schedule(100, ro_gui_download_update_status_wrapper, dw);
	else
		schedule_remove(ro_gui_download_update_status_wrapper, dw);
}


/**
 * Wrapper for ro_gui_download_update_status(), suitable for schedule().
 */

void ro_gui_download_update_status_wrapper(void *p)
{
	ro_gui_download_update_status((struct gui_download_window *) p);
}


/**
 * Handle failed downloads.
 *
 * \param  dw         download window
 * \param  error_msg  error message
 */

void gui_download_window_error(struct gui_download_window *dw,
		const char *error_msg)
{
	os_error *error;

	dw->fetch = 0;
	dw->error = true;

	schedule_remove(ro_gui_download_update_status_wrapper, dw);

	/* place error message in status icon in red */
	strncpy(dw->status, error_msg, sizeof dw->status);
	error = xwimp_set_icon_state(dw->window,
			ICON_DOWNLOAD_STATUS,
			wimp_COLOUR_RED << wimp_ICON_FG_COLOUR_SHIFT,
			wimp_ICON_FG_COLOUR);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	/* grey out pathname icon */
	error = xwimp_set_icon_state(dw->window, ICON_DOWNLOAD_PATH,
			wimp_ICON_SHADED, 0);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Handle completed downloads.
 *
 * \param  dw  download window
 */

void gui_download_window_done(struct gui_download_window *dw)
{
	os_error *error;

	dw->fetch = 0;
	ro_gui_download_update_status(dw);

	error = xosfind_closew(dw->file);
	if (error) {
		LOG(("xosfind_closew: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
	}
	dw->file = 0;

	if (dw->saved) {
		error = xosfile_set_type(dw->path,
				dw->file_type);
		if (error) {
			LOG(("xosfile_set_type: 0x%x: %s",
				error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}

		schedule(200, ro_gui_download_window_destroy_wrapper, dw);
	}
}


/**
 * Convert a RISC OS window handle to a gui_download_window.
 *
 * \param  w  RISC OS window handle
 * \return  pointer to a structure if found, 0 otherwise
 */

struct gui_download_window * ro_gui_download_window_lookup(wimp_w w)
{
	struct gui_download_window *dw;
	for (dw = download_window_list; dw; dw = dw->next)
		if (dw->window == w)
			return dw;
	return 0;
}


/**
 * Handle Mouse_Click events in a download window.
 *
 * \param  dw       download window
 * \param  pointer  block returned by Wimp_Poll
 */

void ro_gui_download_window_click(struct gui_download_window *dw,
		wimp_pointer *pointer)
{
	char command[256] = "Filer_OpenDir ";
	char *dot;
	os_error *error;

	if (pointer->i == ICON_DOWNLOAD_ICON && !dw->error &&
			!dw->saved) {
		gui_current_drag_type = GUI_DRAG_DOWNLOAD_SAVE;
		download_window_current = dw;
		ro_gui_drag_icon(pointer);

	} else if (pointer->i == ICON_DOWNLOAD_DESTINATION) {
		strncpy(command + 14, dw->path, 242);
		command[255] = 0;
		dot = strrchr(command, '.');
		if (dot) {
			*dot = 0;
			error = xos_cli(command);
			if (error) {
				LOG(("xos_cli: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MiscError", error->errmess);
			}
		}
	}
}


/**
 * Handle User_Drag_Box event for a drag from a download window.
 *
 * \param  drag  block returned by Wimp_Poll
 */

void ro_gui_download_drag_end(wimp_dragged *drag)
{
	wimp_pointer pointer;
	wimp_message message;
	struct gui_download_window *dw = download_window_current;
	os_error *error;

	if (dw->saved || dw->error)
		return;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	message.your_ref = 0;
	message.action = message_DATA_SAVE;
	message.data.data_xfer.w = pointer.w;
	message.data.data_xfer.i = pointer.i;
	message.data.data_xfer.pos.x = pointer.pos.x;
	message.data.data_xfer.pos.y = pointer.pos.y;
	message.data.data_xfer.est_size = dw->total_size ? dw->total_size :
			dw->received;
	message.data.data_xfer.file_type = dw->file_type;
	strncpy(message.data.data_xfer.file_name, dw->path, 212);
	message.size = 44 + ((strlen(message.data.data_xfer.file_name) + 4) &
			(~3u));

	error = xwimp_send_message_to_window(wimp_USER_MESSAGE, &message,
			pointer.w, pointer.i, 0);
	if (error) {
		LOG(("xwimp_send_message_to_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Handle Message_DataSaveAck for a drag from a download window.
 *
 * \param  message  block returned by Wimp_Poll
 */

void ro_gui_download_datasave_ack(wimp_message *message)
{
	char temp_name[40];
	char *file_name;
	struct gui_download_window *dw = download_window_current;
	os_error *error;

	if (dw->saved || dw->error)
		return;

	file_name = message->data.data_xfer.file_name;
	snprintf(temp_name, sizeof temp_name, "<Wimp$ScrapDir>.ns%x",
			(unsigned int) dw);

	/* close temporary file */
	if (dw->file) {
		error = xosfind_closew(dw->file);
		dw->file = 0;
		if (error) {
			LOG(("xosfind_closew: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
			if (dw->fetch)
				fetch_abort(dw->fetch);
			gui_download_window_error(dw, error->errmess);
			return;
		}
	}

	/* move or copy temporary file to destination file */
	error = xosfscontrol_rename(temp_name, file_name);
	if (error && error->errnum == error_BAD_RENAME) {
		/* rename failed: copy with delete */
		error = xosfscontrol_copy(temp_name, file_name,
				osfscontrol_COPY_FORCE |
				osfscontrol_COPY_DELETE |
				osfscontrol_COPY_LOOK,
				0, 0, 0, 0, 0);
		if (error) {
			LOG(("xosfscontrol_copy: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
			if (dw->fetch)
				fetch_abort(dw->fetch);
			gui_download_window_error(dw, error->errmess);
			return;
		}
	} else if (error) {
		LOG(("xosfscontrol_rename: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		if (dw->fetch)
			fetch_abort(dw->fetch);
		gui_download_window_error(dw, error->errmess);
		return;
	}

	if (dw->fetch) {
		/* open new destination file if still fetching */
		error = xosfile_write(file_name, 0xdeaddead, 0xdeaddead,
				fileswitch_ATTR_OWNER_READ |
				fileswitch_ATTR_OWNER_WRITE);
		if (error) {
			LOG(("xosfile_write: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}

		error = xosfind_openupw(osfind_NO_PATH | osfind_ERROR_IF_DIR,
				file_name, 0, &dw->file);
		if (error) {
			LOG(("xosfind_openupw: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
			if (dw->fetch)
				fetch_abort(dw->fetch);
			gui_download_window_error(dw, error->errmess);
			return;
		}

		error = xosargs_set_ptrw(dw->file, dw->received);
		if (error) {
			LOG(("xosargs_set_ptrw: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
			if (dw->fetch)
				fetch_abort(dw->fetch);
			gui_download_window_error(dw, error->errmess);
			return;
		}

	} else {
		/* otherwise just set the file type */
		error = xosfile_set_type(file_name,
				dw->file_type);
		if (error) {
			LOG(("xosfile_set_type: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}
	}

	dw->saved = true;
	strncpy(dw->path, file_name, sizeof dw->path);

	/* hide writeable path icon and show destination icon */
	error = xwimp_set_icon_state(dw->window, ICON_DOWNLOAD_PATH,
			wimp_ICON_DELETED, wimp_ICON_DELETED);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	error = xwimp_set_icon_state(dw->window,
			ICON_DOWNLOAD_DESTINATION, 0, wimp_ICON_DELETED);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	/* Ack successful save with message_DATA_LOAD */
	message->action = message_DATA_LOAD;
	message->your_ref = message->my_ref;
	error = xwimp_send_message_to_window(wimp_USER_MESSAGE, message,
			message->data.data_xfer.w,
			message->data.data_xfer.i, 0);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	if (!dw->fetch)
		schedule(200, ro_gui_download_window_destroy_wrapper, dw);
}


/**
 * Close a download window and free any related resources.
 *
 * \param  dw  download window
 */

void ro_gui_download_window_destroy(struct gui_download_window *dw)
{
	char temp_name[40];
	os_error *error;

	schedule_remove(ro_gui_download_update_status_wrapper, dw);
	schedule_remove(ro_gui_download_window_destroy_wrapper, dw);

	/* remove from list */
	if (dw->prev)
		dw->prev->next = dw->next;
	else
		download_window_list = dw->next;
	if (dw->next)
		dw->next->prev = dw->prev;

	/* delete window */
	error = xwimp_delete_window(dw->window);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	/* close download file */
	if (dw->file) {
		error = xosfind_closew(dw->file);
		if (error) {
			LOG(("xosfind_closew: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}
	}

	/* delete temporary file */
	if (!dw->saved) {
		snprintf(temp_name, sizeof temp_name, "<Wimp$ScrapDir>.ns%x",
				(unsigned int) dw);
		error = xosfile_delete(temp_name, 0, 0, 0, 0, 0);
		if (error) {
			LOG(("xosfile_delete: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}
	}

	if (dw->fetch)
		fetch_abort(dw->fetch);

	free(dw);
}


/**
 * Wrapper for ro_gui_download_window_destroy(), suitable for schedule().
 */

void ro_gui_download_window_destroy_wrapper(void *p)
{
	ro_gui_download_window_destroy((struct gui_download_window *) p);
}
