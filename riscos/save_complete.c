/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <string.h>
#include <unixlib/local.h> /* for __riscosify */

#include "libxml/HTMLtree.h"

#include "oslib/osfile.h"
#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/render/form.h"
#include "netsurf/render/layout.h"
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

struct url_entry {
        char *url;   /**< Fully qualified URL, as per url_join output */
        char *par;   /**< Base URL of parent object */
        int  ptr;    /**< Pointer to object's location in memory */
        struct url_entry *next; /**< Next entry in list */
};

static void save_imported_sheets(struct content *c, char *p, char* fn,
                          struct url_entry *imports);
/*static char *leafname(const char *url);
static int rewrite_stylesheet_urls(const char* sheet, int isize, char* buffer,
                            int osize, struct url_entry *head);*/
static int rewrite_document_urls(xmlDoc *doc, struct url_entry *head, char *fname);
static int rewrite_urls(xmlNode *n, struct url_entry *head, char *fname);
static void rewrite_url_data(xmlNode *n, struct url_entry *head, char *fname);
static void rewrite_url_href(xmlNode *n, struct url_entry *head, char *fname);
static void rewrite_url_src(xmlNode *n, struct url_entry *head, char *fname);

/* this is temporary. */
const char * const SAVE_PATH = "<NetSurf$Dir>.savetest.";
const char * const OBJ_DIR = "_files";

/** \todo this will probably want to take a filename */
void save_complete(struct content *c) {

	char *fname = 0, *spath;
	unsigned int i;
	struct url_entry urls = {0, 0, 0, 0}; /* sentinel at head */
	struct url_entry *object;
	xmlDoc *toSave;

	if (c->type != CONTENT_HTML)
		return;

	fname = "test";  /*get_filename(c->data.html.base_url);*/

	spath = xcalloc(strlen(SAVE_PATH)+strlen(OBJ_DIR)+strlen(fname)+50,
			sizeof(char));

	sprintf(spath, "%s%s%s", SAVE_PATH, fname, OBJ_DIR);
	xosfile_create_dir(spath, 77);

        /* save stylesheets, ignoring the base sheet and <style> elements */
        for (i = 2; i != c->data.html.stylesheet_count; i++) {
		struct content *css = c->data.html.stylesheet_content[i];
		struct url_entry imports = {0, 0, 0, 0};
		//char *source;
		//int source_len;

                if (!css)
                        continue;

                save_imported_sheets(css, spath, fname, &imports);

                /*source = xcalloc(css->source_size+100, sizeof(char));
                source_len = rewrite_stylesheet_urls(css->source_data,
                                                     css->source_size,
                                                     source,
                                                     css->source_size+100,
                                                     &imports);*/

                sprintf(spath, "%s%s%s.%p", SAVE_PATH, fname, OBJ_DIR, css);
                /*if (source_len > 0) {
                xosfile_save_stamped(spath, 0xf79, source,
                                     source + source_len);
		}
		xfree(source);*/
		xosfile_save_stamped(spath, 0xf79, css->source_data,
		                     css->source_data + css->source_size);

		/* Now add the url of this sheet to the list
		 * of objects imported by the parent page
		 */
		object = xcalloc(1, sizeof(struct url_entry));
		object->url = css->url;
		object->par = url_normalize(c->data.html.base_url);
		object->ptr = (int)css;
		object->next = urls.next;
		urls.next = object;
        }

	/* save objects */
	for (i = 0; i != c->data.html.object_count; i++) {
		struct content *obj = c->data.html.object[i].content;

		/* skip difficult content types */
		if (!obj || obj->type >= CONTENT_PLUGIN) {
			continue;
		}

		sprintf(spath, "%s%s%s.%p", SAVE_PATH, fname, OBJ_DIR, c->data.html.object[i].content);

		xosfile_save_stamped(spath,
				ro_content_filetype(obj),
				obj->source_data,
				obj->source_data + obj->source_size);

		/* Add to list, as for stylesheets */
		object = xcalloc(1, sizeof(struct url_entry));
		object->url = obj->url;
		object->par = url_normalize(c->data.html.base_url);
		object->ptr = (int)obj;
		object->next = urls.next;
		urls.next = object;
	}

	/* make a copy of the document tree */
	toSave = xmlCopyDoc(c->data.html.document, 1);

	if (!toSave) {
	        xfree(spath);
	        return;
	}

	/* rewrite all urls we know about */
       	if (rewrite_document_urls(toSave, &urls, fname) == 0) {
       	        xfree(spath);
       	        xmlFreeDoc(toSave);
       	        return;
       	}

	/* save the html file out last of all */
	sprintf(spath, "%s%s", SAVE_PATH, fname);
	htmlSaveFile(spath, toSave);
	xosfile_set_type(spath, 0xfaf);

        xmlFreeDoc(toSave);
	xfree(spath);
	//xfree(fname);
}

