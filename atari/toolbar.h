#ifndef NS_ATARI_TOOLBAR_H
#define NS_ATARI_TOOLBAR_H

#include <stdbool.h>
#include <stdint.h>

#include "desktop/textarea.h"
#include "desktop/browser.h"

struct s_toolbar;

enum toolbar_textarea {
    URL_INPUT_TEXT_AREA = 1
};

struct s_url_widget
{
	struct textarea *textarea;
	GRECT area;
};

struct s_throbber_widget
{
	short index;
	short max_index;
	bool running;
};

struct s_toolbar
{
	struct s_gui_win_root *owner;
	struct s_url_widget url;
	struct s_throbber_widget throbber;
	GRECT area;
	/* size & location of buttons: */
	struct s_tb_button * buttons;
	int btcnt;
	int style;
	bool attached;
	bool redraw;
    bool reflow;
    bool visible;
    bool search_visible;
};


void toolbar_init(void);
struct s_toolbar *toolbar_create(struct s_gui_win_root *owner);
void toolbar_destroy(struct s_toolbar * tb);
void toolbar_exit( void );
bool toolbar_text_input(struct s_toolbar *tb, char *text);
bool toolbar_key_input(struct s_toolbar *tb, short nkc);
void toolbar_mouse_input(struct s_toolbar *tb, short obj, short mbut);
void toolbar_update_buttons(struct s_toolbar *tb, struct browser_window *bw,
                            short idx);
void toolbar_get_grect(struct s_toolbar *tb, short which, GRECT *g);
struct textarea *toolbar_get_textarea(struct s_toolbar *tb,
                                       enum toolbar_textarea which);
void toolbar_set_throbber_state(struct s_toolbar *tb, bool active);
void toolbar_set_attached(struct s_toolbar *tb, bool attached);
void toolbar_set_visible(struct s_toolbar *tb, short area, bool visible);
void toolbar_set_reflow(struct s_toolbar *tb, bool do_reflow);
void toolbar_set_width(struct s_toolbar *tb, short w);
void toolbar_set_origin(struct s_toolbar *tb, short x, short y);
void toolbar_set_dimensions(struct s_toolbar *tb, GRECT *area);
void toolbar_set_url(struct s_toolbar *tb, const char *text);
void toolbar_redraw(struct s_toolbar *tb, GRECT *clip);
void toolbar_throbber_progress(struct s_toolbar *tb);
/* public events handlers: */
void toolbar_back_click(struct s_toolbar *tb);
void toolbar_reload_click(struct s_toolbar *tb);
void toolbar_forward_click(struct s_toolbar *tb);
void toolbar_home_click(struct s_toolbar *tb);
void toolbar_stop_click(struct s_toolbar *tb);


#endif
