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
#include <sys/stat.h>
#include <unistd.h>
#include "content/content.h"
#include "content/content_type.h"
#include "content/hlcache.h"
#include "gtk/gtk_gui.h"
#include "gtk/gtk_scaffolding.h"
#include "gtk/gtk_menu.h"
#include "gtk/gtk_theme.h"
#include "gtk/gtk_window.h"
#include "gtk/options.h"
#include "gtk/dialogs/gtk_options.h"
#include "utils/container.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

enum image_sets {
	IMAGE_SET_MAIN_MENU = 0,
	IMAGE_SET_RCLICK_MENU,
	IMAGE_SET_POPUP_MENU,
	IMAGE_SET_BUTTONS,
	IMAGE_SET_COUNT
};

struct nsgtk_theme_cache {
	GdkPixbuf	*image[PLACEHOLDER_BUTTON];
	GdkPixbuf	*searchimage[SEARCH_BUTTONS_COUNT];
	/* apng throbber image */
};

static char *current_theme_name = NULL;
static struct nsgtk_theme_cache *theme_cache_menu = NULL;
static struct nsgtk_theme_cache *theme_cache_toolbar = NULL;

static struct nsgtk_theme *nsgtk_theme_default(GtkIconSize s);
static GtkImage *nsgtk_theme_image_default(nsgtk_toolbar_button i, 
		GtkIconSize s);
static bool nsgtk_theme_verify(const char *themename);
static void nsgtk_theme_cache_image(nsgtk_toolbar_button i,
		const char *filename, const char *path);
static void nsgtk_theme_cache_searchimage(nsgtk_search_buttons i, 
		const char *filename, const char *path);
							   
#ifdef WITH_THEME_INSTALL
static hlcache_handle *theme_install_content = NULL;

static void theme_install_callback(hlcache_handle *c, content_msg msg,
		union content_msg_data data, void *pw);
static bool theme_install_read(const char *data, unsigned long len);
#endif

/**
 * called during gui init phase to retrieve theme name from file then
 * implement
 */

void nsgtk_theme_init(void)
{
	size_t len;
	if (option_current_theme == 0)
		return;
	len = SLEN("themelist") + strlen(res_dir_location) + 1;
	char themefile[len];
	snprintf(themefile, len, "%s%s", res_dir_location, "themelist");
	nsgtk_scaffolding *list = scaf_list;
	nsgtk_theme_verify(NULL);
	FILE *fp = fopen(themefile, "r");
	if (fp == NULL)
		return;
	char buf[50];
	int row_count = 0;
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (buf[0] == '\0')
			continue;

		if (row_count++ == option_current_theme) {
			if (current_theme_name != NULL)
				free(current_theme_name);
			/* clear the '\n' ["\n\0"->"\0\0"] */
			buf[strlen(buf) - 1] = '\0';
			current_theme_name = strdup(buf);
			break;
		}
	}
	fclose(fp);
	
	while (list != NULL) {
		nsgtk_theme_implement(list);
		list = nsgtk_scaffolding_iterate(list);
	}
}

/**
 * return reference to static global current_theme_name; caller then has
 * responsibility for global reference
 */

char *nsgtk_theme_name(void)
{
	return current_theme_name;
}

/**
 * set static global current_theme_name from param; caller is responsible
 * for the integrity of the global reference
 */

void nsgtk_theme_set_name(char *name)
{
	current_theme_name = name;
}

/**
 * adds a theme name to the list of themes
 */

