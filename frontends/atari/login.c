/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#include "utils/config.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <cflib.h>

#include "utils/errors.h"
#include "utils/utils.h"
#include "utils/messages.h"
#include "utils/log.h"

#include "atari/gui.h"
#include "atari/misc.h"
#include "atari/login.h"
#include "atari/res/netsurf.rsh"


bool login_form_do(nsurl * url, char * realm, char ** u_out, char ** p_out)
{
	char user[255];
	char pass[255];
	short exit_obj = 0;
	OBJECT * tree;

	user[0] = 0;
	pass[0] = 0;

	tree = gemtk_obj_get_tree(LOGIN);

	assert(tree != NULL);

	exit_obj = simple_mdial(tree, 0);

	if(exit_obj == LOGIN_BT_LOGIN) {
		get_string(tree, LOGIN_TB_USER, user);
		get_string(tree, LOGIN_TB_PASSWORD, pass);
		int size = strlen((char*)&user) + strlen((char*)&pass) + 2 ;
		*u_out = malloc(strlen((char*)&user) + 1);
		*p_out = malloc(strlen((char*)&pass) + 1);
		if (u_out == NULL || p_out == NULL) {
			free(*u_out);
			free(*p_out);
			return false;
		}
		memcpy(*u_out, (char*)&user, strlen((char*)&user) + 1);
		memcpy(*p_out, (char*)&pass, strlen((char*)&pass) + 1);
	} else {
		*u_out = NULL;
		*p_out = NULL;
	}
	return((exit_obj == LOGIN_BT_LOGIN));
}

