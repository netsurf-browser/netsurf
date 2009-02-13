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
#include <stdint.h>

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "utils/log.h"
#include "utils/utf8.h"
#include "desktop/plotters.h"
#include "desktop/browser.h"
#include "image/bitmap.h"

#include "framebuffer/fb_gui.h"
#include "framebuffer/fb_plotters.h"
#include "framebuffer/fb_bitmap.h"
#include "framebuffer/fb_cursor.h"
#include "framebuffer/fb_frontend.h"
#include "framebuffer/fb_rootwindow.h"

struct fb_cursor_s {
        int x;
        int y;
        int width; /**< width */
        int height; /**< height */

        bool plotted;

        struct bitmap *bitmap; /* pointer bitmap */

        uint8_t *savedata; /* save under area */
};

static const struct {
  unsigned int 	 width;
  unsigned int 	 height;
  unsigned int 	 bytes_per_pixel; /* 3:RGB, 4:RGBA */ 
  unsigned char	 pixel_data[11 * 15 * 4 + 1];
} pointer_image = {
  11, 15, 4,
  "\0\0\0'\0\0\0""8\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0`\230\230\230\275\3\3\3D\0\0\0\1\0\0\0\4\0\0\0\1\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0`\377\377\377\277\230\230\230"
  "\275\2\2\2N\0\0\0\37\0\0\0\23\0\0\0\2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0`\377\377\377\277\377\377\377\277\224\224\224\302\2\2\2`\0\0\0$\0\0\0\24"
  "\0\0\0\2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0`\377\377\377\277\377\377\377\277\370"
  "\370\370\304\220\220\220\307\2\2\2`\0\0\0$\0\0\0\24\0\0\0\2\0\0\0\0\0\0\0"
  "\0\0\0\0`\377\377\377\277\377\377\377\277\370\370\370\304\362\362\362\311"
  "\220\220\220\307\2\2\2`\0\0\0$\0\0\0\24\0\0\0\2\0\0\0\0\0\0\0`\377\377\377"
  "\277\377\377\377\277\370\370\370\304\362\362\362\311\361\361\361\311]]]\307"
  "\0\0\0;\0\0\0$\0\0\0\24\0\0\0\2\0\0\0`\377\377\377\277\377\377\377\277\370"
  "\370\370\304\304\304\304\311222\245\0\0\0Z\0\0\0'\0\0\0&\0\0\0$\0\0\0\24"
  "\0\0\0`\334\334\334\277sss\273\277\277\277\304\340\340\340\311\16\16\16~"
  "\0\0\0&\0\0\0&\0\0\0&\0\0\0&\0\0\0#\0\0\0-\5\5\5T\0\0\0\21]]]\261\362\362"
  "\362\311ccc\276\0\0\0,\0\0\0&\0\0\0&\0\0\0#\0\0\0\26\0\0\0\0\0\0\0\0\0\0"
  "\0\0\11\11\11h\333\333\333\311\306\306\306\311\0\0\0g\0\0\0&\0\0\0&\0\0\0"
  "\31\0\0\0\2\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\36ooo\304\212\212\212\305\15\15"
  "\15q\0\0\0&\0\0\0&\0\0\0!\0\0\0\6\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\0\0\0""8"
  "\0\0\0\35\0\0\0\25\0\0\0&\0\0\0&\0\0\0%\0\0\0\21\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\10\0\0\0#\0\0\0&\0\0\0%\0\0\0\24\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\0\0\0\24\0\0\0\34\0\0\0\22"
  "\0\0\0\4",
};

static void fb_cursor_save(framebuffer_t *fb)
{
        uint8_t *savebuf;
        int savelen;
        uint8_t *pvid;
        int yloop;
        int height = fb->cursor->height;

        if ((fb->height - fb->cursor->y) < height)
                height = fb->height - fb->cursor->y;

        if (height == 0) {
                if (fb->cursor->savedata != NULL)
                        free(fb->cursor->savedata);
                fb->cursor->savedata = NULL;
                return;
        }

        savelen = ((fb->cursor->width * fb->bpp) / 8);
        savebuf = malloc(savelen * height);
        if (savebuf == NULL)
                return;

        if (fb->cursor->savedata != NULL)
                free(fb->cursor->savedata);

        fb->cursor->savedata = savebuf;

        pvid = fb->ptr + 
               (fb->cursor->y  * fb->linelen) + 
               ((fb->cursor->x * fb->bpp) / 8);

        for (yloop=0; yloop < height; yloop++) {
                memcpy(savebuf, pvid, savelen);
                savebuf += savelen;
                pvid += fb->linelen;
        }
}

