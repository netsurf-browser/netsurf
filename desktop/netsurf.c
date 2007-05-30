/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 Andrew Timmins <atimmins@blueyonder.co.uk>
 */

#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <libxml/encoding.h>
#include <libxml/globals.h>
#include <libxml/xmlversion.h>
#include "utils/config.h"
#include "content/fetch.h"
#include "content/fetchcache.h"
#include "content/urldb.h"
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#ifdef riscos
#include "riscos/buffer.h"
#endif
#include "utils/log.h"
#include "utils/url.h"
#include "utils/utf8.h"
#include "utils/utils.h"

bool netsurf_quit = false;
bool verbose_log = false;

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

	if ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == 'v') && (argv[1][2] == 0)) {
	    int argcmv;
	    verbose_log = true;
	    for (argcmv = 2; argcmv < argc; argcmv++) {
		argv[argcmv - 1] = argv[argcmv];
	    }
	    argc--;
	}

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
	LOG(("Closing GUI"));
	gui_quit();
	LOG(("Closing content"));
	content_quit();
	LOG(("Closing fetches"));
	fetch_quit();
	LOG(("Closing utf8"));
	utf8_finalise();
	LOG(("Destroying URLdb"));
	urldb_destroy();
	LOG(("Exited successfully"));
}


/**
 * Initialises the libraries used in NetSurf.
 */
void lib_init(void)
{
	LOG(("xmlParserVersion %s, LIBXML_VERSION_STRING %s",
			xmlParserVersion, LIBXML_VERSION_STRING));

	/* Using encoding "X-SJIS" (unknown to libxmp2/iconv) instead as
	 * "Shift-JIS" is rather popular.
	 */
	if (xmlAddEncodingAlias(xmlGetCharEncodingName(
			XML_CHAR_ENCODING_SHIFT_JIS), "X-SJIS") != 0)
		die("Failed to add encoding alias");
}


