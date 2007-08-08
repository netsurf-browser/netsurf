/*
 * Copyright 2003,4 John M Bell <jmb202@ecs.soton.ac.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
 * 	Streaming data from a plugin is not supported
 *
 * Messages:
 * 	Most Plugin_Opening flags not supported
 * 	No support for Plugin_Focus, Plugin_Busy, Plugin_Action
 * 	No support for Plugin_Abort, Plugin_Inform, Plugin_Informed
 * 	Plugin_URL_Access ignores POST requests.
 *
 * Helpers are not supported (system variable detection is #if 0ed out)
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

#include "utils/config.h"
#include "content/content.h"
#include "content/fetch.h"
#include "content/fetchcache.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "render/html.h"
#include "render/box.h"
#include "riscos/gui.h"
#include "riscos/options.h"
#include "riscos/plugin.h"
#include "riscos/theme.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"


#ifdef WITH_PLUGIN

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

struct plugin_stream {
	struct plugin_stream *next;	/* next in list */

	struct content *plugin;		/* the plugin content */
	struct content *c;		/* the content being fetched for
					 * this stream (may be the same as
					 * plugin iff we've been asked to
					 * fetch the data resource for the
					 * plugin task) */

	/* browser stream handle is the address of this struct in memory */
	plugin_s pluginh;		/* plugin stream handle */

	/* We only support stream types 0 and 3 (Normal and As file only)
	 * Type 1 (Seek only) streams are treated as type 0.
	 * Type 2 (As file) streams are treated as type 3.
	 * Streams are never seekable.
	 */
	enum { NORMAL = plugin_STREAM_NEW_TYPE_NORMAL,
	       AS_FILE = plugin_STREAM_NEW_TYPE_AS_FILE_ONLY } type;
	union {
		struct {
			char *datafile;	/* filename of filestreamed file */

			/* we need this flag as we should only send stream
			 * destroy once. This struct may still persist after
			 * the stream has ended in the case where it's a
			 * file only stream, as we've still got to destroy
			 * the temporary file. We can only do this when
			 * we're certain the plugin's no longer using it
			 * (i.e. after we've sent the plugin close message)
			 */
			bool destroyed;	/* have we destroyed this stream? */
			bool waiting;	/* waiting for data to arrive */
		} file;
		struct {
			unsigned int consumed;	/* size of data consumed by plugin */
			/* The following is nasty, but necessary to prevent
			 * a race condition between the plugin application
			 * handling the stream write message and our fetch
			 * code reallocing the data buffer (and potentially
			 * relocating it)
			 */
#define PLUGIN_STREAM_BUFFER_SIZE (32*1024)
			char buffer[PLUGIN_STREAM_BUFFER_SIZE];	/* buffer for data chunk */
		} normal;
	} stream;
};

#define PLUGIN_SCHEDULE_WAIT (40)	/* time (in cs) to wait between processing data chunks */

#define PLUGIN_PREFIX "Alias$@PlugInType_"
#define HELPER_PREFIX "Alias$@HelperType_"
#define SYSVAR_BUF_SIZE (25)		/* size of buffer to hold system variable */

static bool plugin_create_sysvar(const char *mime_type, char* sysvar,
		bool helper);
static void plugin_create_stream(struct content *plugin, struct content *c,
		const char *url);
static bool plugin_send_stream_new(struct plugin_stream *p);
static void plugin_write_stream(struct plugin_stream *p,
		unsigned int consumed);
static void plugin_stream_write_callback(void *p);
static void plugin_stream_as_file_callback(void *p);
static void plugin_write_stream_as_file(struct plugin_stream *p);
static void plugin_destroy_stream(struct plugin_stream *p,
		plugin_stream_destroy_reason reason);
static bool plugin_write_parameters_file(struct content *c,
		struct object_params *params, const char *base);
static int plugin_calculate_rsize(const char* name, const char* data,
		const char* mime);
static bool plugin_add_item_to_pilist(struct plugin_param_item **pilist,
		plugin_parameter_type type, const char* name,
		const char* value, const char* mime_type);
static char *plugin_get_string_value(os_string_value string, char *msg);
static bool plugin_active(struct content *c);
static void plugin_stream_free(struct plugin_stream *p);
static bool plugin_start_fetch(struct plugin_stream *p, const char *url);
static void plugin_stream_callback(content_msg msg, struct content *c,
		intptr_t p1, intptr_t p2, union content_msg_data data);
