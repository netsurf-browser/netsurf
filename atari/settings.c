#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <windom.h>

#include "desktop/options.h"
#include "atari/res/netsurf.rsh"
#include "atari/settings.h"
#include "atari/global_evnt.h"
#include "atari/misc.h"
#include "atari/options.h"

extern char options[PATH_MAX];

static OBJECT * dlgtree;
static WINDOW * dlgwin;

static float tmp_option_memory_cache_size;
static float tmp_option_minimum_gif_delay;
static unsigned int tmp_option_expire_url;
static unsigned int tmp_option_font_min_size;
static unsigned int tmp_option_font_size;
static unsigned int tmp_option_min_reflow_period;
static unsigned int tmp_option_max_fetchers;
static unsigned int tmp_option_max_fetchers_per_host;
static unsigned int tmp_option_max_cached_fetch_handles;

/* Tab forms and their buttons: */
static int frms[] = {
	CHOICES_TAB_BROWSER,
	CHOICES_TAB_RENDER,
	CHOICES_TAB_STYLE,
	CHOICES_TAB_NETWORK,
	CHOICES_TAB_PATH,
	CHOICES_TAB_CACHE
};

static int buts[] = {
	CHOICES_REG_BROWSER,
	CHOICES_REG_RENDER,
	CHOICES_REG_STYLE,
	CHOICES_REG_NETWORK,
	CHOICES_REG_PATH,
	CHOICES_REG_CACHE
};

#define OBJ_SELECTED(idx) ((dlgtree[idx].ob_state & SELECTED)!=0)
#define OBJ_CHECK(idx) SET_BIT(dlgtree[idx].ob_state, SELECTED, 1);
#define OBJ_UNCHECK(idx) SET_BIT(dlgtree[idx].ob_state, SELECTED, 0);

#define DISABLE_OBJ(idx) SET_BIT(dlgtree[idx].ob_state, DISABLED, 1); \
						 ObjcDraw( OC_FORM, dlgwin, idx, 1 )

#define ENABLE_OBJ(idx) SET_BIT(dlgtree[idx].ob_state, DISABLED, 0); \
						ObjcDraw( OC_FORM, dlgwin, idx, 1 )

#define FORMEVENT(idx) form_event( NULL, idx, 0, NULL );

#define INPUT_HOMEPAGE_URL_MAX_LEN 44
#define INPUT_LOCALE_MAX_LEN 6
#define INPUT_PROXY_HOST_MAX_LEN 31
#define INPUT_PROXY_USERNAME_MAX_LEN 36
#define INPUT_PROXY_PASSWORD_MAX_LEN 36
#define INPUT_PROXY_PORT_MAX_LEN 5
#define INPUT_MIN_REFLOW_PERIOD_MAX_LEN 4
#define LABEL_FONT_RENDERER_MAX_LEN 8
#define LABEL_PATH_MAX_LEN 43

static void toggle_objects( void );
static void display_settings( void );
static void apply_settings( void );
static void __CDECL onclose( WINDOW *win, short buff[8] );
static void __CDECL
	closeform( WINDOW *win, int index, int unused, void *unused2);
static void __CDECL
	saveform( WINDOW *win, int index, int unused, void *unused2);
static void __CDECL
	form_event( WINDOW *win, int index, int unused, void *unused2);
static void __CDECL
	clear_history( WINDOW *win, int index, int unused, void *unused2);

WINDOW * open_settings()
{

	if( dlgtree == NULL){
		dlgtree = get_tree( CHOICES );
		if( dlgtree == NULL ){
			return( NULL );
		}
	}

	if( dlgwin == NULL){
		// TODO: localize title
		dlgwin = FormCreate( dlgtree, WAT_FORM, NULL, (char*)"Settings",
					  NULL, TRUE, FALSE);
		if( !dlgwin ){
			return( NULL );
		}
		FormThumb( dlgwin, frms, buts, sizeof(frms) / sizeof(int) );

		/* Atach events to dialog buttons: */
		ObjcAttachFormFunc( dlgwin, CHOICES_ABORT, closeform, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_OK, saveform, NULL);

		/* Connect interactive dialog elements to generic event handler: */
		ObjcAttachFormFunc( dlgwin, CHOICES_CB_USE_PROXY, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_CB_PROXY_AUTH, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_EDIT_DOWNLOAD_PATH, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_EDIT_HOTLIST_FILE, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_EDIT_CA_BUNDLE, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_EDIT_CA_CERTS_PATH, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_EDIT_EDITOR, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_INC_GIF_DELAY, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_DEC_GIF_DELAY, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_INC_INCREMENTAL_REFLOW, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_DEC_INCREMENTAL_REFLOW, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_INC_CACHED_CONNECTIONS, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_DEC_CACHED_CONNECTIONS, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_INC_MAX_FETCHERS_PER_HOST, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_DEC_MAX_FETCHERS_PER_HOST, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_DEC_MAX_FETCHERS, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_INC_MAX_FETCHERS, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_INC_DEF_FONT_SIZE, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_DEC_DEF_FONT_SIZE, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_INC_MIN_FONT_SIZE, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_DEC_MIN_FONT_SIZE, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_INC_MEM_CACHE, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_DEC_MEM_CACHE, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_INC_HISTORY_AGE, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_DEC_HISTORY_AGE, form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_BT_SEL_FONT_RENDERER,
							form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_BT_SEL_LOCALE,
							form_event, NULL);
		ObjcAttachFormFunc( dlgwin, CHOICES_BT_CLEAR_HISTORY,
							clear_history, NULL);

		EvntAdd( dlgwin, WM_CLOSED, onclose, EV_TOP  );
		display_settings();

	} else {
		WindTop( dlgwin );
		toggle_objects();
	}
	return( dlgwin );
}

void close_settings(void)
{
	if( dlgwin != NULL ){
		WindClose( dlgwin );
		dlgwin = NULL;
	}

}

static void set_text( short idx, char * text, int len )
{
	char spare[255];

	if( len > 254 )
		len = 254;
	if( text != NULL ){
		strncpy( spare, text, 254);
		ObjcStrFmt( spare, text, len );
	} else {
		strcpy(spare, "");
	}
	ObjcStrCpy( dlgtree, idx, spare );

}


static void __CDECL onclose( WINDOW *win, short buff[8] )
{
	dlgwin = NULL;
}

static void __CDECL
closeform( WINDOW *win, int index, int unused, void *unused2)
{
	ObjcChange( OC_FORM, win, index, ~SELECTED, TRUE);
	close_settings();
}

static void __CDECL
saveform( WINDOW *win, int index, int unused, void *unused2)
{
	apply_settings();
	// Save settings
	options_write( (const char*)&options );
	options_read( (const char*)&options );
	close_settings();
	ObjcChange( OC_FORM, win, index, NORMAL, TRUE);
	form_alert(1, "[1][Some options require an netsurf restart!][OK]");
	main_menu_update();
}

static void __CDECL clear_history( WINDOW *win, int index, int unused,
								void *unused2)
{

}

