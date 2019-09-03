/*
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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
 * implementatio of toolbar to control browsing context
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <gtk/gtk.h>

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsoption.h"
#include "utils/file.h"
#include "utils/nsurl.h"
#include "utils/corestrings.h"
#include "desktop/browser_history.h"
#include "desktop/searchweb.h"
#include "desktop/search.h"
#include "desktop/save_complete.h"
#include "desktop/save_text.h"
#include "desktop/print.h"
#include "desktop/hotlist.h"
#include "netsurf/content.h"
#include "netsurf/browser_window.h"
#include "netsurf/keypress.h"

#include "gtk/toolbar_items.h"
#include "gtk/completion.h"
#include "gtk/gui.h"
#include "gtk/warn.h"
#include "gtk/search.h"
#include "gtk/throbber.h"
#include "gtk/scaffolding.h"
#include "gtk/window.h"
#include "gtk/compat.h"
#include "gtk/resources.h"
#include "gtk/schedule.h"
#include "gtk/local_history.h"
#include "gtk/global_history.h"
#include "gtk/viewsource.h"
#include "gtk/download.h"
#include "gtk/viewdata.h"
#include "gtk/tabs.h"
#include "gtk/print.h"
#include "gtk/layout_pango.h"
#include "gtk/preferences.h"
#include "gtk/hotlist.h"
#include "gtk/cookies.h"
#include "gtk/about.h"
#include "gtk/gdk.h"
#include "gtk/bitmap.h"
#include "gtk/toolbar.h"

/**
 * button location indicating button is not to be shown
 */
#define INACTIVE_LOCATION (-1)

/**
 * time (in ms) between throbber animation frame updates
 */
#define THROBBER_FRAME_TIME (100)

/**
 * toolbar item context
 */
struct nsgtk_toolbar_item {
	GtkToolItem *button;
	int location; /* in toolbar */
	bool sensitivity;

	/**
	 * button clicked handler
	 */
	gboolean (*bhandler)(GtkWidget *widget, gpointer data);

	void *dataplus; /* customization -> toolbar */
	void *dataminus; /* customization -> store */
};


/**
 * control toolbar context
 */
struct nsgtk_toolbar {
	/** gtk toolbar widget */
	GtkToolbar *widget;

	/* toolbar size allocation context */
	int offset;
	int toolbarmem;
	int toolbarbase;
	int historybase;

	/**
	 * Toolbar item contexts
	 */
	struct nsgtk_toolbar_item *buttons[PLACEHOLDER_BUTTON];

	/** entry widget holding the url of the current displayed page */
	GtkWidget *url_bar;

	/** Current frame of throbber animation */
	int throb_frame;

	/** Web search widget */
	GtkWidget *webSearchEntry;

	/**
	 * callback to obtain a browser window for navigation
	 */
	struct browser_window *(*get_bw)(void *ctx);

	/**
	 * context passed to get_bw function
	 */
	void *get_ctx;
};


static GtkTargetEntry entry = {(char *)"nsgtk_button_data",
		GTK_TARGET_SAME_APP, 0};

static bool edit_mode = false;

/**
 * toolbar customization window context
 */
struct nsgtk_toolbar_custom_store {
	GtkWidget *window;
	GtkWidget *store_buttons[PLACEHOLDER_BUTTON];
	GtkWidget *widgetvbox;
	GtkWidget *currentbar;
	char numberh; /* current horizontal location while adding */
	GtkBuilder *builder; /* button widgets to store */
	int buttonlocations[PLACEHOLDER_BUTTON];
	int currentbutton;
	bool fromstore;
};

/* the number of buttons that fit in the width of the store window */
#define NSGTK_STORE_WIDTH 6

/* the 'standard' width of a button that makes sufficient of its label
visible */
#define NSGTK_BUTTON_WIDTH 111

/* the 'standard' height of a button that fits as many toolbars as
possible into the store */
#define NSGTK_BUTTON_HEIGHT 70

/* the 'normal' width of the websearch bar */
#define NSGTK_WEBSEARCH_WIDTH 150

static struct nsgtk_toolbar_custom_store store;
static struct nsgtk_toolbar_custom_store *window = &store;


enum image_sets {
	IMAGE_SET_MAIN_MENU = 0,
	IMAGE_SET_RCLICK_MENU,
	IMAGE_SET_POPUP_MENU,
	IMAGE_SET_BUTTONS,
	IMAGE_SET_COUNT
};

typedef enum search_buttons {
	SEARCH_BACK_BUTTON = 0,
	SEARCH_FORWARD_BUTTON,
	SEARCH_CLOSE_BUTTON,
	SEARCH_BUTTONS_COUNT
} nsgtk_search_buttons;

struct nsgtk_theme {
	GtkImage *image[PLACEHOLDER_BUTTON];
	GtkImage *searchimage[SEARCH_BUTTONS_COUNT];
};

/* forward declaration */
void nsgtk_toolbar_connect_all(struct nsgtk_scaffolding *g);
int nsgtk_toolbar_get_id_from_widget(GtkWidget *widget, struct nsgtk_scaffolding *g);


/* define data plus and data minus handlers */
#define TOOLBAR_ITEM(identifier, name, sensitivity, clicked, activate)	\
static gboolean								\
nsgtk_toolbar_##name##_data_plus(GtkWidget *widget,			\
				 GdkDragContext *cont,			\
				 GtkSelectionData *selection,		\
				 guint info,				\
				 guint time,				\
				 gpointer data)				\
{									\
	window->currentbutton = identifier;				\
	window->fromstore = true;					\
	return TRUE;							\
}									\
static gboolean								\
nsgtk_toolbar_##name##_data_minus(GtkWidget *widget,			\
				  GdkDragContext *cont,			\
				  GtkSelectionData *selection,		\
				  guint info,				\
				  guint time,				\
				  gpointer data)			\
{									\
	window->currentbutton = identifier;				\
	window->fromstore = false;					\
	return TRUE;							\
}

#include "gtk/toolbar_items.h"

#undef TOOLBAR_ITEM


/**
 * get default image for buttons / menu items from gtk stock items.
 *
 * \param tbbutton button reference
 * \param iconsize The size of icons to select.
 * \param usedef Use the default image if not found.
 * \return default images.
 */
static GtkImage *
nsgtk_theme_image_default(nsgtk_toolbar_button tbbutton,
			  GtkIconSize iconsize,
			  bool usedef)
{
	GtkImage *image; /* The GTK image to return */

	switch(tbbutton) {

#define BUTTON_IMAGE(p, q)						\
	case p##_BUTTON:					\
		image = GTK_IMAGE(nsgtk_image_new_from_stock(q, iconsize)); \
		break

	BUTTON_IMAGE(BACK, NSGTK_STOCK_GO_BACK);
	BUTTON_IMAGE(FORWARD, NSGTK_STOCK_GO_FORWARD);
	BUTTON_IMAGE(STOP, NSGTK_STOCK_STOP);
	BUTTON_IMAGE(RELOAD, NSGTK_STOCK_REFRESH);
	BUTTON_IMAGE(HOME, NSGTK_STOCK_HOME);
	BUTTON_IMAGE(NEWWINDOW, "gtk-new");
	BUTTON_IMAGE(NEWTAB, "gtk-new");
	BUTTON_IMAGE(OPENFILE, NSGTK_STOCK_OPEN);
	BUTTON_IMAGE(CLOSETAB, NSGTK_STOCK_CLOSE);
	BUTTON_IMAGE(CLOSEWINDOW, NSGTK_STOCK_CLOSE);
	BUTTON_IMAGE(SAVEPAGE, NSGTK_STOCK_SAVE_AS);
	BUTTON_IMAGE(PRINTPREVIEW, "gtk-print-preview");
	BUTTON_IMAGE(PRINT, "gtk-print");
	BUTTON_IMAGE(QUIT, "gtk-quit");
	BUTTON_IMAGE(CUT, "gtk-cut");
	BUTTON_IMAGE(COPY, "gtk-copy");
	BUTTON_IMAGE(PASTE, "gtk-paste");
	BUTTON_IMAGE(DELETE, "gtk-delete");
	BUTTON_IMAGE(SELECTALL, "gtk-select-all");
	BUTTON_IMAGE(FIND, NSGTK_STOCK_FIND);
	BUTTON_IMAGE(PREFERENCES, "gtk-preferences");
	BUTTON_IMAGE(ZOOMPLUS, "gtk-zoom-in");
	BUTTON_IMAGE(ZOOMMINUS, "gtk-zoom-out");
	BUTTON_IMAGE(ZOOMNORMAL, "gtk-zoom-100");
	BUTTON_IMAGE(FULLSCREEN, "gtk-fullscreen");
	BUTTON_IMAGE(VIEWSOURCE, "gtk-index");
	BUTTON_IMAGE(CONTENTS, "gtk-help");
	BUTTON_IMAGE(ABOUT, "gtk-about");
	BUTTON_IMAGE(OPENMENU, NSGTK_STOCK_OPEN_MENU);
#undef BUTTON_IMAGE

	case HISTORY_BUTTON:
		image = GTK_IMAGE(gtk_image_new_from_pixbuf(arrow_down_pixbuf));
		break;

	default:
		image = NULL;
		break;

	}

	if (usedef && (image == NULL)) {
		image = GTK_IMAGE(nsgtk_image_new_from_stock("gtk-missing-image", iconsize));
	}

	return image;
}

/**
 * Get default image for search buttons / menu items from gtk stock items
 *
 * \param tbbutton search button reference
 * \param iconsize The size of icons to select.
 * \param usedef Use the default image if not found.
 * \return default search image.
 */

static GtkImage *
nsgtk_theme_searchimage_default(nsgtk_search_buttons tbbutton,
				GtkIconSize iconsize,
				bool usedef)
{
	GtkImage *image;

	switch (tbbutton) {

	case (SEARCH_BACK_BUTTON):
		image = GTK_IMAGE(nsgtk_image_new_from_stock(
					  NSGTK_STOCK_GO_BACK, iconsize));
		break;

	case (SEARCH_FORWARD_BUTTON):
		image = GTK_IMAGE(nsgtk_image_new_from_stock(
					  NSGTK_STOCK_GO_FORWARD, iconsize));
		break;

	case (SEARCH_CLOSE_BUTTON):
		image = GTK_IMAGE(nsgtk_image_new_from_stock(
					  NSGTK_STOCK_CLOSE, iconsize));
		break;

	default:
		image = NULL;
	}

	if (usedef && (image == NULL)) {
		image = GTK_IMAGE(nsgtk_image_new_from_stock(
					  "gtk-missing-image", iconsize));
	}

	return image;
}

/**
 * initialise a theme structure with gtk images
 *
 * \param iconsize The size of icon to load
 * \param usedef use the default gtk icon if unset
 */
static struct nsgtk_theme *nsgtk_theme_load(GtkIconSize iconsize, bool usedef)
{
	struct nsgtk_theme *theme;
	int btnloop;

	theme = malloc(sizeof(struct nsgtk_theme));
	if (theme == NULL) {
		return NULL;
	}

	for (btnloop = BACK_BUTTON;
	     btnloop < PLACEHOLDER_BUTTON ;
	     btnloop++) {
		theme->image[btnloop] = nsgtk_theme_image_default(btnloop,
								  iconsize,
								  usedef);
	}

	for (btnloop = SEARCH_BACK_BUTTON;
	     btnloop < SEARCH_BUTTONS_COUNT;
	     btnloop++) {
		theme->searchimage[btnloop] =
			nsgtk_theme_searchimage_default(btnloop,
							iconsize,
							usedef);
	}
	return theme;
}

static struct nsgtk_toolbar_item *
nsgtk_scaffolding_button(struct nsgtk_scaffolding *g, int i)
{
	return NULL;
}

