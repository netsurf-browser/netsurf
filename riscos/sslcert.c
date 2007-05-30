/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * SSL Certificate verification UI (implementation)
 */

#include "utils/config.h"

#ifdef WITH_SSL

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "oslib/wimp.h"
#include "content/content.h"
#include "content/fetch.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "desktop/tree.h"
#include "riscos/dialog.h"
#include "riscos/textarea.h"
#include "riscos/treeview.h"
#include "riscos/wimp_event.h"
#include "riscos/wimp.h"
#include "utils/log.h"
#include "utils/utils.h"

#define ICON_SSL_PANE 1
#define ICON_SSL_REJECT 3
#define ICON_SSL_ACCEPT 4

#define ICON_CERT_VERSION 3
#define ICON_CERT_VALID_FROM 5
#define ICON_CERT_TYPE 7
#define ICON_CERT_VALID_TO 9
#define ICON_CERT_SERIAL 11
#define ICON_CERT_ISSUER 13
#define ICON_CERT_SUBJECT 15

static wimp_window *dialog_tree_template;
static wimp_window *dialog_cert_template;
static wimp_window *dialog_display_template;

struct session_data {
	struct session_cert *certs;
	unsigned long num;
	struct browser_window *bw;
	char *url;
	struct tree *tree;
};
struct session_cert {
	char version[16], valid_from[32], valid_to[32], type[8], serial[32];
	char *issuer_t;
	char *subject_t;
	uintptr_t issuer;
	uintptr_t subject;
};

static bool ro_gui_cert_click(wimp_pointer *pointer);
static void ro_gui_cert_close(wimp_w w);
static bool ro_gui_cert_apply(wimp_w w);

/**
 * Load the cert window template
 */

void ro_gui_cert_init(void)
{
	dialog_tree_template = ro_gui_dialog_load_template("tree");
	dialog_cert_template = ro_gui_dialog_load_template("sslcert");
	dialog_display_template = ro_gui_dialog_load_template("ssldisplay");

	dialog_tree_template->flags &= ~(wimp_WINDOW_MOVEABLE |
			wimp_WINDOW_BACK_ICON |
			wimp_WINDOW_CLOSE_ICON |
			wimp_WINDOW_TITLE_ICON |
			wimp_WINDOW_SIZE_ICON |
			wimp_WINDOW_TOGGLE_ICON);
}

/**
 * Open the certificate verification dialog
 */

