/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
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

#include <stdbool.h>
#include <string.h>

#include <libxml/HTMLtree.h>

#include "utils/config.h"
#include "content/content.h"
#include "desktop/save_text.h"
#include "utils/log.h"
#include "utils/utils.h"

#ifdef WITH_TEXT_EXPORT

static void extract_text(xmlDoc *doc);
static void extract_text_from_tree(xmlNode *n);

static FILE *out;

void save_as_text(struct content *c, char *path) {

	htmlParserCtxtPtr toSave;

	if (c->type != CONTENT_HTML) {
		return;
	}

	out = fopen(path, "w");
	if (!out) return;

	toSave = htmlCreateMemoryParserCtxt(c->source_data, c->source_size);
	htmlParseDocument(toSave);

	extract_text(toSave->myDoc);

	fclose(out);

	xmlFreeDoc(toSave->myDoc);
	htmlFreeParserCtxt(toSave);
}

void extract_text(xmlDoc *doc)
{
	xmlNode *html;

	/* find the html element */
	for (html = doc->children;
	     html!=0 && html->type != XML_ELEMENT_NODE;
	     html = html->next)
		;
	if (html == 0 || strcmp((const char*)html->name, "html") != 0) {
		return;
	}

	extract_text_from_tree(html);
}

void extract_text_from_tree(xmlNode *n)
{
	xmlNode *this_node;
	char *text;
	int need_nl = 0;

	if (n->type == XML_ELEMENT_NODE) {
		if (strcmp(n->name, "dl") == 0 ||
		    strcmp(n->name, "h1") == 0 ||
		    strcmp(n->name, "h2") == 0 ||
		    strcmp(n->name, "h3") == 0 ||
		    strcmp(n->name, "ol") == 0 ||
		    strcmp(n->name, "title") == 0 ||
		    strcmp(n->name, "ul") == 0) {
			need_nl = 2;
		}
		else if (strcmp(n->name, "applet") == 0 ||
			 strcmp(n->name, "br") == 0 ||
			 strcmp(n->name, "div") == 0 ||
			 strcmp(n->name, "dt") == 0 ||
			 strcmp(n->name, "h4") == 0 ||
			 strcmp(n->name, "h5") == 0 ||
			 strcmp(n->name, "h6") == 0 ||
			 strcmp(n->name, "li") == 0 ||
			 strcmp(n->name, "object") == 0 ||
			 strcmp(n->name, "p") == 0 ||
			 strcmp(n->name, "tr") == 0) {
			need_nl = 1;
		}
		/* do nothing, we just recurse through these nodes */
	}
	else if (n->type == XML_TEXT_NODE) {
		if ((text = squash_whitespace(n->content)) != NULL) {
			fputs(text, out);
			free(text);
		}
		return;
	}
	else {
		return;
	}

	/* now recurse */
	for (this_node = n->children; this_node != 0; this_node = this_node->next) {
		extract_text_from_tree(this_node);
	}

	while (need_nl--)
		fputc('\n', out);
}

#endif
