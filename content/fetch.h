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

typedef enum {FETCH_TYPE, FETCH_DATA, FETCH_FINISHED, FETCH_ERROR, FETCH_REDIRECT, FETCH_AUTH} fetch_msg;

struct content;
struct fetch;
struct form_successful_control;

void fetch_init(void);
struct fetch * fetch_start(char *url, char *referer,
                 void (*callback)(fetch_msg msg, void *p, char *data, unsigned long size),
		 void *p, bool only_2xx, char *post_urlenc,
		struct form_successful_control *post_multipart);
void fetch_abort(struct fetch *f);
void fetch_poll(void);
void fetch_quit(void);
const char *fetch_filetype(const char *unix_path);

#endif
