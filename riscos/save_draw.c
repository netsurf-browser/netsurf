/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <stdbool.h>
#include <string.h>

#include "oslib/drawfile.h"
#include "oslib/jpeg.h"
#include "oslib/osfile.h"

#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/render/form.h"
#include "netsurf/render/layout.h"
#include "netsurf/riscos/save_draw.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

/* TODO - fix fonts (see below)
 *        fix text issues
 *        fix jpegs
 *        fix duplicate image issue (qv www.iconbar.com)
 *        GUI
 */

#ifdef WITH_DRAW_EXPORT

/* in browser units = OS/2 = draw/512 */
#define A4PAGEWIDTH (744)
#define A4PAGEHEIGHT (1052)
static unsigned long length;
static drawfile_diagram *d;

static void add_font_table(void);
static void add_options(void);
static void add_objects(struct content *content, struct box *box,
                        unsigned long cbc, long x, long y);
static void add_graphic(struct content *content, struct box *box,
                        unsigned long cbc, long x, long y);
static void add_jpeg(struct content *content, struct box *box,
                        unsigned long cbc, long x, long y);
static void add_rect(struct content *content, struct box *box,
                        unsigned long cbc, long x, long y, bool bg);

/* TODO - this will probably want to take a filename/path too... */
void save_as_draw(struct content *c) {

	struct box *box = c->data.html.layout->children;
	int temp = c->width;
	unsigned long bc = 0xffffff;

	d = xcalloc(40, sizeof(char));

        length = 40;

        memcpy((char*)&d->tag, "Draw", 4);
	d->major_version = 201;
	d->minor_version = 0;
	memcpy((char*)&d->source, "NetSurf     ", 12);

	/* recalculate box widths for an A4 page */
	layout_document(box, A4PAGEWIDTH);

	d->bbox.x0 = 0;
	d->bbox.y0 = 0;
	d->bbox.x1 = A4PAGEWIDTH*512;
	d->bbox.y1 = A4PAGEHEIGHT*512;

	add_font_table();

	add_options();

	if (c->data.html.background_colour != TRANSPARENT) {
		bc = c->data.html.background_colour;
		add_rect(c, box, bc<<8, 0, A4PAGEHEIGHT*512, true);
	}

	/* right, traverse the tree and grab the contents */
	add_objects(c, box, bc, 0, A4PAGEHEIGHT*512);

	xosfile_save_stamped("<NetSurf$Dir>.draw", 0xaff, (byte*)d, (byte*)d+length);

	xfree(d);

        /* reset layout to current window width */
	layout_document(box, temp);
}

/**
 * add font table
 * TODO - add all fonts required. for now we just use Homerton Medium
 */
void add_font_table() {

        drawfile_object *dro = xcalloc(8+28, sizeof(char));
        drawfile_font_table *ft = xcalloc(28, sizeof(char));
        drawfile_FONT_DEF(40) fd[] = {
              { 1, "Homerton.Medium\\ELatin1" },
              /*{ 2, "Homerton.Medium.Oblique\\ELatin1" },
              { 3, "Homerton.Bold\\ELatin1" },
              { 4, "Homerton.Bold.Oblique\\ELatin1" },*/
        };

        memcpy(ft->font_def, (char*)&fd, 28);

	dro->type = drawfile_TYPE_FONT_TABLE;
	dro->size = 8+28;
	memcpy((char*)&dro->data.font_table, ft, 28);

        d = xrealloc(d, (unsigned)length+dro->size);

        memcpy((char*)&d->objects, dro, (unsigned)dro->size);

	length += 8+28;

	xfree(ft);
	xfree(dro);
}

/**
 * add options object
 */
