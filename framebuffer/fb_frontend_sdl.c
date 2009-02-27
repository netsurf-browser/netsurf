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

#include <SDL/SDL.h>

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

#define FILE_PFX "/home/vince/netsurf/netsurf/framebuffer/res/"

static SDL_Surface *sdl_screen;

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

        if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
                fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
                return NULL;
        }
        atexit(SDL_Quit);

        newfb->width = fb_width;
        newfb->height = fb_height;
        newfb->bpp = fb_depth;

        sdl_screen = SDL_SetVideoMode(newfb->width, 
                                  newfb->height, 
                                  newfb->bpp, 
                                  SDL_SWSURFACE);

        if ( sdl_screen == NULL ) {
                fprintf(stderr, 
                        "Unable to set video: %s\n", SDL_GetError());
                free(newfb);
                return NULL;
        }

        newfb->ptr = sdl_screen->pixels;
        newfb->linelen = sdl_screen->pitch;

        SDL_ShowCursor(SDL_DISABLE);

        return newfb;
}

void fb_os_quit(framebuffer_t *fb)
{
}

void fb_os_input(struct gui_window *g, bool active) 
{
        int got_event;
        SDL_Event event;

        if (active)
                got_event = SDL_PollEvent(&event);
        else
                got_event = SDL_WaitEvent(&event);

        /* Do nothing if there was no event */
        if (got_event == 0)
	        return;

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
                    fb_window_scroll(g, -100, 0);
                    break;

            case SDLK_RIGHT:
                    fb_window_scroll(g, 100, 0);
                    break;

            default:
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
                        LOG(("Mouse button %d pressed at (%d,%d)\n",
                             event.button.button, event.button.x, event.button.y));

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
                        LOG(("Mouse button %d pressed at (%d,%d)\n",
                             event.button.button, event.button.x, event.button.y));

                }
                break;

        case SDL_QUIT:
            browser_window_destroy(g->bw);
    }

}

void
fb_os_option_override(void)
{
}

/* called by generic code to inform os code of screen update */
void
fb_os_redraw(struct bbox_s *box)
{
        SDL_UpdateRect(sdl_screen, 
                       box->x0, box->y0, 
                       box->x1 - box->x0, box->y1 - box->y0);
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

