/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>
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
 * Creation of URL nodes with use of trees (implementation)
 */


#include <assert.h>
#include <ctype.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>

#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/options.h"
#include "desktop/tree_url_node.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"

/** Flags for each type of url tree node. */
enum tree_element_url {
	TREE_ELEMENT_URL = 0x01,
	TREE_ELEMENT_LAST_VISIT = 0x02,
	TREE_ELEMENT_VISITS = 0x03,
	TREE_ELEMENT_THUMBNAIL = 0x04,
};

#define MAX_ICON_NAME_LEN 256

static bool initialised = false;

static hlcache_handle *folder_icon;

struct icon_entry {
	content_type type;
	hlcache_handle *icon;
};

struct icon_entry icon_table[] = {
	{CONTENT_HTML, NULL},
	{CONTENT_TEXTPLAIN, NULL},
	{CONTENT_CSS, NULL},
	{CONTENT_IMAGE, NULL},
	{CONTENT_NONE, NULL},

	/* this serves as a sentinel */
	{CONTENT_HTML, NULL}
};

static uint32_t tun_users = 0;

void tree_url_node_init(const char *folder_icon_name)
{
	struct icon_entry *entry;
	char icon_name[MAX_ICON_NAME_LEN];
	
	tun_users++;
	
	if (initialised)
		return;
	initialised = true;

	folder_icon = tree_load_icon(folder_icon_name);

	entry = icon_table;
	do {

		tree_icon_name_from_content_type(icon_name, entry->type);
		entry->icon = tree_load_icon(icon_name);

		++entry;
	} while (entry->type != CONTENT_HTML);
}


void tree_url_node_cleanup()
{
	struct icon_entry *entry;
	
	tun_users--;
	
	if (tun_users > 0)
		return;
	
	if (!initialised)
		return;
	initialised = false;
	
	hlcache_handle_release(folder_icon);
	
	entry = icon_table;
	do {
		hlcache_handle_release(entry->icon);
		++entry;
	} while (entry->type != CONTENT_HTML);
}

/**
 * Creates a tree entry for a URL, and links it into the tree
 *
 * \param parent     the node to link to
 * \param url        the URL (copied)
 * \param data	     the URL data to use
 * \param title	     the custom title to use
 * \return the node created, or NULL for failure
 */
struct node *tree_create_URL_node(struct tree *tree, struct node *parent,
		const char *url, const char *title,
		tree_node_user_callback user_callback, void *callback_data)
{
	struct node *node;
	struct node_element *element;
	char *text_cp, *squashed;

	squashed = squash_whitespace(title ? title : url);
	text_cp = strdup(squashed);
	if (text_cp == NULL) {
		LOG(("malloc failed"));
		warn_user("NoMemory", 0);
		return NULL;
	}
	free(squashed);
	node = tree_create_leaf_node(tree, parent, text_cp, true, false,
				     false);
	if (node == NULL) {
		free(text_cp);
		return NULL;
	}

	if (user_callback != NULL)
		tree_set_node_user_callback(node, user_callback,
					    callback_data);

	tree_create_node_element(node, NODE_ELEMENT_BITMAP,
				 TREE_ELEMENT_THUMBNAIL, false);
	tree_create_node_element(node, NODE_ELEMENT_TEXT, TREE_ELEMENT_VISITS,
				 false);
	tree_create_node_element(node, NODE_ELEMENT_TEXT,
				 TREE_ELEMENT_LAST_VISIT, false);
	element = tree_create_node_element(node, NODE_ELEMENT_TEXT,
					   TREE_ELEMENT_URL, true);
	if (element != NULL) {
		text_cp = strdup(url);
		if (text_cp == NULL) {
			tree_delete_node(tree, node, false);
			LOG(("malloc failed"));
			warn_user("NoMemory", 0);
			return NULL;
		}
		tree_update_node_element(tree, element, text_cp, NULL);
	}

	return node;
}


