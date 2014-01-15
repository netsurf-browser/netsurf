
#include "desktop/gui.h"
#include "desktop/gui_factory.h"

struct gui_table *guit = NULL;




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

static bool gui_default_window_drag_start(struct gui_window *g,
					  gui_drag_type type,
					  const struct rect *rect)
{
	return true;
}

static void gui_default_window_save_link(struct gui_window *g,
					 const char *url,
					 const char *title)
{
}

static void gui_default_window_set_icon(struct gui_window *g,
					hlcache_handle *icon)
{
}

static void gui_default_window_scroll_visible(struct gui_window *g,
				       int x0, int y0,
				       int x1, int y1)
{
	guit->window->set_scroll(g, x0, y0);
}

static void gui_default_window_new_content(struct gui_window *g)
{
}


static bool gui_default_window_scroll_start(struct gui_window *g)
{
	return true;
}

static void gui_default_window_set_pointer(struct gui_window *g,
					   gui_pointer_shape shape)
{
}

static void gui_default_window_set_status(struct gui_window *g,
					  const char *text)
{
}

static void gui_default_window_place_caret(struct gui_window *g,
					   int x, int y, int height,
					   const struct rect *clip)
{
}

static void gui_default_window_remove_caret(struct gui_window *g)
{
}

static void gui_default_window_file_gadget_open(struct gui_window *g,
						hlcache_handle *hl,
						struct form_control *gadget)
{
}

static void gui_default_window_drag_save_object(struct gui_window *g,
						hlcache_handle *c,
						gui_save_type type)
{
}

static void gui_default_window_drag_save_selection(struct gui_window *g,
						   const char *selection)
{
}

static void gui_default_window_start_selection(struct gui_window *g)
{
}


