/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003,4 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Acorn Plugin protocol (implementation)
 *
 * This file implements the Acorn plugin protocol.
 * See http://www.ecs.soton.ac.uk/~jmb202/riscos/acorn/funcspec.html
 * for more details.
 *
 * The are still a number of outstanding issues:
 *
 * Stream Protocol:
 * 	Fetching data, then streaming it to a plugin is not supported
 * 	Streaming data from a plugin is not supported
 *
 * Messages:
 * 	Most Plugin_Opening flags not supported
 * 	No support for Plugin_Focus, Plugin_Busy, Plugin_Action
 * 	No support for Plugin_Abort, Plugin_Inform, Plugin_Informed
 * 	Plugin_URL_Access is only part-implemented
 *
 * No support for "helper" applications
 * No support for standalone objects (must be embedded in HTML page)
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oslib/mimemap.h"
#include "oslib/os.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osgbpb.h"
#include "oslib/plugin.h"
#include "oslib/wimp.h"

#include "netsurf/utils/config.h"
#include "netsurf/content/content.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/desktop/gui.h"
#include "netsurf/render/html.h"
#include "netsurf/render/box.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/riscos/plugin.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/messages.h"
#include "netsurf/utils/utils.h"


#ifdef WITH_PLUGIN

static const char * const ALIAS_PREFIX = "Alias$@PlugInType_";

typedef enum {
	PLUGIN_PARAMETER_DATA    = 1,
	PLUGIN_PARAMETER_URL     = 2,
	PLUGIN_PARAMETER_OBJECT  = 3,
	PLUGIN_PARAMETER_SPECIAL = 4
} plugin_parameter_type;

struct plugin_param_item {

        plugin_parameter_type type;
        int rsize;
        int nsize;
        char *name;
        int npad;
        int vsize;
        char *value;
        int vpad;
        int msize;
        char *mime_type;
        int mpad;

        struct plugin_param_item *next;
};

static bool plugin_create_sysvar(const char *mime_type, char* sysvar);
static void plugin_create_stream(struct content *c);
static void plugin_write_stream(struct content *c, unsigned int consumed);
static void plugin_stream_write_callback(void *p);
static void plugin_write_stream_as_file(struct content *c);
static void plugin_destroy_stream(struct content *c);
static bool plugin_write_parameters_file(struct content *c,
		struct object_params *params);
static int plugin_calculate_rsize(const char* name, const char* data,
		const char* mime);
static bool plugin_add_item_to_pilist(struct plugin_param_item **pilist,
		plugin_parameter_type type, const char* name,
		const char* value, const char* mime_type);
static char *plugin_get_string_value(os_string_value string, char *msg);
static bool plugin_active(struct content *c);

/**
 * Initialises plugin system in readiness for receiving object data
 *
 * \param c      The content to hold the data
 * \param params Parameters associated with the content
 * \return true on success, false otherwise
 */
bool plugin_create(struct content *c, const char *params[])
{
	LOG(("plugin_create"));
	c->data.plugin.bw = 0;
	c->data.plugin.page = 0;
	c->data.plugin.box = 0;
	c->data.plugin.taskname = 0;
	c->data.plugin.filename = 0;
	c->data.plugin.datafile = 0;
	c->data.plugin.opened = false;
	c->data.plugin.repeated = 0;
	c->data.plugin.browser = 0;
	c->data.plugin.plugin = 0;
	c->data.plugin.browser_stream = 0;
	c->data.plugin.plugin_stream = 0;
	c->data.plugin.plugin_task = 0;
	c->data.plugin.consumed = 0;
	c->data.plugin.reformat_pending = false;
	c->data.plugin.width = 0;
	c->data.plugin.height = 0;
	c->data.plugin.stream_waiting = false;
	c->data.plugin.file_stream_waiting = false;
	return true;
}

/**
 * Convert a plugin ready for display (does nothing)
 *
 * \param c      The content to convert
 * \param width  Width of available space
 * \param height Height of available space
 * \return true on success, false otherwise
 */
bool plugin_convert(struct content *c, int width, int height)
{
	LOG(("plugin_convert"));
	c->width = width;
	c->height = height;

	/* deal with a plugin waiting for a file stream */
	if (c->data.plugin.file_stream_waiting) {
		c->data.plugin.file_stream_waiting = false;
		plugin_write_stream_as_file(c);
	}

	c->status = CONTENT_STATUS_DONE;
	return true;
}

/**
 * Destroy a plugin content
 *
 * \param c The content to destroy
 */
void plugin_destroy(struct content *c)
{
	LOG(("plugin_destroy"));
	if (c->data.plugin.taskname)
		free(c->data.plugin.taskname);
	if (c->data.plugin.filename)
		free(c->data.plugin.filename);
	if (c->data.plugin.datafile)
		free(c->data.plugin.datafile);
}

