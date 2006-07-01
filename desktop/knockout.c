/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Richard Wilson <info@tinct.net>
 */

/** \file
 * Knockout rendering (implementation).
 */

#include <assert.h>
#include <string.h>
#include "netsurf/desktop/knockout.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/image/bitmap.h"
#include "netsurf/utils/log.h"


#define KNOCKOUT_BOXES 512	/* 28 bytes each */
#define KNOCKOUT_ENTRIES 4096	/* 40 bytes each */
#define KNOCKOUT_POLYGONS 512	/* 4 bytes each */

struct knockout_box;
struct knockout_entry;

static void knockout_calculate(int x0, int y0, int x1, int y1, struct knockout_box *box);
static bool knockout_plot_fill_recursive(struct knockout_box *box, colour c);
static bool knockout_plot_bitmap_tile_recursive(struct knockout_box *box,
		struct knockout_entry *entry);

static bool knockout_plot_clg(colour c);
static bool knockout_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed);
static bool knockout_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed);
static bool knockout_plot_polygon(int *p, unsigned int n, colour fill);
static bool knockout_plot_fill(int x0, int y0, int x1, int y1, colour c);
static bool knockout_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1);
static bool knockout_plot_text(int x, int y, struct css_style *style,
		const char *text, size_t length, colour bg, colour c);
static bool knockout_plot_disc(int x, int y, int radius, colour colour, bool filled);
static bool knockout_plot_arc(int x, int y, int radius, int angle1, int angle2,
		colour c);
static bool knockout_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg);
static bool knockout_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y);
static bool knockout_plot_group_start(const char *name);
static bool knockout_plot_group_end(void);


struct knockout_entry knockout_entries[KNOCKOUT_ENTRIES];
struct knockout_box knockout_boxes[KNOCKOUT_BOXES];
int knockout_polygons[KNOCKOUT_POLYGONS];
int knockout_entry_cur = 0;
int knockout_box_cur = 0;
int knockout_polygon_cur = 0;
struct knockout_box *knockout_list = NULL;

struct plotter_table real_plot;

int clip_x0_cur;
int clip_y0_cur;
int clip_x1_cur;
int clip_y1_cur;


const struct plotter_table knockout_plotters = {
	knockout_plot_clg,
	knockout_plot_rectangle,
	knockout_plot_line,
	knockout_plot_polygon,
	knockout_plot_fill,
	knockout_plot_clip,
	knockout_plot_text,
	knockout_plot_disc,
	knockout_plot_arc,
	knockout_plot_bitmap,
	knockout_plot_bitmap_tile,
	knockout_plot_group_start,
	knockout_plot_group_end
};


typedef enum {
	KNOCKOUT_PLOT_CLG,		/* translated to _FILL */
	KNOCKOUT_PLOT_RECTANGLE,
	KNOCKOUT_PLOT_LINE,
	KNOCKOUT_PLOT_POLYGON,
	KNOCKOUT_PLOT_FILL,		/* knockout, knocked out */
	KNOCKOUT_PLOT_CLIP,
	KNOCKOUT_PLOT_TEXT,
	KNOCKOUT_PLOT_DISC,
	KNOCKOUT_PLOT_ARC,
	KNOCKOUT_PLOT_BITMAP,		/* knockout */
	KNOCKOUT_PLOT_BITMAP_TILE,	/* knockout, knocked out */
	KNOCKOUT_PLOT_GROUP_START,
	KNOCKOUT_PLOT_GROUP_END,
} knockout_type;


struct knockout_box {
	struct {
		int x0;
		int y0;
		int x1;
		int y1;
	} bbox;
	bool deleted;			/* box has been totally knocked out */
	struct knockout_box *child;
	struct knockout_box *next;
};


