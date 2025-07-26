/*
 * Copyright 2025 Vincent Sanders <vince@netsurf-browser.org>
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
 * Implementation of status splitter.
 */

extern "C" {
#include "utils/nsoption.h"
}

#include "qt/statussplitter.cls.h"


NS_StatusSplitter::NS_StatusSplitter(QLabel *status, QScrollBar *scrollbar, QWidget *parent)
	: QSplitter(parent),
	  m_resize_move(false)
{
	setChildrenCollapsible(false);
	addWidget(status);
	addWidget(scrollbar);
	connect(this,&QSplitter::splitterMoved,
		this, &NS_StatusSplitter::moved_slot);
}


void NS_StatusSplitter::moved_slot(int pos,int index)
{
	if (m_resize_move) {
		m_resize_move = false;
	} else {
		nsoption_set_int(toolbar_status_size,
				 ((pos * 10000) / size().width()));
	}
}


void NS_StatusSplitter::resizeEvent(QResizeEvent *event)
{
	QSplitter::resizeEvent(event);
	m_resize_move = true;
	moveSplitter((event->size().width() * nsoption_int(toolbar_status_size))/10000, 1);
}
