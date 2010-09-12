/*
 * Copyright 2010 Vincent Sanders <vince@netsurf-browser.org>
 *
 * This file is part of NetSurf.
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

/* file: URL handling. Based on the data fetcher by Rob Kendrick */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <stdarg.h>

#include "utils/config.h"
#include "content/dirlist.h"
#include "content/fetch.h"
#include "content/fetchers/fetch_file.h"
#include "content/urldb.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/ring.h"

/* Maximum size of read buffer */
#define FETCH_FILE_MAX_BUF_SIZE (1024 * 1024)

/** Context for a fetch */
struct fetch_file_context {
	struct fetch_file_context *r_next, *r_prev;

	struct fetch *fetchh; /**< Handle for this fetch */

	bool aborted; /**< Flag indicating fetch has been aborted */
	bool locked; /**< Flag indicating entry is already entered */

	char *url; /**< The full url the fetch refers to */
	char *path; /**< The actual path to be used with open() */
};

static struct fetch_file_context *ring = NULL;

/** issue fetch callbacks with locking */
static inline bool fetch_file_send_callback(fetch_msg msg,
		struct fetch_file_context *ctx, const void *data,
		unsigned long size, fetch_error_code errorcode)
{
	ctx->locked = true;
	fetch_send_callback(msg, ctx->fetchh, data, size, errorcode);
	ctx->locked = false;

	return ctx->aborted;
}

static bool fetch_file_send_header(struct fetch_file_context *ctx, const char *fmt, ...)
{
	char header[64];
	va_list ap;

	va_start(ap, fmt);

	vsnprintf(header, sizeof header, fmt, ap);

	va_end(ap);

	fetch_file_send_callback(FETCH_HEADER, ctx, header, strlen(header), FETCH_ERROR_NO_ERROR);

	return ctx->aborted;
}

static bool fetch_file_send_time(struct fetch_file_context *ctx, const char *fmt, const time_t *val)
{
	char header[64];
	struct tm btm;

	gmtime_r(val, &btm);

	strftime(header, sizeof header, fmt, &btm);

	fetch_file_send_callback(FETCH_HEADER, ctx, header, strlen(header), FETCH_ERROR_NO_ERROR);

	return ctx->aborted;
}

/** callback to initialise the file fetcher. */
static bool fetch_file_initialise(const char *scheme)
{
	return true;
}

/** callback to initialise the file fetcher. */
static void fetch_file_finalise(const char *scheme)
{
}

/** callback to set up a file fetch context. */
static void *
fetch_file_setup(struct fetch *fetchh,
		 const char *url,
		 bool only_2xx,
		 const char *post_urlenc,
		 const struct fetch_multipart_data *post_multipart,
		 const char **headers)
{
	struct fetch_file_context *ctx;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		return NULL;

	ctx->path = url_to_path(url);
	if (ctx->path == NULL) {
		free(ctx);
		return NULL;
	}

	ctx->url = strdup(url);
	if (ctx->url == NULL) {
		free(ctx->path);
		free(ctx);
		return NULL;
	}

	ctx->fetchh = fetchh;

	RING_INSERT(ring, ctx);

	return ctx;
}

/** callback to free a file fetch */
static void fetch_file_free(void *ctx)
{
	struct fetch_file_context *c = ctx;
	free(c->url);
	free(c->path);
	RING_REMOVE(ring, c);
	free(ctx);
}

/** callback to start a file fetch */
static bool fetch_file_start(void *ctx)
{
	return true;
}

/** callback to abort a file fetch */
static void fetch_file_abort(void *ctx)
{
	struct fetch_file_context *c = ctx;

	/* To avoid the poll loop having to deal with the fetch context
	 * disappearing from under it, we simply flag the abort here.
	 * The poll loop itself will perform the appropriate cleanup.
	 */
	c->aborted = true;
}

