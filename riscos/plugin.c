/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/*
 * TODO:
 *       - Reshaping plugin by request [Plugin_Reshape_Request (&4d545)]
 *       - Finish off stream protocol implementation
 *             [Plugin_Stream_Write (&4d54a), Plugin_Stream_Written (&4d54b)]
 *       - Parse and act upon the rest of the Plugin_Opening flags
 *       - Handle death of Plugin Task
 *       - Implement remaining messages [Plugin_URL_Access, Plugin_Focus,
 *              Plugin_Notify, Plugin_Busy, Plugin_Action, Plugin_Abort,
 *              Plugin_Inform(ed)?]
 *       - Handle standalone objects
 */

// #define NDEBUG

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/html.h"
#include "netsurf/render/box.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/plugin.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#include "oslib/mimemap.h"
#include "oslib/os.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osgbpb.h"
#include "oslib/plugin.h"
#include "oslib/wimp.h"

/* parameters file creation */
void plugin_write_parameters_file(struct object_params *params);
int plugin_calculate_rsize(char* name, char* data, char* mime);
struct plugin_param_item *plugin_add_item_to_pilist(struct plugin_param_item *pilist, int type, char* name, char* value, char* mime_type);

/* stream handling */
void plugin_create_stream(struct browser_window *bw,
                          struct object_params *params, struct content *c);
void plugin_write_stream_as_file(struct browser_window *bw,
                                 struct object_params *params,
                                 struct content *c);
void plugin_destroy_stream(struct browser_window *bw,
                           struct object_params *params, struct content *c);

/* linked list handling */
struct plugin_message *plugin_add_message_to_linked_list(plugin_b browser,
                                                         plugin_p plugin,
                                                         wimp_message *m,
                                               struct plugin_message *reply);
void plugin_remove_message_from_linked_list(struct plugin_message* m);
struct plugin_message *plugin_get_message_from_linked_list(int ref);
void plugin_add_instance_to_list(struct content *c,
                                 struct browser_window *bw,
                                 struct content *page, struct box *box,
                                 struct object_params *params, void **state);
void plugin_remove_instance_from_list(struct object_params *params);
struct plugin_list *plugin_get_instance_from_list(plugin_b browser,
                                                  plugin_p plugin);

/* message handling */
void plugin_open(wimp_message *message);
void plugin_opening(wimp_message *message);
void plugin_close(wimp_message *message);
void plugin_closed(wimp_message *message);
void plugin_reshape_request(wimp_message *message);
void plugin_stream_new(wimp_message *message);
void plugin_status(wimp_message *message);
char *plugin_get_string_value(os_string_value string, char *msg);

/* others */
void plugin_create_sysvar(const char *mime_type, char *sysvar);
int plugin_process_opening(struct object_params *params,
                           struct plugin_message *message);
void plugin_force_redraw(struct content *object, struct content *c,
                         unsigned int i);

/*-------------------------------------------------------------------------*/
/* Linked List pointers                                                    */
/*-------------------------------------------------------------------------*/

static struct plugin_message pm = {0, 0, 0, 0, 0, &pm, &pm};
static struct plugin_message *pmlist = &pm;

static struct plugin_list pl = {0, 0, 0, 0, 0, 0, &pl, &pl};
static struct plugin_list *plist = &pl;

static int need_reformat = 0;

