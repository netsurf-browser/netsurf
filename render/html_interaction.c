/*
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2008 Michael Drake <tlsa@netsurf-browser.org>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>  
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
 * User interaction with a CONTENT_HTML (implementation).
 */

#include <assert.h>
#include <stdbool.h>

#include "content/content.h"
#include "desktop/browser.h"
#include "desktop/frames.h"
#include "desktop/mouse.h"
#include "desktop/options.h"
#include "desktop/scrollbar.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "render/box.h"
#include "render/font.h"
#include "render/form.h"
#include "render/html_internal.h"
#include "render/imagemap.h"
#include "render/textinput.h"
#include "utils/messages.h"
#include "utils/utils.h"


static gui_pointer_shape get_pointer_shape(struct browser_window *bw,
		struct box *box, bool imagemap);
static void html_box_drag_start(struct box *box, int x, int y);


/**
 * End overflow scroll scrollbar drags
 *
 * \param  h      html content's high level cache entry
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 */
static size_t html_selection_drag_end(struct html_content *html,
		browser_mouse_state mouse, int x, int y, int dir)
{
	int pixel_offset;
	struct box *box;
	int dx, dy;
	size_t idx = 0;

	box = box_pick_text_box(html, x, y, dir, &dx, &dy);
	if (box) {
		plot_font_style_t fstyle;

		font_plot_style_from_css(box->style, &fstyle);

		nsfont.font_position_in_string(&fstyle, box->text, box->length,
				dx, &idx, &pixel_offset);

		idx += box->byte_offset;
	}

	return idx;
}


/**
 * Handle mouse tracking (including drags) in an HTML content window.
 *
 * \param  c	  content of type html
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 */

void html_mouse_track(struct content *c, struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	html_content *html = (html_content*) c;

	if (bw->drag_type == DRAGGING_SELECTION && !mouse) {
		int dir = -1;
		size_t idx;

		if (selection_dragging_start(&html->sel))
			dir = 1;

		idx = html_selection_drag_end(html, mouse, x, y, dir);

		if (idx != 0)
			selection_track(&html->sel, mouse, idx);

		browser_window_set_drag_type(bw, DRAGGING_NONE, NULL);
	}

	switch (bw->drag_type) {
		case DRAGGING_SELECTION: {
			struct box *box;
			int dir = -1;
			int dx, dy;

			if (selection_dragging_start(&html->sel))
				dir = 1;

			box = box_pick_text_box(html, x, y, dir, &dx, &dy);

			if (box) {
				int pixel_offset;
				size_t idx;
				plot_font_style_t fstyle;

				font_plot_style_from_css(box->style, &fstyle);

				nsfont.font_position_in_string(&fstyle,
						box->text, box->length,
						dx, &idx, &pixel_offset);

				selection_track(&html->sel, mouse,
						box->byte_offset + idx);
			}
		}
		break;

		default:
			html_mouse_action(c, bw, mouse, x, y);
			break;
	}
}


/**
 * Handle mouse clicks and movements in an HTML content window.
 *
 * \param  c	  content of type html
 * \param  bw	  browser window
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  x	  coordinate of mouse
 * \param  y	  coordinate of mouse
 *
 * This function handles both hovering and clicking. It is important that the
 * code path is identical (except that hovering doesn't carry out the action),
 * so that the status bar reflects exactly what will happen. Having separate
 * code paths opens the possibility that an attacker will make the status bar
 * show some harmless action where clicking will be harmful.
 */

