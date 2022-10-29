/*
 * Copyright 2022 Michael Drake <tlsa@netsurf-browser.org>
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
 * HTML layout implementation: display: flex.
 *
 * Layout is carried out in two stages:
 *
 * 1. + calculation of minimum / maximum box widths, and
 *    + determination of whether block level boxes will have >zero height
 *
 * 2. + layout (position and dimensions)
 *
 * In most cases the functions for the two stages are a corresponding pair
 * layout_minmax_X() and layout_X().
 */

#include <string.h>

#include "utils/log.h"
#include "utils/utils.h"

#include "html/box.h"
#include "html/html.h"
#include "html/private.h"
#include "html/box_inspect.h"
#include "html/layout_internal.h"

struct flex_item_data {
	enum css_flex_basis_e basis;
	css_fixed basis_length;
	css_unit basis_unit;
	struct box *box;

	css_fixed shrink;
	css_fixed grow;

	int min_main;
	int max_main;
	int min_cross;
	int max_cross;

	int target_main_size;
	int base_size;
	int main_size;
	size_t line;

	bool freeze;
	bool min_violation;
	bool max_violation;
};

struct flex_line_data {
	int main_size;
	int cross_size;

	size_t first;
	size_t count;
	size_t frozen;
};

struct flex_ctx {
	html_content *content;
	const struct box *flex;
	const css_unit_ctx *unit_len_ctx;

	int main_size;
	int cross_size;

	bool horizontal;
	enum css_flex_wrap_e wrap;

	struct flex_items {
		size_t count;
		struct flex_item_data *data;
	} item;

	struct flex_lines {
		size_t count;
		size_t alloc;
		struct flex_line_data *data;
	} line;
};

static void layout_flex_ctx__destroy(struct flex_ctx *ctx)
{
	if (ctx != NULL) {
		free(ctx->item.data);
		free(ctx->line.data);
		free(ctx);
	}
}

static struct flex_ctx *layout_flex_ctx__create(
		html_content *content,
		const struct box *flex)
{
	struct flex_ctx *ctx;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		return NULL;
	}
	ctx->line.alloc = 1;

	ctx->item.count = box_count_children(flex);
	ctx->item.data = calloc(ctx->item.count, sizeof(*ctx->item.data));
	if (ctx->item.data == NULL) {
		layout_flex_ctx__destroy(ctx);
		return NULL;
	}

	ctx->line.alloc = 1;
	ctx->line.data = calloc(ctx->line.alloc, sizeof(*ctx->line.data));
	if (ctx->line.data == NULL) {
		layout_flex_ctx__destroy(ctx);
		return NULL;
	}

	ctx->flex = flex;
	ctx->content = content;
	ctx->unit_len_ctx = &content->unit_len_ctx;

	ctx->wrap = css_computed_flex_wrap(flex->style);
	ctx->horizontal = lh__flex_main_is_horizontal(flex);

	return ctx;
}

static bool layout_flex_item(
		const struct flex_ctx *ctx,
		const struct flex_item_data *item,
		int available_width)
{
	bool success;
	struct box *b = item->box;

	switch (b->type) {
	case BOX_BLOCK:
		success = layout_block_context(b, -1,
				ctx->content);
		break;
	case BOX_TABLE:
		b->float_container = b->parent;
		success = layout_table(b, available_width,
				ctx->content);
		b->float_container = NULL;
		break;
	case BOX_FLEX:
		b->float_container = b->parent;
		success = layout_flex(b, available_width,
				ctx->content);
		b->float_container = NULL;
		break;
	default:
		assert(0 && "Bad flex item back type");
		success = false;
		break;
	}

	return success;
}

