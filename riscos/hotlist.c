/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Hotlist (implementation).
 */

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <swis.h>
#include "libxml/HTMLparser.h"
#include "libxml/HTMLtree.h"
#include "oslib/colourtrans.h"
#include "oslib/dragasprite.h"
#include "oslib/osfile.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/theme.h"
#include "netsurf/riscos/tinct.h"
#include "netsurf/riscos/toolbar.h"
#include "netsurf/riscos/wimp.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"
#include "netsurf/utils/url.h"

#define HOTLIST_EXPAND 0
#define HOTLIST_COLLAPSE 1
#define HOTLIST_ENTRY 2
#define HOTLIST_LINE 3
#define HOTLIST_TLINE 4
#define HOTLIST_BLINE 5

#define HOTLIST_TEXT_BUFFER 256

#define HOTLIST_LEAF_INSET 32
#define HOTLIST_ICON_WIDTH 36
#define HOTLIST_LINE_HEIGHT 44
#define HOTLIST_TEXT_PADDING 16

struct hotlist_entry {

	/**	The next hotlist entry at this level, or NULL for no more
	*/
	struct hotlist_entry *next_entry;

	/**	The child hotlist entry (NULL for no children).
		The children value must be set for this value to take effect.
	*/
	struct hotlist_entry *child_entry;

	/**	The hotlist entry that has this entry as its next entry
	*/
	struct hotlist_entry *previous_entry;

	/**	The hotlist entry that this is a child of
	*/
	struct hotlist_entry *parent_entry;

	/**	The number of children (-1 for non-folders, >=0 for folders)
	*/
	int children;

	/**	The title of the hotlist entry/folder, UTF-8.
	*/
	char *title;

	/**	The URL of the hotlist entry (NULL for folders)
	*/
	char *url;

	/**	Whether this entry is expanded
	*/
	bool expanded;

	/**	Whether this entry is selected
	*/
	bool selected;

	/**	The content filetype (not for folders)
	*/
	int filetype;

	/**	The number of visits
	*/
	int visits;

	/**	Add/last visit dates
	*/
	time_t add_date;
	time_t last_date;

	/**	Position on last reformat (relative to window origin)
	*/
	int x0;
	int y0;
	int width;
	int height;

	/**	Cached values
	*/
	int collapsed_width;
	int expanded_width;

	/**	The width of the various lines sub-text
	*/
	int widths[4];

	/**	Whether the item is awaiting processing
	*/
	bool process;
};


/*	A basic window for the toolbar and status
*/
static wimp_window hotlist_window_definition = {
	{0, 0, 600, 800},
	0,
	0,
	wimp_TOP,
	wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_MOVEABLE | wimp_WINDOW_BACK_ICON |
			wimp_WINDOW_CLOSE_ICON | wimp_WINDOW_TITLE_ICON |
			wimp_WINDOW_TOGGLE_ICON | wimp_WINDOW_SIZE_ICON |
			wimp_WINDOW_VSCROLL,
	wimp_COLOUR_BLACK,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_WHITE,
	wimp_COLOUR_DARK_GREY,
	wimp_COLOUR_MID_LIGHT_GREY,
	wimp_COLOUR_CREAM,
	0,
	{0, -800, 16384, 0},
	wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_HCENTRED |
			wimp_ICON_VCENTRED,
	wimp_BUTTON_DOUBLE_CLICK_DRAG << wimp_ICON_BUTTON_TYPE_SHIFT,
	wimpspriteop_AREA,
	1,
	256,
	{""},
	0
};

/*	An icon to plot text with
*/
static wimp_icon text_icon;
static wimp_icon sprite_icon;

/*	Temporary workspace for plotting
*/
static char drag_name[12];
static char icon_name[12];
static char extended_text[HOTLIST_TEXT_BUFFER];

/*	Whether a reformat is pending
*/
static bool reformat_pending = false;
static int max_width = 0;
static int max_height = 0;

/*	The hotlist window, toolbar and plot origins
*/
wimp_w hotlist_window;
struct toolbar *hotlist_toolbar = NULL;
static int origin_x, origin_y;

/*	The current redraw rectangle
*/
static int clip_x0, clip_y0, clip_x1, clip_y1;

/*	The root entry
*/
static struct hotlist_entry root;

/*	The sprite header addresses for Tinct
*/
static char *sprite[6];

/*	The drag buttons
*/
bool dragging;
wimp_mouse_state drag_buttons;

/*	Whether the current selection was from a menu click
*/
bool menu_selection = false;
bool menu_open = false;

/*	Whether the editing facilities are for add so that we know how
	to reset the dialog boxes on a adjust-cancel and the action to
	perform on ok.
*/
bool dialog_folder_add = false;
bool dialog_entry_add = false;
bool hotlist_insert = false;


static bool ro_gui_hotlist_initialise_sprite(const char *name, int number);
static bool ro_gui_hotlist_load(void);
static void ro_gui_hotlist_load_file(const char *filename);
static void ro_gui_hotlist_load_directory(xmlNode *ul,
		struct hotlist_entry *directory);
static void ro_gui_hotlist_load_entry(xmlNode *li,
		struct hotlist_entry *directory);
xmlNode *ro_gui_hotlist_find_element(xmlNode *node, const char *name);
bool ro_gui_hotlist_save_directory(struct hotlist_entry *directory,
		xmlNode *node);
bool ro_gui_hotlist_save_entry(struct hotlist_entry *entry,
		xmlNode *node);
bool ro_gui_hotlist_save_entry_comment(xmlNode *node,
		const char *name, int value);
static void ro_gui_hotlist_link_entry(struct hotlist_entry *link, struct hotlist_entry *entry, bool before);
static void ro_gui_hotlist_delink_entry(struct hotlist_entry *entry);
static void ro_gui_hotlist_delete_entry(struct hotlist_entry *entry, bool siblings);
static void ro_gui_hotlist_visited_update(struct content *content, struct hotlist_entry *entry);
static int ro_gui_hotlist_redraw_tree(struct hotlist_entry *entry, int level, int x0, int y0);
static int ro_gui_hotlist_redraw_item(struct hotlist_entry *entry, int level, int x0, int y0);
static struct hotlist_entry *ro_gui_hotlist_create_entry(const char *title, const char *url,
		int filetype, struct hotlist_entry *folder);
static void ro_gui_hotlist_update_entry_size(struct hotlist_entry *entry);
static struct hotlist_entry *ro_gui_hotlist_find_entry(int x, int y, struct hotlist_entry *entry);
static int ro_gui_hotlist_selection_state(struct hotlist_entry *entry, bool selected, bool redraw);
static void ro_gui_hotlist_selection_drag(struct hotlist_entry *entry,
		int x0, int y0, int x1, int y1,
		bool toggle, bool redraw);
static int ro_gui_hotlist_selection_count(struct hotlist_entry *entry, bool folders);
static void ro_gui_hotlist_update_expansion(struct hotlist_entry *entry, bool only_selected,
		bool folders, bool links, bool expand, bool contract);
static void ro_gui_hotlist_launch_selection(struct hotlist_entry *entry);
static void ro_gui_hotlist_invalidate_statistics(struct hotlist_entry *entry);
static struct hotlist_entry *ro_gui_hotlist_first_selection(struct hotlist_entry *entry);
static void ro_gui_hotlist_selection_to_process(struct hotlist_entry *entry);
static bool ro_gui_hotlist_move_processing(struct hotlist_entry *entry, struct hotlist_entry *destination, bool before);

#define hotlist_ensure_sprite(buffer, fallback) if (xwimpspriteop_read_sprite_info(buffer, 0, 0, 0, 0)) sprintf(buffer, fallback)
#define hotlist_redraw_entry(entry, full) xwimp_force_redraw(hotlist_window, full ? 0 : entry->x0, \
		full ? -16384 : entry->y0, full ? 16384 : entry->x0 + entry->expanded_width, entry->y0 + entry->height);
#define hotlist_redraw_entry_title(entry) xwimp_force_redraw(hotlist_window, entry->x0, \
		entry->y0 + entry->height - HOTLIST_LINE_HEIGHT, entry->x0 + entry->width, entry->y0 + entry->height);



void ro_gui_hotlist_init(void) {
  	const char *title;
	os_box extent = {0, 0, 0, 0};
	os_error *error;

	/*	Set the initial root options
	*/
	root.next_entry = NULL;
	root.child_entry = NULL;
	root.children = 0;
	root.expanded = true;

	/*	Load the hotlist
	*/
	if (!ro_gui_hotlist_load()) return;

	/*	Get our sprite ids for faster plotting.
	*/
	if (ro_gui_hotlist_initialise_sprite("expand", HOTLIST_EXPAND) ||
			ro_gui_hotlist_initialise_sprite("collapse", HOTLIST_COLLAPSE) ||
			ro_gui_hotlist_initialise_sprite("entry", HOTLIST_ENTRY) ||
			ro_gui_hotlist_initialise_sprite("line", HOTLIST_LINE) ||
			ro_gui_hotlist_initialise_sprite("halflinet", HOTLIST_TLINE) ||
			ro_gui_hotlist_initialise_sprite("halflineb", HOTLIST_BLINE)) {
		return;
        }

	/*	Update our text icon
	*/
	text_icon.data.indirected_text.validation = (char *) -1;
	text_icon.data.indirected_text.size = 256;
	sprite_icon.flags = wimp_ICON_SPRITE | wimp_ICON_INDIRECTED |
			 wimp_ICON_HCENTRED | wimp_ICON_VCENTRED |
			  (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
			  (wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT);
	sprite_icon.data.indirected_sprite.area = wimpspriteop_AREA;
	sprite_icon.data.indirected_text.size = 12;

	/*	Create our window
	*/
	title = messages_get("Hotlist");
	hotlist_window_definition.title_data.indirected_text.text = title;
	hotlist_window_definition.title_data.indirected_text.validation =
			(char *) -1;
	hotlist_window_definition.title_data.indirected_text.size = strlen(title);
	error = xwimp_create_window(&hotlist_window_definition, &hotlist_window);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		die(error->errmess);
	}

	/*	Create our toolbar
	*/
	ro_theme_create_hotlist_toolbar();

	/*	Update the extent
	*/
	if (hotlist_toolbar) {
		extent.x1 = 16384;
		extent.y1 = hotlist_toolbar->height;
		extent.y0 = -16384;
		error = xwimp_set_extent(hotlist_window, &extent);
		if (error) {
			LOG(("xwimp_set_extent: 0x%x: %s",
					error->errnum, error->errmess));
			die(error->errmess);
		}
		reformat_pending = true;
	}
}

