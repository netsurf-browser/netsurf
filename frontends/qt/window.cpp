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
 * Implementation of netsurf window (widget) for qt.
 */

#include <QGridLayout>

extern "C" {

#include "utils/errors.h"
#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
#include "utils/messages.h"

#include "netsurf/types.h"
#include "netsurf/mouse.h"
#include "netsurf/window.h"
#include "netsurf/plotters.h"
#include "netsurf/content.h"
#include "netsurf/browser_window.h"
#include "netsurf/mouse.h"

#include "desktop/browser_history.h"

}

#include "qt/window.cls.h"
#include "qt/scaffolding.cls.h"
#include "qt/statussplitter.cls.h"
#include "qt/misc.h"
#include "qt/window.h"

/**
 * time (in ms) between throbber animation frame updates
 */
#define THROBBER_FRAME_TIME (100)

#define THROBBER_FRAME_COUNT (8)

struct gui_window {
	class NS_Window *window;
};


/**
 * netsurf window class constructor
 */
NS_Window::NS_Window(QWidget *parent, struct browser_window *bw)
	: QWidget(parent),
	  m_bw(bw),
	  m_throbber_frame(0),
	  m_favicon(new QIcon(":favicon.png"))
{
	setFocusPolicy(Qt::StrongFocus);

	// setup actions
	m_actions = new NS_Actions(this, m_bw);

	// url bar
	m_nsurlbar = new NS_URLBar(this, m_actions, m_bw);

	// browser drawing canvas widget
	m_nswidget = new NS_Widget(this, m_actions, bw);

	// horizontal scrollbar
	m_hscrollbar = new QScrollBar(Qt::Horizontal);
	m_hscrollbar->setMinimum(0);
	m_hscrollbar->setMaximum(1);
	m_hscrollbar->setPageStep(1);
	connect(m_hscrollbar, &QScrollBar::valueChanged,
		m_nswidget, &NS_Widget::setHorizontalScroll);

	// vertical scrollbar
	m_vscrollbar = new QScrollBar(Qt::Vertical);
	m_vscrollbar->setMinimum(0);
	m_vscrollbar->setMaximum(1);
	m_vscrollbar->setPageStep(1);
	connect(m_vscrollbar, &QScrollBar::valueChanged,
		m_nswidget, &NS_Widget::setVerticalScroll);

	/* status */
	m_status = new QLabel();
	NS_StatusSplitter *splitter = new NS_StatusSplitter(m_status, m_hscrollbar);

	// Build browser window grid layout
	QGridLayout *layout = new QGridLayout(this);
	layout->setContentsMargins(0,0,0,0);
	layout->setHorizontalSpacing(0);
	layout->setVerticalSpacing(0);
	layout->addWidget(m_nsurlbar, 0, 0, 1, 2);
	layout->addWidget(m_nswidget,1,0);
	layout->addWidget(m_vscrollbar, 1,1);
	layout->setRowStretch(1,1);
	layout->addWidget(splitter, 2,0);

	/* actions */
	addAction(m_actions->m_quit);
	addAction(m_actions->m_newtab);
	addAction(m_actions->m_newwindow);

}

NS_Window::~NS_Window()
{
	advance_throbber(false);
	delete m_favicon;
}

void NS_Window::closeEvent(QCloseEvent *event)
{
	browser_window_destroy(m_bw);
}

void NS_Window::wheelEvent(QWheelEvent *event)
{
	QPoint pixels = event->pixelDelta();
	if (!pixels.isNull()) {
		m_hscrollbar->setValue(m_hscrollbar->value() + pixels.x());
		m_vscrollbar->setValue(m_vscrollbar->value() - pixels.y());
	} else {
		QPoint angle = event->angleDelta() / 8;
		if (!angle.isNull()) {
			if (angle.x() >= 15) {
				m_hscrollbar->triggerAction(QAbstractSlider::SliderSingleStepAdd);
			} else if (angle.x() <= -15) {
				m_hscrollbar->triggerAction(QAbstractSlider::SliderSingleStepSub);
			}
			if (angle.y() >= 15) {
				m_vscrollbar->triggerAction(QAbstractSlider::SliderSingleStepSub);
			} else if (angle.y() <= -15) {
				m_vscrollbar->triggerAction(QAbstractSlider::SliderSingleStepAdd);
			}

		}
	}
	event->accept();
}

