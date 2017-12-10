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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>

#include "netsurf/browser_window.h"
#include "desktop/searchweb.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsoption.h"
#include "utils/file.h"

#include "gtk/gui.h"
#include "gtk/warn.h"
#include "gtk/search.h"
#include "gtk/throbber.h"
#include "gtk/scaffolding.h"
#include "gtk/window.h"
#include "gtk/compat.h"
#include "gtk/resources.h"
#include "gtk/toolbar.h"

static GtkTargetEntry entry = {(char *)"nsgtk_button_data",
		GTK_TARGET_SAME_APP, 0};

static bool edit_mode = false;

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
	/* apng throbber element */
};

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

#define BUTTON_IMAGE(p, q)					\
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



/* exported function documented in gtk/toolbar.h */
void nsgtk_theme_implement(struct nsgtk_scaffolding *g)
{
	struct nsgtk_theme *theme[IMAGE_SET_COUNT];
	int i;
	struct nsgtk_button_connect *button;
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
		if ((button->location != -1) && (button->button	!= NULL) &&
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
	struct nsgtk_button_connect *bc;

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
 * widget factory for creation of toolbar item widgets
 * \param g the reference scaffolding
 * \param i the id of the widget
 * \param theme the theme to make the widgets from
 */
static GtkWidget *
nsgtk_toolbar_make_widget(struct nsgtk_scaffolding *g,
			  nsgtk_toolbar_button i,
			  struct nsgtk_theme *theme)
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

	case URL_BAR_ITEM: {
		GtkWidget *entry = nsgtk_entry_new();
		w = GTK_WIDGET(gtk_tool_item_new());

		if ((entry == NULL) || (w == NULL)) {
			nsgtk_warning(messages_get("NoMemory"), 0);
			return NULL;
		}

		nsgtk_entry_set_icon_from_pixbuf(entry,
						 GTK_ENTRY_ICON_PRIMARY,
						 favicon_pixbuf);

		gtk_container_add(GTK_CONTAINER(w), entry);
		gtk_tool_item_set_expand(GTK_TOOL_ITEM(w), TRUE);
		break;
	}

	case THROBBER_ITEM: {
		if ((nsgtk_throbber == NULL) ||
		    (nsgtk_throbber->framedata == NULL) ||
		    (nsgtk_throbber->framedata[0] == NULL)) {
			return NULL;
		}

		if (edit_mode) {
			w = GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(
					gtk_image_new_from_pixbuf(
					nsgtk_throbber->framedata[0])),
							   "[throbber]"));
		} else {
			GtkWidget *image;

			w = GTK_WIDGET(gtk_tool_item_new());

			image = gtk_image_new_from_pixbuf(nsgtk_throbber->framedata[0]);
			if (image != NULL) {
				nsgtk_widget_set_alignment(image,
							   GTK_ALIGN_CENTER,
							   GTK_ALIGN_CENTER);
				nsgtk_widget_set_margins(image, 3, 0);

				gtk_container_add(GTK_CONTAINER(w), image);
			}
		}
		break;
	}

	case WEBSEARCH_ITEM: {
		if (edit_mode)
			return GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(
					nsgtk_image_new_from_stock(NSGTK_STOCK_FIND,
					GTK_ICON_SIZE_LARGE_TOOLBAR)),
					"[websearch]"));

		GtkWidget *entry = nsgtk_entry_new();

		w = GTK_WIDGET(gtk_tool_item_new());

		if ((entry == NULL) || (w == NULL)) {
			nsgtk_warning(messages_get("NoMemory"), 0);
			return NULL;
		}

		gtk_widget_set_size_request(entry, NSGTK_WEBSEARCH_WIDTH, -1);

		nsgtk_entry_set_icon_from_stock(entry, GTK_ENTRY_ICON_PRIMARY,
						NSGTK_STOCK_INFO);

		gtk_container_add(GTK_CONTAINER(w), entry);
		break;
	}