/* exported function documented in gtk/toolbar.h */
void nsgtk_theme_implement(struct nsgtk_scaffolding *g)
{
	struct nsgtk_theme *theme[IMAGE_SET_COUNT];
	int i;
	struct nsgtk_toolbar_item *button;
	struct gtk_search *search;

	theme[IMAGE_SET_MAIN_MENU] = nsgtk_theme_load(GTK_ICON_SIZE_MENU, false);
	theme[IMAGE_SET_RCLICK_MENU] = nsgtk_theme_load(GTK_ICON_SIZE_MENU, false);
	theme[IMAGE_SET_POPUP_MENU] = nsgtk_theme_load(GTK_ICON_SIZE_MENU, false);
	theme[IMAGE_SET_BUTTONS] = nsgtk_theme_load(GTK_ICON_SIZE_LARGE_TOOLBAR, false);

	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		if ((i == URL_BAR_ITEM) || (i == THROBBER_ITEM) ||
		    (i == WEBSEARCH_ITEM))
			continue;

		button = nsgtk_scaffolding_button(g, i);
		if (button == NULL)
			continue;

		#if 0
		/* gtk_image_menu_item_set_image accepts NULL image */
		if ((button->main != NULL) &&
		    (theme[IMAGE_SET_MAIN_MENU] != NULL)) {
			nsgtk_image_menu_item_set_image(
				GTK_WIDGET(button->main),
				GTK_WIDGET(theme[IMAGE_SET_MAIN_MENU]->image[i]));
			gtk_widget_show_all(GTK_WIDGET(button->main));
		}
		if ((button->rclick != NULL) &&
		    (theme[IMAGE_SET_RCLICK_MENU] != NULL)) {
			nsgtk_image_menu_item_set_image(GTK_WIDGET(button->rclick),
						      GTK_WIDGET(
							      theme[IMAGE_SET_RCLICK_MENU]->
							      image[i]));
			gtk_widget_show_all(GTK_WIDGET(button->rclick));
		}
		if ((button->popup != NULL) &&
		    (theme[IMAGE_SET_POPUP_MENU] != NULL)) {
			nsgtk_image_menu_item_set_image(GTK_WIDGET(button->popup),
						      GTK_WIDGET(
							      theme[IMAGE_SET_POPUP_MENU]->
							      image[i]));
			gtk_widget_show_all(GTK_WIDGET(button->popup));
		}
		#endif
		if ((button->location != -1) &&
		    (button->button != NULL) &&
		    (theme[IMAGE_SET_BUTTONS] != NULL)) {
			gtk_tool_button_set_icon_widget(
				GTK_TOOL_BUTTON(button->button),
				GTK_WIDGET(
					theme[IMAGE_SET_BUTTONS]->
					image[i]));
			gtk_widget_show_all(GTK_WIDGET(button->button));
		}
	}

	/* set search bar images */
	search = nsgtk_scaffolding_search(g);
	if ((search != NULL) && (theme[IMAGE_SET_MAIN_MENU] != NULL)) {
		/* gtk_tool_button_set_icon_widget accepts NULL image */
		if (search->buttons[SEARCH_BACK_BUTTON] != NULL) {
			gtk_tool_button_set_icon_widget(
				search->buttons[SEARCH_BACK_BUTTON],
				GTK_WIDGET(theme[IMAGE_SET_MAIN_MENU]->
					   searchimage[SEARCH_BACK_BUTTON]));
			gtk_widget_show_all(GTK_WIDGET(
						    search->buttons[SEARCH_BACK_BUTTON]));
		}
		if (search->buttons[SEARCH_FORWARD_BUTTON] != NULL) {
			gtk_tool_button_set_icon_widget(
				search->buttons[SEARCH_FORWARD_BUTTON],
				GTK_WIDGET(theme[IMAGE_SET_MAIN_MENU]->
					   searchimage[SEARCH_FORWARD_BUTTON]));
			gtk_widget_show_all(GTK_WIDGET(
						    search->buttons[
							    SEARCH_FORWARD_BUTTON]));
		}
		if (search->buttons[SEARCH_CLOSE_BUTTON] != NULL) {
			gtk_tool_button_set_icon_widget(
				search->buttons[SEARCH_CLOSE_BUTTON],
				GTK_WIDGET(theme[IMAGE_SET_MAIN_MENU]->
					   searchimage[SEARCH_CLOSE_BUTTON]));
			gtk_widget_show_all(GTK_WIDGET(
						    search->buttons[SEARCH_CLOSE_BUTTON]));
		}
	}

	for (i = 0; i < IMAGE_SET_COUNT; i++) {
		if (theme[i] != NULL) {
			free(theme[i]);
		}
	}
}


/**
 * callback function to iterate toolbar's widgets
 */
static void nsgtk_toolbar_clear_toolbar(GtkWidget *widget, gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;
	gtk_container_remove(GTK_CONTAINER(nsgtk_scaffolding_toolbar(g)),
			     widget);
}

/**
 * connect temporary handler for toolbar edit events
 *
 * \param g The scaffolding
 * \param bi The button index
 */
static void nsgtk_toolbar_temp_connect(struct nsgtk_scaffolding *g,
				       nsgtk_toolbar_button bi)
{
	struct nsgtk_toolbar_item *bc;

	if (bi != URL_BAR_ITEM) {
		bc = nsgtk_scaffolding_button(g, bi);
		if ((bc->button != NULL) && (bc->dataminus != NULL)) {
			g_signal_connect(bc->button,
					 "drag-data-get",
					 G_CALLBACK(bc->dataminus),
					 g);
		}
	}
}

/**
 * get scaffolding button index of button at location
 *
 * \return toolbar item id from location when there is an item at that logical
 * location; else -1
 */
static nsgtk_toolbar_button
nsgtk_toolbar_get_id_at_location(struct nsgtk_scaffolding *g, int i)
{
	int q;
	for (q = BACK_BUTTON; q < PLACEHOLDER_BUTTON; q++) {
		if (nsgtk_scaffolding_button(g, q)->location == i) {
			return q;
		}
	}
	return -1;
}


/**
 * returns a string without its underscores
 *
 * \param s The string to change.
 * \param replacespace true to insert a space where there was an underscore
 * \return The altered string
 */
static char *remove_underscores(const char *s, bool replacespace)
{
	size_t i, ii, len;
	char *ret;
	len = strlen(s);
	ret = malloc(len + 1);
	if (ret == NULL) {
		return NULL;
	}
	for (i = 0, ii = 0; i < len; i++) {
		if (s[i] != '_') {
			ret[ii++] = s[i];
		} else if (replacespace) {
			ret[ii++] = ' ';
		}
	}
	ret[ii] = '\0';
	return ret;
}


/**
 * create throbber toolbar item widget
 *
 * create a gtk entry widget with a completion attached
 */
static GtkToolItem *
make_toolbar_item_throbber(void)
{
	nserror res;
	GtkToolItem *item;
	GdkPixbuf *pixbuf;
	GtkWidget *image;

	res = nsgtk_throbber_get_frame(0, &pixbuf);
	if (res != NSERROR_OK) {
		return NULL;
	}

	if (edit_mode) {
		item = gtk_tool_button_new(
				GTK_WIDGET(gtk_image_new_from_pixbuf(pixbuf)),
				"[throbber]");
	} else {
		item = gtk_tool_item_new();

		image = gtk_image_new_from_pixbuf(pixbuf);
		if (image != NULL) {
			nsgtk_widget_set_alignment(image,
						   GTK_ALIGN_CENTER,
						   GTK_ALIGN_CENTER);
			nsgtk_widget_set_margins(image, 3, 0);

			gtk_container_add(GTK_CONTAINER(item), image);
		}
	}
	return item;
}

/**
 * create url bar toolbar item widget
 *
 * create a gtk entry widget with a completion attached
 */
static GtkToolItem *
make_toolbar_item_url_bar(void)
{
	GtkToolItem *item;
	GtkWidget *entry;
	GtkEntryCompletion *completion;

	item = gtk_tool_item_new();
	entry = nsgtk_entry_new();
	completion = gtk_entry_completion_new();

	if ((entry == NULL) || (completion == NULL) || (item == NULL)) {
		return NULL;
	}

	gtk_entry_set_completion(GTK_ENTRY(entry), completion);
	gtk_container_add(GTK_CONTAINER(item), entry);
	gtk_tool_item_set_expand(item, TRUE);

	return item;
}


/**
 * create web search toolbar item widget
 */
static GtkToolItem *
make_toolbar_item_websearch(void)
{
	GtkToolItem *item;

	if (edit_mode) {
		item = gtk_tool_button_new(
				GTK_WIDGET(nsgtk_image_new_from_stock(
						NSGTK_STOCK_FIND,
						GTK_ICON_SIZE_LARGE_TOOLBAR)),
				"[websearch]");
	} else {
		nserror res;
		GtkWidget *entry;
		struct bitmap *bitmap;
		GdkPixbuf *pixbuf = NULL;

		entry = nsgtk_entry_new();
		item = gtk_tool_item_new();

		if ((entry == NULL) || (item == NULL)) {
			return NULL;
		}

		gtk_widget_set_size_request(entry, NSGTK_WEBSEARCH_WIDTH, -1);

		res = search_web_get_provider_bitmap(&bitmap);
		if ((res == NSERROR_OK) && (bitmap != NULL)) {
			pixbuf = nsgdk_pixbuf_get_from_surface(bitmap->surface,
							       16, 16);
		}

		if (pixbuf != NULL) {
			nsgtk_entry_set_icon_from_pixbuf(entry,
							 GTK_ENTRY_ICON_PRIMARY,
							 pixbuf);
		} else {
			nsgtk_entry_set_icon_from_stock(entry,
							GTK_ENTRY_ICON_PRIMARY,
							NSGTK_STOCK_INFO);
		}

		gtk_container_add(GTK_CONTAINER(item), entry);
	}

	return item;
}


/**
 * widget factory for creation of toolbar item widgets
 *
 * \param i the id of the widget
 * \param theme the theme to make the widgets from
 * \return gtk widget
 */
