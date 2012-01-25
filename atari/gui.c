/*
 * Copyright 2010 <ole@monochrom.net>
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

 /*
 	This File provides all the mandatory functions prefixed with gui_
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <windom.h>
#include <hubbub/hubbub.h>

#include "content/urldb.h"
#include "content/fetch.h"
#include "content/fetchers/resource.h"
#include "css/utils.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/plotters.h"
#include "desktop/netsurf.h"
#include "desktop/401login.h"

#include "desktop/options.h"
#include "desktop/save_complete.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "desktop/browser.h"
#include "desktop/mouse.h"
#include "render/html.h"
#include "render/font.h"
#include "utils/schedule.h"
#include "utils/url.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include "atari/gui.h"
#include "atari/options.h"
#include "atari/misc.h"
#include "atari/findfile.h"
#include "atari/schedule.h"
#include "atari/browser_win.h"
#include "atari/browser.h"
#include "atari/statusbar.h"
#include "atari/toolbar.h"
#include "atari/verify_ssl.h"
#include "atari/hotlist.h"
#include "atari/login.h"
#include "atari/global_evnt.h"
#include "atari/encoding.h"
#include "atari/res/netsurf.rsh"
#include "atari/plot.h"
#include "atari/clipboard.h"
#include "atari/osspec.h"
#include "atari/search.h"
#include "cflib.h"

#define TODO() (0)/*printf("%s Unimplemented!\n", __FUNCTION__)*/

char *tmp_clipboard;
struct gui_window *input_window = NULL;
struct gui_window *window_list = NULL;
void * h_gem_rsrc;
OBJECT * h_gem_menu;
OBJECT **rsc_trindex;
short vdih;
short rsc_ntree;
long next_poll;
bool rendering = false;


/* Comandline / Options: */
int cfg_width;
int cfg_height;

/* Defaults to option_homepage_url, commandline options overwrites that value */
const char * cfg_homepage_url;

/* path to choices file: */
char options[PATH_MAX];


void gui_poll(bool active)
{
	short winloc[4];
	// int timeout; /* timeout in milliseconds */
	int flags = MU_MESAG | MU_KEYBD | MU_BUTTON ;
	short mx, my, dummy;
	short aestop;

	evnt.timer = schedule_run();

	if( active || rendering ) {
		if( clock() >= next_poll ) {
			evnt.timer = 0;
			flags |= MU_TIMER;
			EvntWindom( flags );
			next_poll = clock() + (CLOCKS_PER_SEC>>2);
		}
	} else {
		if( input_window != NULL ){
			wind_get( 0, WF_TOP, &aestop, &winloc[1], &winloc[2], &winloc[3]);
			if( winloc[1] == _AESapid ){
				/* only check for mouse move when netsurf is on top: */
				// move that into m1 event handler
				graf_mkstate( &mx, &my, &dummy, &dummy );
				flags |= MU_M1;
				evnt.m1_flag = MO_LEAVE;
				evnt.m1_w = evnt.m1_h = 1;
				evnt.m1_x = mx;
				evnt.m1_y = my;
			}
		}
		flags |= MU_TIMER;
		EvntWindom( flags );
	}

	struct gui_window * g;
	for( g = window_list; g != NULL; g=g->next ) {
		if( browser_redraw_required( g ) ){
			browser_redraw( g );
		}
		if( g->root->toolbar ){
			if(g->root->toolbar->url.redraw ){
				tb_url_redraw( g );
			}
		}
	}
	if( evnt.timer != 0 && !active ){
		/* this suits for stuff with lower priority */
		/* TBD: really be spare on redraws??? */
		atari_treeview_redraw( hl.tv );
	}
}

