/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#define NDEBUG

#include <assert.h>
#include <string.h>
#include "netsurf/desktop/401login.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

void login_list_dump(void);

/**
 * Pointer into the linked list
 */
static struct login login = {0, 0, &login, &login};
static struct login *loginlist = &login;

/**
 * Adds an item to the list of login details
 */
void login_list_add(char *host, char* logindets) {

  struct login *nli = xcalloc(1, sizeof(*nli));
  char *temp = get_host_from_url(host);
  char *i;

  /* Go back to the path base ie strip the document name
   * eg. http://www.blah.com/blah/test.htm becomes
   *     http://www.blah.com/blah/
   * This does, however, mean that directories MUST have a '/' at the end
   */
  if (strlen(temp) < strlen(host)) {

    xfree(temp);
    temp = xstrdup(host);
    if (temp[strlen(temp)-1] != '/') {
      i = strrchr(temp, '/');
      temp[(i-temp)+1] = 0;
    }
  }

  nli->host = xstrdup(temp);
  nli->logindetails = xstrdup(logindets);
  nli->prev = loginlist->prev;
  nli->next = loginlist;
  loginlist->prev->next = nli;
  loginlist->prev = nli;

  LOG(("Adding %s", temp));
#ifndef NDEBUG
  login_list_dump();
#endif
  xfree(temp);
}

/**
 * Retrieves an element from the login list
 */
struct login *login_list_get(char *host) {

  struct login *nli;
  char *temp, *temphost;
  char *i;
  int reached_scheme = 0;

  if (host == NULL)
    return NULL;

  if (strncasecmp(host, "http", 4) != 0)
    return NULL;

  temphost = get_host_from_url(host);
  temp = xstrdup(host);

  /* Smallest thing to check for is the scheme + host name + trailing '/'
   * So make sure we've got that at least
   */
  if (strlen(temphost) > strlen(temp)) {
    temp = get_host_from_url(host);
  }

  /* Work backwards through the path, directory at at time.
   * Finds the closest match.
   * eg. http://www.blah.com/moo/ matches the url
   *     http://www.blah.com/moo/test/index.htm
   * This allows multiple realms (and login details) per host.
   * Only one set of login details per realm are allowed.
   */
  do {

    LOG(("%s, %d", temp, strlen(temp)));

    for (nli = loginlist->next; nli != loginlist &&
                                (strcasecmp(nli->host, temp)!=0);
         nli = nli->next) ;

    if (nli != loginlist) {
      LOG(("Got %s", nli->host));
      xfree(temphost);
      return nli;
    }
    else {

      if (temp[strlen(temp)-1] == '/') {
        temp[strlen(temp)-1] = 0;
      }

      i = strrchr(temp, '/');

      if (temp[(i-temp)-1] != '/') /* reached the scheme? */
        temp[(i-temp)+1] = 0;
      else {
        reached_scheme = 1;
      }
    }
  } while (reached_scheme == 0);

  xfree(temphost);
  return NULL;
}

/**
 * Remove a realm's login details from the list
 */
void login_list_remove(char *host) {

  struct login *nli = login_list_get(host);

  if (nli != NULL) {
    nli->prev->next = nli->next;
    nli->next->prev = nli->prev;
    xfree(nli->logindetails);
    xfree(nli->host);
    xfree(nli);
  }

  LOG(("Removing %s", host));
#ifndef NDEBUG
  login_list_dump();
#endif
}

/**
 * Dumps the list of login details (base paths only)
 */
void login_list_dump(void) {

  struct login *nli;

  for (nli = loginlist->next; nli != loginlist; nli = nli->next) {
    LOG(("%s", nli->host));
  }
}
