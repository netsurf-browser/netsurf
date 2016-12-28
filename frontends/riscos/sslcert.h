/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2010 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * RISC OS SSL certificate viewer interface.
 */

#ifndef NETSURF_RISCOS_SSLCERT_H
#define NETSURF_RISCOS_SSLCERT_H

struct node;

/**
 * Load and initialise the certificate window template.
 */
void ro_gui_cert_initialise(void);

/**
 * Prompt the user to verify a certificate with issuse.
 *
 * \param url The URL being verified.
 * \param certs The certificate to be verified
 * \param num The number of certificates to be verified.
 * \param cb Callback upon user decision.
 * \param cbpw Context pointer passed to cb
 */
nserror gui_cert_verify(struct nsurl *url, const struct ssl_cert_info *certs, unsigned long num, nserror (*cb)(bool proceed, void *pw), void *cbpw);

#endif

