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
#include "desktop/browser_private.h"
#include "desktop/mouse.h"
#include "render/font.h"
#include "utils/schedule.h"
#include "utils/url.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include "atari/gemtk/gemtk.h"
#include "atari/gui.h"
#include "atari/misc.h"
#include "atari/findfile.h"
#include "atari/schedule.h"
#include "atari/rootwin.h"
#include "atari/browser.h"
#include "atari/statusbar.h"
#include "atari/toolbar.h"
#include "atari/verify_ssl.h"
#include "atari/hotlist.h"
#include "atari/history.h"
#include "atari/login.h"
#include "atari/encoding.h"
#include "atari/res/netsurf.rsh"
#include "atari/plot/plot.h"
#include "atari/clipboard.h"
#include "atari/osspec.h"
#include "atari/search.h"
#include "atari/deskmenu.h"
#include "cflib.h"

#define TODO() (0)/*printf("%s Unimplemented!\n", __FUNCTION__)*/

char *tmp_clipboard;
struct gui_window *input_window = NULL;
struct gui_window *window_list = NULL;
void * h_gem_rsrc;
long next_poll;
bool rendering = false;
bool gui_poll_repeat = false;
GRECT desk_area;


/* Comandline / Options: */
int option_window_width;
int option_window_height;
int option_window_x;
int option_window_y;

/* Defaults to option_homepage_url, commandline options overwrites that value */
const char * option_homepage_url;

/* path to choices file: */
char options[PATH_MAX];

EVMULT_IN aes_event_in = {
    .emi_flags = MU_MESAG | MU_TIMER | MU_KEYBD | MU_BUTTON | MU_M1,
    .emi_bclicks = 258,
    .emi_bmask = 3,
    .emi_bstate = 0,
    .emi_m1leave = MO_ENTER,
    .emi_m1 = {0,0,0,0},
    .emi_m2leave = 0,
    .emi_m2 = {0,0,0,0},
    .emi_tlow = 0,
    .emi_thigh = 0
};
EVMULT_OUT aes_event_out;
short aes_msg_out[8];



void gui_poll(bool active)
{

    short mx, my, dummy;
	unsigned short nkc = 0;

	gui_poll_repeat = false;

    aes_event_in.emi_tlow = schedule_run();

	if(active || rendering)
		aes_event_in.emi_tlow = 0;

	if(aes_event_in.emi_tlow < 0){
		aes_event_in.emi_tlow = 10000;
		printf("long poll!\n");
	}

    struct gui_window * g;

    if( !active ) {
		if(input_window && input_window->root->redraw_slots.areas_used > 0){
			window_process_redraws(input_window->root);
		}
        /* this suits for stuff with lower priority */
        /* TBD: really be spare on redraws??? */
        hotlist_redraw();
        global_history_redraw();
    }

	// Handle events until there are no more messages pending or
	// until the engine indicates activity:
	bool skip = false;
/*
	if (active || rendering){
		if ((clock() < next_poll)){
			skip = true;
		} else {
			next_poll = clock() + (CLOCKS_PER_SEC>>5);
		}
	}
*/
	//if (skip == false) {
		do {
			short mx, my, dummy;

			graf_mkstate(&mx, &my, &dummy, &dummy);
			aes_event_in.emi_m1.g_x = mx;
			aes_event_in.emi_m1.g_y = my;
			evnt_multi_fast(&aes_event_in, aes_msg_out, &aes_event_out);
			if(!guiwin_dispatch_event(&aes_event_in, &aes_event_out, aes_msg_out)) {
				if( (aes_event_out.emo_events & MU_MESAG) != 0 ) {
					LOG(("WM: %d\n", aes_msg_out[0]));
					switch(aes_msg_out[0]) {

						case MN_SELECTED:
							LOG(("Menu Item: %d\n",aes_msg_out[4]));
							deskmenu_dispatch_item(aes_msg_out[3], aes_msg_out[4]);
							break;
						default:
							break;
						}
				}
				if((aes_event_out.emo_events & MU_KEYBD) != 0) {
					uint16_t nkc = gem_to_norm( (short)aes_event_out.emo_kmeta,
										(short)aes_event_out.emo_kreturn);
					deskmenu_dispatch_keypress(aes_event_out.emo_kreturn,
												aes_event_out.emo_kmeta, nkc);
				}
			}
		} while ( gui_poll_repeat && !(active||rendering));
		if(input_window && input_window->root->redraw_slots.areas_used > 0){
			window_process_redraws(input_window->root);
		}
	//} else {
		//printf("skip poll %d (%d)\n", next_poll, clock());
	//}

}


