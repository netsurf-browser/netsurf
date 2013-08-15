/*
 * Copyright 2012 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2013 Michael Drake <tlsa@netsurf-browser.org>
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

#include <assert.h>
#include <stdlib.h>

#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>

#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/hotlist.h"
#include "desktop/treeview.h"
#include "utils/corestrings.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/libdom.h"
#include "utils/log.h"

#define N_DAYS 28
#define N_SEC_PER_DAY (60 * 60 * 24)

enum hotlist_fields {
	HL_TITLE,
	HL_URL,
	HL_LAST_VISIT,
	HL_VISITS,
	HL_FOLDER,
	HL_N_FIELDS
};

struct hotlist_folder {
	treeview_node *folder;
	struct treeview_field_data data;
};

struct hotlist_ctx {
	treeview *tree;
	struct treeview_field_desc fields[HL_N_FIELDS];
	bool built;
};
struct hotlist_ctx hl_ctx;

struct hotlist_entry {
	nsurl *url;
	treeview_node *entry;

	struct treeview_field_data data[HL_N_FIELDS - 1];
};


/**
 * Set a hotlist entry's data from the url_data.
 *
 * \param e		hotlist entry to set up
 * \param title		Title for entry, or NULL if using title from data
 * \param url_data	Data associated with entry's URL
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror hotlist_create_treeview_field_data(
		struct hotlist_entry *e, const char *title,
		const struct url_data *data)
{
	char buffer[16];
	const char *last_visited;
	char *last_visited2;
	int len;

	if (title == NULL) {
		title = (data->title != NULL) ?
				strdup(data->title) :
				strdup("<No title>");
	}

	e->data[HL_TITLE].field = hl_ctx.fields[HL_TITLE].field;
	e->data[HL_TITLE].value = title;
	e->data[HL_TITLE].value_len = (e->data[HL_TITLE].value != NULL) ?
			strlen(title) : 0;

	e->data[HL_URL].field = hl_ctx.fields[HL_URL].field;
	e->data[HL_URL].value = nsurl_access(e->url);
	e->data[HL_URL].value_len = nsurl_length(e->url);

	last_visited = ctime(&data->last_visit);
	last_visited2 = strdup(last_visited);
	if (last_visited2 != NULL) {
		assert(last_visited2[24] == '\n');
		last_visited2[24] = '\0';
	}

	e->data[HL_LAST_VISIT].field = hl_ctx.fields[HL_LAST_VISIT].field;
	e->data[HL_LAST_VISIT].value = last_visited2;
	e->data[HL_LAST_VISIT].value_len = (last_visited2 != NULL) ? 24 : 0;

	len = snprintf(buffer, 16, "%u", data->visits);
	if (len == 16) {
		len--;
		buffer[len] = '\0';
	}

	e->data[HL_VISITS].field = hl_ctx.fields[HL_VISITS].field;
	e->data[HL_VISITS].value = strdup(buffer);
	e->data[HL_VISITS].value_len = len;

	return NSERROR_OK;
}

/**
 * Add a hotlist entry to the treeview
 *
 * \param e		Entry to add to treeview
 * \param relation	Existing node to insert as relation of, or NULL
 * \param rel		Folder's relationship to relation
 * \return NSERROR_OK on success, or appropriate error otherwise
 *
 * It is assumed that the entry is unique (for its URL) in the global
 * hotlist table
 */
static nserror hotlist_entry_insert(struct hotlist_entry *e,
		treeview_node *relation, enum treeview_relationship rel)
{
	nserror err;

	err = treeview_create_node_entry(hl_ctx.tree, &(e->entry),
			relation, rel, e->data, e, hl_ctx.built ?
			TREE_CREATE_NONE : TREE_CREATE_SUPPRESS_RESIZE);
	if (err != NSERROR_OK) {
		return err;
	}

	return NSERROR_OK;
}


