/*
 * Copyright 2004-2010 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2010-2016 Vincent Sanders <vince@netsurf-browser.org>
 * Copyright 2004-2009 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>
 * Copyright 2006-2009 Daniel Silverstone <dsilvers@netsurf-browser.org>
 * Copyright 2006-2008 Rob Kendrick <rjek@netsurf-browser.org>
 * Copyright 2008 John Tytgat <joty@netsurf-browser.org>
 * Copyright 2008 Adam Blokus <adamblokus@gmail.com>
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

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <gtk/gtk.h>

#include "utils/filepath.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/file.h"
#include "utils/nsoption.h"
#include "netsurf/keypress.h"
#include "netsurf/url_db.h"
#include "netsurf/cookie_db.h"
#include "netsurf/browser_window.h"
#include "netsurf/misc.h"
#include "netsurf/netsurf.h"
#include "content/fetch.h"
#include "content/backing_store.h"
#include "desktop/save_complete.h"
#include "desktop/save_pdf.h"
#include "desktop/searchweb.h"

#include "gtk/compat.h"
#include "gtk/warn.h"
#include "gtk/completion.h"
#include "gtk/cookies.h"
#include "gtk/download.h"
#include "gtk/fetch.h"
#include "gtk/gui.h"
#include "gtk/global_history.h"
#include "gtk/hotlist.h"
#include "gtk/throbber.h"
#include "gtk/scaffolding.h"
#include "gtk/window.h"
#include "gtk/schedule.h"
#include "gtk/selection.h"
#include "gtk/search.h"
#include "gtk/ssl_cert.h"
#include "gtk/bitmap.h"
#include "gtk/resources.h"
#include "gtk/login.h"
#include "gtk/layout_pango.h"

bool nsgtk_complete = false;

char *nsgtk_config_home; /* exported global defined in gtk/gui.h */

GdkPixbuf *favicon_pixbuf; /** favicon default pixbuf */
GdkPixbuf *win_default_icon_pixbuf; /** default window icon pixbuf */
GdkPixbuf *arrow_down_pixbuf; /** arrow down pixbuf */

GtkBuilder *warning_builder;

char **respaths; /** resource search path vector */

/**
 * Cause an abnormal program termination.
 *
 * \note This never returns and is intended to terminate without any cleanup.
 *
 * \param error The message to display to the user.
 */
static void die(const char * const error)
{
	fprintf(stderr, "%s", error);
	exit(EXIT_FAILURE);
}

/**
 * Create an array of valid paths to search for resources.
 *
 * The idea is that all the complex path computation to find resources
 * is performed here, once, rather than every time a resource is
 * searched for.
 */
static char **
nsgtk_init_resource_path(const char *config_home)
{
	char *resource_path;
	int resource_path_len;
	const gchar * const *langv;
	char **pathv; /* resource path string vector */
	char **respath; /* resource paths vector */

	if (config_home != NULL) {
		resource_path_len = snprintf(NULL, 0,
					     "%s:${NETSURFRES}:%s",
					     config_home,
					     GTK_RESPATH);
		resource_path = malloc(resource_path_len + 1);
		if (resource_path == NULL) {
			return NULL;
		}
		snprintf(resource_path, resource_path_len + 1,
			 "%s:${NETSURFRES}:%s",
			 config_home,
			 GTK_RESPATH);
	} else {
		resource_path_len = snprintf(NULL, 0,
					     "${NETSURFRES}:%s",
					     GTK_RESPATH);
		resource_path = malloc(resource_path_len + 1);
		if (resource_path == NULL) {
			return NULL;
		}
		snprintf(resource_path,
			 resource_path_len + 1,
			 "${NETSURFRES}:%s",
			 GTK_RESPATH);
	}

	pathv = filepath_path_to_strvec(resource_path);

	langv = g_get_language_names();

	respath = filepath_generate(pathv, langv);

	filepath_free_strvec(pathv);

	free(resource_path);

	return respath;
}


/**
 * Set option defaults for gtk frontend.
 *
 * @param defaults The option table to update.
 * @return error status.
 */
