/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Export a content as a DrawFile (implementation).
 */

#include <assert.h>
#include <limits.h>
#include <oslib/draw.h>
#include <oslib/osfile.h>
#include <pencil.h>
#include "netsurf/content/content.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/riscos/bitmap.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/save_draw.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"


static bool ro_save_draw_clg(colour c);
static bool ro_save_draw_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed);
static bool ro_save_draw_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed);
static bool ro_save_draw_polygon(int *p, unsigned int n, colour fill);
static bool ro_save_draw_fill(int x0, int y0, int x1, int y1, colour c);
static bool ro_save_draw_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1);
static bool ro_save_draw_text(int x, int y, struct css_style *style,
		const char *text, size_t length, colour bg, colour c);
static bool ro_save_draw_disc(int x, int y, int radius, colour colour);
static bool ro_save_draw_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg);
static bool ro_save_draw_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y);
static bool ro_save_draw_group_start(const char *name);
static bool ro_save_draw_group_end(void);
static bool ro_save_draw_error(pencil_code code);


const struct plotter_table ro_save_draw_plotters = {
	ro_save_draw_clg,
	ro_save_draw_rectangle,
	ro_save_draw_line,
	ro_save_draw_polygon,
	ro_save_draw_fill,
	ro_save_draw_clip,
	ro_save_draw_text,
	ro_save_draw_disc,
	ro_save_draw_bitmap,
	ro_save_draw_bitmap_tile,
	ro_save_draw_group_start,
	ro_save_draw_group_end
};

struct pencil_diagram *ro_save_draw_diagram;
int ro_save_draw_width;
int ro_save_draw_height;


/**
 * Export a content as a DrawFile.
 *
 * \param  c     content to export
 * \param  path  path to save DrawFile as
 * \return  true on success, false on error and error reported
 */