void nsgtk_theme_add(const char *themename)
{
	size_t len;
	GtkWidget *notification, *label;
	len = SLEN("themelist") + strlen(res_dir_location) + 1;
	char themefile[len];
	snprintf(themefile, len, "%s%s", res_dir_location, "themelist");
	/* conduct verification here; no adding duplicates to list */
	if (nsgtk_theme_verify(themename) == false) {
		warn_user(messages_get("gtkThemeDup"), 0);
		return;
	}
	FILE *fp = fopen(themefile, "a");
	if (fp == NULL) {
		warn_user(messages_get("gtkFileError"), themefile);
		return;
	}
	fprintf(fp, "%s\n", themename);
	fclose(fp);
	
	/* notification that theme was added successfully */
	notification = gtk_dialog_new_with_buttons(messages_get("gtkThemeAdd"),
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_OK,
			GTK_RESPONSE_NONE, NULL);
	if (notification == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return;
	}
	len = SLEN("\t\t\t\t\t\t") + strlen(messages_get("gtkThemeAdd")) + 1;
	char labelcontent[len];
	snprintf(labelcontent, len, "\t\t\t%s\t\t\t",
			messages_get("gtkThemeAdd"));
	label = gtk_label_new(labelcontent);
	if (label == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return;
	}
	g_signal_connect_swapped(notification, "response",
			G_CALLBACK(gtk_widget_destroy), notification);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(notification)->vbox), label);
	gtk_widget_show_all(notification);
	
	/* update combo */
	if (wndPreferences == NULL)
		return;
	nsgtk_options_combo_theme_add(themename);

}

/**
 * \param themename contains a name of theme to check whether it may 
 * properly be added to the list; alternatively NULL to check the integrity
 * of the list
 * \return true for themename may be added / every item in the list is
 * a valid directory
 */

bool nsgtk_theme_verify(const char *themename)
{	
	long filelength;
	FILE *fp;
	size_t val = SLEN("themelist") + strlen(res_dir_location) + 1;
	char buf[50];
	char themefile[val];
	snprintf(themefile, val, "%s%s", res_dir_location, "themelist");
	if (themename == NULL) {
		char *filecontent, *testfile;
		struct stat sta;
		fp = fopen(themefile, "r+");
		if (fp == NULL) {
			warn_user(messages_get("gtkFileError"), themefile);
			return true;
		}
		fseek(fp, 0L, SEEK_END);
		filelength = ftell(fp);
		filecontent = malloc(filelength +
				SLEN("gtk default theme\n") + SLEN("\n")
				+ 1);
		if (filecontent == NULL) {
			warn_user(messages_get("NoMemory"), 0);
			fclose(fp);
			return true;
		}
		strcpy(filecontent, "gtk default theme\n");
		fseek(fp, 0L, SEEK_SET);
		while (fgets(buf, sizeof(buf), fp) != NULL) {
		/* iterate list */
			buf[strlen(buf) - 1] = '\0';
			/* "\n\0" -> "\0\0" */
			testfile = malloc(strlen(res_dir_location) +
					SLEN("themes/") + strlen(buf) + 1);
			if (testfile == NULL) {
				warn_user(messages_get("NoMemory"), 0);
				free(filecontent);
				fclose(fp);
				return false;
			}
			sprintf(testfile, "%sthemes/%s", res_dir_location,
					buf);
			/* check every directory */
			if (access(testfile, R_OK) == 0) {
				stat(testfile, &sta);
				if (S_ISDIR(sta.st_mode)) {
					buf[strlen(buf)] = '\n';
					/* "\0\0" -> "\n\0" */
					strcat(filecontent, buf);
				}
			}
			free(testfile);
		}
		fclose(fp);
		fp = fopen(themefile, "w");
		if (fp == NULL) {
			warn_user(messages_get("gtkFileError"), themefile);
			free(filecontent);
			return true;
		}
		val = fwrite(filecontent, strlen(filecontent), 1, fp);
		if (val == 0)
			LOG(("empty write themelist"));
		fclose(fp);
		free(filecontent);
		return true;
	} else {
		fp = fopen(themefile, "r");
		if (fp == NULL) {
			warn_user(messages_get("gtkFileError"), themefile);
			return false;
		}
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			buf[strlen(buf) - 1] = '\0';
			/* "\n\0" -> "\0\0" */
			if (strcmp(buf, themename) == 0) {
				fclose(fp);
				return false;
			}
		}
		fclose(fp);
		return true;
	}
	
}

/**
 * sets the images for a particular scaffolding according to the current theme
 */

