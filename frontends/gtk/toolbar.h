/*
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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

#ifndef NETSURF_GTK_TOOLBAR_H_
#define NETSURF_GTK_TOOLBAR_H_

/**
 * control toolbar context
 */
struct nsgtk_toolbar;


/**
 * create a control toolbar
 *
 * \param[in] builder The gtk builder object the toolbar is being created from
 * \param[out] toolbar a pointer to receive the result.
 * \return NSERROR_OK and toolbar updated on success else error code
 */
nserror nsgtk_toolbar_create(GtkBuilder *builder, struct browser_window *(*get_bw)(void *ctx), void *get_bw_ctx,struct nsgtk_toolbar **toolbar);


/**
 * Destroy toolbar previously created
 *
 * \param toolbar A toolbar returned from a creation
 * \return NSERROR_OK on success
 */
nserror nsgtk_toolbar_destroy(struct nsgtk_toolbar *toolbar);


/**
 * Update toolbar style and size based on current settings
 *
 * \param toolbar A toolbar returned from a creation
 * \return NSERROR_OK on success
 */
nserror nsgtk_toolbar_update(struct nsgtk_toolbar *tb);

/**
 * Start or stop a throbber in a toolbar
 *
 * \param toolbar A toolbar returned from a creation
 * \param active True if the throbber animation should play.
 * \return NSERROR_OK on success
 */
nserror nsgtk_toolbar_throbber(struct nsgtk_toolbar *tb, bool active);

/**
 * Update the toolbar url entry
 *
 * \param toolbar A toolbar returned from a creation
 * \param url The URL to set
 * \return NSERROR_OK on success
 */
nserror nsgtk_toolbar_set_url(struct nsgtk_toolbar *tb, nsurl *url);

/**
 * sets up the images for scaffolding.
 */
void nsgtk_theme_implement(struct nsgtk_scaffolding *g);

void nsgtk_toolbar_customization_init(struct nsgtk_scaffolding *g);
void nsgtk_toolbar_connect_all(struct nsgtk_scaffolding *g);
int nsgtk_toolbar_get_id_from_widget(GtkWidget *widget, struct nsgtk_scaffolding *g);


#endif
