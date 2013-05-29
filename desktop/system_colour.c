/*
 * Copyright 2011 Vincent Sanders <vince@netsurf-browser.org>
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

/** \file
 * System colour handling
 *
 */

#include "utils/utils.h"
#include "utils/log.h"
#include "desktop/gui.h"
#include "utils/nsoption.h"



#define colour_list_len ((NSOPTION_SYS_COLOUR_END - NSOPTION_SYS_COLOUR_START) + 1)

static lwc_string *colour_list[colour_list_len];

static lwc_string **gui_system_colour_pw = NULL;


bool gui_system_colour_init(void)
{
	unsigned int ccount;

	if (gui_system_colour_pw != NULL) 
		return false;

	/* Intern colour strings */
	for (ccount = 0; ccount < colour_list_len; ccount++) {
		struct nsoption_s *opt;
		opt = &nsoptions[ccount + NSOPTION_SYS_COLOUR_START];
		if (lwc_intern_string(opt->key + SLEN("sys_colour_"), 
				      opt->key_len - SLEN("sys_colour_"), 
				      &(colour_list[ccount])) != lwc_error_ok) {
			return false;
		}
	}

	gui_system_colour_pw = colour_list;
	
	return true;
}

void gui_system_colour_finalize(void)
{
	unsigned int ccount;

	for (ccount = 0; ccount < colour_list_len; ccount++) {
		lwc_string_unref(colour_list[ccount]);
	}
}

colour gui_system_colour_char(const char *name)
{
	colour ret = 0;
	unsigned int ccount;

	for (ccount = 0; ccount < colour_list_len; ccount++) {
		if (strcmp(name,
			   nsoptions[ccount + NSOPTION_SYS_COLOUR_START].key + SLEN("sys_colour_")) == 0) {
			ret = nsoptions[ccount + NSOPTION_SYS_COLOUR_START].value.c;
			break;
		}
	}
	return ret;
}

#define ns_color_to_nscss(c) (((0xff - (((c) >> 24) & 0xff)) << 24) | ((c) & 0xFF00) | (((c) >> 16) & 0xFF) | (((c) & 0xFF) << 16))

css_error gui_system_colour(void *pw, lwc_string *name, css_color *colour)
{
	unsigned int ccount;
	bool match;

	for (ccount = 0; ccount < colour_list_len; ccount++) {
		if (lwc_string_caseless_isequal(name, 
				colour_list[ccount],
				&match) == lwc_error_ok && match) {
			*colour = ns_color_to_nscss(nsoptions[ccount + NSOPTION_SYS_COLOUR_START].value.c);
			return CSS_OK;
		}
	}	

	return CSS_INVALID;
}
