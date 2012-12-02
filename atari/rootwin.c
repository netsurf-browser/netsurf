/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <windom.h>
#include <assert.h>
#include <math.h>
#include <osbind.h>

#include "utils/log.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "desktop/browser_private.h"
#include "desktop/mouse.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "css/css.h"
#include "atari/gemtk/gemtk.h"
#include "atari/gui.h"
#include "atari/rootwin.h"
#include "atari/browser.h"
#include "atari/misc.h"
#include "atari/plot/plot.h"
#include "atari/global_evnt.h"
#include "atari/res/netsurf.rsh"
#include "atari/browser.h"
#include "atari/toolbar.h"
#include "atari/statusbar.h"
#include "atari/search.h"
#include "atari/osspec.h"
#include "atari/encoding.h"
#include "atari/redrawslots.h"
#include "atari/toolbar.h"
#include "atari/gemtk/gemtk.h"

extern struct gui_window *input_window;
extern EVMULT_OUT aes_event_out;

struct rootwin_data_s {
    struct s_gui_win_root *rootwin;
};

/* -------------------------------------------------------------------------- */
/* Static module methods                                                      */
/* -------------------------------------------------------------------------- */
static void on_redraw(ROOTWIN *rootwin, short msg[8]);
static void on_resized(ROOTWIN *rootwin);
static void on_file_dropped(ROOTWIN *rootwin, short msg[8]);
static short on_window_key_input(ROOTWIN * rootwin, unsigned short nkc);
static bool on_content_mouse_click(ROOTWIN *rootwin);

static bool redraw_active = false;

static const struct redraw_context rootwin_rdrw_ctx = {
    .interactive = true,
    .background_images = true,
    .plot = &atari_plotters
};

/* -------------------------------------------------------------------------- */
/* Module public functions:                                                   */
/* -------------------------------------------------------------------------- */


static short handle_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{
    short retval = 0;
    struct rootwin_data_s * data = guiwin_get_user_data(win);

    if ((ev_out->emo_events & MU_MESAG) != 0) {
        // handle message
        printf("root win msg: %d\n", msg[0]);
        switch (msg[0]) {

        case WM_REDRAW:
            on_redraw(data->rootwin, msg);
            break;

        case WM_REPOSED:
        case WM_SIZED:
        case WM_MOVED:
        case WM_FULLED:
            on_resized(data->rootwin);
            break;

        case WM_ICONIFY:
            if( input_window->root == data->rootwin) {
                input_window = NULL;
            }
            break;

        case WM_TOPPED:
        case WM_NEWTOP:
        case WM_UNICONIFY:
            input_window = data->rootwin->active_gui_window;
            break;

        case WM_CLOSED:
            // TODO: this needs to iterate through all gui windows and
            // check if the rootwin is this window...
            if (data->rootwin->active_gui_window != NULL) {
                browser_window_destroy(
                    data->rootwin->active_gui_window->browser->bw);
            }
            break;

        case AP_DRAGDROP:
            on_file_dropped(data->rootwin, msg);
            break;

        case WM_TOOLBAR:
            toolbar_mouse_input(data->rootwin->toolbar, msg[4]);
            break;

        default:
            break;
        }
    }
    if ((ev_out->emo_events & MU_KEYBD) != 0) {

        // handle key
        uint16_t nkc = gem_to_norm( (short)ev_out->emo_kmeta,
                                    (short)ev_out->emo_kreturn);
        retval = on_window_key_input(data->rootwin, nkc);

    }
    if ((ev_out->emo_events & MU_BUTTON) != 0) {
        LOG(("Mouse click at: %d,%d\n", ev_out->emo_mouse.p_x,
             ev_out->emo_mouse.p_y));
        GRECT carea;
        guiwin_get_grect(data->rootwin->win, GUIWIN_AREA_CONTENT, &carea);
        if (POINT_WITHIN(ev_out->emo_mouse.p_x, ev_out->emo_mouse.p_y, carea)) {
            on_content_mouse_click(data->rootwin);
        }
    }

    return(retval);
}


