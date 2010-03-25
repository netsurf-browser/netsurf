/*
 * Copyright 2008-10 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

/** \file
 * Fetching of data from a file (implementation).
 */

#include <string.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/openurl.h>

struct Library *OpenURLBase;
struct OpenURLIFace *IOpenURL;

/**
 * Initialise the fetcher.
 *
 * Must be called once before any other function.
 */

void ami_openurl_open(void)
{
	if(OpenURLBase = OpenLibrary("openurl.library",0))
	{
		IOpenURL = (struct OpenURLIFace *)GetInterface(OpenURLBase,"main",1,NULL);
	}
}

void ami_openurl_close(const char *scheme)
{
	if(IOpenURL) DropInterface((struct Interface *)IOpenURL);
	if(OpenURLBase) CloseLibrary(OpenURLBase);
}

void gui_launch_url(const char *url)
{
	APTR procwin = SetProcWindow((APTR)-1L);
	char *launchurl = NULL;
	BPTR fptr = 0;

	if((strncasecmp(url,"ABOUT:",6)) &&
		(strncasecmp(url,"JAVASCRIPT:",11)))
	{
		launchurl = ASPrintf("URL:%s",url);

		if(launchurl && (fptr = Open(launchurl,MODE_OLDFILE)))
		{
			Close(fptr);
		}
		else if(IOpenURL)
			URL_OpenA(url,NULL);

		FreeVec(launchurl);
	}

	SetProcWindow(procwin);		
}