static GtkWidget *
make_toolbar_item(nsgtk_toolbar_button i, struct nsgtk_theme *theme)
{
	GtkWidget *w = NULL;

	switch(i) {

/* gtk_tool_button_new() accepts NULL args */
#define MAKE_STOCKBUTTON(p, q)					\
	case p##_BUTTON: {					\
		GtkStockItem item;					\
		char *label = NULL;					\
		if (nsgtk_stock_lookup(q, &item) &&			\
		    (item.label != NULL) &&				\
		    ((label = remove_underscores(item.label, false)) != NULL)) { \
			w = GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(	\
					   theme->image[p##_BUTTON]), label)); \
			free(label);					\
		} else {						\
			w = GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(	\
					   theme->image[p##_BUTTON]), q)); \
		}							\
		break;							\
	}

	MAKE_STOCKBUTTON(HOME, NSGTK_STOCK_HOME)
	MAKE_STOCKBUTTON(BACK, NSGTK_STOCK_GO_BACK)
	MAKE_STOCKBUTTON(FORWARD, NSGTK_STOCK_GO_FORWARD)
	MAKE_STOCKBUTTON(STOP, NSGTK_STOCK_STOP)
	MAKE_STOCKBUTTON(RELOAD, NSGTK_STOCK_REFRESH)
#undef MAKE_STOCKBUTTON

	case HISTORY_BUTTON:
		w = GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(
				theme->image[HISTORY_BUTTON]), "H"));
		break;

	case URL_BAR_ITEM:
		w = GTK_WIDGET(make_toolbar_item_url_bar());
		break;

	case THROBBER_ITEM:
		w = GTK_WIDGET(make_toolbar_item_throbber());
		break;

	case WEBSEARCH_ITEM:
		w = GTK_WIDGET(make_toolbar_item_websearch());
		break;

/* gtk_tool_button_new accepts NULL args */
#define MAKE_MENUBUTTON(p, q)						\
	case p##_BUTTON: {						\
		char *label = NULL;					\
		label = remove_underscores(messages_get(#q), false);	\
		w = GTK_WIDGET(gtk_tool_button_new(			\
					GTK_WIDGET(theme->image[p##_BUTTON]), \
					label));			\
		if (label != NULL) {					\
			free(label);					\
		}							\
		break;							\
	}

	MAKE_MENUBUTTON(NEWWINDOW, gtkNewWindow)
	MAKE_MENUBUTTON(NEWTAB, gtkNewTab)
	MAKE_MENUBUTTON(OPENFILE, gtkOpenFile)
	MAKE_MENUBUTTON(CLOSETAB, gtkCloseTab)
	MAKE_MENUBUTTON(CLOSEWINDOW, gtkCloseWindow)
	MAKE_MENUBUTTON(SAVEPAGE, gtkSavePage)
	MAKE_MENUBUTTON(PRINTPREVIEW, gtkPrintPreview)
	MAKE_MENUBUTTON(PRINT, gtkPrint)
	MAKE_MENUBUTTON(QUIT, gtkQuitMenu)
	MAKE_MENUBUTTON(CUT, gtkCut)
	MAKE_MENUBUTTON(COPY, gtkCopy)
	MAKE_MENUBUTTON(PASTE, gtkPaste)
	MAKE_MENUBUTTON(DELETE, gtkDelete)
	MAKE_MENUBUTTON(SELECTALL, gtkSelectAll)
	MAKE_MENUBUTTON(PREFERENCES, gtkPreferences)
	MAKE_MENUBUTTON(ZOOMPLUS, gtkZoomPlus)
	MAKE_MENUBUTTON(ZOOMMINUS, gtkZoomMinus)
	MAKE_MENUBUTTON(ZOOMNORMAL, gtkZoomNormal)
	MAKE_MENUBUTTON(FULLSCREEN, gtkFullScreen)
	MAKE_MENUBUTTON(VIEWSOURCE, gtkViewSource)
	MAKE_MENUBUTTON(CONTENTS, gtkContents)
	MAKE_MENUBUTTON(ABOUT, gtkAbout)
	MAKE_MENUBUTTON(PDF, gtkPDF)
	MAKE_MENUBUTTON(PLAINTEXT, gtkPlainText)
	MAKE_MENUBUTTON(DRAWFILE, gtkDrawFile)
	MAKE_MENUBUTTON(POSTSCRIPT, gtkPostScript)
	MAKE_MENUBUTTON(FIND, gtkFind)
	MAKE_MENUBUTTON(DOWNLOADS, gtkDownloads)
	MAKE_MENUBUTTON(SAVEWINDOWSIZE, gtkSaveWindowSize)
	MAKE_MENUBUTTON(TOGGLEDEBUGGING, gtkToggleDebugging)
	MAKE_MENUBUTTON(SAVEBOXTREE, gtkDebugBoxTree)
	MAKE_MENUBUTTON(SAVEDOMTREE, gtkDebugDomTree)
	MAKE_MENUBUTTON(LOCALHISTORY, gtkLocalHistory)
	MAKE_MENUBUTTON(GLOBALHISTORY, gtkGlobalHistory)
	MAKE_MENUBUTTON(ADDBOOKMARKS, gtkAddBookMarks)
	MAKE_MENUBUTTON(SHOWBOOKMARKS, gtkShowBookMarks)
	MAKE_MENUBUTTON(SHOWCOOKIES, gtkShowCookies)
	MAKE_MENUBUTTON(OPENLOCATION, gtkOpenLocation)
	MAKE_MENUBUTTON(NEXTTAB, gtkNextTab)
	MAKE_MENUBUTTON(PREVTAB, gtkPrevTab)
	MAKE_MENUBUTTON(GUIDE, gtkGuide)
	MAKE_MENUBUTTON(INFO, gtkUserInformation)
	MAKE_MENUBUTTON(OPENMENU, gtkOpenMenu)
#undef MAKE_MENUBUTTON

	default:
		break;

	}

	if (w == NULL) {
		nsgtk_warning(messages_get("NoMemory"), 0);
	}

	return w;
}

/* exported interface documented in gtk/scaffolding.h */
static void nsgtk_scaffolding_reset_offset(struct nsgtk_scaffolding *g)
{
	//g->offset = 0;
}

/**
 * called when a widget is dropped onto the toolbar
 */
static gboolean
nsgtk_toolbar_data(GtkWidget *widget,
		   GdkDragContext *gdc,
		   gint x,
		   gint y,
		   guint time,
		   gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;
	int ind = gtk_toolbar_get_drop_index(nsgtk_scaffolding_toolbar(g),
			x, y);
	int q, i;
	if (window->currentbutton == -1)
		return TRUE;
	struct nsgtk_theme *theme =
		nsgtk_theme_load(GTK_ICON_SIZE_LARGE_TOOLBAR, false);
	if (theme == NULL) {
		nsgtk_warning(messages_get("NoMemory"), 0);
		return TRUE;
	}
	if (nsgtk_scaffolding_button(g, window->currentbutton)->location
			!= -1) {
		/* widget was already in the toolbar; so replace */
		if (nsgtk_scaffolding_button(g, window->currentbutton)->
				location < ind)
			ind--;
		gtk_container_remove(GTK_CONTAINER(
				nsgtk_scaffolding_toolbar(g)), GTK_WIDGET(
				nsgtk_scaffolding_button(g,
				window->currentbutton)->button));
		/* 'move' all widgets further right than the original location,
		 * one place to the left in logical schema */
		for (i = nsgtk_scaffolding_button(g, window->currentbutton)->
				location + 1; i < PLACEHOLDER_BUTTON; i++) {
			q = nsgtk_toolbar_get_id_at_location(g, i);
			if (q == -1)
				continue;
			nsgtk_scaffolding_button(g, q)->location--;
		}
		nsgtk_scaffolding_button(g, window->currentbutton)->
				location = -1;
	}
	nsgtk_scaffolding_button(g, window->currentbutton)->button =
			GTK_TOOL_ITEM(make_toolbar_item(window->currentbutton, theme));
	free(theme);
	if (nsgtk_scaffolding_button(g, window->currentbutton)->button
			== NULL) {
		nsgtk_warning("NoMemory", 0);
		return TRUE;
	}
	/* update logical schema */
	nsgtk_scaffolding_reset_offset(g);
	/* 'move' all widgets further right than the new location, one place to
	 * the right in logical schema */
	for (i = PLACEHOLDER_BUTTON - 1; i >= ind; i--) {
		q = nsgtk_toolbar_get_id_at_location(g, i);
		if (q == -1)
			continue;
		nsgtk_scaffolding_button(g, q)->location++;
	}
	nsgtk_scaffolding_button(g, window->currentbutton)->location = ind;

	/* complete action */
	GtkToolItem *current_button;

	current_button = GTK_TOOL_ITEM(nsgtk_scaffolding_button(g, window->currentbutton)->button);

	gtk_toolbar_insert(nsgtk_scaffolding_toolbar(g), current_button, ind);

	gtk_tool_item_set_use_drag_window(current_button, TRUE);
	gtk_drag_source_set(GTK_WIDGET(current_button),
			    GDK_BUTTON1_MASK, &entry, 1,
			    GDK_ACTION_COPY);
	nsgtk_toolbar_temp_connect(g, window->currentbutton);
	gtk_widget_show_all(GTK_WIDGET(current_button));


	window->currentbutton = -1;

	return TRUE;
}

/**
 * connected to toolbutton drop; perhaps one day it'll work properly so it may
 * replace the global current_button
 */
static gboolean
nsgtk_toolbar_move_complete(GtkWidget *widget,
			    GdkDragContext *gdc,
			    gint x,
			    gint y,
			    GtkSelectionData *selection,
			    guint info,
			    guint time,
			    gpointer data)
{
	return FALSE;
}

/**
 * called when hovering an item above the toolbar
 */
static gboolean
nsgtk_toolbar_action(GtkWidget *widget, GdkDragContext *gdc, gint x,
		gint y, guint time, gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;
	GtkToolItem *item = gtk_tool_button_new(NULL, NULL);
	if (item != NULL)
		gtk_toolbar_set_drop_highlight_item(
				nsgtk_scaffolding_toolbar(g),
				GTK_TOOL_ITEM(item),
				gtk_toolbar_get_drop_index(
				nsgtk_scaffolding_toolbar(g), x, y));
	return FALSE;
}

/**
 * called when hovering stops
 */
static void
nsgtk_toolbar_clear(GtkWidget *widget, GdkDragContext *gdc, guint time,
		gpointer data)
{
	gtk_toolbar_set_drop_highlight_item(GTK_TOOLBAR(widget), NULL, 0);
}

/**
 * add item to toolbar.
 *
 * the function should be called, when multiple items are being added,
 * in ascending order.
 *
 * \param g the scaffolding whose toolbar an item is added to.
 * \param i the location in the toolbar.
 * \param theme The theme in use.
 */
static void
nsgtk_toolbar_add_item_to_toolbar(struct nsgtk_scaffolding *g, int i,
		struct nsgtk_theme *theme)
{
	int q;
	for (q = BACK_BUTTON; q < PLACEHOLDER_BUTTON; q++)
		if (nsgtk_scaffolding_button(g, q)->location == i) {
			nsgtk_scaffolding_button(g, q)->button = GTK_TOOL_ITEM(
					make_toolbar_item(q, theme));
			gtk_toolbar_insert(nsgtk_scaffolding_toolbar(g),
					nsgtk_scaffolding_button(g, q)->button,
					i);
			break;
		}
}

/**
 * cleanup code physical update of all toolbars; resensitize
 * \param g the 'front' scaffolding that called customize
 */
static void nsgtk_toolbar_close(struct nsgtk_scaffolding *g)
{
	int i;

	struct nsgtk_scaffolding *list;
	struct nsgtk_theme *theme;

	list = nsgtk_scaffolding_iterate(NULL);
	while (list) {
		theme =	nsgtk_theme_load(GTK_ICON_SIZE_LARGE_TOOLBAR, false);
		if (theme == NULL) {
			nsgtk_warning(messages_get("NoMemory"), 0);
			continue;
		}
		/* clear toolbar */
		gtk_container_foreach(GTK_CONTAINER(nsgtk_scaffolding_toolbar(
				list)), nsgtk_toolbar_clear_toolbar, list);
		/* then add items */
		for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
			nsgtk_toolbar_add_item_to_toolbar(list, i, theme);
		}
		nsgtk_toolbar_connect_all(list);
		gtk_widget_show_all(GTK_WIDGET(nsgtk_scaffolding_toolbar(
				list)));
		nsgtk_scaffolding_set_sensitivity(list);
		nsgtk_widget_override_background_color(
			GTK_WIDGET(nsgtk_window_get_layout(nsgtk_scaffolding_top_level(list))),
			GTK_STATE_FLAG_NORMAL,
			0, 0xFFFF, 0xFFFF, 0xFFFF);
		g_signal_handler_unblock(GTK_WIDGET(
				nsgtk_window_get_layout(
				nsgtk_scaffolding_top_level(list))),
				nsgtk_window_get_signalhandler(
				nsgtk_scaffolding_top_level(list),
				NSGTK_WINDOW_SIGNAL_CLICK));
		g_signal_handler_unblock(GTK_WIDGET(
				nsgtk_window_get_layout(
				nsgtk_scaffolding_top_level(list))),
				nsgtk_window_get_signalhandler(
				nsgtk_scaffolding_top_level(list),
				NSGTK_WINDOW_SIGNAL_REDRAW));
		browser_window_refresh_url_bar(
				nsgtk_get_browser_window(
				nsgtk_scaffolding_top_level(list)));

		if (list != g)
			gtk_widget_set_sensitive(GTK_WIDGET(
					nsgtk_scaffolding_window(list)), TRUE);
		free(theme);
		list = nsgtk_scaffolding_iterate(list);
	}
	gtk_widget_set_sensitive(GTK_WIDGET(nsgtk_scaffolding_notebook(g)),
			TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(nsgtk_scaffolding_menu_bar(g)),
			TRUE);
	/* update favicon etc */
	nsgtk_scaffolding_set_top_level(nsgtk_scaffolding_top_level(g));

	search_web_select_provider(-1);
}


/**
 * set toolbar logical -> physical; physically visible toolbar buttons are made
 * to correspond to the logically stored schema in terms of location
 * visibility etc
 */
static void nsgtk_toolbar_set_physical(struct nsgtk_scaffolding *g)
{
	int i;
	struct nsgtk_theme *theme;

	theme =	nsgtk_theme_load(GTK_ICON_SIZE_LARGE_TOOLBAR, false);
	if (theme == NULL) {
		nsgtk_warning(messages_get("NoMemory"), 0);
		return;
	}
	/* simplest is to clear the toolbar then reload it from memory */
	gtk_container_foreach(GTK_CONTAINER(nsgtk_scaffolding_toolbar(g)),
			nsgtk_toolbar_clear_toolbar, g);
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		nsgtk_toolbar_add_item_to_toolbar(g, i, theme);
	}
	gtk_widget_show_all(GTK_WIDGET(nsgtk_scaffolding_toolbar(g)));
	free(theme);
}


/**
 * when cancel button is clicked
 */
static gboolean nsgtk_toolbar_cancel_clicked(GtkWidget *widget, gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;

	edit_mode = false;
	/* reset g->buttons->location */
	for (int i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		nsgtk_scaffolding_button(g, i)->location =
				window->buttonlocations[i];
	}
	nsgtk_toolbar_set_physical(g);
	nsgtk_toolbar_connect_all(g);
	nsgtk_toolbar_close(g);
	nsgtk_scaffolding_set_sensitivity(g);
	gtk_widget_destroy(window->window);
	return TRUE;
}

/**
 * physically add widgets to store window
 */
static bool nsgtk_toolbar_add_store_widget(GtkWidget *widget)
{
	if (window->numberh >= NSGTK_STORE_WIDTH) {
		window->currentbar = gtk_toolbar_new();
		if (window->currentbar == NULL) {
			nsgtk_warning("NoMemory", 0);
			return false;
		}
		gtk_toolbar_set_style(GTK_TOOLBAR(window->currentbar),
				GTK_TOOLBAR_BOTH);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(window->currentbar),
				GTK_ICON_SIZE_LARGE_TOOLBAR);
		gtk_box_pack_start(GTK_BOX(window->widgetvbox),
			window->currentbar, FALSE, FALSE, 0);
		window->numberh = 0;
	}
	gtk_widget_set_size_request(widget, NSGTK_BUTTON_WIDTH,
			NSGTK_BUTTON_HEIGHT);
	gtk_toolbar_insert(GTK_TOOLBAR(window->currentbar), GTK_TOOL_ITEM(
			widget), window->numberh++);
	gtk_tool_item_set_use_drag_window(GTK_TOOL_ITEM(widget), TRUE);
	gtk_drag_source_set(widget, GDK_BUTTON1_MASK, &entry, 1,
			GDK_ACTION_COPY);
	gtk_widget_show_all(window->window);
	return true;
}


/**
 * cast toolbar settings to all scaffoldings referenced from the global linked
 * list of gui_windows
 */
static void nsgtk_toolbar_cast(struct nsgtk_scaffolding *g)
{
	int i;
	struct nsgtk_scaffolding *list;

	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		window->buttonlocations[i] =
				((nsgtk_scaffolding_button(g, i)->location
				>= -1) &&
				(nsgtk_scaffolding_button(g, i)->location
				< PLACEHOLDER_BUTTON)) ?
				nsgtk_scaffolding_button(g, i)->location : -1;
	}

	list = nsgtk_scaffolding_iterate(NULL);
	while (list) {
		if (list != g)
			for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++)
				nsgtk_scaffolding_button(list, i)->location =
						window->buttonlocations[i];
		list = nsgtk_scaffolding_iterate(list);
	}
}




