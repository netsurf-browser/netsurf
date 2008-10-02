/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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

void beos_fetch_filetype_init(void)
{
#if 0
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
#endif
}

void beos_fetch_filetype_fin(void)
{
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
}

char *fetch_mimetype(const char *unix_path)
{
	return strdup(fetch_filetype(unix_path));
}

