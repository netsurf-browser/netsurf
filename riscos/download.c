/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Rob Jackson <jacko@xms.ms>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
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
#include "netsurf/riscos/query.h"
#include "netsurf/riscos/wimp.h"
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
	bool close_confirmed;
	bool error;		/**< Error occurred, aborted. */
	/** RISC OS file handle, of temporary file when !saved, and of
	 * destination when saved. */
	os_fw file;

	query_id query;
	bool query_quit;

	struct timeval start_time;	/**< Time download started. */
	struct timeval last_time;	/**< Time status was last updated. */
	unsigned int last_received;	/**< Value of received at last_time. */

	bool send_dataload;	/**< Should send DataLoad message when finished */
	wimp_message save_message;	/**< Copy of wimp DataSaveAck message */

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
static bool ro_gui_download_save(struct gui_download_window *dw, const char *file_name);
static void ro_gui_download_send_dataload(struct gui_download_window *dw);
static void ro_gui_download_window_destroy_wrapper(void *p);
static void ro_gui_download_close_confirmed(query_id, enum query_response res, void *p);
static void ro_gui_download_close_cancelled(query_id, enum query_response res, void *p);

static const query_callback close_funcs =
{
	ro_gui_download_close_confirmed,
	ro_gui_download_close_cancelled,
	ro_gui_download_close_cancelled
};


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
	url_func_result res;

	dw = malloc(sizeof *dw);
	if (!dw) {
		warn_user("NoMemory", 0);
		return 0;
	}

	dw->fetch = fetch;
	dw->saved = false;
	dw->close_confirmed = false;
	dw->error = false;
	dw->query = QUERY_INVALID;
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
	if (!ro_gui_wimp_sprite_exists(dw->sprite_name))
		strcpy(dw->sprite_name, "file_xxx");
	download_template->icons[ICON_DOWNLOAD_ICON].data.indirected_sprite.id =
			(osspriteop_id) dw->sprite_name;

	if ((res = url_nice(url, &nice)) == URL_FUNC_OK) {
		strcpy(dw->path, nice);
		free(nice);
	}
	else
		strcpy(dw->path, messages_get("SaveObject"));

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
	if (download_window_list)
		download_window_list->prev = dw;
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

		if (dw->send_dataload)
			ro_gui_download_send_dataload(dw);

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
		const char *sprite = ro_gui_get_icon_string(pointer->w, pointer->i);
		gui_current_drag_type = GUI_DRAG_DOWNLOAD_SAVE;
		download_window_current = dw;
		ro_gui_drag_icon(pointer->pos.x, pointer->pos.y, sprite);

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
 * Handler Key_Press events in a download window.
 *
 * \param  dw       download window
 * \param  key  key press returned by Wimp_Poll
 * \return true iff key press handled
 */

bool ro_gui_download_window_keypress(struct gui_download_window *dw, wimp_key *key)
{
	switch (key->c)
	{
		case wimp_KEY_ESCAPE:
			ro_gui_download_window_destroy(dw, false);
			return true;

		case wimp_KEY_RETURN: {
				char *name = ro_gui_get_icon_string(dw->window, ICON_DOWNLOAD_PATH);
				if (!strrchr(name, '.'))
				{
					warn_user("NoPathError", NULL);
					return true;
				}
				ro_gui_convert_save_path(dw->path, sizeof dw->path, name);

				dw->send_dataload = false;
				if (ro_gui_download_save(dw, dw->path) && !dw->fetch)
				{
					/* finished already */
					schedule(200, ro_gui_download_window_destroy_wrapper, dw);
				}
				return true;
			}
			break;
	}

	/* ignore all other keypresses (F12 etc) */
	return false;
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
	const char *leaf;
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

	/* ignore drags to the download window itself */
	if (pointer.w == dw->window) return;

	leaf = strrchr(dw->path, '.');
	if (leaf)
		leaf++;
	else
		leaf = dw->path;
	ro_gui_convert_save_path(message.data.data_xfer.file_name, 212, leaf);

	message.your_ref = 0;
	message.action = message_DATA_SAVE;
	message.data.data_xfer.w = pointer.w;
	message.data.data_xfer.i = pointer.i;
	message.data.data_xfer.pos.x = pointer.pos.x;
	message.data.data_xfer.pos.y = pointer.pos.y;
	message.data.data_xfer.est_size = dw->total_size ? dw->total_size :
			dw->received;
	message.data.data_xfer.file_type = dw->file_type;
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
	struct gui_download_window *dw = download_window_current;

	dw->send_dataload = true;
	memcpy(&dw->save_message, message, sizeof(wimp_message));

	if (!ro_gui_download_save(dw, message->data.data_xfer.file_name))
		return;

	if (!dw->fetch)
	{
		/* Ack successful completed save with message_DATA_LOAD immediately
		   to reduce the chance of the target app getting confused by it
		   being delayed */

		ro_gui_download_send_dataload(dw);

		schedule(200, ro_gui_download_window_destroy_wrapper, dw);
	}
}


