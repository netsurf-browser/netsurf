/*
 * Copyright 2004-2008 James Bursa <bursa@users.sourceforge.net>
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
 * Scrollbar widget (implementation).
 */

#include <stdbool.h>
#include <stdlib.h>

#include "desktop/mouse.h"
#include "desktop/scrollbar.h"
#include "desktop/plotters.h"
#include "desktop/plot_style.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"


struct scrollbar {
	bool horizontal;	/* Horizontal scrollbar if true, else vertical
				 */
	int length;		/* Length of the scrollbar widget */

	int full_size;		/* Length of the full scrollable area */
	int visible_size;	/* Length visible part of the scrollable area */

	int offset;		/* Current scroll offset to visible area */

	int bar_pos;		/* Position of the scrollbar */
	int bar_len;		/* Length of the scrollbar */

	scrollbar_client_callback client_callback;	/* Callback receiving
							 * scrollbar events */
	void *client_data;	/* User data passed to the callback */

	bool dragging;		/* Flag indicating drag at progess */
	int drag_start_coord;	/* Coordinate value at drag start */
	int drag_start_bar_pos;	/* Scrollbar offset at drag start */
	bool reverse;		/* Flag indicating that the scrollbar moves
				 * in the opposite direction to the mouse
				 */

	struct scrollbar *pair;	/* Parpendicular scrollbar, or NULL */
	bool pair_drag;		/* Flag indicating that the current drag affects
				   the perpendicular scrollbar too */
};


/** Overflow scrollbar colours
 *
 * Overflow scrollbar colours can be set by front end code to try to match
 * scrollbar colours used on the desktop.
 *
 * If a front end doesn't set scrollbar colours, these defaults are used.
 */
colour scrollbar_widget_fg_colour = 0x00d9d9d9; /* light grey */
colour scrollbar_widget_bg_colour = 0x006b6b6b; /* mid grey */
colour scrollbar_widget_arrow_colour = 0x00444444; /* dark grey */


/**
 * Create a scrollbar.
 *
 * \param horizontal		true for a horizontal scrollbar false for a
 * 				vertical one
 * \param length		full length of the scrollbar widget
 * \param scrolled_dimension	full length of the scrolled area
 * \param scrolled_visible	length of the visible part of the scrolled area
 * \param client_data		data for the client callback
 * \param client_callback	client callback for scrollbar events
 * \param scrollbar		gets updated to point at the newly created
 * 				scrollbar
 * \return			true if scrollbar has been created succesfully
 *				or false on memory exhaustion
 */

bool scrollbar_create(bool horizontal, int length,
		int scrolled_dimension, int scrolled_visible,
  		void *client_data, scrollbar_client_callback client_callback,
		struct scrollbar **scrollbar_pt)
{
	struct scrollbar *scrollbar;
	int well_length;

	scrollbar = malloc(sizeof(struct scrollbar));
	if (scrollbar == NULL) {
		LOG(("malloc failed"));
		warn_user("NoMemory", 0);
		*scrollbar_pt = NULL;
		return false;
	}

	scrollbar->horizontal = horizontal;
	scrollbar->length = length;
	scrollbar->full_size = scrolled_dimension;
	scrollbar->visible_size = scrolled_visible;
	scrollbar->offset = 0;
	scrollbar->bar_pos = 0;
	scrollbar->reverse = false;
	scrollbar->pair = NULL;
	scrollbar->pair_drag = false;

	well_length = length - 2 * SCROLLBAR_WIDTH;
	scrollbar->bar_len = (well_length * scrolled_visible) /
			scrolled_dimension;

	scrollbar->client_callback = client_callback;
	scrollbar->client_data = client_data;

	scrollbar->dragging = false;

	*scrollbar_pt = scrollbar;

	return true;
}


/**
 * Destroy a scrollbar.
 *
 * \param scrollbar	the scrollbar to be destroyed
 */

