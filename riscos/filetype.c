/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include <stdlib.h>
#include <string.h>
#include <unixlib/local.h>
#include "oslib/osfile.h"
#include "netsurf/content/fetch.h"
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
 * filetype -- determine the MIME type of a local file
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


int cmp_type(const void *x, const void *y)
{
	const bits *p = x;
	const struct type_entry *q = y;
	return *p < q->file_type ? -1 : (*p == q->file_type ? 0 : +1);
}