static nserror set_defaults(struct nsoption_s *defaults)
{
	char *fname;

	/* cookie file default */
	fname = NULL;
	netsurf_mkpath(&fname, NULL, 2, nsgtk_config_home, "Cookies");
	if (fname != NULL) {
		nsoption_setnull_charp(cookie_file, fname);
	}

	/* cookie jar default */
	fname = NULL;
	netsurf_mkpath(&fname, NULL, 2, nsgtk_config_home, "Cookies");
	if (fname != NULL) {
		nsoption_setnull_charp(cookie_jar, fname);
	}

	/* url database default */
	fname = NULL;
	netsurf_mkpath(&fname, NULL, 2, nsgtk_config_home, "URLs");
	if (fname != NULL) {
		nsoption_setnull_charp(url_file, fname);
	}

	/* bookmark database default */
	fname = NULL;
	netsurf_mkpath(&fname, NULL, 2, nsgtk_config_home, "Hotlist");
	if (fname != NULL) {
		nsoption_setnull_charp(hotlist_path, fname);
	}

	/* download directory default */
	fname = getenv("HOME");
	if (fname != NULL) {
		nsoption_setnull_charp(downloads_directory, strdup(fname));
	}

	/* default path to certificates */
	nsoption_setnull_charp(ca_path, strdup("/etc/ssl/certs"));

	if ((nsoption_charp(cookie_file) == NULL) ||
	    (nsoption_charp(cookie_jar) == NULL) ||
	    (nsoption_charp(url_file) == NULL) ||
	    (nsoption_charp(hotlist_path) == NULL) ||
	    (nsoption_charp(downloads_directory) == NULL) ||
	    (nsoption_charp(ca_path) == NULL)) {
		LOG("Failed initialising default resource paths");
		return NSERROR_BAD_PARAMETER;
	}

	/* set default font names */
	nsoption_set_charp(font_sans, strdup("Sans"));
	nsoption_set_charp(font_serif, strdup("Serif"));
	nsoption_set_charp(font_mono, strdup("Monospace"));
	nsoption_set_charp(font_cursive, strdup("Serif"));
	nsoption_set_charp(font_fantasy, strdup("Serif"));

	return NSERROR_OK;
}




/**
 * Initialize GTK interface.
 */
static nserror nsgtk_init(int argc, char** argv, char **respath)
{
	char buf[PATH_MAX];
	char *resource_filename;
	char *addr = NULL;
	nsurl *url;
	nserror error;

	error = nsgtk_builder_new_from_resname("warning", &warning_builder);
	if (error != NSERROR_OK) {
		LOG("Unable to initialise warning dialog");
		return error;
	}

	gtk_builder_connect_signals(warning_builder, NULL);

	/* set default icon if its available */
	error = nsgdk_pixbuf_new_from_resname("netsurf.xpm",
					      &win_default_icon_pixbuf);
	if (error == NSERROR_OK) {
		LOG("Seting default window icon");
		gtk_window_set_default_icon(win_default_icon_pixbuf);
	}

	/* Search engine sources */
	resource_filename = filepath_find(respath, "SearchEngines");
	search_web_init(resource_filename);
	if (resource_filename != NULL) {
		LOG("Using '%s' as Search Engines file", resource_filename);
		free(resource_filename);
	}

	/* Default favicon */
	error = nsgdk_pixbuf_new_from_resname("favicon.png", &favicon_pixbuf);
	if (error != NSERROR_OK) {
		favicon_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
						false, 8, 16, 16);
	}

	/* arrow down icon */
	error = nsgdk_pixbuf_new_from_resname("arrow_down_8x32.png",
					      &arrow_down_pixbuf);
	if (error != NSERROR_OK) {
		arrow_down_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
						   false, 8, 8, 32);
	}

	/* initialise throbber */
	error = nsgtk_throbber_init();
	if (error != NSERROR_OK) {
		LOG("Unable to initialise throbber.");
		return error;
	}

	/* Initialise completions - cannot fail */
	nsgtk_completion_init();

	filepath_sfinddef(respath, buf, "mime.types", "/etc/");
	gtk_fetch_filetype_init(buf);

	save_complete_init();

	urldb_load(nsoption_charp(url_file));
	urldb_load_cookies(nsoption_charp(cookie_file));

	/* The tree view system needs to know the screen's DPI, so we
	 * find that out here, rather than when we create a first browser
	 * window.
	 */
	browser_set_dpi(gdk_screen_get_resolution(gdk_screen_get_default()));
	LOG("Set CSS DPI to %d", browser_get_dpi());

	/* Initialise top level UI elements */
	error = nsgtk_download_init();
	if (error != NSERROR_OK) {
		LOG("Unable to initialise download window.");
		return error;
	}

	/* If there is a url specified on the command line use it */
	if (argc > 1) {
		struct stat fs;
		if (stat(argv[1], &fs) == 0) {
			size_t addrlen;
			char *rp = realpath(argv[1], NULL);
			assert(rp != NULL);

			/* calculate file url length including terminator */
			addrlen = SLEN("file://") + strlen(rp) + 1;
			addr = malloc(addrlen);
			assert(addr != NULL);
			snprintf(addr, addrlen, "file://%s", rp);
			free(rp);
		} else {
			addr = strdup(argv[1]);
		}
	}
	if (addr != NULL) {
		/* managed to set up based on local launch */
	} else if (nsoption_charp(homepage_url) != NULL) {
		addr = strdup(nsoption_charp(homepage_url));
	} else {
		addr = strdup(NETSURF_HOMEPAGE);
	}

	/* create an initial browser window */
	error = nsurl_create(addr, &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(url);
	}

	free(addr);

	return error;
}



