/*
 * Copyright 2014 Vincent Sanders <vince@netsurf-browser.org>
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
 *
 * Interface to platform-specific miscellaneous browser operation table.
 */

#ifndef _NETSURF_DESKTOP_GUI_MISC_H_
#define _NETSURF_DESKTOP_GUI_MISC_H_

struct form_control;
struct gui_window;
struct ssl_cert_info;
struct nsurl;

/**
 * Graphical user interface browser misc function table.
 *
 * function table implementing GUI interface to miscelaneous browser
 * functionality
 */
struct gui_browser_table {
	/* Mandantory entries */

	/**
	 * Schedule a callback.
	 *
	 * \param t interval before the callback should be made in ms or
	 *          negative value to remove any existing callback.
	 * \param callback callback function
	 * \param p user parameter passed to callback function
	 * \return NSERROR_OK on sucess or appropriate error on faliure
	 *
	 * The callback function will be called as soon as possible
	 * after the timeout has elapsed.
	 *
	 * Additional calls with the same callback and user parameter will
	 * reset the callback time to the newly specified value.
	 *
	 */
	nserror (*schedule)(int t, void (*callback)(void *p), void *p);

	/* Optional entries */

	/**
	 * called to allow the gui to cleanup.
	 */
	void (*quit)(void);

	/**
	 * core has no fetcher for url
	 */
	nserror (*launch_url)(struct nsurl *url);

	/**
	 * create a form select menu
	 */
	void (*create_form_select_menu)(struct gui_window *gw, struct form_control *control);

	/**
	 * verify certificate
	 */
	void (*cert_verify)(struct nsurl *url, const struct ssl_cert_info *certs, unsigned long num, nserror (*cb)(bool proceed, void *pw), void *cbpw);

	/**
	 * Prompt user for login
	 */
	void (*login)(struct nsurl *url, const char *realm,
			nserror (*cb)(bool proceed, void *pw), void *cbpw);

};

#endif