/**
 * Initialise a hotlist sprite for use with Tinct
 *
 * \param name   the name of the sprite
 * \param number the sprite cache number
 * \return whether an error occurred
 */
bool ro_gui_hotlist_initialise_sprite(const char *name, int number) {
	os_error *error;
	sprintf(icon_name, "tr_%s", name);
	error = xosspriteop_select_sprite(osspriteop_USER_AREA, gui_sprites,
				(osspriteop_id)icon_name,
				(osspriteop_header **)&sprite[number]);
	if (error) {
		warn_user("MiscError", error->errmess);
		LOG(("Failed to load hotlist sprite 'tr_%s'", name));
		return true;
	}
	return false;
}


/**
 * Shows the hotlist window.
 */
void ro_gui_hotlist_show(void) {
	os_error *error;
	int screen_width, screen_height;
	wimp_window_state state;
	int dimension;
	int scroll_width;

	/*	We may have failed to initialise
	*/
	if (!hotlist_window) return;

	/*	Get the window state
	*/
	state.w = hotlist_window;
	error = xwimp_get_window_state(&state);
	if (error) {
		warn_user("WimpError", error->errmess);
		return;
	}

	/*	If we're open we jump to the top of the stack, if not then we
		open in the centre of the screen.
	*/
	if (!(state.flags & wimp_WINDOW_OPEN)) {
		/*	Clear the selection/expansion states
		*/
		ro_gui_hotlist_update_expansion(root.child_entry, false, true, true, false, true);
		ro_gui_hotlist_selection_state(root.child_entry, false, false);

		/*	Get the current screen size
		*/
		ro_gui_screen_size(&screen_width, &screen_height);

		/*	Move to the centre
		*/
		dimension = state.visible.x1 - state.visible.x0;
		scroll_width = ro_get_vscroll_width(hotlist_window);
		state.visible.x0 = (screen_width - (dimension + scroll_width)) / 2;
		state.visible.x1 = state.visible.x0 + dimension;
		dimension = state.visible.y1 - state.visible.y0;
		state.visible.y0 = (screen_height - dimension) / 2;
		state.visible.y1 = state.visible.y0 + dimension;
		state.xscroll = 0;
		state.yscroll = 0;
		if (hotlist_toolbar) state.yscroll = hotlist_toolbar->height;
	}

	/*	Open the window at the top of the stack
	*/
	ro_gui_menu_prepare_hotlist();
	state.next = wimp_TOP;
	error = xwimp_open_window((wimp_open*)&state);
	if (error) {
		warn_user("WimpError", error->errmess);
		return;
	}

	/*	Set the caret position
	*/
	xwimp_set_caret_position(state.w, -1, -100,
			-100, 32, -1);
}


bool ro_gui_hotlist_load(void) {
	fileswitch_object_type obj_type = 0;
	struct hotlist_entry *netsurf;
	struct hotlist_entry *entry;

	/*	Check if we have an initial hotlist. OS_File does funny things relating to errors,
		so we use the object type to determine success
	*/
	xosfile_read_stamped_no_path("Choices:WWW.NetSurf.Hotlist", &obj_type,
			(bits)0, (bits)0, (int *)0, (fileswitch_attr)0, (bits)0);
	if (obj_type == fileswitch_IS_FILE) {
		ro_gui_hotlist_load_file("Choices:WWW.NetSurf.Hotlist");
		return true;

	} else {
		/*	Create a folder
		*/
		netsurf = ro_gui_hotlist_create_entry("NetSurf", NULL, 0, &root);
		if (!netsurf)
			return false;

		/*	Add some content
		*/
		entry = ro_gui_hotlist_create_entry("NetSurf homepage",
				"http://netsurf.sourceforge.net/",
				0xfaf, netsurf);
		if (!entry)
			return false;
		entry->add_date = (time_t) -1;
		entry = ro_gui_hotlist_create_entry("NetSurf test builds",
				"http://netsurf.strcprstskrzkrk.co.uk/",
				0xfaf, netsurf);
		if (!entry)
			return false;
		entry->add_date = (time_t) -1;

		/*	We succeeded
		*/
		return true;
	}
}


/**
 * Load the hotlist from file.
 *
 * \param  filename  name of file to read
 */

void ro_gui_hotlist_load_file(const char *filename)
{
	xmlDoc *doc;
	xmlNode *html, *body, *ul;

	doc = htmlParseFile(filename, "iso-8859-1");
	if (!doc) {
		warn_user("HotlistLoadError", messages_get("ParsingFail"));
		return;
	}

	html = ro_gui_hotlist_find_element((xmlNode *) doc, "html");
	body = ro_gui_hotlist_find_element(html, "body");
	ul = ro_gui_hotlist_find_element(body, "ul");
	if (!ul) {
		xmlFreeDoc(doc);
		warn_user("HotlistLoadError",
				"(<html>...<body>...<ul> not found.)");
		return;
	}

	ro_gui_hotlist_load_directory(ul, &root);

	xmlFreeDoc(doc);
}


/**
 * Parse a directory represented as a ul.
 *
 * \param  ul         xmlNode for parsed ul
 * \param  directory  directory to add this directory to
 */

void ro_gui_hotlist_load_directory(xmlNode *ul,
		struct hotlist_entry *directory)
{
	char *title;
	struct hotlist_entry *dir;
	xmlNode *n;

	for (n = ul->children; n; n = n->next) {
		/* The ul may contain entries as a li, or directories as
		 * an h4 followed by a ul. Non-element nodes may be present
		 * (eg. text, comments), and are ignored. */

		if (n->type != XML_ELEMENT_NODE)
			continue;

		if (strcmp(n->name, "li") == 0) {
			/* entry */
			ro_gui_hotlist_load_entry(n, directory);

		} else if (strcmp(n->name, "h4") == 0) {
			/* directory */
			title = (char *) xmlNodeGetContent(n);
			if (!title) {
				warn_user("HotlistLoadError", "(Empty <h4> "
						"or memory exhausted.)");
				return;
			}

			for (n = n->next;
					n && n->type != XML_ELEMENT_NODE;
					n = n->next)
				;
			if (!n || strcmp(n->name, "ul") != 0) {
				/* next element isn't expected ul */
				free(title);
				warn_user("HotlistLoadError", "(Expected "
						"<ul> not present.)");
				return;
			}

			dir = ro_gui_hotlist_create_entry(title, NULL, 0,
					directory);
			if (!dir)
				return;
			ro_gui_hotlist_load_directory(n, dir);
		}
	}
}


/**
 * Parse an entry represented as a li.
 *
 * \param  li         xmlNode for parsed li
 * \param  directory  directory to add this entry to
 */

void ro_gui_hotlist_load_entry(xmlNode *li,
		struct hotlist_entry *directory)
{
	char *url = 0;
	char *title = 0;
  	int filetype = 0xfaf;
	int add_date = -1;
	int last_date = -1;
	int visits = 0;
	char *comment;
	struct hotlist_entry *entry;
	xmlNode *n;

	for (n = li->children; n; n = n->next) {
		/* The li must contain an "a" element, and may contain
		 * some additional data as comments. */

		if (n->type == XML_ELEMENT_NODE &&
				strcmp(n->name, "a") == 0) {
			url = (char *) xmlGetProp(n, (const xmlChar *) "href");
			title = (char *) xmlNodeGetContent(n);

		} else if (n->type == XML_COMMENT_NODE) {
			comment = (char *) xmlNodeGetContent(n);
			if (!comment)
				continue;
			if (strncmp("Type:", comment, 5) == 0)
			  	filetype = atoi(comment + 5);
			else if (strncmp("Added:", comment, 6) == 0)
		  		add_date = atoi(comment + 6);
			else if (strncmp("LastVisit:", comment, 10) == 0)
		  		last_date = atoi(comment + 10);
			else if (strncmp("Visits:", comment, 7) == 0)
		  		visits = atoi(comment + 7);
		}
	}

	if (!url || !title) {
		warn_user("HotlistLoadError", "(Missing <a> in <li> or "
				"memory exhausted.)");
		return;
	}

	entry = ro_gui_hotlist_create_entry(title, url, filetype, directory);
	if (!entry)
		return;
	entry->add_date = add_date;
	entry->last_date = last_date;
	entry->visits = visits;
}


/**
 * Search the children of an xmlNode for an element.
 *
 * \param  node  xmlNode to search children of, or 0
 * \param  name  name of element to find
 * \return  first child of node which is an element and matches name, or
 *          0 if not found or parameter node is 0
 */

xmlNode *ro_gui_hotlist_find_element(xmlNode *node, const char *name)
{
	xmlNode *n;
	if (!node)
		return 0;
	for (n = node->children;
			n && !(n->type == XML_ELEMENT_NODE &&
			strcmp(n->name, name) == 0);
			n = n->next)
		;
	return n;
}


/**
 * Perform a save to the default file
 */

void ro_gui_hotlist_save(void) {
	/*	Don't save if we didn't load
	*/
	if (!hotlist_window) return;

	/*	Ensure we have a directory to save to later.
	*/
	xosfile_create_dir("<Choices$Write>.WWW", 0);
	xosfile_create_dir("<Choices$Write>.WWW.NetSurf", 0);

	/*	Save to our file
	*/
	ro_gui_hotlist_save_as("<Choices$Write>.WWW.NetSurf.Hotlist");
}


/**
 * Perform a save to a specified file
 *
 * /param  filename  the file to save to
 */

