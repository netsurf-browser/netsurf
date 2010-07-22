/*
 * Copyright 2004-2010 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2010 Vincent Sanders <vince@debian.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <curl/curl.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <hubbub/hubbub.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "content/content.h"
#include "content/fetch.h"
#include "content/fetchers/fetch_curl.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/401login.h"
#include "desktop/browser.h"
#include "desktop/cookies.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "desktop/save_pdf/pdf_plotters.h"
#include "desktop/searchweb.h"
#include "desktop/textinput.h"
#include "gtk/gtk_gui.h"
#include "gtk/dialogs/gtk_options.h"
#include "gtk/gtk_completion.h"
#include "gtk/gtk_window.h"
#include "gtk/options.h"
#include "gtk/gtk_throbber.h"
#include "gtk/gtk_history.h"
#include "gtk/gtk_filetype.h"
#include "gtk/gtk_download.h"
#include "gtk/gtk_schedule.h"
#include "render/box.h"
#include "render/form.h"
#include "render/html.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utf8.h"
#include "utils/utils.h"

char *default_stylesheet_url;
char *quirks_stylesheet_url;
char *adblock_stylesheet_url;
char *options_file_location;
char *glade_netsurf_file_location;
char *glade_password_file_location;
char *glade_warning_file_location;
char *glade_login_file_location;
char *glade_ssl_file_location;
char *glade_toolbar_file_location;
char *toolbar_indices_file_location;
char *res_dir_location;
char *print_options_file_location;

GtkWindow *wndWarning;
GladeXML *gladeNetsurf;
GladeXML *gladePassword;
GladeXML *gladeWarning;
GladeXML *gladeLogin;
GladeXML *gladeSsl;

static GtkWidget *select_menu;
static struct browser_window *select_menu_bw;
static struct form_control *select_menu_control;

static void nsgtk_init_glade(void);
static void nsgtk_check_homedir(void);
static bool nsgtk_throbber_init(int framec);
static void nsgtk_ssl_accept(GtkButton *w, gpointer data);
static void nsgtk_ssl_reject(GtkButton *w, gpointer data);
static void nsgtk_select_menu_clicked(GtkCheckMenuItem *checkmenuitem,
					gpointer user_data);
#ifdef WITH_PDF_EXPORT
static void nsgtk_PDF_set_pass(GtkButton *w, gpointer data);
static void nsgtk_PDF_no_pass(GtkButton *w, gpointer data);
#endif

#define THROBBER_FRAMES 9

static char *
nsgtk_vsfindfile(char *str, const char *format, va_list ap)
{
	char *realpathname;
	char *pathname;
	int len;

	pathname = malloc(PATH_MAX);
	if (pathname == NULL)
		return NULL; /* unable to allocate memory */

	len = vsnprintf(pathname, PATH_MAX, format, ap);

	if ((len < 0) || (len >= PATH_MAX)) {
		/* error or output exceeded PATH_MAX length so
		 * operation is doomed to fail.
		 */
		free(pathname);
		return NULL;
	}

	realpathname = realpath(pathname, str);
	
	free(pathname);
	
	if (realpathname != NULL) {
		/* sucessfully expanded pathname */
		if (access(realpathname, R_OK) != 0) {
			/* unable to read the file */
			return NULL;
		}	
	}

	return realpathname;
}

static char *
nsgtk_sfindfile(char *str, const char *format, ...)
{
	va_list ap;
	char *ret;

	va_start(ap, format);
	ret = nsgtk_vsfindfile(str, format, ap);
	va_end(ap);

	return ret;
}

/* create a normalised path 
 *
 * if the file described by the format exists and is accessible the
 * normalised path is returned or NULL if not.
 */
static char *
nsgtk_findfile(const char *format, ...)
{
	char *str;
	char *ret;
	va_list ap;

	str = malloc(PATH_MAX);
	if (str == NULL)
		return NULL; /* unable to allocate memory */

	va_start(ap, format);
	ret = nsgtk_vsfindfile(str, format, ap);
	va_end(ap);

	if (ret == NULL)
		free(str);

	return ret;
}


