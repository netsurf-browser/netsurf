/*
 * Copyright 2021 Vincent Sanders <vince@netsurf-browser.org>
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
#include <string.h>

extern "C" {

#include "utils/errors.h"
#include "utils/filepath.h"

}

#include "qt/resources.h"

/** resource search path vector */
char **respaths;

/** maximum number of languages in language vector */
#define LANGV_SIZE 32
/** maximum length of all strings in language vector */
#define LANGS_SIZE 4096

/**
 * obtain language from environment
 *
 * start with GNU extension LANGUAGE environment variable and then try
 * POSIX variables LC_ALL, LC_MESSAGES and LANG
 *
 */
static const char *get_language(void)
{
	const char *lang;

	lang = getenv("LANGUAGE");
	if ((lang != NULL) && (lang[0] != '\0')) {
		return lang;
	}

	lang = getenv("LC_ALL");
	if ((lang != NULL) && (lang[0] != '\0')) {
		return lang;
	}

	lang = getenv("LC_MESSAGES");
	if ((lang != NULL) && (lang[0] != '\0')) {
		return lang;
	}

	lang = getenv("LANG");
	if ((lang != NULL) && (lang[0] != '\0')) {
		return lang;
	}

	return NULL;
}


/** provide a string vector of languages in preference order
 *
 * environment variables are processed to aquire a colon separated
 * list of languages which are converted into a string vector. The
 * vector will always have the C language as its last entry.
 *
 * This implementation creates an internal static representation of
 * the vector when first called and returns that for all subsequent
 * calls. i.e. changing the environment does not change the returned
 * vector on repeated calls.
 *
 * If the environment variables have more than LANGV_SIZE languages or
 * LANGS_SIZE bytes of data the results list will be curtailed.
 */
static const char * const *get_languagev(void)
{
	static const char *langv[LANGV_SIZE];
	int langidx = 0; /* index of next entry in vector */
	static char langs[LANGS_SIZE];
	char *curp; /* next language parameter in langs string */
	const char *lange; /* language from environment variable */
	int lang_len;
	char *cln; /* colon in lange */

	/* return cached vector */
	if (langv[0] != NULL) {
		return &langv[0];
	}

	curp = &langs[0];

	lange = get_language();

	if (lange != NULL) {
		lang_len = strlen(lange) + 1;
		if (lang_len < (LANGS_SIZE - 2)) {
			memcpy(curp, lange, lang_len);
			while ((curp[0] != 0) &&
			       (langidx < (LANGV_SIZE - 2))) {
				/* avoid using strchrnul as it is not portable */
				cln = strchr(curp, ':');
				if (cln == NULL) {
					langv[langidx++] = curp;
					curp += lang_len;
					break;
				} else {
					if ((cln - curp) > 1) {
						/* only place non empty entries in vector */
						langv[langidx++] = curp;
					}
					*cln++ = 0; /* null terminate */
					lang_len -= (cln - curp);
					curp = cln;
				}
			}
		}
	}

	/* ensure C language is present */
	langv[langidx++] = curp;
	*curp++ = 'C';
	*curp++ = 0;
	langv[langidx] = NULL;

	return &langv[0];
}


/* exported interface documented in qt/resources.h */
nserror nsqt_init_resource_path(const char *resource_path)
{
	const char * const *langv;
	char **pathv; /* resource path string vector */

	pathv = filepath_path_to_strvec(resource_path);

	langv = get_languagev();

	respaths = filepath_generate(pathv, langv);

	filepath_free_strvec(pathv);

	return NSERROR_OK;
}
