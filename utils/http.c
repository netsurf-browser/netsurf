/*
 * Copyright 2010 John-Mark Bell <jmb@netsurf-browser.org>
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

/** \file
 * HTTP header parsing functions
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/http.h"

/**
 * Representation of an HTTP parameter
 */
struct http_parameter {
	struct http_parameter *next;	/**< Next parameter in list, or NULL */

	char *name;			/**< Parameter name */
	char *value;			/**< Parameter value */
};

/**
 * Determine if a character is valid for an HTTP token
 *
 * \param c  Character to consider
 * \return True if character is valid, false otherwise
 */
static bool http_is_token_char(uint8_t c)
{
	/* [ 32 - 126 ] except ()<>@,;:\"/[]?={} SP HT */

	if (c <= ' ' || 126 < c)
		return false;

	return (strchr("()<>@,;:\\\"/[]?={}", c) == NULL);
}

/**
 * Parse an HTTP token
 *
 * \param input  Pointer to current input byte. Updated on exit.
 * \param value  Pointer to location to receive on-heap token value.
 * \return NSERROR_OK on success,
 * 	   NSERROR_NOMEM on memory exhaustion
 *
 * The returned value is owned by the caller
 */
static nserror http_parse_token(const char **input, char **value)
{
	const uint8_t *start = (const uint8_t *) *input;
	const uint8_t *end;
	char *token;

	end = start;
	while (http_is_token_char(*end))
		end++;

	token = malloc(end - start + 1);
	if (token == NULL)
		return NSERROR_NOMEM;

	memcpy(token, start, end - start);
	token[end - start] = '\0';

	*value = token;
	*input = (const char *) end;

	return NSERROR_OK;
}

/**
 * Parse an HTTP quoted-string
 *
 * \param input  Pointer to current input byte. Updated on exit.
 * \param value  Pointer to location to receive on-heap string value.
 * \return NSERROR_OK on success,
 * 	   NSERROR_NOMEM on memory exhaustion
 *
 * The returned value is owned by the caller
 */
static nserror http_parse_quoted_string(const char **input, char **value)
{
	const uint8_t *start = (const uint8_t *) *input;
	const uint8_t *end;
	uint8_t c;
	char *string_value;

	/* <"> *( qdtext | quoted-pair ) <">
	 * qdtext = any TEXT except <">
	 * quoted-pair = "\" CHAR
	 * TEXT = [ HT, CR, LF, 32-126, 128-255 ]
	 * CHAR = [ 0 - 127 ]
	 *
	 * \todo TEXT may contain non 8859-1 chars encoded per RFC 2047
	 * \todo Support quoted-pairs
	 */

	if (*start == '"') {
		end = start = start + 1;

		c = *end;
		while (c == '\t' || c == '\r' || c == '\n' || 
				c == ' ' || c == '!' ||
				('#' <= c && c <= 126) || c > 127) {
			end++;
			c = *end;
		}

		if (*end != '"') {
			start--;
			end = start;
		}
	}

	string_value = malloc(end - start + 1);
	if (string_value == NULL)
		return NSERROR_NOMEM;

	memcpy(string_value, start, end - start);
	string_value[end - start] = '\0';

	*value = string_value;

	if (end != start)
		*input = (const char *) end + 1;

	return NSERROR_OK;
}

/**
 * Parse an HTTP parameter
 *
 * \param input      Pointer to current input byte. Updated on exit.
 * \param parameter  Pointer to location to receive on-heap parameter.
 * \return NSERROR_OK on success,
 * 	   NSERROR_NOMEM on memory exhaustion
 *
 * The returned parameter is owned by the caller.
 */
static nserror http_parse_parameter(const char **input,
		http_parameter **parameter)
{
	const char *pos = *input;
	char *name;
	char *value;
	http_parameter *param;
	nserror error;

	/* token "=" ( token | quoted-string ) */

	error = http_parse_token(&pos, &name);
	if (error != NSERROR_OK)
		return error;

	while (*pos == ' ' || *pos == '\t')
		pos++;

	if (*pos != '=') {
		value = strdup("");
		if (value == NULL) {
			free(name);
			return NSERROR_NOMEM;
		}
	} else {
		pos++;

		while (*pos == ' ' || *pos == '\t')
			pos++;

		if (*pos == '"')
			error = http_parse_quoted_string(&pos, &value);
		else
			error = http_parse_token(&pos, &value);

		if (error != NSERROR_OK) {
			free(name);
			return error;
		}
	}

	param = malloc(sizeof(*param));
	if (param == NULL) {
		free(value);
		free(name);
		return NSERROR_NOMEM;
	}

	param->next = NULL;
	param->name = name;
	param->value = value;

	*parameter = param;
	*input = pos;

	return NSERROR_OK;
}

