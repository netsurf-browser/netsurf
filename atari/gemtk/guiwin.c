/*
 * Copyright 2012 Ole Loots <ole@monochrom.net>
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

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <gem.h>
#include <gemx.h>
#include <cflib.h>

#include "gemtk.h"

//#define DEBUG_PRINT(x)		printf x
#define DEBUG_PRINT(x)

struct gui_window_s {
    short handle;
    guiwin_event_handler_f handler_func;
    uint32_t flags;
    uint32_t state;
    OBJECT *toolbar;
    short toolbar_edit_obj;
    short toolbar_idx;
    GRECT toolbar_dim;
    OBJECT *form;
    short form_edit_obj;
    short form_focus_obj;
    short form_idx;
    struct guiwin_scroll_info_s scroll_info;
    void *user_data;
    struct gui_window_s *next, *prev;
};

static GUIWIN * winlist;
static VdiHdl v_vdi_h = -1;
static short work_out[57];

static void move_rect(GUIWIN * win, GRECT *rect, int dx, int dy)
{
    INT16 xy[ 8];
    long dum = 0L;
    GRECT g;

    VdiHdl vh = guiwin_get_vdi_handle(win);

    while(!wind_update(BEG_UPDATE));
    graf_mouse(M_OFF, 0L);

    /* get intersection with screen area */
    wind_get_grect(0, WF_CURRXYWH, &g);
    rc_intersect(&g, rect);
    xy[0] = rect->g_x;
    xy[1] = rect->g_y;
    xy[2] = xy[0] + rect->g_w-1;
    xy[3] = xy[1] + rect->g_h-1;
    xy[4] = xy[0] + dx;
    xy[5] = xy[1] + dy;
    xy[6] = xy[2] + dx;
    xy[7] = xy[3] + dy;
    vro_cpyfm(vh, S_ONLY, xy, (MFDB *)&dum, (MFDB *)&dum);

    graf_mouse(M_ON, 0L);
    wind_update(END_UPDATE);
}

