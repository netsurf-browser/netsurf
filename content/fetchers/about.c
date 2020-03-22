/*
 * Copyright 2011 Vincent Sanders <vince@netsurf-browser.org>
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

/**
 * \file
 *
 * URL handling for the "about" scheme.
 *
 * Based on the data fetcher by Rob Kendrick
 * This fetcher provides a simple scheme for the user to access
 * information from the browser from a known, fixed URL.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "netsurf/plot_style.h"

#include "utils/log.h"
#include "testament.h"
#include "utils/corestrings.h"
#include "utils/nsoption.h"
#include "utils/utils.h"
#include "utils/messages.h"
#include "utils/ring.h"

#include "content/fetch.h"
#include "content/fetchers.h"
#include "content/fetchers/about.h"
#include "image/image_cache.h"

#include "desktop/system_colour.h"

struct fetch_about_context;

typedef bool (*fetch_about_handler)(struct fetch_about_context *);

/**
 * Context for an about fetch
 */
struct fetch_about_context {
	struct fetch_about_context *r_next, *r_prev;

	struct fetch *fetchh; /**< Handle for this fetch */

	bool aborted; /**< Flag indicating fetch has been aborted */
	bool locked; /**< Flag indicating entry is already entered */

	nsurl *url; /**< The full url the fetch refers to */

	const struct fetch_multipart_data *multipart; /**< post data */

	fetch_about_handler handler;
};

static struct fetch_about_context *ring = NULL;

/**
 * handler info for about scheme
 */
struct about_handlers {
	const char *name; /**< name to match in url */
	int name_len;
	lwc_string *lname; /**< Interned name */
	fetch_about_handler handler; /**< handler for the url */
	bool hidden; /**< If entry should be hidden in listing */
};


/**
 * issue fetch callbacks with locking
 */
static inline bool
fetch_about_send_callback(const fetch_msg *msg, struct fetch_about_context *ctx)
{
	ctx->locked = true;
	fetch_send_callback(msg, ctx->fetchh);
	ctx->locked = false;

	return ctx->aborted;
}

static inline bool
fetch_about_send_finished(struct fetch_about_context *ctx)
{
	fetch_msg msg;
	msg.type = FETCH_FINISHED;
	return fetch_about_send_callback(&msg, ctx);
}

static bool
fetch_about_send_header(struct fetch_about_context *ctx, const char *fmt, ...)
{
	char header[64];
	fetch_msg msg;
	va_list ap;

	va_start(ap, fmt);

	vsnprintf(header, sizeof header, fmt, ap);

	va_end(ap);

	msg.type = FETCH_HEADER;
	msg.data.header_or_data.buf = (const uint8_t *) header;
	msg.data.header_or_data.len = strlen(header);

	return fetch_about_send_callback(&msg, ctx);
}

/**
 * send formatted data on a fetch
 */
static nserror ssenddataf(struct fetch_about_context *ctx, const char *fmt, ...)
{
	char buffer[1024];
	fetch_msg msg;
	va_list ap;
	int slen;

	va_start(ap, fmt);

	slen = vsnprintf(buffer, sizeof(buffer), fmt, ap);

	va_end(ap);

	if (slen >= (int)sizeof(buffer)) {
		return NSERROR_NOSPACE;
	}

	msg.type = FETCH_DATA;
	msg.data.header_or_data.buf = (const uint8_t *) buffer;
	msg.data.header_or_data.len = slen;

	if (fetch_about_send_callback(&msg, ctx)) {
		return NSERROR_INVALID;
	}

	return NSERROR_OK;
}


/**
 * Generate a 500 server error respnse
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_srverror(struct fetch_about_context *ctx)
{
	nserror res;

	fetch_set_http_code(ctx->fetchh, 500);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/plain"))
		return false;

	res = ssenddataf(ctx, "Server error 500");
	if (res != NSERROR_OK) {
		return false;
	}

	fetch_about_send_finished(ctx);

	return true;
}


/**
 * Handler to generate about scheme cache page.
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_blank_handler(struct fetch_about_context *ctx)
{
	fetch_msg msg;
	const char buffer[2] = { ' ', '\0' };

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, 200);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/html"))
		goto fetch_about_blank_handler_aborted;

	msg.type = FETCH_DATA;
	msg.data.header_or_data.buf = (const uint8_t *) buffer;
	msg.data.header_or_data.len = strlen(buffer);

	if (fetch_about_send_callback(&msg, ctx))
		goto fetch_about_blank_handler_aborted;

	msg.type = FETCH_FINISHED;

	fetch_about_send_callback(&msg, ctx);

	return true;

fetch_about_blank_handler_aborted:
	return false;
}


/**
 * Handler to generate about scheme credits page.
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_credits_handler(struct fetch_about_context *ctx)
{
	fetch_msg msg;

	/* content is going to return redirect */
	fetch_set_http_code(ctx->fetchh, 302);

	msg.type = FETCH_REDIRECT;
	msg.data.redirect = "resource:credits.html";

	fetch_about_send_callback(&msg, ctx);

	return true;
}


/**
 * Handler to generate about scheme licence page.
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_licence_handler(struct fetch_about_context *ctx)
{
	fetch_msg msg;

	/* content is going to return redirect */
	fetch_set_http_code(ctx->fetchh, 302);

	msg.type = FETCH_REDIRECT;
	msg.data.redirect = "resource:licence.html";

	fetch_about_send_callback(&msg, ctx);

	return true;
}


