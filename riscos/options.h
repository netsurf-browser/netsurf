/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

#ifndef _NETSURF_RISCOS_OPTIONS_H_
#define _NETSURF_RISCOS_OPTIONS_H_

#include "netsurf/desktop/options.h"

#define PLATFORM_OPTIONS \
	int use_mouse_gestures;\
	int allow_text_selection;\
	int use_riscos_elements;\
	int show_toolbar;\
	int show_print_preview;\
	\
	char* theme;

/* choices made easier for the dialogue boxes.  only used by the interface */

struct browser_choices
{
	int use_mouse_gestures;
	int allow_text_selection;
	int use_riscos_elements;
	int show_toolbar;
	int show_print_preview;
} ;

struct proxy_choices
{
	int http;
	char http_proxy[256];
	int http_port;
} ;

struct theme_choices
{
	char name[256];
};

struct ro_choices
{
	struct browser_choices browser;
	struct proxy_choices proxy;
	struct theme_choices theme;
};

void options_to_ro(struct options* opt, struct ro_choices* ro);
void ro_to_options(struct ro_choices* ro, struct options* opt);

#endif


