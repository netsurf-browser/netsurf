/*
 * Copyright 2020 Vincent Sanders <vince@netsurf-browser.org>
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
 * helpers for X509 certificate chains
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "utils/errors.h"
#include "utils/log.h"

#include "netsurf/ssl_certs.h"

/*
 * create new certificate chain
 *
 * exported interface documented in netsurf/ssl_certs.h
 */
nserror
cert_chain_alloc(size_t depth, struct cert_chain **chain_out)
{
	struct cert_chain* chain;

	chain = calloc(1, sizeof(struct cert_chain));
	if (chain == NULL) {
		return NSERROR_NOMEM;
	}

	chain->depth = depth;

	*chain_out = chain;

	return NSERROR_OK;
}


/*
 * duplicate certificate chain
 *
 * exported interface documented in netsurf/ssl_certs.h
 */
nserror
cert_chain_dup(const struct cert_chain *src, struct cert_chain **dst_out)
{
	struct cert_chain* dst;
	size_t depth;
	nserror res;

	res = cert_chain_alloc(src->depth, &dst);
	if (res != NSERROR_OK) {
		return res;
	}

	for (depth = 0; depth < src->depth; depth++) {
		dst->certs[depth].err = src->certs[depth].err;
		dst->certs[depth].der_length = src->certs[depth].der_length;
		if (src->certs[depth].der != NULL) {
			dst->certs[depth].der = malloc(src->certs[depth].der_length);
			if (dst->certs[depth].der == NULL) {
				cert_chain_free(dst);
				return NSERROR_NOMEM;
			}
			memcpy(dst->certs[depth].der,
			       src->certs[depth].der,
			       src->certs[depth].der_length);
		}

	}

	*dst_out = dst;
	return NSERROR_OK;
}


/*
 * free certificate chain
 *
 * exported interface documented in netsurf/ssl_certs.h
 */
nserror cert_chain_free(struct cert_chain* chain)
{
	size_t depth;

	if (chain != NULL) {
		for (depth = 0; depth < chain->depth; depth++) {
			if (chain->certs[depth].der != NULL) {
				free(chain->certs[depth].der);
			}
		}

		free(chain);
	}

	return NSERROR_OK;
}


/*
 * calculate storage used of certificate chain
 *
 * exported interface documented in netsurf/ssl_certs.h
 */
size_t cert_chain_size(const struct cert_chain *chain)
{
	size_t size = 0;
	size_t depth;

	if (chain != NULL) {
		size += sizeof(struct cert_chain);

		for (depth = 0; depth < chain->depth; depth++) {
			if (chain->certs[depth].der != NULL) {
				size += chain->certs[depth].der_length;
			}
		}
	}

	return size;
}
