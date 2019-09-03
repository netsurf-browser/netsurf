/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
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

#ifndef NETSURF_GTK_TOOLBAR_ITEMS_H
#define NETSURF_GTK_TOOLBAR_ITEMS_H

typedef enum {
	BACK_BUTTON = 0,
	HISTORY_BUTTON,
	FORWARD_BUTTON,
	STOP_BUTTON,
	RELOAD_BUTTON,
	HOME_BUTTON,
	URL_BAR_ITEM,
	WEBSEARCH_ITEM,
	THROBBER_ITEM,
	NEWWINDOW_BUTTON,
	NEWTAB_BUTTON,
	OPENFILE_BUTTON,
	CLOSETAB_BUTTON,
	CLOSEWINDOW_BUTTON,
	SAVEPAGE_BUTTON,
	PDF_BUTTON,
	PLAINTEXT_BUTTON,
	DRAWFILE_BUTTON,
	POSTSCRIPT_BUTTON,
	PRINTPREVIEW_BUTTON,
	PRINT_BUTTON,
	QUIT_BUTTON,
	CUT_BUTTON,
	COPY_BUTTON,
	PASTE_BUTTON,
	DELETE_BUTTON,
	SELECTALL_BUTTON,
	FIND_BUTTON,
	PREFERENCES_BUTTON,
	ZOOMPLUS_BUTTON,
	ZOOMMINUS_BUTTON,
	ZOOMNORMAL_BUTTON,
	FULLSCREEN_BUTTON,
	VIEWSOURCE_BUTTON,
	DOWNLOADS_BUTTON,
	SAVEWINDOWSIZE_BUTTON,
	TOGGLEDEBUGGING_BUTTON,
	SAVEBOXTREE_BUTTON,
	SAVEDOMTREE_BUTTON,
	LOCALHISTORY_BUTTON,
	GLOBALHISTORY_BUTTON,
	ADDBOOKMARKS_BUTTON,
	SHOWBOOKMARKS_BUTTON,
	SHOWCOOKIES_BUTTON,
	OPENLOCATION_BUTTON,
	NEXTTAB_BUTTON,
	PREVTAB_BUTTON,
	CONTENTS_BUTTON,
	GUIDE_BUTTON,
	INFO_BUTTON,
	ABOUT_BUTTON,
	OPENMENU_BUTTON,
	PLACEHOLDER_BUTTON /* size indicator; array maximum indices */
} nsgtk_toolbar_button;    /* PLACEHOLDER_BUTTON - 1 */

#endif

/*
 * Item fields are:
 *   identifier enum
 *   name
 *   initial sensitivity
 *   if there is a toolbar click signal handler
 *   if there is a menu activate signal handler
 *   if the menu activate signal handler calls the toolbar click handler
 */

#ifndef TOOLBAR_ITEM
#define TOOLBAR_ITEM(a, b, c, d, e, f)
#define TOOLBAR_ITEM_SET
#endif

