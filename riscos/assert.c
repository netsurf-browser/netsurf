/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Assert reporting (RISC OS implementation).
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <oslib/wimp.h>


/**
 * Report an assert() failure and exit.
 */

void __assert2(const char *expr, const char *function, const char *file,
		int line)
{
	static const os_error error = { 1, "NetSurf has detected a serious "
			"error and must exit. Please submit a bug report, "
			"attaching the browser log file." };

	fprintf(stderr, "\n\"%s\", line %d: %s%sAssertion failed: %s\n",
			file, line,
			function ? function : "",
			function ? ": " : "",
			expr);
	fflush(stderr);

	xwimp_report_error_by_category(&error,
			wimp_ERROR_BOX_GIVEN_CATEGORY |
			wimp_ERROR_BOX_CATEGORY_ERROR <<
				wimp_ERROR_BOX_CATEGORY_SHIFT,
			"NetSurf", "!netsurf",
			(osspriteop_area *) 1, "Quit", 0);

	xos_cli("Filer_Run <Wimp$ScrapDir>.WWW.NetSurf.Log");

	abort();
}