void ro_gui_hotlist_save_as(const char *filename)
{
	int res;
	xmlDoc *doc;
	xmlNode *html, *head, *title, *body;

	/* Unfortunately the Browse Hotlist format is invalid HTML,
	 * so this is a lie. */
	doc = htmlNewDoc("http://www.w3.org/TR/html4/strict.dtd",
			"-//W3C//DTD HTML 4.01//EN");
	if (!doc) {
		warn_user("NoMemory", 0);
		return;
	}

	html = xmlNewNode(NULL, "html");
	if (!html) {
		warn_user("NoMemory", 0);
		xmlFreeDoc(doc);
		return;
	}
	xmlDocSetRootElement(doc, html);

	head = xmlNewChild(html, NULL, "head", NULL);
	if (!head) {
		warn_user("NoMemory", 0);
		xmlFreeDoc(doc);
		return;
	}

	title  = xmlNewTextChild(head, NULL, "title", "NetSurf Hotlist");
	if (!title) {
		warn_user("NoMemory", 0);
		xmlFreeDoc(doc);
		return;
	}

	body = xmlNewChild(html, NULL, "body", NULL);
	if (!body) {
		warn_user("NoMemory", 0);
		xmlFreeDoc(doc);
		return;
	}

	if (!ro_gui_hotlist_save_directory(&root, body)) {
		warn_user("NoMemory", 0);
		xmlFreeDoc(doc);
		return;
	}

	doc->charset = XML_CHAR_ENCODING_UTF8;
	res = htmlSaveFileEnc(filename, doc, "iso-8859-1");
	if (res == -1) {
		warn_user("HotlistSaveError", 0);
		xmlFreeDoc(doc);
		return;
	}

	xmlFreeDoc(doc);

	xosfile_set_type(filename, 0xfaf);
}


/**
 * Add a directory to the HTML tree for saving.
 *
 * \param  directory  hotlist directory to add
 * \param  node       node to add ul to
 * \return  true on success, false on memory exhaustion
 */

bool ro_gui_hotlist_save_directory(struct hotlist_entry *directory,
		xmlNode *node)
{
	struct hotlist_entry *child;
	xmlNode *ul, *h4;

	ul = xmlNewChild(node, NULL, "ul", NULL);
	if (!ul)
		return false;

	for (child = directory->child_entry; child; child = child->next_entry) {
		if (child->url) {
			/* entry */
			if (!ro_gui_hotlist_save_entry(child, ul))
				return false;

		} else {
			/* directory */
			/* invalid HTML */
			h4 = xmlNewTextChild(ul, NULL, "h4", child->title);
			if (!h4)
				return false;

			if (!ro_gui_hotlist_save_directory(child, ul))
				return false;
		}
	}

	return true;
}


/**
 * Add an entry to the HTML tree for saving.
 *
 * \param  entry  hotlist entry to add
 * \param  node   node to add li to
 * \return  true on success, false on memory exhaustion
 */

bool ro_gui_hotlist_save_entry(struct hotlist_entry *entry,
		xmlNode *node)
{
	xmlNode *li, *a;
	xmlAttr *href;

	li = xmlNewChild(node, NULL, "li", NULL);
	if (!li)
		return false;

	a = xmlNewTextChild(li, NULL, "a", entry->title);
	if (!a)
		return false;

	href = xmlNewProp(a, "href", entry->url);
	if (!href)
		return false;

	if (entry->filetype != 0xfaf)
		if (!ro_gui_hotlist_save_entry_comment(li,
				"Type", entry->filetype))
			return false;

	if (entry->add_date != -1)
		if (!ro_gui_hotlist_save_entry_comment(li,
				"Added", entry->add_date))
			return false;

	if (entry->last_date != -1)
		if (!ro_gui_hotlist_save_entry_comment(li,
				"LastVisit", entry->last_date))
			return false;

	if (entry->visits != 0)
		if (!ro_gui_hotlist_save_entry_comment(li,
				"Visits", entry->visits))
			return false;

	return true;
}


/**
 * Add a special comment node to the HTML tree for saving.
 *
 * \param  node   node to add comment to
 * \param  name   name of special comment
 * \param  value  value of special comment
 * \return  true on success, false on memory exhaustion
 */

bool ro_gui_hotlist_save_entry_comment(xmlNode *node,
		const char *name, int value)
{
	char s[40];
	xmlNode *comment;

	snprintf(s, sizeof s, "%s:%i", name, value);
	s[sizeof s - 1] = 0;

	comment = xmlNewComment(s);
	if (!comment)
		return false;
	if (!xmlAddChild(node, comment)) {
		xmlFreeNode(comment);
		return false;
	}

	return true;
}


/**
 * Adds a hotlist entry to the root of the tree.
 *
 * \param title	  the entry title
 * \param content the content to add
 */
void ro_gui_hotlist_add(char *title, struct content *content) {
	ro_gui_hotlist_create_entry(title, content->url, ro_content_filetype(content), &root);
}


/**
 * Informs the hotlist that some content has been visited
 *
 * \param content the content visited
 */
void hotlist_visited(struct content *content) {
	if ((!content) || (!content->url)) return;
	ro_gui_hotlist_visited_update(content, root.child_entry);
}


/**
 * Informs the hotlist that some content has been visited (internal)
 *
 * \param content the content visited
 * \param entry	  the entry to check siblings and children of
 */
void ro_gui_hotlist_visited_update(struct content *content, struct hotlist_entry *entry) {
  	char *url;
	bool full = false;

	/*	Update the hotlist
	*/
	url = content->url;
	while (entry) {
		if ((entry->url) && (strcmp(url, entry->url) == 0)) {
			/*	Check if we're going to need a full redraw downwards
			*/
			full = ((entry->visits == 0) || (entry->last_date == -1));

			/*	Update our values
			*/
			if (entry->children == -1) entry->filetype = ro_content_filetype(content);
			entry->visits++;
			entry->last_date = time(NULL);
			ro_gui_hotlist_update_entry_size(entry);

			/*	Redraw the least we can get away with
			*/
			if (entry->expanded) hotlist_redraw_entry(entry, full);
		}
		if (entry->child_entry) ro_gui_hotlist_visited_update(content, entry->child_entry);
		entry = entry->next_entry;
	}
}


/**
 * Adds a hotlist entry to a folder of the tree.
 *
 * \param  title     the entry title (copied)
 * \param  url	     the entry url (NULL to create a folder) (copied)
 * \param  filetype  filetype of entry
 * \param  folder    the folder to add the entry into
 * \return  the new entry, or NULL on error and error reported
 */
struct hotlist_entry *ro_gui_hotlist_create_entry(const char *title,
		const char *url, int filetype,
		struct hotlist_entry *folder) {
	struct hotlist_entry *entry;

	/*	Check we have a title or a URL
	*/
	if (!title && !url) return NULL;

	/*	Allocate some memory
	*/
	entry = (struct hotlist_entry *)calloc(1, sizeof(struct hotlist_entry));
	if (!entry) {
		warn_user("NoMemory", 0);
		return NULL;
	}

	/*	Normalise the URL and add the title if we have one, or
		use the URL instead
	*/
	entry->url = 0;
	if ((url) && ((entry->url = url_normalize(url)) == 0)) {
		warn_user("NoMemory", 0);
		free(entry->url);
		free(entry);
		return NULL;
	}
	if (title) {
		entry->title = strdup(title);
		if (!entry->title) {
			warn_user("NoMemory", 0);
			free(entry->url);
			free(entry);
			return NULL;
		}
		entry->title = strip(entry->title);
	} else {
		entry->title = strdup(entry->url);
		if (!entry->title) {
			warn_user("NoMemory", 0);
			free(entry->url);
			free(entry);
			return NULL;
		}
	}

	/*	Set the other values
	*/
	entry->children = (url == NULL) ? 0 : -1;
	entry->filetype = filetype;
	entry->visits = 0;
	entry->add_date = time(NULL);
	entry->last_date = (time_t)-1;
	ro_gui_hotlist_update_entry_size(entry);

	/*	Link into the tree
	*/
	ro_gui_hotlist_link_entry(folder, entry, false);
	return entry;
}


/**
 * Links a hotlist entry into the tree.
 *
 * \param link    the entry to link as a child (folders) or before/after (link)
 * \param entry	  the entry to link
 * \param before  whether to link siblings before or after the supplied link
 */
void ro_gui_hotlist_link_entry(struct hotlist_entry *link, struct hotlist_entry *entry, bool before) {
	struct hotlist_entry *link_entry;

	if ((!link || !entry) || (link == entry)) return;

	/*	Check if the parent is a folder or an entry
	*/
	if (link->children == -1) {
		entry->parent_entry = link->parent_entry;
		entry->parent_entry->children++;
		if (before) {
			entry->next_entry = link;
			entry->previous_entry = link->previous_entry;
			if (link->previous_entry) link->previous_entry->next_entry = entry;
			link->previous_entry = entry;
			if (link->parent_entry) {
				if (link->parent_entry->child_entry == link) {
					link->parent_entry->child_entry = entry;
				}
			}
		} else {
			entry->previous_entry = link;
			entry->next_entry = link->next_entry;
			if (link->next_entry) link->next_entry->previous_entry = entry;
			link->next_entry = entry;
		}
	} else {
		link_entry = link->child_entry;

		/*	Link into the tree as a child at the end
		*/
		if (!link_entry) {
			link->child_entry = entry;
			entry->previous_entry = NULL;
		} else {
			while (link_entry->next_entry) link_entry = link_entry->next_entry;
			link_entry->next_entry = entry;
			entry->previous_entry = link_entry;
		}

		/*	Update the links
		*/
		entry->parent_entry = link;
		entry->next_entry = NULL;

		/*	Increment the number of children
		*/
		link->children += 1;
	}

	/*	Force a redraw
	*/
	reformat_pending = true;
	xwimp_force_redraw(hotlist_window, 0, -16384, 16384, 0);
}


/**
 * De-links a hotlist entry from the tree.
 *
 * \param entry	  the entry to de-link
 */
