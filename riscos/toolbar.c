/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Customisable toolbars (implementation).
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/toolbar.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


struct toolbar_icon {
	/*	The desired WIMP icon number (-1 for separator)
	*/
	int icon_number;

	/*	Set to non-zero to display the icon
	*/
  	unsigned int available;

  	/*	Icon dimensions (OS units)
  	*/
	unsigned int width;
	unsigned int height;

	/*	Icon validation string
	*/
	char validation[40];

	/*	The next icon (linked list)
	*/
	struct toolbar_icon *next_icon;	// Next toolbar icon
};


/*	A basic window for the toolbar and status
*/
static wimp_window empty_window = {
	{0, 0, 16384, 16384},
	0,
	0,
	wimp_TOP,
	wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_MOVEABLE | wimp_WINDOW_AUTO_REDRAW | wimp_WINDOW_FURNITURE_WINDOW,
	wimp_COLOUR_BLACK,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_VERY_LIGHT_GREY,
	wimp_COLOUR_DARK_GREY,
	wimp_COLOUR_MID_LIGHT_GREY,
	wimp_COLOUR_CREAM,
	wimp_WINDOW_NEVER3D | 0x16u /* RISC OS 5.03+ - greyed icons detected for interactive help */,
	{0, 0, 16384, 16384},
	0,
	0,
	wimpspriteop_AREA,
	12,
	1,
	{""},
	0
};

/*	Holder for quick icon creation
*/
static wimp_icon_create empty_icon;

/*	Shared URL validation
*/
static char url_validation[] = "Pptr_write\0";
static char resize_validation[] = "R1;Pptr_lr,8,6\0";
static char null_text_string[] = "\0";


static struct toolbar *ro_toolbar_create_icons(struct toolbar *toolbar, osspriteop_area *sprite_area,
							char *url_buffer, char *throbber_buffer);
static struct toolbar_icon *ro_toolbar_initialise_icon(osspriteop_area *sprite_area,
		const char *sprite, unsigned int icon);
static struct toolbar_icon *ro_toolbar_create_separator(void);
static void ro_toolbar_destroy_icon(struct toolbar_icon *icon);
static void ro_toolbar_add_icon(struct toolbar *toolbar, struct toolbar_icon *icon);




/**
 * Creates a toolbar with a complete set of icons
 *
 * \param  sprite_area  the sprite area to read from
 */
struct toolbar *ro_toolbar_create(osspriteop_area *sprite_area, char *url_buffer,
						char *status_buffer, char *throbber_buffer,
						int toolbar_type) {
	struct toolbar *toolbar;
	wimp_i icon_handle;

	/*	Create a new toolbar
	*/
	toolbar = calloc(1, sizeof(struct toolbar));
	if (!toolbar) return NULL;
	toolbar->update_pending = true;
	toolbar->standard_buttons = true;
	toolbar->url_bar = (toolbar_type == TOOLBAR_BROWSER);
	toolbar->throbber = (toolbar_type == TOOLBAR_BROWSER);
	toolbar->status_window = (toolbar_type == TOOLBAR_BROWSER);
	toolbar->status_old_width = 0xffffffff;
	toolbar->type = toolbar_type;

	/*	Load the toolbar icons
	*/
	if ((sprite_area) && (toolbar_type == TOOLBAR_BROWSER)) {
		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "back", ICON_TOOLBAR_BACK));
		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "forward", ICON_TOOLBAR_FORWARD));
		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "stop", ICON_TOOLBAR_STOP));
		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "reload", ICON_TOOLBAR_RELOAD));
		ro_toolbar_add_icon(toolbar, ro_toolbar_create_separator());
/* 		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "home", ICON_TOOLBAR_HOME)); */
/* 		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "up", ICON_TOOLBAR_UP)); */
/* 		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "search", ICON_TOOLBAR_SEARCH)); */
		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "history", ICON_TOOLBAR_HISTORY));
		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "scale", ICON_TOOLBAR_SCALE));
		ro_toolbar_add_icon(toolbar, ro_toolbar_create_separator());
		if (hotlist_window) {
 			ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "mark", ICON_TOOLBAR_BOOKMARK));
 		}
		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "save", ICON_TOOLBAR_SAVE));
