/*
 * Copyright 2021 Vincent Sanders <vince@netsurf-browser.org>
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

/**
 * \file
 * Implementation of plotters for qt.
 */

#include <stddef.h>
#include <QPainter>
#include <QPainterPath>

extern "C" {

#include "utils/log.h"
#include "utils/errors.h"
#include "netsurf/types.h"
#include "netsurf/mouse.h"
#include "netsurf/window.h"
#include "netsurf/plotters.h"

}

#include "qt/window.h"
#include "qt/layout.h"
#include "qt/plotters.h"


/**
 * setup qt painter styles according to netsurf plot style
 */
static nserror nsqt_set_style(QPainter* painter, const plot_style_t *style)
{
	QColor fillcolour(style->fill_colour & 0xFF,
			  (style->fill_colour & 0xFF00) >>8,
			  (style->fill_colour & 0xFF0000) >>16);
	/*NSLOG(netsurf, WARNING,"fill_colour %x -> %d,%d,%d",
	      style->fill_colour,
	      style->fill_colour & 0xFF,
	      (style->fill_colour & 0xFF00) >>8,
	      (style->fill_colour & 0xFF0000) >>16);*/
	Qt::BrushStyle brushstyle = Qt::NoBrush;
	if (style->fill_type != PLOT_OP_TYPE_NONE) {
		brushstyle = Qt::SolidPattern;
	}
	QBrush brush(fillcolour, brushstyle);
	painter->setBrush(brush);

	QColor strokecolour(style->stroke_colour & 0xFF,
			    (style->stroke_colour & 0xFF00) >>8,
			    (style->stroke_colour & 0xFF0000) >>16);
	QPen pen(strokecolour);
	Qt::PenStyle penstyle = Qt::NoPen;
	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		penstyle = Qt::SolidLine;
	}
	pen.setStyle(penstyle);

	painter->setPen(pen);

	return NSERROR_OK;
}


/**
 * \brief Sets a clip rectangle for subsequent plot operations.
 *
 * \param ctx The current redraw context.
 * \param clip The rectangle to limit all subsequent plot
 *              operations within.
 * \return NSERROR_OK on success else error code.
 */
static nserror
nsqt_plot_clip(const struct redraw_context *ctx, const struct rect *clip)
{
	QPainter* painter = (QPainter*)ctx->priv;

	painter->setClipRect(clip->x0,clip->y0,clip->x1-clip->x0, clip->y1-clip->y0);
	return NSERROR_OK;
}


/**
 * Plots an arc
 *
 * plot an arc segment around (x,y), anticlockwise from angle1
 *  to angle2. Angles are measured anticlockwise from
 *  horizontal, in degrees.
 *
 * \param ctx The current redraw context.
 * \param style Style controlling the arc plot.
 * \param x The x coordinate of the arc.
 * \param y The y coordinate of the arc.
 * \param radius The radius of the arc.
 * \param angle1 The start angle of the arc.
 * \param angle2 The finish angle of the arc.
 * \return NSERROR_OK on success else error code.
 */
static nserror
nsqt_plot_arc(const struct redraw_context *ctx,
		const plot_style_t *style,
		int x, int y, int radius, int angle1, int angle2)
{
	return NSERROR_OK;
}


/**
 * Plots a circle
 *
 * Plot a circle centered on (x,y), which is optionally filled.
 *
 * \param ctx The current redraw context.
 * \param style Style controlling the circle plot.
 * \param x x coordinate of circle centre.
 * \param y y coordinate of circle centre.
 * \param radius circle radius.
 * \return NSERROR_OK on success else error code.
 */
static nserror
nsqt_plot_disc(const struct redraw_context *ctx,
		 const plot_style_t *style,
		 int x, int y, int radius)
{
	return NSERROR_OK;
}


/**
 * Plots a line
 *
 * plot a line from (x0,y0) to (x1,y1). Coordinates are at
 *  centre of line width/thickness.
 *
 * \param ctx The current redraw context.
 * \param style Style controlling the line plot.
 * \param line A rectangle defining the line to be drawn
 * \return NSERROR_OK on success else error code.
 */
static nserror
nsqt_plot_line(const struct redraw_context *ctx,
		 const plot_style_t *style,
		 const struct rect *line)
{
	QPainter* painter = (QPainter*)ctx->priv;
	nsqt_set_style(painter, style);

	painter->drawLine(line->x0, line->y0, line->x1, line->y1);

	return NSERROR_OK;
}


/**
 * Plots a rectangle.
 *
 * The rectangle can be filled an outline or both controlled
 *  by the plot style The line can be solid, dotted or
 *  dashed. Top left corner at (x0,y0) and rectangle has given
 *  width and height.
 *
 * \param ctx The current redraw context.
 * \param style Style controlling the rectangle plot.
 * \param rect A rectangle defining the line to be drawn
 * \return NSERROR_OK on success else error code.
 */
static nserror
nsqt_plot_rectangle(const struct redraw_context *ctx,
		      const plot_style_t *style,
		      const struct rect *rect)
{
	QPainter* painter = (QPainter*)ctx->priv;
	nsqt_set_style(painter, style);
	painter->drawRect(rect->x0,
			  rect->y0,
			  rect->x1 - rect->x0,
			  rect->y1 - rect->y0);
	return NSERROR_OK;
}


/**
 * Plot a polygon
 *
 * Plots a filled polygon with straight lines between
 * points. The lines around the edge of the ploygon are not
 * plotted. The polygon is filled with the non-zero winding
 * rule.
 *
 * \param ctx The current redraw context.
 * \param style Style controlling the polygon plot.
 * \param p verticies of polygon
 * \param n number of verticies.
 * \return NSERROR_OK on success else error code.
 */
