/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Philip Pemberton <philpem@users.sourceforge.net>
 */

#ifndef _NETSURF_DESKTOP_CONTENT_H_
#define _NETSURF_DESKTOP_CONTENT_H_

#include "libxml/HTMLparser.h"
#ifdef riscos
#include "libpng/png.h"
#include "oslib/osspriteop.h"
#endif
#include "netsurf/content/cache.h"
#include "netsurf/content/fetch.h"
#include "netsurf/css/css.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"


/**
 * A struct content corresponds to a single url.
 *
 * It is in one of the following states:
 *   CONTENT_FETCHING - the data is being fetched and/or converted
 *                      for use by the browser
 *   CONTENT_READY - the content has been processed and is ready
 *                      to display
 *
 * The converted data is stored in the cache, not the source data.
 * Users of the structure are counted in use_count; when use_count = 0
 * the content may be removed from the memory cache.
 */

typedef enum {
	CONTENT_HTML,
	CONTENT_TEXTPLAIN,
#ifdef riscos
	CONTENT_JPEG,
#endif
	CONTENT_CSS,
#ifdef riscos
	CONTENT_PNG,
	CONTENT_GIF,
	CONTENT_PLUGIN,
#endif
	CONTENT_OTHER,
	CONTENT_UNKNOWN  /* content-type not received yet */
} content_type;

struct box_position
{
  struct box* box;
  int actual_box_x;
  int actual_box_y;
  int plot_index;
  int pixel_offset;
  int char_offset;
};

typedef enum {
	CONTENT_MSG_LOADING,   /* fetching or converting */
	CONTENT_MSG_READY,     /* may be displayed */
	CONTENT_MSG_DONE,      /* finished */
	CONTENT_MSG_ERROR,     /* error occurred */
	CONTENT_MSG_STATUS,    /* new status string */
	CONTENT_MSG_REDIRECT   /* replacement URL */
} content_msg;

struct content_user
{
	void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, const char *error);
	void *p1;
	void *p2;
	struct content_user *next;
};

struct content
{
  char *url;
  content_type type;
  char *mime_type;
  enum {
	  CONTENT_STATUS_TYPE_UNKNOWN,  /* type not yet known */
	  CONTENT_STATUS_LOADING,  /* content is being fetched or converted
			              and is not safe to display */
	  CONTENT_STATUS_READY,    /* some parts of content still being
			              loaded, but can be displayed */
	  CONTENT_STATUS_DONE      /* all finished */
  } status;
  unsigned long width, height;
  unsigned long available_width;

  union
  {
    struct
    {
      htmlParserCtxt* parser;
      char* source;
      int length;
      struct box* layout;
      colour background_colour;
      unsigned int stylesheet_count;
      struct content **stylesheet_content;
      struct css_style* style;
      struct {
        struct box_position start;
        struct box_position end;
        enum {alter_UNKNOWN, alter_START, alter_END} altering;
        int selected; /* 0 = unselected, 1 = selected */
      } text_selection;
      struct font_set* fonts;
      struct page_elements elements;
      unsigned int object_count;  /* images etc. */
      struct {
        char *url;
        struct content *content;
	struct box *box;
      } *object;
    } html;

    struct
    {
      struct css_stylesheet *css;
      unsigned int import_count;
      char **import_url;
      struct content **import_content;
    } css;
#ifdef riscos
    struct
    {
      char * data;
      unsigned long length;
    } jpeg;

    struct
    {
      png_structp png;
      png_infop info;
      unsigned long rowbytes;
      int interlace;
      osspriteop_area *sprite_area;
      char *sprite_image;
      enum { PNG_PALETTE, PNG_DITHER, PNG_DEEP } type;
    } png;

    // Structure for the GIF handler
    struct
    {
      char *data;                         // GIF data
      unsigned long length;               // Length of GIF data
      unsigned long buffer_pos;           // Position in the buffer
      osspriteop_area *sprite_area;       // Sprite area
      char *sprite_image;                 // Sprite image
    } gif;

    /* Structure for plugin */
    struct
    {
      char *data;                         /* object data */
      unsigned long length;               /* object length */
      char* sysvar;                       /* system variable set by plugin */
    } plugin;
#endif
    /* downloads */
    struct
    {
      char *data;
      unsigned long length;
    } other;

  } data;

  struct cache_entry *cache;
  unsigned long size;
  char *title;
  unsigned int active;
  int error;
  struct content_user *user_list;
  char status_message[80];
  struct fetch *fetch;
  unsigned long fetch_size, total_size;
};


struct browser_window;


content_type content_lookup(const char *mime_type);
struct content * content_create(char *url);
void content_set_type(struct content *c, content_type type, char *mime_type);
void content_process_data(struct content *c, char *data, unsigned long size);
void content_convert(struct content *c, unsigned long width, unsigned long height);
void content_revive(struct content *c, unsigned long width, unsigned long height);
void content_reformat(struct content *c, unsigned long width, unsigned long height);
void content_destroy(struct content *c);
void content_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height);
void content_add_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, const char *error),
		void *p1, void *p2);
void content_remove_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, const char *error),
		void *p1, void *p2);
void content_broadcast(struct content *c, content_msg msg, char *error);
void content_add_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);
void content_remove_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);
void content_reshape_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);

#endif
