/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
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
