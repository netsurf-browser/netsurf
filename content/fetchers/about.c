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

#include "netsurf/inttypes.h"
#include "netsurf/plot_style.h"

#include "utils/log.h"
#include "testament.h"
#include "utils/corestrings.h"
#include "utils/nscolour.h"
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
	char *dbuff;
	fetch_msg msg;
	va_list ap;
	int slen;

	va_start(ap, fmt);

	slen = vsnprintf(buffer, sizeof(buffer), fmt, ap);

	va_end(ap);

	if (slen < (int)sizeof(buffer)) {
		msg.type = FETCH_DATA;
		msg.data.header_or_data.buf = (const uint8_t *) buffer;
		msg.data.header_or_data.len = slen;

		if (fetch_about_send_callback(&msg, ctx)) {
			return NSERROR_INVALID;
		}

		return NSERROR_OK;
	}

	dbuff = malloc(slen + 1);
	if (dbuff == NULL) {
		return NSERROR_NOSPACE;
	}

	va_start(ap, fmt);

	slen = vsnprintf(dbuff, slen + 1, fmt, ap);

	va_end(ap);

	msg.type = FETCH_DATA;
	msg.data.header_or_data.buf = (const uint8_t *)dbuff;
	msg.data.header_or_data.len = slen;

	if (fetch_about_send_callback(&msg, ctx)) {
		free(dbuff);
		return NSERROR_INVALID;
	}

	free(dbuff);
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
	bool even = false;

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, code);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/html"))
		goto fetch_about_imagecache_handler_aborted;

	/* page head */
	res = ssenddataf(ctx,
			 "<html>\n<head>\n"
			"<title>Image Cache Status</title>\n"
			"<link rel=\"stylesheet\" type=\"text/css\" "
			"href=\"resource:internal.css\">\n"
			"</head>\n"
			"<body id =\"cachelist\" class=\"ns-even-bg ns-even-fg ns-border\">\n"
			"<h1 class=\"ns-border\">Image Cache Status</h1>\n");
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
		"<h2 class=\"ns-border\">Current contents</h2>\n");
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
		if (even) {
			elen = image_cache_snentryf(buffer + slen,
						   sizeof buffer - slen,
					cent_loop,
					"<a href=\"%U\">"
					"<span class=\"ns-border\">%e</span>"
					"<span class=\"ns-border\">%k</span>"
					"<span class=\"ns-border\">%r</span>"
					"<span class=\"ns-border\">%c</span>"
					"<span class=\"ns-border\">%a</span>"
					"<span class=\"ns-border\">%g</span>"
					"<span class=\"ns-border\">%s</span>"
					"<span class=\"ns-border\">%o</span>"
					"</a>\n");
		} else {
			elen = image_cache_snentryf(buffer + slen,
						   sizeof buffer - slen,
					cent_loop,
					"<a class=\"ns-odd-bg\" href=\"%U\">"
					"<span class=\"ns-border\">%e</span>"
					"<span class=\"ns-border\">%k</span>"
					"<span class=\"ns-border\">%r</span>"
					"<span class=\"ns-border\">%c</span>"
					"<span class=\"ns-border\">%a</span>"
					"<span class=\"ns-border\">%g</span>"
					"<span class=\"ns-border\">%s</span>"
					"<span class=\"ns-border\">%o</span>"
					"</a>\n");
		}
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
			even = !even;
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
 * certificate name parameters
 */
struct ns_cert_name {
	char *common_name;
	char *organisation;
	char *organisation_unit;
	char *locality;
	char *province;
	char *country;
};

/**
 * Certificate public key parameters
 */
struct ns_cert_pkey {
	char *algor;
	int size;
	char *modulus;
	char *exponent;
	char *curve;
	char *public;
};

/**
 * Certificate subject alternative name
 */
struct ns_cert_san {
	struct ns_cert_san *next;
	char *name;
};

/**
 * certificate information for certificate chain
 */
struct ns_cert_info {
	struct ns_cert_name subject_name; /**< Subject details */
	struct ns_cert_name issuer_name; /**< Issuer details */
	struct ns_cert_pkey public_key; /**< public key details */
	long version;		/**< Certificate version */
	char *not_before;	/**< Valid from date */
	char *not_after;	/**< Valid to date */
	int sig_type;		/**< Signature type */
	char *sig_algor;        /**< Signature Algorithm */
	char *serialnum;	/**< Serial number */
	char *sha1fingerprint; /**< fingerprint shar1 encoded */
	char *sha256fingerprint; /**< fingerprint shar256 encoded */
	struct ns_cert_san *san; /**< subject alternative names */
	ssl_cert_err err;       /**< Whatever is wrong with this certificate */
};

