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
#ifndef _NETSURF_GTK_MENU_H_
#define _NETSURF_GTK_MENU_H_

#include <gtk/gtk.h>

struct nsgtk_file_menu {
	GtkMenuItem                     *file; /* File menu item on menubar */
	GtkMenu				*file_menu;
	GtkImageMenuItem		*newwindow_menuitem;
	GtkImageMenuItem		*newtab_menuitem;
	GtkImageMenuItem		*openfile_menuitem;
	GtkImageMenuItem		*closewindow_menuitem;
	GtkImageMenuItem		*savepage_menuitem;
	GtkImageMenuItem		*export_menuitem;
	struct nsgtk_export_submenu	*export_submenu;
	GtkImageMenuItem		*printpreview_menuitem;
	GtkImageMenuItem		*print_menuitem;
	GtkImageMenuItem		*quit_menuitem;
};

struct nsgtk_edit_menu {
	GtkMenuItem             *edit; /* Edit menu item on menubar */
	GtkMenu			*edit_menu;
	GtkImageMenuItem	*cut_menuitem;
	GtkImageMenuItem	*copy_menuitem;
	GtkImageMenuItem	*paste_menuitem;
	GtkImageMenuItem	*delete_menuitem;
	GtkImageMenuItem	*selectall_menuitem;
	GtkImageMenuItem	*find_menuitem;
	GtkImageMenuItem	*preferences_menuitem;
};

struct nsgtk_view_menu {
	GtkMenuItem             *view; /* View menu item on menubar */
	GtkMenu			*view_menu; /* gtk menu attached to menu item */
	GtkImageMenuItem		*stop_menuitem;
	GtkImageMenuItem		*reload_menuitem;
	GtkImageMenuItem		*scaleview_menuitem;
	struct nsgtk_scaleview_submenu	*scaleview_submenu;
	GtkImageMenuItem		*fullscreen_menuitem;
	GtkImageMenuItem		*viewsource_menuitem;
	GtkImageMenuItem		*images_menuitem;
	struct nsgtk_images_submenu	*images_submenu;
	GtkImageMenuItem		*toolbars_menuitem;
	struct nsgtk_toolbars_submenu	*toolbars_submenu;
	GtkImageMenuItem		*tabs_menuitem;
	struct nsgtk_tabs_submenu	*tabs_submenu;
	GtkImageMenuItem		*downloads_menuitem;
	GtkImageMenuItem		*savewindowsize_menuitem;
	GtkImageMenuItem		*debugging_menuitem;
	struct nsgtk_debugging_submenu	*debugging_submenu;
};

struct nsgtk_nav_menu {
	GtkMenuItem             *nav; /* Nav menu item on menubar */
	GtkMenu			*nav_menu;
	GtkImageMenuItem	*back_menuitem;
	GtkImageMenuItem	*forward_menuitem;
	GtkImageMenuItem	*home_menuitem;
	GtkImageMenuItem	*localhistory_menuitem;
	GtkImageMenuItem	*globalhistory_menuitem;
	GtkImageMenuItem	*addbookmarks_menuitem;
	GtkImageMenuItem	*showbookmarks_menuitem;
	GtkImageMenuItem	*openlocation_menuitem;
};

struct nsgtk_help_menu {
	GtkMenuItem             *help; /* Help menu item on menubar */
	GtkMenu			*help_menu;
	GtkImageMenuItem	*contents_menuitem;
	GtkImageMenuItem	*guide_menuitem;
	GtkImageMenuItem	*info_menuitem;
	GtkImageMenuItem	*about_menuitem;
};

struct nsgtk_export_submenu {
	GtkMenu			*export_menu;
	GtkImageMenuItem	*plaintext_menuitem;
	GtkImageMenuItem	*drawfile_menuitem;
	GtkImageMenuItem	*postscript_menuitem;
	GtkImageMenuItem	*pdf_menuitem;
};

struct nsgtk_scaleview_submenu {
	GtkMenu			*scaleview_menu;
	GtkImageMenuItem	*zoomplus_menuitem;
	GtkImageMenuItem	*zoomminus_menuitem;
	GtkImageMenuItem	*zoomnormal_menuitem;
};

struct nsgtk_tabs_submenu {
	GtkMenu			*tabs_menu;
	GtkImageMenuItem	*nexttab_menuitem;
	GtkImageMenuItem	*prevtab_menuitem;
	GtkImageMenuItem	*closetab_menuitem;
};

struct nsgtk_images_submenu {
	GtkMenu			*images_menu;
	GtkCheckMenuItem	*foregroundimages_menuitem;
	GtkCheckMenuItem	*backgroundimages_menuitem;
};

struct nsgtk_toolbars_submenu {
	GtkMenu			*toolbars_menu;
	GtkCheckMenuItem	*menubar_menuitem;
	GtkCheckMenuItem	*toolbar_menuitem;
};

struct nsgtk_debugging_submenu {
	GtkMenu			*debugging_menu;
	GtkImageMenuItem	*toggledebugging_menuitem;
	GtkImageMenuItem	*saveboxtree_menuitem;
	GtkImageMenuItem	*savedomtree_menuitem;
};


struct nsgtk_menu {
	struct nsgtk_file_menu	*file;
	struct nsgtk_file_menu	*rclick_file;
	struct nsgtk_edit_menu	*edit;
	struct nsgtk_edit_menu	*rclick_edit;
	struct nsgtk_view_menu	*view;
	struct nsgtk_view_menu	*rclick_view;
	struct nsgtk_nav_menu	*nav;
	struct nsgtk_nav_menu	*rclick_nav;
	struct nsgtk_tabs_menu	*tabs;
	struct nsgtk_tabs_menu	*rclick_tabs;
	struct nsgtk_help_menu	*help;
	struct nsgtk_help_menu	*rclick_help;
};

struct nsgtk_menu *nsgtk_menu_create(GladeXML *xml, GtkWindow *window);

#endif
