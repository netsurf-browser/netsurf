/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netsurf/content/content.h"
#include "netsurf/render/html.h"
#include "netsurf/riscos/plugin.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#include "oslib/mimemap.h"


/**
 * plugin_create
 * initialises plugin system in readiness for recieving object data
 *
 * TODO: implement aborting the fetch
 */
void plugin_create(struct content *c)
{
	c->data.plugin.data = xcalloc(0, 1);
	c->data.plugin.length = 0;
	/* we can't create the plugin here, because this is only called
	 * once, even if the object appears several times */
}


/**
 * plugin_add_instance
 *
 * The content has been added to a page somewhere: launch the plugin.
 * This may be called anytime after plugin_create any number of times.
 * Each must launch a new plugin.
 *
 * bw is the window which the plugin is in
 * page, box, params are 0 if the object is standalone
 * state may be used to store a pointer to state data
 */
void plugin_add_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state)
{
  /* ok, it looks like we can handle this object.
   * Broadcast Message_PlugIn_Open (&4D540) and listen for response
   * Message_PlugIn_Opening (&4D541). If no response, try to launch
   * plugin by Wimp_StartTask(sysvar). Then re-broadcast Message_PlugIn_Open
   * and listen for response. If there is still no response, give up and set
   * can_handle to FALSE.
   * NB: For the bounding box in Message_PlugIn_Open, we choose arbitrary
   *     values outside the area displayed. This is corrected when
   *     plugin_redraw is called.
   */
}


/**
 * plugin_remove_instance
 *
 * A plugin is no longer required, eg. the page containing it has
 * been closed.
 *
 * Any storage associated with state must be freed.
 */
void plugin_remove_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state)
{
}


/**
 * plugin_reshape_instance
 *
 * The box containing the plugin has moved or resized,
 * or the window containing the plugin has resized if standalone.
 */
void plugin_reshape_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state)
{
  /* By now, we've got the plugin up and running in a nested window
   * off the viewable page area. Now we want to display it in its place.
   * Therefore, broadcast a Message_PlugIn_Reshape (&4D544) with the values
   * given to us.
   */
}


static const char * const ALIAS_PREFIX = "Alias$@PlugInType_";

/**
 * plugin_handleable
 * Tests whether we can handle an object using a browser plugin
 * returns TRUE if we can handle it, FALSE if we can't.
 */
bool plugin_handleable(const char *mime_type)
{
  char sysvar[40];  /* must be sufficient for ALIAS_PREFIX and a hex number */
  unsigned int *fv;
  os_error *e;

  e = xmimemaptranslate_mime_type_to_filetype(mime_type, (bits *) &fv);
  if (e) {
    LOG(("xmimemaptranslate_mime_type_to_filetype failed: %s", e->errmess));
    return FALSE;
  }

  sprintf(sysvar, "%s%x", ALIAS_PREFIX, fv);
  if (getenv(sysvar) == 0)
	  return FALSE;
  return TRUE;
}

/**
 * plugin_process_data
 * processes data retrieved by the fetch process
 *
 * TODO: plugin stream protocol
 *
 */
void plugin_process_data(struct content *c, char *data, unsigned long size)
{

  /* If the plugin requests, we send the data to it via the
   * plugin stream protocol.
   * Also, we should listen for Message_PlugIn_URL_Access (&4D54D)
   * as the plugin may need us to retrieve URLs for it.
   * We should also listen for Message_PlugIn_Closed (&4D543).
   * If this occurs, the plugin has exited with an error.
   * Therefore, we need to stop the fetch and exit.
   */

  /* I think we should just buffer the data here, in case the
   * plugin requests it sometime in the future. - James */

	c->data.plugin.data = xrealloc(c->data.plugin.data, c->data.plugin.length + size);
	memcpy(c->data.plugin.data + c->data.plugin.length, data, size);
	c->data.plugin.length += size;
	c->size += size;
}

/**
 * plugin_convert
 * This isn't needed by the plugin system as all the data processing is done
 * externally. Therefore, just tell NetSurf that everything's OK.
 */
int plugin_convert(struct content *c, unsigned int width, unsigned int height)
{
  c->status=CONTENT_STATUS_DONE;
  return 0;
}

void plugin_revive(struct content *c, unsigned int width, unsigned int height)
{
}

void plugin_reformat(struct content *c, unsigned int width, unsigned int height)
{
}

/**
 * plugin_destroy
 * we've finished with this data, destroy it. Also, shutdown plugin.
 *
 * TODO: clean up
 */
void plugin_destroy(struct content *c)
{
}

/**
 * plugin_redraw
 * redraw plugin on page.
 *
 * TODO: Message_PlugIn_Reshape
 */
void plugin_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height)
{

}