/**
 * free all resources associated with a certificate information structure
 */
static nserror free_ns_cert_info(struct ns_cert_info *cinfo)
{
	struct ns_cert_san *san;

	free(cinfo->subject_name.common_name);
	free(cinfo->subject_name.organisation);
	free(cinfo->subject_name.organisation_unit);
	free(cinfo->subject_name.locality);
	free(cinfo->subject_name.province);
	free(cinfo->subject_name.country);
	free(cinfo->issuer_name.common_name);
	free(cinfo->issuer_name.organisation);
	free(cinfo->issuer_name.organisation_unit);
	free(cinfo->issuer_name.locality);
	free(cinfo->issuer_name.province);
	free(cinfo->issuer_name.country);
	free(cinfo->public_key.algor);
	free(cinfo->public_key.modulus);
	free(cinfo->public_key.exponent);
	free(cinfo->public_key.curve);
	free(cinfo->public_key.public);
	free(cinfo->not_before);
	free(cinfo->not_after);
	free(cinfo->sig_algor);
	free(cinfo->serialnum);

	/* free san list avoiding use after free */
	san = cinfo->san;
	while (san != NULL) {
		struct ns_cert_san *next;
		next = san->next;
		free(san);
		san = next;
	}

	free(cinfo);

	return NSERROR_OK;
}

#ifdef WITH_OPENSSL

#include <openssl/ssl.h>
#include <openssl/x509v3.h>

/* OpenSSL 1.0.x, 1.0.2, 1.1.0 and 1.1.1 API all changed
 * LibreSSL declares its OpenSSL version as 2.1 but only supports 1.0.x API
 */
#if (defined(LIBRESSL_VERSION_NUMBER) || (OPENSSL_VERSION_NUMBER < 0x1010000fL))
/* 1.0.x */

#if (defined(LIBRESSL_VERSION_NUMBER) || (OPENSSL_VERSION_NUMBER < 0x1000200fL))
/* pre 1.0.2 */
static int ns_X509_get_signature_nid(X509 *cert)
{
	return OBJ_obj2nid(cert->cert_info->key->algor->algorithm);
}
#else
#define ns_X509_get_signature_nid X509_get_signature_nid
#endif

static const unsigned char *ns_ASN1_STRING_get0_data(ASN1_STRING *asn1str)
{
	return (const unsigned char *)ASN1_STRING_data(asn1str);
}

static const BIGNUM *ns_RSA_get0_n(const RSA *d)
{
	return d->n;
}

static const BIGNUM *ns_RSA_get0_e(const RSA *d)
{
	return d->e;
}

static int ns_RSA_bits(const RSA *rsa)
{
	return RSA_size(rsa) * 8;
}

static int ns_DSA_bits(const DSA *dsa)
{
	return DSA_size(dsa) * 8;
}

static int ns_DH_bits(const DH *dh)
{
	return DH_size(dh) * 8;
}

#elif (OPENSSL_VERSION_NUMBER < 0x1010100fL)
/* 1.1.0 */
#define ns_X509_get_signature_nid X509_get_signature_nid
#define ns_ASN1_STRING_get0_data ASN1_STRING_get0_data

static const BIGNUM *ns_RSA_get0_n(const RSA *r)
{
	const BIGNUM *n;
	const BIGNUM *e;
	const BIGNUM *d;
	RSA_get0_key(r, &n, &e, &d);
	return n;
}

static const BIGNUM *ns_RSA_get0_e(const RSA *r)
{
	const BIGNUM *n;
	const BIGNUM *e;
	const BIGNUM *d;
	RSA_get0_key(r, &n, &e, &d);
	return e;
}

#define ns_RSA_bits RSA_bits
#define ns_DSA_bits DSA_bits
#define ns_DH_bits DH_bits

#else
/* 1.1.1 and later */
#define ns_X509_get_signature_nid X509_get_signature_nid
#define ns_ASN1_STRING_get0_data ASN1_STRING_get0_data
#define ns_RSA_get0_n RSA_get0_n
#define ns_RSA_get0_e RSA_get0_e
#define ns_RSA_bits RSA_bits
#define ns_DSA_bits DSA_bits
#define ns_DH_bits DH_bits
#endif

/**
 * extract certificate name information
 *
 * \param xname The X509 name to convert. The reference is borrowed so is not freeed
 * \param iname The info structure to recive the extracted parameters.
 * \return NSERROR_OK on success else error code
 */
