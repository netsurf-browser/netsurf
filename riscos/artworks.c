/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
 */

/** \file
 * Content for image/artworks (RISC OS implementation).
 *
 * Uses the ArtworksRenderer module
 */

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include "swis.h"
#include "oslib/os.h"
#include "oslib/wimp.h"
#include "utils/config.h"
#include "desktop/plotters.h"
#include "content/content.h"
#include "riscos/artworks.h"
#include "riscos/gui.h"
#include "utils/utils.h"
#include "utils/messages.h"
#include "utils/log.h"

#ifdef WITH_ARTWORKS

#define AWRender_FileInitAddress 0x46080
#define AWRender_RenderAddress   0x46081
#define AWRender_DocBounds       0x46082
#define AWRender_SendDefs        0x46083
#define AWRender_ClaimVectors    0x46084
#define AWRender_ReleaseVectors  0x46085
#define AWRender_FindFirstFont   0x46086
#define AWRender_FindNextFont    0x46087


#define INITIAL_BLOCK_SIZE 0x1000


struct awinfo_block {
	int ditherx;
	int dithery;
	int clip_x0;
	int clip_y0;
	int clip_x1;
	int clip_y1;
	int print_lowx;
	int print_lowy;
	int print_handle;
	int print_x1;
	int print_y1;
	int bgcolour;
};


/* Assembler routines for interfacing with the ArtworksRenderer module */

os_error *awrender_init(char **doc,
		size_t *doc_size,
		void *routine,
		void *workspace);

os_error *awrender_render(const char *doc,
		const struct awinfo_block *info,
		const os_trfm *trans,
		const int *vdu_vars,
		void **rsz_block,
		size_t *rsz_size,
		int wysiwyg_setting,
		int output_dest,
		size_t doc_size,
		void *routine,
		void *workspace);



/**
 * Convert a CONTENT_ARTWORKS for display.
 *
 * No conversion is necessary. We merely read the ArtWorks
 * bounding box bottom-left.
 */

