/**
 * $Id: fetch.c,v 1.7 2003/01/06 00:04:43 bursa Exp $
 */

#include "libxml/HTMLparser.h"
#include "netsurf/render/box.h"
#include "netsurf/render/css.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/fetch.h"
#include "netsurf/render/utils.h"
#include "netsurf/utils/log.h"
#include "curl/curl.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

struct fetch* create_fetch(char* location, char* previous, fetch_flags f, struct fetch_request* r)
{
  struct fetch* fetch = (struct fetch*) xcalloc(1, sizeof(struct fetch));

  if (fetch->location != NULL)
    free(fetch->location);

  fetch->location = xstrdup(location);
  fetch->type = fetch_CURL;

  fetch->flags = f;

  fetch->status = fetch_STATUS_WAIT;
  fetch->bytes_fetched = 0;
  fetch->bytes_total = -1;

  fetch->request = r;

  fetch->start_time = time(&fetch->start_time);

  fetch->next = netsurf_fetches;
  netsurf_fetches = fetch;

  return fetch;
}

void fetch_destroy(struct fetch* f)
{
  if (f == NULL)
    return;

  if (netsurf_fetches == f)
    netsurf_fetches = f->next;
  else
  {
    struct fetch* ff = netsurf_fetches;
    while (ff->next != f && ff->next != NULL)
      ff = ff->next;
    if (ff->next == f)
      ff->next = f->next;
  }

  xfree(f->location);
  xfree(f->request);
  xfree(f);
}

struct fetch* fetch_cancel(struct fetch* f)
{
  if (f == NULL)
    return NULL;

  /* may need to contact server here */

  f->status = fetch_DELETED;
  /* fetch may not necessarily be destroyed if the cancelling can't be done
     instantly */
  return f;
}

void fetch_receive(struct fetch* f, int amount, char* bytes)
{
  struct browser_message msg;

  f->bytes_fetched = f->bytes_fetched + amount;

  switch (f->request->type)
  {
    case REQUEST_FROM_BROWSER:
      msg.type = msg_FETCH_DATA;
      msg.f = f;
      msg.data.fetch_data.block = bytes;
      msg.data.fetch_data.block_size = amount;
      if (browser_window_message(f->request->requestor.browser, &msg) != 0)
      {
        fetch_cancel(f);
        return;
      }
      break;
    default:
      break;
  }

  if (f->bytes_fetched >= f->bytes_total && f->bytes_total != -1)
  {
    msg.type = msg_FETCH_FINISHED;
    msg.f = f;
    browser_window_message(f->request->requestor.browser, &msg);
    fetch_destroy(f);
  }

  return;
}

size_t fetch_curl_data(void * data, size_t size, size_t nmemb, struct fetch* f)
{
  struct browser_message msg;
  msg.type = msg_FETCH_DATA;
  msg.f = f;
  msg.data.fetch_data.block = data;
  msg.data.fetch_data.block_size = size * nmemb;
  LOG(("sending curl's FETCH_DATA to browser"));
  browser_window_message(f->request->requestor.browser, &msg);
  return size * nmemb;
}

struct fetch* fetch_poll(struct fetch* f)
{
  struct fetch* ret = f;

/*   LOG(("polling...")); */

  if (f == NULL)
  {
/*     LOG(("null fetch; returning")); */
    return f;
  }

  if (f->status == fetch_DELETED)
  {
    ret = f->next;
    LOG(("deleting marked fetch"));
    fetch_destroy(f);
    LOG(("moving on..."));
    return fetch_poll(ret);
  }
  else if (f->type == fetch_CURL && f->status == fetch_STATUS_WAIT)
  {
    struct browser_message msg;
    CURL* curl;

    LOG(("init curl"));
    curl = curl_easy_init();
    LOG(("init curl returned"));
    if (curl != 0)
    {
      LOG(("init curl OK"));
      /* shouldn't assume this!  somehow work it out instead. */
      msg.type = msg_FETCH_FETCH_INFO;
      msg.f = f;
      msg.data.fetch_info.type = type_HTML;
      msg.data.fetch_info.total_size = -1;

      if (browser_window_message(f->request->requestor.browser, &msg) == 0)
      {
        LOG(("about to set options"));
        curl_easy_setopt(curl, CURLOPT_URL, f->location);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fetch_curl_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "NetSurf/0.00 (alpha)");
        LOG(("about to perform"));
        curl_easy_perform(curl);
        LOG(("about to cleanup"));
        curl_easy_cleanup(curl);

        LOG(("cleanup finished"));
        msg.type = msg_FETCH_FINISHED;
        msg.f = f;
        LOG(("sending FETCH_FINISHED to browser"));
        browser_window_message(f->request->requestor.browser, &msg);
        LOG(("FETCH_FINISHED accepted"));

        ret = f->next;
        LOG(("Destroying f"));
        fetch_destroy(f);
        LOG(("Moving on..."));
        return fetch_poll(ret);
      }
      LOG(("about to cleanup since requestor went funny"));
      curl_easy_cleanup(curl);

      LOG(("Requesting browser didn't like something"));
      ret = f->next;
      LOG(("Cancelling fetch"));
      f = fetch_cancel(f);
      return fetch_poll(ret);
    }

    LOG(("we are aborting the mission"));
    msg.type = msg_FETCH_ABORT;
    msg.f = f;
    browser_window_message(f->request->requestor.browser, &msg);
    LOG(("ABORT message sent to browser"));

    ret = f->next;
    fetch_destroy(f);
    return fetch_poll(ret); /* carry on polling */
  }

  LOG(("Moving on (at end of function with f->next)"));
  f->next = fetch_poll(f->next);
  return f;
}