struct gui_window *
gui_create_browser_window(struct browser_window *bw,
                          struct browser_window *clone,
                          bool new_tab) {
    struct gui_window *gw=NULL;
    LOG(( "gw: %p, BW: %p, clone %p, tab: %d\n" , gw,  bw, clone,
          (int)new_tab
        ));

    gw = calloc( sizeof(struct gui_window), 1);
    if (gw == NULL)
        return NULL;

    LOG(("new window: %p, bw: %p\n", gw, bw));
    window_create(gw, bw, WIDGET_STATUSBAR|WIDGET_TOOLBAR|WIDGET_RESIZE\
                  |WIDGET_SCROLL);
    if (gw->root->win) {
        GRECT pos = {
            option_window_x, option_window_y,
            option_window_width, option_window_height
        };
        gui_window_set_url(gw, "");
        gui_window_set_pointer(gw, BROWSER_POINTER_DEFAULT);
        window_set_active_gui_window(gw->root, gw);
        window_open(gw->root, pos );
        /* Recalculate windows browser area now */
        gui_set_input_gui_window(gw);
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

	if (input_window == w) {
		gui_set_input_gui_window(NULL);
    }

    search_destroy(w);
    browser_destroy(w->browser);
    free(w->status);
    free(w->title);
    free(w->url);

    /* unlink the window: */
    if(w->prev != NULL ) {
        w->prev->next = w->next;
    } else {
        window_list = w->next;
    }
    if( w->next != NULL ) {
        w->next->prev = w->prev;
    }

    window_unref_gui_window(w->root, w);

    free(w);
    w = NULL;

    if(input_window == NULL){
        w = window_list;
        while( w != NULL ) {
            if(w->root) {
            	gui_set_input_gui_window(w);
                break;
            }
            w = w->next;
        }
    }
}

void gui_window_get_dimensions(struct gui_window *w, int *width, int *height,
                               bool scaled)
{
    if (w == NULL)
        return;
    GRECT rect;
    browser_get_rect( w, BR_CONTENT, &rect  );
    *width = rect.g_w;
    *height = rect.g_h;
}

void gui_window_set_title(struct gui_window *gw, const char *title)
{

    if (gw == NULL)
        return;

    if (gw->root) {

        int l;
        char * conv;
        l = strlen(title)+1;
        if (utf8_to_local_encoding(title, l, &conv) == UTF8_CONVERT_OK ) {
            l = MIN((uint32_t)atari_sysinfo.aes_max_win_title_len, strlen(conv));
            if(gw->title == NULL)
                gw->title = malloc(l);
            else
                gw->title = realloc(gw->title, l);

            strncpy(gw->title, conv, l);
            free( conv );
        } else {
            l = MIN((size_t)atari_sysinfo.aes_max_win_title_len, strlen(title));
            if(gw->title == NULL)
                gw->title = malloc(l);
            else
                gw->title = realloc(gw->title, l);
            strncpy(gw->title, title, l);
        }
        gw->title[l] = 0;
        if(input_window == gw)
            window_set_title(gw->root, gw->title);
    }
}

/**
 * set the status bar message
 */
void gui_window_set_status(struct gui_window *w, const char *text)
{
    int l;
    if (w == NULL || text == NULL)
        return;

    assert(w->root);

    l = strlen(text)+1;
    if(w->status == NULL)
        w->status = malloc(l);
    else
        w->status = realloc(w->status, l);

    strncpy(w->status, text, l);
    w->status[l] = 0;

    if(input_window == w)
        window_set_stauts(w->root, (char*)text);
}

void gui_window_redraw_window(struct gui_window *gw)
{
    CMP_BROWSER b;
    GRECT rect;
    if (gw == NULL)
        return;
    b = gw->browser;
    guiwin_get_grect(gw->root->win, GUIWIN_AREA_CONTENT, &rect);
    window_schedule_redraw_grect(gw->root, &rect);
}

void gui_window_update_box(struct gui_window *gw, const struct rect *rect)
{
	GRECT area;
	struct guiwin_scroll_info_s *slid;

    if (gw == NULL)
        return;

    slid = guiwin_get_scroll_info(gw->root->win);

    guiwin_get_grect(gw->root->win, GUIWIN_AREA_CONTENT, &area);
	area.g_x += rect->x0 - (slid->x_pos * slid->x_unit_px);
	area.g_y += rect->y0 - (slid->y_pos * slid->y_unit_px);
    area.g_w = rect->x1 - rect->x0;
    area.g_h = rect->y1 - rect->y0;
    //dbg_grect("update box", &area);
    window_schedule_redraw_grect(gw->root, &area);
}

bool gui_window_get_scroll(struct gui_window *w, int *sx, int *sy)
{
	int x,y;
    if (w == NULL)
        return false;

	window_get_scroll(w->root, sx, sy);

    return( true );
}

void gui_window_set_scroll(struct gui_window *w, int sx, int sy)
{
    int units = 0;
    if ((w == NULL)
		|| (w->browser->bw == NULL)
			|| (w->browser->bw->current_content == NULL))
				return;

	//printf("scroll %d, %d\n", sx, sy);
	window_scroll_by(w->root, sx, sy);
    return;

}

void gui_window_scroll_visible(struct gui_window *w, int x0, int y0, int x1, int y1)
{
    LOG(("%s:(%p, %d, %d, %d, %d)", __func__, w, x0, y0, x1, y1));
    printf("scroll visible\n");
    gui_window_set_scroll(w,x0,y0);
    //browser_schedule_redraw_rect( w, 0, 0, x1-x0,y1-y0);
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
        // TODO: store content size!
        if(window_get_active_gui_window(gw->root) == gw){
            window_set_content_size( gw->root,
                                      content_get_width(gw->browser->bw->current_content),
                                      content_get_height(gw->browser->bw->current_content)
                                    );
            window_update_back_forward(gw->root);
            GRECT area;
            guiwin_get_grect(gw->root->win, GUIWIN_AREA_CONTENT, &area);
            window_schedule_redraw_grect(gw->root, &area);
        }
    }
}


