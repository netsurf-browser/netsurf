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
 * Local history corewindow.
 */
extern "C" {
#include "netsurf/content.h"
#include "netsurf/browser_window.h"

#include "desktop/local_history.h"
#include "desktop/browser_private.h"
}

#include "qt/local_history.cls.h"

NS_Local_history::NS_Local_history(QWidget *parent, struct browser_window *bw)
	: NS_Corewindow(parent, Qt::Popup),
	  m_session(nullptr)
{
	setMaximumSize(bw);
	local_history_init((struct core_window *)m_core_window, bw, &m_session);
}

NS_Local_history::~NS_Local_history()
{
	local_history_fini(m_session);
}

void NS_Local_history::setMaximumSize(struct browser_window *bw)
{
	QSize size(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
	int w, h;

	if (browser_window_get_dimensions(bw, &w, &h) == NSERROR_OK) {
		NS_Corewindow::setMaximumSize(w, h);
	}
}

nserror NS_Local_history::setbw(struct browser_window *bw)
{
	setMaximumSize(bw);
	return local_history_set(m_session, bw);
}

void NS_Local_history::draw(struct rect *clip, struct redraw_context *ctx)
{
	local_history_redraw(m_session, 0, 0, clip, ctx);
}

bool NS_Local_history::key_press(uint32_t nskey)
{
	return local_history_keypress(m_session, nskey);
}

void NS_Local_history::mouse_action(browser_mouse_state mouse_state, int x, int y)
{
	if (((mouse_state) && (!geometry().contains(mapToGlobal(QPoint(x,y))))) ||
	    (local_history_mouse_action(m_session, mouse_state, x, y) == NSERROR_OK)) {
		close();
	}
}
