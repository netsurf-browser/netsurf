/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#ifndef NETSURF_DESKTOP_401LOGIN_H
#define NETSURF_DESKTOP_401LOGIN_H

#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"

struct login {

        char *host;             /**< hostname */
	char *logindetails;     /**< string containing "username:password" */
	struct login *next;     /**< next in list */
	struct login *prev;     /**< previous in list */
};

void gui_401login_open(struct browser_window *bw, struct content *c,
                       char *realm);
void login_list_add(char *host, char *logindets);
struct login *login_list_get(char *host);
void login_list_remove(char *host);

#endif
