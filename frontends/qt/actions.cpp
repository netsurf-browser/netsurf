/*
 * Copyright 2023 Vincent Sanders <vince@netsurf-browser.org>
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
 * Implementations of netsurf qt actions.
 */

#include <cfloat>

#include <QWidget>
#include <QStyle>
#include <QPainter>
#include <QLabel>
#include <QHBoxLayout>
#include <QClipboard>

extern "C" {
#include "utils/messages.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
#include "utils/log.h"

#include "netsurf/content.h"

#include "desktop/browser_history.h"
#include "desktop/hotlist.h"
#include "desktop/searchweb.h"
}

#include "qt/application.cls.h"
#include "qt/urlbar.cls.h"
#include "qt/actions.cls.h"

NS_Actions::NS_Actions(QWidget* parent, struct browser_window *bw) :
	QObject(parent),
	m_back(new QAction(parent->style()->standardIcon(QStyle::SP_ArrowLeft),
			   messages_get("Back"),
			   parent)),
	m_forward(new QAction(parent->style()->standardIcon(QStyle::SP_ArrowRight),
			      messages_get("Forward"),
			      parent)),
	m_stop_reload(new QAction(parent)),
	m_settings(new QAction(messages_get("Settings"), parent)),
	m_bookmarks(new QAction(messages_get("ManageBookmarks"), parent)),
	m_add_edit_bookmark(new QAction(QIcon(":/icons/hotlist-add.png"),
					messages_get("AddBookmark"),
					parent)),
	m_local_history(new QAction(QIcon(":/local-history.png"),
				    messages_get("HistLocalNS"),
				    parent)),
	m_global_history(new QAction(messages_get("HistGlobalNS"), parent)),
	m_cookies(new QAction(messages_get("ShowCookiesNS"), parent)),
	m_page_info(new QAction(QIcon(":/icons/page-info-internal.svg"),
				messages_get("PageInfo"),
				parent)),
	m_page_scale(new QAction(messages_get("PageScale"), parent)),
	m_reset_page_scale(new QAction(messages_get("PageScaleReset"), parent)),
	m_reduce_page_scale(new QAction(messages_get("PageScaleReduce"), parent)),
	m_increase_page_scale(new QAction(messages_get("PageScaleIncrease"), parent)),
	m_newtab(new QAction(messages_get("NewTab"), parent)),
	m_newwindow(new QAction(messages_get("NewWindowNS"), parent)),
	m_quit(new QAction(messages_get("Quit"), parent)),
	m_page_save(new QAction(messages_get("PageSave"), parent)),
	m_page_source(new QAction(messages_get("PageSource"), parent)),
	m_debug_render(new QAction(messages_get("DebugRender"), parent)),
	m_debug_box_tree(new QAction(messages_get("DebugBoxTree"), parent)),
	m_debug_dom_tree(new QAction(messages_get("DebugDomTree"), parent)),
	m_about_netsurf(new QAction(messages_get("About"), parent)),
	m_link_new_tab(new QAction(messages_get("LinkNewTab"), parent)),
	m_link_new_window(new QAction(messages_get("LinkNewWin"), parent)),
	m_link_bookmark(new QAction(messages_get("LinkBookmark"), parent)),
	m_link_save(new QAction(messages_get("LinkSave"), parent)),
	m_link_copy(new QAction(messages_get("LinkCopy"), parent)),
	m_img_new_tab(new QAction(messages_get("ImageNewTab"), parent)),
	m_img_save(new QAction(messages_get("ImageSave"), parent)),
	m_img_copy(new QAction(messages_get("ImageCopy"), parent)),
	m_obj_save(new QAction(messages_get("ObjectSave"), parent)),
	m_obj_copy(new QAction(messages_get("ObjectCopy"), parent)),
	m_sel_copy(new QAction(messages_get("CopyNS"), parent)),
	m_sel_search(new QAction(messages_get("SearchWeb"), parent)),
	m_bw(bw),
	m_marked(false),
	m_pistate(PAGE_STATE_INTERNAL),
	m_link(nullptr),
	m_object(nullptr),
	m_selection(nullptr)
{
	/* shortcuts */
	m_back->setShortcut(QKeySequence::Back);
	m_forward->setShortcut(QKeySequence::Forward);
	m_bookmarks->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
	m_global_history->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
	m_newtab->setShortcut(QKeySequence::AddTab);
	m_newwindow->setShortcut(QKeySequence::New);
	m_quit->setShortcut(QKeySequence::Quit);
	m_page_source->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));

	//m_quit->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	//m_newtab->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	//m_newwindow->setShortcutContext(Qt::WidgetWithChildrenShortcut);

	/* icon texts */
	m_reduce_page_scale->setIconText(messages_get("PageScaleReduceShort"));
	m_increase_page_scale->setIconText(messages_get("PageScaleIncreaseShort"));

	/* hook up the signals */
	connect(m_back, &QAction::triggered,
		this, &NS_Actions::back_slot);

	connect(m_forward, &QAction::triggered,
		this, &NS_Actions::forward_slot);

	connect(m_stop_reload, &QAction::triggered,
		this, &NS_Actions::stop_reload_slot);

	connect(m_settings, &QAction::triggered,
		this, &NS_Actions::settings_slot);

	connect(m_bookmarks, &QAction::triggered,
		this, &NS_Actions::bookmarks_slot);

	connect(m_add_edit_bookmark, &QAction::triggered,
		this, &NS_Actions::add_edit_bookmark_slot);

	connect(m_local_history, &QAction::triggered,
		this, &NS_Actions::local_history_slot);

	connect(m_global_history, &QAction::triggered,
		this, &NS_Actions::global_history_slot);

	connect(m_cookies, &QAction::triggered,
		this, &NS_Actions::cookies_slot);

	connect(m_page_info, &QAction::triggered,
		this, &NS_Actions::page_info_slot);

	connect(m_page_scale, &QAction::triggered,
		this, &NS_Actions::reset_page_scale_slot);

	connect(m_reset_page_scale, &QAction::triggered,
		this, &NS_Actions::reset_page_scale_slot);

	connect(m_reduce_page_scale, &QAction::triggered,
		this, &NS_Actions::reduce_page_scale_slot);

	connect(m_increase_page_scale, &QAction::triggered,
		this, &NS_Actions::increase_page_scale_slot);

	connect(m_newtab, &QAction::triggered,
		this, &NS_Actions::newtab_slot);

	connect(m_newwindow, &QAction::triggered,
		this, &NS_Actions::newwindow_slot);

	connect(m_quit, &QAction::triggered,
		this, &NS_Actions::quit_slot);

	connect(m_page_save, &QAction::triggered,
		this, &NS_Actions::page_save_slot);

	connect(m_page_source, &QAction::triggered,
		this, &NS_Actions::page_source_slot);

	connect(m_debug_render, &QAction::triggered,
		this, &NS_Actions::debug_render_slot);

	connect(m_debug_box_tree, &QAction::triggered,
		this, &NS_Actions::debug_box_tree_slot);

	connect(m_debug_dom_tree, &QAction::triggered,
		this, &NS_Actions::debug_dom_tree_slot);

	connect(m_about_netsurf, &QAction::triggered,
		this, &NS_Actions::about_netsurf_slot);

	connect(m_link_new_tab, &QAction::triggered,
		this, &NS_Actions::link_new_tab_slot);

	connect(m_link_new_window, &QAction::triggered,
		this, &NS_Actions::link_new_window_slot);

	connect(m_link_bookmark, &QAction::triggered,
		this, &NS_Actions::link_bookmark_slot);

	connect(m_link_save, &QAction::triggered,
		this, &NS_Actions::link_save_slot);

	connect(m_link_copy, &QAction::triggered,
		this, &NS_Actions::link_copy_slot);

	connect(m_img_new_tab, &QAction::triggered,
		this, &NS_Actions::img_new_tab_slot);

	connect(m_img_save, &QAction::triggered,
		this, &NS_Actions::img_save_slot);

	/* image link copy uses object copy slot */
	connect(m_img_copy, &QAction::triggered,
		this, &NS_Actions::obj_copy_slot);

	connect(m_obj_save, &QAction::triggered,
		this, &NS_Actions::obj_save_slot);

	connect(m_obj_copy, &QAction::triggered,
		this, &NS_Actions::obj_copy_slot);

	connect(m_sel_copy, &QAction::triggered,
		this, &NS_Actions::sel_copy_slot);

	connect(m_sel_search, &QAction::triggered,
		this, &NS_Actions::sel_search_slot);

	update_navigation(NS_Actions::UpdateInactive);
	update_page_info();
	update_page_scale();
	update_bookmarks();
}


