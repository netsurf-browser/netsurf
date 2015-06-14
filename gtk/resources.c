/*
 * Copyright 2015 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 * Implementation of gtk builtin resource handling.
 *
 * This presents a unified interface to the rest of the codebase to
 * obtain resources. Note this is not anything to do with the resource
 * scheme handling beyond possibly providing the underlying data.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "utils/log.h"
#include "utils/filepath.h"

#include "gtk/compat.h"
#include "gtk/resources.h"

#ifdef __GNUC__
extern const guint8 menu_cursor_pixdata[] __attribute__ ((__aligned__ (4)));
#else
extern const guint8 menu_cursor_pixdata[];
#endif

enum nsgtk_resource_type_e {
	NSGTK_RESOURCE_FILE,
	NSGTK_RESOURCE_BUILTIN,
};

struct nsgtk_resource_s {
	const char *name;
	unsigned int len;
	enum nsgtk_resource_type_e type;
	char *path;
};

static struct nsgtk_resource_s ui_resource[] = {
	{ "netsurf", 7, NSGTK_RESOURCE_FILE, NULL },
	{ "tabcontents", 10, NSGTK_RESOURCE_FILE, NULL },
	{ "password", 8, NSGTK_RESOURCE_FILE, NULL },
	{ "login", 5, NSGTK_RESOURCE_FILE, NULL },
	{ "ssl", 3, NSGTK_RESOURCE_FILE, NULL },
	{ "toolbar", 7, NSGTK_RESOURCE_FILE, NULL },
	{ "downloads", 9, NSGTK_RESOURCE_FILE, NULL },
	{ "history", 7, NSGTK_RESOURCE_FILE, NULL },
	{ "options", 7, NSGTK_RESOURCE_FILE, NULL },
	{ "hotlist", 7, NSGTK_RESOURCE_FILE, NULL },
	{ "cookies", 7, NSGTK_RESOURCE_FILE, NULL },
	{ "viewdata", 8, NSGTK_RESOURCE_FILE, NULL },
	{ "warning", 7, NSGTK_RESOURCE_FILE, NULL },
	{ NULL, 0, NSGTK_RESOURCE_FILE, NULL },
};

static struct nsgtk_resource_s gen_resource[] = {
	{ "favicon.png", 11, NSGTK_RESOURCE_FILE, NULL },
	{ "netsurf.xpm", 11, NSGTK_RESOURCE_FILE, NULL },
	{ NULL, 0, NSGTK_RESOURCE_FILE, NULL },
};

/* exported interface documented in gtk/resources.h */
GdkCursor *nsgtk_create_menu_cursor(void)
{
	GdkCursor *cursor = NULL;
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_new_from_inline(-1, menu_cursor_pixdata, FALSE, NULL);
	cursor = gdk_cursor_new_from_pixbuf(gdk_display_get_default(), pixbuf, 0, 3);
	g_object_unref (pixbuf);

	return cursor;
}


/*
 * locate a resource
 *
 * The way GTK accesses resource files has changed greatly between
 * releases. This initilises the interface that hides all the
 * implementation details from the rest of the code.
 *
 * If the GResource is not enabled or the item cannot be found in the
 * compiled in resources the files will be loaded directly from disc
 * instead.
 *
 * \param respath A string vector containing the valid resource search paths
 * \param ui_res A resource entry to initialise
 */
static nserror
init_resource(char **respath, struct nsgtk_resource_s *resource)
{
	int resnamelen;
	char *resname;

#ifdef WITH_GRESOURCE
	gboolean present;

	resnamelen = snprintf(NULL, 0, "/org/netsurf/%s", resource->name);

	resname = malloc(resnamelen + 1);
	if (resname == NULL) {
		return NSERROR_NOMEM;
	}
	snprintf(resname, resnamelen + 1, "/org/netsurf/%s", resource->name);

	present = g_resources_get_info(resname,
				       G_RESOURCE_LOOKUP_FLAGS_NONE,
				       NULL, NULL, NULL);
	if (present == TRUE) {
		/* found an entry in the resources */
		resource->path = resname;
		resource->type = NSGTK_RESOURCE_BUILTIN;
		LOG("Found gresource path %s", resource->path);
		return NSERROR_OK;
	}
	LOG("gresource \"%s\" not found", resname);
	free(resname);
#endif

	resname = filepath_find(respath, resource->name);
	if (resname == NULL) {
		LOG("Unable to find resource %s on resource path",
		    resource->name);
		return NSERROR_NOT_FOUND;
	}

	/* found an entry on the path */
	resource->path = resname;
	resource->type = NSGTK_RESOURCE_FILE;

	LOG("Found file resource path %s", resource->path);
	return NSERROR_OK;
}

/*
 * locate a ui resource
 *
 * UI resources need their resource name changing to account for gtk versions
 *
 * \param respath A string vector containing the valid resource search paths
 * \param ui_res A resource entry to initialise
 */
