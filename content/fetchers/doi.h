/*
 * Copyright 2011 François Revol <mmu_man@users.sourceforge.net>
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
 * doi: URL method handler.
 * 
 * The doi fetcher is intended to provide a redirection of doi URLs
 * to the canonical doi website accessible via HTTP.
 * cf. http://tools.ietf.org/html/draft-paskin-doi-uri
 *
 */

#ifndef NETSURF_CONTENT_FETCHERS_FETCH_DOI_H
#define NETSURF_CONTENT_FETCHERS_FETCH_DOI_H

/**
 * Register the resource scheme.
 * 
 * should only be called from the fetch initialise
 */
void fetch_doi_register(void);

#endif