/**
* Handles common events.
* returns 0 when the event was not handled, 1 otherwise.
*/
static short preproc_wm(GUIWIN * gw, EVMULT_OUT *ev_out, short msg[8])
{
    GRECT g, g_ro, g2;
    short retval = 1;
    int val = 1, old_val;
    struct guiwin_scroll_info_s *slid;

    switch(msg[0]) {

    case WM_HSLID:
        guiwin_get_grect(gw, GUIWIN_AREA_CONTENT, &g);
        wind_set(gw->handle, WF_HSLIDE, msg[4], 0, 0, 0);
        slid = guiwin_get_scroll_info(gw);
        val = (float)(slid->x_units-(g.g_w/slid->x_unit_px))/1000*(float)msg[4];
        if(val != slid->x_pos) {
            if (val < slid->x_pos) {
                val = -(MAX(0, slid->x_pos-val));
            } else {
                val = val-slid->x_pos;
            }
            guiwin_scroll(gw, GUIWIN_HSLIDER, val, false);
        }
        break;

    case WM_VSLID:
        guiwin_get_grect(gw, GUIWIN_AREA_CONTENT, &g);
        wind_set(gw->handle, WF_VSLIDE, msg[4], 0, 0, 0);
        slid = guiwin_get_scroll_info(gw);
        val = (float)(slid->y_units-(g.g_h/slid->y_unit_px))/1000*(float)msg[4];
        if(val != slid->y_pos) {
            if (val < slid->y_pos) {
                val = -(slid->y_pos - val);
            } else {
                val = val -slid->y_pos;
            }
            guiwin_scroll(gw, GUIWIN_VSLIDER, val, false);
        }
        break;

    case WM_ARROWED:
        if((gw->flags & GW_FLAG_CUSTOM_SCROLLING) == 0) {

            slid = guiwin_get_scroll_info(gw);
            guiwin_get_grect(gw, GUIWIN_AREA_CONTENT, &g);
            g_ro = g;

            switch(msg[4]) {

            case WA_UPPAGE:
                /* scroll page up */
                guiwin_scroll(gw, GUIWIN_VSLIDER, -(g.g_h/slid->y_unit_px),
                              true);
                break;

            case WA_UPLINE:
                /* scroll line up */
                guiwin_scroll(gw, GUIWIN_VSLIDER, -1, true);
                break;

            case WA_DNPAGE:
                /* scroll page down */
                guiwin_scroll(gw, GUIWIN_VSLIDER, g.g_h/slid->y_unit_px,
                              true);
                break;

            case WA_DNLINE:
                /* scroll line down */
                guiwin_scroll(gw, GUIWIN_VSLIDER, +1, true);
                break;

            case WA_LFPAGE:
                /* scroll page left */
                guiwin_scroll(gw, GUIWIN_HSLIDER, -(g.g_w/slid->x_unit_px),
                              true);
                break;

            case WA_LFLINE:
                /* scroll line left */
                guiwin_scroll(gw, GUIWIN_HSLIDER, -1,
                              true);
                break;

            case WA_RTPAGE:
                /* scroll page right */
                guiwin_scroll(gw, GUIWIN_HSLIDER, (g.g_w/slid->x_unit_px),
                              true);
                break;

            case WA_RTLINE:
                /* scroll line right */
                guiwin_scroll(gw, GUIWIN_HSLIDER, 1,
                              true);
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

        if (gw->form) {

			guiwin_get_grect(gw, GUIWIN_AREA_CONTENT, &g);
			slid = guiwin_get_scroll_info(gw);

			gw->form[gw->form_idx].ob_x = g.g_x -
					(slid->x_pos * slid->x_unit_px);

			gw->form[gw->form_idx].ob_y = g.g_y -
					(slid->y_pos * slid->y_unit_px);
        }

        break;

    case WM_SIZED:
    case WM_REPOSED:
        wind_get_grect(gw->handle, WF_FULLXYWH, &g2);
        wind_get_grect(gw->handle, WF_CURRXYWH, &g);
        g.g_w = MIN(msg[6], g2.g_w);
        g.g_h = MIN(msg[7], g2.g_h);
        if(g2.g_w != g.g_w || g2.g_h != g.g_h) {
            wind_set(gw->handle, WF_CURRXYWH, g.g_x, g.g_y, g.g_w, g.g_h);
            if((gw->flags & GW_FLAG_CUSTOM_SCROLLING) == 0) {
                if(guiwin_update_slider(gw, GUIWIN_VH_SLIDER)) {
                    guiwin_send_redraw(gw, NULL);
                }
            }
        }


        break;

    case WM_FULLED:
        wind_get_grect(DESKTOP_HANDLE, WF_WORKXYWH, &g);
        wind_get_grect(gw->handle, WF_CURRXYWH, &g2);
        if(g.g_w == g2.g_w && g.g_h == g2.g_h) {
            wind_get_grect(gw->handle, WF_PREVXYWH, &g);
        }
        wind_set_grect(gw->handle, WF_CURRXYWH, &g);
        if((gw->flags & GW_FLAG_CUSTOM_SCROLLING) == 0) {
            if(guiwin_update_slider(gw, GUIWIN_VH_SLIDER)) {
                guiwin_send_redraw(gw, NULL);
            }
        }
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
        if ((gw->flags & GW_FLAG_TOOLBAR_REDRAW)
                && (gw->flags & GW_FLAG_CUSTOM_TOOLBAR) == 0) {
            g.g_x = msg[4];
            g.g_y = msg[5];
            g.g_w = msg[6];
            g.g_h = msg[7];
            guiwin_toolbar_redraw(gw, &g);
        }
        if (gw->form != NULL) {
			g.g_x = msg[4];
            g.g_y = msg[5];
            g.g_w = msg[6];
            g.g_h = msg[7];
			guiwin_form_redraw(gw, &g);
        }
        break;

    default:
        retval = 0;
        break;

    }

    return(retval);
}

/**
* Preprocess mouse events
*/
static short preproc_mu_button(GUIWIN * gw, EVMULT_OUT *ev_out, short msg[8])
{
    short retval = 0;

    DEBUG_PRINT(("preproc_mu_button\n"));

    // toolbar handling:
    if ((gw->flags & GW_FLAG_CUSTOM_TOOLBAR) == 0
            && gw->toolbar != NULL) {

        GRECT tb_area;

        guiwin_get_grect(gw, GUIWIN_AREA_TOOLBAR, &tb_area);

        if (POINT_WITHIN(ev_out->emo_mouse.p_x,
                         ev_out->emo_mouse.p_y, tb_area)) {
            // send WM_TOOLBAR message
            gw->toolbar[gw->toolbar_idx].ob_x = tb_area.g_x;
            gw->toolbar[gw->toolbar_idx].ob_y = tb_area.g_y;
            short obj_idx = objc_find(gw->toolbar,
                                      gw->toolbar_idx, 8,
                                      ev_out->emo_mouse.p_x,
                                      ev_out->emo_mouse.p_y);

            DEBUG_PRINT(("Toolbar index: %d\n", obj_idx));
            if (obj_idx > 0) {
                if ((gw->toolbar[obj_idx].ob_flags & OF_SELECTABLE)!=0
                        && ((gw->flags & GW_FLAG_CUSTOM_TOOLBAR) == 0)
                        && ((gw->flags & GW_FLAG_TOOLBAR_REDRAW) == 1)) {
                    gw->toolbar[obj_idx].ob_state |= OS_SELECTED;
                    // TODO: optimize redraw by setting the object clip:
                    guiwin_toolbar_redraw(gw, NULL);
                }
            }

            short oldevents = ev_out->emo_events;
            short msg_out[8] = {WM_TOOLBAR, gl_apid,
                                0, gw->handle,
                                obj_idx, ev_out->emo_mclicks,
                                ev_out->emo_kmeta, ev_out->emo_mbutton
                               };
            ev_out->emo_events = MU_MESAG;
            // notify the window about toolbar click:
            gw->handler_func(gw, ev_out, msg_out);
            ev_out->emo_events = oldevents;
            retval = 1;
        }
    }

    if (gw->form != NULL) {

        GRECT content_area;
        struct guiwin_scroll_info_s *slid;

        DEBUG_PRINT(("preproc_mu_button: handling form click.\n"));

        guiwin_get_grect(gw, GUIWIN_AREA_CONTENT, &content_area);

        if (POINT_WITHIN(ev_out->emo_mouse.p_x,
                         ev_out->emo_mouse.p_y, content_area)) {

            slid = guiwin_get_scroll_info(gw);

			// adjust form position (considering window and scroll position):
            gw->form[gw->form_idx].ob_x = content_area.g_x -
                                          (slid->x_pos * slid->x_unit_px);
            gw->form[gw->form_idx].ob_y = content_area.g_y -
                                          (slid->y_pos * slid->y_unit_px);

            gw->form_focus_obj = objc_find(gw->form, gw->form_idx, 8,
                                           ev_out->emo_mouse.p_x, ev_out->emo_mouse.p_y);

			DEBUG_PRINT(("Window Form click, obj: %d\n", gw->form_focus_obj));
            if (gw->form_focus_obj > -1
                    && (gw->form[gw->form_focus_obj].ob_state & OS_DISABLED)== 0) {

                uint16_t type = (gw->form[gw->form_focus_obj].ob_type & 0xFF);
                uint16_t xtype = (gw->form[gw->form_focus_obj].ob_type & 0xFF00);
                uint16_t nextobj, edit_idx;

                DEBUG_PRINT(("type: %d, xtype: %d\n", type, xtype));

                if (type == G_FTEXT || type == G_FBOXTEXT)  {

                	// edit field handling, this causes ugly redraws when the
                	// form is scrolled and larger than the window in which
                	// it is attached.

                    // report mouse click to the tree:
                    retval = form_wbutton(gw->form, gw->form_focus_obj,
                                      ev_out->emo_mclicks, &nextobj,
                                      gw->handle);

                    // end edit mode for active edit object:
                    if(gw->form_edit_obj != -1) {
                        objc_wedit(gw->form, gw->form_edit_obj,
                                  ev_out->emo_kreturn, &edit_idx,
                                  EDEND, gw->handle);
                    }

                    // activate the new edit object:
                    gw->form_edit_obj = gw->form_focus_obj;
                    objc_wedit(gw->form, gw->form_edit_obj,
                              ev_out->emo_kreturn, &edit_idx,
                              EDINIT, gw->handle);

                } else {

                    // end edit mode for active edit object:
                    if(gw->form_edit_obj != -1) {
                        objc_wedit(gw->form, gw->form_edit_obj,
                                  ev_out->emo_kreturn, &edit_idx,
                                  EDEND, gw->handle);
                        gw->form_edit_obj = -1;
                    }

                    if ((xtype & GW_XTYPE_CHECKBOX) != 0) {

                        if ((gw->form[gw->form_focus_obj].ob_state & OS_SELECTED) != 0) {
                            gw->form[gw->form_focus_obj].ob_state &= ~(OS_SELECTED|OS_CROSSED);
                        } else {
                            gw->form[gw->form_focus_obj].ob_state |= (OS_SELECTED|OS_CROSSED);
                        }
						guiwin_form_redraw(gw, obj_screen_rect(gw->form,
                                                           gw->form_focus_obj));
                    }
                    short oldevents = ev_out->emo_events;
                    short msg_out[8] = {GUIWIN_WM_FORM, gl_apid,
                                        0, gw->handle,
                                        gw->form_focus_obj, ev_out->emo_mclicks,
                                        ev_out->emo_kmeta, 0
                                       };
                    ev_out->emo_events = MU_MESAG;
                    // notify the window about form click:
                    gw->handler_func(gw, ev_out, msg_out);
                    ev_out->emo_events = oldevents;
                    retval = 1;
                    evnt_timer(150);
                }
            }
        }
    }

    return(retval);
}

/**
* Preprocess keyboard events (for forms/toolbars)
*/
static short preproc_mu_keybd(GUIWIN * gw, EVMULT_OUT *ev_out, short msg[8])
{

    if((gw->form != NULL) && (gw->form_edit_obj > -1) ) {

        short next_edit_obj = gw->form_edit_obj;
        short next_char = -1;
        short edit_idx;
        short r;

        r = form_wkeybd(gw->form, gw->form_edit_obj, next_edit_obj,
                       ev_out->emo_kreturn,
                       &next_edit_obj, &next_char, gw->handle);

        if (next_edit_obj != gw->form_edit_obj) {

			if(gw->form_edit_obj != -1) {
				objc_wedit(gw->form, gw->form_edit_obj,
                      ev_out->emo_kreturn, &edit_idx,
                      EDEND, gw->handle);
			}

            gw->form_edit_obj = next_edit_obj;

            objc_wedit(gw->form, gw->form_edit_obj,
                      ev_out->emo_kreturn, &edit_idx,
                      EDINIT, gw->handle);
        } else {
            if(next_char > 13)
                r = objc_wedit(gw->form, gw->form_edit_obj,
                              ev_out->emo_kreturn, &edit_idx,
                              EDCHAR, gw->handle);
        }
    }
}

/**
* Event Dispatcher function. The guiwin API doesn't own an event loop,
* so you have to inform it for every event that you want it to handle.
*/
short guiwin_dispatch_event(EVMULT_IN *ev_in, EVMULT_OUT *ev_out, short msg[8])
{
    GUIWIN *dest;
    short retval = 0;
    bool handler_called = false;

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
                        handler_called = true;
                    }
                } else {
                    if (dest->handler_func) {
                        retval = dest->handler_func(dest, ev_out, msg);
                        handler_called = true;
                    }
                }

            }
            break;
        }
    } else {

        short h_aes;
        h_aes = wind_find(ev_out->emo_mouse.p_x, ev_out->emo_mouse.p_y);
        if(h_aes > 0 && (ev_out->emo_events != MU_TIMER))  {

            dest = guiwin_find(h_aes);

            if (dest == NULL || dest->handler_func == NULL)
                return(0);

            if ((ev_out->emo_events & MU_BUTTON) != 0) {

                DEBUG_PRINT(("Found MU_BUTTON dest: %p (%d), flags: %d, cb: %p\n", dest, dest->handle, dest->flags, dest->handler_func));
                retval = preproc_mu_button(dest, ev_out, msg);
                if(retval != 0) {
                    handler_called = true;
                }
            }

            if ((ev_out->emo_events & MU_KEYBD)) {
                retval = preproc_mu_keybd(dest, ev_out, msg);
            }

            if (handler_called==false) {
                dest->handler_func(dest, ev_out, msg);
            }
        }
    }

    return(retval);
}