/**
 * Add an entry to the hotlist (creates the entry).
 *
 * If the treeview has already been created, the entry will be added to the
 * treeview.  Otherwise, the entry will have to be added to the treeview later.
 *
 * When we first create the hotlist we create it without the treeview, to
 * simplfy sorting the entries.
 *
 * If set, 'title' must be allocated on the heap, ownership is yeilded to
 * this function.
 *
 * \param url		URL for entry to add to hotlist.
 * \param title		Title for entry, or NULL if using title from data
 * \param data		URL data for the entry
 * \param relation	Existing node to insert as relation of, or NULL
 * \param rel		Entry's relationship to relation
 * \param entry		Updated to new treeview entry node
 * \return NSERROR_OK on success, or appropriate error otherwise
 */
static nserror hotlist_add_entry_internal(nsurl *url, const char *title,
		const struct url_data *data, treeview_node *relation,
		enum treeview_relationship rel, treeview_node **entry)
{
	nserror err;
	struct hotlist_entry *e;

	/* Create new local hotlist entry */
	e = malloc(sizeof(struct hotlist_entry));
	if (e == NULL) {
		return NSERROR_NOMEM;
	}

	e->url = nsurl_ref(url);
	e->entry = NULL;

	err = hotlist_create_treeview_field_data(e, title, data);
	if (err != NSERROR_OK) {
		return err;
	}

	err = hotlist_entry_insert(e, relation, rel);
	if (err != NSERROR_OK) {
		return err;
	}

	*entry = e->entry;

	return NSERROR_OK;
}


/**
 * Delete a hotlist entry
 *
 * This does not delete the treeview node, rather it should only be called from
 * the treeview node delete event message.
 *
 * \param e		Entry to delete
 */
static void hotlist_delete_entry_internal(struct hotlist_entry *e)
{
	assert(e != NULL);
	assert(e->entry == NULL);

	/* Destroy fields */
	free((void *)e->data[HL_TITLE].value); /* Eww */
	free((void *)e->data[HL_LAST_VISIT].value); /* Eww */
	free((void *)e->data[HL_VISITS].value); /* Eww */
	nsurl_unref(e->url);

	/* Destroy entry */
	free(e);
}


static nserror hotlist_tree_node_folder_cb(
		struct treeview_node_msg msg, void *data)
{
	struct treeview_field_data *f = data;

	switch (msg.msg) {
	case TREE_MSG_NODE_DELETE:
		free((void*)f->value); /* Eww */
		free(f);
		break;

	case TREE_MSG_NODE_EDIT:
		break;

	case TREE_MSG_NODE_LAUNCH:
		break;
	}

	return NSERROR_OK;
}
static nserror hotlist_tree_node_entry_cb(
		struct treeview_node_msg msg, void *data)
{
	struct hotlist_entry *e = data;

	switch (msg.msg) {
	case TREE_MSG_NODE_DELETE:
		e->entry = NULL;
		hotlist_delete_entry_internal(e);
		break;

	case TREE_MSG_NODE_EDIT:
		break;

	case TREE_MSG_NODE_LAUNCH:
	{
		nserror error;
		struct browser_window *clone = NULL;
		enum browser_window_nav_flags flags =
				BROWSER_WINDOW_VERIFIABLE |
				BROWSER_WINDOW_HISTORY |
				BROWSER_WINDOW_TAB;

		/* TODO: Set clone window, to window that new tab appears in */

		if (msg.data.node_launch.mouse &
				(BROWSER_MOUSE_MOD_1 | BROWSER_MOUSE_MOD_2) ||
				clone == NULL) {
			/* Shift or Ctrl launch, open in new window rather
			 * than tab. */
			flags ^= BROWSER_WINDOW_TAB;
		}

		error = browser_window_create(flags, e->url, NULL, clone, NULL);
		if (error != NSERROR_OK) {
			warn_user(messages_get_errorcode(error), 0);
		}
	}
		break;
	}
	return NSERROR_OK;
}
struct treeview_callback_table hl_tree_cb_t = {
	.folder = hotlist_tree_node_folder_cb,
	.entry = hotlist_tree_node_entry_cb
};