void html_mouse_action(struct content *c, struct browser_window *bw,
		browser_mouse_state mouse, int x, int y)
{
	html_content *html = (html_content *) c;
	enum { ACTION_NONE, ACTION_SUBMIT, ACTION_GO } action = ACTION_NONE;
	const char *title = 0;
	nsurl *url = 0;
	const char *target = 0;
	char status_buffer[200];
	const char *status = 0;
	gui_pointer_shape pointer = GUI_POINTER_DEFAULT;
	bool imagemap = false;
	int box_x = 0, box_y = 0;
	int gadget_box_x = 0, gadget_box_y = 0;
	int text_box_x = 0;
	struct box *url_box = 0;
	struct box *gadget_box = 0;
	struct box *text_box = 0;
	hlcache_handle *h = bw->current_content;
	struct box *box;
	hlcache_handle *content = h;
	hlcache_handle *gadget_content = h;
	struct form_control *gadget = 0;
	hlcache_handle *object = NULL;
	struct browser_window *iframe = NULL;
	struct box *next_box;
	struct box *drag_candidate = NULL;
	struct scrollbar *scrollbar = NULL;
	plot_font_style_t fstyle;
	int scroll_mouse_x = 0, scroll_mouse_y = 0;
	int padding_left, padding_right, padding_top, padding_bottom;

	if (bw->drag_type != DRAGGING_NONE && !mouse &&
			html->visible_select_menu != NULL) {
		/* drag end: select menu */
		form_select_mouse_drag_end(html->visible_select_menu,
				mouse, x, y);
	}

	if (html->visible_select_menu != NULL) {
		box = html->visible_select_menu->box;
		box_coords(box, &box_x, &box_y);

		box_x -= box->border[LEFT].width;
		box_y += box->height + box->border[BOTTOM].width +
				box->padding[BOTTOM] + box->padding[TOP];
		status = form_select_mouse_action(html->visible_select_menu,
				mouse, x - box_x, y - box_y);
		if (status != NULL)
			browser_window_set_status(bw, status);
		else {
			int width, height;
			form_select_get_dimensions(html->visible_select_menu,
					&width, &height);
			html->visible_select_menu = NULL;
			browser_window_redraw_rect(bw, box_x, box_y,
					width, height);					
		}
		return;
	}

	if (!mouse && html->scrollbar != NULL) {
		/* drag end: scrollbar */
		html_overflow_scroll_drag_end(html->scrollbar, mouse, x, y);
	}

	if (html->scrollbar != NULL) {
		struct html_scrollbar_data *data =
				scrollbar_get_data(html->scrollbar);
		box = data->box;
		box_coords(box, &box_x, &box_y);
		if (scrollbar_is_horizontal(html->scrollbar)) {
			scroll_mouse_x = x - box_x ;
			scroll_mouse_y = y - (box_y + box->padding[TOP] +
					box->height + box->padding[BOTTOM] -
					SCROLLBAR_WIDTH);
			status = scrollbar_mouse_action(html->scrollbar, mouse,
					scroll_mouse_x, scroll_mouse_y);
		} else {
			scroll_mouse_x = x - (box_x + box->padding[LEFT] +
					box->width + box->padding[RIGHT] -
					SCROLLBAR_WIDTH);
			scroll_mouse_y = y - box_y;
			status = scrollbar_mouse_action(html->scrollbar, mouse, 
					scroll_mouse_x, scroll_mouse_y);
		}
		
		browser_window_set_status(bw, status);
		return;
	}

	/* Content related drags handled by now */
	browser_window_set_drag_type(bw, DRAGGING_NONE, NULL);

	/* search the box tree for a link, imagemap, form control, or
	 * box with scrollbars */

	box = html_get_box_tree(h);

	/* Consider the margins of the html page now */
	box_x = box->margin[LEFT];
	box_y = box->margin[TOP];

	while ((next_box = box_at_point(box, x, y, &box_x, &box_y, &content)) !=
			NULL) {
		box = next_box;

		if (box->style && css_computed_visibility(box->style) == 
				CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->object)
			object = box->object;

		if (box->iframe)
			iframe = box->iframe;

		if (box->href) {
			url = box->href;
			target = box->target;
			url_box = box;
		}

		if (box->usemap) {
			url = imagemap_get(html, box->usemap,
					box_x, box_y, x, y, &target);
			if (url) {
				imagemap = true;
				url_box = box;
			}
		}

		if (box->gadget) {
			gadget_content = content;
			gadget = box->gadget;
			gadget_box = box;
			gadget_box_x = box_x;
			gadget_box_y = box_y;
			if (gadget->form)
				target = gadget->form->target;
		}

		if (box->title)
			title = box->title;

		pointer = get_pointer_shape(bw, box, false);

		if ((box->scroll_x != NULL || box->scroll_y != NULL) &&
				   drag_candidate == NULL)
			drag_candidate = box;
		
		if (box->scroll_y != NULL || box->scroll_x != NULL) {
			padding_left = box_x +
					scrollbar_get_offset(box->scroll_x);
			padding_right = padding_left + box->padding[LEFT] +
					box->width + box->padding[RIGHT];
			padding_top = box_y +
					scrollbar_get_offset(box->scroll_y);
			padding_bottom = padding_top + box->padding[TOP] +
					box->height + box->padding[BOTTOM];
			
			if (x > padding_left && x < padding_right &&
					y > padding_top && y < padding_bottom) {
				/* mouse inside padding box */
				
				if (box->scroll_y != NULL && x > padding_right -
						SCROLLBAR_WIDTH) {
					/* mouse above vertical box scroll */
					
					scrollbar = box->scroll_y;
					scroll_mouse_x = x - (padding_right -
							     SCROLLBAR_WIDTH);
					scroll_mouse_y = y - padding_top;
					break;
				
				} else if (box->scroll_x != NULL &&
						y > padding_bottom -
						SCROLLBAR_WIDTH) {
					/* mouse above horizontal box scroll */
							
					scrollbar = box->scroll_x;
					scroll_mouse_x = x - padding_left;
					scroll_mouse_y = y - (padding_bottom -
							SCROLLBAR_WIDTH);
					break;
				}
			}
		}

		if (box->text && !box->object) {
			text_box = box;
			text_box_x = box_x;
		}
	}

	/* use of box_x, box_y, or content below this point is probably a
	 * mistake; they will refer to the last box returned by box_at_point */

	if (scrollbar) {
		status = scrollbar_mouse_action(scrollbar, mouse,
				scroll_mouse_x, scroll_mouse_y);
		pointer = GUI_POINTER_DEFAULT;
	} else if (gadget) {
		switch (gadget->type) {
		case GADGET_SELECT:
			status = messages_get("FormSelect");
			pointer = GUI_POINTER_MENU;
			if (mouse & BROWSER_MOUSE_CLICK_1 &&
					option_core_select_menu) {
				html->visible_select_menu = gadget;
				form_open_select_menu(c, gadget,
						form_select_menu_callback,
						c);
				pointer =  GUI_POINTER_DEFAULT;
			} else if (mouse & BROWSER_MOUSE_CLICK_1)
				gui_create_form_select_menu(bw, gadget);
			break;
		case GADGET_CHECKBOX:
			status = messages_get("FormCheckbox");
			if (mouse & BROWSER_MOUSE_CLICK_1) {
				gadget->selected = !gadget->selected;
				html_redraw_a_box(gadget_content, gadget_box);
			}
			break;
		case GADGET_RADIO:
			status = messages_get("FormRadio");
			if (mouse & BROWSER_MOUSE_CLICK_1)
				form_radio_set(gadget_content, gadget);
			break;
		case GADGET_IMAGE:
			if (mouse & BROWSER_MOUSE_CLICK_1) {
				gadget->data.image.mx = x - gadget_box_x;
				gadget->data.image.my = y - gadget_box_y;
			}
			/* drop through */
		case GADGET_SUBMIT:
			if (gadget->form) {
				snprintf(status_buffer, sizeof status_buffer,
						messages_get("FormSubmit"),
						gadget->form->action);
				status = status_buffer;
				pointer = get_pointer_shape(bw, gadget_box,
						false);
				if (mouse & (BROWSER_MOUSE_CLICK_1 |
						BROWSER_MOUSE_CLICK_2))
					action = ACTION_SUBMIT;
			} else {
				status = messages_get("FormBadSubmit");
			}
			break;
		case GADGET_TEXTAREA:
			status = messages_get("FormTextarea");
			pointer = get_pointer_shape(bw, gadget_box, false);

			if (mouse & (BROWSER_MOUSE_PRESS_1 |
					BROWSER_MOUSE_PRESS_2)) {
				if (text_box && selection_root(&html->sel) !=
						gadget_box)
					selection_init(&html->sel, gadget_box);

				textinput_textarea_click(c, mouse,
						gadget_box,
						gadget_box_x,
						gadget_box_y,
						x - gadget_box_x,
						y - gadget_box_y);
			}

			if (text_box) {
				int pixel_offset;
				size_t idx;

				font_plot_style_from_css(text_box->style, 
						&fstyle);

				nsfont.font_position_in_string(&fstyle,
					text_box->text,
					text_box->length,
					x - gadget_box_x - text_box->x,
					&idx,
					&pixel_offset);

				selection_click(&html->sel, mouse,
						text_box->byte_offset + idx);

				if (selection_dragging(&html->sel)) {
					bw->drag_type = DRAGGING_SELECTION;
					status = messages_get("Selecting");
				} else
					status = content_get_status_message(h);
			}
			else if (mouse & BROWSER_MOUSE_PRESS_1)
				selection_clear(&html->sel, true);
			break;
		case GADGET_TEXTBOX:
		case GADGET_PASSWORD:
			status = messages_get("FormTextbox");
			pointer = get_pointer_shape(bw, gadget_box, false);

			if ((mouse & BROWSER_MOUSE_PRESS_1) &&
					!(mouse & (BROWSER_MOUSE_MOD_1 |
					BROWSER_MOUSE_MOD_2))) {
				textinput_input_click(c,
						gadget_box,
						gadget_box_x,
						gadget_box_y,
						x - gadget_box_x,
						y - gadget_box_y);
			}
			if (text_box) {
				int pixel_offset;
				size_t idx;

				if (mouse & (BROWSER_MOUSE_DRAG_1 |
						BROWSER_MOUSE_DRAG_2))
					selection_init(&html->sel, gadget_box);

				font_plot_style_from_css(text_box->style,
						&fstyle);

				nsfont.font_position_in_string(&fstyle,
					text_box->text,
					text_box->length,
					x - gadget_box_x - text_box->x,
					&idx,
					&pixel_offset);

				selection_click(&html->sel, mouse,
						text_box->byte_offset + idx);

				if (selection_dragging(&html->sel))
					bw->drag_type = DRAGGING_SELECTION;
			}
			else if (mouse & BROWSER_MOUSE_PRESS_1)
				selection_clear(&html->sel, true);
			break;
		case GADGET_HIDDEN:
			/* not possible: no box generated */
			break;
		case GADGET_RESET:
			status = messages_get("FormReset");
			break;
		case GADGET_FILE:
			status = messages_get("FormFile");
			break;
		case GADGET_BUTTON:
			/* This gadget cannot be activated */
			status = messages_get("FormButton");
			break;
		}

	} else if (object && (mouse & BROWSER_MOUSE_MOD_2)) {

		if (mouse & BROWSER_MOUSE_DRAG_2)
			gui_drag_save_object(GUI_SAVE_OBJECT_NATIVE, object,
					bw->window);
		else if (mouse & BROWSER_MOUSE_DRAG_1)
			gui_drag_save_object(GUI_SAVE_OBJECT_ORIG, object,
					bw->window);

		/* \todo should have a drag-saving object msg */
		status = content_get_status_message(h);

	} else if (iframe) {
		int pos_x, pos_y;
		browser_window_get_position(iframe, false, &pos_x, &pos_y);
		pos_x /= bw->scale;
		pos_y /= bw->scale;

		if (mouse & BROWSER_MOUSE_CLICK_1 ||
				mouse & BROWSER_MOUSE_CLICK_2) {
			browser_window_mouse_click(iframe, mouse,
					x - pos_x, y - pos_y);
		} else {
			browser_window_mouse_track(iframe, mouse,
					x - pos_x, y - pos_y);
		}
	} else if (url) {
		if (title) {
			snprintf(status_buffer, sizeof status_buffer, "%s: %s",
					nsurl_access(url), title);
			status = status_buffer;
		} else
			status = nsurl_access(url);

		pointer = get_pointer_shape(bw, url_box, imagemap);

		if (mouse & BROWSER_MOUSE_CLICK_1 &&
				mouse & BROWSER_MOUSE_MOD_1) {
			/* force download of link */
			browser_window_go_post(bw, nsurl_access(url), 0, 0,
					false, nsurl_access(hlcache_handle_get_url(h)),
					true, true, 0);
		} else if (mouse & BROWSER_MOUSE_CLICK_2 &&
				mouse & BROWSER_MOUSE_MOD_1) {
				gui_window_save_link(bw->window,
						nsurl_access(url), title);
		} else if (mouse & (BROWSER_MOUSE_CLICK_1 |
				BROWSER_MOUSE_CLICK_2))
			action = ACTION_GO;

	} else {
		bool done = false;

		/* frame resizing */
		if (bw->parent) {
			struct browser_window *parent;
			for (parent = bw->parent; parent->parent;
					parent = parent->parent);
			browser_window_resize_frames(parent, mouse,
					x + bw->x, y + bw->y,
					&pointer, &status, &done);
		}

		/* if clicking in the main page, remove the selection from any
		 * text areas */
		if (!done) {
			struct box *layout = html_get_box_tree(h);

			if (mouse && (mouse < BROWSER_MOUSE_MOD_1) &&
					selection_root(&html->sel) != layout) {
				selection_init(&html->sel, layout);
			}

			if (text_box) {
				int pixel_offset;
				size_t idx;

				font_plot_style_from_css(text_box->style,
						&fstyle);

				nsfont.font_position_in_string(&fstyle,
					text_box->text,
					text_box->length,
					x - text_box_x,
					&idx,
					&pixel_offset);

				if (selection_click(&html->sel, mouse,
						text_box->byte_offset + idx)) {
					/* key presses must be directed at the
					 * main browser window, paste text
					 * operations ignored */

					if (selection_dragging(&html->sel)) {
						bw->drag_type =
							DRAGGING_SELECTION;
						status =
							messages_get("Selecting");
					} else
						status = content_get_status_message(h);

					done = true;
				}
			}
			else if (mouse & BROWSER_MOUSE_PRESS_1)
				selection_clear(&html->sel, true);
		}

		if (!done) {
			if (title)
				status = title;
			else if (bw->loading_content)
				status = content_get_status_message(
						bw->loading_content);
			else
				status = content_get_status_message(h);

			if (mouse & BROWSER_MOUSE_DRAG_1) {
				if (mouse & BROWSER_MOUSE_MOD_2) {
					gui_drag_save_object(GUI_SAVE_COMPLETE,
							h, bw->window);
				} else {
					if (drag_candidate == NULL)
						browser_window_page_drag_start(
								bw, x, y);
					else {
						html_box_drag_start(
								drag_candidate,
								x, y);
					}
					pointer = GUI_POINTER_MOVE;
				}
			}
			else if (mouse & BROWSER_MOUSE_DRAG_2) {
				if (mouse & BROWSER_MOUSE_MOD_2) {
					gui_drag_save_object(GUI_SAVE_SOURCE,
							h, bw->window);
				} else {
					if (drag_candidate == NULL)
						browser_window_page_drag_start(
								bw, x, y);
					else {
						html_box_drag_start(
								drag_candidate,
								x, y);
					}
					pointer = GUI_POINTER_MOVE;
				}
			}
		}
		if (mouse && mouse < BROWSER_MOUSE_MOD_1) {
			/* ensure key presses still act on the browser window */
			browser_window_remove_caret(bw);
		}
	}


	if (action == ACTION_SUBMIT || action == ACTION_GO)
		bw->last_action = wallclock();

	if (status != NULL)
		browser_window_set_status(bw, status);

	if (!iframe)
		browser_window_set_pointer(bw, pointer);

	/* deferred actions that can cause this browser_window to be destroyed
	 * and must therefore be done after set_status/pointer
	 */
	switch (action) {
	case ACTION_SUBMIT:
		form_submit(bw->current_content,
				browser_window_find_target(bw, target, mouse),
				gadget->form, gadget);
		break;
	case ACTION_GO:
		browser_window_go(browser_window_find_target(bw, target, mouse),
				nsurl_access(url),
				nsurl_access(hlcache_handle_get_url(h)), true);
		break;
	case ACTION_NONE:
		break;
	}
}