struct knockout_entry {
	knockout_type type;
	struct knockout_box *box;	/* relating series of knockout clips */
	union {
		struct {
			colour c;
		} clg;
		struct {
			int x0;
			int y0;
			int width;
			int height;
			int line_width;
			colour c;
			bool dotted;
			bool dashed;
		} rectangle;
		struct {
			int x0;
			int y0;
			int x1;
			int y1;
			int width;
			colour c;
			bool dotted;
			bool dashed;
		} line;
		struct {
			int *p;
			unsigned int n;
			colour fill;
		} polygon;
		struct {
			int x0;
			int y0;
			int x1;
			int y1;
			colour c;
		} fill;
		struct {
			int x0;
			int y0;
			int x1;
			int y1;
		} clip;
		struct {
			int x;
			int y;
			struct css_style *style;
			const char *text;
			size_t length;
			colour bg;
			colour c;
		} text;
		struct {
			int x;
			int y;
			int radius;
			colour colour;
			bool filled;
		} disc;
		struct {
			int x;
			int y;
			int radius;
			int angle1;
			int angle2;
			colour c;
		} arc;
		struct {
			int x;
			int y;
			int width;
			int height;
			struct bitmap *bitmap;
			colour bg;
		} bitmap;
		struct {
			int x;
			int y;
			int width;
			int height;
			struct bitmap *bitmap;
			colour bg;
			bool repeat_x;
			bool repeat_y;
		} bitmap_tile;
		struct {
			const char *name;
		} group_start;
	} data;
};


/**
 * Start a knockout plotting session
 *
 * \param  plotter  the plotter to use
 * \return  true on success, false otherwise
 */
bool knockout_plot_start(struct plotter_table *plotter)
{
	/* end any previous sessions */
	if (knockout_entry_cur > 0)
		knockout_plot_end();

	/* take over the plotter */
	real_plot = *plotter;
	plot = knockout_plotters;
	return true;
}


/**
 * End a knockout plotting session
 *
 * \return  true on success, false otherwise
 */