struct gui_window *
gui_create_browser_window(struct browser_window *bw,
			  struct browser_window *clone,
			  bool new_tab)
{
	struct gui_window *gw=NULL;
	LOG(( "gw: %p, BW: %p, clone %p, tab: %d\n" , gw,  bw, clone,
		(int)new_tab
	));

	gw = malloc( sizeof(struct gui_window) );
	if (gw == NULL)
		return NULL;
	memset( gw, 0, sizeof(struct gui_window) );

	LOG(("new window: %p, bw: %p\n", gw, bw));
	window_create(gw, bw, WIDGET_STATUSBAR|WIDGET_TOOLBAR|WIDGET_RESIZE|WIDGET_SCROLL );
	if( gw->root->handle ) {
		GRECT pos = {
			app.w/2-(cfg_width/2), (app.h/2)-(cfg_height/2)+16,
			cfg_width, cfg_height
		};
		window_open( gw , pos );
		/* Recalculate windows browser area now */
		browser_update_rects( gw );
		tb_update_buttons( gw );
		input_window = gw;
		/* TODO:... this line: placeholder to create a local history widget ... */
	}

	/* add the window to the window list: */
	if( window_list == NULL ) {
		window_list = gw;
		gw->next = NULL;
		gw->prev = NULL;
	} else {
		struct gui_window * tmp = window_list;
		while( tmp->next != NULL ) {
			tmp = tmp->next;
		}
		tmp->next = gw;
		gw->prev = tmp;
		gw->next = NULL;
	}

	return( gw );

}

void gui_window_destroy(struct gui_window *w)
{
	if (w == NULL)
		return;

	LOG(("%s\n", __FUNCTION__ ));

	input_window = NULL;

	window_destroy( w );

	/* unlink the window: */
	if(w->prev != NULL ) {
		w->prev->next = w->next;
	} else {
		window_list = w->next;
	}
	if( w->next != NULL ) {
		w->next->prev = w->prev;
	}
	free(w);
	w = NULL;

	w = window_list;
	while( w != NULL ) {
		if( w->root ) {
			input_window = w;
			break;
		}
		w = w->next;
	}
}

void gui_window_get_dimensions(struct gui_window *w, int *width, int *height,
			       bool scaled)
{
	if (w == NULL)
		return;
	LGRECT rect;
	browser_get_rect( w, BR_CONTENT, &rect  );
	*width = rect.g_w;
	*height = rect.g_h;
}

void gui_window_set_title(struct gui_window *gw, const char *title)
{
	int l;
	char * conv;

	if (gw == NULL)
		return;
	if( gw->root ){
		l = strlen(title);
		if( utf8_to_local_encoding(title, l, &conv) == UTF8_CONVERT_OK ){
			strncpy(gw->root->title, conv, atari_sysinfo.aes_max_win_title_len);
                	free( conv );
		} else {
			strncpy(gw->root->title, title, atari_sysinfo.aes_max_win_title_len);
		}
		gw->root->title[atari_sysinfo.aes_max_win_title_len] = 0;
		WindSetStr( gw->root->handle, WF_NAME, gw->root->title );
	}
}

/**
 * set the status bar message
 */
void gui_window_set_status(struct gui_window *w, const char *text)
{
	static char * msg_loading = NULL;
	static char * msg_fetch = NULL;

	if( msg_loading == NULL ){
		msg_loading = messages_get("Loading");
		msg_fetch = messages_get("Fetch");
	}

	if( (strncmp(msg_loading, text, 4) == 0)
		||
		(strncmp(msg_fetch, text, 4)) == 0 ) {
			rendering = true;
	} else {
		rendering = false;
	}

	if (w == NULL || text == NULL )
		return;
	window_set_stauts( w , (char*)text );
}

void gui_window_redraw_window(struct gui_window *gw)
{
	CMP_BROWSER b;
	LGRECT rect;
	if (gw == NULL)
		return;
	b = gw->browser;
	browser_get_rect( gw, BR_CONTENT, &rect );
	browser_schedule_redraw( gw, 0, 0, rect.g_w, rect.g_h );
}

void gui_window_update_box(struct gui_window *gw, const struct rect *rect)
{
	CMP_BROWSER b;
	if (gw == NULL)
		return;
	b = gw->browser;
	/* the box values are actually floats */
	int x0 = rect->x0 - b->scroll.current.x;
	int y0 = rect->y0 - b->scroll.current.y;
	int w,h;
	w = rect->x1 - rect->x0;
	h = rect->y1 - rect->y0;
 	browser_schedule_redraw_rect( gw, x0, y0, w,h);
}

