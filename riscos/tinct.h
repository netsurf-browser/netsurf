/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@hotmail.com>
 */

/** \file
 * Tinct SWI numbers
 */

#ifndef _NETSURF_RISCOS_TINCT_H_
#define _NETSURF_RISCOS_TINCT_H_

/*	Tinct_PlotAlpha plots the RGBA sprite pointed to by R2 at the OS screen location (R3,R4). Flags are
	supplied in R7 - see the Tinct documentation for further details.
*/
#define Tinct_PlotAlpha 0x57240

/*	Tinct_PlotScaledAlpha plots the RGBA sprite pointed to by R2 at the OS screen location (R3,R4)
	scaled to (R5,R6) OS units. Flags are supplied in R7 - see the Tinct documentation for further details.
*/
#define Tinct_PlotScaledAlpha 0x57241

/*	Tinct_PlotAlpha plots the RGB0 sprite pointed to by R2 at the OS screen location (R3,R4). Flags are
	supplied in R7 - see the Tinct documentation for further details.
*/
#define Tinct_Plot 0x57242

/*	Tinct_PlotScaledAlpha plots the RGB0 sprite pointed to by R2 at the OS screen location (R3,R4)
	scaled to (R5,R6) OS units. Flags are supplied in R7 - see the Tinct documentation for further details.
*/
#define Tinct_PlotScaled 0x57243

/*	Tinct_ConvertSprite creates a 32bpp sprite (pointer to memory supplied in R3) from a paletted sprite
	provided in R2 - see the Tinct documentation for further details.
*/
#define Tinct_ConvertSprite 0x57244

#endif
