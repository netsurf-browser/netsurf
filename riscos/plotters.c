/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * Target independent plotting (RISC OS screen implementation).
 */

#include "oslib/colourtrans.h"
#include "oslib/draw.h"
#include "oslib/os.h"
#include "netsurf/desktop/plotters.h"
#include "netsurf/render/font.h"
#include "netsurf/riscos/bitmap.h"
#include "netsurf/riscos/image.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/utils/log.h"


static bool ro_plot_clg(colour c);
static bool ro_plot_rectangle(int x0, int y0, int width, int height,
		colour c, bool dotted);
static bool ro_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed);
static bool ro_plot_polygon(int *p, unsigned int n, colour fill);
static bool ro_plot_fill(int x0, int y0, int x1, int y1, colour c);
static bool ro_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1);
static bool ro_plot_text(int x, int y, struct font_data *font,
		const char *text, size_t length, colour bg, colour c);
static bool ro_plot_disc(int x, int y, int radius, colour colour);
static bool ro_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg);
static bool ro_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y);


struct plotter_table plot;

const struct plotter_table ro_plotters = {
	ro_plot_clg,
	ro_plot_rectangle,
	ro_plot_line,
	ro_plot_polygon,
	ro_plot_fill,
	ro_plot_clip,
	ro_plot_text,
	ro_plot_disc,
	ro_plot_bitmap,
	ro_plot_bitmap_tile
};

int ro_plot_origin_x = 0;
int ro_plot_origin_y = 0;

os_trfm ro_plot_trfm = { {
		{ 0x10000, 0 },
		{ 0, 0x10000, },
		{ 0, 0 } } };


