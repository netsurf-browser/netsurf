/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netsurf/content/content.h"
#include "netsurf/render/html.h"
#include "netsurf/render/box.h"
#include "netsurf/riscos/plugin.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#include "oslib/mimemap.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osgbpb.h"

void plugin_write_parameters_file(struct object_params *params);

/**
 * plugin_create
 * initialises plugin system in readiness for receiving object data
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
 * state may be use to store a pointer to state data
 */
void plugin_add_instance(struct content *c, struct browser_window *bw,
                struct content *page, struct box *box,
                struct object_params *params, void **state)
{
        char sysvar[40];

	assert(params != 0);

        /* write parameters file */
        plugin_write_parameters_file(params);

        plugin_create_sysvar(c->mime_type, &sysvar);


  /* Broadcast Message_PlugIn_Open (&4D540) and listen for response
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
	assert(params != 0);

        /* delete parameters file */
	xosfile_delete((char const*)params->filename, NULL, NULL, NULL, NULL, NULL);
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
 * plugin_create_sysvar
 * creates system variable from mime type
 */
void plugin_create_sysvar(const char *mime_type, char* sysvar)
{
  unsigned int *fv;
  os_error *e;

  e = xmimemaptranslate_mime_type_to_filetype(mime_type, (bits *) &fv);

  sprintf(sysvar, "%s%x", ALIAS_PREFIX, fv);
}

/**
 * plugin_handleable
 * Tests whether we can handle an object using a browser plugin
 * returns true if we can handle it, false if we can't.
 */
bool plugin_handleable(const char *mime_type)
{
  char sysvar[40];
  unsigned int *fv;
  os_error *e;

  e = xmimemaptranslate_mime_type_to_filetype(mime_type, (bits *) &fv);
  if (e) {
    LOG(("xmimemaptranslate_mime_type_to_filetype failed: %s", e->errmess));
    return false;
  }

  sprintf(sysvar, "%s%x", ALIAS_PREFIX, fv);
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


/**
 * plugin_write_parameters_file
 * Writes the parameters file.
 * Beware, this function is long and nasty. It appears to work, however.
 */
void plugin_write_parameters_file(struct object_params *params)
{
        struct plugin_params* temp;
        int *time;
        byte pdata[4] = {0, 0, 0, 0};
        os_fw pfile;
        int i, j, rsize;
        char *tstr;

        /* Create the file */
        xosfile_create_dir("<Wimp$ScrapDir>.WWW", 77);
        xosfile_create_dir("<Wimp$ScrapDir>.WWW.NetSurf", 77);
        /* path + filename + terminating NUL */
        params->filename = xcalloc(23+10+1 , sizeof(char));
        xos_read_monotonic_time((int*)&time);
        tstr = xcalloc(40, sizeof(char));
        sprintf(tstr, "%01u", (unsigned int)time<<8);
        sprintf(params->filename, "<Wimp$ScrapDir>.WWW.NetSurf.p%1.9s", tstr);
        xfree(tstr);
        LOG(("filename: %s", params->filename));

        xosfind_openoutw(osfind_NO_PATH, params->filename, NULL, &pfile);

        /* Write object attributes first */

        /* classid is checked first */
        if(params->classid != 0 && params->codetype != 0) {

          /* Record Type */
          pdata[0] = 1;
          xosgbpb_writew(pfile, pdata, 4, NULL);

          /* Record size */
          rsize = 0;
          rsize += (4 + 7 + 1);
          rsize += (4 + strlen(params->classid));
          if((strlen(params->classid)%4) != 0)
                  rsize += (4-(strlen(params->classid)%4));
          rsize += (4 + strlen(params->codetype));
          if((strlen(params->codetype)%4) != 0)
                  rsize += (4-(strlen(params->codetype)%4));

          pdata[0] = rsize & 0xff;
          pdata[1] = (rsize >> 0x08) & 0xff;
          pdata[2] = (rsize >> 0x10) & 0xff;
          pdata[3] = (rsize >> 0x18) & 0xff;
          xosgbpb_writew(pfile, pdata, 4, NULL);

          /* name */
          /* size */
          rsize = strlen("CLASSID");
          pdata[0] = rsize & 0xff;
          pdata[1] = (rsize >> 0x08) & 0xff;
          pdata[2] = (rsize >> 0x10) & 0xff;
          pdata[3] = (rsize >> 0x18) & 0xff;
          xosgbpb_writew(pfile, pdata, 4, NULL);

          /* name */
          xosgbpb_writew(pfile, (byte const*)"CLASSID", rsize, NULL);

          /* pad to word boundary */
          for(i=0; i!=4; i++)
                  pdata[i] = 0;
          xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);

          /* value */
          /* size */
          rsize = strlen(params->classid);
          pdata[0] = rsize & 0xff;
          pdata[1] = (rsize >> 0x08) & 0xff;
          pdata[2] = (rsize >> 0x10) & 0xff;
          pdata[3] = (rsize >> 0x18) & 0xff;
          xosgbpb_writew(pfile, pdata, 4, NULL);

          /* name */
          xosgbpb_writew(pfile, (byte const*)params->classid, rsize, NULL);

          /* pad to word boundary */
          for(i=0; i!=4; i++)
                  pdata[i] = 0;
          xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);

          /* type */
          /* size */
          rsize = strlen(params->codetype);
          pdata[0] = rsize & 0xff;
          pdata[1] = (rsize >> 0x08) & 0xff;
          pdata[2] = (rsize >> 0x10) & 0xff;
          pdata[3] = (rsize >> 0x18) & 0xff;
          xosgbpb_writew(pfile, pdata, 4, NULL);

          /* name */
          xosgbpb_writew(pfile, (byte const*)params->codetype, rsize, NULL);

          /* pad to word boundary */
          for(i=0; i!=4; i++)
                  pdata[i] = 0;
          xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);

        }
        /* otherwise, we check the data attribute */
        else if(params->data !=0 && params->type != 0) {

          /* Record Type */
          pdata[0] = 1;
          xosgbpb_writew(pfile, pdata, 4, NULL);

          /* Record size */
          rsize = 0;
          rsize += (4 + 4);
          rsize += (4 + strlen(params->data));
          if((strlen(params->data)%4) != 0)
                  rsize += (4-(strlen(params->data)%4));
          rsize += (4 + strlen(params->type));
          if((strlen(params->type)%4) != 0)
                  rsize += (4-(strlen(params->type)%4));
          pdata[0] = rsize & 0xff;
          pdata[1] = (rsize >> 0x08) & 0xff;
          pdata[2] = (rsize >> 0x10) & 0xff;
          pdata[3] = (rsize >> 0x18) & 0xff;
          xosgbpb_writew(pfile, pdata, 4, NULL);

          /* name */
          /* size */
          rsize = strlen("DATA");
          pdata[0] = rsize & 0xff;
          pdata[1] = (rsize >> 0x08) & 0xff;
          pdata[2] = (rsize >> 0x10) & 0xff;
          pdata[3] = (rsize >> 0x18) & 0xff;
          xosgbpb_writew(pfile, pdata, 4, NULL);

          /* name */
          xosgbpb_writew(pfile, (byte const*)"DATA", rsize, NULL);

          /* pad to word boundary */
          for(i=0; i!=4; i++)
                  pdata[i] = 0;
          xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);

          /* value */
          /* size */
          rsize = strlen(params->data);
          pdata[0] = rsize & 0xff;
          pdata[1] = (rsize >> 0x08) & 0xff;
          pdata[2] = (rsize >> 0x10) & 0xff;
          pdata[3] = (rsize >> 0x18) & 0xff;
          xosgbpb_writew(pfile, pdata, 4, NULL);

          /* name */
          xosgbpb_writew(pfile, (byte const*)params->data, rsize, NULL);

          /* pad to word boundary */
          for(i=0; i!=4; i++)
                  pdata[i] = 0;
          xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);

          /* type */
          /* size */
          rsize = strlen(params->type);
          pdata[0] = rsize & 0xff;
          pdata[1] = (rsize >> 0x08) & 0xff;
          pdata[2] = (rsize >> 0x10) & 0xff;
          pdata[3] = (rsize >> 0x18) & 0xff;
          xosgbpb_writew(pfile, pdata, 4, NULL);

          /* name */
          xosgbpb_writew(pfile, (byte const*)params->type, rsize, NULL);

          /* pad to word boundary */
          for(i=0; i!=4; i++)
                  pdata[i] = 0;
          xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);
        }

        /* if codebase is specified, write it as well */
        if(params->codebase != 0) {

                /* Record Type */
                pdata[0] = 1;
                xosgbpb_writew(pfile, pdata, 4, NULL);

                /* Record size */
                rsize = 0;
                rsize += (4 + 8 + 1);
                rsize += (4 + strlen(params->codebase));
                if((strlen(params->codebase)%4) != 0)
                        rsize += (4-(strlen(params->codebase)%4));
                pdata[0] = rsize & 0xff;
                pdata[1] = (rsize >> 0x08) & 0xff;
                pdata[2] = (rsize >> 0x10) & 0xff;
                pdata[3] = (rsize >> 0x18) & 0xff;
                xosgbpb_writew(pfile, pdata, 4, NULL);

                /* name */
                /* size */
                rsize = strlen("CODEBASE");
                pdata[0] = rsize & 0xff;
                pdata[1] = (rsize >> 0x08) & 0xff;
                pdata[2] = (rsize >> 0x10) & 0xff;
                pdata[3] = (rsize >> 0x18) & 0xff;
                xosgbpb_writew(pfile, pdata, 4, NULL);

                /* name */
                xosgbpb_writew(pfile, (byte const*)"CODEBASE", rsize, NULL);

                /* pad to word boundary */
                for(i=0; i!=4; i++)
                        pdata[i] = 0;
                xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);

                /* value */
                /* size */
                rsize = strlen(params->codebase);
                pdata[0] = rsize & 0xff;
                pdata[1] = (rsize >> 0x08) & 0xff;
                pdata[2] = (rsize >> 0x10) & 0xff;
                pdata[3] = (rsize >> 0x18) & 0xff;
                xosgbpb_writew(pfile, pdata, 4, NULL);

                /* name */
                xosgbpb_writew(pfile, (byte const*)params->codebase, rsize, NULL);

                /* pad to word boundary */
                for(i=0; i!=4; i++)
                        pdata[i] = 0;
                xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);

        }

        /* Iterate through the parameter list, creating the parameters
         * file as we go. We can free up the memory as we go.
         */
        while(params->params != 0) {

                LOG(("name: %s", params->params->name == 0 ? "not set" : params->params->name));
                LOG(("value: %s", params->params->value == 0 ? "not set" : params->params->value));
                LOG(("type: %s", params->params->type == 0 ? "not set" : params->params->type));
                LOG(("valuetype: %s", params->params->valuetype));


                /* Record Type */
                if(strcasecmp(params->params->valuetype, "data") == 0)
                        pdata[0] = 1;
                if(strcasecmp(params->params->valuetype, "ref") == 0)
                        pdata[0] = 2;
                if(strcasecmp(params->params->valuetype, "object") == 0)
                        pdata[0] = 3;
                xosgbpb_writew(pfile, pdata, 4, NULL);

                /* Record Size */
                rsize = 0;
                if(params->params->name != 0) {
                        rsize += 4;
                        rsize += strlen(params->params->name);
                        if((strlen(params->params->name) % 4) != 0)
                                rsize += (4 - (strlen(params->params->name) % 4));
                }
                if(params->params->value != 0) {
                        rsize += 4;
                        rsize += strlen(params->params->value);
                        if((strlen(params->params->value) % 4) != 0)
                                rsize += 4 - ((strlen(params->params->value) % 4));
                }
                if(params->params->type != 0) {
                        rsize += 4;
                        rsize += strlen(params->params->type);
                        if((strlen(params->params->type) % 4) != 0)
                                rsize += (4 - (strlen(params->params->type) % 4));
                }
                pdata[0] = rsize & 0xff;
                pdata[1] = (rsize >> 0x08) & 0xff;
                pdata[2] = (rsize >> 0x10) & 0xff;
                pdata[3] = (rsize >> 0x18) & 0xff;
                xosgbpb_writew(pfile, pdata, 4, NULL);

                /* Record Name */
                if(params->params->name != 0) {

                        /* Size */
                        rsize = strlen(params->params->name);
                        pdata[0] = rsize & 0xff;
                        pdata[1] = (rsize >> 0x08) & 0xff;
                        pdata[2] = (rsize >> 0x10) & 0xff;
                        pdata[3] = (rsize >> 0x18) & 0xff;
                        xosgbpb_writew(pfile, pdata, 4, NULL);

                        /* Name */
                        xosgbpb_writew(pfile, (byte const*)params->params->name, rsize, NULL);

                        /* Pad to word boundary */
                        for(i=0; i!=4; i++)
                                pdata[i] = 0;

                        xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);
                }

                /* Record Value */
                if(params->params->value != 0) {

                        /* Size */
                        rsize = strlen(params->params->value);
                        pdata[0] = rsize & 0xff;
                        pdata[1] = (rsize >> 0x08) & 0xff;
                        pdata[2] = (rsize >> 0x10) & 0xff;
                        pdata[3] = (rsize >> 0x18) & 0xff;
                        xosgbpb_writew(pfile, pdata, 4, NULL);

                        /* Name */
                        xosgbpb_writew(pfile, (byte const*)params->params->value, rsize, NULL);

                        /* Pad to word boundary */
                        for(i=0; i!=4; i++)
                                pdata[i] = 0;

                        xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);
                }

                /* Record Type */
                if(params->params->type != 0) {

                        /* Size */
                        rsize = strlen(params->params->type);
                        pdata[0] = rsize & 0xff;
                        pdata[1] = (rsize >> 0x08) & 0xff;
                        pdata[2] = (rsize >> 0x10) & 0xff;
                        pdata[3] = (rsize >> 0x18) & 0xff;
                        xosgbpb_writew(pfile, pdata, 4, NULL);

                        /* Name */
                        xosgbpb_writew(pfile, (byte const*)params->params->type, rsize, NULL);

                        /* Pad to word boundary */
                        for(i=0; i!=4; i++)
                                pdata[i] = 0;

                        xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);
                }

                temp = params->params;
                params->params = params->params->next;
                xfree(temp);
         }

         /* Now write mandatory special parameters
          *
          * Case:     Parameter:
          *
          *  0        BASEHREF
          *  1        USERAGENT
          *  2        UAVERSION
          *  3        APIVERSION
          *  4        BGCOLOR - needs fixing to work properly.
          *                     Currently, it assumes FFFFFF00 (BBGGRR00)
          */
         for(j=0; j!=5; j++) {

                 pdata[0] = 4;
                 xosgbpb_writew(pfile, pdata, 4, NULL);

                 switch(j) {

                         case 0: rsize = 0;
                                 rsize += (4 + 8);
                                 rsize += (4 + strlen(params->basehref));
                                 if((strlen(params->basehref)%4) != 0)
                                   rsize += (4-(strlen(params->basehref)%4));
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 rsize = strlen("BASEHREF");
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 xosgbpb_writew(pfile, (byte const*)"BASEHREF", rsize, NULL);

                                 for(i=0; i!=4; i++)
                                         pdata[i] = 0;
                                 xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);

                                 rsize = strlen(params->basehref);
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 xosgbpb_writew(pfile, (byte const*)params->basehref, rsize, NULL);

                                 for(i=0; i!=4; i++)
                                         pdata[i] = 0;
                                 xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);
                                 break;

                         case 1: rsize = 0;
                                 rsize += (4 + 9 + 3);
                                 rsize += (4 + strlen("NetSurf"));
                                 if((strlen("NetSurf")%4) != 0)
                                     rsize += (4-(strlen("NetSurf")%4));
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 rsize = strlen("USERAGENT");
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 xosgbpb_writew(pfile, (byte const*)"USERAGENT", rsize, NULL);

                                 for(i=0; i!=4; i++)
                                         pdata[i] = 0;
                                 xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);

                                 rsize = strlen("NetSurf");
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 xosgbpb_writew(pfile, (byte const*)"NetSurf", rsize, NULL);

                                 for(i=0; i!=4; i++)
                                         pdata[i] = 0;
                                 xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);
                                 break;

                         case 2: rsize = 0;
                                 rsize += (4 + 9 + 3);
                                 rsize += (4 + strlen("0.01"));
                                 if((strlen("0.01")%4) != 0)
                                     rsize += (4-(strlen("0.01")%4));
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 rsize = strlen("UAVERSION");
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 xosgbpb_writew(pfile, (byte const*)"UAVERSION", rsize, NULL);

                                 for(i=0; i!=4; i++)
                                         pdata[i] = 0;
                                 xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);

                                 rsize = strlen("0.01");
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 xosgbpb_writew(pfile, (byte const*)"0.01", rsize, NULL);

                                 for(i=0; i!=4; i++)
                                         pdata[i] = 0;
                                 xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);
                                 break;

                         case 3: rsize = 0;
                                 rsize += (4 + 10 + 2);
                                 rsize += (4 + strlen("1.10"));
                                 if((strlen("1.10")%4) != 0)
                                     rsize += (4-(strlen("1.10")%4));
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 rsize = strlen("APIVERSION");
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 xosgbpb_writew(pfile, (byte const*)"APIVERSION", rsize, NULL);

                                 for(i=0; i!=4; i++)
                                         pdata[i] = 0;
                                 xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);

                                 rsize = strlen("1.10");
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 xosgbpb_writew(pfile, (byte const*)"1.10", rsize, NULL);

                                 for(i=0; i!=4; i++)
                                         pdata[i] = 0;
                                 xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);
                                 break;
                         case 4: rsize = 0;
                                 rsize += (4 + 7 + 1);
                                 rsize += (4 + strlen("FFFFFF00"));
                                 if((strlen("FFFFFF00")%4) != 0)
                                     rsize += (4-(strlen("FFFFFF00")%4));
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 rsize = strlen("BGCOLOR");
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 xosgbpb_writew(pfile, (byte const*)"BGCOLOR", rsize, NULL);

                                 for(i=0; i!=4; i++)
                                         pdata[i] = 0;
                                 xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);

                                 rsize = strlen("FFFFFF00");
                                 pdata[0] = rsize & 0xff;
                                 pdata[1] = (rsize >> 0x08) & 0xff;
                                 pdata[2] = (rsize >> 0x10) & 0xff;
                                 pdata[3] = (rsize >> 0x18) & 0xff;
                                 xosgbpb_writew(pfile, pdata, 4, NULL);

                                 xosgbpb_writew(pfile, (byte const*)"FFFFFF00", rsize, NULL);

                                 for(i=0; i!=4; i++)
                                         pdata[i] = 0;
                                 xosgbpb_writew(pfile, pdata, (4 - ((rsize%4) == 0 ? 4 : (rsize%4))), NULL);
                                 break;
                 }

         }

         /* Write terminator */
         for(i=0; i!=4; i++)
                 pdata[i] = 0;

         xosgbpb_writew(pfile, pdata, 4, NULL);
         xosfind_closew(pfile);
}