/* 		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "print", ICON_TOOLBAR_PRINT)); */
	} else if ((sprite_area) && (toolbar_type == TOOLBAR_HOTLIST)) {
		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "create", ICON_TOOLBAR_CREATE));
		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "delete", ICON_TOOLBAR_DELETE));
		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "launch", ICON_TOOLBAR_LAUNCH));
		ro_toolbar_add_icon(toolbar, ro_toolbar_create_separator());
		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "open", ICON_TOOLBAR_OPEN));
		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "expand", ICON_TOOLBAR_EXPAND));
		ro_toolbar_add_icon(toolbar, ro_toolbar_create_separator());
		ro_toolbar_add_icon(toolbar, ro_toolbar_initialise_icon(sprite_area, "sort", ICON_TOOLBAR_SORT));
		ro_toolbar_add_icon(toolbar, ro_toolbar_create_separator());
	}

	/*	Set the sprite area
	*/
	if (sprite_area) {
		empty_window.sprite_area = sprite_area;
	} else {
		empty_window.sprite_area = (osspriteop_area *) 1;
	}

	/*	Create the basic windows
	*/
	if (toolbar_type == TOOLBAR_BROWSER) {
		empty_window.ymin = 36;
		if (xwimp_create_window(&empty_window, &toolbar->status_handle)) {
			ro_toolbar_destroy(toolbar);
			return NULL;
		}
	}
	empty_window.ymin = 1;
	if (xwimp_create_window(&empty_window, &toolbar->toolbar_handle)) {
		ro_toolbar_destroy(toolbar);
		return NULL;
	}

	/*	Create the status window icons. First the status text
	*/
	if (toolbar->status_handle) {
		empty_icon.w = toolbar->status_handle;
		empty_icon.icon.extent.x0 = 0;
		empty_icon.icon.extent.y0 = 0;
		empty_icon.icon.extent.x1 = 16384;
		empty_icon.icon.extent.y1 = 36;
		empty_icon.icon.flags = wimp_ICON_TEXT | (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
				wimp_ICON_INDIRECTED | wimp_ICON_VCENTRED;
		empty_icon.icon.data.indirected_text.text = status_buffer;
		empty_icon.icon.data.indirected_text.validation = 0;
		empty_icon.icon.data.indirected_text.size = 256;
		if (xwimp_create_icon(&empty_icon, &icon_handle)) {
			ro_toolbar_destroy(toolbar);
			return NULL;
		}

		/*	And finally the status resize icon
		*/
		empty_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
				wimp_ICON_BORDER | wimp_ICON_FILLED |
				(wimp_COLOUR_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT) |
				(wimp_BUTTON_CLICK_DRAG << wimp_ICON_BUTTON_TYPE_SHIFT);
		empty_icon.icon.extent.x1 = 0;
		empty_icon.icon.data.indirected_text.text = null_text_string;
		empty_icon.icon.data.indirected_text.validation = resize_validation;
		empty_icon.icon.data.indirected_text.size = 1;
		if (xwimp_create_icon(&empty_icon, &icon_handle)) {
			ro_toolbar_destroy(toolbar);
			return NULL;
		}
	}

	/*	Create the icons
	*/
	toolbar = ro_toolbar_create_icons(toolbar, sprite_area, url_buffer, throbber_buffer);

	/*	Return the toolbar
	*/
	return toolbar;
}


/**
 * Creates a WIMP icons for the toolbar
 *
 * \param  toolbar      the toolbar to build from
 * \param  sprite_area  the sprite area to plot sprites from
 */
