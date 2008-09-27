/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "desktop/tree.h"
#include <proto/listbrowser.h>
#include <proto/window.h>
#include <proto/layout.h>
#include <classes/window.h>
#include <gadgets/listbrowser.h>
#include <gadgets/layout.h>
#include <reaction/reaction_macros.h>
#include "amiga/gui.h"
#include "content/urldb.h"
#include <proto/exec.h>
#include <assert.h>
#include <proto/intuition.h>
#include "amiga/tree.h"

struct treeview_window {
	struct Window *win;
	Object *objects[OID_LAST];
	struct Gadget *gadgets[GID_LAST];
	struct nsObject *node;
	ULONG pad[5];
	struct tree *tree;
	struct List *listbrowser_list;
};

void ami_add_elements(struct treeview_window *twin,struct node *root,WORD *gen);

void tree_initialise_redraw(struct tree *tree)
{
}

void tree_redraw_area(struct tree *tree, int x, int y, int width, int height)
{
}

void tree_draw_line(int x, int y, int width, int height)
{
}

void tree_draw_node_element(struct tree *tree, struct node_element *element)
{
/* add element to listbrowser list */

	struct Node *lbnode;
	struct treeview_window *twin = tree->handle;
	struct node *tempnode;
	int generation=1;
	BOOL edit = FALSE;

	tempnode = element->parent;
	edit = tempnode->editable;

	while(tempnode)
	{
		tempnode = tempnode->parent;
		generation++;
	}

	switch (element->type) {
		case NODE_ELEMENT_TEXT_PLUS_SPRITE:
		case NODE_ELEMENT_TEXT:
        		if (lbnode = AllocListBrowserNode(3,
//				LBNA_UserData,nodetime,
			LBNA_Generation,1,
	            LBNA_Column, 0,
				LBNCA_CopyText,TRUE,
 	               LBNCA_Text, element->text,
				LBNCA_Editable,edit,
 	           LBNA_Column, 1,
				LBNCA_CopyText,TRUE,
 	               LBNCA_Text, "",
 	           LBNA_Column, 2,
				LBNCA_CopyText,TRUE,
 	               LBNCA_Text, "",
 	           TAG_DONE))
	  	      {
 		           AddTail(twin->listbrowser_list, lbnode);
 		       }
		break;
	}
}

void tree_draw_node_expansion(struct tree *tree, struct node *node)
{
}

void tree_recalculate_node_element(struct node_element *element)
{
}

void tree_update_URL_node(struct node *node, const char *url,
	const struct url_data *data)
{
	struct node_element *element;
	char buffer[256];

	assert(node);

	element = tree_find_element(node, TREE_ELEMENT_URL);
	if (!element)
		return;
	if (data) {
		/* node is linked, update */
		assert(!node->editable);
		if (!data->title)
			urldb_set_url_title(url, url);

		if (!data->title)
			return;

		node->data.text = data->title;
	} else {
		/* node is not linked, find data */
		assert(node->editable);
		data = urldb_get_url_data(element->text);
		if (!data)
			return;
	}

/* not implemented yet
	if (element) {
		sprintf(buffer, "small_%.3x", ro_content_filetype_from_type(data->type));
		if (ro_gui_wimp_sprite_exists(buffer))
			tree_set_node_sprite(node, buffer, buffer);
		else
			tree_set_node_sprite(node, "small_xxx", "small_xxx");
	}
*/

	element = tree_find_element(node, TREE_ELEMENT_LAST_VISIT);
	if (element) {
		snprintf(buffer, 256, messages_get("TreeLast"),
				(data->last_visit > 0) ?
					ctime((time_t *)&data->last_visit) :
					messages_get("TreeUnknown"));
		if (data->last_visit > 0)
			buffer[strlen(buffer) - 1] = '\0';
		free((void *)element->text);
		element->text = strdup(buffer);
	}

	element = tree_find_element(node, TREE_ELEMENT_VISITS);
	if (element) {
		snprintf(buffer, 256, messages_get("TreeVisits"),
				data->visits);
		free((void *)element->text);
		element->text = strdup(buffer);
	}
}

void tree_resized(struct tree *tree)
{
}

void tree_set_node_sprite_folder(struct node *node)
{
}

void tree_set_node_sprite(struct node *node, const char *sprite,
	const char *expanded)
{
}

