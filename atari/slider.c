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

#include "atari/slider.h"

long inline slider_pages( long content_dim, long workarea_dim )
{
	long ret;
	ret = (long)ceil( (float)content_dim / (float)workarea_dim );
	if( ret <= 0 )
		ret = 1;
	return( ret );
}

float inline slider_pages_dec( long content_dim, long workarea_dim )
{
	float ret;
	ret = (float)content_dim / (float)workarea_dim );
	if( ret <= 0 )
		ret = 1;
	return( ret );
}


int slider_gem_size( long content_dim, long workarea_dim )
{
	int ret;
	int pages = slider_pages(content_dim, workarea_dim);
	if( pages <= 0 )
		return 1;
	ret = 1000 / pages;
	if( ret <= 0 )
		ret = 1;
	return( ret );
}


int slider_pos_to_gem_pos( long content_dim, long workarea_dim, long slider_pos )
{
	int ret;
	int spos = slider_max_pos( content_dim, workarea_dim );
	if( spos < 1 )
		return( 0 );
	float x = (float)1000 / spos;
	if(content_dim >= 1000)
		ret = (int)( x * slider_pos );
	else
		ret = (int)ceilf( (float)x * slider_pos );
	return( max(0, ret) );
}


long slider_gem_pos_to_pos( long content_dim, long workarea_dim, int slider_pos )
{
	long ret;
	int nmax = slider_max_pos( content_dim, workarea_dim );
	float x = (float)1000 / nmax;
	if(content_dim >= 1000)
		ret = (int) ( (float) slider_pos * x);
	else
		ret = (int) ceilf( (float)slider_pos * x );
	return( max(0, ret)  );
}


long inline slider_gem_size_to_res( long workarea_dim, int gem_size )
{
	/* subtract [<-] and [->] buttons (16*2) from workarea: */
	int factor = 1000 / (workarea_dim - 16*2);
	return( max(1, gem_size / factor) );
}


long slider_gem_pos_to_res( long content_dim, long workarea_dim, int gem_pos )
{
	/* subtract size of boxchar: */
	int room = workarea_dim - 16 * 2 - slider_gem_size_to_res(workarea_dim, slider_gem_sz );

	/*
		1. Berechnen welchem Prozentsatz die GEM Position entspricht:
	 1000 = Grundwert
	 gem_pos = Prozentwert
	 Prozentsatz = ?
	 Rechnung: Prozentwert * 100 / 1000
	 Gekürzt: Prozentwert / 10
	 p_calc = der Prozentsatz / 100 -> fuer vereinfachte nutzung
	*/
	/* float p_gem = (float)gem_pos / 10; */
	float p_calc = (float)gem_pos / 1000;

	/*
		2. Berechnen welchem Pixel der Prozentsatz entspricht
		room = Grundwert
		p_gem = Prozentsatz
		Prozentwert = ?
		Rechnung = G * p / 100
		Anmerkung: es wird p_calc verwendet, da praktischer...
	*/
	int pixel = (float)((float)room * p_calc)+0.5;

	return( pixel );
}


/*

Not implemented

long slider_pos_to_res( long content_dim, long workarea_dim, int pos )
{
	long max_pos = slider_max_pos( content_dim, workarea_dim );
	return( -1 );
}
 */


