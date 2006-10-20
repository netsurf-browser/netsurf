/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 John-Mark Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Single/Multi-line UTF-8 text area (interface)
 */

#ifndef _NETSURF_RISCOS_TEXTAREA_H_
#define _NETSURF_RISCOS_TEXTAREA_H_
#include <stdbool.h>
#include <stdint.h>
#include "rufl.h"
#include "oslib/wimp.h"

/* Text area flags */
#define TEXTAREA_MULTILINE	0x01	/**< Text area is multiline */
#define TEXTAREA_READONLY	0x02	/**< Text area is read only */

uintptr_t textarea_create(wimp_w parent, wimp_i icon, unsigned int flags,
		const char *font_family, unsigned int font_size,
		rufl_style font_style);
bool textarea_update(uintptr_t self);
void textarea_destroy(uintptr_t self);
bool textarea_set_text(uintptr_t self, const char *text);
int textarea_get_text(uintptr_t self, char *buf, unsigned int len);
void textarea_insert_text(uintptr_t self, unsigned int index,
		const char *text);
void textarea_replace_text(uintptr_t self, unsigned int start,
		unsigned int end, const char *text);
void textarea_set_caret(uintptr_t self, unsigned int caret);
void textarea_set_caret_xy(uintptr_t self, int x, int y);
unsigned int textarea_get_caret(uintptr_t self);


#endif
