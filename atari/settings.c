
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <cflib.h>

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
static short edit_obj = -1;
static short any_obj = -1;
static GUIWIN * settings_guiwin = NULL;
static OBJECT * dlgtree;

#define OBJ_SELECTED(idx) ((bool)((dlgtree[idx].ob_state & OS_SELECTED)!=0))

#define OBJ_CHECK(idx) (dlgtree[idx].ob_state |= (OS_SELECTED | OS_CROSSED));

#define OBJ_UNCHECK(idx) (dlgtree[idx].ob_state &= ~(OS_SELECTED)); \
							(dlgtree[idx].ob_state &= ~(OS_CROSSED));

#define OBJ_REDRAW(idx) guiwin_send_redraw(settings_guiwin, \
										obj_screen_rect(dlgtree, idx));

#define DISABLE_OBJ(idx) (dlgtree[idx].ob_state |= OS_DISABLED); \
						 guiwin_send_redraw(settings_guiwin, \
											obj_screen_rect(dlgtree, idx));

#define ENABLE_OBJ(idx) (dlgtree[idx].ob_state &= ~(OS_DISABLED)) \
						 guiwin_send_redraw(settings_guiwin, \
											obj_screen_rect(dlgtree, idx));

#define FORMEVENT(idx) form_event(idx, 0, NULL);

#define INPUT_HOMEPAGE_URL_MAX_LEN 44
#define INPUT_LOCALE_MAX_LEN 6
#define INPUT_PROXY_HOST_MAX_LEN 31
#define INPUT_PROXY_USERNAME_MAX_LEN 36
#define INPUT_PROXY_PASSWORD_MAX_LEN 36
#define INPUT_PROXY_PORT_MAX_LEN 5
#define INPUT_MIN_REFLOW_PERIOD_MAX_LEN 4
#define LABEL_FONT_RENDERER_MAX_LEN 8
#define LABEL_PATH_MAX_LEN 43
#define LABEL_ICONSET_MAX_LEN 8
#define INPUT_TOOLBAR_COLOR_MAX_LEN 6

#define CB_SELECTED (OS_SELECTED | OS_CROSSED)

static void on_close(void);
static void on_redraw(GRECT *clip);
static void form_event(int index, int external, void *unused2);

static bool obj_is_inside(OBJECT * tree, short obj, GRECT *area)
{
	GRECT obj_screen;
	bool ret = false;

	objc_offset(tree, obj, &obj_screen.g_x, &obj_screen.g_y);
	obj_screen.g_w = dlgtree[obj].ob_width;
	obj_screen.g_h = dlgtree[obj].ob_height;
	ret = rc_intersect(area, &obj_screen);

	return(ret);
}

static GRECT * obj_screen_rect(OBJECT * tree, short obj)
{
	static GRECT obj_screen;

	get_objframe(tree, obj, &obj_screen);

	return(&obj_screen);
}

static void set_text( short idx, char * text, int len )
{
	char spare[255];

	if( len > 254 )
		len = 254;
	if( text != NULL ){
		strncpy( spare, text, 254);
	} else {
		strcpy(spare, "");
	}

	set_string( dlgtree, idx, spare);
}

/**
 * Toogle all objects which are directly influenced by other GUI elements
 * ( like checkbox )
 */
static void toggle_objects( void )
{
	/* enable / disable (refresh) objects depending on radio button values: */
	FORMEVENT(SETTINGS_CB_USE_PROXY);
	FORMEVENT(SETTINGS_CB_PROXY_AUTH);
	FORMEVENT(SETTINGS_BT_SEL_FONT_RENDERER);
}


