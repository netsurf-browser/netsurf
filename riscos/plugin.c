/**
 * $Id: plugin.c,v 1.1 2003/05/31 18:47:00 jmb Exp $
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

        ret = strrchr(data, '.');
        LOG(("ret = %s", ++ret));

        if ((stricmp(ret,"jpg")) == 0 || (stricmp(ret,"jpeg")) == 0) {
                strcpy(ret,"image/jpeg");
                LOG(("jpeg image"));

        } else if ((stricmp(ret,"png")) == 0) {
                strcpy(ret,"image/png");
                LOG(("png image"));

        } /*else if ((stricmp(ret, "gif")) == 0) {
                ret = "image/gif";
        }*/ else {

                ret = NULL;
        }

        return ret;
}
