/*
 * Copyright 2021 Vincent Sanders <vince@netsurf-browser.org>
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

/**
 * \file
 * Implementation of netsurf fetch for qt.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include <QResource>

extern "C" {

#include "utils/errors.h"
#include "utils/log.h"
#include "utils/filepath.h"
#include "utils/file.h"
#include "netsurf/fetch.h"

}

#include "qt/fetch.h"
#include "qt/resources.h"

/**
 * Determine the MIME type of a local file.
 *
 * @note used in file fetcher
 *
 * \param unix_path Unix style path to file on disk
 * \return Pointer to MIME type string (should not be freed) -
 *	   invalidated on next call to fetch_filetype.
 */
static const char *nsqt_fetch_filetype(const char *unix_path)
{
	int l;
	char * res = (char*)"text/html";
	l = strlen(unix_path);
	NSLOG(netsurf, INFO, "unix path: %s", unix_path);


	if (2 < l && strcasecmp(unix_path + l - 3, "f79") == 0)
		res = (char*)"text/css";
	else if (2 < l && strcasecmp(unix_path + l - 3, "css") == 0)
		res = (char*)"text/css";
	else if (2 < l && strcasecmp(unix_path + l - 3, "jpg") == 0)
		res = (char*)"image/jpeg";
	else if (3 < l && strcasecmp(unix_path + l - 4, "jpeg") == 0)
		res = (char*)"image/jpeg";
	else if (2 < l && strcasecmp(unix_path + l - 3, "gif") == 0)
		res = (char*)"image/gif";
	else if (2 < l && strcasecmp(unix_path + l - 3, "png") == 0)
		res = (char*)"image/png";
	else if (2 < l && strcasecmp(unix_path + l - 3, "jng") == 0)
		res = (char*)"image/jng";
	else if (2 < l && strcasecmp(unix_path + l - 3, "svg") == 0)
		res = (char*)"image/svg";
	else if (2 < l && strcasecmp(unix_path + l - 3, "txt") == 0)
		res = (char*)"text/plain";

	NSLOG(netsurf, INFO, "mime type: %s", res);
	return res;
}

/**
 * Translate resource path to full url.
 *
 * @note Only used in resource protocol fetcher
 *
 * Transforms a resource protocol path into a full URL. The returned URL
 * is used as the target for a redirect. The caller takes ownership of
 * the returned nsurl including unrefing it when finished with it.
 *
 * \param path The path of the resource to locate.
 * \return A netsurf url object containing the full URL of the resource path
 *           or NULL if a suitable resource URL can not be generated.
 */
static nsurl *nsqt_get_resource_url(const char *path)
{
	char buf[PATH_MAX];
	nsurl *url = NULL;

	netsurf_path_to_nsurl(filepath_sfind(respaths, buf, path), &url);

	return url;
}

static nserror
nsqt_get_resource_data(const char *resname,
		       const uint8_t **data_out,
		       size_t *data_size_out)
{
	QResource resource(resname);
	if (!resource.isValid()) {
		return NSERROR_NOT_FOUND;
	}
	QByteArray resource_data = resource.uncompressedData();
	qint64 size = resource_data.size();
	if (size < 1) {
		return NSERROR_NOT_FOUND;
	}
	uint8_t *data = (uint8_t *)malloc(size);
	if (data == NULL) {
		return NSERROR_NOT_FOUND;
	}

	memcpy(data, resource_data.data(), size);

	*data_out = data;
	*data_size_out = size;

	NSLOG(netsurf, DEBUG, "returning resource data for: %s", resname);
	return NSERROR_OK;
}

static nserror nsqt_release_resource_data(const uint8_t *data)
{
	free((uint8_t *)data);
	return NSERROR_OK;
}

static struct gui_fetch_table fetch_table = {
	.filetype = nsqt_fetch_filetype,

	.get_resource_url = nsqt_get_resource_url,
	.get_resource_data = nsqt_get_resource_data,
	.release_resource_data = nsqt_release_resource_data,
	.mimetype = NULL,
	.socket_open = NULL,
	.socket_close = NULL,
};

struct gui_fetch_table *nsqt_fetch_table = &fetch_table;