/**
 * Creates a read only tree entry for a URL, and links it into the tree.
 *
 * \param parent      the node to link to
 * \param url         the URL
 * \param data	      the URL data to use
 * \return the node created, or NULL for failure
 */
struct node *tree_create_URL_node_readonly(struct tree *tree, 
		struct node *parent, const char *url, 
		const struct url_data *data, 
		tree_node_user_callback user_callback, void *callback_data)
{
	struct node *node;
	struct node_element *element;
	char *title;

	assert(url && data);

	if (data->title != NULL) {
		title = strdup(data->title);
	} else {
		title = strdup(url);
	}

	if (title == NULL)
		return NULL;

	node = tree_create_leaf_node(tree, parent, title, false, false, false);
	if (node == NULL) {
		free(title);
		return NULL;
	}

	if (user_callback != NULL) {
		tree_set_node_user_callback(node, user_callback,
					    callback_data);
	}

	tree_create_node_element(node, NODE_ELEMENT_BITMAP,
				 TREE_ELEMENT_THUMBNAIL, false);
	tree_create_node_element(node, NODE_ELEMENT_TEXT, TREE_ELEMENT_VISITS,
				 false);
	tree_create_node_element(node, NODE_ELEMENT_TEXT,
				 TREE_ELEMENT_LAST_VISIT, false);
	element = tree_create_node_element(node, NODE_ELEMENT_TEXT,
					   TREE_ELEMENT_URL, false);
	if (element != NULL) {
		tree_update_node_element(tree, element, url, NULL);
	}

	tree_update_URL_node(tree, node, url, data);

	return node;
}


/**
 * Updates the node details for a URL node.
 *
 * \param node  the node to update
 */
void tree_update_URL_node(struct tree *tree, struct node *node,
		const char *url, const struct url_data *data)
{
	struct node_element *element;
	struct bitmap *bitmap = NULL;
	struct icon_entry *entry;
	char *text_cp;

	assert(node != NULL);

	element = tree_node_find_element(node, TREE_ELEMENT_URL, NULL);
	if (element == NULL)
		return;

	if (data != NULL) {
		if (data->title == NULL)
			urldb_set_url_title(url, url);

		if (data->title == NULL)
			return;

		element = tree_node_find_element(node, TREE_ELEMENT_TITLE,
						 NULL);
			
		text_cp = strdup(data->title);
		if (text_cp == NULL) {
			LOG(("malloc failed"));
			warn_user("NoMemory", 0);
			return;
		}
		tree_update_node_element(tree, element,	text_cp, NULL);
	} else {
		data = urldb_get_url_data(url);
		if (data == NULL)
			return;
	}

	entry = icon_table;
	do {
		if (entry->type == data->type) {
			if (entry->icon != NULL)
				tree_set_node_icon(tree, node, entry->icon);
			break;
		}
		++entry;
	} while (entry->type != CONTENT_HTML);

	/* update last visit text */
	element = tree_node_find_element(node, TREE_ELEMENT_LAST_VISIT, element);
	tree_update_element_text(tree,
		element, 
		messages_get_buff("TreeLast",
			(data->last_visit > 0) ?
			ctime((time_t *)&data->last_visit) :
			messages_get("TreeUnknown")));


	/* update number of visits text */
	element = tree_node_find_element(node, TREE_ELEMENT_VISITS, element);
	tree_update_element_text(tree,
		element, 
		messages_get_buff("TreeVisits", data->visits));


	/* update thumbnail */
	element = tree_node_find_element(node, TREE_ELEMENT_THUMBNAIL, element);
	if (element != NULL) {
		bitmap = urldb_get_thumbnail(url);

		if (bitmap != NULL) {
			tree_update_node_element(tree, element, NULL, bitmap);
		}
	}
}


const char *tree_url_node_get_title(struct node *node)
{
	struct node_element *element;
	element = tree_node_find_element(node, TREE_ELEMENT_TITLE, NULL);
	if (element == NULL)
		return NULL;
	return tree_node_element_get_text(element);
}


