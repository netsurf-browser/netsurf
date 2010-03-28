/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004-2007 James Bursa <bursa@users.sourceforge.net>
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
 * Save HTML document with dependencies (implementation).
 */

#include "utils/config.h"

#define _GNU_SOURCE /* for strndup */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>
#include <libxml/HTMLtree.h>
#include <libxml/parserInternals.h>
#include "utils/config.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "css/css.h"
#include "render/box.h"
#include "desktop/save_complete.h"
#include "utils/log.h"
#include "utils/url.h"
#include "utils/utils.h"

regex_t save_complete_import_re;

/** An entry in save_complete_list. */
struct save_complete_entry {
	hlcache_handle *content;
	struct save_complete_entry *next; /**< Next entry in list */
};

static bool save_complete_html(hlcache_handle *c, const char *path,
		bool index, struct save_complete_entry **list);
static bool save_imported_sheets(struct nscss_import *imports, uint32_t count, 
		const char *path, struct save_complete_entry **list);
static char * rewrite_stylesheet_urls(const char *source, unsigned int size,
		int *osize, const char *base,
		struct save_complete_entry *list);
static bool rewrite_document_urls(xmlDoc *doc, const char *base,
		struct save_complete_entry *list);
static bool rewrite_urls(xmlNode *n, const char *base,
		struct save_complete_entry *list);
static bool rewrite_url(xmlNode *n, const char *attr, const char *base,
		struct save_complete_entry *list);
static bool save_complete_list_add(hlcache_handle *content,
		struct save_complete_entry **list);
static hlcache_handle * save_complete_list_find(const char *url,
		struct save_complete_entry *list);
static bool save_complete_list_check(hlcache_handle *content,
		struct save_complete_entry *list);
/* static void save_complete_list_dump(void); */
static bool save_complete_inventory(const char *path,
		struct save_complete_entry *list);

/**
 * Save an HTML page with all dependencies.
 *
 * \param  c     CONTENT_HTML to save
 * \param  path  directory to save to (must exist)
 * \return  true on success, false on error and error reported
 */

bool save_complete(hlcache_handle *c, const char *path)
{
	bool result;
	struct save_complete_entry *list = NULL;
	
	result = save_complete_html(c, path, true, &list);

	if (result)
		result = save_complete_inventory(path, list);

	/* free save_complete_list */
	while (list) {
		struct save_complete_entry *next = list->next;
		free(list);
		list = next;
	}

	return result;
}


/**
 * Save an HTML page with all dependencies, recursing through imported pages.
 *
 * \param  c      CONTENT_HTML to save
 * \param  path   directory to save to (must exist)
 * \param  index  true to save as "index"
 * \return  true on success, false on error and error reported
 */

bool save_complete_html(hlcache_handle *c, const char *path, bool index,
		struct save_complete_entry **list)
{
	struct html_stylesheet *sheets;
	struct content_html_object *objects;
	const char *base_url;
	char filename[256];
	unsigned int i, count;
	xmlDocPtr doc;
	bool res;

	if (content_get_type(c) != CONTENT_HTML)
		return false;

	if (save_complete_list_check(c, *list))
		return true;

	base_url = html_get_base_url(c);

	/* save stylesheets, ignoring the base and adblocking sheets */
	sheets = html_get_stylesheets(c, &count);

	for (i = STYLESHEET_START; i != count; i++) {
		hlcache_handle *css;
		const char *css_data;
		unsigned long css_size;
		char *source;
		int source_len;
		struct nscss_import *imports;
		uint32_t import_count;

		if (sheets[i].type == HTML_STYLESHEET_INTERNAL) {
			if (save_imported_sheets(
					sheets[i].data.internal->imports, 
					sheets[i].data.internal->import_count, 
					path, list) == false)
				return false;

			continue;
		}

		css = sheets[i].data.external;

		if (!css)
			continue;
		if (save_complete_list_check(css, *list))
			continue;

		if (!save_complete_list_add(css, list)) {
			warn_user("NoMemory", 0);
			return false;
		}

		imports = nscss_get_imports(css, &import_count);
		if (!save_imported_sheets(imports, import_count, path, list))
			return false;

		snprintf(filename, sizeof filename, "%p", css);

		css_data = content_get_source_data(css, &css_size);

		source = rewrite_stylesheet_urls(css_data, css_size, 
				&source_len, content_get_url(css),
				*list);
		if (!source) {
			warn_user("NoMemory", 0);
			return false;
		}
		res = save_complete_gui_save(path, filename, source_len,
				source, CONTENT_CSS);
		free(source);
		if (res == false)
			return false;
	}
	