static void fetch_file_process_error(struct fetch_file_context *ctx, int code)
{
	char buffer[1024];
	const char *title;
	char key[8];

	/* content is going to return error code */
	fetch_set_http_code(ctx->fetchh, code);

	/* content type */
	if (fetch_file_send_header(ctx, "Content-Type: text/html"))
		goto fetch_file_process_error_aborted;

	snprintf(key, sizeof key, "HTTP%03d", code);
	title = messages_get(key);

	snprintf(buffer, sizeof buffer, "<html><head><title>%s</title></head><body><h1>%s</h1><p>Error %d while fetching file %s</p></body></html>", title, title, code,ctx->url);

	if (fetch_file_send_callback(FETCH_DATA, ctx, buffer, strlen(buffer), FETCH_ERROR_NO_ERROR))
		goto fetch_file_process_error_aborted;

	fetch_file_send_callback(FETCH_FINISHED, ctx, 0, 0, FETCH_ERROR_NO_ERROR);

fetch_file_process_error_aborted:
	return;
}


/** Process object as a regular file */
static void fetch_file_process_plain(struct fetch_file_context *ctx,
				     struct stat *fdstat)
{
	char *buf;
	size_t buf_size;

	ssize_t tot_read = 0;
	ssize_t res;

	int fd; /**< The file descriptor of the object */
	fd = open(ctx->path, O_RDONLY);
	if (fd < 0) {
		/* process errors as appropriate */
		switch (errno) {
		case EACCES:
			fetch_file_process_error(ctx, 403);
			break;

		case ENOENT:
			fetch_file_process_error(ctx, 404);
			break;

		default:
			fetch_file_process_error(ctx, 500);
			break;
		}
		return;
	}

	/* set buffer size */
	buf_size = fdstat->st_size;
	if (buf_size > FETCH_FILE_MAX_BUF_SIZE)
		buf_size = FETCH_FILE_MAX_BUF_SIZE;

	/* allocate the buffer storage */
	buf = malloc(buf_size);
	if (buf == NULL) {
		fetch_file_send_callback(FETCH_ERROR, ctx,
			"Unable to allocate memory for file data buffer",
			0, FETCH_ERROR_MEMORY);
		return;
	}

	/* fetch is going to be successful */
	fetch_set_http_code(ctx->fetchh, 200);

	/* Any callback can result in the fetch being aborted.
	 * Therefore, we _must_ check for this after _every_ call to
	 * fetch_file_send_callback().
	 */

	/* content type */
	if (fetch_file_send_header(ctx, "Content-Type: %s", fetch_filetype(ctx->path)))
		goto fetch_file_process_aborted;

	/* content length */
	if (fetch_file_send_header(ctx, "Content-Length: %zd", fdstat->st_size))
		goto fetch_file_process_aborted;

	/* Set Last modified header */
	if (fetch_file_send_time(ctx, "Last-Modified: %a, %d %b %Y %H:%M:%S GMT", &fdstat->st_mtime))
		goto fetch_file_process_aborted;

	/* create etag */
	if (fetch_file_send_header(ctx, "ETag: \"%10" PRId64 "\"", (int64_t) fdstat->st_mtime))
		goto fetch_file_process_aborted;

	/* main data loop */
	do {
		res = read(fd, buf, buf_size);
		if (res == -1) {
			fetch_file_send_callback(FETCH_ERROR, ctx, "Error reading file", 0, FETCH_ERROR_PARTIAL_FILE);
			goto fetch_file_process_aborted;
		}

		if (res == 0) {
			fetch_file_send_callback(FETCH_ERROR, ctx, "Unexpected EOF reading file", 0, FETCH_ERROR_PARTIAL_FILE);
			goto fetch_file_process_aborted;
		}

		tot_read += res;

		if (fetch_file_send_callback(FETCH_DATA, ctx, buf, res, FETCH_ERROR_NO_ERROR))
			break;

	} while (tot_read < fdstat->st_size);

	if (!ctx->aborted)
		fetch_file_send_callback(FETCH_FINISHED, ctx, 0, 0, FETCH_ERROR_NO_ERROR);

fetch_file_process_aborted:

	close(fd);
	free(buf);
	return;
}