bool knockout_plot_end(void)
{
	int i;
	bool success = true;
	struct knockout_box *box;

	/* release our plotter */
	plot = real_plot;

	for (i = 0; i < knockout_entry_cur; i++) {
		switch (knockout_entries[i].type) {
			case KNOCKOUT_PLOT_CLG:
				success &= plot.clg(
						knockout_entries[i].data.clg.c);
				break;
			case KNOCKOUT_PLOT_RECTANGLE:
				success &= plot.rectangle(
						knockout_entries[i].data.rectangle.x0,
						knockout_entries[i].data.rectangle.y0,
						knockout_entries[i].data.rectangle.width,
						knockout_entries[i].data.rectangle.height,
						knockout_entries[i].data.rectangle.line_width,
						knockout_entries[i].data.rectangle.c,
						knockout_entries[i].data.rectangle.dotted,
						knockout_entries[i].data.rectangle.dashed);
				break;
			case KNOCKOUT_PLOT_LINE:
				success &= plot.line(
						knockout_entries[i].data.line.x0,
						knockout_entries[i].data.line.y0,
						knockout_entries[i].data.line.x1,
						knockout_entries[i].data.line.y1,
						knockout_entries[i].data.line.width,
						knockout_entries[i].data.line.c,
						knockout_entries[i].data.line.dotted,
						knockout_entries[i].data.line.dashed);
				break;
			case KNOCKOUT_PLOT_POLYGON:
				success &= plot.polygon(
						knockout_entries[i].data.polygon.p,
						knockout_entries[i].data.polygon.n,
						knockout_entries[i].data.polygon.fill);
				break;
			case KNOCKOUT_PLOT_FILL:
				box = knockout_entries[i].box->child;
				if (box)
					success &= knockout_plot_fill_recursive(box,
							knockout_entries[i].data.fill.c);
				else if (!knockout_entries[i].box->deleted)
					success &= plot.fill(
							knockout_entries[i].data.fill.x0,
							knockout_entries[i].data.fill.y0,
							knockout_entries[i].data.fill.x1,
							knockout_entries[i].data.fill.y1,
							knockout_entries[i].data.fill.c);
				break;
			case KNOCKOUT_PLOT_CLIP:
				success &= plot.clip(
						knockout_entries[i].data.clip.x0,
						knockout_entries[i].data.clip.y0,
						knockout_entries[i].data.clip.x1,
						knockout_entries[i].data.clip.y1);
				break;
			case KNOCKOUT_PLOT_TEXT:
				success &= plot.text(
						knockout_entries[i].data.text.x,
						knockout_entries[i].data.text.y,
						knockout_entries[i].data.text.style,
						knockout_entries[i].data.text.text,
						knockout_entries[i].data.text.length,
						knockout_entries[i].data.text.bg,
						knockout_entries[i].data.text.c);
				break;
			case KNOCKOUT_PLOT_DISC:
				success &= plot.disc(
						knockout_entries[i].data.disc.x,
						knockout_entries[i].data.disc.y,
						knockout_entries[i].data.disc.radius,
						knockout_entries[i].data.disc.colour,
						knockout_entries[i].data.disc.filled);
				break;
			case KNOCKOUT_PLOT_ARC:
				success &= plot.arc(
						knockout_entries[i].data.arc.x,
						knockout_entries[i].data.arc.y,
						knockout_entries[i].data.arc.radius,
						knockout_entries[i].data.arc.angle1,
						knockout_entries[i].data.arc.angle2,
						knockout_entries[i].data.arc.c);
				break;
			case KNOCKOUT_PLOT_BITMAP:
				success &= plot.bitmap(
						knockout_entries[i].data.bitmap.x,
						knockout_entries[i].data.bitmap.y,
						knockout_entries[i].data.bitmap.width,
						knockout_entries[i].data.bitmap.height,
						knockout_entries[i].data.bitmap.bitmap,
						knockout_entries[i].data.bitmap.bg);
				break;
			case KNOCKOUT_PLOT_BITMAP_TILE:
				box = knockout_entries[i].box->child;
				if (box) {
					success &= knockout_plot_bitmap_tile_recursive(box,
							&knockout_entries[i]);
					success &= plot.clip(knockout_entries[i].box->bbox.x0,
							knockout_entries[i].box->bbox.y0,
							knockout_entries[i].box->bbox.x1,
							knockout_entries[i].box->bbox.y1);
				} else if (!knockout_entries[i].box->deleted) {
					success &= plot.bitmap_tile(
							knockout_entries[i].data.
									bitmap_tile.x,
							knockout_entries[i].data.
									bitmap_tile.y,
							knockout_entries[i].data.
									bitmap_tile.width,
							knockout_entries[i].data.
									bitmap_tile.height,
							knockout_entries[i].data.
									bitmap_tile.bitmap,
							knockout_entries[i].data.
									bitmap_tile.bg,
							knockout_entries[i].data.
									bitmap_tile.repeat_x,
							knockout_entries[i].data.
									bitmap_tile.repeat_y);
				}
				break;
			case KNOCKOUT_PLOT_GROUP_START:
				success &= plot.group_start(
						knockout_entries[i].data.group_start.name);
				break;
			case KNOCKOUT_PLOT_GROUP_END:
				success &= plot.group_end();
				break;
		}
	}

	knockout_entry_cur = 0;
	knockout_box_cur = 0;
	knockout_polygon_cur = 0;
	knockout_list = NULL;
	return success;
}


