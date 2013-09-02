/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
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
 * Generic tree handling (implementation).
 */


#define __STDBOOL_H__	1
extern "C" {
#include "utils/config.h"
#include "desktop/tree.h"
}

const char tree_directory_icon_name[] = "directory.png";
const char tree_content_icon_name[] = "content.png";



