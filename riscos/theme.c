/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Window themes and toolbars (implementation).
 */

#include <alloca.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/osgbpb.h"
#include "oslib/osgbpb.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osspriteop.h"
#include "oslib/squash.h"
#include "oslib/wimp.h"
#include "oslib/wimpextend.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


#define THEME_URL_MEMORY 256
#define THEME_THROBBER_MEMORY 12
#define THEME_STATUS_MEMORY 256

struct toolbar_icon {
	int icon_number;			/**< wimp icon number */
	bool display;				/**< whether to display the icon */
	int width;				/**< icon width */
	int height;				/**< icon height */
	char name[12];				/**< icon name */
	char validation[40];			/**< validation string */
	struct toolbar_icon *next;		/**< next toolbar icon, or NULL for no more */
};

struct theme_file_header {
	unsigned int magic_value;
	unsigned int parser_version;
	char name[32];
	char author[64];
	char browser_bg;
	char hotlist_bg;
	char status_bg;
	char status_fg;
	char throbber_left;
	char future_expansion_1;
	char future_expansion_2;
	char future_expansion_3;
	unsigned int compressed_sprite_size;
	unsigned int decompressed_sprite_size;
};


static struct theme_descriptor *theme_current = NULL;
static struct theme_descriptor *theme_descriptors = NULL;

static void ro_gui_theme_free(struct theme_descriptor *descriptor, bool list);
static void ro_gui_theme_add_toolbar_icon(struct toolbar *toolbar, const char *name, int icon_number);
static void ro_gui_theme_update_toolbar_icon(struct toolbar *toolbar, struct toolbar_icon *icon);
static void ro_gui_theme_destroy_toolbar_icon(struct toolbar_icon *icon);


/*	A basic window for the toolbar and status
*/
static wimp_window theme_toolbar_window = {
	{0, 0, 16384, 16384},
	0,
	0,
	wimp_TOP,
	wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_MOVEABLE | wimp_WINDOW_AUTO_REDRAW |
			wimp_WINDOW_FURNITURE_WINDOW,
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
	0/*,
	{ } */
};


/*	Shared icon validation
*/
static char theme_url_validation[] = "Pptr_write\0";
static char theme_resize_validation[] = "R1;Pptr_lr,8,6\0";
static char theme_null_text_string[] = "\0";


/**
 * Initialise the theme handler
 */
void ro_gui_theme_initialise(void) {
	/*	Get an initial theme list
	*/
	theme_descriptors = ro_gui_theme_get_available();
}


/**
 * Finalise the theme handler
 */
void ro_gui_theme_finalise(void) {
	/*	Free all closed descriptors
	*/
	ro_gui_theme_close(theme_current, false);
	ro_gui_theme_free(theme_descriptors, true);
}


/**
 * Finds a theme from the cached values.
 *
 * The returned theme is only guaranteed to be valid until the next call
 * to ro_gui_theme_get_available() unless it has been opened using
 * ro_gui_theme_open().
 *
 * \param filename  the filename of the theme_descriptor to return
 * \return the requested theme_descriptor, or NULL if not found
 */
struct theme_descriptor *ro_gui_theme_find(const char *filename) {
	struct theme_descriptor *descriptor;
	
	/*	Check for bad filename
	*/
	if (!filename) return NULL;
	
	/*	Work through until we find our required filename
	*/
	descriptor = theme_descriptors;
	while (descriptor) {
		if (!strcmp(filename, descriptor->filename)) return descriptor; 
		descriptor = descriptor->next; 
	}
	return NULL;
}


/**
 * Reads and caches the currently available themes.
 *
 * \return the requested theme_descriptor, or NULL if not found
 */
struct theme_descriptor *ro_gui_theme_get_available(void) {
  	struct theme_file_header file_header;
	struct theme_descriptor *current;
	struct theme_descriptor *test;
	char pathname[256];
	int context = 0;
	int read_count;
	osgbpb_INFO(100) info;
	int output_left;
	os_fw file_handle;
	os_error *error;

	/*	Close any descriptors we've got so far
	*/
	ro_gui_theme_free(theme_descriptors, true);

