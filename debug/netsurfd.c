#include <string.h>
#include "netsurf/content/fetch.h"
#include "netsurf/content/cache.h"
#include "netsurf/content/content.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/utils/log.h"

int done;

void callback(content_msg msg, struct content *c, void *p1,
		void *p2, const char *error)
{
	LOG(("content %s, message %i", c->url, msg));
	if (msg == CONTENT_MSG_DONE || msg == CONTENT_MSG_ERROR)
		done = 1;
	else if (msg == CONTENT_MSG_STATUS)
		printf("=== STATUS: %s\n", c->status_message);
	else if (msg == CONTENT_MSG_REDIRECT) {
		printf("=== REDIRECT to '%s'\n", error);
		done = 1;
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
		gets(url);
		c = fetchcache(url, 0, callback, 0, 0, 100, 1000);
		done = c->status == CONTENT_STATUS_DONE;
		while (!done)
			fetch_poll();
		puts("=== SUCCESS, dumping cache");
		cache_dump();
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