/**
*	Initialises the guiwin API
*/
short guiwin_init(void)
{
    if(v_vdi_h == -1) {
        short dummy;
        short work_in[12] = {Getrez()+2,1,1,1,1,1,1,1,1,1,2,1};
        v_vdi_h=graf_handle(&dummy, &dummy, &dummy, &dummy);
        v_opnvwk(work_in, &v_vdi_h, work_out);
    }
    return(0);
}

void guiwin_exit(void)
{
    v_clsvwk(v_vdi_h);
}

/**
* Adds and AES handle to the guiwin list and creates and GUIWIN management
* structure.
*
* \param handle The AES handle
* \param flags Creation flags, configures how the AES window is handled
* \param cb event handler function for that window
*/
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

    DEBUG_PRINT(("Added guiwin: %p, tb: %p\n", win, win->toolbar));
    return(win);
}

/**
* Returns an GUIWIN* for AES handle, when that AES window is managed by guiwin
*/
GUIWIN *guiwin_find(short handle)
{
    GUIWIN *g;
    DEBUG_PRINT(("guiwin search handle: %d\n", handle));
    for( g = winlist; g != NULL; g=g->next ) {
        if(g->handle == handle) {
            DEBUG_PRINT(("guiwin found handle: %p\n", g));
            return(g);
        }
    }
    return(NULL);
}