void ami_open_tree(struct tree *tree)
{
	struct treeview_window *twin;
	BOOL msel = TRUE;
	static WORD gen=0;
/* open window with listbrowser gadget added */

	twin = AllocVec(sizeof(struct treeview_window),MEMF_CLEAR);
	twin->listbrowser_list = AllocVec(sizeof(struct List),MEMF_CLEAR);

	static struct ColumnInfo columninfo[] =
	{
    	{ 22,"Name", CIF_DRAGGABLE | CIF_SORTABLE},
    	{ 5,"URL", CIF_DRAGGABLE },
    	{ 5,"Visits", CIF_DRAGGABLE },
    	{ -1, (STRPTR)~0, -1 }
	};

	if(tree->single_selection) msel = FALSE;

	NewList(twin->listbrowser_list);

	tree->handle = (void *)twin;
	twin->tree = tree;
	ami_add_elements(twin,twin->tree->root,&gen);

	twin->objects[OID_MAIN] = WindowObject,
      	    WA_ScreenTitle,nsscreentitle,
           	WA_Title, "treeview window",
           	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, TRUE,
           	WA_SizeGadget, TRUE,
			WA_CustomScreen,scrn,
		WINDOW_SharedPort,sport,
			WINDOW_IconifyGadget, TRUE,
         	WINDOW_Position, WPOS_CENTERSCREEN,
           	WINDOW_ParentGroup, twin->gadgets[GID_MAIN] = VGroupObject,
				LAYOUT_AddChild, twin->gadgets[GID_TREEBROWSER] = ListBrowserObject,
    		   	 		GA_ID, GID_TREEBROWSER,
        				GA_RelVerify, TRUE,
					GA_ReadOnly,FALSE,
					LISTBROWSER_ColumnInfo, &columninfo,
					LISTBROWSER_ColumnTitles, TRUE,
					LISTBROWSER_Hierarchical,TRUE,
//	LISTBROWSER_TitleClickable,TRUE,
					LISTBROWSER_AutoFit, TRUE,
					LISTBROWSER_HorizontalProp, TRUE,
					LISTBROWSER_Labels, twin->listbrowser_list,
					LISTBROWSER_MultiSelect,msel,
        		ListBrowserEnd,
				CHILD_NominalSize,TRUE,
			EndGroup,
		EndWindow;

	twin->win = (struct Window *)RA_OpenWindow(twin->objects[OID_MAIN]);

	twin->node = AddObject(window_list,AMINS_TVWINDOW);
	twin->node->objstruct = twin;
}

/**
 * Launches a node using all known methods.
 *
 * \param node  the node to launch
 * \return whether the node could be launched
 */
bool ami_tree_launch_node(struct tree *tree, struct node *node)
{
	struct node_element *element;

	assert(node);

	element = tree_find_element(node, TREE_ELEMENT_URL);
	if (element) {
		browser_window_create(element->text, NULL, 0, true, false);
		return true;
	}

#ifdef WITH_SSL
/* not implemented yet
	element = tree_find_element(node, TREE_ELEMENT_SSL);
	if (element) {
		ro_gui_cert_open(tree, node);
		return true;
	}
*/
#endif

	return false;
}

void ami_tree_close(struct tree *tree)
{
	struct treeview_window *twin = tree->handle;

	DisposeObject(twin->objects[OID_MAIN]);
	FreeListBrowserList(twin->listbrowser_list);
	//free_browserlist(twin->listbrowser_list);
	DelObject(twin->node);
	win_destroyed = true;
}

void free_browserlist(struct List *list)
{
    struct Node *node, *nextnode;

	if(IsListEmpty(list)) return;

    node = list->lh_Head;
    while (nextnode = node->ln_Succ)
    {
		FreeVec(node->ln_Name);
        FreeListBrowserNode(node);
        node = nextnode;
    }
}

void ami_add_elements(struct treeview_window *twin,struct node *root,WORD *gen)
{
	struct Node *lbnode;
	struct tree *tree = twin->tree;
	struct node *tempnode;
	int generation=1;
	BOOL edit = FALSE;
	struct node_element *element=NULL,*element2=NULL,*element3=NULL;
	struct node *node;

	*gen = *gen + 1;
	for (node = root; node; node = node->next)
	{
		element = tree_find_element(node, TREE_ELEMENT_NAME);
		if(!element) element = tree_find_element(node, TREE_ELEMENT_TITLE);
//		printf("node %lx url %s gen %ld\n",node,element->text,*gen);
		element2 = tree_find_element(node, TREE_ELEMENT_URL);
		element3 = tree_find_element(node, TREE_ELEMENT_VISITS);

		switch (element->type) {
			case NODE_ELEMENT_TEXT_PLUS_SPRITE:
			case NODE_ELEMENT_TEXT:
    	    		if (lbnode = AllocListBrowserNode(3,
//					LBNA_UserData,nodetime,
				LBNA_Generation,*gen,
	            	LBNA_Column, 0,
					LBNCA_CopyText,TRUE,
	 	               LBNCA_Text, element->text,
					LBNCA_Editable,edit,
 	    	       LBNA_Column, 1,
					LBNCA_CopyText,TRUE,
 	            	   LBNCA_Text, element2->text,
	 	           LBNA_Column, 2,
					LBNCA_CopyText,TRUE,
	 	               LBNCA_Text, "",
 		           TAG_DONE))
					{
						AddTail(twin->listbrowser_list, lbnode);
 		       		}
			break;
		}

		if (node->child)
		{
			ami_add_elements(twin,node->child,gen);
		}
	}
	*gen = *gen - 1;
}
