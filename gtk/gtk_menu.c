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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade-xml.h>

#include "gtk/gtk_menu.h"
#include "utils/messages.h"
#include "utils/utils.h"

/**
 * adds image menu item to specified menu
 * \param menu the menu to add the item to
 * \param item a pointer to the item's location in the menu struct
 * \param message the menu item I18n lookup value
 * \param messageAccel the menu item accelerator I18n lookup value
 * \param group the 'global' in a gtk sense accelerator group
 */

static bool nsgtk_menu_add_image_item(GtkMenu *menu,
		GtkImageMenuItem **item, const char *message,
		const char *messageAccel, GtkAccelGroup *group)
{
	unsigned int key;
	GdkModifierType mod;
	*item = GTK_IMAGE_MENU_ITEM(gtk_image_menu_item_new_with_mnemonic(
			messages_get(message)));
	if (*item == NULL)
		return false;
	gtk_accelerator_parse(messages_get(messageAccel), &key, &mod);
	if (key > 0)
		gtk_widget_add_accelerator(GTK_WIDGET(*item), "activate",
				group, key, mod, GTK_ACCEL_VISIBLE);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), GTK_WIDGET(*item));
	gtk_widget_show(GTK_WIDGET(*item));
	return true;
}

#define IMAGE_ITEM(p, q, r, s, t)\
	nsgtk_menu_add_image_item(s->p##_menu, &(s->q##_menuitem), #r,\
			#r "Accel", t)

#define CHECK_ITEM(p, q, r, s)\
	s->q##_menuitem = GTK_CHECK_MENU_ITEM(\
			gtk_check_menu_item_new_with_mnemonic(\
			messages_get(#r)));\
	if ((s->q##_menuitem != NULL) && (s->p##_menu != NULL)) {\
		gtk_menu_shell_append(GTK_MENU_SHELL(s->p##_menu),\
				GTK_WIDGET(s->q##_menuitem));\
		gtk_widget_show(GTK_WIDGET(s->q##_menuitem));\
	}
	
#define SET_SUBMENU(q, r)					\
	do {							\
		r->q##_submenu = nsgtk_menu_##q##_submenu(group);	\
		if ((r->q##_submenu != NULL) &&				\
		    (r->q##_submenu->q##_menu != NULL) &&		\
		    (r->q##_menuitem != NULL)) {			\
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(r->q##_menuitem), \
						  GTK_WIDGET(r->q##_submenu->q##_menu)); \
		}							\
	} while(0) 

#define ADD_SEP(q, r)							\
	do {								\
		GtkWidget *w = gtk_separator_menu_item_new();		\
		if ((w != NULL) && (r->q##_menu != NULL)) {		\
			gtk_menu_shell_append(GTK_MENU_SHELL(r->q##_menu), w); \
			gtk_widget_show(w);				\
		}							\
	} while(0) 

#define ATTACH_PARENT(parent, msgname, menuv, group)			\
	do {								\
		if (parent != NULL) {					\
			/* create top level menu entry and attach to parent */ \
			menuv = GTK_MENU_ITEM(gtk_menu_item_new_with_mnemonic(messages_get(#msgname))); \
			gtk_menu_shell_append(parent, GTK_WIDGET(menuv)); \
			gtk_widget_show(GTK_WIDGET(menuv));		\
			/* attach submenu to parent */			\
			gtk_menu_item_set_submenu(menuv, GTK_WIDGET(menuv##_menu)); \
			gtk_menu_set_accel_group(menuv##_menu, group);	\
		}							\
	} while(0) 

/** 
* creates an export submenu
* \param group the 'global' in a gtk sense accelerator reference
*/

static struct nsgtk_export_submenu *nsgtk_menu_export_submenu(GtkAccelGroup *group)
{
	struct nsgtk_export_submenu *ret = malloc(sizeof(struct
			nsgtk_export_submenu));
	if (ret == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}
	ret->export_menu = GTK_MENU(gtk_menu_new());
	if (ret->export_menu == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(ret);
		return NULL;
	}
	IMAGE_ITEM(export, plaintext, gtkPlainText, ret, group);
	IMAGE_ITEM(export, drawfile, gtkDrawFile, ret, group);
	IMAGE_ITEM(export, postscript, gtkPostScript, ret, group);
	IMAGE_ITEM(export, pdf, gtkPDF, ret, group);
	return ret;
}

/** 
* creates a scaleview submenu
* \param group the 'global' in a gtk sense accelerator reference
*/

static struct nsgtk_scaleview_submenu *nsgtk_menu_scaleview_submenu(
		GtkAccelGroup *group)
{
	struct nsgtk_scaleview_submenu *ret = 
			malloc(sizeof(struct nsgtk_scaleview_submenu));
	if (ret == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}
	ret->scaleview_menu = GTK_MENU(gtk_menu_new());
	if (ret->scaleview_menu == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(ret);
		return NULL;
	}
	IMAGE_ITEM(scaleview, zoomplus, gtkZoomPlus, ret, group);
	IMAGE_ITEM(scaleview, zoomnormal, gtkZoomNormal, ret, group);
	IMAGE_ITEM(scaleview, zoomminus, gtkZoomMinus, ret, group);
	return ret;
}

/** 
* creates a tab navigation submenu
* \param group the 'global' in a gtk sense accelerator reference
*/

static struct nsgtk_tabs_submenu *nsgtk_menu_tabs_submenu(GtkAccelGroup *group)
{
	struct nsgtk_tabs_submenu *ret = malloc(sizeof(struct nsgtk_tabs_submenu));
	if (ret == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}
	ret->tabs_menu = GTK_MENU(gtk_menu_new());
	if (ret->tabs_menu == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(ret);
		return NULL;
	}
	IMAGE_ITEM(tabs, nexttab, gtkNextTab, ret, group);
	IMAGE_ITEM(tabs, prevtab, gtkPrevTab, ret, group);
	IMAGE_ITEM(tabs, closetab, gtkCloseTab, ret, group);

	return ret;
}

/** 
* creates an images submenu
* \param group the 'global' in a gtk sense accelerator reference
*/

static struct nsgtk_images_submenu *nsgtk_menu_images_submenu(GtkAccelGroup *group)
{
	struct nsgtk_images_submenu *ret =
			malloc(sizeof(struct nsgtk_images_submenu));
	if (ret == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}
	ret->images_menu = GTK_MENU(gtk_menu_new());
	if (ret->images_menu == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(ret);
		return NULL;
	}
	CHECK_ITEM(images, foregroundimages, gtkForegroundImages, ret)
	CHECK_ITEM(images, backgroundimages, gtkBackgroundImages, ret)
	return ret;
}

/** 
* creates a toolbars submenu
* \param group the 'global' in a gtk sense accelerator reference
*/

static struct nsgtk_toolbars_submenu *nsgtk_menu_toolbars_submenu(
		GtkAccelGroup *group)
{
	struct nsgtk_toolbars_submenu *ret =
			malloc(sizeof(struct nsgtk_toolbars_submenu));
	if (ret == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}
	ret->toolbars_menu = GTK_MENU(gtk_menu_new());
	if (ret->toolbars_menu == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(ret);
		return NULL;
	}
	CHECK_ITEM(toolbars, menubar, gtkMenuBar, ret)
	if (ret->menubar_menuitem != NULL)
		gtk_check_menu_item_set_active(ret->menubar_menuitem, TRUE);
	CHECK_ITEM(toolbars, toolbar, gtkToolBar, ret)
	if (ret->toolbar_menuitem != NULL)	
		gtk_check_menu_item_set_active(ret->toolbar_menuitem, TRUE);
	return ret;
}

/** 
* creates a debugging submenu
* \param group the 'global' in a gtk sense accelerator reference
*/

static struct nsgtk_debugging_submenu *nsgtk_menu_debugging_submenu(
		GtkAccelGroup *group)
{
	struct nsgtk_debugging_submenu *ret =
			malloc(sizeof(struct nsgtk_debugging_submenu));
	if (ret == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}
	ret->debugging_menu = GTK_MENU(gtk_menu_new());
	if (ret->debugging_menu == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(ret);
		return NULL;
	}
	IMAGE_ITEM(debugging, toggledebugging, gtkToggleDebugging, ret, group);
	IMAGE_ITEM(debugging, saveboxtree, gtkSaveBoxTree, ret, group);
	IMAGE_ITEM(debugging, savedomtree, gtkSaveDomTree, ret, group);
	return ret;
}
	
/** 
 * creates the file menu
 * \param group The gtk 'global' accelerator reference
 * \param parent The parent menu to attach to or NULL
 */
static struct nsgtk_file_menu *nsgtk_menu_file_menu(GtkAccelGroup *group, 
						    GtkMenuShell *parent)
{
	struct nsgtk_file_menu *fmenu;

	fmenu = malloc(sizeof(struct nsgtk_file_menu));
	if (fmenu == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}

	fmenu->file_menu = GTK_MENU(gtk_menu_new());
	if (fmenu->file_menu == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(fmenu);
		return NULL;
	}

	IMAGE_ITEM(file, newwindow, gtkNewWindow, fmenu, group);
	IMAGE_ITEM(file, newtab, gtkNewTab, fmenu, group);
	IMAGE_ITEM(file, openfile, gtkOpenFile, fmenu, group);
	IMAGE_ITEM(file, closewindow, gtkCloseWindow, fmenu, group);
	ADD_SEP(file, fmenu);
	IMAGE_ITEM(file, savepage, gtkSavePage, fmenu, group);
	IMAGE_ITEM(file, export, gtkExport, fmenu, group);
	ADD_SEP(file, fmenu);
	IMAGE_ITEM(file, printpreview, gtkPrintPreview, fmenu, group);
	IMAGE_ITEM(file, print, gtkPrint, fmenu, group);
	ADD_SEP(file, fmenu);
	IMAGE_ITEM(file, quit, gtkQuitMenu, fmenu, group);
	SET_SUBMENU(export, fmenu);

	ATTACH_PARENT(parent, gtkFile, fmenu->file, group);

	return fmenu;
}

/** 
* creates an edit menu
* \param group the 'global' in a gtk sense accelerator reference
*/

static struct nsgtk_edit_menu *nsgtk_menu_edit_menu(GtkAccelGroup *group, 
						    GtkMenuShell *parent)
{
	struct nsgtk_edit_menu *ret = malloc(sizeof(struct nsgtk_edit_menu));
	if (ret == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}
	ret->edit_menu = GTK_MENU(gtk_menu_new());
	if (ret->edit_menu == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(ret);
		return NULL;
	}
	IMAGE_ITEM(edit, cut, gtkCut, ret, group);
	IMAGE_ITEM(edit, copy, gtkCopy, ret, group);
	IMAGE_ITEM(edit, paste, gtkPaste, ret, group);
	IMAGE_ITEM(edit, delete, gtkDelete, ret, group);
	ADD_SEP(edit, ret);
	IMAGE_ITEM(edit, selectall, gtkSelectAll, ret, group);
	ADD_SEP(edit, ret);
	IMAGE_ITEM(edit, find, gtkFind, ret, group);
	ADD_SEP(edit, ret);
	IMAGE_ITEM(edit, preferences, gtkPreferences, ret, group);

	ATTACH_PARENT(parent, gtkEdit, ret->edit, group);

	return ret;
}

/** 
* creates a view menu
* \param group the 'global' in a gtk sense accelerator reference
*/

static struct nsgtk_view_menu *nsgtk_menu_view_menu(GtkAccelGroup *group, 
						    GtkMenuShell *parent)
{
	struct nsgtk_view_menu *ret = malloc(sizeof(struct nsgtk_view_menu));
	if (ret == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}
	ret->view_menu = GTK_MENU(gtk_menu_new());
	if (ret->view_menu == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(ret);
		return NULL;
	}
	IMAGE_ITEM(view, stop, gtkStop, ret, group);
	IMAGE_ITEM(view, reload, gtkReload, ret, group);
	ADD_SEP(view, ret);
	IMAGE_ITEM(view, scaleview, gtkScaleView, ret, group);
	IMAGE_ITEM(view, fullscreen, gtkFullScreen, ret, group);
	IMAGE_ITEM(view, viewsource, gtkViewSource, ret, group);
	ADD_SEP(view, ret);
	IMAGE_ITEM(view, images, gtkImages, ret, group);
	IMAGE_ITEM(view, toolbars, gtkToolbars, ret, group);
	IMAGE_ITEM(view, tabs, gtkTabs, ret, group);
	ADD_SEP(view, ret);
	IMAGE_ITEM(view, downloads, gtkDownloads, ret, group);
	IMAGE_ITEM(view, savewindowsize, gtkSaveWindowSize, ret, group);
	IMAGE_ITEM(view, debugging, gtkDebugging, ret, group);
	SET_SUBMENU(scaleview, ret);
	SET_SUBMENU(images, ret);
	SET_SUBMENU(toolbars, ret);
	SET_SUBMENU(tabs, ret);
	SET_SUBMENU(debugging, ret);

	ATTACH_PARENT(parent, gtkView, ret->view, group);

	return ret;
}

/** 
* creates a nav menu
* \param group the 'global' in a gtk sense accelerator reference
*/

static struct nsgtk_nav_menu *nsgtk_menu_nav_menu(GtkAccelGroup *group, 
						  GtkMenuShell *parent)
{
	struct nsgtk_nav_menu *ret = malloc(sizeof(struct nsgtk_nav_menu));
	if (ret == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}
	ret->nav_menu = GTK_MENU(gtk_menu_new());
	if (ret->nav_menu == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(ret);
		return NULL;
	}
	IMAGE_ITEM(nav, back, gtkBack, ret, group);
	IMAGE_ITEM(nav, forward, gtkForward, ret, group);
	IMAGE_ITEM(nav, home, gtkHome, ret, group);
	ADD_SEP(nav, ret);
	IMAGE_ITEM(nav, localhistory, gtkLocalHistory, ret, group);
	IMAGE_ITEM(nav, globalhistory, gtkGlobalHistory, ret, group);
	ADD_SEP(nav, ret);
	IMAGE_ITEM(nav, addbookmarks, gtkAddBookMarks, ret, group);
	IMAGE_ITEM(nav, showbookmarks, gtkShowBookMarks, ret, group);
	ADD_SEP(nav, ret);
	IMAGE_ITEM(nav, openlocation, gtkOpenLocation, ret, group);

	ATTACH_PARENT(parent, gtkNavigate, ret->nav, group);

	return ret;
}

/** 
* creates a help menu
* \param group the 'global' in a gtk sense accelerator reference
*/

static struct nsgtk_help_menu *nsgtk_menu_help_menu(GtkAccelGroup *group, 
						  GtkMenuShell *parent)
{
	struct nsgtk_help_menu *ret = malloc(sizeof(struct nsgtk_help_menu));
	if (ret == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}
	ret->help_menu = GTK_MENU(gtk_menu_new());
	if (ret->help_menu == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		free(ret);
		return NULL;
	}
	IMAGE_ITEM(help, contents, gtkContents, ret, group);
	IMAGE_ITEM(help, guide, gtkGuide, ret, group);
	IMAGE_ITEM(help, info, gtkUserInformation, ret, group);
	ADD_SEP(help, ret);
	IMAGE_ITEM(help, about, gtkAbout, ret, group);

	ATTACH_PARENT(parent, gtkHelp, ret->help, group);

	return ret;
}

#define MENUBAR_MENU(p, q, r)					\
	p->q = nsgtk_menu_##q##_menu(group, GTK_MENU_SHELL(r));	\
	p->rclick_##q = nsgtk_menu_##q##_menu(group, NULL)		


struct nsgtk_menu *nsgtk_menu_create(GladeXML *xml, GtkWindow *window)
{
	GtkAccelGroup *group;
	struct nsgtk_menu *nmenu;
	GtkMenuBar *menubar;

	nmenu = malloc(sizeof(struct nsgtk_menu));
	if (nmenu == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return NULL;
	}

	group = gtk_accel_group_new();
	gtk_window_add_accel_group(window, group);
	menubar = GTK_MENU_BAR(glade_xml_get_widget(xml, "menubar"));

	MENUBAR_MENU(nmenu, file, menubar);
	MENUBAR_MENU(nmenu, edit, menubar);
	MENUBAR_MENU(nmenu, view, menubar);
	MENUBAR_MENU(nmenu, nav, menubar);
	MENUBAR_MENU(nmenu, help, menubar);

	return nmenu;
}