/* this gets called each time the settings dialog is opened: */
static void display_settings(void)
{
	char spare[255];
	// read current settings and display them

	/* "Browser" tab: */
	set_text( SETTINGS_EDIT_HOMEPAGE, nsoption_charp(homepage_url),
			INPUT_HOMEPAGE_URL_MAX_LEN );

	if( nsoption_bool(block_ads) ){
		OBJ_CHECK( SETTINGS_CB_HIDE_ADVERTISEMENT );
	} else {
		OBJ_UNCHECK( SETTINGS_CB_HIDE_ADVERTISEMENT );
	}
	if( nsoption_bool(target_blank) ){
		OBJ_UNCHECK( SETTINGS_CB_DISABLE_POPUP_WINDOWS );
	} else {
		OBJ_CHECK( SETTINGS_CB_DISABLE_POPUP_WINDOWS );
	}
	if( nsoption_bool(send_referer) ){
		OBJ_CHECK( SETTINGS_CB_SEND_HTTP_REFERRER );
	} else {
		OBJ_UNCHECK( SETTINGS_CB_SEND_HTTP_REFERRER );
	}
	if( nsoption_bool(do_not_track) ){
		OBJ_CHECK( SETTINGS_CB_SEND_DO_NOT_TRACK );
	} else {
		OBJ_UNCHECK( SETTINGS_CB_SEND_DO_NOT_TRACK );
	}

	set_text( SETTINGS_BT_SEL_LOCALE,
		  nsoption_charp(accept_language) ? nsoption_charp(accept_language) : (char*)"en",
			INPUT_LOCALE_MAX_LEN );

	tmp_option_expire_url = nsoption_int(expire_url);
	snprintf( spare, 255, "%02d", nsoption_int(expire_url) );
	set_text( SETTINGS_EDIT_HISTORY_AGE, spare, 2 );

	/* "Cache" tab: */
	tmp_option_memory_cache_size = nsoption_int(memory_cache_size) / 100000;
	snprintf( spare, 255, "%03.1f", tmp_option_memory_cache_size );
	set_text( SETTINGS_STR_MAX_MEM_CACHE, spare, 5 );

	/* "Paths" tab: */
	set_text( SETTINGS_EDIT_DOWNLOAD_PATH, nsoption_charp(downloads_path),
			LABEL_PATH_MAX_LEN );
	set_text( SETTINGS_EDIT_HOTLIST_FILE, nsoption_charp(hotlist_file),
			LABEL_PATH_MAX_LEN );
	set_text( SETTINGS_EDIT_CA_BUNDLE, nsoption_charp(ca_bundle),
			LABEL_PATH_MAX_LEN );
	set_text( SETTINGS_EDIT_CA_CERTS_PATH, nsoption_charp(ca_path),
			LABEL_PATH_MAX_LEN );
	set_text( SETTINGS_EDIT_EDITOR, nsoption_charp(atari_editor),
			LABEL_PATH_MAX_LEN );

	/* "Rendering" tab: */
	set_text( SETTINGS_BT_SEL_FONT_RENDERER, nsoption_charp(atari_font_driver),
			LABEL_FONT_RENDERER_MAX_LEN );
	SET_BIT(dlgtree[SETTINGS_CB_TRANSPARENCY].ob_state,
			CB_SELECTED, nsoption_int(atari_transparency) ? 1 : 0 );
	SET_BIT(dlgtree[SETTINGS_CB_ENABLE_ANIMATION].ob_state,
			CB_SELECTED, nsoption_bool(animate_images) ? 1 : 0 );
	SET_BIT(dlgtree[SETTINGS_CB_FG_IMAGES].ob_state,
			CB_SELECTED, nsoption_bool(foreground_images) ? 1 : 0 );
	SET_BIT(dlgtree[SETTINGS_CB_BG_IMAGES].ob_state,
			CB_SELECTED, nsoption_bool(background_images) ? 1 : 0 );

/*
	TODO: enable this option?
	SET_BIT(dlgtree[SETTINGS_CB_INCREMENTAL_REFLOW].ob_state,
			CB_SELECTED, nsoption_bool(incremental_reflow) ? 1 : 0 );
*/
	SET_BIT(dlgtree[SETTINGS_CB_ANTI_ALIASING].ob_state,
			CB_SELECTED, nsoption_int(atari_font_monochrom) ? 0 : 1 );

/*
	TODO: activate this option?
	tmp_option_min_reflow_period = nsoption_int(min_reflow_period);
	snprintf( spare, 255, "%04d", tmp_option_min_reflow_period );
	set_text( SETTINGS_EDIT_MIN_REFLOW_PERIOD, spare,
			INPUT_MIN_REFLOW_PERIOD_MAX_LEN );
*/

	tmp_option_minimum_gif_delay = (float)nsoption_int(minimum_gif_delay) / (float)100;
	snprintf( spare, 255, "%01.1f", tmp_option_minimum_gif_delay );
	set_text( SETTINGS_EDIT_MIN_GIF_DELAY, spare, 3 );

	/* "Network" tab: */
	set_text( SETTINGS_EDIT_PROXY_HOST, nsoption_charp(http_proxy_host),
			INPUT_PROXY_HOST_MAX_LEN );
	snprintf( spare, 255, "%5d", nsoption_int(http_proxy_port) );
	set_text( SETTINGS_EDIT_PROXY_PORT, spare,
			INPUT_PROXY_PORT_MAX_LEN );

	set_text( SETTINGS_EDIT_PROXY_USERNAME, nsoption_charp(http_proxy_auth_user),
			INPUT_PROXY_USERNAME_MAX_LEN );
	set_text( SETTINGS_EDIT_PROXY_PASSWORD, nsoption_charp(http_proxy_auth_pass),
			INPUT_PROXY_PASSWORD_MAX_LEN );
	SET_BIT(dlgtree[SETTINGS_CB_USE_PROXY].ob_state,
			CB_SELECTED, nsoption_bool(http_proxy) ? 1 : 0 );
	SET_BIT(dlgtree[SETTINGS_CB_PROXY_AUTH].ob_state,
			CB_SELECTED, nsoption_int(http_proxy_auth) ? 1 : 0 );

	tmp_option_max_cached_fetch_handles = nsoption_int(max_cached_fetch_handles);
	snprintf( spare, 255, "%2d", nsoption_int(max_cached_fetch_handles) );
	set_text( SETTINGS_EDIT_MAX_CACHED_CONNECTIONS, spare , 2 );

	tmp_option_max_fetchers = nsoption_int(max_fetchers);
	snprintf( spare, 255, "%2d", nsoption_int(max_fetchers) );
	set_text( SETTINGS_EDIT_MAX_FETCHERS, spare , 2 );

	tmp_option_max_fetchers_per_host = nsoption_int(max_fetchers_per_host);
	snprintf( spare, 255, "%2d", nsoption_int(max_fetchers_per_host) );
	set_text( SETTINGS_EDIT_MAX_FETCHERS_PER_HOST, spare , 2 );


	/* "Style" tab: */
	tmp_option_font_min_size = nsoption_int(font_min_size);
	snprintf( spare, 255, "%3d", nsoption_int(font_min_size) );
	set_text( SETTINGS_EDIT_MIN_FONT_SIZE, spare , 3 );

	tmp_option_font_size = nsoption_int(font_size);
	snprintf( spare, 255, "%3d", nsoption_int(font_size) );
	set_text( SETTINGS_EDIT_DEF_FONT_SIZE, spare , 3 );
}


