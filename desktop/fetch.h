/**
 * $Id: fetch.h,v 1.1 2002/09/11 14:24:02 monkeyson Exp $
 */

#ifndef _NETSURF_DESKTOP_FETCH_H_
#define _NETSURF_DESKTOP_FETCH_H_

#include "libxml/HTMLparser.h"
#include "netsurf/render/box.h"
#include "netsurf/render/css.h"
#include "netsurf/desktop/browser.h"
#include <time.h>

typedef enum {fetch_FILE, fetch_CURL} fetch_type;
typedef enum {fetch_STATUS_SEND, fetch_STATUS_WAIT, fetch_STATUS_FETCH, fetch_STATUS_FINISH, fetch_DELETED} fetch_status;

typedef int fetch_flags;
#define fetch_DO_NOT_CHECK_CACHE       ((fetch_flags) 1);
#define fetch_DO_NOT_STORE_IN_CACHE    ((fetch_flags) 2);

struct fetch_request {
  enum {REQUEST_FROM_BROWSER} type;
  union {struct browser_window* browser;} requestor;
};

struct fetch
{
  char* location;
  fetch_type type;
  fetch_flags flags;

  fetch_status status;
  int bytes_fetched;
  int bytes_total;

  struct fetch_request* request;

  time_t start_time;

  struct fetch* next;
};

struct fetch* create_fetch(char* location, char* previous, fetch_flags f, struct fetch_request* r);
void fetch_destroy(struct fetch* f);
struct fetch* fetch_cancel(struct fetch* f);
void fetch_receive(struct fetch* f, int amount, char* bytes);
struct fetch* fetch_poll(struct fetch* f);

#endif