/*-------------------------------------------------------------------------*/
/* Externally visible functions                                            */
/*-------------------------------------------------------------------------*/

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
        char *varval;
        os_error *e;
        wimp_message m;
        plugin_message_open *pmo;
        os_box b;
        struct plugin_message *npm = xcalloc(1, sizeof(*npm));
        struct plugin_message *temp;
        struct plugin_list *npl = xcalloc(1, sizeof(*npl));
        int offset;
        unsigned char *pchar = (unsigned char*)&m.data;
        int flags = 0;

	if (params == 0) {
        	fprintf(stderr,
        	        "Cannot handle standalone objects at this time");
        	gui_window_set_status(bw->window,
        	"Plugin Error: Cannot handle standalone objects at this time");
        	xfree(npm);
        	xfree(npl);
        	return;
        }

        /* write parameters file */
        plugin_write_parameters_file(params);

        /* Get contents of Alias$@PlugInType_xxx system variable. */
        plugin_create_sysvar(c->mime_type, sysvar);
        varval = getenv(sysvar);
        LOG(("%s: %s", sysvar, varval));

  /* Broadcast Message_PlugIn_Open (&4D540) and listen for response
   * Message_PlugIn_Opening (&4D541). If no response, try to launch
   * plugin by Wimp_StartTask(sysvar). Then re-broadcast Message_PlugIn_Open
   * and listen for response. If there is still no response, give up.
   * NB: For the bounding box in Message_PlugIn_Open, we choose arbitrary
   *     values outside the area displayed. This is corrected when
   *     plugin_redraw is called.
   */
        /* Initialise bounding box */
        b.x0 = 100;
        b.x1 = 1000;
        b.y0 = 100;
        b.y1 = 1000;

        /* populate plugin_message_open struct */
        pmo = (plugin_message_open*)&m.data;
        pmo->flags = 0;
        pmo->reserved = 0;
        pmo->browser = (plugin_b)params->browser;
        pmo->parent_window = bw->window->data.browser.window;
        pmo->bbox = b;
        xmimemaptranslate_mime_type_to_filetype(c->mime_type, &pmo->file_type);
        pmo->filename.pointer = params->filename;

        m.size = 60;
        m.your_ref = 0;
        m.action = message_PLUG_IN_OPEN;

        /* add message to list */
        temp = plugin_add_message_to_linked_list((plugin_b)params->browser, (plugin_p)0, &m, (struct plugin_message*)0);

        LOG(("Sending Message: &4D540"));
        LOG(("Message Size: %d", m.size));
        e = xwimp_send_message(wimp_USER_MESSAGE_RECORDED, &m, wimp_BROADCAST);

        if(e) LOG(("Error: %s", e->errmess));

        /* wait for wimp poll */
        while(temp->poll == 0)
                gui_poll();

        if(temp->plugin != 0 && temp->reply != 0) {

                /* ok, we got a reply */
                LOG(("Reply to message %p: %p", temp, temp->reply));
                flags = plugin_process_opening(params, temp);
                plugin_remove_message_from_linked_list(temp->reply);
                plugin_remove_message_from_linked_list(temp);
                xfree(npm);
        	xfree(npl);
        } else {

               /* no reply so issue Wimp_StartTask(varval) */
               LOG(("No reply to message %p", temp));
               plugin_remove_message_from_linked_list(temp);

               LOG(("Starting task: %s", varval));
               e = xwimp_start_task((char const*)varval, NULL);

               if(e) LOG(("Error: %s", e->errmess));

               /* hmm, deja-vu */
               temp = plugin_add_message_to_linked_list((plugin_b)params->browser, (plugin_p)0, &m, (struct plugin_message*)0);
               LOG(("Re-Sending Message &4D540"));
               xwimp_send_message(wimp_USER_MESSAGE_RECORDED, &m,
                                  wimp_BROADCAST);

               while(temp->poll == 0)
                       gui_poll();

               if(temp->plugin != 0 && temp->reply != 0) {

                       /* ok, we got a reply */
                       LOG(("Reply to message %p: %p", temp, temp->reply));
                       flags = plugin_process_opening(params, temp);
                       plugin_remove_message_from_linked_list(temp->reply);
                       plugin_remove_message_from_linked_list(temp);
                       xfree(npm);
                       xfree(npl);
               } else {

                       /* no reply so give up */
                       LOG(("No reply to message %p", temp));
                       plugin_remove_message_from_linked_list(temp);
                       xfree(npm);
                       xfree(npl);
                       return;
               }
        }

        /* At this point, it's certain that we can handle this object so
         * add it to the list of plugin instances.
         */
        plugin_add_instance_to_list(c, bw, page, box, params, state);

        /* TODO - handle other flags (see below) */
        if(flags & 0x4)
               plugin_create_stream(bw, params, c);

        plugin_destroy_stream(bw, params, c);

}

/**
 * plugin_process_opening
 * process plugin_opening message flags
 * NB: this is NOT externally visible.
 *     it's just here because it's referred to in the TODO above
 */
