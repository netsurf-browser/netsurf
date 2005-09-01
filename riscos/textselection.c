/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
 */

/** \file
  * Text selection code (platform-dependent implementation)
  */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "oslib/osfile.h"
#include "oslib/wimp.h"
#include "netsurf/desktop/selection.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utf8.h"
#include "netsurf/utils/utils.h"


static bool owns_clipboard = false;
static bool owns_caret_and_selection = false;

/* current clipboard contents if we own the clipboard */
static char *clipboard = NULL;
static size_t clip_alloc = 0;
static size_t clip_length = 0;

static bool copy_handler(struct box *box, int offset, size_t length, void *handle);
static void ro_gui_discard_clipboard_contents(void);


/**
 * Start drag-selecting text within a browser window (RO-dependent part)
 *
 * \param g  gui window
 */

void gui_start_selection(struct gui_window *g)
{
	wimp_full_message_claim_entity msg;
	wimp_auto_scroll_info scroll;
	wimp_window_state state;
	wimp_drag drag;
	os_error *error;

	LOG(("starting text_selection drag"));

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* claim caret and selection */
	msg.size = sizeof(msg);
	msg.your_ref = 0;
	msg.action = message_CLAIM_ENTITY;
	msg.flags = wimp_CLAIM_CARET_OR_SELECTION;

	error = xwimp_send_message(wimp_USER_MESSAGE, (wimp_message*)&msg, wimp_BROADCAST);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	owns_caret_and_selection = true;

	scroll.w = g->window;
	scroll.pause_zone_sizes.x0 = 80;
	scroll.pause_zone_sizes.y0 = 80;
	scroll.pause_zone_sizes.x1 = 80;
	scroll.pause_zone_sizes.y1 = 80;
	scroll.pause_duration = 0;
	scroll.state_change = (void *)0;
	error = xwimp_auto_scroll(wimp_AUTO_SCROLL_ENABLE_VERTICAL |
			wimp_AUTO_SCROLL_ENABLE_HORIZONTAL,
			&scroll, 0);
	if (error)
		LOG(("xwimp_auto_scroll: 0x%x: %s",
				error->errnum, error->errmess));

	gui_current_drag_type = GUI_DRAG_SELECTION;

	drag.type = wimp_DRAG_USER_POINT;
	drag.bbox.x0 = state.visible.x0;
	drag.bbox.y0 = state.visible.y0;
	drag.bbox.x1 = state.visible.x1;
	drag.bbox.y1 = state.visible.y1;

	error = xwimp_drag_box(&drag);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * End of text selection drag operation
 *
 * \param g        gui window
 * \param dragged  position of pointer at conclusion of drag
 */

void ro_gui_selection_drag_end(struct gui_window *g, wimp_dragged *drag)
{
	wimp_auto_scroll_info scroll;
	wimp_window_state state;
	wimp_pointer pointer;
	os_error *error;
	int x, y;

	gui_current_drag_type = GUI_DRAG_NONE;

	scroll.w = g->window;
	error = xwimp_auto_scroll(0, &scroll, 0);
	if (error)
		LOG(("xwimp_auto_scroll: 0x%x: %s", error->errnum, error->errmess));

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
 * Selection traversal routine for appending text to the current contents
 * of the clipboard.

 * \param box     pointer to text box being (partially) added
 * \param offset  start offset of text within box (bytes)
 * \param length  length of text to be appended (bytes)
 * \param handle  unused handle, we don't need one
 * \return true iff successful and traversal should continue
 */

bool copy_handler(struct box *box, int offset, size_t length, void *handle)
{
	bool space = false;
	const char *text;
	size_t len;

	if (box) {
    	len = min(length, box->length - offset);
		text = box->text + offset;
		if (box->space && length > len) space = true;
	}
	else {
		text = "\n";
		len = 1;
	}

	return gui_add_to_clipboard(text, len, space);
}


/**
 * Empty the clipboard, called prior to gui_add_to_clipboard and
 * gui_commit_clipboard
 *
 * \return true iff successful
 */

bool gui_empty_clipboard(void)
{
	const int init_size = 1024;

	if (!clip_alloc) {
		clipboard = malloc(init_size);
		if (!clipboard) {
			LOG(("out of memory"));
			warn_user("NoMemory", 0);
			return false;
		}
		clip_alloc = init_size;
	}

	clip_length = 0;

	return true;
}


/**
 * Add some text to the clipboard, optionally appending a trailing space.
 *
 * \param  text    text to be added
 * \param  length  length of text in bytes
 * \param  space   indicates whether a trailing space should be appended also
 * \return true iff successful
 */

bool gui_add_to_clipboard(const char *text, size_t length, bool space)
{
	size_t new_length = clip_length + length + (space ? 1 : 0);

	if (new_length > clip_alloc) {
		size_t new_alloc = clip_alloc + (clip_alloc / 4);
		char *new_cb;

		if (new_alloc < new_length) new_alloc = new_length;

		new_cb = realloc(clipboard, new_alloc);
		if (!new_cb) return false;

		clipboard = new_cb;
		clip_alloc = new_alloc;
	}

	memcpy(clipboard + clip_length, text, length);
	clip_length += length;
	if (space) clipboard[clip_length++] = ' ';

	return true;
}


/**
 * Commit the changes made by gui_empty_clipboard and gui_add_to_clipboard.
 *
 * \return true iff successful
 */

bool gui_commit_clipboard(void)
{
	utf8_convert_ret res;
	char *new_cb;

	res = utf8_to_local_encoding(clipboard, clip_length, &new_cb);
	if (res == UTF8_CONVERT_OK) {
		free(clipboard);
		clipboard = new_cb;
/* \todo utf8_to_local_encoding should return the length! */
		clip_alloc = clip_length = strlen(new_cb);
	}

	if (!owns_clipboard) {
		wimp_full_message_claim_entity msg;
		os_error *error;

		LOG(("claiming clipboard"));

		msg.size = sizeof(msg);
		msg.your_ref = 0;
		msg.action = message_CLAIM_ENTITY;
		msg.flags = wimp_CLAIM_CLIPBOARD;

		error = xwimp_send_message(wimp_USER_MESSAGE, (wimp_message*)&msg,
				wimp_BROADCAST);
		if (error) {
			LOG(("xwimp_send_message: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		owns_clipboard = true;
	}

	LOG(("clipboard now holds %d bytes", clip_length));

	return true;
}



/**
 * Copy the selected contents to the global clipboard,
 * and claim ownership of the clipboard from other apps.
 *
 * \param s  selection
 * \return true iff successful, ie. cut operation can proceed without losing data
 */

bool gui_copy_to_clipboard(struct selection *s)
{
	if (!gui_empty_clipboard())
		return false;

	selection_traverse(s, copy_handler, NULL);

	return gui_commit_clipboard();
}


/**
 * Request to paste the clipboard contents into a textarea/input field
 * at a given position. Note that the actual operation may take place
 * straight away (local clipboard) or in a number of chunks at some
 * later time (clipboard owned by another app).
 *
 * \param  g  gui window
 * \param  x  x ordinate at which to paste text
 * \param  y  y ordinate at which to paste text
 */

void gui_paste_from_clipboard(struct gui_window *g, int x, int y)
{
	if (owns_clipboard) {
		if (clip_length > 0)
			browser_window_paste_text(g->bw, clipboard, clip_length, true);
	}
	else {
		wimp_full_message_data_request msg;
		os_error *error;
		os_coord pos;

		if (!window_screen_pos(g, x, y, &pos))
			return;

		msg.size = sizeof(msg);
		msg.your_ref = 0;
		msg.action = message_DATA_REQUEST;
		msg.w = g->window;
		msg.i = -1;
		msg.pos.x = pos.x;
		msg.pos.y = pos.y;
		msg.flags = wimp_DATA_REQUEST_CLIPBOARD;
		msg.file_types[0] = osfile_TYPE_TEXT;
		msg.file_types[1] = ~0;

		error = xwimp_send_message(wimp_USER_MESSAGE, (wimp_message*)&msg,
				wimp_BROADCAST);
		if (error) {
			LOG(("xwimp_send_message: 0x%x : %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
}


/**
 * Discard the current contents of the clipboard, if any, releasing the
 * memory it uses.
 */

void ro_gui_discard_clipboard_contents(void)
{
	if (clip_alloc) free(clipboard);
	clip_alloc = 0;
	clip_length = 0;
}


/**
 * Responds to CLAIM_ENTITY message notifying us that the caret
 * and selection or clipboard have been claimed by another application.
 *
 * \param claim  CLAIM_ENTITY message
 */

void ro_gui_selection_claim_entity(wimp_full_message_claim_entity *claim)
{
	/* ignore our own broadcasts! */
	if (claim->sender != task_handle) {

		LOG(("%x", claim->flags));

		if (claim->flags & wimp_CLAIM_CARET_OR_SELECTION) {
			owns_caret_and_selection = false;
		}

		if (claim->flags & wimp_CLAIM_CLIPBOARD) {
			ro_gui_discard_clipboard_contents();
			owns_clipboard = false;
		}
	}
}


/**
 * Responds to DATA_REQUEST message, returning information about the
 * clipboard contents if we own the clipboard.
 *
 * \param  req  DATA_REQUEST message
 */

void ro_gui_selection_data_request(wimp_full_message_data_request *req)
{
	if (owns_clipboard && clip_length > 0 &&
		(req->flags & wimp_DATA_REQUEST_CLIPBOARD)) {
		wimp_full_message_data_xfer message;
		int size;
//		int i;

//		for(i = 0; i < NOF_ELEMENTS(req->file_types); i++) {
//			bits ftype = req->file_types[i];
//			if (ftype == ~0U) break;	/* list terminator */
//
//			LOG(("type %x", ftype));
//			i++;
//		}

		/* we can only supply text at the moment, so that's what you're getting! */
		size = offsetof(wimp_full_message_data_xfer, file_name) + 9;
		message.size = (size + 3) & ~3;
		message.your_ref = req->my_ref;
		message.action = message_DATA_SAVE;
		message.w = req->w;
		message.i = req->i;
		message.pos = req->pos;
		message.file_type = osfile_TYPE_TEXT;
		message.est_size = clip_length;
		memcpy(message.file_name, "TextFile", 9);

		ro_gui_send_datasave(GUI_SAVE_CLIPBOARD_CONTENTS, &message, req->sender);
	}
}


/**
 * Save the clipboard contents to a file.
 *
 * \param  path  the pathname of the file
 * \return true iff success, otherwise reporting the error before returning false
 */

bool ro_gui_save_clipboard(const char *path)
{
	os_error *error;

	assert(clip_length > 0 && clipboard);
	error = xosfile_save_stamped(path, osfile_TYPE_TEXT,
			(byte*)clipboard,
			(byte*)clipboard + clip_length);
	if (error) {
		LOG(("xosfile_save_stamped: 0x%x: %s", error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		return false;
	}
	return true;
}