NS_Actions::~NS_Actions()
{
	if (m_selection != NULL) {
		free(m_selection);
	}
}


/*
 * get tool button associated with action
 */
QToolButton *NS_Actions::QToolButtonFromQAction(QAction *action)
{
	QList <QObject *> widget_list = action->associatedObjects();

	/* iterate objects associated with action */
	for (int idx = 0; idx < widget_list.count(); idx++) {
#ifdef BUTTON_QOBJECT_CAST
		// @todo find out why qobject_cast fails (always null) here
		QToolButton *button = qobject_cast<QToolButton *>(widget_list.at(idx));
#else
		const char *clsname = widget_list.at(idx)->metaObject()->className();
		//NSLOG(netsurf, WARN, "%s",clsname);
		if ((strcmp(clsname, "QToolButton") == 0) ||
		    (strcmp(clsname, "QLineEditIconButton") == 0)) {
			return (QToolButton *)(widget_list.at(idx));
		}
#endif
	}
	return nullptr;
}


/*
 * obtain the bottom left corner global location of an action attached widget
 */
QPoint NS_Actions::actionGlobal(QAction *action)
{
	QToolButton *button = QToolButtonFromQAction(action);
	if (button == nullptr) {
		/* no associated button default to cursor position */
		return QCursor::pos();
	}
	return button->mapToGlobal(QPoint(0, button->height()));
}

