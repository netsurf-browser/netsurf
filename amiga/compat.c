/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include "compat.h"
#include <proto/exec.h>

/*
char *strndup(const char *s,size_t n)
{
	char *res;
	size_t len = strlen(s);

	if(n<len) len=n;
		
	res = malloc(len+1);
	if(!res)
		return(0);

	res[len] = '\0';

	return memcpy(res,s,len);
}
*/

void shutdown(void)
{
	/* dummy */
}

void jpeg_destroy_compress(void)
{
	/* dummy */
}

int uname(struct utsname *uts)
{
	struct Library *VersionBase;

	if(VersionBase = OpenLibrary("version.library",50))
	{
		sprintf(uts->release,"%ld.%ld",VersionBase->lib_Version,VersionBase->lib_Version);

		if((VersionBase->lib_Version >= 50) && (VersionBase->lib_Version <=52))
		{
			strcpy(uts->version,"4.0");
		}
		else if((VersionBase->lib_Version >= 53))
		{
			strcpy(uts->version,"4.1");
		}

		CloseLibrary(VersionBase);
	}

	strcpy(uts->sysname,"AmigaOS");
	strcpy(uts->nodename,"amiga");
	strcpy(uts->machine,"ppc");

}

uid_t geteuid(void)
{
	return 0;
}

uid_t getuid(void)
{
	return 0;
}

gid_t getgid(void)
{
	return 0;
}

gid_t getegid(void)
{
	return 0;
}

int tcsetattr(int fildes, int optional_actions, const struct termios *termios_p)
{
	return 0;
}

int tcgetattr(int fildes, struct termios *termios_p)
{
	return 0;
}
