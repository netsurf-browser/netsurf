/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Save HTML document with dependencies (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>
#include "libxml/HTMLtree.h"
#include "oslib/osfile.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/render/box.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/save_complete.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_SAVE_COMPLETE

/** \todo URL rewriting for @import rules
 *        Objects used by embedded html pages
 *        GUI
 */

regex_t save_complete_import_re;

/** An entry in save_complete_list. */
struct save_complete_entry {
        char *url;   /**< Fully qualified URL, as per url_join output */
        int ptr;    /**< Pointer to object's location in memory */
        struct save_complete_entry *next; /**< Next entry in list */
};
/** List of urls seen and saved so far. */
static struct save_complete_entry *save_complete_list;


static void save_imported_sheets(struct content *c, const char *path);
static char * rewrite_stylesheet_urls(const char *source, unsigned int size,
		int *osize, const char *base);
static int rewrite_document_urls(xmlDoc *doc, const char *base);
static int rewrite_urls(xmlNode *n, const char *base);
static void rewrite_url(xmlNode *n, const char *attr, const char *base);
static void save_complete_add_url(const char *url, int id);
static int save_complete_find_url(const char *url);


/**
 * Save an HTML page with all dependencies.
 *
 * \param  c     CONTENT_HTML to save
 * \param  path  directory to save to (must exist)
 */

void save_complete(struct content *c, const char *path)
{
	char spath[256];
	unsigned int i;
	htmlParserCtxtPtr toSave;

	if (c->type != CONTENT_HTML)
		return;

	save_complete_list = 0;

        /* save stylesheets, ignoring the base sheet and <style> elements */
        for (i = 2; i != c->data.html.stylesheet_count; i++) {
		struct content *css = c->data.html.stylesheet_content[i];
		char *source;
		int source_len;

                if (!css)
                        continue;

		save_complete_add_url(css->url, (int) css);

                save_imported_sheets(css, path);

		snprintf(spath, sizeof spath, "%s.%x", path,
				(unsigned int) css);
		source = rewrite_stylesheet_urls(css->source_data,
				css->source_size, &source_len, css->url);
		if (source) {
			xosfile_save_stamped(spath, 0xf79, source,
					source + source_len);
			free(source);
		}
        }

	/* save objects */
	for (i = 0; i != c->data.html.object_count; i++) {
		struct content *obj = c->data.html.object[i].content;

		/* skip difficult content types */
		if (!obj || obj->type >= CONTENT_OTHER || !obj->source_data)
			continue;

		save_complete_add_url(obj->url, (int) obj);

		snprintf(spath, sizeof spath, "%s.%x", path,
				(unsigned int) obj);
		xosfile_save_stamped(spath,
				ro_content_filetype(obj),
				obj->source_data,
				obj->source_data + obj->source_size);
	}

	/* make a copy of the document tree */
	toSave = htmlCreateMemoryParserCtxt(c->source_data, c->source_size);
	htmlParseDocument(toSave);

	/* rewrite all urls we know about */
       	if (rewrite_document_urls(toSave->myDoc, c->data.html.base_url) == 0) {
       	        xfree(spath);
       	        xmlFreeDoc(toSave->myDoc);
       	        htmlFreeParserCtxt(toSave);
       	        return;
       	}

	/* save the html file out last of all */
	snprintf(spath, sizeof spath, "%s.index", path);
	htmlSaveFile(spath, toSave->myDoc);
	xosfile_set_type(spath, 0xfaf);

        xmlFreeDoc(toSave->myDoc);
        htmlFreeParserCtxt(toSave);

	/* free save_complete_list */
	while (save_complete_list) {
		struct save_complete_entry *next = save_complete_list->next;
		free(save_complete_list->url);
		free(save_complete_list);
		save_complete_list = next;
	}
}


void save_imported_sheets(struct content *c, const char *path)
{
	char spath[256];
        unsigned int j;
        char *source;
        int source_len;

        for (j = 0; j != c->data.css.import_count; j++) {
		struct content *css = c->data.css.import_content[j];

                if (!css)
                        continue;

		save_complete_add_url(css->url, (int) css);

                save_imported_sheets(css, path);

		snprintf(spath, sizeof spath, "%s.%x", path,
				(unsigned int) css);
		source = rewrite_stylesheet_urls(css->source_data,
				css->source_size, &source_len, css->url);
		if (source) {
			xosfile_save_stamped(spath, 0xf79, source,
					source + source_len);
			free(source);
		}
        }
}


/**
 * Initialise the save_complete module.
 */

void save_complete_init(void)
{
	/* Match an @import rule - see CSS 2.1 G.1. */
	regcomp_wrapper(&save_complete_import_re,
			"@import"		/* IMPORT_SYM */
			"[ \t\r\n\f]*"		/* S* */
			/* 1 */
			"("			/* [ */
			/* 2 3 */
			"\"(([^\"]|[\\]\")*)\""	/* STRING (approximated) */
			"|"
			/* 4 5 */
			"'(([^']|[\\]')*)'"
			"|"			/* | */
			"url\\([ \t\r\n\f]*"	/* URI (approximated) */
			     /* 6 7 */
			     "\"(([^\"]|[\\]\")*)\""
			     "[ \t\r\n\f]*\\)"
			"|"
			"url\\([ \t\r\n\f]*"
			    /* 8 9 */
			     "'(([^']|[\\]')*)'"
			     "[ \t\r\n\f]*\\)"
			"|"
			"url\\([ \t\r\n\f]*"
			   /* 10 */
			     "([^) \t\r\n\f]*)"
			     "[ \t\r\n\f]*\\)"
			")",			/* ] */
			REG_EXTENDED | REG_ICASE);
}


/**
 * Rewrite stylesheet @import rules for save complete.
 *
 * @param  source  stylesheet source
 * @param  size    size of source
 * @param  osize   updated with the size of the result
 * @param  head    pointer to the head of the list containing imported urls
 * @param  base    url of stylesheet
 * @return  converted source, or 0 on error
 */

char * rewrite_stylesheet_urls(const char *source, unsigned int size,
		int *osize, const char *base)
{
	char *res;
	const char *url;
	char *url2;
	char buf[20];
	unsigned int offset = 0;
	int url_len = 0;
	int id;
	int m;
	unsigned int i;
	unsigned int imports = 0;
	regmatch_t match[11];

	/* count number occurences of @import to (over)estimate result size */
	/* can't use strstr because source is not 0-terminated string */
	for (i = 0; 7 < size && i != size - 7; i++) {
		if (source[i] == '@' &&
				tolower(source[i + 1]) == 'i' &&
				tolower(source[i + 2]) == 'm' &&
				tolower(source[i + 3]) == 'p' &&
				tolower(source[i + 4]) == 'o' &&
				tolower(source[i + 5]) == 'r' &&
				tolower(source[i + 6]) == 't')
			imports++;
	}

	res = malloc(size + imports * 20);
	if (!res) {
		warn_user("NoMemory");
		return 0;
	}
	*osize = 0;

	while (offset < size) {
		m = regexec(&save_complete_import_re, source + offset,
				11, match, 0);
		if (m)
			break;

		/*for (unsigned int i = 0; i != 11; i++) {
			if (match[i].rm_so == -1)
				continue;
			fprintf(stderr, "%i: '%.*s'\n", i,
					match[i].rm_eo - match[i].rm_so,
					source + offset + match[i].rm_so);
		}*/

		url = 0;
		if (match[2].rm_so != -1) {
			url = source + offset + match[2].rm_so;
			url_len = match[2].rm_eo - match[2].rm_so;
		} else if (match[4].rm_so != -1) {
			url = source + offset + match[4].rm_so;
			url_len = match[4].rm_eo - match[4].rm_so;
		} else if (match[6].rm_so != -1) {
			url = source + offset + match[6].rm_so;
			url_len = match[6].rm_eo - match[6].rm_so;
		} else if (match[8].rm_so != -1) {
			url = source + offset + match[8].rm_so;
			url_len = match[8].rm_eo - match[8].rm_so;
		} else if (match[10].rm_so != -1) {
			url = source + offset + match[10].rm_so;
			url_len = match[10].rm_eo - match[10].rm_so;
		}
		assert(url);

		url2 = strndup(url, url_len);
		if (!url2) {
			warn_user("NoMemory");
			free(res);
			return 0;
		}
		url = url_join(url2, base);
		free(url2);
		if (!url) {
			warn_user("NoMemory");
			free(res);
			return 0;
                }

		/* copy data before match */
		memcpy(res + *osize, source + offset, match[0].rm_so);
		*osize += match[0].rm_so;

		id = save_complete_find_url(url);
		if (id) {
			/* replace import */
			sprintf(buf, "@import '%x'", id);
			memcpy(res + *osize, buf, strlen(buf));
			*osize += strlen(buf);
		} else {
			/* copy import */
			memcpy(res + *osize, source + offset + match[0].rm_so,
					match[0].rm_eo - match[0].rm_so);
			*osize += match[0].rm_eo - match[0].rm_so;
		}

		assert(0 < match[0].rm_eo);
		offset += match[0].rm_eo;
	}

	/* copy rest of source */
	if (offset < size) {
		memcpy(res + *osize, source + offset, size - offset);
		*osize += size - offset;
	}

	return res;
}


/**
 * Rewrite URLs in a HTML document to be relative
 *
 * @param doc The root of the document tree
 * @return 0 on error. >0 otherwise
 * \param  base  base url of document
 */

int rewrite_document_urls(xmlDoc *doc, const char *base)
{
        xmlNode *html;

        /* find the html element */
        for (html = doc->children;
             html!=0 && html->type != XML_ELEMENT_NODE;
             html = html->next)
                ;
        if (html == 0 || strcmp((const char*)html->name, "html") != 0) {
                return 0;
        }

	rewrite_urls(html, base);

        return 1;
}


/**
 * Traverse tree, rewriting URLs as we go.
 *
 * @param n The root of the tree
 * @return 0 on error. >0 otherwise
 * \param  base  base url of document
 */

int rewrite_urls(xmlNode *n, const char *base)
{
        xmlNode *this;

        /**
         * We only need to consider the following cases:
         *
         * Attribute:      Elements:
         *
         * 1)   data         <object>
         * 2)   href         <a> <area> <link> <base>
         * 3)   src          <script> <input> <frame> <iframe> <img>
         */
        if (n->type == XML_ELEMENT_NODE) {
                /* 1 */
                if (strcmp(n->name, "object") == 0) {
                      rewrite_url(n, "data", base);
                }
                /* 2 */
                else if (strcmp(n->name, "a") == 0 ||
                         strcmp(n->name, "area") == 0 ||
                         strcmp(n->name, "link") == 0 ||
                         strcmp(n->name, "base") == 0) {
                      rewrite_url(n, "href", base);
                }
                /* 3 */
                else if (strcmp(n->name, "frame") == 0 ||
                         strcmp(n->name, "iframe") == 0 ||
                         strcmp(n->name, "input") == 0 ||
                         strcmp(n->name, "img") == 0 ||
                         strcmp(n->name, "script") == 0) {
                      rewrite_url(n, "src", base);
                }
        }
        else {
                return 0;
        }

        /* now recurse */
	for (this = n->children; this != 0; this = this->next) {
                rewrite_urls(this, base);
	}

        return 1;
}


/**
 * Rewrite an URL in a HTML document.
 *
 * \param  n     The node to modify
 * \param  attr  The html attribute to modify
 * \param  base  base url of document
 */

void rewrite_url(xmlNode *n, const char *attr, const char *base)
{
        char *url, *data;
        char rel[256];
	int id;

        data = xmlGetProp(n, (const xmlChar*)attr);

        if (!data) return;

        url = url_join(data, base);
        if (!url) {
                xmlFree(data);
                return;
        }

	id = save_complete_find_url(url);
	if (id) {
		/* found a match */
		snprintf(rel, sizeof rel, "%x", id);
		xmlSetProp(n, (const xmlChar *) attr, (xmlChar *) rel);
	} else {
		/* no match found */
		xmlSetProp(n, (const xmlChar *) attr, (xmlChar *) url);
	}

        free(url);
        xmlFree(data);
}


/**
 * Add a url to the save_complete_list.
 *
 * \param  url  url to add (copied)
 * \param  id   id to use for url
 */

void save_complete_add_url(const char *url, int id)
{
	struct save_complete_entry *entry;
	entry = malloc(sizeof (*entry));
	if (!entry)
		return;
	entry->url = strdup(url);
	if (!url) {
		free(entry);
		return;
	}
	entry->ptr = id;
	entry->next = save_complete_list;
	save_complete_list = entry;
}


/**
 * Look up a url in the save_complete_list.
 *
 * \param  url   url to find
 * \param  len   length of url
 * \return  id to use for url, or 0 if not present
 */

int save_complete_find_url(const char *url)
{
	struct save_complete_entry *entry;
	for (entry = save_complete_list; entry; entry = entry->next)
		if (strcmp(url, entry->url) == 0)
			break;
	if (entry)
		return entry->ptr;
	return 0;
}

#endif