bool gui_window_get_scroll(struct gui_window *w, int *sx, int *sy)
{
	if (w == NULL)
		return false;
	*sx = w->browser->scroll.current.x;
	*sy = w->browser->scroll.current.y;
	return( true );
}

void gui_window_set_scroll(struct gui_window *w, int sx, int sy)
{
	if ((w == NULL) ||
	    (w->browser->bw == NULL) ||
	    (w->browser->bw->current_content == NULL))
		return;
	if( sx != 0 ) {
		if( sx < 0 ) {
			browser_scroll(w, WA_LFLINE, abs(sx), true );
		} else {
			browser_scroll(w, WA_RTLINE, abs(sx), true );
		}
	}

	if( sy != 0 ) {
		if( sy < 0) {
			browser_scroll(w, WA_UPLINE, abs(sy), true );
		} else {
			browser_scroll(w, WA_DNLINE, abs(sy), true );
		}
	}
	return;

}

void gui_window_scroll_visible(struct gui_window *w, int x0, int y0, int x1, int y1)
{
	LOG(("%s:(%p, %d, %d, %d, %d)", __func__, w, x0, y0, x1, y1));
	gui_window_set_scroll(w,x0,y0);
	browser_schedule_redraw_rect( w, 0, 0, x1-x0,y1-y0);
}


/* It seems this method is called when content size got adjusted,
	so that we can adjust scroll info. We also have to call it when tab
	change occurs.
*/
void gui_window_update_extent(struct gui_window *gw)
{
	int oldx, oldy;
	oldx = gw->browser->scroll.current.x;
	oldy = gw->browser->scroll.current.y;
	if( gw->browser->bw->current_content != NULL ) {
		browser_set_content_size( gw,
			content_get_width(gw->browser->bw->current_content),
			content_get_height(gw->browser->bw->current_content)
		);
	}
}


void gui_clear_selection(struct gui_window *g)
{

}



/**
 * set the pointer shape
 */
void gui_window_set_pointer(struct gui_window *w, gui_pointer_shape shape)
{
	if (w == NULL)
		return;
	switch (shape) {
	case GUI_POINTER_POINT: /* link */
		gem_set_cursor(&gem_cursors.hand);
		break;

	case GUI_POINTER_MENU:
		gem_set_cursor(&gem_cursors.menu);
		break;

	case GUI_POINTER_CARET: /* input */
		gem_set_cursor(&gem_cursors.ibeam);
		break;

	case GUI_POINTER_CROSS:
		gem_set_cursor(&gem_cursors.cross);
		break;

	case GUI_POINTER_MOVE:
		gem_set_cursor(&gem_cursors.sizeall);
		break;

	case GUI_POINTER_RIGHT:
	case GUI_POINTER_LEFT:
		gem_set_cursor(&gem_cursors.sizewe);
		break;

	case GUI_POINTER_UP:
	case GUI_POINTER_DOWN:
		gem_set_cursor(&gem_cursors.sizens);
		break;

	case GUI_POINTER_RU:
	case GUI_POINTER_LD:
		gem_set_cursor(&gem_cursors.sizenesw);
		break;

	case GUI_POINTER_RD:
	case GUI_POINTER_LU:
		gem_set_cursor(&gem_cursors.sizenwse);
		break;

	case GUI_POINTER_WAIT:
		gem_set_cursor(&gem_cursors.wait);
		break;

	case GUI_POINTER_PROGRESS:
		gem_set_cursor(&gem_cursors.appstarting);
		break;

	case GUI_POINTER_NO_DROP:
		gem_set_cursor(&gem_cursors.nodrop);
		break;

	case GUI_POINTER_NOT_ALLOWED:
		gem_set_cursor(&gem_cursors.deny);
		break;

	case GUI_POINTER_HELP:
		gem_set_cursor(&gem_cursors.help);
		break;

	default:
		gem_set_cursor(&gem_cursors.arrow);
		break;
	}
}

void gui_window_hide_pointer(struct gui_window *w)
{
	TODO();
}


