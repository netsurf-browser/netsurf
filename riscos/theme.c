/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 */

/** \file
 * Toolbar themes (implementation).
 */

#include <alloca.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/osgbpb.h"
#include "oslib/osfile.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/toolbar.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/log.h"

/*	Current theme
*/
static struct theme_entry *ro_theme_current = NULL;


/**
 * Apply the current theme
 *
 * /param  theme  the theme to apply
 */
void ro_theme_apply(struct theme_entry *theme) {
#ifdef WITH_KIOSK_THEMES
	char *kioskfilename = alloca(strlen(pathname) + 16);
#endif

	/*	Release any previous theme
	*/
	if (ro_theme_current) ro_theme_free(ro_theme_current);
  
	/*	Set the current theme
	*/
	ro_theme_current = theme;

       /*       Load the window furniture if using Kiosk Themes
        *
        *       Yes I know this is one serious hack!
        *       I'll do something a little more "realistic" when I've
        *       finished various other bits... Right now it works.
        */
#ifdef WITH_KIOSK_THEMES
        sprintf(kioskfilename, "%s.!SetTheme", pathname);
        xos_cli(kioskfilename);
#endif
	
	/* todo: update all current windows */
}


/**
 * Load a theme from a directory.
 *
 * Ideally, the directory should contain a Sprite file and a Text options file.
 * If the path is invalid, or neither of these are present then a default theme
 * is returned with no icons present.
 */

struct theme_entry *ro_theme_load(char *pathname) {
	osbool mask;
	os_mode mode;
 	os_coord dimensions;
	int size, i, n;
	char *filename = alloca(strlen(pathname) + 16);
	fileswitch_object_type obj_type;
	struct theme_entry *theme;
	os_error *error;
	
	/*	Get some memory for the theme
	*/
	theme = (struct theme_entry *)calloc(1, sizeof(struct theme_entry));
	if (!theme) {
	  	LOG(("Failed to claim memory to hold theme."));
		warn_user("NoMemory", 0);
		return NULL;
	}
	theme->default_settings = true;

	/*	Load the sprites
	*/
	sprintf(filename, "%s.Sprites", pathname);
	xosfile_read_no_path(filename, &obj_type, 0, 0, &size, 0);

	/*	Claim memory for a sprite file if we have one
	*/
	if (obj_type & fileswitch_IS_FILE) theme->sprite_area = malloc(size + 16);

	/*	Load the sprite file if we have any memory
	*/
	if (theme->sprite_area) {

	  	/*	Initialise then load
	  	*/
		theme->sprite_area->size = size + 16;
		theme->sprite_area->sprite_count = 0;
		theme->sprite_area->first = 16;
		theme->sprite_area->used = 16;
		xosspriteop_clear_sprites(osspriteop_USER_AREA, theme->sprite_area);
		error = xosspriteop_load_sprite_file(osspriteop_USER_AREA, theme->sprite_area,
				filename);
		if (error) {
			free(theme->sprite_area);
			theme->sprite_area = NULL;
		}
	}
	
	/*	Get the throbber details
	*/
	if (theme->sprite_area) {
	  	/*	We aren't default
	  	*/
	  	theme->default_settings = false;

		/*	Find the highest sprite called 'throbber%i', and get the maximum
			dimensions for all 'thobber%i' icons. We use the filename buffer
			as the temporary spritename buffer as it is guaranteed to be at
			least 12 bytes (max sprite name size).
		*/
		for (i = 1; i <= theme->sprite_area->sprite_count; i++) {
			osspriteop_return_name(osspriteop_USER_AREA,
					theme->sprite_area, filename, 12, i);
			if (strncmp(filename, "throbber", 8) == 0) {
			  	/*	Get the max sprite width/height
			  	*/
				xosspriteop_read_sprite_info(osspriteop_USER_AREA,
					theme->sprite_area, (osspriteop_id)filename,
					&dimensions.x, &dimensions.y, &mask, &mode);
				ro_convert_pixels_to_os_units(&dimensions, mode);
				if (dimensions.x > theme->throbber_width)
						theme->throbber_width = dimensions.x;
				if (dimensions.y > theme->throbber_height)
						theme->throbber_height = dimensions.y;

				/*	Get the throbber number
				*/
				n = atoi(filename + 8);
				if (theme->throbber_frames < n) theme->throbber_frames = n;
			}
		}
	}
	
	/*	Load the options
	*/
	theme->browser_background = wimp_COLOUR_VERY_LIGHT_GREY;
	theme->hotlist_background = wimp_COLOUR_VERY_LIGHT_GREY;
	theme->status_background = wimp_COLOUR_VERY_LIGHT_GREY;
	theme->status_foreground = wimp_COLOUR_BLACK;
	/* todo: impement option loading */
	
