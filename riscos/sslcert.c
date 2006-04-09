/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * SSL Certificate verification UI (implementation)
 */

#include "netsurf/utils/config.h"

#ifdef WITH_SSL

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "oslib/wimp.h"
#include "netsurf/content/content.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/urldb.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/riscos/dialog.h"
#include "netsurf/riscos/wimp_event.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#define ICON_CERT_VERSION 1
#define ICON_CERT_VALID_FROM 2
#define ICON_CERT_TYPE 3
#define ICON_CERT_VALID_TO 4
#define ICON_CERT_SERIAL 5
#define ICON_CERT_ISSUER 6
#define ICON_CERT_SUBJECT 7
#define ICON_CERT_REJECT 8
#define ICON_CERT_ACCEPT 9

static wimp_window *dialog_cert_template;

struct session_cert {
	char version[16], valid_from[32], valid_to[32], type[8], serial[32],
		issuer[256], subject[256];
	char *url;
	struct browser_window *bw;
};

static void ro_gui_cert_open(struct browser_window *bw, const char *url,
		const struct ssl_cert_info *certdata);
static void ro_gui_cert_close(wimp_w w);
static bool ro_gui_cert_apply(wimp_w w);

/**
 * Load the cert window template
 */

void ro_gui_cert_init(void)
{
	dialog_cert_template = ro_gui_dialog_load_template("sslcert");
}

/**
 * Open the certificate verification dialog
 */

void gui_cert_verify(struct browser_window *bw, struct content *c,
		const struct ssl_cert_info *certs, unsigned long num)
{
	assert(bw && c && certs);

	/** \todo Display entire certificate chain */
	ro_gui_cert_open(bw, c->url, certs);
}

void ro_gui_cert_open(struct browser_window *bw, const char *url,
		const struct ssl_cert_info *certdata)
{
	struct session_cert *session;
	wimp_w w;

	session = malloc(sizeof(struct session_cert));
	if (!session) {
		warn_user("NoMemory", 0);
		return;
	}

	session->url = strdup(url);
	if (!session->url) {
		free(session);
		warn_user("NoMemory", 0);
		return;
	}

	session->bw = bw;

	snprintf(session->version, sizeof session->version, "%ld",
			certdata->version);
	snprintf(session->valid_from, sizeof session->valid_from, "%s",
			certdata->not_before);
	snprintf(session->type, sizeof session->type, "%d",
			certdata->cert_type);
	snprintf(session->valid_to, sizeof session->valid_to, "%s",
			certdata->not_after);
	snprintf(session->serial, sizeof session->serial, "%ld",
			certdata->serial);
	snprintf(session->issuer, sizeof session->issuer, "%s",
			certdata->issuer);
	snprintf(session->subject, sizeof session->subject, "%s",
			certdata->subject);

	dialog_cert_template->icons[ICON_CERT_VERSION].data.indirected_text.text = session->version;
	dialog_cert_template->icons[ICON_CERT_VERSION].data.indirected_text.size = strlen(session->version) + 1;
	dialog_cert_template->icons[ICON_CERT_VALID_FROM].data.indirected_text.text = session->valid_from;
	dialog_cert_template->icons[ICON_CERT_VALID_FROM].data.indirected_text.size = strlen(session->valid_from) + 1;
	dialog_cert_template->icons[ICON_CERT_TYPE].data.indirected_text.text = session->type;
	dialog_cert_template->icons[ICON_CERT_TYPE].data.indirected_text.size = strlen(session->type) + 1;
	dialog_cert_template->icons[ICON_CERT_VALID_TO].data.indirected_text.text = session->valid_to;
	dialog_cert_template->icons[ICON_CERT_VALID_TO].data.indirected_text.size = strlen(session->valid_to) + 1;
	dialog_cert_template->icons[ICON_CERT_SERIAL].data.indirected_text.text = session->serial;
	dialog_cert_template->icons[ICON_CERT_SERIAL].data.indirected_text.size = strlen(session->serial) + 1;
	dialog_cert_template->icons[ICON_CERT_ISSUER].data.indirected_text.text = session->issuer;
	dialog_cert_template->icons[ICON_CERT_ISSUER].data.indirected_text.size = strlen(session->issuer) + 1;
	dialog_cert_template->icons[ICON_CERT_SUBJECT].data.indirected_text.text = session->subject;
	dialog_cert_template->icons[ICON_CERT_SUBJECT].data.indirected_text.size = strlen(session->subject) + 1;

	w = wimp_create_window(dialog_cert_template);

	ro_gui_wimp_event_register_cancel(w, ICON_CERT_REJECT);
	ro_gui_wimp_event_register_ok(w, ICON_CERT_ACCEPT, ro_gui_cert_apply);
	ro_gui_wimp_event_register_close_window(w, ro_gui_cert_close);
	ro_gui_wimp_event_set_user_data(w, session);

	ro_gui_dialog_open_persistent(bw->window->window, w, false);
}

/**
 * Handle closing of certificate verification dialog
 */
void ro_gui_cert_close(wimp_w w)
{
	os_error *error;
	struct session_cert *session;

	session = (struct session_cert *)ro_gui_wimp_event_get_user_data(w);

	assert(session);

	free(session->url);
	free(session);

	ro_gui_wimp_event_finalise(w);

	error = xwimp_delete_window(w);
	if (error)
		LOG(("xwimp_delete_window: 0x%x: %s",
				error->errnum, error->errmess));
}

/**
 * Handle acceptance of certificate
 */
bool ro_gui_cert_apply(wimp_w w)
{
	struct session_cert *session;

	session = (struct session_cert *)ro_gui_wimp_event_get_user_data(w);

	assert(session);

	urldb_set_cert_permissions(session->url, true);

	browser_window_go(session->bw, session->url, 0);

	return true;
}

#endif