typedef struct {
	treeview *tree;
	treeview_node *rel;
	enum treeview_relationship relshp;
	bool last_was_h4;
	dom_string *title;
} hotlist_load_ctx;


/**
 * Parse an entry represented as a li.
 *
 * \param li		DOM node for parsed li
 * \param ctx		Our hotlist loading context.
 * \return NSERROR_OK on success, or appropriate error otherwise
 */
static nserror hotlist_load_entry(dom_node *li, hotlist_load_ctx *ctx)
{
	dom_node *a;
	dom_string *title1;
	dom_string *url1;
	char *title;
	nsurl *url;
	const struct url_data *data;
	dom_exception derror;
	nserror err;

	/* The li must contain an "a" element */
	a = libdom_find_first_element(li, corestring_lwc_a);
	if (a == NULL) {
		warn_user("TreeLoadError", "(Missing <a> in <li>)");
		return NSERROR_NOMEM;
	}

	derror = dom_node_get_text_content(a, &title1);
	if (derror != DOM_NO_ERR) {
		warn_user("TreeLoadError", "(No title)");
		dom_node_unref(a);
		return NSERROR_NOMEM;
	}

	derror = dom_element_get_attribute(a, corestring_dom_href, &url1);
	if (derror != DOM_NO_ERR || url1 == NULL) {
		warn_user("TreeLoadError", "(No URL)");
		dom_string_unref(title1);
		dom_node_unref(a);
		return NSERROR_NOMEM;
	}
	dom_node_unref(a);

	if (title1 != NULL) {
		title = strndup(dom_string_data(title1),
				dom_string_byte_length(title1));
		dom_string_unref(title1);
	} else {
		title = strdup("");
	}
	if (title == NULL) {
		warn_user("NoMemory", NULL);
		dom_string_unref(url1);
		return NSERROR_NOMEM;
	}

	/* Need to get URL as a nsurl object */
	err = nsurl_create(dom_string_data(url1), &url);
	dom_string_unref(url1);

	if (err != NSERROR_OK) {
		LOG(("Failed normalising '%s'", dom_string_data(url1)));

		warn_user(messages_get_errorcode(err), NULL);

		free(title);

		return err;
	}

	/* Get the URL data */
	data = urldb_get_url_data(url);
	if (data == NULL) {
		/* No entry in database, so add one */
		urldb_add_url(url);
		/* now attempt to get url data */
		data = urldb_get_url_data(url);
	}
	if (data == NULL) {
		nsurl_unref(url);
		free(title);

		return NSERROR_NOMEM;
	}

	/* Make this URL persistent */
	urldb_set_url_persistence(url, true);

	/* Add the entry */
	err = hotlist_add_entry_internal(url, title, data, ctx->rel,
			ctx->relshp, &ctx->rel);
	nsurl_unref(url);
	ctx->relshp = TREE_REL_NEXT_SIBLING;

	if (err != NSERROR_OK) {
		free(title);

		return err;
	}

	return NSERROR_OK;
}


/*
 * Callback for libdom_iterate_child_elements, which dispite the namespace is
 * a NetSurf function.
 *
 * \param node		Node that is a child of the directory UL node
 * \param ctx		Our hotlist loading context.
 */
static nserror hotlist_load_directory_cb(dom_node *node, void *ctx);

/**
 * Parse a directory represented as a ul.
 *
 * \param  ul		DOM node for parsed ul
 * \param  directory	directory to add this directory to
 * \return NSERROR_OK on success, or appropriate error otherwise
 */
static nserror hotlist_load_directory(dom_node *ul, hotlist_load_ctx *ctx)
{
	assert(ul != NULL);
	assert(ctx != NULL);

	return libdom_iterate_child_elements(ul,
			hotlist_load_directory_cb, ctx);
}