static char *
nsgtk_find_messages(void)
{
	int lang_loop = 0;
	char *buf;
	const gchar * const *lang_list;

	lang_list = g_get_language_names();

	while (lang_list[lang_loop] != NULL) {
		buf = nsgtk_findfile("%s/%s/Messages", getenv("NETSURFRES"), lang_list[lang_loop]);

		if (buf != NULL)
			return buf;

		buf = nsgtk_findfile("%s/%s/Messages", GTK_RESPATH, lang_list[lang_loop]);
		if (buf != NULL)
			return buf;

		lang_loop++;
	}

	buf = nsgtk_findfile("./gtk/res/en/Messages");
	if (buf != NULL)
		return buf;

	buf = strdup("./gtk/res/messages");

	return buf;
}

/**
 * Locate a shared resource file by searching known places in order.
 *
 * \param  buf      buffer to write to.  must be at least PATH_MAX chars. May be NULL and routine will allocate string which must be freed by caller.
 * \param  filename file to look for
 * \param  def      default to return if file not found
 * \return buf
 *
 * Search order is: ~/.netsurf/, $NETSURFRES/ (where NETSURFRES is an
 * environment variable), and finally the path specified by the define
 * GTK_RESPATH.
 */
static char *
nsgtk_find_resource(char *buf, const char *filename, const char *def)
{
	if (buf == NULL) {
		buf = malloc(PATH_MAX);
		if (buf == NULL)
			return NULL;
	}

	if (nsgtk_sfindfile(buf, "%s/.netsurf/%s", getenv("HOME"), filename) != NULL)
		return buf;

	if (nsgtk_sfindfile(buf, "%s/%s", getenv("NETSURFRES"), filename) != NULL)
		return buf;

	if (nsgtk_sfindfile(buf, "%s/%s", GTK_RESPATH, filename) != NULL)
		return buf;


	if (def[0] == '~') {
		char t[PATH_MAX];
		snprintf(t, PATH_MAX, "%s%s", getenv("HOME"), def + 1);
		if (realpath(t, buf) == NULL) {
                        strcpy(buf, t);
                }
	} else {
		if (realpath(def, buf) == NULL) {
                        strcpy(buf, def);
                }
	}

	return buf;
}


/**
 * Initialize GTK interface.
 */
