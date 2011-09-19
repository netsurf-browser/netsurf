#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <windom.h>

#include "desktop/gui.h"
#include "desktop/browser.h"
#include "desktop/search.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "atari/gui.h"
#include "atari/misc.h"
#include "atari/browser.h"
#include "atari/search.h"
#include "atari/res/netsurf.rsh"

extern struct gui_window * input_window;
extern void * h_gem_rsrc;


static SEARCH_FORM_SESSION current;

static void nsatari_search_set_status(bool found, void *p);
static void nsatari_search_set_hourglass(bool active, void *p);
static void nsatari_search_add_recent(const char *string, void *p);
void nsatari_search_set_forward_state(bool active, void *p);
void nsatari_search_set_back_state(bool active, void *p);

static struct search_callbacks nsatari_search_callbacks = {
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
	if( active && current != NULL )
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


static SEARCH_FORM_SESSION get_search_session(WINDOW * win)
{
	return (current);
}

static void destroy_search_session( SEARCH_FORM_SESSION s )
{
	if( s != NULL ){
		LOG((""));
		free( s );
	}
}

static int apply_form( WINDOW * win, struct s_search_form_state * s )
{
	OBJECT * obj = ObjcTree(OC_FORM, win );
	if( obj == NULL ){
		goto error;
	}
	s->flags = 0;
	if( (obj[SEARCH_CB_FWD].ob_state & SELECTED) != 0 )
		s->flags = SEARCH_FLAG_FORWARDS;
	if( (obj[SEARCH_CB_CASESENSE].ob_state & SELECTED) != 0 )
		s->flags |= SEARCH_FLAG_CASE_SENSITIVE;
	if( (obj[SEARCH_CB_SHOWALL].ob_state & SELECTED) != 0 )
		s->flags |= SEARCH_FLAG_SHOWALL;

	char * cstr = ObjcString( obj, SEARCH_TB_SRCH, NULL );
	if( cstr != NULL ) {
		strncpy((char*)&s->text[0], cstr, 31 );
	}
	return ( 0 );

error:
	s->flags = SEARCH_FLAG_FORWARDS;
	/* s->forward = true; */
	strncpy((char*)&s->text[0], "", 31 );
	return( 1 );
}

/* checks if search parameters changes */
static bool form_changed( WINDOW * w )
{
	bool check;
	struct s_search_form_state cur;
	SEARCH_FORM_SESSION s = get_search_session(w);
	if( s == NULL )
		return false;
	OBJECT * obj = ObjcTree(OC_FORM,  w);
	assert( s != NULL && obj != NULL );
	uint32_t flags_old = s->state.flags;
	apply_form(w, &cur);

	/* adjust the forward flag, it should not init an new search */
	flags_old |= SEARCH_FLAG_FORWARDS;
	cur.flags |= SEARCH_FLAG_FORWARDS;
	if( cur.flags != flags_old ){
		return( true );
	}

	char * cstr = ObjcString( obj, SEARCH_TB_SRCH, NULL );
	if( cstr != NULL ){
		if( strcmp(cstr, (char*)&s->state.text) != 0 ) {
			return ( true );
		}
	}

	return( false );
}


static void __CDECL evnt_bt_srch_click( WINDOW *win, int index, int unused, void *unused2)
{

	bool fwd;
	SEARCH_FORM_SESSION s = get_search_session(win);
	OBJECT * obj = ObjcTree(OC_FORM, s->formwind );
	search_flags_t flags = 0;

	ObjcChange(OC_FORM, win, index, ~SELECTED , TRUE);
	if( form_changed(win) ){
		browser_window_search_destroy_context(s->bw);
		apply_form( win, &s->state );
	} else {
		/* get search direction manually: */
		if( (obj[SEARCH_CB_FWD].ob_state & SELECTED) != 0 )
			s->state.flags |= SEARCH_FLAG_FORWARDS;
		 else
			s->state.flags &= (~SEARCH_FLAG_FORWARDS);
	}
	if( browser_window_search_verify_new(s->bw, &nsatari_search_callbacks, s) ){
		browser_window_search_step(s->bw, s->state.flags,  ObjcString( obj, SEARCH_TB_SRCH, NULL ) );
	}

}

static void __CDECL evnt_cb_click( WINDOW *win, int index, int unused, void *unused2)
{

	short newstate;
	OBJECT * obj = ObjcTree(OC_FORM, get_search_session(win)->formwind );
}

static void __CDECL evnt_close( WINDOW *win, short buff[8])
{
	/* Free Search Contexts */
	/* todo: destroy search context, if any? */
	SEARCH_FORM_SESSION s = get_search_session(win);
	if( s != NULL ){
		destroy_search_session( s );
	}
	current = NULL;
	ApplWrite( _AESapid, WM_DESTROY, win->handle, 0,0,0,0 );
}

void search_destroy( struct gui_window * gw )
{
	LOG(("search_destroy %p / %p", gw, current ));
	if( current != NULL && current->formwind != NULL ){
		ApplWrite( _AESapid, WM_CLOSED, current->formwind->handle, 0,0,0,0);
		/* Handle Close event */
		EvntWindom( MU_MESAG );
		/* Handle Destroy Event */
		EvntWindom( MU_MESAG );
	}
	LOG(("done"));
}

SEARCH_FORM_SESSION open_browser_search( struct gui_window * gw )
{
	char * title;
	SEARCH_FORM_SESSION sfs;
	OBJECT * tree = get_tree(SEARCH);
	if( tree == NULL ){
		return( NULL );
	}
	sfs = calloc(1, sizeof(struct s_search_form_session));
	if( sfs == NULL )
		return( NULL );
	title = (char*)messages_get("FindTextNS");
	if( title == NULL )
		title = (char*)"Find text ...";

	search_destroy( gw );
	current = sfs;
	sfs->bw = gw->browser->bw;
	sfs->formwind = mt_FormCreate( &app, tree, WAT_FORM,
								NULL, title,
								NULL, true, false);

	ObjcAttachFormFunc( sfs->formwind, SEARCH_BT_SEARCH, evnt_bt_srch_click, NULL);
	ObjcAttachFormFunc( sfs->formwind, SEARCH_CB_CASESENSE, evnt_cb_click, NULL);
	ObjcAttachFormFunc( sfs->formwind, SEARCH_CB_SHOWALL, evnt_cb_click, NULL);
	ObjcAttachFormFunc( sfs->formwind, SEARCH_CB_FWD, evnt_cb_click, NULL);
	EvntAdd( sfs->formwind, WM_CLOSED, evnt_close, EV_TOP);
	apply_form(sfs->formwind, &sfs->state );
	strncpy( ObjcString( tree, SEARCH_TB_SRCH, NULL ), "", SEARCH_MAX_SLEN);

	return( current );

}
