/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

/** \file
 * Container format handling for themes etc. */
 
#ifndef __CONTAINER_H__
#define __CONTAINER_H__

#include <sys/types.h>

struct container_ctx;

/* reading interface */
struct container_ctx *container_open(const char *filename);
const unsigned char *container_get(struct container_ctx *ctx,
					const unsigned char *entryname,
					u_int32_t *size);
const unsigned char *container_get_name(struct container_ctx *ctx);
const unsigned char *container_get_author(struct container_ctx *ctx);
const unsigned char *container_iterate(struct container_ctx *ctx,
					int *state);

/* creating interface */
struct container_ctx *container_create(const char *filename,
					const unsigned char *name,
					const unsigned char *author);
void container_add(struct container_ctx *ctx, const unsigned char *entryname,
					const unsigned char *data,
					const u_int32_t datalen);
					
/* common interface */
void container_close(struct container_ctx *ctx);

#endif /* __CONTAINER_H__ */