int window_create(struct gui_window * gw,
                  struct browser_window * bw,
                  unsigned long inflags)
{
    int err = 0;
    bool tb, sb;
    int flags;
    short aes_handle;

    tb = (inflags & WIDGET_TOOLBAR);
    sb = (inflags & WIDGET_STATUSBAR);

    flags = CLOSER | MOVER | NAME | FULLER | SMALLER;
    if( inflags & WIDGET_SCROLL ) {
        flags |= (UPARROW | DNARROW | LFARROW | RTARROW | VSLIDE | HSLIDE);
    }
    if( inflags & WIDGET_RESIZE ) {
        flags |= ( SIZER );
    }
    if( inflags & WIDGET_STATUSBAR ) {
        flags |= ( INFO );
    }

    gw->root = malloc(sizeof(struct s_gui_win_root));
    if (gw->root == NULL)
        return(-1);
    memset(gw->root, 0, sizeof(struct s_gui_win_root) );
    gw->root->title = malloc(atari_sysinfo.aes_max_win_title_len+1);

    redraw_slots_init(&gw->root->redraw_slots, 8);

    // TODO: use desk size
    aes_handle = wind_create(flags, 40, 40, app.w, app.h);
    if(aes_handle<0) {
        free(gw->root->title);
        free(gw->root);
        return( -1 );
    }
    gw->root->win = guiwin_add(aes_handle,
                               GW_FLAG_PREPROC_WM | GW_FLAG_RECV_PREPROC_WM, handle_event);

    struct rootwin_data_s * data = malloc(sizeof(struct rootwin_data_s));
    data->rootwin = gw->root;
    guiwin_set_user_data(gw->root->win, (void*)data);
    struct guiwin_scroll_info_s *slid = guiwin_get_scroll_info(gw->root->win);
    slid->y_unit_px = 16;
    slid->x_unit_px = 16;

    /* create toolbar component: */
    guiwin_set_toolbar(gw->root->win, get_tree(TOOLBAR), 0, 0);
    if( tb ) {
        gw->root->toolbar = toolbar_create(gw->root);
        assert(gw->root->toolbar);
    } else {
        gw->root->toolbar = NULL;
    }

    /* create browser component: */
    gw->browser = browser_create( gw, bw, NULL, CLT_HORIZONTAL, 1, 1 );

    /* create statusbar component: */
    if(sb) {
        gw->root->statusbar = sb_create( gw );
    } else {
        gw->root->statusbar = NULL;
    }

    // Setup some window defaults:
    wind_set_str(aes_handle, WF_ICONTITLE, (char*)"NetSurf");
    wind_set(aes_handle, WF_OPTS, 1, WO0_FULLREDRAW, 0, 0);
    wind_set(aes_handle, WF_OPTS, 1, WO0_NOBLITW, 0, 0);
    wind_set(aes_handle, WF_OPTS, 1, WO0_NOBLITH, 0, 0);

    if (inflags & WIN_TOP) {
        window_set_focus(gw->root, BROWSER, gw->browser);
    }

    return (err);
}

void window_unref_gui_window(ROOTWIN *rootwin, struct gui_window *gw)
{
    struct gui_window *w;
    input_window = NULL;

    LOG(("window: %p, gui_window: %p", rootwin, gw));

    w = window_list;
    // find the next active tab:
    while( w != NULL ) {
        if(w->root == rootwin && w != gw) {
            gui_set_input_gui_window(w);
            break;
        }
        w = w->next;
    }
    if(input_window == NULL) {
        // the last gui window for this rootwin was removed:
        redraw_slots_free(&rootwin->redraw_slots);
        window_destroy(rootwin);
    }
}

int window_destroy(ROOTWIN *rootwin)
{
    int err = 0;
    struct gui_window *w;

    assert(rootwin != NULL);

    LOG(("%p", rootwin));

    if (guiwin_get_user_data(rootwin->win) != NULL) {
        free(guiwin_get_user_data(rootwin->win));
    }

    // make sure we do not destroy windows which have gui_windows attached:
    w = window_list;
    while( w != NULL ) {
        if(w->root == rootwin) {
            assert(rootwin == NULL);
        }
        w = w->next;
    }

    if (rootwin->toolbar)
        toolbar_destroy(rootwin->toolbar);

    if(rootwin->statusbar)
        sb_destroy(rootwin->statusbar);

    if(rootwin->title)
        free(rootwin->title);

    guiwin_remove(rootwin->win);
    free(rootwin);
    return(err);
}



void window_open(ROOTWIN *rootwin, GRECT pos)
{
    GRECT br, g;

    assert(rootwin->active_gui_window != NULL);

    short aes_handle = guiwin_get_handle(rootwin->win);
    wind_open(aes_handle, pos.g_x, pos.g_y, pos.g_w, pos.g_h );
    wind_set_str(aes_handle, WF_NAME, (char *)"");

    rootwin->active_gui_window->browser->attached = true;
    if(rootwin->statusbar != NULL) {
        sb_attach(rootwin->statusbar, rootwin->active_gui_window);
    }
    guiwin_get_grect(rootwin->win, GUIWIN_AREA_TOOLBAR, &g);
    toolbar_set_attached(rootwin->toolbar, true);
    toolbar_set_dimensions(rootwin->toolbar, &g);
    window_update_back_forward(rootwin);
    /*TBD: get already present content and set size? */
    input_window = rootwin->active_gui_window;
    window_set_focus(rootwin, BROWSER, rootwin->active_gui_window->browser);
}



/* update back forward buttons (see tb_update_buttons (bug) ) */
void window_update_back_forward(struct s_gui_win_root *rootwin)
{
    struct gui_window * active_gw = rootwin->active_gui_window;
    toolbar_update_buttons(rootwin->toolbar, active_gw->browser->bw, -1);
}

void window_set_stauts(struct s_gui_win_root *rootwin, char * text)
{
    assert(rootwin != NULL);

    CMP_STATUSBAR sb = rootwin->statusbar;

    if( sb == NULL)
        return;

    if(text != NULL)
        sb_set_text(sb, text);
    else
        sb_set_text(sb, "");
}

void window_set_title(struct s_gui_win_root * rootwin, char *title)
{
    wind_set_str(guiwin_get_handle(rootwin->win), WF_NAME, title);
}

void window_set_content_size(ROOTWIN *rootwin, int width, int height)
{
    GRECT area;
    struct guiwin_scroll_info_s *slid = guiwin_get_scroll_info(rootwin->win);

    guiwin_get_grect(rootwin->win, GUIWIN_AREA_CONTENT, &area);
    slid->x_units = (width/slid->x_unit_px);
    slid->y_units = (height/slid->y_unit_px);
    guiwin_update_slider(rootwin->win, GUIWIN_VH_SLIDER);
    // TODO: reset slider to 0
}

/* set focus to an arbitary element */
void window_set_focus(struct s_gui_win_root *rootwin,
                      enum focus_element_type type, void * element)
{
    struct text_area * ta;

    assert(rootwin != NULL);

    if (rootwin->focus.type != type || rootwin->focus.element != element) {
        LOG(("Set focus: %p (%d)\n", element, type));
        rootwin->focus.type = type;
        rootwin->focus.element = element;
        if( element != NULL ) {
            switch( type ) {

            case URL_WIDGET:
                // TODO: make something like: toolbar_text_select_all();
                ta = toolbar_get_textarea(rootwin->toolbar,
                                          URL_INPUT_TEXT_AREA);
                textarea_keypress(ta, KEY_SELECT_ALL);
                break;

            default:
                break;

            }
        }
    }
}

/* check if the url widget has focus */
bool window_url_widget_has_focus(struct s_gui_win_root *rootwin)
{
    assert(rootwin != NULL);

    if (rootwin->focus.type == URL_WIDGET) {
        return true;
    }
    return false;
}

/* check if an arbitary window widget / or frame has the focus */
bool window_widget_has_focus(struct s_gui_win_root *rootwin,
                             enum focus_element_type t, void * element)
{
    assert(rootwin != NULL);
    if( element == NULL  ) {
        return((rootwin->focus.type == t));
    }

    return((element == rootwin->focus.element && t == rootwin->focus.type));
}

void window_set_icon(ROOTWIN *rootwin, struct bitmap * bmp )
{
    rootwin->icon = bmp;
    /* redraw window when it is iconyfied: */
    if (rootwin->icon != NULL) {
        short info, dummy;
        if (guiwin_get_state(rootwin->win) & GW_STATUS_ICONIFIED) {
            window_redraw_favicon(rootwin, NULL);
        }
    }
}

void window_set_active_gui_window(ROOTWIN *rootwin, struct gui_window *gw)
{
    if (rootwin->active_gui_window != NULL) {
        if(rootwin->active_gui_window == gw) {
            return;
        }
    }
    rootwin->active_gui_window = gw;

    window_set_icon(rootwin, gw->icon);
    window_set_stauts(rootwin, gw->status);
    window_set_title(rootwin, gw->title);
    toolbar_set_url(rootwin->toolbar, gw->url);
    // TODO: implement window_restore_browser()
    // window_restore_browser(gw->browser);
}

struct gui_window * window_get_active_gui_window(ROOTWIN * rootwin) {
    return(rootwin->active_gui_window);
}


/**
 * Redraw the favicon
*/
void window_redraw_favicon(ROOTWIN *rootwin, GRECT *clip)
{
    GRECT work;

    assert(rootwin);

    guiwin_clear(rootwin->win);
    guiwin_get_grect(rootwin->win, GUIWIN_AREA_WORK, &work);

    if (clip == NULL) {
        clip = &work;
    } else {
        if(!rc_intersect(&work, clip)) {
            return;
        }
    }

    if (rootwin->icon == NULL) {
        OBJECT * tree = get_tree(ICONIFY);
        tree->ob_x = work.g_x;
        tree->ob_y = work.g_y;
        tree->ob_width = work.g_w;
        tree->ob_height = work.g_h;
        objc_draw(tree, 0, 8, clip->g_x, clip->g_y, clip->g_w, clip->g_h);
    } else {
        // TODO: consider the clipping rectangle
        struct rect work_clip = { 0,0,work.g_w,work.g_h };
        int xoff=0;
        if (work.g_w > work.g_h) {
            xoff = ((work.g_w-work.g_h)/2);
            work.g_w = work.g_h;
        }
        plot_set_dimensions( work.g_x+xoff, work.g_y, work.g_w, work.g_h);
        plot_clip(&work_clip);
        atari_plotters.bitmap(0, 0, work.g_w, work.g_h, rootwin->icon, 0xffffff, 0);
    }
}

/***
*   Schedule an redraw area, redraw requests during redraw are
*   not optimized (merged) into other areas, so that the redraw
*   functions can spot the change.
*
*/
void window_schedule_redraw_grect(ROOTWIN *rootwin, GRECT *area)
{
    GRECT work;


    //dbg_grect("window_schedule_redraw_grect input ", area);

    guiwin_get_grect(rootwin->win, GUIWIN_AREA_WORK, &work);
    rc_intersect(area, &work);

    dbg_grect("window_schedule_redraw_grect intersection ", &work);

    redraw_slot_schedule_grect(&rootwin->redraw_slots, &work, redraw_active);
}

static void window_redraw_content(ROOTWIN *rootwin, GRECT *content_area,
                                  GRECT *clip,
                                  struct guiwin_scroll_info_s * slid,
                                  struct browser_window *bw)
{

    struct rect redraw_area;
    GRECT content_area_rel;

    if(bw->window->browser->reformat_pending) {
        browser_window_reformat(bw, true, content_area->g_w,
                                content_area->g_h);
        bw->window->browser->reformat_pending = false;
        //return;
    }

    //dbg_grect("browser redraw, content area", content_area);
    //dbg_grect("browser redraw, content clip", clip);

    plot_set_dimensions(content_area->g_x, content_area->g_y,
                        content_area->g_w, content_area->g_h);