/**
 * Start of save operation, user has specified where the file should be saved.
 *
 * \param  dw         download window
 * \param  file_name  pathname of destination file
 * \return true iff save successfully initiated
 */

bool ro_gui_download_save(struct gui_download_window *dw, const char *file_name)
{
	char temp_name[40];
	os_error *error;
	wimp_caret caret;

	if (dw->saved || dw->error)
		return true;

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
			return false;
		}
	}

	/* move or copy temporary file to destination file */
	error = xosfscontrol_rename(temp_name, file_name);
	/* Errors from a filing system have number 0x1XXnn, where XX is the FS
	 * number, and nn the error number. 0x9F is "Not same disc". */
	if (error && (error->errnum == error_BAD_RENAME ||
			(error->errnum & 0xFF00FFu) == 0x1009Fu)) {
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
			return false;
		}
	} else if (error) {
		LOG(("xosfscontrol_rename: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		if (dw->fetch)
			fetch_abort(dw->fetch);
		gui_download_window_error(dw, error->errmess);
		return false;
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
			return false;
		}

		error = xosargs_set_ptrw(dw->file, dw->received);
		if (error) {
			LOG(("xosargs_set_ptrw: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
			if (dw->fetch)
				fetch_abort(dw->fetch);
			gui_download_window_error(dw, error->errmess);
			return false;
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

	/* hide the caret but preserve input focus */
	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	else if (caret.w == dw->window) {
		error = xwimp_set_caret_position(dw->window, (wimp_i)-1, 0, 0, 1 << 25, -1);
		if (error) {
			LOG(("xwimp_get_caret_position: 0x%x : %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}

	return true;
}


/**
 * Send DataLoad message in response to DataSaveAck, informing the
 * target application that the transfer is complete.
 *
 * \param  dw  download window
 */

void ro_gui_download_send_dataload(struct gui_download_window *dw)
{
	/* Ack successful save with message_DATA_LOAD */
	wimp_message *message = &dw->save_message;
	os_error *error;

	assert(dw->send_dataload);
	dw->send_dataload = false;

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

	schedule(200, ro_gui_download_window_destroy_wrapper, dw);
}


/**
 * Close a download window and free any related resources.
 *
 * \param  dw   download window
 * \param  quit destroying because we're quitting the whole app
 * \return true iff window destroyed, not waiting for user confirmation
 */

bool ro_gui_download_window_destroy(struct gui_download_window *dw, bool quit)
{
	bool safe = dw->saved && !dw->fetch;
	char temp_name[40];
	os_error *error;

	if (!safe && !dw->close_confirmed)
	{
		if (dw->query != QUERY_INVALID && dw->query_quit != quit) {
			query_close(dw->query);
			dw->query = QUERY_INVALID;
		}

		dw->query_quit = quit;
		if (dw->query == QUERY_INVALID)
			dw->query = query_user(quit ? "QuitDownload" : "AbortDownload",
					NULL, &close_funcs, dw);
		else
			ro_gui_query_window_bring_to_front(dw->query);

		return false;
	}

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

	return true;
}


/**
 * Wrapper for ro_gui_download_window_destroy(), suitable for schedule().
 */

void ro_gui_download_window_destroy_wrapper(void *p)
{
	struct gui_download_window *dw = p;
	if (dw->query != QUERY_INVALID)
		query_close(dw->query);
	dw->query = QUERY_INVALID;
	dw->close_confirmed = true;
	ro_gui_download_window_destroy(dw, false);
}


/**
 * User has opted to cancel the close, leaving the download to continue.
 */

void ro_gui_download_close_cancelled(query_id id, enum query_response res, void *p)
{
	struct gui_download_window *dw = p;
	dw->query = QUERY_INVALID;
}


/**
 * Download aborted, close window and tidy up.
 */

void ro_gui_download_close_confirmed(query_id id, enum query_response res, void *p)
{
	struct gui_download_window *dw = (struct gui_download_window *)p;
	dw->query = QUERY_INVALID;
	dw->close_confirmed = true;
	if (dw->query_quit) {

		/* destroy all our downloads */
		while (download_window_list)
			ro_gui_download_window_destroy_wrapper(download_window_list);

		/* and restart the shutdown */
		if (ro_gui_prequit())
			netsurf_quit = true;
	}
	else
		ro_gui_download_window_destroy(dw, false);
}


/**
 * Respond to PreQuit message, displaying a prompt message if we need
 * the user to confirm the shutdown.
 *
 * \return true iff we can shutdown straightaway
 */

bool ro_gui_download_prequit(void)
{
	while (download_window_list)
	{
		if (!ro_gui_download_window_destroy(download_window_list, true))
			return false;	/* awaiting user confirmation */
	}
	return true;
}