	/* save objects */
	objects = html_get_objects(c, &count);

	for (i = 0; i != count; i++) {
		hlcache_handle *obj = objects[i].content;
		const char *obj_data;
		unsigned long obj_size;

		if (obj == NULL || content_get_type(obj) >= CONTENT_OTHER)
			continue;

		obj_data = content_get_source_data(obj, &obj_size);

		if (obj_data == NULL)
			continue;

		if (save_complete_list_check(obj, *list))
			continue;

		if (!save_complete_list_add(obj, list)) {
			warn_user("NoMemory", 0);
			return false;
		}

		if (content_get_type(obj) == CONTENT_HTML) {
			if (!save_complete_html(obj, path, false, list))
				return false;
			continue;
		}

		snprintf(filename, sizeof filename, "%p", obj);
		res = save_complete_gui_save(path, filename, 
				obj_size, obj_data, content_get_type(obj));
		if(res == false)
			return false;
	}

	/*save_complete_list_dump();*/

	/* copy document */
	doc = xmlCopyDoc(html_get_document(c), 1);
	if (doc == NULL) {
		warn_user("NoMemory", 0);
		return false;
	}

	/* rewrite all urls we know about */
	if (!rewrite_document_urls(doc, html_get_base_url(c), *list)) {
		xmlFreeDoc(doc);
		warn_user("NoMemory", 0);
		return false;
	}

	/* save the html file out last of all */
	if (index)
		snprintf(filename, sizeof filename, "index");
	else 
		snprintf(filename, sizeof filename, "%p", c);

	errno = 0;
	if (save_complete_htmlSaveFileFormat(path, filename, doc, 0, 0) == -1) {
		if (errno)
			warn_user("SaveError", strerror(errno));
		else
			warn_user("SaveError", "htmlSaveFileFormat failed");

		xmlFreeDoc(doc);
		return false;
	}	

	xmlFreeDoc(doc);

	return true;
}


/**
 * Save stylesheets imported by a CONTENT_CSS.
 *
 * \param imports  Array of imports
 * \param count    Number of imports in list
 * \param path     Path to save to
 * \return  true on success, false on error and error reported
 */