/**
 * Knockout a section of previous rendering
 *
 * \param  x0	the left edge of the removal box
 * \param  y0	the bottom edge of the removal box
 * \param  x1	the right edge of the removal box
 * \param  y1	the top edge of the removal box
 * \param  box  the current box set to consider
*/
void knockout_calculate(int x0, int y0, int x1, int y1, struct knockout_box *box)
{
	struct knockout_box *parent;
	int nx0, ny0, nx1, ny1;

	for (parent = box; parent; parent = parent->next) {
		if (parent->deleted)
			continue;
		/* reject non-overlapping boxes */
		if ((parent->bbox.x0 >= x1) ||
				(parent->bbox.x1 <= x0) ||
				(parent->bbox.y0 >= y1) ||
				(parent->bbox.y1 <= y0))
			continue;
		/* has the box been replaced by children? */
		if (parent->child) {
			knockout_calculate(x0, y0, x1, y1, parent->child);
		} else {
		  	nx0 = parent->bbox.x0;
		  	ny0 = parent->bbox.y0;
		  	nx1 = parent->bbox.x1;
		  	ny1 = parent->bbox.y1;

			/* check for a total knockout */
			if ((x0 <= nx0) && (x1 >= nx1) && (y0 <= ny0) && (y1 >= ny1)) {
				parent->deleted = true;
				continue;
			}

			/* we need a maximum of 4 child boxes */
			if (knockout_box_cur + 4 >= KNOCKOUT_ENTRIES) {
				knockout_plot_start(&real_plot);
				return;
			}

			/* clip top */
			if (y1 < ny1) {
			  	knockout_boxes[knockout_box_cur].bbox.x0 = nx0;
			  	knockout_boxes[knockout_box_cur].bbox.y0 = y1;
			  	knockout_boxes[knockout_box_cur].bbox.x1 = nx1;
			  	knockout_boxes[knockout_box_cur].bbox.y1 = ny1;
			  	knockout_boxes[knockout_box_cur].deleted = false;
			  	knockout_boxes[knockout_box_cur].child = NULL;
			  	knockout_boxes[knockout_box_cur].next = parent->child;
				parent->child = &knockout_boxes[knockout_box_cur++];
				ny1 = y1;
			}
			/* clip bottom */
			if (y0 > ny0) {
			  	knockout_boxes[knockout_box_cur].bbox.x0 = nx0;
			  	knockout_boxes[knockout_box_cur].bbox.y0 = ny0;
			  	knockout_boxes[knockout_box_cur].bbox.x1 = nx1;
			  	knockout_boxes[knockout_box_cur].bbox.y1 = y0;
			  	knockout_boxes[knockout_box_cur].deleted = false;
			  	knockout_boxes[knockout_box_cur].child = NULL;
			  	knockout_boxes[knockout_box_cur].next = parent->child;
				parent->child = &knockout_boxes[knockout_box_cur++];
				ny0 = y0;
			}
			/* clip right */
			if (x1 < nx1) {
			  	knockout_boxes[knockout_box_cur].bbox.x0 = x1;
			  	knockout_boxes[knockout_box_cur].bbox.y0 = ny0;
			  	knockout_boxes[knockout_box_cur].bbox.x1 = nx1;
			  	knockout_boxes[knockout_box_cur].bbox.y1 = ny1;
			  	knockout_boxes[knockout_box_cur].deleted = false;
			  	knockout_boxes[knockout_box_cur].child = NULL;
			  	knockout_boxes[knockout_box_cur].next = parent->child;
				parent->child = &knockout_boxes[knockout_box_cur++];
				nx1 = x1;
			}
			/* clip left */
			if (x0 > nx0) {
			  	knockout_boxes[knockout_box_cur].bbox.x0 = nx0;
			  	knockout_boxes[knockout_box_cur].bbox.y0 = ny0;
			  	knockout_boxes[knockout_box_cur].bbox.x1 = x0;
			  	knockout_boxes[knockout_box_cur].bbox.y1 = ny1;
			  	knockout_boxes[knockout_box_cur].deleted = false;
			  	knockout_boxes[knockout_box_cur].child = NULL;
			  	knockout_boxes[knockout_box_cur].next = parent->child;
				parent->child = &knockout_boxes[knockout_box_cur++];
				//nx0 = x0;
			}
		}
	}
}


