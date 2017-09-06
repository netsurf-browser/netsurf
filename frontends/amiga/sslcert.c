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
 * Implementation of Amiga certificate viewing using core windows.
 */

#include <stdint.h>
#include <stdlib.h>

#include <proto/intuition.h>

#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/layout.h>
#include <gadgets/scroller.h>
#include <gadgets/space.h>
#include <images/label.h>

#include <intuition/icclass.h>
#include <reaction/reaction_macros.h>

#include "utils/log.h"
#include "netsurf/keypress.h"
#include "netsurf/plotters.h"
#include "desktop/sslcert_viewer.h"
#include "utils/messages.h"
#include "utils/nsoption.h"

#include "amiga/corewindow.h"
#include "amiga/libs.h"
#include "amiga/sslcert.h"
#include "amiga/utf8.h"


/**
 * Amiga certificate viewing window context
 */
enum {
	GID_SSLCERT_ACCEPT = GID_CW_LAST,
	GID_SSLCERT_REJECT,
	GID_SSLCERT_LAST
};

#define GID_SSLCERT_SIZE GID_SSLCERT_LAST - GID_CW_LAST

struct ami_crtvrfy_window {
	/** Amiga core window context */
	struct ami_corewindow core;

	/** Amiga GUI stuff */
	Object *sslcert_objects[GID_SSLCERT_LAST]; // technically wasting a few bytes here

	char *sslerr;
	char *sslaccept;
	char *sslreject;

	/** SSL certificate viewer context data */
	struct sslcert_session_data *ssl_data;
};

/**
 * destroy a previously created certificate view
 */
static nserror
ami_crtvrfy_destroy(struct ami_crtvrfy_window *crtvrfy_win)
{
	nserror res;

	res = sslcert_viewer_fini(crtvrfy_win->ssl_data);
	if (res == NSERROR_OK) {
		ami_utf8_free(crtvrfy_win->sslerr);
		ami_utf8_free(crtvrfy_win->sslaccept);
		ami_utf8_free(crtvrfy_win->sslreject);
		res = ami_corewindow_fini(&crtvrfy_win->core); /* closes the window for us */
	}
	return res;
}

static void
ami_crtvrfy_accept(struct ami_corewindow *ami_cw)
{
	struct ami_crtvrfy_window *crtvrfy_win;
	/* technically degenerate container of */
	crtvrfy_win = (struct ami_crtvrfy_window *)ami_cw;

	sslcert_viewer_accept(crtvrfy_win->ssl_data);

	ami_crtvrfy_destroy(crtvrfy_win);
}

static void
ami_crtvrfy_reject(struct ami_corewindow *ami_cw)
{
	struct ami_crtvrfy_window *crtvrfy_win;
	/* technically degenerate container of */
	crtvrfy_win = (struct ami_crtvrfy_window *)ami_cw;

	sslcert_viewer_reject(crtvrfy_win->ssl_data);

	ami_crtvrfy_destroy(crtvrfy_win);
}

/**
 * callback for unknown events on Amiga core window
 * eg. buttons in the ssl cert window
 * (result & WMHI_CLASSMASK) gives the class of event (eg. WMHI_GADGETUP)
 * (result & WMHI_GADGETMASK) gives the gadget ID (eg. GID_SSLCERT_ACCEPT)
 *
 * \param ami_cw The Amiga core window structure.
 * \param result event as returned by RA_HandleInput()
 * \return TRUE if window closed during event processing
 */
static BOOL
ami_crtvrfy_event(struct ami_corewindow *ami_cw, ULONG result)
{
	if((result & WMHI_CLASSMASK) == WMHI_GADGETUP) {
		switch(result & WMHI_GADGETMASK) {
			case GID_SSLCERT_ACCEPT:
				ami_crtvrfy_accept(ami_cw);
				return TRUE;
			break;

			case GID_SSLCERT_REJECT:
				ami_crtvrfy_reject(ami_cw);
				return TRUE;
			break;
		}
	}
	return FALSE;
}

/**
 * callback for mouse action for certificate verify on core window
 *
 * \param ami_cw The Amiga core window structure.
 * \param mouse_state netsurf mouse state on event
 * \param x location of event
 * \param y location of event
 * \return NSERROR_OK on success otherwise apropriate error code
 */
static nserror
ami_crtvrfy_mouse(struct ami_corewindow *ami_cw,
					browser_mouse_state mouse_state,
					int x, int y)
{
	struct ami_crtvrfy_window *crtvrfy_win;
	/* technically degenerate container of */
	crtvrfy_win = (struct ami_crtvrfy_window *)ami_cw;

	sslcert_viewer_mouse_action(crtvrfy_win->ssl_data, mouse_state, x, y);

	return NSERROR_OK;
}

