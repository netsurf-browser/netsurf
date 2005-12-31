/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Window themes and toolbars (implementation).
 */

#include <alloca.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "oslib/dragasprite.h"
#include "oslib/os.h"
#include "oslib/osgbpb.h"
#include "oslib/osgbpb.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osspriteop.h"
#include "oslib/wimpspriteop.h"
#include "oslib/squash.h"
#include "oslib/wimp.h"
#include "oslib/wimpextend.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/menus.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/treeview.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#define THEME_URL_MEMORY 256
#define THEME_THROBBER_MEMORY 12
#define THEME_STATUS_MEMORY 256

static struct theme_descriptor *theme_current = NULL;
static struct theme_descriptor *theme_descriptors = NULL;
static struct toolbar *theme_toolbar_drag = NULL;
static struct toolbar_icon *theme_toolbar_icon_drag = NULL;
static bool theme_toolbar_editor_drag = false;

/* these order of the icons must match the numbers defined in riscos/gui.h */
static const char * theme_browser_icons[] = {"back", "forward", "stop",
		"reload", "home", "history", "save", "print", "hotlist",
		"scale", "search", NULL};
static const char * theme_hotlist_icons[] = {"delete", "expand", "open",
		"launch", "create", NULL};
static const char * theme_history_icons[] = {"delete", "expand", "open",
		"launch", NULL};

static void ro_gui_theme_redraw(wimp_draw *redraw);
static void ro_gui_theme_get_available_in_dir(const char *directory);
static void ro_gui_theme_free(struct theme_descriptor *descriptor);
static struct toolbar_icon *ro_gui_theme_add_toolbar_icon(
		struct toolbar *toolbar, const char *name, int icon_number);
static void ro_gui_theme_update_toolbar_icon(struct toolbar *toolbar,
		struct toolbar_icon *icon);
static void ro_gui_theme_destroy_toolbar_icon(struct toolbar_icon *icon);
static void ro_gui_theme_link_toolbar_icon(struct toolbar *toolbar,
		struct toolbar_icon *icon, struct toolbar_icon *link,
		bool before);
static void ro_gui_theme_delink_toolbar_icon(struct toolbar *toolbar,
		struct toolbar_icon *icon);
static struct toolbar_icon *ro_gui_theme_toolbar_get_insert_icon(
		struct toolbar *toolbar, int x, int y, bool *before);
static void ro_gui_theme_add_toolbar_icons(struct toolbar *toolbar,
		const char* icons[], const char* ident);
static void ro_gui_theme_set_help_prefix(struct toolbar *toolbar);


/*	A basic window for the toolbar and status
*/
static wimp_window theme_toolbar_window = {
	{0, 0, 1, 1},
	0,
	0,
	wimp_TOP,
	wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_MOVEABLE |
			wimp_WINDOW_FURNITURE_WINDOW |
			wimp_WINDOW_IGNORE_XEXTENT | wimp_WINDOW_IGNORE_YEXTENT,
	wimp_COLOUR_BLACK,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_VERY_LIGHT_GREY,
	wimp_COLOUR_DARK_GREY,
	wimp_COLOUR_MID_LIGHT_GREY,
	wimp_COLOUR_CREAM,
	wimp_WINDOW_NEVER3D | 0x16u /* RISC OS 5.03+ */,
	{0, 0, 1, 1},
	0,
	0,
	wimpspriteop_AREA,
	12,
	1,
	{""},
	0,
	{ }
};


/*	Shared icon validation
*/
static char theme_url_validation[] = "Pptr_write;KN\0";
static char theme_resize_validation[] = "R1;Pptr_lr,8,6\0";
static char theme_null_text_string[] = "\0";
static char theme_separator_name[] = "separator\0";


/**
 * Initialise the theme handler
 */
void ro_gui_theme_initialise(void) {
	theme_descriptors = ro_gui_theme_get_available();
}


/**
 * Finalise the theme handler
 */
void ro_gui_theme_finalise(void) {
	ro_gui_theme_close(theme_current, false);
	ro_gui_theme_free(theme_descriptors);
}


/**
 * Finds a theme from the cached values.
 *
 * The returned theme is only guaranteed to be valid until the next call
 * to ro_gui_theme_get_available() unless it has been opened using
 * ro_gui_theme_open().
 *
 * \param leafname  the filename of the theme_descriptor to return
 * \return the requested theme_descriptor, or NULL if not found
 */
struct theme_descriptor *ro_gui_theme_find(const char *leafname) {
	struct theme_descriptor *descriptor;

	if (!leafname)
		return NULL;

	for (descriptor = theme_descriptors; descriptor;
			descriptor = descriptor->next)
		if (!strcmp(leafname, descriptor->leafname))
			return descriptor;
	/* fallback for 10 chars on old filesystems */
	for (descriptor = theme_descriptors; descriptor;
			descriptor = descriptor->next)
		if (!strncmp(leafname, descriptor->leafname, 10))
			return descriptor;
	return NULL;
}


/**
 * Reads and caches the currently available themes.
 *
 * \return the requested theme_descriptor, or NULL if not found
 */
struct theme_descriptor *ro_gui_theme_get_available(void) {
	struct theme_descriptor *current;
	struct theme_descriptor *test;
	char pathname[256];

	/*	Close any descriptors we've got so far
	*/
	ro_gui_theme_free(theme_descriptors);

	/* scan !NetSurf.Resources.* and our choices directory */
	snprintf(pathname, 256, "%s.Resources", NETSURF_DIR);
	pathname[255] = '\0';
	ro_gui_theme_get_available_in_dir(pathname);
	snprintf(pathname, 256, "%s%s", THEME_PATH_R, THEME_LEAFNAME);
	pathname[255] = '\0';
	ro_gui_theme_get_available_in_dir(pathname);