void gui_window_set_url(struct gui_window *w, const char *url)
{
	if (w == NULL)
		return;
	tb_url_set(w, (char*)url );
}

static void throbber_advance( void * data )
{
	LGRECT work;
	struct gui_window * gw = (struct gui_window *)data;
	if( gw->root == NULL )
		return;
	if( gw->root->toolbar == NULL )
		return;
	if( gw->root->toolbar->throbber.running == false )
		return;
	mt_CompGetLGrect(&app, gw->root->toolbar->throbber.comp,
						WF_WORKXYWH, &work);
	gw->root->toolbar->throbber.index++;
	if( gw->root->toolbar->throbber.index > gw->root->toolbar->throbber.max_index )
		gw->root->toolbar->throbber.index = THROBBER_MIN_INDEX;
	ApplWrite( _AESapid, WM_REDRAW,  gw->root->handle->handle,
		work.g_x, work.g_y, work.g_w, work.g_h );
	schedule(100, throbber_advance, gw );
}

void gui_window_start_throbber(struct gui_window *w)
{
	LGRECT work;
	if (w == NULL)
		return;
	if( w->root->toolbar->throbber.running == true )
		return;
	mt_CompGetLGrect(&app, w->root->toolbar->throbber.comp,
						WF_WORKXYWH, &work);
	w->root->toolbar->throbber.running = true;
	w->root->toolbar->throbber.index = THROBBER_MIN_INDEX;
	schedule(100, throbber_advance, w );
	ApplWrite( _AESapid, WM_REDRAW,  w->root->handle->handle,
		work.g_x, work.g_y, work.g_w, work.g_h );
}

void gui_window_stop_throbber(struct gui_window *w)
{
	LGRECT work;
	if (w == NULL)
		return;
	if( w->root->toolbar->throbber.running == false )
		return;

	schedule_remove(throbber_advance, w);

	mt_CompGetLGrect(&app, w->root->toolbar->throbber.comp,
						WF_WORKXYWH, &work);
	w->root->toolbar->throbber.running = false;
	ApplWrite( _AESapid, WM_REDRAW,  w->root->handle->handle,
		work.g_x, work.g_y, work.g_w, work.g_h );
}

/* Place caret in window */
void gui_window_place_caret(struct gui_window *w, int x, int y, int height)
{
	if (w == NULL)
		return;
	if( w->browser->caret.current.g_w > 0 )
		gui_window_remove_caret( w );
	w->browser->caret.requested.g_x = x;
	w->browser->caret.requested.g_y = y;
	w->browser->caret.requested.g_w = 1;
	w->browser->caret.requested.g_h = height;
	w->browser->caret.redraw = true;
	return;
}


/**
 * clear window caret
 */
void
gui_window_remove_caret(struct gui_window *w)
{
	if (w == NULL)
		return;

	if( w->browser->caret.background.fd_addr != NULL ){
		browser_restore_caret_background( w, NULL );
		w->browser->caret.requested.g_w = 0;
		w->browser->caret.current.g_w = 0;
	}
	return;
}

void
gui_window_set_icon(struct gui_window *g, hlcache_handle *icon)
{
	g->icon = (icon != NULL) ? content_get_bitmap(icon) : NULL;
}

void
gui_window_set_search_ico(hlcache_handle *ico)
{
	TODO();
}

void gui_window_new_content(struct gui_window *w)
{
	w->browser->scroll.current.x = 0;
	w->browser->scroll.current.y = 0;
	w->browser->scroll.requested.x = 0;
	w->browser->scroll.requested.y = 0;
	w->browser->scroll.required = true;
	gui_window_redraw_window( w );
}

bool gui_window_scroll_start(struct gui_window *w)
{
	TODO();
	return true;
}

bool gui_window_drag_start(struct gui_window *g, gui_drag_type type,
		const struct rect *rect)
{
	TODO();
	return true;
}

void gui_window_save_link(struct gui_window *g, const char *url,
		const char *title)
{
	LOG(("%s -> %s", title, url ));
	TODO();
}

void gui_drag_save_object(gui_save_type type, hlcache_handle *c,
			  struct gui_window *w)
{
	LOG((""));
	TODO();
}