static void fb_cursor_clear(framebuffer_t *fb) 
{
        uint8_t *savebuf;
        int savelen;
        uint8_t *pvid;
        int yloop;
        int height = fb->cursor->height;
        bbox_t cursorbox;

        if ((fb->height - fb->cursor->y) < height)
                height = fb->height - fb->cursor->y ;

        if (height == 0)
                return;

        if (fb->cursor->plotted == false)
                return;
        fb->cursor->plotted = false;

        savebuf = fb->cursor->savedata;
        if (savebuf == NULL)
                return;

        savelen = ((fb->cursor->width * fb->bpp) / 8);

        pvid = fb->ptr + 
               (fb->cursor->y  * fb->linelen) + 
               ((fb->cursor->x * fb->bpp) / 8);

        for (yloop=0; yloop < height; yloop++) {
                memcpy(pvid, savebuf, savelen);
                savebuf += savelen;
                pvid += fb->linelen;
        }

        /* callback to the os specific routine in case it needs to do something
         * explicit to redraw 
         */
        cursorbox.x0 = fb->cursor->x;
        cursorbox.y0 = fb->cursor->y;
        cursorbox.x1 = fb->cursor->x + fb->cursor->width;
        cursorbox.y1 = fb->cursor->y + fb->cursor->height;
        fb_os_redraw(&cursorbox);

}

void
fb_cursor_move_abs(framebuffer_t *fb, int x, int y)
{
        fb_cursor_clear(fb);

        fb->cursor->x = x;
        fb->cursor->y = y;
        if (fb->cursor->x < 0)
                fb->cursor->x = 0;
        if (fb->cursor->y < 0)
                fb->cursor->y = 0;
        if (fb->cursor->x > fb->width)
                fb->cursor->x = fb->width;
        if (fb->cursor->y > fb->height)
                fb->cursor->y = fb->height;

}

void
fb_cursor_move(framebuffer_t *fb, int x, int y)
{
        fb_cursor_move_abs(fb, fb->cursor->x + x, fb->cursor->y + y);
}

void 
fb_cursor_plot(framebuffer_t *fb)
{
        bbox_t saved_plot_ctx;
        bbox_t cursorbox;

        /* if cursor is not currently plotted give up early */
        if (fb->cursor->plotted)
                return;

        /* enlarge the clipping rectangle to the whole screen for plotting the
         * cursor 
         */
        saved_plot_ctx = fb_plot_ctx;

        fb_plot_ctx.x0 = 0;
        fb_plot_ctx.y0 = 0;
        fb_plot_ctx.x1 = fb->width;
        fb_plot_ctx.y1 = fb->height;

        /* save under the cursor */
        fb_cursor_save(fb);

        /* plot the cursor image */
        plot.bitmap(fb->cursor->x, fb->cursor->y, 
                    fb->cursor->width, fb->cursor->height,
                    fb->cursor->bitmap, 0, NULL);

        /* callback to the os specific routine in case it needs to do something
         * explicit to redraw 
         */
        cursorbox.x0 = fb->cursor->x;
        cursorbox.y0 = fb->cursor->y;
        cursorbox.x1 = fb->cursor->x + fb->cursor->width;
        cursorbox.y1 = fb->cursor->y + fb->cursor->height;
        fb_os_redraw(&cursorbox);

        fb->cursor->plotted = true;

        /* restore clipping rectangle */
        fb_plot_ctx = saved_plot_ctx;
}


fb_cursor_t *
fb_cursor_init(framebuffer_t *fb)
{
        fb_cursor_t *cursor;
        cursor = calloc(1, sizeof(fb_cursor_t));

        cursor->x = fb->width / 2;
        cursor->y = fb->height / 2;

        cursor->width = pointer_image.width;
        cursor->height = pointer_image.height;
        cursor->bitmap = bitmap_create(cursor->width, cursor->height, 0);

        memcpy(cursor->bitmap->pixdata, 
               pointer_image.pixel_data, 
               pointer_image.width * 
               pointer_image.height * 
               pointer_image.bytes_per_pixel);

        cursor->plotted = false;

        return cursor;
}

int fb_cursor_x(framebuffer_t *fb)
{
        return fb->cursor->x;
}

int fb_cursor_y(framebuffer_t *fb)
{
        return fb->cursor->y;
}


/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
