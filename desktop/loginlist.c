/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <string.h>
#include "netsurf/desktop/401login.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#define NDEBUG

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
  char *temp = xstrdup(host);
  char *i;

  /* Go back to the path base ie strip the document name
   * eg. http://www.blah.com/blah/test.htm becomes
   *     http://www.blah.com/blah/
   * This does, however, mean that directories MUST have a '/' at the end
   */
  if (temp[strlen(temp)-1] != '/') {
    i = strrchr(temp, '/');
    temp[(i-temp)+1] = 0;
  }

  nli->host = xstrdup(temp);
  nli->logindetails = xstrdup(logindets);
  nli->prev = loginlist->prev;
  nli->next = loginlist;
  loginlist->prev->next = nli;
  loginlist->prev = nli;

  LOG(("Adding %s : %s", temp, logindets));
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
  char* i;

  if (host == NULL)
    return NULL;

  temphost = get_host_from_url(host);
  temp = xstrdup(host);

  /* Work backwards through the path, directory at at time.
   * Finds the closest match.
   * eg. http://www.blah.com/moo/ matches the url
   *     http://www.blah.com/moo/test/index.htm
   * This allows multiple realms (and login details) per host.
   * Only one set of login details per realm are allowed.
   */
  while (strcasecmp(temp, temphost) != 0) {

    LOG(("%s, %d", temp, strlen(temp)));

    for (nli = loginlist->next; nli != loginlist &&
                                (strcasecmp(nli->host, temp)!=0);
         nli = nli->next) ;

    if (temp[strlen(temp)-1] == '/') {
      temp[strlen(temp)-1] = 0;
    }

    i = strrchr(temp, '/');

    temp[(i-temp)+1] = 0;

    if (nli != loginlist) {
      LOG(("Got %s", nli->host));
      xfree(temphost);
      return nli;
    }
  }

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
