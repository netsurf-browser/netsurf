/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "oslib/drawfile.h"
#include "oslib/jpeg.h"
#include "oslib/osgbpb.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"

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

/* Must be a power of 2 */
#define DRAWBUF_INITIAL_SIZE (1<<14)

typedef enum {
	DrawBuf_eHeader,
	DrawBuf_eFontTable,
	DrawBuf_eBody
} drawbuf_type_e;

typedef struct {
	byte *bufP;
	size_t currentSize;
	size_t maxSize;
} drawbuf_part_t;

typedef struct {
	drawbuf_part_t header;
	drawbuf_part_t fontTable;
	drawbuf_part_t body;
	void **fontNamesP; /**< 256 element malloc() pointer array */
	size_t numFonts;
} drawbuf_t;

static byte *drawbuf_claim(size_t size, drawbuf_type_e type);
static void drawbuf_free(void);
static bool drawbuf_add_font(const char *fontNameP, byte *fontIndex);
static bool drawbuf_save_file(const char *drawfilename);

static bool add_options(void);
static bool add_box(struct box *box, unsigned long cbc, long x, long y);
static bool add_graphic(struct content *content, struct box *box,
		unsigned long cbc, long x, long y);
static bool add_rect(struct box *box,
		unsigned long cbc, long x, long y, bool bg);
static bool add_line(struct box *box, unsigned long cbc, long x, long y);
static bool add_circle(struct box *box, unsigned long cbc, long x, long y);
static bool add_text(struct box *box, unsigned long cbc, long x, long y);


static drawbuf_t oDrawBuf; /* static -> complete struct inited to 0 */

/**
 * Export a content as a Drawfile.
 *
 * \param  c     content to export
 * \param  path  path to save Drawfile as
 * \return  true on success, false on error and error reported
 */

bool save_as_draw(struct content *c, const char *path)
{
	struct box *box;
	int current_width;
	unsigned long bc;
	drawfile_diagram_base *diagram;

	if (c->type != CONTENT_HTML)
		return false;

	box = c->data.html.layout->children;
	current_width = c->available_width;

	if ((diagram = drawbuf_claim(sizeof(drawfile_diagram_base), DrawBuf_eHeader)) == NULL)
		goto draw_save_error;

	/* write the Draw diagram */
	memcpy(diagram->tag, "Draw", 4);
	diagram->major_version = 201;
	diagram->minor_version = 0;
	memcpy(diagram->source, "NetSurf     ", 12);

	/* recalculate box widths for an A4 page */
	if (!layout_document(box, A4PAGEWIDTH, c->data.html.box_pool)) {
		warn_user("NoMemory", 0);
		goto draw_save_error;
	}

	diagram->bbox.x0 = 0;
	diagram->bbox.y0 = 0;
	diagram->bbox.x1 = A4PAGEWIDTH*512;
	diagram->bbox.y1 = A4PAGEHEIGHT*512;

	if (!add_options())
		goto draw_save_error;

	bc = 0xffffff;
	if (c->data.html.background_colour != TRANSPARENT) {
		bc = c->data.html.background_colour;
		if (!add_rect(box, bc<<8, 0, A4PAGEHEIGHT*512, true))
			goto draw_save_error;
	}

	/* right, traverse the tree and grab the contents */
	if (!add_box(box, bc, 0, A4PAGEHEIGHT*512))
		goto draw_save_error;

	if (!drawbuf_save_file(path))
		goto draw_save_error;

	drawbuf_free();

	/* reset layout to current window width */
	if (!layout_document(box, current_width, c->data.html.box_pool)) {
		warn_user("NoMemory", 0);
		return false;
	}

	return true;

draw_save_error:
	drawbuf_free();
	/* attempt to reflow back on failure */
	(void)layout_document(box, current_width, c->data.html.box_pool);
	return false;
}


/**
 * Claim size number of bytes available in that
 * particular buffer.
 *
 * \param size number of bytes to claim
 * \param type defines which Draw buffer needs its size to be ensured
 * \return non NULL when buffer size got correctly claimed, NULL on failure
 */
