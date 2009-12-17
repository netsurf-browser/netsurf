/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
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

/** \file
 * Free text search (implementation)
 */

#include "utils/config.h"
#include <ctype.h>
#include <string.h>
#include "content/content.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "desktop/search.h"
#include "desktop/selection.h"
#include "render/box.h"
#include "render/html.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "amiga/search.h"
#include "amiga/object.h"
#include <proto/intuition.h>
#include <proto/exec.h>

#include <proto/window.h>
#include <proto/layout.h>
#include <proto/string.h>
#include <proto/button.h>
#include <proto/label.h>
#include <proto/checkbox.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/string.h>
#include <gadgets/button.h>
#include <gadgets/checkbox.h>
#include <images/label.h>
#include <reaction/reaction_macros.h>

#ifndef NOF_ELEMENTS
#define NOF_ELEMENTS(array) (sizeof(array)/sizeof(*(array)))
#endif

struct list_entry {
	unsigned start_idx;	/* start position of match */
	unsigned end_idx;	/* end of match */

	struct box *start_box;	/* used only for html contents */
	struct box *end_box;

	struct selection *sel;

	struct list_entry *prev;
	struct list_entry *next;
};

static bool search_insert;

static struct find_window *fwin = NULL;

search_flags_t ami_search_flags(void);
char *ami_search_string(void);
static void ami_search_set_status(bool found, void *p);
static void ami_search_set_hourglass(bool active, void *p);
static void ami_search_add_recent(const char *string, void *p);
static void ami_search_set_forward_state(bool active, void *p);
static void ami_search_set_back_state(bool active, void *p);

static struct search_callbacks ami_search_callbacks = {
	ami_search_set_forward_state,
	ami_search_set_back_state,
	ami_search_set_status,
	ami_search_set_hourglass,
	ami_search_add_recent
};


/**
 * Change the displayed search status.
 *
 * \param found  search pattern matched in text
 */
void ami_search_open(struct gui_window *gwin)
{
	struct content *c = gwin->shared->bw->current_content;

	/* only handle html/textplain contents */
	if ((!c) || (c->type != CONTENT_HTML &&
			c->type != CONTENT_TEXTPLAIN))
		return;

	if (gwin->shared->bw->search_context == NULL)
		search_create_context(gwin->shared->bw,
				&ami_search_callbacks, NULL);
	search_insert = true;

	if(fwin)
	{
		if(fwin->gwin->shared->bw->search_context != NULL)
			search_destroy_context(fwin->gwin->shared->bw->
					search_context);
		ami_search_set_forward_state(true, NULL);
		ami_search_set_back_state(true, NULL);
		fwin->gwin->shared->searchwin = NULL;
		fwin->gwin = gwin;
		gwin->shared->searchwin = fwin;
		WindowToFront(fwin->win);
		ActivateWindow(fwin->win);
		return;
	}

	fwin = AllocVec(sizeof(struct find_window),MEMF_PRIVATE | MEMF_CLEAR);

	fwin->objects[OID_MAIN] = WindowObject,
      	WA_ScreenTitle,nsscreentitle,
           	WA_Title,messages_get("FindTextNS"),
           	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, TRUE,
           	WA_SizeGadget, TRUE,
		WA_CustomScreen,scrn,
		WINDOW_SharedPort,sport,
		WINDOW_UserData,fwin,
		WINDOW_IconifyGadget, FALSE,
		WINDOW_LockHeight,TRUE,
         	WINDOW_Position, WPOS_CENTERSCREEN,
           	WINDOW_ParentGroup, fwin->gadgets[GID_MAIN] = VGroupObject,
				LAYOUT_AddChild, fwin->gadgets[GID_SEARCHSTRING] = StringObject,
					GA_ID,GID_SEARCHSTRING,
					GA_TabCycle,TRUE,
					GA_RelVerify,TRUE,
				StringEnd,
/*
				CHILD_Label, LabelObject,
					LABEL_Text,messages_get("searchstring"),
				LabelEnd,
*/
				CHILD_WeightedHeight,0,
				LAYOUT_AddChild, fwin->gadgets[GID_CASE] = CheckBoxObject,
					GA_ID,GID_CASE,
					GA_Text,messages_get("CaseSens"),
					GA_Selected,FALSE,
					GA_TabCycle,TRUE,
					GA_RelVerify,TRUE,
				CheckBoxEnd,
				LAYOUT_AddChild, fwin->gadgets[GID_SHOWALL] = CheckBoxObject,
					GA_ID,GID_SHOWALL,
					GA_Text,messages_get("ShowAll"),
					GA_Selected,FALSE,
					GA_TabCycle,TRUE,
					GA_RelVerify,TRUE,
				CheckBoxEnd,

				LAYOUT_AddChild, HGroupObject,
					LAYOUT_AddChild, fwin->gadgets[GID_PREV] = ButtonObject,
						GA_ID,GID_PREV,
						GA_RelVerify,TRUE,
						GA_Text,messages_get("Prev"),
						GA_TabCycle,TRUE,
						GA_Disabled,TRUE,
					ButtonEnd,
					CHILD_WeightedHeight,0,
					LAYOUT_AddChild, fwin->gadgets[GID_NEXT] = ButtonObject,
						GA_ID,GID_NEXT,
						GA_RelVerify,TRUE,
						GA_Text,messages_get("Next"),
						GA_TabCycle,TRUE,
						GA_Disabled,TRUE,
					ButtonEnd,
				LayoutEnd,
				CHILD_WeightedHeight,0,
			EndGroup,
		EndWindow;

	fwin->win = (struct Window *)RA_OpenWindow(fwin->objects[OID_MAIN]);
	fwin->gwin = gwin;
	fwin->node = AddObject(window_list,AMINS_FINDWINDOW);
	fwin->node->objstruct = fwin;
	gwin->shared->searchwin = fwin;
}

