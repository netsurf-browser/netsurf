/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#ifndef _NETSURF_RISCOS_PLUGIN_H_
#define _NETSURF_RISCOS_PLUGIN_H_

#include <stdbool.h>
#include "oslib/plugin.h"
#include "oslib/wimp.h"

struct box;
struct browser_window;
struct content;
struct object_params;

struct content_plugin_data {
	char *data;		/* object data */
	unsigned long length;	/* object length */
	char *sysvar;		/* system variable set by plugin */
};

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

struct plugin_param_item {

        int type;
        int rsize;
        int nsize;
        char *name;
        int npad;
        int vsize;
        char *value;
        int vpad;
        int msize;
        char *mime_type;
        int mpad;

        struct plugin_param_item *next;
};

/* function definitions */
bool plugin_handleable(const char *mime_type);
void plugin_msg_parse(wimp_message *message, int ack);
bool plugin_create(struct content *c, const char *params[]);
bool plugin_process_data(struct content *c, char *data, unsigned int size);
bool plugin_convert(struct content *c, int width, int height);
void plugin_reformat(struct content *c, int width, int height);
void plugin_destroy(struct content *c);
void plugin_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale);
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
