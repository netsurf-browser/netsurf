//#include "global.h"

#include <stdint.h>
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
	struct gui_window_s *next, *prev;
};

static GUIWIN * winlist;

static short preproc_wm(GUIWIN * gw, EVMULT_OUT *ev_out, short msg[8])
{
    GRECT g, tb_area, tb_area_ro;
    short retval = 1;

    switch(msg[0]) {

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
		wind_get_grect(0, WF_WORKXYWH, &g);
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
			if(rc_intersect((GRECT*)&msg[4], &tb_area)){
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
                }
                else {
					if (dest->handler_func) {
						retval = dest->handler_func(dest, ev_out, msg);
					}
                }

            }
            break;
        }
    }
    if( (ev_out->emo_events & MU_BUTTON) != 0){
    	short info[4];

    	wind_get( 0, WF_TOP, &info[0], &info[1], &info[2], &info[3]);
    	if(info[0] != 0 && info[1] == gl_apid){
			dest = guiwin_find(info[0]);
			if (dest) {
                DEBUG_PRINT(("Found MU_BUTTON dest: %p (%d), flags: %d, cb: %p\n", dest, dest->handle, dest->flags, dest->handler_func));

				// toolbar handling:
				if((dest->flags & GW_FLAG_CUSTOM_TOOLBAR) == 0 &&
					dest->toolbar != NULL && dest->handler_func != NULL){
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
							obj_idx, ev_out->emo_mclicks, ev_out->emo_kmeta, 0};
						short oldevents = ev_out->emo_events;
						ev_out->emo_events = MU_MESAG;
						dest->handler_func(dest, ev_out, msg_out);
						ev_out->emo_events = oldevents;
						retval = 1;
					}
				}
            }
    	}
    }
    return(retval);
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
	}
	else if (mode == GUIWIN_AREA_TOOLBAR) {
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

short guiwin_get_handle(GUIWIN *win)
{
	return(win->handle);
}

uint32_t guiwin_get_state(GUIWIN *win)
{
	return(win->state);
}

void guiwin_set_toolbar(GUIWIN *win, OBJECT *toolbar, short idx, uint32_t flags)
{
	win->toolbar = toolbar;
	win->toolbar_idx = idx;
	if(flags & GW_FLAG_HAS_VTOOLBAR){
		win->flags |= GW_FLAG_HAS_VTOOLBAR;
	}
}



