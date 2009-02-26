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

#include <rfb/rfb.h>
#include <rfb/keysym.h>

#include "css/css.h"
#include "desktop/options.h"
#include "desktop/gui.h"
#include "desktop/options.h"
#include "utils/messages.h"
#include "desktop/history_core.h"

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_frontend.h"
#include "framebuffer/fb_cursor.h"
#include "framebuffer/fb_rootwindow.h"
#include "framebuffer/fb_options.h"

#include "utils/log.h"

static rfbScreenInfoPtr vnc_screen;
static struct gui_window *cur_window;

static void fb_vnc_doptr(int buttonMask,int x,int y,rfbClientPtr cl)
{
        struct gui_window *g = cur_window;
        if (buttonMask == 0) {
                fb_rootwindow_move(framebuffer, g, x, y, false);
        } else {

                /* left button */
                /*                if (buttonMask && 0x1) {
                                  fb_rootwindow_click(g, BROWSER_MOUSE_PRESS_1,
                                  fb_cursor_x(framebuffer),
                                  fb_cursor_y(framebuffer));
                                  }*/
                if (buttonMask && 0x1) {
                        fb_rootwindow_click(g, BROWSER_MOUSE_CLICK_1,
                                            fb_cursor_x(framebuffer),
                                            fb_cursor_y(framebuffer));
                }

                /* right button */
                /*if (buttonMask && 0x4) {
                  fb_rootwindow_click(g, BROWSER_MOUSE_PRESS_2,
                  fb_cursor_x(framebuffer),
                  fb_cursor_y(framebuffer));
                  }*/
                if (buttonMask && 0x4) {
                        fb_rootwindow_click(g, BROWSER_MOUSE_CLICK_2,
                                            fb_cursor_x(framebuffer),
                                            fb_cursor_y(framebuffer));
                }

                if (buttonMask && 0x8) {
                        /* wheelup */
                        fb_window_scroll(g, 0, -100);
                }

                if (buttonMask && 0x10) {
                        /* wheeldown */
                        fb_window_scroll(g, 0, 100);
                }



                /*
                  case SDL_MOUSEBUTTONUP:
                  switch (event.button.button) {

                  case SDL_BUTTON_LEFT:
                  fb_rootwindow_click(g, BROWSER_MOUSE_CLICK_1,
                  fb_cursor_x(framebuffer),
                  fb_cursor_y(framebuffer));
                  break;

                  case SDL_BUTTON_RIGHT:
                  fb_rootwindow_click(g, BROWSER_MOUSE_CLICK_2,
                  fb_cursor_x(framebuffer),
                  fb_cursor_y(framebuffer));
                  break;

                  default:
                  printf("Mouse button %d pressed at (%d,%d)\n",
                  event.button.button, event.button.x, event.button.y);

                  }
                  break;
                */
        }
}

static void fb_vnc_dokey(rfbBool down,rfbKeySym key,rfbClientPtr cl)
{
        struct gui_window *g = cur_window;

        LOG(("Processing keycode %d",key));
        if(down) {
                switch (key) {

                case XK_Page_Down:
                        fb_window_scroll(g, 0, g->height);
                        break;

                case XK_Page_Up:
                        fb_window_scroll(g, 0, -g->height);
                        break;

                case XK_Down:
                        fb_window_scroll(g, 0, 100);
                        break;

                case XK_Up:
                        fb_window_scroll(g, 0, -100);
                        break;

                case XK_Escape:
                        browser_window_destroy(g->bw);
                        break;

                case XK_Left:
                        if (history_back_available(g->bw->history))
                                history_back(g->bw, g->bw->history);
                        break;

                case XK_Right:
                        if (history_forward_available(g->bw->history))
                                history_forward(g->bw, g->bw->history);
                        break;

                default:
                        fb_rootwindow_input(g, key);
                        break;
                }
        }
}