void scrollbar_destroy(struct scrollbar *scrollbar)
{
	if (scrollbar->pair != NULL)
		scrollbar->pair->pair = NULL;
	free(scrollbar);
}


/**
 * Draw an outline rectangle common to a several scrollbar elements.
 *
 * \param x0	left border of the outline
 * \param y0	top border of the outline
 * \param x1	right border of the outline
 * \param y1	bottom border of the outline
 * \param c	base colour of the outline, the other colours are created by
 * 		lightening or darkening this one
 * \param inset true for inset outline, false for an outset one
 * \return
 */

static inline bool scrollbar_redraw_scrollbar_rectangle(int x0, int y0,
		int x1, int y1, colour c, bool inset)
{
	static plot_style_t c0 = {
		.stroke_type = PLOT_OP_TYPE_SOLID,
		.stroke_width = 1,
	};

	static plot_style_t c1 = {
		.stroke_type = PLOT_OP_TYPE_SOLID,
		.stroke_width = 1,
	};

	static plot_style_t c2 = {
		.stroke_type = PLOT_OP_TYPE_SOLID,
		.stroke_width = 1,
	};

	if (inset) {
		c0.stroke_colour = darken_colour(c);
		c1.stroke_colour = lighten_colour(c);
	} else {
		c0.stroke_colour = lighten_colour(c);
		c1.stroke_colour = darken_colour(c);
	}
	c2.stroke_colour = blend_colour(c0.stroke_colour, c1.stroke_colour);

	/* Plot the outline */
	if (!plot.line(x0, y0, x1, y0, &c0)) return false;
	if (!plot.line(x1, y0, x1, y1 + 1, &c1)) return false;
	if (!plot.line(x1, y0, x1, y0 + 1, &c2)) return false;
	if (!plot.line(x1, y1, x0, y1, &c1)) return false;
	if (!plot.line(x0, y1, x0, y0, &c0)) return false;
	if (!plot.line(x0, y1, x0, y1 + 1, &c2)) return false;

	return true;
}


/**
 * Redraw a part of the scrollbar.
 *
 * \param scrollbar	the scrollbar to be redrawn
 * \param x		the X coordinate to draw the scrollbar at
 * \param y		the Y coordinate to draw the scrollbar at
 * \param clip		the clipping rectangle
 * \param scale		scale for the redraw
 * \return		true on succes false otherwise
 */