    /* first, we make the coords relative to the content area: */
    content_area_rel.g_x = clip->g_x - content_area->g_x;
    content_area_rel.g_y = clip->g_y - content_area->g_y;
    content_area_rel.g_w = clip->g_w;
    content_area_rel.g_h = clip->g_h;

    if (content_area_rel.g_x < 0) {
        content_area_rel.g_w += content_area_rel.g_x;
        content_area_rel.g_x = 0;
    }

    if (content_area_rel.g_y < 0) {
        content_area_rel.g_h += content_area_rel.g_y;
        content_area_rel.g_y = 0;
    }

    dbg_grect("browser redraw, relative plot coords:", &content_area_rel);

    redraw_area.x0 = content_area_rel.g_x;
    redraw_area.y0 = content_area_rel.g_y;
    redraw_area.x1 = content_area_rel.g_x + content_area_rel.g_w;
    redraw_area.y1 = content_area_rel.g_y + content_area_rel.g_h;

    plot_clip(&redraw_area);

    browser_window_redraw( bw, -(slid->x_pos*slid->x_unit_px),
                           -(slid->y_pos*slid->y_unit_px), &redraw_area, &rootwin_rdrw_ctx );
}

void window_process_redraws(ROOTWIN * rootwin)
{
    GRECT work, visible_ro, tb_area, content_area;
    short aes_handle, i;
    bool toolbar_rdrw_required;
    struct guiwin_scroll_info_s *slid =NULL;

    redraw_active = true;

    aes_handle = guiwin_get_handle(rootwin->win);

    guiwin_get_grect(rootwin->win, GUIWIN_AREA_TOOLBAR, &tb_area);
    guiwin_get_grect(rootwin->win, GUIWIN_AREA_CONTENT, &content_area);

    short pxy_clip[4];

    pxy_clip[0] = tb_area.g_x;
    pxy_clip[0] = tb_area.g_y;
    pxy_clip[0] = pxy_clip[0] + tb_area.g_w + content_area.g_w - 1;
    pxy_clip[0] = pxy_clip[1] + tb_area.g_h + content_area.g_h - 1;
    vs_clip(guiwin_get_vdi_handle(rootwin->win), 1, pxy_clip);

    while(plot_lock() == false);

    wind_get_grect(aes_handle, WF_FIRSTXYWH, &visible_ro);
    while (visible_ro.g_w > 0 && visible_ro.g_h > 0) {

        // TODO: optimze the rectangle list -
        // remove rectangles which were completly inside the visible area.
        // that way we don't have to loop over again...
        for(i=0; i<rootwin->redraw_slots.areas_used; i++) {

            GRECT rdrw_area_ro = {
                rootwin->redraw_slots.areas[i].x0,
                rootwin->redraw_slots.areas[i].y0,
                rootwin->redraw_slots.areas[i].x1 -
                rootwin->redraw_slots.areas[i].x0,
                rootwin->redraw_slots.areas[i].y1 -
                rootwin->redraw_slots.areas[i].y0
            };
            rc_intersect(&visible_ro, &rdrw_area_ro);
            GRECT rdrw_area = rdrw_area_ro;

            if (rc_intersect(&tb_area, &rdrw_area)) {
                toolbar_redraw(rootwin->toolbar, &rdrw_area);
            }

            rdrw_area = rdrw_area_ro;
            if (rc_intersect(&content_area, &rdrw_area)) {
                if(slid == NULL)
                    slid = guiwin_get_scroll_info(rootwin->win);
                window_redraw_content(rootwin, &content_area, &rdrw_area, slid,
                                      rootwin->active_gui_window->browser->bw);
            }

        }
        wind_get_grect(aes_handle, WF_NEXTXYWH, &visible_ro);
    }
    vs_clip(guiwin_get_vdi_handle(rootwin->win), 0, pxy_clip);
    rootwin->redraw_slots.areas_used = 0;
    redraw_active = false;

    plot_unlock();
}


/* -------------------------------------------------------------------------- */
/* Event Handlers:                                                            */
/* -------------------------------------------------------------------------- */