/**
 * save toolbar settings to file
 */
static nserror nsgtk_toolbar_customization_save(struct nsgtk_scaffolding *g)
{
	char *choices = NULL;
	char *order;
	int order_len = PLACEHOLDER_BUTTON * 12; /* length of order buffer */
	int tbidx;
	char *cur;
	int plen;

	order = malloc(order_len);

	if (order == NULL) {
		return NSERROR_NOMEM;
	}
	cur = order;

	for (tbidx = BACK_BUTTON; tbidx < PLACEHOLDER_BUTTON; tbidx++) {
		plen = snprintf(cur,
				order_len,
				"%d;%d|",
				tbidx,
				nsgtk_scaffolding_button(g, tbidx)->location);
		if (plen == order_len) {
			/* ran out of space, bail early */
			NSLOG(netsurf, INFO,
			      "toolbar ordering exceeded available space");
			break;
		}
		cur += plen;
		order_len -= plen;
	}

	nsoption_set_charp(toolbar_order, order);

	/* ensure choices are saved */
	netsurf_mkpath(&choices, NULL, 2, nsgtk_config_home, "Choices");
	if (choices != NULL) {
		nsoption_write(choices, NULL, NULL);
		free(choices);
	}

	return NSERROR_OK;
}


/**
 * when 'save settings' button is clicked
 */
static gboolean nsgtk_toolbar_persist(GtkWidget *widget, gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;

	edit_mode = false;
	/* save state to file, update toolbars for all windows */
	nsgtk_toolbar_customization_save(g);
	nsgtk_toolbar_cast(g);
	nsgtk_toolbar_set_physical(g);
	nsgtk_toolbar_close(g);
	gtk_widget_destroy(window->window);
	return TRUE;
}

/**
 * when 'reload defaults' button is clicked
 */
static gboolean nsgtk_toolbar_reset(GtkWidget *widget, gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;
	int i;
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++)
		nsgtk_scaffolding_button(g, i)->location =
				(i <= THROBBER_ITEM) ? i : -1;
	nsgtk_toolbar_set_physical(g);
	for (i = BACK_BUTTON; i <= THROBBER_ITEM; i++) {
		if (i == URL_BAR_ITEM)
			continue;
		gtk_tool_item_set_use_drag_window(GTK_TOOL_ITEM(
				nsgtk_scaffolding_button(g, i)->button), TRUE);
		gtk_drag_source_set(GTK_WIDGET(
				nsgtk_scaffolding_button(g, i)->button),
				GDK_BUTTON1_MASK, &entry, 1, GDK_ACTION_COPY);
		nsgtk_toolbar_temp_connect(g, i);
	}
	return TRUE;
}

/**
 * when titlebar / alt-F4 window close event happens
 */
static gboolean nsgtk_toolbar_delete(GtkWidget *widget, GdkEvent *event,
		gpointer data)
{
	edit_mode = false;
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;
	/* reset g->buttons->location */
	for (int i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		nsgtk_scaffolding_button(g, i)->location =
				window->buttonlocations[i];
	}
	nsgtk_toolbar_set_physical(g);
	nsgtk_toolbar_connect_all(g);
	nsgtk_toolbar_close(g);
	nsgtk_scaffolding_set_sensitivity(g);
	gtk_widget_destroy(window->window);
	return TRUE;
}

/**
 * called when a widget is dropped onto the store window
 */
static gboolean
nsgtk_toolbar_store_return(GtkWidget *widget, GdkDragContext *gdc,
		gint x, gint y, guint time, gpointer data)
{
	struct nsgtk_scaffolding *g = (struct nsgtk_scaffolding *)data;
	int q, i;

	if ((window->fromstore) || (window->currentbutton == -1)) {
		window->currentbutton = -1;
		return FALSE;
	}
	if (nsgtk_scaffolding_button(g, window->currentbutton)->location
			!= -1) {
		/* 'move' all widgets further right, one place to the left
		 * in logical schema */
		for (i = nsgtk_scaffolding_button(g, window->currentbutton)->
				location + 1; i < PLACEHOLDER_BUTTON; i++) {
			q = nsgtk_toolbar_get_id_at_location(g, i);
			if (q == -1)
				continue;
			nsgtk_scaffolding_button(g, q)->location--;
		}
		gtk_container_remove(GTK_CONTAINER(
				nsgtk_scaffolding_toolbar(g)), GTK_WIDGET(
				nsgtk_scaffolding_button(g,
				window->currentbutton)->button));
		nsgtk_scaffolding_button(g, window->currentbutton)->location
				= -1;
	}
	window->currentbutton = -1;
	gtk_drag_finish(gdc, TRUE, TRUE, time);
	return FALSE;
}

/**
 * called when hovering above the store
 */
static gboolean
nsgtk_toolbar_store_action(GtkWidget *widget, GdkDragContext *gdc,
		gint x, gint y, guint time, gpointer data)
{
	return FALSE;
}

/**
 * create store window
 */
static void nsgtk_toolbar_window_open(struct nsgtk_scaffolding *g)
{
	struct nsgtk_theme *theme;
	nserror res;

	theme =	nsgtk_theme_load(GTK_ICON_SIZE_LARGE_TOOLBAR, true);
	if (theme == NULL) {
		nsgtk_warning(messages_get("NoMemory"), 0);
		nsgtk_toolbar_cancel_clicked(NULL, g);
		return;
	}

	res = nsgtk_builder_new_from_resname("toolbar", &window->builder);
	if (res != NSERROR_OK) {
		NSLOG(netsurf, INFO, "Toolbar UI builder init failed");
		nsgtk_warning("Toolbar UI builder init failed", 0);
		nsgtk_toolbar_cancel_clicked(NULL, g);
		free(theme);
		return;
	}

	gtk_builder_connect_signals(window->builder, NULL);

	window->window = GTK_WIDGET(gtk_builder_get_object(
					    window->builder, "dialogToolbar"));
	if (window->window == NULL) {
		nsgtk_warning(messages_get("NoMemory"), 0);
		nsgtk_toolbar_cancel_clicked(NULL, g);
		free(theme);
		return;
	}

	gtk_window_set_transient_for(GTK_WINDOW(window->window),
				     nsgtk_scaffolding_window(g));

	window->widgetvbox = GTK_WIDGET(gtk_builder_get_object(
						window->builder, "widgetvbox"));
	if (window->widgetvbox == NULL) {
		nsgtk_warning(messages_get("NoMemory"), 0);
		nsgtk_toolbar_cancel_clicked(NULL, g);
		free(theme);
		return;
	}

	/* preset to width [in buttons] of */
	window->numberh = NSGTK_STORE_WIDTH;

	/*  store to cause creation of a new toolbar */
	window->currentbutton = -1;

	/* load toolbuttons */
	/* add toolbuttons to window */
	/* set event handlers */
	for (int i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		if (i == URL_BAR_ITEM)
			continue;
		window->store_buttons[i] =
			make_toolbar_item(i, theme);
		if (window->store_buttons[i] == NULL) {
			nsgtk_warning(messages_get("NoMemory"), 0);
			continue;
		}
		nsgtk_toolbar_add_store_widget(window->store_buttons[i]);
		g_signal_connect(window->store_buttons[i], "drag-data-get",
				 G_CALLBACK(
					 nsgtk_scaffolding_button(g, i)->dataplus), g);
	}
	free(theme);


	gtk_window_set_accept_focus(GTK_WINDOW(window->window), FALSE);

	gtk_drag_dest_set(GTK_WIDGET(window->window), GTK_DEST_DEFAULT_MOTION |
			  GTK_DEST_DEFAULT_DROP, &entry, 1, GDK_ACTION_COPY);

	g_signal_connect(GTK_WIDGET(gtk_builder_get_object(
					    window->builder, "close")),
			 "clicked",
			 G_CALLBACK(nsgtk_toolbar_persist),
			 g);

	g_signal_connect(GTK_WIDGET(gtk_builder_get_object(
					    window->builder, "reset")),
			 "clicked",
			 G_CALLBACK(nsgtk_toolbar_reset),
			 g);

	g_signal_connect(window->window, "delete-event",
			 G_CALLBACK(nsgtk_toolbar_delete), g);

	g_signal_connect(window->window, "drag-drop",
			 G_CALLBACK(nsgtk_toolbar_store_return), g);

	g_signal_connect(window->window, "drag-motion",
			 G_CALLBACK(nsgtk_toolbar_store_action), g);

	gtk_widget_show_all(window->window);
}

/**
 * change behaviour of scaffoldings while editing toolbar
 *
 * All buttons as well as window clicks are desensitized; then buttons
 * in the front window are changed to movable buttons
 */
void nsgtk_toolbar_customization_init(struct nsgtk_scaffolding *g)
{
	int i;
	struct nsgtk_scaffolding *list;
	edit_mode = true;

	list = nsgtk_scaffolding_iterate(NULL);
	while (list) {
		g_signal_handler_block(GTK_WIDGET(
				nsgtk_window_get_layout(
				nsgtk_scaffolding_top_level(list))),
				nsgtk_window_get_signalhandler(
				nsgtk_scaffolding_top_level(list),
				NSGTK_WINDOW_SIGNAL_CLICK));
		g_signal_handler_block(GTK_WIDGET(
				nsgtk_window_get_layout(
				nsgtk_scaffolding_top_level(list))),
				nsgtk_window_get_signalhandler(
				nsgtk_scaffolding_top_level(list),
				NSGTK_WINDOW_SIGNAL_REDRAW));
		nsgtk_widget_override_background_color(
			GTK_WIDGET(nsgtk_window_get_layout(
				nsgtk_scaffolding_top_level(list))),
			GTK_STATE_NORMAL, 0, 0xEEEE, 0xEEEE, 0xEEEE);

		if (list == g) {
			list = nsgtk_scaffolding_iterate(list);
			continue;
		}
		/* set sensitive for all gui_windows save g */
		gtk_widget_set_sensitive(GTK_WIDGET(nsgtk_scaffolding_window(
				list)), FALSE);
		list = nsgtk_scaffolding_iterate(list);
	}
	/* set sensitive for all of g save toolbar */
	gtk_widget_set_sensitive(GTK_WIDGET(nsgtk_scaffolding_menu_bar(g)),
			FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(nsgtk_scaffolding_notebook(g)),
			FALSE);

	/* set editable aspect for toolbar */
	gtk_container_foreach(GTK_CONTAINER(nsgtk_scaffolding_toolbar(g)),
			nsgtk_toolbar_clear_toolbar, g);
	nsgtk_toolbar_set_physical(g);
	/* memorize button locations, set editable */
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		window->buttonlocations[i] = nsgtk_scaffolding_button(g, i)
				->location;
		if ((window->buttonlocations[i] == -1) || (i == URL_BAR_ITEM))
			continue;
		gtk_tool_item_set_use_drag_window(GTK_TOOL_ITEM(
				nsgtk_scaffolding_button(g, i)->button), TRUE);
		gtk_drag_source_set(GTK_WIDGET(nsgtk_scaffolding_button(
				g, i)->button),	GDK_BUTTON1_MASK, &entry, 1,
				GDK_ACTION_COPY);
		nsgtk_toolbar_temp_connect(g, i);
	}

	/* add move button listeners */
	g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_toolbar(g)),
			"drag-drop", G_CALLBACK(nsgtk_toolbar_data), g);
	g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_toolbar(g)),
			"drag-data-received", G_CALLBACK(
			nsgtk_toolbar_move_complete), g);
	g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_toolbar(g)),
			"drag-motion", G_CALLBACK(nsgtk_toolbar_action), g);
	g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_toolbar(g)),
			"drag-leave", G_CALLBACK(
			nsgtk_toolbar_clear), g);

	/* set data types */
	gtk_drag_dest_set(GTK_WIDGET(nsgtk_scaffolding_toolbar(g)),
			GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
			&entry, 1, GDK_ACTION_COPY);

	/* open toolbar window */
	nsgtk_toolbar_window_open(g);
}


