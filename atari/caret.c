#include <stdbool.h>

#include "desktop/plot_style.h"
#include "atari/plot/plot.h"
#include "atari/caret.h"

extern struct s_vdi_sysinfo vdi_sysinfo;

static void caret_restore_background(struct s_caret *c, VdiHdl vh, GRECT *clip);

void caret_show(struct s_caret *caret, VdiHdl vh, GRECT *dimensions, GRECT *clip)
{
	GRECT visible, old_dim;
	MFDB screen;
	short pxy[8];

	struct rect old_clip;

	return;



	plot_get_clip(&old_clip);
	plot_get_dimensions(&old_dim);


	// store background:
	visible = *dimensions;
	visible.g_x += clip->g_x;
	visible.g_y += clip->g_y;

	if(!rc_intersect(clip, &visible)){
		printf("no isect...\n");
		return;
	}


	// TODO: do not alloc / free on each move...
	if (caret->background.fd_addr != NULL) {
		//caret_restore_background(caret, vh, clip);
	}

	plot_lock();
	plot_set_dimensions(clip->g_x, clip->g_y, clip->g_w, clip->g_h);

	caret->dimensions.g_x = dimensions->g_x;
	caret->dimensions.g_y = dimensions->g_y;
	caret->dimensions.g_w = visible.g_w;
	caret->dimensions.g_h = visible.g_h;

	dbg_grect("clip", clip);
	dbg_grect("visible", &visible);
	// TODO: do not alloc / free on every redraw...
	init_mfdb(vdi_sysinfo.scr_bpp, visible.g_w, visible.g_h, 0,
			&caret->background);
	init_mfdb(0, visible.g_w, visible.g_h, 0, &screen);
	pxy[0] = visible.g_x;
	pxy[1] = visible.g_y;
	pxy[2] = visible.g_x + visible.g_w;
	pxy[3] = visible.g_y + visible.g_h;
	pxy[4] = 0;
	pxy[5] = 0;
	pxy[6] = visible.g_w;
	pxy[7] = visible.g_h;
	vro_cpyfm (vh, S_ONLY, pxy, &screen, &caret->background);

	plot_line(dimensions->g_x, dimensions->g_y, dimensions->g_x,
			dimensions->g_y + dimensions->g_h, plot_style_caret);

	plot_set_dimensions(old_clip.x0, old_clip.y0, old_clip.x1, old_clip.y1);
	plot_clip(&old_clip);

	plot_unlock();
	caret->visible = true;
}

void caret_hide(struct s_caret *caret, VdiHdl vh, GRECT *clip)
{
	struct rect old_clip;
	GRECT old_dim;

	plot_lock();
	plot_get_clip(&old_clip);
	plot_get_dimensions(&old_dim);
	plot_set_dimensions(clip->g_x, clip->g_y, clip->g_w, clip->g_h);
	caret_restore_background(caret, vh, clip);
	plot_set_dimensions(old_clip.x0, old_clip.y0, old_clip.x1, old_clip.y1);
	plot_clip(&old_clip);
	plot_unlock();
	caret->visible = false;
}

static void caret_restore_background(struct s_caret *caret, VdiHdl vh, GRECT *clip)
{
	MFDB screen;
	GRECT visible;
	short pxy[8];

	visible = caret->dimensions;
	visible.g_x += clip->g_x;
	visible.g_y += clip->g_y;

	dbg_grect("restore ", &visible);

	if(!rc_intersect(clip, &visible)){
		goto exit;
	}

	// TODO: check isect

	// restore mfdb

	init_mfdb(0, caret->dimensions.g_w, caret->dimensions.g_h, 0, &screen);
	pxy[0] = 0;
	pxy[1] = 0;
	pxy[2] = caret->dimensions.g_w;
	pxy[3] = caret->dimensions.g_h;
	pxy[4] = clip->g_x + caret->dimensions.g_x;
	pxy[5] = clip->g_y + caret->dimensions.g_y;
	pxy[6] = pxy[2];
	pxy[7] = pxy[3];
	vro_cpyfm(vh, S_ONLY, pxy, &caret->background, &screen);
	// exit:
	// TODO: do not alloc / free on every redraw...

exit:
	free(caret->background.fd_addr);
	caret->background.fd_addr = NULL;
}
