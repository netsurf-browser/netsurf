/*
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
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

/** \file
  * Text selection code (platform-dependent implementation)
  */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "oslib/osfile.h"
#include "oslib/wimp.h"
#include "content/hlcache.h"
#include "desktop/gui.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "riscos/gui.h"
#include "riscos/menus.h"
#include "riscos/message.h"
#include "riscos/save.h"
#include "riscos/textselection.h"
#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/utils.h"

#ifndef wimp_DRAG_CLAIM_SUPPRESS_DRAGBOX
#define wimp_DRAG_CLAIM_SUPPRESS_DRAGBOX ((wimp_drag_claim_flags) 0x2u)
#endif



/** Receive of Dragging message has claimed it */
static bool dragging_claimed = false;
static wimp_t dragging_claimant;
static os_box dragging_box = { -34, -34, 34, 34 };  /* \todo - size properly */
static wimp_drag_claim_flags last_claim_flags = 0;
static struct gui_window *last_start_window;

static bool drag_claimed = false;

static bool owns_clipboard = false;
static bool owns_caret_and_selection = false;

/* current clipboard contents if we own the clipboard */
static char *clipboard = NULL;
static size_t clip_alloc = 0;
static size_t clip_length = 0;

static void ro_gui_discard_clipboard_contents(void);
static void ro_gui_dragging_bounced(wimp_message *message);


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

	error = xwimp_send_message(wimp_USER_MESSAGE,
			(wimp_message*)&msg, wimp_BROADCAST);
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
	gui_track_gui_window = g;

	drag.type = wimp_DRAG_USER_POINT;
	/* Don't constrain mouse pointer during drags */
	drag.bbox.x0 = -16384;
	drag.bbox.y0 = -16384;
	drag.bbox.x1 = 16384;
	drag.bbox.y1 = 16384;

	error = xwimp_drag_box(&drag);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	last_start_window = g;
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
	wimp_pointer pointer;
	os_error *error;
	os_coord pos;

	LOG(("ending text selection drag"));

	gui_current_drag_type = GUI_DRAG_NONE;

	scroll.w = g->window;
	error = xwimp_auto_scroll(0, &scroll, 0);
	if (error)
		LOG(("xwimp_auto_scroll: 0x%x: %s",
				error->errnum, error->errmess));

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

	if (ro_gui_window_to_window_pos(g, drag->final.x0, drag->final.y0, &pos))
		browser_window_mouse_track(g->bw, 0, pos.x, pos.y);
}


/**
 * Perform tasks after a selection has been cleared.
 *
 * \param g  gui window
 */

void gui_clear_selection(struct gui_window *g)
{
	/* Refresh any open menu, in case it's the browser window menu.  */
	ro_gui_menu_refresh(0);
}


/**
 * Core tells front end to put given text in clipboard
 *
 * \param  buffer    UTF-8 text, owned by core
 * \param  length    Byte length of UTF-8 text in buffer
 * \param  styles    Array of styles given to text runs, owned by core, or NULL
 * \param  n_styles  Number of text run styles in array
 */
