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

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_frontend.h"

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
        newfb->bpp = 16;

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

        return newfb;
}

void fb_os_quit(framebuffer_t *fb)
{
}

void fb_os_input(struct gui_window *g) 
{
        SDL_Event event;

        SDL_PollEvent(&event);//SDL_WaitEvent(&event);

        switch (event.type) {
        case SDL_KEYDOWN:
            printf("The %s key was pressed!\n",
                   SDL_GetKeyName(event.key.keysym.sym));
            break;

        case SDL_QUIT:
            browser_window_destroy(g->bw);
    }

}

void
fb_os_option_override(void)
{
}

void
fb_os_redraw(struct gui_window *g, struct bbox_s *box)
{
        SDL_UpdateRect(sdl_screen, box->x0, box->y0, box->x1, box->y1);
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */

