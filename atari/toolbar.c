/*
 * Copyright 2012 Ole Loots <ole@monochrom.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "utils/log.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "desktop/browser_private.h"
#include "desktop/mouse.h"
#include "desktop/plot_style.h"
#include "desktop/plotters.h"
#include "desktop/tree.h"
#include "desktop/options.h"
#include "utils/utf8.h"
#include "atari/clipboard.h"
#include "atari/gui.h"
#include "atari/toolbar.h"
#include "atari/rootwin.h"
#include "atari/browser.h"
#include "atari/clipboard.h"
#include "atari/misc.h"
#include "atari/global_evnt.h"
#include "atari/plot/plot.h"
#include "cflib.h"
#include "atari/res/netsurf.rsh"

#include "desktop/textarea.h"
#include "desktop/textinput.h"
#include "content/hlcache.h"
#include "atari/browser.h"

#define TB_BUTTON_WIDTH 32
#define THROBBER_WIDTH 32
#define THROBBER_MIN_INDEX 1
#define THROBBER_MAX_INDEX 12
#define THROBBER_INACTIVE_INDEX 13

#define TOOLBAR_URL_MARGIN_LEFT 	2
#define TOOLBAR_URL_MARGIN_RIGHT 	2
#define TOOLBAR_URL_MARGIN_TOP		2
#define TOOLBAR_URL_MARGIN_BOTTOM	2

enum e_toolbar_button_states {
        button_on = 0,
        button_off = 1
};
#define TOOLBAR_BUTTON_NUM_STATES   2

struct s_toolbar;

struct s_tb_button
{
	short rsc_id;
	void (*cb_click)(struct s_toolbar *tb);
	hlcache_handle *icon[TOOLBAR_BUTTON_NUM_STATES];
	struct s_toolbar *owner;
    short state;
    short index;
    GRECT area;
};


struct s_url_widget
{
    /* widget is only redrawn when this flag is set */
	bool redraw;
	struct text_area *textarea;
	GRECT rdw_area;
	GRECT area;
};

struct s_throbber_widget
{
	short index;
	short max_index;
	bool running;
	GRECT area;
};

struct s_toolbar
{
	struct s_gui_win_root *owner;
	struct s_url_widget url;
	struct s_throbber_widget throbber;
	GRECT btdim;
	GRECT area;
	/* size & location of buttons: */
	struct s_tb_button * buttons;
	bool hidden;
	int btcnt;
	int style;
	bool redraw;
    bool reflow;
};

extern char * option_homepage_url;
extern void * h_gem_rsrc;
extern struct gui_window * input_window;
extern long atari_plot_flags;
extern int atari_plot_vdi_handle;

static OBJECT * aes_toolbar = NULL;
static OBJECT * throbber_form = NULL;
static bool img_toolbar = false;
static char * toolbar_image_folder = (char *)"default";
static uint32_t toolbar_bg_color = 0xFFFFFF;
static hlcache_handle * toolbar_image;
static hlcache_handle * throbber_image;
static bool toolbar_image_ready = false;
static bool throbber_image_ready = false;
static bool init = false;

static plot_font_style_t font_style_url = {
    .family = PLOT_FONT_FAMILY_SANS_SERIF,
    .size = 14*FONT_SIZE_SCALE,
    .weight = 400,
    .flags = FONTF_NONE,
    .background = 0xffffff,
    .foreground = 0x0
 };


/* prototypes & order for button widgets: */


