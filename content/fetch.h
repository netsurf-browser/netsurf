/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/**
 * This module handles fetching of data from any url.
 *
 * Usage:
 *
 * fetch_init() must be called once before any other function. fetch_quit()
 * must be called before exiting.
 *
 * fetch_start() will begin fetching a url. The function returns immediately.
 * A pointer to an opaque struct fetch is returned, which can be passed to
 * fetch_abort() to abort the fetch at any time. The caller must supply a
 * callback function which is called when anything interesting happens. The
 * callback function is first called with msg = FETCH_TYPE, with the
 * Content-Type header in data, then one or more times with FETCH_DATA with
 * some data for the url, and finally with FETCH_FINISHED. Alternatively,
 * FETCH_ERROR indicates an error occurred: data contains an error message.
 * FETCH_REDIRECT may replace the FETCH_TYPE, FETCH_DATA, FETCH_FINISHED
 * sequence if the server sends a replacement URL.
 * Some private data can be passed as the last parameter to fetch_start, and
 * callbacks will contain this.
 *
 * fetch_poll() must be called regularly to make progress on fetches.
 *
 * fetch_filetype() is used internally to determine the mime type of local
 * files. It is platform specific, and implemented elsewhere.
 */

#ifndef _NETSURF_DESKTOP_FETCH_H_
#define _NETSURF_DESKTOP_FETCH_H_

#include <stdbool.h>

typedef enum {FETCH_TYPE, FETCH_DATA, FETCH_FINISHED, FETCH_ERROR, FETCH_REDIRECT} fetch_msg;

struct content;
struct fetch;

void fetch_init(void);
struct fetch * fetch_start(char *url, char *referer,
                 void (*callback)(fetch_msg msg, void *p, char *data, unsigned long size),
		 void *p, bool only_2xx);
void fetch_abort(struct fetch *f);
void fetch_poll(void);
void fetch_quit(void);
const char *fetch_filetype(const char *unix_path);

#endif