void ro_gui_hotlist_delink_entry(struct hotlist_entry *entry) {
	if (!entry) return;

	/*	Sort out if the entry was the initial child reference
	*/
	if (entry->parent_entry) {
		entry->parent_entry->children -= 1;
		if (entry->parent_entry->children == 0) entry->parent_entry->expanded = false;
		if (entry->parent_entry->child_entry == entry) {
			entry->parent_entry->child_entry = entry->next_entry;
		}
		entry->parent_entry = NULL;
	}

	/*	Remove the entry from siblings
	*/
	if (entry->previous_entry) {
		entry->previous_entry->next_entry = entry->next_entry;
	}
	if (entry->next_entry) {
	  	entry->next_entry->previous_entry = entry->previous_entry;
	}
	entry->previous_entry = NULL;
	entry->next_entry = NULL;

	/*	Force a redraw
	*/
	reformat_pending = true;
	xwimp_force_redraw(hotlist_window, 0, -16384, 16384, 0);
}


/**
 * Delete an entry and all children
 * This function also performs any necessary delinking
 *
 * \param entry the entry to delete
 * \param siblings delete all following siblings
 */
void ro_gui_hotlist_delete_entry(struct hotlist_entry *entry, bool siblings) {
  	struct hotlist_entry *next_entry = NULL;
	while (entry) {

		/*	Recurse to children first
		*/
		if (entry->child_entry) ro_gui_hotlist_delete_entry(entry->child_entry, true);

		/*	Free our memory
		*/
		free(entry->url);
		free(entry->title);

		/*	Get the next entry before we de-link and delete
		*/
		if (siblings) next_entry = entry->next_entry;

		/*	Delink and delete our entry and move on
		*/
		ro_gui_hotlist_delink_entry(entry);
		free(entry);
		entry = next_entry;
	}
}


/**
 * Updates and entrys size
 */
void ro_gui_hotlist_update_entry_size(struct hotlist_entry *entry) {
	int width;
	int max_width;
	int line_number = 0;

	/*	Get the width of the title
	*/
	xwimptextop_string_width(entry->title,
			strlen(entry->title) > 256 ? 256 : strlen(entry->title),
			&width);
	entry->collapsed_width = width;
	max_width = width;

	/*	Get the width of the URL
	*/
	if (entry->url) {
		snprintf(extended_text, HOTLIST_TEXT_BUFFER,
			messages_get("HotlistURL"), entry->url);
		if (strlen(extended_text) >= 255) {
			extended_text[252] = '.';
			extended_text[253] = '.';
			extended_text[254] = '.';
			extended_text[255] = '\0';
		}
		xwimptextop_string_width(extended_text,
				strlen(extended_text) > 256 ? 256 : strlen(extended_text),
				&width);
		if (width > max_width) max_width = width;
		entry->widths[line_number++] = width;
	}

	/*	Get the width of the add date
	*/
	if (entry->add_date != -1) {
		snprintf(extended_text, HOTLIST_TEXT_BUFFER,
				messages_get("HotlistAdded"), ctime(&entry->add_date));
		xwimptextop_string_width(extended_text,
				strlen(extended_text) > 256 ? 256 : strlen(extended_text),
				&width);
		if (width > max_width) max_width = width;
		entry->widths[line_number++] = width;
	}

	/*	Get the width of the last visit
	*/
	if (entry->last_date != -1) {
		snprintf(extended_text, HOTLIST_TEXT_BUFFER,
				messages_get("HotlistLast"), ctime(&entry->last_date));
		xwimptextop_string_width(extended_text,
				strlen(extended_text) > 256 ? 256 : strlen(extended_text),
				&width);
		if (width > max_width) max_width = width;
		entry->widths[line_number++] = width;
	}

	/*	Get the width of the visit count
	*/
	if (entry->visits > 0) {
		snprintf(extended_text, HOTLIST_TEXT_BUFFER,
				messages_get("HotlistVisits"), entry->visits);
		xwimptextop_string_width(extended_text,
				strlen(extended_text) > 256 ? 256 : strlen(extended_text),
				&width);
		if (width > max_width) max_width = width;
		entry->widths[line_number++] = width;
	}

	/*	Increase the text width by the borders
	*/
	entry->expanded_width = max_width + HOTLIST_LEAF_INSET + HOTLIST_ICON_WIDTH + HOTLIST_TEXT_PADDING;
	entry->collapsed_width += HOTLIST_LEAF_INSET + HOTLIST_ICON_WIDTH + HOTLIST_TEXT_PADDING;
	reformat_pending = true;
}


/**
 * Redraws a section of the hotlist window
 *
 * \param redraw the area to redraw
 */
void ro_gui_hotlist_redraw(wimp_draw *redraw) {
	wimp_window_state state;
	osbool more;
	os_box extent = {0, 0, 0, 0};;

	/*	Reset our min/max sizes
	*/
	max_width = 0;
	max_height = 0;

	/*	Redraw each rectangle
	*/
	more = wimp_redraw_window(redraw);
	while (more) {
		clip_x0 = redraw->clip.x0;
		clip_y0 = redraw->clip.y0;
		clip_x1 = redraw->clip.x1;
		clip_y1 = redraw->clip.y1;
		origin_x = redraw->box.x0 - redraw->xscroll;
		origin_y = redraw->box.y1 - redraw->yscroll;
		ro_gui_hotlist_redraw_tree(root.child_entry, 0,
				origin_x + 8, origin_y - 4);
		more = wimp_get_rectangle(redraw);
	}

	/*	Check if we should reformat
	*/
	if (reformat_pending) {
		max_width += 8;
		max_height -= 4;
		if (max_width < 600) max_width = 600;
		if (max_height > -800) max_height = -800;
		extent.x1 = max_width;
		extent.y0 = max_height;
		if (hotlist_toolbar) {
			extent.y1 += hotlist_toolbar->height;
		}
		xwimp_set_extent(hotlist_window, &extent);
		state.w = hotlist_window;
		wimp_get_window_state(&state);
		wimp_open_window((wimp_open *) &state);
		reformat_pending = false;
	}
}


/**
 * Redraws a section of the hotlist window (non-WIMP interface)
 *
 * \param entry the entry to draw descendants and siblings of
 * \param level the tree level of the entry
 * \param x0	the x co-ordinate to plot from
 * \param y0	the y co-ordinate to plot from
 * \returns the height of the tree
 */
int ro_gui_hotlist_redraw_tree(struct hotlist_entry *entry, int level, int x0, int y0) {
	bool first = true;
	int cumulative = 0;
	int height = 0;
	int box_y0;

	if (!entry) return 0;

	/*	Repeatedly draw our entries
	*/
	while (entry) {

		/*	Redraw the item
		*/
		height = ro_gui_hotlist_redraw_item(entry, level, x0 + HOTLIST_LEAF_INSET, y0);
		box_y0 = y0;
		cumulative += height;

		/*	Update the entry position
		*/
		if (entry->children == -1) {
			entry->height = height;
		} else {
			entry->height = HOTLIST_LINE_HEIGHT;
		}
		entry->x0 = x0 - origin_x;
		entry->y0 = y0 - origin_y - entry->height;
		if (entry->expanded) {
			entry->width = entry->expanded_width;
		} else {
			entry->width = entry->collapsed_width;
		}

		/*	Get the maximum extents
		*/
		if ((x0 + entry->width) > (max_width + origin_x))
				max_width = x0 + entry->width - origin_x;
		if ((y0 - height) < (max_height + origin_y))
				max_height = y0 - height - origin_y;

		/*	Draw the vertical links
		*/
		if (entry->next_entry) {
			/*	Draw a half-line for the first entry in the top tree
			*/
			if (first && (level == 0)) {
				_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
						sprite[HOTLIST_BLINE], x0 + 8, y0 - HOTLIST_LINE_HEIGHT, 0);
				y0 -= HOTLIST_LINE_HEIGHT;
				height -= HOTLIST_LINE_HEIGHT;
			}

			/*	Draw the rest of the lines
			*/
			while (height > 0) {
				_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
						sprite[HOTLIST_LINE], x0 + 8, y0 - HOTLIST_LINE_HEIGHT, 0);
				y0 -= HOTLIST_LINE_HEIGHT;
				height -= HOTLIST_LINE_HEIGHT;
			}

		} else {
			/*	Draw a half-line for the last entry
			*/
			if (!first || (level != 0)) {
				_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
						sprite[HOTLIST_TLINE], x0 + 8, y0 - 22, 0);
				height -= HOTLIST_LINE_HEIGHT;
				y0 -= HOTLIST_LINE_HEIGHT;
			}
		}

		/*	Draw the expansion type
		*/
		if (entry->children == 0) {
			_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
					sprite[HOTLIST_ENTRY], x0, box_y0 - 23, 0);
		} else {
			if (entry->expanded) {
				_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
						sprite[HOTLIST_COLLAPSE], x0, box_y0 - 31, 0);
			} else {
				_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
						sprite[HOTLIST_EXPAND], x0, box_y0 - 31, 0);
			}
		}

		/*	Move to the next entry
		*/
		entry = entry->next_entry;
		first = false;
	}

	/*	Return our height
	*/
	return cumulative;
}


/**
 * Redraws an entry in the tree and any children
 *
 * \param entry the entry to redraw
 * \param level the level of the entry
 * \param x0	the x co-ordinate to plot at
 * \param y0	the y co-ordinate to plot at
 * \return the height of the entry
 */