void add_options() {

        drawfile_object *dro = xcalloc(8+80, sizeof(char));
        drawfile_options *dfo = xcalloc(80, sizeof(char));

        dfo->bbox.x0 = dfo->bbox.y0 = dfo->bbox.x1 = dfo->bbox.y1 = 0;
        dfo->paper_size = 0x500; /* A4 */
        dfo->paper_options = (drawfile_paper_options)0;
        dfo->grid_spacing = 1;
        dfo->grid_division = 2;
        dfo->isometric = false;
        dfo->auto_adjust = false;
        dfo->show = false;
        dfo->lock = false;
        dfo->cm = true;
        dfo->zoom_mul = 1;
        dfo->zoom_div = 1;
        dfo->zoom_lock = false;
        dfo->toolbox = true;
        dfo->entry_mode = drawfile_ENTRY_MODE_SELECT;
        dfo->undo_size = 5000;

        dro->type = drawfile_TYPE_OPTIONS;
        dro->size = 8+80;
        memcpy((char*)&dro->data.options, dfo,(unsigned)dro->size-8);

        d = xrealloc(d, length+dro->size);
        memcpy((char*)d+length, dro, (unsigned)dro->size);

        length += dro->size;

        xfree(dfo);
        xfree(dro);
}

/**
 * Traverses box tree, adding objects to the diagram as it goes.
 */
void add_objects(struct content *content, struct box *box,
                 unsigned long cbc, long x, long y) {

	struct box *c;
	int width, height, colour;

	x += box->x * 512;
	y -= box->y * 512;
	width = (box->padding[LEFT] + box->width + box->padding[RIGHT]) * 2;
	height = (box->padding[TOP] + box->height + box->padding[BOTTOM]) * 2;

	if (box->style->visibility == CSS_VISIBILITY_HIDDEN) {
		for (c = box->children; c; c = c->next)
			add_objects(content, c, cbc, x, y);
		return;
	}

	if (box->style != 0 && box->style->background_color != TRANSPARENT) {
		cbc = box->style->background_color;
		add_rect(content, box, cbc<<8, x, y, false);
	}

	if (box->object) {
	        if (box->object->type == CONTENT_PLUGIN    ||
	            box->object->type == CONTENT_OTHER     ||
	            box->object->type == CONTENT_UNKNOWN   ||
	            box->object->type == CONTENT_HTML      ||
	            box->object->type == CONTENT_TEXTPLAIN ||
	            box->object->type == CONTENT_CSS       ||
	            box->object->type == CONTENT_DRAW) {
		        return; /* don't handle these */
		}
		else {
		  add_graphic(box->object, box, cbc, x, y);
		  return;
		}
	}
	else if (box->gadget && (box->gadget->type == GADGET_CHECKBOX ||
		 box->gadget->type == GADGET_RADIO)) {
		return; /* nor these... */
	}
	else if (box->text && box->font) {
		colour = box->style->color;
		colour = ((((colour >> 16) + (cbc >> 16)) / 2) << 16)
			| (((((colour >> 8) & 0xff) +
			     ((cbc >> 8) & 0xff)) / 2) << 8)
			| ((((colour & 0xff) + (cbc & 0xff)) / 2) << 0);
		/* ignore text decorations for now */
		{
			drawfile_object *dro = xcalloc(8+44+((strlen(box->text)+1+3)/4*4), sizeof(char));
			drawfile_text *dt = xcalloc(44+((strlen(box->text)+1+3)/4*4), sizeof(char));

			dt->bbox.x0 = x;
			dt->bbox.y0 = y-(box->height*1.5*512);
			dt->bbox.x1 = x+(box->width*512);
			dt->bbox.y1 = y;
			dt->fill = colour<<8;
			dt->bg_hint = cbc<<8;
			dt->style.font_index = 1;
			dt->xsize = box->font->size*40;
			dt->ysize = box->font->size*40;
			dt->base.x = x;
			dt->base.y = y-(box->height*512)+1536;
			memcpy(dt->text, box->text, strlen(box->text));

			dro->type = drawfile_TYPE_TEXT;
			dro->size = ((strlen(box->text)+1+3)/4*4) + 44 + 8;
			memcpy((char*)&dro->data.text, dt, (unsigned)dro->size-8);
			d = xrealloc(d, (unsigned)length + dro->size);
			memcpy((char*)d+length, dro, (unsigned)dro->size);
			length += dro->size;

			xfree(dt);
			xfree(dro);
			return;
		}
	}
	else {
		for (c = box->children; c != 0; c = c->next) {
			add_objects(content, c, cbc, x, y);
		}
		for (c = box->float_children; c !=  0; c = c->next_float) {
			add_objects(content, c, cbc, x, y);
		}
	}
}

