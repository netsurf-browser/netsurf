/**
 * $Id: plugin.h,v 1.1 2003/05/31 18:47:00 jmb Exp $
 */

#ifndef _NETSURF_RISCOS_PLUGIN_H_
#define _NETSURF_RISCOS_PLUGIN_H_

struct plugin_object {

        char* data;
        char* src;
        char* type;
        char* codetype;
        char* codebase;
        char* classid;
        char* paramds;       /* very likely to change */
        unsigned int* width;
        unsigned int* height;

};


void plugin_fetch(struct content* content, char* url, struct box* box,
                  struct plugin_object* po);

#endif
