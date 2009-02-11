/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
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
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

#include "css/css.h"
#include "desktop/options.h"
#include "desktop/gui.h"
#include "desktop/options.h"
#include "utils/messages.h"

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_frontend.h"

#include "utils/log.h"

#define FILE_PFX "/home/vince/netsurf/netsurf-fb/framebuffer/res/"

framebuffer_t *fb_os_init(int argc, char** argv)
{
        framebuffer_t *newfb;

        newfb = calloc(1, sizeof(framebuffer_t));

        newfb->width = 800;
        newfb->height = 600;
        newfb->bpp = 16;
        newfb->ptr = malloc((newfb->width * newfb->height * newfb->bpp) / 8);
        newfb->linelen = (newfb->width * newfb->bpp) / 8;


        /* load browser messages */
        messages_load(FILE_PFX "messages");

        /* load browser options */        
	options_read(FILE_PFX "Options");


        default_stylesheet_url = strdup("file://" FILE_PFX "default.css");

        return newfb;
}

void fb_os_quit(framebuffer_t *fb)
{
}

void fb_os_input(struct gui_window *g) 
{
}

/* called by generic code to inform os code of screen update */
void
fb_os_redraw(struct bbox_s *box)
{
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