void NS_Window::keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
	case Qt::Key_Left:
		m_hscrollbar->triggerAction(QAbstractSlider::SliderSingleStepSub);
		break;

	case Qt::Key_Right:
		m_hscrollbar->triggerAction(QAbstractSlider::SliderSingleStepAdd);
		break;

	case Qt::Key_Up:
		m_vscrollbar->triggerAction(QAbstractSlider::SliderSingleStepSub);
		break;

	case Qt::Key_Down:
		m_vscrollbar->triggerAction(QAbstractSlider::SliderSingleStepAdd);
		break;

	case Qt::Key_Home:
		m_vscrollbar->setValue(m_vscrollbar->minimum());
		break;

	case Qt::Key_End:
		m_vscrollbar->setValue(m_vscrollbar->maximum());
		break;

	case Qt::Key_PageUp:
		m_vscrollbar->triggerAction(QAbstractSlider::SliderPageStepSub);
		break;

	case Qt::Key_PageDown:
		m_vscrollbar->triggerAction(QAbstractSlider::SliderPageStepAdd);
		break;

	default:
		QWidget::keyPressEvent(event);
		break;
	}
}

/**
 * destroy a tab
 */
void NS_Window::destroy(void)
{
	browser_window_destroy(m_bw);
}


void NS_Window::set_status(const char *text)
{
	m_status->setText(text);
}


nserror NS_Window::set_scroll(const struct rect *rect)
{
	m_hscrollbar->setValue(rect->x0);
	m_vscrollbar->setValue(rect->y0);

	return NSERROR_OK;
}


nserror NS_Window::update_extent()
{
	int ew, eh;
	nserror res;
	res = browser_window_get_extents(m_bw, true, &ew, &eh);
	if (res != NSERROR_OK) {
		return res;
	}

	int width = m_nswidget->size().width();
	m_hscrollbar->setMaximum(std::max(ew - width, m_hscrollbar->minimum()));
	m_hscrollbar->setPageStep(width);
	m_hscrollbar->setSingleStep(width / 16);
	int height = m_nswidget->size().height();
	m_vscrollbar->setMaximum(std::max(eh - height,m_vscrollbar->minimum()));
	m_vscrollbar->setPageStep(height);
	m_vscrollbar->setSingleStep(height / 16);

	return NSERROR_OK;
}

static void next_throbber_frame(void *p)
{
	NS_Window *window = (NS_Window *)p;
	window->advance_throbber(true);
}

nserror NS_Window::advance_throbber(bool cont)
{
	if (cont == false) {
		nsqt_schedule(-1, next_throbber_frame, this);
		m_throbber_frame = 0;
		iconChanged(*m_favicon);
		return NSERROR_OK;
	}

	m_throbber_frame++;
	if (m_throbber_frame > THROBBER_FRAME_COUNT) {
		m_throbber_frame = 1;
	}

	QIcon frame(":throbber" + QString().setNum(m_throbber_frame) + ".png");
	iconChanged(frame);

	nsqt_schedule(THROBBER_FRAME_TIME, next_throbber_frame, this);
	return NSERROR_OK;
}

void NS_Window::set_favicon(QIcon *icon)
{
	delete m_favicon;
	if (icon == nullptr) {
		m_favicon = new QIcon(":favicon.png");
	} else {
		m_favicon = icon;
	}

	if (m_throbber_frame == 0) {
		iconChanged(*m_favicon);
	}
}

/* static methods */

/**
 * Set the status bar message of a browser window.
 *
 * \param g gui_window to update
 * \param text new status text
 */
void NS_Window::static_set_status(struct gui_window *gw, const char *text)
{
	gw->window->set_status(text);
}


/**
 * Set the title of a window.
 *
 * \param gw The gui window to set title of.
 * \param title new window title
 */
void NS_Window::static_set_title(struct gui_window *gw, const char *title)
{
	gw->window->titleChanged(title);
}


/**
 * Set the icon of a window.
 *
 * \param gw The gui window to set title of.
 * \param title new window title
 */
void NS_Window::static_set_icon(struct gui_window *gw,
				struct hlcache_handle *icon_handle)
{
	QIcon *icon = nullptr;

