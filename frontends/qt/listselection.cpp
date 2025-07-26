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
 * Implementation of netsurf list selection widget for qt.
 */

#include <QVBoxLayout>

#include "qt/listselection.cls.h"

NS_ListSelection::NS_ListSelection(QWidget *parent) :
	QWidget(parent),
	m_source(new FirstListWidget),
	m_selected(new FirstListWidget),
	m_add(new QToolButton),
	m_rem(new QToolButton),
	m_sel_up(new QToolButton),
	m_sel_down(new QToolButton)
{
	m_count = 0;

	m_source->setResizeMode(QListView::Adjust);
	m_selected->setResizeMode(QListView::Adjust);
	m_selected->setDragDropMode(QAbstractItemView::InternalMove);

	m_add->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
	m_add->setAccessibleName("Add");
	connect(m_add, &QAbstractButton::clicked,
		this, &NS_ListSelection::addtoselection);

	m_rem->setIcon(style()->standardIcon(QStyle::SP_ArrowLeft));
	m_rem->setAccessibleName("Remove");
	connect(m_rem, &QAbstractButton::clicked,
		this, &NS_ListSelection::remfromselection);

	m_sel_up->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
	m_sel_up->setAccessibleName("Move up");
	connect(m_sel_up, &QAbstractButton::clicked,
		this, &NS_ListSelection::selectionup);

	m_sel_down->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
	m_sel_down->setAccessibleName("Move down");
	connect(m_sel_down, &QAbstractButton::clicked,
		this, &NS_ListSelection::selectiondown);

	QVBoxLayout *midlayout = new QVBoxLayout;
	midlayout->addWidget(m_add);
	midlayout->addWidget(m_rem);
	QVBoxLayout *endlayout = new QVBoxLayout;
	endlayout->addWidget(m_sel_up);
	endlayout->addWidget(m_sel_down);

	QHBoxLayout *layout = new QHBoxLayout;
	layout->addWidget(m_source);
	layout->addLayout(midlayout);
	layout->addWidget(m_selected);
	layout->addLayout(endlayout);

	setLayout(layout);
}


void NS_ListSelection::addItem(const char *label, const char *data)
{
	QListWidgetItem *item = new QListWidgetItem(label, m_source);
	item->setData(Qt::UserRole, data);
	item->setData(NS_ListSelection::SourcePosRole, m_count++);
}


void NS_ListSelection::addItem(QByteArray &label, QByteArray &data)
{
	QListWidgetItem *item = new QListWidgetItem(label, m_source);
	item->setData(Qt::UserRole, QString(data));
	item->setData(NS_ListSelection::SourcePosRole, m_count++);
}


void NS_ListSelection::selectItem(const char *data)
{
	int source_count = m_source->count();
	int idx;
	for (idx=0; idx < source_count; idx++) {
		QListWidgetItem *item = m_source->item(idx);
		if (item != NULL) {
			if (item->data(Qt::UserRole) == data) {
				selectRow(idx);
				break;
			}
		}
	}
}


void NS_ListSelection::deselectAll()
{
	while (m_selected->count() > 0) {
		deselectRow(0);
	}
}


QList<QByteArray> NS_ListSelection::selection()
{
	QList<QByteArray>ret;
	int scount = m_selected->count();
	int idx;
	for (idx=0; idx < scount; idx++) {
		ret.append(m_selected->item(idx)->data(Qt::UserRole).toString().toUtf8());
	}
	return ret;
}


void NS_ListSelection::addtoselection(bool checked)
{
	selectRow(m_source->currentRow());
}


void NS_ListSelection::remfromselection(bool checked)
{
	deselectRow(m_selected->currentRow());
}


void NS_ListSelection::selectionup(bool checked)
{
	int c_row=m_selected->currentRow();
	if (c_row > 0) {
		QListWidgetItem *titem = m_selected->takeItem(c_row);
		m_selected->insertItem(c_row - 1, titem);
		m_selected->setCurrentRow(c_row - 1);
	}
}


void NS_ListSelection::selectiondown(bool checked)
{
	int c_row = m_selected->currentRow();
	if (c_row < (m_selected->count()-1)) {
		QListWidgetItem *titem = m_selected->takeItem(c_row);
		m_selected->insertItem(c_row + 1, titem);
		m_selected->setCurrentRow(c_row + 1);
	}
}


void NS_ListSelection::selectRow(int selected_row)
{
	QListWidgetItem *titem = m_source->takeItem(selected_row);
	if (titem == NULL) {
		return;
	}
	m_selected->addItem(titem);
	m_selected->setCurrentItem(titem);
	m_selected->updateGeometry();

}


void NS_ListSelection::deselectRow(int selected_row)
{
	QListWidgetItem *titem = m_selected->takeItem(selected_row);
	if (titem == NULL) {
		return;
	}

	int sposition = titem->data(NS_ListSelection::SourcePosRole).toInt();
	// compute how many earlier source rows are selected
	int sposoff = 0;
	int scount = m_selected->count();
	for(int idx = 0; idx < scount; idx++) {
		if (m_selected->item(idx)->data(NS_ListSelection::SourcePosRole).toInt() < sposition) {
			sposoff++;
		}
	}

	m_source->insertItem(sposition - sposoff, titem);
	m_source->setCurrentItem(titem);
	m_source->updateGeometry();
}
