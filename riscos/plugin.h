/*
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
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

#ifndef _NETSURF_RISCOS_PLUGIN_H_
#define _NETSURF_RISCOS_PLUGIN_H_

#include "utils/config.h"
#ifdef WITH_PLUGIN

#include <stdbool.h>
#include "oslib/plugin.h"
#include "oslib/wimp.h"

struct box;
struct browser_window;
struct content;
struct object_params;
struct plugin_stream;

/* We have one content per instance of a plugin */
struct content_plugin_data {
	struct browser_window *bw;	/* window containing this content */
	struct content *page;		/* parent content */
	struct box *box;		/* box containing this content */
	char *taskname;			/* plugin task to launch */
	char *filename;			/* filename of parameters file */
	bool opened;			/* has this plugin been opened? */
	int repeated;			/* indication of opening state */
	unsigned int browser;		/* browser handle */
	unsigned int plugin;		/* plugin handle */
	unsigned int plugin_task;	/* plugin task handle */
	bool reformat_pending;		/* is a reformat pending? */
	int width, height;		/* reformat width & height */
	struct plugin_stream *streams;	/* list of active streams */
};

/* function definitions */
bool plugin_handleable(const char *mime_type);
void plugin_msg_parse(wimp_message *message, int ack);
bool plugin_create(struct content *c, struct content *parent,
		const char *params[]);
bool plugin_convert(struct content *c, int width, int height);
void plugin_reformat(struct content *c, int width, int height);
void plugin_destroy(struct content *c);
bool plugin_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour);
void plugin_open(struct content *c, struct browser_window *bw,
		struct content *page, unsigned int index, struct box *box,
		struct object_params *params);
void plugin_close(struct content *c);

/* message handlers */
void plugin_open_msg(wimp_message *message);
void plugin_opening(wimp_message *message);
void plugin_close_msg(wimp_message *message);
void plugin_closed(wimp_message *message);
void plugin_reshape_request(wimp_message *message);
void plugin_status(wimp_message *message);
void plugin_stream_new(wimp_message *message);
void plugin_stream_written(wimp_message *message);
void plugin_url_access(wimp_message *message);

#endif /* WITH_PLUGIN */

#endif
