/*
 * Copyright 2024 Vincent Sanders <vince@netsurf-browser.org>
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

/**
 * \file
 * Implementation of netsurf application for qt frontend.
 */

#include <QSettings>
#include <QStandardPaths>
#include <QResource>
#include <QDir>

#include <sys/stat.h>

extern "C" {
#include "utils/utils.h"
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
#include "utils/file.h"
#include "utils/messages.h"
#include "utils/filepath.h"

#include "netsurf/bitmap.h"
#include "netsurf/netsurf.h"
#include "netsurf/content.h"
#include "netsurf/browser_window.h"
#include "netsurf/cookie_db.h"
#include "netsurf/url_db.h"

#include "desktop/hotlist.h"
#include "desktop/searchweb.h"
}

#include "qt/resources.h"
#include "qt/misc.h"

#include "qt/application.cls.h"
#include "qt/settings.cls.h"
#include "qt/bookmarks.cls.h"
#include "qt/global_history.cls.h"
#include "qt/cookies.cls.h"
#include "qt/page_info.cls.h"

//QTimer
//QSocketNotifier

static NS_Application *nsqtapp;

NS_Application* NS_Application::instance()
{
	return nsqtapp;
}

/**
 * Ensures output logging stream is correctly configured
 */
bool NS_Application::nslog_stream_configure(FILE *fptr)
{
	/* set log stream to be non-buffering */
	setbuf(fptr, NULL);

	return true;
}

static char *accept_language_from_qlocale(QLocale &loc)
{
	QString alang;
	QStringList llist = loc.uiLanguages();
	int lidx;
	float quality = 1.0;

	for (lidx = 0;lidx < llist.size(); lidx++) {
		QStringList lparts = llist.at(lidx).split('-', Qt::SkipEmptyParts);
		if (lparts.size() == 0 || lparts.size() > 2) {
			continue;
		}
		quality -= 0.1;
		if (quality <= 0.2) {
			quality = 0.2;
		}
		alang += lparts.at(0);
		if (lparts.size() == 2) {
			alang += "-" + lparts.at(1);
		}
		alang += ";q=" + QString::number(quality) + ", ";
	}
	if (alang.size() < 3) {
		return NULL;
	}
	return strdup(alang.left(alang.size() - 2).toUtf8().data());
}

/*
 * set system-color nsoptions from QT palette
 */