/**
 * Handler to generate about:imagecache page.
 *
 * Shows details of current image cache.
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_imagecache_handler(struct fetch_about_context *ctx)
{
	fetch_msg msg;
	char buffer[2048]; /* output buffer */
	int code = 200;
	int slen;
	unsigned int cent_loop = 0;
	int elen = 0; /* entry length */
	nserror res;

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, code);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/html"))
		goto fetch_about_imagecache_handler_aborted;

	/* page head */
	res = ssenddataf(ctx,
			 "<html>\n<head>\n"
			"<title>NetSurf Browser Image Cache Status</title>\n"
			"<link rel=\"stylesheet\" type=\"text/css\" "
			"href=\"resource:internal.css\">\n"
			"</head>\n"
			"<body id =\"cachelist\">\n"
			"<p class=\"banner\">"
			"<a href=\"http://www.netsurf-browser.org/\">"
			"<img src=\"resource:netsurf.png\" alt=\"NetSurf\"></a>"
			"</p>\n"
			"<h1>NetSurf Browser Image Cache Status</h1>\n");
	if (res != NSERROR_OK) {
		goto fetch_about_imagecache_handler_aborted;
	}

	/* image cache summary */
	slen = image_cache_snsummaryf(buffer, sizeof(buffer),
		"<p>Configured limit of %a hysteresis of %b</p>\n"
		"<p>Total bitmap size in use %c (in %d)</p>\n"
		"<p>Age %es</p>\n"
		"<p>Peak size %f (in %g)</p>\n"
		"<p>Peak image count %h (size %i)</p>\n"
		"<p>Cache total/hit/miss/fail (counts) %j/%k/%l/%m "
				"(%pj%%/%pk%%/%pl%%/%pm%%)</p>\n"
		"<p>Cache total/hit/miss/fail (size) %n/%o/%q/%r "
				"(%pn%%/%po%%/%pq%%/%pr%%)</p>\n"
		"<p>Total images never rendered: %s "
				"(includes %t that were converted)</p>\n"
		"<p>Total number of excessive conversions: %u "
				"(from %v images converted more than once)"
				"</p>\n"
		"<p>Bitmap of size %w had most (%x) conversions</p>\n"
		"<h2>Current image cache contents</h2>\n");
	if (slen >= (int) (sizeof(buffer))) {
		goto fetch_about_imagecache_handler_aborted; /* overflow */
	}

	/* send image cache summary */
	msg.type = FETCH_DATA;
	msg.data.header_or_data.buf = (const uint8_t *) buffer;
	msg.data.header_or_data.len = slen;
	if (fetch_about_send_callback(&msg, ctx)) {
		goto fetch_about_imagecache_handler_aborted;
	}

	/* image cache entry table */
	res = ssenddataf(ctx, "<p class=\"imagecachelist\">\n"
			"<strong>"
			"<span>Entry</span>"
			"<span>Content Key</span>"
			"<span>Redraw Count</span>"
			"<span>Conversion Count</span>"
			"<span>Last Redraw</span>"
			"<span>Bitmap Age</span>"
			"<span>Bitmap Size</span>"
			"<span>Source</span>"
			"</strong>\n");
	if (res != NSERROR_OK) {
		goto fetch_about_imagecache_handler_aborted;
	}

	slen = 0;
	do {
		elen = image_cache_snentryf(buffer + slen,
					   sizeof buffer - slen,
				cent_loop,
				"<a href=\"%U\">"
				"<span>%e</span>"
				"<span>%k</span>"
				"<span>%r</span>"
				"<span>%c</span>"
				"<span>%a</span>"
				"<span>%g</span>"
				"<span>%s</span>"
				"<span>%o</span>"
				"</a>\n");
		if (elen <= 0)
			break; /* last option */

		if (elen >= (int) (sizeof buffer - slen)) {
			/* last entry would not fit in buffer, submit buffer */
			msg.data.header_or_data.len = slen;
			if (fetch_about_send_callback(&msg, ctx))
				goto fetch_about_imagecache_handler_aborted;
			slen = 0;
		} else {
			/* normal addition */
			slen += elen;
			cent_loop++;
		}
	} while (elen > 0);

	slen += snprintf(buffer + slen, sizeof buffer - slen,
			 "</p>\n</body>\n</html>\n");

	msg.data.header_or_data.len = slen;
	if (fetch_about_send_callback(&msg, ctx))
		goto fetch_about_imagecache_handler_aborted;

	fetch_about_send_finished(ctx);

	return true;

fetch_about_imagecache_handler_aborted:
	return false;
}


/**
 * ssl certificate information for certificate chain
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
	ssl_cert_err err;       /**< Whatever is wrong with this certificate */
};

#ifdef WITH_OPENSSL

#include <openssl/ssl.h>
#include <openssl/x509v3.h>

static nserror
der_to_certinfo(const uint8_t *der,
		size_t der_length,
		struct ssl_cert_info *info)
{
	BIO *mem;
	BUF_MEM *buf;
	const ASN1_INTEGER *asn1_num;
	BIGNUM *bignum;
	X509 *cert;		/**< Pointer to certificate */

	if (der == NULL) {
		return NSERROR_OK;
	}

	cert = d2i_X509(NULL, &der, der_length);
	if (cert == NULL) {
		return NSERROR_INVALID;
	}

	/* get certificate version */
	info->version = X509_get_version(cert);

	/* not before date */
	mem = BIO_new(BIO_s_mem());
	ASN1_TIME_print(mem, X509_get_notBefore(cert));
	BIO_get_mem_ptr(mem, &buf);
	(void) BIO_set_close(mem, BIO_NOCLOSE);
	BIO_free(mem);
	memcpy(info->not_before,
	       buf->data,
	       min(sizeof(info->not_before) - 1, (unsigned)buf->length));
	info->not_before[min(sizeof(info->not_before) - 1, (unsigned)buf->length)] = 0;
	BUF_MEM_free(buf);

	/* not after date */
	mem = BIO_new(BIO_s_mem());
	ASN1_TIME_print(mem,
			X509_get_notAfter(cert));
	BIO_get_mem_ptr(mem, &buf);
	(void) BIO_set_close(mem, BIO_NOCLOSE);
	BIO_free(mem);
	memcpy(info->not_after,
	       buf->data,
	       min(sizeof(info->not_after) - 1, (unsigned)buf->length));
	info->not_after[min(sizeof(info->not_after) - 1, (unsigned)buf->length)] = 0;
	BUF_MEM_free(buf);

	/* signature type */
	info->sig_type = X509_get_signature_type(cert);

	/* serial number */
	asn1_num = X509_get_serialNumber(cert);
	if (asn1_num != NULL) {
		bignum = ASN1_INTEGER_to_BN(asn1_num, NULL);
		if (bignum != NULL) {
			char *tmp = BN_bn2hex(bignum);
			if (tmp != NULL) {
				strncpy(info->serialnum,
					tmp,
					sizeof(info->serialnum));
				info->serialnum[sizeof(info->serialnum)-1] = '\0';
				OPENSSL_free(tmp);
			}
			BN_free(bignum);
			bignum = NULL;
		}
	}

	/* issuer name */
	mem = BIO_new(BIO_s_mem());
	X509_NAME_print_ex(mem,
			   X509_get_issuer_name(cert),
			   0, XN_FLAG_SEP_CPLUS_SPC |
			   XN_FLAG_DN_REV | XN_FLAG_FN_NONE);
	BIO_get_mem_ptr(mem, &buf);
	(void) BIO_set_close(mem, BIO_NOCLOSE);
	BIO_free(mem);
	memcpy(info->issuer,
	       buf->data,
	       min(sizeof(info->issuer) - 1, (unsigned) buf->length));
	info->issuer[min(sizeof(info->issuer) - 1, (unsigned) buf->length)] = 0;
	BUF_MEM_free(buf);

