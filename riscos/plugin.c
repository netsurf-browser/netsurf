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
	/* we can't create the plugin here, because this is only called
	 * once, even if the object appears several times */
}


/**
 * plugin_add_user
 *
 * The content has been added to a page somewhere: launch the plugin.
 * This may be called anytime after plugin_create any number of times.
 * Each must launch a new plugin.
 */
void plugin_add_user(struct content *c, struct object_params *params)
{
	assert(params != 0);
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
 * plugin_remove_user
 *
 * A plugin is no longer required, eg. the page containing it has
 * been closed.
 */
void plugin_remove_user(struct content *c, struct object_params *params)
{
	assert(params != 0);
}



static const char * const ALIAS_PREFIX = "Alias$@PlugInType_";

/**
 * plugin_handleable
 * Tests whether we can handle an object using a browser plugin
 * returns TRUE if we can handle it, FALSE if we can't.
 */
bool plugin_handleable(const char *mime_type)
{
  char *sysvar;
  unsigned int *fv;
  os_error *e;

  /* prefix + 3 for file type + 1 for terminating \0 */
  sysvar = xcalloc(strlen(ALIAS_PREFIX)+4, sizeof(char));

  e = xmimemaptranslate_mime_type_to_filetype(mime_type, (bits *) &fv);

  sprintf(sysvar, "%s%x", ALIAS_PREFIX, e == NULL ? fv : 0 );
  if (getenv(sysvar) == 0)
	  return false;
  return true;
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

  /* By now, we've got the plugin up and running in a nested window
   * off the viewable page area. Now we want to display it in its place.
   * Therefore, broadcast a Message_PlugIn_Reshape (&4D544) with the values
   * given to us.
   */
}
