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
	size_t offset;
} drawbuf_group_t;

typedef struct {
	drawbuf_part_t header;
	drawbuf_part_t fontTable;
	drawbuf_part_t body;

	drawbuf_group_t *groupsP;
	size_t currentNumGroups;
	size_t maxNumGroups;

	void **fontNamesP; /**< 256 element malloc() pointer array */
	size_t numFonts;
} drawbuf_t;

static void *drawbuf_claim(size_t size, drawbuf_type_e type);
static void drawbuf_free(void);
static bool drawbuf_add_font(const char *fontNameP, byte *fontIndex);
static bool drawbuf_save_file(const char *drawfilename);
static bool drawbuf_group_begin(const char *name);
static bool drawbuf_group_end(void);

static bool add_options(void);
static bool add_box(struct box *box, colour cbc, int x, int y);
static bool add_graphic(struct content *content, struct box *box,
		colour cbc, int x, int y);
static bool add_line(struct box *box, os_colour cbc, int x, int y);
static bool add_text(struct box *box, os_colour cbc, os_colour fc,
		int x, int y);

static bool add_checkbox(int x, int y, int width, int height, bool selected);
static bool add_radio(int x, int y, int width, int height, bool selected);

static bool add_rect(int x, int y, int width, int height, os_colour col);
static bool add_circle(int x0, int y0, int radius, os_colour colour);
static bool add_border(os_colour col, int width, css_border_style style,
		int x0, int y0, int x1, int y1);


static drawbuf_t oDrawBuf; /* static -> complete struct inited to 0 */

/**
 * Export a content as a Drawfile.
 *
 * \param  c     content to export
 * \param  path  path to save Drawfile as
 * \return  true on success, false on error (error got reported)
 */
bool save_as_draw(struct content *c, const char *path)
{
	struct box *box;
	int current_width;
	colour bc;
	drawfile_diagram_base *diagram;

	if (c->type != CONTENT_HTML)
		return false;

	box = c->data.html.layout->children;
	current_width = c->available_width;

	if ((diagram = (drawfile_diagram_base *)drawbuf_claim(sizeof(drawfile_diagram_base), DrawBuf_eHeader)) == NULL)
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
		if (!add_rect(0, A4PAGEHEIGHT*512, A4PAGEWIDTH*512,
				A4PAGEHEIGHT*512, (os_colour)bc<<8))
			goto draw_save_error;
	}

	/* right, traverse the tree and grab the contents */
	if (!add_box(box, bc, 0, A4PAGEHEIGHT*512))
		goto draw_save_error;

	if (oDrawBuf.currentNumGroups != 0) {
		/** \todo: give GUI error. */
		goto draw_save_error;
	}

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
static void *drawbuf_claim(size_t size, drawbuf_type_e type)
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
	free(oDrawBuf.groupsP); oDrawBuf.groupsP = NULL;
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
 * \return  true on success, false on error (error got reported)
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
 * \return  true on success, false on error (error got reported)
 */