static inline bool layout_flex__base_and_main_sizes(
		const struct flex_ctx *ctx,
		struct flex_item_data *item,
		int available_width)
{
	struct box *b = item->box;
	int content_min_width = b->min_width;
	int content_max_width = b->max_width;
	int delta_outer_main = lh__delta_outer_main(ctx->flex, b);

	NSLOG(flex, WARNING, "box %p: delta_outer_main: %i",
			b, delta_outer_main);

	if (item->basis == CSS_FLEX_BASIS_SET) {
		if (item->basis_unit == CSS_UNIT_PCT) {
			item->base_size = FPCT_OF_INT_TOINT(
					item->basis_length,
					available_width);
		} else {
			item->base_size = FIXTOINT(css_unit_len2device_px(
					b->style, ctx->unit_len_ctx,
					item->basis_length,
					item->basis_unit));
		}

	} else if (item->basis == CSS_FLEX_BASIS_AUTO) {
		item->base_size = ctx->horizontal ? b->width : b->height;
	} else {
		item->base_size = AUTO;
	}

	if (ctx->horizontal == false) {
		if (b->width == AUTO) {
			b->width = min(available_width, content_max_width);
			b->width -= lh__delta_outer_width(b);
		}

		if (!layout_flex_item(ctx, item, b->width)) {
			NSLOG(flex, WARNING, "box %p: layout failed", b);
			return false;
		}
	}

	if (item->base_size == AUTO) {
		if (ctx->horizontal == false) {
			item->base_size = b->height;
		} else {
			item->base_size = content_max_width - delta_outer_main;
		}
	}

	item->base_size += delta_outer_main;

	if (ctx->horizontal) {
		item->base_size = min(item->base_size, available_width);
		item->base_size = max(item->base_size, content_min_width);
	}

	item->target_main_size = item->base_size;
	item->main_size = item->base_size;

	if (item->max_main > 0 &&
	    item->main_size > item->max_main + delta_outer_main) {
		item->main_size = item->max_main + delta_outer_main;
	}

	if (item->main_size < item->min_main + delta_outer_main) {
		item->main_size = item->min_main + delta_outer_main;
	}

	NSLOG(flex, WARNING, "flex-item box: %p: base_size: %i, main_size %i",
			b, item->base_size, item->main_size);

	return true;
}

static void layout_flex_ctx__populate_item_data(
		const struct flex_ctx *ctx,
		const struct box *flex,
		int available_width)
{
	size_t i = 0;
	bool horizontal = ctx->horizontal;

	for (struct box *b = flex->children; b != NULL; b = b->next) {
		struct flex_item_data *item = &ctx->item.data[i++];

		b->float_container = b->parent;
		layout_find_dimensions(ctx->unit_len_ctx, available_width, -1,
				b, b->style, &b->width, &b->height,
				horizontal ? &item->max_main : &item->max_cross,
				horizontal ? &item->min_main : &item->min_cross,
				horizontal ? &item->max_cross : &item->max_main,
				horizontal ? &item->min_cross : &item->min_main,
				b->margin, b->padding, b->border);
		b->float_container = NULL;

		NSLOG(flex, WARNING, "flex-item box: %p: width: %i",
				b, b->width);

		item->box = b;
		item->basis = css_computed_flex_basis(b->style,
				&item->basis_length, &item->basis_unit);

		css_computed_flex_shrink(b->style, &item->shrink);
		css_computed_flex_grow(b->style, &item->grow);

		layout_flex__base_and_main_sizes(ctx, item, available_width);
	}
}

static bool layout_flex_ctx__ensure_line(struct flex_ctx *ctx)
{
	struct flex_line_data *temp;
	size_t line_alloc = ctx->line.alloc * 2;

	if (ctx->line.alloc > ctx->line.count) {
		return true;
	}

	temp = realloc(ctx->line.data, sizeof(*ctx->line.data) * line_alloc);
	if (temp == NULL) {
		return false;
	}
	ctx->line.data = temp;

	memset(ctx->line.data + ctx->line.alloc, 0,
	       sizeof(*ctx->line.data) * (line_alloc - ctx->line.alloc));
	ctx->line.alloc = line_alloc;

	return true;
}

static struct flex_line_data *layout_flex__build_line(struct flex_ctx *ctx,
		size_t item_index, int available_width, html_content *content)
{
	struct flex_line_data *line;
	int available_main;
	int used_main = 0;

	if (!layout_flex_ctx__ensure_line(ctx)) {
		return 0;
	}

	line = &ctx->line.data[ctx->line.count];
	line->first = item_index;

	if (ctx->horizontal) {
		available_main = available_width;
	} else {
		available_main = ctx->flex->height;
	}

	NSLOG(flex, WARNING, "flex container %p: available main: %i",
			ctx->flex, available_main);

	while (item_index < ctx->item.count) {
		struct flex_item_data *item = &ctx->item.data[item_index];
		struct box *b = item->box;
		int main;

		main = ctx->horizontal ?
				item->main_size :
				b->height + lh__delta_outer_main(ctx->flex, b);

		if (ctx->wrap == CSS_FLEX_WRAP_NOWRAP ||
		    main + used_main <= available_main ||
		    lh__box_is_absolute(item->box) ||
		    available_main == AUTO ||
		    line->count == 0 ||
		    main == 0) {
			if (lh__box_is_absolute(item->box) == false) {
				line->main_size += item->main_size;
				used_main += main;
			}
			item->line = ctx->line.count;
			line->count++;
			item_index++;
		} else {
			break;
		}
	}

	if (line->count > 0) {
		ctx->line.count++;
	} else {
		NSLOG(layout, ERROR, "Failed to fit any flex items");
	}

	return line;
}