void gui_set_clipboard(const char *buffer, size_t length,
		nsclipboard_styles styles[], int n_styles)
{
	utf8_convert_ret res;
	char *new_cb;

	if (length == 0)
		return;

	/* Convert to local encoding */
	res = utf8_to_local_encoding(buffer, length, &new_cb);

	if (res != UTF8_CONVERT_OK || new_cb == NULL)
		return;

	/* Replace existing clipboard contents with converted text */
	free(clipboard);
	clipboard = new_cb;
	clip_alloc = clip_length = strlen(new_cb);

	if (!owns_clipboard) {
		/* Tell RO we now own clipboard */
		wimp_full_message_claim_entity msg;
		os_error *error;

		LOG(("claiming clipboard"));

		msg.size = sizeof(msg);
		msg.your_ref = 0;
		msg.action = message_CLAIM_ENTITY;
		msg.flags = wimp_CLAIM_CLIPBOARD;

		error = xwimp_send_message(wimp_USER_MESSAGE,
				(wimp_message*)&msg, wimp_BROADCAST);
		if (error) {
			LOG(("xwimp_send_message: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		owns_clipboard = true;
	}

	LOG(("clipboard now holds %zd bytes", clip_length));
}


/**
 * Core asks front end for clipboard contents.
 *
 * \param  buffer  UTF-8 text, allocated by front end, ownership yeilded to core
 * \param  length  Byte length of UTF-8 text in buffer
 */
void gui_get_clipboard(char **buffer, size_t *length)
{
	*buffer = NULL;
	*length = 0;

	if (owns_clipboard) {
		if (clip_length > 0) {
			char *utf8;
			utf8_convert_ret ret;
			/* Clipboard is in local encoding so
			 * convert to UTF8 */
			ret = utf8_from_local_encoding(clipboard,
					clip_length, &utf8);
			if (ret == UTF8_CONVERT_OK) {
				*buffer = utf8;
				*length = strlen(utf8);
			}
		}
	} else {
		/** TODO: Handle case when we don't own the clipboard */

/*  http://www.starfighter.acornarcade.com/mysite/articles/SelectionModel.html
 */

/*		wimp_full_message_data_request msg;
		os_error *error;
		os_coord pos;

		if (!ro_gui_window_to_screen_pos(g, x, y, &pos))
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

		error = xwimp_send_message(wimp_USER_MESSAGE,
				(wimp_message*)&msg, wimp_BROADCAST);
		if (error) {
			LOG(("xwimp_send_message: 0x%x : %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
*/	}
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

		ro_gui_send_datasave(GUI_SAVE_CLIPBOARD_CONTENTS,
				&message, req->sender);
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
		LOG(("xosfile_save_stamped: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		return false;
	}
	return true;
}


/**
 * Handler for Message_Dragging, used to implement auto-scrolling and
 * ghost caret when a drag is in progress.
 */

void ro_gui_selection_dragging(wimp_message *message)
{
	wimp_full_message_dragging *drag = (wimp_full_message_dragging*)message;
	struct gui_window *g;
	os_coord pos;

	/* with autoscrolling, we will probably need to remember the
	 * gui_window and override the drag->w window handle which
	 * could be any window on the desktop */
	g = ro_gui_window_lookup(drag->w);

	if ((drag->flags & wimp_DRAGGING_TERMINATE_DRAG) || !g) {

		drag_claimed = false;
		return;
	}

	if (!ro_gui_window_to_window_pos(g, drag->pos.x, drag->pos.y, &pos))
		return;

	drag_claimed = false;
}



/**
 * Reset drag-and-drop state when drag completes (DataSave received)
 */

void ro_gui_selection_drag_reset(void)
{
	drag_claimed = false;
}


/**
 *
 */

void ro_gui_selection_drag_claim(wimp_message *message)
{
	wimp_full_message_drag_claim *claim = (wimp_full_message_drag_claim*)message;

	dragging_claimant = message->sender;
	dragging_claimed = true;

	/* have we been asked to remove the drag box/sprite? */
	if (claim->flags & wimp_DRAG_CLAIM_SUPPRESS_DRAGBOX) {
		ro_gui_drag_box_cancel();
	}
	else {
		/* \todo - restore it here? */
	}

	/* do we need to restore the default pointer shape? */
	if ((last_claim_flags & wimp_DRAG_CLAIM_POINTER_CHANGED) &&
			!(claim->flags & wimp_DRAG_CLAIM_POINTER_CHANGED)) {
		gui_window_set_pointer(last_start_window, GUI_POINTER_DEFAULT);
	}

	last_claim_flags = claim->flags;
}


void ro_gui_selection_send_dragging(wimp_pointer *pointer)
{
	wimp_full_message_dragging dragmsg;

	LOG(("sending DRAGGING to %p, %d", pointer->w, pointer->i));

	dragmsg.size = offsetof(wimp_full_message_dragging, file_types) + 8;
	dragmsg.your_ref = 0;
	dragmsg.action = message_DRAGGING;
	dragmsg.w = pointer->w;
	dragmsg.i = pointer->i;
	dragmsg.pos = pointer->pos;
/* \todo - this is interesting because it depends upon not just the state of the
      shift key, but also whether it /can/ be deleted, ie. from text area/input
      rather than page contents */
	dragmsg.flags = wimp_DRAGGING_FROM_SELECTION;
	dragmsg.box = dragging_box;
	dragmsg.file_types[0] = osfile_TYPE_TEXT;
	dragmsg.file_types[1] = ~0;

	/* if the message_dragmsg messages have been claimed we must address them
	   to the claimant task, which is not necessarily the task that owns whatever
	   window happens to be under the pointer */

	if (dragging_claimed) {
		ro_message_send_message(wimp_USER_MESSAGE_RECORDED,
				(wimp_message*)&dragmsg, dragging_claimant, ro_gui_dragging_bounced);
	}
	else {
		ro_message_send_message_to_window(wimp_USER_MESSAGE_RECORDED,
				(wimp_message*)&dragmsg, pointer->w, pointer->i,
				ro_gui_dragging_bounced, &dragging_claimant);
	}
}


/**
 * Our message_DRAGGING message was bounced, ie. the intended recipient does not
 * support the drag-and-drop protocol or cannot receive the data at the pointer
 * position.
 */

void ro_gui_dragging_bounced(wimp_message *message)
{
	dragging_claimed = false;
}