	/* subject */
	mem = BIO_new(BIO_s_mem());
	X509_NAME_print_ex(mem,
			   X509_get_subject_name(cert),
			   0,
			   XN_FLAG_SEP_CPLUS_SPC |
			   XN_FLAG_DN_REV |
			   XN_FLAG_FN_NONE);
	BIO_get_mem_ptr(mem, &buf);
	(void) BIO_set_close(mem, BIO_NOCLOSE);
	BIO_free(mem);
	memcpy(info->subject,
	       buf->data,
	       min(sizeof(info->subject) - 1, (unsigned)buf->length));
	info->subject[min(sizeof(info->subject) - 1, (unsigned) buf->length)] = 0;
	BUF_MEM_free(buf);

	/* type of certificate */
	info->cert_type = X509_certificate_type(cert, X509_get_pubkey(cert));

	X509_free(cert);

	return NSERROR_OK;
}

/* copy certificate data */
static nserror
convert_chain_to_cert_info(const struct cert_chain *chain,
			   struct ssl_cert_info **cert_info_out)
{
	struct ssl_cert_info *certs;
	size_t depth;
	nserror res;

	certs = calloc(chain->depth, sizeof(struct ssl_cert_info));
	if (certs == NULL) {
		return NSERROR_NOMEM;
	}

	for (depth = 0; depth < chain->depth;depth++) {
		res = der_to_certinfo(chain->certs[depth].der,
				      chain->certs[depth].der_length,
				      certs + depth);
		if (res != NSERROR_OK) {
			free(certs);
			return res;
		}
		certs[depth].err = chain->certs[depth].err;
	}

	*cert_info_out = certs;
	return NSERROR_OK;
}

#else
static nserror
convert_chain_to_cert_info(const struct cert_chain *chain,
			   struct ssl_cert_info **cert_info_out)
{
	return NSERROR_NOT_IMPLEMENTED;
}
#endif


/**
 * Handler to generate about:certificate page.
 *
 * Shows details of a certificate chain
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_certificate_handler(struct fetch_about_context *ctx)
{
	int code = 200;
	nserror res;
	struct cert_chain *chain = NULL;

	colour bg;
	colour fg;
	colour border;
	colour outside;

	res = ns_system_colour_char("Window", &bg);
	if (res != NSERROR_OK) {
		return false;
	}

	res = ns_system_colour_char("WindowText", &fg);
	if (res != NSERROR_OK) {
		return false;
	}

	outside = mix_colour(fg, bg, 0x0c);
	border = mix_colour(fg, bg, 0x40);

	bg = colour_rb_swap(bg);
	fg = colour_rb_swap(fg);
	border = colour_rb_swap(border);
	outside = colour_rb_swap(outside);

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, code);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/html"))
		goto fetch_about_certificate_handler_aborted;

	/* page head */
	res = ssenddataf(ctx,
			"<html>\n<head>\n"
			"<title>NetSurf Browser Certificate Viewer</title>\n"
			"<link rel=\"stylesheet\" type=\"text/css\" "
					"href=\"resource:internal.css\">\n"
			"<style>\n"
			"html {\n"
			"\tbackground-color: #%06x;\n"
			"}\n"
			"body {\n"
			"\tcolor: #%06x;\n"
			"\tbackground-color: #%06x;\n"
			"\tborder-color: #%06x;\n"
			"}\n"
			"h2 {\n"
			"\tborder-color: #%06x;\n"
			"}\n"
			"</style>\n"
			"</head>\n"
			"<body id =\"certificate\">\n"
			"<h1>Certificate</h1>\n",
			outside, fg, bg, border, border);
	if (res != NSERROR_OK) {
		goto fetch_about_certificate_handler_aborted;
	}

	res = cert_chain_from_query(ctx->url, &chain);
	if (res != NSERROR_OK) {
		res = ssenddataf(ctx, "<p>Could not process that</p>\n");
		if (res != NSERROR_OK) {
			goto fetch_about_certificate_handler_aborted;
		}
	} else {
		struct ssl_cert_info *cert_info;
		res = convert_chain_to_cert_info(chain, &cert_info);
		if (res == NSERROR_OK) {
			size_t depth;
			for (depth = 0; depth < chain->depth; depth++) {
				res = ssenddataf(ctx,
						 "<h2>Certificate: %d</h2>\n"
						 "<p>Subject: %s</p>"
						 "<p>Serial Number: %s</p>"
						 "<p>Type: %i</p>"
						 "<p>Version: %ld</p>"
						 "<p>Issuer: %s</p>"
						 "<p>Valid From: %s</p>"
						 "<p>Valid Untill: %s</p>",
						 depth,
						 cert_info[depth].subject,
						 cert_info[depth].serialnum,
						 cert_info[depth].cert_type,
						 cert_info[depth].version,
						 cert_info[depth].issuer,
						 cert_info[depth].not_before,
						 cert_info[depth].not_after);
				if (res != NSERROR_OK) {
					goto fetch_about_certificate_handler_aborted;
				}

			}
			free(cert_info);
		} else {
			res = ssenddataf(ctx,
					 "<p>Invalid certificate data</p>\n");
			if (res != NSERROR_OK) {
				goto fetch_about_certificate_handler_aborted;
			}
		}
	}


	/* page footer */
	res = ssenddataf(ctx, "</body>\n</html>\n");
	if (res != NSERROR_OK) {
		goto fetch_about_certificate_handler_aborted;
	}

	fetch_about_send_finished(ctx);

	cert_chain_free(chain);

	return true;

fetch_about_certificate_handler_aborted:
	cert_chain_free(chain);
	return false;
}


