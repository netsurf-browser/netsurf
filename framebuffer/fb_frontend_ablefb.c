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
#include <able/input.h>

#include "css/css.h"
#include "desktop/options.h"
#include "desktop/gui.h"
#include "desktop/options.h"
#include "utils/messages.h"
/* #include "desktop/textinput.h" cannot include this because it conflicts with the able defines */
#define NSKEY_PAGE_DOWN 135
#define NSKEY_PAGE_UP 134
#define NSKEY_DOWN 31
#define NSKEY_UP 30
#define NSKEY_LEFT 28
#define NSKEY_RIGHT 29
#define NSKEY_ESCAPE 27

#define KEY_LEFTSHIFT 1
#define KEY_RIGHTSHIFT 2
#define KEY_PAGEDOWN 3
#define KEY_PAGEUP 4
#define KEY_DOWN 5
#define KEY_UP 6
#define KEY_LEFT 7
#define KEY_RIGHT 8
#define KEY_ESC 9

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_tk.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_frontend.h"
#include "framebuffer/fb_options.h"

#include "utils/log.h"

int devfd;
int eventfd;
static const char *fbdevname = "(fb0)";
static const char *inputdevname = "(inputevent)";

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

        eventfd = open(inputdevname, O_RDONLY | O_NONBLOCK );

        return newfb;
}

void fb_os_quit(framebuffer_t *fb)
{
}

static int keymap[] = {
          -1,  -1, '1',  '2', '3', '4', '5', '6', '7', '8', /*  0 -  9 */
         '9', '0', '-',  '=',   8,   9, 'q', 'w', 'e', 'r', /* 10 - 19 */
         't', 'y', 'u',  'i', 'o', 'p', '[', ']',  13,  -1, /* 20 - 29 */
         'a', 's', 'd',  'f', 'g', 'h', 'j', 'k', 'l', ';', /* 30 - 39 */
         '\'', '#', -1, '\\', 'z', 'x', 'c', 'v', 'b', 'n', /* 40 - 49 */
         'm', ',', '.',  '/',  -1,  -1,  -1, ' ',  -1,  -1, /* 50 - 59 */
};

static int sh_keymap[] = {
          -1,  -1, '!', '"', 0xa3, '$', '%', '^', '&', '*', /*  0 -  9 */
         '(', ')', '_', '+',    8,   9, 'Q', 'W', 'E', 'R', /* 10 - 19 */
         'T', 'Y', 'U', 'I',  'O', 'P', '{', '}',  13,  -1, /* 20 - 29 */
         'A', 'S', 'D', 'F',  'G', 'H', 'J', 'K', 'L', ':', /* 30 - 39 */
         '@', '~',  -1, '|',  'Z', 'X', 'C', 'V', 'B', 'N', /* 40 - 49 */
         'M', '<', '>', '?',   -1,  -1,  -1, ' ',  -1,  -1, /* 50 - 59 */
};

/* performs character mapping */
static int keycode_to_ucs4(int code, bool shift)
{
        int ucs4 = -1;

        if (shift) {
                if ((code >= 0) && (code < sizeof(sh_keymap)))
                        ucs4 = sh_keymap[code];
        } else {
                if ((code >= 0) && (code < sizeof(keymap)))
                        ucs4 = keymap[code];
        }
        return ucs4;
}

void fb_os_input(fbtk_widget_t *root, bool active) 
{
        ssize_t amt;
        char key;
        struct input_event event;
        int ucs4 = -1;
        static bool shift = false;

        amt = read(0, &key, 1);

        if (amt > 0) {
                if (key == 'j') {
                        fbtk_input(root, NSKEY_UP);
                }
                if (key == 'k') {
                        fbtk_input(root, NSKEY_DOWN);
                }
                if (key == 'q') {
                        netsurf_quit = true;
                }
                if (key == 'd') {
                        list_schedule();
                }
        }

        amt = read(eventfd, &event, sizeof(struct input_event));
        if (amt == sizeof(struct input_event)) {
                if (event.type == EV_KEY) {
                        if (event.value == 0) {
                                /* key up */
                                switch (event.code) {
                                case KEY_LEFTSHIFT:
                                case KEY_RIGHTSHIFT:
                                        shift = false;
                                        break;

                                case BTN_LEFT:
                                        fbtk_click(root, BROWSER_MOUSE_CLICK_1);
                                        break;
                                }
                                return;
                        }

                        switch (event.code) {
                        case KEY_PAGEDOWN:
                                ucs4 = NSKEY_PAGE_DOWN;
                                break;

                        case KEY_PAGEUP:
                                ucs4 = NSKEY_PAGE_UP;
                                break;

                        case KEY_DOWN:
                                ucs4 = NSKEY_DOWN;
                                break;

                        case KEY_UP:
                                ucs4 = NSKEY_UP;
                                break;

                        case KEY_LEFT:
                                ucs4 = NSKEY_LEFT;
                                break;

                        case KEY_RIGHT:
                                ucs4 = NSKEY_RIGHT;
                                break;

                        case KEY_ESC:
                                ucs4 = NSKEY_ESCAPE;
                                break;

                        case BTN_LEFT:
                                fbtk_click(root, BROWSER_MOUSE_PRESS_1);
                                break;

                        case KEY_LEFTSHIFT:
                        case KEY_RIGHTSHIFT:
                                shift = true;
                                break;

                        default:
                                ucs4 = keycode_to_ucs4(event.code, shift);

                        }
                } else if (event.type == EV_REL) {
                        switch (event.code) {
                        case REL_X:
                                fbtk_move_pointer(root, event.value, 0, true);
                                break;

                        case REL_Y:
                                fbtk_move_pointer(root, 0, event.value, true);
                                break;

                        case REL_WHEEL:
                                if (event.value > 0)
                                        fbtk_input(root, NSKEY_UP);
                                else
                                        fbtk_input(root, NSKEY_DOWN);
                                break;
                        }
                } else if (event.type == EV_ABS) {
                        switch (event.code) {
                        case ABS_X:
                                fbtk_move_pointer(root, event.value, -1, false);
                                break;

                        case ABS_Y:
                                fbtk_move_pointer(root, -1, event.value, false);
                                break;

                        }
                }

                if (ucs4 != -1) {
                        fbtk_input(root, ucs4);
                        ucs4 = -1;
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

char *realpath(const char *path, char *resolved_path)
{
        strcpy(resolved_path, path);
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

