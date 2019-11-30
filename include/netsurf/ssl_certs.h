/*
 * Copyright 2019 Daniel Silverstone <dsilvers@netsurf-browser.org>
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
 *
 * SSL related types and values
 */

#ifndef NETSURF_SSL_CERTS_H_
#define NETSURF_SSL_CERTS_H_

/**
 * ssl certificate error status
 *
 * Do not reorder / remove entries because these may be persisted to the disk
 * cache as simple ints.
 */
typedef enum {
	SSL_CERT_ERR_OK,	/**< Nothing wrong with this certificate */
	SSL_CERT_ERR_UNKNOWN,	/**< Unknown error */
	SSL_CERT_ERR_BAD_ISSUER, /**< Bad issuer */
	SSL_CERT_ERR_BAD_SIG,	/**< Bad signature on this certificate */
	SSL_CERT_ERR_TOO_YOUNG,	/**< This certificate is not yet valid */
	SSL_CERT_ERR_TOO_OLD,	/**< This certificate is no longer valid */
	SSL_CERT_ERR_SELF_SIGNED, /**< This certificate (or the chain) is self signed */
	SSL_CERT_ERR_CHAIN_SELF_SIGNED, /**< This certificate chain is self signed */
	SSL_CERT_ERR_REVOKED,	/**< This certificate has been revoked */
	SSL_CERT_ERR_HOSTNAME_MISMATCH, /**< This certificate host did not match the server */
} ssl_cert_err;

/** Always the max known ssl certificate error type */
#define SSL_CERT_ERR_MAX_KNOWN SSL_CERT_ERR_HOSTNAME_MISMATCH

/**
 * ssl certificate information for certificate error message
 */
struct ssl_cert_info {
	long version;		/**< Certificate version */
	char not_before[32];	/**< Valid from date */
	char not_after[32];	/**< Valid to date */
	int sig_type;		/**< Signature type */
	char serialnum[64];	/**< Serial number */
	char issuer[256];	/**< Issuer details */
	char subject[256];	/**< Subject details */
	int cert_type;		/**< Certificate type */
	ssl_cert_err err;	/**< Whatever is wrong with this certificate */
};

/** maximum number of X509 certificates in chain for TLS connection */
#define MAX_SSL_CERTS 10

#endif /* NETSURF_SSL_CERTS_H_ */