/**
 * \return toolbar item id when a widget is an element of the scaffolding
 * else -1
 */
int nsgtk_toolbar_get_id_from_widget(GtkWidget *widget,
				     struct nsgtk_scaffolding *g)
{
	int i;
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		if ((nsgtk_scaffolding_button(g, i)->location != -1)
				&& (widget == GTK_WIDGET(
				nsgtk_scaffolding_button(g, i)->button))) {
			return i;
		}
	}
	return -1;
}



/**
 * connect 'normal' handlers to toolbar buttons
 */
void nsgtk_toolbar_connect_all(struct nsgtk_scaffolding *g)
{
	int q, i;
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		q = nsgtk_toolbar_get_id_at_location(g, i);
		if (q == -1)
			continue;

	}
}


/**
 * Apply the user toolbar button settings from configuration
 *
 * GTK specific user option string is a set of fields arranged as
 * [itemreference];[itemlocation]|[itemreference];[itemlocation]| etc
 *
 * \param tb The toolbar to apply customization to
 * \param NSERROR_OK on success else error code.
 */
static nserror
apply_user_button_customization(struct nsgtk_toolbar *tb)
{
	int i, ii;
	char *buffer;
	char *buffer1, *subbuffer, *ptr = NULL, *pter = NULL;

	/* set all button locations to inactive */
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		tb->buttons[i]->location = INACTIVE_LOCATION;
	}

	/* if no user config is present apply the defaults */
	if (nsoption_charp(toolbar_order) == NULL) {
		tb->buttons[BACK_BUTTON]->location = 0;
		tb->buttons[HISTORY_BUTTON]->location = 1;
		tb->buttons[FORWARD_BUTTON]->location = 2;
		tb->buttons[STOP_BUTTON]->location = 3;
		tb->buttons[RELOAD_BUTTON]->location = 4;
		tb->buttons[URL_BAR_ITEM]->location = 5;
		tb->buttons[WEBSEARCH_ITEM]->location = 6;
		tb->buttons[THROBBER_ITEM]->location = 7;

		return NSERROR_OK;
	}

	buffer = strdup(nsoption_charp(toolbar_order));
	if (buffer == NULL) {
		return NSERROR_NOMEM;
	}

	i = BACK_BUTTON;
	ii = BACK_BUTTON;
	buffer1 = strtok_r(buffer, "|", &ptr);
	while (buffer1 != NULL) {
		subbuffer = strtok_r(buffer1, ";", &pter);
		if (subbuffer != NULL) {
			i = atoi(subbuffer);
			subbuffer = strtok_r(NULL, ";", &pter);
			if (subbuffer != NULL) {
				ii = atoi(subbuffer);
				if ((i >= BACK_BUTTON) &&
				    (i < PLACEHOLDER_BUTTON) &&
				    (ii >= -1) &&
				    (ii < PLACEHOLDER_BUTTON)) {
					tb->buttons[i]->location = ii;
				}
			}
		}
		buffer1 = strtok_r(NULL, "|", &ptr);
	}

	free(buffer);
	return NSERROR_OK;
}


/**
 * append item to gtk toolbar container
 *
 * \param tb toolbar
 * \param theme in use
 * \param location item location being appended
 * \return NSERROR_OK on success else error code.
 */
static nserror
add_item_to_toolbar(struct nsgtk_toolbar *tb,
		       struct nsgtk_theme *theme,
		       int location)
{
	int bidx; /* button index */

	for (bidx = BACK_BUTTON; bidx < PLACEHOLDER_BUTTON; bidx++) {

		if (tb->buttons[bidx]->location == location) {

			tb->buttons[bidx]->button = GTK_TOOL_ITEM(
					make_toolbar_item(bidx, theme));

			/* set widgets initial sensitivity */
			gtk_widget_set_sensitive(GTK_WIDGET(tb->buttons[bidx]->button),
						 tb->buttons[bidx]->sensitivity);

			gtk_toolbar_insert(tb->widget,
					   tb->buttons[bidx]->button,
					   location);
			break;
		}
	}
	return NSERROR_OK;
}


/**
 * callback function to remove a widget from a container
 */
static void container_remove_widget(GtkWidget *widget, gpointer data)
{
	GtkContainer *container = GTK_CONTAINER(data);
	gtk_container_remove(container, widget);
}


/**
 * populates the gtk toolbar container with widgets in correct order
 */
static nserror populate_gtk_toolbar_widget(struct nsgtk_toolbar *tb)
{
	struct nsgtk_theme *theme; /* internal theme context */
	int lidx; /* location index */

	theme =	nsgtk_theme_load(GTK_ICON_SIZE_LARGE_TOOLBAR, false);
	if (theme == NULL) {
		return NSERROR_NOMEM;
	}

	/* clear the toolbar container of all widgets */
	gtk_container_foreach(GTK_CONTAINER(tb->widget),
			      container_remove_widget,
			      tb->widget);

	/* add widgets to toolbar */
	for (lidx = 0; lidx < PLACEHOLDER_BUTTON; lidx++) {
		add_item_to_toolbar(tb, theme, lidx);
	}

	gtk_widget_show_all(GTK_WIDGET(tb->widget));
	free(theme);

	return NSERROR_OK;
}


/**
 * find the toolbar item with a given location.
 *
 * \param tb the toolbar instance
 * \param locaction the location to search for
 * \return the item id for a location
 */
static nsgtk_toolbar_button
itemid_from_location(struct nsgtk_toolbar *tb, int location)
{
	int iidx;
	for (iidx = BACK_BUTTON; iidx < PLACEHOLDER_BUTTON; iidx++) {
		if (tb->buttons[iidx]->location == location) {
			break;
		}
	}
	return iidx;
}

/**
 * find the toolbar item with a given gtk widget.
 *
 * \param tb the toolbar instance
 * \param toolitem the tool item widget to search for
 * \return the item id matching the widget
 */
static nsgtk_toolbar_button
itemid_from_gtktoolitem(struct nsgtk_toolbar *tb, GtkToolItem *toolitem)
{
	int iidx;
	for (iidx = BACK_BUTTON; iidx < PLACEHOLDER_BUTTON; iidx++) {
		if ((tb->buttons[iidx]->location != INACTIVE_LOCATION) &&
		    (tb->buttons[iidx]->button == toolitem)) {
			break;
		}
	}
	return iidx;
}


/**
 * set a toolbar items sensitivity
 *
 * note this does not set menu items sensitivity
 */
static nserror
set_item_sensitivity(struct nsgtk_toolbar_item *item, bool sensitivity)
{
	if (item->sensitivity != sensitivity) {
		/* item requires sensitivity changing */
		item->sensitivity = sensitivity;

		if ((item->location != -1) && (item->button != NULL)) {
			gtk_widget_set_sensitive(GTK_WIDGET(item->button),
						 item->sensitivity);
		}
	}

	return NSERROR_OK;
}


/**
 * cause the toolbar browsing context to navigate to a new url.
 *
 * \param tb the toolbar context.
 * \param urltxt The url string.
 * \return NSERROR_OK on success else appropriate error code.
 */
static nserror
toolbar_navigate_to_url(struct nsgtk_toolbar *tb, const char *urltxt)
{
	struct browser_window *bw;
	nsurl *url;
	nserror res;

	res = nsurl_create(urltxt, &url);
	if (res != NSERROR_OK) {
		return res;
	}

	bw = tb->get_bw(tb->get_ctx);

	res = browser_window_navigate(bw,
				      url,
				      NULL,
				      BW_NAVIGATE_HISTORY,
				      NULL,
				      NULL,
				      NULL);
	nsurl_unref(url);

	return res;
}


/**
 * run a gtk file chooser as a save dialog to obtain a path
 */
static nserror
nsgtk_saveas_dialog(struct browser_window *bw,
		    const char *title,
		    GtkWindow *parent,
		    bool folder,
		    gchar **path_out)
{
	nserror res;
	GtkWidget *fc; /* file chooser widget */
	GtkFileChooserAction action;
	char *path; /* proposed path */

	if (!browser_window_has_content(bw)) {
		/* cannot save a page with no content */
		return NSERROR_INVALID;
	}

	if (folder) {
		action = GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER;
	} else {
		action = GTK_FILE_CHOOSER_ACTION_SAVE;
	}

	fc = gtk_file_chooser_dialog_new(title,
					 parent,
					 action,
					 NSGTK_STOCK_CANCEL,
					 GTK_RESPONSE_CANCEL,
					 NSGTK_STOCK_SAVE,
					 GTK_RESPONSE_ACCEPT,
					 NULL);

	/* set a default file name */
	res = nsurl_nice(browser_window_access_url(bw), &path, false);
	if (res != NSERROR_OK) {
		path = strdup(messages_get("SaveText"));
		if (path == NULL) {
			gtk_widget_destroy(fc);
			return NSERROR_NOMEM;
		}
	}

	if ((!folder) || (access(path, F_OK) != 0)) {
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(fc), path);
	}
	free(path);

	/* confirm overwriting */
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc), TRUE);

	/* run the dialog to let user select path */
	if (gtk_dialog_run(GTK_DIALOG(fc)) != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy(fc);
		return NSERROR_NOT_FOUND;
	}

	*path_out = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));

	gtk_widget_destroy(fc);

	return NSERROR_OK;
}


/*
 * Toolbar button clicked handlers
 */

/**
 * callback for all toolbar items widget size allocation
 *
 * handler connected to all toolbar items for the size-allocate signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param alloc The size allocation being set.
 * \param data The toolbar context passed when the signal was connected
 */
static void
toolbar_item_size_allocate_cb(GtkWidget *widget,
			      GtkAllocation *alloc,
			      gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	nsgtk_toolbar_button itemid;

	itemid = itemid_from_gtktoolitem(tb, GTK_TOOL_ITEM(widget));

	if ((tb->toolbarmem == alloc->x) ||
	    (tb->buttons[itemid]->location < tb->buttons[HISTORY_BUTTON]->location)) {
		/*
		 * no reallocation after first adjustment,
		 * no reallocation for buttons left of history button
		 */
		return;
	}

	if (itemid == HISTORY_BUTTON) {
		if (alloc->width == 20) {
			return;
		}

		tb->toolbarbase = alloc->y + alloc->height;
		tb->historybase = alloc->x + 20;
		if (tb->offset == 0) {
			tb->offset = alloc->width - 20;
		}
		alloc->width = 20;
	} else if (tb->buttons[itemid]->location <= tb->buttons[URL_BAR_ITEM]->location) {
		alloc->x -= tb->offset;
		if (itemid == URL_BAR_ITEM) {
			alloc->width += tb->offset;
		}
	}
	tb->toolbarmem = alloc->x;

	gtk_widget_size_allocate(widget, alloc);
}


