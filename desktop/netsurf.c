/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 */

#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#ifdef riscos
#include "netsurf/riscos/buffer.h"
#endif
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

bool netsurf_quit = false;

static void netsurf_init(int argc, char** argv);
static void netsurf_poll(void);
static void netsurf_exit(void);
static void lib_init(void);


/**
 * Gui NetSurf main().
 */

int main(int argc, char** argv)
{
	setbuf(stderr, NULL);
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

#ifdef _MEMDEBUG_H_
	memdebug_memdebug("memdump");
#endif

	LOG(("version '%s'", netsurf_version));
	if (uname(&utsname) != 0)
		LOG(("Failed to extract machine information"));
	else
		LOG(("NetSurf on <%s>, node <%s>, release <%s>, version <%s>, "
				"machine <%s>", utsname.sysname,
				utsname.nodename, utsname.release,
				utsname.version, utsname.machine));
	lib_init();
	url_init();
	gui_init(argc, argv);
	setlocale(LC_ALL, "");
	fetch_init();
	fetchcache_init();
	gui_init2(argc, argv);
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
	gui_quit();
	content_quit();
	fetch_quit();
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
		die("Failed to add encoding alias");
}


