/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Philip Pemberton <philpem@users.sourceforge.net>
 */

/** \file
 * Content handling (interface).
 *
 * The content functions manipulate struct contents, which correspond to URLs.
 *
 * Each content has a type. The type is used to call a specific implementation
 * of functions such as content_process_data().
 *
 * The source data fetched from the URL is placed in the source_data buffer as
 * it arrives.
 *
 * Contents have an associated set of users, which are informed by a callback
 * when the state of the content changes or something interesting happens.
 *
 * Optionally, contents may have instances (depending on type). Instances
 * represent copies of the same URL, for example if a page is open in two
 * windows, or a page contains the same image twice.
 *
 * The status of a content follows a fixed order. Certain content functions
 * change the state, and each change of state results in a message to all users
 * of the content. The diagram below shows this:
 * \dot
 *   digraph status {
 *     node [shape=plaintext, fontname=Helvetica, fontsize=9];
 *     edge [fontname=Helvetica, fontsize=9];
 *
 *     content_create -> TYPE_UNKNOWN [style=bold];
 *     TYPE_UNKNOWN -> content_set_type [style=bold];
 *     content_set_type -> LOADING [label=MSG_LOADING, style=bold];
 *     content_set_type -> ERROR [label=MSG_ERROR];
 *     LOADING -> content_process_data [style=bold];
 *     content_process_data -> LOADING [style=bold];
 *     content_process_data -> ERROR [label=MSG_ERROR];
 *     LOADING -> content_convert [style=bold];
 *     content_convert -> READY [label=MSG_READY, style=bold];
 *     content_convert -> DONE [label="MSG_READY\nMSG_DONE", style=bold];
 *     content_convert -> ERROR [label=MSG_ERROR];
 *     READY -> READY [style=bold];
 *     READY -> DONE [label=MSG_DONE, style=bold];
 *     READY -> content_stop;
 *     content_stop -> DONE [label=MSG_DONE];
 *
 *     TYPE_UNKNOWN [shape=ellipse];
 *     LOADING [shape=ellipse];
 *     READY [shape=ellipse];
 *     DONE [shape=ellipse];
 *     ERROR [shape=ellipse];
 *   }
 * \enddot
 *
 * To implement a new content type, implement the following functions:
 *
 * - <i>type</i>_create(): called to initialise type-specific fields in the
 *   content structure. Optional.
 *
 * - <i>type</i>_process_data(): called when some data arrives. Optional.
 *
 * - <i>type</i>_convert(): called when data has finished arriving. The
 *   content needs to be converted for display. Must set the status to one of
 *   CONTENT_STATUS_READY or CONTENT_STATUS_DONE if no error occurs. Optional,
 *   but probably required for non-trivial types.
 *
 * - <i>type</i>_reformat(): called when, for example, the window has been
 *   resized, and the content needs reformatting for the new size. Optional.
 *
 * - <i>type</i>_destroy(): called when the content is being destroyed. Free all
 *   resources. Optional.
 *
 * - <i>type</i>_redraw(): called to plot the content to screen.
 *
 * - <i>type</i>_stop(): called when the user interrupts in status
 *   CONTENT_STATUS_READY. Must stop any processing and set the status to
 *   CONTENT_STATUS_DONE. Required iff the status can be CONTENT_STATUS_READY.
 *
 * - <i>type</i>_(add|remove|reshape)_instance: ask James, this will probably
 *   be redesigned sometime.
 *
 * - <i>type</i>_create(), <i>type</i>_process_data(), <i>type</i>_convert():
 *   if an error occurs, must broadcast CONTENT_MSG_ERROR and return false.
 *   Optionally use warn_user() for serious errors. The _destroy function will
 *   be called soon after.
 */

#ifndef _NETSURF_DESKTOP_CONTENT_H_
#define _NETSURF_DESKTOP_CONTENT_H_

#include "netsurf/utils/config.h"
#include "netsurf/content/content_type.h"
#include "netsurf/css/css.h"
#include "netsurf/render/box.h"
#include "netsurf/render/font.h"
#include "netsurf/render/html.h"
#ifdef WITH_JPEG
#include "netsurf/riscos/jpeg.h"
#endif
#ifdef WITH_GIF
#include "netsurf/riscos/gif.h"
#endif
#ifdef WITH_PLUGIN
#include "netsurf/riscos/plugin.h"
#endif
#ifdef WITH_PNG
#include "netsurf/riscos/png.h"
#endif
#ifdef WITH_SPRITE
#include "netsurf/riscos/sprite.h"
#endif
#ifdef WITH_DRAW
#include "netsurf/riscos/draw.h"
#endif


struct fetch;


/** Used in callbacks to indicate what has occurred. */
typedef enum {
	CONTENT_MSG_LOADING,   /**< fetching or converting */
	CONTENT_MSG_READY,     /**< may be displayed */
	CONTENT_MSG_DONE,      /**< finished */
	CONTENT_MSG_ERROR,     /**< error occurred */
	CONTENT_MSG_STATUS,    /**< new status string */
	CONTENT_MSG_REDIRECT,  /**< replacement URL */
	CONTENT_MSG_REFORMAT,  /**< content_reformat done */
	CONTENT_MSG_REDRAW,    /**< needs redraw (eg. new animation frame) */
#ifdef WITH_AUTH
	CONTENT_MSG_AUTH       /**< authentication required */
#endif
} content_msg;

