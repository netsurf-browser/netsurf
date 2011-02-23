/*
 * Copyright 2011 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf.
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
 * resource: URL method handler
 */

#ifndef NETSURF_CONTENT_FETCHERS_FETCH_RESOURCE_H
#define NETSURF_CONTENT_FETCHERS_FETCH_RESOURCE_H

/**
 * register the resource scheme.
 * 
 * should only be called from the fetch initialise
 */
void fetch_resource_register(void);

/**
 * callback to translate resource to full url
 */
char* gui_find_resource(const char *filename);

#endif