/** verify window table is valid */
static nserror verify_window_register(struct gui_window_table *gwt)
{
	/* check table is present */
	if (gwt == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	/* check the mandantory fields are set */
	if (gwt->create == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (gwt->destroy == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (gwt->redraw == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (gwt->update == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (gwt->get_scroll == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (gwt->set_scroll == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (gwt->get_dimensions == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (gwt->update_extent == NULL) {
		return NSERROR_BAD_PARAMETER;
	}


	/* fill in the optional entries with defaults */
	if (gwt->set_title == NULL) {
		gwt->set_title = gui_default_window_set_title;
	}
	if (gwt->set_url == NULL) {
		gwt->set_url = gui_default_window_set_url;
	}
	if (gwt->set_icon == NULL) {
		gwt->set_icon = gui_default_window_set_icon;
	}
	if (gwt->set_status == NULL) {
		gwt->set_status = gui_default_window_set_status;
	}
	if (gwt->set_pointer == NULL) {
		gwt->set_pointer = gui_default_window_set_pointer;
	}
	if (gwt->place_caret == NULL) {
		gwt->place_caret = gui_default_window_place_caret;
	}
	if (gwt->remove_caret == NULL) {
		gwt->remove_caret = gui_default_window_remove_caret;
	}
	if (gwt->start_throbber == NULL) {
		gwt->start_throbber = gui_default_window_start_throbber;
	}
	if (gwt->stop_throbber == NULL) {
		gwt->stop_throbber = gui_default_window_stop_throbber;
	}
	if (gwt->drag_start == NULL) {
		gwt->drag_start = gui_default_window_drag_start;
	}
	if (gwt->save_link == NULL) {
		gwt->save_link = gui_default_window_save_link;
	}
	if (gwt->scroll_visible == NULL) {
		gwt->scroll_visible = gui_default_window_scroll_visible;
	}
	if (gwt->new_content == NULL) {
		gwt->new_content = gui_default_window_new_content;
	}
	if (gwt->scroll_start == NULL) {
		gwt->scroll_start = gui_default_window_scroll_start;
	}
	if (gwt->file_gadget_open == NULL) {
		gwt->file_gadget_open = gui_default_window_file_gadget_open;
	}
	if (gwt->drag_save_object == NULL) {
		gwt->drag_save_object = gui_default_window_drag_save_object;
	}
	if (gwt->drag_save_selection == NULL) {
		gwt->drag_save_selection = gui_default_window_drag_save_selection;
	}
	if (gwt->start_selection == NULL) {
		gwt->start_selection = gui_default_window_start_selection;
	}

	return NSERROR_OK;
}



static struct gui_download_window *
gui_default_download_create(download_context *ctx, struct gui_window *parent)
{
	return NULL;
}

static nserror gui_default_download_data(struct gui_download_window *dw,
				  const char *data, unsigned int size)
{
	return NSERROR_OK;
}

static void gui_default_download_error(struct gui_download_window *dw,
				const char *error_msg)
{
}

static void gui_default_download_done(struct gui_download_window *dw)
{
}


/** verify download window table is valid */
static nserror verify_download_register(struct gui_download_table *gdt)
{
	/* check table is present */
	if (gdt == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	/* all enties are mandantory */
	if (gdt->create == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (gdt->data == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (gdt->error == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (gdt->done == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	return NSERROR_OK;
}

static void gui_default_get_clipboard(char **buffer, size_t *length)
{
	*buffer = NULL;
	*length = 0;
}

static void gui_default_set_clipboard(const char *buffer, size_t length,
		nsclipboard_styles styles[], int n_styles)
{
}

static nserror verify_clipboard_register(struct gui_clipboard_table *gct)
{
	/* check table is present */
	if (gct == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	/* optional operations */
	if (gct->get == NULL) {
		gct->get = gui_default_get_clipboard;
	}
	if (gct->set == NULL) {
		gct->set = gui_default_set_clipboard;
	}
	return NSERROR_OK;
}


static void gui_default_quit(void)
{
}

static void gui_default_set_search_ico(hlcache_handle *ico)
{
}

static nsurl *gui_default_get_resource_url(const char *path)
{
	return NULL;
}

static void gui_default_launch_url(const char *url)
{
}

static void gui_default_create_form_select_menu(struct browser_window *bw,
						struct form_control *control)
{
}


static void gui_default_cert_verify(nsurl *url,
				    const struct ssl_cert_info *certs,
				    unsigned long num,
				    nserror (*cb)(bool proceed, void *pw),
				    void *cbpw)
{
	cb(false, cbpw);
}


static nserror verify_browser_register(struct gui_browser_table *gbt)
{
	/* check table is present */
	if (gbt == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	/* check the mandantory fields are set */
	if (gbt->poll == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	/* fill in the optional entries with defaults */
	if (gbt->quit == NULL) {
		gbt->quit = gui_default_quit;
	}
	if (gbt->set_search_ico == NULL) {
		gbt->set_search_ico = gui_default_set_search_ico;
	}
	if (gbt->get_resource_url == NULL) {
		gbt->get_resource_url = gui_default_get_resource_url;
	}
	if (gbt->launch_url == NULL) {
		gbt->launch_url = gui_default_launch_url;
	}
	if (gbt->create_form_select_menu == NULL) {
		gbt->create_form_select_menu = gui_default_create_form_select_menu;
	}
	if (gbt->cert_verify == NULL) {
		gbt->cert_verify = gui_default_cert_verify;
	}
	return NSERROR_OK;
}



static struct gui_download_table default_download_table = {
	.create = gui_default_download_create,
	.data = gui_default_download_data,
	.error = gui_default_download_error,
	.done = gui_default_download_done,
};

static struct gui_clipboard_table default_clipboard_table = {
	.get = gui_default_get_clipboard,
	.set = gui_default_set_clipboard,
};

nserror gui_factory_register(struct gui_table *gt)
{
	nserror err;

	/* ensure not already initialised */
	if (guit != NULL) {
		return NSERROR_INIT_FAILED;
	}

	/* check table is present */
	if (gt == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	/* browser table */
	err = verify_browser_register(gt->browser);
	if (err != NSERROR_OK) {
		return err;
	}

	/* window table */
	err = verify_window_register(gt->window);
	if (err != NSERROR_OK) {
		return err;
	}

	/* download table */
	if (gt->download == NULL) {
		/* set default download table */
		gt->download = &default_download_table;
	}
	err = verify_download_register(gt->download);
	if (err != NSERROR_OK) {
		return err;
	}

	/* clipboard table */
	if (gt->clipboard == NULL) {
		/* set default clipboard table */
		gt->clipboard = &default_clipboard_table;
	}
	err = verify_clipboard_register(gt->clipboard);
	if (err != NSERROR_OK) {
		return err;
	}

	guit = gt;

	return NSERROR_OK;
}
