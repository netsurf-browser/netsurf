/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

#include <stdbool.h>
#include <string.h>
#include "netsurf/content/fetch.h"
#include "netsurf/content/cache.h"
#include "netsurf/content/content.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

int done, destroyed;

void callback(content_msg msg, struct content *c, void *p1,
		void *p2, const char *error)
{
	LOG(("content %s, message %i", c->url, msg));
	if (msg == CONTENT_MSG_DONE)
		done = 1;
	else if (msg == CONTENT_MSG_ERROR)
		done = destroyed = 1;
	else if (msg == CONTENT_MSG_STATUS)
		printf("=== STATUS: %s\n", c->status_message);
	else if (msg == CONTENT_MSG_REDIRECT) {
		printf("=== REDIRECT to '%s'\n", error);
		done = destroyed = 1;
	}
}

int main(int argc, char *argv[])
{
	char url[1000];
	struct content *c;

	fetch_init();
	cache_init();
	fetchcache_init();

	while (1) {
		puts("=== URL:");
		if (!fgets(url, 1000, stdin))
			break;
		url[strlen(url) - 1] = 0;
		destroyed = 0;
		c = fetchcache(url, 0, callback, 0, 0, 100, 1000, false, 0, 0, true);
		if (c) {
			done = c->status == CONTENT_STATUS_DONE;
			while (!done)
				fetch_poll();
			puts("=== SUCCESS, dumping cache");
		} else {
			destroyed = 1;
			puts("=== FAILURE, dumping cache");
		}
		cache_dump();
		if (!destroyed)
			content_remove_user(c, callback, 0, 0);
	}

	cache_quit();
	fetch_quit();

	return 0;
}

void gui_multitask(void)
{
	LOG(("-"));
}

#ifndef riscos
int stricmp(char *s0, char *s1)
{
	return strcasecmp(s0, s1);
}
#endif

void gui_remove_gadget(void *p)
{
}

void plugin_decode(void *a, void *b, void *c, void *d)
{
}

void html_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long x0, long y0, long x1, long y1)
{
}

void html_add_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state)
{
}

void html_reshape_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state)
{
}

void html_remove_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state)
{
}

void *login_list_get(char *url)
{
	return 0;
}

bool plugin_handleable(const char *mime_type)
{
	return false;
}

#ifdef riscos
void plugin_msg_parse(wimp_message *message, int ack) {}
void plugin_create(struct content *c) {}
void plugin_process_data(struct content *c, char *data, unsigned long size) {}
int plugin_convert(struct content *c, unsigned int width, unsigned int height) {}
void plugin_revive(struct content *c, unsigned int width, unsigned int height) {}
void plugin_reformat(struct content *c, unsigned int width, unsigned int height) {}
void plugin_destroy(struct content *c) {}
void plugin_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long clip_x0, long clip_y0, long clip_x1, long clip_y1) {}
void plugin_add_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state) {}
void plugin_remove_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state) {}
void plugin_reshape_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state) {}

char *NETSURF_DIR = "<NetSurf$Dir>";
#endif