bool save_imported_sheets(struct nscss_import *imports, uint32_t count, 
		const char *path, struct save_complete_entry **list)
{
	char filename[256];
	unsigned int j;
	char *source;
	int source_len;
	bool res;

	for (j = 0; j != count; j++) {
		hlcache_handle *css = imports[j].c;
		const char *css_data;
		unsigned long css_size;
		struct nscss_import *child_imports;
		uint32_t child_import_count;

		if (css == NULL)
			continue;
		if (save_complete_list_check(css, *list))
			continue;

		if (!save_complete_list_add(css, list)) {
			warn_user("NoMemory", 0);
			return false;
		}

		child_imports = nscss_get_imports(css, &child_import_count);
		if (!save_imported_sheets(child_imports, child_import_count, 
				path, list))
			return false;

		snprintf(filename, sizeof filename, "%p", css);

		css_data = content_get_source_data(css, &css_size);

		source = rewrite_stylesheet_urls(css_data, css_size, 
				&source_len, content_get_url(css), 
				*list);
		if (!source) {
			warn_user("NoMemory", 0);
			return false;
		}

		res = save_complete_gui_save(path, filename, source_len,
				source, CONTENT_CSS);
		free(source);
		if (res == false)
			return false;
	}

	return true;
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
 * Rewrite stylesheet \@import rules for save complete.
 *
 * @param  source  stylesheet source
 * @param  size    size of source
 * @param  osize   updated with the size of the result
 * @param  base    url of stylesheet
 * @return  converted source, or 0 on out of memory
 */

char * rewrite_stylesheet_urls(const char *source, unsigned int size,
		int *osize, const char *base,
		struct save_complete_entry *list)
{
	char *res;
	const char *url;
	char *url2;
	char buf[20];
	unsigned int offset = 0;
	int url_len = 0;
	hlcache_handle *content;
	int m;
	unsigned int i;
	unsigned int imports = 0;
	regmatch_t match[11];
	url_func_result result;

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
	if (!res)
		return 0;
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
			free(res);
			return 0;
		}
		result = url_join(url2, base, (char**)&url);
		free(url2);
		if (result == URL_FUNC_NOMEM) {
			free(res);
			return 0;
		}

		/* copy data before match */
		memcpy(res + *osize, source + offset, match[0].rm_so);
		*osize += match[0].rm_so;

		if (result == URL_FUNC_OK) {
			content = save_complete_list_find(url, list);
			if (content) {
				/* replace import */
				snprintf(buf, sizeof buf, "@import '%p'",
						content);
				memcpy(res + *osize, buf, strlen(buf));
				*osize += strlen(buf);
			} else {
				/* copy import */
				memcpy(res + *osize, source + offset + match[0].rm_so,
					match[0].rm_eo - match[0].rm_so);
				*osize += match[0].rm_eo - match[0].rm_so;
			}
		}
		else {
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
 * Rewrite URLs in a HTML document to be relative.
 *
 * \param  doc   root of the document tree
 * \param  base  base url of document
 * \return  true on success, false on out of memory
 */

bool rewrite_document_urls(xmlDoc *doc, const char *base,
		struct save_complete_entry *list)
{
	xmlNode *node;

	for (node = doc->children; node; node = node->next)
		if (node->type == XML_ELEMENT_NODE)
			if (!rewrite_urls(node, base, list))
				return false;

	return true;
}


/**
 * Traverse tree, rewriting URLs as we go.
 *
 * \param  n     xmlNode of type XML_ELEMENT_NODE to rewrite
 * \param  base  base url of document
 * \return  true on success, false on out of memory
 *
 * URLs in the tree rooted at element n are rewritten.
 */

bool rewrite_urls(xmlNode *n, const char *base,
		struct save_complete_entry *list)
{
	xmlNode *child;

	assert(n->type == XML_ELEMENT_NODE);

	/**
	 * We only need to consider the following cases:
	 *
	 * Attribute:      Elements:
	 *
	 * 1)   data         <object>
	 * 2)   href         <a> <area> <link>
	 * 3)   src          <script> <input> <frame> <iframe> <img>
	 * 4)   n/a          <style>
	 * 5)   n/a          any <base> tag
	 * 6)   background   any (except those above)
	 */
	if (!n->name) {
		/* ignore */
	}
	/* 1 */
	else if (strcmp((const char *) n->name, "object") == 0) {
		if (!rewrite_url(n, "data", base, list))
			return false;
	}
	/* 2 */
	else if (strcmp((const char *) n->name, "a") == 0 ||
			strcmp((const char *) n->name, "area") == 0 ||
			strcmp((const char *) n->name, "link") == 0) {
		if (!rewrite_url(n, "href", base, list))
			return false;
	}
	/* 3 */
	else if (strcmp((const char *) n->name, "frame") == 0 ||
			strcmp((const char *) n->name, "iframe") == 0 ||
			strcmp((const char *) n->name, "input") == 0 ||
			strcmp((const char *) n->name, "img") == 0 ||
			strcmp((const char *) n->name, "script") == 0) {
		if (!rewrite_url(n, "src", base, list))
			return false;
	}
	/* 4 */
	else if (strcmp((const char *) n->name, "style") == 0) {
		unsigned int len;
		xmlChar *content;

		for (child = n->children; child != 0; child = child->next) {
			char *rewritten;
			/* Get current content */
			content = xmlNodeGetContent(child);
			if (!content)
				/* unfortunately we don't know if this is
				 * due to memory exhaustion, or because
				 * there is no content for this node */
				continue;

			/* Rewrite @import rules */
			rewritten = rewrite_stylesheet_urls(
					(const char *) content,
					strlen((const char *) content),
					(int *) &len, base, list);
			xmlFree(content);
			if (!rewritten)
				return false;

			/* set new content */
			xmlNodeSetContentLen(child,
					(const xmlChar*)rewritten,
					len);
		}

		return true;
	}
	/* 5 */
	else if (strcmp((const char *) n->name, "base") == 0) {
		/* simply remove any <base> tags from the document */
		xmlUnlinkNode(n);
		xmlFreeNode(n);
		/* base tags have no content, so there's no point recursing
		 * additionally, we've just destroyed this node, so trying
		 * to recurse would result in bad things happening */
		return true;
	}
	/* 6 */
	else {
	        if (!rewrite_url(n, "background", base, list))
	                return false;
	}

	/* now recurse */
	for (child = n->children; child;) {
		/* we must extract the next child now, as if the current
		 * child is a <base> element, it will be removed from the
		 * tree (see 5, above), thus preventing extraction of the
		 * next child */
		xmlNode *next = child->next;
		if (child->type == XML_ELEMENT_NODE) {
			if (!rewrite_urls(child, base, list))
				return false;
		}
		child = next;
	}

	return true;
}


/**
 * Rewrite an URL in a HTML document.
 *
 * \param  n     The node to modify
 * \param  attr  The html attribute to modify
 * \param  base  base url of document
 * \return  true on success, false on out of memory
 */

bool rewrite_url(xmlNode *n, const char *attr, const char *base,
		struct save_complete_entry *list)
{
	char *url, *data;
	char rel[20];
	hlcache_handle *content;
	url_func_result res;

	if (!xmlHasProp(n, (const xmlChar *) attr))
		return true;

	data = (char *) xmlGetProp(n, (const xmlChar *) attr);
	if (!data)
		return false;

	res = url_join(data, base, &url);
	xmlFree(data);
	if (res == URL_FUNC_NOMEM)
		return false;
	else if (res == URL_FUNC_OK) {
		content = save_complete_list_find(url, list);
		if (content) {
			/* found a match */
			free(url);
			snprintf(rel, sizeof rel, "%p", content);
			if (!xmlSetProp(n, (const xmlChar *) attr,
							(xmlChar *) rel))
				return false;
		} else {
			/* no match found */
			if (!xmlSetProp(n, (const xmlChar *) attr,
							(xmlChar *) url)) {
				free(url);
				return false;
			}
			free(url);
		}
	}

	return true;
}


/**
 * Add a content to the save_complete_list.
 *
 * \param  content  content to add
 * \return  true on success, false on out of memory
 */

bool save_complete_list_add(hlcache_handle *content,
		struct save_complete_entry **list)
{
	struct save_complete_entry *entry;
	entry = malloc(sizeof (*entry));
	if (!entry)
		return false;
	entry->content = content;
	entry->next = *list;
	*list = entry;
	return true;
}


/**
 * Look up a url in the save_complete_list.
 *
 * \param  url  url to find
 * \return  content if found, 0 otherwise
 */

hlcache_handle * save_complete_list_find(const char *url,
		struct save_complete_entry *list)
{
	struct save_complete_entry *entry;
	for (entry = list; entry; entry = entry->next)
		if (strcmp(url, content_get_url(entry->content)) == 0)
			return entry->content;
	return 0;
}


/**
 * Look up a content in the save_complete_list.
 *
 * \param  content  pointer to content
 * \return  true if the content is in the save_complete_list
 */

bool save_complete_list_check(hlcache_handle *content,
		struct save_complete_entry *list)
{
	struct save_complete_entry *entry;
	for (entry = list; entry; entry = entry->next)
		if (entry->content == content)
			return true;
	return false;
}


#if 0
/**
 * Dump save complete list to stderr
 */
void save_complete_list_dump(void)
{
	struct save_complete_entry *entry;
	for (entry = save_complete_list; entry; entry = entry->next)
		fprintf(stderr, "%p : %s\n", entry->content,
						entry->content->url);
}
#endif


/**
 * Create the inventory file listing original URLs.
 */

bool save_complete_inventory(const char *path,
		struct save_complete_entry *list)
{
	char urlpath[256];
	FILE *fp;
	char *pathstring, *standardpath = (path[0] == '/') ?
			(char *)(path + 1) : (char *)path;
	struct save_complete_entry *entry;

	snprintf(urlpath, sizeof urlpath, "file:///%s/Inventory", 
			standardpath);
	pathstring = url_to_path(urlpath);
	if (pathstring == NULL) {
		warn_user("NoMemory", 0);
		return false;
	}
	fp = fopen(pathstring, "w");
	free(pathstring);
	if (!fp) {
		LOG(("fopen(): errno = %i", errno));
		warn_user("SaveError", strerror(errno));
		return false;
	}

	for (entry = list; entry; entry = entry->next) {
		fprintf(fp, "%p %s\n", entry->content, 
				content_get_url(entry->content));
	}

	fclose(fp);

	return true;
}