/**
 * callback for keypress for certificate verify on core window
 *
 * \param ami_cw The Amiga core window structure.
 * \param nskey The netsurf key code
 * \return NSERROR_OK on success otherwise apropriate error code
 */
static nserror
ami_crtvrfy_key(struct ami_corewindow *ami_cw, uint32_t nskey)
{
	struct ami_crtvrfy_window *crtvrfy_win;

	/* technically degenerate container of */
	crtvrfy_win = (struct ami_crtvrfy_window *)ami_cw;

	if (sslcert_viewer_keypress(crtvrfy_win->ssl_data, nskey)) {
			return NSERROR_OK;
	}
	return NSERROR_NOT_IMPLEMENTED;
}

/**
 * callback on draw event for certificate verify on core window
 *
 * \param ami_cw The Amiga core window structure.
 * \param x the x coordinate to draw
 * \param y the y coordinate to draw
 * \param r The rectangle of the window that needs updating.
 * \param ctx The drawing context
 * \return NSERROR_OK on success otherwise apropriate error code
 */
static nserror
ami_crtvrfy_draw(struct ami_corewindow *ami_cw, int x, int y, struct rect *r, struct redraw_context *ctx)
{
	struct ami_crtvrfy_window *crtvrfy_win;

	/* technically degenerate container of */
	crtvrfy_win = (struct ami_crtvrfy_window *)ami_cw;

	sslcert_viewer_redraw(crtvrfy_win->ssl_data, x, y, r, ctx);

	return NSERROR_OK;
}