bool knockout_plot_fill_recursive(struct knockout_box *box, colour c)
{
	bool success = true;
	struct knockout_box *parent;

	for (parent = box; parent; parent = parent->next) {
		if (parent->deleted)
			continue;
		if (parent->child)
			knockout_plot_fill_recursive(parent->child, c);
		else
			success &= plot.fill(parent->bbox.x0,
					parent->bbox.y0,
					parent->bbox.x1,
					parent->bbox.y1,
					c);
	}
	return success;
}


bool knockout_plot_bitmap_tile_recursive(struct knockout_box *box,
		struct knockout_entry *entry)
{
	bool success = true;
	struct knockout_box *parent;

	for (parent = box; parent; parent = parent->next) {
		if (parent->deleted)
			continue;
		if (parent->child)
			knockout_plot_bitmap_tile_recursive(parent->child, entry);
		else {
			success &= plot.clip(parent->bbox.x0,
					parent->bbox.y0,
					parent->bbox.x1,
					parent->bbox.y1);
			success &= plot.bitmap_tile(entry->data.bitmap_tile.x,
					entry->data.bitmap_tile.y,
					entry->data.bitmap_tile.width,
					entry->data.bitmap_tile.height,
					entry->data.bitmap_tile.bitmap,
					entry->data.bitmap_tile.bg,
					entry->data.bitmap_tile.repeat_x,
					entry->data.bitmap_tile.repeat_y);
		}
	}
	return success;
}

bool knockout_plot_clg(colour c)
{
	return knockout_plot_fill(clip_x0_cur, clip_y0_cur, clip_x1_cur, clip_y1_cur, c);
}


bool knockout_plot_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed)
{
	knockout_entries[knockout_entry_cur].data.rectangle.x0 = x0;
	knockout_entries[knockout_entry_cur].data.rectangle.y0 = y0;
	knockout_entries[knockout_entry_cur].data.rectangle.width = width;
	knockout_entries[knockout_entry_cur].data.rectangle.height = height;
	knockout_entries[knockout_entry_cur].data.rectangle.line_width = line_width;
	knockout_entries[knockout_entry_cur].data.rectangle.c = c;
	knockout_entries[knockout_entry_cur].data.rectangle.dotted = dotted;
	knockout_entries[knockout_entry_cur].data.rectangle.dashed = dashed;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_RECTANGLE;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_start(&real_plot);
	return true;
}


bool knockout_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed)
{
	knockout_entries[knockout_entry_cur].data.line.x0 = x0;
	knockout_entries[knockout_entry_cur].data.line.y0 = y0;
	knockout_entries[knockout_entry_cur].data.line.x1 = x1;
	knockout_entries[knockout_entry_cur].data.line.y1 = y1;
	knockout_entries[knockout_entry_cur].data.line.width = width;
	knockout_entries[knockout_entry_cur].data.line.c = c;
	knockout_entries[knockout_entry_cur].data.line.dotted = dotted;
	knockout_entries[knockout_entry_cur].data.line.dashed = dashed;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_LINE;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_start(&real_plot);
	return true;
}


bool knockout_plot_polygon(int *p, unsigned int n, colour fill)
{
  	bool success = true;
  	int *dest;

	/* ensure we have sufficient room even when flushed */
	if (n * 2 >= KNOCKOUT_POLYGONS) {
		knockout_plot_end();
		success = plot.polygon(p, n, fill);
		knockout_plot_start(&real_plot);
		return success;
	}
	
	/* ensure we have enough room right now */
	if (knockout_polygon_cur + n * 2 >= KNOCKOUT_POLYGONS)
		knockout_plot_start(&real_plot);
	
	/* copy our data */
	dest = &(knockout_polygons[knockout_polygon_cur]);
	memcpy(dest, p, n * 2 * sizeof(int));
	knockout_polygon_cur += n * 2;
	knockout_entries[knockout_entry_cur].data.polygon.p = dest;
	knockout_entries[knockout_entry_cur].data.polygon.n = n;
	knockout_entries[knockout_entry_cur].data.polygon.fill = fill;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_POLYGON;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_start(&real_plot);
	return true;
}