/**
 * Redraw a content
 *
 * \param c            The content to redraw
 * \param x            Left of content box
 * \param y            Top of content box
 * \param width        Width of content box
 * \param height       Height of content box
 * \param clip[xy][01] Clipping rectangle
 * \param scale        Scale of page (1.0 = 100%)
 */
bool plugin_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale, unsigned long background_colour)
{
	/* do nothing */
	LOG(("plugin_redraw"));
	return true;
}


/**
 * Handle a window containing a CONTENT_PLUGIN being opened
 *
 * \param c      The content to open
 * \param bw     The window to add the content to
 * \param page   The containing content
 * \param box    The containing box
 * \param params Any parameters associated with the content
 * \param state  State data pointer
 */
void plugin_open(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params)
{
	char sysvar[25];
	char *varval;
	plugin_full_message_open pmo;
	os_error *error;


	if (option_no_plugins)
		return;

	/** \todo Standalone plugins */
	if (!params) {
		LOG(("cannot handle standalone plugins"));
		return;
	}

	/* we only do this here cos the box is needed by
	 * write_parameters_file. Ideally it would be at the
	 * end of this function with the other writes to c->data.plugin
	 */
	c->data.plugin.box = box;

	LOG(("writing parameters file"));
	if (!plugin_write_parameters_file(c, params))
		return;

	LOG(("creating sysvar"));
	/* get contents of Alias$@PlugInType_xxx variable */
	if (!plugin_create_sysvar(c->mime_type, sysvar))
		return;

	LOG(("getenv"));
	varval = getenv(sysvar);
	LOG(("%s: '%s'", sysvar, varval));
	if(!varval) {
		return;
	}

	/* The browser instance handle is the content struct pointer */
	c->data.plugin.browser = (unsigned int)c;

	pmo.size = 60;
	pmo.your_ref = 0;
	pmo.action = message_PLUG_IN_OPEN;
	pmo.flags = 0;
	pmo.reserved = 0;
	pmo.browser = (plugin_b)c->data.plugin.browser;
	pmo.parent_window = bw->window->window;
	pmo.bbox.x0 = -100;
	pmo.bbox.x1 = pmo.bbox.y0 = 0;
	pmo.bbox.y1 = 100;
	error = xmimemaptranslate_mime_type_to_filetype(c->mime_type,
						&pmo.file_type);
	if (error) {
		return;
	}
	pmo.filename.pointer = c->data.plugin.filename;

	c->data.plugin.repeated = 0;

	LOG(("sending message"));
	error = xwimp_send_message(wimp_USER_MESSAGE_RECORDED,
				(wimp_message *)&pmo, wimp_BROADCAST);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}

	c->data.plugin.bw = bw;
	c->data.plugin.page = page;
	c->data.plugin.taskname = strdup(varval);

	LOG(("done"));
}


/**
 * Handle a window containing a CONTENT_PLUGIN being closed.
 *
 * \param c      The content to close
 */
void plugin_close(struct content *c)
{
	plugin_full_message_close pmc;
	os_error *error;

	LOG(("plugin_close"));

	if (!plugin_active(c) || !c->data.plugin.opened)
		return;

	pmc.size = 32;
	pmc.your_ref = 0;
	pmc.action = message_PLUG_IN_CLOSE;
	pmc.flags = 0;
	pmc.browser = (plugin_b)c->data.plugin.browser;
	pmc.plugin = (plugin_p)c->data.plugin.plugin;

	LOG(("sending message"));
	error = xwimp_send_message(wimp_USER_MESSAGE_RECORDED,
		(wimp_message *)&pmc, (wimp_t)c->data.plugin.plugin_task);
	if (error) {
		return;
	}

	/* delete the data file used to send the data to the plugin */
	if (c->data.plugin.datafile != 0)
		xosfile_delete(c->data.plugin.datafile, 0, 0, 0, 0, 0);
}

/**
 * Reformat a plugin content on a page
 *
 * \param c      The content to reformat
 * \param width  New width
 * \param height New height
 */