static void gui_init(int argc, char** argv)
{
	char buf[PATH_MAX];
	struct browser_window *bw;
	const char *addr = NETSURF_HOMEPAGE;

	nsgtk_check_homedir();

	nsgtk_find_resource(buf, "netsurf.glade", "./gtk/res/netsurf.glade");
	buf[strlen(buf) - 13] = 0;
	LOG(("Using '%s' as Resources directory", buf));
	res_dir_location = strdup(buf);

	nsgtk_init_glade();

	nsgtk_find_resource(buf, "Aliases", "./gtk/res/Aliases");
	LOG(("Using '%s' as Aliases file", buf));
	if (hubbub_initialise(buf, ns_realloc, NULL) != HUBBUB_OK)
		die("Unable to initialise HTML parsing library.\n");

	nsgtk_find_resource(buf, "netsurf.xpm", "./gtk/res/netsurf.xpm");
	gtk_window_set_default_icon_from_file(buf, NULL);

	nsgtk_completion_init();

	if (nsgtk_throbber_init(THROBBER_FRAMES) == false)
		die("Unable to load throbber image.\n");

	option_core_select_menu = true;

	/* Attempt to handle nonsense status bar widths.  These may exist
	 * in people's Choices as the GTK front end used to abuse the
	 * status bar width option by using it for an absolute value in px.
	 * The GTK front end now correctly uses it as a proportion of window
	 * width.  Here we assume that a value of less than 15% is wrong
	 * and set to the default two thirds. */
	if (option_toolbar_status_width < 1500) {
		option_toolbar_status_width = 6667;
	}

	/* check what the font settings are, setting them to a default font
	 * if they're not set - stops Pango whinging
	 */
#define SETFONTDEFAULT(x,y) (x) = ((x) != NULL) ? (x) : strdup((y))
	SETFONTDEFAULT(option_font_sans, "Sans");
	SETFONTDEFAULT(option_font_serif, "Serif");
	SETFONTDEFAULT(option_font_mono, "Monospace");
	SETFONTDEFAULT(option_font_cursive, "Serif");
	SETFONTDEFAULT(option_font_fantasy, "Serif");

	if (!option_cookie_file) {
		nsgtk_find_resource(buf, "Cookies", "~/.netsurf/Cookies");
		LOG(("Using '%s' as Cookies file", buf));
		option_cookie_file = strdup(buf);
	}
	if (!option_cookie_jar) {
		nsgtk_find_resource(buf, "Cookies", "~/.netsurf/Cookies");
		LOG(("Using '%s' as Cookie Jar file", buf));
		option_cookie_jar = strdup(buf);
	}
	if (!option_cookie_file || !option_cookie_jar)
		die("Failed initialising cookie options");

	if (!option_url_file) {
		nsgtk_find_resource(buf, "URLs", "~/.netsurf/URLs");
		LOG(("Using '%s' as URL file", buf));
		option_url_file = strdup(buf);
	}

        if (!option_ca_path) {
                nsgtk_find_resource(buf, "certs", "/etc/ssl/certs");
                LOG(("Using '%s' as certificate path", buf));
                option_ca_path = strdup(buf);
        }

        if (!option_downloads_directory) {
        	char *home = getenv("HOME");
        	LOG(("Using '%s' as download directory", home));
        	option_downloads_directory = home;
	}


	nsgtk_find_resource(buf, "mime.types", "/etc/mime.types");
	gtk_fetch_filetype_init(buf);

	/* set up stylesheet urls */
	nsgtk_find_resource(buf, "gtkdefault.css", "./gtk/res/gtkdefault.css");
	default_stylesheet_url = path_to_url(buf);
	LOG(("Using '%s' as Default CSS URL", default_stylesheet_url));

	nsgtk_find_resource(buf, "quirks.css", "./gtk/res/quirks.css");
	quirks_stylesheet_url = path_to_url(buf);

	nsgtk_find_resource(buf, "adblock.css", "./gtk/res/adblock.css");
	adblock_stylesheet_url = path_to_url(buf);
	LOG(("Using '%s' as AdBlock CSS URL", adblock_stylesheet_url));

	nsgtk_find_resource(buf, "Print", "~/.netsurf/Print");
	LOG(("Using '%s' as Print Settings file", buf));
	print_options_file_location = strdup(buf);

	nsgtk_find_resource(buf, "SearchEngines", "./gtk/res/SearchEngines");
	LOG(("Using '%s' as Search Engines file", buf));
	search_engines_file_location = strdup(buf);

	nsgtk_find_resource(buf, "default.ico", "./gtk/res/default.ico");
	LOG(("Using '%s' as default search ico", buf));
	search_default_ico_location = strdup(buf);

	nsgtk_find_resource(buf, "toolbarIndices", "./gtk/res/toolbarIndices");
	LOG(("Using '%s' as custom toolbar settings file", buf));
	toolbar_indices_file_location = strdup(buf);

	urldb_load(option_url_file);
	urldb_load_cookies(option_cookie_file);

	nsgtk_history_init();
	nsgtk_download_init();


        if (option_homepage_url != NULL && option_homepage_url[0] != '\0')
                addr = option_homepage_url;

	if (2 <= argc)
		addr = argv[1];

        /* Last step of initialization. Opens the main browser window. */

	bw = browser_window_create(addr, 0, 0, true, false);

}


/**
 * Load definitions from glade files.
 */
void nsgtk_init_glade(void)
{
	char buf[PATH_MAX];

	glade_init();

	nsgtk_find_resource(buf, "netsurf.glade", "./gtk/res/netsurf.glade");
	LOG(("Using '%s' as Netsurf glade template file", buf));
	glade_netsurf_file_location = strdup(buf);

	nsgtk_find_resource(buf, "password.glade", "./gtk/res/password.glade");
	LOG(("Using '%s' as password glade template file", buf));
	glade_password_file_location = strdup(buf);

	nsgtk_find_resource(buf, "warning.glade", "./gtk/res/warning.glade");
	LOG(("Using '%s' as warning glade template file", buf));
	glade_warning_file_location = strdup(buf);

	nsgtk_find_resource(buf, "login.glade", "./gtk/res/login.glade");
	LOG(("Using '%s' as login glade template file", buf));
	glade_login_file_location = strdup(buf);

	nsgtk_find_resource(buf, "ssl.glade", "./gtk/res/ssl.glade");
	LOG(("Using '%s' as ssl glade template file", buf));
	glade_ssl_file_location = strdup(buf);

	gladeWarning = glade_xml_new(glade_warning_file_location, NULL, NULL);
	if (gladeWarning == NULL)
		die("Unable to load glade warning window definitions.\n");
	glade_xml_signal_autoconnect(gladeWarning);

	gladeNetsurf = glade_xml_new(glade_netsurf_file_location, NULL, NULL);
	if (gladeNetsurf == NULL)
		die("Unable to load glade Netsurf window definitions.\n");
	glade_xml_signal_autoconnect(gladeNetsurf);

	gladePassword = glade_xml_new(glade_password_file_location, NULL, NULL);
	if (gladePassword == NULL)
		die("Unable to load glade password window definitions.\n");
	glade_xml_signal_autoconnect(gladePassword);

	gladeLogin = glade_xml_new(glade_login_file_location, NULL, NULL);
	if (gladeLogin == NULL)
		die("Unable to load glade login window definitions.\n");
	glade_xml_signal_autoconnect(gladeLogin);

	gladeSsl = glade_xml_new(glade_ssl_file_location, NULL, NULL);
	if (gladeSsl == NULL)
		die("Unable to load glade ssl window definitions.\n");
	glade_xml_signal_autoconnect(gladeSsl);

	nsgtk_find_resource(buf, "toolbar.glade", "./gtk/res/toolbar.glade");
	LOG(("Using '%s' as glade toolbar file", buf));
	glade_toolbar_file_location = strdup(buf);

	wndWarning = GTK_WINDOW(glade_xml_get_widget(gladeWarning, "wndWarning"));
}