static nserror init_ui_resource(char **respath, struct nsgtk_resource_s *ui_res)
{
#if GTK_CHECK_VERSION(3,0,0)
	int gtkv = 3;
#else
	int gtkv = 2;
#endif
	int resnamelen;
	char *resname;
	struct nsgtk_resource_s resource;
	nserror res;

	resnamelen = ui_res->len + 10; /* allow for the expanded ui name */

	resname = malloc(resnamelen);
	if (resname == NULL) {
		return NSERROR_NOMEM;
	}
	snprintf(resname, resnamelen, "%s.gtk%d.ui", ui_res->name, gtkv);
	resource.name = resname;
	resource.len = ui_res->len;
	resource.path = NULL;

	res = init_resource(respath, &resource);

	ui_res->path = resource.path;
	ui_res->type = resource.type;

	free(resname);

	return res;
}

#define SHOW_GRESOURCE

/**
 * Initialise UI resource table
 *
 */
nserror nsgtk_init_resources(char **respath)
{
	struct nsgtk_resource_s *resource;
	nserror res;

#ifdef SHOW_GRESOURCE
	const char *nspath = "/org/netsurf";
	char **reslist;
	char **cur;
	GError* gerror = NULL;
	reslist = g_resources_enumerate_children(nspath,G_RESOURCE_LOOKUP_FLAGS_NONE, &gerror);
	if (gerror) {
		LOG("gerror %s", gerror->message);
		g_error_free(gerror);

	} else {
	cur = reslist;
	while (cur != NULL && *cur != NULL) {
		LOG("gres %s", *cur);
		cur++;
	}
	g_strfreev(reslist);
	}
#endif

	/* walk the ui resource table and initialise all its members */
	resource = &ui_resource[0];
	while (resource->name != NULL) {
		res = init_ui_resource(respath, resource);
		if (res != NSERROR_OK) {
			return res;
		}
		resource++;
	}

	/* walk the general resource table and initialise all its members */
	resource = &gen_resource[0];
	while (resource->name != NULL) {
		res = init_resource(respath, resource);
		if (res != NSERROR_OK) {
			return res;
		}
		resource++;
	}


	return NSERROR_OK;
}

static struct nsgtk_resource_s *
find_resource_from_name(const char *resname, struct nsgtk_resource_s *resource)
{
	/* find resource from name */
	while ((resource->name != NULL) &&
	       ((resname[0] != resource->name[0]) ||
		(strncmp(resource->name, resname, resource->len) != 0))) {
		resource++;
	}
	return resource;
}

/* exported interface documented in gtk/resources.h */
nserror
nsgdk_pixbuf_new_from_resname(const char *resname, GdkPixbuf **pixbuf_out)
{
	struct nsgtk_resource_s *resource;
	GdkPixbuf *new_pixbuf;
	GError* error = NULL;

	resource = find_resource_from_name(resname, &gen_resource[0]);
	if (resource->name == NULL) {
		return NSERROR_NOT_FOUND;
	}

	if (resource->type == NSGTK_RESOURCE_FILE) {
		new_pixbuf = gdk_pixbuf_new_from_file(resource->path, &error);
	} else {
		new_pixbuf = gdk_pixbuf_new_from_resource(resource->path, &error);
	}
	if (new_pixbuf == NULL) {
		LOG("Unable to create pixbuf from file for %s with path %s \"%s\"",
			    resource->name, resource->path, error->message);
		g_error_free(error);
		return NSERROR_INIT_FAILED;
	}
	*pixbuf_out = new_pixbuf;

	return NSERROR_OK;
}

/* exported interface documented in gtk/resources.h */
nserror
nsgtk_builder_new_from_resname(const char *resname, GtkBuilder **builder_out)
{
	GtkBuilder *new_builder;
	struct nsgtk_resource_s *ui_res;
	GError* error = NULL;

	ui_res = find_resource_from_name(resname, &ui_resource[0]);
	if (ui_res->name == NULL) {
		return NSERROR_NOT_FOUND;
	}

	new_builder = gtk_builder_new();

	if (ui_res->type == NSGTK_RESOURCE_FILE) {
		if (!gtk_builder_add_from_file(new_builder,
					       ui_res->path,
					       &error)) {
			LOG("Unable to add UI builder from file for %s with path %s \"%s\"",
			    ui_res->name, ui_res->path, error->message);
			g_error_free(error);
			g_object_unref(G_OBJECT(new_builder));
			return NSERROR_INIT_FAILED;
		}
	} else {
		if (!nsgtk_builder_add_from_resource(new_builder,
						     ui_res->path,
						     &error)) {
			LOG("Unable to add UI builder from resource for %s with path %s \"%s\"",
			    ui_res->name, ui_res->path, error->message);
			g_error_free(error);
			g_object_unref(G_OBJECT(new_builder));
			return NSERROR_INIT_FAILED;
		}
	}

	*builder_out = new_builder;

	return NSERROR_OK;
}
