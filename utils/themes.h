/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */
 
#ifndef __THEMES_H__
#define __THEMES_H__

#include "netsurf/image/bitmap.h"

struct theme_descriptor;

void themes_initialise(void);
void themes_finalise(void);

void themes_add_new(const unsigned char *filename);

struct theme_descriptor *themes_open(const unsigned char *themename;
void themes_close(struct theme_descriptor *theme);
struct theme_descriptor *themes_enumerate(void **ctx);

struct bitmap *theme_get_image(struct theme_descriptor *theme,
				const unsigned char *name);

void themes_set_default(const unsigned char *themename);

const unsigned char *themes_get_name(struct theme_descriptor *theme);
const unsigned char *themes_get_author(struct theme_descriptor *theme);

#endif /* __THEMES_H__ */
