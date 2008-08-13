/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2007 Rob Kendrick <rjek@netsurf-browser.org>
 * Copyright 2007 Vincent Sanders <vince@debian.org>
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

#define __STDBOOL_H__	1
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <Mime.h>
#include <NodeInfo.h>
#include <String.h>

extern "C" {
#include "content/fetch.h"
#include "utils/log.h"
#include "utils/hashtable.h"
#include "utils/utils.h"
}

#include "beos/beos_filetype.h"

#warning REMOVEME
static struct hash_table *mime_hash = NULL;

void beos_fetch_filetype_init(const char *mimefile)
{
	BMessage mimes;
	status_t err;

	err = BMimeType::GetInstalledTypes(&mimes);
	if (err < B_OK) {
		warn_user("Mime", strerror(err));
		return;
	}

	mime_hash = hash_create(117);

	// just in case
	hash_add(mime_hash, "css", "text/css");
	hash_add(mime_hash, "htm", "text/html");
	hash_add(mime_hash, "html", "text/html");
	hash_add(mime_hash, "jpg", "image/jpeg");
	hash_add(mime_hash, "jpeg", "image/jpeg");
	hash_add(mime_hash, "gif", "image/gif");
	hash_add(mime_hash, "png", "image/png");
	hash_add(mime_hash, "jng", "image/jng");


	BString type;
	int i, j;
	//mimes.PrintToStream();
	for (i = 0; mimes.FindString("types", i, &type) >= B_OK; i++) {
		BMimeType mime(type.String());
		if (!mime.IsValid())
			continue;
		BMessage extensions;
		if (mime.GetFileExtensions(&extensions) < B_OK)
			continue;
		BString ext;
		for (j = 0; extensions.FindString("extentions", i, &ext) >= B_OK; i++) {
			hash_add(mime_hash, ext.String(), type.String());
		}
	}

#warning WRITEME
#if 0
	struct stat statbuf;
	FILE *fh = NULL;

	/* first, check to see if /etc/mime.types in preference */

	if ((stat("/etc/mime.types", &statbuf) == 0) &&
			S_ISREG(statbuf.st_mode)) {
		mimefile = "/etc/mime.types";

	}

	fh = fopen(mimefile, "r");

	/* Some OSes (mentioning no Solarises) have a worthlessly tiny
	 * /etc/mime.types that don't include essential things, so we
	 * pre-seed our hash with the essentials.  These will get
	 * over-ridden if they are mentioned in the mime.types file.
	 */

	hash_add(mime_hash, "css", "text/css");
	hash_add(mime_hash, "htm", "text/html");
	hash_add(mime_hash, "html", "text/html");
	hash_add(mime_hash, "jpg", "image/jpeg");
	hash_add(mime_hash, "jpeg", "image/jpeg");
	hash_add(mime_hash, "gif", "image/gif");
	hash_add(mime_hash, "png", "image/png");
	hash_add(mime_hash, "jng", "image/jng");

	if (fh == NULL) {
		LOG(("Unable to open a mime.types file, so using a minimal one for you."));
		return;
	}

	while (!feof(fh)) {
		char line[256], *ptr, *type, *ext;
		fgets(line, 256, fh);
		if (!feof(fh) && line[0] != '#') {
			ptr = line;

			/* search for the first non-whitespace character */
			while (isspace(*ptr))
				ptr++;

			/* is this line empty other than leading whitespace? */
			if (*ptr == '\n' || *ptr == '\0')
				continue;

			type = ptr;

			/* search for the first non-whitespace char or NUL or
			 * NL */
			while (*ptr && (!isspace(*ptr)) && *ptr != '\n')
				ptr++;

			if (*ptr == '\0' || *ptr == '\n') {
				/* this mimetype has no extensions - read next
				 * line.
				 */
				continue;
			}

			*ptr++ = '\0';

			/* search for the first non-whitespace character which
			 * will be the first filename extenion */
			while (isspace(*ptr))
				ptr++;

			while(true) {
				ext = ptr;

				/* search for the first whitespace char or
				 * NUL or NL which is the end of the ext.
				 */
				while (*ptr && (!isspace(*ptr)) &&
					*ptr != '\n')
					ptr++;

				if (*ptr == '\0' || *ptr == '\n') {
					/* special case for last extension on
					 * the line
					 */
					*ptr = '\0';
					hash_add(mime_hash, ext, type);
					break;
				}

				*ptr++ = '\0';
				hash_add(mime_hash, ext, type);

				/* search for the first non-whitespace char or
				 * NUL or NL, to find start of next ext.
				 */
				while (*ptr && (isspace(*ptr)) && *ptr != '\n')
					ptr++;
			}
		}
	}

	fclose(fh);
#endif
}

