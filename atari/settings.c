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

#define ENABLE_OBJ(idx) (dlgtree[idx].ob_state &= ~(OS_DISABLED)); \
						 guiwin_send_redraw(settings_guiwin, \
											obj_screen_rect(dlgtree, idx));

#define FORMEVENT(idx) form_event(idx, 0);

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

static void on_close(void);
static void on_redraw(GRECT *clip);
static void form_event(int index, int external);
static void apply_settings(void);
static void save_settings(void);


static void set_text( short idx, char * text, int len )
{
    char spare[255];

    if( len > 254 )
        len = 254;
    if( text != NULL ) {
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
static void toggle_objects(void)
{
    /* enable / disable (refresh) objects depending on radio button values: */
    FORMEVENT(SETTINGS_CB_USE_PROXY);
    FORMEVENT(SETTINGS_CB_PROXY_AUTH);
    FORMEVENT(SETTINGS_BT_SEL_FONT_RENDERER);
}


static void save_settings(void)
{
    apply_settings();
    // Save settings
    nsoption_write( (const char*)&options );
    nsoption_read( (const char*)&options );
    close_settings();
    form_alert(1, "[1][Some options require an netsurf restart!][OK]");
    deskmenu_update();
}

/* this gets called each time the settings dialog is opened: */
static void display_settings(void)
{
    char spare[255];
    // read current settings and display them


    /* "Browser" tab: */
    set_text( SETTINGS_EDIT_HOMEPAGE, nsoption_charp(homepage_url),
              INPUT_HOMEPAGE_URL_MAX_LEN );

    if( nsoption_bool(block_ads) ) {
        OBJ_CHECK( SETTINGS_CB_HIDE_ADVERTISEMENT );
    } else {
        OBJ_UNCHECK( SETTINGS_CB_HIDE_ADVERTISEMENT );
    }
    if( nsoption_bool(target_blank) ) {
        OBJ_UNCHECK( SETTINGS_CB_DISABLE_POPUP_WINDOWS );
    } else {
        OBJ_CHECK( SETTINGS_CB_DISABLE_POPUP_WINDOWS );
    }
    if( nsoption_bool(send_referer) ) {
        OBJ_CHECK( SETTINGS_CB_SEND_HTTP_REFERRER );
    } else {
        OBJ_UNCHECK( SETTINGS_CB_SEND_HTTP_REFERRER );
    }
    if( nsoption_bool(do_not_track) ) {
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
    tmp_option_memory_cache_size = nsoption_int(memory_cache_size) / 1000000;
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
            GW_CB_SELECTED, nsoption_int(atari_transparency) ? 1 : 0 );
    SET_BIT(dlgtree[SETTINGS_CB_ENABLE_ANIMATION].ob_state,
            GW_CB_SELECTED, nsoption_bool(animate_images) ? 1 : 0 );
    SET_BIT(dlgtree[SETTINGS_CB_FG_IMAGES].ob_state,
            GW_CB_SELECTED, nsoption_bool(foreground_images) ? 1 : 0 );
    SET_BIT(dlgtree[SETTINGS_CB_BG_IMAGES].ob_state,
            GW_CB_SELECTED, nsoption_bool(background_images) ? 1 : 0 );


    // TODO: enable this option?
    /*	SET_BIT(dlgtree[SETTINGS_CB_INCREMENTAL_REFLOW].ob_state,
    			GW_CB_SELECTED, nsoption_bool(incremental_reflow) ? 1 : 0 );*/

    SET_BIT(dlgtree[SETTINGS_CB_ANTI_ALIASING].ob_state,
            GW_CB_SELECTED, nsoption_int(atari_font_monochrom) ? 0 : 1 );


    // TODO: activate this option?
    tmp_option_min_reflow_period = nsoption_int(min_reflow_period);
    snprintf( spare, 255, "%04d", tmp_option_min_reflow_period );
    set_text( SETTINGS_EDIT_MIN_REFLOW_PERIOD, spare,
              INPUT_MIN_REFLOW_PERIOD_MAX_LEN );


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
            GW_CB_SELECTED, nsoption_bool(http_proxy) ? 1 : 0 );
    SET_BIT(dlgtree[SETTINGS_CB_PROXY_AUTH].ob_state,
            GW_CB_SELECTED, nsoption_int(http_proxy_auth) ? 1 : 0 );

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

    toggle_objects();
}


static void form_event(int index, int external)
{
    char spare[255];
    bool is_button = false;
    bool checked = OBJ_SELECTED(index);
    char * tmp;
    MENU pop_menu, me_data;

    /* For font driver popup: */
    int num_font_drivers = 2;

    /*
    	Just a small collection of locales, each country has at least one
    	ATARI-clone user! :)
    */
    int num_locales = 15;
    short x, y;
    int choice, i;

    switch(index) {

    case SETTINGS_SAVE:
        save_settings();
        break;

    case SETTINGS_ABORT:
        close_settings();
        break;

    case SETTINGS_CB_USE_PROXY:
        if( checked ) {
            ENABLE_OBJ(SETTINGS_EDIT_PROXY_HOST);
            ENABLE_OBJ(SETTINGS_EDIT_PROXY_PORT);
            ENABLE_OBJ(SETTINGS_CB_PROXY_AUTH);
            ENABLE_OBJ(SETTINGS_LBL_PROXY_AUTH);
        } else {
            DISABLE_OBJ(SETTINGS_EDIT_PROXY_HOST);
            DISABLE_OBJ(SETTINGS_EDIT_PROXY_PORT);
            DISABLE_OBJ(SETTINGS_CB_PROXY_AUTH);
            DISABLE_OBJ(SETTINGS_LBL_PROXY_AUTH);
        }
        FORMEVENT(SETTINGS_CB_PROXY_AUTH);
        OBJ_REDRAW(SETTINGS_CB_USE_PROXY);
        break;

    case SETTINGS_CB_PROXY_AUTH:
        if( checked && OBJ_SELECTED( SETTINGS_CB_USE_PROXY ) ) {
            ENABLE_OBJ(SETTINGS_EDIT_PROXY_USERNAME);
            ENABLE_OBJ(SETTINGS_EDIT_PROXY_PASSWORD);
        } else {
            DISABLE_OBJ(SETTINGS_EDIT_PROXY_USERNAME);
            DISABLE_OBJ(SETTINGS_EDIT_PROXY_PASSWORD);
        }
        break;

    case SETTINGS_CB_ENABLE_ANIMATION:
        if( checked ) {
            ENABLE_OBJ( SETTINGS_EDIT_MIN_GIF_DELAY );
        } else {
            DISABLE_OBJ( SETTINGS_EDIT_MIN_GIF_DELAY );
        }
        break;

    case SETTINGS_BT_SEL_FONT_RENDERER:
        if( external ) {
            objc_offset(dlgtree, SETTINGS_BT_SEL_FONT_RENDERER, &x, &y);
            // point mn_tree tree to states popup:
            pop_menu.mn_tree = get_tree(POP_FONT_RENDERER);
            pop_menu.mn_menu = 0;
            pop_menu.mn_item = POP_FONT_RENDERER_INTERNAL;
            pop_menu.mn_scroll = SCROLL_NO;
            pop_menu.mn_keystate = 0;

			// find the selected menu item and uncheck others:
            for(i=pop_menu.mn_item; i<=num_font_drivers; i++) {
                get_string(pop_menu.mn_tree, i, spare);
                tmp = get_text(dlgtree, SETTINGS_BT_SEL_FONT_RENDERER);
                if (strcasecmp(&spare[2], tmp)) {
                    menu_icheck(pop_menu.mn_tree, i, 0);
                } else {
                    menu_icheck(pop_menu.mn_tree, i, 1);
                }
                set_string(pop_menu.mn_tree, i, spare);
            }

            menu_popup(&pop_menu, x, y, &me_data);
            choice = me_data.mn_item;
            if( choice > 0 && choice <= num_font_drivers ) {
                get_string(pop_menu.mn_tree, choice, spare);
                for(i=2; i<(int)strlen(spare); i++) {
                    spare[i]= (char)tolower(spare[i]);
                }
                set_text(SETTINGS_BT_SEL_FONT_RENDERER,
                         (char*)&spare[2],
                         LABEL_FONT_RENDERER_MAX_LEN);
                OBJ_REDRAW(SETTINGS_BT_SEL_FONT_RENDERER);
            }
        }
        tmp = get_text(dlgtree, SETTINGS_BT_SEL_FONT_RENDERER);
        if (strcasecmp(tmp, "freetype") == 0) {
            ENABLE_OBJ(SETTINGS_CB_ANTI_ALIASING);
        } else {
            DISABLE_OBJ(SETTINGS_CB_ANTI_ALIASING);
        }
        break;

    case SETTINGS_BT_SEL_LOCALE:
        objc_offset(dlgtree, SETTINGS_BT_SEL_LOCALE, &x, &y);

        // point mn_tree tree to states popup:
        pop_menu.mn_tree = get_tree(POP_LANGUAGE);
        pop_menu.mn_menu = 0;
        pop_menu.mn_item = POP_LANGUAGE_CS;
        pop_menu.mn_scroll = SCROLL_NO;
        pop_menu.mn_keystate = 0;

		// find the selected menu item and uncheck others:
        for(i=pop_menu.mn_item; i<=num_locales; i++) {
            get_string(pop_menu.mn_tree, i, spare);
            tmp = get_text(dlgtree, SETTINGS_BT_SEL_LOCALE);
            if (strcasecmp(&spare[2], tmp)) {
                menu_icheck(pop_menu.mn_tree, i, 0);
            } else {
                menu_icheck(pop_menu.mn_tree, i, 1);
            }
            set_string(pop_menu.mn_tree, i, spare);
        }


        menu_popup(&pop_menu, x, y, &me_data);
        choice = me_data.mn_item;
        if( choice > 0 && choice <= num_locales ) {
            get_string(pop_menu.mn_tree, choice, spare);
            for(i=2; i<(int)strlen(spare); i++) {
                spare[i]= (char)tolower(spare[i]);
            }
            set_text(SETTINGS_BT_SEL_LOCALE, (char*)&spare[2], 5);
        }

        OBJ_REDRAW(SETTINGS_BT_SEL_LOCALE);
        break;

        /*
        		case SETTINGS_INPUT_TOOLBAR_BGCOLOR:
        			objc_offset( FORM(win), SETTINGS_INPUT_TOOLBAR_BGCOLOR, &x, &y );
        			choice = color_popup(x, y, tmp_option_atari_toolbar_bg);
        			snprintf( spare, 255, "%06x", choice );
        			tmp_option_atari_toolbar_bg = choice;
        			ObjcStrCpy( dlgtree, SETTINGS_INPUT_TOOLBAR_BGCOLOR,
        							spare );
        			is_button = true;
        			OBJ_REDRAW(SETTINGS_INPUT_TOOLBAR_BGCOLOR);
        			break;
        */
        /*
        		case SETTINGS_BT_TOOLBAR_ICONSET:
        			objc_offset( FORM(win), SETTINGS_BT_TOOLBAR_ICONSET, &x, &y );
        			tmp = toolbar_iconset_popup(x,y);
        			if( tmp != NULL ){
        				ObjcStrCpy( dlgtree, SETTINGS_BT_TOOLBAR_ICONSET, tmp );
        			}
        			is_button = true;
        			break;
        */
    case SETTINGS_INC_MEM_CACHE:
    case SETTINGS_DEC_MEM_CACHE:
        if( index == SETTINGS_DEC_MEM_CACHE )
            tmp_option_memory_cache_size -= 0.1;
        else
            tmp_option_memory_cache_size += 0.1;

        if( tmp_option_memory_cache_size < 0.5 )
            tmp_option_memory_cache_size = 0.5;
        if( tmp_option_memory_cache_size > 999.9 )
            tmp_option_memory_cache_size = 999.9;
        snprintf( spare, 255, "%03.1f", tmp_option_memory_cache_size );
        set_text( SETTINGS_STR_MAX_MEM_CACHE, spare, 5 );
        is_button = true;
        OBJ_REDRAW(SETTINGS_STR_MAX_MEM_CACHE);
        break;

    case SETTINGS_INC_CACHED_CONNECTIONS:
    case SETTINGS_DEC_CACHED_CONNECTIONS:
        if( index == SETTINGS_INC_CACHED_CONNECTIONS )
            tmp_option_max_cached_fetch_handles += 1;
        else
            tmp_option_max_cached_fetch_handles -= 1;
        if( tmp_option_max_cached_fetch_handles > 31 )
            tmp_option_max_cached_fetch_handles = 31;

        snprintf( spare, 255, "%02d", tmp_option_max_cached_fetch_handles );
        set_text( SETTINGS_EDIT_MAX_CACHED_CONNECTIONS, spare, 2 );
        is_button = true;
        OBJ_REDRAW(SETTINGS_EDIT_MAX_CACHED_CONNECTIONS);
        break;

    case SETTINGS_INC_MAX_FETCHERS:
    case SETTINGS_DEC_MAX_FETCHERS:
        if( index == SETTINGS_INC_MAX_FETCHERS )
            tmp_option_max_fetchers += 1;
        else
            tmp_option_max_fetchers -= 1;
        if( tmp_option_max_fetchers > 31 )
            tmp_option_max_fetchers = 31;

        snprintf( spare, 255, "%02d", tmp_option_max_fetchers );
        set_text( SETTINGS_EDIT_MAX_FETCHERS, spare, 2 );
        is_button = true;
        OBJ_REDRAW(SETTINGS_EDIT_MAX_FETCHERS);
        break;

    case SETTINGS_INC_MAX_FETCHERS_PER_HOST:
    case SETTINGS_DEC_MAX_FETCHERS_PER_HOST:
        if( index == SETTINGS_INC_MAX_FETCHERS_PER_HOST )
            tmp_option_max_fetchers_per_host += 1;
        else
            tmp_option_max_fetchers_per_host -= 1;
        if( tmp_option_max_fetchers_per_host > 31 )
            tmp_option_max_fetchers_per_host = 31;

        snprintf( spare, 255, "%02d", tmp_option_max_fetchers_per_host );
        set_text( SETTINGS_EDIT_MAX_FETCHERS_PER_HOST, spare, 2 );
        is_button = true;
        OBJ_REDRAW(SETTINGS_EDIT_MAX_FETCHERS_PER_HOST);
        break;

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
        is_button = true;
        OBJ_REDRAW(SETTINGS_EDIT_HISTORY_AGE);
        break;

    case SETTINGS_INC_GIF_DELAY:
    case SETTINGS_DEC_GIF_DELAY:
        if( index == SETTINGS_INC_GIF_DELAY )
            tmp_option_minimum_gif_delay += 0.1;
        else
            tmp_option_minimum_gif_delay -= 0.1;

        if( tmp_option_minimum_gif_delay < 0.1 )
            tmp_option_minimum_gif_delay = 0.1;
        if( tmp_option_minimum_gif_delay > 9.0 )
            tmp_option_minimum_gif_delay = 9.0;
        snprintf( spare, 255, "%01.1f", tmp_option_minimum_gif_delay );
        set_text( SETTINGS_EDIT_MIN_GIF_DELAY, spare, 3 );
        is_button = true;
        OBJ_REDRAW(SETTINGS_EDIT_MIN_GIF_DELAY);
        break;

    case SETTINGS_INC_MIN_FONT_SIZE:
    case SETTINGS_DEC_MIN_FONT_SIZE:
        if( index == SETTINGS_INC_MIN_FONT_SIZE )
            tmp_option_font_min_size += 1;
        else
            tmp_option_font_min_size -= 1;

        if( tmp_option_font_min_size > 500 )
            tmp_option_font_min_size = 500;
        if( tmp_option_font_min_size < 10 )
            tmp_option_font_min_size = 10;

        snprintf( spare, 255, "%03d", tmp_option_font_min_size );
        set_text( SETTINGS_EDIT_MIN_FONT_SIZE, spare, 3 );
        is_button = true;
        OBJ_REDRAW(SETTINGS_EDIT_MIN_FONT_SIZE);
        break;

    case SETTINGS_INC_DEF_FONT_SIZE:
    case SETTINGS_DEC_DEF_FONT_SIZE:
        if( index == SETTINGS_INC_DEF_FONT_SIZE )
            tmp_option_font_size += 1;
        else
            tmp_option_font_size -= 1;

        if( tmp_option_font_size > 999 )
            tmp_option_font_size = 999;
        if( tmp_option_font_size < 50 )
            tmp_option_font_size = 50;

        snprintf( spare, 255, "%03d", tmp_option_font_size );
        set_text( SETTINGS_EDIT_DEF_FONT_SIZE, spare, 3 );
        is_button = true;
        OBJ_REDRAW(SETTINGS_EDIT_DEF_FONT_SIZE);
        break;

    case SETTINGS_INC_INCREMENTAL_REFLOW:
    case SETTINGS_DEC_INCREMENTAL_REFLOW:
        if( index == SETTINGS_INC_INCREMENTAL_REFLOW )
            tmp_option_min_reflow_period += 1;
        else
            tmp_option_min_reflow_period -= 1;

        if( tmp_option_min_reflow_period > 9999 )
            tmp_option_min_reflow_period = 10;

        snprintf( spare, 255, "%04d", tmp_option_min_reflow_period );
        set_text( SETTINGS_EDIT_MIN_REFLOW_PERIOD, spare, 4 );
        is_button = true;
        OBJ_REDRAW(SETTINGS_EDIT_MIN_REFLOW_PERIOD);
        break;

    default:
        break;
    }

    if( is_button ) {
        // remove selection indicator from button element:
        OBJ_UNCHECK(index);
        OBJ_REDRAW(index);
    }
}


static void apply_settings(void)
{
    /* "Network" tab: */
    nsoption_set_bool(http_proxy, OBJ_SELECTED(SETTINGS_CB_USE_PROXY));
    if ( OBJ_SELECTED(SETTINGS_CB_PROXY_AUTH) ) {
        nsoption_set_int(http_proxy_auth, OPTION_HTTP_PROXY_AUTH_BASIC);
    } else {
        nsoption_set_int(http_proxy_auth, OPTION_HTTP_PROXY_AUTH_NONE);
    }
    nsoption_set_charp(http_proxy_auth_pass,
                       get_text(dlgtree, SETTINGS_EDIT_PROXY_PASSWORD));
    nsoption_set_charp(http_proxy_auth_user,
                       get_text(dlgtree, SETTINGS_EDIT_PROXY_USERNAME));
    nsoption_set_charp(http_proxy_host,
                       get_text(dlgtree, SETTINGS_EDIT_PROXY_HOST));
    nsoption_set_int(http_proxy_port,
                     atoi( get_text(dlgtree, SETTINGS_EDIT_PROXY_PORT) ));
    nsoption_set_int(max_fetchers_per_host,
                     atoi( get_text(dlgtree, SETTINGS_EDIT_MAX_FETCHERS_PER_HOST)));
    nsoption_set_int(max_cached_fetch_handles,
                     atoi( get_text(dlgtree, SETTINGS_EDIT_MAX_CACHED_CONNECTIONS)));
    nsoption_set_int(max_fetchers,
                     atoi( get_text(dlgtree, SETTINGS_EDIT_MAX_FETCHERS) ));
    nsoption_set_bool(foreground_images,
                      OBJ_SELECTED( SETTINGS_CB_FG_IMAGES ));
    nsoption_set_bool(background_images,
                      OBJ_SELECTED( SETTINGS_CB_BG_IMAGES ));

    /* "Style" tab: */
    nsoption_set_int(font_min_size, tmp_option_font_min_size);
    nsoption_set_int(font_size, tmp_option_font_size);

    /* "Rendering" tab: */
    nsoption_set_charp(atari_font_driver,
                       get_text(dlgtree, SETTINGS_BT_SEL_FONT_RENDERER));
    nsoption_set_bool(atari_transparency,
                      OBJ_SELECTED(SETTINGS_CB_TRANSPARENCY));
    nsoption_set_bool(animate_images,
                      OBJ_SELECTED(SETTINGS_CB_ENABLE_ANIMATION));
    nsoption_set_int(minimum_gif_delay,
                     (int)(tmp_option_minimum_gif_delay*100+0.5));
    /*	nsoption_set_bool(incremental_reflow,
    			  OBJ_SELECTED(SETTINGS_CB_INCREMENTAL_REFLOW));*/
    nsoption_set_int(min_reflow_period, tmp_option_min_reflow_period);
    nsoption_set_int(atari_font_monochrom,
                     !OBJ_SELECTED( SETTINGS_CB_ANTI_ALIASING ));

    /* "Paths" tabs: */
    nsoption_set_charp(ca_bundle,
                       get_text(dlgtree, SETTINGS_EDIT_CA_BUNDLE));
    nsoption_set_charp(ca_path,
                       get_text(dlgtree, SETTINGS_EDIT_CA_CERTS_PATH));
    nsoption_set_charp(homepage_url,
                       get_text(dlgtree, SETTINGS_EDIT_CA_CERTS_PATH));
    nsoption_set_charp(hotlist_file,
                       get_text(dlgtree, SETTINGS_EDIT_HOTLIST_FILE));
    nsoption_set_charp(atari_editor,
                       get_text(dlgtree, SETTINGS_EDIT_EDITOR));
    nsoption_set_charp(downloads_path,
                       get_text(dlgtree, SETTINGS_EDIT_DOWNLOAD_PATH));

    /* "Cache" tab: */
    nsoption_set_int(memory_cache_size,
                     tmp_option_memory_cache_size * 1000000);

    /* "Browser" tab: */
    nsoption_set_bool(target_blank,
                      !OBJ_SELECTED(SETTINGS_CB_DISABLE_POPUP_WINDOWS));
    nsoption_set_bool(block_ads,
                      OBJ_SELECTED(SETTINGS_CB_HIDE_ADVERTISEMENT));
    nsoption_set_charp(accept_language,
                       get_text(dlgtree, SETTINGS_BT_SEL_LOCALE));
    nsoption_set_int(expire_url,
                     atoi(get_text(dlgtree, SETTINGS_EDIT_HISTORY_AGE)));
    nsoption_set_bool(send_referer,
                      OBJ_SELECTED(SETTINGS_CB_SEND_HTTP_REFERRER));
    nsoption_set_bool(do_not_track,
                      OBJ_SELECTED(SETTINGS_CB_SEND_DO_NOT_TRACK));
    nsoption_set_charp(homepage_url,
                       get_text(dlgtree, SETTINGS_EDIT_HOMEPAGE));
}

static short on_aes_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{
    short retval = 0;
    GRECT clip, work;
    static short edit_idx = 0;
    struct guiwin_scroll_info_s *slid;

    if ((ev_out->emo_events & MU_MESAG) != 0) {
        // handle message
        // printf("settings win msg: %d\n", msg[0]);
        switch (msg[0]) {


        case WM_CLOSED:
            // TODO: this needs to iterate through all gui windows and
            // check if the rootwin is this window...
            close_settings();
            break;

        case WM_SIZED:
            guiwin_update_slider(win, GUIWIN_VH_SLIDER);
            break;

        case WM_MOVED:
            break;

        case WM_TOOLBAR:
            switch(msg[4]) {
            default:
                break;
            }
            break;

        case GUIWIN_WM_FORM:
            form_event(msg[4], 1);
            break;

        default:
            break;
        }
    }

    if ((ev_out->emo_events & MU_KEYBD) != 0) {
    }

    if ((ev_out->emo_events & MU_BUTTON) != 0) {
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
        curr.g_h = MIN(dlgtree->ob_height, desk_area.g_h-64);
        curr.g_x = 1;
        curr.g_y = (desk_area.g_h / 2) - (curr.g_h / 2);

        wind_calc_grect(WC_BORDER, kind, &curr, &curr);

        dlgtree->ob_x = curr.g_x;
        dlgtree->ob_y = curr.g_y;

        /* set current config values: */
        display_settings();

        wind_open_grect(h_aes_win, &curr);

        guiwin_set_form(settings_guiwin, dlgtree, 0);
        guiwin_set_scroll_grid(settings_guiwin, 32, 32);
        guiwin_get_grect(settings_guiwin, GUIWIN_AREA_CONTENT, &area);

        slid = guiwin_get_scroll_info(settings_guiwin);
        guiwin_set_content_units(settings_guiwin,
                                 (dlgtree->ob_width/slid->x_unit_px),
                                 (dlgtree->ob_height/slid->y_unit_px));
        guiwin_update_slider(settings_guiwin, GUIWIN_VH_SLIDER);
    }
}

void close_settings(void)
{
    LOG((""));
    guiwin_remove(settings_guiwin);
    settings_guiwin = NULL;
    wind_close(h_aes_win);
    wind_delete(h_aes_win);
    h_aes_win = 0;
    LOG(("Done"));
}