void save_imported_sheets(struct content *c, char *p, char *fn,
                          struct url_entry *imports)
{
        unsigned int j;
        //struct url_entry *this;
        //char *source;
        //int source_len;

        for (j = 0; j != c->data.css.import_count; j++) {
		struct content *css = c->data.css.import_content[j];
	        struct url_entry imp = {0, 0, 0, 0};

                if (!css)
                        continue;

                save_imported_sheets(css, p, fn, &imp);

                /*source = xcalloc(css->source_size+100, sizeof(char));
                source_len = rewrite_stylesheet_urls(css->source_data,
                                                     css->source_size,
                                                     source,
                                                     css->source_size+100,
                                                     &imp);*/

                sprintf(p, "%s%s%s.%p", SAVE_PATH, fn, OBJ_DIR, css);
                /*if (source_len > 0) {
                xosfile_save_stamped(p, 0xf79, source, source + source_len);
                }
                xfree(source);*/
                xosfile_save_stamped(p, 0xf79, css->source_data,
                                     css->source_data + css->source_size);

		/* now add the url of this sheet to the list of
		 * sheets imported by the parent sheet.
		 */
		/*this = xcalloc(1, sizeof(struct url_entry));
		this->url = css->url;
		this->par = url_normalize(c->url);
		this->ptr = (int)css;
		this->next = imports->next;
		imports->next = this;*/
        }
}

#if 0

char *leafname(const char *url)
{
        char *res, *temp;

        /* the input URL is that produced by url_join,
         * therefore, we can assume that this will work.
         */
        temp = strrchr(url, '/');

        if ((temp - url) == (int)(strlen(url)-1)) { /* root dir */
                res = xstrdup("index/html");
                return res;
        }

        temp += 1;
        res = xcalloc(strlen(temp), sizeof(char));
        if (__riscosify_std(temp, 0, res, strlen(temp), 0)) {
                return res;
        }

        return NULL;
}

/**
 * Rewrite stylesheet @import rules to use relative urls.
 *
 * @param sheet The source of the stylesheet
 * @param isize The size of the input buffer
 * @param buffer The buffer into which to write the modified sheet
 * @param osize The size of the output buffer
 * @param head Pointer to the head of the list containing imported urls
 * @return The length of the output buffer, or 0 on error.
 */