/* Documented above, in forward declaration */
nserror hotlist_load_directory_cb(dom_node *node, void *ctx)
{
	/* TODO: return appropriate errors */
	hotlist_load_ctx *current_ctx = ctx;
	dom_string *name;
	dom_exception error;
	nserror err;

	/* The ul may contain entries as a li, or directories as
	 * an h4 followed by a ul. Non-element nodes may be present
	 * (eg. text, comments), and are ignored. */

	error = dom_node_get_node_name(node, &name);
	if (error != DOM_NO_ERR || name == NULL)
		return NSERROR_NOMEM;

	if (dom_string_caseless_lwc_isequal(name, corestring_lwc_li)) {
		/* Entry handling */
		hotlist_load_entry(node, current_ctx);
		current_ctx->last_was_h4 = false;

	} else if (dom_string_caseless_lwc_isequal(name, corestring_lwc_h4)) {
		/* Directory handling, part 1: Get title from H4 */
		dom_string *title;

		error = dom_node_get_text_content(node, &title);
		if (error != DOM_NO_ERR || title == NULL) {
			warn_user("TreeLoadError", "(Empty <h4> "
					"or memory exhausted.)");
			dom_string_unref(name);
			return NSERROR_NOMEM;
		}

		if (current_ctx->title != NULL)
			dom_string_unref(current_ctx->title);
		current_ctx->title = title;
		current_ctx->last_was_h4 = true;

	} else if (current_ctx->last_was_h4 &&
			dom_string_caseless_lwc_isequal(name, 
					corestring_lwc_ul)) {
		/* Directory handling, part 2: Make node, and handle children */
		char *title;
		hotlist_load_ctx new_ctx;
		struct treeview_field_data *field;
		treeview_node *rel;

		title = strndup(dom_string_data(current_ctx->title),
				dom_string_byte_length(current_ctx->title));
		if (title == NULL) {
			dom_string_unref(name);
			return NSERROR_NOMEM;
		}

		/* Create the folder node's title field */
		field = malloc(sizeof(struct treeview_field_data));
		if (field == NULL) {
			dom_string_unref(name);
			free(title);
			return NSERROR_NOMEM;
		}
		field->field = hl_ctx.fields[HL_FOLDER].field;
		field->value = title;
		field->value_len = strlen(title);

		/* Create the folder node */
		err = treeview_create_node_folder(current_ctx->tree,
				&rel, current_ctx->rel, current_ctx->relshp,
				field, field, hl_ctx.built ? TREE_CREATE_NONE :
						TREE_CREATE_SUPPRESS_RESIZE);
		current_ctx->rel = rel;
		current_ctx->relshp = TREE_REL_NEXT_SIBLING;

		if (err != NSERROR_OK) {
			dom_string_unref(name);
			return NSERROR_NOMEM;
		}

		new_ctx.tree = current_ctx->tree;
		new_ctx.rel = rel;
		new_ctx.relshp = TREE_REL_FIRST_CHILD;
		new_ctx.last_was_h4 = false;
		new_ctx.title = NULL;

		/* And load its contents */
		err = hotlist_load_directory(node, &new_ctx);
		if (err != NSERROR_OK) {
			dom_string_unref(name);
			return NSERROR_NOMEM;
		}

		if (new_ctx.title != NULL) {
			dom_string_unref(new_ctx.title);
			new_ctx.title = NULL;
		}
		current_ctx->last_was_h4 = false;
	} else {
		current_ctx->last_was_h4 = false;
	}

	dom_string_unref(name);

	return NSERROR_OK;
}


/*
 * Load the hotlist data from file
 *
 * \param path		The path to load the hotlist file from, or NULL
 * \param loaded	Updated to true iff hotlist file loaded, else set false
 * \return NSERROR_OK on success, or appropriate error otherwise
 */