void plugin_reformat(struct content *c, int width, int height)
{
	plugin_full_message_reshape pmr;
	int x, y;
	os_error *error;

	LOG(("plugin_reformat"));

	if (!plugin_active(c))
		return;

	/* if the plugin hasn't yet been opened, queue the reformat */
	if (!c->data.plugin.opened) {
		LOG(("queuing"));
		c->data.plugin.reformat_pending = true;
		c->data.plugin.width = width;
		c->data.plugin.height = height;
		return;
	}

	c->data.plugin.reformat_pending = false;

	box_coords(c->data.plugin.box, &x, &y);
	pmr.size = 52;
	pmr.your_ref = 0;
	pmr.action = message_PLUG_IN_RESHAPE;
	pmr.flags = 0;

	pmr.plugin = (plugin_p)c->data.plugin.plugin;
	pmr.browser = (plugin_b)c->data.plugin.browser;
	pmr.parent_window = c->data.plugin.bw->window->window;
	pmr.bbox.x0 = x * 2;
	pmr.bbox.y1 = -y * 2;
	pmr.bbox.x1 = pmr.bbox.x0 + c->data.plugin.box->width * 2;
	pmr.bbox.y0 = pmr.bbox.y1 - c->data.plugin.box->height * 2;

	LOG(("sending message"));
	error = xwimp_send_message(wimp_USER_MESSAGE, (wimp_message *) &pmr,
				(wimp_t)c->data.plugin.plugin_task);
	if (error) {
		return;
	}
}

/**
 * Creates a system variable from the mimetype
 *
 * \param mime_type The mime type
 * \param sysvar    Pointer to buffer into which the string should be written
 * \return true on success, false otherwise.
 */
bool plugin_create_sysvar(const char *mime_type, char* sysvar)
{
	unsigned int *fv;
	os_error *e;

	e = xmimemaptranslate_mime_type_to_filetype(mime_type, (bits *) &fv);
	if (e) {
		return false;
	}

	sprintf(sysvar, "%s%03x", ALIAS_PREFIX, (unsigned int)fv);

	return true;
}

/**
 * Determines whether a content is handleable by a plugin
 *
 * \param mime_type The mime type of the content
 * \return true if the content is handleable, false otherwise
 */
bool plugin_handleable(const char *mime_type)
{
	char sysvar[25];

	if (plugin_create_sysvar(mime_type, sysvar)) {
		if (getenv(sysvar) != 0) {
			return true;
		}
	}

	return false;
}


/**
 * Handle a bounced plugin_open message
 *
 * \param message The message to handle
 */
void plugin_open_msg(wimp_message *message)
{
	struct content *c;
	os_error *error;
	plugin_message_open *pmo = (plugin_message_open *)&message->data;

	/* retrieve our content */
	c = (struct content *)pmo->browser;

	/* check we expect this message */
	if (!c || !plugin_active(c))
		return;

	LOG(("bounced"));

	/* bail if we've already tried twice */
	if (c->data.plugin.repeated >= 1)
		return;

	/* start plugin app */
	error = xwimp_start_task((char const*)c->data.plugin.taskname, 0);
	if (error) {
		return;
	}

	/* indicate we've already sent this message once */
	c->data.plugin.repeated++;

	/* and resend the message */
	LOG(("resending"));
	message->your_ref = 0;
	error = xwimp_send_message(wimp_USER_MESSAGE_RECORDED, message,
				wimp_BROADCAST);
	if (error) {
		return;
	}
}

/**
 * Handle a plugin_opening message
 *
 * \param message The message to handle
 */
void plugin_opening(wimp_message *message)
{
	struct content *c;
	plugin_message_opening *pmo =
				(plugin_message_opening *)&message->data;

	/* retrieve our content */
	c = (struct content *)pmo->browser;

	/* check we expect this message */
	if (!c || !plugin_active(c))
		return;

	c->data.plugin.repeated = 2; /* make sure open_msg does nothing */
	c->data.plugin.plugin = (unsigned int)pmo->plugin;
	c->data.plugin.plugin_task = (unsigned int)message->sender;
	c->data.plugin.opened = true;

	LOG(("opening"));

	/* if there's a reformat pending, do so now */
	if (c->data.plugin.reformat_pending) {
		LOG(("do pending reformat"));
		plugin_reformat(c, c->data.plugin.width,
					c->data.plugin.height);
	}

	if (pmo->flags & 0x04) { /* plugin wants the data fetching */
		LOG(("wants stream"));
		plugin_create_stream(c);
	}

	if (!(pmo->flags & 0x08)) {
		LOG(("we delete file"));
		/* we don't care if this fails */
		xosfile_delete(c->data.plugin.filename, 0, 0, 0, 0, 0);
	}
}

/**
 * Handle a bounced plugin_close message
 *
 * \param message The message to handle
 */
void plugin_close_msg(wimp_message *message)
{
	plugin_message_close *pmc = (plugin_message_close *)&message->data;
	/* not necessarily true - some plugins don't stop this bouncing */
	LOG(("failed to close plugin: %p", pmc->plugin));
}

/**
 * Handle a plugin_closed message
 *
 * \param message The message to handle
 */
