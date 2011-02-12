#ifndef NS_ATARI_SEARCH_H
#define NS_ATARI_SEARCH_H

#define SEARCH_MAX_SLEN 24

struct s_search_form_state
{
	char text[32];
	uint32_t flags;
};

struct s_search_form_session {
	struct browser_window * bw;
	WINDOW * formwind;
	struct s_search_form_state state;
};


typedef struct s_search_form_session * SEARCH_FORM_SESSION;

SEARCH_FORM_SESSION open_browser_search(struct gui_window * gw);
void search_destroy( struct gui_window * gw );

#endif