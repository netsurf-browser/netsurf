/*
 * Copyright 2011 Daniel Silverstone <dsilvers@digital-scurf.org>
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

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>

#include "monkey/filetype.h"
#include "monkey/options.h"
#include "monkey/poll.h"
#include "monkey/dispatch.h"
#include "monkey/browser.h"

#include "content/urldb.h"
#include "content/fetchers/resource.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/sslcert.h"
#include "utils/resource.h"
#include "utils/url.h"

char *default_stylesheet_url = NULL;
char *quirks_stylesheet_url = NULL;
char *adblock_stylesheet_url = NULL;

static char **respaths; /** resource search path vector */

/* Stolen from gtk/gui.c */
static char **
nsmonkey_init_resource(const char *resource_path)
{
	const gchar * const *langv;
	char **pathv; /* resource path string vector */
	char **respath; /* resource paths vector */

	pathv = resource_path_to_strvec(resource_path);

	langv = g_get_language_names();

	respath = resource_generate(pathv, langv);

	resource_free_strvec(pathv);

	return respath;
}

void gui_quit(void)
{
  urldb_save_cookies(option_cookie_jar);
  urldb_save(option_url_file);
  sslcert_cleanup();
  free(default_stylesheet_url);
  free(quirks_stylesheet_url);
  free(adblock_stylesheet_url);
  free(option_cookie_file);
  free(option_cookie_jar);
  gtk_fetch_filetype_fin();
}

char* gui_find_resource(const char *filename)
{
	char buf[PATH_MAX];
	return path_to_url(resource_sfind(respaths, buf, filename));
}

void
gui_launch_url(const char *url)
{
  fprintf(stdout, "GENERIC LAUNCH URL %s\n", url);
}

static void quit_handler(int argc, char **argv)
{
  netsurf_quit = true;
}

int
main(int argc, char **argv)
{
  char *messages;
  char *options;
  char buf[PATH_MAX];
  
  /* Unbuffer stdin/out/err */
  setbuf(stdin, NULL);
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);
  
  /* Prep the search paths */
  respaths = nsmonkey_init_resource("${HOME}/.netsurf/:${NETSURFRES}:"MONKEY_RESPATH":./monkey/res");
  
  options = resource_find(respaths, "Choices");
  messages = resource_find(respaths, "Messages");

  netsurf_init(&argc, &argv, options, messages);
  
  free(messages);
  free(options);
    
  resource_sfinddef(respaths, buf, "mime.types", "/etc/");
  gtk_fetch_filetype_init(buf);
  
  default_stylesheet_url = strdup("resource:gtkdefault.css");
  quirks_stylesheet_url = strdup("resource:quirks.css");
  adblock_stylesheet_url = strdup("resource:adblock.css");
  
  urldb_load(option_url_file);
  urldb_load_cookies(option_cookie_file);

  sslcert_init("content.png");
  
  monkey_prepare_input();
  monkey_register_handler("QUIT", quit_handler);
  monkey_register_handler("WINDOW", monkey_window_handle_command);
  
  fprintf(stdout, "GENERIC STARTED\n");
  netsurf_main_loop();
  fprintf(stdout, "GENERIC CLOSING_DOWN\n");
  netsurf_exit();
  fprintf(stdout, "GENERIC FINISHED\n");
  return 0;
}