int plugin_process_opening(struct object_params *params,
                            struct plugin_message *message) {

        plugin_message_opening *pmo;

        params->plugin = (int)message->reply->plugin;
        params->plugin_task = (unsigned int)message->reply->m->sender;

        pmo = (plugin_message_opening*)&message->reply->m->data;
/*        LOG(("pmo->flags = %x", pmo->flags));
        if(pmo->flags & 0x1)
                LOG(("accepts input focus"));
        if(pmo->flags & 0x2)
                LOG(("wants code fetching"));
        if(pmo->flags & 0x4)
                LOG(("wants data fetching"));
        if(pmo->flags & 0x8)
                LOG(("will delete parameters"));
        if(pmo->flags & 0x10)
                LOG(("still busy"));
        if(pmo->flags & 0x20)
                LOG(("supports extended actions"));
        if(pmo->flags & 0x40)
                LOG(("has helper window"));
*/
        return (int)pmo->flags;
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
        wimp_message m;
        plugin_message_close *pmc;
        struct plugin_message *temp;
        char *p, *filename = strdup(params->filename);

	if (params == 0) {

	        return;
	}

	pmc = (plugin_message_close*)&m.data;
	pmc->flags = 0;
	pmc->plugin = (plugin_p)params->plugin;
	pmc->browser = (plugin_b)params->browser;
	m.size = 32;
	m.your_ref = 0;
	m.action = message_PLUG_IN_CLOSE;

        temp = plugin_add_message_to_linked_list(pmc->browser, pmc->plugin, &m, 0);
        LOG(("Sending message &4D542"));
	xwimp_send_message(wimp_USER_MESSAGE_RECORDED, &m,
	                   (wimp_t)params->plugin_task);


	while (temp == 0)
	        gui_poll();

        if (temp->reply != 0){

                plugin_remove_message_from_linked_list(temp->reply);
                plugin_remove_message_from_linked_list(temp);
        }
        else {
                LOG(("message_PLUG_IN_CLOSE bounced"));
                plugin_remove_message_from_linked_list(temp);
        }

        /* delete parameters file */
	xosfile_delete((char const*)params->filename, NULL, NULL, NULL, NULL, NULL);
	p = strrchr((const char*)filename, 'p');
	filename[(p-filename)] = 'd';
	xosfile_delete((char const*)filename, NULL, NULL, NULL, NULL, NULL);

	/* delete instance from list */
	plugin_remove_instance_from_list(params);
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
        wimp_message m;
        plugin_message_reshape *pmr;
        os_box bbox;
        unsigned long x, y;

        box_coords(box, (unsigned long*)&x, (unsigned long*)&y);
        bbox.x0 = ((int)x << 1);
        bbox.y1 = -(((int)y << 1));
        bbox.x1 = (bbox.x0 + (box->width << 1));
        bbox.y0 = (bbox.y1 - (box->height << 1));

        LOG(("Box w, h: %ld %ld", box->width, box->height));
        LOG(("BBox: [(%d,%d),(%d,%d)]", bbox.x0, bbox.y0, bbox.x1, bbox.y1));

        pmr = (plugin_message_reshape*)&m.data;
        pmr->flags = 0;
        pmr->plugin = (plugin_p) params->plugin;
        pmr->browser = (plugin_b) params->browser;
        pmr->parent_window = (wimp_w) bw->window->data.browser.window;
        pmr->bbox = bbox;

        m.size = 52;
        m.your_ref = 0;
        m.action = message_PLUG_IN_RESHAPE;

        LOG(("Sending Message &4D544"));
        xwimp_send_message(wimp_USER_MESSAGE, &m, (wimp_t)params->plugin_task);

}


static const char * const ALIAS_PREFIX = "Alias$@PlugInType_";

/**
 * plugin_create_sysvar
 * creates system variable from mime type
 * NB: this is NOT externally visible
 *     it just makes sense to keep it with the ALIAS_PREFIX definition above.
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
  LOG(("%s, %s", mime_type, sysvar));
  if (getenv(sysvar) == 0)
	  return false;
  return true;
}

/**
 * plugin_process_data
 * processes data retrieved by the fetch process
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
 */
void plugin_destroy(struct content *c)
{
        /* simply free buffered data */
        xfree(c->data.plugin.data);
}

/**
 * plugin_redraw
 * redraw plugin on page.
 */
void plugin_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height)
{
        struct plugin_list *npl;

        if(need_reformat) {
                content_reformat(c, c->available_width, 0);
                for(npl = plist->next; npl != plist; npl = npl->next)
                        plugin_reshape_instance(npl->c, npl->bw, npl->page,
                                                npl->box, npl->params,
                                                npl->state);
                need_reformat = 0;
        }
}

/*-------------------------------------------------------------------------*/
/* Parameters file handling functions                                      */
/*-------------------------------------------------------------------------*/

/**
 * plugin_write_parameters_file
 * Writes the parameters file.
 */