static void plugin_fetch_callback(fetch_msg msg, void *p, const void *data,
		unsigned long size);

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
	c->data.plugin.opened = false;
	c->data.plugin.repeated = 0;
	c->data.plugin.browser = 0;
	c->data.plugin.plugin = 0;
	c->data.plugin.plugin_task = 0;
	c->data.plugin.reformat_pending = false;
	c->data.plugin.width = 0;
	c->data.plugin.height = 0;
	c->data.plugin.streams = 0;

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
 * Handle a window containing a CONTENT_PLUGIN being opened.
 *
 * \param  c       content that has been opened
 * \param  bw      browser window containing the content
 * \param  page    content of type CONTENT_HTML containing c, or 0 if not an
 *                 object within a page
 * \param  index   index in page->data.html.object, or 0 if not an object
 * \param  box     box containing c, or 0 if not an object
 * \param  params  object parameters, or 0 if not an object
 */
void plugin_open(struct content *c, struct browser_window *bw,
		struct content *page, unsigned int index, struct box *box,
		struct object_params *params)
{
	bool standalone = false, helper = false;
	const char *base;
	char sysvar[SYSVAR_BUF_SIZE];
	char *varval;
	plugin_full_message_open pmo;
	wimp_window_state state;
	os_error *error;

	if (option_no_plugins)
		return;

	if (!params) {
		/* this is a standalone plugin, so fudge the parameters */
		params = calloc(1, sizeof(struct object_params));
		if (!params) {
			warn_user("NoMemory", 0);
			goto error;
		}

		params->data = strdup(c->url);
		if (!params->data) {
			warn_user("NoMemory", 0);
			goto error;
		}
		params->type = strdup(c->mime_type);
		if (!params->type) {
			warn_user("NoMemory", 0);
			goto error;
		}
		standalone = true;
	}

	/* we only do this here because the box is needed by
	 * write_parameters_file. Ideally it would be at the
	 * end of this function with the other writes to c->data.plugin
	 */
	c->data.plugin.box = box;

	if (params->codebase)
		base = params->codebase;
	else if (page)
		base = page->data.html.base_url;
	else
		base = c->url;

	LOG(("writing parameters file"));
	if (!plugin_write_parameters_file(c, params, base))
		goto error;

	/* get contents of Alias$@PlugInType_xxx variable */
	if (!plugin_create_sysvar(c->mime_type, sysvar, false))
		goto error;

	varval = getenv(sysvar);
	LOG(("%s: '%s'", sysvar, varval));
	if(!varval) {
#if 0
		if (!plugin_create_sysvar(c->mime_type, sysvar, true))
			goto error;
		varval = getenv(sysvar);
		if (!varval)
			goto error;
		helper = true;
#else
		goto error;
#endif
	}

	/* The browser instance handle is the content struct pointer */
	c->data.plugin.browser = (unsigned int)c;

	pmo.size = 60;
	pmo.your_ref = 0;
	pmo.action = message_PLUG_IN_OPEN;
	pmo.flags = helper ? plugin_OPEN_AS_HELPER : 0;
	pmo.reserved = 0;
	pmo.browser = (plugin_b)c->data.plugin.browser;
	pmo.parent_window = bw->window->window;

	/* initial position/dimensions */
	if (standalone) {
		/* if standalone, try to fill the browser window */
		state.w = bw->window->window;
		error = xwimp_get_window_state(&state);
		if (error)
			goto error;

		pmo.bbox.x0 = 10;
		/* avoid toolbar */
		pmo.bbox.y1 = -10 - (bw->window->toolbar ?
					bw->window->toolbar->height : 0);
		pmo.bbox.x1 = (state.visible.x1 - state.visible.x0) - 10;
		pmo.bbox.y0 = (state.visible.y0 - state.visible.y1) - 10;
	}
	else {
		/* open off the left hand edge of the work area */
		pmo.bbox.x0 = -100;
		pmo.bbox.x1 = pmo.bbox.y0 = 0;
		pmo.bbox.y1 = 100;
	}

	error = xmimemaptranslate_mime_type_to_filetype(c->mime_type,
						&pmo.file_type);
	if (error) {
		goto error;
	}
	pmo.filename.pointer = c->data.plugin.filename;