/**
 * Main entry point from OS.
 */
int main(int argc, char** argv)
{
	char options[PATH_MAX];
	char *messages;

	/* Some modern distributions can set ALL_PROXY/all_proxy if configured
	 * to by the user.  Due to a bug in many versions of libcurl
	 * (including the one shipped in Ubuntu 10.04 LTS), this also takes
	 * effect on file:// URLs, meaning that NetSurf cannot load its
	 * default CSS file.  Given all examples of distributions I've checked
	 * also set http_proxy and friends, we can safely unset these.
	 */

	unsetenv("ALL_PROXY");
	unsetenv("all_proxy");

	gtk_init(&argc, &argv);
	
        /* set standard error to be non-buffering */
	setbuf(stderr, NULL);

	nsgtk_find_resource(options, "Choices", "~/.netsurf/Choices");
	options_file_location = strdup(options);

	messages = nsgtk_find_messages();

	/* initialise netsurf */
	netsurf_init(&argc, &argv, options, messages);

	gui_init(argc, argv);

	netsurf_main_loop();

	netsurf_exit();

	free(messages);

	return 0;
}


void gui_poll(bool active)
{
	CURLMcode code;
	fd_set read_fd_set, write_fd_set, exc_fd_set;
	int max_fd;
	GPollFD *fd_list[1000];
	unsigned int fd_count = 0;
	bool block = true;

	if (browser_reformat_pending)
		block = false;

	if (active) {
		FD_ZERO(&read_fd_set);
		FD_ZERO(&write_fd_set);
		FD_ZERO(&exc_fd_set);
		code = curl_multi_fdset(fetch_curl_multi,
				&read_fd_set,
				&write_fd_set,
				&exc_fd_set,
				&max_fd);
		assert(code == CURLM_OK);
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
	}

	gtk_main_iteration_do(block);

	for (unsigned int i = 0; i != fd_count; i++) {
		g_main_context_remove_poll(0, fd_list[i]);
		free(fd_list[i]);
	}

	schedule_run();

	if (browser_reformat_pending)
		nsgtk_window_process_reformats();
}


void gui_multitask(void)
{
	while (gtk_events_pending())
		gtk_main_iteration();
}


void gui_quit(void)
{
	nsgtk_download_destroy();
	urldb_save_cookies(option_cookie_jar);
	urldb_save(option_url_file);
	free(default_stylesheet_url);
	free(quirks_stylesheet_url);
	free(adblock_stylesheet_url);
	free(option_cookie_file);
	free(option_cookie_jar);
	free(print_options_file_location);
	free(search_engines_file_location);
	free(search_default_ico_location);
	free(toolbar_indices_file_location);
	gtk_fetch_filetype_fin();
	/* We don't care if this fails as we're about to die, anyway */
	hubbub_finalise(ns_realloc, NULL);
}




/**
 * Check that ~/.netsurf/ exists, and if it doesn't, create it.
 */