void beos_fetch_filetype_fin(void)
{
	hash_destroy(mime_hash);
}

const char *fetch_filetype(const char *unix_path)
{
	struct stat statbuf;
	status_t err;

	stat(unix_path, &statbuf);
	if (S_ISDIR(statbuf.st_mode))
		return "application/x-netsurf-directory";

	if (strchr(unix_path, '.') == NULL) {
		/* no extension anywhere! */
		return "text/plain";
	}

	// force some types
	const char *ext;
	ext = strrchr(unix_path, '.');
	if (!strcmp(ext, ".css"))
		return "text/css";
	if (!strcmp(ext, ".html"))
		return "text/html";
	if (!strcmp(ext, ".htm"))
		return "text/html";

	BNode node(unix_path);
	if (node.InitCheck() < B_OK) {
		warn_user("Mime", strerror(err));
		return "text/plain";
	}

	BNodeInfo info(&node);
	if (info.InitCheck() < B_OK) {
		warn_user("Mime", strerror(err));
		return "text/plain";
	}

	// NOT THREADSAFE
	static char type[B_MIME_TYPE_LENGTH];
	if (info.GetType(type) < B_OK) {
		// it might not have been sniffed yet...
		update_mime_info(unix_path, false, true, false);
		// try again
		if (info.GetType(type) < B_OK) {
			warn_user("Mime", strerror(err));
			return "text/plain";
		}
	}

	return type;

#warning WRITEME
#if 0
	struct stat statbuf;
	char *ext;
	const char *ptr;
	char *lowerchar;
	const char *type;

	stat(unix_path, &statbuf);
	if (S_ISDIR(statbuf.st_mode))
		return "application/x-netsurf-directory";

	if (strchr(unix_path, '.') == NULL) {
		/* no extension anywhere! */
		return "text/plain";
	}

	ptr = unix_path + strlen(unix_path);
	while (*ptr != '.' && *ptr != '/')
		ptr--;

	if (*ptr != '.')
		return "text/plain";

	ext = strdup(ptr + 1);	/* skip the . */

	/* the hash table only contains lower-case versions - make sure this
	 * copy is lower case too.
	 */
	lowerchar = ext;
	while(*lowerchar) {
		*lowerchar = tolower(*lowerchar);
		lowerchar++;
	}

	type = hash_get(mime_hash, ext);
	free(ext);

	return type != NULL ? type : "text/plain";
#endif
	return NULL;
}

char *fetch_mimetype(const char *unix_path)
{
	return strdup(fetch_filetype(unix_path));
}

#ifdef TEST_RIG

int main(int argc, char *argv[])
{
	unsigned int c1, *c2;
	const char *key;

	beos_fetch_filetype_init("./mime.types");

	c1 = 0; c2 = 0;

	while ( (key = hash_iterate(mime_hash, &c1, &c2)) != NULL) {
		printf("%s ", key);
	}

	printf("\n");

	if (argc > 1) {
		printf("%s maps to %s\n", argv[1], fetch_filetype(argv[1]));
	}

	beos_fetch_filetype_fin();
}

#endif