/**
 * Ensures output logging stream is correctly configured
 */
static bool nslog_stream_configure(FILE *fptr)
{
	/* set log stream to be non-buffering */
	setbuf(fptr, NULL);

	return true;
}


/**
 * Run the gtk event loop.
 *
 * The same as the standard gtk_main loop except this ensures active
 * FD are added to the gtk poll event set.
 */
static void nsgtk_main(void)
{
	fd_set read_fd_set, write_fd_set, exc_fd_set;
	int max_fd;
	GPollFD *fd_list[1000];
	unsigned int fd_count;

	while (!nsgtk_complete) {
		max_fd = -1;
		fd_count = 0;
		FD_ZERO(&read_fd_set);
		FD_ZERO(&write_fd_set);
		FD_ZERO(&exc_fd_set);

		fetch_fdset(&read_fd_set, &write_fd_set, &exc_fd_set, &max_fd);
		for (int i = 0; i <= max_fd; i++) {
			if (FD_ISSET(i, &read_fd_set)) {
				GPollFD *fd = malloc(sizeof *fd);
				fd->fd = i;
				fd->events = G_IO_IN | G_IO_HUP | G_IO_ERR;
				g_main_context_add_poll(0, fd, 0);
				fd_list[fd_count++] = fd;
			}
			if (FD_ISSET(i, &write_fd_set)) {
				GPollFD *fd = malloc(sizeof *fd);
				fd->fd = i;
				fd->events = G_IO_OUT | G_IO_ERR;
				g_main_context_add_poll(0, fd, 0);
				fd_list[fd_count++] = fd;
			}
			if (FD_ISSET(i, &exc_fd_set)) {
				GPollFD *fd = malloc(sizeof *fd);
				fd->fd = i;
				fd->events = G_IO_ERR;
				g_main_context_add_poll(0, fd, 0);
				fd_list[fd_count++] = fd;
			}
		}

		schedule_run();

		gtk_main_iteration();

		for (unsigned int i = 0; i != fd_count; i++) {
			g_main_context_remove_poll(0, fd_list[i]);
			free(fd_list[i]);
		}
	}
}


static void gui_quit(void)
{
	nserror res;

	LOG("Quitting GUI");

	/* Ensure all scaffoldings are destroyed before we go into exit */
	nsgtk_download_destroy();
	urldb_save_cookies(nsoption_charp(cookie_jar));
	urldb_save(nsoption_charp(url_file));

	res = nsgtk_cookies_destroy();
	if (res != NSERROR_OK) {
		LOG("Error finalising cookie viewer: %s",
		    messages_get_errorcode(res));
	}

	res = nsgtk_global_history_destroy();
	if (res != NSERROR_OK) {
		LOG("Error finalising global history viewer: %s",
		    messages_get_errorcode(res));
	}

	res = nsgtk_hotlist_destroy();
	if (res != NSERROR_OK) {
		LOG("Error finalising hotlist viewer: %s",
		    messages_get_errorcode(res));
	}

	free(nsgtk_config_home);

	gtk_fetch_filetype_fin();
}

static nserror gui_launch_url(struct nsurl *url)
{
	gboolean ok;
	GError *error = NULL;

	ok = nsgtk_show_uri(NULL, nsurl_access(url), GDK_CURRENT_TIME, &error);
	if (ok == TRUE) {
		return NSERROR_OK;
	}

	if (error) {
		nsgtk_warning(messages_get("URIOpenError"), error->message);
		g_error_free(error);
	}
	return NSERROR_NO_FETCH_HANDLER;
}

