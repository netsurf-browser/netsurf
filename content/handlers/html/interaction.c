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

/**
 * \file
 * implementation of user interaction with a CONTENT_HTML.
 */

#include <assert.h>
#include <stdbool.h>

#include <dom/dom.h>

#include "utils/corestrings.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "utils/nsoption.h"
#include "netsurf/content.h"
#include "netsurf/browser_window.h"
#include "netsurf/mouse.h"
#include "netsurf/misc.h"
#include "netsurf/layout.h"
#include "netsurf/keypress.h"
#include "content/hlcache.h"
#include "desktop/frames.h"
#include "desktop/scrollbar.h"
#include "desktop/selection.h"
#include "desktop/textarea.h"
#include "javascript/js.h"
#include "desktop/gui_internal.h"

#include "html/box.h"
#include "html/box_textarea.h"
#include "html/font.h"
#include "html/form_internal.h"
#include "html/html_internal.h"
#include "html/imagemap.h"
#include "html/search.h"
#include "html/interaction.h"

/**
 * Get pointer shape for given box
 *
 * \param box       box in question
 * \param imagemap  whether an imagemap applies to the box
 */

static browser_pointer_shape get_pointer_shape(struct box *box, bool imagemap)
{
	browser_pointer_shape pointer;
	css_computed_style *style;
	enum css_cursor_e cursor;
	lwc_string **cursor_uris;

	if (box->type == BOX_FLOAT_LEFT || box->type == BOX_FLOAT_RIGHT)
		style = box->children->style;
	else
		style = box->style;

	if (style == NULL)
		return BROWSER_POINTER_DEFAULT;

	cursor = css_computed_cursor(style, &cursor_uris);

	switch (cursor) {
	case CSS_CURSOR_AUTO:
		if (box->href || (box->gadget &&
				(box->gadget->type == GADGET_IMAGE ||
				box->gadget->type == GADGET_SUBMIT)) ||
				imagemap) {
			/* link */
			pointer = BROWSER_POINTER_POINT;
		} else if (box->gadget &&
				(box->gadget->type == GADGET_TEXTBOX ||
				box->gadget->type == GADGET_PASSWORD ||
				box->gadget->type == GADGET_TEXTAREA)) {
			/* text input */
			pointer = BROWSER_POINTER_CARET;
		} else {
			/* html content doesn't mind */
			pointer = BROWSER_POINTER_AUTO;
		}
		break;
	case CSS_CURSOR_CROSSHAIR:
		pointer = BROWSER_POINTER_CROSS;
		break;
	case CSS_CURSOR_POINTER:
		pointer = BROWSER_POINTER_POINT;
		break;
	case CSS_CURSOR_MOVE:
		pointer = BROWSER_POINTER_MOVE;
		break;
	case CSS_CURSOR_E_RESIZE:
		pointer = BROWSER_POINTER_RIGHT;
		break;
	case CSS_CURSOR_W_RESIZE:
		pointer = BROWSER_POINTER_LEFT;
		break;
	case CSS_CURSOR_N_RESIZE:
		pointer = BROWSER_POINTER_UP;
		break;
	case CSS_CURSOR_S_RESIZE:
		pointer = BROWSER_POINTER_DOWN;
		break;
	case CSS_CURSOR_NE_RESIZE:
		pointer = BROWSER_POINTER_RU;
		break;
	case CSS_CURSOR_SW_RESIZE:
		pointer = BROWSER_POINTER_LD;
		break;
	case CSS_CURSOR_SE_RESIZE:
		pointer = BROWSER_POINTER_RD;
		break;
	case CSS_CURSOR_NW_RESIZE:
		pointer = BROWSER_POINTER_LU;
		break;
	case CSS_CURSOR_TEXT:
		pointer = BROWSER_POINTER_CARET;
		break;
	case CSS_CURSOR_WAIT:
		pointer = BROWSER_POINTER_WAIT;
		break;
	case CSS_CURSOR_PROGRESS:
		pointer = BROWSER_POINTER_PROGRESS;
		break;
	case CSS_CURSOR_HELP:
		pointer = BROWSER_POINTER_HELP;
		break;
	default:
		pointer = BROWSER_POINTER_DEFAULT;
		break;
	}

	return pointer;
}


/**
 * Start drag scrolling the contents of a box
 *
 * \param box	the box to be scrolled
 * \param x	x ordinate of initial mouse position
 * \param y	y ordinate
 */

static void html_box_drag_start(struct box *box, int x, int y)
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


/**
 * End overflow scroll scrollbar drags
 *
 * \param html   html content
 * \param mouse  state of mouse buttons and modifier keys
 * \param x	 coordinate of mouse
 * \param y	 coordinate of mouse
 * \param dir    Direction of drag
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

		font_plot_style_from_css(&html->len_ctx, box->style, &fstyle);

		guit->layout->position(&fstyle, box->text, box->length,
				       dx, &idx, &pixel_offset);

		idx += box->byte_offset;
	}

	return idx;
}


/**
 * Helper for file gadgets to store their filename.
 *
 * Stores the filename unencoded on the dom node associated with the
 * gadget.
 *
 * \todo Get rid of this crap eventually
 *
 * \param operation DOM operation
 * \param key DOM node key being considerd
 * \param _data The data assocated with the key
 * \param src The source DOM node.
 * \param dst The destination DOM node.
 */
static void
html__image_coords_dom_user_data_handler(dom_node_operation operation,
					 dom_string *key,
					 void *_data,
					 struct dom_node *src,
					 struct dom_node *dst)
{
	struct image_input_coords *oldcoords, *coords = _data, *newcoords;

