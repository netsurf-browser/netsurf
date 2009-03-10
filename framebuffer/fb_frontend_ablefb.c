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
#include <able/fb.h>

#include "css/css.h"
#include "desktop/options.h"
#include "desktop/gui.h"
#include "desktop/options.h"
#include "utils/messages.h"
#include "desktop/textinput.h"

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_tk.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_frontend.h"
#include "framebuffer/fb_options.h"

#include "utils/log.h"

int devfd;
static const char *fbdevname = "(fb0)";

framebuffer_t *fb_os_init(int argc, char** argv)
{
        framebuffer_t *newfb;
        struct fb_info_s *fbinfo;
        argb_t palent;
        int ploop;
        int res;

        /* open display device */
        devfd = open(option_fb_device ? option_fb_device : fbdevname, O_RDWR);
        if (devfd < 0) {
                LOG(("Error opening output device %s", fbdevname));
                return NULL;
        }

        LOG(("Opened %s fd is %d",fbdevname, devfd));

        res = ioctl(devfd, IOCTL_FB_GETINFO, &fbinfo);

        if (res < 0) {
                LOG(("Output device error"));
                close(devfd);
                return NULL;
        }

        LOG(("Framebuffer device name %s bpp %d\n",
             fbinfo->name,
             fbinfo->screeninfo->bits_per_pixel));

        newfb = calloc(1, sizeof(framebuffer_t));

        newfb->width = fbinfo->screeninfo->xres;
        newfb->height = fbinfo->screeninfo->yres;
        newfb->ptr = fbinfo->video_start + fbinfo->video_scroll;
        newfb->linelen = fbinfo->line_len;
        newfb->bpp = fbinfo->screeninfo->bits_per_pixel;

        if (newfb->bpp <= 8) {
                for(ploop=0; ploop<256; ploop++) {
                        palent = fbinfo->cur_palette[ploop];

                        newfb->palette[ploop] = 0xFF000000 | 
                                                palent.b << 16 | 
                                                palent.g << 8 | 
                                                palent.r ; 
                }
        }


        /* set stdin to nonblocking */
        fcntl(0, F_SETFL, O_NONBLOCK);

        return newfb;
}

void fb_os_quit(framebuffer_t *fb)
{
}

void fb_os_input(fbtk_widget_t *root, bool active) 
{
        ssize_t amt;
        char key;

        amt = read(0, &key, 1);

        if (amt > 0) {
                if (key == 'j') {
                        fbtk_input(root, KEY_UP);
                }
                if (key == 'k') {
                        fbtk_input(root, KEY_DOWN);
                }
                if (key == 'q') {
                        netsurf_quit = true;
                }
                if (key == 'd') {
                        list_schedule();
                }
        }

}

void
fb_os_option_override(void)
{
        /* override some options */
        option_max_cached_fetch_handles = 1;
        option_max_fetchers = option_max_fetchers_per_host = 1;
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