/**
 * Add images to the drawfile. Uses add_jpeg as a helper.
 */
void add_graphic(struct content *content, struct box *box,
                        unsigned long cbc, long x, long y) {

        drawfile_object *dro;
        drawfile_sprite *ds;
        long sprite_length = 0;

        switch (content->type) {
          case CONTENT_JPEG:
               if (content->data.jpeg.use_module) {
                 sprite_length = -1;
               }
               else {
                 sprite_length = content->data.jpeg.sprite_area->size;
               }
               break;
          case CONTENT_PNG:
               sprite_length = content->data.png.sprite_area->size;
               break;
          case CONTENT_GIF:
               sprite_length = content->data.gif.sprite_area->size;
               break;
          case CONTENT_SPRITE:
               sprite_length = content->data.sprite.length-16;
               break;
          default:
               break;
        }

        if (sprite_length == -1 && content->type == CONTENT_JPEG) {
          add_jpeg(content, box, cbc, x, y);
          return;
        }

        dro = xcalloc((unsigned)8 + 16 + sprite_length, sizeof(char));
        ds = xcalloc((unsigned)16 + sprite_length, sizeof(char));

        ds->bbox.x0 = x;
        ds->bbox.y0 = y-((box->padding[TOP] + box->height + box->padding[BOTTOM])*512);
        ds->bbox.x1 = x+((box->padding[LEFT] + box->width + box->padding[RIGHT])*512);

        ds->bbox.y1 = y;

        switch (content->type) {
          case CONTENT_JPEG:
               memcpy((char*)ds+16, content->data.jpeg.sprite_area+1,
                       (unsigned)sprite_length);
               break;
          case CONTENT_PNG:
               memcpy((char*)ds+16, content->data.png.sprite_area+1,
                       (unsigned)sprite_length);
               break;
          case CONTENT_GIF:
               memcpy((char*)ds+16, content->data.gif.sprite_area+1,
                       (unsigned)sprite_length);
               break;
          case CONTENT_SPRITE:
               memcpy((char*)ds+16, (char*)content->data.sprite.data+16,
                       (unsigned)sprite_length);
               break;
          default:
               break;
        }

        dro->type = drawfile_TYPE_SPRITE;
        dro->size = 8 + 16 + sprite_length;
        memcpy((char*)&dro->data.sprite, ds, (unsigned)16 + sprite_length);

        d = xrealloc(d, length+dro->size);
        memcpy((char*)d+length, dro, (unsigned)dro->size);

        length += dro->size;

        xfree(ds);
        xfree(dro);
}

/**
 * Add jpeg objects which the OS can cope with.
 * Jpegs the OS doesn't understand are added as sprites
 * This may still be a little buggy.
 */