static char *gen_nice_title(char *path)
{
	char *nice_path, *cnv, *tmp;
	char *title;
	int title_length;

	/* Convert path for display */
	nice_path = malloc(strlen(path) * SLEN("&amp;") + 1);
	if (nice_path == NULL) {
		return NULL;
	}

	/* Escape special HTML characters */
	for (cnv = nice_path, tmp = path; *tmp != '\0'; tmp++) {
		if (*tmp == '<') {
			*cnv++ = '&';
			*cnv++ = 'l';
			*cnv++ = 't';
			*cnv++ = ';';
		} else if (*tmp == '>') {
			*cnv++ = '&';
			*cnv++ = 'g';
			*cnv++ = 't';
			*cnv++ = ';';
		} else if (*tmp == '&') {
			*cnv++ = '&';
			*cnv++ = 'a';
			*cnv++ = 'm';
			*cnv++ = 'p';
			*cnv++ = ';';
		} else {
			*cnv++ = *tmp;
		}
	}
	*cnv = '\0';

	/* Construct a localised title string */
	title_length = (cnv - nice_path) + strlen(messages_get("FileIndex"));
	title = malloc(title_length + 1);

	if (title == NULL) {
		free(nice_path);
		return NULL;
	}

	/* Set title to localised "Index of <nice_path>" */
	snprintf(title, title_length, messages_get("FileIndex"), nice_path);

	free(nice_path);

	return title;
}


static void fetch_file_process_dir(struct fetch_file_context *ctx,
				   struct stat *fdstat)
{
	char buffer[1024]; /* Output buffer */
	bool even = false; /* formatting flag */
	char *title; /* pretty printed title */
	url_func_result res; /* result from url routines */
	char *up; /* url of parent */
	char *path; /* url for list entries */
	bool compare; /* result of url compare */

	DIR *scandir; /* handle for enumerating the directory */
	struct dirent* ent; /* leaf directory entry */
	struct stat ent_stat; /* stat result of leaf entry */
	char datebuf[64]; /* buffer for date text */
	char timebuf[64]; /* buffer for time text */
	char urlpath[PATH_MAX]; /* buffer for leaf entry path */

	scandir = opendir(ctx->path);
	if (scandir == NULL) {
		fetch_file_process_error(ctx, 500);
		return;
	}

	/* fetch is going to be successful */
	fetch_set_http_code(ctx->fetchh, 200);

	/* content type */
	if (fetch_file_send_header(ctx, "Content-Type: text/html"))
		goto fetch_file_process_dir_aborted;

	/* directory listing top */
	dirlist_generate_top(buffer, sizeof buffer);
	if (fetch_file_send_callback(FETCH_DATA, ctx, buffer, strlen(buffer), FETCH_ERROR_NO_ERROR))
		goto fetch_file_process_dir_aborted;

	/* directory listing title */
	title = gen_nice_title(ctx->path);
	dirlist_generate_title(title, buffer, sizeof buffer);
	free(title);
	if (fetch_file_send_callback(FETCH_DATA, ctx, buffer, strlen(buffer), FETCH_ERROR_NO_ERROR))
		goto fetch_file_process_dir_aborted;

	/* Print parent directory link */
	res = url_parent(ctx->url, &up);
	if (res == URL_FUNC_OK) {
		res = url_compare(ctx->url, up, false, &compare);
		if ((res == URL_FUNC_OK) && !compare) {
			dirlist_generate_parent_link(up, buffer, sizeof buffer);

			fetch_file_send_callback(FETCH_DATA, ctx,
						 buffer,
						 strlen(buffer),
						 FETCH_ERROR_NO_ERROR);

		}
		free(up);

		if (ctx->aborted)
			goto fetch_file_process_dir_aborted;

	}

	/* directory list headings */
	dirlist_generate_headings(buffer, sizeof buffer);
	if (fetch_file_send_callback(FETCH_DATA, ctx, buffer, strlen(buffer), FETCH_ERROR_NO_ERROR))
		goto fetch_file_process_dir_aborted;

