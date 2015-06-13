/*
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
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

#ifndef NETSURF_GTK_RESOURCES_H
#define NETSURF_GTK_RESOURCES_H 1

/**
 * Creates a menu cursor from internal resources
 */
GdkCursor *nsgtk_create_menu_cursor(void);

nserror nsgtk_init_resources(char **respath);

/**
 * Create gtk builder object for the named ui resource
 *
 * Creating gtk builder objects from a named resource requires the
 * source xml resource to be parsed.
 *
 * This creates a gtk builder instance using an identifier name which
 * is mapped to the ui_resource table which must be initialised with
 * nsgtk_init_resources()
 *
 * \param resname The resource name to construct for
 * \param builder_out The builder result
 * \return NSERROR_OK and builder_out updated or appropriate error code
 */
nserror nsgtk_builder_new_from_resname(const char *resname, GtkBuilder **builder_out);


/**
 * Create gdk pixbuf for the named ui resource
 *
 * This creates a pixbuf using an identifier name which is mapped to
 * the ui_resource table which must be initialised with
 * nsgtk_init_resources()
 *
 * \param resname The resource name to construct for
 * \param pixbuf_out The pixbuf result
 * \return NSERROR_OK and pixbuf_out updated or appropriate error code
 */
nserror nsgdk_pixbuf_new_from_resname(const char *resname, GdkPixbuf **pixbuf_out);

#endif
