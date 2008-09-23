/*
 * Copyright 2008 Daniel Silverstone <dsilvers@netsurf-browser.org>
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

#ifndef _NETSURF_FRAMEBUFFER_OPTIONS_H_
#define _NETSURF_FRAMEBUFFER_OPTIONS_H_

#include "desktop/options.h"

extern char *option_fb_mode;
extern char *option_fb_device;
extern char *option_fb_input_devpath;
extern char *option_fb_input_glob;

#define EXTRA_OPTION_DEFINE                     \
  char *option_fb_mode = 0;                     \
  char *option_fb_device = 0;                   \
  char *option_fb_input_devpath = 0;            \
  char *option_fb_input_glob = 0;

#define EXTRA_OPTION_TABLE \
  { "fb_mode", OPTION_STRING,	&option_fb_mode },      \
  { "fb_device", OPTION_STRING, &option_fb_device },    \
  { "fb_input_devpath", OPTION_STRING, &option_fb_input_devpath },      \
  { "fb_input_glob", OPTION_STRING, &option_fb_input_glob },

#endif
