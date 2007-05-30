/*
 * This file is part of NetSurf, http://netsurf-browser.org/
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
#include "desktop/gui.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "riscos/gui.h"
#include "riscos/message.h"
#include "riscos/save.h"
#include "riscos/textselection.h"
#include "render/form.h"
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

static bool copy_handler(const char *text, size_t length, bool space,
		struct box *box, void *handle);
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
		browser_window_mouse_drag_end(g->bw,
				ro_gui_mouse_click_state(pointer.buttons),
				pos.x, pos.y);
}


/**
 * Selection traversal routine for appending text to the current contents
 * of the clipboard.
 *
 * \param  text    pointer to text being added, or NULL for newline
 * \param  length  length of text to be appended (bytes)
 * \param  space   trailing space required after text
 * \param  box     pointer to text box, or NULL if from textplain
 * \param  handle  unused handle, we don't need one
 * \return true iff successful and traversal should continue
 */

bool copy_handler(const char *text, size_t length, bool space,
		struct box *box, void *handle)
{
	if (!text) {
		text = "\n";
		length = 1;
	}
	return gui_add_to_clipboard(text, length, space);
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
	if (clip_length) {
		utf8_convert_ret res;
		char *new_cb;

		res = utf8_to_local_encoding(clipboard, clip_length, &new_cb);
		if (res == UTF8_CONVERT_OK) {
			free(clipboard);
			clipboard = new_cb;
/* \todo utf8_to_local_encoding should return the length! */
			clip_alloc = clip_length = strlen(new_cb);
		}
	}

	if (!owns_clipboard) {
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
		if (clip_length > 0) {
			char *utf8;
			utf8_convert_ret ret;
			/* Clipboard is in local encoding so
			 * convert to UTF8 */
			ret = utf8_from_local_encoding(clipboard,
					clip_length, &utf8);
			if (ret == UTF8_CONVERT_OK) {
				browser_window_paste_text(g->bw, utf8,
						strlen(utf8), true);
				free(utf8);
			}
		}
	}
	else {
		wimp_full_message_data_request msg;
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
	struct box *textarea = NULL;
	struct box *text_box = NULL;
	struct browser_window *bw;
	struct content *content;
	int gadget_box_x = 0;
	int gadget_box_y = 0;
	struct gui_window *g;
	os_coord pos;
	int box_x = 0;
	int box_y = 0;

/* with autoscrolling, we will probably need to remember the gui_window and
   override the drag->w window handle which could be any window on the desktop */
	g = ro_gui_window_lookup(drag->w);

	if ((drag->flags & wimp_DRAGGING_TERMINATE_DRAG) || !g) {

		if (drag_claimed) {
			/* make sure that we erase the ghost caret */
			caret_remove(&ghost_caret);
		}

		drag_claimed = false;
		return;
	}

	if (!ro_gui_window_to_window_pos(g, drag->pos.x, drag->pos.y, &pos))
		return;

	bw = g->bw;
	content = bw->current_content;
	if (content && content->type == CONTENT_HTML &&
		content->data.html.layout) {

		struct box *box = content->data.html.layout;

		while ((box = box_at_point(box, pos.x, pos.y, &box_x, &box_y, &content))) {
			if (box->style &&
					box->style->visibility == CSS_VISIBILITY_HIDDEN)
				continue;

			if (box->gadget) {
				switch (box->gadget->type) {

					case GADGET_TEXTAREA:
						textarea = box;
						gadget_box_x = box_x;
						gadget_box_y = box_y;
					case GADGET_TEXTBOX:
					case GADGET_PASSWORD:
						text_box = box;
						break;

					default:	/* appease compiler */
						break;
				}
			}
		}
	}

	if (textarea) {
		int pixel_offset;
		int char_offset;

		/* draw/move the ghost caret */
		if (drag_claimed)
			caret_remove(&ghost_caret);
		else
			gui_window_set_pointer(g, GUI_POINTER_CARET);

		text_box = textarea_get_position(textarea, pos.x - gadget_box_x,
					pos.y - gadget_box_y, &char_offset, &pixel_offset);

		caret_set_position(&ghost_caret, bw, text_box, char_offset, pixel_offset);
		drag_claimed = true;
	}
	else {
		if (drag_claimed) {
			/* make sure that we erase the ghost caret */
			caret_remove(&ghost_caret);
		}
		drag_claimed = false;
	}

	if (drag_claimed) {
		wimp_full_message_drag_claim claim;
		os_error *error;

		claim.size = offsetof(wimp_full_message_drag_claim, file_types) + 8;
		claim.your_ref = drag->my_ref;
		claim.action = message_DRAG_CLAIM;
		claim.flags = wimp_DRAG_CLAIM_POINTER_CHANGED | wimp_DRAG_CLAIM_SUPPRESS_DRAGBOX;
		claim.file_types[0] = osfile_TYPE_TEXT;
		claim.file_types[1] = ~0;

		error = xwimp_send_message(wimp_USER_MESSAGE, (wimp_message*)&claim,
					drag->sender);
		if (error) {
			LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		drag_claimed = true;
	}
}



/**
 * Reset drag-and-drop state when drag completes (DataSave received)
 */

void ro_gui_selection_drag_reset(void)
{
	caret_remove(&ghost_caret);
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