int ro_gui_hotlist_redraw_item(struct hotlist_entry *entry, int level, int x0, int y0) {
	int height = HOTLIST_LINE_HEIGHT;
	int line_y0;
	int line_height;

	/*	Set the correct height
	*/
	if ((entry->children == -1) && (entry->expanded)) {
		if (entry->url) height += HOTLIST_LINE_HEIGHT;
		if (entry->visits > 0) height += HOTLIST_LINE_HEIGHT;
		if (entry->add_date != -1) height += HOTLIST_LINE_HEIGHT;
		if (entry->last_date != -1) height += HOTLIST_LINE_HEIGHT;
	}

	/*	Check whether we need to redraw
	*/
	if ((x0 < clip_x1) && (y0 > clip_y0) && ((x0 + entry->width) > clip_x0) &&
			((y0 - height) < clip_y1)) {


		/*	Update the selection state
		*/
		text_icon.flags = wimp_ICON_TEXT | (wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
				 (wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT) |
				wimp_ICON_INDIRECTED | wimp_ICON_VCENTRED;
		if (entry->selected) {
			sprite_icon.flags |= wimp_ICON_SELECTED;
			text_icon.flags |= wimp_ICON_SELECTED;
			text_icon.flags |= wimp_ICON_FILLED;
		}

		/*	Draw our icon type
		*/
		sprite_icon.extent.x0 = x0 - origin_x;
		sprite_icon.extent.x1 = x0 - origin_x + HOTLIST_ICON_WIDTH;
		sprite_icon.extent.y0 = y0 - origin_y - HOTLIST_LINE_HEIGHT;
		sprite_icon.extent.y1 = y0 - origin_y;
		sprite_icon.data.indirected_sprite.id = (osspriteop_id)icon_name;
		if (entry->children != -1) {
			if ((entry->expanded) && (entry->children > 0)) {
				sprintf(icon_name, "small_diro");
				hotlist_ensure_sprite(icon_name, "small_dir");
			} else {
				sprintf(icon_name, "small_dir");
			}
		} else {
			/*	Get the icon sprite
			*/
			sprintf(icon_name, "small_%x", entry->filetype);
			hotlist_ensure_sprite(icon_name, "small_xxx");
		}
		xwimp_plot_icon(&sprite_icon);

		/*	Draw our textual information
		*/
		text_icon.data.indirected_text.text = entry->title;
		text_icon.extent.x0 = x0 - origin_x + HOTLIST_ICON_WIDTH;
		text_icon.extent.x1 = x0 - origin_x + entry->collapsed_width - HOTLIST_LEAF_INSET;
		text_icon.extent.y0 = y0 - origin_y - HOTLIST_LINE_HEIGHT + 2;
		text_icon.extent.y1 = y0 - origin_y - 2;
		xwimp_plot_icon(&text_icon);

		/*	Clear the selection state
		*/
		if (entry->selected) {
			sprite_icon.flags &= ~wimp_ICON_SELECTED;
		}

		/*	Draw our further information if expanded
		*/
		if ((entry->children == -1) && (entry->expanded) && (height > HOTLIST_LINE_HEIGHT)) {
			text_icon.flags = wimp_ICON_TEXT | (wimp_COLOUR_DARK_GREY << wimp_ICON_FG_COLOUR_SHIFT) |
					wimp_ICON_INDIRECTED | wimp_ICON_VCENTRED;
			text_icon.extent.y0 = y0 - origin_y - HOTLIST_LINE_HEIGHT;
			text_icon.extent.y1 = y0 - origin_y;

			/*	Draw the lines
			*/
			y0 -= HOTLIST_LINE_HEIGHT;
			line_y0 = y0;
			line_height = height - HOTLIST_LINE_HEIGHT;
			while (line_height > 0) {
				if (line_height == HOTLIST_LINE_HEIGHT) {
					_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
							sprite[HOTLIST_TLINE], x0 + 16, line_y0 - 22, 0);
				} else {
					_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
							sprite[HOTLIST_LINE], x0 + 16, line_y0 - HOTLIST_LINE_HEIGHT, 0);
				}
				_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
						sprite[HOTLIST_ENTRY], x0 + 8, line_y0 - 23, 0);
				line_height -= HOTLIST_LINE_HEIGHT;
				line_y0 -= HOTLIST_LINE_HEIGHT;
			}

			/*	Set the right extent of the icon to be big enough for anything
			*/
			text_icon.extent.x1 = x0 - origin_x + 4096;

			/*	Plot the URL text
			*/
			text_icon.data.indirected_text.text = extended_text;
			if (entry->url) {
				snprintf(extended_text, HOTLIST_TEXT_BUFFER,
				messages_get("HotlistURL"), entry->url);
				if (strlen(extended_text) >= 255) {
					extended_text[252] = '.';
					extended_text[253] = '.';
					extended_text[254] = '.';
					extended_text[255] = '\0';
				}
				text_icon.extent.y0 -= HOTLIST_LINE_HEIGHT;
				text_icon.extent.y1 -= HOTLIST_LINE_HEIGHT;
				xwimp_plot_icon(&text_icon);
			}

			/*	Plot the date added text
			*/
			if (entry->add_date != -1) {
				snprintf(extended_text, HOTLIST_TEXT_BUFFER,
						messages_get("HotlistAdded"), ctime(&entry->add_date));
				text_icon.extent.y0 -= HOTLIST_LINE_HEIGHT;
				text_icon.extent.y1 -= HOTLIST_LINE_HEIGHT;
				xwimp_plot_icon(&text_icon);
			}

			/*	Plot the last visited text
			*/
			if (entry->last_date != -1) {
				snprintf(extended_text, HOTLIST_TEXT_BUFFER,
						messages_get("HotlistLast"), ctime(&entry->last_date));
				text_icon.extent.y0 -= HOTLIST_LINE_HEIGHT;
				text_icon.extent.y1 -= HOTLIST_LINE_HEIGHT;
				xwimp_plot_icon(&text_icon);
			}

			/*	Plot the visit count text
			*/
			if (entry->visits > 0) {
				snprintf(extended_text, HOTLIST_TEXT_BUFFER,
						messages_get("HotlistVisits"), entry->visits);
				text_icon.extent.y0 -= HOTLIST_LINE_HEIGHT;
				text_icon.extent.y1 -= HOTLIST_LINE_HEIGHT;
				xwimp_plot_icon(&text_icon);
			}
		}
	}

	/*	Draw any children
	*/
	if ((entry->child_entry) && (entry->expanded)) {
		height += ro_gui_hotlist_redraw_tree(entry->child_entry, level + 1,
				x0 + 8, y0 - HOTLIST_LINE_HEIGHT);
	}
	return height;
}


/**
 * Respond to a mouse click
 *
 * /param pointer the pointer state
 */
void ro_gui_hotlist_click(wimp_pointer *pointer) {
	wimp_caret caret;
	wimp_drag drag;
	struct hotlist_entry *entry;
	wimp_window_state state;
	wimp_mouse_state buttons;
	int x, y;
	int x_offset;
	int y_offset;
	bool no_entry = false;
	os_error *error;
	os_box box = { pointer->pos.x - 34, pointer->pos.y - 34,
			pointer->pos.x + 34, pointer->pos.y + 34 };
	int selection;

	/*	Get the button state
	*/
	buttons = pointer->buttons;

	/*	Get the window state. Quite why the Wimp can't give relative
		positions is beyond me.
	*/
	state.w = hotlist_window;
	wimp_get_window_state(&state);

	/*	Translate by the origin/scroll values
	*/
	x = (pointer->pos.x - (state.visible.x0 - state.xscroll));
	y = (pointer->pos.y - (state.visible.y1 - state.yscroll));

	/*	We want the caret on a click
	*/
	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x: %s", error->errnum,
							   error->errmess));
	}
	if (((pointer->buttons == (wimp_CLICK_SELECT << 8)) ||
			(pointer->buttons == (wimp_CLICK_ADJUST << 8))) &&
			(caret.w != state.w)) {
		error = xwimp_set_caret_position(state.w, -1, -100,
						-100, 32, -1);
		if (error) {
			LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		}
	}

	/*	Find our entry
	*/
	entry = ro_gui_hotlist_find_entry(x, y, root.child_entry);
	if (entry) {
		/*	Check if we clicked on the expanding bit
		*/
		x_offset = x - entry->x0;
		y_offset = y - (entry->y0 + entry->height);
		if (((x_offset < HOTLIST_LEAF_INSET) && (y_offset > -HOTLIST_LINE_HEIGHT) &&
				((buttons == wimp_CLICK_SELECT << 8) || (buttons == wimp_CLICK_ADJUST << 8) ||
				(buttons == wimp_DOUBLE_SELECT) || (buttons == wimp_DOUBLE_ADJUST))) ||
				((entry->children != -1) &&
				((buttons == wimp_DOUBLE_SELECT) || (buttons == wimp_DOUBLE_ADJUST)))) {
			if (entry->children != 0) {
				ro_gui_hotlist_update_expansion(entry->child_entry, false, true, true, false, true);
				ro_gui_hotlist_selection_state(entry->child_entry,
						false, false);
				entry->expanded = !entry->expanded;
				if (x_offset >= HOTLIST_LEAF_INSET) entry->selected = false;
				reformat_pending = true;
				hotlist_redraw_entry(entry, true);
				ro_gui_menu_prepare_hotlist();
			}
		} else if (x_offset >= HOTLIST_LEAF_INSET) {

		  	/*	We treat a menu click as a Select click if we have no selections
		  	*/
		  	if (buttons == wimp_CLICK_MENU) {
		  		if (ro_gui_hotlist_selection_count(root.child_entry, true) == 0) {
		  		  	menu_selection = true;
		  			buttons = (wimp_CLICK_SELECT << 8);
		  		}
		  	}

			/*	Check for selection
			*/
			if (buttons == (wimp_CLICK_SELECT << 8)) {
				if (!entry->selected) {
					ro_gui_hotlist_selection_state(root.child_entry,
							false, true);
					entry->selected = true;
					hotlist_redraw_entry_title(entry);
					ro_gui_menu_prepare_hotlist();

				}
			} else if (buttons == (wimp_CLICK_ADJUST << 8)) {
				entry->selected = !entry->selected;
				hotlist_redraw_entry_title(entry);
				ro_gui_menu_prepare_hotlist();
			}

			/*	Check if we should open the URL
			*/
			if (((buttons == wimp_DOUBLE_SELECT) || (buttons == wimp_DOUBLE_ADJUST)) &&
					(entry->children == -1)) {
				browser_window_create(entry->url, NULL);
				if (buttons == wimp_DOUBLE_SELECT) {
					ro_gui_hotlist_selection_state(root.child_entry,
							false, true);
					ro_gui_menu_prepare_hotlist();
				} else {
					entry->selected = false;
					ro_gui_dialog_close_persistant(hotlist_window);
					xwimp_close_window(hotlist_window);
				}
			}

			/*	Check if we should start a drag
			*/
			if ((buttons == (wimp_CLICK_SELECT <<4)) || (buttons == (wimp_CLICK_ADJUST << 4))) {
			  	selection = ro_gui_hotlist_get_selected(true);
			  	if (selection > 0) {
			  	  	gui_current_drag_type = GUI_DRAG_HOTLIST_MOVE;
				  	if (selection > 1) {
				  		sprintf(drag_name, "package");
				  	} else {
				  	  	if (entry->children != -1) {
							if ((entry->expanded) && (entry->children > 0)) {
								sprintf(drag_name, "directoryo");
								hotlist_ensure_sprite(drag_name, "directory");
							} else {
								sprintf(drag_name, "directory");
							}
						} else {
				  			sprintf(drag_name, "file_%x", entry->filetype);
				  			hotlist_ensure_sprite(drag_name, "file_xxx");
				  		}
				  	}
					error = xdragasprite_start(dragasprite_HPOS_CENTRE |
							dragasprite_VPOS_CENTRE |
							dragasprite_BOUND_POINTER |
							dragasprite_DROP_SHADOW,
							(osspriteop_area *) 1, drag_name, &box, 0);
					dragging = true;
				}
			}
		} else {
			if (!((x_offset < HOTLIST_LEAF_INSET) && (y_offset > -HOTLIST_LINE_HEIGHT))) {
				no_entry = true;
			}
		}
	} else {
		no_entry = true;
	}

	/*	Get the original button state back
	*/
	buttons = pointer->buttons;

	/*	Create a menu if we should
	*/
	if (buttons == wimp_CLICK_MENU) {
		ro_gui_create_menu(hotlist_menu, pointer->pos.x,
				pointer->pos.y, NULL);
		menu_open = true;
		return;
	}

	/*	Handle a click without an entry
	*/
	if (no_entry) {
		/*	Deselect everything if we click nowhere
		*/
		if (buttons == (wimp_CLICK_SELECT << 8)) {
			ro_gui_hotlist_selection_state(root.child_entry,
					false, true);
			ro_gui_menu_prepare_hotlist();
		}

		/*	Handle the start of a drag
		*/
		if (buttons == (wimp_CLICK_SELECT << 4) ||
				buttons == (wimp_CLICK_ADJUST << 4)) {

			/*	Clear the current selection
			*/
			if (buttons == (wimp_CLICK_SELECT << 4)) {
				ro_gui_hotlist_selection_state(root.child_entry,
						false, true);
				ro_gui_menu_prepare_hotlist();
			}

			/*	Start a drag box
			*/
			drag_buttons = buttons;
			gui_current_drag_type = GUI_DRAG_HOTLIST_SELECT;
			drag.w = hotlist_window;
			drag.type = wimp_DRAG_USER_RUBBER;
			drag.initial.x0 = pointer->pos.x;
			drag.initial.x1 = pointer->pos.x;
			drag.initial.y0 = pointer->pos.y;
			drag.initial.y1 = pointer->pos.y;
			drag.bbox.x0 = state.visible.x0;
			drag.bbox.x1 = state.visible.x1;
			drag.bbox.y0 = state.visible.y0;
			drag.bbox.y1 = state.visible.y1;
			if (hotlist_toolbar) drag.bbox.y1 -= hotlist_toolbar->height;
			xwimp_drag_box(&drag);
			dragging = true;
		}
	}
}