static void __CDECL
form_event( WINDOW *win, int index, int external, void *unused2)
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

		case CHOICES_CB_USE_PROXY:
			if( checked ){
				ENABLE_OBJ( CHOICES_EDIT_PROXY_HOST );
				ENABLE_OBJ( CHOICES_CB_PROXY_AUTH );
			}
			else {
				DISABLE_OBJ( CHOICES_EDIT_PROXY_HOST );
				DISABLE_OBJ( CHOICES_CB_PROXY_AUTH );
			}
			FORMEVENT( CHOICES_CB_PROXY_AUTH );
			break;

		case CHOICES_CB_PROXY_AUTH:
			if( checked && OBJ_SELECTED( CHOICES_CB_USE_PROXY ) ){
				ENABLE_OBJ(CHOICES_EDIT_PROXY_USERNAME);
				ENABLE_OBJ(CHOICES_EDIT_PROXY_PASSWORD);
			}
			else {
				DISABLE_OBJ(CHOICES_EDIT_PROXY_USERNAME);
				DISABLE_OBJ(CHOICES_EDIT_PROXY_PASSWORD);
			}
			break;

		case CHOICES_CB_ENABLE_ANIMATION:
			if( checked ){
				ENABLE_OBJ( CHOICES_EDIT_MIN_GIF_DELAY );
			}
			else {
				DISABLE_OBJ( CHOICES_EDIT_MIN_GIF_DELAY );
			}
			break;

		case CHOICES_BT_SEL_FONT_RENDERER:
			if( external ){
				objc_offset( FORM(win), CHOICES_BT_SEL_FONT_RENDERER, &x, &y);
				choice = MenuPopUp ( font_driver_items, x, y,
									num_font_drivers,
									 -1, -1, P_LIST + P_WNDW + P_CHCK );
				if( choice > 0 &&
					choice <= num_font_drivers ){
					ObjcStrCpy( dlgtree, CHOICES_BT_SEL_FONT_RENDERER,
								(char*)font_driver_items[choice-1] );
				}
				ObjcChange( OC_FORM, win, index, NORMAL, TRUE);
			}
			tmp = ObjcString( dlgtree, CHOICES_BT_SEL_FONT_RENDERER, NULL);
			if( strcmp(tmp, "freetype") == 0 ){
				ENABLE_OBJ( CHOICES_CB_ANTI_ALIASING );
			} else {
				DISABLE_OBJ( CHOICES_CB_ANTI_ALIASING );
			}
			break;

		case CHOICES_BT_SEL_LOCALE:
			objc_offset( FORM(win), CHOICES_BT_SEL_LOCALE, &x, &y);
			choice = MenuPopUp ( locales, x, y,
								num_locales,
								 -1, -1, P_LIST + P_WNDW + P_CHCK );
			if( choice > 0 && choice <= num_locales ){
				ObjcStrCpy( dlgtree, CHOICES_BT_SEL_LOCALE,
							(char*)locales[choice-1] );
			}
			ObjcChange( OC_FORM, win, index, NORMAL, TRUE);
			break;

		case CHOICES_INC_MEM_CACHE:
		case CHOICES_DEC_MEM_CACHE:
			if( index == CHOICES_DEC_MEM_CACHE )
				tmp_option_memory_cache_size -= 0.1;
			else
				tmp_option_memory_cache_size += 0.1;

			if( tmp_option_memory_cache_size < 0.5 )
				tmp_option_memory_cache_size = 0.5;
			if( tmp_option_memory_cache_size > 999.9 )
				tmp_option_memory_cache_size = 999.9;
			sprintf( spare, "%03.1f", tmp_option_memory_cache_size );
			set_text( CHOICES_STR_MAX_MEM_CACHE, spare, 5 );
			is_button = true;
			ObjcDrawParent(OC_FORM, dlgwin, CHOICES_STR_MAX_MEM_CACHE, 2, 1 );
			break;

		case CHOICES_INC_CACHED_CONNECTIONS:
		case CHOICES_DEC_CACHED_CONNECTIONS:
			if( index == CHOICES_INC_CACHED_CONNECTIONS )
				tmp_option_max_cached_fetch_handles += 1;
			else
				tmp_option_max_cached_fetch_handles -= 1;
			if( tmp_option_max_cached_fetch_handles > 31 )
				tmp_option_max_cached_fetch_handles = 31;

			sprintf( spare, "%02d", tmp_option_max_cached_fetch_handles );
			set_text( CHOICES_EDIT_MAX_CACHED_CONNECTIONS, spare, 2 );
			is_button = true;
			ObjcDrawParent(OC_FORM, dlgwin, CHOICES_EDIT_MAX_CACHED_CONNECTIONS,
							2, 1 );
			break;

		case CHOICES_INC_MAX_FETCHERS:
		case CHOICES_DEC_MAX_FETCHERS:
			if( index == CHOICES_INC_MAX_FETCHERS )
				tmp_option_max_fetchers += 1;
			else
				tmp_option_max_fetchers -= 1;
			if( tmp_option_max_fetchers > 31 )
				tmp_option_max_fetchers = 31;

			sprintf( spare, "%02d", tmp_option_max_fetchers );
			set_text( CHOICES_EDIT_MAX_FETCHERS, spare, 2 );
			is_button = true;
			ObjcDrawParent(OC_FORM, dlgwin, CHOICES_EDIT_MAX_FETCHERS,
							3, 1 );
			break;

		case CHOICES_INC_MAX_FETCHERS_PER_HOST:
		case CHOICES_DEC_MAX_FETCHERS_PER_HOST:
			if( index == CHOICES_INC_MAX_FETCHERS_PER_HOST )
				tmp_option_max_fetchers_per_host += 1;
			else
				tmp_option_max_fetchers_per_host -= 1;
			if( tmp_option_max_fetchers_per_host > 31 )
				tmp_option_max_fetchers_per_host = 31;

			sprintf( spare, "%02d", tmp_option_max_fetchers_per_host );
			set_text( CHOICES_EDIT_MAX_FETCHERS_PER_HOST, spare, 2 );
			is_button = true;
			ObjcDrawParent(OC_FORM, dlgwin, CHOICES_EDIT_MAX_FETCHERS_PER_HOST,
							2, 1 );
			break;

		case CHOICES_INC_HISTORY_AGE:
		case CHOICES_DEC_HISTORY_AGE:
			if( index == CHOICES_INC_HISTORY_AGE )
				tmp_option_expire_url += 1;
			else
				tmp_option_expire_url -= 1;

			if( tmp_option_expire_url > 99 )
				tmp_option_expire_url =  0;

			sprintf( spare, "%02d", tmp_option_expire_url );
			set_text( CHOICES_EDIT_HISTORY_AGE, spare, 2 );
			is_button = true;
			ObjcDrawParent(OC_FORM, dlgwin, CHOICES_EDIT_HISTORY_AGE,
							3, 1 );
			break;

		case CHOICES_INC_GIF_DELAY:
		case CHOICES_DEC_GIF_DELAY:
			if( index == CHOICES_INC_GIF_DELAY )
				tmp_option_minimum_gif_delay += 0.1;
			else
				tmp_option_minimum_gif_delay -= 0.1;

			if( tmp_option_minimum_gif_delay < 0.1 )
				tmp_option_minimum_gif_delay = 0.1;
			if( tmp_option_minimum_gif_delay > 9.0 )
				tmp_option_minimum_gif_delay = 9.0;
			sprintf( spare, "%01.1f", tmp_option_minimum_gif_delay );
			set_text( CHOICES_EDIT_MIN_GIF_DELAY, spare, 3 );
			is_button = true;
			ObjcDrawParent(OC_FORM, dlgwin, CHOICES_EDIT_MIN_GIF_DELAY, 3, 1 );
			break;

		case CHOICES_INC_MIN_FONT_SIZE:
		case CHOICES_DEC_MIN_FONT_SIZE:
			if( index == CHOICES_INC_MIN_FONT_SIZE )
				tmp_option_font_min_size += 1;
			else
				tmp_option_font_min_size -= 1;

			if( tmp_option_font_min_size > 500 )
				tmp_option_font_min_size = 500;
			if( tmp_option_font_min_size < 10 )
				tmp_option_font_min_size = 10;

			sprintf( spare, "%03d", tmp_option_font_min_size );
			set_text( CHOICES_EDIT_MIN_FONT_SIZE, spare, 3 );
			is_button = true;
			ObjcDrawParent(OC_FORM, dlgwin, CHOICES_EDIT_MIN_FONT_SIZE,
							3, 1 );
			break;

		case CHOICES_INC_DEF_FONT_SIZE:
		case CHOICES_DEC_DEF_FONT_SIZE:
			if( index == CHOICES_INC_DEF_FONT_SIZE )
				tmp_option_font_size += 1;
			else
				tmp_option_font_size -= 1;

			if( tmp_option_font_size > 999 )
				tmp_option_font_size = 999;
			if( tmp_option_font_size < 50 )
				tmp_option_font_size = 50;

			sprintf( spare, "%03d", tmp_option_font_size );
			set_text( CHOICES_EDIT_DEF_FONT_SIZE, spare, 3 );
			is_button = true;
			ObjcDrawParent(OC_FORM, dlgwin, CHOICES_EDIT_DEF_FONT_SIZE,
							3, 1 );

			break;

		case CHOICES_INC_INCREMENTAL_REFLOW:
		case CHOICES_DEC_INCREMENTAL_REFLOW:
			if( index == CHOICES_INC_INCREMENTAL_REFLOW )
				tmp_option_min_reflow_period += 1;
			else
				tmp_option_min_reflow_period -= 1;

			if( tmp_option_min_reflow_period > 9999 )
				tmp_option_min_reflow_period = 10;

			sprintf( spare, "%04d", tmp_option_min_reflow_period );
			set_text( CHOICES_EDIT_MIN_REFLOW_PERIOD, spare, 4 );
			is_button = true;
			ObjcDrawParent(OC_FORM, dlgwin, CHOICES_EDIT_MIN_REFLOW_PERIOD,
							3, 1 );
			break;

			default: break;
	}
	if( is_button ){
		// remove selection indicator from button element:
		OBJ_UNCHECK( index );
		ObjcDraw( OC_FORM, dlgwin, index,  1 );
	}
}