bool ro_plot_clg(colour c)
{
	os_error *error;
	error = xcolourtrans_set_gcol(c << 8,
			colourtrans_SET_BG | colourtrans_USE_ECFS,
			os_ACTION_OVERWRITE, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	error = xos_clg();
	if (error) {
		LOG(("xos_clg: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	return true;
}


bool ro_plot_rectangle(int x0, int y0, int width, int height,
		colour c, bool dotted)
{
	os_plot_code plot_code = (dotted ? os_PLOT_DOTTED : os_PLOT_SOLID) |
			os_PLOT_BY;
	os_error *error;

	error = xcolourtrans_set_gcol(c << 8, 0, os_ACTION_OVERWRITE, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(os_MOVE_TO,
			ro_plot_origin_x + x0 * 2,
			ro_plot_origin_y - y0 * 2);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(plot_code, width * 2, 0);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(plot_code, 0, -height * 2);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(plot_code, -width * 2, 0);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(plot_code, 0, height * 2);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	return true;
}


static int path[] = { draw_MOVE_TO, 0, 0, draw_LINE_TO, 0, 0,
	draw_END_PATH, 0 };
static const draw_line_style line_style = { draw_JOIN_MITRED,
	draw_CAP_BUTT, draw_CAP_BUTT, 0, 0x7fffffff,
	0, 0, 0, 0 };
static const int dash_pattern_dotted[] = { 0, 1, 512 };
static const int dash_pattern_dashed[] = { 0, 1, 2048 };

bool ro_plot_line(int x0, int y0, int x1, int y1, int width,
		colour c, bool dotted, bool dashed)
{
	const draw_dash_pattern *dash_pattern;
	os_error *error;

	if (dotted)
		dash_pattern = (const draw_dash_pattern *) &dash_pattern_dotted;
	else if (dashed)
		dash_pattern = (const draw_dash_pattern *) &dash_pattern_dashed;
	else
		dash_pattern = NULL;

	path[1] = (ro_plot_origin_x + x0 * 2) * 256;
	path[2] = (ro_plot_origin_y - y0 * 2) * 256;
	path[4] = (ro_plot_origin_x + x1 * 2) * 256;
	path[5] = (ro_plot_origin_y - y1 * 2) * 256;
	error = xcolourtrans_set_gcol(c << 8, 0, os_ACTION_OVERWRITE, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	error = xdraw_stroke((draw_path *) path, 0, 0, 0, width * 256,
			&line_style, dash_pattern);
	if (error) {
		LOG(("xdraw_stroke: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	return true;
}


bool ro_plot_polygon(int *p, unsigned int n, colour fill)
{
	int path[n * 3 + 2];
	unsigned int i;
	os_error *error;

	for (i = 0; i != n; i++) {
		path[i * 3 + 0] = draw_LINE_TO;
		path[i * 3 + 1] = (ro_plot_origin_x + p[i * 2 + 0] * 2) * 256;
		path[i * 3 + 2] = (ro_plot_origin_y - p[i * 2 + 1] * 2) * 256;
	}
	path[0] = draw_MOVE_TO;
	path[n * 3] = draw_END_PATH;
	path[n * 3 + 1] = 0;

	error = xcolourtrans_set_gcol(fill << 8, 0, os_ACTION_OVERWRITE, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
		     error->errnum, error->errmess));
		return false;
	}
	error = xdraw_fill((draw_path *) path, 0, 0, 0);
	if (error) {
		LOG(("xdraw_fill: 0x%x: %s",
		     error->errnum, error->errmess));
		return false;
	}

	return true;
}


bool ro_plot_fill(int x0, int y0, int x1, int y1, colour c)
{
	os_error *error;

	error = xcolourtrans_set_gcol(c << 8, colourtrans_USE_ECFS,
			os_ACTION_OVERWRITE, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	error = xos_plot(os_MOVE_TO,
			ro_plot_origin_x + x0 * 2,
			ro_plot_origin_y - y0 * 2 - 1);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	error = xos_plot(os_PLOT_RECTANGLE | os_PLOT_TO,
			ro_plot_origin_x + x1 * 2 - 1,
			ro_plot_origin_y - y1 * 2);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	return true;
}


bool ro_plot_clip(int clip_x0, int clip_y0,
		int clip_x1, int clip_y1)
{
	os_error *error;

	clip_x0 = ro_plot_origin_x + clip_x0 * 2;
	clip_y0 = ro_plot_origin_y - clip_y0 * 2 - 1;
	clip_x1 = ro_plot_origin_x + clip_x1 * 2 - 1;
	clip_y1 = ro_plot_origin_y - clip_y1 * 2;

	error = xos_set_graphics_window();
	if (error) {
		LOG(("xos_set_graphics_window: 0x%x: %s", error->errnum,
		     error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_x0 & 0xff));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_x0 >> 8));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_y1 & 0xff));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_y1 >> 8));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_x1 & 0xff));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_x1 >> 8));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_y0 & 0xff));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_writec((char) (clip_y0 >> 8));
	if (error) {
		LOG(("xos_writec: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	return true;
}


bool ro_plot_text(int x, int y, struct font_data *font,
		const char *text, size_t length, colour bg, colour c)
{
	os_error *error;

	error = xcolourtrans_set_font_colours(font->handle,
			bg << 8, c << 8, 14, 0, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_font_colours: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	nsfont_paint(font, text, length,
			ro_plot_origin_x + x * 2,
			ro_plot_origin_y - y * 2,
			&ro_plot_trfm);
	return true;
}


bool ro_plot_disc(int x, int y, int radius, colour colour)
{
	os_error *error;

	error = xcolourtrans_set_gcol(colour << 8, 0,
			os_ACTION_OVERWRITE, 0, 0);
	if (error) {
		LOG(("xcolourtrans_set_gcol: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(os_MOVE_TO,
			ro_plot_origin_x + x * 2,
			ro_plot_origin_y - y * 2);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}
	error = xos_plot(os_PLOT_CIRCLE | os_PLOT_BY, radius, 0);
	if (error) {
		LOG(("xos_plot: 0x%x: %s", error->errnum, error->errmess));
		return false;
	}

	return true;
}


bool ro_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg)
{
	return image_redraw(&bitmap->sprite_area,
			ro_plot_origin_x + x * 2,
			ro_plot_origin_y - y * 2,
			width, height,
			bitmap->width,
			bitmap->height,
			bg,
			false, false,
			bitmap->opaque ? IMAGE_PLOT_TINCT_OPAQUE :
			IMAGE_PLOT_TINCT_ALPHA);
}


bool ro_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bool repeat_x, bool repeat_y)
{
	return image_redraw(&bitmap->sprite_area,
			ro_plot_origin_x + x * 2,
			ro_plot_origin_y - y * 2,
			width, height,
			bitmap->width,
			bitmap->height,
			bg,
			repeat_x, repeat_y,
			bitmap->opaque ? IMAGE_PLOT_TINCT_OPAQUE :
			IMAGE_PLOT_TINCT_ALPHA);
}


/**
 * Set the scale for subsequent text plotting.
 */

void ro_plot_set_scale(float scale)
{
	ro_plot_trfm.entries[0][0] = ro_plot_trfm.entries[1][1] =
			scale * 0x10000;
}
