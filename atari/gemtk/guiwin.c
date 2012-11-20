//#include "global.h"

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <cflib.h>

#include <mt_gem.h>
#include "gemtk.h"

//#define DEBUG_PRINT(x)        printf x
#define DEBUG_PRINT(x)

struct gui_window_s {
    short handle;
    guiwin_event_handler_f handler_func;
    uint32_t flags;
    uint32_t state;
    OBJECT * toolbar;
    short toolbar_idx;
    struct guiwin_scroll_info_s scroll_info;
    void *user_data;
    struct gui_window_s *next, *prev;
};

static GUIWIN * winlist;
static VdiHdl v_vdi_h = -1;
static short work_out[57];

static void move_screen( int vhandle, GRECT *screen, int dx, int dy) {
        INT16 xy[ 8];
        long dum = 0L;
        GRECT g;

        /* get intersection with screen area */
        wind_get_grect(0, WF_CURRXYWH, &g);
        rc_intersect(&g, screen);
        xy[ 0] = screen -> g_x;
        xy[ 1] = screen -> g_y;
        xy[ 2] = xy[ 0] + screen -> g_w - 1;
        xy[ 3] = xy[ 1] + screen -> g_h - 1;
        xy[ 4] = xy[ 0] + dx;
        xy[ 5] = xy[ 1] + dy;
        xy[ 6] = xy[ 2] + dx;
        xy[ 7] = xy[ 3] + dy;
        vro_cpyfm( vhandle, S_ONLY, xy, (MFDB *)&dum, (MFDB *)&dum);
}

static short preproc_wm(GUIWIN * gw, EVMULT_OUT *ev_out, short msg[8])
{
    GRECT g, g_ro, tb_area, tb_area_ro;
    short retval = 1;
    int val;
    struct guiwin_scroll_info_s *slid;

    switch(msg[0]) {

    case WM_ARROWED:
        if((gw->flags & GW_FLAG_CUSTOM_SCROLLING) == 0) {

            slid = guiwin_get_scroll_info(gw);
            guiwin_get_grect(gw, GUIWIN_AREA_CONTENT, &g);
            g_ro = g;

            switch(msg[4]) {
            case WA_DNPAGE:

                val = g.g_h;
                // complete redraw
                // increase scroll val by page size...
                break;

            case WA_UPLINE:
                slid->y_pos = MAX(0, slid->y_pos-1);
                // partial redraw
                break;

            case WA_DNLINE:
                slid->y_pos = MIN(slid->y_pos_max, slid->y_pos+1);
                g.g_y += slid->y_unit_px;
                g.g_h -= slid->y_unit_px;
                move_screen(v_vdi_h, &g, 0, -slid->y_unit_px);
                g.g_y = g_ro.g_y + g_ro.g_h - slid->y_unit_px;
                g.g_h = slid->y_unit_px;
                guiwin_send_redraw(gw, &g);
                // move content up by unit size and sched redraw for the
                // bottom 16 px
                // partial redraw
                break;

            case WA_LFPAGE:
                // complete redraw
                // increase scroll val by page size...
                break;

            case WA_RTPAGE:
                // complete redraw
                // increase scroll val by page size...
                break;

            case WA_LFLINE:
                slid->x_pos = MAX(0, slid->x_pos-1);
                // partial redraw
                break;

            case WA_RTLINE:
                slid->x_pos = MIN(slid->x_pos_max, slid->x_pos+1);
                // partial redraw
                break;

            default:
                break;
            }
        }
        break;

    case WM_TOPPED:
        wind_set(gw->handle, WF_TOP, 1, 0, 0, 0);
        break;

    case WM_MOVED:
        wind_get_grect(gw->handle, WF_CURRXYWH, &g);
        wind_set(gw->handle, WF_CURRXYWH, msg[4], msg[5], g.g_w, g.g_h);
        break;

    case WM_SIZED:
    case WM_REPOSED:
        wind_get_grect(gw->handle, WF_CURRXYWH, &g);
        wind_set(gw->handle, WF_CURRXYWH, g.g_x, g.g_y, msg[6], msg[7]);
        break;

    case WM_FULLED:
        wind_get_grect(gw->handle, WF_FULLXYWH, &g);
        wind_set_grect(gw->handle, WF_CURRXYWH, &g);
        break;

    case WM_ICONIFY:
        wind_set(gw->handle, WF_ICONIFY, msg[4], msg[5], msg[6], msg[7]);
        gw->state |= GW_STATUS_ICONIFIED;
        break;

    case WM_UNICONIFY:
        wind_set(gw->handle, WF_UNICONIFY, msg[4], msg[5], msg[6], msg[7]);
        gw->state &= ~(GW_STATUS_ICONIFIED);
        break;

    case WM_SHADED:
        gw->state |= GW_STATUS_SHADED;
        break;

    case WM_UNSHADED:
        gw->state &= ~(GW_STATUS_SHADED);
        break;

    case WM_REDRAW:
        if ((gw->flags & GW_FLAG_CUSTOM_TOOLBAR) == 0) {
            guiwin_get_grect(gw, GUIWIN_AREA_TOOLBAR, &tb_area_ro);
            tb_area = tb_area_ro;
            if(rc_intersect((GRECT*)&msg[4], &tb_area)) {
                wind_get_grect(gw->handle, WF_FIRSTXYWH, &g);
                while (g.g_h > 0 || g.g_w > 0) {
                    gw->toolbar[gw->toolbar_idx].ob_x = tb_area_ro.g_x;
                    gw->toolbar[gw->toolbar_idx].ob_width = tb_area_ro.g_w;
                    gw->toolbar[gw->toolbar_idx].ob_y = tb_area_ro.g_y;
                    gw->toolbar[gw->toolbar_idx].ob_height = tb_area_ro.g_h;
                    objc_draw(gw->toolbar, gw->toolbar_idx, 8, g.g_x, g.g_y,
                              g.g_w, g.g_h);
                    wind_get_grect(gw->handle, WF_NEXTXYWH, &g);
                }
            }
        }
        break;

    default:
        retval = 0;
        break;
    }
    return(retval);
}

