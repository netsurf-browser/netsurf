/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 Richard Wilson <not_ginger_matt@hotmail.com>
 *
 * Complete details on using Tinct are available from http://www.tinct.net.
 */

/** \file
 * Tinct SWI numbers and flags for version 0.06
 */

#ifndef _NETSURF_RISCOS_TINCT_H_
#define _NETSURF_RISCOS_TINCT_H_

/*	Tinct_PlotAlpha plots the RGBA sprite pointed to by R2 at the OS screen location (R3,R4). Flags are
	supplied in R7.
*/
#define Tinct_PlotAlpha 0x57240

/*	Tinct_PlotScaledAlpha plots the RGBA sprite pointed to by R2 at the OS screen location (R3,R4)
	scaled to (R5,R6) OS units. Flags are supplied in R7.
*/
#define Tinct_PlotScaledAlpha 0x57241

/*	Tinct_PlotAlpha plots the RGB0 sprite pointed to by R2 at the OS screen location (R3,R4). Flags are
	supplied in R7.
*/
#define Tinct_Plot 0x57242

/*	Tinct_PlotScaledAlpha plots the RGB0 sprite pointed to by R2 at the OS screen location (R3,R4)
	scaled to (R5,R6) OS units. Flags are supplied in R7.
*/
#define Tinct_PlotScaled 0x57243

/*	Tinct_ConvertSprite creates a 32bpp sprite (pointer to memory supplied in R3) from a paletted sprite
	provided in R2.
*/
#define Tinct_ConvertSprite 0x57244

/*	Tinct_AvailableFeatures returns the current feature set.
*/
#define Tinct_AvailableFeatures 0x57245

/*	Flags
*/
#define tinct_READ_SCREEN_BASE	  0x01	/** <-- Use when hardware scrolling */
#define tinct_BILINEAR_FILTER	  0x02	/** <-- Perform bi-linear filtering */
#define tinct_DITHER		  0x04	/** <-- Perform dithering */
#define tinct_ERROR_DIFFUSE	  0x08	/** <-- Perform error diffusion */
#define tinct_DITHER_INVERTED	  0x0C	/** <-- Perform dithering with inverted pattern */
#define tinct_FILL_HORIZONTALLY	  0x10	/** <-- Horizontally fill clipping region with image */
#define tinct_FILL_VERTICALLY	  0x20	/** <-- Vertically fill clipping region with image */
#define tinct_FORCE_PALETTE_READ  0x40	/** <-- Use after a palette change when out of the desktop */
#define tinct_USE_OS_SPRITE_OP	  0x80	/** <-- Use when printing */

/*	Shifts
*/
#define tinct_BACKGROUND_SHIFT	  0x08

/*	Sprite mode
*/
#define tinct_SPRITE_MODE	  0x301680b5
#endif
