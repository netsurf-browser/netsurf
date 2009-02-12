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

#include "utils/log.h"

#define FILE_PFX "/home/vince/netsurf/netsurf/framebuffer/res/"

static SDL_Surface *sdl_screen;

framebuffer_t *fb_os_init(int argc, char** argv)
{
        framebuffer_t *newfb;

        newfb = calloc(1, sizeof(framebuffer_t));
        if (newfb == NULL)
                return NULL;

        if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
                fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
                return NULL;
        }
        atexit(SDL_Quit);

        newfb->width = 800;
        newfb->height = 600;
        newfb->bpp = 32;

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
        SDL_Event event;

        if (active)
                SDL_PollEvent(&event);
        else
                SDL_WaitEvent(&event);

        switch (event.type) {
        case SDL_KEYDOWN:

            switch (event.key.keysym.sym) {

            case SDLK_j:
                    fb_window_scroll(g, 0, 100);
                    break;
                    
            case SDLK_k:
                    fb_window_scroll(g, 0, -100);
                    break;
                    
            case SDLK_q:
                    browser_window_destroy(g->bw);
                    break;

            case SDLK_b:
                    if (history_back_available(g->bw->history))
                            history_back(g->bw, g->bw->history);
                    break;

            case SDLK_f:
                    if (history_forward_available(g->bw->history))
                            history_forward(g->bw, g->bw->history);
                    break;

            default:
                    printf("The %s key was pressed!\n",
                           SDL_GetKeyName(event.key.keysym.sym));
                    break;
            }
            break;

        case SDL_MOUSEMOTION:
                fb_cursor_move_abs(framebuffer, 
                                   event.motion.x, 
                                   event.motion.y);
                break;

        case SDL_MOUSEBUTTONDOWN:
                fb_cursor_click(framebuffer, g, BROWSER_MOUSE_CLICK_1);
                /*                printf("Mouse button %d pressed at (%d,%d)\n",
                                  event.button.button, event.button.x, event.button.y);*/
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

