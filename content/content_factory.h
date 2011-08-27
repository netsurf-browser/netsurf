/*
 * Copyright 2011 John-Mark Bell <jmb@netsurf-browser.org>
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

#ifndef NETSURF_CONTENT_CONTENT_FACTORY_H_
#define NETSURF_CONTENT_CONTENT_FACTORY_H_

#include <stdbool.h>

#include <libwapcaplet/libwapcaplet.h>

#include "content/content_type.h"
#include "utils/errors.h"

#define CONTENT_FACTORY_REGISTER_TYPES(HNAME, HTYPELIST, HHANDLER)	\
									\
static lwc_string *HNAME##_mime_types[NOF_ELEMENTS(HTYPELIST)];	        \
									\
nserror HNAME##_init(void)						\
{									\
	uint32_t i;							\
	lwc_error lerror;						\
	nserror error;							\
									\
	for (i = 0; i < NOF_ELEMENTS(HNAME##_mime_types); i++) {	\
		lerror = lwc_intern_string(HTYPELIST[i],		\
					   strlen(HTYPELIST[i]),	\
					   &HNAME##_mime_types[i]);	\
		if (lerror != lwc_error_ok) {				\
			error = NSERROR_NOMEM;				\
			goto error;					\
		}							\
									\
		error = content_factory_register_handler(		\
			HNAME##_mime_types[i],				\
			&HHANDLER);					\
		if (error != NSERROR_OK)				\
			goto error;					\
	}								\
									\
	return NSERROR_OK;						\
									\
error:									\
	HNAME##_fini();							\
									\
	return error;							\
}									\
									\
void HNAME##_fini(void)							\
{									\
	uint32_t i;							\
									\
	for (i = 0; i < NOF_ELEMENTS(HNAME##_mime_types); i++) {	\
		if (HNAME##_mime_types[i] != NULL) {			\
			lwc_string_unref(HNAME##_mime_types[i]);	\
		}							\
	}								\
}

struct content;
struct llcache_handle;

typedef struct content_handler content_handler;

void content_factory_fini(void);

nserror content_factory_register_handler(lwc_string *mime_type,
		const content_handler *handler);

struct content *content_factory_create_content(struct llcache_handle *llcache, 
		const char *fallback_charset, bool quirks);

content_type content_factory_type_from_mime_type(lwc_string *mime_type);

#endif