void nsgtk_theme_implement(struct gtk_scaffolding *g)
{
	struct nsgtk_theme *theme[IMAGE_SET_COUNT];
	int i;
	struct nsgtk_button_connect *button;
	struct gtk_search *search;

	for (i = 0; i <= IMAGE_SET_POPUP_MENU; i++)
		theme[i] = nsgtk_theme_load(GTK_ICON_SIZE_MENU);
	
	theme[IMAGE_SET_BUTTONS] =
			nsgtk_theme_load(GTK_ICON_SIZE_LARGE_TOOLBAR);

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
			gtk_image_menu_item_set_image(button->main,
					GTK_WIDGET(
					theme[IMAGE_SET_MAIN_MENU]->
					image[i]));
			gtk_widget_show_all(GTK_WIDGET(button->main));
		}
		if ((button->rclick != NULL)  && 
				(theme[IMAGE_SET_RCLICK_MENU] != NULL)) {
			gtk_image_menu_item_set_image(button->rclick,
					GTK_WIDGET(
					theme[IMAGE_SET_RCLICK_MENU]->
					image[i]));
			gtk_widget_show_all(GTK_WIDGET(button->rclick));
		}
		if ((button->popup != NULL) && 
				(theme[IMAGE_SET_POPUP_MENU] != NULL)) {
			gtk_image_menu_item_set_image(button->popup,
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
	for (i = 0; i < IMAGE_SET_COUNT; i++)
		if (theme[i] != NULL)
			free(theme[i]);
}

/**
 * creates a set of images to add to buttons / menus
 * loads images from cache, calling an update to the cache when necessary
 * \return a struct nsgtk_theme is an array of images
 */

struct nsgtk_theme *nsgtk_theme_load(GtkIconSize s)
{
	if (current_theme_name == NULL)
		return nsgtk_theme_default(s);

	struct nsgtk_theme *theme = malloc(sizeof(struct nsgtk_theme));
	if (theme == NULL)
		return theme;
	
	if ((theme_cache_menu == NULL) || (theme_cache_toolbar == NULL))
		nsgtk_theme_prepare();
	
	/* load theme from cache */
	struct nsgtk_theme_cache *cachetheme = (s == GTK_ICON_SIZE_MENU) ?
			theme_cache_menu : theme_cache_toolbar;
	if (cachetheme == NULL) {
		free(theme);
		return NULL;
	}

#define SET_BUTTON_IMAGE(p, q, r)\
	if (p->image[q##_BUTTON] != NULL)\
		r->image[q##_BUTTON] = GTK_IMAGE(gtk_image_new_from_pixbuf(\
				p->image[q##_BUTTON]));\
	else\
		r->image[q##_BUTTON] = nsgtk_theme_image_default(\
				q##_BUTTON, s);

	SET_BUTTON_IMAGE(cachetheme, BACK, theme)
	SET_BUTTON_IMAGE(cachetheme, HISTORY, theme)
	SET_BUTTON_IMAGE(cachetheme, FORWARD, theme)
	SET_BUTTON_IMAGE(cachetheme, STOP, theme)
	SET_BUTTON_IMAGE(cachetheme, RELOAD, theme)
	SET_BUTTON_IMAGE(cachetheme, HOME, theme)
	SET_BUTTON_IMAGE(cachetheme, NEWWINDOW, theme)
	SET_BUTTON_IMAGE(cachetheme, NEWTAB, theme)
	SET_BUTTON_IMAGE(cachetheme, OPENFILE, theme)
	SET_BUTTON_IMAGE(cachetheme, CLOSETAB, theme)
	SET_BUTTON_IMAGE(cachetheme, CLOSEWINDOW, theme)
	SET_BUTTON_IMAGE(cachetheme, SAVEPAGE, theme)
	SET_BUTTON_IMAGE(cachetheme, PRINTPREVIEW, theme)
	SET_BUTTON_IMAGE(cachetheme, PRINT, theme)
	SET_BUTTON_IMAGE(cachetheme, QUIT, theme)
	SET_BUTTON_IMAGE(cachetheme, CUT, theme)
	SET_BUTTON_IMAGE(cachetheme, COPY, theme)
	SET_BUTTON_IMAGE(cachetheme, PASTE, theme)
	SET_BUTTON_IMAGE(cachetheme, DELETE, theme)
	SET_BUTTON_IMAGE(cachetheme, SELECTALL, theme)
	SET_BUTTON_IMAGE(cachetheme, PREFERENCES, theme)
	SET_BUTTON_IMAGE(cachetheme, ZOOMPLUS, theme)
	SET_BUTTON_IMAGE(cachetheme, ZOOMMINUS, theme)
	SET_BUTTON_IMAGE(cachetheme, ZOOMNORMAL, theme)
	SET_BUTTON_IMAGE(cachetheme, FULLSCREEN, theme)
	SET_BUTTON_IMAGE(cachetheme, VIEWSOURCE, theme)
	SET_BUTTON_IMAGE(cachetheme, CONTENTS, theme)
	SET_BUTTON_IMAGE(cachetheme, ABOUT, theme)
	SET_BUTTON_IMAGE(cachetheme, PDF, theme)
	SET_BUTTON_IMAGE(cachetheme, PLAINTEXT, theme)
	SET_BUTTON_IMAGE(cachetheme, DRAWFILE, theme)
	SET_BUTTON_IMAGE(cachetheme, POSTSCRIPT, theme)
	SET_BUTTON_IMAGE(cachetheme, FIND, theme)
	SET_BUTTON_IMAGE(cachetheme, DOWNLOADS, theme)
	SET_BUTTON_IMAGE(cachetheme, SAVEWINDOWSIZE, theme)
	SET_BUTTON_IMAGE(cachetheme, TOGGLEDEBUGGING, theme)
	SET_BUTTON_IMAGE(cachetheme, SAVEBOXTREE, theme)
	SET_BUTTON_IMAGE(cachetheme, SAVEDOMTREE, theme)
	SET_BUTTON_IMAGE(cachetheme, LOCALHISTORY, theme)
	SET_BUTTON_IMAGE(cachetheme, GLOBALHISTORY, theme)
	SET_BUTTON_IMAGE(cachetheme, ADDBOOKMARKS, theme)
	SET_BUTTON_IMAGE(cachetheme, SHOWBOOKMARKS, theme)
	SET_BUTTON_IMAGE(cachetheme, OPENLOCATION, theme)
	SET_BUTTON_IMAGE(cachetheme, NEXTTAB, theme)
	SET_BUTTON_IMAGE(cachetheme, PREVTAB, theme)
	SET_BUTTON_IMAGE(cachetheme, GUIDE, theme)
	SET_BUTTON_IMAGE(cachetheme, INFO, theme)
#undef SET_BUTTON_IMAGE	
#define SET_BUTTON_IMAGE(p, q, qq, r)\
	if (qq->searchimage[SEARCH_##p##_BUTTON] != NULL)\
		r->searchimage[SEARCH_##p##_BUTTON] =\
				GTK_IMAGE(gtk_image_new_from_pixbuf(\
				qq->searchimage[\
				SEARCH_##p##_BUTTON]));\
	else if (qq->image[q##_BUTTON] != NULL)\
		r->searchimage[SEARCH_##p##_BUTTON] =\
				GTK_IMAGE(gtk_image_new_from_pixbuf(\
				qq->image[q##_BUTTON]));\
	else\
		r->searchimage[SEARCH_##p##_BUTTON] =\
				nsgtk_theme_image_default(\
				PLACEHOLDER_BUTTON + SEARCH_##p##_BUTTON, s);
	
	SET_BUTTON_IMAGE(BACK, BACK, cachetheme, theme)
	SET_BUTTON_IMAGE(FORWARD, FORWARD, cachetheme, theme)
	SET_BUTTON_IMAGE(CLOSE, CLOSEWINDOW, cachetheme, theme)
#undef SET_BUTTON_IMAGE
	return theme;
}

/**
 * caches individual theme images from file
 * \param i the toolbar button reference
 * \param filename the image file name
 * \param path the path to the theme folder
 */
void nsgtk_theme_cache_image(nsgtk_toolbar_button i, const char *filename,
		const char *path)
{
	char fullpath[strlen(filename) + strlen(path) + 1];
	sprintf(fullpath, "%s%s", path, filename);
	if (theme_cache_toolbar != NULL)
		theme_cache_toolbar->image[i] =
				gdk_pixbuf_new_from_file_at_size(fullpath,
				24, 24,	NULL);
	if (theme_cache_menu != NULL)
		theme_cache_menu->image[i] = gdk_pixbuf_new_from_file_at_size(
				fullpath, 16, 16, NULL);
}

void nsgtk_theme_cache_searchimage(nsgtk_search_buttons i,
		const char *filename, const char *path)
{
	char fullpath[strlen(filename) + strlen(path) + 1];
	sprintf(fullpath, "%s%s", path, filename);
	if (theme_cache_toolbar != NULL)
		theme_cache_toolbar->searchimage[i] =
				gdk_pixbuf_new_from_file_at_size(fullpath,
				24, 24,	NULL);
	if (theme_cache_menu != NULL)
		theme_cache_menu->searchimage[i] =
				gdk_pixbuf_new_from_file_at_size(fullpath,
				16, 16, NULL);
}

/**
 * caches theme images from file as pixbufs
 */
void nsgtk_theme_prepare(void)
{
	if (current_theme_name == NULL)
		return;
	if (theme_cache_menu == NULL)
		theme_cache_menu = malloc(sizeof(struct nsgtk_theme_cache));
	if (theme_cache_toolbar == NULL)
		theme_cache_toolbar = malloc(sizeof(struct nsgtk_theme_cache));
	size_t len = strlen(res_dir_location) + SLEN("/themes/") +
			strlen(current_theme_name) + 1;
	char path[len];
	snprintf(path, len, "%sthemes/%s/", res_dir_location, 
			current_theme_name);
#define CACHE_IMAGE(p, q, r)\
	nsgtk_theme_cache_image(p##_BUTTON, #q ".png", r)

	CACHE_IMAGE(BACK, back, path);
	CACHE_IMAGE(HISTORY, history, path);
	CACHE_IMAGE(FORWARD, forward, path);
	CACHE_IMAGE(STOP, stop, path);
	CACHE_IMAGE(RELOAD, reload, path);
	CACHE_IMAGE(HOME, home, path);
	CACHE_IMAGE(NEWWINDOW, newwindow, path);
	CACHE_IMAGE(NEWTAB, newtab, path);
	CACHE_IMAGE(OPENFILE, openfile, path);
	CACHE_IMAGE(CLOSETAB, closetab, path);
	CACHE_IMAGE(CLOSEWINDOW, closewindow, path);
	CACHE_IMAGE(SAVEPAGE, savepage, path);
	CACHE_IMAGE(PRINTPREVIEW, printpreview, path);
	CACHE_IMAGE(PRINT, print, path);
	CACHE_IMAGE(QUIT, quit, path);
	CACHE_IMAGE(CUT, cut, path);
	CACHE_IMAGE(COPY, copy, path);
	CACHE_IMAGE(PASTE, paste, path);
	CACHE_IMAGE(DELETE, delete, path);
	CACHE_IMAGE(SELECTALL, selectall, path);
	CACHE_IMAGE(PREFERENCES, preferences, path);
	CACHE_IMAGE(ZOOMPLUS, zoomplus, path);
	CACHE_IMAGE(ZOOMMINUS, zoomminus, path);
	CACHE_IMAGE(ZOOMNORMAL, zoomnormal, path);
	CACHE_IMAGE(FULLSCREEN, fullscreen, path);
	CACHE_IMAGE(VIEWSOURCE, viewsource, path);
	CACHE_IMAGE(CONTENTS, helpcontents, path);
	CACHE_IMAGE(ABOUT, helpabout, path);
	CACHE_IMAGE(PDF, pdf, path);
	CACHE_IMAGE(PLAINTEXT, plaintext, path);
	CACHE_IMAGE(DRAWFILE, drawfile, path);
	CACHE_IMAGE(POSTSCRIPT, postscript, path);
	CACHE_IMAGE(FIND, find, path);
	CACHE_IMAGE(DOWNLOADS, downloads, path);
	CACHE_IMAGE(SAVEWINDOWSIZE, savewindowsize, path);
	CACHE_IMAGE(TOGGLEDEBUGGING, toggledebugging, path);
	CACHE_IMAGE(SAVEBOXTREE, boxtree, path);
	CACHE_IMAGE(SAVEDOMTREE, domtree, path);
	CACHE_IMAGE(LOCALHISTORY, localhistory, path);
	CACHE_IMAGE(GLOBALHISTORY, globalhistory, path);
	CACHE_IMAGE(ADDBOOKMARKS, addbookmarks, path);
	CACHE_IMAGE(SHOWBOOKMARKS, showbookmarks, path);
	CACHE_IMAGE(OPENLOCATION, openlocation, path);
	CACHE_IMAGE(NEXTTAB, nexttab, path);
	CACHE_IMAGE(PREVTAB, prevtab, path);
	CACHE_IMAGE(GUIDE, helpguide, path);
	CACHE_IMAGE(INFO, helpinfo, path);
#undef CACHE_IMAGE
#define CACHE_IMAGE(p, q, r)\
	nsgtk_theme_cache_searchimage(p, #q ".png", r);
	
	CACHE_IMAGE(SEARCH_BACK_BUTTON, searchback, path);
	CACHE_IMAGE(SEARCH_FORWARD_BUTTON, searchforward, path);
	CACHE_IMAGE(SEARCH_CLOSE_BUTTON, searchclose, path);
#undef CACHE_IMAGE
}

/**
 * returns default image for buttons / menu items from gtk stock items
 * \param i button reference
 */

GtkImage *nsgtk_theme_image_default(nsgtk_toolbar_button i, GtkIconSize s)
{
	char *imagefile;
	GtkImage *image;
	switch(i) {
#define BUTTON_IMAGE(p, q)\
	case p##_BUTTON:\
		return GTK_IMAGE(gtk_image_new_from_stock(#q, s))

	BUTTON_IMAGE(BACK, gtk-go-back);
	case HISTORY_BUTTON: {
		size_t len = SLEN("arrow_down_8x32.png") +
				strlen(res_dir_location) + 1;
		imagefile = malloc(len);
		if (imagefile == NULL) {
			warn_user(messages_get("NoMemory"), 0);
			return NULL;
		}
		snprintf(imagefile, len, "%sarrow_down_8x32.png",
				res_dir_location);
		image = GTK_IMAGE(gtk_image_new_from_file(imagefile));
		free(imagefile);
		return image;
	}
	BUTTON_IMAGE(FORWARD, gtk-go-forward);
	BUTTON_IMAGE(STOP, gtk-stop);
	BUTTON_IMAGE(RELOAD, gtk-refresh);
	BUTTON_IMAGE(HOME, gtk-home);
	BUTTON_IMAGE(NEWWINDOW, gtk-new);
	BUTTON_IMAGE(NEWTAB, gtk-new);
	BUTTON_IMAGE(OPENFILE, gtk-open);
	BUTTON_IMAGE(CLOSETAB, gtk-close);
	BUTTON_IMAGE(CLOSEWINDOW, gtk-close);
	BUTTON_IMAGE(SAVEPAGE, gtk-save-as);
	BUTTON_IMAGE(PRINTPREVIEW, gtk-print-preview);
	BUTTON_IMAGE(PRINT, gtk-print);
	BUTTON_IMAGE(QUIT, gtk-quit);
	BUTTON_IMAGE(CUT, gtk-cut);
	BUTTON_IMAGE(COPY, gtk-copy);
	BUTTON_IMAGE(PASTE, gtk-paste);
	BUTTON_IMAGE(DELETE, gtk-delete);
	BUTTON_IMAGE(SELECTALL, gtk-select-all);
	BUTTON_IMAGE(FIND, gtk-find);
	BUTTON_IMAGE(PREFERENCES, gtk-preferences);
	BUTTON_IMAGE(ZOOMPLUS, gtk-zoom-in);
	BUTTON_IMAGE(ZOOMMINUS, gtk-zoom-out);
	BUTTON_IMAGE(ZOOMNORMAL, gtk-zoom-100);
	BUTTON_IMAGE(FULLSCREEN, gtk-fullscreen);
	BUTTON_IMAGE(VIEWSOURCE, gtk-index);
	BUTTON_IMAGE(CONTENTS, gtk-help);
	BUTTON_IMAGE(ABOUT, gtk-about);
#undef BUTTON_IMAGE
	case (PLACEHOLDER_BUTTON + SEARCH_BACK_BUTTON):
		return GTK_IMAGE(gtk_image_new_from_stock("gtk-go-back", s));
	case (PLACEHOLDER_BUTTON + SEARCH_FORWARD_BUTTON):
		return GTK_IMAGE(gtk_image_new_from_stock("gtk-go-forward", 
				s));
	case (PLACEHOLDER_BUTTON + SEARCH_CLOSE_BUTTON):
		return GTK_IMAGE(gtk_image_new_from_stock("gtk-close", s));
	default: {
		size_t len = SLEN("themes/Alpha.png") +
				strlen(res_dir_location) + 1;
		imagefile = malloc(len);
		if (imagefile == NULL) {
			warn_user(messages_get("NoMemory"), 0);
			return NULL;
		}
		snprintf(imagefile, len, "%sthemes/Alpha.png",
			  res_dir_location);
			  image = GTK_IMAGE(
					gtk_image_new_from_file(imagefile));
		free(imagefile);
		return image;
	}
	}	
}


#ifdef WITH_THEME_INSTALL
/**
 * when CONTENT_THEME needs handling call this function
 */
void theme_install_start(hlcache_handle *c)
{
	assert(c);
	assert(content_get_type(c) == CONTENT_THEME);

	/* stop theme sitting in memory cache */
	content_invalidate_reuse_data(c);
	if (!content_add_user(c, theme_install_callback, NULL)) {
		warn_user("NoMemory", 0);
		return;
	}
}


/**
 * Callback for fetchcache() for theme install fetches.
 */

void theme_install_callback(hlcache_handle *c, content_msg msg,
		union content_msg_data data, void *pw)
{
	switch (msg) {
	case CONTENT_MSG_READY:
		break;

	case CONTENT_MSG_DONE:
	{
		const char *source_data;
		unsigned long source_size;

		theme_install_content = c;

		source_data = content_get_source_data(c, &source_size);
		
		if (!theme_install_read(source_data, source_size))
			warn_user("ThemeInvalid", 0);
	}
		break;

	case CONTENT_MSG_ERROR:
		warn_user(data.error, 0);
		break;

	case CONTENT_MSG_STATUS:
		break;

	case CONTENT_MSG_LOADING:
	case CONTENT_MSG_REFORMAT:
	case CONTENT_MSG_REDRAW:
	default:
		assert(0);
		break;
	}
}

/**
 * handler saves theme data content as a local theme
 */

bool theme_install_read(const char *data, unsigned long len)
{
	char *filename, *newfilename;
	size_t namelen;
	int handle = g_file_open_tmp("nsgtkthemeXXXXXX", &filename, NULL);
	if (handle == -1) {
		warn_user(messages_get("gtkFileError"),
				"temporary theme file");
		return false;
	}
	ssize_t written = write(handle, data, len);
	close(handle);
	if ((unsigned)written != len)
		return false;
	
	/* get name of theme; set as dirname */
	namelen = SLEN("themes/") + strlen(res_dir_location) + 1;
	char dirname[namelen];
	snprintf(dirname, namelen, "%sthemes/", res_dir_location);

	/* save individual files in theme */
	newfilename = container_extract_theme(filename, dirname);
	g_free(filename);
	if (newfilename == NULL)
		return false;
	nsgtk_theme_add(newfilename);
	free(newfilename);
		
	return true;
}
#endif

/**
 * loads the set of default images for the toolbar / menus
 */

struct nsgtk_theme *nsgtk_theme_default(GtkIconSize s)
{
	struct nsgtk_theme *theme = malloc(sizeof(struct nsgtk_theme));
	if (theme == NULL) {
		warn_user("NoMemory", 0);
		return NULL;
	}
	for (int i = BACK_BUTTON; i < PLACEHOLDER_BUTTON + 
			SEARCH_BUTTONS_COUNT; i++)
		theme->image[i] = nsgtk_theme_image_default(i, s);
	return theme;
}

