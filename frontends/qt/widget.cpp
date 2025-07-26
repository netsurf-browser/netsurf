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

#include <QPaintEvent>
#include <QPainter>

extern "C" {
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/nsoption.h"

#include "netsurf/plotters.h"
#include "netsurf/keypress.h"
#include "netsurf/content.h"
}

#include "qt/application.cls.h"
#include "qt/plotters.h"
#include "qt/keymap.h"
#include "qt/misc.h"
#include "qt/widget.cls.h"


#define CARET_WIDTH 1

#define CARET_FRAME_COUNT (2)

/**
 * netsurf widget class constructor
 */
NS_Widget::NS_Widget(QWidget *parent, NS_Actions *actions, struct browser_window *bw)
	: QWidget(parent, Qt::Widget),
	  m_bw(bw),
	  m_actions(actions),
	  m_xoffset(0),
	  m_yoffset(0),
	  m_pointer_shape(GUI_POINTER_DEFAULT),
	  m_contextmenu(new QMenu(this)),
	  m_drag_state(BROWSER_MOUSE_HOVER),
	  m_caret_frame(0)
{
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);
}


/* map qt event modifiers to browser mouse state keyboard modifiers */
void NS_Widget::event_to_bms_modifiers(QMouseEvent *event, unsigned int &bms)
{
	Qt::KeyboardModifiers mods = event->modifiers();
	if ((mods & Qt::ShiftModifier) != 0) {
		bms |= BROWSER_MOUSE_MOD_1;
	}
	if ((mods & Qt::ControlModifier) != 0) {
		bms |= BROWSER_MOUSE_MOD_2;
	}
	if ((mods & Qt::AltModifier) != 0) {
		bms |= BROWSER_MOUSE_MOD_3;
	}
	if ((mods & Qt::MetaModifier) != 0) {
		bms |= BROWSER_MOUSE_MOD_4;
	}
}


/* method to redraw caret called from redraw event */
void NS_Widget::redraw_caret(struct rect *clip, struct redraw_context *ctx)
{
	if (m_caret_frame != 1) {
		return;
	}
	QPainter* painter = (QPainter*)ctx->priv;
	QPen pen(Qt::SolidLine);
	painter->setPen(pen);
	QPainter::CompositionMode oldmode = painter->compositionMode();
	painter->setCompositionMode(QPainter::RasterOp_NotDestination);
	painter->drawLine(m_caret_x - m_xoffset,
			  m_caret_y - m_yoffset,
			  m_caret_x - m_xoffset,
			  m_caret_y + m_caret_h - m_yoffset);
	painter->setCompositionMode(oldmode);
}


/* cause caret animation to advance a frame and schedule the next */
void NS_Widget::next_caret_frame(void *p)
{
	NS_Widget *widget = (NS_Widget *)p;
	int frame_time = NS_Application::cursorFlashTime();

	if (frame_time < 100) {
		frame_time = 0;
	} else if (widget->m_caret_frame > 0) {
		/* first time through animation holds first frame twice as long */
		frame_time = frame_time / 2;
	}

	widget->m_caret_frame++;
	if (widget->m_caret_frame > CARET_FRAME_COUNT) {
		widget->m_caret_frame = 1;
	}

	widget->update(widget->m_caret_x - widget->m_xoffset,
		       widget->m_caret_y - widget->m_yoffset,
		       widget->m_caret_x + CARET_WIDTH - widget->m_xoffset,
		       widget->m_caret_y + widget->m_caret_h - widget->m_yoffset);

	if (frame_time != 0) {
		nsqt_schedule(frame_time, next_caret_frame, p);
	}
}


/* widget has lost focus */
void NS_Widget::focusOutEvent(QFocusEvent *event)
{
	setCaret(false, 0, 0, 0);
}


/**
 * widget has been resized
 */
void NS_Widget::resizeEvent(QResizeEvent *event)
{
	browser_window_schedule_reformat(m_bw);
}


/**
 * redraw the netsurf browsing widget
 */
void NS_Widget::paintEvent(QPaintEvent *event)
{
	QPainter *painter = new QPainter(this);
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &nsqt_plotters,
		.priv = painter,
	};
	/* netsurf render clip region coordinates */
	struct rect clip = {
		.x0 = event->rect().left(),
		.y0 = event->rect().top(),
		.x1 = event->rect().left() + event->rect().width(),
		.y1 = event->rect().top() + event->rect().height(),
	};

	browser_window_redraw(m_bw,
			      - m_xoffset,
			      - m_yoffset,
			      &clip,
			      &ctx);

	redraw_caret(&clip, &ctx);

	delete painter;
}