void plugin_write_parameters_file(struct object_params *params)
{
        struct plugin_params* temp;
        struct plugin_param_item* ppi;
        struct plugin_param_item* pilist = 0;
        int *time;
        char *tstr;
        FILE *fp;

        /* Create the file */
        xosfile_create_dir("<Wimp$ScrapDir>.WWW", 77);
        xosfile_create_dir("<Wimp$ScrapDir>.WWW.NetSurf", 77);
        /* path + filename + terminating NUL */
        params->filename = xcalloc(strlen(getenv("Wimp$ScrapDir"))+13+10+1,
                                   sizeof(char));
        xos_read_monotonic_time((int*)&time);
        tstr = xcalloc(40, sizeof(char));
        sprintf(tstr, "%01u", (unsigned int)time<<8);
        sprintf(params->filename, "%s.WWW.NetSurf.p%1.9s",
                                  getenv("Wimp$ScrapDir"), tstr);
        params->browser = (unsigned int)time<<8;
        xfree(tstr);
        LOG(("filename: %s", params->filename));

        /* Write object attributes first */

        /* classid is checked first */
        if(params->classid != 0 && params->codetype != 0) {

          pilist = plugin_add_item_to_pilist(pilist, 1, "CLASSID",
                                             params->classid,
                                             params->codetype);
        }
        /* otherwise, we check the data attribute */
        else if(params->data !=0 && params->type != 0) {

          pilist = plugin_add_item_to_pilist(pilist, 1, "DATA",
                                             params->data,
                                             params->type);
        }

        /* if codebase is specified, write it as well */
        if(params->codebase != 0) {

                pilist = plugin_add_item_to_pilist(pilist, 1,
                                                   "CODEBASE",
                                                   params->codebase, NULL);

        }

        /* Iterate through the parameter list, creating the parameters
         * file as we go. We can free up the memory as we go.
         */
        while(params->params != 0) {

                LOG(("name: %s", params->params->name == 0 ? "not set" : params->params->name));
                LOG(("value: %s", params->params->value == 0 ? "not set" : params->params->value));
                LOG(("type: %s", params->params->type == 0 ? "not set" : params->params->type));
                LOG(("valuetype: %s", params->params->valuetype));


                if(strcasecmp(params->params->valuetype, "data") == 0)
                        pilist = plugin_add_item_to_pilist(pilist, 1,
                                                  params->params->name,
                                                  params->params->value,
                                                  params->params->type);
                if(strcasecmp(params->params->valuetype, "ref") == 0)
                        pilist = plugin_add_item_to_pilist(pilist, 2,
                                                  params->params->name,
                                                  params->params->value,
                                                  params->params->type);
                if(strcasecmp(params->params->valuetype, "object") == 0)
                        pilist = plugin_add_item_to_pilist(pilist, 3,
                                                  params->params->name,
                                                  params->params->value,
                                                  params->params->type);

                temp = params->params;
                params->params = params->params->next;
                xfree(temp);
         }

         /* Now write mandatory special parameters */

         /* BASEHREF */
         pilist = plugin_add_item_to_pilist(pilist, 4, "BASEHREF",
                                            params->basehref, NULL);

         /* USERAGENT */
         pilist = plugin_add_item_to_pilist(pilist, 4, "USERAGENT",
                                            "NetSurf", NULL);

         /* UAVERSION */
         pilist = plugin_add_item_to_pilist(pilist, 4, "UAVERSION",
                                            "0.01", NULL);

         /* APIVERSION */
         pilist = plugin_add_item_to_pilist(pilist, 4, "APIVERSION",
                                            "1.10", NULL);

         /* BGCOLOR - needs fixing to work properly.
          *           Currently, it assumes FFFFFF00 (BBGGRR00) */
         pilist = plugin_add_item_to_pilist(pilist, 4, "BGCOLOR",
                                            "FFFFFF00", NULL);

         /* Write file */
         fp = fopen(params->filename, "wb+");

         while (pilist != 0) {

                 fwrite(&pilist->type, (unsigned int)sizeof(int), 1, fp);
                 fwrite(&pilist->rsize, (unsigned int)sizeof(int), 1, fp);

                 fwrite(&pilist->nsize, (unsigned int)sizeof(int), 1, fp);
                 fwrite(pilist->name, (unsigned int)strlen(pilist->name), 1, fp);
                 for(; pilist->npad != 0; pilist->npad--)
                         fputc('\0', fp);

                 fwrite(&pilist->vsize, (unsigned int)sizeof(int), 1, fp);
                 fwrite(pilist->value, (unsigned int)strlen(pilist->value), 1, fp);
                 for(; pilist->vpad != 0; pilist->vpad--)
                         fputc('\0', fp);

                 fwrite(&pilist->msize, (unsigned int)sizeof(int), 1, fp);
                 if(pilist->msize > 0) {
                         fwrite(pilist->mime_type,
                                (unsigned int)strlen(pilist->mime_type), 1, fp);
                         for(; pilist->mpad != 0; pilist->mpad--)
                                 fputc('\0', fp);
                 }

                 ppi = pilist;
                 pilist = pilist->next;

                 xfree(ppi);
         }

         fwrite("\0", sizeof(char), 4, fp);

         fclose(fp);
}