void gui_clear_selection(struct gui_window *g)
{

}



/**
 * set the pointer shape
 */
void gui_window_set_pointer(struct gui_window *gw, gui_pointer_shape shape)
{
    if (gw == NULL)
        return;

    switch (shape) {
    case GUI_POINTER_POINT: /* link */
        gw->cursor = &gem_cursors.hand;
        break;

    case GUI_POINTER_MENU:
        gw->cursor = &gem_cursors.menu;
        break;

    case GUI_POINTER_CARET: /* input */
        gw->cursor = &gem_cursors.ibeam;
        break;

    case GUI_POINTER_CROSS:
        gw->cursor = &gem_cursors.cross;
        break;

    case GUI_POINTER_MOVE:
        gw->cursor = &gem_cursors.sizeall;
        break;

    case GUI_POINTER_RIGHT:
    case GUI_POINTER_LEFT:
        gw->cursor = &gem_cursors.sizewe;
        break;

    case GUI_POINTER_UP:
    case GUI_POINTER_DOWN:
        gw->cursor = &gem_cursors.sizens;
        break;

    case GUI_POINTER_RU:
    case GUI_POINTER_LD:
        gw->cursor = &gem_cursors.sizenesw;
        break;

    case GUI_POINTER_RD:
    case GUI_POINTER_LU:
        gw->cursor = &gem_cursors.sizenwse;
        break;

    case GUI_POINTER_WAIT:
        gw->cursor = &gem_cursors.wait;
        break;

    case GUI_POINTER_PROGRESS:
        gw->cursor = &gem_cursors.appstarting;
        break;

    case GUI_POINTER_NO_DROP:
        gw->cursor = &gem_cursors.nodrop;
        break;

    case GUI_POINTER_NOT_ALLOWED:
        gw->cursor = &gem_cursors.deny;
        break;

    case GUI_POINTER_HELP:
        gw->cursor = &gem_cursors.help;
        break;

    default:
        gw->cursor = &gem_cursors.arrow;
        break;
    }

    if (input_window == gw) {
        gem_set_cursor(gw->cursor);
    }
}