static nserror
xname_to_info(X509_NAME *xname, struct ns_cert_name *iname)
{
	int entryidx;
	int entrycnt;
	X509_NAME_ENTRY *entry; /* current name entry */
	ASN1_STRING *value;
	const unsigned char *value_str;
	ASN1_OBJECT *name;
	int name_nid;
	char **field;

	entrycnt = X509_NAME_entry_count(xname);

	for (entryidx = 0; entryidx < entrycnt; entryidx++) {
		entry = X509_NAME_get_entry(xname, entryidx);
		name = X509_NAME_ENTRY_get_object(entry);
		name_nid = OBJ_obj2nid(name);
		value = X509_NAME_ENTRY_get_data(entry);
		value_str = ns_ASN1_STRING_get0_data(value);
		switch (name_nid) {
		case NID_commonName:
			field = &iname->common_name;
			break;
		case NID_countryName:
			field = &iname->country;
			break;
		case NID_localityName:
			field = &iname->locality;
			break;
		case NID_stateOrProvinceName:
			field = &iname->province;
			break;
		case NID_organizationName:
			field = &iname->organisation;
			break;
		case NID_organizationalUnitName:
			field = &iname->organisation_unit;
			break;
		default :
			field = NULL;
			break;
		}
		if (field != NULL) {
			*field = strdup((const char *)value_str);
			NSLOG(netsurf, DEEPDEBUG,
			      "NID:%d value: %s", name_nid, *field);
		} else {
			NSLOG(netsurf, DEEPDEBUG, "NID:%d", name_nid);
		}
	}

	/*
	 * ensure the common name is set to something, this being
	 *  missing means the certificate is broken but this should be
	 *  robust in the face of bad data
	 */
	if (iname->common_name == NULL) {
		iname->common_name = strdup("Unknown");
	}

	return NSERROR_OK;
}


/**
 * duplicate a hex formatted string inserting the colons
 *
 * \todo only uses html entity as separator because netsurfs line breaking
 *       fails otherwise.
 */
static char *hexdup(const char *hex)
{
	int hexlen;
	char *dst;
	char *out;
	int cn = 0;

	hexlen = strlen(hex);
	/* allow space fox XXYY to XX&#58;YY&#58; */
	dst = malloc(((hexlen * 7) + 6) / 2);

	if (dst != NULL) {
		for (out = dst; *hex != 0; hex++) {
			if (cn == 2) {
				cn = 0;
				*out++ = '&';
				*out++ = '#';
				*out++ = '5';
				*out++ = '8';
				*out++ = ';';
			}
			*out++ = *hex;
			cn++;
		}
		*out = 0;
	}
	return dst;
}


/**
 * create a hex formatted string inserting the colons from binary data
 *
 * \todo only uses html entity as separator because netsurfs line breaking
 *       fails otherwise.
 */