/* gtk_tool_button_new accepts NULL args */
#define MAKE_MENUBUTTON(p, q)						\
		case p##_BUTTON: {					\
			char *label = NULL;				\
			label = remove_underscores(messages_get(#q), false); \
			w = GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(	\
					   theme->image[p##_BUTTON]), label)); \
			if (label != NULL)				\
				free(label);				\
			break;						\
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
#undef MAKE_MENUBUTTON

	default:
		break;

	}
	return w;
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
			GTK_TOOL_ITEM(nsgtk_toolbar_make_widget(g,
			window->currentbutton, theme));
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
					nsgtk_toolbar_make_widget(g, q,
					theme));
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
		nsgtk_widget_override_background_color(GTK_WIDGET(nsgtk_window_get_layout(nsgtk_scaffolding_top_level(list))), GTK_STATE_NORMAL, 0, 0xFFFF, 0xFFFF, 0xFFFF);
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
 * load toolbar settings from file; file is a set of fields arranged as
 * [itemreference];[itemlocation]|[itemreference];[itemlocation]| etc
 */
void nsgtk_toolbar_customization_load(struct nsgtk_scaffolding *g)
{
	int i, ii;
	char *buffer;
	char *buffer1, *subbuffer, *ptr = NULL, *pter = NULL;

	/* default toolbar button order */
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		nsgtk_scaffolding_button(g, i)->location =
		(i <= THROBBER_ITEM) ? i : -1;
	}

	/* ensure the option is actually set */
	if (nsoption_charp(toolbar_order) == NULL) {
		return;
	}
	buffer = strdup(nsoption_charp(toolbar_order));

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
					nsgtk_scaffolding_button(g, i)->location = ii;
				}
			}
		}
		buffer1 = strtok_r(NULL, "|", &ptr);
	}

	free(buffer);
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
			nsgtk_toolbar_make_widget(g, i, theme);
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
 * set toolbar logical -> physical; physically visible toolbar buttons are made
 * to correspond to the logically stored schema in terms of location
 * visibility etc
 */
void nsgtk_toolbar_set_physical(struct nsgtk_scaffolding *g)
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
 * add handlers to factory widgets
 * \param g the scaffolding to attach handlers to
 * \param i the toolbar item id
 */
static void
nsgtk_toolbar_set_handler(struct nsgtk_scaffolding *g, nsgtk_toolbar_button i)
{
	switch(i){
	case URL_BAR_ITEM:
		nsgtk_scaffolding_update_url_bar_ref(g);
		g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_urlbar(g)),
				 "activate", G_CALLBACK(
					 nsgtk_window_url_activate_event), g);
		g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_urlbar(g)),
				 "changed", G_CALLBACK(
					 nsgtk_window_url_changed), g);
		break;

	case THROBBER_ITEM:
		nsgtk_scaffolding_update_throbber_ref(g);
		break;

	case WEBSEARCH_ITEM:
		nsgtk_scaffolding_update_websearch_ref(g);
		g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_websearch(g)),
				 "activate", G_CALLBACK(
					 nsgtk_websearch_activate), g);
		g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_websearch(g)),
				 "button-press-event", G_CALLBACK(
					 nsgtk_websearch_clear), g);
		break;

	default:
		if ((nsgtk_scaffolding_button(g, i)->bhandler != NULL) &&
		    (nsgtk_scaffolding_button(g, i)->button != NULL)) {
			g_signal_connect(
				nsgtk_scaffolding_button(g, i)->button,
				"clicked",
				G_CALLBACK(nsgtk_scaffolding_button(
						   g, i)->bhandler), g);
		}
		break;
	}
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
		if (nsgtk_scaffolding_button(g, q)->button != NULL)
			g_signal_connect(
					nsgtk_scaffolding_button(g, q)->button,
					"size-allocate", G_CALLBACK(
					nsgtk_scaffolding_toolbar_size_allocate
					), g);
		nsgtk_toolbar_set_handler(g, q);
	}
}


#define DATAHANDLER(p, q, r)\
gboolean nsgtk_toolbar_##p##_button_data(GtkWidget *widget, GdkDragContext\
		*cont, GtkSelectionData	*selection, guint info, guint time,\
		gpointer data)\
{\
	r->currentbutton = q##_BUTTON;\
	r->fromstore = true;\
	return TRUE;\
}\
gboolean nsgtk_toolbar_##p##_toolbar_button_data(GtkWidget *widget,\
		GdkDragContext *cont, GtkSelectionData *selection, guint info,\
		guint time, gpointer data)\
{\
	r->currentbutton = q##_BUTTON;\
	r->fromstore = false;\
	return TRUE;\
}