static struct s_tb_button tb_buttons[] =
{
	{
        TOOLBAR_BT_BACK,
        toolbar_back_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{
        TOOLBAR_BT_HOME,
        toolbar_home_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{
        TOOLBAR_BT_FORWARD,
        toolbar_forward_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{
        TOOLBAR_BT_STOP,
        toolbar_stop_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{
        TOOLBAR_BT_RELOAD,
        toolbar_reload_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{ 0, 0, {0,0}, 0, -1, 0, {0,0,0,0}}
};

struct s_toolbar_style {
	int font_height_pt;
	int height;
	int icon_width;
	int icon_height;
	int button_hmargin;
	int button_vmargin;
	/* RRGGBBAA: */
	uint32_t icon_bgcolor;
};

static struct s_toolbar_style toolbar_styles[] =
{
	/* small (18 px height) */
	{ 9, 18, 16, 16, 0, 0, 0 },
	/* medium (default - 26 px height) */
	{14, 26, 24, 24, 1, 4, 0 },
	/* large ( 49 px height ) */
	{18, 34, 64, 64, 2, 0, 0 },
	/* custom style: */
	{18, 34, 64, 64, 2, 0, 0 }
};

static void tb_txt_request_redraw(void *data, int x, int y, int w, int h );
static nserror toolbar_icon_callback( hlcache_handle *handle,
		const hlcache_event *event, void *pw );

/**
*   Callback for textarea redraw
*/
static void tb_txt_request_redraw(void *data, int x, int y, int w, int h)
{
	LGRECT work;
	if( data == NULL )
		return;
	CMP_TOOLBAR t = data;
	if( t->url.redraw == false ){
		t->url.redraw = true;
		//t->redraw = true;
		t->url.rdw_area.g_x = x;
		t->url.rdw_area.g_y = y;
		t->url.rdw_area.g_w = w;
		t->url.rdw_area.g_h = h;
	} else {
		/* merge the redraw area to the new area.: */
		int newx1 = x+w;
		int newy1 = y+h;
		int oldx1 = t->url.rdw_area.g_x + t->url.rdw_area.g_w;
		int oldy1 = t->url.rdw_area.g_y + t->url.rdw_area.g_h;
		t->url.rdw_area.g_x = MIN(t->url.rdw_area.g_x, x);
		t->url.rdw_area.g_y = MIN(t->url.rdw_area.g_y, y);
		t->url.rdw_area.g_w = ( oldx1 > newx1 ) ?
			oldx1 - t->url.rdw_area.g_x : newx1 - t->url.rdw_area.g_x;
		t->url.rdw_area.g_h = ( oldy1 > newy1 ) ?
			oldy1 - t->url.rdw_area.g_y : newy1 - t->url.rdw_area.g_y;
	}
}

/**
 * Callback for load_icon(). Should be removed once bitmaps get loaded directly
 * from disc
 */
static nserror toolbar_icon_callback(hlcache_handle *handle,
		const hlcache_event *event, void *pw)
{
	if( event->type == CONTENT_MSG_READY ){
		if( handle == toolbar_image ){
			toolbar_image_ready = true;
			if(input_window != NULL )
				toolbar_update_buttons(input_window->root->toolbar,
                           input_window->browser->bw, 0);
		}
		else if(handle == throbber_image ){
			throbber_image_ready = true;
		}
	}

	return NSERROR_OK;
}

static struct s_tb_button *button_init(struct s_toolbar *tb, OBJECT * tree, int index,
							struct s_tb_button * instance)
{
	*instance = tb_buttons[index];
	instance->owner = tb;

	instance->area.g_w = toolbar_styles[tb->style].icon_width + \
		( toolbar_styles[tb->style].button_vmargin * 2);

    return(instance);
}


static void toolbar_reflow(struct s_toolbar *tb)
{
    LOG((""));
/*
    int i=0, x=0;

    x = 2;
    while (tb->buttons[i].rsc_id > 0) {
        tb->buttons[i].area.g_x = x;
        x += tb->buttons[i].area.g_w;
        x += 2;
		i++;
    }
    tb->url.area.g_x = x;
*/
}


void toolbar_init( void )
{
	int i=0, n;
	short vdicolor[3];
	uint32_t rgbcolor;

	toolbar_image_folder = nsoption_charp(atari_image_toolbar_folder);
	toolbar_bg_color = (nsoption_colour(atari_toolbar_bg));
	img_toolbar = (nsoption_int(atari_image_toolbar) > 0 ) ? true : false;
	if( img_toolbar ){

        char imgfile[PATH_MAX];
        const char * imgfiletmpl = "toolbar/%s/%s";

        while( tb_buttons[i].rsc_id != 0){
			tb_buttons[i].index = i;
			i++;
		}
		snprintf( imgfile, PATH_MAX-1, imgfiletmpl, toolbar_image_folder,
				"main.png" );
		toolbar_image = load_icon( imgfile,
									toolbar_icon_callback, NULL );
		snprintf( imgfile, PATH_MAX-1, imgfiletmpl, toolbar_image_folder,
				"throbber.png" );
		throbber_image = load_icon( imgfile,
									toolbar_icon_callback, NULL );

	} else {
	    aes_toolbar = get_tree(TOOLBAR);
        throbber_form = get_tree(THROBBER);
	}
    n = (sizeof( toolbar_styles ) / sizeof( struct s_toolbar_style ));
    for (i=0; i<n; i++) {
		toolbar_styles[i].icon_bgcolor = ABGR_TO_RGB(toolbar_bg_color);
    }
}


void toolbar_exit(void)
{
	if (toolbar_image)
		hlcache_handle_release(toolbar_image);
	if (throbber_image)
		hlcache_handle_release(throbber_image);
}


struct s_toolbar *toolbar_create(struct s_gui_win_root *owner)
{
	int i;

	LOG((""));

	struct s_toolbar *t = calloc(sizeof(struct s_toolbar), 1);

	assert(t);

	t->owner = owner;
	t->style = 1;

	/* create the root component: */
	t->area.g_h = toolbar_styles[t->style].height;

	/* count buttons and add them as components: */
	i = 0;
	while(tb_buttons[i].rsc_id > 0) {
		i++;
	}
	t->btcnt = i;
	t->buttons = malloc(t->btcnt * sizeof(struct s_tb_button));
	memset( t->buttons, 0, t->btcnt * sizeof(struct s_tb_button));
	for (i=0; i < t->btcnt; i++ ) {
		button_init(t, aes_toolbar, i, &t->buttons[i]);
	}

	/* create the url widget: */
	font_style_url.size =
		toolbar_styles[t->style].font_height_pt * FONT_SIZE_SCALE;

	int ta_height = toolbar_styles[t->style].height;
	ta_height -= (TOOLBAR_URL_MARGIN_TOP + TOOLBAR_URL_MARGIN_BOTTOM);
	t->url.textarea = textarea_create(300, ta_height, 0, &font_style_url,
                                   tb_txt_request_redraw, t);
	if( t->url.textarea != NULL ){
		textarea_set_text(t->url.textarea, "http://");
	}

	/* create the throbber widget: */
	t->throbber.area.g_h = toolbar_styles[t->style].height;
	t->throbber.area.g_w = toolbar_styles[t->style].icon_width + \
		(2*toolbar_styles[t->style].button_vmargin );
	if( img_toolbar == true ){
		t->throbber.index = 0;
		t->throbber.max_index = 8;
	} else {
		t->throbber.index = THROBBER_MIN_INDEX;
		t->throbber.max_index = THROBBER_MAX_INDEX;
	}
	t->throbber.running = false;

	LOG(("created toolbar: %p, root: %p, textarea: %p, throbber: %p", t,
        owner, t->url.textarea, t->throbber));
	return( t );
}


void toolbar_destroy(struct s_toolbar *tb)
{
    free(tb->buttons);
	textarea_destroy( tb->url.textarea );
	free(tb);
}

static void toolbar_objc_reflow(struct s_toolbar *tb)
{

    // position toolbar areas:
    aes_toolbar->ob_x = tb->area.g_x;
    aes_toolbar->ob_y = tb->area.g_y;
    aes_toolbar->ob_width = tb->area.g_w;
    aes_toolbar->ob_height = tb->area.g_h;

    aes_toolbar[TOOLBAR_THROBBER_AREA].ob_x = tb->area.g_w
        - aes_toolbar[TOOLBAR_THROBBER_AREA].ob_width;

    aes_toolbar[TOOLBAR_URL_AREA].ob_width = tb->area.g_w
       - (aes_toolbar[TOOLBAR_NAVIGATION_AREA].ob_width
       + aes_toolbar[TOOLBAR_THROBBER_AREA].ob_width);

    // position throbber image:
    throbber_form[tb->throbber.index].ob_x = tb->area.g_x +
        aes_toolbar[TOOLBAR_THROBBER_AREA].ob_x;

    throbber_form[tb->throbber.index].ob_x = tb->area.g_x
        + aes_toolbar[TOOLBAR_THROBBER_AREA].ob_x +
        ((aes_toolbar[TOOLBAR_THROBBER_AREA].ob_width
        - throbber_form[tb->throbber.index].ob_width) >> 1);

    throbber_form[tb->throbber.index].ob_y = tb->area.g_y +
        ((aes_toolbar[TOOLBAR_THROBBER_AREA].ob_height
        - throbber_form[tb->throbber.index].ob_height) >> 1);

    tb->reflow = false;
}

void toolbar_redraw(struct s_toolbar *tb, GRECT *clip)
{
    if(tb->reflow == true)
        toolbar_objc_reflow(tb);

    objc_draw_grect(aes_toolbar,0,8,clip);

    objc_draw_grect(&throbber_form[tb->throbber.index], 0, 1, clip);
}


void toolbar_update_buttons(struct s_toolbar *tb, struct browser_window *bw,
                       short button)
{
    LOG((""));
}


void toolbar_set_dimensions(struct s_toolbar *tb, GRECT *area)
{
    tb->area = *area;
    tb->reflow = true;
}


void toolbar_set_url(struct s_toolbar *tb, const char * text)
{
    LOG((""));
}


bool toolbar_text_input(struct s_toolbar *tb, char *text)
{
    bool handled = true;

    LOG((""));

    return(handled);
}

bool toolbar_key_input(struct s_toolbar *tb, short nkc)
{
    bool handled = true;

    LOG((""));

    return(handled);
}


void toolbar_mouse_input(struct s_toolbar *tb, short mx, short my)
{
    LOG((""));
}



void toolbar_get_grect(struct s_toolbar *tb, short which, short opt, GRECT *dst)
{

}


struct text_area *toolbar_get_textarea(struct s_toolbar *tb,
                                       enum toolbar_textarea which)
{
    return(tb->url.textarea);
}


/* public event handler */
void toolbar_back_click(struct s_toolbar *tb)
{
    assert(input_window != NULL);

	struct browser_window *bw = input_window->browser->bw;

	if( history_back_available(bw->history) )
		history_back(bw, bw->history);
}

void toolbar_reload_click(struct s_toolbar *tb)
{
    assert(input_window != NULL);
	browser_window_reload(input_window->browser->bw, true);
}

void toolbar_forward_click(struct s_toolbar *tb)
{
    assert(input_window != NULL);
	struct browser_window *bw = input_window->browser->bw;
	if (history_forward_available(bw->history))
		history_forward(bw, bw->history);
}

void toolbar_home_click(struct s_toolbar *tb)
{
    assert(input_window != NULL);
    struct browser_window * bw;
    struct gui_window * gw;

    gw = window_get_active_gui_window(tb->owner);
    bw = gw->browser->bw;
	browser_window_go(bw, option_homepage_url, 0, true);
}


void toolbar_stop_click(struct s_toolbar *tb)
{
    assert(input_window != NULL);
	browser_window_stop(input_window->browser->bw);
}