static char *bindup(unsigned char *bin, unsigned int binlen)
{
	char *dst;
	char *out;
	unsigned int idx;
	const char hex[] = { '0', '1', '2', '3', '4', '5', '6', '7',
			     '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

	/* allow space fox XY to expand to XX&#58;YY&#58; */
	dst = malloc(binlen * 7);

	if (dst != NULL) {
		out = dst;
		for (idx = 0; idx < binlen; idx++) {
			*out++ = hex[(bin[idx] & 0xf0) >> 4];
			*out++ = hex[bin[idx] & 0xf];

			*out++ = '&';
			*out++ = '#';
			*out++ = '5';
			*out++ = '8';
			*out++ = ';';
		}
		out -= 5;
		*out = 0;
	}
	return dst;
}


/**
 * extract RSA key information to info structure
 *
 * \param rsa The RSA key to examine. The reference is dropped on return
 * \param ikey The public key info structure to fill
 * \rerun NSERROR_OK on success else error code.
 */
static nserror
rsa_to_info(RSA *rsa, struct ns_cert_pkey *ikey)
{
	char *tmp;

	if (rsa == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	ikey->algor = strdup("RSA");

	ikey->size = ns_RSA_bits(rsa);

	tmp = BN_bn2hex(ns_RSA_get0_n(rsa));
	if (tmp != NULL) {
		ikey->modulus = hexdup(tmp);
		OPENSSL_free(tmp);
	}

	tmp = BN_bn2dec(ns_RSA_get0_e(rsa));
	if (tmp != NULL) {
		ikey->exponent = strdup(tmp);
		OPENSSL_free(tmp);
	}

	RSA_free(rsa);

	return NSERROR_OK;
}


/**
 * extract DSA key information to info structure
 *
 * \param dsa The RSA key to examine. The reference is dropped on return
 * \param ikey The public key info structure to fill
 * \rerun NSERROR_OK on success else error code.
 */
static nserror
dsa_to_info(DSA *dsa, struct ns_cert_pkey *ikey)
{
	if (dsa == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	ikey->algor = strdup("DSA");

	ikey->size = ns_DSA_bits(dsa);

	DSA_free(dsa);

	return NSERROR_OK;
}


/**
 * extract DH key information to info structure
 *
 * \param dsa The RSA key to examine. The reference is dropped on return
 * \param ikey The public key info structure to fill
 * \rerun NSERROR_OK on success else error code.
 */
static nserror
dh_to_info(DH *dh, struct ns_cert_pkey *ikey)
{
	if (dh == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	ikey->algor = strdup("Diffie Hellman");

	ikey->size = ns_DH_bits(dh);

	DH_free(dh);

	return NSERROR_OK;
}


/**
 * extract EC key information to info structure
 *
 * \param ec The EC key to examine. The reference is dropped on return
 * \param ikey The public key info structure to fill
 * \rerun NSERROR_OK on success else error code.
 */
static nserror
ec_to_info(EC_KEY *ec, struct ns_cert_pkey *ikey)
{
	const EC_GROUP *ecgroup;
	const EC_POINT *ecpoint;
	BN_CTX *bnctx;
	char *ecpoint_hex;

	if (ec == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	ikey->algor = strdup("Elliptic Curve");

	ecgroup = EC_KEY_get0_group(ec);

	if (ecgroup != NULL) {
		ikey->size = EC_GROUP_get_degree(ecgroup);

		ikey->curve = strdup(OBJ_nid2ln(EC_GROUP_get_curve_name(ecgroup)));

		ecpoint = EC_KEY_get0_public_key(ec);
		if (ecpoint != NULL) {
			bnctx = BN_CTX_new();
			ecpoint_hex = EC_POINT_point2hex(ecgroup,
							 ecpoint,
							 POINT_CONVERSION_UNCOMPRESSED,
							 bnctx);
			ikey->public = hexdup(ecpoint_hex);
			OPENSSL_free(ecpoint_hex);
			BN_CTX_free(bnctx);
		}
	}
	EC_KEY_free(ec);

	return NSERROR_OK;
}


/**
 * extract public key information to info structure
 *
 * \param pkey the public key to examine. The reference is dropped on return
 * \param ikey The public key info structure to fill
 * \rerun NSERROR_OK on success else error code.
 */
static nserror
pkey_to_info(EVP_PKEY *pkey, struct ns_cert_pkey *ikey)
{
	nserror res;

	if (pkey == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	switch (EVP_PKEY_base_id(pkey)) {
	case EVP_PKEY_RSA:
		res = rsa_to_info(EVP_PKEY_get1_RSA(pkey), ikey);
		break;

	case EVP_PKEY_DSA:
		res = dsa_to_info(EVP_PKEY_get1_DSA(pkey), ikey);
		break;

	case EVP_PKEY_DH:
		res = dh_to_info(EVP_PKEY_get1_DH(pkey), ikey);
		break;

	case EVP_PKEY_EC:
		res = ec_to_info(EVP_PKEY_get1_EC_KEY(pkey), ikey);
		break;

	default:
		res = NSERROR_NOT_IMPLEMENTED;
		break;
	}

	EVP_PKEY_free(pkey);

	return res;
}

static nserror san_to_info(X509 *cert, struct ns_cert_san **prev_next)
{
	int idx;
	int san_names_nb = -1;
	const GENERAL_NAME *current_name;
	const unsigned char *dns_name;
	struct ns_cert_san *isan;

	STACK_OF(GENERAL_NAME) *san_names = NULL;

	san_names = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
	if (san_names == NULL) {
		return NSERROR_OK;
	}

	san_names_nb = sk_GENERAL_NAME_num(san_names);

	/* Check each name within the extension */
	for (idx = 0; idx < san_names_nb; idx++) {
		current_name = sk_GENERAL_NAME_value(san_names, idx);

		if (current_name->type == GEN_DNS) {
			/* extract DNS name into info structure */
			dns_name = ns_ASN1_STRING_get0_data(current_name->d.dNSName);

			isan = malloc(sizeof(struct ns_cert_san));
			if (isan != NULL) {
				isan->name = strdup((const char *)dns_name);
				isan->next = NULL;
				*prev_next = isan;
				prev_next = &isan->next;
			}
		}
	}

	/* AmiSSL can't cope with the "correct" mechanism of freeing
	 * the GENERAL_NAME stack, which is:
	 * sk_GENERAL_NAME_pop_free(san_names, GENERAL_NAME_free);
	 * So instead we do this open-coded loop which does the same:
	 */
	for (idx = 0; idx < san_names_nb; idx++) {
		GENERAL_NAME *entry = sk_GENERAL_NAME_pop(san_names);
		GENERAL_NAME_free(entry);
	}
	sk_GENERAL_NAME_free(san_names);

	return NSERROR_OK;
}

static nserror
der_to_certinfo(const uint8_t *der,
		size_t der_length,
		struct ns_cert_info *info)
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

	/*
	 * get certificate version
	 *
	 * \note this is defined by standards (X.509 et al) to be one
	 *        less than the certificate version.
	 */
	info->version = X509_get_version(cert) + 1;

	/* not before date */
	mem = BIO_new(BIO_s_mem());
	ASN1_TIME_print(mem, X509_get_notBefore(cert));
	BIO_get_mem_ptr(mem, &buf);
	(void) BIO_set_close(mem, BIO_NOCLOSE);
	BIO_free(mem);
	info->not_before = calloc(1, buf->length + 1);
	if (info->not_before != NULL) {
		memcpy(info->not_before, buf->data, (unsigned)buf->length);
	}
	BUF_MEM_free(buf);

	/* not after date */
	mem = BIO_new(BIO_s_mem());
	ASN1_TIME_print(mem,
			X509_get_notAfter(cert));
	BIO_get_mem_ptr(mem, &buf);
	(void) BIO_set_close(mem, BIO_NOCLOSE);
	BIO_free(mem);
	info->not_after = calloc(1, buf->length + 1);
	if (info->not_after != NULL) {
		memcpy(info->not_after, buf->data, (unsigned)buf->length);
	}
	BUF_MEM_free(buf);

	/* signature type */
	info->sig_type = X509_get_signature_type(cert);

	/* signature algorithm */
	int pkey_nid = ns_X509_get_signature_nid(cert);
	if (pkey_nid != NID_undef) {
		const char* sslbuf = OBJ_nid2ln(pkey_nid);
		if (sslbuf != NULL) {
			info->sig_algor = strdup(sslbuf);
		}
	}

	/* serial number */
	asn1_num = X509_get_serialNumber(cert);
	if (asn1_num != NULL) {
		bignum = ASN1_INTEGER_to_BN(asn1_num, NULL);
		if (bignum != NULL) {
			char *tmp = BN_bn2hex(bignum);
			if (tmp != NULL) {
				info->serialnum = hexdup(tmp);
				OPENSSL_free(tmp);
			}
			BN_free(bignum);
			bignum = NULL;
		}
	}

	/* fingerprints */
	const EVP_MD *digest;
	unsigned int dig_len;
	unsigned char *buff;
	int rc;

	digest = EVP_sha1();
	buff = malloc(EVP_MD_size(digest));
	if (buff != NULL) {
		rc = X509_digest(cert, digest, buff, &dig_len);
		if ((rc == 1) && (dig_len == (unsigned int)EVP_MD_size(digest))) {
			info->sha1fingerprint = bindup(buff, dig_len);
		}
		free(buff);
	}

	digest = EVP_sha256();
	buff = malloc(EVP_MD_size(digest));
	if (buff != NULL) {
		rc = X509_digest(cert, digest, buff, &dig_len);
		if ((rc == 1) && (dig_len == (unsigned int)EVP_MD_size(digest))) {
			info->sha256fingerprint = bindup(buff, dig_len);
		}
		free(buff);
	}

	/* subject alternative names */
	san_to_info(cert, &info->san);

	/* issuer name */
	xname_to_info(X509_get_issuer_name(cert), &info->issuer_name);

	/* subject */
	xname_to_info(X509_get_subject_name(cert), &info->subject_name);

	/* public key */
	pkey_to_info(X509_get_pubkey(cert), &info->public_key);

	X509_free(cert);

	return NSERROR_OK;
}

/* copy certificate data */
static nserror
convert_chain_to_cert_info(const struct cert_chain *chain,
			   struct ns_cert_info **cert_info_out)
{
	struct ns_cert_info *certs;
	size_t depth;
	nserror res;

	certs = calloc(chain->depth, sizeof(struct ns_cert_info));
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
			   struct ns_cert_info **cert_info_out)
{
	return NSERROR_NOT_IMPLEMENTED;
}
#endif


static nserror
format_certificate_name(struct fetch_about_context *ctx,
			struct ns_cert_name *cert_name)
{
	nserror res;
	res = ssenddataf(ctx,
			 "<tr><th>Common Name</th><td>%s</td></tr>\n",
			 cert_name->common_name);
	if (res != NSERROR_OK) {
		return res;
	}

	if (cert_name->organisation != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>Organisation</th><td>%s</td></tr>\n",
				 cert_name->organisation);
		if (res != NSERROR_OK) {
			return res;
		}
	}

	if (cert_name->organisation_unit != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>Organisation Unit</th><td>%s</td></tr>\n",
				 cert_name->organisation_unit);
		if (res != NSERROR_OK) {
			return res;
		}
	}

	if (cert_name->locality != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>Locality</th><td>%s</td></tr>\n",
				 cert_name->locality);
		if (res != NSERROR_OK) {
			return res;
		}
	}

	if (cert_name->province != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>Privince</th><td>%s</td></tr>\n",
				 cert_name->province);
		if (res != NSERROR_OK) {
			return res;
		}
	}

	if (cert_name->country != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>Country</th><td>%s</td></tr>\n",
				 cert_name->country);
		if (res != NSERROR_OK) {
			return res;
		}
	}

	return res;
}