bool scrollbar_redraw(struct scrollbar *scrollbar, int x, int y, 
		const struct rect *clip, float scale)
{
	int w = SCROLLBAR_WIDTH;
	int well_length, bar_pos, bar_c0, bar_c1;
	int v[6]; /* array of triangle vertices */
	int x0, y0, x1, y1;

	plot_style_t pstyle_scrollbar_widget_bg_colour = {
		.fill_type = PLOT_OP_TYPE_SOLID,
		.fill_colour = scrollbar_widget_bg_colour,
	};
	plot_style_t pstyle_scrollbar_widget_fg_colour = {
		.fill_type = PLOT_OP_TYPE_SOLID,
		.fill_colour = scrollbar_widget_fg_colour,
	};
	plot_style_t pstyle_scrollbar_widget_arrow_colour = {
		.fill_type = PLOT_OP_TYPE_SOLID,
		.fill_colour = scrollbar_widget_arrow_colour,
	};

	well_length = scrollbar->length - 2 * SCROLLBAR_WIDTH;
	x0 = x;
	y0 = y;
	x1 = x + (scrollbar->horizontal ?
			scrollbar->length : SCROLLBAR_WIDTH) - 1;
	y1 = y + (scrollbar->horizontal ?
			SCROLLBAR_WIDTH : scrollbar->length) - 1;
	bar_pos = scrollbar->bar_pos;
	bar_c1 = (scrollbar->horizontal ? x0 : y0) + SCROLLBAR_WIDTH +
			scrollbar->bar_pos + scrollbar->bar_len - 1;

	if (scale != 1.0) {
		w *= scale;
		well_length *= scale;
		x0 *= scale;
		y0 *= scale;
		x1 *= scale;
		y1 *= scale;
		bar_pos *= scale;
		bar_c1 *= scale;
	}

	bar_c0 = (scrollbar->horizontal ? x0 : y0) + w + bar_pos;

	if (x1 < clip->x0 || y1 < clip->y0 || clip->x1 < x0 || clip->y1 < y0)
		/* scrollbar is outside the clipping rectangle, nothing to
		 * render */
		return true;

	
	if (scrollbar->horizontal) {
		/* scrollbar is horizontal */
		
		/* scrollbar outline */
		if (!scrollbar_redraw_scrollbar_rectangle(x0, y0, x1, y1,
				scrollbar_widget_bg_colour, true))
			return false;
		/* left arrow icon border */
		if (!scrollbar_redraw_scrollbar_rectangle(x0 + 1,
		     		y0 + 1,
				x0 + w - 2,
    				y1 - 1,
				scrollbar_widget_fg_colour, false))
			return false;
		/* left arrow icon background */
		if (!plot.rectangle(x0 + 2,
		     		y0 + 2,
				x0 + w - 2,
    				y1 - 1,
				&pstyle_scrollbar_widget_fg_colour))
			return false;
		/* left arrow */
		v[0] = x0 + w / 4;
		v[1] = y0 + w / 2;
		v[2] = x0 + w * 3 / 4;
		v[3] = y0 + w / 4;
		v[4] = x0 + w * 3 / 4;
		v[5] = y0 + w * 3 / 4;
		if (!plot.polygon(v, 3, &pstyle_scrollbar_widget_arrow_colour))
			return false;
		/* scrollbar well background */
		if (!plot.rectangle(x0 + w - 1,
		     		y0 + 1,
	 			x1 - w + 2,
    				y1,
				&pstyle_scrollbar_widget_bg_colour))
			return false;
		/* scrollbar position indicator bar */
		if (!scrollbar_redraw_scrollbar_rectangle(bar_c0,
				y0 + 1,
				bar_c1,
				y1 - 1,
				scrollbar_widget_fg_colour, false))
			return false;
		if (!plot.rectangle(bar_c0 + 1,
		     		y0 + 2,
				bar_c1,
    				y1 - 1,
				&pstyle_scrollbar_widget_fg_colour))
			return false;
		/* right arrow icon border */
		if (!scrollbar_redraw_scrollbar_rectangle(x1 - w + 2,
				y0 + 1,
				x1 - 1,
				y1 - 1,
				scrollbar_widget_fg_colour, false))
			return false;
		/* right arrow icon background */
		if (!plot.rectangle(x1 - w + 3,
		     		y0 + 2,
				x1 - 1,
    				y1 - 1,
				&pstyle_scrollbar_widget_fg_colour))
			return false;
		/* right arrow */
		v[0] = x1 - w / 4 + 1;
		v[1] = y0 + w / 2;
		v[2] = x1 - w * 3 / 4 + 1;
		v[3] = y0 + w / 4;
		v[4] = x1 - w * 3 / 4 + 1;
		v[5] = y0 + w * 3 / 4;
		if (!plot.polygon(v, 3, &pstyle_scrollbar_widget_arrow_colour))
			return false;
	} else {
		/* scrollbar is vertical */
		
		/* outline */
		if (!scrollbar_redraw_scrollbar_rectangle(x0, y0, x1, y1,
				scrollbar_widget_bg_colour,
				true))
			return false;
 		/* top arrow background */
		if (!scrollbar_redraw_scrollbar_rectangle(x0 + 1,
				y0 + 1,
				x1 - 1,
				y0 + w - 2,
				scrollbar_widget_fg_colour,
				false))
			return false;
		if (!plot.rectangle(x0 + 2,
				y0 + 2,
				x1 - 1,
				y0 + w - 2,
				&pstyle_scrollbar_widget_fg_colour))
			return false;
		/* up arrow */
		v[0] = x0 + w / 2;
		v[1] = y0 + w / 4;
		v[2] = x0 + w / 4;
		v[3] = y0 + w * 3 / 4;
		v[4] = x0 + w * 3 / 4;
		v[5] = y0 + w * 3 / 4;
		if (!plot.polygon(v, 3, &pstyle_scrollbar_widget_arrow_colour))
			return false;
		/* scrollbar well background */
		if (!plot.rectangle(x0 + 1,
				y0 + w - 1,
				x1,
				y1 - w + 2,
				&pstyle_scrollbar_widget_bg_colour))
			return false;
		/* scrollbar position indicator bar */
		if (!scrollbar_redraw_scrollbar_rectangle(x0 + 1,
				bar_c0,
				x1 - 1,
				bar_c1,
				scrollbar_widget_fg_colour, false))
			return false;
		if (!plot.rectangle(x0 + 2,
				bar_c0 + 1,
				x1 - 1,
				bar_c1,
				&pstyle_scrollbar_widget_fg_colour))
			return false;
		/* bottom arrow background */
		if (!scrollbar_redraw_scrollbar_rectangle(x0 + 1,
				y1 - w + 2,
				x1 - 1,
				y1 - 1,
				scrollbar_widget_fg_colour, false))
			return false;
		if (!plot.rectangle(x0 + 2,
				y1 - w + 3,
				x1 - 1,
				y1 - 1,
				&pstyle_scrollbar_widget_fg_colour))
			return false;
		/* down arrow */
		v[0] = x0 + w / 2;
		v[1] = y1 - w / 4 + 1;
		v[2] = x0 + w / 4;
		v[3] = y1 - w * 3 / 4 + 1;
		v[4] = x0 + w * 3 / 4;
		v[5] = y1 - w * 3 / 4 + 1;
		if (!plot.polygon(v, 3, &pstyle_scrollbar_widget_arrow_colour))
			return false;
	}

	return true;
}