void NS_Application::nsOptionFromPalette(struct nsoption_s *opts)
{
	/* css level 4 reuses only 5 of the legacy system colours
	   sys_colour_ButtonFace
	   sys_colour_ButtonText
	   sys_colour_GrayText
	   sys_colour_Highlight
	   sys_colour_HighlightText

	   css4 system colours (+experimental, *reused)
	   +AccentColor -> QPalette::Active, QPalette::Accent
	   +AccentColorText -> QPalette::Active, QPalette::HighlightedText
	   ActiveText -> QPalette::Active, QPalette::BrightText
	   +ButtonBorder -> QPalette::Active, QPalette::Light
	   *ButtonFace -> QPalette::Active, QPalette::Button
	   *ButtonText -> QPalette::Active, QPalette::ButtonText
	   Canvas -> QPalette::Active, QPalette::Window
	   CanvasText -> QPalette::Active,QPalette::WindowText
	   Field -> QPalette::Active,QPalette::Base
	   FieldText -> QPalette::Active,QPalette::Text
	   *GrayText -> QPalette::Disabled, QPalette::Text
	   *Highlight -> QPalette::Active,QPalette::Highlight
	   *HighlightText -> QPalette::Active,QPalette::HighlightedText
	   LinkText -> QPalette::Active,QPalette::Link
	   +Mark -> QPalette::Active,QPalette::Highlight
	   +MarkText -> QPalette::Active,QPalette::HighlightedText
	   SelectedItem -> QPalette::Active,QPalette::AlternateBase
	   SelectedItemText -> QPalette::Active,QPalette::BrightText
	   VisitedText -> QPalette::LinkVisited

	*/
	struct {
		QPalette::ColorGroup group;
		QPalette::ColorRole role;
		enum nsoption_e option;
	} entries[] = {
		{
			QPalette::Active,
			QPalette::Highlight, /* this could be Accent (qt 6.6) */
			NSOPTION_sys_colour_AccentColor
		}, {
			QPalette::Active,
			QPalette::HighlightedText,
			NSOPTION_sys_colour_AccentColorText
		}, {
			QPalette::Active,
			QPalette::BrightText,
			NSOPTION_sys_colour_ActiveText
		}, {
			QPalette::Active,
			QPalette::Light,
			NSOPTION_sys_colour_ButtonBorder
		}, {
			QPalette::Active,
			QPalette::Button,
			NSOPTION_sys_colour_ButtonFace
		}, {
			QPalette::Active,
			QPalette::ButtonText,
			NSOPTION_sys_colour_ButtonText
		}, {
			QPalette::Active,
			QPalette::Window,
			NSOPTION_sys_colour_Canvas
		}, {
			QPalette::Active,
			QPalette::WindowText,
			NSOPTION_sys_colour_CanvasText
		}, {
			QPalette::Active,
			QPalette::Base,
			NSOPTION_sys_colour_Field
		}, {
			QPalette::Active,
			QPalette::Text,
			NSOPTION_sys_colour_FieldText
		}, {
			QPalette::Disabled,
			QPalette::Text,
			NSOPTION_sys_colour_GrayText
		}, {
			QPalette::Active,
			QPalette::Highlight,
			NSOPTION_sys_colour_Highlight
		}, {
			QPalette::Active,
			QPalette::HighlightedText,
			NSOPTION_sys_colour_HighlightText
		}, {
			QPalette::Active,
			QPalette::Link,
			NSOPTION_sys_colour_LinkText
		}, {
			QPalette::Active,
			QPalette::Highlight,
			NSOPTION_sys_colour_Mark
		}, {
			QPalette::Active,
			QPalette::HighlightedText,
			NSOPTION_sys_colour_MarkText
		}, {
			QPalette::Active,
			QPalette::AlternateBase,
			NSOPTION_sys_colour_SelectedItem
		}, {
			QPalette::Active,
			QPalette::BrightText,
			NSOPTION_sys_colour_SelectedItemText
		}, {
			QPalette::Active,
			QPalette::LinkVisited,
			NSOPTION_sys_colour_VisitedText
		}, {
			QPalette::Active,
			QPalette::NoRole,
			NSOPTION_LISTEND
		},
	};
	const QPalette palette;
	int idx;
	int r,g,b;
	for (idx=0; entries[idx].option != NSOPTION_LISTEND; idx++) {
		palette.color(entries[idx].group,
			      entries[idx].role).getRgb(&r,&g,&b);
		opts[entries[idx].option].value.c =
			((b & 0xff) << 16) |
			((g & 0xff) << 8) |
			(r & 0xff);
	}
}

/*
 * Set option defaults for qt frontend
 */
nserror NS_Application::set_option_defaults(struct nsoption_s *defaults)
{
	QDir config_dir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));

	/* ensure all elements of path exist */
	if (!config_dir.exists()) {
		config_dir.mkpath(config_dir.absoluteFilePath(""));
	}

	/* cookies database default read and write paths */
	nsoption_setnull_charp(cookie_file,
			strdup(config_dir.absoluteFilePath("Cookies").toUtf8()));

	nsoption_setnull_charp(cookie_jar,
			strdup(config_dir.absoluteFilePath("Cookies").toUtf8()));

	/* url database default path */
	nsoption_setnull_charp(url_file,
			strdup(config_dir.absoluteFilePath("URLs").toUtf8()));

	/* bookmark database default path */
	nsoption_setnull_charp(hotlist_path,
			strdup(config_dir.absoluteFilePath("Hotlist").toUtf8()));

	if (nsoption_charp(hotlist_path) == NULL) {
		NSLOG(netsurf, ERROR, "Failed initialising bookmarks resource path");
		return NSERROR_BAD_PARAMETER;
	}

	/* set default font names */
	nsoption_set_charp(font_sans, strdup("Sans"));
	nsoption_set_charp(font_serif, strdup("Serif"));
	nsoption_set_charp(font_mono, strdup("Monospace"));
	nsoption_set_charp(font_cursive, strdup("Serif"));
	nsoption_set_charp(font_fantasy, strdup("Serif"));

	/* use qt locale to generate a default accept language configuration */
	QLocale loc;
	char *alang;
	alang = accept_language_from_qlocale(loc);
	if (alang != NULL) {
		NSLOG(netsurf, DEBUG, "accept_language \"%s\"", alang);
		nsoption_set_charp(accept_language, alang);
	}

	nsOptionFromPalette(defaults);


	return NSERROR_OK;
}

