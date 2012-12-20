
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <windom.h>

#include "desktop/options.h"
#include "desktop/plot_style.h"
#include "atari/res/netsurf.rsh"
#include "atari/settings.h"
#include "atari/deskmenu.h"
#include "atari/misc.h"
#include "atari/plot/plot.h"
#include "atari/bitmap.h"
#include "atari/findfile.h"
#include "atari/gemtk/gemtk.h"

extern char options[PATH_MAX];
extern GRECT desk_area;



static float tmp_option_memory_cache_size;
static float tmp_option_minimum_gif_delay;
static unsigned int tmp_option_expire_url;
static unsigned int tmp_option_font_min_size;
static unsigned int tmp_option_font_size;
static unsigned int tmp_option_min_reflow_period;
static unsigned int tmp_option_max_fetchers;
static unsigned int tmp_option_max_fetchers_per_host;
static unsigned int tmp_option_max_cached_fetch_handles;
static colour tmp_option_atari_toolbar_bg;

static short h_aes_win = 0;
static GUIWIN * settings_guiwin = NULL;
static OBJECT * dlgtree;

static void on_close(void);
static void on_redraw(GRECT *clip);


static void on_redraw(GRECT *clip)
{
	GRECT visible, work, clip_ro;
	int scroll_px_x, scroll_px_y;
	struct guiwin_scroll_info_s *slid;

	/*Walk the AES rectangle list and redraw the visible areas of the window: */
	guiwin_get_grect(settings_guiwin, GUIWIN_AREA_CONTENT, &work);
	slid = guiwin_get_scroll_info(settings_guiwin);

	dlgtree->ob_x = work.g_x - (slid->x_pos * slid->x_unit_px);
	dlgtree->ob_y = work.g_y - (slid->y_pos * slid->y_unit_px);

	wind_get_grect(h_aes_win, WF_FIRSTXYWH, &visible);
	while (visible.g_x && visible.g_y) {
		if (rc_intersect(clip, &visible)) {
			objc_draw_grect(dlgtree, 0, 8, &visible);
		}
		wind_get_grect(h_aes_win, WF_NEXTXYWH, &visible);
	}
}

static short on_aes_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{
    short retval = 0;
    GRECT clip, work;

    if ((ev_out->emo_events & MU_MESAG) != 0) {
        // handle message
        printf("settings win msg: %d\n", msg[0]);
        switch (msg[0]) {

        case WM_REDRAW:
			clip.g_x = msg[4];
			clip.g_y = msg[5];
			clip.g_w = msg[6];
			clip.g_h = msg[7];
			on_redraw(&clip);
            break;

        case WM_CLOSED:
            // TODO: this needs to iterate through all gui windows and
            // check if the rootwin is this window...
			close_settings();
            break;

		case WM_SIZED:
			guiwin_update_slider(win, GUIWIN_VH_SLIDER);
			break;

        case WM_TOOLBAR:
			switch(msg[4]){
				default: break;
			}
            break;

        default:
            break;
        }
    }
    if ((ev_out->emo_events & MU_KEYBD) != 0) {


    }
    if ((ev_out->emo_events & MU_BUTTON) != 0) {

   		struct guiwin_scroll_info_s *slid;

		guiwin_get_grect(settings_guiwin, GUIWIN_AREA_CONTENT, &work);
		slid = guiwin_get_scroll_info(settings_guiwin);
		dlgtree->ob_x = work.g_x - (slid->x_pos * slid->x_unit_px);
		dlgtree->ob_y = work.g_y - (slid->y_pos * slid->y_unit_px);

		short obj = objc_find(dlgtree, 0, 8, ev_out->emo_mouse.p_x,
								ev_out->emo_mouse.p_y);
		printf("clicked: %d\n", obj);
		evnt_timer(150);
    }

    return(retval);
}

void open_settings(void)
{
	if (h_aes_win == 0) {

		GRECT curr, area;
		struct guiwin_scroll_info_s *slid;
		uint32_t kind = CLOSER | NAME | MOVER | VSLIDE | HSLIDE | UPARROW
							| DNARROW | LFARROW | RTARROW | SIZER | FULLER;

		dlgtree = get_tree(SETTINGS);
		area.g_x = area.g_y = 0;
		area.g_w = MIN(dlgtree->ob_width, desk_area.g_w);
		area.g_h = MIN(dlgtree->ob_height, desk_area.g_h);
		wind_calc_grect(WC_BORDER, kind, &area, &area);
		h_aes_win = wind_create_grect(kind, &area);
		wind_set_str(h_aes_win, WF_NAME, "Settings");
		settings_guiwin = guiwin_add(h_aes_win, GW_FLAG_DEFAULTS,
									on_aes_event);
		curr.g_w = MIN(dlgtree->ob_width, desk_area.g_w);
		curr.g_h = 200;
		curr.g_x = (desk_area.g_w / 2) - (curr.g_w / 2);
		curr.g_y = (desk_area.g_h / 2) - (curr.g_h / 2);
		wind_calc_grect(WC_BORDER, kind, &curr, &curr);
		wind_open_grect(h_aes_win, &curr);

		slid = guiwin_get_scroll_info(settings_guiwin);
		slid->y_unit_px = 32;
		slid->x_unit_px = 32;
		guiwin_get_grect(settings_guiwin, GUIWIN_AREA_CONTENT, &area);
		slid->x_units = (dlgtree->ob_width/slid->x_unit_px);
		slid->y_units = (dlgtree->ob_height/slid->y_unit_px);
		guiwin_update_slider(settings_guiwin, GUIWIN_VH_SLIDER);
	}
}

void close_settings(void)
{
	printf("settings close\n");
	guiwin_remove(settings_guiwin);
	settings_guiwin = NULL;
	wind_close(h_aes_win);
	wind_delete(h_aes_win);
	h_aes_win = 0;
}