bool save_as_draw(struct content *c, const char *path)
{
	pencil_code code;
	char *drawfile_buffer;
	size_t drawfile_size;
	os_error *error;

	ro_save_draw_diagram = pencil_create();
	if (!ro_save_draw_diagram) {
		warn_user("NoMemory", 0);
		return false;
	}

	ro_save_draw_width = c->width;
	ro_save_draw_height = c->height;

	plot = ro_save_draw_plotters;
	if (!content_redraw(c, 0, -c->height,
			c->width, c->height,
			INT_MIN, INT_MIN, INT_MAX, INT_MAX,
			1,
			0xFFFFFF))
	{
		pencil_free(ro_save_draw_diagram);
		return false;
	}

	/*pencil_dump(ro_save_draw_diagram);*/

	code = pencil_save_drawfile(ro_save_draw_diagram, "NetSurf",
			&drawfile_buffer, &drawfile_size);
	if (code != pencil_OK) {
		warn_user("SaveError", 0);
		pencil_free(ro_save_draw_diagram);
		return false;
	}
	assert(drawfile_buffer);

	error = xosfile_save_stamped(path, osfile_TYPE_DRAW,
			drawfile_buffer, drawfile_buffer + drawfile_size);
	if (error) {
		LOG(("xosfile_save_stamped failed: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		pencil_free(ro_save_draw_diagram);
		return false;
	}

	pencil_free(ro_save_draw_diagram);

	return true;
}


bool ro_save_draw_clg(colour c)
{
	return ro_save_draw_fill(0, 0, ro_save_draw_width, ro_save_draw_height,
			c);
}


bool ro_save_draw_rectangle(int x0, int y0, int width, int height,
		int line_width, colour c, bool dotted, bool dashed)
{
	pencil_code code;
	const int path[] = { draw_MOVE_TO, x0 * 2, -y0 * 2 - 1,
			draw_LINE_TO, (x0 + width) * 2, -y0 * 2 - 1,
			draw_LINE_TO, (x0 + width) * 2, -(y0 + height) * 2 - 1,
			draw_LINE_TO, x0 * 2, -(y0 + height) * 2 - 1,
			draw_CLOSE_LINE, x0 * 2, -y0 * 2 - 1,
			draw_END_PATH };

	code = pencil_path(ro_save_draw_diagram, path,
			sizeof path / sizeof path[0],
			pencil_TRANSPARENT, c << 8, width, pencil_JOIN_MITRED,
			pencil_CAP_BUTT, pencil_CAP_BUTT, 0, 0, false,
			pencil_SOLID);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


bool ro_save_draw_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed)
{
	pencil_code code;
	const int path[] = { draw_MOVE_TO, x0 * 2, -y0 * 2 - 1,
			draw_LINE_TO, x1 * 2, -y1 * 2 - 1,
			draw_END_PATH };

	code = pencil_path(ro_save_draw_diagram, path,
			sizeof path / sizeof path[0],
			pencil_TRANSPARENT, c << 8, width, pencil_JOIN_MITRED,
			pencil_CAP_BUTT, pencil_CAP_BUTT, 0, 0, false,
			pencil_SOLID);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


bool ro_save_draw_polygon(int *p, unsigned int n, colour fill)
{
	pencil_code code;
	int path[n * 3 + 1];
	unsigned int i;

	for (i = 0; i != n; i++) {
		path[i * 3 + 0] = draw_LINE_TO;
		path[i * 3 + 1] = p[i * 2 + 0] * 2;
		path[i * 3 + 2] = -p[i * 2 + 1] * 2;
	}
	path[0] = draw_MOVE_TO;
	path[n * 3] = draw_END_PATH;

	code = pencil_path(ro_save_draw_diagram, path, n * 3 + 1,
			fill << 8, pencil_TRANSPARENT, 0, pencil_JOIN_MITRED,
			pencil_CAP_BUTT, pencil_CAP_BUTT, 0, 0, false,
			pencil_SOLID);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


bool ro_save_draw_fill(int x0, int y0, int x1, int y1, colour c)
{
	pencil_code code;
	const int path[] = { draw_MOVE_TO, x0 * 2, -y0 * 2 - 1,
			draw_LINE_TO, x1 * 2, -y0 * 2 - 1,
			draw_LINE_TO, x1 * 2, -y1 * 2 - 1,
			draw_LINE_TO, x0 * 2, -y1 * 2 - 1,
			draw_CLOSE_LINE,
			draw_END_PATH };

	code = pencil_path(ro_save_draw_diagram, path,
			sizeof path / sizeof path[0],
			c << 8, pencil_TRANSPARENT, 0, pencil_JOIN_MITRED,
			pencil_CAP_BUTT, pencil_CAP_BUTT, 0, 0, false,
			pencil_SOLID);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


bool ro_save_draw_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1)
{
	return true;
}


bool ro_save_draw_text(int x, int y, struct css_style *style,
		const char *text, size_t length, colour bg, colour c)
{
	pencil_code code;
	const char *font_family;
	unsigned int font_size;
	rufl_style font_style;

	nsfont_read_style(style, &font_family, &font_size, &font_style);

	code = pencil_text(ro_save_draw_diagram, x * 2, -y * 2, font_family,
			font_style, font_size, text, length, c << 8);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


bool ro_save_draw_disc(int x, int y, int radius, colour colour)
{
	return true;
}


bool ro_save_draw_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg)
{
	pencil_code code;

	code = pencil_sprite(ro_save_draw_diagram, x * 2, (-y - height) * 2,
			width * 2, height * 2,
			((char *) bitmap->sprite_area) +
			bitmap->sprite_area->first);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


bool ro_save_draw_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y)
{
	return true;
}


bool ro_save_draw_group_start(const char *name)
{
	pencil_code code;

	code = pencil_group_start(ro_save_draw_diagram, name);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


bool ro_save_draw_group_end(void)
{
	pencil_code code;

	code = pencil_group_end(ro_save_draw_diagram);
	if (code != pencil_OK)
		return ro_save_draw_error(code);

	return true;
}


/**
 * Report an error from pencil.
 *
 * \param  code  error code
 * \return  false
 */

bool ro_save_draw_error(pencil_code code)
{
	LOG(("code %i", code));

	switch (code) {
	case pencil_OK:
		assert(0);
		break;
	case pencil_OUT_OF_MEMORY:
		warn_user("NoMemory", 0);
		break;
	case pencil_FONT_MANAGER_ERROR:
		warn_user("SaveError", rufl_fm_error->errmess);
		break;
	case pencil_FONT_NOT_FOUND:
	case pencil_IO_ERROR:
	case pencil_IO_EOF:
		warn_user("SaveError", "generating the DrawFile failed");
		break;
	}

	return false;
}