void NS_Application::nsOptionLoad()
{
	QSettings settings;
	unsigned int entry; /* index to option being output */
	for (entry = 0; entry < NSOPTION_LISTEND; entry++) {
		struct nsoption_s *option = nsoptions + entry;
		if (settings.contains(option->key)) {
			switch (option->type) {
			case OPTION_BOOL:
				option->value.b = settings.value(option->key).toBool();
				break;

			case OPTION_INTEGER:
				option->value.i = settings.value(option->key).toInt();
				break;

			case OPTION_UINT:
				option->value.u = settings.value(option->key).toUInt();
				break;

			case OPTION_COLOUR:
				option->value.c = settings.value(option->key).toUInt();
				break;

			case OPTION_STRING:
				nsoption_set_tbl_charp(nsoptions,
						       (enum nsoption_e)entry,
						       strdup(settings.value(option->key).toString().toUtf8()));
				break;
			}
		}
	}
}

static size_t set_qtsetting(struct nsoption_s *option, void *ctx)
{
	QSettings *settings = (QSettings *)ctx;

	switch (option->type) {
	case OPTION_BOOL:
		settings->setValue(option->key, option->value.b);
		break;

	case OPTION_INTEGER:
		settings->setValue(option->key, option->value.i);
		break;

	case OPTION_UINT:
		settings->setValue(option->key, option->value.u);
		break;

	case OPTION_COLOUR:
		settings->setValue(option->key, option->value.c);
		break;

	case OPTION_STRING:
		settings->setValue(option->key,
				   QString(((option->value.s == NULL) ||
					    (*option->value.s == 0)) ?
					   "" : option->value.s));
		break;
	}

	return NSERROR_OK;
}

void NS_Application::nsOptionPersist()
{
	QSettings settings;
	settings.clear();
	nsoption_generate(set_qtsetting,
			  &settings,
			  NSOPTION_GENERATE_CHANGED,
			  NULL,
			  NULL);
}

/*
 * apply any options changes which depend on external system configuration
 *
 */
void NS_Application::nsOptionUpdate()
{
	switch (nsoption_uint(colour_selection)) {
	case 0:
	{
		// automaticaly select
		const QPalette palette;
		const bool dark_mode = palette.base().color().lightness()
                            < palette.windowText().color().lightness();
		nsoption_set_bool(prefer_dark_mode, dark_mode);
		break;
	}
	case 1:
		// light
		nsoption_set_bool(prefer_dark_mode, false);
		break;
	case 2:
		// dark
		nsoption_set_bool(prefer_dark_mode, true);
		break;
	case 3:
		// force manual colours
		break;
	}
}