/**
 * Handler to generate about scheme config page
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_config_handler(struct fetch_about_context *ctx)
{
	fetch_msg msg;
	char buffer[1024];
	int slen = 0;
	unsigned int opt_loop = 0;
	int elen = 0; /* entry length */
	nserror res;

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, 200);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/html")) {
		goto fetch_about_config_handler_aborted;
	}

	res = ssenddataf(ctx,
			 "<html>\n<head>\n"
			 "<title>NetSurf Browser Config</title>\n"
			 "<link rel=\"stylesheet\" type=\"text/css\" "
			 "href=\"resource:internal.css\">\n"
			 "</head>\n"
			 "<body id =\"configlist\">\n"
			 "<p class=\"banner\">"
			 "<a href=\"http://www.netsurf-browser.org/\">"
			 "<img src=\"resource:netsurf.png\""
			 " alt=\"NetSurf\"></a>"
			 "</p>\n"
			 "<h1>NetSurf Browser Config</h1>\n"
			 "<table class=\"config\">\n"
			 "<tr><th>Option</th>"
			 "<th>Type</th>"
			 "<th>Provenance</th>"
			 "<th>Setting</th></tr>\n");
	if (res != NSERROR_OK) {
		goto fetch_about_config_handler_aborted;
	}

	msg.type = FETCH_DATA;
	msg.data.header_or_data.buf = (const uint8_t *) buffer;

	do {
		elen = nsoption_snoptionf(buffer + slen,
					 sizeof buffer - slen,
					 opt_loop,
					 "<tr><th>%k</th><td>%t</td><td>%p</td><td>%V</td></tr>\n");
		if (elen <= 0)
			break; /* last option */

		if (elen >= (int) (sizeof buffer - slen)) {
			/* last entry would not fit in buffer, submit buffer */
			msg.data.header_or_data.len = slen;
			if (fetch_about_send_callback(&msg, ctx))
				goto fetch_about_config_handler_aborted;
			slen = 0;
		} else {
			/* normal addition */
			slen += elen;
			opt_loop++;
		}
	} while (elen > 0);

	slen += snprintf(buffer + slen, sizeof buffer - slen,
			 "</table>\n</body>\n</html>\n");

	msg.data.header_or_data.len = slen;
	if (fetch_about_send_callback(&msg, ctx))
		goto fetch_about_config_handler_aborted;

	fetch_about_send_finished(ctx);

	return true;

fetch_about_config_handler_aborted:
	return false;
}


/**
 * Generate the text of a Choices file which represents the current
 * in use options.
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_choices_handler(struct fetch_about_context *ctx)
{
	fetch_msg msg;
	char buffer[1024];
	int code = 200;
	int slen;
	unsigned int opt_loop = 0;
	int res = 0;

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, code);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/plain"))
		goto fetch_about_choices_handler_aborted;

	msg.type = FETCH_DATA;
	msg.data.header_or_data.buf = (const uint8_t *) buffer;

	slen = snprintf(buffer, sizeof buffer,
		 "# Automatically generated current NetSurf browser Choices\n");

	do {
		res = nsoption_snoptionf(buffer + slen,
				sizeof buffer - slen,
				opt_loop,
				"%k:%v\n");
		if (res <= 0)
			break; /* last option */

		if (res >= (int) (sizeof buffer - slen)) {
			/* last entry would not fit in buffer, submit buffer */
			msg.data.header_or_data.len = slen;
			if (fetch_about_send_callback(&msg, ctx))
				goto fetch_about_choices_handler_aborted;
			slen = 0;
		} else {
			/* normal addition */
			slen += res;
			opt_loop++;
		}
	} while (res > 0);

	msg.data.header_or_data.len = slen;
	if (fetch_about_send_callback(&msg, ctx))
		goto fetch_about_choices_handler_aborted;

	fetch_about_send_finished(ctx);

	return true;

fetch_about_choices_handler_aborted:
	return false;
}


typedef struct {
	const char *leaf;
	const char *modtype;
} modification_t;

/**
 * Generate the text of an svn testament which represents the current
 * build-tree status
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_testament_handler(struct fetch_about_context *ctx)
{
	nserror res;
	static modification_t modifications[] = WT_MODIFICATIONS;
	int modidx; /* midification index */

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, 200);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/plain"))
		goto fetch_about_testament_handler_aborted;

	res = ssenddataf(ctx,
		"# Automatically generated by NetSurf build system\n\n");
	if (res != NSERROR_OK) {
		goto fetch_about_testament_handler_aborted;
	}

	res = ssenddataf(ctx,
#if defined(WT_BRANCHISTRUNK) || defined(WT_BRANCHISMASTER)
			"# This is a *DEVELOPMENT* build from the main line.\n\n"
#elif defined(WT_BRANCHISTAG) && (WT_MODIFIED == 0)
			"# This is a tagged build of NetSurf\n"
#ifdef WT_TAGIS
			"#      The tag used was '" WT_TAGIS "'\n\n"
#else
			"\n"
#endif
#elif defined(WT_NO_SVN) || defined(WT_NO_GIT)
			"# This NetSurf was built outside of our revision "
			"control environment.\n"
			"# This testament is therefore not very useful.\n\n"
#else
			"# This NetSurf was built from a branch (" WT_BRANCHPATH ").\n\n"
#endif
#if defined(CI_BUILD)
			"# This build carries the CI build number '" CI_BUILD "'\n\n"
#endif
			);
	if (res != NSERROR_OK) {
		goto fetch_about_testament_handler_aborted;
	}

	res = ssenddataf(ctx,
		"Built by %s (%s) from %s at revision %s on %s\n\n",
		GECOS, USERNAME, WT_BRANCHPATH, WT_REVID, WT_COMPILEDATE);
	if (res != NSERROR_OK) {
		goto fetch_about_testament_handler_aborted;
	}

	res = ssenddataf(ctx, "Built on %s in %s\n\n", WT_HOSTNAME, WT_ROOT);
	if (res != NSERROR_OK) {
		goto fetch_about_testament_handler_aborted;
	}

	if (WT_MODIFIED > 0) {
		res = ssenddataf(ctx,
				"Working tree has %d modification%s\n\n",
				WT_MODIFIED, WT_MODIFIED == 1 ? "" : "s");
	} else {
		res = ssenddataf(ctx, "Working tree is not modified.\n");
	}
	if (res != NSERROR_OK) {
		goto fetch_about_testament_handler_aborted;
	}

	for (modidx = 0; modidx < WT_MODIFIED; ++modidx) {
		res = ssenddataf(ctx,
				 "  %s  %s\n",
				 modifications[modidx].modtype,
				 modifications[modidx].leaf);
		if (res != NSERROR_OK) {
			goto fetch_about_testament_handler_aborted;
		}
	}

	fetch_about_send_finished(ctx);

	return true;

fetch_about_testament_handler_aborted:
	return false;
}


/**
 * Handler to generate about scheme logo page
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_logo_handler(struct fetch_about_context *ctx)
{
	fetch_msg msg;

	/* content is going to return redirect */
	fetch_set_http_code(ctx->fetchh, 302);

	msg.type = FETCH_REDIRECT;
	msg.data.redirect = "resource:netsurf.png";

	fetch_about_send_callback(&msg, ctx);

	return true;
}


/**
 * Handler to generate about scheme welcome page
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_welcome_handler(struct fetch_about_context *ctx)
{
	fetch_msg msg;

	/* content is going to return redirect */
	fetch_set_http_code(ctx->fetchh, 302);

	msg.type = FETCH_REDIRECT;
	msg.data.redirect = "resource:welcome.html";

	fetch_about_send_callback(&msg, ctx);

	return true;
}


/**
 * generate the description of the login query
 */