/**
* Check's if the pointer is managed by the guiwin API.
*/
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

/**
* Remove an GUIWIN from the list of managed windows.
* Call this when the AES window is closed or deleted.
*/
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
    DEBUG_PRINT(("guiwin free: %p\n", win));
    free(win);
    return(0);
}

/** Calculate & get a well known area within the GUIWIN.
* \param win The GUIWIN ptr.
* \param mode Specifies the area to retrieve.
* \param dest The calculated rectangle.
*/
void guiwin_get_grect(GUIWIN *win, enum guwin_area_e mode, GRECT *dest)
{

    assert(win != NULL);

    wind_get_grect(win->handle, WF_WORKXYWH, dest);
    if (mode == GUIWIN_AREA_CONTENT) {
        GRECT tb_area;
        guiwin_get_grect(win, GUIWIN_AREA_TOOLBAR, &tb_area);
        if (win->flags & GW_FLAG_HAS_VTOOLBAR) {
            dest->g_x += tb_area.g_w;
            dest->g_w -= tb_area.g_w;
        }
        else {
			dest->g_y += tb_area.g_h;
            dest->g_h -= tb_area.g_h;
        }
    } else if (mode == GUIWIN_AREA_TOOLBAR) {
    	if (win->toolbar) {
			if (win->flags & GW_FLAG_HAS_VTOOLBAR) {
			   dest->g_w = win->toolbar[win->toolbar_idx].ob_width;
			} else {
				dest->g_h = win->toolbar[win->toolbar_idx].ob_height;
			}
    	}
    	else {
			dest->g_w = 0;
			dest->g_h = 0;
    	}
    }
}


