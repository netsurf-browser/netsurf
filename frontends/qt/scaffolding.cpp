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
 * Implementation of netsurf scaffolding (widget) for qt.
 */

#include <QTabBar>
#include <QProxyStyle>

extern "C" {
#include "utils/log.h"
#include "utils/messages.h"

#include "netsurf/content.h"
}

#include "qt/application.cls.h"
#include "qt/scaffolding.cls.h"
#include "qt/scaffoldstyle.cls.h"
#include "qt/window.cls.h"

static NS_Scaffold *current = nullptr; /**< currently selected scaffold */

ScaffoldStyle::ScaffoldStyle(QObject *parent)
	: QProxyStyle()
{
	setParent(parent);
}

/**
 * proxy style for sub element rect for TabWidgetRightCorner
 *
 * move the right corner widget to just after the tabbar area. padding is added
 * horizontaly and verticaly. padding size is derived from vertical gap to
 * center of the tabbar.
 *
 * \todo cope with reverse layout
 */
QRect ScaffoldStyle::subElementRect(SubElement subElement,
				    const QStyleOption *option,
				    const QWidget *widget) const
{
	if (subElement == QStyle::SE_TabWidgetRightCorner) {
		QRect tabRect = QProxyStyle::subElementRect(QStyle::SE_TabWidgetTabBar, option, widget);
		QRect rightCornerRect = QProxyStyle::subElementRect(QStyle::SE_TabWidgetRightCorner, option, widget);

		int padding = (tabRect.height() - rightCornerRect.height()) / 2;
		int x = qMin(tabRect.left() + tabRect.width() + padding,
			     rightCornerRect.x());
		return QRect(x,
			     rightCornerRect.y() - padding,
			     rightCornerRect.width(),
			     rightCornerRect.height());

	}

	return QProxyStyle::subElementRect(subElement, option, widget);
}


NS_Scaffold::NS_Scaffold(QWidget *parent)
	: QTabWidget(parent),
	m_newtab(new QAction("+", this))
{
	setStyle (new ScaffoldStyle(this));

	m_newtab->setToolTip(messages_get("NewTab"));
	m_newtab->setShortcut(QKeySequence::AddTab);
	connect(m_newtab, &QAction::triggered,
		this, &NS_Scaffold::newtab_slot);

	QToolButton *addbutton = new QToolButton(this);
	addbutton->setDefaultAction(m_newtab);
	addbutton->setStyleSheet("QToolButton {border:0} QToolButton:hover {background-color: rgba(255, 255, 255, 0.5);}");
	setCornerWidget(addbutton, Qt::TopRightCorner);

	connect(tabBar(), &QTabBar::tabCloseRequested,
		this, &NS_Scaffold::destroyTab);
	connect(this, &QTabWidget::currentChanged,
		this, &NS_Scaffold::changeTab);
	setTabsClosable(true);
	setFocusPolicy(Qt::StrongFocus);
}

/**
 * close event destroys all tabs
 */
void NS_Scaffold::closeEvent(QCloseEvent *event)
{
	/* build a list of the window objects and iterate it to call destroy */
	QList<NS_Window *> pages;
	for (int idx = 0; idx < count();idx++) {
		pages.append((NS_Window *)widget(idx));
	}
	for (int idx = 0; idx < pages.size(); idx++) {
		pages.at(idx)->destroy();
	}
}

/**
 * destroy tab slot
 */
void NS_Scaffold::destroyTab(int index)
{
	NS_Window *page;
	page = (NS_Window *)widget(index);
	page->destroy();
}

/**
 * changed tab slot
 */
void NS_Scaffold::changeTab(int index)
{
	if (index == -1) {
		this->deleteLater();
	}
}

void NS_Scaffold::newtab_slot(bool checked)
{
	current = this;
	NS_Application::create_browser_widget(NULL, true);
}

void NS_Scaffold::changeTabTitle(const char *title)
{
	QObject *tabsender = sender();
	QWidget *tabwidget;

	for (int index=0 ;
	     (tabwidget=(NS_Window *)widget(index)) != nullptr;
	     index++) {
		if (tabwidget == tabsender) {
			setTabText(index, title);
			if (isTabVisible(index)) {
				setWindowTitle(title);
			}
			break;
		}
	}
}

void NS_Scaffold::changeTabIcon(const QIcon &icon)
{
	QObject *tabsender = sender();
	QWidget *tabwidget;

	for (int index=0 ;
	     (tabwidget=(NS_Window *)widget(index)) != nullptr;
	     index++) {
		if (tabwidget == tabsender) {
			setTabIcon(index, icon);
			if (isTabVisible(index)) {
				setWindowIcon(icon);
			}
			break;
		}
	}
}


/**
 * get a scaffolding widget
 *
 * \param page An existing page widget or NULL if not present
 * \param use_current True if an current scaffold should be used
 */
NS_Scaffold *
NS_Scaffold::get_scaffold(QWidget *page, bool use_current)
{
	NS_Scaffold *scaffold = nullptr;
	if (use_current && (page != nullptr)) {
		/* todo check page parent is actually a scaffold */
		scaffold = qobject_cast<NS_Scaffold *>(page->parentWidget()->parentWidget());
	} else if ((use_current) && (current != nullptr)) {
		scaffold = current;
	} else {
		scaffold = new NS_Scaffold(nullptr);

	}
	NSLOG(netsurf, DEBUG, "page:%p use_current:%d current:%p scaffold:%p",
	      page, use_current, current, scaffold);
	current = scaffold;
	return scaffold;
}