bool knockout_plot_fill(int x0, int y0, int x1, int y1, colour c)
{
	/* fills both knock out and get knocked out */
	knockout_calculate(x0, y0, x1, y1, knockout_list);
	knockout_boxes[knockout_box_cur].bbox.x0 = x0;
	knockout_boxes[knockout_box_cur].bbox.y0 = y0;
	knockout_boxes[knockout_box_cur].bbox.x1 = x1;
	knockout_boxes[knockout_box_cur].bbox.y1 = y1;
	knockout_boxes[knockout_box_cur].deleted = false;
	knockout_boxes[knockout_box_cur].child = NULL;
	knockout_boxes[knockout_box_cur].next = knockout_list;
	knockout_list = &knockout_boxes[knockout_box_cur];
	knockout_entries[knockout_entry_cur].box = &knockout_boxes[knockout_box_cur];
	knockout_entries[knockout_entry_cur].data.fill.x0 = x0;
	knockout_entries[knockout_entry_cur].data.fill.y0 = y0;
	knockout_entries[knockout_entry_cur].data.fill.x1 = x1;
	knockout_entries[knockout_entry_cur].data.fill.y1 = y1;
	knockout_entries[knockout_entry_cur].data.fill.c = c;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_FILL;
	if ((++knockout_entry_cur >= KNOCKOUT_ENTRIES) ||
			(++knockout_box_cur >= KNOCKOUT_BOXES))
		knockout_plot_start(&real_plot);
	return true;
}


bool knockout_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1)
{
	if (clip_x1 < clip_x0 || clip_y0 > clip_y1) {
		LOG(("bad clip rectangle %i %i %i %i",
				clip_x0, clip_y0, clip_x1, clip_y1));
		return false;
	}

	/* memorise clip for bitmap tiling */
	clip_x0_cur = clip_x0;
	clip_y0_cur = clip_y0;
	clip_x1_cur = clip_x1;
	clip_y1_cur = clip_y1;

	knockout_entries[knockout_entry_cur].data.clip.x0 = clip_x0;
	knockout_entries[knockout_entry_cur].data.clip.y0 = clip_y0;
	knockout_entries[knockout_entry_cur].data.clip.x1 = clip_x1;
	knockout_entries[knockout_entry_cur].data.clip.y1 = clip_y1;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_CLIP;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_start(&real_plot);
	return true;
}


bool knockout_plot_text(int x, int y, struct css_style *style,
		const char *text, size_t length, colour bg, colour c)
{
	knockout_entries[knockout_entry_cur].data.text.x = x;
	knockout_entries[knockout_entry_cur].data.text.y = y;
	knockout_entries[knockout_entry_cur].data.text.style = style;
	knockout_entries[knockout_entry_cur].data.text.text = text;
	knockout_entries[knockout_entry_cur].data.text.length = length;
	knockout_entries[knockout_entry_cur].data.text.bg = bg;
	knockout_entries[knockout_entry_cur].data.text.c = c;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_TEXT;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_start(&real_plot);
	return true;
}


bool knockout_plot_disc(int x, int y, int radius, colour colour, bool filled)
{
	knockout_entries[knockout_entry_cur].data.disc.x = x;
	knockout_entries[knockout_entry_cur].data.disc.y = y;
	knockout_entries[knockout_entry_cur].data.disc.radius = radius;
	knockout_entries[knockout_entry_cur].data.disc.colour = colour;
	knockout_entries[knockout_entry_cur].data.disc.filled = filled;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_DISC;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_start(&real_plot);
	return true;
}

bool knockout_plot_arc(int x, int y, int radius, int angle1, int angle2, colour c)
{
	knockout_entries[knockout_entry_cur].data.arc.x = x;
	knockout_entries[knockout_entry_cur].data.arc.y = y;
	knockout_entries[knockout_entry_cur].data.arc.radius = radius;
	knockout_entries[knockout_entry_cur].data.arc.angle1 = angle1;
	knockout_entries[knockout_entry_cur].data.arc.angle2 = angle2;
	knockout_entries[knockout_entry_cur].data.arc.c = c;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_ARC;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_start(&real_plot);
	return true;
}