/**
 * output formatted certificate subject alternate names
 */
static nserror
format_certificate_san(struct fetch_about_context *ctx,
			      struct ns_cert_san *san)
{
	nserror res;

	if (san == NULL) {
		return NSERROR_OK;
	}

	res = ssenddataf(ctx,
			 "<table class=\"info\">\n"
			 "<tr><th>Alternative Names</th><td><hr></td></tr>\n");
	if (res != NSERROR_OK) {
		return res;
	}

	while (san != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>DNS Name</th><td>%s</td></tr>\n",
				 san->name);
		if (res != NSERROR_OK) {
			return res;
		}

		san = san->next;
	}

	res = ssenddataf(ctx, "</table>\n");

	return res;

}


static nserror
format_certificate_public_key(struct fetch_about_context *ctx,
			      struct ns_cert_pkey *public_key)
{
	nserror res;

	if (public_key->algor == NULL) {
		/* skip the table if no algorithm name */
		return NSERROR_OK;
	}

	res = ssenddataf(ctx,
			 "<table class=\"info\">\n"
			 "<tr><th>Public Key</th><td><hr></td></tr>\n"
			 "<tr><th>Algorithm</th><td>%s</td></tr>\n"
			 "<tr><th>Key Size</th><td>%d</td></tr>\n",
			 public_key->algor,
			 public_key->size);
	if (res != NSERROR_OK) {
		return res;
	}


