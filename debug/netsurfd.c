/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "netsurf/utils/config.h"
#include "netsurf/content/fetch.h"
#include "netsurf/content/cache.h"
#include "netsurf/content/content.h"
#include "netsurf/content/fetchcache.h"
#include "netsurf/desktop/options.h"
#include "netsurf/riscos/save_complete.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

int done, destroyed;

void callback(content_msg msg, struct content *c, void *p1,
		void *p2,  union content_msg_data data)
{
	LOG(("content %s, message %i", c->url, msg));
	if (msg == CONTENT_MSG_DONE)
		done = 1;
	else if (msg == CONTENT_MSG_ERROR) {
		printf("=== ERROR: %s\n", data.error);
		done = destroyed = 1;
	} else if (msg == CONTENT_MSG_STATUS)
		printf("=== STATUS: %s\n", c->status_message);
	else if (msg == CONTENT_MSG_REDIRECT) {
		printf("=== REDIRECT to '%s'\n", data.redirect);
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
	url_init();
	save_complete_init();
	options_read("options");
	messages_load("messages");

	while (1) {
		puts("=== URL:");
		if (!fgets(url, 1000, stdin))
			break;
		url[strlen(url) - 1] = 0;
		destroyed = 0;
		c = fetchcache(url, callback, 0, 0, 1000, 1000, false,
				0, 0, true);
		if (c) {
			fetchcache_go(c, 0, callback, 0, 0, 0, 0, true);
			done = c->status == CONTENT_STATUS_DONE;
			while (!done)
				fetch_poll();
			puts("=== SUCCESS, dumping cache");
		} else {
			destroyed = 1;
			puts("=== FAILURE, dumping cache");
		}
		cache_dump();
		if (!destroyed) {
			/*while (1)
				schedule_run();*/
/* 			content_reformat(c, 1, 1000); */
/*			save_complete(c, "save_complete");*/
			if (c->type == CONTENT_HTML)
				box_dump(c->data.html.layout, 0);
			else if (c->type == CONTENT_CSS)
				css_dump_stylesheet(c->data.css.css);
			else if (c->type == CONTENT_GIF)
				gif_decode_frame(c->data.gif.gif, 0);
			content_remove_user(c, callback, 0, 0);
			c = 0;
		}
	}

/* 	options_write("options"); */
	cache_quit();
	fetch_quit();

	return 0;
}

void gui_multitask(void)
{
/* 	putchar('-'); */
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

#ifdef WITH_PLUGIN
void plugin_decode(void *a, void *b, void *c, void *d)
{
}
#endif

void html_redraw(struct content *c, int x, int y,
		int width, int height,
		int x0, int y0, int x1, int y1,
		float scale)
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

#ifdef WITH_PLUGIN
bool plugin_handleable(const char *mime_type)
{
	return false;
}
#endif

#ifdef WITH_PLUGIN
void plugin_msg_parse(wimp_message *message, int ack) {}
bool plugin_create(struct content *c, const char *params[]) {}
bool plugin_process_data(struct content *c, char *data, unsigned int size) {}
bool plugin_convert(struct content *c, int width, int height) {return 0;}
void plugin_reformat(struct content *c, int width, int height) {}
void plugin_destroy(struct content *c) {}
void plugin_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale) {}
void plugin_add_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state) {}
void plugin_remove_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state) {}
void plugin_reshape_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state) {}
#endif

#ifdef riscos
char *NETSURF_DIR = "<NetSurf$Dir>";
#endif

void xcolourtrans_generate_table_for_sprite(void)
{
	assert(0);
}

os_error *xosspriteop_put_sprite_scaled (osspriteop_flags flags,
      osspriteop_area const *area,
      osspriteop_id id,
      int x,
      int y,
      osspriteop_action action,
      os_factors const *factors,
      osspriteop_trans_tab const *trans_tab)
{
	assert(0);
}

void _swix(void)
{
	assert(0);
}

#ifndef riscos
bool option_filter_sprites = false;
bool option_dither_sprites = false;
int option_minimum_gif_delay = 10;
#endif

void die(const char *error)
{
	printf("die: %s\n", error);
	exit(1);
}

#ifndef riscos
int ro_content_filetype(int x)
{
	return 0;
}

extern os_error *xosfile_save_stamped (char const *file_name,
      bits file_type,
      byte const *data,
      byte const *end)
{
	return 0;
}

extern os_error *xosfile_set_type (char const *file_name,
      bits file_type)
{
	return 0;
}

extern os_t os_read_monotonic_time(void)
{
	return clock() / 10000;
}
#endif

void warn_user(const char *warning, const char *detail)
{
	printf("WARNING: %s %s\n", warning, detail);
}

void *ro_gui_current_redraw_gui = 0;