DATAHANDLER(home, HOME, window)
DATAHANDLER(forward, FORWARD, window)
DATAHANDLER(back, BACK, window)
DATAHANDLER(stop, STOP, window)
DATAHANDLER(reload, RELOAD, window)
DATAHANDLER(history, HISTORY, window)
DATAHANDLER(newwindow, NEWWINDOW, window)
DATAHANDLER(newtab, NEWTAB, window)
DATAHANDLER(openfile, OPENFILE, window)
DATAHANDLER(closetab, CLOSETAB, window)
DATAHANDLER(closewindow, CLOSEWINDOW, window)
DATAHANDLER(savepage, SAVEPAGE, window)
DATAHANDLER(printpreview, PRINTPREVIEW, window)
DATAHANDLER(print, PRINT, window)
DATAHANDLER(quit, QUIT, window)
DATAHANDLER(cut, CUT, window)
DATAHANDLER(copy, COPY, window)
DATAHANDLER(paste, PASTE, window)
DATAHANDLER(delete, DELETE, window)
DATAHANDLER(selectall, SELECTALL, window)
DATAHANDLER(preferences, PREFERENCES, window)
DATAHANDLER(zoomplus, ZOOMPLUS, window)
DATAHANDLER(zoomminus, ZOOMMINUS, window)
DATAHANDLER(zoomnormal, ZOOMNORMAL, window)
DATAHANDLER(fullscreen, FULLSCREEN, window)
DATAHANDLER(viewsource, VIEWSOURCE, window)
DATAHANDLER(contents, CONTENTS, window)
DATAHANDLER(about, ABOUT, window)
DATAHANDLER(pdf, PDF, window)
DATAHANDLER(plaintext, PLAINTEXT, window)
DATAHANDLER(drawfile, DRAWFILE, window)
DATAHANDLER(postscript, POSTSCRIPT, window)
DATAHANDLER(find, FIND, window)
DATAHANDLER(downloads, DOWNLOADS, window)
DATAHANDLER(savewindowsize, SAVEWINDOWSIZE, window)
DATAHANDLER(toggledebugging, TOGGLEDEBUGGING, window)
DATAHANDLER(debugboxtree, SAVEBOXTREE, window)
DATAHANDLER(debugdomtree, SAVEDOMTREE, window)
DATAHANDLER(localhistory, LOCALHISTORY, window)
DATAHANDLER(globalhistory, GLOBALHISTORY, window)
DATAHANDLER(addbookmarks, ADDBOOKMARKS, window)
DATAHANDLER(showbookmarks, SHOWBOOKMARKS, window)
DATAHANDLER(showcookies, SHOWCOOKIES, window)
DATAHANDLER(openlocation, OPENLOCATION, window)
DATAHANDLER(nexttab, NEXTTAB, window)
DATAHANDLER(prevtab, PREVTAB, window)
DATAHANDLER(guide, GUIDE, window)
DATAHANDLER(info, INFO, window)
#undef DATAHANDLER

#define DATAHANDLER(p, q, r)						\
gboolean nsgtk_toolbar_##p##_button_data(GtkWidget *widget, GdkDragContext\
		*cont, GtkSelectionData	*selection, guint info, guint time,\
		gpointer data)\
{\
	r->currentbutton = q##_ITEM;\
	r->fromstore = true;\
	return TRUE;\
}\
gboolean nsgtk_toolbar_##p##_toolbar_button_data(GtkWidget *widget,\
		GdkDragContext *cont, GtkSelectionData *selection, guint info,\
		guint time, gpointer data)\
{\
	r->currentbutton = q##_ITEM;\
	r->fromstore = false;\
	return TRUE;\
}

DATAHANDLER(throbber, THROBBER, window)
DATAHANDLER(websearch, WEBSEARCH, window)
#undef DATAHANDLER