/* exported function documented in gtk/warn.h */
nserror nsgtk_warning(const char *warning, const char *detail)
{
	char buf[300];	/* 300 is the size the RISC OS GUI uses */
	static GtkWindow *nsgtk_warning_window;
	GtkLabel *WarningLabel;

	LOG("%s %s", warning, detail ? detail : "");
	fflush(stdout);

	nsgtk_warning_window = GTK_WINDOW(gtk_builder_get_object(warning_builder, "wndWarning"));
	WarningLabel = GTK_LABEL(gtk_builder_get_object(warning_builder,
							"labelWarning"));

	snprintf(buf, sizeof(buf), "%s %s", messages_get(warning),
			detail ? detail : "");
	buf[sizeof(buf) - 1] = 0;

	gtk_label_set_text(WarningLabel, buf);

	gtk_widget_show_all(GTK_WIDGET(nsgtk_warning_window));

	return NSERROR_OK;
}


static void nsgtk_PDF_set_pass(GtkButton *w, gpointer data)
{
	char **owner_pass = ((void **)data)[0];
	char **user_pass = ((void **)data)[1];
	GtkWindow *wnd = ((void **)data)[2];
	GtkBuilder *password_builder = ((void **)data)[3];
	char *path = ((void **)data)[4];

	char *op, *op1;
	char *up, *up1;

	op = strdup(gtk_entry_get_text(
			GTK_ENTRY(gtk_builder_get_object(password_builder,
					"entryPDFOwnerPassword"))));
	op1 = strdup(gtk_entry_get_text(
			GTK_ENTRY(gtk_builder_get_object(password_builder,
					"entryPDFOwnerPassword1"))));
	up = strdup(gtk_entry_get_text(
			GTK_ENTRY(gtk_builder_get_object(password_builder,
					"entryPDFUserPassword"))));
	up1 = strdup(gtk_entry_get_text(
			GTK_ENTRY(gtk_builder_get_object(password_builder,
					"entryPDFUserPassword1"))));


	if (op[0] == '\0') {
		gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(password_builder,
				"labelInfo")),
				"Owner password must be at least 1 character long:");
		free(op);
		free(up);
	} else if (!strcmp(op, up)) {
		gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(password_builder,
				"labelInfo")),
				"User and owner passwords must be different:");
		free(op);
		free(up);
	} else if (!strcmp(op, op1) && !strcmp(up, up1)) {

		*owner_pass = op;
		if (up[0] == '\0')
			free(up);
		else
			*user_pass = up;

		free(data);
		gtk_widget_destroy(GTK_WIDGET(wnd));
		g_object_unref(G_OBJECT(password_builder));

		save_pdf(path);

		free(path);
	} else {
		gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(password_builder,
				"labelInfo")), "Passwords not confirmed:");
		free(op);
		free(up);
	}

	free(op1);
	free(up1);
}

static void nsgtk_PDF_no_pass(GtkButton *w, gpointer data)
{
	GtkWindow *wnd = ((void **)data)[2];
	GtkBuilder *password_builder = ((void **)data)[3];
	char *path = ((void **)data)[4];

	free(data);

	gtk_widget_destroy(GTK_WIDGET(wnd));
	g_object_unref(G_OBJECT(password_builder));

	save_pdf(path);

	free(path);
}

static void nsgtk_pdf_password(char **owner_pass, char **user_pass, char *path)
{
	GtkButton *ok, *no;
	GtkWindow *wnd;
	void **data;
	GtkBuilder *password_builder;
	nserror res;

	res = nsgtk_builder_new_from_resname("password", &password_builder);
	if (res != NSERROR_OK) {
		LOG("Password UI builder init failed");
		return;
	}

	gtk_builder_connect_signals(password_builder, NULL);

	wnd = GTK_WINDOW(gtk_builder_get_object(password_builder,
						"wndPDFPassword"));

	data = malloc(5 * sizeof(void *));

	*owner_pass = NULL;
	*user_pass = NULL;

	data[0] = owner_pass;
	data[1] = user_pass;
	data[2] = wnd;
	data[3] = password_builder;
	data[4] = path;

	ok = GTK_BUTTON(gtk_builder_get_object(password_builder,
					       "buttonPDFSetPassword"));
	no = GTK_BUTTON(gtk_builder_get_object(password_builder,
					       "buttonPDFNoPassword"));

	g_signal_connect(G_OBJECT(ok), "clicked",
			 G_CALLBACK(nsgtk_PDF_set_pass), (gpointer)data);
	g_signal_connect(G_OBJECT(no), "clicked",
			 G_CALLBACK(nsgtk_PDF_no_pass), (gpointer)data);

	gtk_widget_show(GTK_WIDGET(wnd));
}


