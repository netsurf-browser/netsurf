/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include "netsurf/desktop/options.h"
#include <stdio.h>
#include <string.h>
#include "oslib/messagetrans.h"
#include "oslib/osfile.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

struct options OPTIONS;
static char* lookup(messagetrans_control_block* cb, const char* token, const char* deflt);
static int lookup_yesno(messagetrans_control_block* cb, const char* token, const char* deflt);
static int lookup_i(messagetrans_control_block* cb, const char* token, const char* deflt);
static const char* yesno(int q);

char* lookup(messagetrans_control_block* cb, const char* token, const char* deflt)
{
	int used;
	char buffer[256];

	LOG(("Looking up token '%s'",token));

  	messagetrans_lookup(cb, token, buffer, 256, 0,0,0,0, &used);
	if (used > 0)
	{
		return strdup(buffer);
	}
	else
		return strdup(deflt);
}

int lookup_yesno(messagetrans_control_block* cb, const char* token, const char* deflt)
{
	char* find;
	LOG(("yesno token '%s' (default '%s')", token, deflt));
	find = lookup(cb, token, deflt);
	if (strcmp(find, "Y") == 0)
	{
		xfree(find);
		return -1;
	}
		xfree(find);
		return 0;
}

int lookup_i(messagetrans_control_block* cb, const char* token, const char* deflt)
{
	char* find = lookup(cb, token, deflt);
	int ret = atoi(find);
		xfree(find);
		return ret;
}

const char* yesno(int q)
{
	if (q)
		return "Y";
	else
		return "N";
}

static const char * const WRITE_DIR = "<Choices$Write>.NetSurf";

void options_write(struct options* opt, char* filename)
{
	char* fn;
	FILE* f;

	fn = xcalloc(strlen(WRITE_DIR) + (filename == 0 ? 7 : strlen(filename)) + 10,
			sizeof(char));
	sprintf(fn, "%s.%s", WRITE_DIR, filename == 0 ? "Choices" : filename);

	xosfile_create_dir(WRITE_DIR, 0);

	LOG(("filename: %s", fn));

	f = fopen(fn, "w");
	if (f != NULL)
	{
		fprintf(f, "# General options - for any platform\n# Proxy\n");
		fprintf(f, "USE_HTTP:%s\n", yesno(opt->http));
		fprintf(f, "HTTP_PROXY:%s\n", opt->http_proxy);
		fprintf(f, "HTTP_PORT:%d\n", opt->http_port);

		fprintf(f, "\n# RISC OS specific options\n# Browser\n");
		fprintf(f, "RO_MOUSE_GESTURES:%s\n", yesno(opt->use_mouse_gestures));
		fprintf(f, "RO_TEXT_SELECTION:%s\n", yesno(opt->allow_text_selection));
		fprintf(f, "RO_FORM_ELEMENTS:%s\n", yesno(opt->use_riscos_elements));
		fprintf(f, "RO_SHOW_TOOLBAR:%s\n", yesno(opt->show_toolbar));
		fprintf(f, "RO_SHOW_PRINT:%s\n", yesno(opt->show_print_preview));
		fprintf(f, "\n# Theme\n");
		fprintf(f, "RO_THEME:%s\n", opt->theme);
	}
	else
	    LOG(("Couldn't open Choices file"));

	fclose(f);
	xfree(fn);
}

void options_init(struct options* opt)
{
	opt->http = 0;
	opt->http_proxy = strdup("");
	opt->http_port = 8080;
	opt->use_mouse_gestures = 0;
	opt->allow_text_selection = 1;
	opt->use_riscos_elements = 1;
	opt->show_toolbar = 1;
	opt->show_print_preview = 0;
	opt->theme = strdup("Default");
}

void options_read(struct options* opt, char* filename)
{
	messagetrans_control_block cb;
  	messagetrans_file_flags flags;
    	char* data;
	char* fn;
	int size;

        fn = xcalloc(20 + (filename == 0 ? 7 : strlen(filename)), sizeof(char));
	sprintf(fn, "Choices:NetSurf.%s", filename == 0 ? "Choices" : filename);

	LOG(("Getting file info"));
  	if (xmessagetrans_file_info(fn, &flags, &size) != NULL)
		return;

        /* catch empty choices file - this is a kludge but should work */
        if (size <= 10) {

                LOG(("Empty Choices file - using defaults"));
                options_init(opt);
                return;
        }

	LOG(("Allocating %d bytes", size));
  	data = xcalloc(size, sizeof(char));
  	messagetrans_open_file(&cb, fn, data);

	opt->http = lookup_yesno(&cb, "USE_HTTP", "N");
	xfree(opt->http_proxy);
	opt->http_proxy = lookup(&cb, "HTTP_PROXY", "");
	opt->http_port = lookup_i(&cb, "HTTP_PORT", "8080");

	opt->use_mouse_gestures = lookup_yesno(&cb, "RO_MOUSE_GESTURES", "N");
	opt->allow_text_selection = lookup_yesno(&cb, "RO_TEXT_SELECTION", "Y");
	opt->use_riscos_elements = lookup_yesno(&cb, "RO_FORM_ELEMENTS", "Y");
	opt->show_toolbar = lookup_yesno(&cb, "RO_SHOW_TOOLBAR", "Y");
	opt->show_print_preview = lookup_yesno(&cb, "RO_SHOW_PRINT", "N");

	xfree(opt->theme);
	opt->theme = lookup(&cb, "RO_THEME", "Default");
	messagetrans_close_file(&cb);
	xfree(data);
	xfree(fn);
}

void options_to_ro(struct options* opt, struct ro_choices* ro)
{
	ro->browser.use_mouse_gestures = opt->use_mouse_gestures;
	ro->browser.allow_text_selection = opt->allow_text_selection;
	ro->browser.use_riscos_elements = opt->use_riscos_elements;
	ro->browser.show_toolbar = opt->show_toolbar;
	ro->browser.show_print_preview = opt->show_print_preview;

	ro->proxy.http = opt->http;
	if (opt->http_proxy != NULL)
		strcpy(ro->proxy.http_proxy, opt->http_proxy);
	else
		strcpy(ro->proxy.http_proxy, "");
	ro->proxy.http_port = opt->http_port;

	if (opt->theme != NULL)
		strcpy(ro->theme.name, opt->theme);
	else
		strcpy(ro->theme.name, "Default");
}

void ro_to_options(struct ro_choices* ro, struct options* opt)
{
	opt->use_mouse_gestures = ro->browser.use_mouse_gestures;
	opt->allow_text_selection = ro->browser.allow_text_selection;
	opt->use_riscos_elements = ro->browser.use_riscos_elements;
	opt->show_toolbar = ro->browser.show_toolbar;
	opt->show_print_preview = ro->browser.show_print_preview;

	opt->http = ro->proxy.http;
	xfree(opt->http_proxy);
	opt->http_proxy = strdup(ro->proxy.http_proxy);
	opt->http_port = ro->proxy.http_port;

	xfree(opt->theme);
	opt->theme = strdup(ro->theme.name);
}

