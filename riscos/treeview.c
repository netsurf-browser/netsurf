/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Generic tree handling (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <swis.h>
#include <time.h>
#include "oslib/colourtrans.h"
#include "oslib/dragasprite.h"
#include "oslib/osbyte.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "netsurf/desktop/tree.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/riscos/treeview.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"

#define TREE_EXPAND 0
#define TREE_COLLAPSE 1


static bool ro_gui_tree_initialise_sprite(const char *name, int number);
static void ro_gui_tree_launch_selected_node(struct node *node, bool all);
static bool ro_gui_tree_launch_node(struct node *node);

/* an array of sprite addresses for Tinct */
static char *ro_gui_tree_sprites[2];

/* origin adjustment */
static int ro_gui_tree_origin_x;
static int ro_gui_tree_origin_y;

/* element drawing */
static wimp_icon ro_gui_tree_icon;
static char ro_gui_tree_icon_validation[24];
static char ro_gui_tree_icon_null[] = "\0";

/* dragging information */
static struct tree *ro_gui_tree_current_drag_tree;
static wimp_mouse_state ro_gui_tree_current_drag_buttons;

/* editing information */
static wimp_icon_create ro_gui_tree_edit_icon;

/* dragging information */
static char ro_gui_tree_drag_name[12];


/**
 * Performs any initialisation for tree rendering
 */
bool ro_gui_tree_initialise(void) {
	if (ro_gui_tree_initialise_sprite("expand", TREE_EXPAND) ||
			ro_gui_tree_initialise_sprite("collapse", TREE_COLLAPSE))
		return false;

	ro_gui_tree_edit_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
			wimp_ICON_VCENTRED | wimp_ICON_FILLED | wimp_ICON_BORDER |
			(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT) |
			(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
			(wimp_BUTTON_WRITABLE << wimp_ICON_BUTTON_TYPE_SHIFT);
	ro_gui_tree_edit_icon.icon.data.indirected_text.validation =
			ro_gui_tree_icon_null;
	ro_gui_tree_edit_icon.icon.data.indirected_text.size = 256;

	return true;
}


/**
 * Initialise a sprite for use with Tinct
 *
 * \param  name	   the name of the sprite
 * \param  number  the sprite cache number
 * \return whether an error occurred during initialisation
 */
bool ro_gui_tree_initialise_sprite(const char *name, int number) {
	char icon_name[12];
	os_error *error;

	sprintf(icon_name, "tr_%s", name);
	error = xosspriteop_select_sprite(osspriteop_USER_AREA, gui_sprites,
				(osspriteop_id)icon_name,
				(osspriteop_header **)&ro_gui_tree_sprites[number]);
	if (error) {
		warn_user("MiscError", error->errmess);
		LOG(("Failed to find sprite 'tr_%s'", name));
		return true;
	}
	return false;
}


/**
 * Informs the current window manager that an area requires updating.
 *
 * \param tree	  the tree that is requesting a redraw
 * \param x	  the x co-ordinate of the redraw area
 * \param y	  the y co-ordinate of the redraw area
 * \param width	  the width of the redraw area
 * \param height  the height of the redraw area
 */
void tree_redraw_area(struct tree *tree, int x, int y, int width, int height) {
	os_error *error;

	assert(tree);
	assert(tree->handle);

	if (tree->toolbar)
		y += ro_gui_theme_toolbar_height(tree->toolbar);
	error = xwimp_force_redraw((wimp_w)tree->handle, tree->offset_x + x - 2,
			-tree->offset_y - y - height, tree->offset_x + x + width + 4,
			-tree->offset_y - y);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Draws a line.
 *
 * \param tree	 the tree to draw a line for
 * \param x	 the x co-ordinate
 * \param x	 the y co-ordinate
 * \param x	 the width of the line
 * \param x	 the height of the line
 */
void tree_draw_line(struct tree *tree, int x, int y, int width, int height) {
	os_error *error;

	assert(tree);

	error = xcolourtrans_set_gcol((os_colour)0x88888800, 0, os_ACTION_OVERWRITE,
			0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		return;
	}
	error = xos_plot(os_MOVE_TO, ro_gui_tree_origin_x + x,
			ro_gui_tree_origin_y - y);
	if (!error)
		xos_plot(os_PLOT_TO, ro_gui_tree_origin_x + x + width,
				ro_gui_tree_origin_y - y - height);
	if (error) {
		LOG(("xos_plot: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		return;
	}
}


/**
 * Draws an element, including any expansion icons
 *
 * \param tree	   the tree to draw an element for
 * \param element  the element to draw
 */
void tree_draw_node_element(struct tree *tree, struct node_element *element) {
	os_error *error;
	int temp;
	int toolbar_height = 0;

	assert(tree);
	assert(element);
	assert(element->parent);
	
	if (tree->toolbar)
		toolbar_height = ro_gui_theme_toolbar_height(tree->toolbar);


	ro_gui_tree_icon.flags = wimp_ICON_INDIRECTED | wimp_ICON_VCENTRED |
			(wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT);
	ro_gui_tree_icon.extent.x0 = tree->offset_x + element->box.x;
	ro_gui_tree_icon.extent.y1 = -tree->offset_y - element->box.y - toolbar_height;
	ro_gui_tree_icon.extent.x1 = tree->offset_x + element->box.x +
			element->box.width;
	ro_gui_tree_icon.extent.y0 = -tree->offset_y - element->box.y -
			element->box.height - toolbar_height;
	if (&element->parent->data == element) {
		if (element->parent->selected)
			ro_gui_tree_icon.flags |= wimp_ICON_SELECTED;
		ro_gui_tree_icon.flags |= (wimp_COLOUR_BLACK <<
				wimp_ICON_FG_COLOUR_SHIFT);
	} else {
		ro_gui_tree_icon.flags |= (wimp_COLOUR_DARK_GREY <<
				wimp_ICON_FG_COLOUR_SHIFT);
	}

	switch (element->type) {
		case NODE_ELEMENT_TEXT_PLUS_SPRITE:
			assert(element->sprite);

			ro_gui_tree_icon.flags |= wimp_ICON_TEXT | wimp_ICON_SPRITE;
			ro_gui_tree_icon.data.indirected_text_and_sprite.text =
					ro_gui_tree_icon_null;
			ro_gui_tree_icon.data.indirected_text_and_sprite.validation =
					ro_gui_tree_icon_validation;
			ro_gui_tree_icon.data.indirected_text_and_sprite.size = 1;
			if (element->parent->expanded) {
				sprintf(ro_gui_tree_icon_validation, "S%s",
						element->sprite->expanded_name);
			} else {
				sprintf(ro_gui_tree_icon_validation, "S%s",
						element->sprite->name);
			}
			temp = ro_gui_tree_icon.extent.x1;
			ro_gui_tree_icon.extent.x1 = ro_gui_tree_icon.extent.x0 +
					NODE_INSTEP;
			error = xwimp_plot_icon(&ro_gui_tree_icon);
			if (error) {
				LOG(("xwimp_plot_icon: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
			}
			ro_gui_tree_icon.extent.x0 = ro_gui_tree_icon.extent.x1;
			ro_gui_tree_icon.extent.x1 = temp;
			ro_gui_tree_icon.flags &= ~wimp_ICON_SPRITE;

		case NODE_ELEMENT_TEXT:
			assert(element->text);

			if (element == tree->editing)
				return;

			if (ro_gui_tree_icon.flags & wimp_ICON_SELECTED)
				ro_gui_tree_icon.flags |= wimp_ICON_FILLED;
			ro_gui_tree_icon.flags |= wimp_ICON_TEXT;
			ro_gui_tree_icon.data.indirected_text.text =
					element->text;
			ro_gui_tree_icon.data.indirected_text.validation =
					ro_gui_tree_icon_null;
			ro_gui_tree_icon.data.indirected_text.size =
					strlen(element->text);
			break;
		case NODE_ELEMENT_SPRITE:
			assert(element->sprite);

			ro_gui_tree_icon.flags |= wimp_ICON_SPRITE;
			ro_gui_tree_icon.data.indirected_sprite.id =
					(osspriteop_id)element->sprite->name;
			ro_gui_tree_icon.data.indirected_sprite.area =
					element->sprite->area;
			ro_gui_tree_icon.data.indirected_sprite.size =
					strlen(element->sprite->name);
			break;
	}

	error = xwimp_plot_icon(&ro_gui_tree_icon);
	if (error) {
		LOG(("xwimp_plot_icon: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Draws an elements expansion icon
 *
 * \param tree	   the tree to draw the expansion for
 * \param element  the element to draw the expansion for
 */
void tree_draw_node_expansion(struct tree *tree, struct node *node) {
	unsigned int type;

	assert(tree);
	assert(node);

	if ((node->child) || (node->data.next)) {
		if (node->expanded) {
			type = TREE_COLLAPSE;
		} else {
			type = TREE_EXPAND;
		}
		_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
				ro_gui_tree_sprites[type],
				ro_gui_tree_origin_x + node->box.x -
					(NODE_INSTEP / 2) - 8,
				ro_gui_tree_origin_y - node->box.y -
					(TREE_TEXT_HEIGHT / 2) - 8,
				tinct_BILINEAR_FILTER);

	}
}


/**
 * Sets the origin variables to the correct values for a specified tree
 *
 * \param tree  the tree to set the origin for
 */
void tree_initialise_redraw(struct tree *tree) {
	os_error *error;
	wimp_window_state state;

	assert(tree->handle);

	state.w = (wimp_w)tree->handle;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	ro_gui_tree_origin_x = state.visible.x0 - state.xscroll + tree->offset_x;
	ro_gui_tree_origin_y = state.visible.y1 - state.yscroll - tree->offset_y;
	if (tree->toolbar)
		ro_gui_tree_origin_y -= ro_gui_theme_toolbar_height(tree->toolbar);
}


/**
 * Recalculates the dimensions of a node element.
 *
 * \param element  the element to recalculate
 */
void tree_recalculate_node_element(struct node_element *element) {
	os_error *error;
	int sprite_width;
	int sprite_height;
	osspriteop_flags flags;

	assert(element);

	switch (element->type) {
		case NODE_ELEMENT_TEXT_PLUS_SPRITE:
			assert(element->sprite);
		case NODE_ELEMENT_TEXT:
			assert(element->text);

			error = xwimptextop_string_width(element->text,
					strlen(element->text),
					&element->box.width);
			if (error) {
				LOG(("xwimptextop_string_width: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
			}
			element->box.width += 16;
			element->box.height = TREE_TEXT_HEIGHT;
			if (element->type == NODE_ELEMENT_TEXT_PLUS_SPRITE)
				element->box.width += NODE_INSTEP;
			break;
		case NODE_ELEMENT_SPRITE:
			assert(element->sprite);

			flags = ((int)element->sprite->area == 1) ?
					osspriteop_SYSTEM_AREA :
					osspriteop_USER_AREA;
			error = xosspriteop_read_sprite_info(flags,
					element->sprite->area,
					(osspriteop_id)element->sprite->name,
					&sprite_width, &sprite_height, 0, 0);
			if (error) {
				LOG(("xosspriteop_read_sprite_info: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
			}
			element->box.width = sprite_width * 2;
			element->box.height = sprite_height * 2;
			if (element->box.height < TREE_TEXT_HEIGHT)
				element->box.height = TREE_TEXT_HEIGHT;
			break;
	}
}


/**
 * Sets a node element as having a specific sprite.
 *
 * \param node	    the node to update
 * \param sprite    the sprite to use
 * \param selected  the expanded sprite name to use
 */
void tree_set_node_sprite(struct node *node, const char *sprite,
		const char *expanded) {
	assert(node);
	assert(sprite);
	assert(expanded);
	assert(node->data.type != NODE_ELEMENT_SPRITE);

	node->data.sprite = calloc(sizeof(struct node_sprite), 1);
	if (!node->data.sprite) return;
	node->data.type = NODE_ELEMENT_TEXT_PLUS_SPRITE;
	node->data.sprite->area = (osspriteop_area *)1;
	sprintf(node->data.sprite->name, sprite);
	sprintf(node->data.sprite->expanded_name, expanded);
}


/**
 * Sets a node element as having a folder sprite
 *
 * \param node  the node to update
 */
void tree_set_node_sprite_folder(struct node *node) {
	assert(node->folder);

	tree_set_node_sprite(node, "small_dir", "small_diro");
}


/**
 * Updates the node details for a URL node.
 * The internal node dimensions are not updated.
 *
 * \param node  the node to update
 */
void tree_update_URL_node(struct node *node) {
	struct node_element *element;
	char buffer[256];

	assert(node);

	element = tree_find_element(node, TREE_ELEMENT_URL);
	if (element) {
		sprintf(buffer, "small_%.3x", element->user_data);
		if (ro_gui_wimp_sprite_exists(buffer))
			tree_set_node_sprite(node, buffer, buffer);
		else
			tree_set_node_sprite(node, "small_xxx", "small_xxx");
	}

	element = tree_find_element(node, TREE_ELEMENT_ADDED);
	if (element) {
		if (element->text) {
			free(element->text);
			element->text = NULL;
		}
		if (element->user_data > 0) {
			snprintf(buffer, 256, messages_get("TreeAdded"),
					ctime((time_t *)&element->user_data));
		} else {
			snprintf(buffer, 256, messages_get("TreeAdded"),
					messages_get("TreeUnknown"));
		}
		element->text = strdup(buffer);
	}

	element = tree_find_element(node, TREE_ELEMENT_LAST_VISIT);
	if (element) {
		if (element->text) {
			free(element->text);
			element->text = NULL;
		}
		if (element->user_data > 0) {
			snprintf(buffer, 256, messages_get("TreeLast"),
					ctime((time_t *)&element->user_data));
		} else {
			snprintf(buffer, 256, messages_get("TreeLast"),
					messages_get("TreeUnknown"));
		}
		element->text = strdup(buffer);
	}

	element = tree_find_element(node, TREE_ELEMENT_VISITED);
	if (element) {
		if (element->text) {
			free(element->text);
			element->text = NULL;
		}
		if (element->user_data > 0) {
			snprintf(buffer, 256, messages_get("TreeVisited"),
					ctime((time_t *)&element->user_data));
		} else {
			snprintf(buffer, 256, messages_get("TreeVisited"),
					messages_get("TreeUnknown"));
		}
		element->text = strdup(buffer);
	}

	element = tree_find_element(node, TREE_ELEMENT_VISITS);
	if (element) {
		if (element->text) {
			free(element->text);
			element->text = NULL;
		}
		snprintf(buffer, 256, messages_get("TreeVisits"),
				element->user_data);
		element->text = strdup(buffer);
	}

}


/**
 * Updates the tree owner following a tree resize
 *
 * \param tree  the tree to update the owner of
 */
void tree_resized(struct tree *tree) {
	os_error *error;
	wimp_window_state state;

	assert(tree->handle);


	state.w = (wimp_w)tree->handle;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if (state.flags & wimp_WINDOW_OPEN)
		ro_gui_tree_open((wimp_open *)&state, tree);
}


/**
 * Redraws a tree window
 *
 * \param redraw  the area to redraw
 * \param tree	  the tree to redraw
 */
void ro_gui_tree_redraw(wimp_draw *redraw, struct tree *tree) {
	osbool more;
	int clip_x0, clip_x1, clip_y0, clip_y1, origin_x, origin_y;
	

	more = wimp_redraw_window(redraw);
	while (more) {
		clip_x0 = redraw->clip.x0;
		clip_y0 = redraw->clip.y0;
		clip_x1 = redraw->clip.x1;
		clip_y1 = redraw->clip.y1;
		origin_x = redraw->box.x0 - redraw->xscroll;
		origin_y = redraw->box.y1 - redraw->yscroll;
		if (tree->toolbar)
			origin_y -= ro_gui_theme_toolbar_height(tree->toolbar);
		tree_draw(tree, clip_x0 - origin_x - tree->offset_x,
				origin_y - clip_y1 - tree->offset_y,
				clip_x1 - clip_x0, clip_y1 - clip_y0);
		more = wimp_get_rectangle(redraw);
	}
}


/**
 * Handles a mouse click for a tree
 *
 * \param pointer  the pointer state
 * \param tree	   the tree to handle a click for
 * \return whether the click was handled#
 */
bool ro_gui_tree_click(wimp_pointer *pointer, struct tree *tree) {
	bool furniture;
	struct node *node;
	struct node_element *element;
	int x, y;
	int alt_pressed = 0;
	wimp_window_state state;
	wimp_caret caret;
	wimp_drag drag;
	wimp_auto_scroll_info scroll;
	os_error *error;
	os_box box = { pointer->pos.x - 34, pointer->pos.y - 34,
			pointer->pos.x + 34, pointer->pos.y + 34 };

	assert(tree);
	assert(tree->root);

	/* gain the input focus when required */
	state.w = (wimp_w)tree->handle;
	error = xwimp_get_window_state(&state);
	if (error)
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
	error = xwimp_get_caret_position(&caret);
	if (error)
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
	if (((pointer->buttons == (wimp_CLICK_SELECT << 8)) ||
			(pointer->buttons == (wimp_CLICK_ADJUST << 8))) &&
			(caret.w != state.w)) {
		error = xwimp_set_caret_position((wimp_w)tree->handle, -1, -100,
						-100, 32, -1);
		if (error)
			LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
	}

	if (!tree->root->child)
		return true;

	tree_initialise_redraw(tree);
	x = pointer->pos.x - ro_gui_tree_origin_x;
	y = ro_gui_tree_origin_y - pointer->pos.y;
	element = tree_get_node_element_at(tree->root->child, x, y, &furniture);


	/* stop editing for anything but a drag */
	if ((tree->editing) && (pointer->i != tree->edit_handle) &&
			(pointer->buttons != (wimp_CLICK_SELECT << 4)))
		ro_gui_tree_stop_edit(tree);

	/* handle a menu click */
	if (pointer->buttons == wimp_CLICK_MENU) {
		if ((!element) || (!tree->root->child) ||
				(tree_has_selection(tree->root->child)))
			return true;

		node = element->parent;
		tree->temp_selection = node;
		node->selected = true;
		tree_handle_node_element_changed(tree, &node->data);
		return true;

	}

	/* no item either means cancel selection on (select) click or a drag */
	if (!element) {
		if ((pointer->buttons == (wimp_CLICK_SELECT << 4)) ||
				(pointer->buttons == (wimp_CLICK_SELECT << 8)))
			tree_set_node_selected(tree, tree->root->child, false);
		if ((pointer->buttons == (wimp_CLICK_SELECT << 4)) ||
				(pointer->buttons == (wimp_CLICK_ADJUST << 4))) {

			scroll.w = (wimp_w)tree->handle;
			scroll.pause_zone_sizes.y0 = 80;
			scroll.pause_zone_sizes.y1 = 80;
			scroll.pause_duration = 0;
			scroll.state_change = (void *)0;
			error = xwimp_auto_scroll(wimp_AUTO_SCROLL_ENABLE_VERTICAL,
					&scroll, 0);
			if (error)
				LOG(("xwimp_auto_scroll: 0x%x: %s",
						error->errnum, error->errmess));

			gui_current_drag_type = GUI_DRAG_TREE_SELECT;
			ro_gui_tree_current_drag_tree = tree;
			ro_gui_tree_current_drag_buttons = pointer->buttons;

			drag.w = (wimp_w)tree->handle;
			drag.type = wimp_DRAG_USER_RUBBER;
			drag.initial.x0 = pointer->pos.x;
			drag.initial.x1 = pointer->pos.x;
			drag.initial.y0 = pointer->pos.y;
			drag.initial.y1 = pointer->pos.y;
			drag.bbox.x0 = state.visible.x0;
			drag.bbox.x1 = state.visible.x1;
			drag.bbox.y0 = -16384;//state.visible.y0;
			drag.bbox.y1 = 16384;//state.visible.y1 - tree->offset_y;
			error = xwimp_drag_box_with_flags(&drag,
					wimp_DRAG_BOX_KEEP_IN_LINE |
					wimp_DRAG_BOX_CLIP);
			if (error)
				LOG(("xwimp_drag_box_with_flags: 0x%x: %s",
						error->errnum, error->errmess));

		}
		return true;
	}

	node = element->parent;

	/* click on furniture or double click on folder toggles node expansion */
	if (((furniture) && ((pointer->buttons == wimp_CLICK_SELECT << 8) ||
			(pointer->buttons == wimp_CLICK_ADJUST << 8) ||
			(pointer->buttons == wimp_CLICK_SELECT) ||
			(pointer->buttons == wimp_CLICK_ADJUST))) ||
			((!furniture) && (node->child) &&
			((pointer->buttons == wimp_CLICK_SELECT) ||
			(pointer->buttons == wimp_CLICK_ADJUST)))) {
		node->expanded = !node->expanded;
		if (!furniture)
			node->selected = false;
		tree_handle_node_changed(tree, node, false, true);
		return true;
	}

	/* no use for any other furniture click */
	if (furniture)
		return true;

	/* single/double alt+click starts editing */
	if ((node->editable) && (!tree->editing) && ((element->user_type == 0) ||
			(element->user_type == TREE_ELEMENT_URL)) &&
			((pointer->buttons == wimp_CLICK_SELECT) ||
			(pointer->buttons == (wimp_CLICK_SELECT << 8)))) {
		xosbyte1(osbyte_SCAN_KEYBOARD, 2 ^ 0x80, 0, &alt_pressed);
		if ((alt_pressed == 0xff) &&
				(element->type != NODE_ELEMENT_SPRITE)) {
			ro_gui_tree_start_edit(tree, element, pointer);
			return true;
		}
	}

	/* double click starts launches the leaf */
	if ((pointer->buttons == wimp_CLICK_SELECT) ||
			(pointer->buttons == wimp_CLICK_ADJUST)) {
		if (!ro_gui_tree_launch_node(node))
			return false;
		if (pointer->buttons == wimp_CLICK_ADJUST)
			ro_gui_tree_keypress(wimp_KEY_CONTROL + wimp_KEY_F2, tree);
		return true;
	}

	/* single click (select) cancels current selection and selects item */
	if (pointer->buttons == (wimp_CLICK_SELECT << 8)) {
		if (!node->selected) {
			tree_set_node_selected(tree, tree->root->child, false);
			node->selected = true;
			tree_handle_node_element_changed(tree, &node->data);
		}
		return true;
	}

	/* single click (adjust) toggles item selection */
	if (pointer->buttons == (wimp_CLICK_ADJUST << 8)) {
		node->selected = !node->selected;
		tree_handle_node_element_changed(tree, &node->data);
		return true;
	}

	/* drag starts a drag operation */
	if ((!tree->editing) && ((pointer->buttons == (wimp_CLICK_SELECT << 4)) ||
			(pointer->buttons == (wimp_CLICK_ADJUST << 4)))) {

		if (!node->selected) {
			node->selected = true;
			tree_handle_node_element_changed(tree, &node->data);
		}

		scroll.w = (wimp_w)tree->handle;
		scroll.pause_zone_sizes.y0 = 80;
		scroll.pause_zone_sizes.y1 = 80;
		scroll.pause_duration = -1;
		scroll.state_change = (void *)0;
		error = xwimp_auto_scroll(wimp_AUTO_SCROLL_ENABLE_VERTICAL,
				&scroll, 0);
		if (error)
			LOG(("xwimp_auto_scroll: 0x%x: %s",
					error->errnum, error->errmess));

		gui_current_drag_type = GUI_DRAG_TREE_MOVE;
		ro_gui_tree_current_drag_tree = tree;
		ro_gui_tree_current_drag_buttons = pointer->buttons;

		node = tree_get_selected_node(tree->root);
		if (node) {
		  	if (node->folder) {
		  		if ((node->expanded) &&
		  				(ro_gui_wimp_sprite_exists("directoryo")))
		  	  		sprintf(ro_gui_tree_drag_name, "directoryo");
		  	  	else
			  	  	sprintf(ro_gui_tree_drag_name, "directory");
		  	} else {
				element = tree_find_element(node, TREE_ELEMENT_URL);
				if (element) {
					sprintf(ro_gui_tree_drag_name, "file_%.3x",
							element->user_data);
				} else {
					sprintf(ro_gui_tree_drag_name, "file_xxx");
				}
				if (!ro_gui_wimp_sprite_exists(ro_gui_tree_drag_name))
					sprintf(ro_gui_tree_drag_name, "file_xxx");
			}
		} else {
	  		sprintf(ro_gui_tree_drag_name, "package");
		}

		error = xdragasprite_start(dragasprite_HPOS_CENTRE |
				dragasprite_VPOS_CENTRE |
				dragasprite_BOUND_POINTER |
				dragasprite_DROP_SHADOW,
				(osspriteop_area *) 1,
				ro_gui_tree_drag_name, &box, 0);
		if (error)
			LOG(("xdragasprite_start: 0x%x: %s",
					error->errnum, error->errmess));
		return true;
	}


	return false;
}


/**
 * Handles a menu closed event
 *
 * \param tree  the tree to handle the event for
 */
void ro_gui_tree_menu_closed(struct tree *tree) {
	assert(tree);

	if (tree->temp_selection) {
		tree->temp_selection->selected = false;
		tree_handle_node_element_changed(tree, &tree->temp_selection->data);
		tree->temp_selection = NULL;
	}
}


/**
 * Respond to a mouse click for a tree (hotlist or history) toolbar
 *
 * \param pointer  the pointer state
 */
void ro_gui_tree_toolbar_click(wimp_pointer* pointer, struct tree *tree) {
	struct node *node;
	bool refresh = true;

	current_toolbar = tree->toolbar;
	ro_gui_tree_stop_edit(tree);

	if (pointer->buttons == wimp_CLICK_MENU) {
		ro_gui_create_menu(toolbar_menu, pointer->pos.x,
				pointer->pos.y, NULL);
		return;
	}

	if (current_toolbar->editor) {
		ro_gui_theme_toolbar_editor_click(tree->toolbar, pointer);
		return; 
	}

	switch (pointer->i) {
	  	case ICON_TOOLBAR_CREATE:
			node = tree_create_folder_node(tree->root,
					messages_get("TreeNewFolder"));
			tree_redraw_area(tree, node->box.x - NODE_INSTEP,
					0, NODE_INSTEP, 16384);
			tree_handle_node_changed(tree, node, false, true);
			ro_gui_tree_start_edit(tree, &node->data, NULL);
	  		break;
	  	case ICON_TOOLBAR_OPEN:
			tree_handle_expansion(tree, tree->root,
					(pointer->buttons == wimp_CLICK_SELECT),
					true, false);
			break;
	  	case ICON_TOOLBAR_EXPAND:
			tree_handle_expansion(tree, tree->root,
					(pointer->buttons == wimp_CLICK_SELECT),
					false, true);
			break;
		case ICON_TOOLBAR_DELETE:
			tree_delete_selected_nodes(tree,
					tree->root);
			break;
		case ICON_TOOLBAR_LAUNCH:
			ro_gui_tree_launch_selected(tree);
			break;
	}
	switch (tree->toolbar->type) {
		case THEME_HOTLIST_TOOLBAR:
			ro_gui_menu_prepare_hotlist();
			break; 
		case THEME_HISTORY_TOOLBAR:
			ro_gui_menu_prepare_global_history();
			break; 	  
		default:
			break;
	}
}


/**
 * Starts an editing session
 *
 * \param tree     the tree to start editing for
 * \param element  the element to edit
 * \param pointer  the pointer data to use for caret positioning (or NULL)
 */
void ro_gui_tree_start_edit(struct tree *tree, struct node_element *element,
		wimp_pointer *pointer) {
	os_error *error;
	wimp_window_state state;
	struct node *parent;
	int toolbar_height = 0;

	assert(tree);
	assert(element);

	if (tree->editing)
		ro_gui_tree_stop_edit(tree);
	if (tree->toolbar)
		toolbar_height = ro_gui_theme_toolbar_height(tree->toolbar);

	parent = element->parent;
	if (&parent->data == element)
		parent = parent->parent;
	for (; parent; parent = parent->parent) {
		if (!parent->expanded) {
		  	parent->expanded = true;
		  	tree_handle_node_changed(tree, parent, false, true);
		}
	}

	tree->editing = element;
	snprintf(tree->edit_buffer, 256, element->text);
	tree->edit_buffer[255] = '\0';
	ro_gui_tree_edit_icon.w = (wimp_w)tree->handle;
	ro_gui_tree_edit_icon.icon.extent.x0 = tree->offset_x + element->box.x - 2;
	ro_gui_tree_edit_icon.icon.extent.x1 = tree->offset_x +
			element->box.x + element->box.width + 2;
	ro_gui_tree_edit_icon.icon.extent.y1 = -tree->offset_y - toolbar_height -
			element->box.y;
	ro_gui_tree_edit_icon.icon.extent.y0 = -tree->offset_y - toolbar_height -
			element->box.y - element->box.height;
	if (element->type == NODE_ELEMENT_TEXT_PLUS_SPRITE)
		ro_gui_tree_edit_icon.icon.extent.x0 += NODE_INSTEP;
	ro_gui_tree_edit_icon.icon.data.indirected_text.text = tree->edit_buffer;
	error = xwimp_create_icon(&ro_gui_tree_edit_icon,
			(wimp_i *)&tree->edit_handle);
	if (error)
		LOG(("xwimp_create_icon: 0x%x: %s",
				error->errnum, error->errmess));
	if (pointer) {
		state.w = (wimp_w)tree->handle;
		error = xwimp_get_window_state(&state);
		if (error)
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
		error = xwimp_set_caret_position((wimp_w)tree->handle,
				(wimp_i)tree->edit_handle,
				pointer->pos.x - state.visible.x0, 0,
				element->box.height, -1);
	} else {
		error = xwimp_set_caret_position((wimp_w)tree->handle,
				(wimp_i)tree->edit_handle,
				0, 0, -1, strlen(tree->edit_buffer));
	}
	if (error)
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
	tree_handle_node_element_changed(tree, element);
	ro_gui_tree_scroll_visible(tree, element);
}


/**
 * Stops any current editing session
 *
 * \param tree  the tree to stop editing for
 */
void ro_gui_tree_stop_edit(struct tree *tree) {
	os_error *error;

	assert(tree);

	if (!tree->editing) return;

	error = xwimp_delete_icon((wimp_w)tree->handle, (wimp_i)tree->edit_handle);
	if (error)
		LOG(("xwimp_delete_icon: 0x%x: %s",
				error->errnum, error->errmess));
	tree_handle_node_element_changed(tree, tree->editing);
	tree->editing = NULL;

	error = xwimp_set_caret_position((wimp_w)tree->handle, -1, -100,
			-100, 32, -1);
	if (error)
		LOG(("xwimp_set_caret_position: 0x%x: %s",
			error->errnum, error->errmess));
	tree_recalculate_size(tree);
}


/**
 * Scrolls the tree to make an element visible
 *
 * \param tree     the tree to scroll
 * \param element  the element to display
 */
void ro_gui_tree_scroll_visible(struct tree *tree, struct node_element *element) {
	wimp_window_state state;
	int x0, x1, y0, y1;
	os_error *error;
	int toolbar_height = 0;

	assert(element);

	if (tree->toolbar)
		toolbar_height = ro_gui_theme_toolbar_height(tree->toolbar);

	state.w = (wimp_w)tree->handle;
	error = xwimp_get_window_state(&state);
	if (error)
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
	if (!(state.flags & wimp_WINDOW_OPEN))
		return;
	x0 = state.xscroll;
	y0 = -state.yscroll;
	x1 = x0 + state.visible.x1 - state.visible.x0 - tree->offset_x;
	y1 = y0 - state.visible.y0 + state.visible.y1 - tree->offset_y - toolbar_height;

	state.yscroll = state.visible.y1 - state.visible.y0 - tree->offset_y -
			toolbar_height - y1;
	if ((element->box.y >= y0) && (element->box.y + element->box.height <= y1))
		return;
	if (element->box.y < y0)
		state.yscroll = -element->box.y;
	if (element->box.y + element->box.height > y1)
		state.yscroll = state.visible.y1 - state.visible.y0 -
				tree->offset_y - toolbar_height -
				(element->box.y + element->box.height);
	ro_gui_tree_open((wimp_open *)&state, tree);
}


/**
 * Shows the a tree window.
 */
void ro_gui_tree_show(struct tree *tree) {
	os_error *error;
	int screen_width, screen_height;
	wimp_window_state state;
	int dimension;
	int scroll_width;
	struct toolbar *toolbar;

	/*	We may have failed to initialise
	*/
	if (!tree) return;
	toolbar = tree->toolbar;

	/*	Get the window state
	*/
	state.w = (wimp_w)tree->handle;
	error = xwimp_get_window_state(&state);
	if (error) {
		warn_user("WimpError", error->errmess);
		return;
	}

	/*	If we're open we jump to the top of the stack, if not then we
		open in the centre of the screen.
	*/
	if (!(state.flags & wimp_WINDOW_OPEN)) {
	  
	  	/*	Cancel any editing
	  	*/
	  	if ((tree->toolbar) && (tree->toolbar->editor))
	  		ro_gui_theme_toggle_edit(tree->toolbar);
	  	ro_gui_tree_stop_edit(tree);
	  
	 	/*	Set the default state
	 	*/
	 	if (tree->root->child)
	 		tree_handle_node_changed(tree, tree->root,
					false, true);

		/*	Get the current screen size
		*/
		ro_gui_screen_size(&screen_width, &screen_height);

		/*	Move to the centre
		*/
		dimension = 600; /*state.visible.x1 - state.visible.x0;*/
		scroll_width = ro_get_vscroll_width((wimp_w)tree->handle);
		state.visible.x0 = (screen_width - (dimension + scroll_width)) / 2;
		state.visible.x1 = state.visible.x0 + dimension;
		dimension = 800; /*state.visible.y1 - state.visible.y0;*/
		state.visible.y0 = (screen_height - dimension) / 2;
		state.visible.y1 = state.visible.y0 + dimension;
		state.xscroll = 0;
		state.yscroll = 0;
		if (toolbar)
			state.yscroll = ro_gui_theme_toolbar_height(toolbar);
	}

	/*	Open the window at the top of the stack
	*/
	state.next = wimp_TOP;
	ro_gui_tree_open((wimp_open*)&state, tree);

	/*	Set the caret position
	*/
	xwimp_set_caret_position(state.w, -1, -100, -100, 32, -1);
}


/**
 * Handles a window open request
 *
 * \param open  the window state
 * \param tree  the tree to handle a request for
 */
void ro_gui_tree_open(wimp_open *open, struct tree *tree) {
	os_error *error;
	int width;
	int height;
	int toolbar_height = 0;
	
	if (!tree)
		return;
	if (tree->toolbar)
		toolbar_height = ro_gui_theme_toolbar_height(tree->toolbar);

	width = open->visible.x1 - open->visible.x0;
	if (width < (tree->offset_x + tree->width))
		width = tree->offset_x + tree->width;
	height = open->visible.y1 - open->visible.y0;
	if (height < (tree->offset_y + toolbar_height + tree->height))
		height = tree->offset_y + toolbar_height + tree->height;

	if ((height != tree->window_height) || (width != tree->window_width)) {
		os_box extent = { 0, -height, width, 0};
		error = xwimp_set_extent((wimp_w)tree->handle, &extent);
		if (error) {
			LOG(("xwimp_set_extent: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		tree->window_width = width;
		tree->window_height = height;
	}

	error = xwimp_open_window(open);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	if (tree->toolbar)
		ro_gui_theme_process_toolbar(tree->toolbar, -1);

}


/**
 * Handles a keypress for a tree
 *
 * \param key   the key pressed
 * \param tree  the tree to handle a keypress for
 * \return whether the key was processed
 */
bool ro_gui_tree_keypress(int key, struct tree *tree) {
  	os_error *error;
	char *new_string;

	/*	Handle basic keys
	*/
	switch (key) {
		case 1:		/* CTRL+A */
			ro_gui_tree_stop_edit(tree);
			if (tree->root->child) {
				tree->temp_selection = NULL;
				tree_set_node_selected(tree, tree->root, true);
			}
			return true;
		case 24:	/* CTRL+X */
			ro_gui_tree_stop_edit(tree);
			tree_delete_selected_nodes(tree, tree->root);
			return true;
		case 26:	/* CTRL+Z */
			tree->temp_selection = NULL;
			ro_gui_tree_stop_edit(tree);
			tree_set_node_selected(tree, tree->root, false);
			return true;
		case wimp_KEY_RETURN:
			if (tree->editing) {
				new_string = strdup(tree->edit_buffer);
				if (new_string) {
					if (tree->editing->text) {
					  	free(tree->editing->text);
					  	tree->editing->text = NULL;
					}
					tree->editing->text = new_string;
				}
				ro_gui_tree_stop_edit(tree);
				tree_recalculate_size(tree);
			} else {
				ro_gui_tree_launch_selected(tree);
			}
			return true;
		case wimp_KEY_CONTROL + wimp_KEY_F2:
			error = xwimp_close_window((wimp_w)tree->handle);
			if (error)
				LOG(("xwimp_close_window: 0x%x: %s",
						error->errnum, error->errmess));
			return true;
		case wimp_KEY_ESCAPE:
			if (tree->editing) {
				ro_gui_tree_stop_edit(tree);
			} else {
				/* \todo cancel drags etc. */
			}
	}
	return false;
}


/**
 * Handles the completion of a selection drag (GUI_DRAG_TREE_SELECT)
 *
 * \param drag  the drag box information
 */
void ro_gui_tree_selection_drag_end(wimp_dragged *drag) {
	wimp_window_state state;
	wimp_auto_scroll_info scroll;
	os_error *error;
	int x0, y0, x1, y1;
	int toolbar_height = 0;
	
	if (ro_gui_tree_current_drag_tree->toolbar)
		toolbar_height = ro_gui_theme_toolbar_height(
				ro_gui_tree_current_drag_tree->toolbar);

	scroll.w = (wimp_w)ro_gui_tree_current_drag_tree->handle;
	error = xwimp_auto_scroll(0, &scroll, 0);
	if (error)
		LOG(("xwimp_auto_scroll: 0x%x: %s", error->errnum, error->errmess));

	state.w = (wimp_w)ro_gui_tree_current_drag_tree->handle;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	x0 = drag->final.x0 - state.visible.x0 - state.xscroll +
			ro_gui_tree_current_drag_tree->offset_x;
	y0 = state.visible.y1 - state.yscroll - drag->final.y0 -
			ro_gui_tree_current_drag_tree->offset_y - toolbar_height;
	x1 = drag->final.x1 - state.visible.x0 - state.xscroll +
			ro_gui_tree_current_drag_tree->offset_x;
	y1 = state.visible.y1 - state.yscroll - drag->final.y1 -
			ro_gui_tree_current_drag_tree->offset_y - toolbar_height;
	tree_handle_selection_area(ro_gui_tree_current_drag_tree, x0, y0,
		x1 - x0, y1 - y0,
		(ro_gui_tree_current_drag_buttons == (wimp_CLICK_ADJUST << 4)));

	/* send an empty keypress to stimulate the tree owner to update the GUI.
	   for this to work, we must always own the caret when this function is
	   called. */
	error = xwimp_process_key(0);
	if (error)
		LOG(("xwimp_process_key: 0x%x: %s",
				error->errnum, error->errmess));
}


/**
 * Converts screen co-ordinates to tree ones
 *
 * \param tree    the tree to calculate for
 * \param x       the screen x co-ordinate
 * \param x       the screen y co-ordinate
 * \param tree_x  updated to the tree x co-ordinate
 * \param tree_y  updated to the tree y co-ordinate
 */
void ro_gui_tree_get_tree_coordinates(struct tree *tree, int x, int y,
		int *tree_x, int *tree_y) {
	wimp_window_state state;
	os_error *error;

	state.w = (wimp_w)tree->handle;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	*tree_x = x - state.visible.x0 - state.xscroll + tree->offset_x;
	*tree_y = state.visible.y1 - state.yscroll - y - tree->offset_y;
	if (tree->toolbar)
		*tree_y -= ro_gui_theme_toolbar_height(tree->toolbar);
}


/**
 * Handles the completion of a move drag (GUI_DRAG_TREE_MOVE)
 *
 * \param drag  the drag box information
 */
void ro_gui_tree_move_drag_end(wimp_dragged *drag) {
	struct gui_window *g;
	wimp_pointer pointer;
	wimp_auto_scroll_info scroll;
	os_error *error;
	struct node *node;
	struct node *single;
	struct node_element *element;
	bool before;
	int x, y;

	scroll.w = (wimp_w)ro_gui_tree_current_drag_tree->handle;
	error = xwimp_auto_scroll(0, &scroll, 0);
	if (error)
		LOG(("xwimp_auto_scroll: 0x%x: %s", error->errnum, error->errmess));

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s", error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (pointer.w != (wimp_w)ro_gui_tree_current_drag_tree->handle) {
	  	/* try to drop into a browser window */
		single = tree_get_selected_node(ro_gui_tree_current_drag_tree->root->child);
	  	element = tree_find_element(single, TREE_ELEMENT_URL);
		if ((single) && (element)) {
			g = ro_gui_window_lookup(pointer.w);
			if (g)
				browser_window_go(g->bw, element->text, 0);
			return;

		}
		/* todo: handle export */
		return;
	}

	/* internal drag */
	if (!ro_gui_tree_current_drag_tree->movable)
		return;
	ro_gui_tree_get_tree_coordinates(ro_gui_tree_current_drag_tree,
			drag->final.x0 + 34, drag->final.y0 + 34, &x, &y);
	node = tree_get_link_details(ro_gui_tree_current_drag_tree, x, y, &before);
	tree_move_selected_nodes(ro_gui_tree_current_drag_tree, node, before);
}


/**
 * Launches all selected nodes.
 *
 * \param tree  the tree to launch all selected nodes for
 */
void ro_gui_tree_launch_selected(struct tree *tree) {
	assert(tree);

  	if (tree->root->child)
		ro_gui_tree_launch_selected_node(tree->root->child, false);
}


/**
 * Launches all selected nodes.
 *
 * \param node  the node to launch all selected nodes for
 */
void ro_gui_tree_launch_selected_node(struct node *node, bool all) {
	for (; node; node = node->next) {
		if (((node->selected) || (all)) && (!node->folder))
			ro_gui_tree_launch_node(node);
		if ((node->child) && ((node->expanded) || (node->selected) | (all)))
			ro_gui_tree_launch_selected_node(node->child,
				(node->selected) | (all));
	}
}


/**
 * Launches a node using all known methods.
 *
 * \param node  the node to launch
 * \return whether the node could be launched
 */
bool ro_gui_tree_launch_node(struct node *node) {
	struct node_element *element;

	assert(node);

	element = tree_find_element(node, TREE_ELEMENT_URL);
	if (element) {
		browser_window_create(element->text, NULL, 0);
		return true;
	}

	return false;
}
