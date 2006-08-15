/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#ifndef __GTK_THROBBER_H__
#define __GTK_THROBBER_H__

#include <gtk/gtk.h>

struct nsgtk_throbber
{
	int		nframes;	/**< Number of frames in the throbber */
	GdkPixbuf	**framedata;
};

extern struct nsgtk_throbber *nsgtk_throbber;

bool nsgtk_throbber_initialise(const char *fn);
void nsgtk_throbber_finalise(void);

#endif /* __GTK_THROBBER_H__ */