	/*	Return our new theme
	*/
	return theme;
}


/**
 * Create a toolbar from the current theme for a browser window.
 *
 * The buffers url_buffer and status_buffer must be at least 256 bytes each,
 * throbber_buffer at least 12 bytes;
 */
void ro_theme_create_browser_toolbar(struct gui_window *g) {
  	struct toolbar *toolbar;

	/*	Destroy any previous toolbar (paranoia)
	*/
	if (g->toolbar) {
		ro_toolbar_destroy(g->toolbar);
		g->toolbar = NULL;
	}
  	/*	Create a toolbar
  	*/
  	toolbar = ro_toolbar_create(ro_theme_current, g->url, g->status,
  			g->throb_buf, TOOLBAR_BROWSER);
  	if (toolbar == NULL) return;

  	/*	Set up the default status width
  	*/
  	toolbar->status_width = 640;

  	/*	Store our toolbar
  	*/
  	g->toolbar = toolbar;

  	/*	Update the toolbar
  	*/
  	ro_theme_update_toolbar(toolbar, g->window);
}


/**
 * Create a toolbar from the current theme for a hotlist window.
 *
 * The buffers url_buffer and status_buffer must be at least 256 bytes each,
 * throbber_buffer at least 12 bytes;
 */
void ro_theme_create_hotlist_toolbar(void) {
  	struct toolbar *toolbar;

	/*	Destroy any previous toolbar (paranoia)
	*/
	if (hotlist_toolbar) {
		ro_toolbar_destroy(hotlist_toolbar);
		hotlist_toolbar = NULL;
	}

  	/*	Create a toolbar
  	*/
  	toolbar = ro_toolbar_create(ro_theme_current, NULL, NULL,
  			NULL, TOOLBAR_HOTLIST);
  	if (toolbar == NULL) return;

  	/*	Store our toolbar
  	*/
  	hotlist_toolbar = toolbar;

  	/*	Update the toolbar
  	*/
  	ro_theme_update_toolbar(toolbar, hotlist_window);
}



/**
 * Updates any toolbar flags (eg closes windows, hides icons etc)
 *
 * \return non-zero if the toolbar height has changed
 */
int ro_theme_update_toolbar(struct toolbar *toolbar, wimp_w window) {
	wimp_outline outline;
	wimp_window_state state;
	int return_value = 0;

	/*	Set an update as pending
	*/
	toolbar->update_pending = true;

  	/*	Close the status window if we should, or resize it
  	*/
  	if (toolbar->status_window) {

	  	/*	Update the status height
  			*/
  		ro_toolbar_resize_status(toolbar, ro_get_hscroll_height(window) - 2);
  	} else {
  	  	xwimp_close_window(toolbar->status_handle);
  	}

	/*	Update the toolbar height
	*/
	return_value = ro_theme_resize_toolbar(toolbar, window);

	/*	Open/close the toolbar
	*/
	if (toolbar->height > 0) {
		outline.w = window;
		xwimp_get_window_outline(&outline);
		state.w = window;
		xwimp_get_window_state(&state);
		state.w = toolbar->toolbar_handle;
		state.visible.x1 = outline.outline.x1 - 2;
		state.visible.y0 = state.visible.y1 - toolbar->height + 2;
		state.xscroll = 0;
		state.yscroll = 0;
		state.next = wimp_TOP;
		xwimp_open_window_nested((wimp_open *)&state, window,
				wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
						<< wimp_CHILD_XORIGIN_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
						<< wimp_CHILD_YORIGIN_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
						<< wimp_CHILD_LS_EDGE_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
						<< wimp_CHILD_BS_EDGE_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
						<< wimp_CHILD_RS_EDGE_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
						<< wimp_CHILD_TS_EDGE_SHIFT);
	} else {
	  	xwimp_close_window(toolbar->toolbar_handle);
	}

	/*	Return
	*/
	return return_value;
}


/**
 * Resize the status and toolbar windows.
 *
 * \return non-zero if the toolbar height has changed
 */
