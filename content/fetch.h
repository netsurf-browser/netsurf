/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Fetching of data from a URL (interface).
 */

#ifndef _NETSURF_DESKTOP_FETCH_H_
#define _NETSURF_DESKTOP_FETCH_H_

#include <stdbool.h>
#include "netsurf/utils/config.h"

typedef enum {
              FETCH_TYPE,
              FETCH_DATA,
              FETCH_FINISHED,
              FETCH_ERROR,
              FETCH_REDIRECT,
#ifdef WITH_AUTH
              FETCH_AUTH
#endif
} fetch_msg;

struct content;
struct fetch;
#ifdef WITH_POST
struct form_successful_control;
#endif

extern bool fetch_active;

void fetch_init(void);
struct fetch * fetch_start(char *url, char *referer,
		void (*callback)(fetch_msg msg, void *p, char *data, unsigned long size),
		void *p, bool only_2xx
#ifdef WITH_POST
		, char *post_urlenc,
		struct form_successful_control *post_multipart
#endif
#ifdef WITH_COOKIES
		,bool cookies
#endif
		);
void fetch_abort(struct fetch *f);
void fetch_poll(void);
void fetch_quit(void);
const char *fetch_filetype(const char *unix_path);
char *fetch_mimetype(const char *ro_path);

#endif