void NS_Widget::mousePressEvent(QMouseEvent *event)
{
	QPointF pos = event->position();
	/** the mouse button the event was generated for */
	Qt::MouseButton button = event->button();
	unsigned int bms = BROWSER_MOUSE_HOVER; /* empty state */

	/* pressed buttons */
	if ((button & Qt::LeftButton) != 0) {
		bms |= BROWSER_MOUSE_PRESS_1;
	}
	if ((button & Qt::MiddleButton) != 0) {
		bms |= BROWSER_MOUSE_PRESS_2;
	}
	if ((button & Qt::RightButton) != 0) {
		bms |= BROWSER_MOUSE_PRESS_3;
	}
	if ((button & Qt::BackButton) != 0) {
		bms |= BROWSER_MOUSE_PRESS_4;
	}
	if ((button & Qt::ForwardButton) != 0) {
		bms |= BROWSER_MOUSE_PRESS_5;
	}

	/* record last press position */
	if (bms != BROWSER_MOUSE_HOVER) {
		m_press_pos = pos;
	}

	/* keyboard modifiers */
	event_to_bms_modifiers(event, bms);

	browser_window_mouse_click(m_bw,
				   (browser_mouse_state)bms,
				   pos.x() + m_xoffset,
				   pos.y() + m_yoffset);
}


void NS_Widget::mouseMoveEvent(QMouseEvent *event)
{
	const QPointF pos = event->position();
	Qt::MouseButtons buttons = event->buttons();
	unsigned int bms = BROWSER_MOUSE_HOVER;

	if (buttons != Qt::NoButton) {
		/* mouse movement with buttons held
		 * press event has set m_press_pos
		 */
		if (m_drag_state == BROWSER_MOUSE_HOVER) {
			/* drag not started,
			 * ensure cursor has moved significantly
			 * from start position
			 */
			if ((fabs(pos.x() - m_press_pos.x()) > 5.0) ||
			    (fabs(pos.y() - m_press_pos.y()) > 5.0)) {
				/* start drag operation */
				if ((buttons & Qt::LeftButton) != 0) {
					/* Start button 1 drag where button initialy pressed */
					browser_window_mouse_click(m_bw,
								   BROWSER_MOUSE_DRAG_1,
								   m_press_pos.x() + m_xoffset,
								   m_press_pos.y() + m_yoffset);
					m_drag_state = BROWSER_MOUSE_HOLDING_1;
				} else if ((buttons & Qt::MiddleButton) != 0) {
					/* Start button 2 drag where button initialy pressed */
					browser_window_mouse_click(m_bw,
								   BROWSER_MOUSE_DRAG_2,
								   m_press_pos.x() + m_xoffset,
								   m_press_pos.y() + m_yoffset);
					m_drag_state = BROWSER_MOUSE_HOLDING_2;
				}
			}
		}

		if (m_drag_state != BROWSER_MOUSE_HOVER) {
			bms = m_drag_state | BROWSER_MOUSE_DRAG_ON;
		}
	}

	/* keyboard modifiers */
	event_to_bms_modifiers(event, bms);

	browser_window_mouse_track(m_bw,
				   (browser_mouse_state)bms,
				   pos.x() + m_xoffset,
				   pos.y() + m_yoffset);
}


void NS_Widget::mouseReleaseEvent(QMouseEvent *event)
{
	/** the position the event was generated for */
	const QPointF pos = event->position();
	/** the mouse button the event was generated for */
	Qt::MouseButton button = event->button();
	/** browser mouse state to report starts empty */
	unsigned int bms = BROWSER_MOUSE_HOVER;

	/* released button */
	if ((button & Qt::LeftButton) != 0) {
		if (m_drag_state == BROWSER_MOUSE_HOLDING_1) {
			/* terminate drag */
			browser_window_mouse_track(m_bw,
						   (browser_mouse_state)BROWSER_MOUSE_HOVER,
						   pos.x() + m_xoffset,
						   pos.y() + m_yoffset);
			m_drag_state = BROWSER_MOUSE_HOVER;
		} else {
			bms |= BROWSER_MOUSE_CLICK_1;
		}
	}
	if ((button & Qt::MiddleButton) != 0) {
		if (m_drag_state == BROWSER_MOUSE_HOLDING_2) {
			/* terminate drag */
			browser_window_mouse_track(m_bw,
						   (browser_mouse_state)BROWSER_MOUSE_HOVER,
						   pos.x() + m_xoffset,
						   pos.y() + m_yoffset);
			m_drag_state = BROWSER_MOUSE_HOVER;
		} else {
			bms |= BROWSER_MOUSE_CLICK_2;
		}
	}
	if ((button & Qt::RightButton) != 0) {
		bms |= BROWSER_MOUSE_CLICK_3;
	}
	if ((button & Qt::BackButton) != 0) {
		bms |= BROWSER_MOUSE_CLICK_4;
	}
	if ((button & Qt::ForwardButton) != 0) {
		bms |= BROWSER_MOUSE_CLICK_5;
	}

	/* keyboard modifiers */
	event_to_bms_modifiers(event, bms);

	browser_window_mouse_click(m_bw,
				   (browser_mouse_state)bms,
				   pos.x() + m_xoffset,
				   pos.y() + m_yoffset);
}