TOOLBAR_ITEM(BACK_BUTTON, back, false, y, y, y)
TOOLBAR_ITEM(HISTORY_BUTTON, history, true, y, n, n)
TOOLBAR_ITEM(FORWARD_BUTTON, forward, false, y, y, y)
TOOLBAR_ITEM(STOP_BUTTON, stop, false, y, y, y)
TOOLBAR_ITEM(RELOAD_BUTTON, reload, true, y, y, y)
TOOLBAR_ITEM(HOME_BUTTON, home, true, y, y, y)
TOOLBAR_ITEM(URL_BAR_ITEM, url_bar, true, n, n, n)
TOOLBAR_ITEM(WEBSEARCH_ITEM, websearch, true, n, n, n)
TOOLBAR_ITEM(THROBBER_ITEM, throbber, true, n, n, n)
TOOLBAR_ITEM(NEWWINDOW_BUTTON, newwindow, true, y, y, y)
TOOLBAR_ITEM(NEWTAB_BUTTON, newtab, true, y, y, y)
TOOLBAR_ITEM(OPENFILE_BUTTON, openfile, true, y, y, y)
TOOLBAR_ITEM(CLOSETAB_BUTTON, closetab, false, n, y, n)
TOOLBAR_ITEM(CLOSEWINDOW_BUTTON, closewindow, true, y, y, y)
TOOLBAR_ITEM(SAVEPAGE_BUTTON, savepage, true, y, y, y)
TOOLBAR_ITEM(PDF_BUTTON, pdf, false, y, y, y)
TOOLBAR_ITEM(PLAINTEXT_BUTTON, plaintext, true, y, y, y)
TOOLBAR_ITEM(DRAWFILE_BUTTON, drawfile, false, n, n, n)
TOOLBAR_ITEM(POSTSCRIPT_BUTTON, postscript, false, n, n, n)
TOOLBAR_ITEM(PRINTPREVIEW_BUTTON, printpreview, false, n, y, y)
TOOLBAR_ITEM(PRINT_BUTTON, print, true, y, y, y)
TOOLBAR_ITEM(QUIT_BUTTON, quit, true, y, y, y)
TOOLBAR_ITEM(CUT_BUTTON, cut, true, y, y, y)
TOOLBAR_ITEM(COPY_BUTTON, copy, true, y, y, y)
TOOLBAR_ITEM(PASTE_BUTTON, paste, true, y, y, y)
TOOLBAR_ITEM(DELETE_BUTTON, delete, false, y, y, y)
TOOLBAR_ITEM(SELECTALL_BUTTON, selectall, true, y, y, y)
TOOLBAR_ITEM(FIND_BUTTON, find, true, n, y, n)
TOOLBAR_ITEM(PREFERENCES_BUTTON, preferences, true, y, y, y)
TOOLBAR_ITEM(ZOOMPLUS_BUTTON, zoomplus, true, y, y, y)
TOOLBAR_ITEM(ZOOMMINUS_BUTTON, zoomminus, true, y, y, y)
TOOLBAR_ITEM(ZOOMNORMAL_BUTTON, zoomnormal, true, y, y, y)
TOOLBAR_ITEM(FULLSCREEN_BUTTON, fullscreen, true, y, y, y)
TOOLBAR_ITEM(VIEWSOURCE_BUTTON, viewsource, true, y, y, y)
TOOLBAR_ITEM(DOWNLOADS_BUTTON, downloads, true, y, y, y)
TOOLBAR_ITEM(SAVEWINDOWSIZE_BUTTON, savewindowsize, true, y, y, y)
TOOLBAR_ITEM(TOGGLEDEBUGGING_BUTTON, toggledebugging, true, y, y, y)
TOOLBAR_ITEM(SAVEBOXTREE_BUTTON, debugboxtree, true, y, y, y)
TOOLBAR_ITEM(SAVEDOMTREE_BUTTON, debugdomtree, true, y, y, y)
TOOLBAR_ITEM(LOCALHISTORY_BUTTON, localhistory, true, y, y, y)
TOOLBAR_ITEM(GLOBALHISTORY_BUTTON, globalhistory, true, y, y, y)
TOOLBAR_ITEM(ADDBOOKMARKS_BUTTON, addbookmarks, true, y, y, y)
TOOLBAR_ITEM(SHOWBOOKMARKS_BUTTON, showbookmarks, true, y, y, y)
TOOLBAR_ITEM(SHOWCOOKIES_BUTTON, showcookies, true, y, y, y)
TOOLBAR_ITEM(OPENLOCATION_BUTTON, openlocation, true, y, y, y)
TOOLBAR_ITEM(NEXTTAB_BUTTON, nexttab, false, n, y, n)
TOOLBAR_ITEM(PREVTAB_BUTTON, prevtab, false, n, y, n)
TOOLBAR_ITEM(CONTENTS_BUTTON, contents, true, y, y, y)
TOOLBAR_ITEM(GUIDE_BUTTON, guide, true, y, y, y)
TOOLBAR_ITEM(INFO_BUTTON, info, true, y, y, y)
TOOLBAR_ITEM(ABOUT_BUTTON, about, true, y, y, y)
TOOLBAR_ITEM(OPENMENU_BUTTON, openmenu, true, y, n, n)

#ifdef TOOLBAR_ITEM_SET
#undef TOOLBAR_ITEM
#undef TOOLBAR_ITEM_SET
#endif
