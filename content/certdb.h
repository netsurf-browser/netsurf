/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * HTTPS certificate verification database (interface)
 */

#ifndef _NETSURF_CONTENT_CERTDB_H_
#define _NETSURF_CONTENT_CERTDB_H_

bool certdb_insert(const char *url);
const char *certdb_get(const char *url);

#endif