	if (public_key->exponent != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>Exponent</th><td>%s</td></tr>\n",
				 public_key->exponent);
		if (res != NSERROR_OK) {
			return res;
		}
	}

	if (public_key->modulus != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>Modulus</th><td class=\"data\">%s</td></tr>\n",
				 public_key->modulus);
		if (res != NSERROR_OK) {
			return res;
		}
	}

	if (public_key->curve != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>Curve</th><td>%s</td></tr>\n",
				 public_key->curve);
		if (res != NSERROR_OK) {
			return res;
		}
	}

	if (public_key->public != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>Public Value</th><td>%s</td></tr>\n",
				 public_key->public);
		if (res != NSERROR_OK) {
			return res;
		}
	}

	res = ssenddataf(ctx, "</table>\n");

	return res;
}

static nserror
format_certificate_fingerprint(struct fetch_about_context *ctx,
			      struct ns_cert_info *cert_info)
{
	nserror res;

	if ((cert_info->sha1fingerprint == NULL) &&
	    (cert_info->sha256fingerprint == NULL))  {
		/* skip the table if no fingerprints */
		return NSERROR_OK;
	}


	res = ssenddataf(ctx,
			 "<table class=\"info\">\n"
			 "<tr><th>Fingerprints</th><td><hr></td></tr>\n");
	if (res != NSERROR_OK) {
		return res;
	}

	if (cert_info->sha256fingerprint != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>SHA-256</th><td class=\"data\">%s</td></tr>\n",
				 cert_info->sha256fingerprint);
		if (res != NSERROR_OK) {
			return res;
		}
	}

	if (cert_info->sha1fingerprint != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>SHA-1</th><td class=\"data\">%s</td></tr>\n",
				 cert_info->sha1fingerprint);
		if (res != NSERROR_OK) {
			return res;
		}
	}

	res = ssenddataf(ctx, "</table>\n");

	return res;
}

