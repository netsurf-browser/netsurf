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

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils/config.h"
#include "utils/utils.h"

#include "gtk/compat.h"
#include "gtk/gui.h"
#include "gtk/scaffolding.h"
#include "gtk/theme.h"

enum image_sets {
	IMAGE_SET_MAIN_MENU = 0,
	IMAGE_SET_RCLICK_MENU,
	IMAGE_SET_POPUP_MENU,
	IMAGE_SET_BUTTONS,
	IMAGE_SET_COUNT
};

/**
 * sets the images for a particular scaffolding according to the current theme
 */

void nsgtk_theme_implement(struct nsgtk_scaffolding *g)
{
	struct nsgtk_theme *theme[IMAGE_SET_COUNT];
	int i;
	struct nsgtk_button_connect *button;
	struct gtk_search *search;

	theme[IMAGE_SET_MAIN_MENU] = nsgtk_theme_load(GTK_ICON_SIZE_MENU);
	theme[IMAGE_SET_RCLICK_MENU] = nsgtk_theme_load(GTK_ICON_SIZE_MENU);
	theme[IMAGE_SET_POPUP_MENU] = nsgtk_theme_load(GTK_ICON_SIZE_MENU);
	theme[IMAGE_SET_BUTTONS] = nsgtk_theme_load(GTK_ICON_SIZE_LARGE_TOOLBAR);

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
			nsgtk_image_menu_item_set_image(GTK_WIDGET(button->main),
						      GTK_WIDGET(
							      theme[IMAGE_SET_MAIN_MENU]->
							      image[i]));
			gtk_widget_show_all(GTK_WIDGET(button->main));
		}
		if ((button->rclick != NULL)  &&
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
 * get default image for buttons / menu items from gtk stock items.
 *
 * \param tbbutton button reference
 * \param iconsize The size of icons to select.
 * \return default images.
 */

static GtkImage *
nsgtk_theme_image_default(nsgtk_toolbar_button tbbutton, GtkIconSize iconsize)
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
		image = GTK_IMAGE(nsgtk_image_new_from_stock("gtk-missing-image",
							     iconsize));
		break;

	}
	return image;

}

/**
 * Get default image for search buttons / menu items from gtk stock items
 *
 * \param tbbutton search button reference
 * \param iconsize The size of icons to select.
 * \return default search image.
 */

static GtkImage *
nsgtk_theme_searchimage_default(nsgtk_search_buttons tbbutton,
		GtkIconSize iconsize)
{
	switch (tbbutton) {

	case (SEARCH_BACK_BUTTON):
		return GTK_IMAGE(nsgtk_image_new_from_stock(NSGTK_STOCK_GO_BACK,
							    iconsize));
	case (SEARCH_FORWARD_BUTTON):
		return GTK_IMAGE(nsgtk_image_new_from_stock(NSGTK_STOCK_GO_FORWARD,
							    iconsize));
	case (SEARCH_CLOSE_BUTTON):
		return GTK_IMAGE(nsgtk_image_new_from_stock(NSGTK_STOCK_CLOSE,
							    iconsize));
	default:
		return NULL;
	}
}

/**
 * loads the set of default images for the toolbar / menus
 */

struct nsgtk_theme *nsgtk_theme_load(GtkIconSize iconsize)
{
	struct nsgtk_theme *theme = malloc(sizeof(struct nsgtk_theme));
	int btnloop;

	if (theme == NULL) {
		warn_user("NoMemory", 0);
		return NULL;
	}

	for (btnloop = BACK_BUTTON; btnloop < PLACEHOLDER_BUTTON ; btnloop++) {
		theme->image[btnloop] = nsgtk_theme_image_default(btnloop, iconsize);
	}

	for (btnloop = SEARCH_BACK_BUTTON; btnloop < SEARCH_BUTTONS_COUNT; btnloop++) {
		theme->searchimage[btnloop] = nsgtk_theme_searchimage_default(btnloop, iconsize);
	}
	return theme;
}

