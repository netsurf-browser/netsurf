/*
 * Copyright 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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
#include <string.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include "utils/utils.h"
#include "utils/messages.h"
#include "content/urldb.h"
#include "content/fetch.h"
#include "desktop/tree.h"
#include "amiga/tree.h"
#include "amiga/gui.h"

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

void ami_gui_cert_close(struct session_data *data);
bool ami_gui_cert_apply(struct session_data *session);

void gui_cert_verify(struct browser_window *bw, struct content *c,
		const struct ssl_cert_info *certs, unsigned long num)
{
	const struct ssl_cert_info *from;
	struct session_cert *to;
	struct session_data *data;
	struct tree *tree;
	struct node *node;
	long i;
	STRPTR yesorno,reqcontents;
	int res = 0;
	struct treeview_window *twin;

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

	tree = calloc(sizeof(struct tree), 1);
	if (!tree) {
		//ro_gui_cert_close(ssl_w);
		warn_user("NoMemory", 0);
		return;
	}
	tree->root = tree_create_folder_node(NULL, "Root");
	if (!tree->root) {
//		ro_gui_cert_close(ssl_w);
		warn_user("NoMemory", 0);
		free(tree);
		tree = NULL;
		return;
	}
	tree->root->expanded = true;
	tree->handle = 0;
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
	tree_initialise(tree);

	ami_open_tree(tree,AMI_TREE_SSLCERT);
	twin = (struct treeview_window *)data->tree->handle;

	if(yesorno = ASPrintf("%s|%s",messages_get("Yes"),messages_get("No")))
	{
		if(reqcontents = ASPrintf("%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n%s: %s\n\n%s",
							messages_get("ssl subject"),
							to->subject_t,
							messages_get("ssl issuer"),
							to->issuer_t,
							messages_get("ssl version"),
							to->version,
							messages_get("ssl valid_from"),
							to->valid_from,
							messages_get("ssl type"),
							to->type,
							messages_get("ssl valid_to"),
							to->valid_to,
							messages_get("ssl serial"),
							to->serial,
							messages_get("ssl question")))
		{
			res = TimedDosRequesterTags(TDR_ImageType,TDRIMAGE_QUESTION,
							TDR_Window,twin->win,
							TDR_TitleString,messages_get("NetSurf"),
							TDR_GadgetString,yesorno,
							TDR_FormatString,reqcontents,
							TAG_DONE);

			FreeVec(reqcontents);
		}

		FreeVec(yesorno);
	}

	if(res == 1)
	{
		ami_gui_cert_apply(data);
	}
	ami_gui_cert_close(data);

}

void ami_gui_cert_close(struct session_data *data)
{
	unsigned long i;

	if(data->tree->handle)
	{
		ami_tree_close((struct treeview_window *)data->tree->handle);
		win_destroyed = true;
	}

	assert(data);

/*
	for (i = 0; i < data->num; i++) {
		if (data->certs[i].subject)
			textarea_destroy(data->certs[i].subject);
		if (data->certs[i].issuer)
			textarea_destroy(data->certs[i].issuer);
	}
*/

	if (data->tree) {
		tree_delete_node(data->tree, data->tree->root, false);
		free(data->tree);
	}

	free(data->certs);
	free(data->url);
	free(data);
}

bool ami_gui_cert_apply(struct session_data *session)
{
	assert(session);

	urldb_set_cert_permissions(session->url, true);
	browser_window_go(session->bw, session->url, 0, true);
	return true;
}
