/*
 * Copyright 2011 Daniel Silverstone <dsilvers@digital-scurf.org>
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

#include "utils/ring.h"
#include "utils/nsurl.h"
#include "content/urldb.h"

#include "monkey/output.h"
#include "monkey/cert.h"

struct monkey_cert {
	struct monkey_cert *r_next, *r_prev;
	uint32_t num;
	nserror (*cb)(bool,void*);
	void *cbpw;
	nsurl *url;
};

static struct monkey_cert *cert_ring = NULL;
static uint32_t cert_ctr = 0;

nserror
gui_cert_verify(nsurl *url,
		const struct ssl_cert_info *certs,
		unsigned long num, nserror (*cb)(bool proceed, void *pw),
		void *cbpw)
{
	struct monkey_cert *mcrt_ctx;
	
	mcrt_ctx = calloc(sizeof(*mcrt_ctx), 1);
	if (mcrt_ctx == NULL) {
		return NSERROR_NOMEM;
	}

	mcrt_ctx->cb = cb;
	mcrt_ctx->cbpw = cbpw;
	mcrt_ctx->num = cert_ctr++;
	mcrt_ctx->url = nsurl_ref(url);
	
	RING_INSERT(cert_ring, mcrt_ctx);

	moutf(MOUT_SSLCERT, "VERIFY CWIN %u URL %s",
	      mcrt_ctx->num, nsurl_access(url));

	return NSERROR_OK;
}


static struct monkey_cert *
monkey_find_sslcert_by_num(uint32_t sslcert_num)
{
	struct monkey_cert *ret = NULL;

	RING_ITERATE_START(struct monkey_cert, cert_ring, c_ring) {
		if (c_ring->num == sslcert_num) {
			ret = c_ring;
			RING_ITERATE_STOP(cert_ring, c_ring);
		}
	} RING_ITERATE_END(cert_ring, c_ring);

	return ret;
}

static void free_sslcert_context(struct monkey_cert *mcrt_ctx) {
	moutf(MOUT_SSLCERT, "DESTROY CWIN %u", mcrt_ctx->num);
	RING_REMOVE(cert_ring, mcrt_ctx);
	if (mcrt_ctx->url) {
		nsurl_unref(mcrt_ctx->url);
	}
	free(mcrt_ctx);
}

static void
monkey_sslcert_handle_go(int argc, char **argv)
{
	struct monkey_cert *mcrt_ctx;

	if (argc != 3) {
		moutf(MOUT_ERROR, "SSLCERT GO ARGS BAD");
		return;
	}

	mcrt_ctx = monkey_find_sslcert_by_num(atoi(argv[2]));
	if (mcrt_ctx == NULL) {
		moutf(MOUT_ERROR, "SSLCERT NUM BAD");
		return;
	}

	urldb_set_cert_permissions(mcrt_ctx->url, true);

	mcrt_ctx->cb(true, mcrt_ctx->cbpw);

	free_sslcert_context(mcrt_ctx);
}

static void
monkey_sslcert_handle_destroy(int argc, char **argv)
{
	struct monkey_cert *mcrt_ctx;

	if (argc != 3) {
		moutf(MOUT_ERROR, "SSLCERT DESTROY ARGS BAD");
		return;
	}

	mcrt_ctx = monkey_find_sslcert_by_num(atoi(argv[2]));
	if (mcrt_ctx == NULL) {
		moutf(MOUT_ERROR, "SSLCERT NUM BAD");
		return;
	}

	mcrt_ctx->cb(false, mcrt_ctx->cbpw);

	free_sslcert_context(mcrt_ctx);
}

void
monkey_sslcert_handle_command(int argc, char **argv)
{
	if (argc == 1)
		return;

	if (strcmp(argv[1], "DESTROY") == 0) {
		monkey_sslcert_handle_destroy(argc, argv);
	} else if (strcmp(argv[1], "GO") == 0) {
		monkey_sslcert_handle_go(argc, argv);
	} else {
		moutf(MOUT_ERROR, "SSLCERT COMMAND UNKNOWN %s", argv[1]);
	}
}