/**
 * Set the value of the scrollbar.
 *
 * \param scrollbar	the scrollbar to have the value set
 * \param scroll_val	the new value to be set
 * \param bar_pos	true if the value is for the scrollbar indication bar
 *			offset, false if it is for the scrolled area one
 */

void scrollbar_set(struct scrollbar *scrollbar, int scroll_val, bool bar_pos)
{
	int well_length;
	struct scrollbar_msg_data msg;

	if (scroll_val < 0)
		scroll_val = 0;

	if (scrollbar->full_size == scrollbar->visible_size)
		return;

	well_length = scrollbar->length - 2 * SCROLLBAR_WIDTH;
	if (bar_pos) {
		if (scroll_val > well_length - scrollbar->bar_len)
			scrollbar->bar_pos = well_length - scrollbar->bar_len;
		else
			scrollbar->bar_pos = scroll_val;

		scrollbar->offset = ((scrollbar->full_size -
				scrollbar->visible_size) *
				(scrollbar->bar_pos)) /
				(well_length - scrollbar->bar_len);

	} else {
		if (scroll_val > scrollbar->full_size - scrollbar->visible_size)
			scrollbar->offset = scrollbar->full_size -
					scrollbar->visible_size;
		else
			scrollbar->offset = scroll_val;

		scrollbar->bar_pos = (well_length * scrollbar->offset) /
				scrollbar->full_size;
	}

	msg.scrollbar = scrollbar;
	msg.msg = SCROLLBAR_MSG_MOVED;
	msg.new_scroll = scrollbar->offset;
	scrollbar->client_callback(scrollbar->client_data, &msg);

	msg.msg = SCROLLBAR_MSG_REDRAW;
	msg.x0 = scrollbar->horizontal ? SCROLLBAR_WIDTH - 1 : 0;
	msg.y0 = scrollbar->horizontal ? 0 : SCROLLBAR_WIDTH - 1;
	msg.x1 = (scrollbar->horizontal ?
			scrollbar->length - SCROLLBAR_WIDTH + 1 :
			SCROLLBAR_WIDTH);
	msg.y1 = (scrollbar->horizontal ?
			SCROLLBAR_WIDTH :
			scrollbar->length - SCROLLBAR_WIDTH + 1);

	scrollbar->client_callback(scrollbar->client_data, &msg);
}


