/*
 * Copyright 2014 Vincent Sanders <vince@netsurf-browser.org>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#include "desktop/gui.h"
#include "utils/url.h"
#include "utils/nsurl.h"
#include "utils/filepath.h"

#include "monkey/filetype.h"
#include "monkey/fetch.h"

extern char **respaths;


static char *path_to_url(const char *path)
{
  int urllen;
  char *url;

  if (path == NULL) {
    return NULL;
  }

  urllen = strlen(path) + FILE_SCHEME_PREFIX_LEN + 1;

  url = malloc(urllen);
  if (url == NULL) {
    return NULL;
  }

  if (*path == '/') {
    path++; /* file: paths are already absolute */
  }

  snprintf(url, urllen, "%s%s", FILE_SCHEME_PREFIX, path);

  return url;
}

static char *url_to_path(const char *url)
{
  char *path;
  char *respath;
  nserror res; /* result from url routines */

  res = url_path(url, &path);
  if (res != NSERROR_OK) {
    return NULL;
  }

  res = url_unescape(path, &respath);
  free(path);
  if (res != NSERROR_OK) {
    return NULL;
  }

  return respath;
}

static nsurl *gui_get_resource_url(const char *path)
{
  char buf[PATH_MAX];
  char *raw;
  nsurl *url = NULL;

  raw = path_to_url(filepath_sfind(respaths, buf, path));
  if (raw != NULL) {
    nsurl_create(raw, &url);
    free(raw);
  }

  return url;
}

static struct gui_fetch_table fetch_table = {
  .filetype = monkey_fetch_filetype,
  .path_to_url = path_to_url,
  .url_to_path = url_to_path,

  .get_resource_url = gui_get_resource_url,
};

struct gui_fetch_table *monkey_fetch_table = &fetch_table;