/**
*	Scroll the content area (GUIWIN_AREA_CONTENT) in the specified dimension
*	(GUIWIN_VSLIDER, GUIWIN_HSLIDER)
*	\param win The GUIWIN
* 	\param orientation GUIWIN_VSLIDER or GUIWIN_HSLIDER
* 	\param units the amout to scroll (pass negative values to scroll up)
*	\param refresh Sliders will be updated when this flag is set
*/
void guiwin_scroll(GUIWIN *win, short orientation, int units, bool refresh)
{
    struct guiwin_scroll_info_s *slid = guiwin_get_scroll_info(win);
    int oldpos = 0, newpos = 0, vis_units=0, pix = 0;
    int abs_pix = 0;
    GRECT *redraw=NULL, g, g_ro;

    guiwin_get_grect(win, GUIWIN_AREA_CONTENT, &g);
    g_ro = g;

    if (orientation == GUIWIN_VSLIDER) {
        pix = units*slid->y_unit_px;
        abs_pix = abs(pix);
        oldpos = slid->y_pos;
        vis_units = g.g_h/slid->y_unit_px;
        newpos = slid->y_pos = MIN(slid->y_units-vis_units,
                                   MAX(0, slid->y_pos+units));
        if(newpos < 0) {
            newpos = slid->y_pos = 0;
        }
        if(oldpos == newpos)
            return;

        if (units>=vis_units || guiwin_has_intersection(win, &g_ro)) {
            // send complete redraw
            redraw = &g_ro;
        } else {
            // only adjust ypos when scrolling down:
            if(pix < 0 ) {
                // blit screen area:
                g.g_h -= abs_pix;
                move_rect(win, &g, 0, abs_pix);
                g.g_y = g_ro.g_y;
                g.g_h = abs_pix;
                redraw = &g;
            } else {
                // blit screen area:
                g.g_y += abs_pix;
                g.g_h -= abs_pix;
                move_rect(win, &g, 0, -abs_pix);
                g.g_y = g_ro.g_y + g_ro.g_h - abs_pix;
                g.g_h = abs_pix;
                redraw = &g;
            }
        }
    } else {
        pix = units*slid->x_unit_px;
        abs_pix = abs(pix);
        oldpos = slid->x_pos;
        vis_units = g.g_w/slid->x_unit_px;
        newpos = slid->x_pos = MIN(slid->x_units-vis_units,
                                   MAX(0, slid->x_pos+units));

        if(newpos < 0) {
            newpos = slid->x_pos = 0;
        }

        if(oldpos == newpos)
            return;
        if (units>=vis_units || guiwin_has_intersection(win, &g_ro)) {
            // send complete redraw
            redraw = &g_ro;
        } else {
            // only adjust ypos when scrolling down:
            if(pix < 0 ) {
                // blit screen area:
                g.g_w -= abs_pix;
                move_rect(win, &g, abs_pix, 0);
                g.g_x = g_ro.g_x;
                g.g_w = abs_pix;
                redraw = &g;
            } else {
                // blit screen area:
                g.g_x += abs_pix;
                g.g_w -= abs_pix;
                move_rect(win, &g, -abs_pix, 0);
                g.g_x = g_ro.g_x + g_ro.g_w - abs_pix;
                g.g_w = abs_pix;
                redraw = &g;
            }
        }
    }

    if (refresh) {
        guiwin_update_slider(win, orientation);
    }

    if ((redraw != NULL) && (redraw->g_h > 0)) {
        guiwin_send_redraw(win, redraw);
    }
}

