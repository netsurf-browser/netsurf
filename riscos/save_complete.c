/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <string.h>

#include <unixlib/local.h> /* for __riscosify */

#include <uri.h> /* possibly just have accessor methods in utils.c */

#include "oslib/osfile.h"

#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/render/form.h"
#include "netsurf/render/layout.h"
#include "netsurf/riscos/save_complete.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

/** \todo Save out CSS. */

#ifdef WITH_SAVE_COMPLETE

char* get_filename(char * url);

/* this is temporary. */
const char * const SAVE_PATH = "<NetSurf$Dir>.savetest.";
const char * const OBJ_DIR = "_files";

/** \todo this will probably want to take a filename */
void save_complete(struct content *c) {

	struct box *box;
	char *fname = 0, *spath, *ofname;
	unsigned int i;

	if (c->type != CONTENT_HTML) {
		return;
	}

	fname = get_filename(c->data.html.base_url);

	if (!fname) { /* no path -> exit */
		return;
	}

	spath = xcalloc(strlen(SAVE_PATH)+strlen(OBJ_DIR)+strlen(fname)+50,
			sizeof(char));

	sprintf(spath, "%s%s%s", SAVE_PATH, fname, OBJ_DIR);
	xosfile_create_dir(spath, 77);

        /* save stylesheets, ignoring the base sheet and <style> elements */
        LOG(("%d", c->data.html.stylesheet_count));
        for (i=2; i!=c->data.html.stylesheet_count; i++) {
                if (c->data.html.stylesheet_content[i] == 0) {
                        continue;
                }

                LOG(("'%s'", c->data.html.stylesheet_content[i]->url));
		/* TODO - the rest of this ;) */

//                ofname = get_filename(c->data.html.stylesheet_content[i]->url);
//                sprintf(spath, "%s%s%s.%s", SAVE_PATH, fname, OBJ_DIR, ofname);
                LOG(("'%s'", spath));
//                xosfile_save_stamped(spath, 0xf79, c->data.html.stylesheet_content[i]->data.css.data, c->data.html.stylesheet_content[i]->data.css.data + c->data.html.stylesheet_content[i]->data.css.length);

//                xfree(ofname);
        }

	/* save objects */
	for (i=0; i!=c->data.html.object_count; i++) {

		/* skip difficult content types */
		if (c->data.html.object[i].content->type >= CONTENT_PLUGIN) {
			continue;
		}

		ofname = get_filename(c->data.html.object[i].url);
		sprintf(spath, "%s%s%s.%s", SAVE_PATH, fname, OBJ_DIR, ofname);

		switch(c->data.html.object[i].content->type) {
			case CONTENT_HTML:
				xosfile_save_stamped(spath, 0xfaf, c->data.html.object[i].content->data.html.source, c->data.html.object[i].content->data.html.source + c->data.html.object[i].content->data.html.length);
				break;
/*
			case CONTENT_TEXTPLAIN:
				break;
			case CONTENT_CSS:
				xosfile_save_stamped(spath, 0xf79, c->data.html.object[i].content->data.css.data, c->data.html.object[i].content->data.css.data + c->data.html.object[i].content->data.css.length);
				break;
*/
			case CONTENT_JPEG:
				xosfile_save_stamped(spath, 0xc85, c->data.html.object[i].content->data.jpeg.data, (char*)c->data.html.object[i].content->data.jpeg.data + c->data.html.object[i].content->data.jpeg.length);
				break;
			case CONTENT_PNG:
				xosfile_save_stamped(spath, 0xb60, c->data.html.object[i].content->data.png.data, c->data.html.object[i].content->data.png.data + c->data.html.object[i].content->data.png.length);
				break;
			case CONTENT_GIF:
				xosfile_save_stamped(spath, 0x695, c->data.html.object[i].content->data.gif.data, c->data.html.object[i].content->data.gif.data + c->data.html.object[i].content->data.gif.length);
				break;
			case CONTENT_SPRITE:
				xosfile_save_stamped(spath, 0xff9, c->data.html.object[i].content->data.sprite.data, (char*)c->data.html.object[i].content->data.sprite.data + c->data.html.object[i].content->data.sprite.length);
				break;
			case CONTENT_DRAW:
				xosfile_save_stamped(spath, 0xaff, c->data.html.object[i].content->data.draw.data, (char*)c->data.html.object[i].content->data.draw.data + c->data.html.object[i].content->data.draw.length);
				break;
			default:
				break;
		}

		xfree(ofname);
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

char* get_filename(char * url) {

	char *ret = 0, *offs;
	uri_t *uri;

	uri = uri_alloc(url, (int)strlen(url));

	if (!uri) {
		return 0;
	}

	if (uri->path) {
	/* Two possible cases here:
	 * a) no page name given (eg http://www.blah.com/) -> index.html
	 * b) page name given
	 */
	 	/* case a */
	 	if (strlen(uri->path) == 0) {
	 		ret = xstrdup("index.html");
	 	}
	 	/* case b */
	 	else {
	 		offs = strrchr(uri->path, '/');
	 		if (!offs) {
	 			ret = xstrdup(uri->path);
	 		}
	 		else {
	 			ret = xstrdup(offs+1);
	 		}
	 	}
	}

	uri_free(uri);

	offs = xcalloc(strlen(ret)+1, sizeof(char));

	__riscosify(ret, 0, 0, offs, strlen(ret)+1, 0);

	xfree(ret);

	return offs;
}
#endif
