#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "utils/config.h"
#include "content/content.h"
#include "css/css.h"
#include "desktop/options.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/utils.h"

bool verbose_log = 0;
int option_font_size = 10;
int option_font_min_size = 10;

void die(const char * const error)
{
}

static bool css_process_data(struct content *c, const char *data,
		unsigned int size)
{
	char *source_data;
	union content_msg_data msg_data;
	unsigned int extra_space;

	assert(c);

	if ((c->source_size + size) > c->source_allocated) {
		extra_space = (c->source_size + size) / 4;
		if (extra_space < 65536)
			extra_space = 65536;
		source_data = talloc_realloc(c, c->source_data, char,
				c->source_size + size + extra_space);
		if (!source_data) {
			c->status = CONTENT_STATUS_ERROR;
			msg_data.error = messages_get("NoMemory");
			content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
			return false;
		}
		c->source_data = source_data;
		c->source_allocated = c->source_size + size + extra_space;
	}
	memcpy(c->source_data + c->source_size, data, size);
	c->source_size += size;

	return true;
}

void content_broadcast(struct content *c, content_msg msg,
		union content_msg_data data)
{
}

void content_remove_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c,
			intptr_t p1, intptr_t p2, union content_msg_data data),
		intptr_t p1, intptr_t p2)
{
}

void content_add_error(struct content *c, const char *token,
		unsigned int line)
{
}

void fetch_abort(struct fetch *f)
{
}

void fetch_poll(void)
{
}

struct content * fetchcache(const char *url,
		void (*callback)(content_msg msg, struct content *c,
			intptr_t p1, intptr_t p2, union content_msg_data data),
		intptr_t p1, intptr_t p2,
		int width, int height,
		bool no_error_pages,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool verifiable,
		bool download)
{
	return NULL;
}

void fetchcache_go(struct content *content, const char *referer,
		void (*callback)(content_msg msg, struct content *c,
			intptr_t p1, intptr_t p2, union content_msg_data data),
		intptr_t p1, intptr_t p2,
		int width, int height,
		char *post_urlenc,
		struct form_successful_control *post_multipart,
		bool verifiable, const char *parent_url)
{
}

void gui_multitask(void)
{
}

int main(int argc, char **argv)
{
/*	const char data[] = "h1 { blah: foo; display: block; }"
		"h1.c1 h2#id1 + h3, h4 h5.c2#id2 { size: 100mm; color: red }"
		"p { background-color: #123; clear: left; color: #ff0000; display: block;"
		"float: left; font-size: 150%; height: blah; line-height: 100;"
		"text-align: left right; width: 90%;}";
*/
	struct content *c;
	FILE *fp;
#define CHUNK_SIZE (4096)
	char data[CHUNK_SIZE];
	size_t len, origlen;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	c = talloc_zero(0, struct content);
	if (c == NULL) {
		fprintf(stderr, "No memory for content\n");
		return 1;
	}

	c->url = talloc_strdup(c, "http://www.example.com/");
	if (c->url == NULL) {
		fprintf(stderr, "No memory for url\n");
		talloc_free(c);
		return 1;
	}

	c->type = CONTENT_CSS;

	fp = fopen(argv[1], "rb");
	if (fp == NULL) {
		fprintf(stderr, "Failed opening %s\n", argv[1]);
		talloc_free(c);
		return 1;
	}

	fseek(fp, 0, SEEK_END);
	origlen = len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	printf("Reading %zu bytes\n", len);

	while (len >= CHUNK_SIZE) {
		fread(data, 1, CHUNK_SIZE, fp);

		css_process_data(c, data, CHUNK_SIZE);

		len -= CHUNK_SIZE;
	}

	if (len > 0) {
		fread(data, 1, len, fp);

		css_process_data(c, data, len);

		len = 0;
	}

	fclose(fp);

	printf("Converting\n");

	css_convert(c, 100, 100);

	printf("Done\n");

	talloc_free(c);

	return 0;
}