static byte *drawbuf_claim(size_t size, drawbuf_type_e type)
{
	drawbuf_part_t *drawBufPartP;

	switch (type) {
		case DrawBuf_eHeader:
			drawBufPartP = &oDrawBuf.header;
			break;
		case DrawBuf_eFontTable:
			drawBufPartP = &oDrawBuf.fontTable;
			break;
		case DrawBuf_eBody:
			drawBufPartP = &oDrawBuf.body;
			break;
		default:
			assert(0);
	}

	if (drawBufPartP->bufP == NULL) {
		const size_t sizeNeeded = (size > DRAWBUF_INITIAL_SIZE) ? size : DRAWBUF_INITIAL_SIZE;
		if ((drawBufPartP->bufP = malloc(sizeNeeded)) == NULL) {
			warn_user("NoMemory", 0);
			return NULL;
		}
		drawBufPartP->currentSize = size;
		drawBufPartP->maxSize = sizeNeeded;
	} else if (drawBufPartP->maxSize < drawBufPartP->currentSize + size) {
		size_t sizeNeeded = drawBufPartP->maxSize;
		while ((sizeNeeded *= 2) < drawBufPartP->currentSize + size)
			;
		if ((drawBufPartP->bufP = realloc(drawBufPartP->bufP, sizeNeeded)) == NULL) {
			warn_user("NoMemory", 0);
			return NULL;
		}
		drawBufPartP->currentSize += size;
		drawBufPartP->maxSize = sizeNeeded;
	} else {
		drawBufPartP->currentSize += size;
	}

	return drawBufPartP->bufP + drawBufPartP->currentSize - size;
}


/**
 * Frees all the Draw buffers.
 */
static void drawbuf_free(void)
{
	free(oDrawBuf.header.bufP); oDrawBuf.header.bufP = NULL;
	free(oDrawBuf.fontTable.bufP); oDrawBuf.fontTable.bufP = NULL;
	free(oDrawBuf.body.bufP); oDrawBuf.body.bufP = NULL;
	if (oDrawBuf.fontNamesP != NULL) {
		while (oDrawBuf.numFonts > 0)
			free(oDrawBuf.fontNamesP[--oDrawBuf.numFonts]);
		free(oDrawBuf.fontNamesP); oDrawBuf.fontNamesP = NULL;
	}
	oDrawBuf.numFonts = 0;
}


/**
 * Return a font index for given RISC OS font name.
 *
 * \param fontNameP NUL terminated RISC OS font name.
 * \param fontIndex Returned font index 0 - 255 for given font name.
 * \return  true on success, false on error and error reported
 */
static bool drawbuf_add_font(const char *fontNameP, byte *fontIndex)
{
	size_t index;

	for (index = 0; index < oDrawBuf.numFonts; ++index) {
		if (!strcmp(oDrawBuf.fontNamesP[index], fontNameP)) {
			*fontIndex = (byte)index + 1;
			return true;
		}
	}

	/* Only max 255 RISC OS outline fonts can be stored in a Draw
	 * file.
	 */
	if (oDrawBuf.numFonts == 255)
		return false; /** \todo: report GUI error */

	if (oDrawBuf.fontNamesP == NULL
	    && ((oDrawBuf.fontNamesP = malloc(255 * sizeof(void *))) == NULL)) {
		warn_user("NoMemory", 0);
		return false;
	}

	*fontIndex = (byte)oDrawBuf.numFonts + 1;
	if ((oDrawBuf.fontNamesP[oDrawBuf.numFonts++] = strdup(fontNameP)) == NULL)
		return false;

	return true;
}


/**
 * Save the Draw file from memory to disk.
 *
 * \param drawfilename RISC OS filename where to save the Draw file.
 * \return  true on success, false on error and error reported
 */