void plugin_closed(wimp_message *message)
{
	struct content *c;
	plugin_message_closed *pmc = (plugin_message_closed *)&message->data;

	/* retrieve our content */
	c = (struct content*)pmc->browser;

	/* check we expect this message */
	if (!c || !plugin_active(c))
		return;

	LOG(("died"));
	c->data.plugin.opened = false;

	if (pmc->flags & 0x4) {
		LOG(("plugin_closed: 0x%x: %s", pmc->error_number,
							pmc->error_text));
		/* not really important enough to do a warn_user */
		gui_window_set_status(c->data.plugin.bw->window,
					pmc->error_text);
	}
}

/**
 * Handles receipt of plugin_reshape_request messages
 *
 * \param message The message to handle
 */
void plugin_reshape_request(wimp_message *message)
{
	struct content *c;
	struct box *b;
	union content_msg_data data;
	plugin_message_reshape_request *pmrr = (plugin_message_reshape_request*)&message->data;

	/* retrieve our content */
	c = (struct content *)pmrr->browser;

	/* check we expect this message */
	if (!c || !plugin_active(c))
		return;

	LOG(("handling reshape request"));

	/* we can be called prior to the box content being set up,
	 * so we set it up here. This is ok as the content won't change
	 * under us.
	 */
	c->data.plugin.box->object = c;

	/* should probably shift by x and y eig values here */
	c->width = pmrr->size.x / 2;
	c->height = pmrr->size.y / 2;
	c->data.plugin.box->style->width.width = CSS_WIDTH_AUTO;
	c->data.plugin.box->style->height.height = CSS_HEIGHT_AUTO;

	/* invalidate parent box widths */
	for (b = c->data.plugin.box->parent; b; b = b->parent)
		b->max_width = UNKNOWN_MAX_WIDTH;

	/* force a reformat of the parent */
	content_reformat(c->data.plugin.page,
				c->data.plugin.page->available_width, 0);
	/* redraw the window */
	content_broadcast(c->data.plugin.bw->current_content,
				CONTENT_MSG_REFORMAT, data);
	/* reshape the plugin */
	plugin_reformat(c, c->width, c->height);
}

/**
 * Handles receipt of plugin_status messages
 *
 * \param message The message to handle
 */
void plugin_status(wimp_message *message)
{
	struct content *c;
	plugin_message_status *pms = (plugin_message_status*)&message->data;

	/* retrieve our content */
	c = (struct content *)pms->browser;

	/* check we expect this message */
	if (!c || !plugin_active(c))
		return;

	gui_window_set_status(c->data.plugin.bw->window,
			(const char*)plugin_get_string_value(pms->message,
								(char*)pms));
}

/**
 * Handles receipt of plugin_stream_new messages
 *
 * \param message The message to handle
 */
void plugin_stream_new(wimp_message *message)
{
	struct content *c;
	int stream_type;
	plugin_message_stream_new *pmsn =
				(plugin_message_stream_new*)&message->data;

	LOG(("plugin_stream_new"));

	/* retrieve our content */
	c = (struct content *)pmsn->browser;

	/* check we expect this message */
	if (!c || !plugin_active(c))
		return;

	/* response to a message we sent */
	if (message->my_ref != 0) {
		c->data.plugin.browser_stream = (unsigned int)pmsn->browser;
		c->data.plugin.plugin_stream = (unsigned int)pmsn->stream;

		LOG(("flags: %x", pmsn->flags));

		stream_type = pmsn->flags & 0xF; /* bottom four bits */

		if (stream_type == 3) {
			LOG(("as file"));
			if (c->source_size == c->total_size)
				plugin_write_stream_as_file(c);
			else {
				LOG(("waiting for data"));
				c->data.plugin.file_stream_waiting = true;
			}
		}
		else if (stream_type < 3) {
			LOG(("write stream"));
			plugin_write_stream(c, 0);
		}
	}
	/* new stream, initiated by plugin */
	else {
		/** \todo plugin-initiated streams */
	}
}

/**
 * Handles receipt of plugin_stream_written messages
 *
 * \param message The message to handle
 */
void plugin_stream_written(wimp_message *message)
{
	struct content *c;
	plugin_message_stream_written *pmsw =
			(plugin_message_stream_written*)&message->data;

	/* retrieve our box */
	c = (struct content *)pmsw->browser;

	/* check we expect this message */
	if (!c || !plugin_active(c))
		return;

	LOG(("got written"));

	plugin_write_stream(c, pmsw->length);
}

/**
 * Handles plugin_url_access messages
 *
 * \param message The message to handle
 */
