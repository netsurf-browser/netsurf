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
#include "utils/filepath.h"
#include "utils/url.h"

static char **respaths; /** resource search path vector */

/* Stolen from gtk/gui.c */
static char **
nsmonkey_init_resource(const char *resource_path)
{
	const gchar * const *langv;
	char **pathv; /* resource path string vector */
	char **respath; /* resource paths vector */

	pathv = filepath_path_to_strvec(resource_path);

	langv = g_get_language_names();

	respath = filepath_generate(pathv, langv);

	filepath_free_strvec(pathv);

	return respath;
}

void gui_quit(void)
{
  urldb_save_cookies(option_cookie_jar);
  urldb_save(option_url_file);
  sslcert_cleanup();
  free(option_cookie_file);
  free(option_cookie_jar);
  gtk_fetch_filetype_fin();
}

nsurl *gui_get_resource_url(const char *path)
{
	char buf[PATH_MAX];
	char *raw;
	nsurl *url = NULL;

	raw = path_to_url(filepath_sfind(respaths, buf, path));
	if (raw != NULL) {
		nsurl_create(raw, &url);
		free(raw);
	}

	return url;
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
  
  options = filepath_find(respaths, "Choices");
  messages = filepath_find(respaths, "Messages");

  netsurf_init(&argc, &argv, options, messages);
  
  free(messages);
  free(options);
    
  filepath_sfinddef(respaths, buf, "mime.types", "/etc/");
  gtk_fetch_filetype_init(buf);
  
  urldb_load(option_url_file);
  urldb_load_cookies(option_cookie_file);

  sslcert_init("content.png");
  
  monkey_prepare_input();
  monkey_register_handler("QUIT", quit_handler);
  monkey_register_handler("WINDOW", monkey_window_handle_command);
  
  fprintf(stdout, "GENERIC STARTED\n");
  netsurf_main_loop();
  fprintf(stdout, "GENERIC CLOSING_DOWN\n");
  monkey_kill_browser_windows();
  
  netsurf_exit();
  fprintf(stdout, "GENERIC FINISHED\n");
  return 0;
}