void gui_drag_save_selection(struct selection *s, struct gui_window *w)
{
	LOG((""));
	TODO();
}

void gui_start_selection(struct gui_window *w)
{
	gui_empty_clipboard();
}

void gui_paste_from_clipboard(struct gui_window *w, int x, int y)
{
	char * clip = scrap_txt_read( &app );
	if( clip == NULL )
		return;
	int clip_length = strlen( clip );
	if (clip_length > 0) {
		char *utf8;
		utf8_convert_ret ret;
		/* Clipboard is in local encoding so
		 * convert to UTF8 */
		ret = utf8_from_local_encoding(clip,
				clip_length, &utf8);
		if (ret == UTF8_CONVERT_OK) {
			browser_window_paste_text(w->browser->bw, utf8,
					strlen(utf8), true);
			free(utf8);
		}
		free( clip );
	}
}

bool gui_empty_clipboard(void)
{
	if( tmp_clipboard != NULL ){
		free( tmp_clipboard );
		tmp_clipboard = NULL;
	}
	return true;
}

bool gui_add_to_clipboard(const char *text_utf8, size_t length_utf8, bool space)
{
	LOG(("(%s): %s (%d)\n", (space)?"space":"", (char*)text_utf8, (int)length_utf8));
	char * oldptr = tmp_clipboard;
	size_t oldlen = 0;
	size_t newlen = 0;
	char * text = NULL;
	char * text2 = NULL;
	bool retval;
	int length = 0;
	if( length_utf8 > 0 && text_utf8 != NULL ) {
		utf8_to_local_encoding(text_utf8,length_utf8,&text);
		if( text == NULL ) {
			LOG(("Conversion failed (%s)", text_utf8));
			goto error;
		} else {
			text2 = text;
		}
	} else {
		if( space == false ) {
			goto success;
		}
		text = malloc(length + 2);
		if( text == NULL ) {
			goto error;
		}
		text2 = text;
		text[length+1] = 0;
		memset(text, ' ', length+1);
	}
	length = strlen(text);
	if( tmp_clipboard != NULL ) {
		oldlen = strlen( tmp_clipboard );
	}
	newlen = oldlen + length + 1;
	if( tmp_clipboard == NULL){
		tmp_clipboard = malloc(newlen);
		if( tmp_clipboard == NULL ) {
			goto error;
		}
		strncpy(tmp_clipboard, text, newlen);
	} else {
		tmp_clipboard = realloc( tmp_clipboard, newlen);
		if( tmp_clipboard == NULL ) {
			goto error;
		}
		strncpy(tmp_clipboard, oldptr, newlen);
		strncat(tmp_clipboard, text, newlen-oldlen);
	}
	goto success;

error:
	retval = false;
	goto fin;

success:
	retval = true;

fin:
	if( text2 != NULL )
		free( text2 );
	return(retval);

}

bool gui_commit_clipboard(void)
{
	int r = scrap_txt_write(&app, tmp_clipboard);
	return( (r>0)?true:false );
}


static bool
gui_selection_traverse_handler(const char *text,
			       size_t length,
			       struct box *box,
			       void *handle,
			       const char *space_text,
			       size_t space_length)
{
	bool add_space = box != NULL ? box->space != 0 : false;

	if (space_text != NULL && space_length > 0) {
		if (!gui_add_to_clipboard(space_text, space_length, false)) {
			return false;
		}
	}

	if (!gui_add_to_clipboard(text, length, add_space))
		return false;

	return true;
}

bool gui_copy_to_clipboard(struct selection *s)
{
	bool ret = false;
	if( s->defined ) {
		gui_empty_clipboard();
		if(selection_traverse(s, gui_selection_traverse_handler, NULL)){
			ret = gui_commit_clipboard();
		}
	}
	gui_empty_clipboard();
	return ret;
}


void gui_create_form_select_menu(struct browser_window *bw,
				 struct form_control *control)
{
	TODO();
}

/**
 * Broadcast an URL that we can't handle.
 */
void gui_launch_url(const char *url)
{
	TODO();
	LOG(("launch file: %s\n", url));
}