bool artworks_convert(struct content *c, int width, int height)
{
	union content_msg_data msg_data;
	void *init_workspace;
	void *init_routine;
	os_error *error;
	int used;

	/* check whether AWViewer has been seen and we can therefore
		locate the ArtWorks rendering modules */
	if (xos_read_var_val_size("Alias$LoadArtWorksModules", 0, os_VARTYPE_STRING,
		&used, NULL, NULL) || used >= 0) {
		LOG(("Alias$LoadArtWorksModules not defined"));
		msg_data.error = messages_get("AWNotSeen");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	/* load the modules, or do nothing if they're already loaded */
	error = xos_cli("LoadArtWorksModules");
	if (error) {
		LOG(("xos_cli: 0x%x: %s",
				error->errnum, error->errmess));
		msg_data.error = error->errmess;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	/* lookup the addresses of the init and render routines */
	error = _swix(AWRender_FileInitAddress, _OUT(0) | _OUT(1),
				&init_routine, &init_workspace);
	if (error) {
		LOG(("AWRender_FileInitAddress: 0x%x: %s",
				error->errnum, error->errmess));
		msg_data.error = error->errmess;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	error = _swix(AWRender_RenderAddress, _OUT(0) | _OUT(1),
				&c->data.artworks.render_routine,
				&c->data.artworks.render_workspace);
	if (error) {
		LOG(("AWRender_RenderAddress: 0x%x: %s",
				error->errnum, error->errmess));
		msg_data.error = error->errmess;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	/* initialise (convert file to new format if required) */
	error = awrender_init(&c->source_data, &c->source_size,
			init_routine, init_workspace);
	if (error) {
		LOG(("awrender_init: 0x%x : %s",
			error->errnum, error->errmess));
		msg_data.error = error->errmess;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	error = _swix(AWRender_DocBounds, _IN(0) | _OUT(2) | _OUT(3) | _OUT(4) | _OUT(5),
			c->source_data,
			&c->data.artworks.x0,
			&c->data.artworks.y0,
			&c->data.artworks.x1,
			&c->data.artworks.y1);
	if (error) {
		LOG(("AWRender_DocBounds: 0x%x: %s",
			error->errnum, error->errmess));
		msg_data.error = error->errmess;
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	LOG(("bounding box: %d,%d,%d,%d",
			c->data.artworks.x0, c->data.artworks.y0,
			c->data.artworks.x1, c->data.artworks.y1));

	/* create the resizable workspace required by the
		ArtWorksRenderer rendering routine */

	c->data.artworks.size = INITIAL_BLOCK_SIZE;
	c->data.artworks.block = malloc(INITIAL_BLOCK_SIZE);
	if (!c->data.artworks.block) {
		LOG(("failed to create block for ArtworksRenderer"));
		msg_data.error = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	c->width = (c->data.artworks.x1 - c->data.artworks.x0) / 512;
	c->height = (c->data.artworks.y1 - c->data.artworks.y0) / 512;

	c->title = malloc(100);
	if (c->title)
		snprintf(c->title, 100, messages_get("ArtWorksTitle"), c->width,
				c->height, c->source_size);
	c->status = CONTENT_STATUS_DONE;
	return true;
}


/**
 * Destroy a CONTENT_ARTWORKS and free all resources it owns.
 */

void artworks_destroy(struct content *c)
{
	free(c->title);
	free(c->data.artworks.block);
}


/**
 * Redraw a CONTENT_ARTWORKS.
 */

bool artworks_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, colour background_colour)
{
	const os_VDU_VAR_LIST(4) vars = {
		{ os_MODEVAR_XEIG_FACTOR,
		os_MODEVAR_YEIG_FACTOR,
		os_MODEVAR_LOG2_BPP,
		os_VDUVAR_END_LIST }
	};
	struct awinfo_block info;
	os_error *error;
	os_trfm matrix;
	int vals[24];

	if (plot.flush && !plot.flush())
		return false;

	/* Scaled image. Transform units (65536*OS units) */
	matrix.entries[0][0] = width * 65536 / c->width;
	matrix.entries[0][1] = 0;
	matrix.entries[1][0] = 0;
	matrix.entries[1][1] = height * 65536 / c->height;
	/* Draw units. (x,y) = bottom left */
	matrix.entries[2][0] = ro_plot_origin_x * 256 + x * 512 -
			c->data.artworks.x0 * width / c->width;
	matrix.entries[2][1] = ro_plot_origin_y * 256 - (y + height) * 512 -
			c->data.artworks.y0 * height / c->height;

	info.ditherx = ro_plot_origin_x;
	info.dithery = ro_plot_origin_y;
	info.clip_x0 = INT_MIN;
	info.clip_y0 = INT_MIN;
	info.clip_x1 = INT_MAX;
	info.clip_y1 = INT_MAX;
	info.print_lowx = 0;
	info.print_lowy = 0;
	info.print_handle = 0;
	info.bgcolour = 0x20000000 | background_colour;

	error = xos_read_vdu_variables((os_vdu_var_list*)&vars, vals);
	if (error) {
		LOG(("xos_read_vdu_variables: 0x%x: %s",
			error->errnum, error->errmess));
		return false;
	}

	error = xwimp_read_palette((os_palette*)&vals[3]);
	if (error) {
		LOG(("xwimp_read_palette: 0x%x: %s",
			error->errnum, error->errmess));
		return false;
	}

	error = awrender_render(c->source_data,
			&info,
			&matrix,
			vals,
			&c->data.artworks.block,
			&c->data.artworks.size,
			110,	/* fully anti-aliased */
			0,
			c->source_size,
			c->data.artworks.render_routine,
			c->data.artworks.render_workspace);

	if (error) {
		LOG(("awrender_render: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	return true;
}

#endif