/**
 * Find an entry at a particular position
 *
 * \param x	the x co-ordinate
 * \param y	the y co-ordinate
 * \param entry the entry to check down from (root->child_entry for the entire tree)
 * /return the entry occupying the positon
 */
struct hotlist_entry *ro_gui_hotlist_find_entry(int x, int y, struct hotlist_entry *entry) {
	struct hotlist_entry *find_entry;
	int inset_x = 0;
	int inset_y = 0;

	/*	Check we have an entry (only applies if we have an empty hotlist)
	*/
	if (!entry) return NULL;

	/*	Get the first child entry
	*/
	while (entry) {
		/*	Check if this entry could possibly match
		*/
		if ((x > entry->x0) && (y > entry->y0) && (x < (entry->x0 + entry->width)) &&
				(y < (entry->y0 + entry->height))) {

			/*	The top line extends all the way left
			*/
			if (y - (entry->y0 + entry->height) > -HOTLIST_LINE_HEIGHT) {
				if (x < (entry->x0 + entry->collapsed_width)) return entry;
				return NULL;
			}

			/*	No other entry can occupy the left edge
			*/
			inset_x = x - entry->x0 - HOTLIST_LEAF_INSET - HOTLIST_ICON_WIDTH;
			if (inset_x < 0) return NULL;

			/*	Check the right edge against our various widths
			*/
			inset_y = -((y - entry->y0 - entry->height) / HOTLIST_LINE_HEIGHT);
			if (inset_x < (entry->widths[inset_y - 1] + HOTLIST_TEXT_PADDING)) return entry;
			return NULL;
		}

		/*	Continue onwards
		*/
		if ((entry->child_entry) && (entry->expanded)) {
			find_entry = ro_gui_hotlist_find_entry(x, y, entry->child_entry);
			if (find_entry) return find_entry;
		}
		entry = entry->next_entry;
	}
	return NULL;
}


/**
 * Updated the selection state of the tree
 *
 * \param entry	   the entry to update all siblings and descendants of
 * \param selected the selection state to set
 * \param redraw   update the icons in the Wimp
 * \return the number of entries that have changed
 */
int ro_gui_hotlist_selection_state(struct hotlist_entry *entry, bool selected, bool redraw) {
	int changes = 0;

	/*	Check we have an entry (only applies if we have an empty hotlist)
	*/
	if (!entry) return 0;

	/*	Get the first child entry
	*/
	while (entry) {
		/*	Check this entry
		*/
		if (entry->selected != selected) {
			/*	Update the selection state
			*/
			entry->selected = selected;
			changes++;

			/*	Redraw the entrys first line
			*/
			if (redraw) hotlist_redraw_entry_title(entry);
		}

		/*	Continue onwards
		*/
		if ((entry->child_entry) && ((!selected) || (entry->expanded))) {
			changes += ro_gui_hotlist_selection_state(entry->child_entry,
					selected, redraw & (entry->expanded));
		}
		entry = entry->next_entry;
	}
	return changes;
}


/**
 * Returns the first selected item
 *
 * \param entry the search siblings and children of
 * \return the first selected item
 */
struct hotlist_entry *ro_gui_hotlist_first_selection(struct hotlist_entry *entry) {
	struct hotlist_entry *test_entry;

	/*	Check we have an entry (only applies if we have an empty hotlist)
	*/
	if (!entry) return NULL;

	/*	Work through our entries
	*/
	while (entry) {
		if (entry->selected) return entry;
		if (entry->child_entry) {
			test_entry = ro_gui_hotlist_first_selection(entry->child_entry);
			if (test_entry) return test_entry;
		}
		entry = entry->next_entry;
	}
	return NULL;
}


/**
 * Return the current number of selected items (internal interface)
 *
 * \param entry the entry to count siblings and children of
 */
int ro_gui_hotlist_selection_count(struct hotlist_entry *entry, bool folders) {
	int count = 0;
	if (!entry) return 0;
	while (entry) {
		if ((entry->selected) && (folders || (entry->children == -1))) count++;
		if (entry->child_entry) count += ro_gui_hotlist_selection_count(entry->child_entry, folders);
		entry = entry->next_entry;
	}
	return count;
}


/**
 * Launch the current selection (internal interface)
 *
 * \param entry the entry to launch siblings and children of
 */
void ro_gui_hotlist_launch_selection(struct hotlist_entry *entry) {
	if (!entry) return;
	while (entry) {
		if ((entry->selected) && (entry->url)) browser_window_create(entry->url, NULL);
		if (entry->child_entry) ro_gui_hotlist_launch_selection(entry->child_entry);
		entry = entry->next_entry;
	}
}


/**
 * Invalidate the statistics for any selected items (internal interface)
 *
 * \param entry the entry to update siblings and children of
 */
void ro_gui_hotlist_invalidate_statistics(struct hotlist_entry *entry) {
	if (!entry) return;
	while (entry) {
		if ((entry->selected) && (entry->children == -1)) {
			entry->visits = 0;
			entry->last_date = (time_t)-1;
			if (entry->expanded) hotlist_redraw_entry(entry, true);
		}
		if (entry->child_entry) ro_gui_hotlist_invalidate_statistics(entry->child_entry);
		entry = entry->next_entry;
	}
}


/**
 * Set the process flag for the current selection (internal interface)
 *
 * \param entry the entry to modify siblings and children of
 */
void ro_gui_hotlist_selection_to_process(struct hotlist_entry *entry) {
	if (!entry) return;
	while (entry) {
		entry->process = entry->selected;
		if (entry->child_entry) ro_gui_hotlist_selection_to_process(entry->child_entry);
		entry = entry->next_entry;
	}
}


/**
 * Toggles the expanded state for selected icons
 * If neither expand not contract are set then the entries are toggled
 *
 * \param entry		the entry to update all siblings and descendants of
 * \param only_selected whether to only update selected icons
 * \param folders       whether to update folders
 * \param links         whether to update links
 * \param expand	force all entries to be expanded (dominant)
 * \param contract	force all entries to be contracted (recessive)
 */