static struct toolbar *ro_toolbar_create_icons(struct toolbar *toolbar, osspriteop_area *sprite_area,
							char *url_buffer, char *throbber_buffer) {
	int index;
	struct toolbar_icon *cur_icon;
	wimp_i icon_handle;
	int max_icon;

	/*	Set the basic icon flags
	*/
	empty_icon.w = toolbar->toolbar_handle;
	empty_icon.icon.extent.x0 = 0;
	empty_icon.icon.extent.y0 = 0;
	empty_icon.icon.extent.x1 = 0;
	empty_icon.icon.extent.y1 = 0;
	empty_icon.icon.data.indirected_text.text = null_text_string;
	empty_icon.icon.data.indirected_text.size = 1;
	empty_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_SPRITE | wimp_ICON_INDIRECTED |
					wimp_ICON_HCENTRED | wimp_ICON_VCENTRED |
					(wimp_BUTTON_CLICK << wimp_ICON_BUTTON_TYPE_SHIFT);

	/*	Create all the required icons
	*/
	max_icon = ICON_TOOLBAR_URL;
	if (toolbar->type == TOOLBAR_HOTLIST) max_icon = ICON_TOOLBAR_HOTLIST_LAST;
	for (index = 0; index < max_icon; index++) {

		/*	Find an icon with the correct index and get the validation
		*/
		empty_icon.icon.data.indirected_text.validation = 0;
		cur_icon = toolbar->icon;
		while (cur_icon) {
			if (cur_icon->icon_number == index) {
				empty_icon.icon.data.indirected_text.validation = cur_icon->validation;
				cur_icon = NULL;
			} else {
				cur_icon = cur_icon->next_icon;
			}
		}

		/*	Create the icon and destroy the toolbar on failure
		*/
		if (xwimp_create_icon(&empty_icon, &icon_handle)) {
			ro_toolbar_destroy(toolbar);
			return NULL;
		}
	}

	/*	Now the URL icon
	*/
	if (toolbar->type == TOOLBAR_BROWSER) {
		empty_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_VCENTRED |
					wimp_ICON_BORDER | wimp_ICON_FILLED |
					(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
					(wimp_BUTTON_WRITE_CLICK_DRAG << wimp_ICON_BUTTON_TYPE_SHIFT);
		empty_icon.icon.data.indirected_text.text = url_buffer;
		empty_icon.icon.data.indirected_text.validation = url_validation;
		empty_icon.icon.data.indirected_text.size = 256;
		if (xwimp_create_icon(&empty_icon, &icon_handle)) {
			ro_toolbar_destroy(toolbar);
			return NULL;
		}

		/*	Now the throbber
		*/
		empty_icon.icon.flags = wimp_ICON_SPRITE | wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED |
						wimp_ICON_VCENTRED;
		empty_icon.icon.data.indirected_sprite.id = (osspriteop_id)throbber_buffer;
		if (sprite_area) {
			empty_icon.icon.data.indirected_sprite.area = sprite_area;
		} else {
			empty_icon.icon.data.indirected_sprite.area = (osspriteop_area *) 1;
		}
		empty_icon.icon.data.indirected_sprite.size = 12;
		if (xwimp_create_icon(&empty_icon, &icon_handle)) {
			ro_toolbar_destroy(toolbar);
			return NULL;
		}
	}

	/*	And finally the status resize icon
	*/
	if (toolbar->status_handle) {
		empty_icon.w = toolbar->status_handle;
		empty_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
					wimp_ICON_BORDER | wimp_ICON_FILLED |
					(wimp_COLOUR_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT) |
					(wimp_BUTTON_CLICK_DRAG << wimp_ICON_BUTTON_TYPE_SHIFT);
		empty_icon.icon.data.indirected_text.text = null_text_string;
		empty_icon.icon.data.indirected_text.validation = resize_validation;
		empty_icon.icon.data.indirected_text.size = 1;
		if (xwimp_create_icon(&empty_icon, &icon_handle)) {
			ro_toolbar_destroy(toolbar);
			return NULL;
		}
	}

	/*	Success - return what we had
	*/
	return toolbar;

}


/**
 * Releases all icons and associated memory for a toolbar
 *
 * \param  toolbar  the toolbar to destroy
 */
void ro_toolbar_destroy(struct toolbar *toolbar) {
	struct toolbar_icon *cur_icon;
	struct toolbar_icon *next_icon;

	/*	Paranoia
	*/
	if (toolbar == NULL) return;

	/*	Free all our icons
	*/
	next_icon = toolbar->icon;
	while((cur_icon = next_icon) != NULL) {
		next_icon = cur_icon->next_icon;
		ro_toolbar_destroy_icon(cur_icon);
	}

	/*	Destroy our windows
	*/
	if (toolbar->status_handle) xwimp_delete_window(toolbar->status_handle);
	if (toolbar->toolbar_handle) xwimp_delete_window(toolbar->toolbar_handle);

	/*	Destroy ourself
	*/
	free(toolbar);
}


/**
 * Creates a toolbar icon
 *
 * \param  sprite_area  the sprite area to read from
 * \param  sprite	the requested sprite
 * \param  icon		the icon number
 */
struct toolbar_icon *ro_toolbar_initialise_icon(osspriteop_area *sprite_area,
		const char *sprite, unsigned int icon) {
	struct toolbar_icon *current_icon;
	os_coord dimensions;
	char name[16];
	osbool mask;
	os_mode mode;
	os_error *error;

	strcpy(name, sprite);

	/*	Get the sprite details
	*/
	error = xosspriteop_read_sprite_info(osspriteop_USER_AREA,
			sprite_area, (osspriteop_id) name,
			&dimensions.x, &dimensions.y, &mask, &mode);
	if (error && error->errnum == error_SPRITE_OP_DOESNT_EXIST) {
		/** \todo  inform user */
		return NULL;
	} else if (error) {
		LOG(("xosspriteop_read_sprite_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("TbarError", error->errmess);
		return NULL;
	}

	/*	Create an icon
	*/
	current_icon = (struct toolbar_icon *)calloc(1, sizeof(struct toolbar_icon));
	if (!current_icon) return NULL;

	/*	Get the validation buffer for 'R5;S<name>,p<name>\0'. We always assume
		there is a pushed variant as RISC OS happily ignores it if it doesn't
		exist.
	*/
	sprintf(current_icon->validation, "R5;S%s,p%s", name, name);

	/*	We want eig factors rather than pixels
	*/
	ro_convert_pixels_to_os_units(&dimensions, mode);
	current_icon->width = dimensions.x;
	current_icon->height = dimensions.y;
	current_icon->icon_number = icon;
	current_icon->available = true;

	/*	Return our structure
	*/
	return current_icon;
}


/**
 * Creates a toolbar separator icon
 *
 */
static struct toolbar_icon *ro_toolbar_create_separator(void) {
	struct toolbar_icon *current_icon;

	/*	Create an icon
	*/
	current_icon = (struct toolbar_icon *)calloc(1, sizeof(struct toolbar_icon));
	if (!current_icon) return NULL;

	/*	Set it as a 8 OS unit separator
	*/
	current_icon->icon_number = -1;
	current_icon->available = true;
	current_icon->width = 16;

	/*	Return our structure
	*/
	return current_icon;
}


/**
 * Removes all associated memory with a toolbar icon
 *
 * \param  icon	    the icon to destroy
 */
static void ro_toolbar_destroy_icon(struct toolbar_icon *icon) {
	free(icon);
}


/**
 * Adds a toolbar icon to the toolbar
 *
 * \param  toolbar  the toolbar to add to
 * \param  icon	    the icon to add
 */
static void ro_toolbar_add_icon(struct toolbar *toolbar, struct toolbar_icon *icon) {
	struct toolbar_icon *cur_icon;

	/*	If we've been given a NULL due to a failure to create a toolbar
		icon then we barf.
	*/
	if (icon == NULL) return;

	/*	Traverse to the end of our linked list.
	*/
	cur_icon = toolbar->icon;
	if (!cur_icon) {

		/*	First icon cannot be a separator. Well, it can, but it's very unlikely
			that this has arisen from anything other than the previous icons not
			being present
		*/
		if (icon->icon_number < 0) return;
		toolbar->icon = icon;
	} else {
		while (cur_icon->next_icon) cur_icon = cur_icon->next_icon;

		/*	Two separators should not follow each other.
		*/
		if ((cur_icon->icon_number < 0) && (icon->icon_number < 0)) return;
		cur_icon->next_icon = icon;
	}

	/*	Stop potential circular linking
	*/
	icon->next_icon = NULL;
}


/**
 * Resizes the status bar height (toolsprites change)
 *
 * \param  toolbar  the toolbar to update
 * \param  height   the new status bar height
 */
void ro_toolbar_resize_status(struct toolbar *toolbar, int height) {
	os_box extent = { 0, 0, 16384, (height - 2) };
	wimp_WINDOW_INFO(3) status_definition;	// Barfs if 2 is used!?!?!
	wimp_window *status_window;

	/*	Paranoia
	*/
	if ((toolbar == NULL) || (toolbar->status_handle == NULL)) return;

	/*	Check if we need to update
	*/
	if (toolbar->status_height == height) return;
	toolbar->status_height = height;

	/*	Get the window info
	*/
	status_definition.w = toolbar->status_handle;
	if (xwimp_get_window_info((wimp_window_info *)&status_definition)) {
		return;
        }

	/*	Modify the window
	*/
	status_window = (wimp_window *)((char *)(&status_definition) + 4);
	status_window->ymin = height - 2;
	status_window->visible.y1 = height - 2;
	status_window->extent.y1 = height - 2;

	/*	Recreate the window
	*/
	xwimp_delete_window(toolbar->status_handle);
	xwimp_create_window(status_window, &toolbar->status_handle);

	/*	Set a big extent (it'll automatically be updated later to
		the correct value
	*/
	xwimp_set_extent(toolbar->status_handle, &extent);
}


/**
 * Reformat the contents of the toolbar/status window
 *
 * \param  toolbar  the toolbar to update
 * \param  width    the new toolbar width
 * \return non-zero if the toolbar height has changed
 */
int ro_toolbar_reformat(struct toolbar *toolbar, int width) {
  	wimp_caret caret;
	unsigned int right_margin = 8;
	int min_width = 0;
	int status_width = 0;
	int old_width;

	/*	Paranoia
	*/
	if (toolbar == NULL) return 0;

	/*	Check we aren't too small
	*/
	toolbar->width = width;
	if (toolbar->throbber) min_width = toolbar->throbber_width + 8;
	if (toolbar->standard_buttons) min_width += toolbar->icon_width;
	if (toolbar->url_bar) min_width += 64;
	if (width < min_width) width = min_width;

	/*	Check if we need to update the icons
	*/
	if (toolbar->update_pending) {
		toolbar->update_pending = 0;
		toolbar->width_internal = 0xffffffff;
		return ro_toolbar_update(toolbar);
	}

	/*	See if we need to move anything
	*/
	if (width != toolbar->width_internal) {
		toolbar->width_internal = width;

		/*	Move the throbber
		*/
		if ((toolbar->throbber) && (toolbar->throbber_width > 0)) {
			xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_THROBBER,
					width - toolbar->throbber_width - 8,
					(toolbar->height - toolbar->throbber_height) / 2,
					width - 8,
					(toolbar->height + toolbar->throbber_height) / 2);
			right_margin += toolbar->throbber_width + 8;
		}

		/*	Resize the URL bar
		*/
		if (toolbar->url_bar) {
			xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_URL,
					toolbar->icon_width,
					(toolbar->height - 52) / 2,
					width - right_margin,
					(toolbar->height + 52) / 2);

			/*	Handle the caret moving
			*/
			if (!xwimp_get_caret_position(&caret)) {
				if ((caret.w == toolbar->toolbar_handle) && (caret.i == ICON_TOOLBAR_URL)) {
					xwimp_set_caret_position(toolbar->toolbar_handle, ICON_TOOLBAR_URL,
							caret.pos.x, caret.pos.y, -1, caret.index);
				}
			}
		}

		/*	Force a redraw
		*/
		xwimp_force_redraw(toolbar->toolbar_handle, toolbar->icon_width, 0, width, toolbar->height);
	}

	/*	Move the status resize icon
	*/
	if (toolbar->status_window) {
	  	status_width = toolbar->width - toolbar->status_width;
	  	if (status_width < 12) status_width = 12;
	  	old_width = toolbar->status_old_width;
	  	toolbar->status_old_width = status_width;
	  	if (old_width != status_width) {
			xwimp_resize_icon(toolbar->status_handle, ICON_STATUS_TEXT,
					0,
					0,
					status_width - 12,
					toolbar->status_height - 2);
			xwimp_resize_icon(toolbar->status_handle, ICON_STATUS_RESIZE,
					status_width - 12,
					0,
					status_width,
					toolbar->status_height - 2);
			xwimp_force_redraw(toolbar->status_handle,
					status_width - 12, 0, status_width, toolbar->status_height - 2);
			xwimp_force_redraw(toolbar->status_handle,
					old_width - 12, 0, old_width, toolbar->status_height - 2);
		}
	}

	/*	No change in height
	*/
	return 0;
}


/**
 * Updates the icon states and positions.
 *
 * Any necessary redrawing is performed for the client.
 * The client is responsible for resizing/opening/closing the window when necessary.
 *
 * \param  toolbar the toolbar to update
 * \return non-zero if the toolbar height has changed
 */
int ro_toolbar_update(struct toolbar *toolbar) {
  	wimp_caret caret;
	struct toolbar_icon *cur_icon;
	unsigned int toolbar_height = 0;
	unsigned int icon_left = 4;
	int return_status;

	/*	Paranoia
	*/
	if (toolbar == NULL) return 0;

	/*	Calculate the toolbar height (4 os unit border)
	*/
	if (toolbar->url_bar) toolbar_height = 52;
	if ((toolbar->throbber) && (toolbar_height < (toolbar->throbber_height + 4))) {
		toolbar_height = toolbar->throbber_height + 4;
	}

	/*	Calculate the maximum height of the icons
	*/
	if (toolbar->standard_buttons) {
		cur_icon = toolbar->icon;
		while (cur_icon) {
			if ((cur_icon->available) && (toolbar_height < (cur_icon->height + 4))) {
				toolbar_height = cur_icon->height + 4;
			}
			cur_icon = cur_icon->next_icon;
		}
	}

	/*	Set our return status
	*/
	if (toolbar_height != 0) toolbar_height += 8 + 2;
	return_status = (toolbar_height == toolbar->height);
	toolbar->height = toolbar_height;

	/*	Move our icons. Icons that are not avaiable are moved off the visible area.
	*/
	cur_icon = toolbar->icon;
	while (cur_icon) {
		if ((cur_icon->available) && (toolbar->standard_buttons)) {
			if (cur_icon->icon_number >= 0) {
				xwimp_resize_icon(toolbar->toolbar_handle, cur_icon->icon_number,
						icon_left,
						(toolbar_height - cur_icon->height) / 2,
						icon_left + cur_icon->width,
						(toolbar_height + cur_icon->height) / 2);
			}
			icon_left += cur_icon->width;

		} else {
		  	if (cur_icon->icon_number >= 0) {
				xwimp_resize_icon(toolbar->toolbar_handle, cur_icon->icon_number,
						0,
						1024 + toolbar_height,
						cur_icon->width,
						1024 + toolbar_height + cur_icon->height);
			}
		}
		cur_icon = cur_icon->next_icon;
	}

	/*	Make a 8 OS unit spacer between icons and URL bar
	*/
	if (icon_left != 4) icon_left += 8;
	toolbar->icon_width = icon_left;

	/*	Hide the URL bar if we should (and shade it to stop caret issues)
	*/
	if (toolbar->type == TOOLBAR_BROWSER) {
		if (!toolbar->url_bar) {
			/*	Handle losing the caret
			*/
			if (!xwimp_get_caret_position(&caret)) {
				if ((caret.w == toolbar->toolbar_handle) && (caret.i == ICON_TOOLBAR_URL)) {
					xwimp_set_caret_position((wimp_w)-1, 0, 0, 0, 0, 0);
				}
			}
			xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_URL,
					0,
					1024 + toolbar_height,
					64,
					1024 + toolbar_height + 52);
			ro_gui_set_icon_shaded_state(toolbar->toolbar_handle, ICON_TOOLBAR_URL, true);
		} else {
			ro_gui_set_icon_shaded_state(toolbar->toolbar_handle, ICON_TOOLBAR_URL, false);
		}

		/*	Hide the throbber if we should
		*/
		if (!toolbar->throbber) {
			xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_THROBBER,
					0,
					1024 + toolbar_height,
					toolbar->throbber_width,
					1024 + toolbar_height + toolbar->throbber_height);
		}
	}

	/*	Redraw the entire window
	*/
	ro_toolbar_reformat(toolbar, toolbar->width);
	xwimp_force_redraw(toolbar->toolbar_handle, 0, 0, toolbar->width, toolbar_height);

	/*	Update the toolbar height
	*/
	return return_status;
}