void NS_Widget::keyPressEvent(QKeyEvent *event)
{
	uint32_t nskey;
	nskey = qkeyevent_to_nskey(event);
	if (browser_window_key_press(m_bw, nskey) == false) {
		QWidget::keyPressEvent(event);
	}
}


/*
 * open a relevant context menu
 *
 * get the features of the browser window where it was opened
 *  | link | object | selection | menu to open     |
 *  |------|--------|-----------|------------------|
 *  |      |        |           | context          |
 *  |   x  |        |           | link             |
 *  |      |   x    |           | object           |
 *  |   x  |   x    |           | link+object      |
 *  |      |        |     x     | copy             |
 *  |   x  |        |     x     | link+copy        |
 *  |      |   x    |     x     | object+copy      |
 *  |   x  |   x    |     x     | link+object+copy |
 *
 */
void NS_Widget::contextMenuEvent(QContextMenuEvent *event)
{
	struct browser_window_features features;
	char *selected_text;

	browser_window_get_features(m_bw,
				    event->x() + m_xoffset,
				    event->y() + m_yoffset,
				    &features);
        selected_text = browser_window_get_selection(m_bw);

	if ((selected_text == NULL) && (features.link_title != NULL)) {
		selected_text = (char *)malloc(features.link_title_length + 1);
		memcpy(selected_text, features.link_title, features.link_title_length);
		*(selected_text + features.link_title_length) = 0;
	}

	m_actions->update(features.link, features.object, selected_text);

	m_contextmenu->clear();

	if ((features.link == NULL) &&
	    (features.object == NULL) &&
	    (selected_text==NULL)) {
		/* populate base menu */
		m_contextmenu->addAction(m_actions->m_back);
		m_contextmenu->addAction(m_actions->m_forward);
		m_contextmenu->addAction(m_actions->m_stop_reload);
		m_contextmenu->addSeparator();
		m_contextmenu->addAction(m_actions->m_add_edit_bookmark);
		m_contextmenu->addSeparator();
		m_contextmenu->addAction(m_actions->m_page_save);
		m_contextmenu->addSeparator();
		m_contextmenu->addAction(m_actions->m_page_source);
	} else {
		bool prev = false; // have there been previous menu entries
		if (features.link != NULL) {
			/* link entries */
			m_contextmenu->addAction(m_actions->m_link_new_tab);
			m_contextmenu->addAction(m_actions->m_link_new_window);
			m_contextmenu->addSeparator();
			m_contextmenu->addAction(m_actions->m_link_bookmark);
			m_contextmenu->addAction(m_actions->m_link_save);
			m_contextmenu->addAction(m_actions->m_link_copy);
			prev = true;
		}
		if (features.object != NULL) {
			/* object/image entries */
			if (prev) {
				m_contextmenu->addSeparator();
			} else {
				prev = true;
			}
			if(content_get_type(features.object) == CONTENT_IMAGE) {
				m_contextmenu->addAction(m_actions->m_img_new_tab);
				m_contextmenu->addAction(m_actions->m_img_save);
				m_contextmenu->addAction(m_actions->m_img_copy);
			} else {
				m_contextmenu->addAction(m_actions->m_obj_save);
				m_contextmenu->addAction(m_actions->m_obj_copy);
			}
		}
		if (selected_text != NULL) {
			if (prev) {
				m_contextmenu->addSeparator();
			} else {
				prev = true;
			}
			m_contextmenu->addAction(m_actions->m_sel_copy);
			m_contextmenu->addAction(m_actions->m_sel_search);
		}
		/** @todo are there any additional entries like "inspect" */
	}
	m_contextmenu->popup(event->globalPos());
}


/**
 * get the current scroll offsets
 */
bool NS_Widget::get_scroll(int *sx, int *sy)
{
	*sx = m_xoffset;
	*sy = m_yoffset;

	return true;
}


