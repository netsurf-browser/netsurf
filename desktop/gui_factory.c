
#include "desktop/gui.h"
#include "desktop/gui_factory.h"

struct gui_table *guit = NULL;


static void gui_default_quit(void)
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

	/* fill in the optional entries with defaults */
	if (gt->quit == NULL) {
		gt->quit = &gui_default_quit;
	}

	guit = gt;

	return NSERROR_OK;
}