/* Change action state appropriate for flag */
void NS_Actions::update(NS_Actions::Update update)
{
	switch (update) {
	case UpdateInactive:
	case UpdateActive:
	case UpdateUnchanged:
		update_navigation(update);
		update_bookmarks();
		update_page_scale();
		break;

	case UpdatePageInfo:
		update_page_info();
		break;

	case UpdateBookmarks:
		update_bookmarks();
		break;

	case UpdatePageScale:
		update_page_scale();
		break;
	}
}

void NS_Actions::update(struct nsurl *link, struct hlcache_handle *object,char *selection)
{
	m_link = link;
	m_object = object;
	if (m_selection != NULL) {
		free(m_selection);
	}
	m_selection = selection;

	m_sel_search->setText(QString::asprintf(messages_get("SearchProviderFor"), nsoption_charp(search_web_provider), m_selection));
}

/*
 * page scale action widget for menu
 */
QWidgetAction *NS_Actions::page_scale_widget_action(QWidget* parent)
{

	QLabel *scalelabel = new QLabel("Page Scale");

	QToolButton *scaleminus = new QToolButton();
	scaleminus->setDefaultAction(m_reduce_page_scale);
	scaleminus->setStyleSheet("QToolButton {border: none; }");

	QToolButton *scalevalue = new QToolButton();
	scalevalue->setDefaultAction(m_page_scale);
	scalevalue->setStyleSheet("QToolButton {border: none; }");

	QToolButton *scaleplus = new QToolButton();
	scaleplus->setDefaultAction(m_increase_page_scale);
	scaleplus->setStyleSheet("QToolButton {border: none; }");

	QHBoxLayout *scalelayout = new QHBoxLayout();
	scalelayout->addSpacing(20);
	scalelayout->addWidget(scalelabel);
	scalelayout->addStretch();
	scalelayout->addWidget(scaleminus);
	scalelayout->addWidget(scalevalue);
	scalelayout->addWidget(scaleplus);

	QWidget *scalewidget = new QWidget();
	scalewidget->setLayout(scalelayout);

	QWidgetAction *scaleaction = new QWidgetAction(parent);
	scaleaction->setDefaultWidget(scalewidget);

	return scaleaction;
}

QIcon NS_Actions::QIconFromText(QString text)
{
	QPixmap pixmap(64, 64);
	QPainter painter(&pixmap);
	painter.fillRect(QRect(0, 0, 64, 64), QColor(230,230,230));
	QFont font = painter.font();
	font.setFamily("Helvetica");
	font.setStretch(75);
	font.setPixelSize(34);
	painter.setFont(font);
	painter.setPen(QColor(Qt::black));
	painter.drawText(0, 45, text);
	return QIcon(pixmap);
}


QString NS_Actions::QStringFromNsurl(struct nsurl *url)
{
	if (url == NULL) {
		return QString();
	}

	size_t idn_url_l;
	char *idn_url_s = NULL;
	if (nsurl_get_utf8(url, &idn_url_s, &idn_url_l) != NSERROR_OK) {
		/* idna conversion failed so use url verbaitum */
		return QString::fromUtf8(nsurl_access(url), nsurl_length(url));
	}
	QString res = QString::fromUtf8(idn_url_s, idn_url_l);
	free(idn_url_s);
	return res;

}