framebuffer_t *fb_os_init(int argc, char** argv)
{
        framebuffer_t *newfb;
        int fb_width;
        int fb_height;
        int fb_depth;

        if ((option_window_width != 0) && (option_window_height != 0)) {
                fb_width = option_window_width;
                fb_height = option_window_height;
        } else {
                fb_width = 800;
                fb_height = 600;
        }

        fb_depth = option_fb_depth;
        if ((fb_depth != 32) && (fb_depth != 16) && (fb_depth != 8))
                fb_depth = 16; /* sanity checked depth in bpp */

        newfb = calloc(1, sizeof(framebuffer_t));
        if (newfb == NULL)
                return NULL;

        newfb->width = fb_width;
        newfb->height = fb_height;
        newfb->bpp = fb_depth;

        vnc_screen = rfbGetScreen(&argc, argv,
                                  newfb->width, newfb->height,
                                  8, 3, (fb_depth / 8));

        vnc_screen->frameBuffer = malloc(newfb->width * newfb->height * (fb_depth / 8));

        switch (fb_depth) {
        case 8:
                break;

        case 16:
                vnc_screen->serverFormat.trueColour=TRUE;
                vnc_screen->serverFormat.redShift = 11;
                vnc_screen->serverFormat.greenShift = 5;
                vnc_screen->serverFormat.blueShift = 0;
                vnc_screen->serverFormat.redMax = 31;
                vnc_screen->serverFormat.greenMax = 63;
                vnc_screen->serverFormat.blueMax = 31;
                break;

        case 32:
                vnc_screen->serverFormat.trueColour=TRUE;
                vnc_screen->serverFormat.redShift = 16;
                vnc_screen->serverFormat.greenShift = 8;
                vnc_screen->serverFormat.blueShift = 0;
                break;
        }

        vnc_screen->alwaysShared = TRUE;
        vnc_screen->ptrAddEvent = fb_vnc_doptr;
        vnc_screen->kbdAddEvent = fb_vnc_dokey;

        rfbInitServer(vnc_screen);

        newfb->ptr = vnc_screen->frameBuffer;
        newfb->linelen = newfb->width * (fb_depth / 8);

        //rfbUndrawCursor(vnc_screen);

        return newfb;
}

void fb_os_quit(framebuffer_t *fb)
{
}

void fb_os_input(struct gui_window *g, bool active)
{
        //SDL_Event event;
        cur_window = g;

        if (active)
                rfbProcessEvents(vnc_screen, 10000);
        else
                rfbProcessEvents(vnc_screen, 100000);
        /*
          switch (event.type) {
          case SDL_KEYDOWN:

          switch (event.key.keysym.sym) {

          case SDLK_PAGEDOWN:
          fb_window_scroll(g, 0, g->height);
          break;

          case SDLK_PAGEUP:
          fb_window_scroll(g, 0, -g->height);
          break;

          case SDLK_DOWN:
          fb_window_scroll(g, 0, 100);
          break;

          case SDLK_UP:
          fb_window_scroll(g, 0, -100);
          break;

          case SDLK_ESCAPE:
          browser_window_destroy(g->bw);
          break;

          case SDLK_LEFT:
          if (history_back_available(g->bw->history))
          history_back(g->bw, g->bw->history);
          break;

          case SDLK_RIGHT:
          if (history_forward_available(g->bw->history))
          history_forward(g->bw, g->bw->history);
          break;

          default:
          printf("The %s key was pressed!\n",
          SDL_GetKeyName(event.key.keysym.sym));
          fb_rootwindow_input(g, event.key.keysym.sym);
          break;
          }
          break;

          case SDL_MOUSEMOTION:
          fb_rootwindow_move(framebuffer, g, event.motion.x, event.motion.y, false);
          break;

          case SDL_MOUSEBUTTONDOWN:
          switch (event.button.button) {

          case SDL_BUTTON_LEFT:
          fb_rootwindow_click(g, BROWSER_MOUSE_PRESS_1,
          fb_cursor_x(framebuffer),
          fb_cursor_y(framebuffer));
          break;

          case SDL_BUTTON_RIGHT:
          fb_rootwindow_click(g, BROWSER_MOUSE_PRESS_2,
          fb_cursor_x(framebuffer),
          fb_cursor_y(framebuffer));
          break;

          case SDL_BUTTON_WHEELUP:
          fb_window_scroll(g, 0, -100);
          break;

          case SDL_BUTTON_WHEELDOWN:
          fb_window_scroll(g, 0, 100);
          break;

          case SDL_BUTTON_MIDDLE:
          default:
          printf("Mouse button %d pressed at (%d,%d)\n",
          event.button.button, event.button.x, event.button.y);

          }
          break;

          case SDL_MOUSEBUTTONUP:
          switch (event.button.button) {

          case SDL_BUTTON_LEFT:
          fb_rootwindow_click(g, BROWSER_MOUSE_CLICK_1,
          fb_cursor_x(framebuffer),
          fb_cursor_y(framebuffer));
          break;

          case SDL_BUTTON_RIGHT:
          fb_rootwindow_click(g, BROWSER_MOUSE_CLICK_2,
          fb_cursor_x(framebuffer),
          fb_cursor_y(framebuffer));
          break;

          default:
          printf("Mouse button %d pressed at (%d,%d)\n",
          event.button.button, event.button.x, event.button.y);

          }
          break;

          case SDL_QUIT:
          browser_window_destroy(g->bw);
          }
        */
}

void
fb_os_option_override(void)
{
}

/* called by generic code to inform os code of screen update */
void
fb_os_redraw(struct bbox_s *box)
{
        rfbMarkRectAsModified(vnc_screen, box->x0, box->y0, box->x1, box->y1);
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