const char *tree_url_node_get_url(struct node *node)
{
	struct node_element *element;
	element = tree_node_find_element(node, TREE_ELEMENT_URL, NULL);
	if (element == NULL)
		return NULL;
	return tree_node_element_get_text(element);
}

void tree_url_node_edit_title(struct tree *tree, struct node *node)
{
	struct node_element *element;
	element = tree_node_find_element(node, TREE_ELEMENT_TITLE, NULL);
	tree_start_edit(tree, element);
}

void tree_url_node_edit_url(struct tree *tree, struct node *node)
{
	struct node_element *element;
	element = tree_node_find_element(node, TREE_ELEMENT_URL, NULL);
	tree_start_edit(tree, element);
}

node_callback_resp tree_url_node_callback(void *user_data,
					  struct node_msg_data *msg_data)
{
	struct tree *tree;
	struct node_element *element;
	nsurl *nsurl;
	nserror error;
	const char *text;
	char *norm_text;
	const struct url_data *data;

	/** @todo memory leaks on non-shared folder deletion. */
	switch (msg_data->msg) {
	case NODE_DELETE_ELEMENT_TXT:
		switch (msg_data->flag) {
			/* only history is using non-editable url
			 * elements so only history deletion will run
			 * this code
			 */
		case TREE_ELEMENT_URL:
			/* reset URL characteristics */
			urldb_reset_url_visit_data(
				msg_data->data.text);
			return NODE_CALLBACK_HANDLED;
		case TREE_ELEMENT_TITLE:
			return NODE_CALLBACK_HANDLED;
		}
		break;
	case NODE_DELETE_ELEMENT_IMG:
		if (msg_data->flag == TREE_ELEMENT_THUMBNAIL ||
		    msg_data->flag == TREE_ELEMENT_TITLE)
			return NODE_CALLBACK_HANDLED;
		break;
	case NODE_LAUNCH:
		element = tree_node_find_element(msg_data->node,
						 TREE_ELEMENT_URL, NULL);
		if (element != NULL) {
			text = tree_node_element_get_text(element);
			if (msg_data->flag == TREE_ELEMENT_LAUNCH_IN_TABS) {
				msg_data->data.bw = browser_window_create(text,
							msg_data->data.bw, 0, true, true);
			} else {
				browser_window_create(text, NULL, 0,
						      true, false);
			}
			return NODE_CALLBACK_HANDLED;
		}
		break;
	case NODE_ELEMENT_EDIT_FINISHING:

		text = msg_data->data.text;

		if (msg_data->flag == TREE_ELEMENT_URL) {
			size_t len;
			error = nsurl_create(text, &nsurl);
			if (error != NSERROR_OK) {
				warn_user("NoMemory", 0);
				return NODE_CALLBACK_REJECT;
			}
			error = nsurl_get(nsurl, NSURL_WITH_FRAGMENT,
					&norm_text, &len);
			nsurl_unref(nsurl);
			if (error != NSERROR_OK) {
				warn_user("NoMemory", 0);
				return NODE_CALLBACK_REJECT;
			}

			msg_data->data.text = norm_text;

			data = urldb_get_url_data(norm_text);
			if (data == NULL) {
				urldb_add_url(norm_text);
				urldb_set_url_persistence(norm_text,
							  true);
				data = urldb_get_url_data(norm_text);
				if (data == NULL)
					return NODE_CALLBACK_REJECT;
			}
			tree = user_data;
			tree_update_URL_node(tree, msg_data->node,
					     norm_text, NULL);
		}
		else if (msg_data->flag == TREE_ELEMENT_TITLE) {
			while (isspace(*text))
				text++;
			norm_text = strdup(text);
			if (norm_text == NULL) {
				LOG(("malloc failed"));
				warn_user("NoMemory", 0);
				return NODE_CALLBACK_REJECT;
			}
			/* don't allow zero length entry text, return
			   false */
			if (norm_text[0] == '\0') {
				warn_user("NoNameError", 0);
				msg_data->data.text = NULL;
				return NODE_CALLBACK_CONTINUE;
			}
			msg_data->data.text = norm_text;
		}

		return NODE_CALLBACK_HANDLED;
	default:
		break;
	}
	return NODE_CALLBACK_NOT_HANDLED;
}

