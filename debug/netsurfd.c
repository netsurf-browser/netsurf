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

	while (1) {
		puts("=== URL:");
		if (!fgets(url, 1000, stdin))
			return 0;
		url[strlen(url) - 1] = 0;
		destroyed = 0;
		c = fetchcache(url, 0, callback, 0, 0, 100, 1000, false, 0, 0);
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

int stricmp(char *s0, char *s1)
{
	return strcasecmp(s0, s1);
}

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

void *login_list_get(char *url)
{
	return 0;
}
