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
 * Widget methods for browsing context display.
 */

#include <QLineEdit>
#include <QWidgetAction>

extern "C" {
#include "utils/messages.h"
#include "utils/nsurl.h"

#include "desktop/browser_history.h"
#include "desktop/searchweb.h"
}

#include "qt/urlbar.cls.h"

/**
 * urlbar constructor
 */
NS_URLBar::NS_URLBar(QWidget* parent, NS_Actions *actions, struct browser_window *bw)
	: QToolBar(parent),
	  m_bw(bw)
{
	addAction(actions->m_back);
	addAction(actions->m_local_history);
	addAction(actions->m_forward);
	addAction(actions->m_stop_reload);

	m_input = new QLineEdit(parent);
	m_input->addAction(actions->m_page_info, QLineEdit::LeadingPosition);
	m_input->addAction(actions->m_add_edit_bookmark, QLineEdit::TrailingPosition);
#ifdef USE_ICON_FOR_SCALE
	m_input->addAction(actions->m_reset_page_scale,
			   QLineEdit::TrailingPosition);
#endif
	addWidget(m_input);

#ifndef USE_ICON_FOR_SCALE
	addAction(actions->m_reset_page_scale);
#endif

	// burger menu
	m_burgermenu = new QMenu(parent);

	m_burgermenu->addAction(actions->m_newtab);
	m_burgermenu->addAction(actions->m_newwindow);

	m_burgermenu->addSeparator();

	QMenu *bookmarksmenu = m_burgermenu->addMenu(messages_get("Bookmarks"));
	bookmarksmenu->addAction(actions->m_add_edit_bookmark);
	bookmarksmenu->addAction(actions->m_bookmarks);

	m_burgermenu->addAction(actions->m_global_history);

	m_burgermenu->addSeparator();

	m_burgermenu->addAction(actions->page_scale_widget_action(parent));

	m_burgermenu->addSeparator();

	m_burgermenu->addAction(actions->m_settings);

	QMenu *moretoolsmenu = m_burgermenu->addMenu(messages_get("MoreTools"));
	moretoolsmenu->addAction(actions->m_cookies);
	moretoolsmenu->addAction(actions->m_page_source);
	moretoolsmenu->addAction(actions->m_debug_render);
	moretoolsmenu->addAction(actions->m_debug_box_tree);
	moretoolsmenu->addAction(actions->m_debug_dom_tree);

	QMenu *helpmenu = m_burgermenu->addMenu(messages_get("Help"));
	helpmenu->addAction(actions->m_about_netsurf);

	m_burgermenu->addSeparator();

	m_burgermenu->addAction(actions->m_quit);

	// buger menu button
	m_burgerbutton = new QToolButton(parent);
	m_burgerbutton->setText("⋮");
	m_burgerbutton->setMenu(m_burgermenu);
	m_burgerbutton->setPopupMode(QToolButton::InstantPopup);
	m_burgerbutton->setStyleSheet("::menu-indicator {image: none}");
	addWidget(m_burgerbutton);

	connect(m_input, &QLineEdit::returnPressed,
		this, &NS_URLBar::input_pressed);
}

void NS_URLBar::input_pressed()
{
	nserror res;
	nsurl *url;
	QByteArray url_string = m_input->text().toUtf8();
	res = search_web_omni(url_string, SEARCH_WEB_OMNI_NONE, &url);
	if (res == NSERROR_OK) {
		res = browser_window_navigate(this->m_bw,
					      url,
					      NULL,
					      BW_NAVIGATE_HISTORY,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(url);
	}
}

nserror NS_URLBar::set_url(struct nsurl *url)
{
	size_t idn_url_l;
	char *idn_url_s = NULL;
	if (nsurl_get_utf8(url, &idn_url_s, &idn_url_l) == NSERROR_OK) {
		m_input->setText(QString::fromUtf8(idn_url_s, idn_url_l));
		free(idn_url_s);
	} else {
		m_input->setText(QString::fromUtf8(nsurl_access(url),
						   nsurl_length(url)));
	}

	return NSERROR_OK;
}
