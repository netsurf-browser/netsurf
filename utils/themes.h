/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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

#ifndef __THEMES_H__
#define __THEMES_H__

#include "image/bitmap.h"

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
