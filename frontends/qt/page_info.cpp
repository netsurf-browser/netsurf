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
 * Page info corewindow.
 */
extern "C" {
#include "desktop/page-info.h"
}

#include "qt/page_info.cls.h"

NS_Page_info::NS_Page_info(QWidget *parent, struct browser_window *bw)
	: NS_Corewindow(parent, Qt::Popup),
	  m_session(nullptr)
{
	setAttribute(Qt::WA_DeleteOnClose);
	page_info_create((struct core_window *)m_core_window, bw, &m_session);
}

NS_Page_info::~NS_Page_info()
{
	page_info_destroy(m_session);
}

void NS_Page_info::draw(struct rect *clip, struct redraw_context *ctx)
{
	page_info_redraw(m_session, 0, 0, clip, ctx);
}

bool NS_Page_info::key_press(uint32_t nskey)
{
	return page_info_keypress(m_session, nskey);
}

void NS_Page_info::mouse_action(browser_mouse_state mouse_state, int x, int y)
{
	bool did_something = false;
	if ((mouse_state) && (!geometry().contains(mapToGlobal(QPoint(x,y))))) {
		/* click outside window */
		did_something = true;
	} else {
		page_info_mouse_action(m_session, mouse_state, x, y, &did_something);
	}
	if (did_something) {
		close();
	}
}