void gui_window_hide_pointer(struct gui_window *w)
{
    TODO();
}


void gui_window_set_url(struct gui_window *w, const char *url)
{
    int l;

    if (w == NULL)
        return;

    l = strlen(url)+1;

    if (w->url == NULL) {
        w->url = malloc(l);
    } else {
        w->url = realloc(w->url, l);
    }
    strncpy(w->url, url, l);
    w->url[l] = 0;
    if(input_window == w->root->active_gui_window){
        toolbar_set_url(w->root->toolbar, url);
    }
}

static void throbber_advance( void * data )
{
    LGRECT work;
    struct gui_window * gw = (struct gui_window *)data;
    if (gw->root == NULL)
        return;
    if (gw->root->toolbar == NULL)
        return;

    if (gw->root->toolbar->throbber.running == false)
        return;

    toolbar_throbber_progress(gw->root->toolbar);
    schedule(100, throbber_advance, gw );
}

void gui_window_start_throbber(struct gui_window *w)
{
    GRECT work;
    if (w == NULL)
        return;

    toolbar_set_throbber_state(w->root->toolbar, true);
    schedule(100, throbber_advance, w );
    rendering = true;
}

void gui_window_stop_throbber(struct gui_window *w)
{
    if (w == NULL)
        return;
    if (w->root->toolbar->throbber.running == false)
        return;

    schedule_remove(throbber_advance, w);

    toolbar_set_throbber_state(w->root->toolbar, false);

    rendering = false;
}

/* Place caret in window */
void gui_window_place_caret(struct gui_window *w, int x, int y, int height)
{
	//printf("gw place caret\n");

	window_place_caret(w->root, 1, x, y, height, NULL);
	w->root->caret.state |= CARET_STATE_ENABLED;
//
//	GRECT clip, dim;
//	struct guiwin_scroll_info_s * slid;
//    if (w == NULL)
//        return;
//
//	slid = guiwin_get_scroll_info(w->root->win);
//	window_get_grect(w->root, BROWSER_AREA_CONTENT, &clip);
//	dim.g_x = x - (slid->x_pos * slid->x_unit_px);
//	dim.g_y = y - (slid->y_pos * slid->y_unit_px);
//	dim.g_h = height;
//	dim.g_w = 2;
//	caret_show(&w->caret, guiwin_get_vdi_handle(w->root->win), &dim, &clip);
////    if( w->browser->caret.current.g_w > 0 )
////        gui_window_remove_caret( w );
////    w->browser->caret.requested.g_x = x;
////    w->browser->caret.requested.g_y = y;
////    w->browser->caret.requested.g_w = 1;
////    w->browser->caret.requested.g_h = height;
////    w->browser->caret.redraw = true;
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



	if(w->root->caret.dimensions.g_h > 0 ){
		//printf("gw hide caret\n");
		window_place_caret(w->root, 0, -1, -1, -1, NULL);
		w->root->caret.state &= ~CARET_STATE_ENABLED;
	}

//    if( w->browser->caret.background.fd_addr != NULL ) {
//        browser_restore_caret_background( w, NULL );
//        w->browser->caret.requested.g_w = 0;
//        w->browser->caret.current.g_w = 0;
//    }
    return;
}