/** Extra data for some content_msg messages. */
union content_msg_data {
	const char *error;	/**< Error message, for CONTENT_MSG_ERROR. */
	char *redirect;	/**< Redirect URL, for CONTENT_MSG_REDIRECT. */
	/** Area of content which needs redrawing, for CONTENT_MSG_REDRAW. */
	struct {
		float x, y, width, height;
		/** Redraw the area fully. If false, object must be set,
		 * and only the object will be redrawn. */
		bool full_redraw;
		/** Object to redraw if full_redraw is false. */
		struct content *object;
		/** Coordinates to plot object at. */
		float object_x, object_y;
		/** Dimensions to plot object with. */
		float object_width, object_height;
	} redraw;
	char *auth_realm;	/**< Realm, for CONTENT_MSG_AUTH. */
};

/** Linked list of users of a content. */
struct content_user
{
	void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data);
	void *p1;
	void *p2;
	bool stop;
	struct content_user *next;
};

/** Corresponds to a single URL. */
struct content {
	char *url;		/**< URL, in standard form as from url_join. */
	content_type type;	/**< Type of content. */
	char *mime_type;	/**< Original MIME type of data, or 0. */

	enum {
		CONTENT_STATUS_TYPE_UNKNOWN,	/**< Type not yet known. */
		CONTENT_STATUS_LOADING,	/**< Content is being fetched or
					  converted and is not safe to display. */
		CONTENT_STATUS_READY,	/**< Some parts of content still being
					  loaded, but can be displayed. */
		CONTENT_STATUS_DONE,	/**< All finished. */
		CONTENT_STATUS_ERROR	/**< Error occurred, content will be
					  destroyed imminently. */
	} status;		/**< Current status. */

	int width, height;	/**< Dimensions, if applicable. */
	int available_width;	/**< Available width (eg window width). */

	/** Data dependent on type. */
	union {
		struct content_html_data html;
		struct content_css_data css;
#ifdef WITH_JPEG
		struct content_jpeg_data jpeg;
#endif
#ifdef WITH_GIF
		struct content_gif_data gif;
#endif
#ifdef WITH_PNG
		struct content_png_data png;
#endif
#ifdef WITH_SPRITE
		struct content_sprite_data sprite;
#endif
#ifdef WITH_DRAW
		struct content_draw_data draw;
#endif
#ifdef WITH_PLUGIN
		struct content_plugin_data plugin;
#endif
	} data;

	/** This content may be given to new users. Indicates that the content
	 *  was fetched using a simple GET, has not expired, and may be
	 *  shared between users. */
	bool fresh;

	unsigned int size;		/**< Estimated size of all data
					  associated with this content. */
	char *title;			/**< Title for browser window. */
	unsigned int active;		/**< Number of child fetches or
					  conversions currently in progress. */
	struct content_user *user_list;	/**< List of users. */
	char status_message[80];	/**< Text for status bar. */

	struct fetch *fetch;		/**< Associated fetch, or 0. */
	char *source_data;		/**< Source data, as received. */
	unsigned long source_size;	/**< Amount of data fetched so far. */
	unsigned long total_size;	/**< Total data size, 0 if unknown. */

	bool no_error_pages;		/**< Used by fetchcache(). */

	/** Array of first n rendering errors or warnings. */
	struct {
		const char *token;
		unsigned int line;	/**< Line no, 0 if not applicable. */
	} error_list[40];
	unsigned int error_count;	/**< Number of valid error entries. */

	struct content *prev;		/**< Previous in global content list. */
	struct content *next;		/**< Next in global content list. */
};

extern struct content *content_list;
extern const char *content_type_name[];
extern const char *content_status_name[];


struct browser_window;


content_type content_lookup(const char *mime_type);
struct content * content_create(const char *url);
struct content * content_get(const char *url);
bool content_set_type(struct content *c, content_type type,
		const char *mime_type, const char *params[]);
void content_set_status(struct content *c, const char *status_message, ...);
bool content_process_data(struct content *c, const char *data,
		unsigned int size);
void content_convert(struct content *c, int width, int height);
void content_reformat(struct content *c, int width, int height);
void content_clean(void);
void content_reset(struct content *c);
void content_redraw(struct content *c, int x, int y,
		int width, int height,
		int clip_x0, int clip_y0, int clip_x1, int clip_y1,
		float scale);
void content_add_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data),
		void *p1, void *p2);
struct content_user * content_find_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data),
		void *p1, void *p2);
void content_remove_user(struct content *c,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data),
		void *p1, void *p2);
void content_broadcast(struct content *c, content_msg msg,
		union content_msg_data data);
void content_stop(struct content *c,
		void (*callback)(content_msg msg, struct content *c, void *p1,
			void *p2, union content_msg_data data),
		void *p1, void *p2);
void content_add_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);
void content_remove_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);
void content_reshape_instance(struct content *c, struct browser_window *bw,
		struct content *page, struct box *box,
		struct object_params *params, void **state);
void content_add_error(struct content *c, const char *token,
		unsigned int line);

#endif
