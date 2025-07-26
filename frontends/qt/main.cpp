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


extern "C" {
#include "utils/log.h"
#include "utils/messages.h"

#include "netsurf/netsurf.h"
}

#include "qt/application.cls.h"

#include "qt/misc.h"
#include "qt/window.h"
#include "qt/corewindow.h"
#include "qt/fetch.h"
#include "qt/bitmap.h"
#include "qt/layout.h"
#include "qt/resources.h"


/**
 * Main entry point from OS.
 */
int main(int argc, char** argv)
{
	int ret = 0;
	struct netsurf_table nsqt_table = {
		.misc = nsqt_misc_table,
		.window = nsqt_window_table,
		.corewindow = nsqt_core_window_table,
		.download = NULL, /* no download functionality */
		.clipboard = NULL, /* no clipboard functionality */
		.fetch = nsqt_fetch_table,
		.file = NULL, /* use the posix default file operations */
		.utf8 = NULL, /* use default utf-8 processing */
		.search = NULL, /* use the default text search */
		.search_web = NULL, /* use default web search */
		.llcache = NULL, /* use default low level cache storage */
		.bitmap = nsqt_bitmap_table,
		.layout = nsqt_layout_table,
	};
	NS_Application *nsapp;

	/* qt application */
	try {
		nsapp = new NS_Application(argc, argv, &nsqt_table);
	} catch(NS_Exception &exp) {
		fprintf(stderr,
			"NetSurf qt application initialisation failed. %s (%s)\n",
			exp.m_str.c_str(),
			messages_get_errorcode(exp.m_err));
		return 2;
	}

	ret = nsapp->exec();

	delete nsapp;

	return ret;
}
