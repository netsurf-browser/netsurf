/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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

#define __STDBOOL_H__	1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <Alert.h>
#include <String.h>
#include <TextControl.h>
#include <View.h>
#include <Window.h>
extern "C" {
#include "utils/log.h"
#include "content/content.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/401login.h"
#include "desktop/gui.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
}
#include "beos/beos_gui.h"
#include "beos/beos_scaffolding.h"
#include "beos/beos_window.h"

class LoginAlert : public BAlert {
public:
			LoginAlert(struct browser_window *bw,
				const char *url, 
				const char *host, 
				const char *realm, 
				const char *text);
	virtual	~LoginAlert();
	void	MessageReceived(BMessage *message);

private:
	BString 	fUrl;				/**< URL being fetched */
	BString		fHost;				/**< Host for user display */
	BString		fRealm;				/**< Authentication realm */
	struct gui_window *fW;			/**< GUI window handle */

	BTextControl	*fUserControl;
	BTextControl	*fPassControl;
};

static void create_login_window(struct browser_window *bw, const char *host,
                const char *realm, const char *fetchurl);


#define TC_H 25
#define TC_MARGIN 10

LoginAlert::LoginAlert(struct browser_window *bw,
				const char *url, 
				const char *host, 
				const char *realm, 
				const char *text)
	: BAlert("Login", text, "Cancel", "Ok", NULL, 
		B_WIDTH_AS_USUAL, B_WARNING_ALERT)
{
	fUrl = url;
	fHost = host;
	fRealm = realm;
	// dereference now as we can't be sure 
	// the main thread won't delete from under our feet
	fW = bw->window;

	SetFeel(B_MODAL_SUBSET_WINDOW_FEEL);
	nsbeos_scaffolding *s = nsbeos_get_scaffold(bw->window);
	if (s) {
		NSBrowserWindow *w = nsbeos_get_bwindow_for_scaffolding(s);
		if (w)
			AddToSubset(w);
	}

	// make space for controls
	ResizeBy(0, 2 * TC_H);
	MoveTo(AlertPosition(Frame().Width() + 1, 
		Frame().Height() + 1));


	BTextView *tv = TextView();
	BRect r(TC_MARGIN, tv->Bounds().bottom - 2 * TC_H, 
		tv->Bounds().right - TC_MARGIN, tv->Bounds().bottom - TC_H);

	fUserControl = new BTextControl(r, "user", "Username", "", 
		new BMessage(), B_FOLLOW_BOTTOM | B_FOLLOW_RIGHT);
	fUserControl->SetDivider(60);
	tv->AddChild(fUserControl);

	r.OffsetBySelf(0, TC_H);

	fPassControl = new BTextControl(r, "pass", "Password", "", 
		new BMessage(), B_FOLLOW_BOTTOM | B_FOLLOW_RIGHT);
	fPassControl->TextView()->HideTyping(true);
	fPassControl->SetDivider(60);
	tv->AddChild(fPassControl);
	
	SetShortcut(0, B_ESCAPE);
}

LoginAlert::~LoginAlert()
{
}

void
LoginAlert::MessageReceived(BMessage *message)
{
	switch (message->what) {
	case 'ALTB':
	{
		int32 which;
		if (message->FindInt32("which", &which) < B_OK)
			break;
		// not 'Ok'
		if (which != 1)
			break;
		BMessage *m = new BMessage(*message);
		m->what = 'nsLO';
		m->AddString("URL", fUrl.String());
		m->AddString("Host", fHost.String());
		m->AddString("Realm", fRealm.String());
		m->AddPointer("gui_window", fW);
		m->AddString("User", fUserControl->Text());
		m->AddString("Pass", fPassControl->Text());
		BString auth(fUserControl->Text());
		auth << ":" << fPassControl->Text();
		m->AddString("Auth", auth.String());
		
		// notify the main thread
		// the event dispatcher will handle it
		nsbeos_pipe_message(m, NULL, fW);
	}
		break;
	default:
		break;
	}
	BAlert::MessageReceived(message);
}


void gui_401login_open(struct browser_window *bw, struct content *c,
		const char *realm)
{
	char *host;
	url_func_result res;

	res = url_host(c->url, &host);
	assert(res == URL_FUNC_OK);

	create_login_window(bw, host, realm, c->url);

	free(host);
}

void create_login_window(struct browser_window *bw, const char *host,
		const char *realm, const char *fetchurl)
{
	BString r("Secure Area");
	if (realm)
		r = realm;
	BString text(/*messages_get(*/"Please login\n");
	text << "Realm:	" << r << "\n";
	text << "Host:	" << host << "\n";
	//text << "\n";

	LoginAlert *a = new LoginAlert(bw, fetchurl, host, r.String(), 
		text.String());
	// asynchronously
	a->Go(NULL);

}