/**
 * plugin_calculate_rsize
 * calculates the size of a parameter file record
 */
int plugin_calculate_rsize(char* name, char* data, char* mime) {

	int ret = 0;
	ret += (4 + strlen(name) + 3) / 4 * 4; /* name */
	ret += (4 + strlen(data) + 3) / 4 * 4; /* data */

	if (mime != NULL)
		ret += (4 + strlen(mime) + 3) / 4 * 4; /* mime type */
	else
		ret += 4;

	return ret;
}

/**
 * plugin_add_item_to_pilist
 * adds an item to the pilist
 */
struct plugin_param_item *plugin_add_item_to_pilist(struct plugin_param_item *pilist, int type, char* name, char* value, char* mime_type) {

        struct plugin_param_item *ppi = xcalloc(1, sizeof(*ppi));

        /* initialise struct */
        ppi->type = 0;
        ppi->rsize = 0;
        ppi->nsize = 0;
        ppi->name = 0;
        ppi->npad = 0;
        ppi->vsize = 0;
        ppi->value = 0;
        ppi->vpad = 0;
        ppi->msize = 0;
        ppi->mime_type = 0;
        ppi->mpad = 0;

        ppi->type = type;
        ppi->rsize = plugin_calculate_rsize(name, value, mime_type);
        ppi->nsize = strlen(name);
        ppi->name = xstrdup(name);
        ppi->npad = 4 - (ppi->nsize%4 == 0 ? 4 : ppi->nsize%4);
        ppi->vsize = strlen(value);
        ppi->value = xstrdup(value);
        ppi->vpad = 4 - (ppi->vsize%4 == 0 ? 4 : ppi->vsize%4);
        if(mime_type != 0) {
                ppi->msize = strlen(mime_type);
                ppi->mime_type = xstrdup(mime_type);
                ppi->mpad = 4 - (ppi->msize%4 == 0 ? 4 : ppi->msize%4);
        }

        ppi->next = pilist;
        return ppi;
}

/*-------------------------------------------------------------------------*/
/* Plugin Stream handling functions                                        */
/*-------------------------------------------------------------------------*/

/**
 * plugin_create_stream
 * creates a plugin stream
 */
void plugin_create_stream(struct browser_window *bw, struct object_params *params, struct content *c) {

        wimp_message m;
        plugin_message_stream_new *pmsn;
        struct plugin_message *temp;
        int offset = 0;
        unsigned char *pchar = (unsigned char*)&m.data;

        pmsn = (plugin_message_stream_new*)&m.data;
        pmsn->flags = 2;
        pmsn->plugin = (plugin_p)params->plugin;
        pmsn->browser = (plugin_b)params->browser;
        pmsn->stream = (plugin_s)0;
        pmsn->browser_stream = (plugin_bs)params->browser;
        pmsn->url.pointer = c->url;
        pmsn->end = c->data.plugin.length;
        pmsn->last_modified_date = 0;
        pmsn->notify_data = 0;
        pmsn->mime_type.pointer = c->mime_type;
        pmsn->target_window.offset = 0;

        m.size = 64;
        m.your_ref = 0;
        m.action = message_PLUG_IN_STREAM_NEW;

        temp = plugin_add_message_to_linked_list(pmsn->browser, pmsn->plugin, &m, 0);

        LOG(("message length = %d", m.size));

        LOG(("Sending message &4D548"));
        xwimp_send_message(wimp_USER_MESSAGE_RECORDED, &m, (wimp_t)params->plugin_task);

        while(temp->poll == 0)
                gui_poll();

        pmsn = (plugin_message_stream_new*)&temp->reply->m->data;
        params->browser_stream = params->browser;
        params->plugin_stream = (int)pmsn->stream;
        LOG(("%d, %d, %d", (int)pmsn->stream, params->plugin_stream, params->browser_stream));
        if((pmsn->flags & 0x02) | (pmsn->flags & 0x03)) {
                plugin_write_stream_as_file(bw, params, c);
        }

        /* clean up */
        plugin_remove_message_from_linked_list(temp->reply);
        plugin_remove_message_from_linked_list(temp);
}

