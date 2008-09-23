/*
 * Copyright 2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
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

#ifndef WITH_HUBBUB

#include <stdbool.h>
#include <string.h>

#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h>

#include "render/parser_binding.h"

#include "utils/log.h"
#include "utils/talloc.h"

typedef struct libxml_ctx {
	htmlParserCtxt *parser;

	/** HTML parser encoding handler. */
	xmlCharEncodingHandler *encoding_handler;

	const char *encoding;
	binding_encoding_source encoding_source;

	bool getenc;
} libxml_ctx;

static bool set_parser_encoding(libxml_ctx *c, const char *encoding);
static const char *detect_encoding(const char **data, size_t *size);

void *binding_create_tree(void *arena, const char *charset)
{
	libxml_ctx *ctx;

	ctx = malloc(sizeof(libxml_ctx));
	if (ctx == NULL)
		return NULL;

	ctx->parser = NULL;
	ctx->encoding_handler = NULL;
	ctx->encoding = charset;
	ctx->encoding_source = ENCODING_SOURCE_HEADER;
	ctx->getenc = true;

	ctx->parser = htmlCreatePushParserCtxt(0, 0, "", 0, 0, 
			XML_CHAR_ENCODING_NONE);
	if (ctx->parser == NULL) {
		free(ctx);
		return NULL;
	}

	if (ctx->encoding != NULL && !set_parser_encoding(ctx, charset)) {
		if (ctx->parser->myDoc != NULL)
			xmlFreeDoc(ctx->parser->myDoc);
		htmlFreeParserCtxt(ctx->parser);
		free(ctx);
		return NULL;
	}

	return (void *) ctx;
}

void binding_destroy_tree(void *ctx)
{
	libxml_ctx *c = (libxml_ctx *) ctx;

	if (ctx == NULL)
		return;

	if (c->parser->myDoc != NULL)
		xmlFreeDoc(c->parser->myDoc);

	if (c->parser != NULL)
		htmlFreeParserCtxt(c->parser);

	c->parser = NULL;
	c->encoding = NULL;

	free(c);
}

binding_error binding_parse_chunk(void *ctx, const uint8_t *data, size_t len)
{
	libxml_ctx *c = (libxml_ctx *) ctx;

	if (c->getenc) {
		/* No encoding was specified in the Content-Type header.
		 * Attempt to detect if the encoding is not 8-bit. If the
		 * encoding is 8-bit, leave the parser unchanged, so that it
		 * searches for a <meta http-equiv="content-type"
		 * content="text/html; charset=...">. */
		const char *encoding;
		encoding = detect_encoding((const char **) (void *) &data, 
				&len);
		if (encoding) {
			if (!set_parser_encoding(c, encoding))
				return BINDING_NOMEM;
			c->encoding = encoding;
			c->encoding_source = ENCODING_SOURCE_DETECTED;
		}
		c->getenc = false;

		/* The data we received may have solely consisted of a BOM.
		 * If so, it will have been stripped by html_detect_encoding.
		 * Therefore, we'll have nothing to do in that case. */
		if (len == 0)
			return BINDING_OK;
	}

	htmlParseChunk(c->parser, (const char *) data, len, 0);
	/** \todo error handling */

	if (!c->encoding && c->parser->input->encoding) {
		/* The encoding was not in headers or detected,
		 * and the parser found a <meta http-equiv="content-type"
		 * content="text/html; charset=...">. */

		/* However, if that encoding is non-ASCII-compatible,
		 * ignore it, as it can't possibly be correct */
		if (strncasecmp((const char *) c->parser->input->encoding,
				"UTF-16", 6) == 0 || /* UTF-16(LE|BE)? */
			strncasecmp((const char *) c->parser->input->encoding,
				"UTF-32", 6) == 0) { /* UTF-32(LE|BE)? */
			c->encoding = "ISO-8859-1";
			c->encoding_source = ENCODING_SOURCE_DETECTED;
		} else {
			c->encoding = (const char *) c->parser->input->encoding;
			c->encoding_source = ENCODING_SOURCE_META;
		}

		if (!c->encoding)
			return BINDING_NOMEM;

		/* have the encoding; don't attempt to detect it */
		c->getenc = false;

		return BINDING_ENCODINGCHANGE;
	}

	return BINDING_OK;
}

binding_error binding_parse_completed(void *ctx)
{
	libxml_ctx *c = (libxml_ctx *) ctx;

	htmlParseChunk(c->parser, "", 0, 1);
	/** \todo error handling */

	return BINDING_OK;
}

const char *binding_get_encoding(void *ctx, binding_encoding_source *source)
{
	libxml_ctx *c = (libxml_ctx *) ctx;

	*source = c->encoding_source;

	return c->encoding;
}