void gui_401login_open(const char *url,	const char *realm,
		nserror (*cb)(bool proceed, void *pw), void *cbpw)
{
	bool bres;
	char * out = NULL;
	bres = login_form_do( (char*)url, (char*)realm, &out  );
	if( bres ) {
		LOG(("url: %s, realm: %s, auth: %s\n", url, realm, out ));
		urldb_set_auth_details(url, realm, out );
	}
	if( out != NULL ){
		free( out );
	}
	if( cb != NULL )
		cb(bres, cbpw);
}

void gui_cert_verify(const char *url, const struct ssl_cert_info *certs,
		unsigned long num,
		nserror (*cb)(bool proceed, void *pw), void *cbpw)
{
	LOG((""));

	bool bres;
	/*bres = verify_ssl_form_do(url, certs, num);
	if( bres )
		urldb_set_cert_permissions(url, true);
	*/
	// TODO: localize string
	int b = form_alert(1, "[2][SSL Verify failed, continue?][Continue|Abort]");
	bres = (b==1)? true : false;
	LOG(("Trust: %d", bres ));
	urldb_set_cert_permissions(url, bres);
	cb(bres, cbpw);
}

void gui_quit(void)
{
	LOG((""));

	struct gui_window * gw = window_list;
	struct gui_window * tmp = window_list;

	while( gw ) {
		tmp = gw->next;
		browser_window_destroy(gw->browser->bw);
		gw = tmp;
	}

	hotlist_destroy();

	urldb_save_cookies(option_cookie_file);
	urldb_save(option_url_file);

	RsrcXtype( 0, rsc_trindex, rsc_ntree);
	unbind_global_events();
	MenuBar( h_gem_menu , 0 );
	if( h_gem_rsrc != NULL ) {
		RsrcXfree(h_gem_rsrc );
	}
	LOG(("Shutting down plotter"));
	atari_plotter_finalise();
	if( tmp_clipboard != NULL ){
		free( tmp_clipboard );
		tmp_clipboard = NULL;
	}
	LOG(("done"));
}




static bool
process_cmdline(int argc, char** argv)
{
	int opt;

	LOG(("argc %d, argv %p", argc, argv));

	if ((option_window_width != 0) && (option_window_height != 0)) {
		cfg_width = option_window_width;
		cfg_height = option_window_height;
	} else {
		if( sys_type() == SYS_TOS ){
			/* on single tasking OS, start as fulled window: */
			cfg_width = app.w;
			cfg_height = app.h;
		} else {
			cfg_width = 600;
			cfg_height = 360;
		}
	}

	if (option_homepage_url != NULL && option_homepage_url[0] != '\0')
		cfg_homepage_url = option_homepage_url;
	else
		cfg_homepage_url = NETSURF_HOMEPAGE;

	while((opt = getopt(argc, argv, "w:h:")) != -1) {
		switch (opt) {
		case 'w':
			cfg_width = atoi(optarg);
			break;

		case 'h':
			cfg_height = atoi(optarg);
			break;

		default:
			fprintf(stderr,
				"Usage: %s [w,h,v] url\n",
				argv[0]);
			return false;
		}
	}

	if (optind < argc) {
		cfg_homepage_url = argv[optind];
	}
	return true;
}

static inline void create_cursor(int flags, short mode, void * form, MFORM_EX * m)
{
	m->flags = flags;
	m->number = mode;
	if( flags & MFORM_EX_FLAG_USERFORM ) {
		m->number = mode;
		m->tree = (OBJECT*)form;
	}
}

nsurl *gui_get_resource_url(const char *path)
{
	char buf[PATH_MAX];
	char *raw;
	nsurl *url = NULL;

	atari_find_resource((char*)&buf, path, path);
	raw = path_to_url((char*)&buf);
	if (raw != NULL) {
		nsurl_create(raw, &url);
		free(raw);
	}

	return url;
}

