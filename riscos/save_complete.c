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

	if (c->type != CONTENT_HTML) {
		return;
	}

	fname = "test";  /*get_filename(c->data.html.base_url);*/

	if (!fname) { /* no path -> exit */
		return;
	}

	spath = xcalloc(strlen(SAVE_PATH)+strlen(OBJ_DIR)+strlen(fname)+50,
			sizeof(char));

	sprintf(spath, "%s%s%s", SAVE_PATH, fname, OBJ_DIR);
	xosfile_create_dir(spath, 77);

        /* save stylesheets, ignoring the base sheet and <style> elements */
        for (i=2; i!=c->data.html.stylesheet_count; i++) {
                if (c->data.html.stylesheet_content[i] == 0) {
                        continue;
                }

                save_imported_sheets(c->data.html.stylesheet_content[i], (int)i, 0, spath, fname);

                sprintf(spath, "%s%s%s.%d/css", SAVE_PATH, fname, OBJ_DIR, i);
                xosfile_save_stamped(spath, 0xf79, c->data.html.stylesheet_content[i]->data.css.data, c->data.html.stylesheet_content[i]->data.css.data + c->data.html.stylesheet_content[i]->data.css.length);
        }

	/* save objects */
	for (i=0; i!=c->data.html.object_count; i++) {

		/* skip difficult content types */
		if (c->data.html.object[i].content->type >= CONTENT_PLUGIN) {
			continue;
		}

		sprintf(spath, "%s%s%s.%d", SAVE_PATH, fname, OBJ_DIR, i);

		switch(c->data.html.object[i].content->type) {
			case CONTENT_HTML:
			        strcat(spath, "/htm");
				xosfile_save_stamped(spath, 0xfaf, c->data.html.object[i].content->data.html.source, c->data.html.object[i].content->data.html.source + c->data.html.object[i].content->data.html.length);
				break;
			case CONTENT_JPEG:
			        strcat(spath, "/jpg");
				xosfile_save_stamped(spath, 0xc85, c->data.html.object[i].content->data.jpeg.data, (char*)c->data.html.object[i].content->data.jpeg.data + c->data.html.object[i].content->data.jpeg.length);
				break;
			case CONTENT_PNG:
			        strcat(spath, "/png");
				xosfile_save_stamped(spath, 0xb60, c->data.html.object[i].content->data.png.data, c->data.html.object[i].content->data.png.data + c->data.html.object[i].content->data.png.length);
				break;
			case CONTENT_GIF:
			        strcat(spath, "/gif");
				xosfile_save_stamped(spath, 0x695, c->data.html.object[i].content->data.gif.data, c->data.html.object[i].content->data.gif.data + c->data.html.object[i].content->data.gif.length);
				break;
			case CONTENT_SPRITE:
			        strcat(spath, "/spr");
				xosfile_save_stamped(spath, 0xff9, c->data.html.object[i].content->data.sprite.data, (char*)c->data.html.object[i].content->data.sprite.data + c->data.html.object[i].content->data.sprite.length);
				break;
			case CONTENT_DRAW:
			        strcat(spath, "/drw");
				xosfile_save_stamped(spath, 0xaff, c->data.html.object[i].content->data.draw.data, (char*)c->data.html.object[i].content->data.draw.data + c->data.html.object[i].content->data.draw.length);
				break;
			default:
				break;
		}
	}

	/** \todo URL rewriting */

	/* save the html file out last of all (allows url rewriting first) */
	sprintf(spath, "%s%s", SAVE_PATH, fname);
	xosfile_save_stamped(spath, 0xfaf,
				c->data.html.source,
				c->data.html.source + c->data.html.length);

	xfree(spath);
	xfree(fname);
}

void save_imported_sheets(struct content *c, int parent, int level, char *p, char *fn) {
        unsigned int j;

        for (j=0; j!=c->data.css.import_count; j++) {
                if (c->data.css.import_content[j] == 0) {
                        continue;
                }
                save_imported_sheets(c->data.css.import_content[j], parent, level+1, p, fn);
                sprintf(p, "%s%s%s.%d%c%d/css", SAVE_PATH, fn, OBJ_DIR, parent, 'a'+level, j);
                xosfile_save_stamped(p, 0xf79, c->data.css.import_content[j]->data.css.data, c->data.css.import_content[j]->data.css.data + c->data.css.import_content[j]->data.css.length);
        }
}

#endif