void NS_Actions::update_page_scale()
{
	double scale = browser_window_get_scale(m_bw) * 100.0;
	QString scaletext = QString::number(scale)+"%";
	m_reset_page_scale->setVisible(scale != nsoption_int(scale));
#ifdef USE_ICON_FOR_SCALE
	m_reset_page_scale->setIcon(QIconFromText(scaletext));
#endif
	m_reset_page_scale->setIconText(scaletext);
	m_page_scale->setIconText(scaletext);
}


void NS_Actions::update_page_info()
{
	/* manage page information state */
	browser_window_page_info_state pistate;
	pistate = browser_window_get_page_info_state(m_bw);
	if (pistate != m_pistate) {
		m_pistate = pistate;
		QString fname(":/icons/page-info-internal.png");
		switch (pistate) {
		case PAGE_STATE_LOCAL:
			fname=":/icons/page-info-local.png";
			break;

		case PAGE_STATE_INSECURE:
			fname=":/icons/page-info-insecure.png";
			break;

		case PAGE_STATE_SECURE_OVERRIDE:
			fname=":/icons/page-info-warning.png";
			break;

		case PAGE_STATE_SECURE_ISSUES:
			fname=":/icons/page-info-warning.png";
			break;

		case PAGE_STATE_SECURE:
			fname=":/icons/page-info-secure.png";
			break;

		default:
			break;
		}
		m_page_info->setIcon(QIcon(fname));
	}
}


void NS_Actions::update_bookmarks()
{
	/* manage bookmark state */
	struct nsurl *url;
	if (browser_window_get_url(m_bw, true, &url) != NSERROR_OK) {
		return;
	}
	bool marked = hotlist_has_url(url);
	nsurl_unref(url);
	if (marked != m_marked) {
		m_marked = marked;
		if (m_marked) {
			m_add_edit_bookmark->setIcon(QIcon(":/icons/hotlist-rmv.png"));
			m_add_edit_bookmark->setText(messages_get("EditBookmark"));
		} else {
			m_add_edit_bookmark->setIcon(QIcon(":/icons/hotlist-add.png"));
			m_add_edit_bookmark->setText(messages_get("AddBookmark"));
		}
	}
}


void NS_Actions::update_navigation(NS_Actions::Update update)
{
	/* manage stop/reload state */
	QWidget* parentw = qobject_cast<QWidget*>(parent());
	switch (update) {
	case UpdateInactive:
		m_active = false;
		m_stop_reload->setIcon(parentw->style()->standardIcon(QStyle::SP_BrowserReload));
		m_stop_reload->setText(messages_get("Reload"));
		m_stop_reload->setShortcut(QKeySequence::Refresh);
		break;

	case UpdateActive:
		m_active = true;
		m_stop_reload->setIcon(parentw->style()->standardIcon(QStyle::SP_BrowserStop));
		m_stop_reload->setText(messages_get("Stop"));
		m_stop_reload->setShortcut(QKeySequence::Cancel);
		break;

	default:
		break;
	}

	/* manage history navigation state */
	m_back->setEnabled(browser_window_history_back_available(m_bw));
	m_forward->setEnabled(browser_window_history_forward_available(m_bw));
}

#define PAGE_SCALE_COUNT 17

static const float page_scales[PAGE_SCALE_COUNT]={ 0.33, 0.50, 0.67, 0.75, 0.80, 0.90, 1.00, 1.10, 1.20, 1.33, 1.50, 1.70, 2.00, 2.40, 3.00, 4.00, 5.00 };

void NS_Actions::change_page_scale(int step)
{
	float scale = browser_window_get_scale(m_bw);
	float max_delta = FLT_MAX;
	int sel_idx = 0;
	for (int idx = 0; idx < PAGE_SCALE_COUNT; idx++) {
		float delta = fabs(page_scales[idx] - scale);
		if (delta < max_delta) {
			max_delta = delta;
			sel_idx = idx;
		}
	}
	sel_idx += step;
	if (sel_idx < 0) {
		sel_idx = 0;
	} else if (sel_idx >= PAGE_SCALE_COUNT) {
		sel_idx = PAGE_SCALE_COUNT - 1;
	}
	browser_window_set_scale(m_bw, page_scales[sel_idx], true);
	update_page_scale();
}

/* slots */

void NS_Actions::back_slot(bool checked)
{
	browser_window_history_back(m_bw, false);
}


void NS_Actions::forward_slot(bool checked)
{
	browser_window_history_forward(m_bw, false);
}