static inline void layout_flex__item_freeze(
		struct flex_line_data *line,
		struct flex_item_data *item)
{
	item->freeze = true;
	line->frozen++;

	NSLOG(flex, WARNING, "flex-item box: %p: Frozen at target_main_size: %i",
			item->box, item->target_main_size);
}

static inline int layout_flex__remaining_free_space(
		struct flex_ctx *ctx,
		struct flex_line_data *line,
		css_fixed *unfrozen_factor_sum,
		int initial_free_space,
		int available_space,
		bool grow)
{
	int remaining_free_space = available_space;
	size_t item_count = line->first + line->count;

	*unfrozen_factor_sum = 0;

	for (size_t i = line->first; i < item_count; i++) {
		struct flex_item_data *item = &ctx->item.data[i];

		if (item->freeze) {
			remaining_free_space -= item->target_main_size;
		} else {
			remaining_free_space -= item->base_size;

			*unfrozen_factor_sum += grow ?
					item->grow : item->shrink;
		}
	}

	if (*unfrozen_factor_sum < F_1) {
		int free_space = FIXTOINT(FMUL(INTTOFIX(initial_free_space),
				*unfrozen_factor_sum));

		if (free_space < remaining_free_space) {
			remaining_free_space = free_space;
		}
	}

	NSLOG(flex, WARNING, "Remaining free space: %i", remaining_free_space);

	return remaining_free_space;
}

static inline int layout_flex__get_min_max_violations(
		struct flex_ctx *ctx,
		struct flex_line_data *line)
{

	int total_violation = 0;
	size_t item_count = line->first + line->count;

	for (size_t i = line->first; i < item_count; i++) {
		struct flex_item_data *item = &ctx->item.data[i];
		int target_main_size = item->target_main_size;

		NSLOG(flex, WARNING, "item %p: target_main_size: %i",
					item->box, target_main_size);

		if (item->freeze) {
			continue;
		}

		if (item->max_main > 0 &&
		    target_main_size > item->max_main) {
			target_main_size = item->max_main;
			item->max_violation = true;
			NSLOG(flex, WARNING, "Violation: max_main: %i",
					item->max_main);
		}

		if (target_main_size < item->min_main) {
			target_main_size = item->min_main;
			item->min_violation = true;
			NSLOG(flex, WARNING, "Violation: min_main: %i",
					item->min_main);
		}

		if (target_main_size < item->box->min_width) {
			target_main_size = item->box->min_width;
			item->min_violation = true;
			NSLOG(flex, WARNING, "Violation: box min_width: %i",
					item->box->min_width);
		}

		if (target_main_size < 0) {
			target_main_size = 0;
			item->min_violation = true;
			NSLOG(flex, WARNING, "Violation: less than 0");
		}

		total_violation += target_main_size - item->target_main_size;
		item->target_main_size = target_main_size;
	}

	NSLOG(flex, WARNING, "Total violation: %i", total_violation);

	return total_violation;
}