uint32_t gtk_gui_gdkkey_to_nskey(GdkEventKey *key)
{
	/* this function will need to become much more complex to support
	 * everything that the RISC OS version does.  But this will do for
	 * now.  I hope.
	 */
	switch (key->keyval) {

	case GDK_KEY(Tab):
		return NS_KEY_TAB;

	case GDK_KEY(BackSpace):
		if (key->state & GDK_SHIFT_MASK)
			return NS_KEY_DELETE_LINE_START;
		else
			return NS_KEY_DELETE_LEFT;

	case GDK_KEY(Delete):
		if (key->state & GDK_SHIFT_MASK)
			return NS_KEY_DELETE_LINE_END;
		else
			return NS_KEY_DELETE_RIGHT;

	case GDK_KEY(Linefeed):
		return 13;

	case GDK_KEY(Return):
		return 10;

	case GDK_KEY(Left):
	case GDK_KEY(KP_Left):
		return NS_KEY_LEFT;

	case GDK_KEY(Right):
	case GDK_KEY(KP_Right):
		return NS_KEY_RIGHT;

	case GDK_KEY(Up):
	case GDK_KEY(KP_Up):
		return NS_KEY_UP;

	case GDK_KEY(Down):
	case GDK_KEY(KP_Down):
		return NS_KEY_DOWN;

	case GDK_KEY(Home):
	case GDK_KEY(KP_Home):
		if (key->state & GDK_CONTROL_MASK)
			return NS_KEY_LINE_START;
		else
			return NS_KEY_TEXT_START;

	case GDK_KEY(End):
	case GDK_KEY(KP_End):
		if (key->state & GDK_CONTROL_MASK)
			return NS_KEY_LINE_END;
		else
			return NS_KEY_TEXT_END;

	case GDK_KEY(Page_Up):
	case GDK_KEY(KP_Page_Up):
		return NS_KEY_PAGE_UP;

	case GDK_KEY(Page_Down):
	case GDK_KEY(KP_Page_Down):
		return NS_KEY_PAGE_DOWN;

	case 'a':
		if (key->state & GDK_CONTROL_MASK)
			return NS_KEY_SELECT_ALL;
		return gdk_keyval_to_unicode(key->keyval);

	case 'u':
		if (key->state & GDK_CONTROL_MASK)
			return NS_KEY_DELETE_LINE;
		return gdk_keyval_to_unicode(key->keyval);

	case 'c':
		if (key->state & GDK_CONTROL_MASK)
			return NS_KEY_COPY_SELECTION;
		return gdk_keyval_to_unicode(key->keyval);

	case 'v':
		if (key->state & GDK_CONTROL_MASK)
			return NS_KEY_PASTE;
		return gdk_keyval_to_unicode(key->keyval);

	case 'x':
		if (key->state & GDK_CONTROL_MASK)
			return NS_KEY_CUT_SELECTION;
		return gdk_keyval_to_unicode(key->keyval);

	case 'Z':
	case 'y':
		if (key->state & GDK_CONTROL_MASK)
			return NS_KEY_REDO;
		return gdk_keyval_to_unicode(key->keyval);

	case 'z':
		if (key->state & GDK_CONTROL_MASK)
			return NS_KEY_UNDO;
		return gdk_keyval_to_unicode(key->keyval);

	case GDK_KEY(Escape):
		return NS_KEY_ESCAPE;

		/* Modifiers - do nothing for now */
	case GDK_KEY(Shift_L):
	case GDK_KEY(Shift_R):
	case GDK_KEY(Control_L):
	case GDK_KEY(Control_R):
	case GDK_KEY(Caps_Lock):
	case GDK_KEY(Shift_Lock):
	case GDK_KEY(Meta_L):
	case GDK_KEY(Meta_R):
	case GDK_KEY(Alt_L):
	case GDK_KEY(Alt_R):
	case GDK_KEY(Super_L):
	case GDK_KEY(Super_R):
	case GDK_KEY(Hyper_L):
	case GDK_KEY(Hyper_R):
		return 0;

	}
	return gdk_keyval_to_unicode(key->keyval);
}