/**
* Refresh the sliders of the window.
* \param win the GUIWIN
* \param mode bitmask, valid bits: GUIWIN_VSLIDER, GUIWIN_HSLIDER
*/
bool guiwin_update_slider(GUIWIN *win, short mode)
{
    GRECT viewport;
    struct guiwin_scroll_info_s * slid;
    unsigned long size, pos;
    int old_x, old_y;

    short handle = guiwin_get_handle(win);
    guiwin_get_grect(win, GUIWIN_AREA_CONTENT, &viewport);
    slid = guiwin_get_scroll_info(win);

    old_x = slid->x_pos;
    old_y = slid->y_pos;

    // TODO: check if the window has sliders of that direction...?

    if((mode & GUIWIN_VSLIDER) && (slid->y_unit_px > 0)) {
        if ( slid->y_units < (long)viewport.g_h/slid->y_unit_px) {
            size = 1000L;
        } else
            size = MAX( 50L, (unsigned long)viewport.g_h*1000L/
                        (unsigned long)(slid->y_unit_px*slid->y_units));
        wind_set(handle, WF_VSLSIZE, (int)size, 0, 0, 0);

        if (slid->y_units > (long)viewport.g_h/slid->y_unit_px) {
            pos = (unsigned long)slid->y_pos *1000L/
                  (unsigned long)(slid->y_units-viewport.g_h/slid->y_unit_px);
            wind_set(handle, WF_VSLIDE, (int)pos, 0, 0, 0);
        } else if (slid->y_pos) {
            slid->y_pos = 0;
            wind_set(handle, WF_VSLIDE, 0, 0, 0, 0);
        }
    }
    if((mode & GUIWIN_HSLIDER) && (slid->x_unit_px > 0)) {
        if ( slid->x_units < (long)viewport.g_w/slid->x_unit_px)
            size = 1000L;
        else
            size = MAX( 50L, (unsigned long)viewport.g_w*1000L/
                        (unsigned long)(slid->x_unit_px*slid->x_units));
        wind_set(handle, WF_HSLSIZE, (int)size, 0, 0, 0);

        if( slid->x_units > (long)viewport.g_w/slid->x_unit_px) {
            pos = (unsigned long)slid->x_pos*1000L/
                  (unsigned long)(slid->x_units-viewport.g_w/slid->x_unit_px);
            wind_set(handle, WF_HSLIDE, (int)pos, 0, 0, 0);
        } else if (slid->x_pos) {
            slid->x_pos = 0;
            wind_set(handle, WF_HSLIDE, 0, 0, 0, 0);
        }
    }

    if(old_x != slid->x_pos || old_y != slid->y_pos) {
        return(true);
    }
    return(false);
}

/**
* Return the AES handle for the GUIWIN.
*/
short guiwin_get_handle(GUIWIN *win)
{
    return(win->handle);
}

/**
* Return the VDI handle for an GUIWIN.
*/
VdiHdl guiwin_get_vdi_handle(GUIWIN *win)
{
    return(v_vdi_h);
}

/**
* Returns the state bitmask of the window
*/
uint32_t guiwin_get_state(GUIWIN *win)
{
    return(win->state);
}


/**
* Set and new event handler function.
*/
void guiwin_set_event_handler(GUIWIN *win,guiwin_event_handler_f cb)
{
    win->handler_func = cb;
}

