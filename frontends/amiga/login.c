/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/os3support.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include <proto/window.h>
#include <proto/layout.h>
#include <proto/string.h>
#include <proto/button.h>
#include <proto/label.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/string.h>
#include <gadgets/button.h>
#include <images/label.h>
#include <reaction/reaction_macros.h>

#include "utils/messages.h"
#include "utils/nsurl.h"
#include "netsurf/mouse.h"
#include "netsurf/window.h"
#include "netsurf/url_db.h"

#include "amiga/gui.h"
#include "amiga/libs.h"
#include "amiga/login.h"
#include "amiga/misc.h"
#include "amiga/object.h"
#include "amiga/utf8.h"

enum {
	AMI_LOGIN_MSG_HOST,
	AMI_LOGIN_MSG_REALM,
	AMI_LOGIN_MSG_USER,
	AMI_LOGIN_MSG_PASS,
	AMI_LOGIN_MSG_LOGIN,
	AMI_LOGIN_MSG_CANCEL,
	AMI_LOGIN_MSG_MAX
};

struct gui_login_window {
	struct ami_generic_window w;
	struct Window *win;
	Object *objects[GID_LAST];
	nserror (*cb)(const char *username, const char *password, void *pw);
	void *cbpw;
	nsurl *url;
	char *realm;
	lwc_string *host;
	char uname[256];
	char pwd[256];
	char *messages[AMI_LOGIN_MSG_MAX];
};

static BOOL ami_401login_event(void *w);
static void ami_401login_close(void *w);

static const struct ami_win_event_table ami_login_table = {
	ami_401login_event,
	ami_401login_close, /* TODO: investigate why this doesn't get called on exit */
};

nserror gui_401login_open(nsurl *url, const char *realm,
		const char *username, const char *password,
		nserror (*cb)(const char *username,
				const char *password,
				void *pw),
		void *cbpw)
{
	struct gui_login_window *lw = calloc(1, sizeof(struct gui_login_window));
	lwc_string *host = nsurl_get_component(url, NSURL_HOST);
	size_t len;

	assert(host != NULL);
	assert(username != NULL);
	assert(password != NULL);

	lw->host = host;
	lw->url = nsurl_ref(url);
	lw->realm = (realm != NULL) ? strdup(realm) : NULL;
	lw->cb = cb;
	lw->cbpw = cbpw;

	lw->messages[AMI_LOGIN_MSG_HOST] = ami_utf8_easy(messages_get("Host"));
	lw->messages[AMI_LOGIN_MSG_REALM] = ami_utf8_easy(messages_get("Realm"));
	lw->messages[AMI_LOGIN_MSG_USER] = ami_utf8_easy(messages_get("Username"));
	lw->messages[AMI_LOGIN_MSG_PASS] = ami_utf8_easy(messages_get("Password"));
	lw->messages[AMI_LOGIN_MSG_LOGIN] = ami_utf8_easy(messages_get("Login"));
	lw->messages[AMI_LOGIN_MSG_CANCEL] = ami_utf8_easy(messages_get("Cancel"));

	/* Convert existing username and password to local charset */
	char *user_utf8 = ami_utf8_easy(username);
	if(user_utf8 != NULL) {
		len = strlen(user_utf8);
		assert(len < sizeof(lw->uname));
		memcpy(lw->uname, user_utf8, len + 1);
		ami_utf8_free(user_utf8);
	}

	char *pass_utf8 = ami_utf8_easy(password);
	if(pass_utf8 != NULL) {
		len = strlen(pass_utf8);
		assert(len < sizeof(lw->pwd));
		memcpy(lw->pwd, pass_utf8, len + 1);
		ami_utf8_free(pass_utf8);
	}

	lw->objects[OID_MAIN] = WindowObj,
      	    WA_ScreenTitle, ami_gui_get_screen_title(),
           	WA_Title, nsurl_access(lw->url),
           	WA_Activate, TRUE,
           	WA_DepthGadget, TRUE,
           	WA_DragBar, TRUE,
           	WA_CloseGadget, FALSE,
           	WA_SizeGadget, TRUE,
			WA_PubScreen,scrn,
			WINDOW_SharedPort,sport,
			WINDOW_UserData,lw,
			WINDOW_IconifyGadget, FALSE,
			WINDOW_LockHeight,TRUE,
         	WINDOW_Position, WPOS_CENTERSCREEN,
           	WINDOW_ParentGroup, lw->objects[GID_MAIN] = LayoutVObj,
				LAYOUT_AddChild, StringObj,
					STRINGA_TextVal, lwc_string_data(lw->host),
					GA_ReadOnly,TRUE,
				StringEnd,
				CHILD_Label, LabelObj,
					LABEL_Text, lw->messages[AMI_LOGIN_MSG_HOST],
				LabelEnd,
				CHILD_WeightedHeight,0,
				LAYOUT_AddChild, StringObj,
					STRINGA_TextVal,lw->realm,
					GA_ReadOnly,TRUE,
				StringEnd,
				CHILD_Label, LabelObj,
					LABEL_Text, lw->messages[AMI_LOGIN_MSG_REALM],
				LabelEnd,
				CHILD_WeightedHeight,0,
				LAYOUT_AddChild, lw->objects[GID_USER] = StringObj,
					GA_ID,GID_USER,
					GA_TabCycle,TRUE,
					STRINGA_TextVal, lw->uname,
				StringEnd,
				CHILD_Label, LabelObj,
					LABEL_Text, lw->messages[AMI_LOGIN_MSG_USER],
				LabelEnd,
				CHILD_WeightedHeight,0,
				LAYOUT_AddChild, lw->objects[GID_PASS] = StringObj,
					GA_ID,GID_PASS,
					STRINGA_HookType,SHK_PASSWORD,
					GA_TabCycle,TRUE,
					STRINGA_TextVal, lw->pwd,
				StringEnd,
				CHILD_Label, LabelObj,
					LABEL_Text, lw->messages[AMI_LOGIN_MSG_PASS],
				LabelEnd,
				CHILD_WeightedHeight,0,
				LAYOUT_AddChild, LayoutHObj,
					LAYOUT_AddChild, lw->objects[GID_LOGIN] = ButtonObj,
						GA_ID,GID_LOGIN,
						GA_RelVerify,TRUE,
						GA_Text, lw->messages[AMI_LOGIN_MSG_LOGIN],
						GA_TabCycle,TRUE,
					ButtonEnd,
					CHILD_WeightedHeight,0,
					LAYOUT_AddChild, lw->objects[GID_CANCEL] = ButtonObj,
						GA_ID,GID_CANCEL,
						GA_RelVerify,TRUE,
						GA_Text, lw->messages[AMI_LOGIN_MSG_CANCEL],
						GA_TabCycle,TRUE,
					ButtonEnd,
				LayoutEnd,
				CHILD_WeightedHeight,0,
			EndGroup,
		EndWindow;

	lw->win = (struct Window *)RA_OpenWindow(lw->objects[OID_MAIN]);
	ami_gui_win_list_add(lw, AMINS_LOGINWINDOW, &ami_login_table);

	return NSERROR_OK;
}

