/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
 * Copyright 2015 Daniel Dilverstone <dsilvers@netsurf-browser.org>
 * Copyright 2016 Michael Drake <tlsa@netsurf-browser.org>
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
 * Duktapeish implementation of javascript engine functions, prototypes.
 */

#ifndef DUKKY_H
#define DUKKY_H

#ifdef JS_DEBUG
#  define JS_LOG(format, args...) NSLOG(netsurf, INFO, format , ##args)
#else
#  define JS_LOG(format, ...) ((void) 0)
#endif

#define LOG(format, args...) NSLOG(netsurf, INFO, format , ##args)

duk_ret_t dukky_create_object(duk_context *ctx, const char *name, int args);
duk_bool_t dukky_push_node_stacked(duk_context *ctx);
duk_bool_t dukky_push_node(duk_context *ctx, struct dom_node *node);
void dukky_inject_not_ctr(duk_context *ctx, int idx, const char *name);
void dukky_register_event_listener_for(duk_context *ctx,
				       struct dom_element *ele,
				       dom_string *name,
				       bool capture);
bool dukky_get_current_value_of_event_handler(duk_context *ctx,
					      dom_string *name,
					      dom_event_target *et);
bool dukky_event_target_push_listeners(duk_context *ctx, bool dont_create);

typedef enum {
	ELF_CAPTURE = 1 << 0,
	ELF_PASSIVE = 1 << 1,
	ELF_ONCE    = 1 << 2,
	ELF_NONE    = 0
} event_listener_flags;

void dukky_shuffle_array(duk_context *ctx, duk_uarridx_t idx);

#endif