	if (!dom_string_isequal(corestring_dom___ns_key_image_coords_node_data,
				key) || coords == NULL) {
		return;
	}

	switch (operation) {
	case DOM_NODE_CLONED:
		newcoords = calloc(1, sizeof(*newcoords));
		if (newcoords != NULL) {
			*newcoords = *coords;
			if (dom_node_set_user_data(dst,
				 corestring_dom___ns_key_image_coords_node_data,
				 newcoords,
				 html__image_coords_dom_user_data_handler,
				 &oldcoords) == DOM_NO_ERR) {
				free(oldcoords);
			}
		}
		break;

	case DOM_NODE_DELETED:
		free(coords);
		break;

	case DOM_NODE_RENAMED:
	case DOM_NODE_IMPORTED:
	case DOM_NODE_ADOPTED:
		break;

	default:
		NSLOG(netsurf, INFO, "User data operation not handled.");
		assert(0);
	}
}


/**
 * End overflow scroll scrollbar drags
 *
 * \param  scrollbar  scrollbar widget
 * \param  mouse   state of mouse buttons and modifier keys
 * \param  x	   coordinate of mouse
 * \param  y	   coordinate of mouse
 */
static void
html_overflow_scroll_drag_end(struct scrollbar *scrollbar,
			      browser_mouse_state mouse,
			      int x, int y)
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
 * handle html mouse action when select menu is open
 *
 */
static nserror
mouse_action_select_menu(html_content *html,
			 struct browser_window *bw,
			 browser_mouse_state mouse,
			 int x, int y)
{
	struct box *box;
	int box_x = 0;
	int box_y = 0;
	const char *status;
	int width, height;
	struct hlcache_handle *bw_content;
	browser_drag_type bw_drag_type;

	assert(html->visible_select_menu != NULL);

	bw_drag_type = browser_window_get_drag_type(bw);
	if (bw_drag_type != DRAGGING_NONE && !mouse) {
		/* drag end: select menu */
		form_select_mouse_drag_end(html->visible_select_menu, mouse, x, y);
	}

	box = html->visible_select_menu->box;
	box_coords(box, &box_x, &box_y);

	box_x -= box->border[LEFT].width;
	box_y += box->height + box->border[BOTTOM].width +
		box->padding[BOTTOM] + box->padding[TOP];

	status = form_select_mouse_action(html->visible_select_menu,
					  mouse,
					  x - box_x,
					  y - box_y);
	if (status != NULL) {
		/* set status if menu still open */
		union content_msg_data msg_data;
		msg_data.explicit_status_text = status;
		content_broadcast((struct content *)html,
				  CONTENT_MSG_STATUS,
				  &msg_data);
		return NSERROR_OK;
	}

	/* close menu and redraw where it was */
	form_select_get_dimensions(html->visible_select_menu, &width, &height);

	html->visible_select_menu = NULL;

	bw_content = browser_window_get_content(bw);
	content_request_redraw(bw_content,
			       box_x,
			       box_y,
			       width,
			       height);
	return NSERROR_OK;
}


/**
 * handle html mouse action when a selection drag is being performed
 *
 */
static nserror
mouse_action_drag_selection(html_content *html,
			    struct browser_window *bw,
			    browser_mouse_state mouse,
			    int x, int y)
{
	struct box *box;
	int dir = -1;
	int dx, dy;
	size_t idx;
	union html_drag_owner drag_owner;
	int pixel_offset;
	plot_font_style_t fstyle;

	if (!mouse) {
		/* End of selection drag */
		if (selection_dragging_start(&html->sel)) {
			dir = 1;
		}

		idx = html_selection_drag_end(html, mouse, x, y, dir);

		if (idx != 0) {
			selection_track(&html->sel, mouse, idx);
		}

		drag_owner.no_owner = true;
		html_set_drag_type(html, HTML_DRAG_NONE, drag_owner, NULL);

		return NSERROR_OK;
	}

	if (selection_dragging_start(&html->sel)) {
		dir = 1;
	}

	box = box_pick_text_box(html, x, y, dir, &dx, &dy);
	if (box != NULL) {
		font_plot_style_from_css(&html->len_ctx, box->style, &fstyle);

		guit->layout->position(&fstyle,
				       box->text,
				       box->length,
				       dx,
				       &idx,
				       &pixel_offset);

		selection_track(&html->sel, mouse, box->byte_offset + idx);
	}
	return NSERROR_OK;
}


/**
 * handle html mouse action when a scrollbar drag is being performed
 *
 */