static nserror
nsqt_plot_polygon(const struct redraw_context *ctx,
		    const plot_style_t *style,
		    const int *p,
		    unsigned int n)
{
	return NSERROR_OK;
}


/**
 * Plots a path.
 *
 * Path plot consisting of cubic Bezier curves. Line and fill colour is
 *  controlled by the plot style.
 *
 * The transform to apply is affine (meaning it omits the three
 * projection factor parameters from the standard 3x3 matrix assumining
 * default values)
 *
 * +--------------+--------------+--------------+
 * | transform[0] | transform[1] |      0.0     |
 * +--------------+--------------+--------------+
 * | transform[2] | transform[3] |      0.0     |
 * +--------------+--------------+--------------+
 * | transform[4] | transform[5] |      1.0     |
 * +--------------+--------------+--------------+

 * \param ctx The current redraw context.
 * \param pstyle Style controlling the path plot.
 * \param p elements of path
 * \param pn nunber of elements on path
 * \param transform A transform to apply to the path.
 * \return NSERROR_OK on success else error code.
 */
static nserror
nsqt_plot_path(const struct redraw_context *ctx,
		 const plot_style_t *pstyle,
		 const float *p,
		 unsigned int pn,
		 const float transform[6])
{
	unsigned int idx = 0;
	QPainter* painter = (QPainter*)ctx->priv;

	if (pn < 3) {
		/* path does not have enough points for initial move */
		return NSERROR_OK;
	}
	if (p[0] != PLOTTER_PATH_MOVE) {
		NSLOG(netsurf, INFO, "Path does not start with move");
		return NSERROR_INVALID;
	}

	QPainterPath qtpath(QPointF(p[1], p[2]));
	for (idx = 3; idx < pn; ) {
		switch ((int)p[idx]) {
		case PLOTTER_PATH_MOVE:
			qtpath.moveTo(p[idx+1], p[idx+2]);
			idx += 3;
			break;
		case PLOTTER_PATH_CLOSE:
			qtpath.closeSubpath();
			idx += 1;
			break;
		case PLOTTER_PATH_LINE:
			qtpath.lineTo(p[idx+1], p[idx+2]);
			idx +=3;
			break;
		case PLOTTER_PATH_BEZIER:
			qtpath.cubicTo(p[idx+1], p[idx+2],
				       p[idx+3], p[idx+4],
				       p[idx+5], p[idx+6]);
			idx +=7;
			break;

		default:
			NSLOG(netsurf, INFO, "bad path command %f", p[idx]);
			return NSERROR_INVALID;
		}
	}

	nsqt_set_style(painter, pstyle);
	const QTransform orig_transform = painter->transform();
	painter->setTransform(QTransform(transform[0], transform[1], 0.0,
					 transform[2], transform[3], 0.0,
					 transform[4], transform[5], 1.0), true);
	painter->drawPath(qtpath);

	painter->setTransform(orig_transform);
	return NSERROR_OK;
}


/**
 * Plot a bitmap
 *
 * Tiled plot of a bitmap image. (x,y) gives the top left
 * coordinate of an explicitly placed tile. From this tile the
 * image can repeat in all four directions -- up, down, left
 * and right -- to the extents given by the current clip
 * rectangle.
 *
 * The bitmap_flags say whether to tile in the x and y
 * directions. If not tiling in x or y directions, the single
 * image is plotted. The width and height give the dimensions
 * the image is to be scaled to.
 *
 * \param ctx The current redraw context.
 * \param bitmap The bitmap to plot
 * \param x The x coordinate to plot the bitmap
 * \param y The y coordiante to plot the bitmap
 * \param width The width of area to plot the bitmap into
 * \param height The height of area to plot the bitmap into
 * \param bg the background colour to alpha blend into
 * \param flags the flags controlling the type of plot operation
 * \return NSERROR_OK on success else error code.
 */
static nserror
nsqt_plot_bitmap(const struct redraw_context *ctx,
		   struct bitmap *bitmap,
		   int x, int y,
		   int width,
		   int height,
		   colour bg,
		   bitmap_flags_t flags)
{
	QImage *img = (QImage *)bitmap;
	QPainter* painter = (QPainter*)ctx->priv;
	QRectF source(0,0,img->width(),img->height());
	QRectF target(x,y,width,height);
	painter->drawImage(target,*img,source);
	return NSERROR_OK;
}


/**
 * Text plotting.
 *
 * \param ctx The current redraw context.
 * \param fstyle plot style for this text
 * \param x x coordinate
 * \param y y coordinate
 * \param text UTF-8 string to plot
 * \param length length of string, in bytes
 * \return NSERROR_OK on success else error code.
 */
static nserror
nsqt_plot_text(const struct redraw_context *ctx,
	       const struct plot_font_style *fstyle,
	       int x,
	       int y,
	       const char *text,
	       size_t length)
{
	return nsqt_layout_plot((QPainter*)ctx->priv, fstyle, x, y, text, length);
}


/**
 * QT plotter table
 */
const struct plotter_table nsqt_plotters = {
	.clip = nsqt_plot_clip,
	.arc = nsqt_plot_arc,
	.disc = nsqt_plot_disc,
	.line = nsqt_plot_line,
	.rectangle = nsqt_plot_rectangle,
	.polygon = nsqt_plot_polygon,
	.path = nsqt_plot_path,
	.bitmap = nsqt_plot_bitmap,
	.text = nsqt_plot_text,
	.group_start = NULL,
	.group_end = NULL,
	.flush = NULL,
	.option_knockout = true
};