void NS_Actions::stop_reload_slot(bool checked)
{
	if (m_active) {
		browser_window_stop(m_bw);
	} else {
		browser_window_reload(m_bw, true);
	}
}


void NS_Actions::settings_slot(bool checked)
{
	NS_Application::instance()->settings_show();
}


void NS_Actions::bookmarks_slot(bool checked)
{
	NS_Application::instance()->bookmarks_show();
}


void NS_Actions::add_edit_bookmark_slot(bool checked)
{
	struct nsurl *url;
	if (browser_window_get_url(m_bw, true, &url) == NSERROR_OK) {
		bool marked = hotlist_has_url(url);
		if (marked) {
			hotlist_remove_url(url);
		} else {
			hotlist_add_url(url);
		}
		update_bookmarks();
		nsurl_unref(url);
	}
}


void NS_Actions::local_history_slot(bool checked)
{
	NS_Application::instance()->local_history_show(m_bw, actionGlobal(m_local_history));
}


void NS_Actions::global_history_slot(bool checked)
{
	NS_Application::instance()->global_history_show();
}


void NS_Actions::cookies_slot(bool checked)
{
	NS_Application::instance()->cookies_show();
}


void NS_Actions::page_info_slot(bool checked)
{
	NS_Application::instance()->page_info_show(m_bw, actionGlobal(m_page_info));
}


void NS_Actions::reset_page_scale_slot(bool checked)
{
	browser_window_set_scale(m_bw, (float)nsoption_int(scale)/100.0, true);
	update_page_scale();
}


void NS_Actions::reduce_page_scale_slot(bool checked)
{
	change_page_scale(-1);
}


void NS_Actions::increase_page_scale_slot(bool checked)
{
	change_page_scale(1);
}


void NS_Actions::newtab_slot(bool checked)
{
	NS_Application::create_browser_widget(m_bw, true);
}


void NS_Actions::newwindow_slot(bool checked)
{
	NS_Application::create_browser_widget(m_bw, false);
}


void NS_Actions::quit_slot(bool checked)
{
	NS_Application::instance()->quit();
}

void NS_Actions::page_save_slot(bool checked)
{
}


void NS_Actions::page_source_slot(bool checked)
{
}


void NS_Actions::debug_render_slot(bool checked)
{
}


void NS_Actions::debug_box_tree_slot(bool checked)
{
}


void NS_Actions::debug_dom_tree_slot(bool checked)
{
}


void NS_Actions::about_netsurf_slot(bool checked)
{
}


void NS_Actions::link_new_tab_slot(bool checked)
{
	NS_Application::create_browser_widget(m_link, m_bw, true);
}


void NS_Actions::link_new_window_slot(bool checked)
{
	NS_Application::create_browser_widget(m_link, m_bw, false);
}


void NS_Actions::link_bookmark_slot(bool checked)
{
	if (m_link == NULL) {
		return;
	}
	hotlist_add_url(m_link);
}


void NS_Actions::link_save_slot(bool checked)
{
}


void NS_Actions::link_copy_slot(bool checked)
{
	QGuiApplication::clipboard()->setText(QStringFromNsurl(m_link));
}


void NS_Actions::img_new_tab_slot(bool checked)
{
	NS_Application::create_browser_widget(m_object, m_bw, true);
}


void NS_Actions::img_save_slot(bool checked)
{
}


void NS_Actions::obj_save_slot(bool checked)
{
}


void NS_Actions::obj_copy_slot(bool checked)
{
	QGuiApplication::clipboard()->setText(QStringFromNsurl(hlcache_handle_get_url(m_object)));
}

void NS_Actions::sel_copy_slot(bool checked)
{
	if (m_selection == NULL) {
		return;
	}
	QGuiApplication::clipboard()->setText(m_selection);
}

void NS_Actions::sel_search_slot(bool checked)
{
	if (m_selection == NULL) {
		return;
	}
	nserror res;
	nsurl *url;

	res = search_web_omni(m_selection, SEARCH_WEB_OMNI_SEARCHONLY, &url);
	if (res == NSERROR_OK) {
		res = browser_window_create(
			(enum browser_window_create_flags)(BW_CREATE_HISTORY | BW_CREATE_TAB | BW_CREATE_FOREGROUND),
			url,
			NULL,
			m_bw,
			NULL);
		nsurl_unref(url);
	}
	if (res != NSERROR_OK) {
		NSLOG(netsurf,WARNING,"web search for %s failed with %s",m_selection, messages_get_errorcode(res));
	}
}
