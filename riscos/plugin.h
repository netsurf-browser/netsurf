/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#ifndef _NETSURF_RISCOS_PLUGIN_H_
#define _NETSURF_RISCOS_PLUGIN_H_

#include <stdbool.h>
#include "netsurf/content/content.h"
#include "netsurf/render/box.h"

#include "oslib/plugin.h"
#include "oslib/wimp.h"

struct plugin_state {
	int dummy;
};

struct plugin_message {

        int poll;
        plugin_b browser;
        plugin_p plugin;
        wimp_message *m;
        struct plugin_message *reply;
        struct plugin_message *next;
        struct plugin_message *prev;
};

struct plugin_list {

        struct content *c;
        struct browser_window *bw;
        struct content *page;
        struct box *box;
        struct object_params *params;
        void **state;
        struct plugin_list *next;
        struct plugin_list *prev;
};

/* function definitions */
bool plugin_handleable(const char *mime_type);
void plugin_msg_parse(wimp_message *message, int ack);
void plugin_create(struct content *c);
void plugin_process_data(struct content *c, char *data, unsigned long size);
int plugin_convert(struct content *c, unsigned int width, unsigned int height);
void plugin_revive(struct content *c, unsigned int width, unsigned int height);
void plugin_reformat(struct content *c, unsigned int width, unsigned int height);
void plugin_destroy(struct content *c);
void plugin_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height);
void plugin_add_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);
void plugin_remove_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);
void plugin_reshape_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);


#endif
