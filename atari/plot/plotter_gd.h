/*
 * Copyright 2012 Ole Loots <ole@monochrom.net>
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
#ifdef WITH_GD_PLOTTER
#ifndef GEM_PLOTTER_GD_H_INCLUDED
#define GEM_PLOTTER_GD_H_INCLUDED


#include <gd.h>
#include "plotter.h"

struct s_gd_priv_data {

};

/* this is an shortcut cast to access the members of the s_gd_priv_data */
#define THIS(instance) ((struct s_gd_priv_data*)self->priv_data)

/* Each driver object must export 1 it's own constructor: */
int ctor_plotter_gd( GEM_PLOTTER p );

#endif
#endif