static nserror
format_certificate(struct fetch_about_context *ctx,
		   struct ns_cert_info *cert_info,
		   size_t depth)
{
	nserror res;

	res = ssenddataf(ctx,
			 "<h2 id=\"%"PRIsizet"\" class=\"ns-border\">%s</h2>\n",
			 depth, cert_info->subject_name.common_name);
	if (res != NSERROR_OK) {
		return res;
	}

	if (cert_info->err != SSL_CERT_ERR_OK) {
		res = ssenddataf(ctx,
				 "<table class=\"info\">\n"
				 "<tr class=\"ns-even-fg-bad\">"
				 "<th>Fault</th>"
				 "<td>%s</td>"
				 "</tr>"
				 "</table>\n",
				 messages_get_sslcode(cert_info->err));
		if (res != NSERROR_OK) {
			return res;
		}
	}

	res = ssenddataf(ctx,
			 "<table class=\"info\">\n"
			 "<tr><th>Issued To</th><td><hr></td></tr>\n");
	if (res != NSERROR_OK) {
		return res;
	}

	res = format_certificate_name(ctx, &cert_info->subject_name);
	if (res != NSERROR_OK) {
		return res;
	}

	res = ssenddataf(ctx,
			 "</table>\n");
	if (res != NSERROR_OK) {
		return res;
	}

	res = ssenddataf(ctx,
			 "<table class=\"info\">\n"
			 "<tr><th>Issued By</th><td><hr></td></tr>\n");
	if (res != NSERROR_OK) {
		return res;
	}

	res = format_certificate_name(ctx, &cert_info->issuer_name);
	if (res != NSERROR_OK) {
		return res;
	}

	res = ssenddataf(ctx,
			 "</table>\n");
	if (res != NSERROR_OK) {
		return res;
	}

	res = ssenddataf(ctx,
			 "<table class=\"info\">\n"
			 "<tr><th>Validity</th><td><hr></td></tr>\n"
			 "<tr><th>Valid From</th><td>%s</td></tr>\n"
			 "<tr><th>Valid Until</th><td>%s</td></tr>\n"
			 "</table>\n",
			 cert_info->not_before,
			 cert_info->not_after);
	if (res != NSERROR_OK) {
		return res;
	}

	res = format_certificate_san(ctx, cert_info->san);
	if (res != NSERROR_OK) {
		return res;
	}

	res = format_certificate_public_key(ctx, &cert_info->public_key);
	if (res != NSERROR_OK) {
		return res;
	}

	res = ssenddataf(ctx,
			 "<table class=\"info\">\n"
			 "<tr><th>Miscellaneous</th><td><hr></td></tr>\n");
	if (res != NSERROR_OK) {
		return res;
	}

	if (cert_info->serialnum != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>Serial Number</th><td>%s</td></tr>\n",
				 cert_info->serialnum);
		if (res != NSERROR_OK) {
			return res;
		}
	}

	if (cert_info->sig_algor != NULL) {
		res = ssenddataf(ctx,
				 "<tr><th>Signature Algorithm</th>"
				 "<td>%s</td></tr>\n",
				 cert_info->sig_algor);
		if (res != NSERROR_OK) {
			return res;
		}
	}

	res = ssenddataf(ctx,
			 "<tr><th>Version</th><td>%ld</td></tr>\n"
			 "</table>\n",
			 cert_info->version);
	if (res != NSERROR_OK) {
		return res;
	}

	res = format_certificate_fingerprint(ctx, cert_info);
	if (res != NSERROR_OK) {
		return res;
	}

	return res;
}

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
			"</head>\n"
			"<body id=\"certificate\" class=\"ns-even-bg ns-even-fg ns-border\">\n"
			"<h1 class=\"ns-border\">Certificate</h1>\n");
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
		struct ns_cert_info *cert_info;
		res = convert_chain_to_cert_info(chain, &cert_info);
		if (res == NSERROR_OK) {
			size_t depth;
			res = ssenddataf(ctx, "<ul>\n");
			if (res != NSERROR_OK) {
				free_ns_cert_info(cert_info);
				goto fetch_about_certificate_handler_aborted;
			}

			for (depth = 0; depth < chain->depth; depth++) {
				res = ssenddataf(ctx, "<li><a href=\"#%"PRIsizet"\">%s</a></li>\n",
						depth, (cert_info + depth)
							->subject_name
								.common_name);
				if (res != NSERROR_OK) {
					free_ns_cert_info(cert_info);
					goto fetch_about_certificate_handler_aborted;
				}

			}

			res = ssenddataf(ctx, "</ul>\n");
			if (res != NSERROR_OK) {
				free_ns_cert_info(cert_info);
				goto fetch_about_certificate_handler_aborted;
			}

			for (depth = 0; depth < chain->depth; depth++) {
				res = format_certificate(ctx, cert_info + depth,
						depth);
				if (res != NSERROR_OK) {
					free_ns_cert_info(cert_info);
					goto fetch_about_certificate_handler_aborted;
				}

			}
			free_ns_cert_info(cert_info);

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
	bool even = false;

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
			"<body "
				"id =\"configlist\" "
				"class=\"ns-even-bg ns-even-fg ns-border\" "
				"style=\"overflow: hidden;\">\n"
			"<h1 class=\"ns-border\">NetSurf Browser Config</h1>\n"
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
		if (even) {
			elen = nsoption_snoptionf(buffer + slen,
					sizeof buffer - slen,
					opt_loop,
					"<tr>"
						"<th class=\"ns-border\">%k</th>"
						"<td class=\"ns-border\">%t</td>"
						"<td class=\"ns-border\">%p</td>"
						"<td class=\"ns-border\">%V</td>"
					"</tr>\n");
		} else {
			elen = nsoption_snoptionf(buffer + slen,
					sizeof buffer - slen,
					opt_loop,
					"<tr class=\"ns-odd-bg\">"
						"<th class=\"ns-border\">%k</th>"
						"<td class=\"ns-border\">%t</td>"
						"<td class=\"ns-border\">%p</td>"
						"<td class=\"ns-border\">%V</td>"
					"</tr>\n");
		}
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
			even = !even;
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
 * Handler to generate the nscolours stylesheet
 *
 * \param ctx The fetcher context.
 * \return true if handled false if aborted.
 */
static bool fetch_about_nscolours_handler(struct fetch_about_context *ctx)
{
	nserror res;
	const char *stylesheet;

	/* content is going to return ok */
	fetch_set_http_code(ctx->fetchh, 200);

	/* content type */
	if (fetch_about_send_header(ctx, "Content-Type: text/css; charset=utf-8")) {
		goto aborted;
	}

	res = nscolour_get_stylesheet(&stylesheet);
	if (res != NSERROR_OK) {
		goto aborted;
	}

	res = ssenddataf(ctx,
			"html {\n"
			"\tbackground-color: #%06x;\n"
			"}\n"
			"%s",
			colour_rb_swap(nscolours[NSCOLOUR_WIN_ODD_BG]),
			stylesheet);
	if (res != NSERROR_OK) {
		goto aborted;
	}

	fetch_about_send_finished(ctx);

	return true;

aborted:

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
			"<body class=\"ns-even-bg ns-even-fg ns-border\" id =\"authentication\">\n"
			"<h1 class=\"ns-border\">%s</h1>\n",
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
	const char *chainurl = NULL;
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
		} else if (strcmp(curmd->name, "chainurl") == 0) {
			chainurl = curmd->value;
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
			"<body class=\"ns-even-bg ns-even-fg ns-border\" id =\"privacy\">\n"
			"<h1 class=\"ns-border ns-odd-fg-bad\">%s</h1>\n",
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

	if (chainurl == NULL) {
		res = ssenddataf(ctx,
				 "<div><p>%s</p></div>"
				 "<div><p>%s</p></div>",
				 reason,
				 messages_get("ViewCertificatesNotPossible"));
	} else {
		res = ssenddataf(ctx,
				 "<div><p>%s</p></div>"
				 "<div><p><a href=\"%s\" target=\"_blank\">%s</a></p></div>",
				 reason,
				 chainurl,
				 messages_get("ViewCertificates"));
	}
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
			"<body class=\"ns-even-bg ns-even-fg ns-border\" id =\"timeout\">\n"
			"<h1 class=\"ns-border ns-odd-fg-bad\">%s</h1>\n",
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
			"<body class=\"ns-even-bg ns-even-fg ns-border\" id =\"fetcherror\">\n"
			"<h1 class=\"ns-border ns-odd-fg-bad\">%s</h1>\n",
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
		"nscolours.css",
		SLEN("nscolours.css"),
		NULL,
		fetch_about_nscolours_handler,
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
			"<title>List of NetSurf pages</title>\n"
			"<link rel=\"stylesheet\" type=\"text/css\" "
			"href=\"resource:internal.css\">\n"
			"</head>\n"
			"<body class=\"ns-even-bg ns-even-fg ns-border\">\n"
			"<h1 class =\"ns-border\">List of NetSurf pages</h1>\n"
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