gui_pointer_shape get_pointer_shape(struct browser_window *bw, struct box *box,
		bool imagemap)
{
	gui_pointer_shape pointer;
	css_computed_style *style;
	enum css_cursor_e cursor;
	lwc_string **cursor_uris;
	bool loading;

	assert(bw);

	loading = (bw->loading_content != NULL || (bw->current_content &&
			content_get_status(bw->current_content) == 
			CONTENT_STATUS_READY));

	if (wallclock() - bw->last_action < 100 && loading)
		/* If less than 1 second since last link followed, show
		 * progress indicating pointer and we're loading something */
		return GUI_POINTER_PROGRESS;

	if (box->type == BOX_FLOAT_LEFT || box->type == BOX_FLOAT_RIGHT)
		style = box->children->style;
	else
		style = box->style;

	if (style == NULL)
		return GUI_POINTER_DEFAULT;

	cursor = css_computed_cursor(style, &cursor_uris);

	switch (cursor) {
	case CSS_CURSOR_AUTO:
		if (box->href || (box->gadget &&
				(box->gadget->type == GADGET_IMAGE ||
				box->gadget->type == GADGET_SUBMIT)) ||
				imagemap) {
			/* link */
			pointer = GUI_POINTER_POINT;
		} else if (box->gadget &&
				(box->gadget->type == GADGET_TEXTBOX ||
				box->gadget->type == GADGET_PASSWORD ||
				box->gadget->type == GADGET_TEXTAREA)) {
			/* text input */
			pointer = GUI_POINTER_CARET;
		} else {
			/* anything else */
			if (loading) {
				/* loading new content */
				pointer = GUI_POINTER_PROGRESS;
			} else {
				pointer = GUI_POINTER_DEFAULT;
			}
		}
		break;
	case CSS_CURSOR_CROSSHAIR:
		pointer = GUI_POINTER_CROSS;
		break;
	case CSS_CURSOR_POINTER:
		pointer = GUI_POINTER_POINT;
		break;
	case CSS_CURSOR_MOVE:
		pointer = GUI_POINTER_MOVE;
		break;
	case CSS_CURSOR_E_RESIZE:
		pointer = GUI_POINTER_RIGHT;
		break;
	case CSS_CURSOR_W_RESIZE:
		pointer = GUI_POINTER_LEFT;
		break;
	case CSS_CURSOR_N_RESIZE:
		pointer = GUI_POINTER_UP;
		break;
	case CSS_CURSOR_S_RESIZE:
		pointer = GUI_POINTER_DOWN;
		break;
	case CSS_CURSOR_NE_RESIZE:
		pointer = GUI_POINTER_RU;
		break;
	case CSS_CURSOR_SW_RESIZE:
		pointer = GUI_POINTER_LD;
		break;
	case CSS_CURSOR_SE_RESIZE:
		pointer = GUI_POINTER_RD;
		break;
	case CSS_CURSOR_NW_RESIZE:
		pointer = GUI_POINTER_LU;
		break;
	case CSS_CURSOR_TEXT:
		pointer = GUI_POINTER_CARET;
		break;
	case CSS_CURSOR_WAIT:
		pointer = GUI_POINTER_WAIT;
		break;
	case CSS_CURSOR_PROGRESS:
		pointer = GUI_POINTER_PROGRESS;
		break;
	case CSS_CURSOR_HELP:
		pointer = GUI_POINTER_HELP;
		break;
	default:
		pointer = GUI_POINTER_DEFAULT;
		break;
	}

	return pointer;
}