static bool on_content_mouse_click(ROOTWIN *rootwin)
{
    short dummy, mbut, mx, my;
    GRECT cwork;
    browser_mouse_state bmstate = 0;
    struct gui_window *gw;
    struct guiwin_scroll_info_s *slid;

    gw = window_get_active_gui_window(rootwin);
    if( input_window != gw ) {
        input_window = gw;
    }

    window_set_focus(gw->root, BROWSER, (void*)gw->browser );
    guiwin_get_grect(gw->root->win, GUIWIN_AREA_CONTENT, &cwork);

    /* convert screen coords to component coords: */
    mx = aes_event_out.emo_mouse.p_x - cwork.g_x;
    my = aes_event_out.emo_mouse.p_y - cwork.g_y;

    /* Translate GEM key state to netsurf mouse modifier */
    if ( aes_event_out.emo_kmeta & (K_RSHIFT | K_LSHIFT)) {
        bmstate |= BROWSER_MOUSE_MOD_1;
    } else {
        bmstate &= ~(BROWSER_MOUSE_MOD_1);
    }
    if ( (aes_event_out.emo_kmeta & K_CTRL) ) {
        bmstate |= BROWSER_MOUSE_MOD_2;
    } else {
        bmstate &= ~(BROWSER_MOUSE_MOD_2);
    }
    if ( (aes_event_out.emo_kmeta & K_ALT) ) {
        bmstate |= BROWSER_MOUSE_MOD_3;
    } else {
        bmstate &= ~(BROWSER_MOUSE_MOD_3);
    }

    /* convert component coords to scrolled content coords: */
    slid = guiwin_get_scroll_info(rootwin->win);
    int sx_origin = (mx + slid->x_pos * slid->x_unit_px);
    int sy_origin = (my + slid->y_pos * slid->y_unit_px);

    short rel_cur_x, rel_cur_y;
    short prev_x=sx_origin, prev_y=sy_origin;
    bool dragmode = false;

    /* Detect left mouse button state and compare with event state: */
    graf_mkstate(&rel_cur_x, &rel_cur_y, &mbut, &dummy);
    if( (mbut & 1) && (evnt.mbut & 1) ) {
        /* Mouse still pressed, report drag */
        rel_cur_x = (rel_cur_x - cwork.g_x) + slid->x_pos * slid->x_unit_px;
        rel_cur_y = (rel_cur_y - cwork.g_y) + slid->y_pos * slid->y_unit_px;
        browser_window_mouse_click( gw->browser->bw,
                                    BROWSER_MOUSE_DRAG_ON|BROWSER_MOUSE_DRAG_1,
                                    sx_origin, sy_origin);
        do {
            // only consider movements of 5px or more as drag...:
            if( abs(prev_x-rel_cur_x) > 5 || abs(prev_y-rel_cur_y) > 5 ) {
                browser_window_mouse_track( gw->browser->bw,
                                            BROWSER_MOUSE_DRAG_ON|BROWSER_MOUSE_DRAG_1,
                                            rel_cur_x, rel_cur_y);
                prev_x = rel_cur_x;
                prev_y = rel_cur_y;
                dragmode = true;
            } else {
                if( dragmode == false ) {
                    browser_window_mouse_track( gw->browser->bw,BROWSER_MOUSE_PRESS_1,
                                                rel_cur_x, rel_cur_y);
                }
            }
            // we may need to process scrolling:
            if (rootwin->redraw_slots.areas_used > 0) {
                window_process_redraws(rootwin);
            }
            graf_mkstate(&rel_cur_x, &rel_cur_y, &mbut, &dummy);
            rel_cur_x = (rel_cur_x - cwork.g_x) + slid->x_pos * slid->x_unit_px;
            rel_cur_y = (rel_cur_y - cwork.g_y) + slid->y_pos * slid->y_unit_px;
        } while( mbut & 1 );
        browser_window_mouse_track(gw->browser->bw, 0, rel_cur_x,rel_cur_y);
    } else {
        /* Right button pressed? */
        if( (evnt.mbut & 2 ) ) {
            context_popup( gw, aes_event_out.emo_mouse.p_x,
                           aes_event_out.emo_mouse.p_x);
        } else {
            browser_window_mouse_click(gw->browser->bw,
                                       bmstate|BROWSER_MOUSE_PRESS_1,
                                       sx_origin,sy_origin);
            browser_window_mouse_click(gw->browser->bw,
                                       bmstate|BROWSER_MOUSE_CLICK_1,
                                       sx_origin,sy_origin);
        }
    }
}