static nserror
mouse_action_drag_scrollbar(html_content *html,
			    struct browser_window *bw,
			    browser_mouse_state mouse,
			    int x, int y)
{
	struct scrollbar *scr;
	struct html_scrollbar_data *data;
	struct box *box;
	int box_x = 0;
	int box_y = 0;
	const char *status;
	int scroll_mouse_x = 0, scroll_mouse_y = 0;
	scrollbar_mouse_status scrollbar_status;

	scr = html->drag_owner.scrollbar;

	if (!mouse) {
		/* drag end: scrollbar */
		html_overflow_scroll_drag_end(scr, mouse, x, y);
	}

	data = scrollbar_get_data(scr);

	box = data->box;

	box_coords(box, &box_x, &box_y);

	if (scrollbar_is_horizontal(scr)) {
		scroll_mouse_x = x - box_x ;
		scroll_mouse_y = y - (box_y + box->padding[TOP] +
				      box->height + box->padding[BOTTOM] -
				      SCROLLBAR_WIDTH);
		scrollbar_status = scrollbar_mouse_action(scr,
							  mouse,
							  scroll_mouse_x,
							  scroll_mouse_y);
	} else {
		scroll_mouse_x = x - (box_x + box->padding[LEFT] +
				      box->width + box->padding[RIGHT] -
				      SCROLLBAR_WIDTH);
		scroll_mouse_y = y - box_y;

		scrollbar_status = scrollbar_mouse_action(scr,
							  mouse,
							  scroll_mouse_x,
							  scroll_mouse_y);
	}
	status = scrollbar_mouse_status_to_message(scrollbar_status);

	if (status != NULL) {
		union content_msg_data msg_data;

		msg_data.explicit_status_text = status;
		content_broadcast((struct content *)html,
				  CONTENT_MSG_STATUS,
				  &msg_data);
	}

	return NSERROR_OK;
}


/**
 * handle mouse actions while dragging in a text area
 */
static nserror
mouse_action_drag_textarea(html_content *html,
			    struct browser_window *bw,
			    browser_mouse_state mouse,
			    int x, int y)
{
	struct box *box;
	int box_x = 0;
	int box_y = 0;

	box = html->drag_owner.textarea;

	assert(box->gadget != NULL);
	assert(box->gadget->type == GADGET_TEXTAREA ||
	       box->gadget->type == GADGET_PASSWORD ||
	       box->gadget->type == GADGET_TEXTBOX);

	box_coords(box, &box_x, &box_y);
	textarea_mouse_action(box->gadget->data.text.ta,
			      mouse,
			      x - box_x,
			      y - box_y);

	/* TODO: Set appropriate statusbar message */
	return NSERROR_OK;
}


/**
 * handle mouse actions while dragging in a content
 */
static nserror
mouse_action_drag_content(html_content *html,
			  struct browser_window *bw,
			  browser_mouse_state mouse,
			  int x, int y)
{
	struct box *box;
	int box_x = 0;
	int box_y = 0;

	box = html->drag_owner.content;
	assert(box->object != NULL);

	box_coords(box, &box_x, &box_y);
	content_mouse_track(box->object,
			    bw, mouse,
			    x - box_x,
			    y - box_y);
	return NSERROR_OK;
}


/* exported interface documented in html/interaction.h */
nserror html_mouse_track(struct content *c,
			 struct browser_window *bw,
			 browser_mouse_state mouse,
			 int x, int y)
{
	return html_mouse_action(c, bw, mouse, x, y);
}


