/*
 * Copyright 2016 Vincent Sanders <vince@netsurf-browser.org>
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
 * Interface to win32 certificate viewing using nsw32 core windows.
 */

#ifndef NETSURF_WINDOWS_SSL_CERT_H
#define NETSURF_WINDOWS_SSL_CERT_H 1

struct nsurl;
struct ssl_cert_info;

/**
 * Prompt the user to verify a certificate with issuse.
 *
 * \param url The URL being verified.
 * \param certs The certificate to be verified
 * \param num The number of certificates to be verified.
 * \param cb Callback upon user decision.
 * \param cbpw Context pointer passed to cb
 * \return NSERROR_OK or error code if prompt creation failed.
 */
nserror nsw32_cert_verify(struct nsurl *url, const struct ssl_cert_info *certs, unsigned long num, nserror (*cb)(bool proceed, void *pw), void *cbpw);

/**
 * Create the ssl viewer window class.
 *
 * \param hinstance The application instance
 * \return NSERROR_OK on success or NSERROR_INIT_FAILED if the class
 *         creation failed.
 */
nserror nsws_create_cert_verify_class(HINSTANCE hinstance);

#endif