static bool drawbuf_save_file(const char *drawfilename)
{
	size_t index;
	os_fw *handle = NULL;
	os_error *error = NULL;

	/* create font table (if needed). */
	if (oDrawBuf.numFonts > 0) {
		drawfile_object *dro;

		if ((dro = drawbuf_claim(8, DrawBuf_eFontTable)) == NULL)
			goto file_save_error;

		dro->type = drawfile_TYPE_FONT_TABLE;
		/* we can't write dro->size yet. */

		for (index = 0; index < oDrawBuf.numFonts; ++index) {
			const char *fontNameP = oDrawBuf.fontNamesP[index];
			size_t len = 1 + strlen(fontNameP) + 1;
			byte *bufP;

			if ((bufP = drawbuf_claim(len, DrawBuf_eFontTable)) == NULL)
				goto file_save_error;
			*bufP++ = (byte)index + 1;
			memcpy(bufP, fontNameP, len + 1);
		}
		/* align to next word boundary */
		if (oDrawBuf.fontTable.currentSize % 4) {
			size_t wordpad = 4 - (oDrawBuf.fontTable.currentSize & 3);
			byte *bufP;

			if ((bufP = drawbuf_claim(wordpad, DrawBuf_eFontTable)) == NULL)
				goto file_save_error;
			memset(bufP, '\0', wordpad);
		}

		/* note that at the point it can be that
		 * dro != oDrawBuf.fontTable.bufP
		 */
		((drawfile_object *)oDrawBuf.fontTable.bufP)->size = oDrawBuf.fontTable.currentSize;
	}

	if ((error = xosfind_openoutw(osfind_NO_PATH, drawfilename, NULL, &handle)) != NULL)
		goto file_save_error;

	/* write Draw header */
	if ((error = xosgbpb_writew(handle, oDrawBuf.header.bufP, oDrawBuf.header.currentSize, NULL)) != NULL)
		goto file_save_error;

	/* write font table (if needed) */
	if (oDrawBuf.fontTable.bufP != NULL
	    && (error = xosgbpb_writew(handle, oDrawBuf.fontTable.bufP, oDrawBuf.fontTable.currentSize, NULL)) != NULL)
		goto file_save_error;

	/* write Draw body */
	if ((error = xosgbpb_writew(handle, oDrawBuf.body.bufP, oDrawBuf.body.currentSize, NULL)) != NULL)
		goto file_save_error;

	if ((error = xosfind_closew(handle)) != NULL)
		goto file_save_error;

	if ((error = xosfile_set_type(drawfilename, osfile_TYPE_DRAW)) != NULL)
		goto file_save_error;

	return true;

file_save_error:
	LOG(("drawbuf_save_file() error: 0x%x: %s",
			error->errnum, error->errmess));
	warn_user("SaveError", error->errmess);
	if (handle != NULL)
		(void)xosfind_closew(handle);
	return false;
}


/**
 * add options object
 */
bool add_options(void)
{
	drawfile_object *dro;
	drawfile_options *dfo;

	if ((dro = drawbuf_claim(8 + sizeof(drawfile_options), DrawBuf_eBody)) == NULL)
		return false;

	dro->type = drawfile_TYPE_OPTIONS;
	dro->size = 8 + sizeof(drawfile_options);

	dfo = &dro->data.options;
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
bool add_box(struct box *box, unsigned long cbc, long x, long y)
{
	struct box *c;

	x += box->x * 512;
	y -= box->y * 512;

	if (box->style && box->style->visibility == CSS_VISIBILITY_HIDDEN) {
		for (c = box->children; c != NULL; c = c->next) {
			if (!add_box(c, cbc, x, y))
				return false;
		}
		return true;
	}

	if (box->style && box->style->background_color != TRANSPARENT) {
		cbc = box->style->background_color;
		if (!add_rect(box, cbc<<8, x, y, false))
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
				return add_graphic(box->object,
						box, cbc, x, y);

			case CONTENT_HTML:
				c = box->object->data.html.layout->children;
				return add_box(c, cbc, x, y);

			default:
				break;
		}

	} else if (box->gadget && box->gadget->type == GADGET_CHECKBOX) {
		return add_rect(box, 0xDEDEDE00, x, y, false);

	} else if (box->gadget && box->gadget->type == GADGET_RADIO) {
		return add_circle(box, 0xDEDEDE00, x, y);

	} else if (box->text && box->font) {
		int colour;

		if (box->length == 0)
			return true;

		/* text-decoration */
		colour = box->style->color;
		colour = ((((colour >> 16) + (cbc >> 16)) / 2) << 16)
			| (((((colour >> 8) & 0xff) +
			     ((cbc >> 8) & 0xff)) / 2) << 8)
			| ((((colour & 0xff) + (cbc & 0xff)) / 2) << 0);
		if (box->style->text_decoration & CSS_TEXT_DECORATION_UNDERLINE
			|| (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_UNDERLINE && box->parent->parent->type == BOX_BLOCK)) {
			if (!add_line(box, (unsigned)colour<<8,
					x, (int)(y+(box->height*0.1*512))))
				return false;
		}
		if (box->style->text_decoration & CSS_TEXT_DECORATION_OVERLINE
			|| (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_OVERLINE && box->parent->parent->type == BOX_BLOCK)) {
			if (!add_line(box, (unsigned)colour<<8,
					x, (int)(y+(box->height*0.9*512))))
				return false;
		}
		if (box->style->text_decoration & CSS_TEXT_DECORATION_LINE_THROUGH || (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_LINE_THROUGH && box->parent->parent->type == BOX_BLOCK)) {
			if (!add_line(box, (unsigned)colour<<8,
					x, (int)(y+(box->height*0.4*512))))
				return false;
		}

		return add_text(box, cbc, x, y);
	} else {
		for (c = box->children; c != 0; c = c->next) {
			if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
				if (!add_box(c, cbc, x, y))
					return false;
		}
		for (c = box->float_children; c !=  0; c = c->next_float) {
			if (!add_box(c, cbc, x, y))
				return false;
		}
	}

	return true;
}


