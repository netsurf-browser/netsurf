/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 */

/** \file
 * Toolbar creation (interface).
 */

#ifndef _NETSURF_RISCOS_TOOLBAR_H_
#define _NETSURF_RISCOS_TOOLBAR_H_

#include "oslib/wimp.h"


struct toolbar_icon;

struct toolbar {

	/*	Internal variables
	*/
	unsigned int resize_status;	// Update status width on next reformat?
	unsigned int update_pending;	// Update icons on next reformat?
	unsigned int icon_width;	// Current width of icons
	int width_internal;	// Width actually used on last reformat
	int status_height;	// Status bar height
	int status_old_width;	// Old status width
  	int width;	// Toolbar width on last reformat
	unsigned int height;	// Toolbar height on last reformat

	/*	General options
	*/
	unsigned int throbber_width;	// Throbber width (0 = unavaiable)
	unsigned int throbber_height;	// Throbber height (0 = unavaiable)
	unsigned int status_window;	// Show status window?
	unsigned int standard_buttons;	// Show standard buttons?
	unsigned int url_bar;	// Show URL bar?
	unsigned int throbber;	// Show Throbber?
	unsigned int status_width;	// Width of status window

	/*	The first toolbar icon
	*/
	struct toolbar_icon *icon;

	/*	Window handles
	*/
	wimp_w toolbar_handle;
	wimp_w status_handle;
};


struct toolbar *ro_toolbar_create(osspriteop_area *sprite_area, char *url_buffer,
		char *status_buffer, char *throbber_buffer);
void ro_toolbar_destroy(struct toolbar *toolbar);
void ro_toolbar_resize_status(struct toolbar *toolbar, int height);
int ro_toolbar_reformat(struct toolbar *toolbar, int width);
void ro_toolbar_status_reformat(struct toolbar *toolbar, int width);
int ro_toolbar_update(struct toolbar *toolbar);

#endif
