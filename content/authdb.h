/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * HTTP authentication database (interface)
 */

#ifndef _NETSURF_CONTENT_AUTHDB_H_
#define _NETSURF_CONTENT_AUTHDB_H_

bool authdb_insert(const char *url, const char *realm, const char *auth);
const char *authdb_get(const char *url);

#endif