/**
 * Get the current scroll offset to the visible part of the full area.
 *
 * \param scrollbar	the scrollbar to get the scroll offset value from
 * \return		current scroll offset
 */

int scrollbar_get_offset(struct scrollbar *scrollbar)
{
	if (scrollbar == NULL)
		return 0;
	return scrollbar->offset;
}


/**
 * Set the length of the scrollbar widget, the size of the visible area, and the
 * size of the full area.
 *
 * \param scrollbar		the scrollbar to set the values for
 * \param length		-1 or the new scrollbar widget length
 * \param scrolled_visible	-1 or the new size of the visible area
 * \param scrolled_dimension	-1 or the new size of the full contained area
 */

void scrollbar_set_extents(struct scrollbar *scrollbar, int length,
		int scrolled_visible, int scrolled_dimension)
{
	int well_length;

	/* TODO: update offset appropriately */

	if (length != -1)
		scrollbar->length = length;
	if (scrolled_visible != -1)
		scrollbar->visible_size = scrolled_visible;
	if (scrolled_dimension != -1)
		scrollbar->full_size = scrolled_dimension;

	well_length = scrollbar->length - 2 * SCROLLBAR_WIDTH;

	scrollbar->bar_len = (well_length * scrollbar->visible_size) /
			scrollbar->full_size;
	scrollbar->bar_pos = (well_length * scrollbar->offset) /
			scrollbar->full_size;
}


/**
 * Check orientation of the scrollbar.
 *
 * \param scrollbar	the scrollbar to check the orientation of
 * \return		true for a horizontal scrollbar, else false (vertical)
 */

bool scrollbar_is_horizontal(struct scrollbar *scrollbar)
{
	return scrollbar->horizontal;
}


/**
 * Internal procedure used for starting a drag scroll for a scrollbar.
 *
 * \param scrollbar	the scrollbar to start the drag for
 * \param x		the X coordinate of the drag start
 * \param y		the Y coordinate of the drag start
 * \param reverse	whether this should be a reverse drag (used when the
 * 			user drags the content area, rather than the scrollbar)
 * \param pair		whether the drag is a '2D' scroll
 */

static void scrollbar_drag_start_internal(struct scrollbar *scrollbar,
		int x, int y, bool reverse, bool pair)
{
	struct scrollbar_msg_data msg;

	scrollbar->drag_start_coord = scrollbar->horizontal ? x : y;
	scrollbar->drag_start_bar_pos = scrollbar->bar_pos;

	scrollbar->dragging = true;
	scrollbar->reverse = reverse;

	msg.scrollbar = scrollbar;

	/* \todo - some proper numbers please! */
	if (scrollbar->horizontal) {
		msg.x0 = -1024;
		msg.x1 = 1024;
		msg.y0 = 0;
		msg.y1 = 0;
	} else {
		msg.x0 = 0;
		msg.x1 = 0;
		msg.y0 = -1024;
		msg.y1 = 1024;
	}

	if (pair && scrollbar->pair != NULL) {
		scrollbar->pair_drag = true;

		scrollbar->pair->drag_start_coord =
				scrollbar->pair->horizontal ? x : y;

		scrollbar->pair->drag_start_bar_pos = scrollbar->pair->bar_pos;

		scrollbar->pair->dragging = true;
		scrollbar->pair->reverse = reverse;

		if (scrollbar->pair->horizontal) {
			msg.x0 = -1024;
			msg.x1 = 1024;
		} else {
			msg.y0 = -1024;
			msg.y1 = 1024;
		}
	}
	msg.msg = SCROLLBAR_MSG_SCROLL_START;
	scrollbar->client_callback(scrollbar->client_data, &msg);
}