void nsgtk_check_homedir(void)
{
	char *hdir = getenv("HOME");
	char buf[BUFSIZ];

	if (hdir == NULL) {
		/* we really can't continue without a home directory. */
		LOG(("HOME is not set - nowhere to store state!"));
		die("NetSurf requires HOME to be set in order to run.\n");

	}

	snprintf(buf, BUFSIZ, "%s/.netsurf", hdir);
	if (access(buf, F_OK) != 0) {
		LOG(("You don't have a ~/.netsurf - creating one for you."));
		if (mkdir(buf, 0777) == -1) {
			LOG(("Unable to create %s", buf));
			die("NetSurf requires ~/.netsurf to exist, but it cannot be created.\n");
		}
	}
}



/* This is an ugly hack to just get the new-style throbber going.
 * It, along with the PNG throbber loader, need making more generic.
 */
bool nsgtk_throbber_init(int framec)
{
	char **filenames;
	char targetname[PATH_MAX];
	char targetdefault[PATH_MAX];
	int frame_num;
	bool ret;

	filenames = calloc(framec, sizeof(char *));
	if (filenames == NULL)
		return false;

	for (frame_num = 0; frame_num < framec; frame_num++) {
		snprintf(targetname, PATH_MAX, "throbber/throbber%d.png", frame_num);
		snprintf(targetdefault, PATH_MAX, "./gtk/res/%s", targetname);
		filenames[frame_num] = nsgtk_find_resource(NULL, targetname, targetdefault);
	}

	ret = nsgtk_throbber_initialise_from_png(frame_num, filenames);

	for (frame_num = 0; frame_num < framec; frame_num++) {
		free(filenames[frame_num]);
	}
	free(filenames);

	return ret;

}


static void nsgtk_select_menu_clicked(GtkCheckMenuItem *checkmenuitem,
					gpointer user_data)
{
	form_select_process_selection(select_menu_bw->current_content,
			select_menu_control, (intptr_t)user_data);
}

void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{

	intptr_t i;
	struct form_option *option;

	GtkWidget *menu_item;

	/* control->data.select.multiple is true if multiple selections
	 * are allowable.  We ignore this, as the core handles it for us.
	 * Yay. \o/
	 */

	if (select_menu != NULL)
		gtk_widget_destroy(select_menu);

	select_menu = gtk_menu_new();
	select_menu_bw = bw;
	select_menu_control = control;

	for (i = 0, option = control->data.select.items; option;
		i++, option = option->next) {
		menu_item = gtk_check_menu_item_new_with_label(option->text);
		if (option->selected)
			gtk_check_menu_item_set_active(
				GTK_CHECK_MENU_ITEM(menu_item), TRUE);

		g_signal_connect(menu_item, "toggled",
			G_CALLBACK(nsgtk_select_menu_clicked), (gpointer)i);

		gtk_menu_shell_append(GTK_MENU_SHELL(select_menu), menu_item);
	}

	gtk_widget_show_all(select_menu);

	gtk_menu_popup(GTK_MENU(select_menu), NULL, NULL, NULL,
			NULL /* data */, 0, gtk_get_current_event_time());

}

void gui_window_save_link(struct gui_window *g, const char *url, 
		const char *title)
{
}

void gui_launch_url(const char *url)
{
}

void warn_user(const char *warning, const char *detail)
{
	char buf[300];	/* 300 is the size the RISC OS GUI uses */

  	LOG(("%s %s", warning, detail ? detail : ""));
	fflush(stdout);

	snprintf(buf, sizeof(buf), "%s %s", messages_get(warning),
			detail ? detail : "");
	buf[sizeof(buf) - 1] = 0;

	gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(gladeWarning, "labelWarning")), buf);

	gtk_widget_show_all(GTK_WIDGET(wndWarning));
}

void die(const char * const error)
{
	fprintf(stderr, "%s", error);
	exit(EXIT_FAILURE);
}


void hotlist_visited(hlcache_handle *content)
{
}

void gui_cert_verify(const char *url, const struct ssl_cert_info *certs, 
		unsigned long num, nserror (*cb)(bool proceed, void *pw),
		void *cbpw)
{
	GladeXML *x = glade_xml_new(glade_ssl_file_location, NULL, NULL);
	GtkWindow *wnd = GTK_WINDOW(glade_xml_get_widget(x, "wndSSLProblem"));
	GtkButton *accept, *reject;
	void **session = calloc(sizeof(void *), 5);

	session[0] = strdup(url);
	session[1] = cb;
	session[2] = cbpw;
	session[3] = x;
	session[4] = wnd;

	accept = GTK_BUTTON(glade_xml_get_widget(x, "sslaccept"));
	reject = GTK_BUTTON(glade_xml_get_widget(x, "sslreject"));

	g_signal_connect(G_OBJECT(accept), "clicked",
			G_CALLBACK(nsgtk_ssl_accept), (gpointer)session);
	g_signal_connect(G_OBJECT(reject), "clicked",
			G_CALLBACK(nsgtk_ssl_reject), (gpointer)session);

	gtk_widget_show(GTK_WIDGET(wnd));
}