xmlDocPtr binding_get_document(void *ctx)
{
	libxml_ctx *c = (libxml_ctx *) ctx;
	xmlDocPtr doc = c->parser->myDoc;

	c->parser->myDoc = NULL;

	return doc;
}

/******************************************************************************/

/**
 * Set the HTML parser character encoding.
 *
 * \param  c         context
 * \param  encoding  name of encoding
 * \return  true on success, false on error and error reported
 */
bool set_parser_encoding(libxml_ctx *c, const char *encoding)
{
	xmlError *error;

	c->encoding_handler = xmlFindCharEncodingHandler(encoding);
	if (!c->encoding_handler) {
		/* either out of memory, or no handler available */
		/* assume no handler available, which is not a fatal error */
		LOG(("no encoding handler for \"%s\"", encoding));
		/* \todo  warn user and ask them to install iconv? */
		return true;
	}

	xmlCtxtResetLastError(c->parser);
	if (xmlSwitchToEncoding(c->parser, c->encoding_handler)) {
		error = xmlCtxtGetLastError(c->parser);
		LOG(("xmlSwitchToEncoding(): %s",
				error ? error->message : "failed"));
		return false;
	}

	/* Dirty hack to get around libxml oddness:
	 * 1) When creating a push parser context, the input flow's encoding
	 *    string is not set (whether an encoding is specified or not)
	 * 2) When switching encoding (as above), the input flow's encoding
	 *    string is never changed
	 * 3) When handling a meta charset, the input flow's encoding string
	 *    is checked to determine if an encoding has already been set.
	 *    If it has been set, then the meta charset is ignored.
	 *
	 * The upshot of this is that, if we don't explicitly set the input
	 * flow's encoding string here, any meta charset in the document
	 * will override our setting, which is incorrect behaviour.
	 *
	 * Ideally, this would be fixed in libxml, but that requires rather
	 * more knowledge than I currently have of what libxml is doing.
	 */
	if (!c->parser->input->encoding)
		c->parser->input->encoding =
				xmlStrdup((const xmlChar *) encoding);

	/* Ensure noone else attempts to reset the encoding */
	c->getenc = false;

	return true;
}

/**
 * Attempt to detect the encoding of some HTML data.
 *
 * \param  data  Pointer to HTML source data
 * \param  size  Pointer to length of data
 * \return  a constant string giving the encoding, or 0 if the encoding
 *          appears to be some 8-bit encoding
 *
 * If a BOM is encountered, *data and *size will be modified to skip over it
 */

const char *detect_encoding(const char **data, size_t *size)
{
	const unsigned char *d = (const unsigned char *) *data;

	/* this detection assumes that the first two characters are <= 0xff */
	if (*size < 4)
		return 0;

	if (d[0] == 0x00 && d[1] == 0x00 &&
			d[2] == 0xfe && d[3] == 0xff) { /* BOM 00 00 fe ff */
		*data += 4;
		*size -= 4;
		return "UTF-32BE";
	} else if (d[0] == 0xff && d[1] == 0xfe &&
			d[2] == 0x00 && d[3] == 0x00) { /* BOM ff fe 00 00 */
		*data += 4;
		*size -= 4;
		return "UTF-32LE";
	}
	else if (d[0] == 0x00 && d[1] != 0x00 &&
			d[2] == 0x00 && d[3] != 0x00)   /* 00 xx 00 xx */
		return "UTF-16BE";
	else if (d[0] != 0x00 && d[1] == 0x00 &&
			d[2] != 0x00 && d[3] == 0x00)   /* xx 00 xx 00 */
		return "UTF-16LE";
	else if (d[0] == 0x00 && d[1] == 0x00 &&
			d[2] == 0x00 && d[3] != 0x00)   /* 00 00 00 xx */
		return "ISO-10646-UCS-4";
	else if (d[0] != 0x00 && d[1] == 0x00 &&
			d[2] == 0x00 && d[3] == 0x00)   /* xx 00 00 00 */
		return "ISO-10646-UCS-4";
	else if (d[0] == 0xfe && d[1] == 0xff) {        /* BOM fe ff */
		*data += 2;
		*size -= 2;
		return "UTF-16BE";
	} else if (d[0] == 0xff && d[1] == 0xfe) {      /* BOM ff fe */
		*data += 2;
		*size -= 2;
		return "UTF-16LE";
	} else if (d[0] == 0xef && d[1] == 0xbb &&
			d[2] == 0xbf) {                 /* BOM ef bb bf */
		*data += 3;
		*size -= 3;
		return "UTF-8";
	}

	return 0;
}

#endif

