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
#include "netsurf/desktop/plotters.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/form.h"
#include "netsurf/render/layout.h"
#include "netsurf/riscos/bitmap.h"
#include "netsurf/riscos/save_draw.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#ifdef WITH_DRAW_EXPORT

/* in browser units = OS/2 = draw/512 */
/* note that all coordinates passed into draw_plot_* are in browser units */
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
static bool draw_plot_clg(colour c);
static bool draw_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed);
static bool draw_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed);
static bool draw_plot_polygon(int *p, unsigned int n, colour fill);
static bool draw_plot_fill(int x0, int y0, int x1, int y1, colour c);
static bool draw_plot_clip(int clip_x0, int clip_y0, int clip_x1,
		int clip_y1);
static bool draw_plot_text(int x, int y, struct css_style *style,
		const char *text, size_t length, colour bc, colour colour);
static bool draw_plot_disc(int x, int y, int radius, colour colour);
static bool draw_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bc);
static bool draw_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y);

static drawbuf_t oDrawBuf; /* static -> complete struct inited to 0 */

/* our plotters */
const struct plotter_table draw_plotters = {
	draw_plot_clg,
	draw_plot_rectangle,
	draw_plot_line,
	draw_plot_polygon,
	draw_plot_fill,
	draw_plot_clip,
	draw_plot_text,
	draw_plot_disc,
	draw_plot_bitmap,
	draw_plot_bitmap_tile,
	drawbuf_group_begin,
	drawbuf_group_end
};

static int draw_plot_origin_y = 0; /* plot origin, in browser units */
static int draw_clip_x0, draw_clip_y0, draw_clip_x1, draw_clip_y1;

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
	drawfile_diagram_base *diagram;

	if (c->type != CONTENT_HTML)
		return false;

	box = c->data.html.layout;
	current_width = c->available_width;

	if ((diagram = (drawfile_diagram_base *)drawbuf_claim(sizeof(drawfile_diagram_base), DrawBuf_eHeader)) == NULL)
		goto draw_save_error;

	/* write the Draw diagram */
	memcpy(diagram->tag, "Draw", 4);
	diagram->major_version = 201;
	diagram->minor_version = 0;
	memcpy(diagram->source, "NetSurf     ", 12);

	/* recalculate box widths for an A4 page */
	if (!layout_document(c, A4PAGEWIDTH, A4PAGEWIDTH)) {
		warn_user("NoMemory", 0);
		goto draw_save_error;
	}

	diagram->bbox.x0 = 0;
	diagram->bbox.y0 = 0;
	diagram->bbox.x1 = A4PAGEWIDTH*512;
	diagram->bbox.y1 = A4PAGEHEIGHT*512;

	if (!add_options())
		goto draw_save_error;

	/* set up plotters */
	plot = draw_plotters;
	draw_plot_origin_y = A4PAGEHEIGHT;
	draw_clip_x0 = 0;
	draw_clip_y0 = draw_plot_origin_y - c->height;
	draw_clip_x1 = A4PAGEWIDTH;
	draw_clip_y1 = draw_plot_origin_y;

	current_redraw_browser = NULL;  /* we don't want to save the selection */

	if (!drawbuf_group_begin("page"))
		goto draw_save_error;

	/* redraw page at 100% and a stupidly large clipping rectangle */
	content_redraw(c, 0, 0, c->width, c->height, 0, 0,
			INT_MAX, INT_MAX, 1.0, 0xFFFFFF);

	if (!drawbuf_group_end())
		goto draw_save_error;

	if (!drawbuf_save_file(path))
		goto draw_save_error;

	drawbuf_free();

	/* reset layout to current window width */
	if (!layout_document(c, current_width, current_width)) {
		warn_user("NoMemory", 0);
		return false;
	}

	return true;

draw_save_error:
	drawbuf_free();
	/* attempt to reflow back on failure */
	(void)layout_document(c, current_width, current_width);
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
 * Simulate clearing the graphics window.
 *
 * \param c the colour to clear to
 * \return true on success, false otherwise.
 */
bool draw_plot_clg(colour c)
{
	return draw_plot_fill(draw_clip_x0, draw_clip_y0,
			draw_clip_x1, draw_clip_y1, c);
}

