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
#include "desktop/global_history.h"
}

#include "qt/global_history.cls.h"

NS_Global_history::NS_Global_history(QWidget *parent) : NS_Corewindow(parent)
{
	global_history_init((struct core_window *)m_core_window);
}

NS_Global_history::~NS_Global_history()
{
	global_history_fini();
}


void NS_Global_history::draw(struct rect *clip, struct redraw_context *ctx)
{
	global_history_redraw(0, 0, clip, ctx);
}

bool NS_Global_history::key_press(uint32_t nskey)
{
	return global_history_keypress(nskey);
}

void NS_Global_history::mouse_action(browser_mouse_state mouse_state, int x, int y)
{
	global_history_mouse_action(mouse_state, x, y);
}