int rewrite_stylesheet_urls(const char *sheet, int isize, char *buffer,
                            int osize, struct url_entry *head)
{
        struct url_entry *item, *next;
        char *rule, *input = sheet, *temp, *end;
        int out_size = 0, out = 0;

        assert(head);

        while (input < sheet+isize) {
                /* find next occurrence of @import in input buffer */
                rule = strstr(input, "@import");

                if (!rule) {
                        if (out_size + ((sheet+isize)-input) > osize) {
                                /* not enough space in buffer -> exit */
                                return 0;
                        }
                        memcpy(buffer, input, ((sheet+isize)-input));
                        out_size += ((sheet+isize)-input);
                        break;
                }

                /* find end of this rule */
                end = strchr(rule, ';');
                if (!end) { /* rule not closed - give up */
                        if (out_size + ((sheet+isize)-input) > osize) {
                                /* not enough space in buffer -> exit */
                                return 0;
                        }
                        memcpy(buffer, input, ((sheet+isize)-input));
                        out_size += ((sheet+isize)-input);
                        break;
                }

                /* skip until after first set of double quotes */
                temp = strchr(rule, '"');
                if (!temp) { /* no quotes - try parentheses */
                        temp = strchr(rule, '(');
                        if (!temp) { /* no parentheses - give up */
                                if (out_size + (rule-input+1) > osize) {
                                /* not enough space in buffer -> exit */
                                        return 0;
                                }
                                memcpy(buffer, input, (rule-input+1));
                                buffer += rule-input+1;
                                out_size += rule-input+1;
                                input = rule + 1;
                                continue;
                        }
                }
                /* check we haven't gone past the end */
                if (temp > end && *temp == '(') { /* tested both */
                        if (out_size + (end-input+1) > osize) {
                               /* not enough space in buffer -> exit */
                                return 0;
                        }
                        memcpy(buffer, input, (end-input+1));
                        buffer += end-input+1;
                        out_size += end-input+1;
                        input = end + 1;
                        continue;
                }
                else if (temp > end) { /* not done parentheses yet */
                        temp = strchr(rule, '(');
                        if (!temp) { /* no parentheses - give up */
                                if (out_size + (rule-input+1) > osize) {
                                /* not enough space in buffer -> exit */
                                        return 0;
                                }
                                memcpy(buffer, input, (rule-input+1));
                                buffer += rule-input+1;
                                out_size += rule-input+1;
                                input = rule + 1;
                                continue;
                        }
                }
                if (temp > end) {
                        if (out_size + (end-input+1) > osize) {
                               /* not enough space in buffer -> exit */
                                return 0;
                        }
                        memcpy(buffer, input, (end-input+1));
                        buffer += end-input+1;
                        out_size += end-input+1;
                        input = end + 1;
                        continue;
                }
                rule = temp + 1;

                /* pointer to end of url */
                temp = strchr(rule, '"');
                if (!temp) {
                        temp = strchr(rule, ')');
                        if (!temp) {
                                if (out_size + (rule-input+1) > osize) {
                                /* not enough space in buffer -> exit */
                                        return 0;
                                }
                                memcpy(buffer, input, (rule-input+1));
                                buffer += rule-input+1;
                                out_size += rule-input+1;
                                input = rule + 1;
                                continue;
                        }
                }
                /* check we haven't gone past the end */
                if (temp > end && *temp == ')') { /* tested both */
                        if (out_size + (end-input+1) > osize) {
                               /* not enough space in buffer -> exit */
                                return 0;
                        }
                        memcpy(buffer, input, (end-input+1));
                        buffer += end-input+1;
                        out_size += end-input+1;
                        input = end + 1;
                        continue;
                }
                else if (temp > end) { /* not done parentheses yet */
                        temp = strchr(rule, ')');
                        if (!temp) { /* no parentheses - give up */
                                if (out_size + (rule-input+1) > osize) {
                                /* not enough space in buffer -> exit */
                                        return 0;
                                }
                                memcpy(buffer, input, (rule-input+1));
                                buffer += rule-input+1;
                                out_size += rule-input+1;
                                input = rule + 1;
                                continue;
                        }
                }
                if (temp > end) {
                        if (out_size + (end-input+1) > osize) {
                               /* not enough space in buffer -> exit */
                                return 0;
                        }
                        memcpy(buffer, input, (end-input+1));
                        buffer += end-input+1;
                        out_size += end-input+1;
                        input = end + 1;
                        continue;
                }
                end = temp;

                /* copy input up to @import rule to output buffer */
                if (out_size + (rule-input) > osize) {
                        /* not enough space in buffer -> exit */
                        return 0;
                }
                memcpy(buffer, input, (out = (rule-input)));
                input = rule;
                buffer += out;
                out_size += out;

                /* copy url into temporary buffer */
                temp = xcalloc((end-rule), sizeof(char));
                strncpy(temp, rule, (end-rule));

                /* iterate over list, looking for url */
                /** \todo make url detection more accurate */
                for (item=head; item->next; item=item->next) {

                        if (strstr(item->next->url, temp) != 0) {
                                /* url found -> rewrite it */
                                int len = 12;
                                char *url = xcalloc(len, sizeof(char));
                                sprintf(url, "./0x%x", item->next->ptr);
                                if (out_size + len > osize) {
                                        return 0;
                                }
                                memcpy(buffer, url, len);
                                xfree(url);
                                out = len;
                                break;
                        }
                }

                if (item->next == 0) {
                        /* url not found -> write temp to buffer */
                        if ((int)(out_size + strlen(temp)) > osize) {
                                return 0;
                        }
                        memcpy(buffer, temp, strlen(temp));
                        out = strlen(temp);
                }

                /* free url */
                xfree(temp);

                input = end;
                buffer += out;
                out_size += out;
        }

        /* free list */
        for (item = head; item->next; item = item->next) {

                next = item->next;
                item->next = item->next->next;
                xfree(next->par);
                xfree(next);

                if (item->next == 0) {
                        break;
                }
        }

        return out_size;
}

#endif

/**
 * Rewrite URLs in a HTML document to be relative
 *
 * @param doc The root of the document tree
 * @param head The head of the list of known URLs
 * @param fname The name of the file to save as
 * @return 0 on error. >0 otherwise
 */
