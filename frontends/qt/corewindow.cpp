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
 * Corewindow implementation for qt.
 */

#include <QPaintEvent>
#include <QPainter>

extern "C" {
#include "utils/errors.h"
#include "netsurf/types.h"
#include "netsurf/plotters.h"
}

#include "qt/corewindow.cls.h"
#include "qt/plotters.h"
#include "qt/keymap.h"

struct nsqt_core_window {
	class NS_Corewindow *cw;
};
//QAbstractScrollArea
NS_Corewindow::NS_Corewindow(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f),
	  m_xoffset(0),
	  m_yoffset(0)
{
	m_core_window = new struct nsqt_core_window;
	m_core_window->cw = this;

	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);
}

void NS_Corewindow::paintEvent(QPaintEvent *event)
{
	struct rect clip;
	QPainter *painter;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &nsqt_plotters,
		.priv = NULL,
	};

	/* netsurf render clip region coordinates */
	clip.x0 = event->rect().left();
	clip.y0 = event->rect().top();
	clip.x1 = clip.x0 + event->rect().width();
	clip.y1 = clip.y0 + event->rect().height();

	painter = new QPainter(this);
	ctx.priv = painter;

	draw(&clip, &ctx);

	delete painter;
}


/**
 * key pressed event
 */
void NS_Corewindow::keyPressEvent(QKeyEvent *event)
{
	uint32_t nskey;
	nskey = qkeyevent_to_nskey(event);
	if (!key_press(nskey)) {
		QWidget::keyPressEvent(event);
	}
}

void NS_Corewindow::mouseMoveEvent(QMouseEvent *event)
{
	const QPointF pos = event->position();
	int bms = BROWSER_MOUSE_HOVER; /* empty state */
	mouse_action((browser_mouse_state)bms,
		     pos.x() + m_xoffset,
		     pos.y() + m_yoffset);
}


void NS_Corewindow::mousePressEvent(QMouseEvent *event)
{
	const QPointF pos = event->position();
	Qt::MouseButton button = event->button();
	int bms = BROWSER_MOUSE_HOVER; /* empty state */
	if ((button & Qt::LeftButton) == Qt::LeftButton) {
		bms |=BROWSER_MOUSE_PRESS_1;
	}
	if ((button & Qt::MiddleButton) == Qt::MiddleButton) {
		bms |=BROWSER_MOUSE_PRESS_2;
	}
	mouse_action((browser_mouse_state)bms,
		     pos.x() + m_xoffset,
		     pos.y() + m_yoffset);
}

void NS_Corewindow::mouseReleaseEvent(QMouseEvent *event)
{
	int bms = BROWSER_MOUSE_HOVER; /* empty state */
	const QPointF pos = event->position();
	Qt::MouseButton button = event->button();
	Qt::KeyboardModifiers mods = event->modifiers();

	if ((button & Qt::LeftButton) == Qt::LeftButton) {
		bms |=BROWSER_MOUSE_CLICK_1;
	}
	if ((button & Qt::MiddleButton) == Qt::MiddleButton) {
		bms |=BROWSER_MOUSE_CLICK_2;
	}
	/* keyboard modifiers */
	if ((mods & Qt::ShiftModifier)!=0)
		bms |= BROWSER_MOUSE_MOD_1;
	if ((mods & Qt::ControlModifier)!=0)
		bms |= BROWSER_MOUSE_MOD_2;
	if ((mods & Qt::AltModifier)!=0)
		bms |= BROWSER_MOUSE_MOD_3;

	mouse_action((browser_mouse_state)bms,
		     pos.x() + m_xoffset,
		     pos.y() + m_yoffset);

}


nserror NS_Corewindow::static_invalidate(struct core_window *cw,
					 const struct rect *rect)
{
	struct nsqt_core_window *nsqtcw = (struct nsqt_core_window *)cw;
	if (rect == NULL) {
		nsqtcw->cw->update();
	} else {
		nsqtcw->cw->update(rect->x0,
		       rect->y0,
		       rect->x1 - rect->x0,
		       rect->y1 - rect->y0);
		       }
	return NSERROR_OK;
}

nserror NS_Corewindow::static_set_extent(struct core_window *cw,
					 int width, int height)
{
	struct nsqt_core_window *nsqtcw = (struct nsqt_core_window *)cw;
	if ((width > 0) && (height > 0)) {
		nsqtcw->cw->resize(width, height);
	}
	return NSERROR_OK;
}

nserror NS_Corewindow::static_set_scroll(struct core_window *cw, int x, int y)
{
	return NSERROR_OK;
}

nserror NS_Corewindow::static_get_scroll(const struct core_window *cw, int *x, int *y)
{
	return NSERROR_OK;
}

nserror NS_Corewindow::static_get_dimensions(const struct core_window *cw,
					     int *width, int *height)
{
	struct nsqt_core_window *nsqtcw = (struct nsqt_core_window *)cw;
	QSize size = nsqtcw->cw->size();
	*width = size.width();
	*height = size.height();

	return NSERROR_OK;
}

nserror NS_Corewindow::static_drag_status(struct core_window *cw, core_window_drag_status ds)
{
	return NSERROR_OK;
}

static struct core_window_table cw_table = {
	.invalidate = NS_Corewindow::static_invalidate,
	.set_extent = NS_Corewindow::static_set_extent,
	.set_scroll = NS_Corewindow::static_set_scroll,
	.get_scroll = NS_Corewindow::static_get_scroll,
	.get_dimensions = NS_Corewindow::static_get_dimensions,
	.drag_status = NS_Corewindow::static_drag_status,
};
struct core_window_table *nsqt_core_window_table = &cw_table;