/**
 * create directory name and check it is acessible and a directory.
 */
static nserror
check_dirname(const char *path, const char *leaf, char **dirname_out)
{
	nserror ret;
	char *dirname = NULL;
	struct stat dirname_stat;

	ret = netsurf_mkpath(&dirname, NULL, 2, path, leaf);
	if (ret != NSERROR_OK) {
		return ret;
	}

	/* ensure access is possible and the entry is actualy
	 * a directory.
	 */
	if (stat(dirname, &dirname_stat) == 0) {
		if (S_ISDIR(dirname_stat.st_mode)) {
			if (access(dirname, R_OK | W_OK) == 0) {
				*dirname_out = dirname;
				return NSERROR_OK;
			} else {
				ret = NSERROR_PERMISSION;
			}
		} else {
			ret = NSERROR_NOT_DIRECTORY;
		}
	} else {
		ret = NSERROR_NOT_FOUND;
	}

	free(dirname);

	return ret;
}

/**
 * Get the path to the config directory.
 *
 * @param config_home_out Path to configuration directory.
 * @return NSERROR_OK on sucess and \a config_home_out updated else error code.
 */
static nserror get_config_home(char **config_home_out)
{
	nserror ret;
	char *home_dir;
	char *xdg_config_dir;
	char *config_home;

	home_dir = getenv("HOME");

	/* The old $HOME/.netsurf/ directory should be used if it
	 * exists and is accessible.
	 */
	if (home_dir != NULL) {
		ret = check_dirname(home_dir, ".netsurf", &config_home);
		if (ret == NSERROR_OK) {
			LOG("\"%s\"", config_home);
			*config_home_out = config_home;
			return ret;
		}
	}

	/* $XDG_CONFIG_HOME defines the base directory
	 * relative to which user specific configuration files
	 * should be stored.
	 */
	xdg_config_dir = getenv("XDG_CONFIG_HOME");

	if ((xdg_config_dir == NULL) || (*xdg_config_dir == 0)) {
		/* If $XDG_CONFIG_HOME is either not set or empty, a
		 * default equal to $HOME/.config should be used.
		 */

		/** @todo the meaning of empty is never defined so I
		 * am assuming it is a zero length string but is it
		 * supposed to mean "whitespace" and if so what counts
		 * as whitespace? (are tabs etc. counted or should
		 * isspace() be used)
		 */

		/* the HOME envvar is required */
		if (home_dir == NULL) {
			return NSERROR_NOT_DIRECTORY;
		}

		ret = check_dirname(home_dir, ".config/netsurf", &config_home);
		if (ret != NSERROR_OK) {
			return ret;
		}
	} else {
		ret = check_dirname(xdg_config_dir, "netsurf", &config_home);
		if (ret != NSERROR_OK) {
			return ret;
		}
	}

	LOG("\"%s\"", config_home);

	*config_home_out = config_home;
	return NSERROR_OK;
}

static nserror create_config_home(char **config_home_out)
{
	char *config_home = NULL;
	char *home_dir;
	char *xdg_config_dir;
	nserror ret;

	LOG("Attempting to create configuration directory");

	/* $XDG_CONFIG_HOME defines the base directory
	 * relative to which user specific configuration files
	 * should be stored.
	 */
	xdg_config_dir = getenv("XDG_CONFIG_HOME");

	if ((xdg_config_dir == NULL) || (*xdg_config_dir == 0)) {
		home_dir = getenv("HOME");

		if ((home_dir == NULL) || (*home_dir == 0)) {
			return NSERROR_NOT_DIRECTORY;
		}

		ret = netsurf_mkpath(&config_home, NULL, 4, home_dir, ".config","netsurf", "/");
		if (ret != NSERROR_OK) {
			return ret;
		}
	} else {
		ret = netsurf_mkpath(&config_home, NULL, 3, xdg_config_dir, "netsurf", "/");
		if (ret != NSERROR_OK) {
			return ret;
		}
	}

	/* ensure all elements of path exist (the trailing / is required) */
	ret = netsurf_mkdir_all(config_home);
	if (ret != NSERROR_OK) {
		free(config_home);
		return ret;
	}

	/* strip the trailing separator */
	config_home[strlen(config_home) - 1] = 0;

	LOG("\"%s\"", config_home);

	*config_home_out = config_home;

	return NSERROR_OK;
}