NS_Application::NS_Application(int &argc, char **argv, struct netsurf_table *nsqt_table)
	:QApplication(argc, argv),
	 m_settings_window(nullptr),
	 m_bookmarks_window(nullptr),
	 m_local_history_window(nullptr),
	 m_global_history_window(nullptr),
	 m_cookies_window(nullptr)
{
	nserror res;

	nsqtapp = this;

	/* register operation tables */
	res = netsurf_register(nsqt_table);
	if (res != NSERROR_OK) {
		throw NS_Exception("NetSurf operation table failed registration", res);
	}

	/* organization setup for settings */
	setOrganizationName("NetSurf");
	setOrganizationDomain("netsurf-browser.org");
	setApplicationName("NetSurf");

	// set up scheduler timer
	m_schedule_timer = new QTimer(this);
	m_schedule_timer->setSingleShot(true);
	connect(m_schedule_timer, &QTimer::timeout,
		this, &NS_Application::schedule_run);

	/* Prep the resource search paths */
	res = nsqt_init_resource_path("${HOME}/.netsurf/:${NETSURFRES}:" QT_RESPATH);
	if (res != NSERROR_OK) {
		throw NS_Exception("Resources failed to initialise",res);
	}

	/* Initialise logging. Not fatal if it fails but not much we
	 * can do about it either.
	 */
	nslog_init(nslog_stream_configure, &argc, argv);

	/* Initialise user options */
	res = nsoption_init(set_option_defaults, &nsoptions, &nsoptions_default);
	if (res != NSERROR_OK) {
		throw NS_Exception("Options failed to initialise", res);
	}

	/* load user options */
	nsOptionLoad();

	/* override loaded options with those from commandline */
	nsoption_commandline(&argc, argv, nsoptions);

	nsOptionUpdate();

	/* setup bitmap format */
	bitmap_fmt_t qtfmt = {
		.layout = BITMAP_LAYOUT_ARGB8888,
		.pma = false,
	};
	bitmap_set_format(&qtfmt);

	QResource messages_res("Messages");
	QByteArray messages_data = messages_res.uncompressedData();
	res = messages_add_from_inline((uint8_t*)messages_data.data(),
				       messages_data.size());

	char *addr = NULL;
	nsurl *url = NULL;

	/* netsurf initialisation */
	res = netsurf_init(NULL);
	if (res != NSERROR_OK) {
		throw NS_Exception("Netsurf core initialisation failed", res);
	}

        /* Web search engine sources */
	char *resource_filename = filepath_find(respaths, "SearchEngines");
	search_web_init(resource_filename);
	if (resource_filename != NULL) {
		NSLOG(netsurf, INFO, "Using '%s' as Search Engines file",
		      resource_filename);
		free(resource_filename);
	}
	search_web_select_provider(nsoption_charp(search_web_provider));

	/* initialise url database from user data */
	urldb_load(nsoption_charp(url_file));

	/* initialise cookies database from user data */
	urldb_load_cookies(nsoption_charp(cookie_file));

	/* initialise the bookmarks from user data */
	hotlist_init(nsoption_charp(hotlist_path),
		     nsoption_charp(hotlist_path));

	/* If there is a url specified on the command line use it */
	if (argc > 1) {
		struct stat fs;
		if (stat(argv[1], &fs) == 0) {
			size_t addrlen;
			char *rp = realpath(argv[1], NULL);
			assert(rp != NULL);

			/* calculate file url length including terminator */
			addrlen = SLEN("file://") + strlen(rp) + 1;
			addr = (char *)malloc(addrlen);
			assert(addr != NULL);
			snprintf(addr, addrlen, "file://%s", rp);
			free(rp);
		} else {
			addr = strdup(argv[1]);
		}

		/* convert initial target to url */
		res = nsurl_create(addr, &url);
		if (res != NSERROR_OK) {
			throw NS_Exception("failed converting initial url", res);
		}
		free(addr);
	}

	res = create_browser_widget(url, NULL, false);

	if (url != NULL) {
		nsurl_unref(url);
	}

	if (res != NSERROR_OK) {
		throw NS_Exception("Opening initial url failed", res);
	}
}

NS_Application::~NS_Application()
{
	nserror res;

	delete m_cookies_window;
	delete m_global_history_window;
	delete m_local_history_window;
	delete m_bookmarks_window;
	delete m_settings_window;

	/* finalise cookie database */
	urldb_save_cookies(nsoption_charp(cookie_jar));

	/* finalise url database */
	urldb_save(nsoption_charp(url_file));

	res = hotlist_fini();
	if (res != NSERROR_OK) {
		NSLOG(netsurf, INFO, "Error finalising hotlist: %s",
		      messages_get_errorcode(res));
	}

	/* common finalisation */
	netsurf_exit();

	/* finalise options */
	nsoption_finalise(nsoptions, nsoptions_default);

	/* finalise logging */
	nslog_finalise();

	delete m_schedule_timer;
}