static nserror
ami_crtvrfy_create_window(struct ami_crtvrfy_window *crtvrfy_win)
{
	struct ami_corewindow *ami_cw = (struct ami_corewindow *)&crtvrfy_win->core;
	ULONG refresh_mode = WA_SmartRefresh;

	if(nsoption_bool(window_simple_refresh) == true) {
		refresh_mode = WA_SimpleRefresh;
	}

	ami_cw->objects[GID_CW_WIN] = WindowObj,
  	    WA_ScreenTitle, ami_gui_get_screen_title(),
       	WA_Title, ami_cw->wintitle,
       	WA_Activate, TRUE,
       	WA_DepthGadget, TRUE,
       	WA_DragBar, TRUE,
       	WA_CloseGadget, FALSE,
       	WA_SizeGadget, TRUE,
		WA_SizeBBottom, TRUE,
		WA_Height, scrn->Height / 2,
		WA_PubScreen, scrn,
		WA_ReportMouse, TRUE,
		refresh_mode, TRUE,
		WA_IDCMP, IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_NEWSIZE |
				IDCMP_RAWKEY | IDCMP_GADGETUP | IDCMP_IDCMPUPDATE |
				IDCMP_EXTENDEDMOUSE | IDCMP_SIZEVERIFY | IDCMP_REFRESHWINDOW,
		WINDOW_IDCMPHook, &ami_cw->idcmp_hook,
		WINDOW_IDCMPHookBits, IDCMP_IDCMPUPDATE | IDCMP_EXTENDEDMOUSE |
				IDCMP_SIZEVERIFY | IDCMP_REFRESHWINDOW,
		WINDOW_SharedPort, sport,
		WINDOW_UserData, crtvrfy_win,
		/* WINDOW_NewMenu, twin->menu,   -> No menu for SSL Cert */
		WINDOW_IconifyGadget, FALSE,
		WINDOW_Position, WPOS_CENTERSCREEN,
		WINDOW_ParentGroup, ami_cw->objects[GID_CW_MAIN] = LayoutVObj,
			LAYOUT_AddImage, LabelObj,
				LABEL_Text, crtvrfy_win->sslerr,
			LabelEnd,
			LAYOUT_AddChild, ami_cw->objects[GID_CW_HSCROLLLAYOUT] = LayoutVObj,
				LAYOUT_AddChild, ami_cw->objects[GID_CW_VSCROLLLAYOUT] = LayoutHObj,
					LAYOUT_AddChild, ami_cw->objects[GID_CW_DRAW] = SpaceObj,
						GA_ID, GID_CW_DRAW,
						SPACE_Transparent, TRUE,
						SPACE_BevelStyle, BVS_DISPLAY,
						GA_RelVerify, TRUE,
		   			SpaceEnd,
					LAYOUT_AddChild, ami_cw->objects[GID_CW_VSCROLL] = ScrollerObj,
						GA_ID, GID_CW_VSCROLL,
						GA_RelVerify, TRUE,
						ICA_TARGET, ICTARGET_IDCMP,
		   			ScrollerEnd,
				LayoutEnd,
				LAYOUT_AddChild, ami_cw->objects[GID_CW_HSCROLL] = ScrollerObj,
					GA_ID, GID_CW_HSCROLL,
					GA_RelVerify, TRUE,
					ICA_TARGET, ICTARGET_IDCMP,
					SCROLLER_Orientation, SORIENT_HORIZ,
	   			ScrollerEnd,
			LayoutEnd,
			LAYOUT_AddChild, LayoutHObj,
				LAYOUT_AddChild, crtvrfy_win->sslcert_objects[GID_SSLCERT_ACCEPT] = ButtonObj,
					GA_ID, GID_SSLCERT_ACCEPT,
					GA_Text, crtvrfy_win->sslaccept,
					GA_RelVerify, TRUE,
				ButtonEnd,
				LAYOUT_AddChild, crtvrfy_win->sslcert_objects[GID_SSLCERT_REJECT] = ButtonObj,
					GA_ID, GID_SSLCERT_REJECT,
					GA_Text, crtvrfy_win->sslreject,
					GA_RelVerify, TRUE,
				ButtonEnd,
			EndGroup,
			CHILD_WeightedHeight, 0,
		EndGroup,
	EndWindow;

	if(ami_cw->objects[GID_CW_WIN] == NULL) {
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}

/* exported interface documented in amiga/ssl_cert.h */
nserror ami_cert_verify(struct nsurl *url,
						const struct ssl_cert_info *certs,
						unsigned long num,
						nserror (*cb)(bool proceed, void *pw),
						void *cbpw)
{
	struct ami_crtvrfy_window *ncwin;
	nserror res;

	ncwin = calloc(1, sizeof(struct ami_crtvrfy_window));
	if (ncwin == NULL) {
		return NSERROR_NOMEM;
	}

	ncwin->core.wintitle = ami_utf8_easy((char *)messages_get("SSLCerts"));
	ncwin->sslerr = ami_utf8_easy((char *)messages_get("SSLError"));
	ncwin->sslaccept = ami_utf8_easy((char *)messages_get("SSL_Certificate_Accept"));
	ncwin->sslreject = ami_utf8_easy((char *)messages_get("SSL_Certificate_Reject"));

	res = ami_crtvrfy_create_window(ncwin);
	if (res != NSERROR_OK) {
		NSLOG(netsurf, INFO, "SSL UI builder init failed");
		ami_utf8_free(ncwin->core.wintitle);
		ami_utf8_free(ncwin->sslerr);
		ami_utf8_free(ncwin->sslaccept);
		ami_utf8_free(ncwin->sslreject);
		free(ncwin);
		return res;
	}

	/* initialise Amiga core window */
	ncwin->core.draw = ami_crtvrfy_draw;
	ncwin->core.key = ami_crtvrfy_key;
	ncwin->core.mouse = ami_crtvrfy_mouse;
	ncwin->core.close = ami_crtvrfy_reject;
	ncwin->core.event = ami_crtvrfy_event;

	res = ami_corewindow_init(&ncwin->core);
	if (res != NSERROR_OK) {
		ami_utf8_free(ncwin->core.wintitle);
		ami_utf8_free(ncwin->sslerr);
		ami_utf8_free(ncwin->sslaccept);
		ami_utf8_free(ncwin->sslreject);
		DisposeObject(ncwin->core.objects[GID_CW_WIN]);
		free(ncwin);
		return res;
	}

	/* initialise certificate viewing interface */
	res = sslcert_viewer_create_session_data(num, url, cb, cbpw, certs,
									   &ncwin->ssl_data);
	if (res != NSERROR_OK) {
		ami_utf8_free(ncwin->core.wintitle);
		ami_utf8_free(ncwin->sslerr);
		ami_utf8_free(ncwin->sslaccept);
		ami_utf8_free(ncwin->sslreject);
		DisposeObject(ncwin->core.objects[GID_CW_WIN]);
		free(ncwin);
		return res;
	}

	res = sslcert_viewer_init(ncwin->core.cb_table,
							  (struct core_window *)ncwin,
							  ncwin->ssl_data);
	if (res != NSERROR_OK) {
		ami_utf8_free(ncwin->core.wintitle);
		ami_utf8_free(ncwin->sslerr);
		ami_utf8_free(ncwin->sslaccept);
		ami_utf8_free(ncwin->sslreject);
		DisposeObject(ncwin->core.objects[GID_CW_WIN]);
		free(ncwin);
		return res;
	}

	return NSERROR_OK;
}