void plugin_url_access(wimp_message *message)
{
	struct content *c;
	plugin_full_message_notify pmn;
	os_error *error;
	plugin_message_url_access *pmua =
				(plugin_message_url_access*)&message->data;
	bool notify = false, post = false, file = false;
	char *url = plugin_get_string_value(pmua->url, (char*)pmua);
	char *window;

	if (pmua->flags & 0x01) notify = true;
	if (pmua->flags & 0x02) post = true;
	if (pmua->flags & 0x04) file = true;

	/* retrieve our content */
	c = (struct content *)pmua->browser;

	/* check we expect this message */
	if (!c || !plugin_active(c))
		return;

	/* fetch url to window */
	if (pmua->target_window.offset != 0 &&
			pmua->target_window.pointer != 0) {
		window = plugin_get_string_value(pmua->target_window,
								(char*)pmua);
		LOG(("flags: %d, url: %s, window: %s", pmua->flags, url, window));
		/** \todo proper _parent and _self support (needs frames)
		 *        other window names
		 */
		if (!post) { /* GET request */
			if (strcasecmp(window, "_self") == 0 ||
			    strcasecmp(window, "_parent") == 0 ||
			    strcasecmp(window, "_top") == 0 ||
			    strcasecmp(window, "") == 0) {
				browser_window_go(c->data.plugin.bw, url, 0);
			}
			else if (strcasecmp(window, "_blank") == 0) {
				browser_window_create(url, NULL, 0);
			}
		}
		else { /* POST request */
			/* fetch URL */
		}
	}
	/* fetch data and stream to plugin */
	else {
		if (!post) { /* GET request */
			/* fetch URL */
		}
		else { /* POST request */
			/* fetch URL */
		}

		/* stream data to plugin */
	}

	if (notify) {
		/* send message_plugin_notify to plugin task */
		pmn.size = 44;
		pmn.your_ref = message->my_ref;
		pmn.action = message_PLUG_IN_NOTIFY;
		pmn.flags = 0;
		pmn.plugin = pmua->plugin;
		pmn.browser = pmua->browser;
		pmn.url.pointer = url;
		pmn.reason = (plugin_notify_reason)0;
		pmn.notify_data = pmua->notify_data;

		error = xwimp_send_message(wimp_USER_MESSAGE,
				(wimp_message*)&pmn, message->sender);
		if (error) {
			return;
		}
	}
}

/**
 * Creates a plugin stream
 *
 * \param c The content to fetch the data for
 */
void plugin_create_stream(struct content *c)
{
	plugin_full_message_stream_new pmsn;
	os_error *error;

	pmsn.size = 64;
	pmsn.your_ref = 0;
	pmsn.action = message_PLUG_IN_STREAM_NEW;
	pmsn.flags = 0;
	pmsn.plugin = (plugin_p)c->data.plugin.plugin;
	pmsn.browser = (plugin_b)c->data.plugin.browser;
	pmsn.stream = (plugin_s)0;
	pmsn.browser_stream = (plugin_bs)c->data.plugin.browser;
	pmsn.url.pointer = c->url;
	pmsn.end = c->total_size;
	pmsn.last_modified_date = 0;
	pmsn.notify_data = 0;
	pmsn.mime_type.pointer = c->mime_type;
	pmsn.target_window.offset = 0;

	LOG(("Sending message &4D548"));
	error = xwimp_send_message(wimp_USER_MESSAGE_RECORDED,
		(wimp_message*)&pmsn, (wimp_t)c->data.plugin.plugin_task);
	if (error) {
		return;
	}
}

/**
 * Writes to an open stream
 *
 * \param c      The content to write data to
 * \param consumed The amount of data consumed
 */
void plugin_write_stream(struct content *c, unsigned int consumed)
{
	plugin_full_message_stream_write pmsw;
	os_error *error;

	c->data.plugin.consumed += consumed;

	pmsw.size = 68;
	pmsw.your_ref = 0;
	pmsw.action = message_PLUG_IN_STREAM_WRITE;
	pmsw.flags = 0;
	pmsw.plugin = (plugin_p)c->data.plugin.plugin;
	pmsw.browser = (plugin_b)c->data.plugin.browser;
	pmsw.stream = (plugin_s)c->data.plugin.plugin_stream;
	pmsw.browser_stream = (plugin_bs)c->data.plugin.browser_stream;
	pmsw.url.pointer = c->url;
	/* end of stream is total_size
	 * (which is conveniently 0 if unknown)
	 */
	pmsw.end = c->total_size;
	pmsw.last_modified_date = 0;
	pmsw.notify_data = 0;
	/* offset into data is amount of data consumed by plugin already */
	pmsw.offset = c->data.plugin.consumed;
	/* length of data available */
	pmsw.length = c->source_size - c->data.plugin.consumed;
	/* pointer to available data */
	pmsw.data = (byte*)c->source_data + c->data.plugin.consumed;

	/* still have data to send */
	if (c->data.plugin.consumed < c->source_size) {
		LOG(("Sending message &4D54A"));
		error = xwimp_send_message(wimp_USER_MESSAGE_RECORDED,
			(wimp_message *)&pmsw,
			(wimp_t)c->data.plugin.plugin_task);
		if (error) {
			return;
		}
	}
	else if (c->source_size < c->total_size) {
		/* the plugin has consumed all the available data,
		 * but there's still more to fetch, so we wait for
		 * 20 cs then try again (note that streams of unknown
		 * total length won't ever get in here as c->total_size
		 * will be 0)
		 */
		schedule(20, plugin_stream_write_callback, c);
	}
	/* no further data => destroy stream */
	else {
		plugin_destroy_stream(c);
	}
}