/**
 * plugin_write_stream_as_file
 * writes a stream as a file
 */
void plugin_write_stream_as_file(struct browser_window *bw, struct object_params *params, struct content *c) {

        wimp_message m;
        plugin_message_stream_as_file *pmsaf;
        int offset = 0;
        unsigned int filetype;
        unsigned char *pchar = (unsigned char*)&m.data;
        char *filename = strdup(params->filename), *p;

        pmsaf = (plugin_message_stream_as_file*)&m.data;

        pmsaf->flags = 0;
        pmsaf->plugin = (plugin_p)params->plugin;
        pmsaf->browser = (plugin_b)params->browser;
        pmsaf->stream = (plugin_s)params->plugin_stream;
        pmsaf->browser_stream = (plugin_bs)params->browser_stream;
        pmsaf->url.pointer = c->url;
        pmsaf->end = 0;
        pmsaf->last_modified_date = 0;
        pmsaf->notify_data = 0;

        p = strrchr((const char*)filename, 'p');
	filename[(p-filename)] = 'd';
        pmsaf->filename.pointer = filename;

        m.size = 60;
        m.your_ref = 0;
        m.action = message_PLUG_IN_STREAM_AS_FILE;

        xmimemaptranslate_mime_type_to_filetype(c->mime_type, (bits *) &filetype);
        xosfile_save_stamped((char const*)filename, filetype, c->data.plugin.data, c->data.plugin.data + c->data.plugin.length);

        LOG(("message length = %d", m.size));

        LOG(("Sending message &4D54C"));
        xwimp_send_message(wimp_USER_MESSAGE, &m, (wimp_t)params->plugin_task);
}

/**
 * plugin_destroy_stream
 * destroys a plugin stream
 */
void plugin_destroy_stream(struct browser_window *bw, struct object_params *params, struct content *c) {

        wimp_message m;
        plugin_message_stream_destroy *pmsd;
        int offset = 0;
        unsigned char *pchar = (unsigned char*)&m.data;

        pmsd = (plugin_message_stream_destroy*)&m.data;

        pmsd->flags = 0;
        pmsd->plugin = (plugin_p)params->plugin;
        pmsd->browser = (plugin_b)params->browser;
        pmsd->stream = (plugin_s)params->plugin_stream;
        pmsd->browser_stream = (plugin_bs)params->browser_stream;
        pmsd->url.pointer = c->url;
        pmsd->end = 0;
        pmsd->last_modified_date = 0;
        pmsd->notify_data = 0;
        pmsd->reason = plugin_STREAM_DESTROY_FINISHED;

        m.size = 60;
        m.your_ref = 0;
        m.action = message_PLUG_IN_STREAM_DESTROY;

        LOG(("Sending message &4D549"));
        xwimp_send_message(wimp_USER_MESSAGE, &m, (wimp_t)params->plugin_task);
}

/*-------------------------------------------------------------------------*/
/* Linked List handling functions                                          */
/*-------------------------------------------------------------------------*/

/**
 * plugin_add_message_to_linked_list
 * adds a message to the list
 */
struct plugin_message *plugin_add_message_to_linked_list(plugin_b browser, plugin_p plugin, wimp_message *m, struct plugin_message *reply) {

         struct plugin_message *npm = xcalloc(1, sizeof(*npm));

         npm->poll = 0;
         npm->browser = browser;
         npm->plugin = plugin;
         npm->m = m;
         npm->reply = reply;
         npm->prev = pmlist->prev;
         npm->next = pmlist;
         pmlist->prev->next = npm;
         pmlist->prev = npm;

         LOG(("Added Message: %p", npm));

         return pmlist->prev;
}

/**
 * plugin_remove_message_from_linked_list
 * removes a message from the list
 */
void plugin_remove_message_from_linked_list(struct plugin_message* m) {

         m->prev->next = m->next;
         m->next->prev = m->prev;
         LOG(("Deleted Message: %p", m));
         xfree(m);
}

/**
 * plugin_get_message_from_linked_list
 * retrieves a message from the list
 * returns NULL if no message is found
 */
struct plugin_message *plugin_get_message_from_linked_list(int ref) {

         struct plugin_message *npm;

         for(npm = pmlist->next; npm != pmlist && npm->m->my_ref != ref;
             npm = npm->next)
                LOG(("my_ref: %d, ref: %d", npm->m->my_ref, ref));