/**
 * Add images to the drawfile.
 */
bool add_graphic(struct content *content, struct box *box,
		unsigned long cbc, long x, long y)
{
	int sprite_length;
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

	if ((dro = (drawfile_object *)drawbuf_claim(8 + 16 + sprite_length, DrawBuf_eBody)) == NULL)
		return false;

	dro->type = drawfile_TYPE_SPRITE;
	dro->size = 8 + 16 + sprite_length;

	ds = &dro->data.sprite;
	ds->bbox.x0 = x;
	ds->bbox.y0 = y - (box->padding[TOP] + box->height + box->padding[BOTTOM])*512;
	ds->bbox.x1 = x + (box->padding[LEFT] + box->width + box->padding[RIGHT])*512;
	ds->bbox.y1 = y;

	switch (content->type) {
		case CONTENT_JPEG:
			memcpy((char*)ds+16, (char*)content->data.jpeg.sprite_area+content->data.jpeg.sprite_area->first, (unsigned)sprite_length);
			break;
#ifdef WITH_PNG
		case CONTENT_PNG:
			memcpy((char*)ds+16, (char*)content->data.png.sprite_area+content->data.png.sprite_area->first, (unsigned)sprite_length);
			break;
#endif
		case CONTENT_GIF:
			memcpy((char*)ds+16, (char*)content->data.gif.gif->frame_image, (unsigned)sprite_length);
			break;
#ifdef WITH_SPRITE
		case CONTENT_SPRITE:
			memcpy((char*)ds+16, (char*)content->data.sprite.data+((osspriteop_area*)content->data.sprite.data)->first, (unsigned)sprite_length);
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
bool add_rect(struct box *box,
		unsigned long cbc, long x, long y, bool bg)
{
	drawfile_object *dro;
	drawfile_path *dp;
	draw_path_element *dpe;

	if ((dro = (drawfile_object *)drawbuf_claim(8 + 96, DrawBuf_eBody)) == NULL)
		return false;

	dro->type = drawfile_TYPE_PATH;
	dro->size = 8 + 96;

	dp = &dro->data.path;
	if (bg) {
		dp->bbox.x0 = 0;
		dp->bbox.y0 = 0;
		dp->bbox.x1 = A4PAGEWIDTH*512;
		dp->bbox.y1 = A4PAGEHEIGHT*512;
	} else {
		dp->bbox.x0 = x;
		dp->bbox.y0 = y - (box->padding[TOP] + box->height + box->padding[BOTTOM])*512;
		dp->bbox.x1 = x + (box->padding[LEFT] + box->width + box->padding[RIGHT])*512;
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
bool add_line(struct box *box, unsigned long cbc, long x, long y)
{
	drawfile_object *dro;
	drawfile_path *dp;
	draw_path_element *dpe;

	if ((dro = (drawfile_object *)drawbuf_claim(8 + 60, DrawBuf_eBody)) == NULL)
		return false;

	dro->type = drawfile_TYPE_PATH;
	dro->size = 8 + 60;

	dp = &dro->data.path;
	dp->bbox.x0 = x;
	dp->bbox.y0 = y - (box->padding[TOP] + box->height + box->padding[BOTTOM])*512;
	dp->bbox.x1 = x + (box->padding[LEFT] + box->width + box->padding[RIGHT])*512;
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
bool add_circle(struct box *box, unsigned long cbc, long x, long y)
{
	double radius = 0., kappa;
	double cx, cy;
	drawfile_object *dro;
	drawfile_path *dp;
	draw_path_element *dpe;

	if ((dro = (drawfile_object *)drawbuf_claim(8 + 160, DrawBuf_eBody)) == NULL)
		return false;

	dro->type = drawfile_TYPE_PATH;
	dro->size = 8 + 160;

	dp = &dro->data.path;
	dp->bbox.x0 = x;
	dp->bbox.y0 = y - (box->padding[TOP] + box->height + box->padding[BOTTOM])*512;
	dp->bbox.x1 = x + (box->padding[LEFT] + box->width + box->padding[RIGHT])*512;
	dp->bbox.y1 = y;

	cx = (dp->bbox.x1 - dp->bbox.x0) / 2.;
	cy = (dp->bbox.y1 - dp->bbox.y0) / 2.;
	if (cx == cy)
		radius = cx;		/* box is square */
	else if (cx > cy) {
		radius = cy;
		dp->bbox.x1 -= cx - cy;	/* reduce box width */
	}
	else if (cy > cx) {
		radius = cx;
		dp->bbox.y0 += cy - cx;	/* reduce box height */
	}
	kappa = radius * 4. * (sqrt(2.) - 1.) / 3.; /* ~= 0.5522847498 */

	dp->fill = cbc;
	dp->outline = cbc;
	dp->width = 0;
	dp->style.flags = drawfile_PATH_ROUND;

	/*
	 *    Z   b   Y
	 *
	 *    a   X   c
	 *
	 *    V   d   W
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


/**
 * Add the text line to the diagram.
 */
static bool add_text(struct box *box, unsigned long cbc, long x, long y)
{
	const char *txt = box->text;
	size_t txt_len = box->length;

	while (txt_len != 0) {
		unsigned int width, rolength, consumed;
		const char *rofontname, *rotext;
		byte fontIndex;
		drawfile_object *dro;
		drawfile_text *dt;

		nsfont_txtenum(box->font, txt, txt_len,
				&width,
				&rofontname,
				&rotext,
				&rolength,
				&consumed);
		LOG(("txtenum <%.*s> (%d bytes), returned width %d, font name <%s>, RISC OS text <%.*s>, consumed %d\n", txt_len, txt, txt_len, width, rofontname, rolength, rotext, consumed));
		/* Error happened ? */
		if (rotext == NULL)
			return false;

		if (!drawbuf_add_font(rofontname, &fontIndex))
			return false;

		if ((dro = (drawfile_object *)drawbuf_claim(8 + 44 + ((rolength + 1 + 3) & -4), DrawBuf_eBody)) == NULL)
			return false;

		dro->type = drawfile_TYPE_TEXT;
		dro->size = 8 + 44 + ((rolength + 1 + 3) & -4);

		dt = &dro->data.text;
		dt->bbox.x0 = x;
		dt->bbox.y0 = y - box->height*1.5*512;
		dt->bbox.x1 = x + width*512;
		dt->bbox.y1 = y;
		dt->fill = box->style->color << 8;
		dt->bg_hint = cbc << 8;
		dt->style.font_index = fontIndex;
		dt->style.reserved[0] = 0;
		dt->style.reserved[1] = 0;
		dt->style.reserved[2] = 0;
		dt->xsize = box->font->size*40;
		dt->ysize = box->font->size*40;
		dt->base.x = x;
		dt->base.y = y - box->height*512 + 1536;
		strncpy(dt->text, rotext, rolength);
		do {
			dt->text[rolength++] = 0;
		} while (rolength % 4);

		free(rotext);

		/* Go to next chunk : */
		x += width * 512;
		txt += consumed;
		txt_len -= consumed;
	}

	return true;
}
#endif