void ami_search_close(void)
{
	if (fwin->gwin->shared->bw->search_context != NULL)
		search_destroy_context(fwin->gwin->shared->bw->search_context);
	ami_search_set_forward_state(true, NULL);
	ami_search_set_back_state(true, NULL);
	fwin->gwin->shared->searchwin = NULL;
	DisposeObject(fwin->objects[OID_MAIN]);
	DelObject(fwin->node);
	fwin=NULL;
}

BOOL ami_search_event(void)
{
	/* return TRUE if window destroyed */
	ULONG class,result,relevent = 0;
	ULONG column;
	uint16 code;
	search_flags_t flags;

	while((result = RA_HandleInput(fwin->objects[OID_MAIN],&code)) != WMHI_LASTMSG)
	{
       	switch(result & WMHI_CLASSMASK) // class
	{
	case WMHI_GADGETUP:
		switch(result & WMHI_GADGETMASK)
		{
			case GID_NEXT:
				search_insert = true;
				flags = SEARCH_FLAG_FORWARDS |
					ami_search_flags();
				if (search_verify_new(
						search_data.search_window,
						&ami_search_callbacks, NULL))
					search_step(fwin->gwin->shared->bw,
							flags,
							ami_search_string());
			break;

			case GID_PREV:
				search_insert = true;
				flags = ~SEARCH_FLAG_FORWARDS &
					ami_search_flags();
				if (search_verify_new(
						search_data.search_window,
						&ami_search_callbacks, NULL))
					search_step(fwin->gwin->shared->bw,
						flags,
						ami_search_string());
				break;

			case GID_SEARCHSTRING:
				if (fwin->gwin->shared->
					bw->search_context 
					!= NULL)
					search_destroy_context(
						fwin->gwin->
						shared->bw->
						search_context);
				ami_search_set_forward_state(
					true, NULL);
				ami_search_set_back_state(
					true, NULL);
						
				RefreshSetGadgetAttrs(fwin->gadgets[GID_PREV],fwin->win,NULL,
					GA_Disabled,FALSE,
					TAG_DONE);

				RefreshSetGadgetAttrs(fwin->gadgets[GID_NEXT],fwin->win,NULL,
					GA_Disabled,FALSE,
					TAG_DONE);
			break;
		}
		break;

	case WMHI_CLOSEWINDOW:
		ami_search_close();
		return TRUE;
		break;
	}
	}
	return FALSE;
}

/**
* Change the displayed search status.
* \param found  search pattern matched in text
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void ami_search_set_status(bool found, void *p)
{
}

/**
* display hourglass while searching
* \param active start/stop indicator
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void ami_search_set_hourglass(bool active, void *p)
{
	SetWindowPointer(fwin->win,
					WA_BusyPointer,active,
					WA_PointerDelay,active,
					TAG_DONE);
}

/**
* retrieve string being searched for from gui
*/

char *ami_search_string(void)
{
	char *text;
	GetAttr(STRINGA_TextVal,fwin->gadgets[GID_SEARCHSTRING],(ULONG *)&text);
	return text;

}

/**
* add search string to recent searches list
* front is at liberty how to implement the bare notification
* should normally store a strdup() of the string;
* core gives no guarantee of the integrity of the const char *
* \param string search pattern
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void ami_search_add_recent(const char *string, void *p)
{
}

/**
* activate search forwards button in gui
* \param active activate/inactivate
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void ami_search_set_forward_state(bool active, void *p)
{
	RefreshSetGadgetAttrs(fwin->gadgets[GID_NEXT],fwin->win,NULL,
			GA_Disabled, active ? FALSE : TRUE, TAG_DONE);

}

/**
* activate search forwards button in gui
* \param active activate/inactivate
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void ami_search_set_back_state(bool active, void *p)
{
	RefreshSetGadgetAttrs(fwin->gadgets[GID_PREV],fwin->win,NULL,
			GA_Disabled, active ? FALSE : TRUE, TAG_DONE);
}

/**
* retrieve state of 'case sensitive', 'show all' checks in gui
*/

search_flags_t ami_search_flags(void)
{
	ULONG case_sensitive, showall;
	search_flags_t flags;
	GetAttr(GA_Selected,fwin->gadgets[GID_CASE],(ULONG *)&case_sensitive);
	GetAttr(GA_Selected,fwin->gadgets[GID_SHOWALL],(ULONG *)&showall);
	flags = 0 | (case_sensitive ? SEARCH_FLAG_CASE_SENSITIVE : 0) |
			(showall ? SEARCH_FLAG_SHOWALL : 0);
	return flags;
}