short guiwin_dispatch_event(EVMULT_IN *ev_in, EVMULT_OUT *ev_out, short msg[8])
{
    GUIWIN *dest;
    short retval = 0;

    if( (ev_out->emo_events & MU_MESAG) != 0 ) {
        DEBUG_PRINT(("guiwin_handle_event_multi_fast: %d\n", msg[0]));
        switch (msg[0]) {
        case WM_REDRAW:
        case WM_CLOSED:
        case WM_TOPPED:
        case WM_ARROWED:
        case WM_HSLID:
        case WM_VSLID:
        case WM_FULLED:
        case WM_SIZED:
        case WM_REPOSED:
        case WM_MOVED:
        case WM_NEWTOP:
        case WM_UNTOPPED:
        case WM_ONTOP:
        case WM_BOTTOM:
        case WM_ICONIFY:
        case WM_UNICONIFY:
        case WM_ALLICONIFY:
        case WM_TOOLBAR:
        case AP_DRAGDROP:
        case AP_TERM:
        case AP_TFAIL:
            dest = guiwin_find(msg[3]);
            if (dest) {
                DEBUG_PRINT(("Found WM_ dest: %p (%d), flags: %d, cb: %p\n", dest, dest->handle, dest->flags, dest->handler_func));
                if (dest->flags&GW_FLAG_PREPROC_WM) {
                    retval = preproc_wm(dest, ev_out, msg);
                    if(((retval == 0)||(dest->flags&GW_FLAG_RECV_PREPROC_WM))) {
                        retval = dest->handler_func(dest, ev_out, msg);
                    }
                } else {
                    if (dest->handler_func) {
                        retval = dest->handler_func(dest, ev_out, msg);
                    }
                }

            }
            break;
        }
    } else {

        short info[4];
        wind_get( 0, WF_TOP, &info[0], &info[1], &info[2], &info[3]);

        if(info[0] != 0 && info[1] == gl_apid) {

            dest = guiwin_find(info[0]);

            if(dest == NULL || dest->handler_func == NULL)
                return(0);

            if( (ev_out->emo_events & MU_BUTTON) != 0) {
                DEBUG_PRINT(("Found MU_BUTTON dest: %p (%d), flags: %d, cb: %p\n", dest, dest->handle, dest->flags, dest->handler_func));

                // toolbar handling:
                if((dest->flags & GW_FLAG_CUSTOM_TOOLBAR) == 0 &&
                        dest->toolbar != NULL) {
                    GRECT tb_area;
                    guiwin_get_grect(dest, GUIWIN_AREA_TOOLBAR, &tb_area);
                    if (POINT_WITHIN(ev_out->emo_mouse.p_x,
                                     ev_out->emo_mouse.p_y, tb_area)) {
                        // send WM_TOOLBAR message
                        dest->toolbar[dest->toolbar_idx].ob_x = tb_area.g_x;
                        dest->toolbar[dest->toolbar_idx].ob_y = tb_area.g_y;
                        short obj_idx = objc_find(dest->toolbar,
                                                  dest->toolbar_idx, 8,
                                                  ev_out->emo_mouse.p_x,
                                                  ev_out->emo_mouse.p_y);
                        short msg_out[8] = {WM_TOOLBAR, gl_apid, 0, dest->handle,
                                            obj_idx, ev_out->emo_mclicks, ev_out->emo_kmeta, 0
                                           };
                        short oldevents = ev_out->emo_events;
                        ev_out->emo_events = MU_MESAG;
                        dest->handler_func(dest, ev_out, msg_out);
                        ev_out->emo_events = oldevents;
                        retval = 1;
                    } else {
                        dest->handler_func(dest, ev_out, msg);
                    }
                }
            } else if(ev_out->emo_events & MU_KEYBD) {
                dest->handler_func(dest, ev_out, msg);
            }
        }
    }

    return(retval);
}