	if (icon_handle != NULL) {
		struct bitmap *icon_bitmap = NULL;
		icon_bitmap = content_get_bitmap(icon_handle);
		if (icon_bitmap != NULL) {
			const QImage img = *(QImage *)icon_bitmap;
			icon = new QIcon(QPixmap::fromImage(img, Qt::AutoColor));
		}
	}

	gw->window->set_favicon(icon);
}


/**
 * Get the scroll position of a browser window.
 *
 * \param gw The gui window to obtain the scroll position from.
 * \param sx receives x ordinate of point at top-left of window
 * \param sy receives y ordinate of point at top-left of window
 * \return true iff successful
 */
bool NS_Window::static_get_scroll(struct gui_window *gw, int *sx, int *sy)
{
	return gw->window->m_nswidget->get_scroll(sx, sy);
}


/**
 * Set the scroll position of a browser window.
 *
 * scrolls the viewport to ensure the specified rectangle of the
 *   content is shown.
 * If the rectangle is of zero size i.e. x0 == x1 and y0 == y1
 *   the contents will be scrolled so the specified point in the
 *   content is at the top of the viewport.
 * If the size of the rectangle is non zero the frontend may
 *   add padding or centre the defined area or it may simply
 *   align as in the zero size rectangle
 *
 * \param gw The gui window to scroll.
 * \param rect The rectangle to ensure is shown.
 * \return NSERROR_OK on success or appropriate error code.
 */
nserror
NS_Window::static_set_scroll(struct gui_window *gw, const struct rect *rect)
{
	return gw->window->set_scroll(rect);
}


/**
 * Set the navigation url.
 *
 * \param gw window to update.
 * \param url The url to use as icon.
 */
nserror NS_Window::static_set_url(struct gui_window *gw, struct nsurl *url)
{
	nserror res;
	res = gw->window->m_nsurlbar->set_url(url);
	gw->window->m_actions->update(NS_Actions::UpdateUnchanged);
	return res;
}


/**
 * Miscellaneous event occurred for a window
 *
 * This is used to inform the frontend of window events which
 *   require no additional parameters.
 *
 * \param gw The gui window the event occurred for
 * \param event Which event has occurred.
 * \return NSERROR_OK if the event was processed else error code.
 */
nserror
NS_Window::static_event(struct gui_window *gw, enum gui_window_event event)
{
	nserror res;

	switch (event) {
	case GW_EVENT_UPDATE_EXTENT:
		res = gw->window->update_extent();
		break;

	case GW_EVENT_REMOVE_CARET:
		gw->window->m_nswidget->setCaret(false, 0, 0, 0);
		break;

	case GW_EVENT_START_THROBBER:
		res = gw->window->advance_throbber(true);
		gw->window->m_actions->update(NS_Actions::UpdateActive);
		break;

	case GW_EVENT_STOP_THROBBER:
		res = gw->window->advance_throbber(false);
		gw->window->m_actions->update(NS_Actions::UpdateInactive);
		break;

	case GW_EVENT_PAGE_INFO_CHANGE:
		gw->window->m_actions->update(NS_Actions::UpdatePageInfo);
		break;

	default:
		res = NSERROR_OK;
		break;
	}
	return res;
}


/**
 * Invalidate an area of a window.
 *
 * The specified area of the window should now be considered
 *  out of date. If the area is NULL the entire window must be
 *  invalidated. It is expected that the windowing system will
 *  then subsequently cause redraw/expose operations as
 *  necessary.
 *
 * \note the frontend should not attempt to actually start the
 *  redraw operations as a result of this callback because the
 *  core redraw functions may already be threaded.
 *
 * \param gw The gui window to invalidate.
 * \param rect area to redraw or NULL for the entire window area
 * \return NSERROR_OK on success or appropriate error code
 */
nserror
NS_Window::static_invalidate(struct gui_window *gw, const struct rect *rect)
{
	return gw->window->m_nswidget->invalidate(rect);
}


/**
 * Find the current dimensions of a browser window's content area.
 *
 * This is used to determine the actual available drawing size
 * in pixels. This allows contents that can be dynamically
 * reformatted, such as HTML, to better use the available
 * space.
 *
 * \param gw The gui window to measure content area of.
 * \param width receives width of window
 * \param height receives height of window
 * \return NSERROR_OK on success and width and height updated
 *          else error code.
 */