/**
 * Stream write callback - used to wait for data to download
 *
 * \param p The appropriate content struct
 */
void plugin_stream_write_callback(void *p)
{
	struct content *c = (struct content *)p;

	/* remove ourselves from the schedule queue */
	schedule_remove(plugin_stream_write_callback, p);

	/* continue writing stream */
	plugin_write_stream(c, 0);
}

/**
 * Writes a stream as a file
 *
 * \param c The content to write data to
 */
void plugin_write_stream_as_file(struct content *c)
{
	plugin_full_message_stream_as_file pmsaf;
	unsigned int filetype;
	os_error *error;

	c->data.plugin.datafile =
		calloc(strlen(getenv("Wimp$ScrapDir"))+13+10, sizeof(char));

	if (!c->data.plugin.datafile) {
		LOG(("malloc failed"));
		warn_user("NoMemory", 0);
		plugin_destroy_stream(c);
		return;
	}

	/* create filename */
	sprintf(c->data.plugin.datafile, "%s.WWW.NetSurf.d%x",
		getenv("Wimp$ScrapDir"),
		(unsigned int)c->data.plugin.box->object_params);

	pmsaf.size = 60;
	pmsaf.your_ref = 0;
	pmsaf.action = message_PLUG_IN_STREAM_AS_FILE;
	pmsaf.flags = 0;
	pmsaf.plugin = (plugin_p)c->data.plugin.plugin;
	pmsaf.browser = (plugin_b)c->data.plugin.browser;
	pmsaf.stream = (plugin_s)c->data.plugin.plugin_stream;
	pmsaf.browser_stream = (plugin_bs)c->data.plugin.browser_stream;
	pmsaf.url.pointer = c->url;
	pmsaf.end = 0;
	pmsaf.last_modified_date = 0;
	pmsaf.notify_data = 0;
	pmsaf.filename.pointer = c->data.plugin.datafile;

	error = xmimemaptranslate_mime_type_to_filetype(c->mime_type,
							(bits *) &filetype);
	if (error) {
		return;
	}

	error = xosfile_save_stamped((char const*)c->data.plugin.datafile,
			filetype, c->source_data,
			c->source_data + c->source_size);
	if (error) {
		return;
	}

	LOG(("Sending message &4D54C"));
	error = xwimp_send_message(wimp_USER_MESSAGE,
		(wimp_message *)&pmsaf, (wimp_t)c->data.plugin.plugin_task);
	if (error) {
		return;
	}
}

/**
 * Destroys a plugin stream
 *
 * \param c The content to destroy
 */
void plugin_destroy_stream(struct content *c)
{
	plugin_full_message_stream_destroy pmsd;
	os_error *error;

	/* reset stream */
	c->data.plugin.consumed = 0;

	pmsd.size = 60;
	pmsd.your_ref = 0;
	pmsd.action = message_PLUG_IN_STREAM_DESTROY;
	pmsd.flags = 0;
	pmsd.plugin = (plugin_p)c->data.plugin.plugin;
	pmsd.browser = (plugin_b)c->data.plugin.browser;
	pmsd.stream = (plugin_s)c->data.plugin.plugin_stream;
	pmsd.browser_stream = (plugin_bs)c->data.plugin.browser_stream;
	pmsd.url.pointer = c->url;
	pmsd.end = c->total_size;
	pmsd.last_modified_date = 0;
	pmsd.notify_data = 0;
	pmsd.reason = plugin_STREAM_DESTROY_FINISHED;

	LOG(("Sending message &4D549"));
	error = xwimp_send_message(wimp_USER_MESSAGE,
		(wimp_message *)&pmsd, (wimp_t)c->data.plugin.plugin_task);
	if (error) {
		return;
	}
}

/**
 * Writes the plugin parameters file
 *
 * \param c      Content to write parameters for
 * \param params Plugin parameters struct
 * \return true on success, false otherwise
 */
