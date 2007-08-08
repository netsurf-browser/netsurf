/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "utils/config.h"
#include "content/fetch.h"
#include "content/content.h"
#include "content/fetchcache.h"
#include "content/urldb.h"
#include "desktop/cookies.h"
#include "desktop/gui.h"
#include "desktop/options.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "desktop/tree.h"
#include "image/bitmap.h"
#include "render/box.h"
#include "riscos/save_complete.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"

int done, destroyed;
bool verbose_log = true;
bool print_active = false;
void *hotlist_toolbar = NULL;
void *hotlist_window = NULL;
struct browser_window *current_redraw_browser = NULL;
struct gui_window *search_current_window = NULL;

#ifndef riscos
char *default_stylesheet_url;
char *adblock_stylesheet_url;
bool option_filter_sprites = false;
bool option_dither_sprites = false;
void *plot = 0;
#endif

#ifdef riscos
void *ro_gui_current_redraw_gui = 0;
const char *NETSURF_DIR = "<NetSurf$Dir>";
char *default_stylesheet_url = "file:///<NetSurf$Dir>/Resources/CSS";
char *adblock_stylesheet_url = "file:///<NetSurf$Dir>/Resources/AdBlock";
#endif

static void callback(content_msg msg, struct content *c, void *p1,
		void *p2,  union content_msg_data data);


int main(int argc, char *argv[])
{
	char url[1000];
	struct content *c;

#ifndef riscos
	default_stylesheet_url = malloc(200);
	adblock_stylesheet_url = malloc(200);
	getcwd(url, sizeof url);
	snprintf(default_stylesheet_url, 200, "file:%s/ns.css", url);
	snprintf(adblock_stylesheet_url, 200, "file:%s/AdBlock", url);
#endif

	fetch_init();
	fetchcache_init();
	url_init();
	options_read("options");
	messages_load("messages");

	while (1) {
		puts("=== URL:");
		if (!fgets(url, 1000, stdin))
			break;
		url[strlen(url) - 1] = 0;
		destroyed = 0;
		c = fetchcache(url, callback, 0, 0, 1000, 1000, false,
				0, 0, true, false);
		if (c) {
			fetchcache_go(c, 0, callback, 0, 0, 1000, 1000,
					0, 0, true, 0);
			done = c->status == CONTENT_STATUS_DONE;
			while (!done)
				fetch_poll();
			puts("=== SUCCESS, dumping cache");
		} else {
			destroyed = 1;
			puts("=== FAILURE, dumping cache");
		}
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
			/*else if (c->type == CONTENT_MNG)
				nsmng_animate(c);*/
			content_remove_user(c, callback, 0, 0);
			c = 0;
		}
		content_clean();
	}

/* 	options_write("options"); */
	fetch_quit();

	return 0;
}


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


void gui_multitask(void)
{
/* 	putchar('-'); */
}


void die(const char *error)
{
	printf("die: %s\n", error);
	exit(1);
}


void warn_user(const char *warning, const char *detail)
{
	printf("WARNING: %s %s\n", warning, detail);
}


#ifdef WITH_PLUGIN
void plugin_msg_parse(wimp_message *message, int ack) {}
bool plugin_create(struct content *c, const char *params[]) {return true;}
bool plugin_convert(struct content *c, int width, int height) {return true;}
void plugin_reformat(struct content *c, int width, int height) {}
void plugin_destroy(struct content *c) {}
bool plugin_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour) {return true;}
void plugin_open(struct content *c, struct browser_window *bw,
		struct content *page, unsigned int index, struct box *box,
		struct object_params *params) {}
void plugin_close(struct content *c) {}
bool plugin_handleable(const char *mime_type) {return false;}
#endif

#ifndef riscos
bool bitmap_get_opaque(struct bitmap *bitmap) { return false; }
bool bitmap_test_opaque(struct bitmap *bitmap) { return false; }
void bitmap_set_opaque(struct bitmap *bitmap, bool opaque) {}
#endif

void tree_initialise_redraw(struct tree *tree) {}
void tree_redraw_area(struct tree *tree, int x, int y, int width, int height) {}
void tree_draw_line(int x, int y, int width, int height) {}
void tree_draw_node_element(struct tree *tree, struct node_element *element) {}
void tree_draw_node_expansion(struct tree *tree, struct node *node) {}
void tree_recalculate_node_element(struct node_element *element) {}
void tree_update_URL_node(struct node *node, const char *url,
		const struct url_data *data) {}
void tree_resized(struct tree *tree) {}
void tree_set_node_sprite_folder(struct node *node) {}

#ifndef riscos
void schedule(int t, void (*callback)(void *p), void *p) {}
void schedule_remove(void (*callback)(void *p), void *p) {}
void schedule_run(void) {}
#endif

bool selection_highlighted(struct selection *s, unsigned start, unsigned end,
		unsigned *start_idx, unsigned *end_idx) { return false; }
bool gui_search_term_highlighted(struct gui_window *g,
		unsigned start_offset, unsigned end_offset,
		unsigned *start_idx, unsigned *end_idx) { return false; }

struct caret ghost_caret;

bool cookies_update(const char *domain, const struct cookie_data *data)
{ return true; }

char *url_to_path(const char *url)
{
	return strdup(url + 5);
}

char *path_to_url(const char *path)
{
	char *r = malloc(strlen(path) + 7 + 1);

	strcpy(r, "file://");
	strcat(r, path);

	return r;
}

void tree_set_node_sprite(struct node *node, const char *sprite,
                const char *expanded) { }