/*
	Report keypress to browser component.
	parameter:
		- unsigned short nkc ( CFLIB normalised key code )
*/
static bool on_content_keypress(struct browser_window *bw, unsigned short nkc)
{
    bool r = false;
    unsigned char ascii = (nkc & 0xFF);
    long ucs4;
    long ik = nkc_to_input_key( nkc, &ucs4 );

    // pass event to specific control?

    if (ik == 0) {
        if (ascii >= 9) {
            r = browser_window_key_press(bw, ucs4);
        }
    } else {
        r = browser_window_key_press(bw, ik);
        if (r == false) {

            GRECT g;
            GUIWIN * w = bw->window->root->win;
            guiwin_get_grect(w, GUIWIN_AREA_CONTENT, &g);

            struct guiwin_scroll_info_s *slid = guiwin_get_scroll_info(w);

            switch( ik ) {
            case KEY_LINE_START:
                guiwin_scroll(w, GUIWIN_HSLIDER, -(g.g_w/slid->x_unit_px),
                              false);
                break;

            case KEY_LINE_END:
                guiwin_scroll(w, GUIWIN_HSLIDER, (g.g_w/slid->x_unit_px),
                              false);
                break;

            case KEY_PAGE_UP:
                guiwin_scroll(w, GUIWIN_VSLIDER, (g.g_h/slid->y_unit_px),
                              false);
                break;

            case KEY_PAGE_DOWN:
                guiwin_scroll(w, GUIWIN_VSLIDER, (g.g_h/slid->y_unit_px),
                              false);
                break;

            case KEY_RIGHT:
                guiwin_scroll(w, GUIWIN_HSLIDER, -1, false);
                break;

            case KEY_LEFT:
                guiwin_scroll(w, GUIWIN_HSLIDER, 1, false);
                break;

            case KEY_UP:
                guiwin_scroll(w, GUIWIN_VSLIDER, -1, false);
                break;

            case KEY_DOWN:
                guiwin_scroll(w, GUIWIN_VSLIDER, 1, false);
                break;

            default:
                break;
            }
            guiwin_update_slider(w, GUIWIN_VSLIDER|GUIWIN_HSLIDER);
        }
    }

    return(r);
}

static short on_window_key_input(ROOTWIN *rootwin, unsigned short nkc)
{
    bool done = false;
    struct gui_window * gw = window_get_active_gui_window(rootwin);
    struct gui_window * gw_tmp;

    if( gw == NULL )
        return(false);

    if(window_url_widget_has_focus((void*)gw->root)) {
        /* make sure we report for the root window and report...: */
        done = toolbar_key_input(gw->root->toolbar, nkc);
    }  else  {
        gw_tmp = window_list;
        /* search for active browser component: */
        while( gw_tmp != NULL && done == false ) {
            /* todo: only handle when input_window == ontop */
            if( window_widget_has_focus(input_window->root, BROWSER,
                                        (void*)gw_tmp->browser)) {
                done = on_content_keypress(gw_tmp->browser->bw, nkc);
                break;
            } else {
                gw_tmp = gw_tmp->next;
            }
        }
    }
    return((done==true) ? 1 : 0);
}


static void on_redraw(ROOTWIN *rootwin, short msg[8])
{
    short handle;

    GRECT clip = {msg[4], msg[5], msg[6], msg[7]};

    if(guiwin_get_state(rootwin->win) & GW_STATUS_ICONIFIED) {
        GRECT clip = {msg[4], msg[5], msg[6], msg[7]};
        window_redraw_favicon(rootwin, &clip);
    } else {
        window_schedule_redraw_grect(rootwin, &clip);
    }
}

