/*
 * Copyright 2008,2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_FONT_H
#define AMIGA_FONT_H

#include "css/css.h"
#include <graphics/text.h>

#define NSA_NORMAL 0
#define NSA_ITALIC 1
#define NSA_BOLD 2
#define NSA_BOLDITALIC 3

struct TextFont *ami_open_font(struct css_style *);
void ami_close_font(struct TextFont *tfont);
ULONG ami_unicode_text(struct RastPort *rp,char *string,ULONG length,struct css_style *style,ULONG x,ULONG y,ULONG c);

void ami_init_fonts(void);
void ami_close_fonts(void);
#endif