/**
 * handler for back tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
back_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;

	bw = tb->get_bw(tb->get_ctx);

	if ((bw != NULL) && browser_window_history_back_available(bw)) {
		/* clear potential search effects */
		browser_window_search_clear(bw);

		browser_window_history_back(bw, false);

		set_item_sensitivity(tb->buttons[BACK_BUTTON],
				browser_window_history_back_available(bw));
		set_item_sensitivity(tb->buttons[FORWARD_BUTTON],
				browser_window_history_forward_available(bw));

		nsgtk_local_history_hide();
	}
	return TRUE;
}


/**
 * handler for forward tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
forward_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;

	bw = tb->get_bw(tb->get_ctx);

	if ((bw != NULL) && browser_window_history_forward_available(bw)) {
		/* clear potential search effects */
		browser_window_search_clear(bw);

		browser_window_history_forward(bw, false);

		set_item_sensitivity(tb->buttons[BACK_BUTTON],
				browser_window_history_back_available(bw));
		set_item_sensitivity(tb->buttons[FORWARD_BUTTON],
				browser_window_history_forward_available(bw));
		nsgtk_local_history_hide();
	}
	return TRUE;
}


/**
 * handler for stop tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
stop_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;

	browser_window_stop(tb->get_bw(tb->get_ctx));

	return TRUE;
}


/**
 * handler for reload tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
reload_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;

	bw = tb->get_bw(tb->get_ctx);

	/* clear potential search effects */
	browser_window_search_clear(bw);

	browser_window_reload(bw, true);

	return TRUE;
}


/**
 * handler for home tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
home_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	nserror res;
	const char *addr;

	if (nsoption_charp(homepage_url) != NULL) {
		addr = nsoption_charp(homepage_url);
	} else {
		addr = NETSURF_HOMEPAGE;
	}

	res = toolbar_navigate_to_url(tb, addr);
	if (res != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(res), 0);
	}

	return TRUE;
}


/**
 * callback for url entry widget activation
 *
 * handler connected to url entry widget for the activate signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE to allow activation.
 */
static gboolean url_entry_activate_cb(GtkWidget *widget, gpointer data)
{
	nserror res;
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	nsurl *url;

	res = search_web_omni(gtk_entry_get_text(GTK_ENTRY(widget)),
			      SEARCH_WEB_OMNI_NONE,
			      &url);
	if (res == NSERROR_OK) {
		bw = tb->get_bw(tb->get_ctx);
		res = browser_window_navigate(
			bw, url, NULL, BW_NAVIGATE_HISTORY, NULL, NULL, NULL);
		nsurl_unref(url);
	}
	if (res != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(res), 0);
	}

	return TRUE;
}


/**
 * callback for url entry widget changing
 *
 * handler connected to url entry widget for the change signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param event The key change event that changed the entry.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE to allow activation.
 */
static gboolean
url_entry_changed_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	return nsgtk_completion_update(GTK_ENTRY(widget));
}


/**
 * handler for web search tool bar entry item activate signal
 *
 * handler connected to web search entry widget for the activate signal
 *
 * \todo make this user selectable to switch between opening in new
 *   and navigating current window. Possibly improve core search_web interfaces
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean websearch_entry_activate_cb(GtkWidget *widget, gpointer data)
{
	nserror res;
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	nsurl *url;

	res = search_web_omni(gtk_entry_get_text(GTK_ENTRY(widget)),
			      SEARCH_WEB_OMNI_SEARCHONLY,
			      &url);
	if (res == NSERROR_OK) {
		temp_open_background = 0;
		bw = tb->get_bw(tb->get_ctx);

		res = browser_window_create(
			BW_CREATE_HISTORY | BW_CREATE_TAB,
			url,
			NULL,
			bw,
			NULL);
		temp_open_background = -1;
		nsurl_unref(url);
	}
	if (res != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(res), 0);
	}

	return TRUE;
}

/**
 * handler for web search tool bar item button press signal
 *
 * allows a click in the websearch entry field to clear the name of the
 * provider.
 *
 * \todo this does not work well, different behaviour wanted perhaps?
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
websearch_entry_button_press_cb(GtkWidget *widget,
			     GdkEventFocus *f,
			     gpointer data)
{
	gtk_editable_select_region(GTK_EDITABLE(widget), 0, -1);
	gtk_widget_grab_focus(GTK_WIDGET(widget));

	return TRUE;
}


/**
 * handler for new window tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
newwindow_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	nserror res;
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	const char *addr;
	nsurl *url;

	if (nsoption_charp(homepage_url) != NULL) {
		addr = nsoption_charp(homepage_url);
	} else {
		addr = NETSURF_HOMEPAGE;
	}

	res = nsurl_create(addr, &url);
	if (res == NSERROR_OK) {
		bw = tb->get_bw(tb->get_ctx);
		res = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      bw,
					      NULL);
		nsurl_unref(url);
	}
	if (res != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(res), 0);
	}

	return TRUE;
}


/**
 * handler for new tab tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
newtab_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	nserror res = NSERROR_OK;
	nsurl *url = NULL;
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;

	if (!nsoption_bool(new_blank)) {
		const char *addr;
		if (nsoption_charp(homepage_url) != NULL) {
			addr = nsoption_charp(homepage_url);
		} else {
			addr = NETSURF_HOMEPAGE;
		}
		res = nsurl_create(addr, &url);
	}

	if (res == NSERROR_OK) {
		bw = tb->get_bw(tb->get_ctx);

		res = browser_window_create(BW_CREATE_HISTORY |
					    BW_CREATE_TAB,
					    url,
					    NULL,
					    bw,
					    NULL);
	}
	if (url != NULL) {
		nsurl_unref(url);
	}
	if (res != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(res), 0);
	}
	return TRUE;
}


/**
 * handler for open file tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
openfile_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	GtkWidget *dlgOpen;
	gint response;
	GtkWidget *toplevel;
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;

	toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);

	dlgOpen = gtk_file_chooser_dialog_new("Open File",
					      GTK_WINDOW(toplevel),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			NSGTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			NSGTK_STOCK_OPEN, GTK_RESPONSE_OK,
			NULL, NULL);

	response = gtk_dialog_run(GTK_DIALOG(dlgOpen));
	if (response == GTK_RESPONSE_OK) {
		char *urltxt;
		gchar *filename;
		nserror res;
		nsurl *url;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlgOpen));

		urltxt = malloc(strlen(filename) + FILE_SCHEME_PREFIX_LEN + 1);
		if (urltxt != NULL) {
			sprintf(urltxt, FILE_SCHEME_PREFIX"%s", filename);

			res = nsurl_create(urltxt, &url);
			if (res == NSERROR_OK) {
				bw = tb->get_bw(tb->get_ctx);
				res = browser_window_navigate(bw,
							url,
							NULL,
							BW_NAVIGATE_HISTORY,
							NULL,
							NULL,
							NULL);
				nsurl_unref(url);
			}
			if (res != NSERROR_OK) {
				nsgtk_warning(messages_get_errorcode(res), 0);
			}
			free(urltxt);
		}


		g_free(filename);
	}

	gtk_widget_destroy(dlgOpen);

	return TRUE;
}


/**
 * handler for close window tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
closewindow_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	GtkWidget *toplevel;
	toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);
	gtk_widget_destroy(toplevel);
	return TRUE;
}


/**
 * handler for full save export tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
savepage_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	DIR *d;
	gchar *path;
	nserror res;
	GtkWidget *toplevel;

	bw = tb->get_bw(tb->get_ctx);
	toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);

	res = nsgtk_saveas_dialog(bw,
				  messages_get("gtkcompleteSave"),
				  GTK_WINDOW(toplevel),
				  true,
				  &path);
	if (res != NSERROR_OK) {
		return FALSE;
	}

	d = opendir(path);
	if (d == NULL) {
		NSLOG(netsurf, INFO,
		      "Unable to open directory %s for complete save: %s",
		      path,
		      strerror(errno));
		if (errno == ENOTDIR) {
			nsgtk_warning("NoDirError", path);
		} else {
			nsgtk_warning("gtkFileError", path);
		}
		g_free(path);
		return TRUE;
	}
	closedir(d);

	save_complete(browser_window_get_content(bw), path, NULL);
	g_free(path);

	return TRUE;
}


/**
 * handler for pdf export tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
pdf_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	GtkWidget *toplevel;
	gchar *filename;
	nserror res;

	bw = tb->get_bw(tb->get_ctx);

	toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);

	res = nsgtk_saveas_dialog(bw,
				  "Export to PDF",
				  GTK_WINDOW(toplevel),
				  false,
				  &filename);
	if (res != NSERROR_OK) {
		return FALSE;
	}

#ifdef WITH_PDF_EXPORT
	struct print_settings *settings;

	/* this way the scale used by PDF functions is synchronised with that
	 * used by the all-purpose print interface
	 */
	haru_nsfont_set_scale((float)option_export_scale / 100);

	settings = print_make_settings(PRINT_OPTIONS,
				       (const char *) filename,
				       &haru_nsfont);
	g_free(filename);
	if (settings == NULL) {
		return TRUE;
	}
	/* This will clean up the print_settings object for us */
	print_basic_run(browser_window_get_content(bw),	&pdf_printer, settings);
#endif
	return TRUE;

}


/**
 * handler for plain text export tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
plaintext_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	GtkWidget *toplevel;
	gchar *filename;
	nserror res;

	bw = tb->get_bw(tb->get_ctx);

	toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);

	res = nsgtk_saveas_dialog(bw,
				  messages_get("gtkplainSave"),
				  GTK_WINDOW(toplevel),
				  false,
				  &filename);
	if (res != NSERROR_OK) {
		return FALSE;
	}


	save_as_text(browser_window_get_content(bw), filename);
	g_free(filename);

	return TRUE;
}


/**
 * handler for print tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
print_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	GtkPrintOperation *print_op;
	GtkPageSetup *page_setup;
	GtkPrintSettings *print_settings;
	GtkPrintOperationResult res = GTK_PRINT_OPERATION_RESULT_ERROR;
	struct print_settings *nssettings;
	char *settings_fname = NULL;
	GtkWidget *toplevel;

	bw = tb->get_bw(tb->get_ctx);

	toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);

	print_op = gtk_print_operation_new();
	if (print_op == NULL) {
		nsgtk_warning(messages_get("NoMemory"), 0);
		return TRUE;
	}

	/* use previously saved settings if any */
	netsurf_mkpath(&settings_fname, NULL, 2, nsgtk_config_home, "Print");
	if (settings_fname != NULL) {
		print_settings = gtk_print_settings_new_from_file(settings_fname, NULL);
		if (print_settings != NULL) {
			gtk_print_operation_set_print_settings(print_op,
							       print_settings);

			/* We're not interested in the settings any more */
			g_object_unref(print_settings);
		}
	}

	content_to_print = browser_window_get_content(bw);

	page_setup = gtk_print_run_page_setup_dialog(GTK_WINDOW(toplevel),
						     NULL,
						     NULL);
	if (page_setup == NULL) {
		nsgtk_warning(messages_get("NoMemory"), 0);
		free(settings_fname);
		g_object_unref(print_op);
		return TRUE;
	}
	gtk_print_operation_set_default_page_setup(print_op, page_setup);

	nssettings = print_make_settings(PRINT_DEFAULT,
					 NULL,
					 nsgtk_layout_table);

	g_signal_connect(print_op,
			 "begin_print",
			 G_CALLBACK(gtk_print_signal_begin_print),
			 nssettings);
	g_signal_connect(print_op,
			 "draw_page",
			 G_CALLBACK(gtk_print_signal_draw_page),
			 NULL);
	g_signal_connect(print_op,
			 "end_print",
			 G_CALLBACK(gtk_print_signal_end_print),
			 nssettings);

	if (content_get_type(browser_window_get_content(bw)) != CONTENT_TEXTPLAIN) {
		res = gtk_print_operation_run(print_op,
					      GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
					      GTK_WINDOW(toplevel),
					      NULL);
	}

	/* if the settings were used save them for future use */
	if (settings_fname != NULL) {
		if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
			/* Do not increment the settings reference */
			print_settings = gtk_print_operation_get_print_settings(print_op);

			gtk_print_settings_to_file(print_settings,
						   settings_fname,
						   NULL);
		}
		free(settings_fname);
	}

	/* Our print_settings object is destroyed by the end print handler */
	g_object_unref(page_setup);
	g_object_unref(print_op);

	return TRUE;
}