static void on_resized(ROOTWIN *rootwin)
{
    GRECT g;
    short handle;
    struct gui_window *gw;

    handle = guiwin_get_handle(rootwin->win);
    gw = window_get_active_gui_window(rootwin);

    //printf("resized...\n");

    assert(gw != NULL);

    if(gw == NULL)
        return;

    wind_get_grect(handle, WF_CURRXYWH, &g);

    if (rootwin->loc.g_w != g.g_w || rootwin->loc.g_h != g.g_h) {
        if ( gw->browser->bw->current_content != NULL ) {
            /* Reformat will happen when redraw is processed: */
            // TODO: call reformat directly, this was introduced because
            // of bad AES knowledge, it's ok to call it directly here...
            //printf("reformat......\n");
            rootwin->active_gui_window->browser->reformat_pending = true;
        }
    }
//    if (rootwin->loc.g_x != g.g_x || rootwin->loc.g_y != g.g_y) {
//        // moved
//    }

    rootwin->loc = g;
    guiwin_get_grect(rootwin->win, GUIWIN_AREA_TOOLBAR, &g);
    toolbar_set_dimensions(rootwin->toolbar, &g);
}

static void on_file_dropped(ROOTWIN *rootwin, short msg[8])
{
    char file[DD_NAMEMAX];
    char name[DD_NAMEMAX];
    char *buff=NULL;
    int dd_hdl;
    int dd_msg; /* pipe-handle */
    long size;
    char ext[32];
    short mx,my,bmstat,mkstat;
    struct gui_window *gw;

    graf_mkstate(&mx, &my, &bmstat, &mkstat);

    gw = window_get_active_gui_window(rootwin);

    if( gw == NULL )
        return;

    if(guiwin_get_state(rootwin->win) & GW_STATUS_ICONIFIED)
        return;

    dd_hdl = ddopen( msg[7], DD_OK);
    if( dd_hdl<0)
        return;	/* pipe not open */
    memset( ext, 0, 32);
    strcpy( ext, "ARGS");
    dd_msg = ddsexts( dd_hdl, ext);
    if( dd_msg<0)
        goto error;
    dd_msg = ddrtry( dd_hdl, (char*)&name[0], (char*)&file[0], (char*)&ext[0], &size);
    if( size+1 >= PATH_MAX )
        goto error;
    if( !strncmp( ext, "ARGS", 4) && dd_msg > 0) {
        ddreply(dd_hdl, DD_OK);
        buff = (char*)malloc(sizeof(char)*(size+1));
        if (buff != NULL) {
            if (Fread(dd_hdl, size, buff ) == size)
                buff[size] = 0;
            LOG(("file: %s, ext: %s, size: %d dropped at: %d,%d\n",
                 (char*)buff, (char*)&ext,
                 size, mx, my
                ));
            {
                GRECT bwrect;
                struct browser_window * bw = gw->browser->bw;
                browser_get_rect(gw, BR_CONTENT, &bwrect);
                mx = mx - bwrect.g_x;
                my = my - bwrect.g_y;
                if( (mx < 0 || mx > bwrect.g_w) || (my < 0 || my > bwrect.g_h) )
                    return;

                utf8_convert_ret ret;
                char *utf8_fn;

                ret = utf8_from_local_encoding(buff, 0, &utf8_fn);
                if (ret != UTF8_CONVERT_OK) {
                    free(buff);
                    /* A bad encoding should never happen */
                    LOG(("utf8_from_local_encoding failed"));
                    assert(ret != UTF8_CONVERT_BADENC);
                    /* no memory */
                    return;
                }
                browser_window_drop_file_at_point( gw->browser->bw,
                                                   mx+gw->browser->scroll.current.x,
                                                   my+gw->browser->scroll.current.y,
                                                   utf8_fn );
                free(utf8_fn);
                free(buff);
            }
        }
    }
error:
    ddclose( dd_hdl);
}