/**
 * get the viewable dimensions of browsing context
 */
nserror NS_Widget::get_dimensions(int *width, int *height)
{
	*width = size().width();
	*height = size().height();

	return NSERROR_OK;
}


/**
 * change pointer
 */
void NS_Widget::set_pointer(enum gui_pointer_shape set_shape)
{
	enum Qt::CursorShape qshape = Qt::ArrowCursor;

	/* if the shape is being changed to what is already set there is
	 * nothing to do.
	 */
	if (m_pointer_shape == set_shape) {
		return;
	}
	m_pointer_shape = set_shape;

	switch (m_pointer_shape) {
	case GUI_POINTER_POINT:
		qshape = Qt::PointingHandCursor;
		break;
	case GUI_POINTER_CARET:
		qshape = Qt::IBeamCursor;
		break;
	case GUI_POINTER_CROSS:
		qshape = Qt::CrossCursor;
		break;
	case GUI_POINTER_MOVE:
		qshape = Qt::OpenHandCursor;
		break;
	case GUI_POINTER_NOT_ALLOWED:
	case GUI_POINTER_NO_DROP:
		qshape = Qt::ForbiddenCursor;
		break;
	case GUI_POINTER_WAIT:
		qshape = Qt::WaitCursor;
		break;
	case GUI_POINTER_HELP:
		qshape = Qt::WhatsThisCursor;
		break;
	case GUI_POINTER_UP:
	case GUI_POINTER_DOWN:
		qshape = Qt::SizeVerCursor;
		break;
	case GUI_POINTER_LEFT:
	case GUI_POINTER_RIGHT:
		qshape = Qt::SizeHorCursor;
		break;
	case GUI_POINTER_RU:
	case GUI_POINTER_LD:
		qshape = Qt::SizeBDiagCursor;
		break;
	case GUI_POINTER_LU:
	case GUI_POINTER_RD:
		qshape = Qt::SizeFDiagCursor;
		break;
	case GUI_POINTER_PROGRESS:
		qshape = Qt::BusyCursor;
		break;
	case GUI_POINTER_MENU:
	case GUI_POINTER_DEFAULT:
	default:
		qshape = Qt::ArrowCursor;
		break;
	}
	setCursor(QCursor(qshape));
}


/**
 * Change position of caret
 */
void NS_Widget::setCaret(bool visible, int cx, int cy, int ch)
{
	/* compute area to redraw and update caret position */
	if (m_caret_frame != 0) {
		int x0, y0, x1, y1;
		x0 = m_caret_x - m_xoffset;
		y0 = m_caret_y - m_yoffset;
		x1 = m_caret_x + CARET_WIDTH - m_xoffset;
		y1 = m_caret_y + m_caret_h - m_yoffset;

		if (visible) {
			/*
			 * caret is being shown and should remain visible.
			 * reschedule animation
			 */
			m_caret_x = cx;
			m_caret_y = cy;
			m_caret_h = ch;

			m_caret_frame = 0;
			next_caret_frame(this);
		} else {
			/*
			 * caret being shown but should no longer be visible.
			 * remove animation scheduling.
			 */
			nsqt_schedule(-1, next_caret_frame, this);
			m_caret_frame = 0;
		}
		/*
		 * update previous caret area in widget relative coordinates.
		 * must be done after updating the cursor position
		 */
		update(x0, y0, x1, y1);
	} else {
		if (visible) {
			/* caret not being shown and now should be update region
			 * of newly drawn caret only
			 */
			m_caret_x = cx;
			m_caret_y = cy;
			m_caret_h = ch;

			next_caret_frame(this);

		} else {
			/* not being shown and should remain so */
		}
	}
}


/**
 * mark an area of the browsing context as invalid
 */
nserror NS_Widget::invalidate(const struct rect *rect)
{
	if (rect == NULL) {
		update();
	} else {
		update(rect->x0,
		       rect->y0,
		       rect->x1 - rect->x0,
		       rect->y1 - rect->y0);
	}
	return NSERROR_OK;
}


/**
 * slot to recive horizontal scroll signal
 */
void NS_Widget::setHorizontalScroll(int value)
{
	m_xoffset = value;
	update();
}

/**
 * slot to recive vertical scroll signal
 */
void NS_Widget::setVerticalScroll(int value)
{
	m_yoffset = value;
	update();
}


QSize NS_Widget::sizeHint() const
{
	int width = nsoption_int(window_width);
	if (width == 0) {
		width = 1000;
	}
	int height = nsoption_int(window_height);
	if (height == 0) {
		height = 700;
	}
	QSize s(width, height);
	return s;
}
