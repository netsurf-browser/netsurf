/*
 * Copyright 2009 Rene W. Olsen <ac@rebels.com>
 * Copyright 2009 Stephen Fellner <sf.amiga@gmail.com>
 * Copyright 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "proto/exec.h"

#include "urlhistory.h"

#include "content/urldb.h"

struct List PageList;

static int InTag_urlhistory = 0;

void URLHistory_Init( void )
{
	// Initialise page list
	NewList( &PageList );
}


void URLHistory_Free( void )
{
	struct Node *node;

	while(( node = RemHead( &PageList )))
	{
		if( node->ln_Name) FreeVec( node->ln_Name );
		FreeVec( node );
	}
}


void URLHistory_ClearList( void )
{
	struct Node *node;

	while(( node = RemHead( &PageList )))
	{
		if( node->ln_Name) FreeVec( node->ln_Name );
		FreeVec( node );
	}
}


struct List * URLHistory_GetList( void )
{
	return &PageList;
}

static bool URLHistoryFound(const char *url, const struct url_data *data)
{
	struct Node *node;

	node = AllocVec( sizeof( struct Node ), MEMF_SHARED|MEMF_CLEAR );

	if ( node )
	{
		STRPTR urladd = (STRPTR) AllocVec( strlen ( url ) + 1, MEMF_SHARED|MEMF_CLEAR );
		if ( urladd )
		{
			strcpy(urladd, url);
			node->ln_Name = urladd;
			AddTail( &PageList, node );
		}
		else
		{
			FreeVec(node);
		}
	}
	return true;
}

struct Node * URLHistory_FindPage( const char *urlString )
{
	struct Node *node = PageList.lh_Head;
	while( node->ln_Succ )
	{
		//printf("compare %s to %s\n", node->ln_Name, urlString);
		if(strcasecmp(node->ln_Name, urlString) == 0) return node;
		node = node->ln_Succ;
	}

	return NULL;
}


void URLHistory_AddPage( const char * urlString )
{
	// Only add if not already in list and length > 0
	if( !URLHistory_FindPage( urlString ) && strlen( urlString ) > 0 )
	{
		urldb_iterate_partial(urlString, URLHistoryFound);
	}
}

