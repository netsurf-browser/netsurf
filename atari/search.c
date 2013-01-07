/*
 * Copyright 2013 Ole Loots <ole@monochrom.net>
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
 *
 * Module Description:
 *
 *
 *
 */


#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "desktop/gui.h"
#include "desktop/browser.h"
#include "desktop/browser_private.h"
#include "desktop/search.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "atari/gui.h"
#include "atari/misc.h"
#include "atari/search.h"
#include "atari/gemtk/gemtk.h"
#include "atari/res/netsurf.rsh"

extern struct gui_window * input_window;
extern void * h_gem_rsrc;
extern GRECT desk_area;


static SEARCH_FORM_SESSION current;
static OBJECT *dlgtree;
static GUIWIN *searchwin;
static short h_aes_win;

static void nsatari_search_set_status(bool found, void *p);
static void nsatari_search_set_hourglass(bool active, void *p);
static void nsatari_search_add_recent(const char *string, void *p);
void nsatari_search_set_forward_state(bool active, void *p);
void nsatari_search_set_back_state(bool active, void *p);

static struct gui_search_callbacks nsatari_search_callbacks = {
	nsatari_search_set_forward_state,
	nsatari_search_set_back_state,
	nsatari_search_set_status,
	nsatari_search_set_hourglass,
	nsatari_search_add_recent
};


/**
* Change the displayed search status.
* \param found  search pattern matched in text
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void nsatari_search_set_status(bool found, void *p)
{
	LOG(("%p set status: %d\n", p, found));
}

/**
* display hourglass while searching
* \param active start/stop indicator
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void nsatari_search_set_hourglass(bool active, void *p)
{
	SEARCH_FORM_SESSION s = (SEARCH_FORM_SESSION)p;
	LOG((""));
	if (active && current != NULL)
		gui_window_set_pointer(s->bw->window, GUI_POINTER_PROGRESS);
	else
		gui_window_set_pointer(s->bw->window, GUI_POINTER_DEFAULT);
}

/**
* add search string to recent searches list
* front is at liberty how to implement the bare notification
* should normally store a strdup() of the string;
* core gives no guarantee of the integrity of the const char *
* \param string search pattern
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void nsatari_search_add_recent(const char *string, void *p)
{
	LOG(("%p add recent: %s\n", p, string));
}

/**
* activate search forwards button in gui
* \param active activate/inactivate
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void nsatari_search_set_forward_state(bool active, void *p)
{
	SEARCH_FORM_SESSION s = (SEARCH_FORM_SESSION)p;
	/* deactivate back cb */
	LOG(("%p: set forward state: %d\n", p, active));
}

/**
* activate search back button in gui
* \param active activate/inactivate
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void nsatari_search_set_back_state(bool active, void *p)
{
	SEARCH_FORM_SESSION s = (SEARCH_FORM_SESSION)p;
	/* deactivate back cb */
	LOG(("%p: set back state: %d\n", p, active));
}


static SEARCH_FORM_SESSION get_search_session(GUIWIN * win)
{
	return (current);
}



static void set_text( short idx, char * text, int len )
{
	char spare[255];

	if( len > 254 )
		len = 254;
	if( text != NULL ){
		strncpy(spare, text, 254);
	} else {
		strcpy(spare, "");
	}

	set_string(dlgtree, idx, spare);
}

static void destroy_search_session(SEARCH_FORM_SESSION s)
{
	if(s != NULL ){
		LOG((""));
		free(s);
	}
}

static int apply_form(GUIWIN *win, struct s_search_form_state * s)
{
	OBJECT * obj = dlgtree;
	char * cstr;

	if( obj == NULL ){
		goto error;
	}

	s->flags = 0;
	if( (obj[SEARCH_CB_FWD].ob_state & OS_SELECTED) != 0 )
		s->flags = SEARCH_FLAG_FORWARDS;
	if( (obj[SEARCH_CB_CASESENSE].ob_state & OS_SELECTED) != 0 )
		s->flags |= SEARCH_FLAG_CASE_SENSITIVE;
	if( (obj[SEARCH_CB_SHOWALL].ob_state & OS_SELECTED) != 0 )
		s->flags |= SEARCH_FLAG_SHOWALL;

	cstr = get_text(dlgtree, SEARCH_TB_SRCH);
	snprintf(s->text, 31, "%s", cstr);
	return ( 0 );

error:
	s->flags = SEARCH_FLAG_FORWARDS;
	strncpy((char*)&s->text[0], "", 31 );
	return( 1 );
}