int rewrite_document_urls(xmlDoc *doc, struct url_entry *head, char *fname)
{
        xmlNode *html;
        struct url_entry *item, *next;

        /* find the html element */
        for (html = doc->children;
             html!=0 && html->type != XML_ELEMENT_NODE;
             html = html->next)
                ;
        if (html == 0 || strcmp((const char*)html->name, "html") != 0) {
                return 0;
        }

	rewrite_urls(html, head, fname);

	/* free list */
        for (item = head; item->next; item = item->next) {

                next = item->next;
                item->next = item->next->next;
                xfree(next->par);
                xfree(next);

                if (item->next == 0) {
                        break;
                }
        }

        return 1;
}

/**
 * Traverse tree, rewriting URLs as we go.
 *
 * @param n The root of the tree
 * @param head The head of the list of known URLs
 * @param fname The name of the file to save as
 * @return 0 on error. >0 otherwise
 */
int rewrite_urls(xmlNode *n, struct url_entry *head, char *fname)
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
                LOG(("%s", n->name));
                /* 1 */
                if (strcmp(n->name, "object") == 0) {
                      rewrite_url_data(n, head, fname);
                }
                /* 2 */
                else if (strcmp(n->name, "a") == 0 ||
                         strcmp(n->name, "area") == 0 ||
                         strcmp(n->name, "link") == 0 ||
                         strcmp(n->name, "base") == 0) {
                      rewrite_url_href(n, head, fname);
                }
                /* 3 */
                else if (strcmp(n->name, "frame") == 0 ||
                         strcmp(n->name, "iframe") == 0 ||
                         strcmp(n->name, "input") == 0 ||
                         strcmp(n->name, "img") == 0 ||
                         strcmp(n->name, "script") == 0) {
                      rewrite_url_src(n, head, fname);
                }
        }
        else {
                return 0;
        }

        /* now recurse */
	for (this = n->children; this != 0; this = this->next) {
                rewrite_urls(this, head, fname);
	}

        return 1;
}

void rewrite_url_data(xmlNode *n, struct url_entry *head, char *fname)
{
        char *url, *data, *rel;
        struct url_entry *item;
        int len = strlen(fname) + strlen(OBJ_DIR) + 13;

        data = xmlGetProp(n, (const xmlChar*)"data");

        if (!data) return;

        url = url_join(data, head->next->par);
        if (!url) {
                xmlFree(data);
                return;
        }

        for (item=head; item->next; item=item->next) {

                if (strcmp(url, item->next->url) == 0) { /* found a match */
                        rel = xcalloc(len, sizeof(char));
                        sprintf(rel, "./%s%s/0x%x", fname, OBJ_DIR,
                                                           item->next->ptr);
                        xmlSetProp(n, (const xmlChar*)"data",
                                      (const xmlChar*) rel);
                        xfree(rel);
                        break;
                }
        }

        xfree(url);
        xmlFree(data);
}

void rewrite_url_href(xmlNode *n, struct url_entry *head, char *fname)
{
        char *url, *href, *rel;
        struct url_entry *item;
        int len = strlen(fname) + strlen(OBJ_DIR) + 13;

        href = xmlGetProp(n, (const xmlChar*)"href");

        if (!href) return;

        url = url_join(href, head->next->par);
        if (!url) {
                xmlFree(href);
                return;
        }

        for (item=head; item->next; item=item->next) {

                if (strcmp(url, item->next->url) == 0) { /* found a match */
                        rel = xcalloc(len, sizeof(char));
                        sprintf(rel, "./%s%s/0x%x", fname, OBJ_DIR,
                                                           item->next->ptr);
                        xmlSetProp(n, (const xmlChar*)"href",
                                      (const xmlChar*) rel);
                        xfree(rel);
                        break;
                }
        }

        xfree(url);
        xmlFree(href);
}

void rewrite_url_src(xmlNode *n, struct url_entry *head, char *fname)
{
        char *url, *src, *rel;
        struct url_entry *item;
        int len = strlen(fname) + strlen(OBJ_DIR) + 13;

        src = xmlGetProp(n, (const xmlChar*)"src");

        if (!src) return;

        url = url_join(src, head->next->par);
        if (!url) {
                xmlFree(src);
                return;
        }

        for (item=head; item->next; item=item->next) {

                if (strcmp(url, item->next->url) == 0) { /* found a match */
                        rel = xcalloc(len, sizeof(char));
                        sprintf(rel, "./%s%s/0x%x", fname, OBJ_DIR,
                                                           item->next->ptr);
                        xmlSetProp(n, (const xmlChar*)"src",
                                      (const xmlChar*) rel);
                        xfree(rel);
                        break;
                }
        }

        xfree(url);
        xmlFree(src);
}
#endif
