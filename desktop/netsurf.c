/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/desktop/options.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

bool netsurf_quit = false;

static void netsurf_init(int argc, char** argv);
static void netsurf_poll(void);
static void netsurf_exit(void);
static void lib_init(void);

#ifndef curl_memdebug
extern void curl_memdebug(const char *logname);
#endif

/**
 * Gui NetSurf main().
 */

int main(int argc, char** argv)
{
	netsurf_init(argc, argv);

	while (!netsurf_quit)
		netsurf_poll();

	netsurf_exit();

	return EXIT_SUCCESS;
}


/**
 * Initialise components used by gui NetSurf.
 */

void netsurf_init(int argc, char** argv)
{
	struct utsname utsname;

	stdout = stderr;
	curl_memdebug("memdump");

	LOG(("version '%s'", netsurf_version));
	if (uname(&utsname) != 0)
		LOG(("Failed to extract machine information\n"));
	else
		LOG(("NetSurf on <%s>, node <%s>, release <%s>, version <%s>, "
				"machine <%s>\n", utsname.sysname,
				utsname.nodename, utsname.release,
				utsname.version, utsname.machine));

	lib_init();
	url_init();
	gui_init(argc, argv);
	setlocale(LC_ALL, "");
	fetch_init();
	fetchcache_init();
}


/**
 * Poll components which require it.
 */

void netsurf_poll(void)
{
	content_clean();
	gui_poll(fetch_active);
	fetch_poll();
}


/**
 * Clean up components used by gui NetSurf.
 */

void netsurf_exit(void)
{
	fetch_quit();
	gui_quit();
}


/**
 * Initialises the libraries used in NetSurf.
 */
static void lib_init(void)
{
	/* Using encoding "X-SJIS" (unknown to libxmp2/iconv) instead as
	 * "Shift-JIS" is rather popular.
	 */
	if (xmlAddEncodingAlias(xmlGetCharEncodingName(XML_CHAR_ENCODING_SHIFT_JIS), "X-SJIS") != 0)
		die(("Failed to add encoding alias"));
}
