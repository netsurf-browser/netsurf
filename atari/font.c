/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#include <inttypes.h>
#include <assert.h>
#include <stdbool.h>
#include <windom.h>

#include "css/css.h"
#include "render/font.h"
#include "utils/utf8.h"
#include "utils/log.h"
#include "desktop/options.h"
#include "desktop/mouse.h"
#include "desktop/plotters.h"

#include "atari/gui.h"
#include "atari/font.h"
#include "atari/plot.h"
#include "atari/options.h"
#include "atari/findfile.h"
#include "atari/gui.h"
#include "atari/plot.h"
#include "atari/plot/plotter.h"

extern GEM_FONT_PLOTTER fplotter;

static bool atari_font_position_in_string(const plot_font_style_t * fstyle,const char *string,
						size_t length,int x, size_t *char_offset, int *actual_x )
{
	fplotter->pixel_pos(fplotter, fstyle, string, length, x, char_offset, actual_x );
	return( true );
}

static bool atari_font_split( const plot_font_style_t * fstyle, const char *string,
					  size_t length,int x, size_t *char_offset, int *actual_x )
{
	fplotter->str_split( fplotter, fstyle, string, length, x, char_offset, actual_x );
	return( true );
}

static bool atari_font_width( const plot_font_style_t *fstyle, const char * str,
					  size_t length, int * width  )
{
	fplotter->str_width( fplotter, fstyle, str, length, width );
	return( true );
}

const struct font_functions nsfont = {
	atari_font_width,
	atari_font_position_in_string,
	atari_font_split
};