static nserror
get_authentication_description(struct nsurl *url,
			       const char *realm,
			       const char *username,
			       const char *password,
			       char **out_str)
{
	nserror res;
	char *url_s;
	size_t url_l;
	char *str = NULL;
	const char *key;

	res = nsurl_get(url, NSURL_HOST, &url_s, &url_l);
	if (res != NSERROR_OK) {
		return res;
	}

	if ((*username == 0) && (*password == 0)) {
		key = "LoginDescription";
	} else {
		key = "LoginAgain";
	}

	str = messages_get_buff(key, url_s, realm);
	if (str != NULL) {
		NSLOG(netsurf, INFO,
		      "key:%s url:%s realm:%s str:%s",
		      key, url_s, realm, str);
		*out_str = str;
	} else {
		res = NSERROR_NOMEM;
	}

	free(url_s);

	return res;
}


/**
 * generate a generic query description
 */
static nserror
get_query_description(struct nsurl *url,
		      const char *key,
		      char **out_str)
{
	nserror res;
	char *url_s;
	size_t url_l;
	char *str = NULL;

	/* get the host in question */
	res = nsurl_get(url, NSURL_HOST, &url_s, &url_l);
	if (res != NSERROR_OK) {
		return res;
	}

	/* obtain the description with the url substituted */
	str = messages_get_buff(key, url_s);
	if (str == NULL) {
		res = NSERROR_NOMEM;
	} else {
		*out_str = str;
	}

	free(url_s);

	return res;
}


/**
 * Handler to generate about scheme authentication query page
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_query_auth_handler(struct fetch_about_context *ctx)
{
	nserror res;
	char *url_s;
	size_t url_l;
	const char *realm = "";
	const char *username = "";
	const char *password = "";
	const char *title;
	char *description = NULL;
	struct nsurl *siteurl = NULL;
	const struct fetch_multipart_data *curmd; /* mutipart data iterator */

	/* extract parameters from multipart post data */
	curmd = ctx->multipart;
	while (curmd != NULL) {
		if (strcmp(curmd->name, "siteurl") == 0) {
			res = nsurl_create(curmd->value, &siteurl);
			if (res != NSERROR_OK) {
				return fetch_about_srverror(ctx);
			}
		} else if (strcmp(curmd->name, "realm") == 0) {
			realm = curmd->value;
		} else if (strcmp(curmd->name, "username") == 0) {
			username = curmd->value;
		} else if (strcmp(curmd->name, "password") == 0) {
			password = curmd->value;
		}
		curmd = curmd->next;
	}

	if (siteurl == NULL) {
		return fetch_about_srverror(ctx);
	}

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, 200);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/html; charset=utf-8")) {
		goto fetch_about_query_auth_handler_aborted;
	}


	title = messages_get("LoginTitle");

	res = ssenddataf(ctx,
			"<html>\n<head>\n"
			"<title>%s</title>\n"
			"<link rel=\"stylesheet\" type=\"text/css\" "
			"href=\"resource:internal.css\">\n"
			"</head>\n"
			"<body id =\"authentication\">\n"
			"<h1>%s</h1>\n",
			title, title);
	if (res != NSERROR_OK) {
		goto fetch_about_query_auth_handler_aborted;
	}

	res = ssenddataf(ctx,
			 "<form method=\"post\""
			 " enctype=\"multipart/form-data\">");
	if (res != NSERROR_OK) {
		goto fetch_about_query_auth_handler_aborted;
	}

	res = get_authentication_description(siteurl,
					     realm,
					     username,
					     password,
					     &description);
	if (res == NSERROR_OK) {
		res = ssenddataf(ctx, "<p>%s</p>", description);
		free(description);
		if (res != NSERROR_OK) {
			goto fetch_about_query_auth_handler_aborted;
		}
	}

	res = ssenddataf(ctx, "<table>");
	if (res != NSERROR_OK) {
		goto fetch_about_query_auth_handler_aborted;
	}

	res = ssenddataf(ctx,
			 "<tr>"
			 "<th><label for=\"name\">%s:</label></th>"
			 "<td><input type=\"text\" id=\"username\" "
			 "name=\"username\" value=\"%s\"></td>"
			 "</tr>",
			 messages_get("Username"), username);
	if (res != NSERROR_OK) {
		goto fetch_about_query_auth_handler_aborted;
	}

	res = ssenddataf(ctx,
			 "<tr>"
			 "<th><label for=\"password\">%s:</label></th>"
			 "<td><input type=\"password\" id=\"password\" "
			 "name=\"password\" value=\"%s\"></td>"
			 "</tr>",
			 messages_get("Password"), password);
	if (res != NSERROR_OK) {
		goto fetch_about_query_auth_handler_aborted;
	}

	res = ssenddataf(ctx, "</table>");
	if (res != NSERROR_OK) {
		goto fetch_about_query_auth_handler_aborted;
	}

	res = ssenddataf(ctx,
			 "<div id=\"buttons\">"
			 "<input type=\"submit\" id=\"login\" name=\"login\" "
			 "value=\"%s\" class=\"default-action\">"
			 "<input type=\"submit\" id=\"cancel\" name=\"cancel\" "
			 "value=\"%s\">"
			 "</div>",
			 messages_get("Login"),
			 messages_get("Cancel"));
	if (res != NSERROR_OK) {
		goto fetch_about_query_auth_handler_aborted;
	}

	res = nsurl_get(siteurl, NSURL_COMPLETE, &url_s, &url_l);
	if (res != NSERROR_OK) {
		url_s = strdup("");
	}
	res = ssenddataf(ctx,
			 "<input type=\"hidden\" name=\"siteurl\" value=\"%s\">",
			 url_s);
	free(url_s);
	if (res != NSERROR_OK) {
		goto fetch_about_query_auth_handler_aborted;
	}

	res = ssenddataf(ctx,
			 "<input type=\"hidden\" name=\"realm\" value=\"%s\">",
			 realm);
	if (res != NSERROR_OK) {
		goto fetch_about_query_auth_handler_aborted;
	}

	res = ssenddataf(ctx, "</form></body>\n</html>\n");
	if (res != NSERROR_OK) {
		goto fetch_about_query_auth_handler_aborted;
	}

	fetch_about_send_finished(ctx);

	nsurl_unref(siteurl);

	return true;

fetch_about_query_auth_handler_aborted:

	nsurl_unref(siteurl);

	return false;
}