static void toggle_objects( void )
{
	// enable / disable objects depending on radio button values.
	FORMEVENT(CHOICES_CB_USE_PROXY);
	FORMEVENT(CHOICES_CB_PROXY_AUTH);
	FORMEVENT(CHOICES_BT_SEL_FONT_RENDERER);
}


/* this gets called each time the settings dialog is opened: */
static void display_settings( void )
{
	char spare[255];
	// read current settings and display them

	/* "Browser" tab: */
	set_text( CHOICES_EDIT_HOMEPAGE, option_homepage_url,
			INPUT_HOMEPAGE_URL_MAX_LEN );

	if( option_block_ads ){
		OBJ_CHECK( CHOICES_CB_HIDE_ADVERTISEMENT );
	} else {
		OBJ_UNCHECK( CHOICES_CB_HIDE_ADVERTISEMENT );
	}
	if( option_target_blank ){
		OBJ_UNCHECK( CHOICES_CB_DISABLE_POPUP_WINDOWS );
	} else {
		OBJ_CHECK( CHOICES_CB_DISABLE_POPUP_WINDOWS );
	}
	if( option_send_referer ){
		OBJ_CHECK( CHOICES_CB_SEND_HTTP_REFERRER );
	} else {
		OBJ_UNCHECK( CHOICES_CB_SEND_HTTP_REFERRER );
	}

	set_text( CHOICES_BT_SEL_LOCALE,
			option_accept_language ? option_accept_language : (char*)"en",
			INPUT_LOCALE_MAX_LEN );

	tmp_option_expire_url = option_expire_url;
	sprintf( spare, "%02d", option_expire_url );
	set_text( CHOICES_EDIT_HISTORY_AGE, spare, 2 );

	/* "Cache" tab: */
	tmp_option_memory_cache_size = option_memory_cache_size / 100000;
	sprintf( spare, "%03.1f", tmp_option_memory_cache_size );
	set_text( CHOICES_STR_MAX_MEM_CACHE, spare, 5 );

	/* "Paths" tab: */
	set_text( CHOICES_EDIT_DOWNLOAD_PATH, option_downloads_path,
			LABEL_PATH_MAX_LEN );
	set_text( CHOICES_EDIT_HOTLIST_FILE, option_hotlist_file,
			LABEL_PATH_MAX_LEN );
	set_text( CHOICES_EDIT_CA_BUNDLE, option_ca_bundle,
			LABEL_PATH_MAX_LEN );
	set_text( CHOICES_EDIT_CA_CERTS_PATH, option_ca_path,
			LABEL_PATH_MAX_LEN );
	set_text( CHOICES_EDIT_EDITOR, option_atari_editor,
			LABEL_PATH_MAX_LEN );

	/* "Rendering" tab: */
	set_text( CHOICES_BT_SEL_FONT_RENDERER, option_atari_font_driver,
			LABEL_FONT_RENDERER_MAX_LEN );
	SET_BIT(dlgtree[CHOICES_CB_TRANSPARENCY].ob_state,
			SELECTED, option_atari_transparency ? 1 : 0 );
	SET_BIT(dlgtree[CHOICES_CB_ENABLE_ANIMATION].ob_state,
			SELECTED, option_animate_images ? 1 : 0 );
	SET_BIT(dlgtree[CHOICES_CB_INCREMENTAL_REFLOW].ob_state,
			SELECTED, option_incremental_reflow ? 1 : 0 );
	SET_BIT(dlgtree[CHOICES_CB_ANTI_ALIASING].ob_state,
			SELECTED, option_atari_font_monochrom ? 0 : 1 );

	tmp_option_min_reflow_period = option_min_reflow_period;
	sprintf( spare, "%04d", tmp_option_min_reflow_period );
	set_text( CHOICES_EDIT_MIN_REFLOW_PERIOD, spare,
			INPUT_MIN_REFLOW_PERIOD_MAX_LEN );

	tmp_option_minimum_gif_delay = (float)option_minimum_gif_delay / (float)100;
	sprintf( spare, "%01.1f", tmp_option_minimum_gif_delay );
	set_text( CHOICES_EDIT_MIN_GIF_DELAY, spare, 3 );

	/* "Network" tab: */
	set_text( CHOICES_EDIT_PROXY_HOST, option_http_proxy_host,
			INPUT_PROXY_HOST_MAX_LEN );
	sprintf( spare, "%5d", option_http_proxy_port );
	set_text( CHOICES_EDIT_PROXY_PORT, spare,
			INPUT_PROXY_PORT_MAX_LEN );

	set_text( CHOICES_EDIT_PROXY_USERNAME, option_http_proxy_auth_user,
			INPUT_PROXY_USERNAME_MAX_LEN );
	set_text( CHOICES_EDIT_PROXY_PASSWORD, option_http_proxy_auth_pass,
			INPUT_PROXY_PASSWORD_MAX_LEN );
	SET_BIT(dlgtree[CHOICES_CB_USE_PROXY].ob_state,
			SELECTED, option_http_proxy ? 1 : 0 );
	SET_BIT(dlgtree[CHOICES_CB_PROXY_AUTH].ob_state,
			SELECTED, option_http_proxy_auth ? 1 : 0 );
	SET_BIT(dlgtree[CHOICES_CB_FG_IMAGES].ob_state,
			SELECTED, option_foreground_images ? 1 : 0 );
	SET_BIT(dlgtree[CHOICES_CB_BG_IMAGES].ob_state,
			SELECTED, option_background_images ? 1 : 0 );

	tmp_option_max_cached_fetch_handles = option_max_cached_fetch_handles;
	sprintf( spare, "%2d", option_max_cached_fetch_handles );
	set_text( CHOICES_EDIT_MAX_CACHED_CONNECTIONS, spare , 2 );

	tmp_option_max_fetchers = option_max_fetchers;
	sprintf( spare, "%2d", option_max_fetchers );
	set_text( CHOICES_EDIT_MAX_FETCHERS, spare , 2 );

	tmp_option_max_fetchers_per_host = option_max_fetchers_per_host;
	sprintf( spare, "%2d", option_max_fetchers_per_host );
	set_text( CHOICES_EDIT_MAX_FETCHERS_PER_HOST, spare , 2 );


	/* "Style" tab: */
	tmp_option_font_min_size = option_font_min_size;
	sprintf( spare, "%3d", option_font_min_size );
	set_text( CHOICES_EDIT_MIN_FONT_SIZE, spare , 3 );

	tmp_option_font_size = option_font_size;
	sprintf( spare, "%3d", option_font_size );
	set_text( CHOICES_EDIT_DEF_FONT_SIZE, spare , 3 );

	/* Only first tab is refreshed: */
	ObjcDraw( OC_FORM, dlgwin, CHOICES_TAB_BROWSER, 4 );

	// update elements (enable/disable) chained to form events:
	toggle_objects();
}

