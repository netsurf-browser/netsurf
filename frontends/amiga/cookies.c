/*
 * Copyright 2017 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

/**
 * \file
 * Implementation of Amiga cookie viewer using core windows.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <proto/intuition.h>

#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/space.h>

#include <reaction/reaction_macros.h>

#include "desktop/cookie_manager.h"
#include "netsurf/keypress.h"
#include "netsurf/plotters.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsoption.h"

#include "amiga/corewindow.h"
#include "amiga/libs.h"
#include "amiga/utf8.h"


/**
 * Amiga cookie viewer window context
 */
struct ami_cookie_window {
	/** Amiga core window context */
	struct ami_corewindow core;
};

static struct ami_cookie_window *cookie_window = NULL;

/**
 * destroy a previously created cookie view
 */
static nserror
ami_cookies_destroy(void)
{
	nserror res;

	if(cookie_window == NULL)
		return NSERROR_OK;

	res = cookie_manager_fini();
	if (res == NSERROR_OK) {
		res = ami_corewindow_fini(&cookie_window->core); /* closes the window for us */
		cookie_window = NULL;
	}
	return res;
}


/**
 * callback for mouse action for cookie viewer on core window
 *
 * \param ami_cw The Amiga core window structure.
 * \param mouse_state netsurf mouse state on event
 * \param x location of event
 * \param y location of event
 * \return NSERROR_OK on success otherwise apropriate error code
 */
static nserror
ami_cookies_mouse(struct ami_corewindow *ami_cw,
					browser_mouse_state mouse_state,
					int x, int y)
{
	cookie_manager_mouse_action(mouse_state, x, y);

	return NSERROR_OK;
}

/**
 * callback for keypress for cookies viewer on core window
 *
 * \param example_cw The Amiga core window structure.
 * \param nskey The netsurf key code
 * \return NSERROR_OK on success otherwise apropriate error code
 */
static nserror
ami_cookies_key(struct ami_corewindow *ami_cw, uint32_t nskey)
{
	if (cookie_manager_keypress(nskey)) {
			return NSERROR_OK;
	}
	return NSERROR_NOT_IMPLEMENTED;
}

/**
 * callback on draw event for cookies viewer on core window
 *
 * \param ami_cw The Amiga core window structure.
 * \param r The rectangle of the window that needs updating.
 * \param ctx The drawing context
 * \return NSERROR_OK on success otherwise apropriate error code
 */
static nserror
ami_cookies_draw(struct ami_corewindow *ami_cw, int x, int y, struct rect *r, struct redraw_context *ctx)
{
	cookie_manager_redraw(x, y, r, ctx);

	return NSERROR_OK;
}

static nserror
ami_cookies_create_window(struct ami_cookie_window *cookie_win)
{
	struct ami_corewindow *ami_cw = (struct ami_corewindow *)&cookie_win->core;

	ami_cw->objects[GID_CW_WIN] = WindowObj,
  	    WA_ScreenTitle, ami_gui_get_screen_title(),
       	WA_Title, ami_cw->wintitle,
       	WA_Activate, TRUE,
       	WA_DepthGadget, TRUE,
       	WA_DragBar, TRUE,
       	WA_CloseGadget, TRUE,
       	WA_SizeGadget, TRUE,
		WA_SizeBRight, TRUE,
		WA_Top, nsoption_int(cookies_window_ypos),
		WA_Left, nsoption_int(cookies_window_xpos),
		WA_Width, nsoption_int(cookies_window_xsize),
		WA_Height, nsoption_int(cookies_window_ysize),
		WA_PubScreen, scrn,
		WA_ReportMouse, TRUE,
       	WA_IDCMP, IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_NEWSIZE |
				IDCMP_RAWKEY | IDCMP_GADGETUP | IDCMP_IDCMPUPDATE |
				IDCMP_EXTENDEDMOUSE | IDCMP_SIZEVERIFY,
		WINDOW_IDCMPHook, &ami_cw->idcmp_hook,
		WINDOW_IDCMPHookBits, IDCMP_IDCMPUPDATE | IDCMP_EXTENDEDMOUSE,
		WINDOW_SharedPort, sport,
		WINDOW_HorizProp, 1,
		WINDOW_VertProp, 1,
		WINDOW_UserData, cookie_win,
		/* WINDOW_NewMenu, twin->menu,   -> No menu for SSL Cert */
		WINDOW_IconifyGadget, FALSE,
		WINDOW_Position, WPOS_CENTERSCREEN,
		WINDOW_ParentGroup, ami_cw->objects[GID_CW_MAIN] = LayoutVObj,
			LAYOUT_AddChild, ami_cw->objects[GID_CW_DRAW] = SpaceObj,
				GA_ID, GID_CW_DRAW,
				SPACE_Transparent, TRUE,
				SPACE_BevelStyle, BVS_DISPLAY,
				GA_RelVerify, TRUE,
   			SpaceEnd,
		EndGroup,
	EndWindow;

	if(ami_cw->objects[GID_CW_WIN] == NULL) {
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}

/* exported interface documented in amiga/cookies.h */
nserror ami_cookies_present(void)
{
	struct ami_cookie_window *ncwin;
	nserror res;

	if(cookie_window != NULL) {
		//windowtofront()
		return NSERROR_OK;
	}

	ncwin = calloc(1, sizeof(struct ami_cookie_window));
	if (ncwin == NULL) {
		return NSERROR_NOMEM;
	}

	ncwin->core.wintitle = ami_utf8_easy((char *)messages_get("Cookies"));

	res = ami_cookies_create_window(ncwin);
	if (res != NSERROR_OK) {
		LOG("SSL UI builder init failed");
		ami_utf8_free(ncwin->core.wintitle);
		free(ncwin);
		return res;
	}

	/* initialise Amiga core window */
	ncwin->core.draw = ami_cookies_draw;
	ncwin->core.key = ami_cookies_key;
	ncwin->core.mouse = ami_cookies_mouse;
	ncwin->core.close = ami_cookies_destroy;
	ncwin->core.event = NULL;

	res = ami_corewindow_init(&ncwin->core);
	if (res != NSERROR_OK) {
		ami_utf8_free(ncwin->core.wintitle);
		DisposeObject(ncwin->core.objects[GID_CW_WIN]);
		free(ncwin);
		return res;
	}

	res = cookie_manager_init(ncwin->core.cb_table, (struct core_window *)ncwin);
	if (res != NSERROR_OK) {
		ami_utf8_free(ncwin->core.wintitle);
		DisposeObject(ncwin->core.objects[GID_CW_WIN]);
		free(ncwin);
		return res;
	}

	cookie_window = ncwin;

	return NSERROR_OK;
}