/**
 * Handler to generate about scheme privacy query page
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_query_privacy_handler(struct fetch_about_context *ctx)
{
	nserror res;
	char *url_s;
	size_t url_l;
	const char *reason = "";
	const char *title;
	struct nsurl *siteurl = NULL;
	char *description = NULL;
	const struct fetch_multipart_data *curmd; /* mutipart data iterator */

	/* extract parameters from multipart post data */
	curmd = ctx->multipart;
	while (curmd != NULL) {
		if (strcmp(curmd->name, "siteurl") == 0) {
			res = nsurl_create(curmd->value, &siteurl);
			if (res != NSERROR_OK) {
				return fetch_about_srverror(ctx);
			}
		} else if (strcmp(curmd->name, "reason") == 0) {
			reason = curmd->value;
		}
		curmd = curmd->next;
	}

	if (siteurl == NULL) {
		return fetch_about_srverror(ctx);
	}

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, 200);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/html; charset=utf-8")) {
		goto fetch_about_query_ssl_handler_aborted;
	}

	title = messages_get("PrivacyTitle");
	res = ssenddataf(ctx,
			"<html>\n<head>\n"
			"<title>%s</title>\n"
			"<link rel=\"stylesheet\" type=\"text/css\" "
			"href=\"resource:internal.css\">\n"
			"</head>\n"
			"<body id =\"privacy\">\n"
			"<h1>%s</h1>\n",
			title, title);
	if (res != NSERROR_OK) {
		goto fetch_about_query_ssl_handler_aborted;
	}

	res = ssenddataf(ctx,
			 "<form method=\"post\""
			 " enctype=\"multipart/form-data\">");
	if (res != NSERROR_OK) {
		goto fetch_about_query_ssl_handler_aborted;
	}

	res = get_query_description(siteurl,
				    "PrivacyDescription",
				    &description);
	if (res == NSERROR_OK) {
		res = ssenddataf(ctx, "<div><p>%s</p></div>", description);
		free(description);
		if (res != NSERROR_OK) {
			goto fetch_about_query_ssl_handler_aborted;
		}
	}
	res = ssenddataf(ctx, "<div><p>%s</p></div>", reason);
	if (res != NSERROR_OK) {
		goto fetch_about_query_ssl_handler_aborted;
	}

	res = ssenddataf(ctx,
			 "<div id=\"buttons\">"
			 "<input type=\"submit\" id=\"back\" name=\"back\" "
			 "value=\"%s\" class=\"default-action\">"
			 "<input type=\"submit\" id=\"proceed\" name=\"proceed\" "
			 "value=\"%s\">"
			 "</div>",
			 messages_get("Backtosafety"),
			 messages_get("Proceed"));
	if (res != NSERROR_OK) {
		goto fetch_about_query_ssl_handler_aborted;
	}

	res = nsurl_get(siteurl, NSURL_COMPLETE, &url_s, &url_l);
	if (res != NSERROR_OK) {
		url_s = strdup("");
	}
	res = ssenddataf(ctx,
			 "<input type=\"hidden\" name=\"siteurl\" value=\"%s\">",
			 url_s);
	free(url_s);
	if (res != NSERROR_OK) {
		goto fetch_about_query_ssl_handler_aborted;
	}

	res = ssenddataf(ctx, "</form></body>\n</html>\n");
	if (res != NSERROR_OK) {
		goto fetch_about_query_ssl_handler_aborted;
	}

	fetch_about_send_finished(ctx);

	nsurl_unref(siteurl);

	return true;

fetch_about_query_ssl_handler_aborted:
	nsurl_unref(siteurl);

	return false;
}


/**
 * Handler to generate about scheme timeout query page
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_query_timeout_handler(struct fetch_about_context *ctx)
{
	nserror res;
	char *url_s;
	size_t url_l;
	const char *reason = "";
	const char *title;
	struct nsurl *siteurl = NULL;
	char *description = NULL;
	const struct fetch_multipart_data *curmd; /* mutipart data iterator */

	/* extract parameters from multipart post data */
	curmd = ctx->multipart;
	while (curmd != NULL) {
		if (strcmp(curmd->name, "siteurl") == 0) {
			res = nsurl_create(curmd->value, &siteurl);
			if (res != NSERROR_OK) {
				return fetch_about_srverror(ctx);
			}
		} else if (strcmp(curmd->name, "reason") == 0) {
			reason = curmd->value;
		}
		curmd = curmd->next;
	}

	if (siteurl == NULL) {
		return fetch_about_srverror(ctx);
	}

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, 200);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/html; charset=utf-8")) {
		goto fetch_about_query_timeout_handler_aborted;
	}

	title = messages_get("TimeoutTitle");
	res = ssenddataf(ctx,
			"<html>\n<head>\n"
			"<title>%s</title>\n"
			"<link rel=\"stylesheet\" type=\"text/css\" "
			"href=\"resource:internal.css\">\n"
			"</head>\n"
			"<body id =\"timeout\">\n"
			"<h1>%s</h1>\n",
			title, title);
	if (res != NSERROR_OK) {
		goto fetch_about_query_timeout_handler_aborted;
	}

	res = ssenddataf(ctx,
			 "<form method=\"post\""
			 " enctype=\"multipart/form-data\">");
	if (res != NSERROR_OK) {
		goto fetch_about_query_timeout_handler_aborted;
	}

	res = get_query_description(siteurl,
				    "TimeoutDescription",
				    &description);
	if (res == NSERROR_OK) {
		res = ssenddataf(ctx, "<div><p>%s</p></div>", description);
		free(description);
		if (res != NSERROR_OK) {
			goto fetch_about_query_timeout_handler_aborted;
		}
	}
	res = ssenddataf(ctx, "<div><p>%s</p></div>", reason);
	if (res != NSERROR_OK) {
		goto fetch_about_query_timeout_handler_aborted;
	}

	res = ssenddataf(ctx,
			 "<div id=\"buttons\">"
			 "<input type=\"submit\" id=\"back\" name=\"back\" "
			 "value=\"%s\" class=\"default-action\">"
			 "<input type=\"submit\" id=\"retry\" name=\"retry\" "
			 "value=\"%s\">"
			 "</div>",
			 messages_get("Backtoprevious"),
			 messages_get("TryAgain"));
	if (res != NSERROR_OK) {
		goto fetch_about_query_timeout_handler_aborted;
	}

	res = nsurl_get(siteurl, NSURL_COMPLETE, &url_s, &url_l);
	if (res != NSERROR_OK) {
		url_s = strdup("");
	}
	res = ssenddataf(ctx,
			 "<input type=\"hidden\" name=\"siteurl\" value=\"%s\">",
			 url_s);
	free(url_s);
	if (res != NSERROR_OK) {
		goto fetch_about_query_timeout_handler_aborted;
	}

	res = ssenddataf(ctx, "</form></body>\n</html>\n");
	if (res != NSERROR_OK) {
		goto fetch_about_query_timeout_handler_aborted;
	}

	fetch_about_send_finished(ctx);

	nsurl_unref(siteurl);

	return true;