/**
* Configure the window so that it shows an toolbar or at least reserves
* an area to draw an toolbar.
* \param win The GUIWIN
* \param toolbar the AES form
* \param idx index within the toolbar tree (0 in most cases...)
* \param flags optional configuration flags
*/
//TODO: document flags
void guiwin_set_toolbar(GUIWIN *win, OBJECT *toolbar, short idx, uint32_t flags)
{
    win->toolbar = toolbar;
    win->toolbar_idx = idx;
    win->toolbar_edit_obj = -1;
    if(flags & GW_FLAG_HAS_VTOOLBAR) {
        win->flags |= GW_FLAG_HAS_VTOOLBAR;
    }
}

/**
* Attach an arbitary pointer to the GUIWIN
*/
void guiwin_set_user_data(GUIWIN *win, void *data)
{
    win->user_data = data;
}

/**
* Retrieve the user_data pointer attached to the GUIWIN.
*/
void *guiwin_get_user_data(GUIWIN *win)
{
    return(win->user_data);
}

/** Get the scroll management structure for a GUIWIN
*/
struct guiwin_scroll_info_s *guiwin_get_scroll_info(GUIWIN *win) {
    return(&win->scroll_info);
}

/**
*	Get the amount of content dimensions within the window
* 	which is calculated by using the scroll_info attached to the GUIWIN.
*/
void guiwin_set_scroll_grid(GUIWIN * win, short x, short y)
{
    struct guiwin_scroll_info_s *slid = guiwin_get_scroll_info(win);

    assert(slid != NULL);

    slid->y_unit_px = x;
    slid->x_unit_px = y;
}


/** Set the size of the content measured in content units
* \param win the GUIWIN
* \param x horizontal size
* \param y vertical size
*/
void guiwin_set_content_units(GUIWIN * win, short x, short y)
{
    struct guiwin_scroll_info_s *slid = guiwin_get_scroll_info(win);

    assert(slid != NULL);

    slid->x_units = x;
    slid->y_units = y;
}

/** Send an Message to a GUIWIN using AES message pipe
* \param win the GUIWIN which shall receive the message
* \param msg_type the WM_ message definition
* \param a the 4th parameter to appl_write
* \param b the 5th parameter to appl_write
* \param c the 6th parameter to appl_write
* \param d the 7th parameter to appl_write
*/
void guiwin_send_msg(GUIWIN *win, short msg_type, short a, short b, short c,
                     short d)
{
    short msg[8];

    msg[0] = msg_type;
    msg[1] = gl_apid;
    msg[2] = 0;
    msg[3] = win->handle;
    msg[4] = a;
    msg[5] = b;
    msg[6] = c;
    msg[7] = d;

    appl_write(gl_apid, 16, &msg);
}