/**
 * Callback for in-page scrollbars.
 */
void html_overflow_scroll_callback(void *client_data,
		struct scrollbar_msg_data *scrollbar_data)
{
	struct html_scrollbar_data *data = client_data;
	html_content *html = (html_content *)data->c;
	struct box *box = data->box;
	int x, y, box_x, box_y, diff_x, diff_y;
	
	switch(scrollbar_data->msg) {
		case SCROLLBAR_MSG_REDRAW:
			diff_x = box->padding[LEFT] + box->width +
					box->padding[RIGHT] - SCROLLBAR_WIDTH;
			diff_y = box->padding[TOP] + box->height +
					box->padding[BOTTOM] - SCROLLBAR_WIDTH;
	
			box_coords(box, &box_x, &box_y);
			if (scrollbar_is_horizontal(
					scrollbar_data->scrollbar)) {
				x = box_x + scrollbar_get_offset(box->scroll_x);
				y = box_y + scrollbar_get_offset(box->scroll_y) +
						diff_y;
			} else {
				x = box_x + scrollbar_get_offset(box->scroll_x) +
						diff_x;
				y = box_y + scrollbar_get_offset(box->scroll_y);
			}
			content__request_redraw((struct content *)html,
					x + scrollbar_data->x0,
					y + scrollbar_data->y0,
     					scrollbar_data->x1 - scrollbar_data->x0,
					scrollbar_data->y1 - scrollbar_data->y0);
			break;
		case SCROLLBAR_MSG_MOVED:
			html_redraw_a_box(html->bw->current_content, box);
			break;
		case SCROLLBAR_MSG_SCROLL_START:
		{
			struct rect rect = {
				.x0 = scrollbar_data->x0,
				.y0 = scrollbar_data->y0,
				.x1 = scrollbar_data->x1,
				.y1 = scrollbar_data->y1
			};
			browser_window_set_drag_type(html->bw,
					DRAGGING_CONTENT_SCROLLBAR, &rect);

			html->scrollbar = scrollbar_data->scrollbar;
		}
			break;
		case SCROLLBAR_MSG_SCROLL_FINISHED:
			html->scrollbar = NULL;

			browser_window_set_drag_type(html->bw,
					DRAGGING_NONE, NULL);
			
			browser_window_set_pointer(html->bw,
					GUI_POINTER_DEFAULT);
			break;
	}
}