	/*	Sort alphabetically in a very rubbish way
	*/
	if ((theme_descriptors) && (theme_descriptors->next)) {
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
 * Adds the themes in a directory to the global cache.
 *
 * \param directory  the directory to scan
 */
static void ro_gui_theme_get_available_in_dir(const char *directory) {
	struct theme_file_header file_header;
	struct theme_descriptor *current;
	char pathname[256];
	int context = 0;
	int read_count;
	osgbpb_INFO(100) info;
	int output_left;
	os_fw file_handle;
	os_error *error;

	/*	Create a new set
	*/
	while (context != -1) {
		/*	Get the next entry
		*/
		error = xosgbpb_dir_entries_info(directory,
				(osgbpb_info_list *) &info, 1, context,
				sizeof(info), 0, &read_count, &context);
		if (error) {
			LOG(("xosgbpb_dir_entries_info: 0x%x: %s",
				error->errnum, error->errmess));
			if (error->errnum == 0xd6)	/* no such dir */
				return;
			warn_user("MiscError", error->errmess);
			break;
		}

		/*	Check if we've read anything
		*/
		if (read_count == 0)
			continue;

		/*	Get our full filename
		*/
		snprintf(pathname, sizeof pathname, "%s.%s",
				directory, info.name);
		pathname[sizeof pathname - 1] = 0;

		/*	Only process files
		*/
		if ((info.obj_type == fileswitch_IS_FILE) &&
				(!ro_gui_theme_find(info.name))) {

			/*	Get the header
			*/
			error = xosfind_openinw(osfind_NO_PATH, pathname, 0,
					&file_handle);
			if (error) {
				LOG(("xosfind_openinw: 0x%x: %s",
					error->errnum, error->errmess));
				warn_user("FileError", error->errmess);
				continue;
			}
			if (file_handle == 0)
				continue;
			error = xosgbpb_read_atw(file_handle,
					(char *)&file_header,
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

			/*	Create a new theme descriptor
			*/
			current = (struct theme_descriptor *)calloc(1,
					sizeof(struct theme_descriptor));
			if (!current) {
				LOG(("calloc failed"));
				warn_user("NoMemory", 0);
				return;
			}

			if (!ro_gui_theme_read_file_header(current,
					&file_header)) {
				free(current);
				continue;
			}

			current->filename = malloc(strlen(pathname) + 1);
			if (!current->filename) {
				LOG(("malloc failed"));
				warn_user("NoMemory", 0);
				free(current);
				return;
			}
			strcpy(current->filename, pathname);
			current->leafname = current->filename +
					strlen(directory) + 1;

			/*	Link in our new descriptor
			*/
			if (theme_descriptors) {
				current->next = theme_descriptors;
				theme_descriptors->previous = current;
			}
			theme_descriptors = current;
		}
	}
}


/**
 * Fills in the basic details for a descriptor from a file header.
 * The filename string is not set.
 *
 * \param descriptor   the descriptor to set up
 * \param file_header  the header to read from
 * \return false for a badly formed theme, true otherwise
 */
bool ro_gui_theme_read_file_header(struct theme_descriptor *descriptor,
		struct theme_file_header *file_header) {

	if ((file_header->magic_value != 0x4d54534e) ||
			(file_header->parser_version > 2))
		return false;

	strcpy(descriptor->name, file_header->name);
	strcpy(descriptor->author, file_header->author);
	descriptor->browser_background = file_header->browser_bg;
	descriptor->hotlist_background = file_header->hotlist_bg;
	descriptor->status_background = file_header->status_bg;
	descriptor->status_foreground = file_header->status_fg;
	descriptor->decompressed_size = file_header->decompressed_sprite_size;
	descriptor->compressed_size = file_header->compressed_sprite_size;
	if (file_header->parser_version >= 2) {
		descriptor->throbber_right =
				!(file_header->theme_flags & (1 << 0));
		descriptor->throbber_redraw =
				file_header->theme_flags & (1 << 1);
	} else {
		descriptor->throbber_right =
				(file_header->theme_flags == 0x00);
		descriptor->throbber_redraw = true;
	}
	return true;
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
	struct theme_descriptor *next_descriptor;
	char sprite_name[16];
	bool result = true;
	int i, n;
	int workspace_size, file_size;
	char *raw_data, *workspace;
	osspriteop_area *decompressed;

	/*	If we are freeing the whole of the list then we need to
		start at the first descriptor.
	*/
	if (list && descriptor)
		while (descriptor->previous) descriptor = descriptor->previous;

	/*	Open the themes
	*/
	next_descriptor = descriptor;
	for (; descriptor; descriptor = next_descriptor) {
		/* see if we should iterate through the entire list */
		if (list)
			next_descriptor = descriptor->next;
		else
			next_descriptor = NULL;

		/* if we are already loaded, increase the usage count */
		if (descriptor->theme) {
			descriptor->theme->users = descriptor->theme->users + 1;
			continue;
		}

		/* create a new theme */
		descriptor->theme = (struct theme *)calloc(1,
				sizeof(struct theme));
		if (!descriptor->theme) {
			LOG(("calloc() failed"));
			warn_user("NoMemory", 0);
			continue;
		}
		descriptor->theme->users = 1;

		/* try to load the associated file */
		error = xosfile_read_stamped_no_path(descriptor->filename,
				&obj_type, 0, 0, &file_size, 0, 0);
		if (error) {
			LOG(("xosfile_read_stamped_no_path: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("FileError", error->errmess);
			continue;
		}
		if (obj_type != fileswitch_IS_FILE)
			continue;
		raw_data = malloc(file_size);
		if (!raw_data) {
			LOG(("malloc() failed"));
			warn_user("NoMemory", 0);
			continue;
		}
		error = xosfile_load_stamped_no_path(descriptor->filename,
				(byte *)raw_data, 0, 0, 0, 0, 0);
		if (error) {
			free(raw_data);
			LOG(("xosfile_load_stamped_no_path: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("FileError", error->errmess);
			continue;
		}

		/* decompress the new data */
		error = xsquash_decompress_return_sizes(-1, &workspace_size, 0);
		if (error) {
			free(raw_data);
			LOG(("xsquash_decompress_return_sizes: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
			continue;
		}
		decompressed = (osspriteop_area *)malloc(
				descriptor->decompressed_size);
		workspace = malloc(workspace_size);
		if ((!decompressed) || (!workspace)) {
			free(decompressed);
			free(raw_data);
			LOG(("malloc() failed"));
			warn_user("NoMemory", 0);
			continue;
		}
		error = xsquash_decompress(squash_INPUT_ALL_PRESENT, workspace,
				(byte *)(raw_data + sizeof(
						struct theme_file_header)),
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
			continue;
		}
		if (status != 0) {
			free(decompressed);
			continue;
		}
		descriptor->theme->sprite_area = decompressed;

		/* find the highest sprite called 'throbber%i', and get the
		 * maximum dimensions for all 'thobber%i' icons. */
		for (i = 1; i <= descriptor->theme->sprite_area->sprite_count;
				i++) {
			error = xosspriteop_return_name(osspriteop_USER_AREA,
					descriptor->theme->sprite_area,
					sprite_name, 16, i, 0);
			if (error) {
				LOG(("xosspriteop_return_name: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MiscError", error->errmess);
				continue;
			}
			if (strncmp(sprite_name, "throbber", 8))
				continue;
			
			/* get the max sprite width/height */
			error = xosspriteop_read_sprite_info(
					osspriteop_USER_AREA,
					descriptor->theme->sprite_area,
					(osspriteop_id)sprite_name,
					&dimensions.x, &dimensions.y,
					(osbool *)0, &mode);
			if (error) {
				LOG(("xosspriteop_read_sprite_info: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MiscError", error->errmess);
				continue;
			}
			ro_convert_pixels_to_os_units(&dimensions, mode);
			if (descriptor->theme->throbber_width <	dimensions.x)
				descriptor->theme->throbber_width =
						dimensions.x;
			if (descriptor->theme->throbber_height < dimensions.y)
				descriptor->theme->throbber_height =
						dimensions.y;

			/* get the throbber number */
			n = atoi(sprite_name + 8);
			if (descriptor->theme->throbber_frames < n)
				descriptor->theme->throbber_frames = n;
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

	/* check if the theme is already applied */
	if (descriptor == theme_current)
		return true;

	/* re-open the new-theme and release the current theme */
	if (!ro_gui_theme_open(descriptor, false))
		return false;
	theme_previous = theme_current;
	theme_current = descriptor;

	/* apply the theme to all the current windows */
	ro_gui_window_update_theme();

	/* release the previous theme */
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

	if (!descriptor)
		return;

	/* move to the start of the list */
	while (list && descriptor->previous)
		descriptor = descriptor->previous;

	/* close the themes */
	while (descriptor) {
		if (descriptor->theme) {
			descriptor->theme->users = descriptor->theme->users - 1;
			if (descriptor->theme->users <= 0) {
				free(descriptor->theme->sprite_area);
				free(descriptor->theme);
				descriptor->theme = NULL;
			}
		}
		if (!list)
			return;
		descriptor = descriptor->next;
	}
}


/**
 * Performs the redraw for a toolbar
 *
 * \param redraw   the redraw area
 * \param toolbar  the toolbar to redraw
 */
void ro_gui_theme_redraw(wimp_draw *redraw) {
	struct toolbar *toolbar;

	struct toolbar_icon *icon;
	osbool more;
	wimp_icon separator_icon;
	os_error *error;
	bool perform_redraw = false;

	toolbar = (struct toolbar *)ro_gui_wimp_event_get_user_data(redraw->w);

	assert(toolbar);

	/* set up the icon */
	if ((toolbar->descriptor) && (toolbar->descriptor->theme) &&
			(toolbar->descriptor->theme->sprite_area)) {
		separator_icon.flags = wimp_ICON_SPRITE | wimp_ICON_INDIRECTED |
				wimp_ICON_HCENTRED | wimp_ICON_VCENTRED;
		separator_icon.data.indirected_sprite.id =
				(osspriteop_id)theme_separator_name;
		separator_icon.data.indirected_sprite.area =
				toolbar->descriptor->theme->sprite_area;
		separator_icon.data.indirected_sprite.size = 12;
		separator_icon.extent.y0 = 0;
		separator_icon.extent.y1 = toolbar->height;
		perform_redraw = true;
	}
	perform_redraw &= toolbar->display_buttons || toolbar->editor;

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	while (more) {
		if (perform_redraw)
			for (icon = toolbar->icon; icon; icon = icon->next)
				if ((icon->icon_number == -1) &&
						(icon->display))  {
					separator_icon.extent.x0 = icon->x;
					separator_icon.extent.x1 = icon->x +
							icon->width;
					xwimp_plot_icon(&separator_icon);
				}
		error = xwimp_get_rectangle(redraw, &more);
		if (error) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
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
void ro_gui_theme_free(struct theme_descriptor *descriptor) {
	struct theme_descriptor *next_descriptor;

	if (!descriptor)
		return;

	/* move to the start of the list */
	while (descriptor->previous)
		descriptor = descriptor->previous;

	/* free closed themes */
	next_descriptor = descriptor;
	for (; descriptor; descriptor = next_descriptor) {
		next_descriptor = descriptor->next;

		/* no theme? no descriptor */
		if (!descriptor->theme) {
			if (descriptor->previous)
				descriptor->previous->next = descriptor->next;
			if (descriptor->next)
				descriptor->next->previous =
						descriptor->previous;

			/* keep the cached list in sync */
			if (theme_descriptors == descriptor)
				theme_descriptors = next_descriptor;

			/* release any memory */
			free(descriptor->filename);
			free(descriptor);
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
struct toolbar *ro_gui_theme_create_toolbar(struct theme_descriptor *descriptor,
		toolbar_type type) {
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
			ro_gui_theme_add_toolbar_icons(toolbar,
					theme_browser_icons,
					option_toolbar_browser);
			toolbar->suggest = ro_gui_theme_add_toolbar_icon(NULL,
			"gright",
					ICON_TOOLBAR_SUGGEST);
			break;
		case THEME_HOTLIST_TOOLBAR:
			ro_gui_theme_add_toolbar_icons(toolbar,
					theme_hotlist_icons,
					option_toolbar_hotlist);
			break;
		case THEME_HISTORY_TOOLBAR:
			ro_gui_theme_add_toolbar_icons(toolbar,
					theme_history_icons,
					option_toolbar_history);
			break;
		case THEME_BROWSER_EDIT_TOOLBAR:
			ro_gui_theme_add_toolbar_icons(toolbar,
					theme_browser_icons,
					"0123456789a|");
			break;
		case THEME_HOTLIST_EDIT_TOOLBAR:
			ro_gui_theme_add_toolbar_icons(toolbar,
					theme_hotlist_icons,
					"40123|");
			break;
		case THEME_HISTORY_EDIT_TOOLBAR:
			ro_gui_theme_add_toolbar_icons(toolbar,
					theme_history_icons,
					"0123|");
			break;
	}

	/*	Claim the memory for our Wimp indirection
	*/
	if (type == THEME_BROWSER_TOOLBAR) {
		toolbar->url_buffer = calloc(1, THEME_URL_MEMORY +
				THEME_THROBBER_MEMORY + THEME_STATUS_MEMORY);
		if (!toolbar->url_buffer) {
			LOG(("No memory for calloc()"));
			ro_gui_theme_destroy_toolbar(toolbar);
			return NULL;
		}
		toolbar->throbber_buffer = toolbar->url_buffer +
				THEME_URL_MEMORY;
		toolbar->status_buffer = toolbar->throbber_buffer +
				THEME_THROBBER_MEMORY;
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
bool ro_gui_theme_update_toolbar(struct theme_descriptor *descriptor,
		struct toolbar *toolbar) {
	wimp_icon_create new_icon;
	os_error *error;
	osspriteop_area *sprite_area;
	struct toolbar_icon *toolbar_icon;
	int width, max_icon;
	wimp_icon_flags icon_flags;
	struct gui_window *g;
	if (!toolbar) return false;

	/*	Set the theme and window sprite area
	*/
	if (!descriptor) descriptor = theme_current;
	toolbar->descriptor = descriptor;
	if ((toolbar->descriptor) && (toolbar->descriptor->theme))
		sprite_area = toolbar->descriptor->theme->sprite_area;
	else
		sprite_area = (osspriteop_area *)1;
	theme_toolbar_window.sprite_area = sprite_area;

	/*	Update the icon sizes
	*/
	for (toolbar_icon = toolbar->icon; toolbar_icon;
			toolbar_icon = toolbar_icon->next)
		ro_gui_theme_update_toolbar_icon(toolbar, toolbar_icon);
	if (toolbar->suggest)
		ro_gui_theme_update_toolbar_icon(toolbar, toolbar->suggest);

	/*	Recreate the toolbar window
	*/
	if (toolbar->descriptor) {
		if (toolbar->type == THEME_BROWSER_TOOLBAR)
			theme_toolbar_window.work_bg =
					toolbar->descriptor->browser_background;
		else
			theme_toolbar_window.work_bg =
					toolbar->descriptor->hotlist_background;
	} else {
		theme_toolbar_window.work_bg = wimp_COLOUR_VERY_LIGHT_GREY;
	}

	theme_toolbar_window.work_flags &= ~wimp_ICON_BUTTON_TYPE;
	if ((toolbar->editor) ||
			(toolbar->type == THEME_HOTLIST_EDIT_TOOLBAR) ||
			(toolbar->type == THEME_HISTORY_EDIT_TOOLBAR) ||
			(toolbar->type == THEME_BROWSER_EDIT_TOOLBAR))
		theme_toolbar_window.work_flags |= (wimp_BUTTON_CLICK_DRAG <<
				wimp_ICON_BUTTON_TYPE_SHIFT);
	theme_toolbar_window.flags &= ~wimp_WINDOW_AUTO_REDRAW;
	theme_toolbar_window.flags |= wimp_WINDOW_NO_BOUNDS;
	theme_toolbar_window.xmin = 1;
	theme_toolbar_window.ymin = 1;
	theme_toolbar_window.extent.x1 = 16384;
	theme_toolbar_window.extent.y1 = 16384;
	theme_toolbar_window.sprite_area = sprite_area;
	if (toolbar->toolbar_handle) {
		error = xwimp_delete_window(toolbar->toolbar_handle);
		if (error)
			LOG(("xwimp_delete_window: 0x%x: %s",
					error->errnum, error->errmess));
		ro_gui_wimp_event_finalise(toolbar->toolbar_handle);
		toolbar->toolbar_handle = NULL;
	}
	error = xwimp_create_window(&theme_toolbar_window,
			&toolbar->toolbar_handle);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	ro_gui_wimp_event_register_redraw_window(toolbar->toolbar_handle,
			ro_gui_theme_redraw);
	ro_gui_wimp_event_set_user_data(toolbar->toolbar_handle, toolbar);
	switch (toolbar->type) {
	 	case THEME_BROWSER_TOOLBAR:
	 	case THEME_BROWSER_EDIT_TOOLBAR:
	 		ro_gui_wimp_event_register_mouse_click(toolbar->toolbar_handle,
	 				ro_gui_toolbar_click);
	 		break;
		case THEME_HOTLIST_TOOLBAR:
		case THEME_HOTLIST_EDIT_TOOLBAR:
		case THEME_HISTORY_TOOLBAR:
	  	case THEME_HISTORY_EDIT_TOOLBAR:
			ro_gui_wimp_event_register_mouse_click(toolbar->toolbar_handle,
					ro_gui_tree_toolbar_click);
			break;
	}

	/*	Create the basic icons
	*/
	if ((toolbar->type == THEME_HOTLIST_TOOLBAR) ||
			(toolbar->type == THEME_HOTLIST_EDIT_TOOLBAR))
		max_icon = ICON_TOOLBAR_HOTLIST_LAST;
	else if ((toolbar->type == THEME_HISTORY_TOOLBAR) ||
			(toolbar->type == THEME_HISTORY_EDIT_TOOLBAR))
		max_icon = ICON_TOOLBAR_HISTORY_LAST;
	else
		max_icon = ICON_TOOLBAR_URL;
	new_icon.w = toolbar->toolbar_handle;
	new_icon.icon.data.indirected_text.size = 1;
	new_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_SPRITE |
			wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED |
			wimp_ICON_VCENTRED;
	if ((toolbar->editor) ||
			(toolbar->type == THEME_HOTLIST_EDIT_TOOLBAR) ||
			(toolbar->type == THEME_HISTORY_EDIT_TOOLBAR) ||
			(toolbar->type == THEME_BROWSER_EDIT_TOOLBAR))
		new_icon.icon.flags |= (wimp_BUTTON_CLICK_DRAG <<
				wimp_ICON_BUTTON_TYPE_SHIFT);
	else
		new_icon.icon.flags |= (wimp_BUTTON_CLICK <<
				wimp_ICON_BUTTON_TYPE_SHIFT);
	if (toolbar->descriptor)
		new_icon.icon.flags |= (toolbar->descriptor->browser_background
				 << wimp_ICON_BG_COLOUR_SHIFT);
	else
		new_icon.icon.flags |= (wimp_COLOUR_VERY_LIGHT_GREY
				 << wimp_ICON_BG_COLOUR_SHIFT);
	icon_flags = new_icon.icon.flags;

	for (int i = 0; i < max_icon; i++) {
		new_icon.icon.data.indirected_text.text =
				theme_null_text_string;
		new_icon.icon.data.indirected_text.validation =
				theme_null_text_string;
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
		new_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
				wimp_ICON_VCENTRED | wimp_ICON_BORDER |
				wimp_ICON_FILLED | (wimp_COLOUR_BLACK <<
						wimp_ICON_FG_COLOUR_SHIFT) |
				(wimp_BUTTON_WRITE_CLICK_DRAG <<
						wimp_ICON_BUTTON_TYPE_SHIFT);
		new_icon.icon.data.indirected_text.text = toolbar->url_buffer;
		new_icon.icon.data.indirected_text.validation =
				theme_url_validation;
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
		new_icon.icon.flags = wimp_ICON_SPRITE | wimp_ICON_INDIRECTED |
				wimp_ICON_HCENTRED | wimp_ICON_VCENTRED;
		new_icon.icon.data.indirected_sprite.id =
				(osspriteop_id)toolbar->throbber_buffer;
		new_icon.icon.data.indirected_sprite.area = sprite_area;
		new_icon.icon.data.indirected_sprite.size =
				THEME_THROBBER_MEMORY;
		error = xwimp_create_icon(&new_icon, 0);
		if (error) {
			LOG(("xwimp_create_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

		/*	Now the URL suggestion icon
		*/
		new_icon.icon.data.indirected_text.text =
				theme_null_text_string;
		new_icon.icon.data.indirected_text.size = 1;
		new_icon.icon.flags = icon_flags | (wimp_BUTTON_CLICK <<
				wimp_ICON_BUTTON_TYPE_SHIFT);
		if (toolbar->suggest)
			new_icon.icon.data.indirected_text.validation =
					toolbar->suggest->validation;
		else
			new_icon.icon.data.indirected_text.validation =
					theme_null_text_string;
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
			ro_gui_wimp_event_finalise(toolbar->status_handle);
		}
		if (toolbar->descriptor)
			theme_toolbar_window.work_bg =
					toolbar->descriptor->status_background;
		else
			theme_toolbar_window.work_bg =
					wimp_COLOUR_VERY_LIGHT_GREY;
		theme_toolbar_window.flags &= ~wimp_WINDOW_NO_BOUNDS;
		theme_toolbar_window.flags |= wimp_WINDOW_AUTO_REDRAW;
		theme_toolbar_window.xmin = 12;
		theme_toolbar_window.ymin =
				ro_get_hscroll_height((wimp_w)0) - 4;
		theme_toolbar_window.extent.y1 = theme_toolbar_window.ymin;
		error = xwimp_create_window(&theme_toolbar_window,
				&toolbar->status_handle);
		if (error) {
			LOG(("xwimp_create_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}
 		ro_gui_wimp_event_register_mouse_click(toolbar->status_handle,
 				ro_gui_status_click);
 		ro_gui_wimp_event_set_help_prefix(toolbar->status_handle, "HelpStatus");

		/*	Create the status resize icon
		*/
		new_icon.w = toolbar->status_handle;
		new_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
				wimp_ICON_BORDER | wimp_ICON_FILLED |
				(wimp_COLOUR_LIGHT_GREY <<
						wimp_ICON_BG_COLOUR_SHIFT) |
				(wimp_BUTTON_CLICK_DRAG <<
						wimp_ICON_BUTTON_TYPE_SHIFT);
		new_icon.icon.data.indirected_text.text =
				theme_null_text_string;
		new_icon.icon.data.indirected_text.validation =
				theme_resize_validation;
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
		new_icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
				wimp_ICON_VCENTRED;
		if (toolbar->descriptor)
			new_icon.icon.flags |=
				(toolbar->descriptor->status_foreground <<
						wimp_ICON_FG_COLOUR_SHIFT) |
				(toolbar->descriptor->status_background <<
						wimp_ICON_BG_COLOUR_SHIFT);
		else
			new_icon.icon.flags |=
				(wimp_COLOUR_BLACK <<
						wimp_ICON_FG_COLOUR_SHIFT) |
				(wimp_COLOUR_VERY_LIGHT_GREY <<
						wimp_ICON_BG_COLOUR_SHIFT);
		new_icon.icon.data.indirected_text.text =
				toolbar->status_buffer;
		new_icon.icon.data.indirected_text.validation =
				theme_null_text_string;
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

	/*	Keep menus up to date etc
	*/
	ro_gui_theme_set_help_prefix(toolbar);
	switch (toolbar->type) {
		case THEME_BROWSER_TOOLBAR:
			g = ro_gui_window_lookup(toolbar->parent_handle);
			if (g)
				ro_gui_prepare_navigate(g);
			break;
		case THEME_HOTLIST_TOOLBAR:
		case THEME_HISTORY_TOOLBAR:
			ro_gui_menu_prepare_action(toolbar->parent_handle,
					TREE_SELECTION, false);
			break;
		default:
			break;
	}
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
	int height;
	int full_height;
	os_error *error;

	if (!toolbar) return false;

	/*	Attach/close the windows
	*/
	toolbar->parent_handle = parent;
	height = ro_gui_theme_toolbar_height(toolbar);
	full_height = ro_gui_theme_toolbar_full_height(toolbar);
	if (height > 0) {
		outline.w = parent;
		xwimp_get_window_outline(&outline);
		state.w = parent;
		xwimp_get_window_state(&state);
		state.w = toolbar->toolbar_handle;
		state.visible.x1 = outline.outline.x1 - 2;
		state.visible.y0 = state.visible.y1 - height + 2;
		state.xscroll = 0;
		state.yscroll = toolbar->height - 2; /* clipped by the WIMP */
		error = xwimp_open_window_nested((wimp_open *)&state, parent,
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
		if (error) {
			LOG(("xwimp_open_window_nested: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}
		if (!toolbar->editor)
			return true;

		state.w = toolbar->editor->toolbar_handle;
		state.visible.y1 -= toolbar->height;
		state.yscroll = toolbar->editor->height - 2;
		error = xwimp_open_window_nested((wimp_open *)&state,
				toolbar->toolbar_handle,
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
		if (error) {
			LOG(("xwimp_open_window_nested: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}
		return true;
	}
	
	error = xwimp_close_window(toolbar->toolbar_handle);
	if (error) {
		LOG(("xwimp_close_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
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
	parent_size = outline.outline.x1 - outline.outline.x0 -
			ro_get_vscroll_width(parent) - 2;

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
	if (status_size <= 12)
		status_size = 0;

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
	wimp_outline outline;
	wimp_window_state state;
	int height = -1;
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
	int collapse_height;

	/* find the parent window handle if we need to process the status
	 * window, or the caller has requested we calculate the width ourself */
	if ((toolbar->parent_handle) && ((width == -1) ||
			((toolbar->status_handle) &&
			(toolbar->display_status)))) {
		outline.w = toolbar->parent_handle;
		error = xwimp_get_window_outline(&outline);
		if (error) {
			LOG(("xwimp_get_window_outline: 0x%x: %s",
				error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}
		if (width == -1)
			width = outline.outline.x1 - outline.outline.x0 - 2;
	}

	/*	Find the parent visible height to clip our toolbar height to
	*/
	if ((toolbar->toolbar_handle) && (toolbar->parent_handle)) {
		/*	Get the current state
		*/
		state.w = toolbar->parent_handle;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

		height = state.visible.y1 - state.visible.y0 + 2;

		/*	We can't obscure the height of the scroll bar as we lose the resize
			icon if we do.
		*/
		if ((state.flags & wimp_WINDOW_SIZE_ICON) &&
				!(state.flags & wimp_WINDOW_HSCROLL))
			height -= ro_get_hscroll_height(0) - 2;

		/*	Update our position
		*/
		if (height != toolbar->max_height) {
			if ((state.flags & wimp_WINDOW_SIZE_ICON) &&
					!(state.flags & wimp_WINDOW_HSCROLL) &&
					(toolbar->height > toolbar->max_height))
				xwimp_force_redraw(toolbar->parent_handle,
					0, -16384, 16384, 16384);
			toolbar->max_height = height;
			collapse_height = toolbar->height +
					(toolbar->editor ? toolbar->editor->height : 0);
			ro_gui_theme_attach_toolbar(toolbar, toolbar->parent_handle);
			if ((state.flags & wimp_WINDOW_SIZE_ICON) &&
					!(state.flags & wimp_WINDOW_HSCROLL) &&
					(collapse_height > toolbar->max_height))
				xwimp_force_redraw(toolbar->parent_handle,
					0, -16384, 16384, 16384);
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
			if (toolbar->height < 52 + 8)
				toolbar->height = 52 + 8;
			if ((toolbar->suggest) && (toolbar->height < (toolbar->suggest->height + 8)))
				toolbar->height = toolbar->suggest->height + 8;
		}

		/*	Get the minimum height of the icons
		*/
		bottom_edge = left_edge;
		if ((toolbar->display_buttons || toolbar->editor) && (toolbar->descriptor) &&
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
			if (toolbar->display_url) {
				bottom_edge += 64;
				if (toolbar->suggest)
					bottom_edge += toolbar->suggest->width + 8;
			}
			if (bottom_edge > right_edge)
				right_edge = bottom_edge;
			if ((toolbar->descriptor) && (toolbar->descriptor->theme) &&
					(toolbar->display_throbber) &&
					(toolbar->descriptor->throbber_right)) {
				bottom_edge += toolbar->descriptor->theme->throbber_width;
				if (bottom_edge > right_edge) right_edge = bottom_edge;
				throbber_x = right_edge - toolbar->descriptor->theme->throbber_width;
				right_edge -= toolbar->descriptor->theme->throbber_width + 8;
			}
		}

		if (toolbar->height != 0)
			toolbar->height += 2;
		if (toolbar->reformat_buttons) {
			/*	Hide the URL bar if we should
			*/
			if ((!toolbar->display_url) && (toolbar->type == THEME_BROWSER_TOOLBAR)) {
				if (!xwimp_get_caret_position(&caret)) {
					if ((caret.w == toolbar->toolbar_handle) &&
							(caret.i == ICON_TOOLBAR_URL)) {
						if (toolbar->parent_handle)
							xwimp_set_caret_position(toolbar->parent_handle,
									wimp_ICON_WINDOW,
									-100, -100, 32, -1);
						else
							xwimp_set_caret_position((wimp_w)-1,
									0, 0, 0, 0, 0);
					}
				}
				xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_URL,
					0, -16384, 0, -16384);
				xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_SUGGEST,
					0, -16384, 0, -16384);
			} else if (toolbar->type == THEME_BROWSER_TOOLBAR) {
				ro_gui_set_icon_shaded_state(toolbar->toolbar_handle, ICON_TOOLBAR_URL,
					!toolbar->display_url);
			}
			xwimp_force_redraw(toolbar->toolbar_handle,
				0, 0, 16384, 16384);

			/*	Move the buttons
			*/
			toolbar_icon = toolbar->icon;
			while (toolbar_icon) {
				if ((toolbar->display_buttons || toolbar->editor) &&
						(toolbar_icon->display)
						&& (toolbar_icon->width > 0)) {
					visible_icon = true;
					bottom_edge = (toolbar->height -
							toolbar_icon->height) / 2;
					toolbar_icon->x = left_edge;
					toolbar_icon->y = bottom_edge;
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


		if (toolbar->type == THEME_BROWSER_TOOLBAR) {
			/*	Move the URL bar
			*/
			if (toolbar->display_url) {
				if (toolbar->suggest) {
					xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_URL,
						left_edge, (toolbar->height / 2) - 26,
						right_edge - toolbar->suggest->width - 8,
						(toolbar->height / 2) + 26);
					xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_SUGGEST,
						right_edge - toolbar->suggest->width,
						(toolbar->height - toolbar->suggest->height) / 2,
						right_edge,
						(toolbar->height + toolbar->suggest->height) / 2);
					xwimp_force_redraw(toolbar->toolbar_handle,
						right_edge - toolbar->suggest->width - 8, 0,
						16384, 16384);
				} else {
					xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_URL,
						left_edge, (toolbar->height / 2) - 26,
						right_edge, (toolbar->height / 2) + 26);
					xwimp_force_redraw(toolbar->toolbar_handle,
						right_edge, 0, 16384, 16384);
				}
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
				if (toolbar->descriptor->throbber_right) {
					xwimp_force_redraw(toolbar->toolbar_handle,
						old_width - width + throbber_x, 0, 16384, 16384);
					xwimp_force_redraw(toolbar->toolbar_handle,
						throbber_x, 0, 16384, 16384);
				}

			} else {
				xwimp_resize_icon(toolbar->toolbar_handle, ICON_TOOLBAR_THROBBER,
					0, -16384, 0, -16384);
			}
		}

		/*	Re-attach to the parent
		*/
		toolbar->toolbar_current = width;
		if (toolbar->reformat_buttons) {
			extent.x1 = 16384;
			extent.y0 = (toolbar->editor ? -toolbar->editor->height : 0);
			extent.y1 = toolbar->height - 2;
			xwimp_set_extent(toolbar->toolbar_handle, &extent);
			if ((toolbar->parent_handle) && (old_height != toolbar->height))
				ro_gui_theme_attach_toolbar(toolbar, toolbar->parent_handle);
		}
		toolbar->reformat_buttons = false;
	}

	/*	Reformat the status bar
	*/
	if ((toolbar->status_handle) && (toolbar->parent_handle)) {
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
			status_max = width - ro_get_vscroll_width(toolbar->parent_handle);
			status_size = (status_max * toolbar->status_width) / 10000;
			if (status_size < 12) status_size = 12;
			status_height = ro_get_hscroll_height(toolbar->parent_handle) - 2;

			/*	Update the extent
			*/
			extent.x0 = 0;
			extent.y0 = 0;
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
			xwimp_open_window_nested((wimp_open *)&state,
					toolbar->parent_handle,
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

	/*	Destroy our editor
	*/
	if (toolbar->editor) {
		toolbar->editor = NULL;
		ro_gui_theme_destroy_toolbar(toolbar->editor);
	}

	/*	Delete our windows
	*/
	if (toolbar->toolbar_handle) {
		xwimp_delete_window(toolbar->toolbar_handle);
		ro_gui_wimp_event_finalise(toolbar->toolbar_handle);
	}
	if (toolbar->status_handle) {
		xwimp_delete_window(toolbar->status_handle);
		ro_gui_wimp_event_finalise(toolbar->status_handle);

        }
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
	ro_gui_theme_destroy_toolbar_icon(toolbar->suggest);
	free(toolbar);
}


/**
 * Toggles the toolbar editing mode
 *
 * \param toolbar      the toolbar to toggle editing for
 */
void ro_gui_theme_toggle_edit(struct toolbar *toolbar) {
	int height;
	int icons = 0;
	struct toolbar_icon *icon;
	struct gui_window *g = NULL;
	wimp_window_state state;
	os_error *error;
	char *option;
	char hex_no[4];

	if (!toolbar)
		return;

	if ((toolbar->type == THEME_BROWSER_TOOLBAR) &&
			(toolbar->parent_handle))
		g = ro_gui_window_lookup(toolbar->parent_handle);

	if (toolbar->editor) {
		/* save options */
		icons = 0;
		for (icon = toolbar->icon; icon; icon = icon->next)
			if (icon->display) icons++;
		option = calloc(icons + 1, 1);
		if (!option) {
			LOG(("No memory to save toolbar options"));
			warn_user("NoMemory", 0);
		} else {
			icons = 0;
			for (icon = toolbar->icon; icon; icon = icon->next)
				if (icon->display) {
					if (icon->icon_number == -1) {
						option[icons] = '|';
					} else {
						sprintf(hex_no, "%x", icon->icon_number);
						option[icons] = hex_no[0];
					}
					icons++;
				}
			switch (toolbar->type) {
				case THEME_BROWSER_TOOLBAR:
					free(option_toolbar_browser);
					option_toolbar_browser = option;
					break;
				case THEME_HOTLIST_TOOLBAR:
					free(option_toolbar_hotlist);
					option_toolbar_hotlist = option;
					break;
				case THEME_HISTORY_TOOLBAR:
					free(option_toolbar_history);
					option_toolbar_history = option;
					break;
				default:
					break;
			}
			ro_gui_save_options();
		}

		/* turn off editing */
		height = toolbar->editor->height;
		ro_gui_theme_destroy_toolbar(toolbar->editor);
		toolbar->editor = NULL;
		ro_gui_theme_update_toolbar(toolbar->descriptor, toolbar);
		switch (toolbar->type) {
			case THEME_BROWSER_TOOLBAR:
				if (g)
					ro_gui_window_update_dimensions(g, height);
				break;
			default:
				if (toolbar->parent_handle)
					xwimp_force_redraw(toolbar->parent_handle,
							0, -16384, 16384, 16384);
				break;
		}
	} else {
		/* create/initialise the toolbar editor */
		switch (toolbar->type) {
			case THEME_BROWSER_TOOLBAR:
				toolbar->editor = ro_gui_theme_create_toolbar(toolbar->descriptor,
						THEME_BROWSER_EDIT_TOOLBAR);
				break;
			case THEME_HOTLIST_TOOLBAR:
				toolbar->editor = ro_gui_theme_create_toolbar(toolbar->descriptor,
						THEME_HOTLIST_EDIT_TOOLBAR);
				break;
			case THEME_HISTORY_TOOLBAR:
				toolbar->editor = ro_gui_theme_create_toolbar(toolbar->descriptor,
						THEME_HISTORY_EDIT_TOOLBAR);
				break;
			default:
				return;
		}
		if (!toolbar->editor) {
			LOG(("Unable to create toolbar editor"));
			return;
		}
		ro_gui_theme_update_toolbar(toolbar->descriptor, toolbar);
		switch (toolbar->type) {
			case THEME_BROWSER_TOOLBAR:
				if (g)
					ro_gui_window_update_dimensions(g, -toolbar->editor->height);
				break;
			default:
				if (toolbar->parent_handle) {
					state.w = toolbar->parent_handle;
					error = xwimp_get_window_state(&state);
					if (error) {
						LOG(("xwimp_get_window_state: 0x%x: %s",
							error->errnum, error->errmess));
						warn_user("WimpError", error->errmess);
						return;
					}
					ro_gui_open_window_request((wimp_open *)&state);
					xwimp_force_redraw(toolbar->parent_handle,
							0, -16384, 16384, 16384);
				}
				break;
		}
		ro_gui_theme_process_toolbar(toolbar, -1);
		ro_gui_theme_toolbar_editor_sync(toolbar);
	}
	ro_gui_theme_set_help_prefix(toolbar);
}


/**
 * Synchronise a toolbar window with the associated editor.
 *
 * \param toolbar  the toolbar to synchronise
 */
void ro_gui_theme_toolbar_editor_sync(struct toolbar *toolbar) {
	struct toolbar_icon *icon;
	struct toolbar_icon *icon_edit;

	if ((!toolbar) || (!toolbar->editor))
		return;

	for (icon = toolbar->icon; icon; icon = icon->next)
		if ((icon->icon_number >= 0) && (icon->width > 0))
			for (icon_edit = toolbar->editor->icon; icon_edit;
					icon_edit = icon_edit->next)
				if (icon_edit->icon_number == icon->icon_number)
					ro_gui_set_icon_shaded_state(toolbar->editor->toolbar_handle,
							icon_edit->icon_number, icon->display);
}


/**
 * Handle a toolbar click during an editor session
 *
 * \param toolbar  the base toolbar (ie not editor) to respond to a click for
 * \param pointer  the WIMP pointer details
 */
void ro_gui_theme_toolbar_editor_click(struct toolbar *toolbar,
		wimp_pointer *pointer) {
	wimp_window_state state;
	os_error *error;
	os_box box;

	if (!toolbar->editor)
		return;
	if ((pointer->buttons != (wimp_CLICK_SELECT << 4)) &&
			(pointer->buttons != (wimp_CLICK_ADJUST << 4)))
		return;

	state.w = pointer->w;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	gui_current_drag_type = GUI_DRAG_TOOLBAR_CONFIG;
	theme_toolbar_drag = toolbar;
	theme_toolbar_editor_drag = !(pointer->w == toolbar->toolbar_handle);
	if (theme_toolbar_editor_drag)
		theme_toolbar_icon_drag =
				ro_gui_theme_toolbar_get_icon(toolbar->editor,
				pointer->pos.x - state.visible.x0,
				state.visible.y1 - pointer->pos.y);
	else
		theme_toolbar_icon_drag =
				ro_gui_theme_toolbar_get_icon(toolbar,
				pointer->pos.x - state.visible.x0,
				state.visible.y1 - pointer->pos.y);
	if (!theme_toolbar_icon_drag)
		return;
	if ((theme_toolbar_icon_drag->icon_number >= 0) &&
			(pointer->w == toolbar->editor->toolbar_handle) &&
			(ro_gui_get_icon_shaded_state(
					toolbar->editor->toolbar_handle,
					theme_toolbar_icon_drag->icon_number)))
		return;

	box.x0 = pointer->pos.x - theme_toolbar_icon_drag->width / 2;
	box.x1 = box.x0 + theme_toolbar_icon_drag->width;
	box.y0 = pointer->pos.y - theme_toolbar_icon_drag->height / 2;
	box.y1 = box.y0 + theme_toolbar_icon_drag->height;
	error = xdragasprite_start(dragasprite_HPOS_CENTRE |
			dragasprite_VPOS_CENTRE |
			dragasprite_BOUND_POINTER |
			dragasprite_DROP_SHADOW,
			toolbar->descriptor->theme->sprite_area,
			theme_toolbar_icon_drag->name, &box, 0);
	if (error)
		LOG(("xdragasprite_start: 0x%x: %s",
				error->errnum, error->errmess));
}


/**
 * Handle the end of a drag
 *
 * \param drag  the details for the drag end
 */
void ro_gui_theme_toolbar_editor_drag_end(wimp_dragged *drag) {
	wimp_window_state state;
	os_error *error;
	wimp_pointer pointer;
	struct toolbar_icon *insert_icon;
	struct toolbar_icon *local_icon = NULL;
	struct toolbar_icon *icon;
	bool before;

	if ((!theme_toolbar_drag) || (!theme_toolbar_icon_drag) ||
			(!theme_toolbar_drag->editor))
		return;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (pointer.w == theme_toolbar_drag->toolbar_handle) {
		/* drag from editor or toolbar to toolbar */
		state.w = pointer.w;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
		insert_icon = ro_gui_theme_toolbar_get_insert_icon(
				theme_toolbar_drag,
				pointer.pos.x - state.visible.x0,
				state.visible.y1 - pointer.pos.y, &before);
		if (theme_toolbar_icon_drag->icon_number == -1) {
			if (theme_toolbar_editor_drag) {
				theme_toolbar_icon_drag =
						ro_gui_theme_add_toolbar_icon(
							theme_toolbar_drag,
							NULL, -1);
				ro_gui_theme_update_toolbar_icon(
						theme_toolbar_drag,
						theme_toolbar_icon_drag);
			}
			/* move the separator */
			if (theme_toolbar_icon_drag != insert_icon) {
				ro_gui_theme_delink_toolbar_icon(
						theme_toolbar_drag,
						theme_toolbar_icon_drag);
				ro_gui_theme_link_toolbar_icon(
						theme_toolbar_drag,
						theme_toolbar_icon_drag,
						insert_icon, before);
			}
		} else {
			/* move/enable the icon */
			for (icon = theme_toolbar_drag->icon; icon;
					icon = icon->next)
				if (theme_toolbar_icon_drag->icon_number ==
						icon->icon_number)
					local_icon = icon;
			if (!local_icon)
				return;
			if (local_icon != insert_icon) {
				ro_gui_theme_delink_toolbar_icon(
						theme_toolbar_drag, local_icon);
				ro_gui_theme_link_toolbar_icon(
						theme_toolbar_drag, local_icon,
						insert_icon, before);
			}
			local_icon->display = true;
		}
	} else if ((pointer.w == theme_toolbar_drag->editor->toolbar_handle) &&
			(!theme_toolbar_editor_drag)) {
		/* drag from toolbar to editor */
		if (theme_toolbar_icon_drag->icon_number == -1) {
			/* delete separators */
			ro_gui_theme_delink_toolbar_icon(theme_toolbar_drag,
					theme_toolbar_icon_drag);
			ro_gui_theme_destroy_toolbar_icon(
					theme_toolbar_icon_drag);
		} else {
			/* hide icons */
			theme_toolbar_icon_drag->display = false;
		}
	}
	theme_toolbar_drag->reformat_buttons = true;
	ro_gui_theme_process_toolbar(theme_toolbar_drag, -1);
	ro_gui_theme_toolbar_editor_sync(theme_toolbar_drag);
}


/**
 * Adds a toolbar icon to the end of a toolbar
 *
 * \param toolbar      the toolbar to add the icon to the end of (or NULL)
 * \param name	       the icon name, or NULL for a separator
 * \param icon_number  RISC OS wimp icon number for the icon (not separators)
 */
struct toolbar_icon *ro_gui_theme_add_toolbar_icon(struct toolbar *toolbar,
		const char *name, int icon_number) {
	struct toolbar_icon *toolbar_icon;
	struct toolbar_icon *link_icon;

	/*	Separators are really a sprite called "separator"
	*/
	if (name == NULL) {
		name = "separator";
		icon_number = -1;
	}

	/*	Create a new toolbar
	*/
	toolbar_icon = calloc(sizeof(struct toolbar_icon), 1);
	if (!toolbar_icon) {
		LOG(("No memory for malloc()"));
		warn_user("NoMemory", 0);
		return NULL;
	}

	/*	Set up and link in the icon
	*/
	sprintf(toolbar_icon->name, name);
	sprintf(toolbar_icon->validation, "R5;S%s,p%s", name, name);
	toolbar_icon->icon_number = icon_number;
	toolbar_icon->display = true;
	if (toolbar) {
		if (!toolbar->icon) {
			toolbar->icon = toolbar_icon;
		} else {
			link_icon = toolbar->icon;
			while (link_icon->next) link_icon = link_icon->next;
			link_icon->next = toolbar_icon;
		}
	}
	return toolbar_icon;
}


/**
 * Updates a toolbar icon with respect to the associated sprite.
 *
 * \param icon	   the toolbar icon to update
 */
void ro_gui_theme_update_toolbar_icon(struct toolbar *toolbar,
		struct toolbar_icon *icon) {
	os_coord dimensions = {0, 0};
	os_mode mode;
	os_error *error = NULL;
	int default_width = 0;
	osspriteop_area *sprite_area = NULL;

	/*	Separators default to a width of 16
	*/
	if (icon->icon_number == -1) default_width = 16;

	/*	Handle no theme/no sprite area
	*/
	if (!toolbar)
		return;
	if ((toolbar->descriptor) && (toolbar->descriptor->theme))
		sprite_area = toolbar->descriptor->theme->sprite_area;

	/*	Get the sprite details
	*/
	if (sprite_area)
		error = xosspriteop_read_sprite_info(osspriteop_USER_AREA,
				sprite_area, (osspriteop_id)icon->name,
				&dimensions.x, &dimensions.y, 0, &mode);

	/* fallback to user area just for 'gright' */
	if ((error || !sprite_area) && (!strcmp(icon->name, "gright")))
		error = xwimpspriteop_read_sprite_info(icon->name,
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


/**
 * Links a toolbar icon
 *
 * \param icon	   the toolbar icon to link
 */
void ro_gui_theme_link_toolbar_icon(struct toolbar *toolbar,
		struct toolbar_icon *icon, struct toolbar_icon *link,
		bool before) {
	struct toolbar_icon *temp;
	assert(toolbar);
	assert(icon);
	assert(icon != link);

	/* no icon set, no link icon, or insert at head of list */
	if ((!toolbar->icon) || (!link) ||
			(before && (toolbar->icon == link))) {
		if (toolbar->icon != icon) {
			icon->next = toolbar->icon;
			toolbar->icon = icon;
		}
		return;
	}

	if (before) {
		for (temp = toolbar->icon; temp; temp = temp->next)
			if (temp->next == link) {
				temp->next = icon;
				icon->next = link;
				return;
			}
	} else {
		icon->next = link->next;
		link->next = icon;
	}
}

/**
 * Delinks a toolbar icon
 *
 * \param icon	   the toolbar icon to delink
 */
void ro_gui_theme_delink_toolbar_icon(struct toolbar *toolbar,
		struct toolbar_icon *icon) {
	struct toolbar_icon *link;
	assert(toolbar);
	assert(icon);

	if (toolbar->icon == icon) {
		toolbar->icon = icon->next;
		icon->next = NULL;
		return;
	}

	for (link = toolbar->icon; link; link = link->next)
		if (link->next == icon) {
			link->next = icon->next;
			icon->next = NULL;
			return;
		}
}


/**
 * Returns the toolbar icon at a specified position
 *
 * \param toolbar  the toolbar to examine
 * \param x	   the x co-ordinate to check
 * \param y	   the y co-ordinate to check
 * \return the toolbar icon at the specified position, or NULL for no icon
 */
struct toolbar_icon *ro_gui_theme_toolbar_get_icon(struct toolbar *toolbar,
		int x, int y) {
	struct toolbar_icon *icon;

	for (icon = toolbar->icon; icon; icon = icon->next)
		if ((icon->display) && (icon->width > 0) &&
				(icon->x <= x) && (icon->y <= y) &&
				(icon->x + icon->width > x) &&
				(icon->y + icon->height > y))
			return icon;
	return NULL;
}


/**
 * Returns the toolbar icon closest to the specified position, and whether the
 * position is before (left) or after (right) of it.
 *
 * \param toolbar  the toolbar to examine
 * \param x	   the x co-ordinate to check
 * \param y	   the y co-ordinate to check
 * \return the toolbar icon closest to the specified position, or NULL
 */
struct toolbar_icon *ro_gui_theme_toolbar_get_insert_icon(
		struct toolbar *toolbar, int x, int y, bool *before) {
	struct toolbar_icon *match = NULL;
	struct toolbar_icon *icon;
	int closest = 65536;
	int distance;

	if (!toolbar->icon)
		return NULL;

	for (icon = toolbar->icon; icon; icon = icon->next) {
		if ((icon->display) && (icon->width > 0)) {
			distance = icon->x + icon->width / 2 - x;
			if (distance < 0)
				distance = -distance;
			if (distance < closest) {
				closest = distance;
				match = icon;
				*before = (icon->x + icon->width / 2 - x) > 0;
			}
		}
	}
	return match;
}


/**
 * Sets up a toolbar with icons according to an identifier string
 */
void ro_gui_theme_add_toolbar_icons(struct toolbar *toolbar,
		const char* icons[], const char* ident) {
	struct toolbar_icon *icon;
	int index = 0;
	int number = 0;
	char hex_no[4];

	/* step 1: add all main icons in their correct state */
	while (icons[index]) {
		icon = ro_gui_theme_add_toolbar_icon(toolbar, icons[index],
				index);
		sprintf(hex_no, "%x", index);
		if ((icon) && (!strchr(ident, hex_no[0])))
			icon->display = false;
		index++;
	}

	/* step 2: re-order and add separators */
	index = strlen(ident);
	while (index--) {
		if (ident[index] == '|') {
			icon = ro_gui_theme_add_toolbar_icon(NULL, NULL, -1);
			if (icon)
				ro_gui_theme_link_toolbar_icon(toolbar, icon,
						NULL, NULL);
		} else {
			hex_no[0] = ident[index];
			hex_no[1] = '\0';
			number = strtol(hex_no, NULL, 16);
			for (icon = toolbar->icon; icon; icon = icon->next)
				if (icon->icon_number == number) {
					ro_gui_theme_delink_toolbar_icon(
							toolbar, icon);
					ro_gui_theme_link_toolbar_icon(toolbar,
							icon, NULL, NULL);
				}
		}
	}
}


/**
 * Sets the correct help prefix for a toolbar
 */
void ro_gui_theme_set_help_prefix(struct toolbar *toolbar) {
	if (toolbar->editor) {
		ro_gui_wimp_event_set_help_prefix(toolbar->toolbar_handle, "HelpEditToolbar");
		return;
	}
	switch (toolbar->type) {
	 	case THEME_BROWSER_TOOLBAR:
			ro_gui_wimp_event_set_help_prefix(toolbar->toolbar_handle,
					"HelpToolbar");
			break;
		case THEME_HOTLIST_TOOLBAR:
			ro_gui_wimp_event_set_help_prefix(toolbar->toolbar_handle,
					"HelpHotToolbar");
			break;
		case THEME_HISTORY_TOOLBAR:
			ro_gui_wimp_event_set_help_prefix(toolbar->toolbar_handle,
					"HelpGHistToolbar");
			break;
	 	case THEME_BROWSER_EDIT_TOOLBAR:
		case THEME_HOTLIST_EDIT_TOOLBAR:
	  	case THEME_HISTORY_EDIT_TOOLBAR:
			ro_gui_wimp_event_set_help_prefix(toolbar->toolbar_handle,
					"HelpEditToolbar");
			break;
	}
}
