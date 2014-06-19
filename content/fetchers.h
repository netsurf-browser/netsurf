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

/** \file content/fetchers.h
 * Interface for fetchers factory.
 */

#ifndef _NETSURF_DESKTOP_FETCHERS_H_
#define _NETSURF_DESKTOP_FETCHERS_H_

#include <libwapcaplet/libwapcaplet.h>

struct nsurl;
struct fetch_multipart_data;
struct fetch;

extern bool fetch_active;

/**
 * Fetcher operations API
 *
 * These are the operations a fetcher must implement.
 */
struct fetcher_operation_table {
	/**
	 * The initialiser for the fetcher.
	 *
	 * Called once to initialise the fetcher.
	 */
	bool (*initialise)(lwc_string *scheme);

	/**
	 * can this fetcher accept a url.
	 *
	 * \param url the URL to check
	 * \return true if the fetcher can handle the url else false.
	 */
	bool (*acceptable)(const struct nsurl *url);

	/**
	 * Setup a fetch
	 */
	void *(*setup)(struct fetch *parent_fetch, struct nsurl *url,
		bool only_2xx, bool downgrade_tls, const char *post_urlenc,
		const struct fetch_multipart_data *post_multipart,
		const char **headers);

	/**
	 * start a fetch.
	 */
	bool (*start)(void *fetch);

	/**
	 * abort a fetch.
	 */
	void (*abort)(void *fetch);

	/**
	 * free a fetch allocated through the setup method.
	 */
	void (*free)(void *fetch);

	/**
	 * poll a fetcher to let it make progress.
	 */
	void (*poll)(lwc_string *scheme);

	/**
	 * finalise the fetcher.
	 */
	void (*finalise)(lwc_string *scheme);
};

/**
 * Register a fetcher for a scheme
 *
 * \param scheme The scheme fetcher is for (caller relinquishes ownership)
 * \param ops The operations for the fetcher.
 * \return NSERROR_OK or appropriate error code.
 */
nserror fetcher_add(lwc_string *scheme, const struct fetcher_operation_table *ops);

/**
 * Initialise the fetchers.
 *
 * @return NSERROR_OK or error code
 */
nserror fetcher_init(void);

/**
 * Clean up for quit.
 *
 * Must be called before exiting.
 */
void fetcher_quit(void);

/**
 * Do some work on current fetches.
 *
 * Must be called regularly to make progress on fetches.
 */
void fetcher_poll(void);

#endif