// TODO: rename, document and implement alternative (guiwin_exec_event)
void guiwin_send_redraw(GUIWIN *win, GRECT *area)
{
    short msg[8], retval;
    GRECT work;

    EVMULT_IN event_in = {
        .emi_flags = MU_MESAG | MU_TIMER | MU_KEYBD | MU_BUTTON,
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
    EVMULT_OUT event_out;

    if (area == NULL) {
        guiwin_get_grect(win, GUIWIN_AREA_WORK, &work);
        if (work.g_w < 1 || work.g_h < 1) {
            if (win->toolbar != NULL) {
                guiwin_get_grect(win, GUIWIN_AREA_TOOLBAR, &work);
                if (work.g_w < 1 || work.g_h < 1) {
                    return;
                }
            }
        }
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

    event_out.emo_events = MU_MESAG;
    retval = preproc_wm(win, &event_out, msg);
    if (retval == 0 || (win->flags & GW_FLAG_PREPROC_WM) != 0){
		win->handler_func(win, &event_out, msg);
    }



    //appl_write(gl_apid, 16, &msg);
}

/** Attach an AES FORM to the GUIWIN, similar feature like the toolbar
*/
void guiwin_set_form(GUIWIN *win, OBJECT *tree, short index)
{
	DEBUG_PRINT(("Setting form %p (%d) for window %p\n", tree, index, win));
    win->form = tree;
    win->form_edit_obj = -1;
    win->form_focus_obj = -1;
    win->form_idx = index;
}

/** Checks if a GUIWIN is overlapped by other windows.
*/
bool guiwin_has_intersection(GUIWIN *win, GRECT *work)
{
    GRECT area, mywork;
    bool retval = true;

    if (work == NULL) {
        guiwin_get_grect(win, GUIWIN_AREA_CONTENT, &mywork);
        work = &mywork;
    }

    wind_get_grect(win->handle, WF_FIRSTXYWH, &area);
    while (area.g_w && area.g_w) {
        //GRECT * ptr = &area;
        if (RC_WITHIN(work, &area)) {
            retval = false;
        }
        wind_get_grect(win->handle, WF_NEXTXYWH, &area);
    }

    return(retval);
}

/** Execute an toolbar redraw
*/
void guiwin_toolbar_redraw(GUIWIN *gw, GRECT *clip)
{
    GRECT tb_area, tb_area_ro, g;

    guiwin_get_grect(gw, GUIWIN_AREA_TOOLBAR, &tb_area_ro);

    if(clip == NULL) {
        clip = &tb_area_ro;
    }

    tb_area = tb_area_ro;

    if(rc_intersect(clip, &tb_area)) {

        // Update object position:
        gw->toolbar[gw->toolbar_idx].ob_x = tb_area_ro.g_x;
        gw->toolbar[gw->toolbar_idx].ob_width = tb_area_ro.g_w;
        gw->toolbar[gw->toolbar_idx].ob_y = tb_area_ro.g_y;
        gw->toolbar[gw->toolbar_idx].ob_height = tb_area_ro.g_h;

        wind_get_grect(gw->handle, WF_FIRSTXYWH, &g);
        while (g.g_h > 0 || g.g_w > 0) {
            if(rc_intersect(&tb_area, &g)) {
                objc_draw(gw->toolbar, gw->toolbar_idx, 8, g.g_x, g.g_y,
                          g.g_w, g.g_h);

            }
            wind_get_grect(gw->handle, WF_NEXTXYWH, &g);
        }
    }
}

/** Execute FORM redraw
*/
void guiwin_form_redraw(GUIWIN *gw, GRECT *clip)
{
    GRECT area, area_ro, g;
	int scroll_px_x, scroll_px_y;
	struct guiwin_scroll_info_s *slid;
	//int new_x, new_y, old_x, old_y;
	short edit_idx;

	DEBUG_PRINT(("guiwin_form_redraw\n"));

	// calculate form coordinates, include scrolling:
    guiwin_get_grect(gw, GUIWIN_AREA_CONTENT, &area_ro);
	slid = guiwin_get_scroll_info(gw);

	// Update form position:
	gw->form[gw->form_idx].ob_x = area_ro.g_x - (slid->x_pos * slid->x_unit_px);
	gw->form[gw->form_idx].ob_y = area_ro.g_y - (slid->y_pos * slid->y_unit_px);

    if(clip == NULL) {
        clip = &area_ro;
    }

    area = area_ro;

	/* Walk the AES rectangle list and redraw the visible areas of the window:*/
    if(rc_intersect(clip, &area)) {

        wind_get_grect(gw->handle, WF_FIRSTXYWH, &g);
        while (g.g_h > 0 || g.g_w > 0) {
            if(rc_intersect(&area, &g)) {
                objc_draw(gw->form, gw->form_idx, 8, g.g_x, g.g_y,
                          g.g_w, g.g_h);

            }
            wind_get_grect(gw->handle, WF_NEXTXYWH, &g);
        }
    }
}


/** Fill the content area with white color
*/
void guiwin_clear(GUIWIN *win)
{
    GRECT area, g;
    short pxy[4];
    VdiHdl vh;

    vh = guiwin_get_vdi_handle(win);

    if(win->state & GW_STATUS_ICONIFIED) {
        // also clear the toolbar area when iconified:
        guiwin_get_grect(win, GUIWIN_AREA_WORK, &area);
    } else {
        guiwin_get_grect(win, GUIWIN_AREA_CONTENT, &area);
    }

    vsf_interior(vh, FIS_SOLID);
    vsf_color(vh, 0);
    vswr_mode(vh, MD_REPLACE);
    wind_get_grect(win->handle, WF_FIRSTXYWH, &g);
    while (g.g_h > 0 || g.g_w > 0) {
        if(rc_intersect(&area, &g)) {
            pxy[0] = g.g_x;
            pxy[1] = g.g_y;
            pxy[2] = g.g_x+g.g_w-1;
            pxy[3] = g.g_y+g.g_h-1;
            v_bar(vh, pxy);
        }
        wind_get_grect(win->handle, WF_NEXTXYWH, &g);
    }
}
