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
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/url.h"
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
		c = fetchcache(url, 0, callback, 0, 0, 1000, 1000, false
#ifdef WITH_POST
		, 0, 0
#endif
#ifdef WITH_COOKIES
		, true
#endif
		);
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
		if (!destroyed) {
/* 			content_reformat(c, 1, 1000); */
/*			save_complete(c, "save_complete");*/
			box_dump(c->data.html.layout, 0);
			content_remove_user(c, callback, 0, 0);
		}
	}

	options_write("options");
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

void html_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height,
		long x0, long y0, long x1, long y1,
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

#ifdef riscos
#ifdef WITH_PLUGIN
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
#endif

char *NETSURF_DIR = "<NetSurf$Dir>";
#endif

int colourtrans_return_colour_number_for_mode(int colour, int mode,
		int *dest_palette)
{
	assert(!dest_palette);
	return colour;
}

int *xjpeginfo_dimensions(void)
{
	return 1;
}

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

os_error *xos_byte(byte op, int r1, int r2, int *r1_out, int *r2_out)
{
	assert(op == 0x87);
	*r2_out = 28;
	return 0;
}

void xjpeg_plot_scaled(void)
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
#endif

void warn_user(const char *warn)
{
	printf("WARNING: %s\n", warn);
}

#ifndef riscos
void schedule(int t, void (*callback)(void *p), void *p)
{
	printf("UNIMPLEMENTED: schedule(%i, %p, %p)\n", t, callback, p);
}

void schedule_remove(void (*callback)(void *p), void *p)
{
	printf("UNIMPLEMENTED: schedule_remove(%p, %p)\n", callback, p);
}
#endif