	while ((ent = readdir(scandir)) != NULL) {

		if (ent->d_name[0] == '.')
			continue;

		strncpy(urlpath, ctx->path, sizeof urlpath);
		if (path_add_part(urlpath, sizeof urlpath, ent->d_name) == false)
			continue;

		if (stat(urlpath, &ent_stat) != 0) {
			ent_stat.st_mode = 0;
			datebuf[0] = 0;
			timebuf[0] = 0;
		} else {
			/* Get date in output format */
			if (strftime((char *)&datebuf, sizeof datebuf,
				     "%a %d %b %Y",
				     localtime(&ent_stat.st_mtime)) == 0) {
				strncpy(datebuf, "-", sizeof datebuf);
			}

			/* Get time in output format */
			if (strftime((char *)&timebuf, sizeof timebuf,
				     "%H:%M",
				     localtime(&ent_stat.st_mtime)) == 0) {
				strncpy(timebuf, "-", sizeof timebuf);
			}
		}

		if((path = path_to_url(urlpath)) == NULL)
			continue;

		if (S_ISREG(ent_stat.st_mode)) {
			/* regular file */
			dirlist_generate_row(even,
					     false,
					     path,
					     ent->d_name,
					     fetch_filetype(urlpath),
					     ent_stat.st_size,
					     datebuf, timebuf,
					     buffer, sizeof(buffer));
		} else if (S_ISDIR(ent_stat.st_mode)) {
			/* directory */
			dirlist_generate_row(even,
					     true,
					     path,
					     ent->d_name,
					     messages_get("FileDirectory"),
					     -1,
					     datebuf, timebuf,
					     buffer, sizeof(buffer));
		} else {
			/* something else */
			dirlist_generate_row(even,
					     false,
					     path,
					     ent->d_name,
					     "",
					     -1,
					     datebuf, timebuf,
					     buffer, sizeof(buffer));
		}

		free(path);

		if (fetch_file_send_callback(FETCH_DATA, ctx,
					     buffer,
					     strlen(buffer),
					     FETCH_ERROR_NO_ERROR))
			goto fetch_file_process_dir_aborted;

		even = !even;
	}

	/* directory listing bottom */
	dirlist_generate_bottom(buffer, sizeof buffer);
	if (fetch_file_send_callback(FETCH_DATA, ctx, buffer, strlen(buffer), FETCH_ERROR_NO_ERROR))
		goto fetch_file_process_dir_aborted;


	fetch_file_send_callback(FETCH_FINISHED, ctx, 0, 0, FETCH_ERROR_NO_ERROR);

fetch_file_process_dir_aborted:

	closedir(scandir);
}


/* process a file fetch */
static void fetch_file_process(struct fetch_file_context *ctx)
{
	struct stat fdstat; /**< The objects stat */

	if (stat(ctx->path, &fdstat) != 0) {
		/* process errors as appropriate */
		fetch_file_process_error(ctx, 500);
		return;
	}

	if (S_ISDIR(fdstat.st_mode)) {
		/* directory listing */
		fetch_file_process_dir(ctx, &fdstat);
		return;
	} else if (S_ISREG(fdstat.st_mode)) {
		/* regular file */
		fetch_file_process_plain(ctx, &fdstat);
		return;
	} else {
		/* unhandled type of file */
		fetch_file_process_error(ctx, 501);
	}

	return;
}

/** callback to poll for additional file fetch contents */
static void fetch_file_poll(const char *scheme)
{
	struct fetch_file_context *c, *next;

	if (ring == NULL) return;

	/* Iterate over ring, processing each pending fetch */
	c = ring;
	do {
		/* Take a copy of the next pointer as we may destroy
		 * the ring item we're currently processing */
		next = c->r_next;

		/* Ignore fetches that have been flagged as locked.
		 * This allows safe re-entrant calls to this function.
		 * Re-entrancy can occur if, as a result of a callback,
		 * the interested party causes fetch_poll() to be called
		 * again.
		 */
		if (c->locked == true) {
			continue;
		}

		/* Only process non-aborted fetches */
		if (!c->aborted) {
			/* file fetches can be processed in one go */
			fetch_file_process(c);
		}


		fetch_remove_from_queues(c->fetchh);
		fetch_free(c->fetchh);

		/* Advance to next ring entry, exiting if we've reached
		 * the start of the ring or the ring has become empty
		 */
	} while ( (c = next) != ring && ring != NULL);
}

void fetch_file_register(void)
{
	fetch_add_fetcher("file",
		fetch_file_initialise,
		fetch_file_setup,
		fetch_file_start,
		fetch_file_abort,
		fetch_file_free,
		fetch_file_poll,
		fetch_file_finalise);
}