static void ami_401login_close(void *w)
{
	struct gui_login_window *lw = (struct gui_login_window *)w;

	/* If continuation exists, then forbid refetch */
	if (lw->cb != NULL)
		lw->cb(NULL, NULL, lw->cbpw);

	DisposeObject(lw->objects[OID_MAIN]);
	lwc_string_unref(lw->host);
	nsurl_unref(lw->url);
	free(lw->realm);

	/* Free local charset version of messages */
	for(int i = 0; i < AMI_LOGIN_MSG_MAX; i++) {
		ami_utf8_free(lw->messages[i]);
	}

	ami_gui_win_list_remove(lw);
}

static void ami_401login_login(struct gui_login_window *lw)
{
	char *user;
	char *pass;

	/* Get username and password from string gadgets */
	GetAttr(STRINGA_TextVal,lw->objects[GID_USER],(ULONG *)&user);
	GetAttr(STRINGA_TextVal,lw->objects[GID_PASS],(ULONG *)&pass);

	/* Convert from local charset to UTF-8 */
	char *user_utf8 = ami_to_utf8_easy(user);
	char *pass_utf8 = ami_to_utf8_easy(pass);

	if(user_utf8 && pass_utf8) {
		lw->cb(user_utf8, pass_utf8, lw->cbpw);

		ami_utf8_free(user_utf8);
		ami_utf8_free(pass_utf8);
	} else {
		amiga_warn_user("NoMemory", "");
	}

	/* Invalidate continuation */
	lw->cb = NULL;
	lw->cbpw = NULL;

	ami_401login_close(lw);
}

static BOOL ami_401login_event(void *w)
{
	/* return TRUE if window destroyed */
	struct gui_login_window *lw = (struct gui_login_window *)w;
	ULONG result;
	uint16 code;

	while((result = RA_HandleInput(lw->objects[OID_MAIN], &code)) != WMHI_LASTMSG)
	{
       	switch(result & WMHI_CLASSMASK) // class
		{
			case WMHI_GADGETUP:
				switch(result & WMHI_GADGETMASK)
				{
					case GID_LOGIN:
						ami_401login_login(lw);
						return TRUE;
					break;

					case GID_CANCEL:
						ami_401login_close(lw);
						return TRUE;
					break;
				}
			break;
		}
	}
	return FALSE;
}