/**
 * handler for quit tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
quit_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	nsgtk_scaffolding_destroy_all();
	return TRUE;
}


/**
 * handler for cut tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
cut_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	GtkWidget *focused;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);

	focused = gtk_window_get_focus(GTK_WINDOW(toplevel));

	/* let gtk handle it if focused widget is an editable */
	if (GTK_IS_EDITABLE(focused)) {
		gtk_editable_cut_clipboard(GTK_EDITABLE(focused));
	} else {
		bw = tb->get_bw(tb->get_ctx);
		browser_window_key_press(bw, NS_KEY_CUT_SELECTION);
	}

	return TRUE;
}


/**
 * handler for copy tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
copy_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	GtkWidget *focused;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);

	focused = gtk_window_get_focus(GTK_WINDOW(toplevel));

	/* let gtk handle it if focused widget is an editable */
	if (GTK_IS_EDITABLE(focused)) {
		gtk_editable_copy_clipboard(GTK_EDITABLE(focused));
	} else {
		bw = tb->get_bw(tb->get_ctx);
		browser_window_key_press(bw, NS_KEY_COPY_SELECTION);
	}

	return TRUE;
}


/**
 * handler for paste tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
paste_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	GtkWidget *focused;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);

	focused = gtk_window_get_focus(GTK_WINDOW(toplevel));

	/* let gtk handle it if focused widget is an editable */
	if (GTK_IS_EDITABLE(focused)) {
		gtk_editable_paste_clipboard(GTK_EDITABLE(focused));
	} else {
		bw = tb->get_bw(tb->get_ctx);
		browser_window_key_press(bw, NS_KEY_PASTE);
	}

	return TRUE;
}


/**
 * handler for delete tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
delete_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	GtkWidget *focused;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);

	focused = gtk_window_get_focus(GTK_WINDOW(toplevel));

	/* let gtk handle it if focused widget is an editable */
	if (GTK_IS_EDITABLE(focused)) {
		gtk_editable_delete_selection(GTK_EDITABLE(focused));
	} else {
		bw = tb->get_bw(tb->get_ctx);
		browser_window_key_press(bw, NS_KEY_CLEAR_SELECTION);
	}

	return TRUE;
}


/**
 * handler for select all tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
selectall_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	GtkWidget *focused;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);

	focused = gtk_window_get_focus(GTK_WINDOW(toplevel));

	/* let gtk handle it if focused widget is an editable */
	if (GTK_IS_EDITABLE(focused)) {
		gtk_editable_select_region(GTK_EDITABLE(focused), 0, -1);
	} else {
		bw = tb->get_bw(tb->get_ctx);
		browser_window_key_press(bw, NS_KEY_SELECT_ALL);
	}

	return TRUE;
}


/**
 * handler for preferences tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
preferences_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	GtkWidget *toplevel;
	GtkWidget *wndpreferences;

	bw = tb->get_bw(tb->get_ctx);

	toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);

	wndpreferences = nsgtk_preferences(bw, GTK_WINDOW(toplevel));
	if (wndpreferences != NULL) {
		gtk_widget_show(wndpreferences);
	}

	return TRUE;
}


/**
 * handler for zoom plus tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
zoomplus_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;

	bw = tb->get_bw(tb->get_ctx);

	browser_window_set_scale(bw, 0.05, false);

	return TRUE;
}


/**
 * handler for zoom minus tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
zoomminus_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;

	bw = tb->get_bw(tb->get_ctx);

	browser_window_set_scale(bw, -0.05, false);

	return TRUE;

}


/**
 * handler for zoom normal tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
zoomnormal_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;

	bw = tb->get_bw(tb->get_ctx);

	browser_window_set_scale(bw, 1.0, true);

	return TRUE;
}


/**
 * handler for full screen tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
fullscreen_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	GtkWindow *gtkwindow; /* gtk window widget is in */
	GdkWindow *gdkwindow;
	GdkWindowState state;

	gtkwindow = GTK_WINDOW(gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW));
	gdkwindow = gtk_widget_get_window(GTK_WIDGET(gtkwindow));
	state = gdk_window_get_state(gdkwindow);

	if (state & GDK_WINDOW_STATE_FULLSCREEN) {
		gtk_window_unfullscreen(gtkwindow);
	} else {
		gtk_window_fullscreen(gtkwindow);
	}
	return TRUE;
}


/**
 * handler for view source tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
viewsource_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	nserror res;
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	GtkWindow *gtkwindow; /* gtk window widget is in */

	bw = tb->get_bw(tb->get_ctx);

	gtkwindow = GTK_WINDOW(gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW));

	res = nsgtk_viewsource(gtkwindow, bw);
	if (res != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(res), 0);
	}

	return TRUE;
}


/**
 * handler for show downloads tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
downloads_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	GtkWindow *gtkwindow; /* gtk window widget is in */
	gtkwindow = GTK_WINDOW(gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW));
	nsgtk_download_show(gtkwindow);
	return TRUE;
}


/**
 * handler for show downloads tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
savewindowsize_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	GtkWindow *gtkwindow; /* gtk window widget is in */
	int x,y,w,h;
	char *choices = NULL;

	gtkwindow = GTK_WINDOW(gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW));

	gtk_window_get_position(gtkwindow, &x, &y);
	gtk_window_get_size(gtkwindow, &w, &h);

	nsoption_set_int(window_width, w);
	nsoption_set_int(window_height, h);
	nsoption_set_int(window_x, x);
	nsoption_set_int(window_y, y);

	netsurf_mkpath(&choices, NULL, 2, nsgtk_config_home, "Choices");
	if (choices != NULL) {
		nsoption_write(choices, NULL, NULL);
		free(choices);
	}

	return TRUE;
}


/**
 * handler for show downloads tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
toggledebugging_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;

	bw = tb->get_bw(tb->get_ctx);

	browser_window_debug(bw, CONTENT_DEBUG_REDRAW);

	nsgtk_window_update_all();

	return TRUE;
}


/**
 * handler for debug box tree tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
debugboxtree_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	gchar *fname;
	gint handle;
	FILE *f;

	handle = g_file_open_tmp("nsgtkboxtreeXXXXXX", &fname, NULL);
	if ((handle == -1) || (fname == NULL)) {
		return TRUE;
	}
	close(handle); /* in case it was binary mode */

	/* save data to temporary file */
	f = fopen(fname, "w");
	if (f == NULL) {
		nsgtk_warning("Error saving box tree dump.",
			      "Unable to open file for writing.");
		unlink(fname);
		return TRUE;
	}

	bw = tb->get_bw(tb->get_ctx);

	browser_window_debug_dump(bw, f, CONTENT_DEBUG_RENDER);

	fclose(f);

	nsgtk_viewfile("Box Tree Debug", "boxtree", fname);

	g_free(fname);

	return TRUE;
}


/**
 * handler for debug dom tree tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
debugdomtree_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	gchar *fname;
	gint handle;
	FILE *f;

	handle = g_file_open_tmp("nsgtkdomtreeXXXXXX", &fname, NULL);
	if ((handle == -1) || (fname == NULL)) {
		return TRUE;
	}
	close(handle); /* in case it was binary mode */

	/* save data to temporary file */
	f = fopen(fname, "w");
	if (f == NULL) {
		nsgtk_warning("Error saving box tree dump.",
			      "Unable to open file for writing.");
		unlink(fname);
		return TRUE;
	}

	bw = tb->get_bw(tb->get_ctx);

	browser_window_debug_dump(bw, f, CONTENT_DEBUG_DOM);

	fclose(f);

	nsgtk_viewfile("DOM Tree Debug", "domtree", fname);

	g_free(fname);

	return TRUE;

}


/**
 * handler for local history tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
localhistory_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	nserror res;
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);
	if (toplevel != NULL) {
		bw = tb->get_bw(tb->get_ctx);

		res = nsgtk_local_history_present(GTK_WINDOW(toplevel), bw);
		if (res != NSERROR_OK) {
			NSLOG(netsurf, INFO,
			      "Unable to present local history window.");
		}
	}
	return TRUE;
}

/**
 * handler for history tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
history_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	return localhistory_button_clicked_cb(widget, data);
}


/**
 * handler for global history tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
globalhistory_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	nserror res;
	res = nsgtk_global_history_present();
	if (res != NSERROR_OK) {
		NSLOG(netsurf, INFO,
		      "Unable to initialise global history window.");
	}
	return TRUE;
}


/**
 * handler for add bookmark tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
addbookmarks_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct browser_window *bw;

	bw = tb->get_bw(tb->get_ctx);
	if (browser_window_has_content(bw)) {
		hotlist_add_url(browser_window_access_url(bw));
	}
	return TRUE;
}


/**
 * handler for show bookmark tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
showbookmarks_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	nserror res;
	res = nsgtk_hotlist_present();
	if (res != NSERROR_OK) {
		NSLOG(netsurf, INFO, "Unable to initialise bookmark window.");
	}
	return TRUE;
}


/**
 * handler for show cookies tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
showcookies_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	nserror res;
	res = nsgtk_cookies_present();
	if (res != NSERROR_OK) {
		NSLOG(netsurf, INFO, "Unable to initialise cookies window.");
	}
	return TRUE;
}


/**
 * handler for open location tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
openlocation_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	GtkToolItem *urltitem;

	urltitem = tb->buttons[URL_BAR_ITEM]->button;
	if (urltitem != NULL) {
		GtkEntry *entry;
		entry = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(urltitem)));
		gtk_widget_grab_focus(GTK_WIDGET(entry));
	}
	return TRUE;
}


/**
 * handler for contents tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
contents_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	nserror res;

	res = toolbar_navigate_to_url(tb, "http://www.netsurf-browser.org/documentation/");
	if (res != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(res), 0);
	}

	return TRUE;
}

/**
 * handler for contents tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
guide_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	nserror res;

	res = toolbar_navigate_to_url(tb, "http://www.netsurf-browser.org/documentation/guide");
	if (res != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(res), 0);
	}

	return TRUE;
}


/**
 * handler for contents tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean
info_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	nserror res;

	res = toolbar_navigate_to_url(tb, "http://www.netsurf-browser.org/documentation/info");
	if (res != NSERROR_OK) {
		nsgtk_warning(messages_get_errorcode(res), 0);
	}

	return TRUE;
}


/**
 * handler for contents tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE
 */
static gboolean about_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	GtkWindow *parent; /* gtk window widget is in */

	parent = GTK_WINDOW(gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW));

	nsgtk_about_dialog_init(parent);
	return TRUE;
}

/**
 * handler for openmenu tool bar item clicked signal
 *
 * \param widget The widget the signal is being delivered to.
 * \param data The toolbar context passed when the signal was connected
 * \return TRUE to indicate signal handled.
 */
static gboolean openmenu_button_clicked_cb(GtkWidget *widget, gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct gui_window *gw;
	struct nsgtk_scaffolding *gs;

	gw = tb->get_ctx; /** \todo stop assuming the context is a gui window */

	gs = nsgtk_get_scaffold(gw);

	nsgtk_scaffolding_burger_menu(gs);

	return TRUE;
}

/**
 * create a toolbar item
 *
 * create a toolbar item and set up its default handlers
 */
static nserror
toolbar_item_create(nsgtk_toolbar_button id,
		    struct nsgtk_toolbar_item **item_out)
{
	struct nsgtk_toolbar_item *item;
	item = calloc(1, sizeof(struct nsgtk_toolbar_item));
	if (item == NULL) {
		return NSERROR_NOMEM;
	}
	item->location = INACTIVE_LOCATION;

	/* set item defaults from macro */
	switch (id) {
#define TOOLBAR_ITEM_y(name)					\
		item->bhandler = name##_button_clicked_cb;
#define TOOLBAR_ITEM_n(name)			\
		item->bhandler = NULL;
#define TOOLBAR_ITEM(identifier, name, snstvty, clicked, activate)	\
	case identifier:						\
		item->sensitivity = snstvty;				\
		item->dataplus = nsgtk_toolbar_##name##_data_plus;	\
		item->dataminus = nsgtk_toolbar_##name##_data_minus;	\
		TOOLBAR_ITEM_ ## clicked(name)				\
		break;
#include "gtk/toolbar_items.h"
#undef TOOLBAR_ITEM_y
#undef TOOLBAR_ITEM_n
#undef TOOLBAR_ITEM

	case PLACEHOLDER_BUTTON:
		free(item);
		return NSERROR_INVALID;
	}

	*item_out = item;
	return NSERROR_OK;
}


/**
 * set a toolbar item to a throbber frame number
 *
 * \param toolbar_item The toolbar item to update
 * \param frame The animation frame number to update to
 * \return NSERROR_OK on success,
 *         NSERROR_INVALID if the toolbar item does not contain an image,
 *         NSERROR_BAD_SIZE if the frame is out of range.
 */
