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
	CUSTOMIZE_BUTTON,
	PLACEHOLDER_BUTTON /* size indicator; array maximum indices */
} nsgtk_toolbar_button;    /* PLACEHOLDER_BUTTON - 1 */

#endif

/*
 * Item fields are:
 *   identifier enum
 *   name
 *   initial sensitivity
 *   y/n - if there is a toolbar click signal handler
 *   y/n/p - if there is a menu activate signal handler and if it calls the
 *             toolbar click handler.
 */

#ifndef TOOLBAR_ITEM
#define TOOLBAR_ITEM(a, b, c, d, e)
#define TOOLBAR_ITEM_SET
#endif

TOOLBAR_ITEM(BACK_BUTTON, back, false, y, p)
TOOLBAR_ITEM(HISTORY_BUTTON, history, true, y, n)
TOOLBAR_ITEM(FORWARD_BUTTON, forward, false, y, p)
TOOLBAR_ITEM(STOP_BUTTON, stop, false, y, p)
TOOLBAR_ITEM(RELOAD_BUTTON, reload, true, y, p)
TOOLBAR_ITEM(HOME_BUTTON, home, true, y, p)
TOOLBAR_ITEM(URL_BAR_ITEM, url_bar, true, n, n)
TOOLBAR_ITEM(WEBSEARCH_ITEM, websearch, true, n, n)
TOOLBAR_ITEM(THROBBER_ITEM, throbber, true, n, n)
TOOLBAR_ITEM(NEWWINDOW_BUTTON, newwindow, true, y, p)
TOOLBAR_ITEM(NEWTAB_BUTTON, newtab, true, y, p)
TOOLBAR_ITEM(OPENFILE_BUTTON, openfile, true, y, p)
TOOLBAR_ITEM(CLOSETAB_BUTTON, closetab, false, n, y)
TOOLBAR_ITEM(CLOSEWINDOW_BUTTON, closewindow, true, y, p)
TOOLBAR_ITEM(SAVEPAGE_BUTTON, savepage, true, y, p)
TOOLBAR_ITEM(PDF_BUTTON, pdf, false, y, p)
TOOLBAR_ITEM(PLAINTEXT_BUTTON, plaintext, true, y, p)
TOOLBAR_ITEM(DRAWFILE_BUTTON, drawfile, false, n, n)
TOOLBAR_ITEM(POSTSCRIPT_BUTTON, postscript, false, n, n)
TOOLBAR_ITEM(PRINTPREVIEW_BUTTON, printpreview, false, n, p)
TOOLBAR_ITEM(PRINT_BUTTON, print, true, y, p)
TOOLBAR_ITEM(QUIT_BUTTON, quit, true, y, p)
TOOLBAR_ITEM(CUT_BUTTON, cut, true, y, p)
TOOLBAR_ITEM(COPY_BUTTON, copy, true, y, p)
TOOLBAR_ITEM(PASTE_BUTTON, paste, true, y, p)
TOOLBAR_ITEM(DELETE_BUTTON, delete, false, y, p)
TOOLBAR_ITEM(SELECTALL_BUTTON, selectall, true, y, p)
TOOLBAR_ITEM(FIND_BUTTON, find, true, n, y)
TOOLBAR_ITEM(PREFERENCES_BUTTON, preferences, true, y, p)
TOOLBAR_ITEM(ZOOMPLUS_BUTTON, zoomplus, true, y, p)
TOOLBAR_ITEM(ZOOMMINUS_BUTTON, zoomminus, true, y, p)
TOOLBAR_ITEM(ZOOMNORMAL_BUTTON, zoomnormal, true, y, p)
TOOLBAR_ITEM(FULLSCREEN_BUTTON, fullscreen, true, y, p)
TOOLBAR_ITEM(VIEWSOURCE_BUTTON, viewsource, true, y, p)
TOOLBAR_ITEM(DOWNLOADS_BUTTON, downloads, true, y, p)
TOOLBAR_ITEM(SAVEWINDOWSIZE_BUTTON, savewindowsize, true, y, p)
TOOLBAR_ITEM(TOGGLEDEBUGGING_BUTTON, toggledebugging, true, y, p)
TOOLBAR_ITEM(SAVEBOXTREE_BUTTON, debugboxtree, true, y, p)
TOOLBAR_ITEM(SAVEDOMTREE_BUTTON, debugdomtree, true, y, p)
TOOLBAR_ITEM(LOCALHISTORY_BUTTON, localhistory, true, y, p)
TOOLBAR_ITEM(GLOBALHISTORY_BUTTON, globalhistory, true, y, p)
TOOLBAR_ITEM(ADDBOOKMARKS_BUTTON, addbookmarks, true, y, p)
TOOLBAR_ITEM(SHOWBOOKMARKS_BUTTON, showbookmarks, true, y, p)
TOOLBAR_ITEM(SHOWCOOKIES_BUTTON, showcookies, true, y, p)
TOOLBAR_ITEM(OPENLOCATION_BUTTON, openlocation, true, y, p)
TOOLBAR_ITEM(NEXTTAB_BUTTON, nexttab, false, n, y)
TOOLBAR_ITEM(PREVTAB_BUTTON, prevtab, false, n, y)
TOOLBAR_ITEM(CONTENTS_BUTTON, contents, true, y, p)
TOOLBAR_ITEM(GUIDE_BUTTON, guide, true, y, p)
TOOLBAR_ITEM(INFO_BUTTON, info, true, y, p)
TOOLBAR_ITEM(ABOUT_BUTTON, about, true, y, p)
TOOLBAR_ITEM(OPENMENU_BUTTON, openmenu, true, y, n)
TOOLBAR_ITEM(CUSTOMIZE_BUTTON, cutomize, true, y, p)

#ifdef TOOLBAR_ITEM_SET
#undef TOOLBAR_ITEM
#undef TOOLBAR_ITEM_SET
#endif