fetch_about_query_timeout_handler_aborted:
	nsurl_unref(siteurl);

	return false;
}


/**
 * Handler to generate about scheme fetch error query page
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool
fetch_about_query_fetcherror_handler(struct fetch_about_context *ctx)
{
	nserror res;
	char *url_s;
	size_t url_l;
	const char *reason = "";
	const char *title;
	struct nsurl *siteurl = NULL;
	char *description = NULL;
	const struct fetch_multipart_data *curmd; /* mutipart data iterator */

	/* extract parameters from multipart post data */
	curmd = ctx->multipart;
	while (curmd != NULL) {
		if (strcmp(curmd->name, "siteurl") == 0) {
			res = nsurl_create(curmd->value, &siteurl);
			if (res != NSERROR_OK) {
				return fetch_about_srverror(ctx);
			}
		} else if (strcmp(curmd->name, "reason") == 0) {
			reason = curmd->value;
		}
		curmd = curmd->next;
	}

	if (siteurl == NULL) {
		return fetch_about_srverror(ctx);
	}

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, 200);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/html; charset=utf-8")) {
		goto fetch_about_query_fetcherror_handler_aborted;
	}

	title = messages_get("FetchErrorTitle");
	res = ssenddataf(ctx,
			"<html>\n<head>\n"
			"<title>%s</title>\n"
			"<link rel=\"stylesheet\" type=\"text/css\" "
			"href=\"resource:internal.css\">\n"
			"</head>\n"
			"<body id =\"fetcherror\">\n"
			"<h1>%s</h1>\n",
			title, title);
	if (res != NSERROR_OK) {
		goto fetch_about_query_fetcherror_handler_aborted;
	}

	res = ssenddataf(ctx,
			 "<form method=\"post\""
			 " enctype=\"multipart/form-data\">");
	if (res != NSERROR_OK) {
		goto fetch_about_query_fetcherror_handler_aborted;
	}

	res = get_query_description(siteurl,
				    "FetchErrorDescription",
				    &description);
	if (res == NSERROR_OK) {
		res = ssenddataf(ctx, "<div><p>%s</p></div>", description);
		free(description);
		if (res != NSERROR_OK) {
			goto fetch_about_query_fetcherror_handler_aborted;
		}
	}
	res = ssenddataf(ctx, "<div><p>%s</p></div>", reason);
	if (res != NSERROR_OK) {
		goto fetch_about_query_fetcherror_handler_aborted;
	}

	res = ssenddataf(ctx,
			 "<div id=\"buttons\">"
			 "<input type=\"submit\" id=\"back\" name=\"back\" "
			 "value=\"%s\" class=\"default-action\">"
			 "<input type=\"submit\" id=\"retry\" name=\"retry\" "
			 "value=\"%s\">"
			 "</div>",
			 messages_get("Backtoprevious"),
			 messages_get("TryAgain"));
	if (res != NSERROR_OK) {
		goto fetch_about_query_fetcherror_handler_aborted;
	}

	res = nsurl_get(siteurl, NSURL_COMPLETE, &url_s, &url_l);
	if (res != NSERROR_OK) {
		url_s = strdup("");
	}
	res = ssenddataf(ctx,
			 "<input type=\"hidden\" name=\"siteurl\" value=\"%s\">",
			 url_s);
	free(url_s);
	if (res != NSERROR_OK) {
		goto fetch_about_query_fetcherror_handler_aborted;
	}

	res = ssenddataf(ctx, "</form></body>\n</html>\n");
	if (res != NSERROR_OK) {
		goto fetch_about_query_fetcherror_handler_aborted;
	}

	fetch_about_send_finished(ctx);

	nsurl_unref(siteurl);

	return true;

fetch_about_query_fetcherror_handler_aborted:
	nsurl_unref(siteurl);

	return false;
}


/* Forward declaration because this handler requires the handler table. */
static bool fetch_about_about_handler(struct fetch_about_context *ctx);

/**
 * List of about paths and their handlers
 */
struct about_handlers about_handler_list[] = {
	{
		"credits",
		SLEN("credits"),
		NULL,
		fetch_about_credits_handler,
		false
	},
	{
		"licence",
		SLEN("licence"),
		NULL,
		fetch_about_licence_handler,
		false
	},
	{
		"license",
		SLEN("license"),
		NULL,
		fetch_about_licence_handler,
		true
	},
	{
		"welcome",
		SLEN("welcome"),
		NULL,
		fetch_about_welcome_handler,
		false
	},
	{
		"config",
		SLEN("config"),
		NULL,
		fetch_about_config_handler,
		false
	},
	{
		"Choices",
		SLEN("Choices"),
		NULL,
		fetch_about_choices_handler,
		false
	},
	{
		"testament",
		SLEN("testament"),
		NULL,
		fetch_about_testament_handler,
		false
	},
	{
		"about",
		SLEN("about"),
		NULL,
		fetch_about_about_handler,
		true
	},
	{
		"logo",
		SLEN("logo"),
		NULL,
		fetch_about_logo_handler,
		true
	},
	{
		/* details about the image cache */
		"imagecache",
		SLEN("imagecache"),
		NULL,
		fetch_about_imagecache_handler,
		true
	},
	{
		/* The default blank page */
		"blank",
		SLEN("blank"),
		NULL,
		fetch_about_blank_handler,
		true
	},
	{
		/* details about a certificate */
		"certificate",
		SLEN("certificate"),
		NULL,
		fetch_about_certificate_handler,
		true
	},
	{
		"query/auth",
		SLEN("query/auth"),
		NULL,
		fetch_about_query_auth_handler,
		true
	},
	{
		"query/ssl",
		SLEN("query/ssl"),
		NULL,
		fetch_about_query_privacy_handler,
		true
	},
	{
		"query/timeout",
		SLEN("query/timeout"),
		NULL,
		fetch_about_query_timeout_handler,
		true
	},
	{
		"query/fetcherror",
		SLEN("query/fetcherror"),
		NULL,
		fetch_about_query_fetcherror_handler,
		true
	}
};

#define about_handler_list_len \
	(sizeof(about_handler_list) / sizeof(struct about_handlers))

/**
 * List all the valid about: paths available
 *
 * \param ctx The fetch context.
 * \return true for sucess or false to generate an error.
 */