static nserror set_throbber_frame(GtkToolItem *toolbar_item, int frame)
{
	nserror res;
	GdkPixbuf *pixbuf;
	GtkImage *throbber;

	if (toolbar_item == NULL) {
		/* no toolbar item */
		return NSERROR_INVALID;
	}

	res = nsgtk_throbber_get_frame(frame, &pixbuf);
	if (res != NSERROR_OK) {
		return res;
	}

	throbber = GTK_IMAGE(gtk_bin_get_child(GTK_BIN(toolbar_item)));

	gtk_image_set_from_pixbuf(throbber, pixbuf);

	return NSERROR_OK;
}


/**
 * Make the throbber run.
 *
 * scheduled callback to update the throbber
 *
 * \param p The context passed when scheduled.
 */
static void next_throbber_frame(void *p)
{
	struct nsgtk_toolbar *tb = p;
	nserror res;

	tb->throb_frame++; /* advance to next frame */

	res = set_throbber_frame(tb->buttons[THROBBER_ITEM]->button,
				 tb->throb_frame);
	if (res == NSERROR_BAD_SIZE) {
		tb->throb_frame = 1;
		res = set_throbber_frame(tb->buttons[THROBBER_ITEM]->button,
					 tb->throb_frame);
	}

	/* only schedule next frame if there are no errors */
	if (res == NSERROR_OK) {
		nsgtk_schedule(THROBBER_FRAME_TIME, next_throbber_frame, p);
	}
}


/**
 * connect signal handlers to a gtk toolbar item
 */
static nserror
toolbar_connect_signal(struct nsgtk_toolbar *tb, nsgtk_toolbar_button itemid)
{
	struct nsgtk_toolbar_item *item;
	GtkEntry *entry;

	item = tb->buttons[itemid];

	if (item->button != NULL) {
		g_signal_connect(item->button,
				 "size-allocate",
				 G_CALLBACK(toolbar_item_size_allocate_cb),
				 tb);
	}

	switch (itemid) {
	case URL_BAR_ITEM:
		entry = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(item->button)));

		g_signal_connect(GTK_WIDGET(entry),
				 "activate",
				 G_CALLBACK(url_entry_activate_cb),
				 tb);
		g_signal_connect(GTK_WIDGET(entry),
				 "changed",
				 G_CALLBACK(url_entry_changed_cb),
				 tb);

		nsgtk_completion_connect_signals(entry,
						 tb->get_bw,
						 tb->get_ctx);
		break;


	case WEBSEARCH_ITEM:
		entry = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(item->button)));

		g_signal_connect(GTK_WIDGET(entry),
				 "activate",
				 G_CALLBACK(websearch_entry_activate_cb),
				 tb);
		g_signal_connect(GTK_WIDGET(entry),
				 "button-press-event",
				 G_CALLBACK(websearch_entry_button_press_cb),
				 tb);
		break;

	default:
		if ((item->bhandler != NULL) && (item->button != NULL)) {
			g_signal_connect(item->button,
					 "clicked",
					 G_CALLBACK(item->bhandler),
					 tb);
		}
		break;

	}

	return NSERROR_OK;
}

/**
 * connect all signals to widgets in a toolbar
 */
static nserror toolbar_connect_signals(struct nsgtk_toolbar *tb)
{
	int location; /* location index */
	nsgtk_toolbar_button itemid; /* item id */

	for (location = BACK_BUTTON; location < PLACEHOLDER_BUTTON; location++) {
		itemid = itemid_from_location(tb, location);
		if (itemid == PLACEHOLDER_BUTTON) {
			/* no more filled locations */
			break;
		}
		toolbar_connect_signal(tb, itemid);
	}

	return NSERROR_OK;
}

/**
 * signal handler for toolbar context menu
 *
 * \param toolbar The toolbar event is being delivered to
 * \param x The x coordinate where the click happened
 * \param y The x coordinate where the click happened
 * \param button the buttons being pressed
 * \param data The context pointer passed when the connection was made.
 * \return TRUE to indicate signal handled.
 */
static gboolean
toolbar_popup_context_menu_cb(GtkToolbar *toolbar,
			      gint x,
			      gint y,
			      gint button,
			      gpointer data)
{
	struct nsgtk_toolbar *tb = (struct nsgtk_toolbar *)data;
	struct gui_window *gw;
	struct nsgtk_scaffolding *gs;

	gw = tb->get_ctx; /** \todo stop assuming the context is a gui window */

	gs = nsgtk_get_scaffold(gw);

	nsgtk_scaffolding_toolbar_context_menu(gs);

	return TRUE;
}

/* exported interface documented in toolbar.h */
nserror
nsgtk_toolbar_create(GtkBuilder *builder,
		     struct browser_window *(*get_bw)(void *ctx),
		     void *get_ctx,
		     struct nsgtk_toolbar **tb_out)
{
	nserror res;
	struct nsgtk_toolbar *tb;
	int bidx; /* button index */

	tb = calloc(1, sizeof(struct nsgtk_toolbar));
	if (tb == NULL) {
		return NSERROR_NOMEM;
	}

	tb->get_bw = get_bw;
	tb->get_ctx = get_ctx;
	/* set the throbber start frame. */
	tb->throb_frame = 0;

	tb->widget = GTK_TOOLBAR(gtk_builder_get_object(builder, "toolbar"));
	gtk_toolbar_set_show_arrow(tb->widget, TRUE);

	g_signal_connect(tb->widget,
			 "popup-context-menu",
			 G_CALLBACK(toolbar_popup_context_menu_cb),
			 tb);

	/* allocate button contexts */
	for (bidx = BACK_BUTTON; bidx < PLACEHOLDER_BUTTON; bidx++) {
		res = toolbar_item_create(bidx, &tb->buttons[bidx]);
		if (res != NSERROR_OK) {
			for (bidx-- ; bidx >= BACK_BUTTON; bidx--) {
				free(tb->buttons[bidx]);
			}
			free(tb);
			return res;
		}
	}

	res = apply_user_button_customization(tb);
	if (res != NSERROR_OK) {
		free(tb);
		return res;
	}

	res = populate_gtk_toolbar_widget(tb);
	if (res != NSERROR_OK) {
		free(tb);
		return res;
	}

	res = nsgtk_toolbar_update(tb);
	if (res != NSERROR_OK) {
		free(tb);
		return res;
	}

	gtk_widget_show_all(GTK_WIDGET(tb->widget));

	/* if there is a history widget set its size */
	if (tb->buttons[HISTORY_BUTTON]->button != NULL) {
		gtk_widget_set_size_request(GTK_WIDGET(
			tb->buttons[HISTORY_BUTTON]->button), 20, -1);
	}

	res = toolbar_connect_signals(tb);
	if (res != NSERROR_OK) {
		free(tb);
		return res;
	}

	*tb_out = tb;
	return NSERROR_OK;
}


/* exported interface documented in toolbar.h */
nserror nsgtk_toolbar_destroy(struct nsgtk_toolbar *tb)
{
	/** \todo free buttons and destroy toolbar container (and widgets) */
	free(tb);
	return NSERROR_OK;
}

/* exported interface documented in toolbar.h */
nserror nsgtk_toolbar_update(struct nsgtk_toolbar *tb)
{
	/*
	 * reset toolbar size allocation so icon size change affects
	 *  allocated widths.
	 */
	tb->offset = 0;

	switch (nsoption_int(button_type)) {

	case 1: /* Small icons */
		gtk_toolbar_set_style(GTK_TOOLBAR(tb->widget),
				      GTK_TOOLBAR_ICONS);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(tb->widget),
					  GTK_ICON_SIZE_SMALL_TOOLBAR);
		break;

	case 2: /* Large icons */
		gtk_toolbar_set_style(GTK_TOOLBAR(tb->widget),
				      GTK_TOOLBAR_ICONS);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(tb->widget),
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
		break;

	case 3: /* Large icons with text */
		gtk_toolbar_set_style(GTK_TOOLBAR(tb->widget),
				      GTK_TOOLBAR_BOTH);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(tb->widget),
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
		break;

	case 4: /* Text icons only */
		gtk_toolbar_set_style(GTK_TOOLBAR(tb->widget),
				      GTK_TOOLBAR_TEXT);
		break;

	default:
		break;
	}

	return NSERROR_OK;
}


/* exported interface documented in toolbar.h */
nserror nsgtk_toolbar_throbber(struct nsgtk_toolbar *tb, bool active)
{
	nserror res;
	struct browser_window *bw;

	bw = tb->get_bw(tb->get_ctx);

	/* when activating the throbber simply schedule the next frame update */
	if (active) {
		nsgtk_schedule(THROBBER_FRAME_TIME, next_throbber_frame, tb);

		set_item_sensitivity(tb->buttons[STOP_BUTTON], true);
		set_item_sensitivity(tb->buttons[RELOAD_BUTTON], false);

		return NSERROR_OK;
	}

	/* stopping the throbber */
	nsgtk_schedule(-1, next_throbber_frame, tb);
	tb->throb_frame = 0;
	res =  set_throbber_frame(tb->buttons[THROBBER_ITEM]->button,
				  tb->throb_frame);

	/* adjust sensitivity of other items */
	set_item_sensitivity(tb->buttons[STOP_BUTTON], false);
	set_item_sensitivity(tb->buttons[RELOAD_BUTTON], true);
	set_item_sensitivity(tb->buttons[BACK_BUTTON],
			     browser_window_history_back_available(bw));
	set_item_sensitivity(tb->buttons[FORWARD_BUTTON],
			     browser_window_history_forward_available(bw));
	nsgtk_local_history_hide();

	return res;
}


/* exported interface documented in toolbar.h */
nserror nsgtk_toolbar_set_url(struct nsgtk_toolbar *tb, nsurl *url)
{
	size_t idn_url_l;
	char *idn_url_s = NULL;
	const char *url_text = NULL;
	GtkEntry *url_entry;

	if (tb->buttons[URL_BAR_ITEM]->button == NULL) {
		/* no toolbar item */
		return NSERROR_INVALID;
	}
	url_entry = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(tb->buttons[URL_BAR_ITEM]->button)));

	if (nsoption_bool(display_decoded_idn) == true) {
		if (nsurl_get_utf8(url, &idn_url_s, &idn_url_l) != NSERROR_OK) {
			idn_url_s = NULL;
		}
		url_text = idn_url_s;
	}
	if (url_text == NULL) {
		url_text = nsurl_access(url);
	}

	gtk_entry_set_text(url_entry, url_text);
	//gtk_editable_set_position(GTK_EDITABLE(url_entry), -1);

	if (idn_url_s != NULL) {
		free(idn_url_s);
	}

	return NSERROR_OK;
}


/* exported interface documented in toolbar.h */
nserror
nsgtk_toolbar_set_websearch_image(struct nsgtk_toolbar *tb, GdkPixbuf *pixbuf)
{
	GtkWidget *entry;

	if (tb->buttons[WEBSEARCH_ITEM]->button == NULL) {
		/* no toolbar item */
		return NSERROR_INVALID;
	}

	entry = gtk_bin_get_child(GTK_BIN(tb->buttons[WEBSEARCH_ITEM]->button));

	if (pixbuf != NULL) {
		nsgtk_entry_set_icon_from_pixbuf(entry,
						 GTK_ENTRY_ICON_PRIMARY,
						 pixbuf);
	} else {
		nsgtk_entry_set_icon_from_stock(entry,
						GTK_ENTRY_ICON_PRIMARY,
						NSGTK_STOCK_INFO);
	}

	return NSERROR_OK;
}


/* exported interface documented in toolbar.h */
nserror
nsgtk_toolbar_item_activate(struct nsgtk_toolbar *tb,
			    nsgtk_toolbar_button itemid)
{
	GtkWidget *widget;

	/* ensure item id in range */
	if ((itemid < BACK_BUTTON) || (itemid >= PLACEHOLDER_BUTTON)) {
		return NSERROR_BAD_PARAMETER;
	}

	if (tb->buttons[itemid]->bhandler == NULL) {
		return NSERROR_INVALID;
	}

	/*
	 * if item has a widget in the current toolbar use that as the
	 *   signal source otherwise use the toolbar widget itself.
	 */
	if (tb->buttons[itemid]->button != NULL) {
		widget = GTK_WIDGET(tb->buttons[itemid]->button);
	} else {
		widget = GTK_WIDGET(tb->widget);
	}

	tb->buttons[itemid]->bhandler(widget, tb);

	return NSERROR_OK;
}
