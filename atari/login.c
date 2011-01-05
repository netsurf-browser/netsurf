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

#include "desktop/401login.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <windom.h>
#include "utils/config.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/401login.h"
#include "desktop/gui.h"
#include "utils/errors.h"
#include "utils/utils.h"
#include "utils/messages.h"
#include "utils/log.h"
#include "utils/url.h"
#include "content/urldb.h"
#include "content/fetch.h"
#include "atari/login.h"
#include "atari/res/netsurf.rsh"


extern void * h_gem_rsrc;

bool login_form_do( char * url, char * realm, char ** out )
{
	OBJECT *tree, *newtree;
	WINDOW * form;
	char user[255];
	char pass[255];
	bool bres = false;
	int res = 0;
	const char * auth;
	char * host;
	assert( url_host( url, &host) == URL_FUNC_OK );	

	if( realm == NULL ){
		realm = (char*)"Secure Area";
	}

	int len = strlen(realm) + strlen(host) + 4;
	char * title = malloc( len );
	strncpy(title, realm, len );
	strncpy(title, ": ", len-strlen(realm) );
	strncat(title, host, len-strlen(realm)+2 );
  	
	auth = urldb_get_auth_details(url, realm);
	user[0] = 0;
	pass[0] = 0;
	/*
		TODO: use auth details if available:
	if( auth == NULL ){

	} else {
		
	}*/
	
	RsrcGaddr (h_gem_rsrc , R_TREE, LOGIN, &tree);
	ObjcChange( OC_OBJC, tree, LOGIN_BT_LOGIN, 0, 0 );
	ObjcChange( OC_OBJC, tree, LOGIN_BT_ABORT, 0, 0 );
	ObjcString( tree, LOGIN_TB_USER, (char*)&user );
	ObjcString( tree,  LOGIN_TB_PASSWORD, (char*)&pass  );
	form = FormWindBegin( tree, (char *)title );
	res = -1;
	while( res != LOGIN_BT_LOGIN && res != LOGIN_BT_ABORT ){
		res = FormWindDo( MU_MESAG );
		switch( res ){
			case LOGIN_BT_LOGIN:
				bres = true;
				break;

			case LOGIN_BT_ABORT:
				bres = false;
				break;
		}
	}
	
	if( bres ) {
		*out = malloc(strlen((char*)&user) + strlen((char*)&pass) + 2 );
		strcpy(*out, (char*)&user);
		strcat(*out, ":");
		strcat(*out, (char*)&pass);
	} else {
		*out = NULL;
	}

	FormWindEnd( );
	free( title );
	return( bres );
}
