/**
 * $Id: fetch.c,v 1.5 2002/12/25 21:38:45 bursa Exp $
 */

#include "libxml/HTMLparser.h"
#include "netsurf/render/box.h"
#include "netsurf/render/css.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/netsurf.h"
#include "netsurf/desktop/fetch.h"
#include "netsurf/render/utils.h"
#include "curl/curl.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

void fetch_identify_location(struct fetch* f, char* location, char* previous)
{
  if (f->location != NULL)
    xfree(f->location);

  f->location = xstrdup(location);

  if (strspn(location, "file:/") == strlen("file:/"))
    f->type = fetch_FILE;
  else
    /* throw everything else at curl, since it can fetch lots of protocols */
    f->type = fetch_CURL;
}

struct fetch* create_fetch(char* location, char* previous, fetch_flags f, struct fetch_request* r)
{
  struct fetch* fetch = (struct fetch*) xcalloc(1, sizeof(struct fetch));

  fetch_identify_location(fetch, location, previous);

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
  Log("fetch_poll","sending curl's FETCH_DATA to browser");
  browser_window_message(f->request->requestor.browser, &msg);
  return size * nmemb;
}

struct fetch* fetch_poll(struct fetch* f)
{
  struct fetch* ret = f;

  Log("fetch_poll","polling...");

  if (f == NULL)
  {
    Log("fetch_poll","null fetch; returning");
    return f;
  }

  if (f->type == fetch_DELETED)
  {
    ret = f->next;
    Log("fetch_poll", "deleting marked fetch");
    fetch_destroy(f);
    Log("fetch_poll", "moving on...");
    return fetch_poll(ret);
  }
  else if (f->type == fetch_CURL && f->status == fetch_STATUS_WAIT)
  {
    struct browser_message msg;
    CURL* curl;

    Log("fetch_poll","init curl");
    curl = curl_easy_init();
    Log("fetch_poll","init curl returned");
    if (curl != 0)
    {
      Log("fetch_poll","init curl OK");
      /* shouldn't assume this!  somehow work it out instead. */
      msg.type = msg_FETCH_FETCH_INFO;
      msg.f = f;
      msg.data.fetch_info.type = type_HTML;
      msg.data.fetch_info.total_size = -1;

      if (browser_window_message(f->request->requestor.browser, &msg) == 0)
      {
        Log("fetch_poll","about to set options");
        curl_easy_setopt(curl, CURLOPT_URL, f->location);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fetch_curl_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "NetSurf");
        Log("fetch_poll","about to perform");
        curl_easy_perform(curl);
        Log("fetch_poll","about to cleanup");
        curl_easy_cleanup(curl);

        Log("fetch_poll","cleanup finished");
        msg.type = msg_FETCH_FINISHED;
        msg.f = f;
        Log("fetch_poll","sending FETCH_FINISHED to browser");
        browser_window_message(f->request->requestor.browser, &msg);
        Log("fetch_poll","FETCH_FINISHED accepted");

        ret = f->next;
        Log("fetch_poll","Destroying f");
        fetch_destroy(f);
        Log("fetch_poll","Moving on...");
        return fetch_poll(ret);
      }
      Log("fetch_poll","about to cleanup since requestor went funny");
      curl_easy_cleanup(curl);

      Log("fetch_poll","Requesting browser didn't like something");
      ret = f->next;
      Log("fetch_poll","Cancelling fetch");
      f = fetch_cancel(f);
      return fetch_poll(ret);
    }

    Log("fetch_poll","we are aborting the mission");
    msg.type = msg_FETCH_ABORT;
    msg.f = f;
    browser_window_message(f->request->requestor.browser, &msg);
    Log("fetch_poll","ABORT message sent to browser");

    ret = f->next;
    fetch_destroy(f);
    return fetch_poll(ret); /* carry on polling */
  }
  else if (f->type == fetch_FILE && f->status == fetch_STATUS_WAIT)
  {
    struct browser_message msg;
    char actual_filename[1024];
    FILE* in;

    gui_file_to_filename(f->location, actual_filename, 1024);
/*    in = fopen("files","a");
    fprintf(in, "%s\n%s\n\n",f->location, actual_filename);
    fclose(in);*/
    in = fopen(actual_filename, "r");

    if (in == NULL)
    {
      /* can't open file -- send abort to requestor, then destroy */
      Log("fetch_poll","can't open file");
      msg.type = msg_FETCH_ABORT;
      msg.f = f;
      browser_window_message(f->request->requestor.browser, &msg);
      Log("fetch_poll","ABORT message sent to browser");

      ret = f->next;
      fetch_destroy(f);
      Log("fetch_poll","destroyed f; moving on");

      return fetch_poll(ret); /* carry on polling */
    }
    else
    {
      /* file opened successfully.  now to send size and type to requestor,
         then the data, then finish. */
      int size;

      /* calculate size */
      Log("fetch_poll","calculating file size");
      fseek(in, 0, SEEK_END);
      size = (int) ftell(in);
      fclose(in);

      /* send file info. (assuming HTML at the mo, but should work out
         what it is, somehow) */
      msg.type = msg_FETCH_FETCH_INFO;
      msg.f = f;
      msg.data.fetch_info.type = type_HTML;
      msg.data.fetch_info.total_size = size;

      Log("fetch_poll","sending FETCH_INFO to browser");
      if (browser_window_message(f->request->requestor.browser, &msg) == 0)
      {
        /* file info accepted. can now load the data and send it */
        Log("fetch_poll","FETCH_INFO accepted");
        f->status = fetch_STATUS_FETCH;

        /* load and send data */
        msg.type = msg_FETCH_DATA;
        msg.f = f;
        msg.data.fetch_data.block = load(actual_filename);
        msg.data.fetch_data.block_size = size;
        Log("fetch_poll","sending FETCH_DATA to browser");
        if (browser_window_message(f->request->requestor.browser, &msg) == 0)
        {
          xfree(msg.data.fetch_data.block);
          /* data accepted.  no more data, so finish */
          Log("fetch_poll","FETCH_DATA accepted");
          f->status = fetch_STATUS_FINISH;

          /* send finish */
          msg.type = msg_FETCH_FINISHED;
          msg.f = f;
          Log("fetch_poll","sending FETCH_FINISHED to browser");
          browser_window_message(f->request->requestor.browser, &msg);
          Log("fetch_poll","FETCH_FINISHED accepted");

          ret = f->next;
          Log("fetch_poll","Destroying f");
          fetch_destroy(f);
          Log("fetch_poll","Moving on...");
          return fetch_poll(ret);
          /* destroy this fetch, then move on to next fetch to poll */
        }
        xfree(msg.data.fetch_data.block);
      }

      /* requestor didn't like something, and wants the fetch cancelled */
      Log("fetch_poll","Requesting browser didn't like something");
      ret = f->next;
      Log("fetch_poll","Cancelling fetch");
      f = fetch_cancel(f);
      return fetch_poll(ret);
    }
  }

  Log("fetch_poll","Moving on (at end of function with f->next)");
  f->next = fetch_poll(f->next);
  return f;
}