void add_jpeg(struct content *content, struct box *box,
                        unsigned long cbc, long x, long y) {

        drawfile_object *dro = xcalloc(8+60+((content->data.jpeg.length+3)/4*4), sizeof(char));
        drawfile_jpeg *dj = xcalloc(60+((content->data.jpeg.length+3)/4*4), sizeof(char));

        dj->bbox.x0 = x;
        dj->bbox.y0 = y-((box->padding[TOP] + box->height + box->padding[BOTTOM])*512);
        dj->bbox.x1 = x+((box->padding[LEFT] + box->width + box->padding[RIGHT])*512);
        dj->bbox.y1 = y;

        xjpeginfo_dimensions((jpeg_image const*)content->data.jpeg.data,
                              (int)content->data.jpeg.length,
                              0, &dj->width, &dj->height,
                              &dj->xdpi, &dj->ydpi, 0);
        dj->width *= 512;
        dj->height *= 512;
        dj->trfm.entries[0][0] = 1 << 16;
        dj->trfm.entries[0][1] = 0;
        dj->trfm.entries[1][0] = 0;
        dj->trfm.entries[1][1] = 1 << 16;
        dj->trfm.entries[2][0] = x;
        dj->trfm.entries[2][1] = dj->bbox.y0;
        dj->len = content->data.jpeg.length;
        memcpy((char*)&dj->image, content->data.jpeg.data, (unsigned)dj->len);

        dro->type = drawfile_TYPE_JPEG;
        dro->size = 8 + 60 + ((dj->len+3)/4*4);
        memcpy((char*)&dro->data.jpeg, dj, (unsigned)dro->size-8);

        d = xrealloc(d, length+dro->size);
        memcpy((char*)d+length, dro, (unsigned)dro->size);

        length += dro->size;

        xfree(dj);
        xfree(dro);
}

/**
 * Add a filled, borderless rectangle to the diagram
 * Set bg to true to produce the background rectangle.
 */
void add_rect(struct content *content, struct box *box,
              unsigned long cbc, long x, long y, bool bg) {

        drawfile_object *dro = xcalloc(8+96, sizeof(char));
        drawfile_path *dp = xcalloc(96, sizeof(char));
        draw_path_element *dpe = xcalloc(12, sizeof(char));

        if (bg) {
                dp->bbox.x0 = 0;
                dp->bbox.y0 = 0;
                dp->bbox.x1 = A4PAGEWIDTH*512;
                dp->bbox.y1 = A4PAGEHEIGHT*512;
        }
        else {
                dp->bbox.x0 = x;
                dp->bbox.y0 = y-((box->padding[TOP] + box->height + box->padding[BOTTOM])*512);
                dp->bbox.x1 = x+((box->padding[LEFT] + box->width + box->padding[RIGHT])*512);
                dp->bbox.y1 = y;
        }

        dp->fill = cbc;
        dp->outline = cbc;
        dp->width = 0;
        dp->style.flags = 0;

        /**
         *     X<------X
         *     |       ^
         *     |       |
         *     v       |
         *  -->X------>X
         */

        /* bottom left */
        dpe->tag = draw_MOVE_TO;
        dpe->data.move_to.x = dp->bbox.x0;
        dpe->data.move_to.y = dp->bbox.y0;
        memcpy((char*)&dp->path, dpe, 12);
        /* bottom right */
        dpe->tag = draw_LINE_TO;
        dpe->data.line_to.x = dp->bbox.x1;
        dpe->data.line_to.y = dp->bbox.y0;
        memcpy((char*)&dp->path+12, dpe, 12);
        /* top right */
        dpe->tag = draw_LINE_TO;
        dpe->data.line_to.x = dp->bbox.x1;
        dpe->data.line_to.y = dp->bbox.y1;
        memcpy((char*)&dp->path+24, dpe, 12);
        /* top left */
        dpe->tag = draw_LINE_TO;
        dpe->data.line_to.x = dp->bbox.x0;
        dpe->data.line_to.y = dp->bbox.y1;
        memcpy((char*)&dp->path+36, dpe, 12);
        /* bottom left */
        dpe->tag = draw_LINE_TO;
        dpe->data.line_to.x = dp->bbox.x0;
        dpe->data.line_to.y = dp->bbox.y0;
        memcpy((char*)&dp->path+48, dpe, 12);
        /* end */
        dpe->tag = draw_END_PATH;
        memcpy((char*)&dp->path+60, dpe, 4);

        dro->type = drawfile_TYPE_PATH;
        dro->size = 8+96;
        memcpy((char*)&dro->data.path, dp, (unsigned)dro->size-8);

        d = xrealloc(d, length+dro->size);
        memcpy((char*)d+length, dro, (unsigned)dro->size);

        length += dro->size;

        xfree(dpe);
        xfree(dp);
        xfree(dro);
}
#endif