static inline void layout_flex__distribute_free_space(
		struct flex_ctx *ctx,
		struct flex_line_data *line,
		css_fixed unfrozen_factor_sum,
		int remaining_free_space,
		bool grow)
{
	size_t item_count = line->first + line->count;

	if (grow) {
		for (size_t i = line->first; i < item_count; i++) {
			struct flex_item_data *item = &ctx->item.data[i];
			css_fixed ratio;

			if (item->freeze) {
				continue;
			}

			ratio = FDIV(item->grow, unfrozen_factor_sum);

			item->target_main_size = item->base_size +
					FIXTOINT(FMUL(
					INTTOFIX(remaining_free_space),
					ratio));
		}
	} else {
		css_fixed scaled_shrink_factor_sum = 0;

		for (size_t i = line->first; i < item_count; i++) {
			struct flex_item_data *item = &ctx->item.data[i];
			css_fixed scaled_shrink_factor;

			if (item->freeze) {
				continue;
			}

			scaled_shrink_factor = FMUL(
					item->shrink,
					INTTOFIX(item->base_size));
			scaled_shrink_factor_sum += scaled_shrink_factor;
		}

		for (size_t i = line->first; i < item_count; i++) {
			struct flex_item_data *item = &ctx->item.data[i];
			css_fixed scaled_shrink_factor;
			css_fixed ratio;

			if (item->freeze) {
				continue;
			} else if (scaled_shrink_factor_sum == 0) {
				item->target_main_size = item->main_size;
				layout_flex__item_freeze(line, item);
				continue;
			}

			scaled_shrink_factor = FMUL(
					item->shrink,
					INTTOFIX(item->base_size));
			ratio = FDIV(scaled_shrink_factor,
				     scaled_shrink_factor_sum);

			item->target_main_size = item->base_size -
					FIXTOINT(FMUL(
					INTTOFIX(abs(remaining_free_space)),
					ratio));
		}
	}
}

static bool layout_flex__resolve_line_horizontal(
		struct flex_ctx *ctx,
		struct flex_line_data *line,
		int available_width)
{
	size_t item_count = line->first + line->count;
	int x = ctx->flex->padding[LEFT];

	for (size_t i = line->first; i < item_count; i++) {
		struct flex_item_data *item = &ctx->item.data[i];
		struct box *b = item->box;
		bool success = false;

		b->width = item->target_main_size - lh__delta_outer_width(b);

		success = layout_flex_item(ctx, item, b->width);
		if (!success) {
			NSLOG(flex, WARNING, "box %p: layout failed", b);
			return false;
		}

		b->y = ctx->flex->padding[TOP] + ctx->cross_size +
				lh__non_auto_margin(b, TOP) +
				b->border[TOP].width;

		b->x = x + lh__non_auto_margin(b, LEFT) +
				b->border[LEFT].width;

		if (lh__box_is_absolute(b) == false) {
			int height;

			height = b->height + lh__delta_outer_height(b);
			if (line->cross_size < height) {
				line->cross_size = height;
			}

			x += b->width + lh__delta_outer_width(b);
		}
	}

	return true;
}

static bool layout_flex__resolve_line_vertical(
		struct flex_ctx *ctx,
		struct flex_line_data *line,
		int available_width)
{
	size_t item_count = line->first + line->count;
	int y = ctx->flex->padding[TOP];

	for (size_t i = line->first; i < item_count; i++) {
		struct flex_item_data *item = &ctx->item.data[i];
		struct box *b = item->box;

		b->x = ctx->flex->padding[LEFT] + ctx->cross_size +
				lh__non_auto_margin(b, LEFT) +
				b->border[LEFT].width;

		b->y = y + lh__non_auto_margin(b, TOP) +
				b->border[TOP].width;

		if (lh__box_is_absolute(b) == false) {
			int width;

			width = b->width + lh__delta_outer_width(b);
			if (line->cross_size < width) {
				line->cross_size = width;
			}

			y += b->height + lh__delta_outer_height(b);
		}
	}

	return true;
}

