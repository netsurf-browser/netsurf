/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Option reading and saving (interface).
 *
 * Non-platform specific options can be added by editing this file and
 * netsurf/desktop/options.c
 *
 * Platform specific options should be added in the platform options.h.
 *
 * The following types of options are supported:
 *  - bool (OPTION_BOOL)
 *  - int (OPTION_INTEGER)
 *  - char* (OPTION_STRING) (must be allocated on heap, may be 0, free before
 *                           assigning a new value)
 */

#ifndef _NETSURF_DESKTOP_OPTIONS_H_
#define _NETSURF_DESKTOP_OPTIONS_H_

enum { OPTION_HTTP_PROXY_AUTH_NONE = 0, OPTION_HTTP_PROXY_AUTH_BASIC = 1,
		OPTION_HTTP_PROXY_AUTH_NTLM = 2 };

extern bool option_http_proxy;
extern char *option_http_proxy_host;
extern int option_http_proxy_port;
extern int option_http_proxy_auth;
extern char *option_http_proxy_auth_user;
extern char *option_http_proxy_auth_pass;
extern int option_font_size;
extern int option_font_min_size;
extern char *option_accept_language;
extern bool option_ssl_verify_certificates;
extern int option_memory_cache_size;
extern bool option_block_ads;

void options_read(const char *path);
void options_write(const char *path);

#endif