void nsgtk_ssl_accept(GtkButton *w, gpointer data)
{
	void **session = data;
	char *url = session[0];
	nserror (*cb)(bool proceed, void *pw) = session[1];
	void *cbpw = session[2];
	GladeXML *x = session[3];
	GtkWindow *wnd = session[4];

  	urldb_set_cert_permissions(url, true);

	cb(true, cbpw);

	gtk_widget_destroy(GTK_WIDGET(wnd));
	g_object_unref(G_OBJECT(x));
	free(url);
	free(session);
}


void nsgtk_ssl_reject(GtkButton *w, gpointer data)
{
	void **session = data;
	nserror (*cb)(bool proceed, void *pw) = session[1];
	void *cbpw = session[2];
	GladeXML *x = session[3];
	GtkWindow *wnd = session[4];

	cb(false, cbpw);

	gtk_widget_destroy(GTK_WIDGET(wnd));
	g_object_unref(G_OBJECT(x));
	free(session[0]);
	free(session);
}


utf8_convert_ret utf8_to_local_encoding(const char *string, size_t len,
		char **result)
{
	assert(string && result);

	if (len == 0)
		len = strlen(string);

	*result = strndup(string, len);
	if (!(*result))
		return UTF8_CONVERT_NOMEM;

	return UTF8_CONVERT_OK;
}


utf8_convert_ret utf8_from_local_encoding(const char *string, size_t len,
		char **result)
{
	assert(string && result);

	if (len == 0)
		len = strlen(string);

	*result = strndup(string, len);
	if (!(*result))
		return UTF8_CONVERT_NOMEM;

	return UTF8_CONVERT_OK;
}


char *path_to_url(const char *path)
{
	int urllen = strlen(path) + FILE_SCHEME_PREFIX_LEN + 1;
	char *url = malloc(urllen);

	if (url == NULL) {
		return NULL;
	}

	if (*path == '/') {
		path++; /* file: paths are already absolute */
	} 

	snprintf(url, urllen, "%s%s", FILE_SCHEME_PREFIX, path);

	return url;
}


char *url_to_path(const char *url)
{
	char *url_path = curl_unescape(url, 0);
	char *path;

	/* return the absolute path including leading / */
	path = strdup(url_path + (FILE_SCHEME_PREFIX_LEN - 1));
	curl_free(url_path);

	return path;
}


bool cookies_update(const char *domain, const struct cookie_data *data)
{
	return true;
}

#ifdef WITH_PDF_EXPORT

void PDF_Password(char **owner_pass, char **user_pass, char *path)
{
	GladeXML *x = glade_xml_new(glade_password_file_location, NULL, NULL);
	GtkWindow *wnd = GTK_WINDOW(glade_xml_get_widget(x, "wndPDFPassword"));
	GtkButton *ok, *no;
	void **data = malloc(5 * sizeof(void *));

	*owner_pass = NULL;
	*user_pass = NULL;

	data[0] = owner_pass;
	data[1] = user_pass;
	data[2] = wnd;
	data[3] = x;
	data[4] = path;

	ok = GTK_BUTTON(glade_xml_get_widget(x, "buttonPDFSetPassword"));
	no = GTK_BUTTON(glade_xml_get_widget(x, "buttonPDFNoPassword"));

	g_signal_connect(G_OBJECT(ok), "clicked",
			 G_CALLBACK(nsgtk_PDF_set_pass), (gpointer)data);
	g_signal_connect(G_OBJECT(no), "clicked",
			 G_CALLBACK(nsgtk_PDF_no_pass), (gpointer)data);

	gtk_widget_show(GTK_WIDGET(wnd));
}