void ro_gui_hotlist_update_expansion(struct hotlist_entry *entry, bool only_selected,
		bool folders, bool links, bool expand, bool contract) {
	bool current;

	/*	Set a reformat to be pending
	*/
	reformat_pending = true;

	/*	Check we have an entry (only applies if we have an empty hotlist)
	*/
	if (!entry) return;

	/*	Get the first child entry
	*/
	while (entry) {
		/*	Check this entry
		*/
		if ((entry->selected) || (!only_selected)) {
			current = entry->expanded;

			/*	Only update what we should
			*/
			if (((links) && (entry->children == -1)) || ((folders) && (entry->children > 0))) {
				/*	Update the expansion state
				*/
				if (expand) {
					entry->expanded = true;
				} else if (contract) {
					entry->expanded = false;
				} else {
					entry->expanded = !entry->expanded;
				}
			}

			/*	If we have contracted then we de-select and collapse any children
			*/
			if (entry->child_entry && !entry->expanded) {
				ro_gui_hotlist_update_expansion(entry->child_entry, false, true, true, false, true);
				ro_gui_hotlist_selection_state(entry->child_entry, false, false);
			}

			/*	Redraw the entrys first line
			*/
			if (current != entry->expanded) hotlist_redraw_entry(entry, true);
		}

		/*	Continue onwards (child entries cannot be selected if the parent is
			not expanded)
		*/
		if (entry->child_entry && entry->expanded) {
			ro_gui_hotlist_update_expansion(entry->child_entry,
					only_selected, folders, links, expand, contract);
		}
		entry = entry->next_entry;
	}
}


/**
 * Updated the selection state of the tree
 *
 * \param entry	 the entry to update all siblings and descendants of
 * \param x0	 the left edge of the box
 * \param y0	 the top edge of the box
 * \param x1	 the right edge of the box
 * \param y1	 the bottom edge of the box
 * \param toggle toggle the selection state, otherwise set
 * \param redraw update the icons in the Wimp
 */
void ro_gui_hotlist_selection_drag(struct hotlist_entry *entry,
		int x0, int y0, int x1, int y1,
		bool toggle, bool redraw) {
	bool do_update;
	int line;
	int test_y;

	/*	Check we have an entry (only applies if we have an empty hotlist)
	*/
	if (!entry) return;

	/*	Get the first child entry
	*/
	while (entry) {
		/*	Check if this entry could possibly match
		*/
		if ((x1 > (entry->x0 + HOTLIST_LEAF_INSET)) && (y0 > entry->y0) &&
				(x0 < (entry->x0 + entry->width)) &&
				(y1 < (entry->y0 + entry->height))) {
			do_update = false;

			/*	Check the exact area of the title line
			*/
			if ((x1 > (entry->x0 + HOTLIST_LEAF_INSET)) &&
					(y0 > entry->y0 + entry->height - HOTLIST_LINE_HEIGHT) &&
					(x0 < (entry->x0 + entry->collapsed_width)) &&
					(y1 < (entry->y0 + entry->height))) {
				do_update = true;
			}

			/*	Check the other lines
			*/
			line = 1;
			test_y = entry->y0 + entry->height - HOTLIST_LINE_HEIGHT;
			while (((line * HOTLIST_LINE_HEIGHT) < entry->height) && (!do_update)) {
				/*	Check this line
				*/
				if ((x1 > (entry->x0 + HOTLIST_LEAF_INSET + HOTLIST_ICON_WIDTH)) &&
						(y1 < test_y) && (y0 > test_y - HOTLIST_LINE_HEIGHT) &&
						(x0 < (entry->x0 + entry->widths[line - 1] +
							HOTLIST_LEAF_INSET + HOTLIST_ICON_WIDTH +
							HOTLIST_TEXT_PADDING))) {
					do_update = true;
				}

				/*	Move to the next line
				*/
				line++;
				test_y -= HOTLIST_LINE_HEIGHT;
			}

			/*	Redraw the entrys first line
			*/
			if (do_update) {
				if (toggle) {
					entry->selected = !entry->selected;
				} else {
					entry->selected = true;
				}
				if (redraw) hotlist_redraw_entry_title(entry);
			}
		}

		/*	Continue onwards
		*/
		if ((entry->child_entry) && (entry->expanded)) {
			ro_gui_hotlist_selection_drag(entry->child_entry,
					x0, y0, x1, y1, toggle, redraw);
		}
		entry = entry->next_entry;
		do_update = false;
	}
}


/**
 * The end of a selection drag has been reached
 *
 * \param drag the final drag co-ordinates
 */
void ro_gui_hotlist_selection_drag_end(wimp_dragged *drag) {
	wimp_window_state state;
	int x0, y0, x1, y1;
	int toolbar_height = 0;

	/*	Reset our dragging state
	*/
	dragging = false;

	/*	Get the toolbar height
	*/
	if (hotlist_toolbar) toolbar_height = hotlist_toolbar->height * 2;

	/*	Get the window state to make everything relative
	*/
	state.w = hotlist_window;
	wimp_get_window_state(&state);

	/*	Create the relative positions
	*/
	x0 = drag->final.x0 - state.visible.x0 - state.xscroll;
	x1 = drag->final.x1 - state.visible.x0 - state.xscroll;
	y0 = drag->final.y0 - state.visible.y1 - state.yscroll + toolbar_height;
	y1 = drag->final.y1 - state.visible.y1 - state.yscroll + toolbar_height;

	/*	Make sure x0 < x1 and y0 > y1
	*/
	if (x0 > x1) {
		x0 ^= x1;
		x1 ^= x0;
		x0 ^= x1;
	}
	if (y0 < y1) {
		y0 ^= y1;
		y1 ^= y0;
		y0 ^= y1;
	}

	/*	Update the selection state
	*/
	if (drag_buttons == (wimp_CLICK_SELECT << 4)) {
		ro_gui_hotlist_selection_drag(root.child_entry, x0, y0, x1, y1, false, true);
	} else {
		ro_gui_hotlist_selection_drag(root.child_entry, x0, y0, x1, y1, true, true);
	}
	ro_gui_menu_prepare_hotlist();
}


/**
 * The end of a item moving drag has been reached
 *
 * \param drag the final drag co-ordinates
 */
void ro_gui_hotlist_move_drag_end(wimp_dragged *drag) {
	wimp_pointer pointer;
	int toolbar_height = 0;
	wimp_window_state state;
	struct hotlist_entry *test_entry;
	struct hotlist_entry *entry;
	int x, y, x0, y0, x1, y1;
	bool before = false;

	/*	Reset our dragging state
	*/
	dragging = false;

	/*	Check we dropped to our window
	*/
	xwimp_get_pointer_info(&pointer);
	if (pointer.w != hotlist_window) return;

	/*	Get the toolbar height
	*/
	if (hotlist_toolbar) toolbar_height = hotlist_toolbar->height * 2;

  	/*	Set the process flag for all selected items
  	*/
  	ro_gui_hotlist_selection_to_process(root.child_entry);

	/*	Get the window state to make everything relative
	*/
	state.w = hotlist_window;
	wimp_get_window_state(&state);

	/*	Create the relative positions
	*/
	x0 = drag->final.x0 - state.visible.x0 - state.xscroll;
	x1 = drag->final.x1 - state.visible.x0 - state.xscroll;
	y0 = drag->final.y0 - state.visible.y1 - state.yscroll + toolbar_height;
	y1 = drag->final.y1 - state.visible.y1 - state.yscroll + toolbar_height;
	x = (x0 + x1) / 2;
	y = (y0 + y1) / 2;

	/*	Find our entry
	*/
	entry = ro_gui_hotlist_find_entry(x, y, root.child_entry);
	if (!entry) entry = &root;

	/*	No parent of the destination can be processed
	*/
	test_entry = entry;
	while (test_entry != NULL) {
		if (test_entry->process) return;
		test_entry = test_entry->parent_entry;
	}

	/*	Check for before/after
	*/
	before = ((y - (entry->y0 + entry->height)) > (-HOTLIST_LINE_HEIGHT / 2));

	/*	Start our recursive moving
	*/
	while (ro_gui_hotlist_move_processing(root.child_entry, entry, before));
}



bool ro_gui_hotlist_move_processing(struct hotlist_entry *entry, struct hotlist_entry *destination, bool before) {
  	bool result = false;
	if (!entry) return false;
	while (entry) {
		if (entry->process) {
			entry->process = false;
			ro_gui_hotlist_delink_entry(entry);
			ro_gui_hotlist_link_entry(destination, entry, before);
			result = true;
		}
		if (entry->child_entry) {
			result |= ro_gui_hotlist_move_processing(entry->child_entry, destination, before);
		}
		entry = entry->next_entry;
	}
	return result;
}

/**
 * Handle a menu being closed
 */
void ro_gui_hotlist_menu_closed(void) {
  	menu_open = false;
	if (menu_selection) {
		ro_gui_hotlist_selection_state(root.child_entry, false, true);
		menu_selection = false;
	}
}

/**
 * Handle a keypress
 *
 * \param key the key pressed
 * \return whether the key was processed
 */
bool ro_gui_hotlist_keypress(int key) {
	wimp_window_state state;
	int y;

	/*	Handle basic keys
	*/
	switch (key) {
		case 1:		/* CTRL+A */
			ro_gui_hotlist_selection_state(root.child_entry, true, true);
			if (menu_open) ro_gui_create_menu(hotlist_menu, 0, 0, NULL);
			return true;
		case 26:	/* CTRL+Z */
			ro_gui_hotlist_selection_state(root.child_entry, false, true);
			if (menu_open) ro_gui_create_menu(hotlist_menu, 0, 0, NULL);
			return true;
		case 32:	/* SPACE */
			ro_gui_hotlist_update_expansion(root.child_entry, true, true, true, false, false);
			if (menu_open) ro_gui_create_menu(hotlist_menu, 0, 0, NULL);
			return true;
		case wimp_KEY_RETURN:
			ro_gui_hotlist_launch_selection(root.child_entry);
			return true;
		case wimp_KEY_F3:
			ro_gui_hotlist_save();
			return true;
		case wimp_KEY_UP:
		case wimp_KEY_DOWN:
		case wimp_KEY_PAGE_UP:
		case wimp_KEY_PAGE_DOWN:
		case wimp_KEY_CONTROL | wimp_KEY_UP:
		case wimp_KEY_CONTROL | wimp_KEY_DOWN:
			break;

		default:
			return false;
	}

	/*	Handle keypress scrolling
	*/
	state.w = hotlist_window;
	wimp_get_window_state(&state);
	y = state.visible.y1 - state.visible.y0 - 32;
	switch (key) {
		case wimp_KEY_UP:
			state.yscroll += 32;
			break;
		case wimp_KEY_DOWN:
			state.yscroll -= 32;
			break;
		case wimp_KEY_PAGE_UP:
			state.yscroll += y;
			break;
		case wimp_KEY_PAGE_DOWN:
			state.yscroll -= y;
			break;
		case wimp_KEY_CONTROL | wimp_KEY_UP:
			state.yscroll = 1000;
			break;
		case wimp_KEY_CONTROL | wimp_KEY_DOWN:
			state.yscroll = -0x10000000;
			break;
	}
	xwimp_open_window((wimp_open *) &state);
	return true;
}