/**
 * Get the path to the cache directory.
 *
 * @param cache_home_out Path to cache directory.
 * @return NSERROR_OK on sucess and \a cache_home_out updated else error code.
 */
static nserror get_cache_home(char **cache_home_out)
{
	nserror ret;
	char *xdg_cache_dir;
	char *cache_home;
	char *home_dir;

	/* $XDG_CACHE_HOME defines the base directory relative to
	 * which user specific non-essential data files should be
	 * stored.
	 */
	xdg_cache_dir = getenv("XDG_CACHE_HOME");

	if ((xdg_cache_dir == NULL) || (*xdg_cache_dir == 0)) {
		/* If $XDG_CACHE_HOME is either not set or empty, a
		 * default equal to $HOME/.cache should be used.
		 */

		home_dir = getenv("HOME");

		/* the HOME envvar is required */
		if (home_dir == NULL) {
			return NSERROR_NOT_DIRECTORY;
		}

		ret = check_dirname(home_dir, ".cache/netsurf", &cache_home);
		if (ret != NSERROR_OK) {
			return ret;
		}
	} else {
		ret = check_dirname(xdg_cache_dir, "netsurf", &cache_home);
		if (ret != NSERROR_OK) {
			return ret;
		}
	}

	LOG("\"%s\"", cache_home);

	*cache_home_out = cache_home;
	return NSERROR_OK;
}

static nserror create_cache_home(char **cache_home_out)
{
	char *cache_home = NULL;
	char *home_dir;
	char *xdg_cache_dir;
	nserror ret;

	LOG("Attempting to create configuration directory");

	/* $XDG_CACHE_HOME defines the base directory
	 * relative to which user specific cache files
	 * should be stored.
	 */
	xdg_cache_dir = getenv("XDG_CACHE_HOME");

	if ((xdg_cache_dir == NULL) || (*xdg_cache_dir == 0)) {
		home_dir = getenv("HOME");

		if ((home_dir == NULL) || (*home_dir == 0)) {
			return NSERROR_NOT_DIRECTORY;
		}

		ret = netsurf_mkpath(&cache_home, NULL, 4, home_dir, ".cache", "netsurf", "/");
		if (ret != NSERROR_OK) {
			return ret;
		}
	} else {
		ret = netsurf_mkpath(&cache_home, NULL, 3, xdg_cache_dir, "netsurf", "/");
		if (ret != NSERROR_OK) {
			return ret;
		}
	}

	/* ensure all elements of path exist (the trailing / is required) */
	ret = netsurf_mkdir_all(cache_home);
	if (ret != NSERROR_OK) {
		free(cache_home);
		return ret;
	}

	/* strip the trailing separator */
	cache_home[strlen(cache_home) - 1] = 0;

	LOG("\"%s\"", cache_home);

	*cache_home_out = cache_home;

	return NSERROR_OK;
}

static nserror nsgtk_option_init(int *pargc, char** argv)
{
	nserror ret;
	char *choices = NULL;

	/* user options setup */
	ret = nsoption_init(set_defaults, &nsoptions, &nsoptions_default);
	if (ret != NSERROR_OK) {
		return ret;
	}

	/* Attempt to load the user choices */
	ret = netsurf_mkpath(&choices, NULL, 2, nsgtk_config_home, "Choices");
	if (ret == NSERROR_OK) {
		nsoption_read(choices, nsoptions);
		free(choices);
	}

	/* overide loaded options with those from commandline */
	nsoption_commandline(pargc, argv, nsoptions);

	/* ensure all options fall within sensible bounds */

	/* Attempt to handle nonsense status bar widths.  These may exist
	 * in people's Choices as the GTK front end used to abuse the
	 * status bar width option by using it for an absolute value in px.
	 * The GTK front end now correctly uses it as a proportion of window
	 * width.  Here we assume that a value of less than 15% is wrong
	 * and set to the default two thirds. */
	if (nsoption_int(toolbar_status_size) < 1500) {
		nsoption_set_int(toolbar_status_size, 6667);
	}

	return NSERROR_OK;
}

static struct gui_misc_table nsgtk_misc_table = {
	.schedule = nsgtk_schedule,
	.warning = nsgtk_warning,