void gui_cert_verify(struct browser_window *bw, struct content *c,
		const struct ssl_cert_info *certs, unsigned long num)
{
	wimp_w w;
	wimp_w ssl_w;
	const struct ssl_cert_info *from;
	struct session_cert *to;
	struct session_data *data;
	struct tree *tree;
	struct node *node;
	wimp_window_state state;
	wimp_icon_state istate;
	os_error *error;
	long i;

	assert(bw && c && certs);

	/* copy the certificate information */
	data = calloc(1, sizeof(struct session_data));
	if (!data) {
		warn_user("NoMemory", 0);
		return;
	}
	data->url = strdup(c->url);
	if (!data->url) {
		free(data);
		warn_user("NoMemory", 0);
		return;
	}
	data->bw = bw;
	data->num = num;
	data->certs = calloc(num, sizeof(struct session_cert));
	if (!data->certs) {
		free(data->url);
		free(data);
		warn_user("NoMemory", 0);
		return;
	}
	for (i = 0; i < (long)num; i++) {
		to = &data->certs[i];
		from = &certs[i];
		to->subject_t = strdup(from->subject);
		to->issuer_t = strdup(from->issuer);
		if ((!to->subject_t) || (!to->issuer_t)) {
			for (; i >= 0; i--) {
				to = &data->certs[i];
				free(to->subject_t);
				free(to->issuer_t);
			}
			free(data->certs);
			free(data->url);
			free(data);
			warn_user("NoMemory", 0);
			return;
		}
		snprintf(to->version, sizeof data->certs->version, "%ld",
				from->version);
		snprintf(to->valid_from, sizeof data->certs->valid_from, "%s",
				from->not_before);
		snprintf(to->type, sizeof data->certs->type, "%d",
				from->cert_type);
		snprintf(to->valid_to, sizeof data->certs->valid_to, "%s",
				from->not_after);
		snprintf(to->serial, sizeof data->certs->serial, "%ld",
				from->serial);
	}

	/* create the SSL window */
	error = xwimp_create_window(dialog_cert_template, &ssl_w);
	if (error) {
		free(data->certs);
		free(data->url);
		free(data);
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}

	/* automated SSL window event handling */
	ro_gui_wimp_event_set_user_data(ssl_w, data);
	ro_gui_wimp_event_register_cancel(ssl_w, ICON_SSL_REJECT);
	ro_gui_wimp_event_register_ok(ssl_w, ICON_SSL_ACCEPT, ro_gui_cert_apply);
	ro_gui_dialog_open_persistent(bw->window->window, ssl_w, false);

	/* create a tree window (styled as a list) */
	error = xwimp_create_window(dialog_tree_template, &w);
	if (error) {
		ro_gui_cert_close(ssl_w);
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}
	tree = calloc(sizeof(struct tree), 1);
	if (!tree) {
		ro_gui_cert_close(ssl_w);
		warn_user("NoMemory", 0);
		return;
	}
	tree->root = tree_create_folder_node(NULL, "Root");
	if (!tree->root) {
		ro_gui_cert_close(ssl_w);
		warn_user("NoMemory", 0);
		free(tree);
		tree = NULL;
	}
	tree->root->expanded = true;
	tree->handle = (int)w;
	tree->movable = false;
	tree->no_drag = true;
	tree->no_vscroll = true;
	tree->no_furniture = true;
	tree->single_selection = true;
	data->tree = tree;

	/* put the SSL names in the tree */
	for (i = 0; i < (long)num; i++) {
		node = tree_create_leaf_node(tree->root, certs[i].subject);
		if (node) {
			node->data.data = TREE_ELEMENT_SSL;
			tree_set_node_sprite(node, "small_xxx", "small_xxx");
		}
	}

	/* automated treeview event handling */
	ro_gui_wimp_event_set_user_data(w, tree);
	ro_gui_wimp_event_register_keypress(w, ro_gui_tree_keypress);
	ro_gui_wimp_event_register_redraw_window(w, ro_gui_tree_redraw);
	ro_gui_wimp_event_register_open_window(w, ro_gui_tree_open);
	ro_gui_wimp_event_register_close_window(w, ro_gui_wimp_event_finalise);
	ro_gui_wimp_event_register_mouse_click(w, ro_gui_cert_click);

	/* nest the tree window inside the pane window */
	state.w = ssl_w;
	error = xwimp_get_window_state(&state);
	if (error) {
		ro_gui_cert_close(ssl_w);
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}

	istate.w = ssl_w;
	istate.i = ICON_SSL_PANE;
	error = xwimp_get_icon_state(&istate);
	if (error) {
		ro_gui_cert_close(ssl_w);
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}
	state.w = w;
	state.visible.x1 = state.visible.x0 + istate.icon.extent.x1 - 20 -
			ro_get_vscroll_width(w);
	state.visible.x0 += istate.icon.extent.x0 + 20;
	state.visible.y0 = state.visible.y1 + istate.icon.extent.y0 + 20;
	state.visible.y1 += istate.icon.extent.y1 - 32;
	error = xwimp_open_window_nested((wimp_open *)&state, ssl_w,
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_XORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_YORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_LS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_RS_EDGE_SHIFT);
	if (error) {
		ro_gui_cert_close(ssl_w);
		LOG(("xwimp_open_window_nested: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}
	tree_initialise(tree);
}

void ro_gui_cert_open(struct tree *tree, struct node *node)
{
	struct node *n;
	struct session_data *data;
	struct session_cert *session;
	wimp_window_state state;
	wimp_w child;
	wimp_w parent;
	wimp_w w;
	unsigned long i;
	os_error *error;

	assert(tree->root);

	/* firstly we need to get our node index in the list */
	for (n = tree->root->child, i = 0; n; i++, n = n->next)
		if (n == node)
			break;
	assert(n);

	/* now we get the handle of our list window */
	child = (wimp_w)tree->handle;
	assert(child);

	/* now we can get the linked parent handle */
	state.w = child;
	error = xwimp_get_window_state_and_nesting(&state, &parent, 0);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	assert(parent);

	/* from this we can get our session data */
	data = (struct session_data *)ro_gui_wimp_event_get_user_data(parent);
	assert(data);
	assert(data->tree == tree);

	/* and finally the nodes session certificate data */
	session = &data->certs[i];
	assert(session);

	dialog_display_template->icons[ICON_CERT_VERSION].data.indirected_text.text = session->version;
	dialog_display_template->icons[ICON_CERT_VERSION].data.indirected_text.size = strlen(session->version) + 1;
	dialog_display_template->icons[ICON_CERT_VALID_FROM].data.indirected_text.text = session->valid_from;
	dialog_display_template->icons[ICON_CERT_VALID_FROM].data.indirected_text.size = strlen(session->valid_from) + 1;
	dialog_display_template->icons[ICON_CERT_TYPE].data.indirected_text.text = session->type;
	dialog_display_template->icons[ICON_CERT_TYPE].data.indirected_text.size = strlen(session->type) + 1;
	dialog_display_template->icons[ICON_CERT_VALID_TO].data.indirected_text.text = session->valid_to;
	dialog_display_template->icons[ICON_CERT_VALID_TO].data.indirected_text.size = strlen(session->valid_to) + 1;
	dialog_display_template->icons[ICON_CERT_SERIAL].data.indirected_text.text = session->serial;
	dialog_display_template->icons[ICON_CERT_SERIAL].data.indirected_text.size = strlen(session->serial) + 1;

	error = xwimp_create_window(dialog_display_template, &w);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		free(session);
		warn_user("MiscError", error->errmess);
		return;
	}
	if (session->issuer)
		textarea_destroy(session->issuer);
	session->issuer = textarea_create(w, ICON_CERT_ISSUER,
			TEXTAREA_MULTILINE | TEXTAREA_READONLY,
			ro_gui_desktop_font_family, ro_gui_desktop_font_size,
			ro_gui_desktop_font_style);
	if (!session->issuer) {
		xwimp_delete_window(w);
		warn_user("NoMemory", 0);
		return;
	}
	if (!textarea_set_text(session->issuer, session->issuer_t)) {
		textarea_destroy(session->issuer);
		xwimp_delete_window(w);
		warn_user("NoMemory", 0);
		return;
	}

	if (session->subject)
		textarea_destroy(session->subject);
	session->subject = textarea_create(w, ICON_CERT_SUBJECT,
			TEXTAREA_MULTILINE | TEXTAREA_READONLY,
			ro_gui_desktop_font_family, ro_gui_desktop_font_size,
			ro_gui_desktop_font_style);
	if (!session->subject) {
		textarea_destroy(session->issuer);
		xwimp_delete_window(w);
		warn_user("NoMemory", 0);
		return;
	}
	if (!textarea_set_text(session->subject, session->subject_t)) {
		textarea_destroy(session->subject);
		textarea_destroy(session->issuer);
		xwimp_delete_window(w);
		warn_user("NoMemory", 0);
		return;
	}
	ro_gui_wimp_event_register_close_window(w, ro_gui_wimp_event_finalise);
	ro_gui_dialog_open_persistent(parent, w, false);
}

/**
 * Handle closing of certificate verification dialog
 */
void ro_gui_cert_close(wimp_w w)
{
	struct session_data *data;
	os_error *error;
	unsigned long i;

	data = (struct session_data *)ro_gui_wimp_event_get_user_data(w);
	assert(data);

	for (i = 0; i < data->num; i++) {
		if (data->certs[i].subject)
			textarea_destroy(data->certs[i].subject);
		if (data->certs[i].issuer)
			textarea_destroy(data->certs[i].issuer);
	}
	free(data->certs);
	free(data->url);
	free(data);

	if (data->tree) {
		tree_delete_node(data->tree, data->tree->root, false);
		ro_gui_dialog_close((wimp_w)data->tree->handle);
		error = xwimp_delete_window((wimp_w)data->tree->handle);
		if (error) {
			LOG(("xwimp_delete_window: 0x%x:%s",
				error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		ro_gui_wimp_event_finalise((wimp_w)data->tree->handle);
		free(data->tree);
	}

	ro_gui_dialog_close(w);
	error = xwimp_delete_window(w);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x:%s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

}

/**
 * Handle acceptance of certificate
 */
bool ro_gui_cert_apply(wimp_w w)
{
	struct session_data *session;

	session = (struct session_data *)ro_gui_wimp_event_get_user_data(w);
	assert(session);

	urldb_set_cert_permissions(session->url, true);
	browser_window_go(session->bw, session->url, 0, true);
	return true;
}

bool ro_gui_cert_click(wimp_pointer *pointer)
{
	struct tree *tree;

	tree = (struct tree *)ro_gui_wimp_event_get_user_data(pointer->w);
	ro_gui_tree_click(pointer, tree);
	return true;
}

#endif
