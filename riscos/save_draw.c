/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "oslib/drawfile.h"
#include "oslib/jpeg.h"
#include "oslib/osfile.h"

#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/css/css.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/form.h"
#include "netsurf/render/layout.h"
#include "netsurf/riscos/save_draw.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_DRAW_EXPORT

/* in browser units = OS/2 = draw/512 */
#define A4PAGEWIDTH (744)
#define A4PAGEHEIGHT (1052)

static bool add_font_table(int **d, unsigned int *length,
		struct content *content);
static bool add_options(int **d, unsigned int *length);
static bool add_box(int **d, unsigned int *length, struct box *box,
		unsigned long cbc, long x, long y);
static bool add_graphic(int **d, unsigned int *length,
		struct content *content, struct box *box,
		unsigned long cbc, long x, long y);
static bool add_rect(int **d, unsigned int *length, struct box *box,
		unsigned long cbc, long x, long y, bool bg);
static bool add_line(int **d, unsigned int *length, struct box *box,
		unsigned long cbc, long x, long y);
static bool add_circle(int **d, unsigned int *length, struct box *box,
		unsigned long cbc, long x, long y);


/**
 * Export a content as a Drawfile.
 *
 * \param  c     content to export
 * \param  path  path to save Drawfile as
 * \return  true on success, false on error and error reported
 */