/**
 * Handle mouse actions other then drag ends.
 *
 * \param scrollbar	the scrollbar which gets the mouse action
 * \param mouse		mouse state
 * \param x		X coordinate of the mouse
 * \param y		Y coordinate of the mouse
 * \return		message for the status bar or NULL on failure
 */

const char *scrollbar_mouse_action(struct scrollbar *scrollbar,
		browser_mouse_state mouse, int x, int y)
{
	int x0, y0, x1, y1;
	int val;
	const char *status;
	bool h;

	/* we want mouse presses and mouse drags that were not started at the
	 * scrollbar indication bar to be launching actions on the scroll area
	 */
	bool but1 = ((mouse & BROWSER_MOUSE_PRESS_1) ||
			((mouse & BROWSER_MOUSE_HOLDING_1) &&
			(mouse & BROWSER_MOUSE_DRAG_ON) &&
			!scrollbar->dragging));
	bool but2 = ((mouse & BROWSER_MOUSE_PRESS_2) ||
			((mouse & BROWSER_MOUSE_HOLDING_2) &&
			(mouse & BROWSER_MOUSE_DRAG_ON) &&
			!scrollbar->dragging));

	h = scrollbar->horizontal;

	x0 = 0;
	y0 = 0;
	x1 = h ? scrollbar->length : SCROLLBAR_WIDTH;
	y1 = h ? SCROLLBAR_WIDTH : scrollbar->length;

	if (!scrollbar->dragging &&
			!(x >= x0 && x <= x1 && y >= y0 && y <= y1)) {
		/* Not a drag and mouse outside scrollbar widget */
		return NULL;
	}


	if (h)
		val = x;
	else
		val = y;

	if (scrollbar->dragging) {
		val -= scrollbar->drag_start_coord;
		if (scrollbar->reverse)
			val = -val;
		if (val != 0)
			scrollbar_set(scrollbar,
					scrollbar->drag_start_bar_pos + val,
					true);
		if (scrollbar->pair_drag) {
			scrollbar_mouse_action(scrollbar->pair, mouse, x, y);
			status = messages_get("ScrollBoth");
		} else
			status = messages_get(h ? "ScrollH" : "ScrollV");

		return status;
	}

	if (val < SCROLLBAR_WIDTH) {
		/* left/up arrow */
		
		status = messages_get(h ? "ScrollLeft" : "ScrollUp");
		if (but1)
			scrollbar_set(scrollbar,
					scrollbar->bar_pos - SCROLLBAR_WIDTH,
					true);
		else if (but2)
			scrollbar_set(scrollbar,
					scrollbar->bar_pos + SCROLLBAR_WIDTH,
					true);

	} else if (val < SCROLLBAR_WIDTH + scrollbar->bar_pos) {
		/* well between left/up arrow and bar */

		status = messages_get(h ? "ScrollPLeft" : "ScrollPUp");

		if (but1)
			scrollbar_set(scrollbar,
					scrollbar->offset - scrollbar->length,
					false);
		else if (but2)
			scrollbar_set(scrollbar,
					scrollbar->offset + scrollbar->length,
					false);

	} else if (val > scrollbar->length - SCROLLBAR_WIDTH) {
		/* right/down arrow */

		status = messages_get(h ? "ScrollRight" : "ScrollDown");

		if (but1)
			scrollbar_set(scrollbar,
					scrollbar->bar_pos + SCROLLBAR_WIDTH,
					true);
		else if (but2)
			scrollbar_set(scrollbar,
					scrollbar->bar_pos - SCROLLBAR_WIDTH,
					true);

	} else if (val > SCROLLBAR_WIDTH + scrollbar->bar_pos +
			scrollbar->bar_len) {
		/* well between right/down arrow and bar */

		status = messages_get(h ? "ScrollPRight" : "ScrollPDown");
		if (but1)
			scrollbar_set(scrollbar,
					scrollbar->offset + scrollbar->length,
					false);
		else if (but2)
			scrollbar_set(scrollbar,
					scrollbar->offset - scrollbar->length,
					false);
	}
	else {
		/* scrollbar position indication bar */
		
		status = messages_get(h ? "ScrollH" : "ScrollV");
	}

	
	if (mouse & (BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_2) &&
		   	(val >= SCROLLBAR_WIDTH + scrollbar->bar_pos
		   	&& val < SCROLLBAR_WIDTH + scrollbar->bar_pos +
		   			scrollbar->bar_len))
		/* The mouse event is a drag start on the scrollbar position
		 * indication bar. */
		scrollbar_drag_start_internal(scrollbar, x, y, false,
				(mouse & BROWSER_MOUSE_DRAG_2) ? true : false);

	return status;
}