/**
 * Search the children of an xmlNode for an element.
 *
 * \param  node  xmlNode to search children of, or 0
 * \param  name  name of element to find
 * \return  first child of node which is an element and matches name, or
 *          0 if not found or parameter node is 0
 */
static xmlNode *tree_url_find_xml_element(xmlNode *node, const char *name)
{
	xmlNode *xmlnode;
	if (node == NULL)
		return NULL;

	for (xmlnode = node->children;
	     xmlnode && !(xmlnode->type == XML_ELEMENT_NODE &&
		    strcmp((const char *) xmlnode->name, name) == 0);
	     xmlnode = xmlnode->next)
		;

	return xmlnode;
}

/**
 * Parse an entry represented as a li.
 *
 * \param  li         xmlNode for parsed li
 * \param  directory  directory to add this entry to
 */
static void tree_url_load_entry(xmlNode *li, struct tree *tree,
		struct node *directory, tree_node_user_callback callback,
		void *callback_data)
{
	char *url1 = NULL;
	char *title = NULL;
	struct node *entry;
	xmlNode *xmlnode;
	const struct url_data *data;
	nsurl *url;
	nserror error;

	for (xmlnode = li->children; xmlnode; xmlnode = xmlnode->next) {
		/* The li must contain an "a" element */
		if (xmlnode->type == XML_ELEMENT_NODE &&
		    strcmp((const char *)xmlnode->name, "a") == 0) {
			url1 = (char *)xmlGetProp(xmlnode,
					(const xmlChar *) "href");
			title = (char *)xmlNodeGetContent(xmlnode);
		}
	}

	if ((url1 == NULL) || (title == NULL)) {
		warn_user("TreeLoadError", "(Missing <a> in <li> or "
			  "memory exhausted.)");
		return;
	}

	/* We're loading external input.
	 * This may be garbage, so attempt to normalise via nsurl
	 */
	error = nsurl_create(url1, &url);
	if (error != NSERROR_OK) {
		LOG(("Failed normalising '%s'", url1));

		warn_user("NoMemory", NULL);

		xmlFree(url1);
		xmlFree(title);

		return;
	}

	/* No longer need this */
	xmlFree(url1);

	data = urldb_get_url_data(nsurl_access(url));
	if (data == NULL) {
		/* No entry in database, so add one */
		urldb_add_url(nsurl_access(url));
		/* now attempt to get url data */
		data = urldb_get_url_data(nsurl_access(url));
	}
	if (data == NULL) {
		xmlFree(title);
		nsurl_unref(url);

		return;
	}

	/* Make this URL persistent */
	urldb_set_url_persistence(nsurl_access(url), true);

	/* Force the title in the hotlist */
	urldb_set_url_title(nsurl_access(url), title);

	entry = tree_create_URL_node(tree, directory, nsurl_access(url), title,
				     callback, callback_data);

 	if (entry == NULL) {
 		/** \todo why isn't this fatal? */
 		warn_user("NoMemory", 0);
 	} else {
		tree_update_URL_node(tree, entry, nsurl_access(url), data);
	}


	xmlFree(title);
	nsurl_unref(url);
}

/**
 * Parse a directory represented as a ul.
 *
 * \param  ul         xmlNode for parsed ul
 * \param  directory  directory to add this directory to
 */
