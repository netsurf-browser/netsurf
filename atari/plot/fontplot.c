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

#include "atari/plot/fontplot.h"

const struct s_font_driver_table_entry font_driver_table[] =
{
#ifdef WITH_VDI_FONT_DRIVER
	{"vdi", ctor_font_plotter_vdi, 0},
#endif
#ifdef WITH_FREETYPE_FONT_DRIVER
	{"freetype", ctor_font_plotter_freetype, 0},
#endif
#ifdef WITH_INTERNAL_FONT_DRIVER
	{"internal", ctor_font_plotter_internal, 0},
#endif
	{(char*)NULL, NULL, 0}
};

void dump_font_drivers(void)
{
	int i = 0;
	while( font_driver_table[i].name != NULL ) {
		printf("%s -> flags: %d\n",
			font_driver_table[i].name,
			font_driver_table[i].flags
		);
		i++;
	}
}


/*
 *	Create an new text plotter object
 *	Available: "vdi", "freetype"
 *	@param vdihandle the vdi handle to act upon,
 *	@param name selector ID (string) of the font plotter.
 *	@flags flags configration flags of the plotter,
 *	             available flags:
 *	             FONTPLOT_FLAG_MONOGLYPH - Enable 1 bit font plotting
 *	@param error set to != 0 when errors occur
*/
FONT_PLOTTER new_font_plotter( int vdihandle, char * name, unsigned long flags,
		int * error)
{
	int i=0;
	int res = 0-ERR_PLOTTER_NOT_AVAILABLE;
	FONT_PLOTTER fplotter;

	/* allocate the font plotter instance: */
	fplotter = (FONT_PLOTTER)malloc( sizeof(struct s_font_plotter) );
	if( fplotter == NULL ) {
		*error = 0-ERR_NO_MEM;
		return( NULL );
	}

	/* Initialize the font plotter with the requested settings: */
	memset( fplotter, 0, sizeof(FONT_PLOTTER));
	fplotter->vdi_handle = vdihandle;
	fplotter->name = name;
	fplotter->flags = 0;
	fplotter->flags |= flags;

	/* Find the selector string in the font plotter table:  */
	/* And bail out when the font plotter is not available: */
	for( i = 0; ; i++) {
		if( font_driver_table[i].name == NULL ) {
			res = 0-ERR_PLOTTER_NOT_AVAILABLE;
			break;
		} else {
			if( strcmp(name, font_driver_table[i].name) == 0 ) {
				if( font_driver_table[i].ctor  ) {
					res = font_driver_table[i].ctor( fplotter );
					*error = 0;
				} else {
					res = 0-ERR_PLOTTER_NOT_AVAILABLE;
					*error = res;
					return (NULL);
				}
				break;
			}
		}
	}
	if( res < 0 ) {
		free( fplotter );
		*error = res;
		return( NULL );
   	}
	return( fplotter );
}

/*
	Free an font plotter
*/
int delete_font_plotter(FONT_PLOTTER p)
{
	if( p ) {
		p->dtor(p);
		free( p );
		p = NULL;
	}
	else
		return( -1 );
	return( 0 );
}