static bool drawbuf_save_file(const char *drawfilename)
{
	size_t index;
	os_fw handle = 0;
	os_error *error = NULL;

	/* create font table (if needed). */
	if (oDrawBuf.numFonts > 0) {
		drawfile_object *dro;

		if ((dro = (drawfile_object *)drawbuf_claim(8, DrawBuf_eFontTable)) == NULL)
			goto file_save_error;

		dro->type = drawfile_TYPE_FONT_TABLE;
		/* we can't write dro->size yet. */

		for (index = 0; index < oDrawBuf.numFonts; ++index) {
			const char *fontNameP = oDrawBuf.fontNamesP[index];
			size_t len = 1 + strlen(fontNameP) + 1;
			byte *bufP;

			if ((bufP = (byte *)drawbuf_claim(len, DrawBuf_eFontTable)) == NULL)
				goto file_save_error;
			*bufP++ = (byte)index + 1;
			memcpy(bufP, fontNameP, len + 1);
		}
		/* align to next word boundary */
		if (oDrawBuf.fontTable.currentSize % 4) {
			size_t wordpad = 4 - (oDrawBuf.fontTable.currentSize & 3);
			byte *bufP;

			if ((bufP = (byte *)drawbuf_claim(wordpad, DrawBuf_eFontTable)) == NULL)
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
	handle = 0;

	if ((error = xosfile_set_type(drawfilename, osfile_TYPE_DRAW)) != NULL)
		goto file_save_error;

	return true;

file_save_error:
	LOG(("drawbuf_save_file() error: 0x%x: %s",
			error->errnum, error->errmess));
	warn_user("SaveError", error->errmess);
	if (handle != 0)
		(void)xosfind_closew(handle);
	return false;
}


/**
 * Create a new Draw group.  Successful call needs to be matched with
 * drawbuf_group_end().
 *
 * \param name name of the Draw group (max 12 printable chars long)
 * \return  true on success, false on error (error got reported)
 */
static bool drawbuf_group_begin(const char *name)
{
	drawfile_object *dro;
	drawfile_group_base *dgb;
	size_t offsetGroup, nameLen;

	if (oDrawBuf.groupsP == NULL) {
		if ((oDrawBuf.groupsP = (drawbuf_group_t *)malloc(4 * sizeof(drawbuf_group_t))) == NULL) {
			warn_user("NoMemory", 0);
			return false;
		}
		oDrawBuf.currentNumGroups = 0;
		oDrawBuf.maxNumGroups = 4;
	} else if (oDrawBuf.currentNumGroups == oDrawBuf.maxNumGroups) {
		if ((oDrawBuf.groupsP = (drawbuf_group_t *)realloc(oDrawBuf.groupsP, 2*oDrawBuf.maxNumGroups * sizeof(drawbuf_group_t))) == NULL) {
			warn_user("NoMemory", 0);
			return false;
		}
		oDrawBuf.maxNumGroups *= 2;
	}

	offsetGroup = oDrawBuf.body.currentSize;
	if ((dro = (drawfile_object *)drawbuf_claim(8 + sizeof(drawfile_group_base), DrawBuf_eBody)) == NULL)
		return false;

	oDrawBuf.groupsP[oDrawBuf.currentNumGroups++].offset = offsetGroup;

	dro->type = drawfile_TYPE_GROUP;
	dro->size = 8 + sizeof(drawfile_group_base); /* will be correctly filled in during drawbuf_group_end() */

	dgb = (drawfile_group_base *)&dro->data.group;
	dgb->bbox.x0 = dgb->bbox.y0 = dgb->bbox.x1 = dgb->bbox.y1 = 0; /* will be filled in during drawbuf_group_end() */
	nameLen = strlen(name);
	if (nameLen >= sizeof(dgb->name))
		memcpy(dgb->name, name, sizeof(dgb->name));
	else {
		memcpy(dgb->name, name, nameLen);
		memset(&dgb->name[nameLen], ' ', sizeof(dgb->name) - nameLen);
	}

	return true;
}


/**
 * Ends an outstanding Draw group instance.
 *
 * \return  true on success, false on error (error got reported)
 */
static bool drawbuf_group_end(void)
{
	size_t bgOffset, egOffset, offset;
	drawfile_object *dro;
	os_box bbox;

	if (oDrawBuf.currentNumGroups == 0)
		return false; /** \todo: add GUI error */

	offset = bgOffset = oDrawBuf.groupsP[--oDrawBuf.currentNumGroups].offset;
	egOffset = oDrawBuf.body.currentSize;
	bbox.y0 = bbox.x0 = INT_MAX;
	bbox.y1 = bbox.y0 = INT_MIN;
	while (offset < egOffset) {
		dro = (drawfile_object *)&oDrawBuf.body.bufP[offset];

		if (dro->type != drawfile_TYPE_FONT_TABLE && dro->type != drawfile_TYPE_OPTIONS) {
			/* we assume there is a bbox. */
			if (dro->data.path.bbox.x0 < bbox.x0)
				bbox.x0 = dro->data.path.bbox.x0;
			if (dro->data.path.bbox.x1 > bbox.x1)
				bbox.x1 = dro->data.path.bbox.x1;
			if (dro->data.path.bbox.y0 < bbox.y0)
				bbox.y0 = dro->data.path.bbox.y0;
			if (dro->data.path.bbox.y1 > bbox.y1)
				bbox.y1 = dro->data.path.bbox.y1;
		}

		assert(dro->size != 0);
		offset += dro->size;
	}
	if (offset != egOffset)
		return false; /** \todo: add GUI error */

	dro = (drawfile_object *)&oDrawBuf.body.bufP[bgOffset];
	dro->size = egOffset - bgOffset;
	if (bbox.x0 != INT_MAX && bbox.y0 != INT_MAX
			&& bbox.x1 != INT_MIN && bbox.y1 != INT_MIN)
		dro->data.group.bbox = bbox;

	return true;
}


/**
 * Add options object
 *
 * \return  true on success, false on error (error got reported)
 */
static bool add_options(void)
{
	drawfile_object *dro;
	drawfile_options *dfo;

	if ((dro = (drawfile_object *)drawbuf_claim(8 + sizeof(drawfile_options), DrawBuf_eBody)) == NULL)
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
 *
 * \param box
 * \param cbc current background color
 * \param x
 * \param y
 * \return  true on success, false on error (error got reported)
 *
 * Very similar to html_redraw_box.
 */
static bool add_box(struct box *box, colour cbc, int x, int y)
{
	struct box *c;
	int width, height;
	int padding_left, padding_top;
	int padding_width, padding_height;

	x += box->x * 512;
	y -= box->y * 512;
	width = box->width * 512;
	height = box->height * 512;
	padding_left = box->padding[LEFT] * 512;
	padding_top = box->padding[TOP] * 512;
	padding_width = (box->padding[LEFT] + box->width +
			box->padding[RIGHT]) * 512;
	padding_height = (box->padding[TOP] + box->height +
			box->padding[BOTTOM]) * 512;

	/* if visibility is hidden render children only */
	if (box->style && box->style->visibility == CSS_VISIBILITY_HIDDEN) {
		if (!drawbuf_group_begin("hidden box"))
			return false;
		for (c = box->children; c != NULL; c = c->next) {
			if (!add_box(c, cbc, x, y))
				return false;
		}
		return drawbuf_group_end();
	}

	if (!drawbuf_group_begin("vis box"))
		return false;

	/* borders */
	if (box->style && box->border[TOP])
		add_border((os_colour)box->style->border[TOP].color << 8,
				box->border[TOP] * 512,
				box->style->border[TOP].style,
				x - box->border[LEFT] * 512,
				y + box->border[TOP] * 256,
				x + padding_width + box->border[RIGHT] * 512,
				y + box->border[TOP] * 256);
	if (box->style && box->border[RIGHT])
		add_border((os_colour)box->style->border[RIGHT].color << 8,
				box->border[RIGHT] * 512,
				box->style->border[RIGHT].style,
				x + padding_width + box->border[RIGHT] * 256,
				y + box->border[TOP] * 512,
				x + padding_width + box->border[RIGHT] * 256,
				y - padding_height - box->border[BOTTOM] * 512);
	if (box->style && box->border[BOTTOM])
		add_border((os_colour)box->style->border[BOTTOM].color << 8,
				box->border[BOTTOM] * 512,
				box->style->border[BOTTOM].style,
				x - box->border[LEFT] * 512,
				y - padding_height - box->border[BOTTOM] * 256,
				x + padding_width + box->border[RIGHT] * 512,
				y - padding_height - box->border[BOTTOM] * 256);
	if (box->style && box->border[LEFT])
		add_border((os_colour)box->style->border[LEFT].color << 8,
				box->border[LEFT] * 512,
				box->style->border[LEFT].style,
				x - box->border[LEFT] * 256,
				y + box->border[TOP] * 512,
				x - box->border[LEFT] * 256,
				y - padding_height - box->border[BOTTOM] * 512);

	/* background */
	if (box->style && (box->type != BOX_INLINE ||
			box->style != box->parent->parent->style)) {
		if (box->style->background_color != TRANSPARENT) {
			cbc = box->style->background_color;
			if (!add_rect(x, y,
					padding_width,
					padding_height,
					(os_colour)cbc<<8))
				return false;
		}
	}

	if (box->object) {
		switch (box->object->type) {
			case CONTENT_JPEG:
#ifdef WITH_PNG
			case CONTENT_PNG:
#endif
#ifdef WITH_MNG
			case CONTENT_JNG:
			case CONTENT_MNG:
#endif
			case CONTENT_GIF:
#ifdef WITH_SPRITE
			case CONTENT_SPRITE:
#endif
				if (!add_graphic(box->object,
						box, cbc, x, y))
					return false;
				break;

			case CONTENT_HTML:
				c = box->object->data.html.layout->children;
				if (!add_box(c, cbc, x, y))
					return false;
				break;

			default:
				break;
		}

	} else if (box->gadget && box->gadget->type == GADGET_CHECKBOX) {
		if (!add_checkbox(x + padding_left, y - padding_top,
				width, height,
				box->gadget->selected))
			return false;
		return drawbuf_group_end();

	} else if (box->gadget && box->gadget->type == GADGET_RADIO) {
		if (!add_radio(x + padding_left, y - padding_top,
				width, height,
				box->gadget->selected))
			return false;

	} else if (box->gadget && box->gadget->type == GADGET_FILE) {
		/** \todo: something to do here ? */

	} else if (box->text && box->font) {
		colour colour;

		if (box->length == 0)
			return drawbuf_group_end();

		/* text-decoration */
		colour = box->style->color;
		colour = ((((colour >> 16) + (cbc >> 16)) >> 1) << 16)
			| (((((colour >> 8) & 0xff) + ((cbc >> 8) & 0xff)) >> 1) << 8)
			| ((((colour & 0xff) + (cbc & 0xff)) >> 1) << 0);
		if (box->style->text_decoration & CSS_TEXT_DECORATION_UNDERLINE
			|| (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_UNDERLINE && box->parent->parent->type == BOX_BLOCK)) {
			if (!add_line(box, (os_colour)colour<<8,
					x, (int)(y+(box->height*0.1*512))))
				return false;
		}
		if (box->style->text_decoration & CSS_TEXT_DECORATION_OVERLINE
			|| (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_OVERLINE && box->parent->parent->type == BOX_BLOCK)) {
			if (!add_line(box, (os_colour)colour<<8,
					x, (int)(y+(box->height*0.9*512))))
				return false;
		}
		if (box->style->text_decoration & CSS_TEXT_DECORATION_LINE_THROUGH || (box->parent->parent->style->text_decoration & CSS_TEXT_DECORATION_LINE_THROUGH && box->parent->parent->type == BOX_BLOCK)) {
			if (!add_line(box, (os_colour)colour<<8,
					x, (int)(y+(box->height*0.4*512))))
				return false;
		}

		if (!add_text(box, (os_colour)cbc<<8,
				(os_colour)box->style->color<<8, x, y))
			return false;

	} else {
		for (c = box->children; c != NULL; c = c->next) {
			if (c->type != BOX_FLOAT_LEFT && c->type != BOX_FLOAT_RIGHT)
				if (!add_box(c, cbc, x, y))
					return false;
		}

		for (c = box->float_children; c !=  NULL; c = c->next_float) {
			if (!add_box(c, cbc, x, y))
				return false;
		}
	}

	return drawbuf_group_end();
}


/**
 * Add images to the drawfile.
 *
 * \return  true on success, false on error (error got reported)
 */
static bool add_graphic(struct content *content, struct box *box,
		colour cbc, int x, int y)
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
#ifdef WITH_MNG
		case CONTENT_JNG:
		case CONTENT_MNG:
			sprite_length = ((osspriteop_header*)((char*)content->data.mng.sprite_area+content->data.mng.sprite_area->first))->size;
			break;
#endif
		case CONTENT_GIF:
			sprite_length = ((osspriteop_header*)((char*)content->data.gif.gif->frame_image+content->data.gif.gif->frame_image->first))->size;
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
#ifdef WITH_MNG
		case CONTENT_JNG:
		case CONTENT_MNG:
			memcpy((char*)ds+16, (char*)content->data.mng.sprite_area+content->data.mng.sprite_area->first, (unsigned)sprite_length);
			break;
#endif
		case CONTENT_GIF:
			memcpy((char*)ds+16, (char*)content->data.gif.gif->frame_image+content->data.gif.gif->frame_image->first, (unsigned)sprite_length);
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
 *
 * \param x,y top-left coord of the rectangle.
 * \param width,height width,height of the rectangle
 * \param col rectangle colour Draw format
 * \return  true on success, false on error (error got reported)
 */
static bool add_rect(int x, int y, int width, int height, os_colour col)
{
	drawfile_object *dro;
	drawfile_path *dp;
	draw_path_element *dpe;

	if ((dro = (drawfile_object *)drawbuf_claim(8 + 96, DrawBuf_eBody)) == NULL)
		return false;

	dro->type = drawfile_TYPE_PATH;
	dro->size = 8 + 96;

	dp = &dro->data.path;
	dp->bbox.x0 = x;
	dp->bbox.y0 = y - height;
	dp->bbox.x1 = x + width;
	dp->bbox.y1 = y;

	dp->fill = col;
	dp->outline = 0xFFFFFFFF; /* no stroke */
	dp->width = 0;
	dp->style.flags = 0;

	dpe = (draw_path_element *) (((int *) &dp->path) + 0 / sizeof (int));
	dpe->tag = draw_MOVE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.move_to.x = x;
	dpe->data.move_to.y = y;

	dpe = (draw_path_element *) (((int *) &dp->path) + 12 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = x;
	dpe->data.line_to.y = y - height;

	dpe = (draw_path_element *) (((int *) &dp->path) + 24 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = x + width;
	dpe->data.line_to.y = y - height;

	dpe = (draw_path_element *) (((int *) &dp->path) + 36 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = x + width;
	dpe->data.line_to.y = y;

	dpe = (draw_path_element *) (((int *) &dp->path) + 48 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = x;
	dpe->data.line_to.y = y;

	dpe = (draw_path_element *) (((int *) &dp->path) + 60 / sizeof (int));
	dpe->tag = draw_END_PATH;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;

	return true;
}


/**
 * Add a line to the diagram
 *
 * \return  true on success, false on error (error got reported)
 */
static bool add_line(struct box *box, os_colour cbc, int x, int y)
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

	dp->fill = 0xFFFFFFFF; /* do not fill */
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
 * Add a filled circle to the diagram.
 *
 * \return  true on success, false on error (error got reported)
 */
static bool add_circle(int x0, int y0, int radius, os_colour colour)
{
	int kappa;
	drawfile_object *dro;
	drawfile_path *dp;
	draw_path_element *dpe;

	if ((dro = (drawfile_object *)drawbuf_claim(8 + 160, DrawBuf_eBody)) == NULL)
		return false;

	dro->type = drawfile_TYPE_PATH;
	dro->size = 8 + 160;

	dp = &dro->data.path;
	dp->bbox.x0 = x0 - radius;
	dp->bbox.y0 = y0 - radius;
	dp->bbox.x1 = x0 + radius;
	dp->bbox.y1 = y0 + radius;

	kappa = (int)(radius * 4. * (sqrt(2.) - 1.) / 3.); /* ~= 0.5522847498 */

	dp->fill = colour;
	dp->outline = 0xFFFFFFFF; /* do not stroke */
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
	dpe->data.move_to.x = x0 - radius;
	dpe->data.move_to.y = y0;

	/* point1->point2 : (point1)(ctrl1)(ctrl2)(point2) */

	/* a->b : (x-r, y)(x-r, y+k)(x-k, y+r)(x, y+r) */
	dpe = (draw_path_element *) (((int *) &dp->path) + 12 / sizeof (int));
	dpe->tag = draw_BEZIER_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.bezier_to[0].x = x0 - radius;
	dpe->data.bezier_to[0].y = y0 + kappa;
	dpe->data.bezier_to[1].x = x0 - kappa;
	dpe->data.bezier_to[1].y = y0 + radius;
	dpe->data.bezier_to[2].x = x0;
	dpe->data.bezier_to[2].y = y0 + radius;

	/* b->c : (x, y+r)(x+k, y+r)(x+r, y+k)(x+r, y)*/
	dpe = (draw_path_element *) (((int *) &dp->path) + 40 / sizeof (int));
	dpe->tag = draw_BEZIER_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.bezier_to[0].x = x0 + kappa;
	dpe->data.bezier_to[0].y = y0 + radius;
	dpe->data.bezier_to[1].x = x0 + radius;
	dpe->data.bezier_to[1].y = y0 + kappa;
	dpe->data.bezier_to[2].x = x0 + radius;
	dpe->data.bezier_to[2].y = y0;

	/* c->d : (x+r, y)(x+r, y-k)(x+k, y-r)(x, y-r) */
	dpe = (draw_path_element *) (((int *) &dp->path) + 68 / sizeof (int));
	dpe->tag = draw_BEZIER_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.bezier_to[0].x = x0 + radius;
	dpe->data.bezier_to[0].y = y0 - kappa;
	dpe->data.bezier_to[1].x = x0 + kappa;
	dpe->data.bezier_to[1].y = y0 - radius;
	dpe->data.bezier_to[2].x = x0;
	dpe->data.bezier_to[2].y = y0 - radius;

	/* d->a : (x, y-r)(x-k, y-r)(x-r, y-k)(x-r, y)*/
	dpe = (draw_path_element *) (((int *) &dp->path) + 96 / sizeof (int));
	dpe->tag = draw_BEZIER_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.bezier_to[0].x = x0 - kappa;
	dpe->data.bezier_to[0].y = y0 - radius;
	dpe->data.bezier_to[1].x = x0 - radius;
	dpe->data.bezier_to[1].y = y0 - kappa;
	dpe->data.bezier_to[2].x = x0 - radius;
	dpe->data.bezier_to[2].y = y0;

	/* end */
	dpe = (draw_path_element *) (((int *) &dp->path) + 124 / sizeof (int));
	dpe->tag = draw_END_PATH;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;

	return true;
}


/**
 * Add the text line to the diagram.
 *
 * \param cbc background colour (Draw format)
 * \param fc foreground (text) colour (Draw format)
 * \param x,y top-left corner
 * \return  true on success, false on error (error got reported)
 */
static bool add_text(struct box *box, os_colour cbc, os_colour fc,
		int x, int y)
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
		dt->bbox.y0 = y - box->height*512;
		dt->bbox.x1 = x + width*512;
		dt->bbox.y1 = y;
		dt->fill = fc;
		dt->bg_hint = cbc;
		dt->style.font_index = fontIndex;
		dt->style.reserved[0] = dt->style.reserved[1] = dt->style.reserved[2] = 0;
		dt->xsize = box->font->size*40;
		dt->ysize = box->font->size*40;
		dt->base.x = x;
		dt->base.y = y - (int)(box->height*.75*512);
		strncpy(dt->text, rotext, rolength);
		do {
			dt->text[rolength++] = 0;
		} while (rolength % 4);

		free((void *)rotext);

		/* Go to next chunk : */
		x += width * 512;
		txt += consumed;
		txt_len -= consumed;
	}

	return true;
}

/**
 * Add checkbox object
 *
 * \param x,y top left of checkbox object
 * \param width,height width and height of the checkbox object
 * \param selected true when this checkbox object is selected, false otherwise
 * \return  true on success, false on error (error got reported)
 *
 * Very similar to html_redraw_checkbox()
 */
static bool add_checkbox(int x, int y, int width, int height, bool selected)
{
	const int min = (width < height) ? width : height;
	const int z = (min == 0) ? 1 : (int)(min * 0.15);

	if (!add_rect(x, y, width, height, os_COLOUR_BLACK)
		|| !add_rect(x + z, y - z, width - z - z, height - z - z,
			os_COLOUR_WHITE))
		return false;

	if (selected
		&& !add_rect(x + z + z, y - z - z,
				width - z - z - z - z,
				height - z - z - z - z,
				os_COLOUR_RED))
		return false;

	return true;
}


/**
 * Add radio object
 *
 * \param x,y top left of radio object
 * \param width,height width and height of the bounding box of radio object
 * \param selected true when this radio object is selected, false otherwise
 * \return  true on success, false on error (error got reported)
 *
 * Very similar to html_redraw_radio()
 */
static bool add_radio(int x, int y, int width, int height, bool selected)
{
	const int orgx = x + width / 2;
	const int orgy = y - height / 2;
	const int radius = (width < height) ? width : height;

	if (!add_circle(orgx, orgy, (int)(radius * .5), os_COLOUR_BLACK)
		|| !add_circle(orgx, orgy, (int)(radius * .4), os_COLOUR_WHITE))
		return false;

	if (selected
		&& !add_circle(orgx, orgy, (int)(radius * .3), os_COLOUR_RED))
		return false;

	return true;
}


/**
 * Add one of the borders of a box.
 *
 * \param col border colour (Draw format)
 * \param width border width (Draw format)
 * \param style CSS style specifying the type of the border
 * \param x0,y0,x1,y1 start & end of the border (Draw coordinates)
 * \return  true on success, false on error (error got reported)
 *
 * Very similar to html_redraw_border().
 */
static bool add_border(os_colour col, int width, css_border_style style,
		int x0, int y0, int x1, int y1)
{
	drawfile_object *dro;
	drawfile_path *dp;
	draw_path_element *dpe;
	static const int dash_pattern_dotted[] = { 0, 1, 512 };
	static const int dash_pattern_dashed[] = { 0, 1, 2048 };
	const draw_dash_pattern *dash_pattern;
	size_t sizeNeeded;

	if (style == CSS_BORDER_STYLE_DOTTED)
		dash_pattern = (const draw_dash_pattern *) &dash_pattern_dotted;
	else if (style == CSS_BORDER_STYLE_DASHED)
		dash_pattern = (const draw_dash_pattern *) &dash_pattern_dashed;
	else
		dash_pattern = NULL;

	sizeNeeded = (dash_pattern != NULL) ? 8 + 12 + 60 : 8 + 60;
	if ((dro = (drawfile_object *)drawbuf_claim(sizeNeeded, DrawBuf_eBody)) == NULL)
		return false;

	dro->type = drawfile_TYPE_PATH;
	dro->size = sizeNeeded;

	dp = &dro->data.path;
	dp->bbox.x0 = (x0 < x1) ? x0 : x1;
	dp->bbox.y0 = (y0 < y1) ? y0 : y1;
	dp->bbox.x1 = (x0 < x1) ? x1 : x0;
	dp->bbox.y1 = (y0 < y1) ? y1 : y0;

	dp->fill = 0xFFFFFFFF; /* do not fill */
	dp->outline = col;
	dp->width = width;
	dp->style.flags = (drawfile_PATH_MITRED << drawfile_PATH_JOIN_SHIFT)
		| (drawfile_PATH_BUTT << drawfile_PATH_END_SHIFT)
		| (drawfile_PATH_BUTT << drawfile_PATH_START_SHIFT);
	if (dash_pattern != NULL)
		dp->style.flags |= drawfile_PATH_DASHED;
	dp->style.reserved = 0;
	dp->style.cap_width = 0;
	dp->style.cap_length = 0;

	/* dash ? */
	if (dash_pattern != NULL) {
		draw_dash_pattern *dpp = (draw_dash_pattern *)&dp->path;
		memcpy(dpp, dash_pattern, sizeof(dash_pattern_dotted));
		dpe = (draw_path_element *)(((byte *)dpp) + sizeof(dash_pattern_dotted));
	} else
		dpe = (draw_path_element *)&dp->path;

	/* left end */
	dpe->tag = draw_MOVE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.move_to.x = x0;
	dpe->data.move_to.y = y0;

	/* right end */
	dpe = (draw_path_element *)(((int *)dpe) + 3);
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = x1;
	dpe->data.line_to.y = y1;

	/* end */
	dpe = (draw_path_element *)(((int *)dpe) + 3);
	dpe->tag = draw_END_PATH;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;

	return true;
}
#endif