	/*	Create a new set
	*/
	while (context != -1) {
		/*	Get the next entry
		*/
		error = xosgbpb_dir_entries_info(THEMES_DIR,
				(osgbpb_info_list *) &info, 1, context,
				sizeof(info), 0, &read_count, &context);
		if (error) {
			LOG(("xosgbpb_dir_entries_info: 0x%x: %s",
				error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
			break;
		}
		
		/*	Check if we've read anything
		*/
		if (read_count == 0)
			continue;
		
		/*	Only process files
		*/
		if ((info.obj_type == fileswitch_IS_FILE) && (!ro_gui_theme_find(info.name))) {

			/*	Get our full filename
			*/
			snprintf(pathname, sizeof pathname, "%s.%s",
					THEMES_DIR, info.name);
			pathname[sizeof pathname - 1] = 0;
			
			/*	Get the header
			*/
			error = xosfind_openinw(osfind_NO_PATH, pathname, 0, &file_handle);
			if (error) {
				LOG(("xosfind_openinw: 0x%x: %s",
					error->errnum, error->errmess));
				warn_user("FileError", error->errmess);
				continue;
			}
			if (file_handle == 0)
				continue;
			error = xosgbpb_read_atw(file_handle, (char *)&file_header,
					sizeof (struct theme_file_header),
					0, &output_left);
			xosfind_closew(file_handle);
			if (error) {
				LOG(("xosbgpb_read_atw: 0x%x: %s",
					error->errnum, error->errmess));
				warn_user("FileError", error->errmess);
				continue;
			}
			if (output_left > 0)
				continue;	/* should try to read more? */			
	
			/*	Check we are a valid theme
			*/
			if ((file_header.magic_value != 0x4d54534e) ||
					(file_header.parser_version > 1))
				continue;

			/*	Create a new theme descriptor
			*/
			current = (struct theme_descriptor *)calloc(1,
					sizeof(struct theme_descriptor));
			if (!current) {
				LOG(("calloc failed"));
				warn_user("NoMemory", 0);
				return theme_descriptors;
			}
			current->filename = malloc(strlen(info.name) + 1);
			if (!current->filename) {
				LOG(("malloc failed"));
				warn_user("NoMemory", 0);
				free(current);
				return theme_descriptors;
			}
			strcpy(current->filename, info.name);
			strcpy(current->name, file_header.name);
			strcpy(current->author, file_header.author);
			current->browser_background = file_header.browser_bg;
			current->hotlist_background = file_header.hotlist_bg;
			current->status_background = file_header.status_bg;
			current->status_foreground = file_header.status_fg;
			current->throbber_right = (file_header.throbber_left == 0x00);
			current->decompressed_size = file_header.decompressed_sprite_size;
			current->compressed_size = file_header.compressed_sprite_size;
		
			/*	Link in our new descriptor alphabetically
			*/
			if (theme_descriptors) {
				current->next = theme_descriptors;
				theme_descriptors->previous = current;
			}
			theme_descriptors = current;
		}
	}
	
	/*	Sort alphabetically in a very rubbish way
	*/
	if (theme_descriptors->next) {
		current = theme_descriptors;
		while ((test = current->next)) {
			if (strcmp(current->name, test->name) > 0) {
				current->next->previous = current->previous;
				if (current->previous)
					current->previous->next = current->next;
				current->next = test->next;
				test->next = current;
				current->previous = test;
				if (current->next)
					current->next->previous = current;

				current = test->previous;
				if (!current) current = test; 
			} else {
				current = current->next;
			}
		}
		while (theme_descriptors->previous)
			theme_descriptors = theme_descriptors->previous;
	}
	return theme_descriptors;
}


/**
 * Opens a theme ready for use.
 *
 * \param descriptor  the theme_descriptor to open
 * \param list	      whether to open all themes in the list
 * \return whether the operation was successful
 */
bool ro_gui_theme_open(struct theme_descriptor *descriptor, bool list) {	
	fileswitch_object_type obj_type;
	squash_output_status status;
	os_coord dimensions;
	os_mode mode;
	os_error *error;
	char pathname[256];
	bool result = true;
	int i, n;
	int workspace_size, file_size;
	char *raw_data, *workspace;
	osspriteop_area *decompressed;
	
	/*	If we are freeing the whole of the list then we need to
		start at the first descriptor.
	*/
	if (list) {
		while (descriptor->previous) descriptor = descriptor->previous;
	}

	/*	Open the themes
	*/
	while (descriptor) {
		/*	If we are already loaded, increase the usage count
		*/
		if (descriptor->theme) {
			descriptor->theme->users = descriptor->theme->users + 1;
		} else if (descriptor->decompressed_size > 0) {
			/*	Create a new theme
			*/
			descriptor->theme = (struct theme *)calloc(1, sizeof(struct theme));
			if (!descriptor->theme) {
				LOG(("calloc failed"));
				warn_user("NoMemory", 0);
				return false;
			}
			descriptor->theme->users = 1;

			/*	Get our full filename
			*/
			snprintf(pathname, sizeof pathname, "%s.%s",
					THEMES_DIR, descriptor->filename);
			pathname[sizeof pathname - 1] = 0;

			/*	Load the file. We use a goto to exit from here on in as using
				a continue leaves us in an infinite loop - it's nasty, and really
				should be rewritten properly.
			*/
			error = xosfile_read_stamped_no_path(pathname,
					&obj_type, 0, 0, &file_size, 0, 0);
			if (error) {
				LOG(("xosfile_read_stamped_no_path: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("FileError", error->errmess);
				goto ro_gui_theme_open_continue;
			}
			if (obj_type != fileswitch_IS_FILE)
				goto ro_gui_theme_open_continue;
			raw_data = malloc(file_size);
			if (!raw_data) {
				LOG(("No memory for malloc()"));
				warn_user("NoMemory", 0);
				return false;
			}
			error = xosfile_load_stamped_no_path(pathname, (byte *)raw_data,
					0, 0, 0, 0, 0);
			if (error) {
				free(raw_data);
				LOG(("xosfile_load_stamped_no_path: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("FileError", error->errmess);
				goto ro_gui_theme_open_continue;
			}
			
			/*	Decompress the sprites
			*/
			error = xsquash_decompress_return_sizes(-1, &workspace_size, 0);
			if (error) {
				free(raw_data);
				LOG(("xsquash_decompress_return_sizes: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MiscError", error->errmess);
				goto ro_gui_theme_open_continue;
			}
			decompressed = (osspriteop_area *)malloc(descriptor->decompressed_size);
			workspace = malloc(workspace_size);
			if ((!decompressed) || (!workspace)) {
				if (decompressed) free(decompressed);
				if (workspace) free(raw_data);
				LOG(("No memory for malloc()"));
				warn_user("NoMemory", 0);
				return false;
			}
			error = xsquash_decompress(squash_INPUT_ALL_PRESENT,
					workspace,
					(byte *)(raw_data + sizeof(struct theme_file_header)),
					descriptor->compressed_size,
					(byte *)decompressed,
					descriptor->decompressed_size,
					&status, 0, 0, 0, 0);
			free(workspace);
			free(raw_data);
			if (error) {
				free(decompressed);
				LOG(("xsquash_decompress: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MiscError", error->errmess);
				goto ro_gui_theme_open_continue;
			}
			if (status != 0) {
			  	free(decompressed);
				goto ro_gui_theme_open_continue;
			}
			descriptor->theme->sprite_area = decompressed;

			/*	Find the highest sprite called 'throbber%i', and get the
				maximum dimensions for all 'thobber%i' icons.
			*/
			for (i = 1; i <= descriptor->theme->sprite_area->sprite_count; i++) {
				osspriteop_return_name(osspriteop_USER_AREA,
						descriptor->theme->sprite_area, pathname, 12, i);
				if (strncmp(pathname, "throbber", 8) == 0) {
					/*	Get the max sprite width/height
					*/
					xosspriteop_read_sprite_info(osspriteop_USER_AREA,
						descriptor->theme->sprite_area,
						(osspriteop_id)pathname,
						&dimensions.x, &dimensions.y,
						(osbool *)0, &mode);
					ro_convert_pixels_to_os_units(&dimensions, mode);
					if (dimensions.x > descriptor->theme->throbber_width)
						descriptor->theme->throbber_width = dimensions.x;
					if (dimensions.y > descriptor->theme->throbber_height)
						descriptor->theme->throbber_height = dimensions.y;
	
					/*	Get the throbber number
					*/
					n = atoi(pathname + 8);
					if (descriptor->theme->throbber_frames < n)
						descriptor->theme->throbber_frames = n;
				}
	  		}
		}
	  	
ro_gui_theme_open_continue:
		/*	Loop or return depending on whether the entire list
			is to be processed.
		*/
		if (list) {
			descriptor = descriptor->next;
		} else {
			return result;
		} 
	}
	return result;
}


/**
 * Applies the theme to all current windows and subsequent ones.
 *
 * \param descriptor  the theme_descriptor to open
 * \return whether the operation was successful
 */
bool ro_gui_theme_apply(struct theme_descriptor *descriptor) {
	struct theme_descriptor *theme_previous;

	/*	Check if the theme is already applied
	*/
	if (descriptor == theme_current) return true;
	
	/*	Re-open the new-theme and release the current theme
	*/
	if (!ro_gui_theme_open(descriptor, false)) {
		/*	The error has already been reported
		*/
		return false; 
	}
	theme_previous = theme_current;
	theme_current = descriptor;
	
	/*	Apply the theme to all the current windows
	*/
	ro_gui_window_update_theme();
	
	/*	Release the previous theme
	*/
	ro_gui_theme_close(theme_previous, false);
	return true;
}


/**
 * Closes a theme after use.
 *
 * \param descriptor  the theme_descriptor to close
 * \param list	      whether to open all themes in the list
 * \return whether the operation was successful
 */
void ro_gui_theme_close(struct theme_descriptor *descriptor, bool list) {

	/*	We might not have created any descriptors yet to close.
	*/
	if (!descriptor) return;
	
	/*	If we are freeing the whole of the list then we need to
		start at the first descriptor.
	*/
	if (list) {
		while (descriptor->previous) descriptor = descriptor->previous;
	}
	
	/*	Close the themes
	*/
	while (descriptor) {
		/*	Lower the theme usage count
		*/
		if (descriptor->theme) {
			descriptor->theme->users = descriptor->theme->users - 1;
			if (descriptor->theme->users <= 0) {
				free(descriptor->theme->sprite_area);
				free(descriptor->theme);
				descriptor->theme = NULL;
			}
		}

		/*	Loop or return depending on whether the entire list
			is to be processed.
		*/
		if (list) {
			descriptor = descriptor->next;
		} else {
			return;
		} 
	}
}


/**
 * Frees any unused theme descriptors.
 *
 * \param descriptor  the theme_descriptor to free
 * \param list	      whether to open all themes in the list
 * \return whether the operation was successful
 */
void ro_gui_theme_free(struct theme_descriptor *descriptor, bool list) {
	struct theme_descriptor *next_descriptor;

	/*	We might not have created any descriptors yet to close.
	*/
	if (!descriptor) return;
	
	/*	If we are freeing the whole of the list then we need to
		start at the first descriptor.
	*/
	if (list) {
		while (descriptor->previous) descriptor = descriptor->previous;
	}

	/*	Close the themes
	*/
	while (descriptor) {
		/*	Remember where we are going next
		*/
		next_descriptor = descriptor->next;
	  	
		/*	If we have no loaded theme then we can kill the descriptor
		*/
		if (!descriptor->theme) {
			/*	De-link the descriptor
			*/
			if (descriptor->previous)
					descriptor->previous->next = descriptor->next;
			if (descriptor->next)
					descriptor->next->previous = descriptor->previous;
			
			/*	Keep the cached list in sync
			*/
			if (theme_descriptors == descriptor)
					theme_descriptors = next_descriptor;
			/*	Release any memory
			*/
			free(descriptor->filename);
			free(descriptor);
		}

		/*	Loop or return depending on whether the entire list
			is to be processed.
		*/
		if (list) {
			descriptor = next_descriptor;
		} else {
			return;
		} 
	}
}


/**
 * Creates a toolbar.
 *
 * \param descriptor  the theme to use, or NULL for current
 * \param type	      the toolbar type
 * \return a new toolbar, or NULL for failure
 */
struct toolbar *ro_gui_theme_create_toolbar(struct theme_descriptor *descriptor, toolbar_type type) {
	struct toolbar *toolbar;

	/*	Create a new toolbar
	*/
	toolbar = calloc(sizeof(struct toolbar), 1);
	if (!toolbar) {
		LOG(("No memory for malloc()"));
		warn_user("NoMemory", 0);
		return NULL;
	}
	toolbar->type = type;
	
	/*	Store the theme
	*/
	if (!descriptor) descriptor = theme_current;
	toolbar->descriptor = descriptor;

	/*	Apply the default settings
	*/
	toolbar->display_buttons = true;
	toolbar->toolbar_current = 16384;
	switch (type) {
		case THEME_BROWSER_TOOLBAR:
			toolbar->display_url = true;
			toolbar->display_throbber = true;
			toolbar->display_status = true;
			ro_gui_theme_add_toolbar_icon(toolbar, "back", ICON_TOOLBAR_BACK);
			ro_gui_theme_add_toolbar_icon(toolbar, "forward", ICON_TOOLBAR_FORWARD);
			ro_gui_theme_add_toolbar_icon(toolbar, "stop", ICON_TOOLBAR_STOP);
			ro_gui_theme_add_toolbar_icon(toolbar, "reload", ICON_TOOLBAR_RELOAD);
			ro_gui_theme_add_toolbar_icon(toolbar, "home", ICON_TOOLBAR_HOME);
			ro_gui_theme_add_toolbar_icon(toolbar, NULL, -1);
			ro_gui_theme_add_toolbar_icon(toolbar, "search", ICON_TOOLBAR_SEARCH);
			ro_gui_theme_add_toolbar_icon(toolbar, "history", ICON_TOOLBAR_HISTORY);
			ro_gui_theme_add_toolbar_icon(toolbar, "scale", ICON_TOOLBAR_SCALE);
			ro_gui_theme_add_toolbar_icon(toolbar, NULL, -1);
			ro_gui_theme_add_toolbar_icon(toolbar, "mark", ICON_TOOLBAR_BOOKMARK);
			ro_gui_theme_add_toolbar_icon(toolbar, "save", ICON_TOOLBAR_SAVE);
			ro_gui_theme_add_toolbar_icon(toolbar, "print", ICON_TOOLBAR_PRINT);
			break;
		case THEME_HOTLIST_TOOLBAR:
			ro_gui_theme_add_toolbar_icon(toolbar, "create", ICON_TOOLBAR_CREATE);
			ro_gui_theme_add_toolbar_icon(toolbar, "delete", ICON_TOOLBAR_DELETE);
			ro_gui_theme_add_toolbar_icon(toolbar, "launch", ICON_TOOLBAR_LAUNCH);
			ro_gui_theme_add_toolbar_icon(toolbar, NULL, -1);
			ro_gui_theme_add_toolbar_icon(toolbar, "open", ICON_TOOLBAR_OPEN);
			ro_gui_theme_add_toolbar_icon(toolbar, "expand", ICON_TOOLBAR_EXPAND);
			break;
	}
        
	/*	Claim the memory for our Wimp indirection
	*/
	if (type == THEME_BROWSER_TOOLBAR) {
		toolbar->url_buffer = calloc(1, THEME_URL_MEMORY + THEME_THROBBER_MEMORY +
				THEME_STATUS_MEMORY);
		if (!toolbar->url_buffer) {
			LOG(("No memory for calloc()"));
			ro_gui_theme_destroy_toolbar(toolbar);
			return NULL;
		}
		toolbar->throbber_buffer = toolbar->url_buffer + THEME_URL_MEMORY;
		toolbar->status_buffer = toolbar->throbber_buffer + THEME_THROBBER_MEMORY;
		sprintf(toolbar->throbber_buffer, "throbber0");
	}

	/*	Apply the desired theme to the toolbar
	*/
	if (!ro_gui_theme_update_toolbar(descriptor, toolbar)) {
		ro_gui_theme_destroy_toolbar(toolbar);
		return NULL;
	}
	return toolbar;
}


/**
 * Updates a toolbar to use a particular theme.
 * The toolbar may be unstable on failure and should be destroyed.
 *
 * \param descriptor  the theme to use, or NULL for current
 * \param toolbar     the toolbar to update
 * \return whether the operation was successful
 */
bool ro_gui_theme_update_toolbar(struct theme_descriptor *descriptor, struct toolbar *toolbar) {
	wimp_icon_create new_icon;
	os_error *error;
	osspriteop_area *sprite_area;
	struct toolbar_icon *toolbar_icon;
	int width;
	if (!toolbar) return false;
	
	/*	Set the theme and window sprite area
	*/
	if (!descriptor) descriptor = theme_current;
	toolbar->descriptor = descriptor;
	if ((toolbar->descriptor) && (toolbar->descriptor->theme)) {
		sprite_area = toolbar->descriptor->theme->sprite_area;
	} else {
		sprite_area = (osspriteop_area *)1;
	}
	theme_toolbar_window.sprite_area = sprite_area;
	
	/*	Update the icon sizes
	*/
	toolbar_icon = toolbar->icon;
	while (toolbar_icon) {
		ro_gui_theme_update_toolbar_icon(toolbar, toolbar_icon);
		toolbar_icon = toolbar_icon->next;
	}
	
	/*	Recreate the toolbar window
	*/
	if (toolbar->descriptor) {
		if (toolbar->type == THEME_BROWSER_TOOLBAR) {
			theme_toolbar_window.work_bg = toolbar->descriptor->browser_background;
		} else {
			theme_toolbar_window.work_bg = toolbar->descriptor->hotlist_background;
		}
	} else {
		theme_toolbar_window.work_bg = wimp_COLOUR_VERY_LIGHT_GREY;
	}
	theme_toolbar_window.flags |= wimp_WINDOW_NO_BOUNDS;
	theme_toolbar_window.xmin = 1;
	theme_toolbar_window.ymin = 1;
	theme_toolbar_window.extent.x1 = 16384;
	theme_toolbar_window.extent.y1 = 16384;
	if (toolbar->toolbar_handle) {
		xwimp_delete_window(toolbar->toolbar_handle);
		toolbar->toolbar_handle = NULL;
	}
	error = xwimp_create_window(&theme_toolbar_window, &toolbar->toolbar_handle);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	LOG(("Created window 1"));
	
	/*	Create the basic icons
	*/	
	int max_icon = ICON_TOOLBAR_URL;
	if (toolbar->type == THEME_HOTLIST_TOOLBAR) max_icon = ICON_TOOLBAR_HOTLIST_LAST;
	new_icon.w = toolbar->toolbar_handle;
	new_icon.icon.data.indirected_text.size = 1;
	new_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_SPRITE | wimp_ICON_INDIRECTED |
					wimp_ICON_HCENTRED | wimp_ICON_VCENTRED |
					(wimp_BUTTON_CLICK << wimp_ICON_BUTTON_TYPE_SHIFT);
	if (toolbar->descriptor) {
		new_icon.icon.flags |= (toolbar->descriptor->browser_background
				 << wimp_ICON_BG_COLOUR_SHIFT);
	} else {
		new_icon.icon.flags |= (wimp_COLOUR_VERY_LIGHT_GREY
				 << wimp_ICON_BG_COLOUR_SHIFT);	  
	}
	for (int i = 0; i < max_icon; i++) {
		new_icon.icon.data.indirected_text.text = theme_null_text_string;
		toolbar_icon = toolbar->icon;
		while (toolbar_icon) {
			if (toolbar_icon->icon_number == i) {
				new_icon.icon.data.indirected_text.validation =
					toolbar_icon->validation;
				break;
			} else {
				toolbar_icon = toolbar_icon->next;
			}
		}
		error = xwimp_create_icon(&new_icon, 0);
		if (error) {
			LOG(("xwimp_create_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}
	}
	
	/*	Create the URL/throbber icons
	*/
	if (toolbar->type == THEME_BROWSER_TOOLBAR) {
		new_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_VCENTRED |
				wimp_ICON_BORDER | wimp_ICON_FILLED |
				(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
				(wimp_BUTTON_WRITE_CLICK_DRAG << wimp_ICON_BUTTON_TYPE_SHIFT);
		new_icon.icon.data.indirected_text.text = toolbar->url_buffer;
		new_icon.icon.data.indirected_text.validation = theme_url_validation;
		new_icon.icon.data.indirected_text.size = THEME_URL_MEMORY;
		error = xwimp_create_icon(&new_icon, 0);
		if (error) {
			LOG(("xwimp_create_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

		/*	Now the throbber
		*/
		new_icon.icon.flags = wimp_ICON_SPRITE | wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED |
				wimp_ICON_VCENTRED;
		new_icon.icon.data.indirected_sprite.id = (osspriteop_id)toolbar->throbber_buffer;
		new_icon.icon.data.indirected_sprite.area = sprite_area;
		new_icon.icon.data.indirected_sprite.size = THEME_THROBBER_MEMORY;
		error = xwimp_create_icon(&new_icon, 0);
		if (error) {
			LOG(("xwimp_create_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}
	}
	if (toolbar->parent_handle) {
		ro_gui_theme_attach_toolbar(toolbar, toolbar->parent_handle);
	}
	
	/*	Recreate the status window
	*/
	if (toolbar->type == THEME_BROWSER_TOOLBAR) {
		/*	Delete the old window and create a new one
		*/
		if (toolbar->status_handle) {
			xwimp_delete_window(toolbar->status_handle);
			toolbar->status_handle = NULL;
		}
		if (toolbar->descriptor) {
			theme_toolbar_window.work_bg = toolbar->descriptor->status_background;
		} else {
			theme_toolbar_window.work_bg = wimp_COLOUR_VERY_LIGHT_GREY;
		}
		theme_toolbar_window.flags &= ~wimp_WINDOW_NO_BOUNDS;
		theme_toolbar_window.xmin = 12;
		theme_toolbar_window.ymin = ro_get_hscroll_height((wimp_w)0) - 4;
		theme_toolbar_window.extent.y1 = theme_toolbar_window.ymin;
		error = xwimp_create_window(&theme_toolbar_window, &toolbar->status_handle);
		if (error) {
			LOG(("xwimp_create_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}
		
		/*	Create the status resize icon
		*/
		new_icon.w = toolbar->status_handle;
		new_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
				wimp_ICON_BORDER | wimp_ICON_FILLED |
				(wimp_COLOUR_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT) |
				(wimp_BUTTON_CLICK_DRAG << wimp_ICON_BUTTON_TYPE_SHIFT);
		new_icon.icon.data.indirected_text.text = theme_null_text_string;
		new_icon.icon.data.indirected_text.validation = theme_resize_validation;
		new_icon.icon.data.indirected_text.size = 1;
		error = xwimp_create_icon(&new_icon, 0);
		if (error) {
			LOG(("xwimp_create_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

		/*	And finally our status display icon
		*/
		new_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_VCENTRED;
		if (toolbar->descriptor) {
			new_icon.icon.flags |=
				(toolbar->descriptor->status_foreground << wimp_ICON_FG_COLOUR_SHIFT) |
				(toolbar->descriptor->status_background << wimp_ICON_BG_COLOUR_SHIFT);
		} else {
			new_icon.icon.flags |=
				(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
				(wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT);
		}
		new_icon.icon.data.indirected_text.text = toolbar->status_buffer;
		new_icon.icon.data.indirected_text.validation = theme_null_text_string;
		new_icon.icon.data.indirected_text.size = THEME_STATUS_MEMORY;
		error = xwimp_create_icon(&new_icon, 0);
		if (error) {
			LOG(("xwimp_create_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

	}
	
	/*	Force a re-processing of the toolbar
	*/
	width = toolbar->toolbar_current;
	toolbar->reformat_buttons = true;
	toolbar->toolbar_current = -1;
	toolbar->status_current = -1;
	ro_gui_theme_process_toolbar(toolbar, width);
	return true;
}


/**
 * Attaches a toolbar to a window.
 *
 * \param toolbar     the toolbar to update
 * \param parent      the window to contain the toolbar
 * \return whether the operation was successful
 */
bool ro_gui_theme_attach_toolbar(struct toolbar *toolbar, wimp_w parent) {
	wimp_outline outline;
	wimp_window_state state;
	if (!toolbar) return false;

	/*	Attach/close the window
	*/
	toolbar->parent_handle = parent;
	if (toolbar->height > 0) {
		outline.w = parent;
		xwimp_get_window_outline(&outline);
		state.w = parent;
		xwimp_get_window_state(&state);
		state.w = toolbar->toolbar_handle;
		state.visible.x1 = outline.outline.x1 - 2;
		state.visible.y0 = state.visible.y1 - toolbar->height + 2;
		state.xscroll = 0;
		state.yscroll = 0;
		xwimp_open_window_nested((wimp_open *)&state, parent,
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
	return true;
}


/**
 * Updates the toolbars status bar settings to reflect the current size
 *
 * \param toolbar     the toolbar to update
 */
void ro_gui_theme_resize_toolbar_status(struct toolbar *toolbar) {
	os_error *error;
	wimp_outline outline;
	wimp_window_state state;
	wimp_w parent = NULL;
	int parent_size, status_size;
	if ((!toolbar) || (!toolbar->parent_handle)) return;
	
	/*	Get the width to scale to
	*/
	parent = toolbar->parent_handle;
	outline.w = toolbar->parent_handle;
	error = xwimp_get_window_outline(&outline);
	if (error) {
		LOG(("xwimp_get_window_outline: 0x%x: %s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	parent_size = outline.outline.x1 - outline.outline.x0 - ro_get_vscroll_width(parent) - 2;

	/*	Get the current size
	*/
	state.w = toolbar->status_handle;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	status_size = state.visible.x1 - state.visible.x0;

	/*	Store the new size
	*/
	toolbar->status_width = (10000 * status_size) / parent_size;
	if (toolbar->status_width > 10000) toolbar->status_width = 10000;
	ro_gui_theme_process_toolbar(toolbar, -1);
}


/**
 * Updates the toolbar to reflect changes to the icon flags and any reformatting
 * required due to the change in parent window size.
 *
 * \param toolbar     the toolbar to update
 * \param width	      a specific width to resize to, or -1 to use parent width
 * \return whether the operation was successful
 */
bool ro_gui_theme_process_toolbar(struct toolbar *toolbar, int width) {
	wimp_caret caret;
	os_box extent = { 0, 0, 0, 0 };
	os_error *error;
	wimp_w parent = NULL;
	wimp_outline outline;
	wimp_window_state state;
	int throbber_x = -1;
	int status_max;
	int left_edge, right_edge, bottom_edge;
	int status_size = 0;
	int status_height = 0;
	if (!toolbar) return false;
	int old_height = toolbar->height;
	int old_width = toolbar->toolbar_current;
	struct toolbar_icon *toolbar_icon;
	bool visible_icon = false;
  	
	/*	Find the parent window handle if we need to process the status window,
		or the caller has requested we calculate the width ourself.
	*/
	if ((width == -1) || ((toolbar->status_handle) && (toolbar->display_status))) {
		parent = toolbar->parent_handle;
		
		/*	Get the window outline width
		*/ 
		if (width == -1) {
			if (!parent) return false;
			outline.w = toolbar->parent_handle;
			error = xwimp_get_window_outline(&outline);
			if (error) {
				LOG(("xwimp_get_window_outline: 0x%x: %s",
					error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
				return false;
			}
			width = outline.outline.x1 - outline.outline.x0 - 2;
		}
	}
	
	/*	Reformat the buttons starting with the throbber
	*/
	if ((width != old_width) || (toolbar->reformat_buttons)) {
		left_edge = 6;
		right_edge = width - 8;
		toolbar->height = 0;
		if ((toolbar->descriptor) && (toolbar->descriptor->theme) &&
				(toolbar->type == THEME_BROWSER_TOOLBAR) &&
				(toolbar->display_throbber)) {
			if (!toolbar->descriptor->throbber_right) {
				throbber_x = left_edge;
				left_edge += toolbar->descriptor->theme->throbber_width + 8;
			}
			toolbar->height = toolbar->descriptor->theme->throbber_height + 8;
		}
		if ((toolbar->type == THEME_BROWSER_TOOLBAR) &&	(toolbar->display_url)) {
			if (toolbar->height < 52 + 8) toolbar->height = 52 + 8;
		}

		/*	Get the minimum height of the icons
		*/
		bottom_edge = left_edge;
		if ((toolbar->display_buttons) && (toolbar->descriptor) &&
				(toolbar->descriptor->theme)) {
			toolbar_icon = toolbar->icon;
			while (toolbar_icon) {
				if (toolbar_icon->display) {
					bottom_edge += toolbar_icon->width;
					visible_icon = true;
					if ((toolbar_icon->height != 0) && 
						(toolbar->height < toolbar_icon->height + 8)) {
						toolbar->height = toolbar_icon->height + 8;
					}
				}
				toolbar_icon = toolbar_icon->next;
			}
			if (visible_icon) bottom_edge += 8;
		}
		
		/*	Check for minimum widths
		*/
		if (toolbar->type == THEME_BROWSER_TOOLBAR) {
			if (!toolbar->reformat_buttons) left_edge = bottom_edge;
			if (toolbar->display_url) bottom_edge += 32;
			if (bottom_edge > right_edge) right_edge = bottom_edge;
			if ((toolbar->descriptor) && (toolbar->descriptor->theme) &&
					(toolbar->display_throbber) &&
					(toolbar->descriptor->throbber_right)) {
				bottom_edge += toolbar->descriptor->theme->throbber_width;
				if (bottom_edge > right_edge) right_edge = bottom_edge;
				throbber_x = right_edge - toolbar->descriptor->theme->throbber_width;
				right_edge -= toolbar->descriptor->theme->throbber_width + 8;
			}
		}

		if (toolbar->reformat_buttons) {
			/*	Hide the URL bar if we should
			*/
			if (!toolbar->display_url) {
				if (!xwimp_get_caret_position(&caret)) {
					if ((caret.w == toolbar->toolbar_handle) &&
							(caret.i == ICON_TOOLBAR_URL))
						xwimp_set_caret_position((wimp_w)-1, 0, 0, 0, 0, 0);
				}
				xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_URL,
					0, -16384, 0, -16384);
			}
			ro_gui_set_icon_shaded_state(toolbar->toolbar_handle, ICON_TOOLBAR_URL,
				!toolbar->display_url);
			xwimp_force_redraw(toolbar->toolbar_handle,
				0, 0, 16384, 16384);
			
			/*	Move the buttons
			*/
			toolbar_icon = toolbar->icon;
			while (toolbar_icon) {
				if ((toolbar->display_buttons) && (toolbar_icon->display)
					&& (toolbar_icon->width > 0)) {
				  	visible_icon = true;
					bottom_edge = (toolbar->height -
							toolbar_icon->height) / 2;
					xwimp_resize_icon(toolbar->toolbar_handle,
							toolbar_icon->icon_number,
							left_edge, bottom_edge,
							left_edge + toolbar_icon->width,
							bottom_edge + toolbar_icon->height);
					left_edge += toolbar_icon->width;
				} else {
					xwimp_resize_icon(toolbar->toolbar_handle,
							toolbar_icon->icon_number,
							0, -16384, 0, -16384);
				}
				toolbar_icon = toolbar_icon->next;
			}
			if (visible_icon) left_edge += 8;
		}


		if (toolbar->height != 0) toolbar->height += 2;
		if (toolbar->type == THEME_BROWSER_TOOLBAR) {
			/*	Move the URL bar
			*/
			if (toolbar->display_url) {
				xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_URL,
					left_edge, (toolbar->height / 2) - 26,
					right_edge, (toolbar->height / 2) + 26);
				xwimp_force_redraw(toolbar->toolbar_handle,
					right_edge, 0, 16384, 16384);
				if (!xwimp_get_caret_position(&caret)) {
					if ((caret.w == toolbar->toolbar_handle) &&
							(caret.i == ICON_TOOLBAR_URL)) {
						xwimp_set_caret_position(toolbar->toolbar_handle,
								ICON_TOOLBAR_URL,
								caret.pos.x, caret.pos.y,
								-1, caret.index);
					}
				}
				ro_gui_redraw_icon(toolbar->toolbar_handle, ICON_TOOLBAR_URL);
			}

			/*	Move the throbber
			*/
			if ((toolbar->descriptor) && (toolbar->descriptor->theme) &&
					(throbber_x >= 0) && (toolbar->display_throbber)) {
				xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_THROBBER,
					throbber_x, 0,
					throbber_x + toolbar->descriptor->theme->throbber_width,
					toolbar->height);
				if (toolbar->descriptor->throbber_right) 
					xwimp_force_redraw(toolbar->toolbar_handle,
						old_width - width + throbber_x, 0, 16384, 16384);
					xwimp_force_redraw(toolbar->toolbar_handle,
						throbber_x, 0, 16384, 16384);

			} else {
				xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_THROBBER,
					0, -16384, 0, -16384);
			}
		}

		/*	Re-attach to the parent
		*/
		toolbar->toolbar_current = width;
		if ((toolbar->reformat_buttons) && (parent) && (old_height != toolbar->height)) {
		  	extent.x1 = 16384;
		  	extent.y0 = 0;
		  	extent.y1 = toolbar->height;
		  	xwimp_set_extent(toolbar->toolbar_handle, &extent);
			ro_gui_theme_attach_toolbar(toolbar, parent);
		}
		toolbar->reformat_buttons = false;
	}

	/*	Reformat the status bar
	*/
	if ((toolbar->status_handle) && (parent)) {
		/*	Get the current state
		*/
		state.w = toolbar->status_handle;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}
		
		/*	Open or close the window
		*/
		if (!toolbar->display_status) {
			if (state.flags & wimp_WINDOW_OPEN)
				xwimp_close_window(toolbar->status_handle);
		} else {
			/*	Get the status bar height/width
			*/
			status_max = width - ro_get_vscroll_width(parent);
			status_size = (status_max * toolbar->status_width) / 10000;
			if (status_size < 12) status_size = 12;
			status_height = ro_get_hscroll_height(parent) - 2;
			
			/*	Update the extent
			*/
			extent.x1 = status_max;
			extent.y1 = status_height - 2;
			xwimp_set_extent(toolbar->status_handle, &extent);
			
			/*	Re-open the window
			*/
			state.w = toolbar->status_handle;
			state.xscroll = 0;
			state.yscroll = 0;
			state.next = wimp_TOP;
			state.visible.x0 = outline.outline.x0;
			state.visible.x1 = outline.outline.x0 + status_size;
			state.visible.y0 = outline.outline.y0 - status_height;
			state.visible.y1 = outline.outline.y0 - 2;
			xwimp_open_window_nested((wimp_open *)&state, parent,
					wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
							<< wimp_CHILD_XORIGIN_SHIFT |
					wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
							<< wimp_CHILD_YORIGIN_SHIFT |
					wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
							<< wimp_CHILD_LS_EDGE_SHIFT |
					wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
							<< wimp_CHILD_BS_EDGE_SHIFT |
					wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
							<< wimp_CHILD_RS_EDGE_SHIFT |
					wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
							<< wimp_CHILD_TS_EDGE_SHIFT);

			/*	Resize and redraw the icons
			*/
			status_size = state.visible.x1 - state.visible.x0;
			if (status_size != toolbar->status_current) {
				xwimp_resize_icon(toolbar->status_handle, ICON_STATUS_TEXT,
						0, 0,
						status_size - 12, status_height - 2);
				xwimp_resize_icon(toolbar->status_handle, ICON_STATUS_RESIZE,
						status_size - 12, 0,
						status_size, status_height - 2);
				xwimp_force_redraw(toolbar->status_handle,
						toolbar->status_current - 12, 0,
						status_size - 12, status_height - 2);
				xwimp_force_redraw(toolbar->status_handle,
						status_size - 12, 0,
						status_size, status_height - 2);
				toolbar->status_current = status_size;
			}
		}
	}
	return true;
}


/**
 * Destroys a toolbar and frees any associated memory.
 *
 * \param toolbar     the toolbar to destroy
 */
void ro_gui_theme_destroy_toolbar(struct toolbar *toolbar) {
	struct toolbar_icon *icon;
	struct toolbar_icon *next_icon;
	if (!toolbar) return;
  	
	/*	Delete our windows
	*/
	if (toolbar->toolbar_handle)
		xwimp_delete_window(toolbar->toolbar_handle);
	if (toolbar->status_handle)
		xwimp_delete_window(toolbar->status_handle);

	/*	Free the Wimp buffer (we only created one for them all)
	*/
	free(toolbar->url_buffer);
	
	/*	Free all the icons
	*/
	next_icon = toolbar->icon;
	while ((icon = next_icon) != NULL) {
		next_icon = icon->next;
		ro_gui_theme_destroy_toolbar_icon(icon);
	}
	free(toolbar);
}

void ro_gui_theme_add_toolbar_icon(struct toolbar *toolbar, const char *name, int icon_number) {
	if (!toolbar) return;
	struct toolbar_icon *toolbar_icon;
	struct toolbar_icon *link_icon;

	/*	Separators are really a sprite called "separator"
	*/
	if (name == NULL) name = "separator";

	/*	Create a new toolbar
	*/
	toolbar_icon = calloc(sizeof(struct toolbar_icon), 1);
	if (!toolbar_icon) {
		LOG(("No memory for malloc()"));
		warn_user("NoMemory", 0);
		return;
	}
	
	/*	Set up and link in the icon
	*/
	sprintf(toolbar_icon->name, name);
	sprintf(toolbar_icon->validation, "R5;S%s,p%s", name, name);
	toolbar_icon->icon_number = icon_number;
	toolbar_icon->display = true;
	if (!toolbar->icon) {
		toolbar->icon = toolbar_icon;
	} else {
		link_icon = toolbar->icon;
		while (link_icon->next) link_icon = link_icon->next;
		link_icon->next = toolbar_icon;
	}
}


/**
 * Updates a toolbar icon with respect to the associated sprite.
 *
 * \param icon	   the toolbar icon to update
 */
void ro_gui_theme_update_toolbar_icon(struct toolbar *toolbar, struct toolbar_icon *icon) {
	os_coord dimensions;
	os_mode mode;
	os_error *error;
	int default_width = 0;
  	
	/*	Separators default to a width of 16
	*/
	if (icon->icon_number == -1) default_width = 16;
  	
	/*	Handle no theme/no sprite area
	*/
	if (!toolbar) return;
	if ((!toolbar->descriptor) || (!toolbar->descriptor->theme) ||
			(!toolbar->descriptor->theme->sprite_area)) {
		icon->width = 0;
		icon->height = 0;
		return;
	}

	/*	Get the sprite details
	*/
	error = xosspriteop_read_sprite_info(osspriteop_USER_AREA,
			toolbar->descriptor->theme->sprite_area, (osspriteop_id)icon->name,
			&dimensions.x, &dimensions.y, 0, &mode);
	if (error) {
		icon->width = default_width;
		icon->height = 0;
		if (error->errnum != error_SPRITE_OP_DOESNT_EXIST) {
			LOG(("xosspriteop_read_sprite_info: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
		}
		return;
	}
	
	/*	Store the details
	*/
	ro_convert_pixels_to_os_units(&dimensions, mode);
	icon->width = dimensions.x;
	icon->height = dimensions.y;
}


/**
 * Destroys a toolbar icon and frees any associated memory.
 * The icon is not removed from any linked list.
 *
 * \param icon	   the toolbar icon to destroy
 */
void ro_gui_theme_destroy_toolbar_icon(struct toolbar_icon *icon) {
	free(icon);
}