short guiwin_init(void)
{
    if(v_vdi_h == -1) {
        short dummy;
        static short work_in[12] = {1,1,1,1,1,1,1,1,1,1,2,1};
        v_vdi_h=graf_handle(&dummy, &dummy, &dummy, &dummy);
        v_opnvwk(work_in, &v_vdi_h, work_out);
    }
    return(0);
}

void guiwin_exit(void)
{
    v_clsvwk(v_vdi_h);
}

GUIWIN * guiwin_add(short handle, uint32_t flags, guiwin_event_handler_f cb)
{

    GUIWIN *win = calloc(sizeof(GUIWIN),1);

    assert(win!=NULL);
    DEBUG_PRINT(("guiwin_add: %d, %p, cb: %p\n", handle, win, cb));

    win->handle = handle;
    win->handler_func = cb;
    win->flags = flags;
    if (winlist == NULL) {
        winlist = win;
        win->next = NULL;
        win->prev = NULL;
    } else {
        GUIWIN *tmp = winlist;
        while( tmp->next != NULL ) {
            tmp = tmp->next;
        }
        tmp->next = win;
        win->prev = tmp;
        win->next = NULL;
    }
    return(win);
}

GUIWIN *guiwin_find(short handle)
{
    GUIWIN *g;
    DEBUG_PRINT(("guiwin_find: handle: %d\n", handle));
    for( g = winlist; g != NULL; g=g->next ) {
        DEBUG_PRINT(("guiwin search: %d\n", g->handle));
        if(g->handle == handle) {
            DEBUG_PRINT(("guiwin_find: %p\n", g));
            return(g);
        }
    }
    return(NULL);
}

GUIWIN *guiwin_validate_ptr(GUIWIN *win)
{
    GUIWIN *g;
    for( g = winlist; g != NULL; g=g->next ) {
        DEBUG_PRINT(("guiwin guiwin_validate_ptr check: %p\n", g));
        if(g == win) {
            DEBUG_PRINT(("guiwin_validate_ptr valid: %p\n", g));
            return(g);
        }
    }
    return(NULL);
}

short guiwin_remove(GUIWIN *win)
{
    win = guiwin_validate_ptr(win);
    if (win == NULL)
        return(-1);

    /* unlink the window: */
    if(win->prev != NULL ) {
        win->prev->next = win->next;
    } else {
        winlist = win->next;
    }
    if (win->next != NULL) {
        win->next->prev = win->prev;
    }

    free(win);
    return(0);
}

void guiwin_get_grect(GUIWIN *win, enum guwin_area_e mode, GRECT *dest)
{
    wind_get_grect(win->handle, WF_WORKXYWH, dest);
    if (mode == GUIWIN_AREA_CONTENT) {
        GRECT tb_area;
        guiwin_get_grect(win, GUIWIN_AREA_TOOLBAR, &tb_area);
        if (win->flags & GW_FLAG_HAS_VTOOLBAR) {
            dest->g_x += tb_area.g_w;
            dest->g_w -= tb_area.g_w;
        } else {
            dest->g_y += tb_area.g_h;
            dest->g_h -= tb_area.g_h;
        }
    } else if (mode == GUIWIN_AREA_TOOLBAR) {
        if (win->toolbar != NULL) {
            if (win->flags & GW_FLAG_HAS_VTOOLBAR) {
                dest->g_w = win->toolbar[win->toolbar_idx].ob_width;
            } else {
                dest->g_h = win->toolbar[win->toolbar_idx].ob_height;
            }
        } else {
            dest->g_h = 0;
            dest->g_w = 0;
        }
    }
}