bool knockout_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg)
{
	/* opaque bitmaps knockout, but don't get knocked out */
	if (bitmap_get_opaque(bitmap))
		knockout_calculate(x, y, x + width, y + height, knockout_list);
	knockout_entries[knockout_entry_cur].data.bitmap.x = x;
	knockout_entries[knockout_entry_cur].data.bitmap.x = x;
	knockout_entries[knockout_entry_cur].data.bitmap.y = y;
	knockout_entries[knockout_entry_cur].data.bitmap.width = width;
	knockout_entries[knockout_entry_cur].data.bitmap.height = height;
	knockout_entries[knockout_entry_cur].data.bitmap.bitmap = bitmap;
	knockout_entries[knockout_entry_cur].data.bitmap.bg = bg;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_BITMAP;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_start(&real_plot);
	return true;
}


bool knockout_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y)
{
	/* tiled bitmaps both knock out and get knocked out */
	if (bitmap_get_opaque(bitmap)) {
	  	if ((repeat_x && repeat_y) ||
	  			/* horizontally repeating, full height */
				(repeat_x && (y <= clip_y0_cur) &&
					(y + height >= clip_y1_cur)) ||
				/* vertically repeating, full width */
				(repeat_y && (x <= clip_x0_cur) &&
					(x + width >= clip_x1_cur)) ||
				/* no repeat, full width & height */
				((y <= clip_y0_cur) && (y + height >= clip_y1_cur) &&
					(x <= clip_x0_cur) && (x + width >= clip_x1_cur)))
			knockout_calculate(clip_x0_cur, clip_y0_cur,
					clip_x1_cur, clip_y1_cur, knockout_list);
	}
	knockout_boxes[knockout_box_cur].bbox.x0 = clip_x0_cur;
	knockout_boxes[knockout_box_cur].bbox.y0 = clip_y0_cur;
	knockout_boxes[knockout_box_cur].bbox.x1 = clip_x1_cur;
	knockout_boxes[knockout_box_cur].bbox.y1 = clip_y1_cur;
	knockout_boxes[knockout_box_cur].deleted = false;
	knockout_boxes[knockout_box_cur].child = NULL;
	knockout_boxes[knockout_box_cur].next = knockout_list;
	knockout_list = &knockout_boxes[knockout_box_cur];
	knockout_entries[knockout_entry_cur].box = &knockout_boxes[knockout_box_cur];
	knockout_entries[knockout_entry_cur].data.bitmap_tile.x = x;
	knockout_entries[knockout_entry_cur].data.bitmap_tile.y = y;
	knockout_entries[knockout_entry_cur].data.bitmap_tile.width = width;
	knockout_entries[knockout_entry_cur].data.bitmap_tile.height = height;
	knockout_entries[knockout_entry_cur].data.bitmap_tile.bitmap = bitmap;
	knockout_entries[knockout_entry_cur].data.bitmap_tile.bg = bg;
	knockout_entries[knockout_entry_cur].data.bitmap_tile.repeat_x = repeat_x;
	knockout_entries[knockout_entry_cur].data.bitmap_tile.repeat_y = repeat_y;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_BITMAP_TILE;
	if ((++knockout_entry_cur >= KNOCKOUT_ENTRIES) ||
			(++knockout_box_cur >= KNOCKOUT_BOXES))
		knockout_plot_start(&real_plot);
	return true;
}

bool knockout_plot_group_start(const char *name)
{
	knockout_entries[knockout_entry_cur].data.group_start.name = name;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_GROUP_START;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_start(&real_plot);
	return true;
}

bool knockout_plot_group_end(void)
{
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_GROUP_END;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_start(&real_plot);
	return true;
}