void
gui_window_set_icon(struct gui_window *g, hlcache_handle *icon)
{
    struct bitmap *bmp_icon;

    bmp_icon = (icon != NULL) ? content_get_bitmap(icon) : NULL;
    g->icon = bmp_icon;
    if(input_window == g){
        window_set_icon(g->root, bmp_icon);
    }
}

void
gui_window_set_search_ico(hlcache_handle *ico)
{
    TODO();
}

void gui_window_new_content(struct gui_window *w)
{
	struct guiwin_scroll_info_s *slid = guiwin_get_scroll_info(w->root->win);
	slid->x_pos = 0;
	slid->y_pos = 0;
	guiwin_update_slider(w->root->win, GUIWIN_VH_SLIDER);
    gui_window_redraw_window(w);
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
    char * clip = scrap_txt_read();
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
    }

	free( clip );
}

bool gui_empty_clipboard(void)
{
    if( tmp_clipboard != NULL ) {
        free( tmp_clipboard );
        tmp_clipboard = NULL;
    }
    return true;
}

bool gui_add_to_clipboard(const char *text_utf8, size_t length_utf8, bool space,
                          const plot_font_style_t *fstyle)
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
    if( tmp_clipboard == NULL) {
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
    int r = scrap_txt_write(tmp_clipboard);
    return( (r>0)?true:false );
}