/* checks for search parameters changes */
static bool form_changed(GUIWIN * w)
{
	bool check;
	struct s_search_form_state cur;
	SEARCH_FORM_SESSION s = get_search_session(w);
	if( s == NULL )
		return false;
	OBJECT * obj = dlgtree;
	assert(s != NULL && obj != NULL);
	uint32_t flags_old = s->state.flags;
	apply_form(w, &cur);

	/* adjust the forward flag, it should not init an new search */
	flags_old |= SEARCH_FLAG_FORWARDS;
	cur.flags |= SEARCH_FLAG_FORWARDS;
	if( cur.flags != flags_old ){
		return( true );
	}

	char * cstr;
	cstr = get_text(obj, SEARCH_TB_SRCH);
	if (cstr != NULL){
		if (strcmp(cstr, (char*)&s->state.text) != 0) {
			return (true);
		}
	}

	return( false );
}


static void __CDECL evnt_bt_srch_click(GUIWIN * win, int index, int unused, void *unused2)
{

	bool fwd;
	SEARCH_FORM_SESSION s = get_search_session(searchwin);
	OBJECT * obj = dlgtree;
	search_flags_t flags = 0;


	if( form_changed(searchwin) ){
		browser_window_search_destroy_context(s->bw);
		apply_form(searchwin, &s->state);
	} else {
		/* get search direction manually: */
		if( (obj[SEARCH_CB_FWD].ob_state & OS_SELECTED) != 0 )
			s->state.flags |= SEARCH_FLAG_FORWARDS;
		 else
			s->state.flags &= (~SEARCH_FLAG_FORWARDS);
	}
	if( browser_window_search_verify_new(s->bw, &nsatari_search_callbacks, s) ){
		browser_window_search_step(s->bw, s->state.flags,  get_text(obj, SEARCH_TB_SRCH));
	}

}

static void __CDECL evnt_cb_click(GUIWIN *win, int index, int unused, void *unused2)
{

	short newstate;

}

static void __CDECL evnt_close(GUIWIN *win, short buff[8])
{

}

void search_destroy(struct gui_window *gw)
{
	/* Free Search Contexts */
	/* todo: destroy search context, if any? */
	LOG((""));

	if (current != NULL){
		destroy_search_session(current);
		current = NULL;
	}

	guiwin_remove(searchwin);
	searchwin = NULL;

	wind_close(h_aes_win);
	wind_delete(h_aes_win);
	h_aes_win = -1;

	LOG(("done"));
}

SEARCH_FORM_SESSION open_browser_search(struct gui_window * gw)
{
	char * title;
	SEARCH_FORM_SESSION sfs;
	GRECT pos, treesize;
	uint32_t kind = CLOSER | NAME | MOVER;

	if (dlgtree == NULL) {
		dlgtree = get_tree(SEARCH);
		if (dlgtree == NULL ) {
			return( NULL );
		}
	}

	if(searchwin){
		search_destroy(gw);
	}


	sfs = calloc(1, sizeof(struct s_search_form_session));
	if( sfs == NULL )
		return( NULL );

	title = (char*)messages_get("FindTextNS");
	if (title == NULL)
		title = (char*)"Find text ...";

	/* setup dipslay position: right corner */
	treesize.g_x = 0;
	treesize.g_y = 0;
	treesize.g_w = dlgtree->ob_width;
	treesize.g_h = dlgtree->ob_height;
	wind_calc_grect(WC_BORDER, kind, &treesize, &pos);
	pos.g_x =  desk_area.g_w - pos.g_w;
	pos.g_y = desk_area.g_h - pos.g_h;

	/* create the dialog: */
	h_aes_win = wind_create_grect(kind, &pos);
	wind_set_str(h_aes_win, WF_NAME, title);


	current = sfs;
	sfs->bw = gw->browser->bw;
/*
	sfs->formwind = mt_FormCreate( &app, tree, WAT_FORM,
								NULL, title,
								&pos, true, false);
*/
/*
	ObjcAttachFormFunc(sfs->formwind, SEARCH_BT_SEARCH, evnt_bt_srch_click,
						NULL);
	ObjcAttachFormFunc(sfs->formwind, SEARCH_CB_CASESENSE, evnt_cb_click, NULL);
	ObjcAttachFormFunc(sfs->formwind, SEARCH_CB_SHOWALL, evnt_cb_click, NULL);
	ObjcAttachFormFunc(sfs->formwind, SEARCH_CB_FWD, evnt_cb_click, NULL);
	EvntAdd(sfs->formwind, WM_CLOSED, evnt_close, EV_TOP);
*/
	apply_form(searchwin, &sfs->state );
	set_text(SEARCH_TB_SRCH, "", 31);

	return( current );

}