	.quit = gui_quit,
	.launch_url = gui_launch_url,
	.cert_verify = gtk_cert_verify,
	.login = gui_401login_open,
	.pdf_password = nsgtk_pdf_password,
};


static nserror nsgtk_messages_init(char **respaths)
{
	const char *messages;
	nserror ret;
	const uint8_t *data;
	size_t data_size;

	ret = nsgtk_data_from_resname("Messages", &data, &data_size);
	if (ret == NSERROR_OK) {
		ret = messages_add_from_inline(data, data_size);
	} else {
		/* Obtain path to messages */
		ret = nsgtk_path_from_resname("Messages", &messages);
		if (ret == NSERROR_OK) {
			ret = messages_add_from_file(messages);
		}
	}
	return ret;
}

/**
 * Main entry point from OS.
 */
int main(int argc, char** argv)
{
	char *cache_home = NULL;
	nserror ret;
	struct netsurf_table nsgtk_table = {
		.misc = &nsgtk_misc_table,
		.window = nsgtk_window_table,
		.clipboard = nsgtk_clipboard_table,
		.download = nsgtk_download_table,
		.fetch = nsgtk_fetch_table,
		.llcache = filesystem_llcache_table,
		.search = nsgtk_search_table,
		.search_web = nsgtk_search_web_table,
		.bitmap = nsgtk_bitmap_table,
		.layout = nsgtk_layout_table,
	};

	ret = netsurf_register(&nsgtk_table);
	if (ret != NSERROR_OK) {
		die("NetSurf operation table failed registration\n");
	}

	/* Locate the correct user configuration directory path */
	ret = get_config_home(&nsgtk_config_home);
	if (ret == NSERROR_NOT_FOUND) {
		/* no config directory exists yet so try to create one */
		ret = create_config_home(&nsgtk_config_home);
	}
	if (ret != NSERROR_OK) {
		LOG("Unable to locate a configuration directory.");
		nsgtk_config_home = NULL;
	}

	/* Initialise gtk */
	gtk_init(&argc, &argv);

	/* initialise logging. Not fatal if it fails but not much we
	 * can do about it either.
	 */
	nslog_init(nslog_stream_configure, &argc, argv);

	/* build the common resource path list */
	respaths = nsgtk_init_resource_path(nsgtk_config_home);
	if (respaths == NULL) {
		fprintf(stderr, "Unable to locate resources\n");
		return 1;
	}

	/* initialise the gtk resource handling */
	ret = nsgtk_init_resources(respaths);
	if (ret != NSERROR_OK) {
		fprintf(stderr, "GTK resources failed to initialise (%s)\n",
			messages_get_errorcode(ret));
		return 1;
	}

	/* Initialise user options */
	ret = nsgtk_option_init(&argc, argv);
	if (ret != NSERROR_OK) {
		fprintf(stderr, "Options failed to initialise (%s)\n",
			messages_get_errorcode(ret));
		return 1;
	}

	/* Initialise translated messages */
	ret = nsgtk_messages_init(respaths);
	if (ret != NSERROR_OK) {
		fprintf(stderr, "Unable to load translated messages (%s)\n",
			messages_get_errorcode(ret));
		LOG("Unable to load translated messages");
		/** \todo decide if message load faliure should be fatal */
	}

	/* Locate the correct user cache directory path */
	ret = get_cache_home(&cache_home);
	if (ret == NSERROR_NOT_FOUND) {
		/* no cache directory exists yet so try to create one */
		ret = create_cache_home(&cache_home);
	}
	if (ret != NSERROR_OK) {
		LOG("Unable to locate a cache directory.");
	}

	/* core initialisation */
	ret = netsurf_init(cache_home);
	free(cache_home);
	if (ret != NSERROR_OK) {
		fprintf(stderr, "NetSurf core failed to initialise (%s)\n",
			messages_get_errorcode(ret));
		return 1;
	}

	/* run the browser */
	ret = nsgtk_init(argc, argv, respaths);
	if (ret != NSERROR_OK) {
		fprintf(stderr, "NetSurf gtk initialise failed (%s)\n",
			messages_get_errorcode(ret));
	} else {
		nsgtk_main();
	}

	/* common finalisation */
	netsurf_exit();

	/* finalise options */
	nsoption_finalise(nsoptions, nsoptions_default);

	return 0;
}