/* exported interface documented in html/interaction.h */
nserror
html_mouse_action(struct content *c,
		  struct browser_window *bw,
		  browser_mouse_state mouse,
		  int x, int y)
{
	html_content *html = (html_content *) c;
	enum { ACTION_NONE, ACTION_SUBMIT, ACTION_GO } action = ACTION_NONE;
	const char *title = 0;
	nsurl *url = 0;
	char *url_s = NULL;
	size_t url_l = 0;
	const char *target = 0;
	char status_buffer[200];
	const char *status = 0;
	browser_pointer_shape pointer = BROWSER_POINTER_DEFAULT;
	bool imagemap = false;
	int box_x = 0, box_y = 0;
	int gadget_box_x = 0, gadget_box_y = 0;
	int html_object_pos_x = 0, html_object_pos_y = 0;
	int text_box_x = 0;
	struct box *url_box = 0;
	struct box *gadget_box = 0;
	struct box *text_box = 0;
	struct box *box;
	struct form_control *gadget = 0;
	hlcache_handle *object = NULL;
	struct box *html_object_box = NULL;
	struct browser_window *iframe = NULL;
	struct box *drag_candidate = NULL;
	struct scrollbar *scrollbar = NULL;
	plot_font_style_t fstyle;
	int scroll_mouse_x = 0, scroll_mouse_y = 0;
	int padding_left, padding_right, padding_top, padding_bottom;
	union content_msg_data msg_data;
	struct dom_node *node = html->layout->node; /* Default to the <HTML> */
	union html_selection_owner sel_owner;
	bool click = mouse & (BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_PRESS_2 |
			BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_CLICK_2 |
			BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_2);

	nserror res = NSERROR_OK;

	/* handle open select menu */
	if (html->visible_select_menu != NULL) {
		return mouse_action_select_menu(html, bw, mouse, x, y);
	}

	/* handle content drag */
	switch (html->drag_type) {
	case HTML_DRAG_SELECTION:
		return mouse_action_drag_selection(html, bw, mouse, x, y);

	case HTML_DRAG_SCROLLBAR:
		return mouse_action_drag_scrollbar(html, bw, mouse, x, y);

	case HTML_DRAG_TEXTAREA_SELECTION:
	case HTML_DRAG_TEXTAREA_SCROLLBAR:
		return mouse_action_drag_textarea(html, bw, mouse, x, y);

	case HTML_DRAG_CONTENT_SELECTION:
	case HTML_DRAG_CONTENT_SCROLL:
		return mouse_action_drag_content(html, bw, mouse, x, y);

	case HTML_DRAG_NONE:
		break;

	default:
		/* Unknown content related drag type */
		assert(0 && "Unknown content related drag type");
	}

	/* search the box tree for a link, imagemap, form control, or
	 * box with scrollbars
	 */

	box = html->layout;

	/* Consider the margins of the html page now */
	box_x = box->margin[LEFT];
	box_y = box->margin[TOP];

	/* descend through visible boxes setting more specific values for:
	 * box - deepest box at point
	 * html_object_box - html object
	 * html_object_pos_x - html object
	 * html_object_pos_y - html object
	 * object - non html object
	 * iframe - iframe
	 * url - href or imagemap
	 * target - href or imagemap or gadget
	 * url_box - href or imagemap
	 * imagemap - imagemap
	 * gadget - gadget
	 * gadget_box - gadget
	 * gadget_box_x - gadget
	 * gadget_box_y - gadget
	 * title - title
	 * pointer
	 *
	 * drag_candidate - first box with scroll
	 * padding_left - box with scroll
	 * padding_right
	 * padding_top
	 * padding_bottom
	 * scrollbar - inside padding box stops decent
	 * scroll_mouse_x - inside padding box stops decent
	 * scroll_mouse_y - inside padding box stops decent
	 *
	 * text_box - text box
	 * text_box_x - text_box
	 */
	do {
		if ((box->style != NULL) &&
		    (css_computed_visibility(box->style) ==
		     CSS_VISIBILITY_HIDDEN)) {
			continue;
		}

		if (box->node != NULL) {
			node = box->node;
		}

		if (box->object) {
			if (content_get_type(box->object) == CONTENT_HTML) {
				html_object_box = box;
				html_object_pos_x = box_x;
				html_object_pos_y = box_y;
			} else {
				object = box->object;
			}
		}

		if (box->iframe) {
			iframe = box->iframe;
		}

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
			gadget = box->gadget;
			gadget_box = box;
			gadget_box_x = box_x;
			gadget_box_y = box_y;
			if (gadget->form)
				target = gadget->form->target;
		}

		if (box->title) {
			title = box->title;
		}

		pointer = get_pointer_shape(box, false);

		if ((box->scroll_x != NULL) ||
		    (box->scroll_y != NULL)) {

			if (drag_candidate == NULL) {
				drag_candidate = box;
			}

			padding_left = box_x +
					scrollbar_get_offset(box->scroll_x);
			padding_right = padding_left + box->padding[LEFT] +
					box->width + box->padding[RIGHT];
			padding_top = box_y +
					scrollbar_get_offset(box->scroll_y);
			padding_bottom = padding_top + box->padding[TOP] +
					box->height + box->padding[BOTTOM];

			if ((x > padding_left) &&
			    (x < padding_right) &&
			    (y > padding_top) &&
			    (y < padding_bottom)) {
				/* mouse inside padding box */

				if ((box->scroll_y != NULL) &&
						(x > (padding_right -
							SCROLLBAR_WIDTH))) {
					/* mouse above vertical box scroll */

					scrollbar = box->scroll_y;
					scroll_mouse_x = x - (padding_right -
							     SCROLLBAR_WIDTH);
					scroll_mouse_y = y - padding_top;
					break;

				} else if ((box->scroll_x != NULL) &&
						(y > (padding_bottom -
							SCROLLBAR_WIDTH))) {
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
	} while ((box = box_at_point(&html->len_ctx, box, x, y,
			&box_x, &box_y)) != NULL);

	/* use of box_x, box_y, or content below this point is probably a
	 * mistake; they will refer to the last box returned by box_at_point */
	assert(node != NULL);

	if (scrollbar) {
		status = scrollbar_mouse_status_to_message(
				scrollbar_mouse_action(scrollbar, mouse,
						scroll_mouse_x,
						scroll_mouse_y));
		pointer = BROWSER_POINTER_DEFAULT;
	} else if (gadget) {
		textarea_mouse_status ta_status;

		switch (gadget->type) {
		case GADGET_SELECT:
			status = messages_get("FormSelect");
			pointer = BROWSER_POINTER_MENU;
			if (mouse & BROWSER_MOUSE_CLICK_1 &&
			    nsoption_bool(core_select_menu)) {
				html->visible_select_menu = gadget;
				res = form_open_select_menu(c, gadget,
						form_select_menu_callback,
						c);
				if (res != NSERROR_OK) {
					NSLOG(netsurf, ERROR,
					      "%s",
					      messages_get_errorcode(res));
					html->visible_select_menu = NULL;
				}
				pointer = BROWSER_POINTER_DEFAULT;
			} else if (mouse & BROWSER_MOUSE_CLICK_1) {
				msg_data.select_menu.gadget = gadget;
				content_broadcast(c, CONTENT_MSG_SELECTMENU,
						&msg_data);
			}
			break;
		case GADGET_CHECKBOX:
			status = messages_get("FormCheckbox");
			if (mouse & BROWSER_MOUSE_CLICK_1) {
				gadget->selected = !gadget->selected;
				dom_html_input_element_set_checked(
					(dom_html_input_element *)(gadget->node),
					gadget->selected);
				html__redraw_a_box(html, gadget_box);
			}
			break;
		case GADGET_RADIO:
			status = messages_get("FormRadio");
			if (mouse & BROWSER_MOUSE_CLICK_1)
				form_radio_set(gadget);
			break;
		case GADGET_IMAGE:
			/* This falls through to SUBMIT */
			if (mouse & BROWSER_MOUSE_CLICK_1) {
				struct image_input_coords *coords, *oldcoords;
				/** \todo Find a way to not ignore errors */
				coords = calloc(1, sizeof(*coords));
				if (coords == NULL) {
					return NSERROR_OK;
				}
				coords->x = x - gadget_box_x;
				coords->y = y - gadget_box_y;
				if (dom_node_set_user_data(
					    gadget->node,
					    corestring_dom___ns_key_image_coords_node_data,
					    coords, html__image_coords_dom_user_data_handler,
					    &oldcoords) != DOM_NO_ERR)
					return NSERROR_OK;
				free(oldcoords);
			}
			/* Fall through */
		case GADGET_SUBMIT:
			if (gadget->form) {
				snprintf(status_buffer, sizeof status_buffer,
						messages_get("FormSubmit"),
						gadget->form->action);
				status = status_buffer;
				pointer = get_pointer_shape(gadget_box, false);
				if (mouse & (BROWSER_MOUSE_CLICK_1 |
						BROWSER_MOUSE_CLICK_2))
					action = ACTION_SUBMIT;
			} else {
				status = messages_get("FormBadSubmit");
			}
			break;
		case GADGET_TEXTBOX:
		case GADGET_PASSWORD:
		case GADGET_TEXTAREA:
			if (gadget->type == GADGET_TEXTAREA)
				status = messages_get("FormTextarea");
			else
				status = messages_get("FormTextbox");

			if (click && (html->selection_type !=
					HTML_SELECTION_TEXTAREA ||
					html->selection_owner.textarea !=
					gadget_box)) {
				sel_owner.none = true;
				html_set_selection(html, HTML_SELECTION_NONE,
						sel_owner, true);
			}

			ta_status = textarea_mouse_action(gadget->data.text.ta,
					mouse, x - gadget_box_x,
					y - gadget_box_y);

			if (ta_status & TEXTAREA_MOUSE_EDITOR) {
				pointer = get_pointer_shape(gadget_box, false);
			} else {
				pointer = BROWSER_POINTER_DEFAULT;
				status = scrollbar_mouse_status_to_message(
						ta_status >> 3);
			}
			break;
		case GADGET_HIDDEN:
			/* not possible: no box generated */
			break;
		case GADGET_RESET:
			status = messages_get("FormReset");
			break;
		case GADGET_FILE:
			status = messages_get("FormFile");
			if (mouse & BROWSER_MOUSE_CLICK_1) {
				msg_data.gadget_click.gadget = gadget;
				content_broadcast(c, CONTENT_MSG_GADGETCLICK,
						&msg_data);
			}
			break;
		case GADGET_BUTTON:
			/* This gadget cannot be activated */
			status = messages_get("FormButton");
			break;
		}

	} else if (object && (mouse & BROWSER_MOUSE_MOD_2)) {

		if (mouse & BROWSER_MOUSE_DRAG_2) {
			msg_data.dragsave.type = CONTENT_SAVE_NATIVE;
			msg_data.dragsave.content = object;
			content_broadcast(c, CONTENT_MSG_DRAGSAVE, &msg_data);

		} else if (mouse & BROWSER_MOUSE_DRAG_1) {
			msg_data.dragsave.type = CONTENT_SAVE_ORIG;
			msg_data.dragsave.content = object;
			content_broadcast(c, CONTENT_MSG_DRAGSAVE, &msg_data);
		}

		/* \todo should have a drag-saving object msg */

	} else if (iframe) {
		int pos_x, pos_y;
		float scale = browser_window_get_scale(bw);

		browser_window_get_position(iframe, false, &pos_x, &pos_y);

		if (mouse & BROWSER_MOUSE_CLICK_1 ||
		    mouse & BROWSER_MOUSE_CLICK_2) {
			browser_window_mouse_click(iframe, mouse,
						   (x * scale) - pos_x,
						   (y * scale) - pos_y);
		} else {
			browser_window_mouse_track(iframe, mouse,
						   (x * scale) - pos_x,
						   (y * scale) - pos_y);
		}
	} else if (html_object_box) {

		if (click && (html->selection_type != HTML_SELECTION_CONTENT ||
				html->selection_owner.content !=
						html_object_box)) {
			sel_owner.none = true;
			html_set_selection(html, HTML_SELECTION_NONE,
					sel_owner, true);
		}
		if (mouse & BROWSER_MOUSE_CLICK_1 ||
				mouse & BROWSER_MOUSE_CLICK_2) {
			content_mouse_action(html_object_box->object,
					bw, mouse,
					x - html_object_pos_x,
					y - html_object_pos_y);
		} else {
			content_mouse_track(html_object_box->object,
					bw, mouse,
					x - html_object_pos_x,
					y - html_object_pos_y);
		}
	} else if (url) {
		if (nsoption_bool(display_decoded_idn) == true) {
			res = nsurl_get_utf8(url, &url_s, &url_l);
			if (res != NSERROR_OK) {
				/* Unable to obtain a decoded IDN. This is not
				 *  a fatal error.  Ensure the string pointer
				 *  is NULL so we use the encoded version.
				 */
				url_s = NULL;
			}
		}

		if (title) {
			snprintf(status_buffer, sizeof status_buffer, "%s: %s",
					url_s ? url_s : nsurl_access(url), title);
		} else {
			snprintf(status_buffer, sizeof status_buffer, "%s",
					url_s ? url_s : nsurl_access(url));
		}

		status = status_buffer;

		if (url_s != NULL)
			free(url_s);

		pointer = get_pointer_shape(url_box, imagemap);

		if (mouse & BROWSER_MOUSE_CLICK_1 &&
				mouse & BROWSER_MOUSE_MOD_1) {
			/* force download of link */
			browser_window_navigate(bw,
				url,
				content_get_url(c),
				BW_NAVIGATE_DOWNLOAD,
				NULL,
				NULL,
				NULL);

		} else if (mouse & BROWSER_MOUSE_CLICK_2 &&
				mouse & BROWSER_MOUSE_MOD_1) {
			msg_data.savelink.url = url;
			msg_data.savelink.title = title;
			content_broadcast(c, CONTENT_MSG_SAVELINK, &msg_data);

		} else if (mouse & (BROWSER_MOUSE_CLICK_1 |
				BROWSER_MOUSE_CLICK_2))
			action = ACTION_GO;
	} else {
		bool done = false;

		/* frame resizing */
		if (browser_window_frame_resize_start(bw, mouse, x, y,
				&pointer)) {
			if (mouse & (BROWSER_MOUSE_DRAG_1 |
					BROWSER_MOUSE_DRAG_2)) {
				status = messages_get("FrameDrag");
			}
			done = true;
		}

		/* if clicking in the main page, remove the selection from any
		 * text areas */
		if (!done) {

			if (click && html->focus_type != HTML_FOCUS_SELF) {
				union html_focus_owner fo;
				fo.self = true;
				html_set_focus(html, HTML_FOCUS_SELF, fo,
						true, 0, 0, 0, NULL);
			}
			if (click && html->selection_type !=
					HTML_SELECTION_SELF) {
				sel_owner.none = true;
				html_set_selection(html, HTML_SELECTION_NONE,
						sel_owner, true);
			}

			if (text_box) {
				int pixel_offset;
				size_t idx;

				font_plot_style_from_css(&html->len_ctx,
						text_box->style, &fstyle);

				guit->layout->position(&fstyle,
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
					html_drag_type drag_type;
					union html_drag_owner drag_owner;

					if (selection_dragging(&html->sel)) {
						drag_type = HTML_DRAG_SELECTION;
						drag_owner.no_owner = true;
						html_set_drag_type(html,
								drag_type,
								drag_owner,
								NULL);
						status = messages_get(
								"Selecting");
					}

					done = true;
				}

			} else if (mouse & BROWSER_MOUSE_PRESS_1) {
				sel_owner.none = true;
				selection_clear(&html->sel, true);
			}

			if (selection_defined(&html->sel)) {
				sel_owner.none = false;
				html_set_selection(html, HTML_SELECTION_SELF,
						sel_owner, true);
			} else if (click && html->selection_type !=
					HTML_SELECTION_NONE) {
				sel_owner.none = true;
				html_set_selection(html, HTML_SELECTION_NONE,
						sel_owner, true);
			}
		}

		if (!done) {
			if (title)
				status = title;

			if (mouse & BROWSER_MOUSE_DRAG_1) {
				if (mouse & BROWSER_MOUSE_MOD_2) {
					msg_data.dragsave.type =
							CONTENT_SAVE_COMPLETE;
					msg_data.dragsave.content = NULL;
					content_broadcast(c,
							CONTENT_MSG_DRAGSAVE,
							&msg_data);
				} else {
					if (drag_candidate == NULL) {
						browser_window_page_drag_start(
								bw, x, y);
					} else {
						html_box_drag_start(
								drag_candidate,
								x, y);
					}
					pointer = BROWSER_POINTER_MOVE;
				}
			}
			else if (mouse & BROWSER_MOUSE_DRAG_2) {
				if (mouse & BROWSER_MOUSE_MOD_2) {
					msg_data.dragsave.type =
							CONTENT_SAVE_SOURCE;
					msg_data.dragsave.content = NULL;
					content_broadcast(c,
							CONTENT_MSG_DRAGSAVE,
							&msg_data);
				} else {
					if (drag_candidate == NULL) {
						browser_window_page_drag_start(
								bw, x, y);
					} else {
						html_box_drag_start(
								drag_candidate,
								x, y);
					}
					pointer = BROWSER_POINTER_MOVE;
				}
			}
		}
		if (mouse && mouse < BROWSER_MOUSE_MOD_1) {
			/* ensure key presses still act on the browser window */
			union html_focus_owner fo;
			fo.self = true;
			html_set_focus(html, HTML_FOCUS_SELF, fo,
					true, 0, 0, 0, NULL);
		}
	}

	if (!iframe && !html_object_box) {
		msg_data.explicit_status_text = status;
		content_broadcast(c, CONTENT_MSG_STATUS, &msg_data);

		msg_data.pointer = pointer;
		content_broadcast(c, CONTENT_MSG_POINTER, &msg_data);
	}

	/* fire dom click event */
	if (mouse & BROWSER_MOUSE_CLICK_1) {
		fire_generic_dom_event(corestring_dom_click, node, true, true);
	}

	/* deferred actions that can cause this browser_window to be destroyed
	 * and must therefore be done after set_status/pointer
	 */
	switch (action) {
	case ACTION_SUBMIT:
		res = form_submit(content_get_url(c),
				  browser_window_find_target(bw, target, mouse),
				  gadget->form,
				  gadget);
		break;

	case ACTION_GO:
		res = browser_window_navigate(
				browser_window_find_target(bw, target, mouse),
				url,
				content_get_url(c),
				BW_NAVIGATE_HISTORY,
				NULL,
				NULL,
				NULL);
		break;

	case ACTION_NONE:
		res = NSERROR_OK;
		break;
	}

	if (res != NSERROR_OK) {
		NSLOG(netsurf, ERROR, "%s", messages_get_errorcode(res));
	}

	return res;
}


/**
 * Handle keypresses.
 *
 * \param  c	content of type HTML
 * \param  key	The UCS4 character codepoint
 * \return true if key handled, false otherwise
 */

bool html_keypress(struct content *c, uint32_t key)
{
	html_content *html = (html_content *) c;
	struct selection *sel = &html->sel;

	/** \todo
	 * At the moment, the front end interface for keypress only gives
	 * us a UCS4 key value.  This doesn't doesn't have all the information
	 * we need to fill out the event properly.  We don't get to know about
	 * modifier keys, and things like CTRL+C are passed in as
	 * \ref NS_KEY_COPY_SELECTION, a magic value outside the valid Unicode
	 * range.
	 *
	 * We need to:
	 *
	 * 1. Update the front end interface so that both press and release
	 *    events reach the core.
	 * 2. Stop encoding the special keys like \ref NS_KEY_COPY_SELECTION as
	 *    magic values in the front ends, so we just get the events, e.g.:
	 *    1. Press ctrl
	 *    2. Press c
	 *    3. Release c
	 *    4. Release ctrl
	 * 3. Pass all the new info to the DOM KeyboardEvent events.
	 * 4. If there is a focused element, fire the event at that, instead of
	 *    `html->layout->node`.
	 * 5. Rebuild the \ref NS_KEY_COPY_SELECTION values from the info we
	 *    now get given, and use that for the code below this
	 *    \ref fire_dom_keyboard_event call.
	 * 6. Move the code after this \ref fire_dom_keyboard_event call into
	 *    the default action handler for DOM events.
	 *
	 * This will mean that if the JavaScript event listener does
	 * `event.preventDefault()` then we won't handle the event when
	 * we're not supposed to.
	 */
	if (html->layout != NULL && html->layout->node != NULL) {
		fire_dom_keyboard_event(corestring_dom_keydown,
				html->layout->node, true, true, key);
	}

	switch (html->focus_type) {
	case HTML_FOCUS_CONTENT:
		return content_keypress(html->focus_owner.content->object, key);

	case HTML_FOCUS_TEXTAREA:
		if (box_textarea_keypress(html, html->focus_owner.textarea, key) == NSERROR_OK) {
			return true;
		} else {
			return false;
		}

	default:
		/* Deal with it below */
		break;
	}

	switch (key) {
	case NS_KEY_COPY_SELECTION:
		selection_copy_to_clipboard(sel);
		return true;

	case NS_KEY_CLEAR_SELECTION:
		selection_clear(sel, true);
		return true;

	case NS_KEY_SELECT_ALL:
		selection_select_all(sel);
		return true;

	case NS_KEY_ESCAPE:
		if (selection_defined(sel)) {
			selection_clear(sel, true);
			return true;
		}

		/* if there's no selection, leave Escape for the caller */
		return false;
	}

	return false;
}


/**
 * Handle search.
 *
 * \param c content of type HTML
 * \param context front end private data
 * \param flags search flags
 * \param string search string
 */
void
html_search(struct content *c,
	    void *context,
	    search_flags_t flags,
	    const char *string)
{
	html_content *html = (html_content *)c;

	assert(c != NULL);

	if ((string != NULL) &&
	    (html->search_string != NULL) &&
	    (strcmp(string, html->search_string) == 0) &&
	    (html->search != NULL)) {
		/* Continue prev. search */
		search_step(html->search, flags, string);

	} else if (string != NULL) {
		/* New search */
		free(html->search_string);
		html->search_string = strdup(string);
		if (html->search_string == NULL)
			return;

		if (html->search != NULL) {
			search_destroy_context(html->search);
			html->search = NULL;
		}

		html->search = search_create_context(c, CONTENT_HTML, context);

		if (html->search == NULL)
			return;

		search_step(html->search, flags, string);

	} else {
		/* Clear search */
		html_search_clear(c);

		free(html->search_string);
		html->search_string = NULL;
	}
}


/**
 * Terminate a search.
 *
 * \param  c			content of type HTML
 */
void html_search_clear(struct content *c)
{
	html_content *html = (html_content *)c;

	assert(c != NULL);

	free(html->search_string);
	html->search_string = NULL;

	if (html->search != NULL) {
		search_destroy_context(html->search);
	}
	html->search = NULL;
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
	union content_msg_data msg_data;
	html_drag_type drag_type;
	union html_drag_owner drag_owner;

	switch(scrollbar_data->msg) {
	case SCROLLBAR_MSG_MOVED:

		if (html->reflowing == true) {
			/* Can't redraw during layout, and it will
			 * be redrawn after layout anyway. */
			break;
		}

		html__redraw_a_box(html, box);
		break;
	case SCROLLBAR_MSG_SCROLL_START:
	{
		struct rect rect = {
			.x0 = scrollbar_data->x0,
			.y0 = scrollbar_data->y0,
			.x1 = scrollbar_data->x1,
			.y1 = scrollbar_data->y1
		};
		drag_type = HTML_DRAG_SCROLLBAR;
		drag_owner.scrollbar = scrollbar_data->scrollbar;
		html_set_drag_type(html, drag_type, drag_owner, &rect);
	}
		break;
	case SCROLLBAR_MSG_SCROLL_FINISHED:
		drag_type = HTML_DRAG_NONE;
		drag_owner.no_owner = true;
		html_set_drag_type(html, drag_type, drag_owner, NULL);

		msg_data.pointer = BROWSER_POINTER_AUTO;
		content_broadcast(data->c, CONTENT_MSG_POINTER, &msg_data);
		break;
	}
}


/* Documented in html_internal.h */
void html_set_drag_type(html_content *html, html_drag_type drag_type,
		union html_drag_owner drag_owner, const struct rect *rect)
{
	union content_msg_data msg_data;

	assert(html != NULL);

	html->drag_type = drag_type;
	html->drag_owner = drag_owner;

	switch (drag_type) {
	case HTML_DRAG_NONE:
		assert(drag_owner.no_owner == true);
		msg_data.drag.type = CONTENT_DRAG_NONE;
		break;

	case HTML_DRAG_SCROLLBAR:
	case HTML_DRAG_TEXTAREA_SCROLLBAR:
	case HTML_DRAG_CONTENT_SCROLL:
		msg_data.drag.type = CONTENT_DRAG_SCROLL;
		break;

	case HTML_DRAG_SELECTION:
		assert(drag_owner.no_owner == true);
		/* Fall through */
	case HTML_DRAG_TEXTAREA_SELECTION:
	case HTML_DRAG_CONTENT_SELECTION:
		msg_data.drag.type = CONTENT_DRAG_SELECTION;
		break;
	}
	msg_data.drag.rect = rect;

	/* Inform of the content's drag status change */
	content_broadcast((struct content *)html, CONTENT_MSG_DRAG, &msg_data);
}

/* Documented in html_internal.h */
void html_set_focus(html_content *html, html_focus_type focus_type,
		union html_focus_owner focus_owner, bool hide_caret,
		int x, int y, int height, const struct rect *clip)
{
	union content_msg_data msg_data;
	int x_off = 0;
	int y_off = 0;
	struct rect cr;
	bool textarea_lost_focus = html->focus_type == HTML_FOCUS_TEXTAREA &&
			focus_type != HTML_FOCUS_TEXTAREA;

	assert(html != NULL);

	switch (focus_type) {
	case HTML_FOCUS_SELF:
		assert(focus_owner.self == true);
		if (html->focus_type == HTML_FOCUS_SELF)
			/* Don't need to tell anyone anything */
			return;
		break;

	case HTML_FOCUS_CONTENT:
		box_coords(focus_owner.content, &x_off, &y_off);
		break;

	case HTML_FOCUS_TEXTAREA:
		box_coords(focus_owner.textarea, &x_off, &y_off);
		break;
	}

	html->focus_type = focus_type;
	html->focus_owner = focus_owner;

	if (textarea_lost_focus) {
		msg_data.caret.type = CONTENT_CARET_REMOVE;
	} else if (focus_type != HTML_FOCUS_SELF && hide_caret) {
		msg_data.caret.type = CONTENT_CARET_HIDE;
	} else {
		if (clip != NULL) {
			cr = *clip;
			cr.x0 += x_off;
			cr.y0 += y_off;
			cr.x1 += x_off;
			cr.y1 += y_off;
		}

		msg_data.caret.type = CONTENT_CARET_SET_POS;
		msg_data.caret.pos.x = x + x_off;
		msg_data.caret.pos.y = y + y_off;
		msg_data.caret.pos.height = height;
		msg_data.caret.pos.clip = (clip == NULL) ? NULL : &cr;
	}

	/* Inform of the content's drag status change */
	content_broadcast((struct content *)html, CONTENT_MSG_CARET, &msg_data);
}

/* Documented in html_internal.h */
void html_set_selection(html_content *html, html_selection_type selection_type,
		union html_selection_owner selection_owner, bool read_only)
{
	union content_msg_data msg_data;
	struct box *box;
	bool changed = false;
	bool same_type = html->selection_type == selection_type;

	assert(html != NULL);

	if ((selection_type == HTML_SELECTION_NONE &&
			html->selection_type != HTML_SELECTION_NONE) ||
			(selection_type != HTML_SELECTION_NONE &&
			html->selection_type == HTML_SELECTION_NONE))
		/* Existance of selection has changed, and we'll need to
		 * inform our owner */
		changed = true;

	/* Clear any existing selection */
	if (html->selection_type != HTML_SELECTION_NONE) {
		switch (html->selection_type) {
		case HTML_SELECTION_SELF:
			if (same_type)
				break;
			selection_clear(&html->sel, true);
			break;
		case HTML_SELECTION_TEXTAREA:
			if (same_type && html->selection_owner.textarea ==
					selection_owner.textarea)
				break;
			box = html->selection_owner.textarea;
			textarea_clear_selection(box->gadget->data.text.ta);
			break;
		case HTML_SELECTION_CONTENT:
			if (same_type && html->selection_owner.content ==
					selection_owner.content)
				break;
			box = html->selection_owner.content;
			content_clear_selection(box->object);
			break;
		default:
			break;
		}
	}

	html->selection_type = selection_type;
	html->selection_owner = selection_owner;

	if (!changed)
		/* Don't need to report lack of change to owner */
		return;

	/* Prepare msg */
	switch (selection_type) {
	case HTML_SELECTION_NONE:
		assert(selection_owner.none == true);
		msg_data.selection.selection = false;
		break;
	case HTML_SELECTION_SELF:
		assert(selection_owner.none == false);
		/* fall through */
	case HTML_SELECTION_TEXTAREA:
	case HTML_SELECTION_CONTENT:
		msg_data.selection.selection = true;
		break;
	default:
		break;
	}
	msg_data.selection.read_only = read_only;

	/* Inform of the content's selection status change */
	content_broadcast((struct content *)html, CONTENT_MSG_SELECTION,
			&msg_data);
}