static void apply_settings( void )
{


	/* "Network" tab: */
	option_http_proxy = OBJ_SELECTED(CHOICES_CB_USE_PROXY);
	if( OBJ_SELECTED(CHOICES_CB_PROXY_AUTH) )
		option_http_proxy_auth = OPTION_HTTP_PROXY_AUTH_BASIC;
	else
		option_http_proxy_auth = OPTION_HTTP_PROXY_AUTH_NONE;
	option_http_proxy_auth_pass =
		ObjcString( dlgtree, CHOICES_EDIT_PROXY_PASSWORD, NULL);
	option_http_proxy_auth_user =
		ObjcString( dlgtree, CHOICES_EDIT_PROXY_USERNAME, NULL);
	option_http_proxy_host =
		ObjcString( dlgtree, CHOICES_EDIT_PROXY_HOST, NULL);
	option_http_proxy_port =
		atoi( ObjcString( dlgtree, CHOICES_EDIT_PROXY_PORT, NULL) );
	option_max_fetchers_per_host =
		atoi( ObjcString( dlgtree, CHOICES_EDIT_MAX_FETCHERS_PER_HOST, NULL));
	option_max_cached_fetch_handles =
		atoi( ObjcString( dlgtree, CHOICES_EDIT_MAX_CACHED_CONNECTIONS, NULL));
	option_max_fetchers =
		atoi( ObjcString( dlgtree, CHOICES_EDIT_MAX_FETCHERS, NULL) );
	option_foreground_images = OBJ_SELECTED( CHOICES_CB_FG_IMAGES );
	option_background_images = OBJ_SELECTED( CHOICES_CB_BG_IMAGES );

	/* "Style" tab: */
	option_font_min_size = tmp_option_font_min_size;
	option_font_size = tmp_option_font_size;

	/* "Rendering" tab: */
	option_atari_font_driver = ObjcString( dlgtree,
										CHOICES_BT_SEL_FONT_RENDERER, NULL);
	option_atari_transparency = OBJ_SELECTED(CHOICES_CB_TRANSPARENCY);
	option_animate_images = OBJ_SELECTED(CHOICES_CB_ENABLE_ANIMATION);
	option_minimum_gif_delay = (int)(tmp_option_minimum_gif_delay*100+0.5);
	option_incremental_reflow = OBJ_SELECTED(CHOICES_CB_INCREMENTAL_REFLOW);
	option_min_reflow_period = tmp_option_min_reflow_period;
	option_atari_font_monochrom = !OBJ_SELECTED( CHOICES_CB_ANTI_ALIASING );

	/* "Paths" tabs: */
	option_ca_bundle = ObjcString( dlgtree, CHOICES_EDIT_CA_BUNDLE, NULL);
	option_ca_path = ObjcString( dlgtree, CHOICES_EDIT_CA_CERTS_PATH, NULL);
	option_homepage_url = ObjcString( dlgtree, CHOICES_EDIT_CA_CERTS_PATH, NULL);
	option_hotlist_file = ObjcString( dlgtree, CHOICES_EDIT_HOTLIST_FILE, NULL);
	option_atari_editor = ObjcString( dlgtree, CHOICES_EDIT_EDITOR, NULL);
	option_downloads_path = ObjcString( dlgtree, CHOICES_EDIT_DOWNLOAD_PATH, NULL);

	/* "Cache" tab: */
	option_memory_cache_size = tmp_option_memory_cache_size * 100000;

	/* "Browser" tab: */
	option_target_blank = !OBJ_SELECTED(CHOICES_CB_DISABLE_POPUP_WINDOWS);
	option_block_ads = OBJ_SELECTED(CHOICES_CB_HIDE_ADVERTISEMENT);
	option_accept_language = ObjcString( dlgtree, CHOICES_BT_SEL_LOCALE, NULL);
	option_expire_url = atoi(ObjcString( dlgtree, CHOICES_EDIT_HISTORY_AGE,
										NULL));
	option_send_referer = OBJ_SELECTED(CHOICES_CB_SEND_HTTP_REFERRER);
	option_homepage_url = ObjcString( dlgtree, CHOICES_EDIT_HOMEPAGE, NULL);
}

#undef OBJ_SELECTED
#undef OBJ_CHECK
#undef OBJ_UNCHECK
#undef DISABLE_OBJ
#undef ENABLE_OBJ
#undef FORMEVENT