static nserror hotlist_load(const char *path, bool *loaded)
{
	dom_document *document;
	dom_node *html, *body, *ul;
	hotlist_load_ctx ctx;
	nserror err;

	*loaded = false;

	/* Handle no path */
	if (path == NULL) {
		LOG(("No hotlist file path provided."));
		return NSERROR_OK;
	}

	/* Load hotlist file */
	err = libdom_parse_file(path, "iso-8859-1", &document);
	if (err != NSERROR_OK) {
		return err;
	}

	/* Find HTML element */
	html = libdom_find_first_element((dom_node *) document,
			corestring_lwc_html);
	if (html == NULL) {
		dom_node_unref(document);
		warn_user("TreeLoadError", "(<html> not found)");
		return NSERROR_OK;
	}

	/* Find BODY element */
	body = libdom_find_first_element(html, corestring_lwc_body);
	if (body == NULL) {
		dom_node_unref(html);
		dom_node_unref(document);
		warn_user("TreeLoadError", "(<html>...<body> not found)");
		return NSERROR_OK;
	}

	/* Find UL element */
	ul = libdom_find_first_element(body, corestring_lwc_ul);
	if (ul == NULL) {
		dom_node_unref(body);
		dom_node_unref(html);
		dom_node_unref(document);
		warn_user("TreeLoadError",
					"(<html>...<body>...<ul> not found.)");
		return NSERROR_OK;
	}

	/* Set up the hotlist loading context */
	ctx.tree = hl_ctx.tree;
	ctx.rel = NULL;
	ctx.relshp = TREE_REL_FIRST_CHILD;
	ctx.last_was_h4 = false;
	ctx.title = NULL;

	err = hotlist_load_directory(ul, &ctx);

	if (ctx.title != NULL) {
		dom_string_unref(ctx.title);
		ctx.title = NULL;
	}

	dom_node_unref(ul);
	dom_node_unref(body);
	dom_node_unref(html);
	dom_node_unref(document);

	if (err != NSERROR_OK) {
		warn_user("TreeLoadError", "(Failed building tree.)");
		return NSERROR_OK;
	}

	return NSERROR_OK;
}


/*
 * Generate default hotlist
 *
 * \return NSERROR_OK on success, or appropriate error otherwise
 */
static nserror hotlist_generate(void)
{
	/* TODO */
	return NSERROR_OK;
}


/*
 * Save hotlist to file
 *
 * \return NSERROR_OK on success, or appropriate error otherwise
 */
static nserror hotlist_export(const char *path)
{
	/* TODO */
	return NSERROR_OK;
}


/**
 * Initialise the treeview entry feilds
 *
 * \return NSERROR_OK on success, or appropriate error otherwise
 */
static nserror hotlist_initialise_entry_fields(void)
{
	int i;
	const char *label;

	for (i = 0; i < HL_N_FIELDS; i++)
		hl_ctx.fields[i].field = NULL;

	hl_ctx.fields[HL_TITLE].flags = TREE_FLAG_DEFAULT;
	label = "TreeviewLabelTitle";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&hl_ctx.fields[HL_TITLE].field) !=
			lwc_error_ok) {
		goto error;
	}

	hl_ctx.fields[HL_URL].flags = TREE_FLAG_NONE;
	label = "TreeviewLabelURL";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&hl_ctx.fields[HL_URL].field) !=
			lwc_error_ok) {
		goto error;
	}

	hl_ctx.fields[HL_LAST_VISIT].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelLastVisit";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&hl_ctx.fields[HL_LAST_VISIT].field) !=
			lwc_error_ok) {
		goto error;
	}

	hl_ctx.fields[HL_VISITS].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelVisits";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&hl_ctx.fields[HL_VISITS].field) !=
			lwc_error_ok) {
		goto error;
	}

	hl_ctx.fields[HL_FOLDER].flags = TREE_FLAG_DEFAULT;
	label = "TreeviewLabelFolder";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&hl_ctx.fields[HL_FOLDER].field) !=
			lwc_error_ok) {
		return false;
	}

	return NSERROR_OK;