bool plugin_write_parameters_file(struct content *c,
		struct object_params *params)
{
	struct plugin_params *temp;
	struct plugin_param_item *ppi;
	struct plugin_param_item *pilist = 0;
	char bgcolor[10] = {0};
	FILE *fp;

	/* Create the file */
	xosfile_create_dir("<Wimp$ScrapDir>.WWW", 77);
	xosfile_create_dir("<Wimp$ScrapDir>.WWW.NetSurf", 77);
	/* path + filename + terminating NUL */
	c->data.plugin.filename =
		calloc(strlen(getenv("Wimp$ScrapDir"))+13+10, sizeof(char));

	if (!c->data.plugin.filename) {
		LOG(("malloc failed"));
		warn_user("NoMemory", 0);
		return false;
	}
	sprintf(c->data.plugin.filename, "%s.WWW.NetSurf.p%x",
			getenv("Wimp$ScrapDir"), (unsigned int)params);
	LOG(("filename: %s", c->data.plugin.filename));

	/* Write object attributes first */

	/* classid is checked first */
	if (params->classid != 0 && params->codetype != 0) {
		if (!plugin_add_item_to_pilist(&pilist,
				PLUGIN_PARAMETER_DATA, "CLASSID",
				(const char*)params->classid,
				(const char*)params->codetype))
			goto error;
	}
	/* otherwise, we check the data attribute */
	else if (params->data !=0 && params->type != 0) {
		if (!plugin_add_item_to_pilist(&pilist,
				PLUGIN_PARAMETER_DATA, "DATA",
				(const char *)params->data,
				(const char *)params->type))
			goto error;
	}

	/* if codebase is specified, write it as well */
	if (params->codebase != 0) {
		if (!plugin_add_item_to_pilist(&pilist,
				PLUGIN_PARAMETER_DATA, "CODEBASE",
				(const char *)params->codebase,
				NULL))
			goto error;
	}

	/* Iterate through the parameter list, creating the parameters
	 * file as we go. We can free up the memory as we go.
	 */
	while (params->params != 0) {
		LOG(("name: %s", params->params->name == 0 ? "not set" : params->params->name));
		LOG(("value: %s", params->params->value == 0 ? "not set" : params->params->value));
		LOG(("type: %s", params->params->type == 0 ? "not set" : params->params->type));
		LOG(("valuetype: %s", params->params->valuetype));


		if (strcasecmp(params->params->valuetype, "data") == 0)
			if (!plugin_add_item_to_pilist(&pilist,
					PLUGIN_PARAMETER_DATA,
					(const char *)params->params->name,
					(const char *)params->params->value,
					(const char *)params->params->type))
				goto error;
		if (strcasecmp(params->params->valuetype, "ref") == 0)
			if (!plugin_add_item_to_pilist(&pilist,
					PLUGIN_PARAMETER_URL,
					(const char *)params->params->name,
					(const char *)params->params->value,
					(const char *)params->params->type))
				goto error;
		if (strcasecmp(params->params->valuetype, "object") == 0)
			if (!plugin_add_item_to_pilist(&pilist,
					PLUGIN_PARAMETER_OBJECT,
					(const char *)params->params->name,
					(const char *)params->params->value,
					(const char *)params->params->type))
				goto error;

		temp = params->params;
		params->params = params->params->next;

		free(temp->name);
		free(temp->value);
		free(temp->type);
		free(temp->valuetype);
		temp->name = temp->value = temp->type = temp->valuetype = 0;
		free(temp);
		temp = 0;
	}

	/* Now write mandatory special parameters */

	/* BASEHREF */
	if (!plugin_add_item_to_pilist(&pilist, PLUGIN_PARAMETER_SPECIAL,
					"BASEHREF",
					(const char *)params->basehref,
					NULL))
		goto error;

	/* USERAGENT */
	if (!plugin_add_item_to_pilist(&pilist, PLUGIN_PARAMETER_SPECIAL,
					"USERAGENT", "NetSurf", NULL))
		goto error;

	/* UAVERSION */
	if (!plugin_add_item_to_pilist(&pilist, PLUGIN_PARAMETER_SPECIAL,
					"UAVERSION", "0.01", NULL))
		goto error;

	/* APIVERSION */
	if (!plugin_add_item_to_pilist(&pilist, PLUGIN_PARAMETER_SPECIAL,
					"APIVERSION", "1.10", NULL))
		goto error;

	/* BGCOLOR */
	if (c->data.plugin.box->style->background_color <= 0xFFFFFF)
		sprintf(bgcolor, "%X00",
		(unsigned int)c->data.plugin.box->style->background_color);
	else
		sprintf(bgcolor, "FFFFFF");
	if (!plugin_add_item_to_pilist(&pilist, PLUGIN_PARAMETER_SPECIAL,
					"BGCOLOR",
					(const char *)bgcolor,
					NULL))
		goto error;