static void tree_url_load_directory(xmlNode *ul, struct tree *tree,
		struct node *directory, tree_node_user_callback callback,
		void *callback_data)
{
	char *title;
	struct node *dir;
	xmlNode *xmlnode;
	xmlChar *id;

	assert(ul != NULL);
	assert(directory != NULL);

	for (xmlnode = ul->children; xmlnode; xmlnode = xmlnode->next) {
		/* The ul may contain entries as a li, or directories as
		 * an h4 followed by a ul. Non-element nodes may be present
		 * (eg. text, comments), and are ignored. */

		if (xmlnode->type != XML_ELEMENT_NODE)
			continue;

		if (strcmp((const char *)xmlnode->name, "li") == 0) {
			/* entry */
			tree_url_load_entry(xmlnode, tree, directory, callback,
					    callback_data);

		} else if (strcmp((const char *)xmlnode->name, "h4") == 0) {
			/* directory */
			bool dir_is_default = false;
			title = (char *) xmlNodeGetContent(xmlnode );
			if (!title) {
				warn_user("TreeLoadError", "(Empty <h4> "
					  "or memory exhausted.)");
				return;
			}

			for (xmlnode = xmlnode->next;
			     xmlnode && xmlnode->type != XML_ELEMENT_NODE;
			     xmlnode = xmlnode->next)	
				;
			if ((xmlnode == NULL) || 
			    strcmp((const char *)xmlnode->name, "ul") != 0) {
				/* next element isn't expected ul */
				free(title);
				warn_user("TreeLoadError", "(Expected "
					  "<ul> not present.)");
				return;
			} else {
				id = xmlGetProp(xmlnode,
						(const xmlChar *) "id");
				if (id != NULL) {
					if(strcmp((const char *)id, "default") == 0)
						dir_is_default = true;
					xmlFree(id);
				}
			}

			dir = tree_create_folder_node(tree, directory, title,
						      true, false, false);
			if (dir == NULL) {
				free(title);
				return;
			}

			if(dir_is_default == true) {
				tree_set_default_folder_node(tree, dir);
			}

			if (callback != NULL)
				tree_set_node_user_callback(dir, callback,
							    callback_data);

 			if (folder_icon != NULL)
 				tree_set_node_icon(tree, dir, folder_icon);

			tree_url_load_directory(xmlnode, tree, dir, callback,
						callback_data);
		}
	}
}

/**
 * Loads an url tree from a specified file.
 *
 * \param  filename  	name of file to read
 * \param  tree		empty tree which data will be read into
 * \return the file represented as a tree, or NULL on failure
 */
bool tree_urlfile_load(const char *filename, struct tree *tree,
		       tree_node_user_callback callback, void *callback_data)
{
	xmlDoc *doc;
	xmlNode *html, *body, *ul;
	struct node *root;
	FILE *fp = NULL;

	if (filename == NULL) {
		return false;
	}

	fp = fopen(filename, "r");
	if (fp == NULL) {
		return false;
	}
	fclose(fp);

	doc = htmlParseFile(filename, "iso-8859-1");
	if (doc == NULL) {
		warn_user("TreeLoadError", messages_get("ParsingFail"));
		return false;
	}

	html = tree_url_find_xml_element((xmlNode *) doc, "html");
	body = tree_url_find_xml_element(html, "body");
	ul = tree_url_find_xml_element(body, "ul");
	if (ul == NULL) {
		xmlFreeDoc(doc);
		warn_user("TreeLoadError",
			  "(<html>...<body>...<ul> not found.)");
		return false;
	}

	root = tree_get_root(tree);
	tree_url_load_directory(ul, tree, root, callback, callback_data);
	tree_set_node_expanded(tree, root, true, false, false);

	xmlFreeDoc(doc);
	return true;
}

/**
 * Add an entry to the HTML tree for saving.
 *
 * The node must contain a sequence of node_elements in the following order:
 *
 * \param  entry  hotlist entry to add
 * \param  node   node to add li to
 * \return  true on success, false on memory exhaustion
 */
