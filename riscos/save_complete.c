/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <string.h>
#include <unixlib/local.h> /* for __riscosify */
#include "oslib/osfile.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/render/form.h"
#include "netsurf/render/layout.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/save_complete.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_SAVE_COMPLETE

/** \todo URL rewriting
 *        Objects used by embedded html pages
 *        GUI
 */

void save_imported_sheets(struct content *c, int parent, int level, char *p, char* fn);

/* this is temporary. */
const char * const SAVE_PATH = "<NetSurf$Dir>.savetest.";
const char * const OBJ_DIR = "_files";

/** \todo this will probably want to take a filename */
void save_complete(struct content *c) {

	char *fname = 0, *spath;
	unsigned int i;

	if (c->type != CONTENT_HTML)
		return;

	fname = "test";  /*get_filename(c->data.html.base_url);*/

	spath = xcalloc(strlen(SAVE_PATH)+strlen(OBJ_DIR)+strlen(fname)+50,
			sizeof(char));

	sprintf(spath, "%s%s%s", SAVE_PATH, fname, OBJ_DIR);
	xosfile_create_dir(spath, 77);

        /* save stylesheets, ignoring the base sheet and <style> elements */
        for (i = 2; i != c->data.html.stylesheet_count; i++) {
		struct content *css = c->data.html.object[i].content;

                if (!css)
                        continue;

                save_imported_sheets(css, (int)i, 0, spath, fname);

                sprintf(spath, "%s%s%s.%d/css", SAVE_PATH, fname, OBJ_DIR, i);
                xosfile_save_stamped(spath, 0xf79,
				css->source_data,
				css->source_data + css->source_size);
        }

	/* save objects */
	for (i = 0; i != c->data.html.object_count; i++) {
		struct content *obj = c->data.html.object[i].content;

		/* skip difficult content types */
		if (!obj || obj->type >= CONTENT_PLUGIN) {
			continue;
		}

		sprintf(spath, "%s%s%s.%d", SAVE_PATH, fname, OBJ_DIR, i);

		xosfile_save_stamped(spath,
				ro_content_filetype(obj),
				obj->source_data,
				obj->source_data + obj->source_size);
	}

	/** \todo URL rewriting */

	/* save the html file out last of all (allows url rewriting first) */
	sprintf(spath, "%s%s", SAVE_PATH, fname);
	xosfile_save_stamped(spath, 0xfaf,
			c->source_data,
			c->source_data + c->source_size);

	xfree(spath);
	xfree(fname);
}

void save_imported_sheets(struct content *c, int parent, int level, char *p, char *fn)
{
        unsigned int j;

        for (j = 0; j != c->data.css.import_count; j++) {
		struct content *css = c->data.css.import_content[j];

                if (!css)
                        continue;

                save_imported_sheets(css, parent, level+1, p, fn);
                sprintf(p, "%s%s%s.%d%c%d/css", SAVE_PATH, fn, OBJ_DIR, parent, 'a'+level, j);
                xosfile_save_stamped(p, 0xf79,
				css->source_data,
				css->source_data + css->source_size);
        }
}

#endif
