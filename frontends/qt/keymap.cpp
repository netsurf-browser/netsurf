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
 * qt to netsurf key mapping.
 */

#include <QKeyEvent>

extern "C" {
#include "netsurf/keypress.h"
}

#include "qt/keymap.h"

uint32_t qkeyevent_to_nskey(QKeyEvent *event)
{
	uint32_t nskey = 0;
	Qt::KeyboardModifiers modifiers = event->modifiers();

	switch (event->key()) {
	case Qt::Key_Escape:
		nskey = NS_KEY_ESCAPE;
		break;

	case Qt::Key_Tab:
		nskey = NS_KEY_TAB;
		break;

	case Qt::Key_Backspace:
		if (modifiers & Qt::ShiftModifier) {
			nskey = NS_KEY_DELETE_LINE_START;
		} else if (modifiers & Qt::ControlModifier) {
			nskey = NS_KEY_DELETE_WORD_LEFT;
		} else {
			nskey = NS_KEY_DELETE_LEFT;
		}
		break;

	case Qt::Key_Delete:
		if (modifiers & Qt::ShiftModifier) {
			nskey = NS_KEY_DELETE_LINE_END;
		} else if (modifiers & Qt::ControlModifier) {
			nskey = NS_KEY_DELETE_WORD_RIGHT;
		} else {
			nskey = NS_KEY_DELETE_RIGHT;
		}
		break;

	case Qt::Key_Return:
	case Qt::Key_Enter:
		nskey = 10;
		break;

	case Qt::Key_Left:
		if (modifiers & Qt::ControlModifier) {
			nskey = NS_KEY_WORD_LEFT;
		} else {
			nskey = NS_KEY_LEFT;
		}
		break;

	case Qt::Key_Right:
		if (modifiers & Qt::ControlModifier) {
			nskey = NS_KEY_WORD_RIGHT;
		} else {
			nskey = NS_KEY_RIGHT;
		}
		break;

	case Qt::Key_Up:
		nskey = NS_KEY_UP;
		break;

	case Qt::Key_Down:
		nskey = NS_KEY_DOWN;
		break;

	case Qt::Key_Home:
		if (modifiers & Qt::ControlModifier) {
			nskey = NS_KEY_LINE_START;
		} else {
			nskey = NS_KEY_TEXT_START;
		}
		break;

	case Qt::Key_End:
		if (modifiers & Qt::ControlModifier) {
			nskey = NS_KEY_LINE_END;
		} else {
			nskey = NS_KEY_TEXT_END;
		}
		break;

	case Qt::Key_PageUp:
		nskey = NS_KEY_PAGE_UP;
		break;

	case Qt::Key_PageDown:
		nskey = NS_KEY_PAGE_DOWN;
		break;

	case Qt::Key_A:
		if (modifiers & Qt::ControlModifier) {
			nskey = NS_KEY_SELECT_ALL;
		}
		break;

	case Qt::Key_C:
		if (modifiers & Qt::ControlModifier) {
			nskey = NS_KEY_COPY_SELECTION;
		}
		break;

	case Qt::Key_U:
		if (modifiers & Qt::ControlModifier) {
			nskey = NS_KEY_DELETE_LINE;
		}
		break;

	case Qt::Key_V:
		if (modifiers & Qt::ControlModifier) {
			nskey = NS_KEY_PASTE;
		}
		break;

	case Qt::Key_X:
		if (modifiers & Qt::ControlModifier) {
			nskey = NS_KEY_CUT_SELECTION;
		}
		break;

	case Qt::Key_Y:
		if (modifiers & Qt::ControlModifier) {
			nskey = NS_KEY_REDO;
		}
		break;

	case Qt::Key_Z:
		if (modifiers & Qt::ControlModifier) {
			nskey = NS_KEY_UNDO;
		}
		break;

	default:
		break;
	}

	/* if no other match attempt to convert event to unicode code */
	if (nskey == 0) {
		QList<uint> lst = event->text().toUcs4();
		if (lst.isEmpty() == false) {
			nskey =	(uint32_t)lst[0];
		}
	}

	return nskey;
}
