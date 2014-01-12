
#include "desktop/gui.h"
#include "desktop/gui_factory.h"

struct gui_table *guit = NULL;


static void gui_default_quit(void)
{
}

static void gui_default_window_set_title(struct gui_window *g, const char *title)
{
}

static void gui_default_window_set_url(struct gui_window *g, const char *url)
{
}

static void gui_default_window_start_throbber(struct gui_window *g)
{
}

static void gui_default_window_stop_throbber(struct gui_window *g)
{
}

nserror gui_factory_register(struct gui_table *gt)
{
	/* ensure not already initialised */
	if (guit != NULL) {
		return NSERROR_INIT_FAILED;
	}


	/* check the mandantory fields are set */
	if (gt->poll == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (gt->window_create == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (gt->window_destroy == NULL) {
		return NSERROR_BAD_PARAMETER;
	}


	/* fill in the optional entries with defaults */
	if (gt->quit == NULL) {
		gt->quit = gui_default_quit;
	}
	if (gt->window_set_title == NULL) {
		gt->window_set_title = gui_default_window_set_title;
	}
	if (gt->window_set_url == NULL) {
		gt->window_set_url = gui_default_window_set_url;
	}
	if (gt->window_start_throbber == NULL) {
		gt->window_start_throbber = gui_default_window_start_throbber;
	}
	if (gt->window_stop_throbber == NULL) {
		gt->window_stop_throbber = gui_default_window_stop_throbber;
	}

	guit = gt;

	return NSERROR_OK;
}