static bool fetch_about_about_handler(struct fetch_about_context *ctx)
{
	nserror res;
	unsigned int abt_loop = 0;

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, 200);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/html"))
		goto fetch_about_config_handler_aborted;

	res = ssenddataf(ctx,
			"<html>\n<head>\n"
			"<title>NetSurf List of About pages</title>\n"
			"<link rel=\"stylesheet\" type=\"text/css\" "
			"href=\"resource:internal.css\">\n"
			"</head>\n"
			"<body id =\"aboutlist\">\n"
			"<p class=\"banner\">"
			"<a href=\"http://www.netsurf-browser.org/\">"
			"<img src=\"resource:netsurf.png\" alt=\"NetSurf\"></a>"
			"</p>\n"
			"<h1>NetSurf List of About pages</h1>\n"
			"<ul>\n");
	if (res != NSERROR_OK) {
		goto fetch_about_config_handler_aborted;
	}

	for (abt_loop = 0; abt_loop < about_handler_list_len; abt_loop++) {

		/* Skip over hidden entries */
		if (about_handler_list[abt_loop].hidden)
			continue;

		res = ssenddataf(ctx,
			       "<li><a href=\"about:%s\">about:%s</a></li>\n",
			       about_handler_list[abt_loop].name,
			       about_handler_list[abt_loop].name);
		if (res != NSERROR_OK) {
			goto fetch_about_config_handler_aborted;
		}
	}

	res = ssenddataf(ctx, "</ul>\n</body>\n</html>\n");
	if (res != NSERROR_OK) {
		goto fetch_about_config_handler_aborted;
	}

	fetch_about_send_finished(ctx);

	return true;

fetch_about_config_handler_aborted:
	return false;
}

static bool
fetch_about_404_handler(struct fetch_about_context *ctx)
{
	nserror res;

	/* content is going to return 404 */
	fetch_set_http_code(ctx->fetchh, 404);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/plain; charset=utf-8")) {
		return false;
	}

	res = ssenddataf(ctx, "Unknown page: %s", nsurl_access(ctx->url));
	if (res != NSERROR_OK) {
		return false;
	}

	fetch_about_send_finished(ctx);

	return true;
}

/**
 * callback to initialise the about scheme fetcher.
 */
static bool fetch_about_initialise(lwc_string *scheme)
{
	unsigned int abt_loop = 0;
	lwc_error error;

	for (abt_loop = 0; abt_loop < about_handler_list_len; abt_loop++) {
		error = lwc_intern_string(about_handler_list[abt_loop].name,
					about_handler_list[abt_loop].name_len,
					&about_handler_list[abt_loop].lname);
		if (error != lwc_error_ok) {
			while (abt_loop-- != 0) {
				lwc_string_unref(about_handler_list[abt_loop].lname);
			}
			return false;
		}
	}

	return true;
}


/**
 * callback to finalise the about scheme fetcher.
 */
static void fetch_about_finalise(lwc_string *scheme)
{
	unsigned int abt_loop = 0;
	for (abt_loop = 0; abt_loop < about_handler_list_len; abt_loop++) {
		lwc_string_unref(about_handler_list[abt_loop].lname);
	}
}


static bool fetch_about_can_fetch(const nsurl *url)
{
	return true;
}


/**
 * callback to set up a about scheme fetch.
 *
 * \param post_urlenc post data in urlenc format, owned by the llcache object
 *                        hence valid the entire lifetime of the fetch.
 * \param post_multipart post data in multipart format, owned by the llcache
 *                        object hence valid the entire lifetime of the fetch.
 */
static void *
fetch_about_setup(struct fetch *fetchh,
		  nsurl *url,
		  bool only_2xx,
		  bool downgrade_tls,
		  const char *post_urlenc,
		  const struct fetch_multipart_data *post_multipart,
		  const char **headers)
{
	struct fetch_about_context *ctx;
	unsigned int handler_loop;
	lwc_string *path;
	bool match;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		return NULL;

	path = nsurl_get_component(url, NSURL_PATH);

	for (handler_loop = 0;
	     handler_loop < about_handler_list_len;
	     handler_loop++) {
		if (lwc_string_isequal(path,
				       about_handler_list[handler_loop].lname,
				       &match) == lwc_error_ok && match) {
			ctx->handler = about_handler_list[handler_loop].handler;
			break;
		}
	}

	if (path != NULL)
		lwc_string_unref(path);

	ctx->fetchh = fetchh;
	ctx->url = nsurl_ref(url);
	ctx->multipart = post_multipart;

	RING_INSERT(ring, ctx);

	return ctx;
}


/**
 * callback to free a about scheme fetch
 */
static void fetch_about_free(void *ctx)
{
	struct fetch_about_context *c = ctx;
	nsurl_unref(c->url);
	free(ctx);
}


/**
 * callback to start an about scheme fetch
 */
static bool fetch_about_start(void *ctx)
{
	return true;
}


/**
 * callback to abort a about fetch
 */
static void fetch_about_abort(void *ctx)
{
	struct fetch_about_context *c = ctx;

	/* To avoid the poll loop having to deal with the fetch context
	 * disappearing from under it, we simply flag the abort here.
	 * The poll loop itself will perform the appropriate cleanup.
	 */
	c->aborted = true;
}


/**
 * callback to poll for additional about fetch contents
 */
static void fetch_about_poll(lwc_string *scheme)
{
	struct fetch_about_context *c, *save_ring = NULL;

	/* Iterate over ring, processing each pending fetch */
	while (ring != NULL) {
		/* Take the first entry from the ring */
		c = ring;
		RING_REMOVE(ring, c);

		/* Ignore fetches that have been flagged as locked.
		 * This allows safe re-entrant calls to this function.
		 * Re-entrancy can occur if, as a result of a callback,
		 * the interested party causes fetch_poll() to be called
		 * again.
		 */
		if (c->locked == true) {
			RING_INSERT(save_ring, c);
			continue;
		}

		/* Only process non-aborted fetches */
		if (c->aborted == false) {
			/* about fetches can be processed in one go */
			if (c->handler == NULL) {
				fetch_about_404_handler(c);
			} else {
				c->handler(c);
			}
		}

		/* And now finish */
		fetch_remove_from_queues(c->fetchh);
		fetch_free(c->fetchh);
	}

	/* Finally, if we saved any fetches which were locked, put them back
	 * into the ring for next time
	 */
	ring = save_ring;
}


nserror fetch_about_register(void)
{
	lwc_string *scheme = lwc_string_ref(corestring_lwc_about);
	const struct fetcher_operation_table fetcher_ops = {
		.initialise = fetch_about_initialise,
		.acceptable = fetch_about_can_fetch,
		.setup = fetch_about_setup,
		.start = fetch_about_start,
		.abort = fetch_about_abort,
		.free = fetch_about_free,
		.poll = fetch_about_poll,
		.finalise = fetch_about_finalise
	};

	return fetcher_add(scheme, &fetcher_ops);
}