static void
form_event(int index, int external, void *unused2)
{
	char spare[255];
	bool is_button = false;
	bool checked = OBJ_SELECTED( index );
	char * tmp;

	/* For font driver popup: */
	const char *font_driver_items[] = {"freetype", "internal" };
	int num_font_drivers = (sizeof(font_driver_items)/sizeof(char*));

	/*
		Just a small collection of locales, each country has at least one
		ATARI-clone user! :)
	*/
	const char *locales[] = {
		"cs", "de", "de-de" , "en", "en-gb", "en-us", "es",
		"fr", "it", "nl", "no", "pl", "ru", "sk", "sv"
	};
	int num_locales = (sizeof(locales)/sizeof(char*));
	short x, y;
	int choice;

	switch( index ){


		case SETTINGS_INC_HISTORY_AGE:
		case SETTINGS_DEC_HISTORY_AGE:
			if( index == SETTINGS_INC_HISTORY_AGE )
				tmp_option_expire_url += 1;
			else
				tmp_option_expire_url -= 1;

			if( tmp_option_expire_url > 99 )
				tmp_option_expire_url =  0;

			snprintf( spare, 255, "%02d", tmp_option_expire_url );
			set_text( SETTINGS_EDIT_HISTORY_AGE, spare, 2 );
			OBJ_REDRAW(SETTINGS_EDIT_HISTORY_AGE);
			is_button = true;

			default: break;
	}
	if( is_button ){
		// remove selection indicator from button element:
		OBJ_UNCHECK(index);
		OBJ_REDRAW(index);
	}
}