static void gui_init(int argc, char** argv)
{
	char buf[PATH_MAX];
	OBJECT * cursors;

	atari_find_resource(buf, "netsurf.rsc", "./res/netsurf.rsc");
	LOG(("%s ", (char*)&buf));
	h_gem_rsrc = RsrcXload( (char*) &buf );

	if( !h_gem_rsrc )
		die("Uable to open GEM Resource file!");
	rsc_trindex = RsrcGhdr(h_gem_rsrc)->trindex;
	rsc_ntree   = RsrcGhdr(h_gem_rsrc)->ntree;

	RsrcGaddr( h_gem_rsrc, R_TREE, MAINMENU , &h_gem_menu );
	RsrcXtype( RSRC_XTYPE, rsc_trindex, rsc_ntree);

	create_cursor(0, POINT_HAND, NULL, &gem_cursors.hand );
	create_cursor(0, TEXT_CRSR,  NULL, &gem_cursors.ibeam );
	create_cursor(0, THIN_CROSS, NULL, &gem_cursors.cross);
 	create_cursor(0, BUSY_BEE, NULL, &gem_cursors.wait);
	create_cursor(0, ARROW, NULL, &gem_cursors.arrow);
	create_cursor(0, OUTLN_CROSS, NULL, &gem_cursors.sizeall);
	create_cursor(0, OUTLN_CROSS, NULL, &gem_cursors.sizenesw);
	create_cursor(0, OUTLN_CROSS, NULL, &gem_cursors.sizenwse);
	RsrcGaddr( h_gem_rsrc, R_TREE, CURSOR , &cursors );
	create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_APPSTART,
		cursors, &gem_cursors.appstarting);
	gem_set_cursor( &gem_cursors.appstarting );
	create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_SIZEWE,
		cursors, &gem_cursors.sizewe);
	create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_SIZENS,
		cursors, &gem_cursors.sizens);
	create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_NODROP,
		cursors, &gem_cursors.nodrop);
	create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_DENY,
		cursors, &gem_cursors.deny);
	create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_MENU,
		cursors, &gem_cursors.menu);
	create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_HELP,
		cursors, &gem_cursors.help);

	LOG(("Enabling core select menu"));
	option_core_select_menu = true;

	if( strlen(option_url_file) ){
		urldb_load(option_url_file);
	}
	if( strlen(option_cookie_file) ){
		urldb_load_cookies(option_cookie_file);
		LOG(("Loading cookies from: %s", option_cookie_file ));
	}

	if (process_cmdline(argc,argv) != true)
		die("unable to process command line.\n");

	nkc_init();
	atari_plotter_init( option_atari_screen_driver, option_atari_font_driver );
}

static char *theapp = (char*)"NetSurf";
static void gui_init2(int argc, char** argv)
{
	MenuBar( h_gem_menu , 1 );
	bind_global_events();
	menu_register( -1, theapp);
	if (sys_type() & (SYS_MAGIC|SYS_NAES|SYS_XAAES)) {
		menu_register( _AESapid, (char*)"  NetSurf ");
	}
 	tree_set_icon_dir( option_tree_icons_path );
	hotlist_init();
}

/* #define WITH_DBG_LOGFILE 1 */
/** Entry point from OS.
 *
 * /param argc The number of arguments in the string vector.
 * /param argv The argument string vector.
 * /return The return code to the OS
 */
int main(int argc, char** argv)
{
	char messages[PATH_MAX];

	setbuf(stderr, NULL);
	setbuf(stdout, NULL);
#ifdef WITH_DBG_LOGFILE
	freopen("stdout.log", "a+", stdout);
	freopen("stderr.log", "a+", stderr);
#endif
	ApplInit();
	graf_mouse(BUSY_BEE, NULL);
	init_os_info();
	atari_find_resource((char*)&messages, "messages", "res/messages");
	atari_find_resource((char*)&options, "Choices", "Choices");
	netsurf_init(&argc, &argv, options, messages);
	gui_init(argc, argv);
	gui_init2(argc, argv);
	browser_window_create(cfg_homepage_url, 0, 0, true, false);
	graf_mouse( ARROW , NULL);
	netsurf_main_loop();
	netsurf_exit();
	LOG(("ApplExit"));
	ApplExit();
#ifdef WITH_DBG_LOGFILE
	fclose(stdout);
	fclose(stderr);
#endif

	return 0;
}


