/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#include <stdlib.h>
#include <string.h>
#include <unixlib/local.h>
#include "oslib/mimemap.h"
#include "oslib/osfile.h"
#include "netsurf/content/content.h"
#include "netsurf/content/fetch.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

/* type_map must be in sorted order by file_type */
struct type_entry {
	bits file_type;
	char mime_type[40];
};
static const struct type_entry type_map[] = {
        {0x188, "application/x-shockwave-flash"},
	{0x695, "image/gif"},
	{0xaff, "image/x-drawfile"},
	{0xb60, "image/png"},
	{0xc85, "image/jpeg"},
	{0xf79, "text/css"},
	{0xfaf, "text/html"},
	{0xff9, "image/x-riscos-sprite"},
	{0xfff, "text/plain"},
};
#define TYPE_MAP_COUNT (sizeof(type_map) / sizeof(type_map[0]))


static int cmp_type(const void *x, const void *y);


/**
 * Determine the MIME type of a local file.
 */

const char *fetch_filetype(const char *unix_path)
{
	struct type_entry *t;
	unsigned int len = strlen(unix_path) + 100;
	char *path = xcalloc(len, 1);
	char *r;
	os_error *error;
	bits file_type;

	LOG(("unix_path = '%s'", unix_path));

	/* convert path to RISC OS format and read file type */
	r = __riscosify(unix_path, 0, 0, path, len, 0);
	if (r == 0) {
		LOG(("__riscosify failed"));
		return "application/riscos";
	}
	LOG(("riscos path '%s'", path));

	error = xosfile_read_stamped_no_path(path, 0, 0, 0, 0, 0, &file_type);
	if (error != 0) {
		LOG(("xosfile_read_stamped_no_path failed: %s", error->errmess));
		return "application/riscos";
	}

	/* search for MIME type */
	t = bsearch(&file_type, type_map, TYPE_MAP_COUNT, sizeof(type_map[0]), cmp_type);
	if (t == 0)
		return "application/riscos";
	LOG(("mime type '%s'", t->mime_type));
	return t->mime_type;
}


char *fetch_mimetype(const char *ro_path) {

        os_error *e;
        bits filetype = 0, load;
        int objtype;
        char *mime = xcalloc(256, sizeof(char));

        e = xosfile_read_no_path(ro_path, &objtype, &load, 0, 0, 0);
        if (e) return 0;

        if (objtype == 0x2) return 0; /* directories are pointless */

        if ((load >> 20) & 0xFFF) {
                filetype = (load>>8) & 0x000FFF;
        }
        else {
                return 0; /* no idea */
        }

        e = xmimemaptranslate_filetype_to_mime_type(filetype, mime);
        if (e) return 0;

        return mime;
}


int cmp_type(const void *x, const void *y)
{
	const bits *p = x;
	const struct type_entry *q = y;
	return *p < q->file_type ? -1 : (*p == q->file_type ? 0 : +1);
}


/**
 * Determine the RISC OS filetype for a content.
 */

int ro_content_filetype(struct content *content)
{
	int file_type;
	os_error *error;

	switch (content->type) {
		case CONTENT_HTML:	return 0xfaf;
		case CONTENT_TEXTPLAIN:	return 0xfff;
		case CONTENT_CSS:	return 0xf79;
		case CONTENT_JPEG:	return 0xc85;
		case CONTENT_PNG:	return 0xb60;
		case CONTENT_GIF:	return 0x695;
		case CONTENT_SPRITE:	return 0xff9;
		case CONTENT_DRAW:	return 0xaff;
		default:		break;
	}

	error = xmimemaptranslate_mime_type_to_filetype(content->mime_type,
			&file_type);
	if (error)
		return 0xffd;
	return file_type;
}