	/* Write file */
	fp = fopen(c->data.plugin.filename, "wb+");

	while (pilist != 0) {
		fwrite(&pilist->type, (unsigned int)sizeof(int), 1, fp);
		fwrite(&pilist->rsize, (unsigned int)sizeof(int), 1, fp);

		fwrite(&pilist->nsize, (unsigned int)sizeof(int), 1, fp);
		fwrite(pilist->name, (unsigned int)strlen(pilist->name), 1, fp);
		for (; pilist->npad != 0; pilist->npad--)
			fputc('\0', fp);

		fwrite(&pilist->vsize, (unsigned int)sizeof(int), 1, fp);
		fwrite(pilist->value, (unsigned int)strlen(pilist->value), 1, fp);
		for(; pilist->vpad != 0; pilist->vpad--)
			fputc('\0', fp);

		fwrite(&pilist->msize, (unsigned int)sizeof(int), 1, fp);
		if (pilist->msize > 0) {
			fwrite(pilist->mime_type,
				(unsigned int)strlen(pilist->mime_type), 1, fp);
			for (; pilist->mpad != 0; pilist->mpad--)
				fputc('\0', fp);
		}

		ppi = pilist;
		pilist = pilist->next;

		free(ppi->name);
		free(ppi->value);
		free(ppi->mime_type);
		ppi->name = ppi->value = ppi->mime_type = 0;
		free(ppi);
		ppi = 0;
	}

	fwrite("\0", sizeof(char), 4, fp);

	fclose(fp);

	return true;

error:
	while (pilist != 0) {
		ppi = pilist;
		pilist = pilist->next;

		free(ppi->name);
		free(ppi->value);
		free(ppi->mime_type);
		ppi->name = ppi->value = ppi->mime_type = 0;
		free(ppi);
		ppi = 0;
	}

	while (params->params) {
		temp = params->params;
		params->params = params->params->next;

		free(temp->name);
		free(temp->value);
		free(temp->type);
		free(temp->valuetype);
		temp->name = temp->value = temp->type = temp->valuetype = 0;
		free(temp);
		temp = 0;
	}

	free(c->data.plugin.filename);
	c->data.plugin.filename = 0;
	return false;
}

/**
 * Calculates the size of a parameter file record
 *
 * \param name Record name
 * \param data Record data
 * \param mime Record mime type
 * \return length of record
 */
int plugin_calculate_rsize(const char* name, const char* data,
		const char* mime)
{
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
 * Adds an item to the list of parameter file records
 *
 * \param pilist    Pointer to list of parameters
 * \param type      Type of record to add
 * \param name      Name of record
 * \param value     Value of record
 * \param mime_type Mime type of record
 * \return true on success, false otherwise
 */
bool plugin_add_item_to_pilist(struct plugin_param_item **pilist,
		plugin_parameter_type type, const char* name,
		const char* value, const char* mime_type)
{
	struct plugin_param_item *ppi = calloc(1, sizeof(*ppi));

	if (!ppi)
		return false;

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
	ppi->name = strdup(name);
	if (!ppi->name) {
		free(ppi);
		return false;
	}

	ppi->npad = 4 - (ppi->nsize%4 == 0 ? 4 : ppi->nsize%4);
	ppi->vsize = strlen(value);
	ppi->value = strdup(value);
	if (!ppi->value) {
		free(ppi->name);
		free(ppi);
		return false;
	}

	ppi->vpad = 4 - (ppi->vsize%4 == 0 ? 4 : ppi->vsize%4);
	if(mime_type != 0) {
		ppi->msize = strlen(mime_type);
		ppi->mime_type = strdup(mime_type);
		if (!ppi->mime_type) {
			free(ppi->name);
			free(ppi->value);
			free(ppi);
			return false;
		}

		ppi->mpad = 4 - (ppi->msize%4 == 0 ? 4 : ppi->msize%4);
	}

	ppi->next = (*pilist);
	(*pilist) = ppi;
	return true;
}

/**
 * Utility function to grab string data from plugin message blocks
 *
 * \param string Containing structure
 * \param msg    Containing message
 * \return the string data
 */
char *plugin_get_string_value(os_string_value string, char *msg)
{
	if(string.offset == 0 || string.offset > 256) {
		return string.pointer;
	}
	return &msg[string.offset];
}

/**
 * Determines whether a content is still active
 *
 * \param c The content to examine
 * \return true if active, false otherwise
 */
bool plugin_active(struct content *c)
{
	struct content *d;

	if (c->user_list == 0)
		return false;

	for (d = content_list; d; d = d->next) {
		if (d == c)
			return true;
	}

	return false;
}
#endif
