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
#include "netsurf/utils/config.h"
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
	{0xf78, "image/jng"},
	{0xf79, "text/css"},
	{0xf83, "image/mng"},
	{0xfaf, "text/html"},
	{0xff9, "image/x-riscos-sprite"},
	{0xfff, "text/plain"},
};
#define TYPE_MAP_COUNT (sizeof(type_map) / sizeof(type_map[0]))

#define BUF_SIZE (256)
static char type_buf[BUF_SIZE];


static int cmp_type(const void *x, const void *y);

/**
 * Determine the MIME type of a local file.
 *
 * \param unix_path Unix style path to file on disk
 * \return Pointer to MIME type string (should not be freed) - invalidated
 *         on next call to fetch_filetype.
 */
const char *fetch_filetype(const char *unix_path)
{
	struct type_entry *t;
	unsigned int len = strlen(unix_path) + 100;
	char *path = calloc(len, 1);
	char *r, *slash;
	os_error *error;
	bits file_type, temp;

	if (!path) {
		LOG(("Insufficient memory for calloc"));
		warn_user("NoMemory", 0);
		return "application/riscos";
	}
	LOG(("unix_path = '%s'", unix_path));

	/* convert path to RISC OS format and read file type */
	r = __riscosify(unix_path, 0, __RISCOSIFY_NO_SUFFIX, path, len, 0);
	if (r == 0) {
		LOG(("__riscosify failed"));
		free(path);
		return "application/riscos";
	}
	LOG(("riscos path '%s'", path));

	error = xosfile_read_stamped_no_path(path, 0, 0, 0, 0, 0, &file_type);
	if (error) {
		LOG(("xosfile_read_stamped_no_path failed: %s",
				error->errmess));
		free(path);
		return "application/riscos";
	}

	/* If filetype is text and the file has an extension, try to map the
	 * extension to a filetype via the MimeMap file. */
	if (file_type == osfile_TYPE_TEXT) {
		slash = strrchr(path, '/');
		if (slash) {
			error = xmimemaptranslate_extension_to_filetype(
					slash+1, &temp);
			if (error)
				/* ignore error and leave file_type alone */
				LOG(("0x%x %s",
					error->errnum, error->errmess));
			else
				file_type = temp;
		}
	}

	/* search for MIME type in our internal table */
	t = bsearch(&file_type, type_map, TYPE_MAP_COUNT,
				sizeof(type_map[0]), cmp_type);
	if (t) {
		/* found, so return it */
		free(path);
		return t->mime_type;
	}

	/* not in internal table, so ask MimeMap */
	error = xmimemaptranslate_filetype_to_mime_type(file_type, type_buf);
	if (error) {
		LOG(("0x%x %s", error->errnum, error->errmess));
		free(path);
		return "application/riscos";
	}
	/* make sure we're NULL terminated. If we're not, the MimeMap
	 * module's probably written past the end of the buffer from
	 * SVC mode. Short of rewriting MimeMap with an incompatible API,
	 * there's nothing we can do about it.
	 */
	type_buf[BUF_SIZE - 1] = '\0';

	free(path);

	LOG(("mime type '%s'", type_buf));
	return (const char *)type_buf;

}

/**
 * Find a MIME type for a local file
 *
 * \param ro_path RISC OS style path to file on disk
 * \return MIME type string (on heap, caller should free), or NULL
 */
char *fetch_mimetype(const char *ro_path)
{
	os_error *e;
	bits filetype = 0, load;
	int objtype;
	char *mime = calloc(BUF_SIZE, sizeof(char));
	char *slash;

	if (!mime) {
		LOG(("Insufficient memory for calloc"));
		warn_user("NoMemory", 0);
		return 0;
	}

	e = xosfile_read_no_path(ro_path, &objtype, &load, 0, 0, 0);
	if (e)
		return 0;

	if (objtype == osfile_IS_DIR)
		return 0; /* directories are pointless */

	if ((load >> 20) & 0xFFF) {
		filetype = (load>>8) & 0x000FFF;
	}
	else {
		return 0; /* no idea */
	}

	/* If filetype is text and the file has an extension, try to map the
	 * extension to a filetype via the MimeMap file. */
	slash = strrchr(ro_path, '/');
	if (slash && filetype == osfile_TYPE_TEXT) {
		e = xmimemaptranslate_extension_to_filetype(slash+1, &load);
		if (e)
			/* if we get an error here, simply ignore it and
			 * leave filetype unchanged */
			LOG(("0x%x %s", e->errnum, e->errmess));
		else
			filetype = load;
	}

	e = xmimemaptranslate_filetype_to_mime_type(filetype, mime);
	if (e)
		return 0;
	/* make sure we're NULL terminated. If we're not, the MimeMap
	 * module's probably written past the end of the buffer from
	 * SVC mode. Short of rewriting MimeMap with an incompatible API,
	 * there's nothing we can do about it.
	 */
	mime[BUF_SIZE - 1] = '\0';

	return mime;
}

/**
 * Comparison function for bsearch
 */
int cmp_type(const void *x, const void *y)
{
	const bits *p = x;
	const struct type_entry *q = y;
	return *p < q->file_type ? -1 : (*p == q->file_type ? 0 : +1);
}

/**
 * Determine the RISC OS filetype for a content.
 *
 * \param content The content to examine.
 * \return The RISC OS filetype corresponding to the content
 */
int ro_content_filetype(struct content *content)
{
	int file_type;
	os_error *error;

	switch (content->type) {
		case CONTENT_HTML:	return 0xfaf;
		case CONTENT_TEXTPLAIN:	return 0xfff;
		case CONTENT_CSS:	return 0xf79;
#ifdef WITH_MNG
		case CONTENT_JNG:	return 0xf78;
		case CONTENT_MNG:	return 0xf84;
#endif
#ifdef WITH_JPEG
		case CONTENT_JPEG:	return 0xc85;
#endif
#ifdef WITH_PNG
		case CONTENT_PNG:	return 0xb60;
#endif
#ifdef WITH_GIF
		case CONTENT_GIF:	return 0x695;
#endif
#ifdef WITH_SPRITE
		case CONTENT_SPRITE:	return 0xff9;
#endif
#ifdef WITH_DRAW
		case CONTENT_DRAW:	return 0xaff;
#endif
		default:		break;
	}

	error = xmimemaptranslate_mime_type_to_filetype(content->mime_type,
			&file_type);
	if (error)
		return 0xffd;
	return file_type;
}