static bool tree_url_save_entry(struct node *entry, xmlNode *node)
{
	xmlNode *li, *a;
	xmlAttr *href;
	const char *text;

	li = xmlNewChild(node, NULL, (const xmlChar *) "li", NULL);
	if (li == NULL)
		return false;


	text = tree_url_node_get_title(entry);
	if (text == NULL)
		return false;
	a = xmlNewTextChild(li, NULL, (const xmlChar *) "a",
			    (const xmlChar *) text);
	if (a == NULL)
		return false;

	text = tree_url_node_get_url(entry);
	if (text == NULL)
		return false;

	href = xmlNewProp(a, (const xmlChar *) "href", (const xmlChar *) text);
	if (href == NULL)
		return false;
	return true;
}

/**
 * Add a directory to the HTML tree for saving.
 *
 * \param  directory  hotlist directory to add
 * \param  node       node to add ul to
 * \return  true on success, false on memory exhaustion
 */
static bool tree_url_save_directory(struct node *directory, xmlNode *node)
{
	struct node *child;
	xmlNode *ul, *h4;
	const char *text;

	ul = xmlNewChild(node, NULL, (const xmlChar *)"ul", NULL);
	if (ul == NULL)
		return false;
	if (tree_node_is_default(directory) == true)
		xmlSetProp(ul, (const xmlChar *) "id",
				(const xmlChar *) "default");

	for (child = tree_node_get_child(directory); child;
	     child = tree_node_get_next(child)) {
		if (!tree_node_is_folder(child)) {
			/* entry */
			if (!tree_url_save_entry(child, ul))
				return false;
		} else {
			/* directory */
			/* invalid HTML */

			text = tree_url_node_get_title(child);
			if (text == NULL)
				return false;

			h4 = xmlNewTextChild(ul, NULL,
					     (const xmlChar *) "h4",
					     (const xmlChar *) text);
			if (h4 == NULL)
				return false;

			if (!tree_url_save_directory(child, ul))
				return false;
		}	}

	return true;
}


/**
 * Perform a save to a specified file in the form of a html page
 *
 * \param filename	the file to save to
 * \param page_title 	title of the page
 */
bool tree_urlfile_save(struct tree *tree, const char *filename,
		       const char *page_title)
{
	int res;
	xmlDoc *doc;
	xmlNode *html, *head, *title, *body;

	/* Unfortunately the Browse Hotlist format is invalid HTML,
	 * so this is a lie. 
	 */
	doc = htmlNewDoc(
		(const xmlChar *) "http://www.w3.org/TR/html4/strict.dtd",
		(const xmlChar *) "-//W3C//DTD HTML 4.01//EN");
	if (doc == NULL) {
		warn_user("NoMemory", 0);
		return false;
	}

	html = xmlNewNode(NULL, (const xmlChar *) "html");
	if (html == NULL) {
		warn_user("NoMemory", 0);
		xmlFreeDoc(doc);
		return false;
	}
	xmlDocSetRootElement(doc, html);

	head = xmlNewChild(html, NULL, (const xmlChar *) "head", NULL);
	if (head == NULL) {
		warn_user("NoMemory", 0);
		xmlFreeDoc(doc);
		return false;
	}

	title  = xmlNewTextChild(head, NULL, (const xmlChar *) "title",
				 (const xmlChar *) page_title);
	if (title == NULL) {
		warn_user("NoMemory", 0);
		xmlFreeDoc(doc);
		return false;
	}

	body = xmlNewChild(html, NULL, (const xmlChar *) "body", NULL);
	if (body == NULL) {
		warn_user("NoMemory", 0);
		xmlFreeDoc(doc);
		return false;
	}

	if (!tree_url_save_directory(tree_get_root(tree), body)) {
 		warn_user("NoMemory", 0);
 		xmlFreeDoc(doc);
 		return false;
 	}

	doc->charset = XML_CHAR_ENCODING_UTF8;
	res = htmlSaveFileEnc(filename, doc, "iso-8859-1");
	if (res == -1) {
		warn_user("HotlistSaveError", 0);
		xmlFreeDoc(doc);
		return false;
	}

	xmlFreeDoc(doc);
	return true;
}