nserror
NS_Window::static_get_dimensions(struct gui_window *gw, int *width, int *height)
{
	return gw->window->m_nswidget->get_dimensions(width, height);
}


/**
 * Change mouse pointer shape
 *
 * \param gw The gui window to set pointer for.
 * \param shape The new shape to change to.
 */
void
NS_Window::static_set_pointer(struct gui_window *gw, enum gui_pointer_shape shape)
{
	gw->window->m_nswidget->set_pointer(shape);
}

/**
 * Place the caret in a browser window.
 *
 * \param  gw	   window with caret
 * \param  x	   document relative x coordinate of caret
 * \param  y	   document relative y coordinate of caret
 * \param  height  height of caret
 * \param  clip	   document relative clip rectangle, or NULL if none
 */
void
NS_Window::static_place_caret(struct gui_window *gw,
		       int x, int y, int height,
		       const struct rect *clip)
{
	gw->window->m_nswidget->setCaret(true, x, y, height);
}

/**
 * Create and open a gui window for a browsing context.
 *
 * The implementing front end must create a context suitable
 *  for it to display a window referred to as the "gui window".
 *
 * The frontend will be expected to request the core redraw
 *  areas of the gui window which have become invalidated
 *  either from toolkit expose events or as a result of a
 *  invalidate() call.
 *
 * Most core operations used by the frontend concerning browser
 *  windows require passing the browser window context therefor
 *  the gui window must include a reference to the browser
 *  window passed here.
 *
 * If GW_CREATE_CLONE flag is set existing is non-NULL.
 *
 * \param bw The core browsing context associated with the gui window
 * \param existing An existing gui_window, may be NULL.
 * \param flags flags to control the gui window creation.
 * \return gui window, or NULL on error.
 */
struct gui_window *
NS_Window::static_create(struct browser_window *bw,
		     struct gui_window *existing,
		     gui_window_create_flags flags)
{
	struct gui_window *gw;
	NS_Scaffold *scaffold;
	QWidget *existingpage=nullptr;
	int tabidx;

	gw = (struct gui_window *)calloc(1, sizeof(struct gui_window));

	if (gw == NULL) {
		return NULL;
	}

	if (existing != NULL) {
		existingpage = existing->window;
	}
	scaffold = NS_Scaffold::get_scaffold(existingpage,
					     ((flags & GW_CREATE_TAB) != 0));
	if (scaffold == NULL) {
		free(gw);
		return NULL;
	}

	gw->window = new NS_Window(nullptr, bw);
	if (gw->window == NULL) {
		free(gw);
		return NULL;
	}

	tabidx = scaffold->addTab(gw->window, messages_get("NewTab"));
	connect(gw->window, &NS_Window::titleChanged,
		scaffold, &NS_Scaffold::changeTabTitle);
	connect(gw->window, &NS_Window::iconChanged,
		scaffold, &NS_Scaffold::changeTabIcon);
	if (flags & GW_CREATE_FOREGROUND) {
		scaffold->setCurrentIndex(tabidx);
	}
	scaffold->show();

	return gw;
}


/**
 * Destroy previously created gui window
 *
 * \param gw The gui window to destroy.
 */
void NS_Window::static_destroy(struct gui_window *gw)
{
	gw->window->deleteLater();
	free(gw);
}


/**
 * window operations table for qt frontend
 */
static struct gui_window_table window_table = {
	.create = NS_Window::static_create,
	.destroy = NS_Window::static_destroy,
	.invalidate = NS_Window::static_invalidate,
	.get_scroll = NS_Window::static_get_scroll,
	.set_scroll = NS_Window::static_set_scroll,
	.get_dimensions = NS_Window::static_get_dimensions,
	.event = NS_Window::static_event,
	.set_title = NS_Window::static_set_title,
	.set_url = NS_Window::static_set_url,
	.set_icon = NS_Window::static_set_icon,
	.set_status = NS_Window::static_set_status,
	.set_pointer = NS_Window::static_set_pointer,
	.place_caret = NS_Window::static_place_caret,
	.drag_start = NULL,
	.save_link = NULL,
	.create_form_select_menu = NULL,
	.file_gadget_open = NULL,
	.drag_save_object = NULL,
	.drag_save_selection = NULL,
	.console_log = NULL,
};

struct gui_window_table *nsqt_window_table = &window_table;