/**
 * Handle end of mouse drags.
 *
 * \param scrollbar	the scrollbar for which the drag ends
 * \param mouse		mouse state
 * \param x		X coordinate of the mouse
 * \param y		Y coordinate of the mouse
 */

void scrollbar_mouse_drag_end(struct scrollbar *scrollbar,
		browser_mouse_state mouse, int x, int y)
{
	struct scrollbar_msg_data msg;
	int val;

	assert(scrollbar->dragging);

	val = (scrollbar->horizontal ? x : y) - scrollbar->drag_start_coord;
	if (scrollbar->reverse)
		val = -val;
	if (val != 0)
		scrollbar_set(scrollbar, scrollbar->drag_start_bar_pos + val,
				true);

	scrollbar->dragging = false;
	scrollbar->reverse = false;
	if (scrollbar->pair_drag) {
		scrollbar->pair_drag = false;

		val = (scrollbar->pair->horizontal ? x : y) -
				scrollbar->pair->drag_start_coord;
		if (scrollbar->pair->reverse)
			val = -val;
		if (val != 0)
			scrollbar_set(scrollbar->pair,
					scrollbar->pair->drag_start_bar_pos + val,
     					true);

		scrollbar->pair->dragging = false;
		scrollbar->pair->reverse = false;
	}
	msg.scrollbar = scrollbar;
	msg.msg = SCROLLBAR_MSG_SCROLL_FINISHED;
	scrollbar->client_callback(scrollbar->client_data, &msg);
}


/**
 * Called when the content is being dragged to the scrollbars have to adjust.
 * If the content has both scrollbars, and scrollbar_make_pair has beed called
 * before, only the one scroll which will receive further mouse events has to be
 * passed.
 *
 * \param scrollbar	one of the the scrollbars owned by the dragged content
 * \param x		X coordinate of mouse during drag start
 * \param y		Y coordinate of mouse during drag start
 */

void scrollbar_start_content_drag(struct scrollbar *scrollbar, int x, int y)
{
	scrollbar_drag_start_internal(scrollbar, x, y, true, true);
}


/**
 * Connect a horizontal and a vertical scrollbar into a pair so that they
 * co-operate during 2D drags.
 *
 * \param horizontal_scrollbar	the scrollbar used for horizontal scrolling
 * \param vertical_scrollbar	the scrollbar used for vertical scrolling
 */

void scrollbar_make_pair(struct scrollbar *horizontal_scrollbar,
		struct scrollbar *vertical_scrollbar)
{
	assert(horizontal_scrollbar->horizontal &&
			!vertical_scrollbar->horizontal);

	horizontal_scrollbar->pair = vertical_scrollbar;
	vertical_scrollbar->pair = horizontal_scrollbar;
}


void *scrollbar_get_data(struct scrollbar *scrollbar)
{
	return scrollbar->client_data;
}