bool gui_copy_to_clipboard(struct selection *s)
{
    bool ret = false;
    if( s->defined ) {
        gui_empty_clipboard();
        if(selection_copy_to_clipboard(s)) {
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

void gui_401login_open(nsurl *url, const char *realm,
                       nserror (*cb)(bool proceed, void *pw), void *cbpw)
{
    bool bres;
    char * out = NULL;
    bres = login_form_do( url, (char*)realm, &out);
    if (bres) {
        LOG(("url: %s, realm: %s, auth: %s\n", url, realm, out ));
        urldb_set_auth_details(url, realm, out);
    }
    if (out != NULL) {
        free( out );
    }
    if (cb != NULL) {
		cb(bres, cbpw);
    }

}

void gui_cert_verify(nsurl *url, const struct ssl_cert_info *certs,
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

void gui_set_input_gui_window(struct gui_window *gw)
{
	LOG(("Setting input window from: %p to %p\n", input_window, gw));
	input_window = gw;
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

    global_history_destroy();
    hotlist_destroy();
    toolbar_exit();

    urldb_save_cookies(nsoption_charp(cookie_file));
    urldb_save(nsoption_charp(url_file));

    deskmenu_destroy();
    guiwin_exit();

    rsrc_free();

    LOG(("Shutting down plotter"));
    plot_finalise();
    if( tmp_clipboard != NULL ) {
        free( tmp_clipboard );
        tmp_clipboard = NULL;
    }
    LOG(("done"));
}




static bool
process_cmdline(int argc, char** argv)
{
    int opt;
    bool set_default_dimensions = true;

    LOG(("argc %d, argv %p", argc, argv));

    if ((nsoption_int(window_width) != 0) && (nsoption_int(window_height) != 0)) {

        option_window_width = nsoption_int(window_width);
        option_window_height = nsoption_int(window_height);
        option_window_x = nsoption_int(window_x);
        option_window_y = nsoption_int(window_y);

        if (option_window_width <= app.w && option_window_height < app.h) {
            set_default_dimensions = false;
        }
    }

    if (set_default_dimensions) {
        if( sys_type() == SYS_TOS ) {
            /* on single tasking OS, start as fulled window: */
            option_window_width = app.w;
            option_window_height = app.h-20;
            option_window_x = app.w/2-(option_window_width/2);
            option_window_y = (app.h/2)-(option_window_height/2);
        } else {
            option_window_width = 600;
            option_window_height = 360;
            option_window_x = 10;
            option_window_y = 30;
        }
    }

    if (nsoption_charp(homepage_url) != NULL)
        option_homepage_url = nsoption_charp(homepage_url);
    else
        option_homepage_url = NETSURF_HOMEPAGE;

    while((opt = getopt(argc, argv, "w:h:")) != -1) {
        switch (opt) {
        case 'w':
            option_window_width = atoi(optarg);
            break;

        case 'h':
            option_window_height = atoi(optarg);
            break;

        default:
            fprintf(stderr,
                    "Usage: %s [w,h,v] url\n",
                    argv[0]);
            return false;
        }
    }

    if (optind < argc) {
        option_homepage_url = argv[optind];
    }
    return true;
}

static inline void create_cursor(int flags, short mode, void * form,
                                 MFORM_EX * m)
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

/* Documented in desktop/options.h */
void gui_options_init_defaults(void)
{
    /* Set defaults for absent option strings */
    nsoption_setnull_charp(cookie_file, strdup("cookies"));

    if (nsoption_charp(cookie_file) == NULL) {
        die("Failed initialising string options");
    }
}

static void gui_init(int argc, char** argv)
{
    char buf[PATH_MAX];
    OBJECT * cursors;

    atari_find_resource(buf, "netsurf.rsc", "./res/netsurf.rsc");
    LOG(("%s ", (char*)&buf));
    if (rsrc_load(buf)==0) {
        die("Uable to open GEM Resource file!");
    }

    create_cursor(0, POINT_HAND, NULL, &gem_cursors.hand );
    create_cursor(0, TEXT_CRSR,  NULL, &gem_cursors.ibeam );
    create_cursor(0, THIN_CROSS, NULL, &gem_cursors.cross);
    create_cursor(0, BUSY_BEE, NULL, &gem_cursors.wait);
    create_cursor(0, ARROW, NULL, &gem_cursors.arrow);
    create_cursor(0, OUTLN_CROSS, NULL, &gem_cursors.sizeall);
    create_cursor(0, OUTLN_CROSS, NULL, &gem_cursors.sizenesw);
    create_cursor(0, OUTLN_CROSS, NULL, &gem_cursors.sizenwse);
    cursors = get_tree(CURSOR);
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
    nsoption_set_bool(core_select_menu, true);

    LOG(("Loading url.db from: %s", nsoption_charp(url_file) ));
    if( strlen(nsoption_charp(url_file)) ) {
        urldb_load(nsoption_charp(url_file));
    }

    LOG(("Loading cookies from: %s", nsoption_charp(cookie_file) ));
    if( strlen(nsoption_charp(cookie_file)) ) {
        urldb_load_cookies(nsoption_charp(cookie_file));
    }

    if (process_cmdline(argc,argv) != true)
        die("unable to process command line.\n");

    nkc_init();
    plot_init(nsoption_charp(atari_font_driver));
    tree_set_icon_dir(nsoption_charp(tree_icons_path));

	wind_get_grect(0, WF_WORKXYWH, &desk_area);
	aes_event_in.emi_m1leave = MO_LEAVE;
	aes_event_in.emi_m1.g_w = 1;
	aes_event_in.emi_m1.g_h = 1;
	next_poll = clock() + (CLOCKS_PER_SEC>>3);

}

static char *theapp = (char*)"NetSurf";
static void gui_init2(int argc, char** argv)
{
    deskmenu_init();
    menu_register( -1, theapp);
    if (sys_type() & (SYS_MAGIC|SYS_NAES|SYS_XAAES)) {
        menu_register( _AESapid, (char*)"  NetSurf ");
    }
    guiwin_init();
    global_history_init();
    hotlist_init();
    toolbar_init();
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
    // todo: replace with appl_init
    ApplInit();
    gl_apid = _AESapid;
    graf_mouse(BUSY_BEE, NULL);
    init_os_info();
    atari_find_resource((char*)&messages, "messages", "res/messages");
    atari_find_resource((char*)&options, "Choices", "Choices");
    netsurf_init(&argc, &argv, options, messages);
    gui_init(argc, argv);
    gui_init2(argc, argv);
    browser_window_create(option_homepage_url, 0, 0, true, false);
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