         LOG(("my_ref: %d, ref: %d", npm->m->my_ref, ref));
         if(npm != pmlist) {
                 LOG(("Got message: %p", npm));
                 return npm;
         }

         return NULL;
}

/**
 * plugin_add_instance_to_list
 * Adds a plugin instance to the list of plugin instances.
 */
void plugin_add_instance_to_list(struct content *c, struct browser_window *bw, struct content *page, struct box *box, struct object_params *params, void **state) {

         struct plugin_list *npl = xcalloc(1, sizeof(*npl));

         npl->c = c;
         npl->bw = bw;
         npl->page = page;
         npl->box = box;
         npl->params = params;
         npl->state = state;
         npl->prev = plist->prev;
         npl->next = plist;
         plist->prev->next = npl;
         plist->prev = npl;
}

/**
 * plugin_remove_instance_from_list
 * Removes a plugin instance from the list
 */
void plugin_remove_instance_from_list(struct object_params *params) {

         struct plugin_list *temp =
                plugin_get_instance_from_list((plugin_b)params->browser,
                                              (plugin_p)params->plugin);
         if(temp != NULL) {

                 temp->prev->next = temp->next;
                 temp->next->prev = temp->prev;
                 xfree(temp);
         }
}

/**
 * plugin_get_instance_from_list
 * retrieves an instance of a plugin from the list
 * returns NULL if no instance is found
 */
struct plugin_list *plugin_get_instance_from_list(plugin_b browser, plugin_p plugin) {

         struct plugin_list *npl;

         for(npl = plist->next; (npl != plist)
                             && (((plugin_b)npl->params->browser != browser)
                             && ((plugin_p)npl->params->plugin != plugin));
             npl = npl->next)
                ;

         if(npl != plist)
                 return npl;

         return NULL;
}

/*-------------------------------------------------------------------------*/
/* WIMP Message processing functions                                       */
/*-------------------------------------------------------------------------*/

/**
 * plugin_msg_parse
 * parses wimp messages
 */
void plugin_msg_parse(wimp_message *message, int ack)
{
         LOG(("Parsing message"));
         switch(message->action) {

                 case message_PLUG_IN_OPENING:
                          plugin_opening(message);
                          break;
                 case message_PLUG_IN_CLOSED:
                          plugin_closed(message);
                          break;
                 case message_PLUG_IN_RESHAPE_REQUEST:
                          plugin_reshape_request(message);
                          break;
                 case message_PLUG_IN_FOCUS:
                   //       plugin_focus();
                          break;
                 case message_PLUG_IN_URL_ACCESS:
                   //       plugin_url_access();
                          break;
                 case message_PLUG_IN_STATUS:
                          plugin_status(message);
                          break;
                 case message_PLUG_IN_BUSY:
                   //       plugin_busy();
                          break;
                 /* OSLib doesn't provide this, as it's
                  * reasonably new and not obviously documented.
                  * We ignore it for now.

                 case message_PLUG_IN_INFORMED:
                  */
                 case message_PLUG_IN_STREAM_NEW:
                          plugin_stream_new(message);
                          break;
                 case message_PLUG_IN_STREAM_WRITE:
                   //       plugin_stream_write();
                          break;
                 case message_PLUG_IN_STREAM_WRITTEN:
                   //       plugin_stream_written();
                          break;
                 case message_PLUG_IN_STREAM_DESTROY:
                   //       plugin_stream_destroy();
                          break;

                 /* These cases occur when a message is bounced
                  * For simplicity, we do nothing unless the message came in
                  * a wimp_USER_MESSAGE_ACKNOWLEDGE (ie ack = 1)
                  */
                 case message_PLUG_IN_OPEN:
                          if(ack)
                                  plugin_open(message);
                          break;
                 case message_PLUG_IN_CLOSE:
                          if(ack)
                                  plugin_close(message);
                          break;
                 case message_PLUG_IN_RESHAPE:
                 case message_PLUG_IN_STREAM_AS_FILE:
                 case message_PLUG_IN_NOTIFY:
                 case message_PLUG_IN_ABORT:
                 case message_PLUG_IN_ACTION:
                 default:
                          break;
         }
}

/**
 * plugin_open
 * handles receipt of plugin_open messages
 */
void plugin_open(wimp_message *message) {

         struct plugin_message *npm = plugin_get_message_from_linked_list(message->my_ref);

         LOG(("Acknowledgement of %p", npm));
         /* notify plugin_open message entry in list */
         if (npm != NULL)
               npm->poll = 1;
}

/**
 * plugin_opening
 * handles receipt of plugin_open messages
 */
