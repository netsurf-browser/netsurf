/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2007 Daniel Silverstone <dsilvers@digital-scurf.org>
 */

/** \file
 * Fetching of data from a URL (Registration).
 */

#ifndef NETSURF_CONTENT_FETCHERS_FETCH_CURL_H
#define NETSURF_CONTENT_FETCHERS_FETCH_CURL_H

#include <curl/curl.h>

void register_curl_fetchers(void);

/** Global cURL multi handle. */
extern CURLM *fetch_curl_multi;

#endif