/** 9.7. Resolving Flexible Lengths */
static bool layout_flex__resolve_line(
		struct flex_ctx *ctx,
		struct flex_line_data *line,
		int available_width)
{
	bool grow = (line->main_size < available_width);
	size_t item_count = line->first + line->count;
	int available_space = available_width;
	int initial_free_space;

	available_space = available_width;
	if (ctx->horizontal == false) {
		available_space = ctx->flex->height;
		if (available_space == AUTO) {
			available_space = INT_MAX;
		}
	}

	initial_free_space = available_space;

	NSLOG(flex, WARNING, "box %p: line %zu: first: %zu, count: %zu",
			ctx->flex, line - ctx->line.data,
			line->first, line->count);
	NSLOG(flex, WARNING, "Line main_size: %i, available_space: %i",
			line->main_size, available_space);

	for (size_t i = line->first; i < item_count; i++) {
		struct flex_item_data *item = &ctx->item.data[i];

		/* 3. Size inflexible items */
		if (grow) {
			if (item->grow == 0 ||
			    item->base_size > item->main_size) {
				item->target_main_size = item->main_size;
				layout_flex__item_freeze(line, item);
			}
		} else {
			if (item->shrink == 0 ||
			    item->base_size < item->main_size) {
				item->target_main_size = item->main_size;
				layout_flex__item_freeze(line, item);
			}
		}

		/* 4. Calculate initial free space */
		if (item->freeze) {
			initial_free_space -= item->target_main_size;
		} else {
			initial_free_space -= item->base_size;
		}
	}

	/* 5. Loop */
	while (line->frozen < line->count) {
		css_fixed unfrozen_factor_sum;
		int remaining_free_space;
		int total_violation;

		NSLOG(flex, WARNING, "flex-container: %p: Resolver pass",
				ctx->flex);

		/* b */
		remaining_free_space = layout_flex__remaining_free_space(ctx,
				line, &unfrozen_factor_sum, initial_free_space,
				available_space, grow);

		/* c */
		if (remaining_free_space != 0) {
			layout_flex__distribute_free_space(ctx,
					line, unfrozen_factor_sum,
					remaining_free_space, grow);
		}

		/* d */
		total_violation = layout_flex__get_min_max_violations(
				ctx, line);

		/* e */
		for (size_t i = line->first; i < item_count; i++) {
			struct flex_item_data *item = &ctx->item.data[i];

			if (total_violation == 0 ||
			    (total_violation > 0 && item->min_violation) ||
			    (total_violation < 0 && item->max_violation)) {
				layout_flex__item_freeze(line, item);
			}
		}
	}

	if (ctx->horizontal) {
		if (!layout_flex__resolve_line_horizontal(ctx,
				line, available_width)) {
			return false;
		}
	} else {
		if (!layout_flex__resolve_line_vertical(ctx,
				line, available_width)) {
			return false;
		}
	}

	return true;
}

static bool layout_flex__collect_items_into_lines(
		struct flex_ctx *ctx,
		int available_width,
		html_content *content)
{
	size_t pos = 0;

	while (pos < ctx->item.count) {
		struct flex_line_data *line;

		line = layout_flex__build_line(ctx, pos,
				available_width, content);
		if (line == NULL) {
			return false;
		}

		pos += line->count;

		NSLOG(flex, WARNING, "flex-container: %p: "
				"fitted: %zu (total: %zu/%zu)",
				ctx->flex, line->count,
				pos, ctx->item.count);

		if (!layout_flex__resolve_line(ctx, line, available_width)) {
			return false;
		}

		ctx->cross_size += line->cross_size;
		if (ctx->main_size < line->main_size) {
			ctx->main_size = line->main_size;
		}
	}

	return true;
}

/**
 * Layout a flex container.
 *
 * \param[in] flex             table to layout
 * \param[in] available_width  width of containing block
 * \param[in] content          memory pool for any new boxes
 * \return  true on success, false on memory exhaustion
 */
bool layout_flex(struct box *flex, int available_width,
		html_content *content)
{
	int max_height, min_height;
	struct flex_ctx *ctx;
	bool success = false;

	ctx = layout_flex_ctx__create(content, flex);
	if (ctx == NULL) {
		return false;
	}

	NSLOG(flex, WARNING, "box %p: %s, available_width %i, width: %i", flex,
			ctx->horizontal ? "horizontal" : "vertical",
			available_width, flex->width);

	layout_find_dimensions(
			ctx->unit_len_ctx, available_width, -1,
			flex, flex->style, NULL, &flex->height,
			NULL, NULL, &max_height, &min_height,
			flex->margin, flex->padding, flex->border);

	available_width = min(available_width, flex->width);

	layout_flex_ctx__populate_item_data(ctx, flex, available_width);

	/* Place items onto lines. */
	success = layout_flex__collect_items_into_lines(ctx,
			available_width, content);
	if (!success) {
		goto cleanup;
	}

	if (flex->height == AUTO) {
		flex->height = ctx->horizontal ?
				ctx->cross_size :
				ctx->main_size;
	}

	if (flex->height != AUTO) {
		if (max_height >= 0 && flex->height > max_height) {
			flex->height = max_height;
		}
		if (min_height >  0 && flex->height < min_height) {
			flex->height = min_height;
		}
	}

	success = true;

cleanup:
	layout_flex_ctx__destroy(ctx);

	NSLOG(flex, WARNING, "box %p: %s", flex,
			success ? "success" : "failure");
	return success;
}
