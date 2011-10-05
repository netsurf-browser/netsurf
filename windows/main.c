/*
 * Copyright 2011 Vincent Sanders <vince@simtec.co.uk>
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

#include <limits.h>
#include <stdbool.h>
#include <windows.h>

#include "desktop/gui.h"
#include "desktop/options.h"
#include "desktop/browser.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "utils/url.h"
#include "utils/filepath.h"
#include "content/fetchers/resource.h"

#include "windows/findfile.h"
#include "windows/drawable.h"
#include "windows/gui.h"

static char **respaths; /** resource search path vector. */

char *options_file_location;

nsurl *gui_get_resource_url(const char *path)
{
	char buf[PATH_MAX];
	char *raw;
	nsurl *url = NULL;

	raw = path_to_url(filepath_sfind(respaths, buf, path));
	if (raw != NULL) {
		nsurl_create(raw, &url);
		free(raw);
	}

	return url;
}

void gui_launch_url(const char *url)
{
}

void gui_quit(void)
{
	LOG(("gui_quit"));
}

/** 
 * Ensures output stdio stream is available
 */
bool nslog_ensure(FILE *fptr)
{
	/* mwindows compile flag normally invalidates standard io unless
	 *  already redirected 
	 */
	if (_get_osfhandle(fileno(fptr)) == -1) {
		AllocConsole();
		freopen("CONOUT$", "w", fptr);
	}
	return true;
}

/**
 * Entry point from operating system
 **/
int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hLastInstance, LPSTR lpcli, int ncmd)
{
	char **argv = NULL;
	int argc = 0, argctemp = 0;
	size_t len;
	LPWSTR *argvw;
	char *messages;
	nserror ret;
	struct browser_window *bw;
	const char *addr = NETSURF_HOMEPAGE;

	if (SLEN(lpcli) > 0) {
		argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
	}

	setbuf(stderr, NULL);

	/* Construct a unix style argc/argv */
	argv = malloc(sizeof(char *) * argc);
	while (argctemp < argc) {
		len = wcstombs(NULL, argvw[argctemp], 0) + 1;
		if (len > 0) {
			argv[argctemp] = malloc(len);
		}

		if (argv[argctemp] != NULL) {
			wcstombs(argv[argctemp], argvw[argctemp], len);
			/* alter windows-style forward slash flags to
			 * hyphen flags.
			 */
			if (argv[argctemp][0] == '/')
				argv[argctemp][0] = '-';
		}
		argctemp++;
	}

	respaths = nsws_init_resource("${APPDATA}\\NetSurf:${HOME}\\.netsurf:${NETSURFRES}:${PROGRAMFILES}\\NetSurf\\res:"NETSURF_WINDOWS_RESPATH);

	messages = filepath_find(respaths, "messages");

	options_file_location = filepath_find(respaths, "preferences");

	/* initialise netsurf */
	netsurf_init(&argc, &argv, options_file_location, messages);

	free(messages);

	ret = nsws_create_main_class(hInstance);
	ret = nsws_create_drawable_class(hInstance);
	ret = nsws_create_localhistory_class(hInstance);

	option_target_blank = false;

	nsws_window_init_pointers(hInstance);

	/* ensure homepage option has a default */
	if (option_homepage_url == NULL || option_homepage_url[0] == '\0')
		option_homepage_url = strdup(NETSURF_HOMEPAGE);

	/* If there is a url specified on the command line use it */
	if (argc > 1)
		addr = argv[1];
	else
		addr = option_homepage_url;

	LOG(("calling browser_window_create"));
	bw = browser_window_create(addr, 0, 0, true, false);

	netsurf_main_loop();

	netsurf_exit();

	free(options_file_location);

	return 0;
}