void plugin_opening(wimp_message *message) {

         struct plugin_message *npm = plugin_get_message_from_linked_list(message->your_ref);
         struct plugin_message *reply;
         plugin_message_opening *pmo = (plugin_message_opening*)&message->data;

         /* add this message to linked list */
         reply = plugin_add_message_to_linked_list(pmo->browser, pmo->plugin, message, 0);

         /* notify plugin_open message entry in list */
         if (npm != NULL) {

               npm->poll = 1;
               npm->plugin = pmo->plugin;
               npm->reply = reply;
         }
}

/**
 * plugin_close
 * handles receipt of plugin_close messages
 */
void plugin_close(wimp_message *message) {

         struct plugin_message *npm = plugin_get_message_from_linked_list(message->my_ref);

         /* notify plugin_open message entry in list */
         if (npm != NULL)
               npm->poll = 1;
}

/**
 * plugin_closed
 * handles receipt of plugin_closed messages
 */
void plugin_closed(wimp_message *message) {

         struct plugin_message *npm = plugin_get_message_from_linked_list(message->your_ref);
         struct plugin_message *reply;
         plugin_message_closed *pmc = (plugin_message_closed*)&message->data;

         /* add this message to linked list */
         reply = plugin_add_message_to_linked_list(pmc->browser, pmc->plugin, message, 0);

         /* notify plugin_open message entry in list */
         if (npm != NULL) {

               npm->poll = 1;
               npm->reply = reply;
         }
}

/**
 * plugin_reshape_request
 * handles receipt of plugin_reshape_request messages
 */
void plugin_reshape_request(wimp_message *message) {

         struct plugin_list *npl;
         plugin_message_reshape_request *pmrr = (plugin_message_reshape_request*)&message->data;
         unsigned int i;

         npl = plugin_get_instance_from_list(pmrr->browser, pmrr->plugin);

         for (i = 0; i != npl->page->data.html.object_count &&
                   npl->page->data.html.object[i].content != npl->c;
              i++) ;

         if (i != npl->page->data.html.object_count) {
                 /* should probably shift by x and y eigen values here */
                 npl->c->width = pmrr->size.x >> 1;
                 npl->c->height = pmrr->size.y >> 1;
                 plugin_force_redraw(npl->c, npl->page, i);
         }

         LOG(("requested (width, height): (%d, %d)", pmrr->size.x, pmrr->size.y));
}

/**
 * plugin_stream_new
 * handles receipt of plugin_stream_new messages
 */
void plugin_stream_new(wimp_message *message) {

         struct plugin_message *npm = plugin_get_message_from_linked_list(message->your_ref);
         struct plugin_message *reply;
         plugin_message_stream_new *pmsn = (plugin_message_stream_new*)&message->data;

         /* add this message to linked list */
         reply = plugin_add_message_to_linked_list(pmsn->browser, pmsn->plugin, message, 0);

         /* notify plugin_open message entry in list */
         if(npm != NULL) {

               npm->poll = 1;
               npm->plugin = pmsn->plugin;
               npm->reply = reply;
         }

}

/**
 * plugin_status
 * handles receipt of plugin_status messages
 */
void plugin_status(wimp_message *message) {

         plugin_message_status *pms = (plugin_message_status*)&message->data;
         struct plugin_list *npl = plugin_get_instance_from_list(pms->browser, pms->plugin);

         gui_window_set_status(npl->bw->window,
                        (const char*)plugin_get_string_value(pms->message,
                                                             (char*)pms));
}

/**
 * plugin_get_string_value
 * utility function to grab string data from plugin message blocks
 */
char *plugin_get_string_value(os_string_value string, char *msg) {

         if(string.offset == 0 || string.offset > 256) {
                 return string.pointer;
         }
         return &msg[string.offset];
}

void plugin_force_redraw(struct content *object, struct content *c,
                         unsigned int i) {

	struct box *box = c->data.html.object[i].box;

	box->object = object;

        box->width = box->min_width = box->max_width = object->width;
        box->height = object->height;

        box->style->width.width = CSS_WIDTH_LENGTH;
	box->style->width.value.length.unit = CSS_UNIT_PX;
	box->style->width.value.length.value = object->width;

        box->style->height.height = CSS_HEIGHT_LENGTH;
	box->style->height.length.unit = CSS_UNIT_PX;
	box->style->height.length.value = object->height;

        need_reformat = 1;
	/* We don't call content_reformat here
	   beacuse doing so breaks things :-)
	 */
}
