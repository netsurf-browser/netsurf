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

bool plugin_handleable(struct content* c);

/**
 * plugin_decode
 * This function checks that the contents of the plugin_object struct
 * are valid. If they are, it initiates the fetch process. If they are
 * not, it exits, leaving the box structure as it was on entry. This is
 * necessary as there are multiple ways of declaring an object's attributes.
 *
 * TODO: alt html
 *       params - create parameters file and put the filename string
 *                somewhere such that it is accessible from plugin_create.
 */
void plugin_decode(struct content* content, char* url, struct box* box,
                  struct plugin_object* po)
{
  os_error *e;
  unsigned int *fv;

  /* Check if the codebase attribute is defined.
   * If it is not, set it to the codebase of the current document.
   */
   if(po->codebase == 0)
           po->codebase = strdup(content->url);
   else
           po->codebase = url_join(po->codebase, content->url);

  /* Check that we have some data specified.
   * First, check the data attribute.
   * Second, check the classid attribute.
   * The data attribute takes precedence.
   * If neither are specified or if classid begins "clsid:",
   * we can't handle this object.
   */
   if(po->data == 0 && po->classid == 0) {
           xfree(po);
           return;
   }
   if(po->data == 0 && po->classid != 0) {
           if(strnicmp(po->classid, "clsid:", 6) == 0) {
                   LOG(("ActiveX object - n0"));
                   xfree(po);
                   return;
           }
           else {
                   url = url_join(po->classid, po->codebase);
           }
   }
   else {
           url = url_join(po->data, po->codebase);
   }

   /* Check if the declared mime type is understandable.
    * ie. is it referenced in the mimemap file?
    * Checks type and codetype attributes.
    */
    if(po->type != 0) {
          e = xmimemaptranslate_mime_type_to_filetype((const char*)po->type,
                                                      (unsigned int*)&fv);
          LOG(("fv: &%x", (int) fv));
          if(e != NULL) {
                  xfree(po);
                  return;
          }
          /* If a filetype of &ffd (Data) is returned,
           * one of the following mime types is possible :
           * application/octet-stream
           * multipart/x-mixed-replace
           * unknown mime type (* / *)
           * we assume it to be the last one as the other two
           * are unlikely to occur in an <object> definition.
           */
          if((int)fv == 0xffd) {
                  xfree(po);
                  return;
          }
          /* TODO: implement GUI for iframes/frames
           * For now, we just discard the data and
           * render the alternative html
           */
          if((int)fv == 0xfaf) {
                  xfree(po);
                  return;
          }
    }
    if(po->codetype != 0) {
      e = xmimemaptranslate_mime_type_to_filetype((const char*)po->codetype,
                                                  (unsigned int*)&fv);
          if(e != NULL) {
                  xfree(po);
                  return;
          }
          /* If a filetype of &ffd (Data) is returned,
           * one of the following mime types is possible :
           * application/octet-stream
           * multipart/x-mixed-replace
           * unknown mime type (* / *)
           * we assume it to be the last one as the other two
           * are unlikely to occur in an <object> definition.
           */
          if((int)fv == 0xffd) {
                  xfree(po);
                  return;
          }
          /* TODO: implement GUI for iframes/frames
           * For now, we just discard the data and
           * render the alternative html
           */
          if((int)fv == 0xfaf) {
                  xfree(po);
                  return;
          }
    }

  /* If we've got to here, the object declaration has provided us with
   * enough data to enable us to have a go at downloading and displaying it.
   */
   xfree(po);
   html_fetch_object(content, url, box);
}

/**
 * plugin_create
 * initialises plugin system in readiness for recieving object data
 *
 * TODO: implement aborting the fetch
 *       get parameter filename from wherever it was put by plugin_decode
 *       launch plugin system
 */
void plugin_create(struct content *c)
{
  bool can_handle = TRUE; /* we assume we can handle all types */

  LOG(("mime type: %s", c->mime_type));

  /* check if we can handle this type */
  can_handle = plugin_handleable(c);
  LOG(("can_handle = %s", can_handle ? "TRUE" : "FALSE"));
  LOG(("sysvar: %s", can_handle ? c->data.plugin.sysvar : "not set"));

  if(!can_handle) {
          /* TODO: need to find a way of stopping the fetch
           * if we can't handle this type
           */
  }

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



  /* Recheck if can_handle is false. If it is, stop fetch and exit .*/
  if(!can_handle) {
          /* TODO: need to find a way of stopping the fetch
           * if we can't handle this type
           */
  }

}

static const char * const ALIAS_PREFIX = "Alias$@PlugInType_";

/**
 * plugin_handleable
 * Tests whether we can handle an object using a browser plugin
 * returns TRUE if we can handle it, FALSE if we can't.
 */
bool plugin_handleable(struct content* c)
{
  bool ret = TRUE;
  char *sysvar;
  unsigned int *fv;
  int used;
  os_error *e;

  /* prefix + 3 for file type + 1 for terminating \0 */
  sysvar = xcalloc(strlen(ALIAS_PREFIX)+4, sizeof(char));

  e = xmimemaptranslate_mime_type_to_filetype((const char*)c->mime_type,
                                               (unsigned int*)&fv);

  sprintf(sysvar, "%s%x", ALIAS_PREFIX, e == NULL ? (int)fv : 0 );

  xos_read_var_val_size((const char*)sysvar,0, os_VARTYPE_STRING,
                                            &used, 0, os_VARTYPE_STRING);

  if(used == 0)
          /* No system variable set => no plugin available */
          ret = FALSE;

  if(ret)
          c->data.plugin.sysvar = strdup(sysvar);

  xfree(sysvar);

  return ret;
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