bool NS_Application::event(QEvent* event)
{
	if (event->type() == QEvent::ApplicationPaletteChange) {
		nsOptionUpdate();
	}
	return QApplication::event(event);
}

/**
 * scheduled timer slot
 */
void NS_Application::schedule_run()
{
	int ms;
	ms = nsqt_schedule_run();
	if (ms >= 0) {
		m_schedule_timer->start(ms);
	}
}

/**
 *
 */
void NS_Application::next_schedule(int ms)
{
	if ((m_schedule_timer->isActive()==false) ||
	    (m_schedule_timer->remainingTime() > ms)) {
		m_schedule_timer->start(ms);
	}
}


/**
 * show settings window
 */
void NS_Application::settings_show()
{
	if (m_settings_window == nullptr) {
		m_settings_window = new NS_Settings(nullptr);
	}
	m_settings_window->show();
	m_settings_window->raise();
}


/**
 * show bookmarks window
 */
void NS_Application::bookmarks_show()
{
	if (m_bookmarks_window == nullptr) {
		m_bookmarks_window = new NS_Bookmarks(nullptr);
	}
	m_bookmarks_window->show();
	m_bookmarks_window->raise();
}

/**
 * show local history window
 */
void NS_Application::local_history_show(struct browser_window *bw, const QPoint &pos)
{
	if (m_local_history_window == nullptr) {
		m_local_history_window = new NS_Local_history(nullptr, bw);
	} else {
		m_local_history_window->setbw(bw);
	}
	m_local_history_window->move(pos);
	m_local_history_window->show();
	m_local_history_window->raise();
}


/**
 * show page info window
 */
void NS_Application::page_info_show(struct browser_window *bw, const QPoint &pos)
{
	NS_Page_info *page_info = new NS_Page_info(nullptr, bw);
	page_info->move(pos);
	page_info->show();
	page_info->raise();
}

/**
 * show global history window
 */
void NS_Application::global_history_show()
{
	if (m_global_history_window == nullptr) {
		m_global_history_window = new NS_Global_history(nullptr);
	}
	m_global_history_window->show();
	m_global_history_window->raise();
}

/**
 * show cookies window
 */
nserror NS_Application::cookies_show(const char *search_term)
{
	nserror res;
	if (m_cookies_window == nullptr) {
		m_cookies_window = new NS_Cookies(nullptr);
	}
	res = m_cookies_window->setSearch(search_term);
	m_cookies_window->show();
	m_cookies_window->raise();

	return res;
}

nserror NS_Application::create_browser_widget(struct browser_window *existing,
					      bool intab)
{
	nsurl *url = NULL;
	return create_browser_widget(url, existing, intab);
}

nserror NS_Application::create_browser_widget(struct hlcache_handle *hlchandle,
					      struct browser_window *existing,
					      bool intab)
{
	if (hlchandle == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	return create_browser_widget(hlcache_handle_get_url(hlchandle), existing, intab);
}

/* create a new browsing context in a tab or window */
nserror NS_Application::create_browser_widget(nsurl *url,
					      struct browser_window *existing,
					      bool intab)
{
	nserror res = NSERROR_OK;
	bool urlcreated = false; /* was a url created */
	int flags = BW_CREATE_HISTORY | BW_CREATE_FOCUS_LOCATION | BW_CREATE_FOREGROUND;

	if (intab) {
		flags |= BW_CREATE_TAB;
	}

	if ((url == NULL) && (!nsoption_bool(new_blank))) {
		const char *addr;
		if (nsoption_charp(homepage_url) != NULL) {
			addr = nsoption_charp(homepage_url);
		} else {
			addr = NETSURF_HOMEPAGE;
		}
		res = nsurl_create(addr, &url);
		if (res == NSERROR_OK) {
			urlcreated = true;
		}
	}

	if (res == NSERROR_OK) {
		res = browser_window_create((enum browser_window_create_flags)flags,
					    url,
					    NULL,
					    existing,
					    NULL);
	}

	if (urlcreated) {
		nsurl_unref(url);
	}

	return res;
}