void ro_gui_hotlist_toolbar_click(wimp_pointer* pointer) {
  	int selection;

	/*	Reject Menu clicks
	*/
	if (pointer->buttons == wimp_CLICK_MENU) return;

	/*	Handle the buttons appropriately
	*/
	switch (pointer->i) {
	  	case ICON_TOOLBAR_CREATE:
	  		hotlist_insert = false;
	  		if (pointer->buttons == wimp_CLICK_SELECT) {
	  			ro_gui_hotlist_prepare_folder_dialog(false);
	  			ro_gui_dialog_open_persistant(hotlist_window, dialog_folder, true);
	  		} else {
	  			ro_gui_hotlist_prepare_entry_dialog(false);
	  			ro_gui_dialog_open_persistant(hotlist_window, dialog_entry, true);
	  		}
	  		break;
	  	case ICON_TOOLBAR_OPEN:
	  		selection = ro_gui_hotlist_get_selected(true);
			ro_gui_hotlist_update_expansion(root.child_entry, (selection != 0), true, false,
					(pointer->buttons == wimp_CLICK_SELECT),
					(pointer->buttons == wimp_CLICK_ADJUST));
			break;
	  	case ICON_TOOLBAR_EXPAND:
	  		selection = ro_gui_hotlist_get_selected(true);
			ro_gui_hotlist_update_expansion(root.child_entry, (selection != 0), false, true,
					(pointer->buttons == wimp_CLICK_SELECT),
					(pointer->buttons == wimp_CLICK_ADJUST));
			break;
		case ICON_TOOLBAR_DELETE:
			ro_gui_hotlist_delete_selected();
			break;
		case ICON_TOOLBAR_LAUNCH:
			ro_gui_hotlist_keypress(wimp_KEY_RETURN);
			break;
	}
}


void ro_gui_hotlist_prepare_folder_dialog(bool selected) {
	struct hotlist_entry *entry = NULL;
	if (selected) entry = ro_gui_hotlist_first_selection(root.child_entry);

	/*	Update the title
	*/
	dialog_folder_add = selected;
	if (selected) {
		ro_gui_set_window_title(dialog_folder, messages_get("EditFolder"));
	} else {
		ro_gui_set_window_title(dialog_folder, messages_get("NewFolder"));
	}

	/*	Update the icons
	*/
	if (entry == NULL) {
		ro_gui_set_icon_string(dialog_folder, 1, messages_get("Folder"));
	} else {
		ro_gui_set_icon_string(dialog_folder, 1, entry->title);
	}
}

void ro_gui_hotlist_prepare_entry_dialog(bool selected) {
	struct hotlist_entry *entry = NULL;
	if (selected) entry = ro_gui_hotlist_first_selection(root.child_entry);

	/*	Update the title
	*/
	dialog_entry_add = selected;
	if (selected) {
		ro_gui_set_window_title(dialog_entry, messages_get("EditLink"));
	} else {
		ro_gui_set_window_title(dialog_entry, messages_get("NewLink"));
	}

	/*	Update the icons
	*/
	if (entry == NULL) {
		ro_gui_set_icon_string(dialog_entry, 1, messages_get("Link"));
		ro_gui_set_icon_string(dialog_entry, 3, "");
	} else {
		ro_gui_set_icon_string(dialog_entry, 1, entry->title);
		ro_gui_set_icon_string(dialog_entry, 3, entry->url);
	}
}


/**
 * Set all items to either selected or deselected
 *
 * \param selected the state to set all items to
 */
void ro_gui_hotlist_set_selected(bool selected) {
	ro_gui_hotlist_selection_state(root.child_entry, selected, true);
	menu_selection = false;
}


/**
 * Reset the statistics for selected entries
 */
void ro_gui_hotlist_reset_statistics(void) {
 	ro_gui_hotlist_invalidate_statistics(root.child_entry);
}


/**
 * Return the current number of selected items
 *
 * \param folders include folders in the selection count
 * \return the number of selected items
 */
int ro_gui_hotlist_get_selected(bool folders) {
	return ro_gui_hotlist_selection_count(root.child_entry, folders);
}


/**
 * Set all items to either selected or deselected
 *
 * \param expand  whether to expand (collapse otherwise)
 * \param folders whether to update folders
 * \param links   whether to update links
 */
void ro_gui_hotlist_set_expanded(bool expand, bool folders, bool links) {
	ro_gui_hotlist_update_expansion(root.child_entry, false, folders, links, expand, !expand);
}


/**
 * Deletes any selected items
 *
 * \param selected the state to set all items to
 */
void ro_gui_hotlist_delete_selected(void) {
  	struct hotlist_entry *entry;
	while ((entry = ro_gui_hotlist_first_selection(root.child_entry)) != NULL) {
		ro_gui_hotlist_delete_entry(entry, false);
	}
}

void ro_gui_hotlist_dialog_click(wimp_pointer *pointer) {
	struct hotlist_entry *entry = NULL;
	char *title = NULL;
	char *url = NULL;
	char *old_value;
	int icon = pointer->i;
	int close_icon, ok_icon;
	bool folder;
	bool add;

	/*	Get our data
	*/
	if (pointer->w == dialog_entry) {
		title = strip(ro_gui_get_icon_string(pointer->w, 1));
		url = strip(ro_gui_get_icon_string(pointer->w, 3));
		close_icon = 4;
		ok_icon = 5;
		folder = false;
		add = !dialog_entry_add;
	} else {
		title = strip(ro_gui_get_icon_string(pointer->w, 1));
		close_icon = 2;
		ok_icon = 3;
		folder = true;
		add = !dialog_folder_add;
	}

	/*	Check for cancelling
	*/
	if (icon == close_icon) {
		if (pointer->buttons == wimp_CLICK_SELECT) {
			ro_gui_dialog_close(pointer->w);
  		  	xwimp_create_menu((wimp_menu *)-1, 0, 0);
			return;
		}
		if (folder) {
			ro_gui_hotlist_prepare_folder_dialog(dialog_folder_add);
		} else {
			ro_gui_hotlist_prepare_entry_dialog(dialog_entry_add);
		}
		return;
	}

	/*	Check for ok
	*/
	if (icon != ok_icon) return;

	/*	Check we have valid values
	*/
	if ((title != NULL) && (strlen(title) == 0)) {
		warn_user("NoNameError", 0);
		return;
	}
	if ((url != NULL) && (strlen(url) == 0)) {
		warn_user("NoURLError", 0);
		return;
	}

	/*	Update/insert our data
	*/
	if (add) {
		/* todo: insert at the selection place if hotlist_insert is set */
		ro_gui_hotlist_create_entry(title, url, folder ? 0 : 0xfaf, &root);
	} else {
		entry = ro_gui_hotlist_first_selection(root.child_entry);
		if (entry == NULL) return;
		if (url) {
			old_value = entry->url;
			entry->url = url_normalize(url);
			if (!entry->url) {
				warn_user("NoMemory", 0);
				entry->url = old_value;
				return;
			}
			if (old_value) free(old_value);
		}
		if (title) {
			old_value = entry->title;
			entry->title = strdup(title);
			if (!entry->title) {
				warn_user("NoMemory", 0);
				entry->title = old_value;
				return;
			}
			free(old_value);
		}
		ro_gui_hotlist_update_entry_size(entry);
	}

	/*	Close if we should
	*/
	if (pointer->buttons == wimp_CLICK_SELECT) {
	  	xwimp_create_menu((wimp_menu *)-1, 0, 0);
		ro_gui_dialog_close(pointer->w);
		return;
	}

	/*	Update our display
	*/
	if (folder) {
		ro_gui_hotlist_prepare_folder_dialog(dialog_folder_add);
	} else {
		ro_gui_hotlist_prepare_entry_dialog(dialog_entry_add);
	}
}

int ro_gui_hotlist_help(int x, int y) {
	struct hotlist_entry *entry;
	wimp_window_state state;
	int toolbar_height = 0;
	int x_offset, y_offset;

	/*	Return the dragging codes
	*/
	if (dragging) {
		if (gui_current_drag_type == GUI_DRAG_HOTLIST_SELECT) return 6;
		if (gui_current_drag_type == GUI_DRAG_HOTLIST_MOVE) return 7;
		return -1;
	}

	/*	Get the toolbar height
	*/
	if (hotlist_toolbar) toolbar_height = hotlist_toolbar->height * 2;

	/*	Get the window state to make everything relative
	*/
	state.w = hotlist_window;
	wimp_get_window_state(&state);

	/*	Create the relative positions
	*/
	x = x - state.visible.x0 - state.xscroll;
	y = y - state.visible.y1 - state.yscroll + toolbar_height;

	/*	Get the current entry
	*/
	entry = ro_gui_hotlist_find_entry(x, y, root.child_entry);
	if (entry == NULL) return -1;

	/*	Return the relevant code
	*/
	x_offset = x - entry->x0;
	y_offset = y - (entry->y0 + entry->height);
	if ((x_offset < HOTLIST_LEAF_INSET) && (y_offset > -HOTLIST_LINE_HEIGHT)) {
		if (entry->children == 0) return -1;
		return (((entry->children == -1) ? 2 : 0) + ((entry->expanded) ? 1 : 0));
	}
	return ((entry->children == -1) ? 5 : 4);
}