static void nsgtk_PDF_set_pass(GtkButton *w, gpointer data)
{
	char **owner_pass = ((void **)data)[0];
	char **user_pass = ((void **)data)[1];
	GtkWindow *wnd = ((void **)data)[2];
	GladeXML *x = ((void **)data)[3];
	char *path = ((void **)data)[4];

	char *op, *op1;
	char *up, *up1;

	op = strdup(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(x,
			"entryPDFOwnerPassword"))));
	op1 = strdup(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(x,
			"entryPDFOwnerPassword1"))));
	up = strdup(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(x,
		    	"entryPDFUserPassword"))));
	up1 = strdup(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(x,
		     	"entryPDFUserPassword1"))));


	if (op[0] == '\0') {
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(x,
				"labelInfo")),
       				"Owner password must be at least 1 character long:");
		free(op);
		free(up);
	}
	else if (!strcmp(op, up)) {
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(x,
				"labelInfo")),
       				"User and owner passwords must be different:");
		free(op);
		free(up);
	}
	else if (!strcmp(op, op1) && !strcmp(up, up1)) {

		*owner_pass = op;
		if (up[0] == '\0')
			free(up);
		else
			*user_pass = up;

		free(data);
		gtk_widget_destroy(GTK_WIDGET(wnd));
		g_object_unref(G_OBJECT(x));

		save_pdf(path);
		free(path);
	}
	else {
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(x,
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
	GladeXML *x = ((void **)data)[3];
	char *path = ((void **)data)[4];

	free(data);

	gtk_widget_destroy(GTK_WIDGET(wnd));
	g_object_unref(G_OBJECT(x));

	save_pdf(path);
	free(path);
}
#endif

uint32_t gtk_gui_gdkkey_to_nskey(GdkEventKey *key)
{
	/* this function will need to become much more complex to support
	 * everything that the RISC OS version does.  But this will do for
	 * now.  I hope.
	 */
	switch (key->keyval)
	{
		case GDK_BackSpace:
			if (key->state & GDK_SHIFT_MASK)
				return KEY_DELETE_LINE_START;
			else
				return KEY_DELETE_LEFT;
		case GDK_Delete:
			if (key->state & GDK_SHIFT_MASK)
				return KEY_DELETE_LINE_END;
			else
				return KEY_DELETE_RIGHT;
		case GDK_Linefeed:	return 13;
		case GDK_Return:	return 10;
		case GDK_Left:		return KEY_LEFT;
		case GDK_Right:		return KEY_RIGHT;
		case GDK_Up:		return KEY_UP;
		case GDK_Down:		return KEY_DOWN;
		case GDK_Home:
			if (key->state & GDK_CONTROL_MASK)
				return KEY_TEXT_START;
			else
				return KEY_LINE_START;
		case GDK_End:
			if (key->state & GDK_CONTROL_MASK)
				return KEY_TEXT_END;
			else
				return KEY_LINE_END;
		case GDK_Page_Up:
			return KEY_PAGE_UP;
		case GDK_Page_Down:
			return KEY_PAGE_DOWN;
		case 'a':
			if (key->state & GDK_CONTROL_MASK)
				return KEY_SELECT_ALL;
			return gdk_keyval_to_unicode(key->keyval);
		case 'u':
			if (key->state & GDK_CONTROL_MASK)
				return KEY_CLEAR_SELECTION;
			return gdk_keyval_to_unicode(key->keyval);
		case GDK_Escape:
			return KEY_ESCAPE;

		/* Modifiers - do nothing for now */
		case GDK_Shift_L:
		case GDK_Shift_R:
		case GDK_Control_L:
		case GDK_Control_R:
		case GDK_Caps_Lock:
		case GDK_Shift_Lock:
		case GDK_Meta_L:
		case GDK_Meta_R:
		case GDK_Alt_L:
		case GDK_Alt_R:
		case GDK_Super_L:
		case GDK_Super_R:
		case GDK_Hyper_L:
		case GDK_Hyper_R:	return 0;

		default:		return gdk_keyval_to_unicode(
							key->keyval);
        }
}

/**
 * Return the filename part of a full path
 *
 * \param path full path and filename
 * \return filename (will be freed with free())
 */

char *filename_from_path(char *path)
{
	char *leafname;

	leafname = strrchr(path, '/');
	if (!leafname)
		leafname = path;
	else
		leafname += 1;

	return strdup(leafname);
}

/**
 * Add a path component/filename to an existing path
 *
 * \param path buffer containing path + free space
 * \param length length of buffer "path"
 * \param newpart string containing path component to add to path
 * \return true on success
 */

bool path_add_part(char *path, int length, const char *newpart)
{
	if(path[strlen(path) - 1] != '/')
		strncat(path, "/", length);

	strncat(path, newpart, length);

	return true;
}