	c->data.plugin.repeated = 0;

	LOG(("sending message"));
	error = xwimp_send_message(wimp_USER_MESSAGE_RECORDED,
				(wimp_message *)&pmo, wimp_BROADCAST);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
		goto error;
	}

	c->data.plugin.bw = bw;
	c->data.plugin.page = page;
	c->data.plugin.taskname = strdup(varval);

error:
	/* clean up standalone stuff */
	if (standalone) {
		free(params->type);
		free(params->data);
		free(params);
	}

	LOG(("done"));
}


/**
 * Handle a window containing a CONTENT_PLUGIN being closed.
 *
 * \param c      The content to close
 */
void plugin_close(struct content *c)
{
	struct plugin_stream *p, *q;
	plugin_full_message_close pmc;
	os_error *error;

	LOG(("plugin_close"));

	if (!plugin_active(c) || !c->data.plugin.opened)
		return;

	/* destroy all active streams */
	for (p = c->data.plugin.streams; p; p = q) {
		q = p->next;

		plugin_destroy_stream(p, plugin_STREAM_DESTROY_USER_REQUEST);
	}

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

	/* delete any temporary files */
	for (p = c->data.plugin.streams; p; p = q) {
		q = p->next;

		assert(p->type == AS_FILE);

		/* delete the data file used to send the
		 * data to the plugin */
		xosfile_delete(p->stream.file.datafile, 0, 0, 0, 0, 0);

		/* and destroy the struct */
		free(p->stream.file.datafile);
		free(p);
	}

	/* paranoia */
	c->data.plugin.streams = 0;
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

	/* top left of plugin area, relative to top left of browser window */
	if (c->data.plugin.box) {
		box_coords(c->data.plugin.box, &x, &y);
	}
	else {
		/* standalone */
		x = 10 / 2;
		/* avoid toolbar */
		y = (10 + (c->data.plugin.bw->window->toolbar ?
			c->data.plugin.bw->window->toolbar->height : 0)) / 2;
	}

	pmr.size = 52;
	pmr.your_ref = 0;
	pmr.action = message_PLUG_IN_RESHAPE;
	pmr.flags = 0;

	pmr.plugin = (plugin_p)c->data.plugin.plugin;
	pmr.browser = (plugin_b)c->data.plugin.browser;
	pmr.parent_window = c->data.plugin.bw->window->window;
	pmr.bbox.x0 = x * 2;
	pmr.bbox.y1 = -y * 2;

	if (c->data.plugin.box) {
		pmr.bbox.x1 = pmr.bbox.x0 + c->data.plugin.box->width * 2;
		pmr.bbox.y0 = pmr.bbox.y1 - c->data.plugin.box->height * 2;
	}
	else {
		/* standalone */
		pmr.bbox.x1 = pmr.bbox.x0 + width * 2;
		pmr.bbox.y0 = pmr.bbox.y1 - height * 2;
	}

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
 * \param sysvar    Pointer to buffer of length >= SYSVAR_BUF_SIZE into
 *                  which the string should be written
 * \param helper    Whether we're interested in the helper variable
 * \return true on success, false otherwise.
 */
bool plugin_create_sysvar(const char *mime_type, char* sysvar, bool helper)
{
	unsigned int *fv;
	os_error *e;

	e = xmimemaptranslate_mime_type_to_filetype(mime_type, (bits *) &fv);
	if (e) {
		return false;
	}

	snprintf(sysvar, SYSVAR_BUF_SIZE, "%s%03x",
			helper ? HELPER_PREFIX : PLUGIN_PREFIX,
			(unsigned int)fv);

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
	char sysvar[SYSVAR_BUF_SIZE];

	/* Look for Alias$@PluginType_xxx */
	if (plugin_create_sysvar(mime_type, sysvar, false)) {
		if (getenv(sysvar) != 0) {
			return true;
		}
	}
#if 0
	/* Look for Alias$@HelperType_xxx */
	if (plugin_create_sysvar(mime_type, sysvar, true)) {
		if (getenv(sysvar) != 0) {
			return true;
		}
	}
#endif
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

	if (pmo->flags & plugin_OPENING_WANTS_DATA_FETCHING) {
		LOG(("wants stream"));
		plugin_create_stream(c, c, NULL);
	}

	if (!(pmo->flags & plugin_OPENING_WILL_DELETE_PARAMETERS)) {
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

	if (pmc->flags & plugin_CLOSED_WITH_ERROR) {
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
	 * under us. However, the box may not exist (if we're standalone)
	 */
	if (c->data.plugin.box)
		c->data.plugin.box->object = c;

	/* should probably shift by x and y eig values here */
	c->width = pmrr->size.x / 2;
	c->height = pmrr->size.y / 2;

	if (c->data.plugin.box)
		/* invalidate parent box widths */
		for (b = c->data.plugin.box->parent; b; b = b->parent)
			b->max_width = UNKNOWN_MAX_WIDTH;

	if (c->data.plugin.page)
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
	struct plugin_stream *p;
	int stream_type;
	plugin_message_stream_new *pmsn =
				(plugin_message_stream_new*)&message->data;

	LOG(("plugin_stream_new"));

	p = (struct plugin_stream *)pmsn->browser_stream;

	/* check we expect this message */
	if (!p || !p->plugin || !plugin_active(p->plugin))
		return;

	/* response to a message we sent */
	if (message->my_ref != 0) {
		p->pluginh = pmsn->stream;

		LOG(("flags: %x", pmsn->flags));

		/* extract the stream type */
		stream_type = pmsn->flags & plugin_STREAM_NEW_TYPE;

		if (stream_type == plugin_STREAM_NEW_TYPE_AS_FILE_ONLY ||
				stream_type ==
					plugin_STREAM_NEW_TYPE_AS_FILE) {
			LOG(("as file"));

			p->type = AS_FILE;

			/* received all data => go ahead and stream
			 * we have to check the content's status too, as
			 * we could be dealing with a stream of unknown
			 * length (ie c->total_size == 0). If the status
			 * is CONTENT_STATUS_DONE, we've received all the
			 * data anyway, regardless of the total size.
			 */
			if (p->c->source_size == p->c->total_size ||
					p->c->status == CONTENT_STATUS_DONE)
				plugin_write_stream_as_file(p);
			else {
				LOG(("waiting for data"));
				p->stream.file.waiting = true;
				/* schedule a callback */
				schedule(PLUGIN_SCHEDULE_WAIT,
					plugin_stream_as_file_callback, p);
			}
		}
		else if (stream_type == plugin_STREAM_NEW_TYPE_SEEK_ONLY ||
				stream_type ==
					plugin_STREAM_NEW_TYPE_NORMAL) {
			LOG(("write stream"));
			plugin_write_stream(p, 0);
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
	struct plugin_stream *p;
	plugin_message_stream_written *pmsw =
			(plugin_message_stream_written*)&message->data;

	/* retrieve our stream context */
	p = (struct plugin_stream *)pmsw->browser_stream;

	/* check we expect this message */
	if (!p || !p->plugin || !plugin_active(p->plugin))
		return;

	LOG(("got written"));

	plugin_write_stream(p, pmsw->length);
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

	notify = (pmua->flags & plugin_URL_ACCESS_NOTIFY_COMPLETION);
	post = (pmua->flags & plugin_URL_ACCESS_USE_POST);
	file = (pmua->flags & plugin_URL_ACCESS_POST_FILE);

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
			if (strcasecmp(url,
			    c->data.plugin.bw->current_content->url) &&
			    (strcasecmp(window, "_self") == 0 ||
			    strcasecmp(window, "_parent") == 0 ||
			    strcasecmp(window, "_top") == 0 ||
			    strcasecmp(window, "") == 0)) {
				/* only open in current window if not
				 * already at the URL requested, else you
				 * end up in an infinite loop of fetching
				 * the same page
				 */
				browser_window_go(c->data.plugin.bw, url, 0, true);
			}
			else if (!option_block_popups &&
					strcasecmp(window, "_blank") == 0) {
				/* don't do this if popups are blocked */
				browser_window_create(url, NULL, 0, true);
			}
		}
		else { /* POST request */
			/* fetch URL */
		}
	}
	/* fetch data and stream to plugin */
	else {
		if (!post) { /* GET request */
			/* stream to plugin */
			plugin_create_stream(c, NULL, url);
		}
		else { /* POST request */
			/* fetch URL */
		}
	}

	/* this may be a little early to send this, but tough. */
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
 * \param plugin The content to fetch the data for
 * \param c The content being fetched, or NULL.
 * \param url The url of the resource to fetch, or NULL if content provided.
 */
void plugin_create_stream(struct content *plugin, struct content *c,
		const char *url)
{
	struct plugin_stream *p;

	assert(plugin && plugin->type == CONTENT_PLUGIN &&
			((c && !url) || (!c && url)));

	p = malloc(sizeof(struct plugin_stream));
	if (!p)
		return;

	if (url) {
		if (!plugin_start_fetch(p, url)) {
			free(p);
			return;
		}
	}
	else
		p->c = c;

	p->plugin = plugin;
	p->pluginh = 0;
	p->type = NORMAL;
	p->stream.normal.consumed = 0;

	/* add to head of list */
	p->next = plugin->data.plugin.streams;
	plugin->data.plugin.streams = p;

	if (url)
		/* we'll send this later, once some data is arriving */
		return;

	plugin_send_stream_new(p);
}

/**
 * Send a plugin stream new message
 *
 * \param p The stream context
 * \return true on success, false otherwise
 */
bool plugin_send_stream_new(struct plugin_stream *p)
{
	plugin_full_message_stream_new pmsn;
	os_error *error;

	pmsn.size = 64;
	pmsn.your_ref = 0;
	pmsn.action = message_PLUG_IN_STREAM_NEW;
	pmsn.flags = 0;
	pmsn.plugin = (plugin_p)p->plugin->data.plugin.plugin;
	pmsn.browser = (plugin_b)p->plugin->data.plugin.browser;
	pmsn.stream = (plugin_s)0;
	pmsn.browser_stream = (plugin_bs)p;
	pmsn.url.pointer = p->c->url;
	pmsn.end = p->c->total_size;
	pmsn.last_modified_date = 0;
	pmsn.notify_data = 0;
	pmsn.mime_type.pointer = p->c->mime_type;
	pmsn.target_window.offset = 0;

	LOG(("Sending message &4D548"));
	error = xwimp_send_message(wimp_USER_MESSAGE_RECORDED,
		(wimp_message*)&pmsn,
		(wimp_t)p->plugin->data.plugin.plugin_task);
	if (error) {
		plugin_stream_free(p);
		return false;
	}

	return true;
}

/**
 * Writes to an open stream
 *
 * \param c      The stream context
 * \param consumed The amount of data consumed
 */
void plugin_write_stream(struct plugin_stream *p, unsigned int consumed)
{
	plugin_full_message_stream_write pmsw;
	os_error *error;

	assert(p->type == NORMAL);

	p->stream.normal.consumed += consumed;

	pmsw.size = 68;
	pmsw.your_ref = 0;
	pmsw.action = message_PLUG_IN_STREAM_WRITE;
	pmsw.flags = 0;
	pmsw.plugin = (plugin_p)p->plugin->data.plugin.plugin;
	pmsw.browser = (plugin_b)p->plugin->data.plugin.browser;
	pmsw.stream = (plugin_s)p->pluginh;
	pmsw.browser_stream = (plugin_bs)p;
	pmsw.url.pointer = p->c->url;
	/* end of stream is p->c->total_size
	 * (which is conveniently 0 if unknown)
	 */
	pmsw.end = p->c->total_size;
	pmsw.last_modified_date = 0;
	pmsw.notify_data = 0;
	/* offset into data is amount of data consumed by plugin already */
	pmsw.offset = p->stream.normal.consumed;
	/* length of data available is <= sizeof fixed buffer */
	pmsw.length = p->c->source_size - p->stream.normal.consumed;
	if (pmsw.length >= PLUGIN_STREAM_BUFFER_SIZE)
		pmsw.length = PLUGIN_STREAM_BUFFER_SIZE;

	/* copy data into buffer */
	memcpy(p->stream.normal.buffer,
		p->c->source_data + p->stream.normal.consumed, pmsw.length);

	/* pointer to available data */
	pmsw.data = (byte*)(p->stream.normal.buffer);

	/* still have data to send */
	if (p->stream.normal.consumed < p->c->source_size) {
		LOG(("Sending message &4D54A"));
		error = xwimp_send_message(wimp_USER_MESSAGE_RECORDED,
			(wimp_message *)&pmsw,
			(wimp_t)p->plugin->data.plugin.plugin_task);
		if (error) {
			plugin_destroy_stream(p,
					plugin_STREAM_DESTROY_ERROR);
			return;
		}
	}
	else if (p->c->source_size < p->c->total_size) {
		/* the plugin has consumed all the available data,
		 * but there's still more to fetch, so we wait for
		 * 40 cs then try again (note that streams of unknown
		 * total length won't ever get in here as
		 * p->c->total_size will be 0)
		 */
		schedule(PLUGIN_SCHEDULE_WAIT,
				plugin_stream_write_callback, p);
	}
	/* no further data => destroy stream */
	else {
		plugin_destroy_stream(p, plugin_STREAM_DESTROY_FINISHED);
	}
}

/**
 * Stream write callback - used to wait for data to download
 *
 * \param p The stream context
 */
void plugin_stream_write_callback(void *p)
{
	/* remove ourselves from the schedule queue */
	schedule_remove(plugin_stream_write_callback, p);

	/* continue writing stream */
	plugin_write_stream((struct plugin_stream *)p, 0);
}

/**
 * Stream as file callback - used to wait for data to download
 *
 * \param p The stream context
 */
void plugin_stream_as_file_callback(void *p)
{
	struct plugin_stream *s = (struct plugin_stream *)p;

	/* remove ourselves from the schedule queue */
	schedule_remove(plugin_stream_as_file_callback, p);

	if (s->c->source_size < s->c->total_size ||
			s->c->status != CONTENT_STATUS_DONE) {
		/* not got all the data so wait some more */
		schedule(PLUGIN_SCHEDULE_WAIT,
				plugin_stream_as_file_callback, p);
		return;
	}

	/* deal with a plugin waiting for a file stream */
	if (s->stream.file.waiting) {
		s->stream.file.waiting = false;
		plugin_write_stream_as_file(s);
	}
}

/**
 * Writes a stream as a file
 *
 * \param c The stream context
 */
void plugin_write_stream_as_file(struct plugin_stream *p)
{
	plugin_full_message_stream_as_file pmsaf;
	unsigned int filetype;
	os_error *error;

	assert(p->type == AS_FILE);

	p->stream.file.datafile =
		calloc(strlen(getenv("Wimp$ScrapDir"))+13+10, sizeof(char));

	if (!p->stream.file.datafile) {
		LOG(("malloc failed"));
		warn_user("NoMemory", 0);
		plugin_destroy_stream(p, plugin_STREAM_DESTROY_ERROR);
		return;
	}

	/* create filename */
	sprintf(p->stream.file.datafile, "%s.WWW.NetSurf.d%x",
			getenv("Wimp$ScrapDir"), (unsigned int)p);

	pmsaf.size = 60;
	pmsaf.your_ref = 0;
	pmsaf.action = message_PLUG_IN_STREAM_AS_FILE;
	pmsaf.flags = 0;
	pmsaf.plugin = (plugin_p)p->plugin->data.plugin.plugin;
	pmsaf.browser = (plugin_b)p->plugin->data.plugin.browser;
	pmsaf.stream = (plugin_s)p->pluginh;
	pmsaf.browser_stream = (plugin_bs)p;
	pmsaf.url.pointer = p->c->url;
	pmsaf.end = p->c->total_size;
	pmsaf.last_modified_date = 0;
	pmsaf.notify_data = 0;
	pmsaf.filename.pointer = p->stream.file.datafile;

	error = xmimemaptranslate_mime_type_to_filetype(p->c->mime_type,
							(bits *) &filetype);
	if (error) {
		plugin_destroy_stream(p, plugin_STREAM_DESTROY_ERROR);
		return;
	}

	error = xosfile_save_stamped((char const*)p->stream.file.datafile,
			filetype, p->c->source_data,
			p->c->source_data + p->c->source_size);
	if (error) {
		plugin_destroy_stream(p, plugin_STREAM_DESTROY_ERROR);
		return;
	}

	LOG(("Sending message &4D54C"));
	error = xwimp_send_message(wimp_USER_MESSAGE,
		(wimp_message *)&pmsaf,
		(wimp_t)p->plugin->data.plugin.plugin_task);
	if (error) {
		plugin_destroy_stream(p, plugin_STREAM_DESTROY_ERROR);
		return;
	}

	plugin_destroy_stream(p, plugin_STREAM_DESTROY_FINISHED);
}

/**
 * Destroys a plugin stream
 *
 * \param c The stream context to destroy
 * \param reason The reason for the destruction
 */
void plugin_destroy_stream(struct plugin_stream *p,
		plugin_stream_destroy_reason reason)
{
	plugin_full_message_stream_destroy pmsd;
	os_error *error;

	if (p->type == AS_FILE && p->stream.file.destroyed)
		/* we've already destroyed this stream */
		return;

	/* stop any scheduled callbacks */
	if (p->type == NORMAL)
		schedule_remove(plugin_stream_write_callback, p);
	else
		schedule_remove(plugin_stream_as_file_callback, p);

	pmsd.size = 60;
	pmsd.your_ref = 0;
	pmsd.action = message_PLUG_IN_STREAM_DESTROY;
	pmsd.flags = 0;
	pmsd.plugin = (plugin_p)p->plugin->data.plugin.plugin;
	pmsd.browser = (plugin_b)p->plugin->data.plugin.browser;
	pmsd.stream = (plugin_s)p->pluginh;
	pmsd.browser_stream = (plugin_bs)p;
	pmsd.url.pointer = p->c->url;
	pmsd.end = p->c->total_size;
	pmsd.last_modified_date = 0;
	pmsd.notify_data = 0;
	pmsd.reason = reason;

	LOG(("Sending message &4D549"));
	error = xwimp_send_message(wimp_USER_MESSAGE,
		(wimp_message *)&pmsd,
		(wimp_t)p->plugin->data.plugin.plugin_task);
	if (error) {
		LOG(("0x%x %s", error->errnum, error->errmess));
	}

	plugin_stream_free(p);
}

/**
 * Writes the plugin parameters file
 *
 * \param  c       Content to write parameters for
 * \param  params  Plugin parameters struct
 * \param  base    base URL for object
 * \return true on success, false otherwise
 */
bool plugin_write_parameters_file(struct content *c,
		struct object_params *params, const char *base)
{
	struct object_param *p;
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
	 * file as we go.
	 */
	for (p = params->params; p != 0; p = p->next) {
		LOG(("name: %s", p->name == 0 ? "not set" : p->name));
		LOG(("value: %s", p->value == 0 ? "not set" : p->value));
		LOG(("type: %s", p->type == 0 ? "not set" : p->type));
		LOG(("valuetype: %s", p->valuetype));


		if (strcasecmp(p->valuetype, "data") == 0)
			if (!plugin_add_item_to_pilist(&pilist,
					PLUGIN_PARAMETER_DATA,
					(const char *)p->name,
					(const char *)p->value,
					(const char *)p->type))
				goto error;
		if (strcasecmp(p->valuetype, "ref") == 0)
			if (!plugin_add_item_to_pilist(&pilist,
					PLUGIN_PARAMETER_URL,
					(const char *)p->name,
					(const char *)p->value,
					(const char *)p->type))
				goto error;
		if (strcasecmp(p->valuetype, "object") == 0)
			if (!plugin_add_item_to_pilist(&pilist,
					PLUGIN_PARAMETER_OBJECT,
					(const char *)p->name,
					(const char *)p->value,
					(const char *)p->type))
				goto error;
	}

	/* Now write mandatory special parameters */

	/* BASEHREF */
	if (!plugin_add_item_to_pilist(&pilist, PLUGIN_PARAMETER_SPECIAL,
					"BASEHREF", base,
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
	if (c->data.plugin.box && c->data.plugin.box->style &&
			c->data.plugin.box->style->background_color
								<= 0xFFFFFF)
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

/**
 * Free a plugin_stream struct and unlink it from the list
 */
void plugin_stream_free(struct plugin_stream *p)
{
	if (p->c != p->plugin) {
		if (p->c->fetch) {
			/* abort fetch, if active */
			fetch_abort(p->c->fetch);
			p->c->fetch = 0;
			p->c->status = CONTENT_STATUS_DONE;
		}
		content_remove_user(p->c, plugin_stream_callback,
				(intptr_t)p, 0);
	}

	/* free normal stream context. file streams get freed later */
	if (p->type == NORMAL) {
		struct plugin_stream *q;
		for (q = p->plugin->data.plugin.streams; q && q->next != p;
				q = q->next)
			/* do nothing */;
		assert(q || p == p->plugin->data.plugin.streams);
		if (q)
			q->next = p->next;
		else
			p->plugin->data.plugin.streams = p->next;

		free(p);
	}
	else
		p->stream.file.destroyed = true;
}

/**
 * Initialise a fetch for a plugin
 *
 * \param p The stream context to fetch for
 * \param url The URL to fetch
 * \return true on successful fetch initiation and p->c filled in, false
 *         otherwise.
 */
bool plugin_start_fetch(struct plugin_stream *p, const char *url)
{
	char *url2;
	struct content *c;
	url_func_result res;

	assert(p && url);

	res = url_normalize(url, &url2);
	if (res != URL_FUNC_OK) {
		return false;
	}

	if (!fetch_can_fetch(url2)) {
		free(url2);
		return false;
	}

	c = fetchcache(url2, plugin_stream_callback, (intptr_t)p, 0,
			100, 100, true, 0, 0, false, true);
	free(url2);
	if (!c) {
		return false;
	}

	p->c = c;
	fetchcache_go(c, 0, plugin_stream_callback, (intptr_t)p, 0,
			100, 100, 0, 0, false, 0);

	return true;
}

/**
 * Callback for fetchcache() for plugin stream fetches.
 */
void plugin_stream_callback(content_msg msg, struct content *c,
		intptr_t p1, intptr_t p2, union content_msg_data data)
{
	struct plugin_stream *p = (struct plugin_stream *)p1;

	switch (msg) {
		case CONTENT_MSG_LOADING:
			assert(p->c == c);
			assert(c->type == CONTENT_OTHER);
			fetch_change_callback(c->fetch,
					plugin_fetch_callback, p);
			/* and kickstart the stream protocol */
			plugin_send_stream_new(p);
			break;

		case CONTENT_MSG_ERROR:
			/* The plugin we were fetching may have been
			 * redirected, in that case, the object pointers
			 * will differ, so ensure that the object that's
			 * in error is still in use by us before destroying
			 * the stream */
			if (p->c == c)
				plugin_destroy_stream(p,
						plugin_STREAM_DESTROY_ERROR);
			break;

		case CONTENT_MSG_REDIRECT:
			/* and re-start fetch with new URL */
			p->c = 0;
			if (!plugin_start_fetch(p, data.redirect))
				plugin_destroy_stream(p,
						plugin_STREAM_DESTROY_ERROR);
			break;

		case CONTENT_MSG_NEWPTR:
			p->c = c;
			break;

		case CONTENT_MSG_AUTH:
			/**\todo handle authentication */
			plugin_destroy_stream(p, plugin_STREAM_DESTROY_ERROR);
			break;

		case CONTENT_MSG_STATUS:
			/* ignore this */
			break;

#ifdef WITH_SSL
		case CONTENT_MSG_SSL:
			plugin_destroy_stream(p, plugin_STREAM_DESTROY_ERROR);
			break;
#endif

		case CONTENT_MSG_READY:
		case CONTENT_MSG_DONE:
		case CONTENT_MSG_REFORMAT:
		case CONTENT_MSG_REDRAW:
		default:
			/* not possible */
			assert(0);
			break;
	}
}

/**
 * Callback for plugin fetch
 */
void plugin_fetch_callback(fetch_msg msg, void *p, const void *data,
		unsigned long size)
{
	struct plugin_stream *s = p;
	union content_msg_data msg_data;

	switch (msg) {
		case FETCH_PROGRESS:
			break;

		case FETCH_DATA:
			if (!content_process_data(s->c, data, size)) {
				fetch_abort(s->c->fetch);
				s->c->fetch = 0;
			}
			break;

		case FETCH_FINISHED:
			s->c->fetch = 0;
			s->c->status = CONTENT_STATUS_DONE;
			break;

		case FETCH_ERROR:
			s->c->fetch = 0;
			s->c->status = CONTENT_STATUS_ERROR;
			msg_data.error = data;
			content_broadcast(s->c, CONTENT_MSG_ERROR, msg_data);
			break;

		case FETCH_TYPE:
		case FETCH_REDIRECT:
		case FETCH_NOTMODIFIED:
		case FETCH_AUTH:
#ifdef WITH_SSL
		case FETCH_CERT_ERR:
#endif
		default:
			/* not possible */
			assert(0);
			break;
	}
}

#endif