static void on_redraw(GRECT *clip)
{
	GRECT visible, work, clip_ro;
	int scroll_px_x, scroll_px_y;
	struct guiwin_scroll_info_s *slid;
	int new_x, new_y, old_x, old_y;
	short edit_idx;

	/* Walk the AES rectangle list and redraw the visible areas of the window: */
	guiwin_get_grect(settings_guiwin, GUIWIN_AREA_CONTENT, &work);
	slid = guiwin_get_scroll_info(settings_guiwin);

	old_x = dlgtree->ob_x;
	old_y = dlgtree->ob_y;
	dlgtree->ob_x = new_x = work.g_x - (slid->x_pos * slid->x_unit_px);
	dlgtree->ob_y = new_y = work.g_y - (slid->y_pos * slid->y_unit_px);

	if ((edit_obj > -1) && (obj_is_inside(dlgtree, edit_obj, &work) == true)) {
		dlgtree->ob_x = old_x;
		dlgtree->ob_y = old_y;
		objc_edit(dlgtree, edit_obj, 0, &edit_idx,
			EDEND);
		edit_obj = -1;

		dlgtree->ob_x = new_x;
		dlgtree->ob_y = new_y;
	}

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
    static short edit_idx = 0;

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

    	if((edit_obj > -1) && obj_is_inside(dlgtree, edit_obj, &work)){

    		short next_edit_obj = edit_obj;
    		short next_char = -1;
    		short r;

    		guiwin_get_grect(settings_guiwin, GUIWIN_AREA_CONTENT, &work);

			r = form_keybd(dlgtree, edit_obj, next_edit_obj, ev_out->emo_kreturn,
									&next_edit_obj, &next_char);
			if (next_edit_obj != edit_obj) {
				objc_edit(dlgtree, edit_obj, ev_out->emo_kreturn, &edit_idx,
							EDEND);
				edit_obj = next_edit_obj;
				objc_edit(dlgtree, edit_obj, ev_out->emo_kreturn, &edit_idx,
							EDINIT);
			} else {
				if(next_char > 13)
					r = objc_edit(dlgtree, edit_obj, ev_out->emo_kreturn, &edit_idx,
								EDCHAR);
			}

    	}

    }
    if ((ev_out->emo_events & MU_BUTTON) != 0) {

   		struct guiwin_scroll_info_s *slid;
   		short nextobj, ret=-1;

		guiwin_get_grect(settings_guiwin, GUIWIN_AREA_CONTENT, &work);

		slid = guiwin_get_scroll_info(settings_guiwin);
		dlgtree->ob_x = work.g_x - (slid->x_pos * slid->x_unit_px);
		dlgtree->ob_y = work.g_y - (slid->y_pos * slid->y_unit_px);

		any_obj = objc_find(dlgtree, 0, 8, ev_out->emo_mouse.p_x,
								ev_out->emo_mouse.p_y);


		uint16_t type = (dlgtree[any_obj].ob_type & 0xFF);
		if (type == G_FTEXT || type == G_FBOXTEXT)  {
			printf("text??\n");
			ret = form_button(dlgtree, any_obj, ev_out->emo_mclicks, &nextobj);
			if(edit_obj != -1){
				if (obj_is_inside(dlgtree, edit_obj, &work)) {
					objc_edit(dlgtree, edit_obj, ev_out->emo_kreturn, &edit_idx, EDEND);
				}
			}
			if (obj_is_inside(dlgtree, any_obj, &work)) {
				edit_obj = any_obj;
				objc_edit(dlgtree, edit_obj, ev_out->emo_kreturn, &edit_idx, EDINIT);
			}
		} else {
			if ((edit_obj != -1) && obj_is_inside(dlgtree, edit_obj, &work)){
				objc_edit(dlgtree, edit_obj, ev_out->emo_kreturn, &edit_idx, EDEND);
			}
			edit_obj = -1;
			printf("xtype: %d\n", dlgtree[any_obj].ob_type & 0xff00 );
			if (((dlgtree[any_obj].ob_type & 0xff00) & GW_XTYPE_CHECKBOX) != 0) {
				if (OBJ_SELECTED(any_obj)) {
					dlgtree[any_obj].ob_state &= ~(OS_SELECTED|OS_CROSSED);
				} else {
					dlgtree[any_obj].ob_state |= (OS_SELECTED|OS_CROSSED);
				}
				guiwin_send_redraw(win, obj_screen_rect(dlgtree, any_obj));
			}
			form_event(any_obj, 1, NULL);
		}
		printf("clicked: %d / %d\n", any_obj, ret);
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

		/* set current config values: */
		display_settings();

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