void guiwin_update_slider(GUIWIN *win, short mode)
{
    GRECT viewport;
    struct guiwin_scroll_info_s * slid;
    unsigned long size, pos;

    short handle = guiwin_get_handle(win);
    guiwin_get_grect(win, GUIWIN_AREA_CONTENT, &viewport);
    slid = guiwin_get_scroll_info(win);

    if((mode & 1) && (slid->y_unit_px > 0)) {
        if ( slid->y_pos_max < (long)viewport.g_h/slid->y_unit_px)
            size = 1000L;
        else
            size = MAX( 50L, (unsigned long)viewport.g_h*1000L/(unsigned long)(slid->y_unit_px*slid->y_pos_max));
        wind_set(handle, WF_VSLSIZE, (int)size, 0, 0, 0);

        if (slid->y_pos_max > (long)viewport.g_h/slid->y_unit_px) {
            pos = (unsigned long)slid->y_pos *1000L/(unsigned long)(slid->y_pos_max-viewport.g_h/slid->y_unit_px);
            wind_set(handle, WF_VSLIDE, (int)pos, 0, 0, 0);
        } else if (slid->y_pos) {
            slid->y_pos = 0;
            wind_set(handle, WF_VSLIDE, 0, 0, 0, 0);
        }
    }
    if((mode & 2) && (slid->x_unit_px > 0)) {
        if ( slid->x_pos_max < (long)viewport.g_w/slid->x_unit_px)
            size = 1000L;
        else
            size = MAX( 50L, (unsigned long)viewport.g_w*1000L/(unsigned long)(slid->x_unit_px*slid->x_pos_max));
        wind_set(handle, WF_HSLSIZE, (int)size, 0, 0, 0);

        if( slid->x_pos_max > (long)viewport.g_w/slid->x_unit_px) {
            pos = (unsigned long)slid->x_pos*1000L/(unsigned long)(slid->x_pos_max-viewport.g_w/slid->x_unit_px);
            wind_set(handle, WF_HSLIDE, (int)pos, 0, 0, 0);
        } else if (slid->x_pos) {
            slid->x_pos = 0;
            wind_set(handle, WF_HSLIDE, 0, 0, 0, 0);
        }
    }
}

short guiwin_get_handle(GUIWIN *win)
{
    return(win->handle);
}

uint32_t guiwin_get_state(GUIWIN *win)
{
    return(win->state);
}

void guiwin_set_event_handler(GUIWIN *win,guiwin_event_handler_f cb)
{
    win->handler_func = cb;
}

void guiwin_set_toolbar(GUIWIN *win, OBJECT *toolbar, short idx, uint32_t flags)
{
    win->toolbar = toolbar;
    win->toolbar_idx = idx;
    if(flags & GW_FLAG_HAS_VTOOLBAR) {
        win->flags |= GW_FLAG_HAS_VTOOLBAR;
    }
}

void guiwin_set_user_data(GUIWIN *win, void *data)
{
    win->user_data = data;
}

void *guiwin_get_user_data(GUIWIN *win)
{
    return(win->user_data);
}

struct guiwin_scroll_info_s *guiwin_get_scroll_info(GUIWIN *win)
{
    return(&win->scroll_info);
}

void guiwin_send_redraw(GUIWIN *win, GRECT *area)
{
	short msg[8];
	GRECT work;

	if(area == NULL){
		guiwin_get_grect(win, GUIWIN_AREA_WORK, &work);
		area = &work;
	}

	msg[0] = WM_REDRAW;
	msg[1] = gl_apid;
	msg[2] = 0;
	msg[3] = win->handle;
	msg[4] = area->g_x;
	msg[5] = area->g_y;
	msg[6] = area->g_w;
	msg[7] = area->g_h;

	appl_write(gl_apid, 16, &msg);
}
/*
void guiwin_exec_redraw(){

}
*/