/**
 * Draw a rectangle outline, optionally dotted.
 *
 * \param x0 Left edge of rectangle
 * \param y0 Top edge of rectangle
 * \param width Width of rectangle
 * \param height Height of rectangle
 * \param c colour of outline
 * \param dotted whether outline should be plotted as a dotted line
 * \return true on success, false otherwise.
 */
bool draw_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed)
{
	drawfile_object *dro;
	drawfile_path *dp;
	draw_path_element *dpe;

	if ((dro = (drawfile_object *)drawbuf_claim(8 + 96, DrawBuf_eBody)) == NULL)
		return false;

	dro->type = drawfile_TYPE_PATH;
	dro->size = 8 + 96;

	dp = &dro->data.path;
	dp->bbox.x0 = x0 * 512;
	dp->bbox.y0 = (draw_plot_origin_y - y0 - height) * 512;
	dp->bbox.x1 = (x0 + width) * 512;
	dp->bbox.y1 = (draw_plot_origin_y - y0) * 512;

	/* If saving with the debug rectangles visible, some bbox y
	 * coordinates end up in the wrong order, due to the box height
	 * being negative. we correct that here.
	 */
	if (height < 0) {
		dp->bbox.y0 = (draw_plot_origin_y - y0) * 512;
		dp->bbox.y1 = (draw_plot_origin_y - y0 - height) * 512;
	}

	dp->fill = 0xFFFFFFFF; /* not filled */
	dp->outline = c<<8;
	dp->width = 0;
	dp->style.flags = 0; /**\todo dotted rectangles */

	dpe = (draw_path_element *) (((int *) &dp->path) + 0 / sizeof (int));
	dpe->tag = draw_MOVE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.move_to.x = dp->bbox.x0;
	dpe->data.move_to.y = dp->bbox.y0;

	dpe = (draw_path_element *) (((int *) &dp->path) + 12 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = dp->bbox.x0;
	dpe->data.line_to.y = dp->bbox.y1;

	dpe = (draw_path_element *) (((int *) &dp->path) + 24 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = dp->bbox.x1;
	dpe->data.line_to.y = dp->bbox.y1;

	dpe = (draw_path_element *) (((int *) &dp->path) + 36 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = dp->bbox.x1;
	dpe->data.line_to.y = dp->bbox.y0;

	dpe = (draw_path_element *) (((int *) &dp->path) + 48 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = dp->bbox.x0;
	dpe->data.line_to.y = dp->bbox.y0;

	dpe = (draw_path_element *) (((int *) &dp->path) + 60 / sizeof (int));
	dpe->tag = draw_END_PATH;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;

	return true;
}

/**
 * Add a line to the diagram
 *
 * \param x0,y0 top left coordinate of line
 * \param x1,y1 bottom right coordinate of line
 * \param width width of line
 * \param c the line's colour
 * \param dotted whether to plot dotted
 * \param dashed whether to plot dashed
 * \return  true on success, false on error (error got reported)
 */
bool draw_plot_line(int x0, int y0, int x1, int y1, int width, colour c,
		bool dotted, bool dashed)
{
	drawfile_object *dro;
	drawfile_path *dp;
	draw_path_element *dpe;

	if ((dro = (drawfile_object *)drawbuf_claim(8 + 60, DrawBuf_eBody)) == NULL)
		return false;

	dro->type = drawfile_TYPE_PATH;
	dro->size = 8 + 60;

	dp = &dro->data.path;
	if (x0 < x1) {
		dp->bbox.x0 = x0 * 512;
		dp->bbox.x1 = x1 * 512;
	}
	else {
		dp->bbox.x0 = x1 * 512;
		dp->bbox.x1 = x0 * 512;
	}
	if (y0 < y1) {
		dp->bbox.y0 = (draw_plot_origin_y - y1) * 512;
		dp->bbox.y1 = (draw_plot_origin_y - y0) * 512;
	}
	else {
		dp->bbox.y0 = (draw_plot_origin_y - y0) * 512;
		dp->bbox.y1 = (draw_plot_origin_y - y1) * 512;
	}

	dp->fill = 0xFFFFFFFF; /* do not fill */
	dp->outline = c<<8;
	dp->width = width * 512;
	dp->style.flags = 0; /**\todo dashed/dotted lines*/
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
	dpe->data.line_to.y = dp->bbox.y1;

	/* end */
	dpe = (draw_path_element *) (((int *) &dp->path) + 24 / sizeof (int));
	dpe->tag = draw_END_PATH;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;

	return true;
}

/**
 * Add a filled polygon to the diagram
 *
 * \param p array of coordinate pairs (x,y)
 * \param n number of points
 * \param the fill colour
 * \return true on success, false otherwise
 */
bool draw_plot_polygon(int *p, unsigned int n, colour fill)
{
	drawfile_object *dro;
	drawfile_path *dp;
	draw_path_element *dpe;
	int xmin = A4PAGEWIDTH, ymin = draw_plot_origin_y,
	    xmax = 0, ymax = 0;
	unsigned int i;

	if ((dro = (drawfile_object *)drawbuf_claim(8 + 36 + n * 12, DrawBuf_eBody)) == NULL)
		return false;

	dro->type = drawfile_TYPE_PATH;
	dro->size = 8 + 36 + n * 12;

	dp = &dro->data.path;

	/* bbox filled in at end (as we need to calculate it first) */

	dp->fill = fill<<8;
	dp->outline = 0xFFFFFFFF; /* no outline */
	dp->width = 0;
	dp->style.flags = 0;
	dp->style.reserved = 0;
	dp->style.cap_width = 0;
	dp->style.cap_length = 0;


	for (i = 0; i != n; i++) {
		dpe = (draw_path_element *) (((int *) &dp->path) + (12 * i) / sizeof (int));
		dpe->tag = draw_LINE_TO;
		dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
		dpe->data.line_to.x = p[i*2+0] * 512;
		dpe->data.line_to.y = (draw_plot_origin_y - p[i*2+1]) * 512;

		if (p[i*2+0] < xmin) xmin = p[i*2+0];
		if (p[i*2+0] > xmax) xmax = p[i*2+0];
		if ((draw_plot_origin_y - p[i*2+1]) < ymin)
			ymin = draw_plot_origin_y - p[i*2+1];
		if ((draw_plot_origin_y - p[i*2+1]) > ymax)
			ymax = draw_plot_origin_y - p[i*2+1];
	}

	dpe = (draw_path_element *) (((int *) &dp->path) + 0 / sizeof (int));
	dpe->tag = draw_MOVE_TO;

	dpe = (draw_path_element *) (((int *) &dp->path) + (12 * i) / sizeof (int));
	dpe->tag = draw_END_PATH;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;

	/* fill in bounding box */
	dp->bbox.x0 = xmin * 512;
	dp->bbox.y0 = ymin * 512;
	dp->bbox.x1 = xmax * 512;
	dp->bbox.y1 = ymax * 512;

	return true;
}

/**
 * Add a filled, borderless rectangle to the diagram
 *
 * \param x0,y0 top left coordinate of rectangle
 * \param x1,y1 bottom right coordinate of rectangle
 * \param c the rectangle's colour
 * \return  true on success, false on error (error got reported)
 */
bool draw_plot_fill(int x0, int y0, int x1, int y1, colour c)
{
	drawfile_object *dro;
	drawfile_path *dp;
	draw_path_element *dpe;

	if ((dro = (drawfile_object *)drawbuf_claim(8 + 96, DrawBuf_eBody)) == NULL)
		return false;

	dro->type = drawfile_TYPE_PATH;
	dro->size = 8 + 96;

	dp = &dro->data.path;
	if (x0 < x1) {
		dp->bbox.x0 = x0 * 512;
		dp->bbox.x1 = x1 * 512;
	}
	else {
		dp->bbox.x0 = x1 * 512;
		dp->bbox.x1 = x0 * 512;
	}
	if (y0 < y1) {
		dp->bbox.y0 = (draw_plot_origin_y - y1) * 512;
		dp->bbox.y1 = (draw_plot_origin_y - y0) * 512;
	}
	else {
		dp->bbox.y0 = (draw_plot_origin_y - y0) * 512;
		dp->bbox.y1 = (draw_plot_origin_y - y1) * 512;
	}

	dp->fill = c<<8;
	dp->outline = 0xFFFFFFFF; /* no stroke */
	dp->width = 0;
	dp->style.flags = 0;

	dpe = (draw_path_element *) (((int *) &dp->path) + 0 / sizeof (int));
	dpe->tag = draw_MOVE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.move_to.x = dp->bbox.x0;
	dpe->data.move_to.y = dp->bbox.y0;

	dpe = (draw_path_element *) (((int *) &dp->path) + 12 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = dp->bbox.x0;
	dpe->data.line_to.y = dp->bbox.y1;

	dpe = (draw_path_element *) (((int *) &dp->path) + 24 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = dp->bbox.x1;
	dpe->data.line_to.y = dp->bbox.y1;

	dpe = (draw_path_element *) (((int *) &dp->path) + 36 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = dp->bbox.x1;
	dpe->data.line_to.y = dp->bbox.y0;

	dpe = (draw_path_element *) (((int *) &dp->path) + 48 / sizeof (int));
	dpe->tag = draw_LINE_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.line_to.x = dp->bbox.x0;
	dpe->data.line_to.y = dp->bbox.y0;

	dpe = (draw_path_element *) (((int *) &dp->path) + 60 / sizeof (int));
	dpe->tag = draw_END_PATH;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;

	return true;
}

bool draw_plot_clip(int clip_x0, int clip_y0, int clip_x1, int clip_y1)
{
	draw_clip_x0 = clip_x0;
	draw_clip_y0 = clip_y0;
	draw_clip_x1 = clip_x1;
	draw_clip_y1 = clip_y1;
	return true;
}

/**
 * Add the text line to the diagram.
 *
 * \param x,y top left of area containing text
 * \param font the font data
 * \param text the text to plot
 * \param length the length of the text
 * \param bc the background colour to blend to
 * \param colour the text colour
 * \return  true on success, false on error (error got reported)
 */
bool draw_plot_text(int x, int y, struct css_style *style,
		const char *text, size_t length, colour bc, colour colour)
{
#if 0
	while (length != 0) {
		size_t width, rolength, consumed;
		const char *rofontname, *rotext;
		byte fontIndex;
		drawfile_object *dro;
		drawfile_text *dt;

		nsfont_txtenum(font, text, length,
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
		dt->bbox.x0 = x * 512;
		dt->bbox.y0 = (draw_plot_origin_y - y) * 512 - font->size*40;
		dt->bbox.x1 = (x + width) * 512;
		dt->bbox.y1 = (draw_plot_origin_y - y) * 512;
		dt->fill = colour<<8;
		dt->bg_hint = bc<<8;
		dt->style.font_index = fontIndex;
		dt->style.reserved[0] = dt->style.reserved[1] = dt->style.reserved[2] = 0;
		dt->xsize = font->size * 40;
		dt->ysize = font->size * 40;
		dt->base.x = x * 512;
		dt->base.y = (draw_plot_origin_y - y) * 512;
		strncpy(dt->text, rotext, rolength);
		do {
			dt->text[rolength++] = 0;
		} while (rolength % 4);

		free((void *)rotext);

		/* Go to next chunk : */
		x += width;
		text += consumed;
		length -= consumed;
	}
#endif

	return true;
}

/**
 * Add a filled circle to the diagram.
 *
 * \param x,y the centre of the circle
 * \param radius the circle's radius
 * \param colour the colour of the circle
 * \return  true on success, false on error (error got reported)
 */
bool draw_plot_disc(int x, int y, int radius, colour colour)
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
	dp->bbox.x0 = (x - radius) * 512;
	dp->bbox.y0 = (draw_plot_origin_y - y - radius) * 512;
	dp->bbox.x1 = (x + radius) * 512;
	dp->bbox.y1 = (draw_plot_origin_y - y + radius) * 512;

	kappa = (int)(radius * 4. * (sqrt(2.) - 1.) / 3.); /* ~= 0.5522847498 */

	dp->fill = colour<<8;
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
	dpe->data.move_to.x = (x - radius) * 512;
	dpe->data.move_to.y = (draw_plot_origin_y - y) * 512;

	/* point1->point2 : (point1)(ctrl1)(ctrl2)(point2) */

	/* a->b : (x-r, y)(x-r, y+k)(x-k, y+r)(x, y+r) */
	dpe = (draw_path_element *) (((int *) &dp->path) + 12 / sizeof (int));
	dpe->tag = draw_BEZIER_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.bezier_to[0].x = (x - radius) * 512;
	dpe->data.bezier_to[0].y = (draw_plot_origin_y - y + kappa) * 512;
	dpe->data.bezier_to[1].x = (x - kappa) * 512;
	dpe->data.bezier_to[1].y = (draw_plot_origin_y - y + radius) * 512;
	dpe->data.bezier_to[2].x = x * 512;
	dpe->data.bezier_to[2].y = (draw_plot_origin_y - y + radius) * 512;

	/* b->c : (x, y+r)(x+k, y+r)(x+r, y+k)(x+r, y)*/
	dpe = (draw_path_element *) (((int *) &dp->path) + 40 / sizeof (int));
	dpe->tag = draw_BEZIER_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.bezier_to[0].x = (x + kappa) * 512;
	dpe->data.bezier_to[0].y = (draw_plot_origin_y - y + radius) * 512;
	dpe->data.bezier_to[1].x = (x + radius) * 512;
	dpe->data.bezier_to[1].y = (draw_plot_origin_y - y + kappa) * 512;
	dpe->data.bezier_to[2].x = (x + radius) * 512;
	dpe->data.bezier_to[2].y = (draw_plot_origin_y - y) * 512;

	/* c->d : (x+r, y)(x+r, y-k)(x+k, y-r)(x, y-r) */
	dpe = (draw_path_element *) (((int *) &dp->path) + 68 / sizeof (int));
	dpe->tag = draw_BEZIER_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.bezier_to[0].x = (x + radius) * 512;
	dpe->data.bezier_to[0].y = (draw_plot_origin_y - y - kappa) * 512;
	dpe->data.bezier_to[1].x = (x + kappa) * 512;
	dpe->data.bezier_to[1].y = (draw_plot_origin_y - y - radius) * 512;
	dpe->data.bezier_to[2].x = x * 512;
	dpe->data.bezier_to[2].y = (draw_plot_origin_y - y - radius) * 512;

	/* d->a : (x, y-r)(x-k, y-r)(x-r, y-k)(x-r, y)*/
	dpe = (draw_path_element *) (((int *) &dp->path) + 96 / sizeof (int));
	dpe->tag = draw_BEZIER_TO;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;
	dpe->data.bezier_to[0].x = (x - kappa) * 512;
	dpe->data.bezier_to[0].y = (draw_plot_origin_y - y - radius) * 512;
	dpe->data.bezier_to[1].x = (x - radius) * 512;
	dpe->data.bezier_to[1].y = (draw_plot_origin_y - y - kappa) * 512;
	dpe->data.bezier_to[2].x = (x - radius) * 512;
	dpe->data.bezier_to[2].y = (draw_plot_origin_y - y) * 512;

	/* end */
	dpe = (draw_path_element *) (((int *) &dp->path) + 124 / sizeof (int));
	dpe->tag = draw_END_PATH;
	dpe->reserved[0] = dpe->reserved[1] = dpe->reserved[2] = 0;

	return true;
}

/**
 * Add images to the drawfile.
 *
 * \param x,y top left coordinate of image
 * \param width image width
 * \param height image height
 * \param bitmap the data area containing the image
 * \param bc the background colour behind the image (unused)
 * \return  true on success, false on error (error got reported)
 */
bool draw_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bc)
{
	int sprite_length;
	drawfile_object *dro;
	drawfile_sprite *ds;

	sprite_length = ((osspriteop_header*)((char*)bitmap->sprite_area+bitmap->sprite_area->first))->size;

	if ((dro = (drawfile_object *)drawbuf_claim(8 + 16 + sprite_length, DrawBuf_eBody)) == NULL)
		return false;

	dro->type = drawfile_TYPE_SPRITE;
	dro->size = 8 + 16 + sprite_length;

	ds = &dro->data.sprite;
	ds->bbox.x0 = x * 512;
	ds->bbox.y0 = (draw_plot_origin_y - y - height) * 512;
	ds->bbox.x1 = (x + width) * 512;
	ds->bbox.y1 = (draw_plot_origin_y - y) * 512;

	memcpy((char*)ds+16, (char*)bitmap->sprite_area+bitmap->sprite_area->first, (unsigned)sprite_length);

	return true;
}

bool draw_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y)
{
#if 0
	/* this doesn't work particularly well (needs clipping support in
	 * drawfiles)
	 */
	int cy, cx;

	if (!drawbuf_group_begin("background"))
		return false;

	cy = draw_clip_y1;
	for (; cy >= draw_clip_y0; cy -= height) {
		cx = (x < draw_clip_x0) ? draw_clip_x0 : x;
		for (; cx <= draw_clip_x1; cx += width) {
			if (!draw_plot_bitmap(cx, cy, width, height,
					bitmap, bg))
				return false;
			if (!repeat_x)
				break;
		}
		if (!repeat_y)
			break;
	}

	if (!drawbuf_group_end())
		return false;
#endif
	return true;
}
#endif
