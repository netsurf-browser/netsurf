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

#include <stdarg.h>
#include <stdio.h>

#include "utils/config.h"
#include "utils/sys_time.h"
#include "utils/utsname.h"
#include "desktop/version.h"

#include "utils/log.h"

/** flag to enable verbose logging */
bool verbose_log = false;

/** The stream to which logging is sent */
static FILE *logfile;

nserror nslog_init(nslog_ensure_t *ensure, int *pargc, char **argv)
{
	struct utsname utsname;
	nserror ret = NSERROR_OK;

	if (((*pargc) > 1) &&
	    (argv[1][0] == '-') &&
	    (argv[1][1] == 'v') &&
	    (argv[1][2] == 0)) {
		int argcmv;

		/* verbose logging to stderr */
		logfile = stderr;

		/* remove -v from argv list */
		for (argcmv = 2; argcmv < (*pargc); argcmv++) {
			argv[argcmv - 1] = argv[argcmv];
		}
		(*pargc)--;

		/* ensure we actually show logging */
		verbose_log = true;
	} else if (((*pargc) > 2) &&
	    (argv[1][0] == '-') &&
	    (argv[1][1] == 'V') &&
	    (argv[1][2] == 0)) {
		int argcmv;

		/* verbose logging to file */
		logfile = fopen(argv[2], "a+");

		/* remove -V and filename from argv list */
		for (argcmv = 3; argcmv < (*pargc); argcmv++) {
			argv[argcmv - 2] = argv[argcmv];
		}
		(*pargc) -= 2;

		if (logfile == NULL) {
			/* could not open log file for output */
			ret = NSERROR_NOT_FOUND;
			verbose_log = false;
		} else {

			/* ensure we actually show logging */
			verbose_log = true;
		}
	} else if (verbose_log == true) {
		/* default is logging to stderr */
		logfile = stderr;
	}

	/* ensure output file handle is correctly configured */
	if ((verbose_log == true) &&
	    (ensure != NULL) &&
	    (ensure(logfile) == false)) {
		/* failed to ensure output configuration */
		ret = NSERROR_INIT_FAILED;
		verbose_log = false;
	}

	/* sucessfull logging initialisation so log system info */
	if (ret == NSERROR_OK) {
		LOG("NetSurf version '%s'", netsurf_version);
		if (uname(&utsname) < 0) {
			LOG("Failed to extract machine information");
		} else {
			LOG("NetSurf on <%s>, node <%s>, release <%s>, version <%s>, machine <%s>",
			    utsname.sysname,
			    utsname.nodename,
			    utsname.release,
			    utsname.version,
			    utsname.machine);
		}
	}

	return ret;
}

#ifndef NDEBUG

/* Subtract the `struct timeval' values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0.
*/

static int
timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_usec < y->tv_usec) {
		int nsec = (int)(y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	if ((int)(x->tv_usec - y->tv_usec) > 1000000) {
		int nsec = (int)(x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
	   tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

/**
 * Obtain a formatted string suitable for prepending to a log message
 *
 * \return formatted string of the time since first log call
 */
static const char *nslog_gettime(void)
{
	static struct timeval start_tv;
	static char buff[32];

	struct timeval tv;
	struct timeval now_tv;

	if (!timerisset(&start_tv)) {
		gettimeofday(&start_tv, NULL);
	}
	gettimeofday(&now_tv, NULL);

	timeval_subtract(&tv, &now_tv, &start_tv);

	snprintf(buff, sizeof(buff),"(%ld.%06ld)",
			(long)tv.tv_sec, (long)tv.tv_usec);

	return buff;
}

void nslog_log(const char *file, const char *func, int ln, const char *format, ...)
{
	va_list ap;

	fprintf(logfile, "%s %s:%i %s: ", nslog_gettime(), file, ln, func);

	va_start(ap, format);

	vfprintf(logfile, format, ap);

	va_end(ap);

	fputc('\n', logfile);
}

#endif
