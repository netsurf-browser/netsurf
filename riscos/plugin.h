/**
 * $Id: plugin.h,v 1.3 2003/06/06 03:12:28 jmb Exp $
 */

#ifndef _NETSURF_RISCOS_PLUGIN_H_
#define _NETSURF_RISCOS_PLUGIN_H_

struct plugin_object {

        char* data;
        char* type;
        char* codetype;
        char* codebase;
        char* classid;
        char* paramds;       /* very likely to change */
        unsigned int* width;
        unsigned int* height;

};


void plugin_decode(struct content* content, char* url, struct box* box,
                  struct plugin_object* po);

#endif