error:
	for (i = 0; i < HL_N_FIELDS; i++)
		if (hl_ctx.fields[i].field != NULL)
			lwc_string_unref(hl_ctx.fields[i].field);

	return NSERROR_UNKNOWN;
}


/*
 * Populate the hotlist from file, or generate default hotlist if no file
 *
 * \param path		The path to load the hotlist file from, or NULL
 * \return NSERROR_OK on success, or appropriate error otherwise
 */
static nserror hotlist_populate(const char *path)
{
	nserror err;
	bool loaded;

	/* Load hotlist file */
	err = hotlist_load(path, &loaded);

	/* Ignoring errors, since if there was an error, we want to generate
	 * the default hotlist anyway. */

	if (loaded)
		return NSERROR_OK;

	/* Couldn't load hotlist, generate a default one */
	err = hotlist_generate();
	if (err != NSERROR_OK) {
		return err;
	}

	return NSERROR_OK;
}


/* Exported interface, documented in hotlist.h */
nserror hotlist_init(struct core_window_callback_table *cw_t,
		void *core_window_handle, const char *path)
{
	nserror err;

	LOG(("Loading hotlist"));

	/* Init. hotlist treeview entry fields */
	err = hotlist_initialise_entry_fields();
	if (err != NSERROR_OK) {
		hl_ctx.tree = NULL;
		return err;
	}

	/* Create the hotlist treeview */
	err = treeview_create(&hl_ctx.tree, &hl_tree_cb_t,
			HL_N_FIELDS, hl_ctx.fields,
			cw_t, core_window_handle,
			TREEVIEW_NO_MOVES | TREEVIEW_DEL_EMPTY_DIRS);
	if (err != NSERROR_OK) {
		hl_ctx.tree = NULL;
		return err;
	}

	/* Populate the hotlist */
	err = hotlist_populate(path);
	if (err != NSERROR_OK) {
		return err;
	}

	/* Hotlist tree is built
	 * We suppress the treeview height callback on entry insertion before
	 * the treeview is built. */
	hl_ctx.built = true;

	LOG(("Loaded hotlist"));

	return NSERROR_OK;
}


/* Exported interface, documented in hotlist.h */
nserror hotlist_fini(const char *path)
{
	int i;
	nserror err;

	LOG(("Finalising hotlist"));

	hl_ctx.built = false;

	/* Save the hotlist */
	err = hotlist_export(path);
	if (err != NSERROR_OK) {
		warn_user("Couldn't save the hotlist.", 0);
	}

	/* Destroy the hotlist treeview */
	err = treeview_destroy(hl_ctx.tree);

	/* Free hotlist treeview entry fields */
	for (i = 0; i < HL_N_FIELDS; i++)
		if (hl_ctx.fields[i].field != NULL)
			lwc_string_unref(hl_ctx.fields[i].field);

	LOG(("Finalised hotlist"));

	return err;
}


/* Exported interface, documented in hotlist.h */
nserror hotlist_add(nsurl *url)
{
	const struct url_data *data;

	/* If we don't have a hotlist at the moment, just return OK */
	if (hl_ctx.tree == NULL)
		return NSERROR_OK;

	data = urldb_get_url_data(url);
	if (data == NULL) {
		LOG(("Can't add URL to hotlist that's not present in urldb."));
		return NSERROR_BAD_PARAMETER;
	}

	/* TODO */
	//hotlist_add_entry(url, data);

	return NSERROR_OK;
}


/* Exported interface, documented in hotlist.h */
void hotlist_redraw(int x, int y, struct rect *clip,
		const struct redraw_context *ctx)
{
	treeview_redraw(hl_ctx.tree, x, y, clip, ctx);
}


/* Exported interface, documented in hotlist.h */
void hotlist_mouse_action(browser_mouse_state mouse, int x, int y)
{
	treeview_mouse_action(hl_ctx.tree, mouse, x, y);
}


/* Exported interface, documented in hotlist.h */
void hotlist_keypress(uint32_t key)
{
	treeview_keypress(hl_ctx.tree, key);
}