bool save_as_draw(struct content *c, char *path)
{
	struct box *box;
	int current_width;
	unsigned long bc;
	int *d;
	unsigned int length;
	drawfile_diagram *diagram;
	os_error *error;

	if (c->type != CONTENT_HTML) {
		return false;
	}

	box = c->data.html.layout->children;
	current_width = c->available_width;
	bc = 0xffffff;

	d = calloc(40, sizeof(char));
	if (!d) {
		warn_user("NoMemory", 0);
		return false;
	}

	length = 40;

	diagram = (drawfile_diagram *) d;
	memcpy(diagram->tag, "Draw", 4);
	diagram->major_version = 201;
	diagram->minor_version = 0;
	memcpy(diagram->source, "NetSurf     ", 12);

	/* recalculate box widths for an A4 page */
	if (!layout_document(box, A4PAGEWIDTH, c->data.html.box_pool))
		goto no_memory;

	diagram->bbox.x0 = 0;
	diagram->bbox.y0 = 0;
	diagram->bbox.x1 = A4PAGEWIDTH*512;
	diagram->bbox.y1 = A4PAGEHEIGHT*512;

	if (!add_font_table(&d, &length, c))
		goto no_memory;

	if (!add_options(&d, &length))
		goto no_memory;

	if (c->data.html.background_colour != TRANSPARENT) {
		bc = c->data.html.background_colour;
		if (!add_rect(&d, &length, box, bc<<8, 0,
				A4PAGEHEIGHT*512, true))
			goto no_memory;
	}

	/* right, traverse the tree and grab the contents */
	if (!add_box(&d, &length, box, bc, 0, A4PAGEHEIGHT*512))
		goto no_memory;

	error = xosfile_save_stamped(path, osfile_TYPE_DRAW, (char *) d,
			(char *) d + length);

	free(d);

	if (error) {
		LOG(("xosfile_save_stamped: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		/* attempt to reflow back on failure */
		layout_document(box, current_width, c->data.html.box_pool);
		return false;
	}

	/* reset layout to current window width */
	if (!layout_document(box, current_width, c->data.html.box_pool))
		warn_user("NoMemory", 0);

	return true;

no_memory:
	free(d);
	/* attempt to reflow back on failure */
	layout_document(box, current_width, c->data.html.box_pool);
	warn_user("NoMemory", 0);
	return false;
}


/**
 * add font table
 */
bool add_font_table(int **d, unsigned int *length,
		struct content *content)
{
	int *d2;
	unsigned int length0 = *length;
	unsigned int i;
	unsigned int padding;
	int handle = 0;
	int ftlen = 0;
	const char *name;
	drawfile_object *dro;
	drawfile_font_table *ft;

	d2 = realloc(*d, *length += 8);
	if (!d2)
		return false;
	*d = d2;
	dro = (drawfile_object *) (*d + length0 / sizeof *d);
	ft = &dro->data.font_table;

	dro->type = drawfile_TYPE_FONT_TABLE;

	do {
		name = enumerate_fonts(content->data.html.fonts, &handle);
		if (handle == -1 && name == 0)
			break;

		/* at this point, handle is always (font_table entry + 1) */
		d2 = realloc(*d, *length += 1 + strlen(name) + 1);
		if (!d2)
			return false;
		*d = d2;
		dro = (drawfile_object *) (*d + length0 / sizeof *d);
		ft = &dro->data.font_table;

		((char *) ft)[ftlen] = handle;
		strcpy(((char *) ft) + ftlen + 1, name);

		ftlen += 1 + strlen(name) + 1;
	} while (handle != -1);

	/* word align end of list */
	padding = (ftlen + 3) / 4 * 4 - ftlen;

	d2 = realloc(*d, *length + padding);
	if (!d2)
		return false;
	*d = d2;
	dro = (drawfile_object *) (*d + length0 / sizeof *d);
	ft = &dro->data.font_table;

	for (i = 0; i != padding; i++)
		((char *) *d)[*length + i] = 0;
	*length += padding;
	ftlen += padding;

	dro->size = 8 + ftlen;

	return true;
}


/**
 * add options object
 */
bool add_options(int **d, unsigned int *length)
{
	int *d2;
	unsigned int length0 = *length;
	drawfile_object *dro;
	drawfile_options *dfo;

	d2 = realloc(*d, *length += 8 + 80);
	if (!d2)
		return false;
	*d = d2;
	dro = (drawfile_object *) (*d + length0 / sizeof *d);
	dfo = &dro->data.options;

	dro->type = drawfile_TYPE_OPTIONS;
	dro->size = 8 + 80;

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

	return true;
}


/**
 * Traverses box tree, adding objects to the diagram as it goes.
 */
bool add_box(int **d, unsigned int *length, struct box *box,
		unsigned long cbc, long x, long y)
{
	int *d2;
	unsigned int length0 = *length;
	struct box *c;
	int width, height, colour;
	unsigned int i;
	drawfile_object *dro;
	drawfile_text *dt;

	x += box->x * 512;
	y -= box->y * 512;
	width = (box->padding[LEFT] + box->width + box->padding[RIGHT]) * 2;
	height = (box->padding[TOP] + box->height + box->padding[BOTTOM]) * 2;

	if (box->style && box->style->visibility == CSS_VISIBILITY_HIDDEN) {
		for (c = box->children; c; c = c->next) {
			if (!add_box(d, length, c, cbc, x, y))
				return false;
		}
		return true;
	}

	if (box->style != 0 && box->style->background_color != TRANSPARENT) {
		cbc = box->style->background_color;
		if (!add_rect(d, length, box, cbc<<8, x, y, false))
			return false;
	}

	if (box->object) {
		switch (box->object->type) {
			case CONTENT_JPEG:
#ifdef WITH_PNG
			case CONTENT_PNG:
#endif
			case CONTENT_GIF:
#ifdef WITH_SPRITE
			case CONTENT_SPRITE:
#endif
				return add_graphic(d, length, box->object,
						box, cbc, x, y);

			case CONTENT_HTML:
				c = box->object->data.html.layout->children;
				return add_box(d, length, c, cbc, x, y);

			default:
				break;
		}

	} else if (box->gadget && box->gadget->type == GADGET_CHECKBOX) {
		return add_rect(d, length, box, 0xDEDEDE00, x, y, false);

	} else if (box->gadget && box->gadget->type == GADGET_RADIO) {
		return add_circle(d, length, box, 0xDEDEDE00, x, y);

	} else if (box->text && box->font) {

		if (box->length == 0) {
			return true;
		}

		/* text-decoration */
		colour = box->style->color;
		colour = ((((colour >> 16) + (cbc >> 16)) / 2) << 16)
			| (((((colour >> 8) & 0xff) +
			     ((cbc >> 8) & 0xff)) / 2) << 8)
			| ((((colour & 0xff) + (cbc & 0xff)) / 2) << 0);
		if (box->style->text_decoration & CSS_TEXT_DECORATION_UNDERLINE || (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_UNDERLINE && box->parent->parent->type == BOX_BLOCK)) {
			if (!add_line(d, length, box, (unsigned)colour<<8,
					x, (int)(y+(box->height*0.1*512))))
				return false;
		}
		if (box->style->text_decoration & CSS_TEXT_DECORATION_OVERLINE || (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_OVERLINE && box->parent->parent->type == BOX_BLOCK)) {
			if (!add_line(d, length, box, (unsigned)colour<<8,
					x, (int)(y+(box->height*0.9*512))))
				return false;
		}
		if (box->style->text_decoration & CSS_TEXT_DECORATION_LINE_THROUGH || (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_LINE_THROUGH && box->parent->parent->type == BOX_BLOCK)) {
			if (!add_line(d, length, box, (unsigned)colour<<8,
					x, (int)(y+(box->height*0.4*512))))
				return false;
		}

		/* normal text */
		length0 = *length;
		d2 = realloc(*d, *length += 8 + 44 +
				(box->length + 1 + 3) / 4 * 4);
		if (!d2)
			return false;
		*d = d2;
		dro = (drawfile_object *) (*d + length0 / sizeof *d);
		dt = &dro->data.text;

		dro->type = drawfile_TYPE_TEXT;
		dro->size = 8 + 44 + (box->length + 1 + 3) / 4 * 4;

		dt->bbox.x0 = x;
		dt->bbox.y0 = y-(box->height*1.5*512);
		dt->bbox.x1 = x+(box->width*512);
		dt->bbox.y1 = y;
		dt->fill = box->style->color<<8;
		dt->bg_hint = cbc<<8;
		dt->style.font_index = box->font->id + 1;
		dt->style.reserved[0] = 0;
		dt->style.reserved[1] = 0;
		dt->style.reserved[2] = 0;
		dt->xsize = box->font->size*40;
		dt->ysize = box->font->size*40;
		dt->base.x = x;
		dt->base.y = y-(box->height*512)+1536;
		strncpy(dt->text, box->text, box->length);
		dt->text[box->length] = 0;
		for (i = box->length + 1; i % 4; i++)
			dt->text[i] = 0;

		return true;

	} else {
		for (c = box->children; c != 0; c = c->next) {
			if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
				if (!add_box(d, length, c, cbc, x, y))
					return false;
		}
		for (c = box->float_children; c !=  0; c = c->next_float) {
			if (!add_box(d, length, c, cbc, x, y))
				return false;
		}
	}

	return true;
}


/**
 * Add images to the drawfile. Uses add_jpeg as a helper.
 */
bool add_graphic(int **d, unsigned int *length,
		struct content *content, struct box *box,
		unsigned long cbc, long x, long y) {

	int *d2;
	unsigned int length0 = *length;
	int sprite_length = 0;
	drawfile_object *dro;
	drawfile_sprite *ds;

	/* cast-tastic... */
	switch (content->type) {
	  case CONTENT_JPEG:
	       sprite_length = ((osspriteop_header*)((char*)content->data.jpeg.sprite_area+content->data.jpeg.sprite_area->first))->size;
	       break;
#ifdef WITH_PNG
	  case CONTENT_PNG:
	       sprite_length = ((osspriteop_header*)((char*)content->data.png.sprite_area+content->data.png.sprite_area->first))->size;
	       break;
#endif
	  case CONTENT_GIF:
	       sprite_length = content->data.gif.gif->frame_image->size;
	       break;
#ifdef WITH_SPRITE
	  case CONTENT_SPRITE:
	       sprite_length = ((osspriteop_header*)((char*)content->data.sprite.data+(((osspriteop_area*)content->data.sprite.data)->first)))->size;
	       break;
#endif
	  default:
	       assert(0);
	}

	d2 = realloc(*d, *length += 8 + 16 + sprite_length);
	if (!d2)
		return false;
	*d = d2;
	dro = (drawfile_object *) (*d + length0 / sizeof *d);
	ds = &dro->data.sprite;

	dro->type = drawfile_TYPE_SPRITE;
	dro->size = 8 + 16 + sprite_length;

	ds->bbox.x0 = x;
	ds->bbox.y0 = y-((box->padding[TOP] + box->height + box->padding[BOTTOM])*512);
	ds->bbox.x1 = x+((box->padding[LEFT] + box->width + box->padding[RIGHT])*512);

	ds->bbox.y1 = y;

	switch (content->type) {
	  case CONTENT_JPEG:
	       memcpy((char*)ds+16, (char*)content->data.jpeg.sprite_area+content->data.jpeg.sprite_area->first,
		       (unsigned)sprite_length);
	       break;
#ifdef WITH_PNG
	  case CONTENT_PNG:
	       memcpy((char*)ds+16, (char*)content->data.png.sprite_area+content->data.png.sprite_area->first,
		       (unsigned)sprite_length);
	       break;
#endif
	  case CONTENT_GIF:
	       memcpy((char*)ds+16, (char*)content->data.gif.gif->frame_image,
		       (unsigned)sprite_length);
	       break;
#ifdef WITH_SPRITE
	  case CONTENT_SPRITE:
	       memcpy((char*)ds+16, (char*)content->data.sprite.data+((osspriteop_area*)content->data.sprite.data)->first,
		       (unsigned)sprite_length);
	       break;
#endif
	  default:
	       assert(0);
	}

	return true;
}


/**
 * Add a filled, borderless rectangle to the diagram
 * Set bg to true to produce the background rectangle.
 */
bool add_rect(int **d, unsigned int *length, struct box *box,
		unsigned long cbc, long x, long y, bool bg) {

	int *d2;
	unsigned int length0 = *length;
	drawfile_object *dro;
	drawfile_path *dp;
	draw_path_element *dpe;

	d2 = realloc(*d, *length += 8 + 96);
	if (!d2)
		return false;
	*d = d2;
	dro = (drawfile_object *) (*d + length0 / sizeof *d);
	dp = &dro->data.path;

	dro->type = drawfile_TYPE_PATH;
	dro->size = 8 + 96;

	if (bg) {
		dp->bbox.x0 = 0;
		dp->bbox.y0 = 0;
		dp->bbox.x1 = A4PAGEWIDTH*512;
		dp->bbox.y1 = A4PAGEHEIGHT*512;
	} else {
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
	dpe = (draw_path_element *) (((int *) &dp->path) + 0 / sizeof (int));
	dpe->tag = draw_MOVE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.move_to.x = dp->bbox.x0;
	dpe->data.move_to.y = dp->bbox.y0;

	/* bottom right */
	dpe = (draw_path_element *) (((int *) &dp->path) + 12 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = dp->bbox.x1;
	dpe->data.line_to.y = dp->bbox.y0;

	/* top right */
	dpe = (draw_path_element *) (((int *) &dp->path) + 24 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = dp->bbox.x1;
	dpe->data.line_to.y = dp->bbox.y1;

	/* top left */
	dpe = (draw_path_element *) (((int *) &dp->path) + 36 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = dp->bbox.x0;
	dpe->data.line_to.y = dp->bbox.y1;

	/* bottom left */
	dpe = (draw_path_element *) (((int *) &dp->path) + 48 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = dp->bbox.x0;
	dpe->data.line_to.y = dp->bbox.y0;

	/* end */
	dpe = (draw_path_element *) (((int *) &dp->path) + 60 / sizeof (int));
	dpe->tag = draw_END_PATH;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;

	return true;
}


/**
 * add a line to the diagram
 */
bool add_line(int **d, unsigned int *length, struct box *box,
		unsigned long cbc, long x, long y) {

	int *d2;
	unsigned int length0 = *length;
	drawfile_object *dro;
	drawfile_path *dp;
	draw_path_element *dpe;

	d2 = realloc(*d, *length += 8 + 60);
	if (!d2)
		return false;
	*d = d2;
	dro = (drawfile_object *) (*d + length0 / sizeof *d);
	dp = &dro->data.path;

	dro->type = drawfile_TYPE_PATH;
	dro->size = 8 + 60;

	dp->bbox.x0 = x;
	dp->bbox.y0 = y-((box->padding[TOP] + box->height + box->padding[BOTTOM])*512);
	dp->bbox.x1 = x+((box->padding[LEFT] + box->width + box->padding[RIGHT])*512);
	dp->bbox.y1 = y;

	dp->fill = cbc;
	dp->outline = cbc;
	dp->width = 0;
	dp->style.flags = 0;
	dp->style.reserved = 0;
	dp->style.cap_width = 0;
	dp->style.cap_length = 0;

	/* left end */
	dpe = (draw_path_element *) (((int *) &dp->path) + 0 / sizeof (int));
	dpe->tag = draw_MOVE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.move_to.x = dp->bbox.x0;
	dpe->data.move_to.y = dp->bbox.y0;

	/* right end */
	dpe = (draw_path_element *) (((int *) &dp->path) + 12 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = dp->bbox.x1;
	dpe->data.line_to.y = dp->bbox.y0;

	/* end */
	dpe = (draw_path_element *) (((int *) &dp->path) + 24 / sizeof (int));
	dpe->tag = draw_END_PATH;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;

	return true;
}


/**
 * add a circle to the diagram.
 */
bool add_circle(int **d, unsigned int *length, struct box *box,
		unsigned long cbc, long x, long y) {

	int *d2;
	unsigned int length0 = *length;
	double radius = 0, kappa;
	double cx, cy;
	drawfile_object *dro;
	drawfile_path *dp;
	draw_path_element *dpe;

	d2 = realloc(*d, *length += 8 + 160);
	if (!d2)
		return false;
	*d = d2;
	dro = (drawfile_object *) (*d + length0 / sizeof *d);
	dp = &dro->data.path;

	dro->type = drawfile_TYPE_PATH;
	dro->size = 8 + 160;

	dp->bbox.x0 = x;
	dp->bbox.y0 = y-((box->padding[TOP] + box->height + box->padding[BOTTOM])*512);
	dp->bbox.x1 = x+((box->padding[LEFT] + box->width + box->padding[RIGHT])*512);
	dp->bbox.y1 = y;

	cx = ((dp->bbox.x1-dp->bbox.x0)/2.0);
	cy = ((dp->bbox.y1-dp->bbox.y0)/2.0);
	if (cx == cy) {
		radius = cx; /* box is square */
	}
	else if (cx > cy) {
		radius = cy;
		dp->bbox.x1 -= (cx-cy); /* reduce box width */
	}
	else if (cy > cx) {
		radius = cx;
		dp->bbox.y0 += (cy-cx); /* reduce box height */
	}
	kappa = radius * ((4.0/3.0)*(sqrt(2.0)-1.0)); /* ~= 0.5522847498 */

	dp->fill = cbc;
	dp->outline = cbc;
	dp->width = 0;
	dp->style.flags = drawfile_PATH_ROUND;

	/*
	 *    Z	  b   Y
	 *
	 *    a	  X   c
	 *
	 *    V	  d   W
	 *
	 *    V = (x0,y0)
	 *    W = (x1,y0)
	 *    Y = (x1,y1)
	 *    Z = (x0,y1)
	 *
	 *    X = centre of circle (x0+cx, y0+cx)
	 *
	 *    The points a,b,c,d are where the circle intersects
	 *    the bounding box. at these points, the bounding box is
	 *    tangental to the circle.
	 */

	/* start at a */
	dpe = (draw_path_element *) (((int *) &dp->path) + 0 / sizeof (int));
	dpe->tag = draw_MOVE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.move_to.x = (dp->bbox.x0+cx)-radius;
	dpe->data.move_to.y = (dp->bbox.y0+cy);

	/* point1->point2 : (point1)(ctrl1)(ctrl2)(point2) */

	/* a->b : (x-r, y)(x-r, y+k)(x-k, y+r)(x, y+r) */
	dpe = (draw_path_element *) (((int *) &dp->path) + 12 / sizeof (int));
	dpe->tag = draw_BEZIER_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.bezier_to[0].x = (dp->bbox.x0+cx)-radius;
	dpe->data.bezier_to[0].y = (dp->bbox.y0+cy)+kappa;
	dpe->data.bezier_to[1].x = (dp->bbox.x0+cx)-kappa;
	dpe->data.bezier_to[1].y = (dp->bbox.y0+cy)+radius;
	dpe->data.bezier_to[2].x = (dp->bbox.x0+cx);
	dpe->data.bezier_to[2].y = (dp->bbox.y0+cy)+radius;

	/* b->c : (x, y+r)(x+k, y+r)(x+r, y+k)(x+r, y)*/
	dpe = (draw_path_element *) (((int *) &dp->path) + 40 / sizeof (int));
	dpe->tag = draw_BEZIER_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.bezier_to[0].x = (dp->bbox.x0+cx)+kappa;
	dpe->data.bezier_to[0].y = (dp->bbox.y0+cy)+radius;
	dpe->data.bezier_to[1].x = (dp->bbox.x0+cx)+radius;
	dpe->data.bezier_to[1].y = (dp->bbox.y0+cy)+kappa;
	dpe->data.bezier_to[2].x = (dp->bbox.x0+cx)+radius;
	dpe->data.bezier_to[2].y = (dp->bbox.y0+cy);

	/* c->d : (x+r, y)(x+r, y-k)(x+k, y-r)(x, y-r) */
	dpe = (draw_path_element *) (((int *) &dp->path) + 68 / sizeof (int));
	dpe->tag = draw_BEZIER_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.bezier_to[0].x = (dp->bbox.x0+cx)+radius;
	dpe->data.bezier_to[0].y = (dp->bbox.y0+cy)-kappa;
	dpe->data.bezier_to[1].x = (dp->bbox.x0+cx)+kappa;
	dpe->data.bezier_to[1].y = (dp->bbox.y0+cy)-radius;
	dpe->data.bezier_to[2].x = (dp->bbox.x0+cx);
	dpe->data.bezier_to[2].y = (dp->bbox.y0+cy)-radius;

	/* d->a : (x, y-r)(x-k, y-r)(x-r, y-k)(x-r, y)*/
	dpe = (draw_path_element *) (((int *) &dp->path) + 96 / sizeof (int));
	dpe->tag = draw_BEZIER_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.bezier_to[0].x = (dp->bbox.x0+cx)-kappa;
	dpe->data.bezier_to[0].y = (dp->bbox.y0+cy)-radius;
	dpe->data.bezier_to[1].x = (dp->bbox.x0+cx)-radius;
	dpe->data.bezier_to[1].y = (dp->bbox.y0+cy)-kappa;
	dpe->data.bezier_to[2].x = (dp->bbox.x0+cx)-radius;
	dpe->data.bezier_to[2].y = (dp->bbox.y0+cy);

	/* end */
	dpe = (draw_path_element *) (((int *) &dp->path) + 124 / sizeof (int));
	dpe->tag = draw_END_PATH;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;

	return true;
}

#endif
