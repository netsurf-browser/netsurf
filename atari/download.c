/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <windom.h>
#include <hubbub/hubbub.h>

#include "content/urldb.h"
#include "content/fetch.h"
#include "css/utils.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/plotters.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "desktop/save_complete.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "render/html.h"
#include "utils/url.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include "atari/gui.h"

struct gui_download_window *gui_download_window_create(download_context *ctx,
		struct gui_window *parent)
{
        return NULL;
}

nserror gui_download_window_data(struct gui_download_window *dw, 
		const char *data, unsigned int size)
{
	return NSERROR_OK;
}

void gui_download_window_error(struct gui_download_window *dw,
                               const char *error_msg)
{
}

void gui_download_window_done(struct gui_download_window *dw)
{
}