/**
 * End overflow scroll scrollbar drags
 *
 * \param  scroll  scrollbar widget
 * \param  mouse   state of mouse buttons and modifier keys
 * \param  x	   coordinate of mouse
 * \param  y	   coordinate of mouse
 */
void html_overflow_scroll_drag_end(struct scrollbar *scrollbar,
		browser_mouse_state mouse, int x, int y)
{
	int scroll_mouse_x, scroll_mouse_y, box_x, box_y;
	struct html_scrollbar_data *data = scrollbar_get_data(scrollbar);
	struct box *box;

	box = data->box;
	box_coords(box, &box_x, &box_y);

	if (scrollbar_is_horizontal(scrollbar)) {
		scroll_mouse_x = x - box_x;
		scroll_mouse_y = y - (box_y + box->padding[TOP] +
				box->height + box->padding[BOTTOM] -
				SCROLLBAR_WIDTH);
		scrollbar_mouse_drag_end(scrollbar, mouse,
				scroll_mouse_x, scroll_mouse_y);
	} else {
		scroll_mouse_x = x - (box_x + box->padding[LEFT] +
				box->width + box->padding[RIGHT] -
				SCROLLBAR_WIDTH);
		scroll_mouse_y = y - box_y;
		scrollbar_mouse_drag_end(scrollbar, mouse,
				scroll_mouse_x, scroll_mouse_y);
	}
}


/**
 * Start drag scrolling the contents of a box
 *
 * \param box	the box to be scrolled
 * \param x	x ordinate of initial mouse position
 * \param y	y ordinate
 */

void html_box_drag_start(struct box *box, int x, int y)
{
	int box_x, box_y;
	int scroll_mouse_x, scroll_mouse_y;
	
	box_coords(box, &box_x, &box_y);
	
	if (box->scroll_x != NULL) {
		scroll_mouse_x = x - box_x ;
		scroll_mouse_y = y - (box_y + box->padding[TOP] +
				box->height + box->padding[BOTTOM] -
				SCROLLBAR_WIDTH);
		scrollbar_start_content_drag(box->scroll_x,
				scroll_mouse_x, scroll_mouse_y);
	} else if (box->scroll_y != NULL) {
		scroll_mouse_x = x - (box_x + box->padding[LEFT] +
				box->width + box->padding[RIGHT] -
				SCROLLBAR_WIDTH);
		scroll_mouse_y = y - box_y;
		
		scrollbar_start_content_drag(box->scroll_y,
				scroll_mouse_x, scroll_mouse_y);
	}
}
