/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
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
 * jsapi pseudo class glue.
 */

#ifndef _NETSURF_JAVASCRIPT_JSAPI_JSCLASS_H_
#define _NETSURF_JAVASCRIPT_JSAPI_JSCLASS_H_

#ifndef JSCLASS_NAME
#error "The class name must be defined"
#endif

#ifndef JSCLASS_TYPE
#define CLASS jsclass
#define PRIVATE priv
#define EXPAND(a,b) PASTE(a,b)
#define PASTE(x,y) x##_##y
#define JSCLASS_OBJECT EXPAND(CLASS,JSCLASS_NAME)
#define JSCLASS_TYPE EXPAND(JSCLASS_OBJECT,PRIVATE)
#endif

#endif
