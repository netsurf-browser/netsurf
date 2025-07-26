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
 * Global history corewindow.
 */
extern "C" {
#include "desktop/cookie_manager.h"
}

#include "qt/cookies.cls.h"

NS_Cookies::NS_Cookies(QWidget *parent) : NS_Corewindow(parent)
{
	cookie_manager_init((struct core_window *)m_core_window);
}

NS_Cookies::~NS_Cookies()
{
	cookie_manager_fini();
}

nserror NS_Cookies::setSearch(const char *search_term)
{
	return cookie_manager_set_search_string(search_term);
}

void NS_Cookies::draw(struct rect *clip, struct redraw_context *ctx)
{
	cookie_manager_redraw(0, 0, clip, ctx);
}

bool NS_Cookies::key_press(uint32_t nskey)
{
	return cookie_manager_keypress(nskey);
}

void NS_Cookies::mouse_action(browser_mouse_state mouse_state, int x, int y)
{
	cookie_manager_mouse_action(mouse_state, x, y);
}