/**
 * Parse an HTTP parameter list
 *
 * \param input       Pointer to current input byte. Updated on exit.
 * \param parameters  Pointer to location to receive on-heap parameter list.
 * \return NSERROR_OK on success,
 * 	   NSERROR_NOMEM on memory exhaustion
 *
 * The returned parameter list is owned by the caller
 */
static nserror http_parse_parameter_list(const char **input, 
		http_parameter **parameters)
{
	const char *pos = *input;
	http_parameter *param;
	http_parameter *list = NULL;
	nserror error;

	/* 1*( ";" parameter ) */

	while (*pos == ';') {
		pos++;

		while (*pos == ' ' || *pos == '\t')
			pos++;

		error = http_parse_parameter(&pos, &param);
		if (error != NSERROR_OK) {
			while (list != NULL) {
				param = list;

				list = param->next;

				free(param->name);
				free(param->value);
				free(param);
			}
			return error;
		}

		if (list != NULL)
			param->next = list;

		list = param;

		while (*pos == ' ' || *pos == '\t')
			pos++;
	}

	*parameters = list;
	*input = pos;

	return NSERROR_OK;
}

/* See http.h for documentation */
nserror http_parse_content_type(const char *header_value, char **media_type, 
		http_parameter **parameters)
{
	const char *pos = header_value;
	char *type;
	char *subtype = NULL;
	http_parameter *params = NULL;
	char *mime;
	size_t mime_len;
	nserror error;

	/* type "/" subtype *( ";" parameter ) */

	while (*pos == ' ' || *pos == '\t')
		pos++;

	error = http_parse_token(&pos, &type);
	if (error != NSERROR_OK)
		return error;

	while (*pos == ' ' || *pos == '\t')
		pos++;

	if (*pos == '/') {
		pos++;

		while (*pos == ' ' || *pos == '\t')
			pos++;

		error = http_parse_token(&pos, &subtype);
		if (error != NSERROR_OK) {
			free(type);
			return error;
		}

		while (*pos == ' ' || *pos == '\t')
			pos++;

		if (*pos == ';') {
			pos++;

			while (*pos == ' ' || *pos == '\t')
				pos++;

			error = http_parse_parameter_list(&pos, &params);
			if (error != NSERROR_OK) {
				free(subtype);
				free(type);
				return error;
			}
		}
	}

	/* <type> + <subtype> + '/' */
	mime_len = strlen(type) + (subtype != NULL ? strlen(subtype) : 0) + 1;

	mime = malloc(mime_len + 1);
	if (mime == NULL) {
		http_parameter_list_destroy(params);
		free(subtype);
		free(type);
		return NSERROR_OK;
	}

	sprintf(mime, "%s/%s", type, subtype != NULL ? subtype : "");

	free(subtype);
	free(type);

	*media_type = mime;
	*parameters = params;

	return NSERROR_OK;
}

/* See http.h for documentation */
nserror http_parameter_list_find_item(const http_parameter *list,
		const char *name, const char **value)
{
	while (list != NULL && strcasecmp(name, list->name) != 0)
		list = list->next;

	if (list == NULL)
		return NSERROR_NOT_FOUND;

	*value = list->value;

	return NSERROR_OK;
}

/* See http.h for documentation */
const http_parameter *http_parameter_list_iterate(const http_parameter *cur,
		const char **name, const char **value)
{
	if (cur == NULL)
		return NULL;

	*name = cur->name;
	*value = cur->value;

	return cur->next;
}

/* See http.h for documentation */
void http_parameter_list_destroy(http_parameter *list)
{
	while (list != NULL) {
		http_parameter *victim = list;

		list = victim->next;

		free(victim->name);
		free(victim->value);
		free(victim);
	}
}

