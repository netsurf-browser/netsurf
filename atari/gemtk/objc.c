/*
 * Copyright 2013 Ole Loots <ole@monochrom.net>
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
 *
 * Module Description:
 *
 * AES Object tree tools.
 *
 */

 #include "gemtk.h"


OBJECT *get_tree(int idx)
{

  OBJECT *tree;

  rsrc_gaddr(R_TREE, idx, &tree);

  return tree;
}

bool obj_is_inside(OBJECT * tree, short obj, GRECT *area)
{
	GRECT obj_screen;
	bool ret = false;

	objc_offset(tree, obj, &obj_screen.g_x, &obj_screen.g_y);
	obj_screen.g_w = tree[obj].ob_width;
	obj_screen.g_h = tree[obj].ob_height;

	ret = RC_WITHIN(&obj_screen, area);

	return(ret);
}

GRECT * obj_screen_rect(OBJECT * tree, short obj)
{
	static GRECT obj_screen;

	get_objframe(tree, obj, &obj_screen);

	return(&obj_screen);
}
