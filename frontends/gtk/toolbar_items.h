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
	PLACEHOLDER_BUTTON /* size indicator; array maximum indices */
} nsgtk_toolbar_button;    /* PLACEHOLDER_BUTTON - 1 */

#endif

/*
 * Item fields are:
 *   item identifier enum
 *   item name
 *   item initial visibility
 */

#ifndef TOOLBAR_ITEM
#define TOOLBAR_ITEM(a, b, c, d)
#define TOOLBAR_ITEM_SET
#endif

TOOLBAR_ITEM(BACK_BUTTON, back, false, back_button_clicked_cb)
TOOLBAR_ITEM(HISTORY_BUTTON, history, true, localhistory_button_clicked_cb)
TOOLBAR_ITEM(FORWARD_BUTTON, forward, false, forward_button_clicked_cb)
TOOLBAR_ITEM(STOP_BUTTON, stop, false, stop_button_clicked_cb)
TOOLBAR_ITEM(RELOAD_BUTTON, reload, true, reload_button_clicked_cb)
TOOLBAR_ITEM(HOME_BUTTON, home, true, home_button_clicked_cb)
TOOLBAR_ITEM(URL_BAR_ITEM, url_bar, true, NULL)
TOOLBAR_ITEM(WEBSEARCH_ITEM, websearch, true, NULL)
TOOLBAR_ITEM(THROBBER_ITEM, throbber, true, NULL)
TOOLBAR_ITEM(NEWWINDOW_BUTTON, newwindow, true, newwindow_button_clicked_cb)
TOOLBAR_ITEM(NEWTAB_BUTTON, newtab, true, newtab_button_clicked_cb)
TOOLBAR_ITEM(OPENFILE_BUTTON, openfile, true, openfile_button_clicked_cb)
TOOLBAR_ITEM(CLOSETAB_BUTTON, closetab, false, closetab_button_clicked_cb)
TOOLBAR_ITEM(CLOSEWINDOW_BUTTON, closewindow, true, closewindow_button_clicked_cb)
TOOLBAR_ITEM(SAVEPAGE_BUTTON, savepage, true, savepage_button_clicked_cb)
TOOLBAR_ITEM(PDF_BUTTON, pdf, false, pdf_button_clicked_cb)
TOOLBAR_ITEM(PLAINTEXT_BUTTON, plaintext, true, plaintext_button_clicked_cb)
TOOLBAR_ITEM(DRAWFILE_BUTTON, drawfile, false, NULL)
TOOLBAR_ITEM(POSTSCRIPT_BUTTON, postscript, false, NULL)
TOOLBAR_ITEM(PRINTPREVIEW_BUTTON, printpreview, false, NULL)
TOOLBAR_ITEM(PRINT_BUTTON, print, true, print_button_clicked_cb)
TOOLBAR_ITEM(QUIT_BUTTON, quit, true, quit_button_clicked_cb)
TOOLBAR_ITEM(CUT_BUTTON, cut, true, cut_button_clicked_cb)
TOOLBAR_ITEM(COPY_BUTTON, copy, true, copy_button_clicked_cb)
TOOLBAR_ITEM(PASTE_BUTTON, paste, true, paste_button_clicked_cb)
TOOLBAR_ITEM(DELETE_BUTTON, delete, false, delete_button_clicked_cb)
TOOLBAR_ITEM(SELECTALL_BUTTON, selectall, true, selectall_button_clicked_cb)
TOOLBAR_ITEM(FIND_BUTTON, find, true, NULL)
TOOLBAR_ITEM(PREFERENCES_BUTTON, preferences, true, preferences_button_clicked_cb)
TOOLBAR_ITEM(ZOOMPLUS_BUTTON, zoomplus, true, zoomplus_button_clicked_cb)
TOOLBAR_ITEM(ZOOMMINUS_BUTTON, zoomminus, true, zoomminus_button_clicked_cb)
TOOLBAR_ITEM(ZOOMNORMAL_BUTTON, zoomnormal, true, zoomnormal_button_clicked_cb)
TOOLBAR_ITEM(FULLSCREEN_BUTTON, fullscreen, true, fullscreen_button_clicked_cb)
TOOLBAR_ITEM(VIEWSOURCE_BUTTON, viewsource, true, viewsource_button_clicked_cb)
TOOLBAR_ITEM(DOWNLOADS_BUTTON, downloads, true, downloads_button_clicked_cb)
TOOLBAR_ITEM(SAVEWINDOWSIZE_BUTTON, savewindowsize, true, savewindowsize_button_clicked_cb)
TOOLBAR_ITEM(TOGGLEDEBUGGING_BUTTON, toggledebugging, true, toggledebugging_button_clicked_cb)
TOOLBAR_ITEM(SAVEBOXTREE_BUTTON, debugboxtree, true, debugboxtree_button_clicked_cb)
TOOLBAR_ITEM(SAVEDOMTREE_BUTTON, debugdomtree, true, debugdomtree_button_clicked_cb)
TOOLBAR_ITEM(LOCALHISTORY_BUTTON, localhistory, true, localhistory_button_clicked_cb)
TOOLBAR_ITEM(GLOBALHISTORY_BUTTON, globalhistory, true, NULL)
TOOLBAR_ITEM(ADDBOOKMARKS_BUTTON, addbookmarks, true, NULL)
TOOLBAR_ITEM(SHOWBOOKMARKS_BUTTON, showbookmarks, true, NULL)
TOOLBAR_ITEM(SHOWCOOKIES_BUTTON, showcookies, true, NULL)
TOOLBAR_ITEM(OPENLOCATION_BUTTON, openlocation, true, NULL)
TOOLBAR_ITEM(NEXTTAB_BUTTON, nexttab, false, NULL)
TOOLBAR_ITEM(PREVTAB_BUTTON, prevtab, false, NULL)
TOOLBAR_ITEM(CONTENTS_BUTTON, contents, true, NULL)
TOOLBAR_ITEM(GUIDE_BUTTON, guide, true, NULL)
TOOLBAR_ITEM(INFO_BUTTON, info, true, NULL)
TOOLBAR_ITEM(ABOUT_BUTTON, about, true, NULL)

#ifdef TOOLBAR_ITEM_SET
#undef TOOLBAR_ITEM
#undef TOOLBAR_ITEM_SET
#endif
