/**
 * $Id: plugin.c,v 1.5 2003/06/06 02:30:00 jmb Exp $
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

char* create_mime_from_ext(char* data);
char* create_sysvar(char* mime);
void plugin_fetch(/* vars here */);

/**
 * plugin_decode
 * Processes the contents of the plugin_object struct (defined in plugin.h)
 * in order to work out if NetSurf can handle the object.
 * For more information, read
 * http://www.ecs.soton.ac.uk/~jmb202/riscos/acorn/browse-plugins.html
 * as this code is based heavily on that description.
 */
void plugin_decode(struct content* content, char* url, struct box* box,
                  struct plugin_object* po) {


        content_type mime_type;
        bool can_handle = TRUE;
        char* alias_sysvar;

        if (po->data != NULL) {

                if (po->type != NULL) {

                        /* acquire NS mime type from actual mime type */
                        mime_type = content_lookup((const char*)po->type);
                }
                else {

                        /* create actual mime type - if we're wrong,
                         * it doesn't matter as the HTTP content-type
                         * header should override whatever we think.
                         * however, checking the header hasn't been
                         * implemented yet so it will just b0rk :(
                         */
                        po->type = create_mime_from_ext(po->data);

                        if (po->type != NULL)
                          mime_type = content_lookup((const char*)po->type);

                        else {

                          /* failed to create mime type, clean up and exit */
                          xfree(po);
                          can_handle = FALSE;
                        }

                }
        }
        else {

                /* no data so try using classid instead */

                po->data = strdup(po->classid);

                if (po->data != NULL) {

                        if (strnicmp(po->data,"clsid:",6) == 0) {

                                /* We can't handle ActiveX objects */
                                xfree(po);
                                can_handle = FALSE;
                        }

                        if (po->codetype != NULL) {

                                /* use codetype instead of type if we can */
                                po->type = strdup(po->codetype);
                                mime_type = content_lookup(
                                                 (const char*)po->codetype);
                        }
                        else {
                                /* try ye olde file extension munging */
                                po->codetype = create_mime_from_ext(
                                                                  po->data);

                                if (po->codetype != NULL) {

                                  /* well, it appeared to work... */
                                  mime_type = content_lookup(
                                                 (const char*)po->codetype);
                                  po->type = strdup(po->codetype);
                                }
                                else {

                                  /* arse, failed. oh well */
                                  xfree(po);
                                  can_handle = FALSE;
                                }
                        }
                }
                else {

                        /* we don't have sufficient data to handle this
                         * object :(
                         * TODO: start fetch anyway and check header.
                         *       if we can handle the content, continue
                         *       fetch and carry on as if the proper HTML
                         *       was written.
                         *       if we can't handle the content, stop fetch
                         *       and clean up.
                         */
                        xfree(po);
                        can_handle = FALSE;
                }
        }


        /* so, you think you can handle it do you? */
        if (can_handle == TRUE) {

                /* We think we can handle this object. Now check that
                 * we can.
                 * 1) Is it an image? Yes - send to image handler
                 *                    No - continue checking
                 * 2) Is a suitable Alias$... System Variable set?
                 *    Yes - invoke plugin
                 *    No - we can't handle it. Display alternative HTML
                 */


                /* TODO: There must be a better way than this...
                 *       Perhaps checking if the mime type begins "image/"
                 *       would be better?
                 */
                if (mime_type == CONTENT_JPEG || mime_type == CONTENT_PNG
                   || mime_type == CONTENT_GIF) {

                    /* OK, we have an image. Let's make the image handler
                     * deal with it.
                     */
                        xfree(po);
                        LOG(("sending data to image handler"));
                        /* TODO - get image handler to draw it */
                        /*html_fetch_image(content, url, box);*/
                }
                else { /* not an image; is sys var set? */

                        /* Create Alias variable */
                        alias_sysvar = create_sysvar(po->type);
                        if (alias_sysvar == NULL) {

                                /* oh dear, you can't handle it */
                                xfree(po);
                                xfree(alias_sysvar);
                                can_handle = FALSE;
                        }
                        else {

                                /* Right, we have a variable.
                                 * Does it actually exist?
                                 */
                                 int used;
                                 xos_read_var_val_size(
                                              (const char*)alias_sysvar,
                                              0, os_VARTYPE_STRING,
                                              &used, 0, os_VARTYPE_STRING);

                                if (used == 0) {

                                        /* no, doesn't exist */
                                        xfree(po);
                                        xfree(alias_sysvar);
                                        can_handle = FALSE;
                                }
                                else {
                                        /* yes, it exists */
                                        LOG(("%s exists", alias_sysvar));
                                        plugin_fetch(/* insert vars here */);
                                }
                        }
                }
        }

        if (can_handle == FALSE) {

                /* Get alternative HTML as we can't handle the object */
        }
}

/**
 * create_mime_from_ext
 * attempts to create a mime type from the filename extension.
 * returns NULL if it fails.
 */

char* create_mime_from_ext(char* data){

        char* ret;
        os_error *e;

        LOG(("Creating Mime Type from File Extension"));

        ret = strrchr(data, '.');
        LOG(("Extension = %s", ret));

        /* Let's make the mime map module do the work for us */
        e = xmimemaptranslate_extension_to_mime_type((const char*)ret,
                                                      ret);
        LOG(("Mime Type = %s", ret));

        if (e != NULL) ret = NULL;

        return ret;
}

/**
 * create_sysvar
 * attempts to create a system variable of the form Alias$@PlugInType_XXX
 * where XXX is the filetype.
 * returns NULL if unsuccessful.
 */

char* create_sysvar(char* mime) {

        char* ret;
        char* ft;
        unsigned int* fv;
        os_error *e;

        LOG(("Creating System Variable from Mime Type"));

        ret = xcalloc(22, sizeof(char));
        ret = strdup("Alias$@PlugInType_");

        LOG(("Mime Type: %s", mime));

        e = xmimemaptranslate_mime_type_to_filetype((const char*)mime,
                                                     (unsigned int*)&fv);
        if (e != NULL) ret = NULL;

        else {

                sprintf(ft, "%x", (int)fv);
                strcat(ret, ft);
                LOG(("Alias Var: %s", ret));
        }

        return ret;
}

/**
 * plugin_fetch
 * attempts to negotiate with the plugin.
 * also fetches the object for the plugin to handle.
 */
void plugin_fetch (/* insert vars here */) {

}
