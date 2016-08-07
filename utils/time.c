/*
 * Copyright 2007 Rob Kendrick <rjek@netsurf-browser.org>
 * Copyright 2004-2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 John Tytgat <joty@netsurf-browser.org>
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

/**
 * \file utils/time.c
 * \brief Implementation of time operations.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <curl/curl.h>

#include "utils/errors.h"
#include "utils/time.h"


/* exported interface documented in utils/time.h */
const char *rfc1123_date(time_t t)
{
	static char ret[30];

	struct tm *tm = gmtime(&t);
	const char *days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" },
		*months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	snprintf(ret, sizeof ret, "%s, %02d %s %d %02d:%02d:%02d GMT",
			days[tm->tm_wday], tm->tm_mday, months[tm->tm_mon],
			tm->tm_year + 1900, tm->tm_hour, tm->tm_min,
			tm->tm_sec);

	return ret;
}


/* exported function documented in utils/time.h */
int nsc_sntimet(char *str, size_t size, time_t *timep)
{
#ifndef HAVE_STRFTIME
	long long val;
	val = (long long)*timep;

	return snprintf(str, size, "%lld", val);
#else
	struct tm *ltm;

	ltm = localtime(timep);
	if (ltm == NULL) {
		return -1;
	}

	return strftime(str, size, "%s", ltm);
#endif
}


/* exported function documented in utils/time.h */
nserror nsc_snptimet(const char *str, size_t size, time_t *timep)
{
	time_t time_out;

#ifndef HAVE_STRPTIME
	char *rstr;

	if (size < 1) {
		return NSERROR_BAD_PARAMETER;
	}

	errno = 0;
	time_out = (time_t)strtoll(str, &rstr, 10);

	/* The conversion may have a range faliure or no digits were found */
	if ((errno != 0) || (rstr == str)) {
		return NSERROR_BAD_PARAMETER;
	}

#else
	struct tm ltm;

	if (size < 1) {
		return NSERROR_BAD_PARAMETER;
	}

	if (strptime(str, "%s", &ltm) == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	time_out = mktime(&ltm);

#endif
	*timep = time_out;

	return NSERROR_OK;
}


/* exported function documented in utils/time.h */
nserror nsc_strntimet(const char *str, size_t size, time_t *timep)
{
	time_t result;

	result = curl_getdate(str, NULL);

	if (result == -1) {
		return NSERROR_INVALID;
	}

	*timep = result;

	return NSERROR_OK;
}
