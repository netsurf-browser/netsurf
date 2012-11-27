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

void toolbar_init(void);
struct s_toolbar *toolbar_create(struct s_gui_win_root *owner);
void toolbar_destroy(struct s_toolbar * tb);
void toolbar_exit( void );
void toolbar_set_dimensions(struct s_toolbar *tb, GRECT *area);
void toolbar_set_url(struct s_toolbar *tb, const char *text);
bool toolbar_text_input(struct s_toolbar *tb, char *text);
bool toolbar_key_input(struct s_toolbar *tb, short nkc);
void toolbar_mouse_input(struct s_toolbar *tb, short mx, short my);
void toolbar_update_buttons(struct s_toolbar *tb, struct browser_window *bw,
                            short idx);
void toolbar_get_grect(struct s_toolbar *tb, short which, short opt, GRECT *g);
struct text_area *toolbar_get_textarea(struct s_toolbar *tb,
                                       enum toolbar_textarea which);
/* public events handlers: */
void toolbar_back_click(struct s_toolbar *tb);
void toolbar_reload_click(struct s_toolbar *tb);
void toolbar_forward_click(struct s_toolbar *tb);
void toolbar_home_click(struct s_toolbar *tb);
void toolbar_stop_click(struct s_toolbar *tb);


#endif
