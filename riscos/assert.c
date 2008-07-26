/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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
 * Assert reporting (RISC OS implementation).
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "oslib/wimp.h"


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
