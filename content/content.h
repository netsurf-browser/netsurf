/**
 * $Id: content.h,v 1.7 2003/04/10 21:44:45 bursa Exp $
 */

#ifndef _NETSURF_DESKTOP_CONTENT_H_
#define _NETSURF_DESKTOP_CONTENT_H_

#include "libxml/HTMLparser.h"
#include "netsurf/content/cache.h"
#include "netsurf/css/css.h"
#include "netsurf/render/box.h"
#include "netsurf/riscos/font.h"


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
	CONTENT_JPEG,
	CONTENT_CSS,
	CONTENT_PNG,
	CONTENT_OTHER
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

struct content
{
  char *url;
  content_type type;
  enum {CONTENT_LOADING, CONTENT_READY} status;
  unsigned long width, height;

  union
  {
    struct
    {
      htmlParserCtxt* parser;
      struct box* layout;
      unsigned int stylesheet_count;
      char **stylesheet_url;
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
    } html;

    struct
    {
      struct css_stylesheet *css;
      unsigned int import_count;
      char **import_url;
      struct content **import_content;
    } css;

    struct
    {
      char * data;
      unsigned long length;
    } jpeg;

  } data;

  struct cache_entry *cache;
  unsigned long size;
  char *title;
  unsigned int active;
  int error;
  void (*status_callback)(void *p, const char *status);
  void *status_p;
};


content_type content_lookup(const char *mime_type);
struct content * content_create(content_type type, char *url);
void content_process_data(struct content *c, char *data, unsigned long size);
int content_convert(struct content *c, unsigned long width, unsigned long height);
void content_revive(struct content *c, unsigned long width, unsigned long height);
void content_reformat(struct content *c, unsigned long width, unsigned long height);
void content_destroy(struct content *c);

#endif