int ro_theme_resize_toolbar(struct toolbar *toolbar, wimp_w window) {
	os_box extent = { 0, 0, 0, 0 };
	wimp_outline outline;
	wimp_outline status_outline;
	wimp_window_state state;
	int width, status_width;
	int return_value = 0;

	/*	Paranoia
	*/
	if (toolbar == NULL) return 0;

	/*	Get the toolbar width
	*/
	outline.w = window;
	if (xwimp_get_window_outline(&outline)) return 0;
	width = outline.outline.x1 - outline.outline.x0 - 2;

	/*	Reformat if we should
	*/
	if ((toolbar->width != width) || (toolbar->resize_status) || (toolbar->update_pending)) {
 	  	if ((toolbar->resize_status) && (toolbar->status_handle)) {
		 	status_outline.w = toolbar->status_handle;
			if (xwimp_get_window_outline(&status_outline)) return 0;
			toolbar->status_width = width -
					(status_outline.outline.x1 - status_outline.outline.x0 - 4);
  	  	  	toolbar->resize_status = 0;
  	  	} else if (toolbar->status_handle) {
  	  	  	/*	Update the extent of the status window
  	  	  	*/
 	  		state.w = window;
 	  		if (xwimp_get_window_state(&state)) return 0;
 	  		extent.x1 = state.visible.x1 - state.visible.x0;
 	  		extent.y1 = toolbar->status_height - 2;
 	  		xwimp_set_extent(toolbar->status_handle, &extent);

		  	/*	Re-open the status window as we can't use the nested
		  		wimp to manage everything as it would keep extending
		  		the size incorrectly.
		  	*/
		  	status_width = width - toolbar->status_width;
		  	if (status_width < 12) status_width = 12;
		  	if (toolbar->status_window) {
		  		state.w = toolbar->status_handle;
				state.xscroll = 0;
				state.yscroll = 0;
				state.next = wimp_TOP;
				state.visible.x0 = outline.outline.x0;
				state.visible.x1 = outline.outline.x0 + status_width;
				state.visible.y0 = outline.outline.y0 - toolbar->status_height;
				state.visible.y1 = outline.outline.y0 - 2;
				xwimp_open_window_nested((wimp_open *) &state, window,
						wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
								<< wimp_CHILD_XORIGIN_SHIFT |
						wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
								<< wimp_CHILD_YORIGIN_SHIFT |
						wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
								<< wimp_CHILD_LS_EDGE_SHIFT |
						wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
								<< wimp_CHILD_BS_EDGE_SHIFT |
						wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
								<< wimp_CHILD_RS_EDGE_SHIFT |
						wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
								<< wimp_CHILD_TS_EDGE_SHIFT);
	  		} else {
	  		 	if (toolbar->update_pending) {
	  		 		xwimp_close_window(toolbar->status_handle);
	  		 	}
	  		}
		}

	  	/*	Resize the toolbar
	  	*/
		return ro_toolbar_reformat(toolbar, width);
	}
	return return_value;
}


/**
 * Make a list of available themes.
 *
 * \return a forwardly link list of available themes
 */

struct theme_entry *ro_theme_list(unsigned int *entries) {
	char pathname[256];
	int context = 0;
	int read_count;
	struct theme_entry *first = NULL;
	struct theme_entry *last = NULL;
	struct theme_entry *theme = NULL;
	osgbpb_INFO(100) info;
	os_error *error;

	*entries = 0;
	while (context != -1) {
		error = xosgbpb_dir_entries_info(THEMES_DIR,
				(osgbpb_info_list *) &info, 1, context,
				sizeof(info), 0, &read_count, &context);
		if (error) {
			LOG(("xosgbpb_dir_entries_info: 0x%x: %s",
				error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
			ro_theme_free(first);
			*entries = 0;
			return NULL;
		}

		if (read_count == 0)
			continue;

		/*	Get our directory name
		*/
		snprintf(pathname, sizeof pathname, "%s.%s",
				THEMES_DIR, info.name);
		pathname[sizeof pathname - 1] = 0;
		
		/*	Load the theme and link it in
		*/
		theme = ro_theme_load(pathname);
		if (theme && !(theme->default_settings)) {
			if (first) {
				last->next = theme;
			} else {
				first = theme;
			}
			last = theme;
			*entries = *entries + 1;

			/*	Copy name. This should be done when loading.
			*/
			theme->name = strdup(info.name);
			if (!theme->name) {
				warn_user("NoMemory", 0);
				ro_theme_free(first);
				*entries = 0;
				return NULL;
			}
		} else {
			if (theme) ro_theme_free(theme);
		}
	}
	return first;
}


/**
 * Free a linked list of themes.
 *
 * \param  theme  the list of themes to free
 */

void ro_theme_free(struct theme_entry *theme) {
  	struct theme_entry *next;
	while (theme) {
		free(theme->name);
		free(theme->author);
		free(theme->sprite_area);
		next = theme->next;
		free(theme);
		theme = next;
	}
}

