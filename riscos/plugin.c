/**
 * $Id: plugin.c,v 1.2 2003/06/02 21:09:50 jmb Exp $
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

void plugin_fetch(struct content* content, char* url, struct box* box,
                  struct plugin_object* po) {


        content_type mime_type;

        if (po->data != NULL) {

                if (po->type != NULL) {

                        mime_type = content_lookup((const char*)po->type);
                }
                else {

                        po->type = create_mime_from_ext(po->data);

                        if (po->type != NULL)
                          mime_type = content_lookup((const char*)po->type);

                }

                /* OK, we have an image. Let's make the image handler
                   deal with it */
                if (mime_type == CONTENT_JPEG || mime_type == CONTENT_PNG) {

                        xfree(po);
                        LOG(("sending data to image handler"));
                        /* TODO - stop segfault when redrawing window */
                        /*html_fetch_image(content, url, box);*/
                }
        }
        else {


        }
        /* TODO - this function.*/

}

/**
 * create_mime_from_ext
 * attempts to create a mime type from the filename extension.
 * returns NULL if it fails.
 */

char* create_mime_from_ext(char* data){

        char* ret;
        os_error *e;

        ret = strrchr(data, '.');
        LOG(("Extension = %s", ret));

        /* Let's make the mime map module do the work for us */
        e = xmimemaptranslate_extension_to_mime_type((const char*)ret,
                                                      ret);
        LOG(("Mime Type = %s", ret));

        if(e != NULL) ret = NULL;

        return ret;
}
